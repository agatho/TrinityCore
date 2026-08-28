/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "WorldSession.h"
#include "Common.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Group.h"
#include "GroupMgr.h"
#include "LFG.h"
#include "LFGMgr.h"
#include "Log.h"
#include "Loot.h"
#include "MiscPackets.h"
#include "ObjectAccessor.h"
#include "PartyPackets.h"
#include "Player.h"
#include "SocialMgr.h"
#include "World.h"
#include <algorithm>

class Aura;

/* differeces from off:
    -you can uninvite yourself - is is useful
    -you can accept invitation even if leader went offline
*/
/* todo:
    -group_destroyed msg is sent but not shown
    -reduce xp gaining when in raid group
    -quest sharing has to be corrected
    -FIX sending PartyMemberStats
*/

void WorldSession::SendPartyResult(PartyOperation operation, const std::string& member, PartyResult res, uint32 val /* = 0 */)
{
    WorldPackets::Party::PartyCommandResult packet;

    packet.Name = member;
    packet.Command = uint8(operation);
    packet.Result = uint8(res);
    packet.ResultData = val;
    packet.ResultGUID = ObjectGuid::Empty;

    SendPacket(packet.Write());
}

// Maps how the leader knows the player who suggested the invite onto Enum.PartyRequestJoinRelation
// (PartyConstantsDocumentation.lua:22-38). The client uses it to pick between the
// INVITE_CONFIRMATION_REQUEST_FRIEND / _GUILD / _COMMUNITY wordings.
static uint8 GetPartyRequestJoinRelation(Player const* leader, Player const* referredBy)
{
    if (leader->GetSocial()->HasFriend(referredBy->GetGUID()))
        return 1;                           // Enum.PartyRequestJoinRelation.Friend

    if (leader->GetGuildId() && leader->GetGuildId() == referredBy->GetGuildId())
        return 2;                           // Enum.PartyRequestJoinRelation.Guild

    return 0;                               // Enum.PartyRequestJoinRelation.None
}

// The rules that decide whether inviter may put invitee in a group at all - everything
// HandlePartyInviteOpcode used to check inline, in its original order and with its original codes.
//
// Pulled out because there are now TWO doors that issue an invite, and they issue it in the name of
// DIFFERENT players. CMSG_PARTY_INVITE checks the sender; the confirmation path checks nothing and
// then issues the invite in the LEADER's name a moment later, so none of the checks that ran
// against the suggesting member carry over. Left unshared, a leader whom the target has on the
// ignore list gets in through any group member as a middleman - the very refusal
// ERR_IGNORING_YOU_S exists for.
//
// Every caller reports the result as SendPartyResult(PARTY_OP_INVITE, invitee->GetName(), result),
// which is why the codes and not the messages live here.
static PartyResult CheckPartyInviteEligibility(Player const* inviter, Player const* invitee)
{
    // player trying to invite himself (most likely cheating)
    if (inviter == invitee)
        return ERR_BAD_PLAYER_NAME_S;

    // restrict invite to GMs
    if (!sWorld->getBoolConfig(CONFIG_ALLOW_GM_GROUP) && !inviter->IsGameMaster() && invitee->IsGameMaster())
        return ERR_BAD_PLAYER_NAME_S;

    // can't group with
    if (!inviter->IsGameMaster() && !sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GROUP) && inviter->GetTeam() != invitee->GetTeam())
        return ERR_PLAYER_WRONG_FACTION;

    if (inviter->GetInstanceId() != 0 && invitee->GetInstanceId() != 0 && inviter->GetInstanceId() != invitee->GetInstanceId() && inviter->GetMapId() == invitee->GetMapId())
        return ERR_TARGET_NOT_IN_INSTANCE_S;

    // just ignore us
    if (invitee->GetInstanceId() != 0 && invitee->GetDungeonDifficultyID() != inviter->GetDungeonDifficultyID())
        return ERR_IGNORING_YOU_S;

    if (invitee->GetSocial()->HasIgnore(inviter->GetGUID(), inviter->GetSession()->GetAccountGUID()))
        return ERR_IGNORING_YOU_S;

    if (!invitee->GetSocial()->HasFriend(inviter->GetGUID()) && inviter->GetLevel() < sWorld->getIntConfig(CONFIG_PARTY_LEVEL_REQ))
        return ERR_INVITE_RESTRICTED;

    return ERR_PARTY_RESULT_OK;
}

// Forwards a suggested invite from a member without invite rights to the group leader.
//
// Two different messages carry this, split by whether the leader can still act on the suggestion:
//   * group has room -> SMSG_CONFIRM_PARTY_INVITE with ReferredByGUID set, which the client turns
//     into confirmationType 2 (LE_INVITE_CONFIRMATION_SUGGEST) and shows as an Accept/Decline
//     dialog built from INVITE_CONFIRMATION_SUGGEST (LFGUtil.lua:270). Answering it sends
//     CMSG_QUICK_JOIN_RESPOND_TO_INVITE back.
//   * group is full  -> SMSG_SUGGEST_INVITE_INFORM, a plain chat line
//     (ERR_INFORM_SUGGEST_INVITE_SS, consumer RVA 0x1E20770). There is nothing to confirm, so the
//     leader is only informed. The same line covers the case where the leader already holds the
//     four confirmations the client can keep.
// UNVERIFIED: the split itself. Both messages exist and both are addressed to the leader, but no
// recording shows which one retail picks for which case, and the client cannot be asked - it simply
// renders whichever arrives. See AGENT_BRIEF_W4_GRUPPE_PARTY.md O-A.
// Returns false only when the suggestion could not be handed on at all, which is the caller's
// signal to answer the suggester with ERR_NOT_LEADER. A repetition swallowed by the cooldown
// returns true on purpose: the server dropped it as a duplicate of one it DID deliver a moment
// ago, and answering that with an error would put back exactly the false line this return value
// exists to remove.
bool WorldSession::SendSuggestedInvite(Group* group, Player* suggester, Player* target)
{
    Player* leader = ObjectAccessor::FindConnectedPlayer(group->GetLeaderGUID());
    if (!leader)
        return false;                       // nobody to suggest to - the suggester gets the error line

    // One suggestion per cooldown and session, counted on the SUGGESTER. This path is the only one
    // in the invite flow where a packet is produced at a THIRD player on somebody else's command:
    // every CMSG_PARTY_INVITE from a member without invite rights lands on the leader's screen, at
    // whatever rate the member chooses to send. Neither branch below throttles itself - the
    // confirmation refuses a duplicate ticket, but the chat line is exactly the fallback for that
    // refusal, so without this the message merely changes shape and keeps coming.
    //
    // The interval is a server side choice with no measured counterpart; nothing in the client
    // prescribes one.
    //
    // GAP, named rather than left implicit: retail answers the refused repetition with
    // SMSG_GROUP_ACTION_THROTTLED (0x450024), the immediate neighbour of
    // SMSG_SUMMON_RAID_MEMBER_VALIDATE_FAILED and part of this same flow. That opcode sits in
    // rest_kern_weltzustand of zuschnitt_0x45.json and is NOT in this unit's cut (brief 1.3 O5),
    // which requires a written change to extend. Until it is built, the throttled suggester gets
    // silence where retail shows him the throttle line. He is not misinformed, only uninformed.
    TimePoint const now = GameTime::Now();
    if (now < _nextSuggestedInviteTime)
        return true;

    _nextSuggestedInviteTime = now + SuggestedInviteCooldown;

    // The chat line is also the fallback when the leader's confirmation queue is already full: four
    // outstanding dialogs is all the client holds, so the fifth would be dropped clientside. Better
    // one line in the chat frame than a suggestion that vanishes.
    if (!group->IsFull() && SendInviteConfirmation(leader, group, target, suggester))
        return true;

    WorldPackets::Party::SuggestInviteInform suggestInviteInform;
    // UNVERIFIED: name order. ERR_INFORM_SUGGEST_INVITE_SS takes two %s and the client passes
    // both through unchanged, so the binary cannot decide it. (suggester, target) follows the
    // argument order of INVITE_CONFIRMATION_SUGGEST in LFGUtil.lua:270 and the reading of the
    // neighbouring GameError codes 81..85 as one direction.
    suggestInviteInform.SuggesterName = suggester->GetName();
    suggestInviteInform.TargetName = target->GetName();
    leader->SendDirectMessage(suggestInviteInform.Write());
    return true;
}

// Asks a group leader to confirm inviting target. referredBy set produces confirmationType 2
// (SUGGEST), leaving it empty produces 3 (QUEUE_WARNING); type 1 (REQUEST) additionally needs
// ShowChatLine and belongs to the quick join flow.
bool WorldSession::SendInviteConfirmation(Player* leader, Group* group, Player* target, Player* referredBy)
{
    WorldPackets::Party::ConfirmPartyInvite confirmPartyInvite;
    confirmPartyInvite.PartyGUID = group->GetGUID();
    confirmPartyInvite.InitializeApplicant(target);

    confirmPartyInvite.IsCrossFaction = leader->GetTeam() != target->GetTeam();

    // WillConvertToRaid is false in this tree, and that is a measured constant rather than an
    // oversight. The flag means "accepting this turns your party into a raid", which needs the
    // party to already hold MAX_GROUP_SIZE members - but TrinityCore has no convert-on-invite at
    // all: Group::AddMember refuses the sixth member with ERR_GROUP_FULL, and this dialog is only
    // ever built for a group that is not full, because SendSuggestedInvite routes the full group to
    // the chat line instead. Setting the bit would promise the client the
    // LFG_LIST_CONVERT_TO_RAID_WARNING and then have the accept path answer ERR_GROUP_FULL.
    confirmPartyInvite.WillConvertToRaid = false;

    // Queues the applicant is stuck in. The client hands these to Lua through
    // C_PartyInfo.GetInviteConfirmationInvalidQueues and formats the queue warning from them.
    if (sLFGMgr->GetState(target->GetGUID()) == lfg::LFG_STATE_QUEUED)
        for (uint32 dungeonId : sLFGMgr->GetSelectedDungeons(target->GetGUID()))
            confirmPartyInvite.InvalidLFG.push_back(dungeonId);

    // UNVERIFIED: InvalidLFGList stays empty because the premade group finder (C_LFGList) does not
    // exist in this tree; InvalidPvP stays empty because the wire ids the client expects for the
    // battleground queues are not decoded. Both may legitimately be empty - that is the normal case.

    if (referredBy)
    {
        confirmPartyInvite.ReferredByGUID = referredBy->GetGUID();
        confirmPartyInvite.ReferredByName = referredBy->GetName();
        confirmPartyInvite.ReferralRelation = GetPartyRequestJoinRelation(leader, referredBy);
        // UNVERIFIED: ReferredByClubID and the three unnamed referral numbers (wire offsets +776,
        // +796, +808) are sent as 0. C_PartyInfo.GetInviteReferralInfo reads them, but its
        // evaluation is not readable in the memory image, so their meaning is unknown. Zero does not
        // make the client discard the message; it only affects which of the seven
        // INVITE_CONFIRMATION_REQUEST_* wordings is chosen.
    }

    WorldSession* leaderSession = leader->GetSession();
    if (!leaderSession->AddPendingInviteConfirmation({
            .ApplicantGUID = confirmPartyInvite.ApplicantGUID,
            .PartyGUID = confirmPartyInvite.PartyGUID,
            .ReferredByGUID = confirmPartyInvite.ReferredByGUID,
            .ExpireTime = GameTime::Now() + PendingInviteConfirmationTimeout }))
        return false;   // leader already has four unanswered confirmations, the client would drop this one

    leader->SendDirectMessage(confirmPartyInvite.Write());
    return true;
}

bool WorldSession::AddPendingInviteConfirmation(PendingInviteConfirmation confirmation)
{
    TimePoint const now = GameTime::Now();
    std::erase_if(_pendingInviteConfirmations, [&](PendingInviteConfirmation const& pending)
    {
        return pending.ExpireTime <= now;
    });

    // A repeated suggestion for the same applicant is refused rather than replacing the live
    // ticket. Dropping the old entry first would have let the four-at-a-time limit be walked past
    // by simply asking again - the count never grows, so every repetition produced another
    // SMSG_CONFIRM_PARTY_INVITE at the leader. The leader already has this dialog open; there is
    // nothing to redraw.
    if (std::ranges::any_of(_pendingInviteConfirmations, [&](PendingInviteConfirmation const& pending)
        { return pending.ApplicantGUID == confirmation.ApplicantGUID; }))
        return false;

    if (_pendingInviteConfirmations.size() >= MaxPendingInviteConfirmations)
        return false;

    _pendingInviteConfirmations.push_back(confirmation);
    return true;
}

// Both guids are part of the key, and deliberately so: matching on the applicant alone and
// checking the party afterwards would let an answer carrying a wrong PartyGUID consume the ticket
// and return it unused, so the leader's real Accept would then find nothing and do nothing. Here a
// mismatch simply finds no ticket and leaves the open confirmation standing.
Optional<WorldSession::PendingInviteConfirmation> WorldSession::TakePendingInviteConfirmation(ObjectGuid applicantGuid, ObjectGuid partyGuid)
{
    TimePoint const now = GameTime::Now();
    auto itr = std::ranges::find_if(_pendingInviteConfirmations, [&](PendingInviteConfirmation const& pending)
    {
        return pending.ApplicantGUID == applicantGuid && pending.PartyGUID == partyGuid && pending.ExpireTime > now;
    });

    if (itr == _pendingInviteConfirmations.end())
        return {};

    PendingInviteConfirmation confirmation = *itr;
    _pendingInviteConfirmations.erase(itr);
    return confirmation;
}

// CMSG_QUICK_JOIN_RESPOND_TO_INVITE (0x430130) - the leader pressed Accept or Decline in the
// GROUP_INVITE_CONFIRMATION dialog. This closes the round trip that SMSG_CONFIRM_PARTY_INVITE
// opens, so it is registered here, next to the send side, rather than left to the CMSG family
// sweep: the copy of this opcode on feature/cmsg-sweep answers with a TC_LOG_DEBUG and its comment
// states the round trip cannot be built in the worldserver - which the measurement of the client
// disproves (AGENT_BRIEF_W4_GRUPPE_PARTY.md 2 K1). See registrierung/w4_gruppe_party.frag.json:
// when the branches are merged, THIS registration and this body are the ones to keep, and the
// MiscPackets/MiscHandler copy is the one to drop.
void WorldSession::HandleQuickJoinRespondToInvite(WorldPackets::Party::QuickJoinRespondToInvite& packet)
{
    // Mind the order - the client sends the applicant guid first, see PartyPackets.h.
    HandleInviteConfirmationResponse(packet.ApplicantGUID, packet.PartyGUID, packet.Accept);
}

void WorldSession::HandleInviteConfirmationResponse(ObjectGuid applicantGuid, ObjectGuid partyGuid, bool accept)
{
    // The client echoes back both guids we sent, so a mismatch is not something a well behaved
    // client produces - it is looked up by both so that a crafted one cannot spend somebody's
    // ticket by naming the wrong group, see TakePendingInviteConfirmation.
    Optional<PendingInviteConfirmation> confirmation = TakePendingInviteConfirmation(applicantGuid, partyGuid);
    if (!confirmation)
        return;                             // unknown, wrong group, already answered or expired

    Player* leader = GetPlayer();
    if (!leader)
        return;

    Group* group = sGroupMgr->GetGroupByGUID(confirmation->PartyGUID);
    if (!group || !group->IsLeader(leader->GetGUID()))
        return;

    Player* target = ObjectAccessor::FindConnectedPlayer(applicantGuid);
    if (!target)
    {
        SendPartyResult(PARTY_OP_INVITE, "", ERR_BAD_PLAYER_NAME_S);
        return;
    }

    if (!accept)
    {
        // Tell whoever suggested the invite that the leader turned it down.
        // UNVERIFIED: the code itself. Nothing decides it - the binary does not evaluate a refusal
        // path, the Lua has no handler for one, and there is no recording of this direction. Brief
        // 7.3 documents only the opposite direction (GameError 82, ERR_SUGGEST_INVITE_PLAYER_S);
        // whether retail answers a declined suggestion at all is unknown. ERR_INVITE_RESTRICTED is
        // picked as the one party result that says "not allowed, no reason given", and it is a
        // guess. It also collides with the level requirement in CheckPartyInviteEligibility, so the
        // suggester sees the same line for two unrelated causes.
        if (Player* referredBy = ObjectAccessor::FindConnectedPlayer(confirmation->ReferredByGUID))
            referredBy->GetSession()->SendPartyResult(PARTY_OP_INVITE, target->GetName(), ERR_INVITE_RESTRICTED);
        return;
    }

    if (group->IsFull())
    {
        SendPartyResult(PARTY_OP_INVITE, "", ERR_GROUP_FULL);
        return;
    }

    if (target->GetGroup() || target->GetGroupInvite())
    {
        SendPartyResult(PARTY_OP_INVITE, target->GetName(), ERR_ALREADY_IN_GROUP_S);
        return;
    }

    // The invite goes out in the LEADER's name from here on, so the leader has to pass the same
    // seven rules a leader who typed /invite would face. They were checked against the suggesting
    // member when CMSG_PARTY_INVITE arrived and say nothing about the leader.
    if (PartyResult eligibility = CheckPartyInviteEligibility(leader, target); eligibility != ERR_PARTY_RESULT_OK)
    {
        SendPartyResult(PARTY_OP_INVITE, target->GetName(), eligibility);
        return;
    }

    if (!group->AddInvite(target))
        return;

    WorldPackets::Party::PartyInvite partyInvite;
    partyInvite.Initialize(leader, 0, true);
    target->SendDirectMessage(partyInvite.Write());

    SendPartyResult(PARTY_OP_INVITE, target->GetName(), ERR_PARTY_RESULT_OK);
}

void WorldSession::HandlePartyInviteOpcode(WorldPackets::Party::PartyInviteClient& packet)
{
    Player* invitingPlayer = GetPlayer();
    Player* invitedPlayer = ObjectAccessor::FindPlayerByName(packet.TargetName);

    // no player
    if (!invitedPlayer)
    {
        SendPartyResult(PARTY_OP_INVITE, packet.TargetName, ERR_BAD_PLAYER_NAME_S);
        return;
    }

    // Self invite, GM, faction, instance, difficulty, ignore list and level requirement - the same
    // seven rules the confirmation path applies to the leader, see CheckPartyInviteEligibility.
    if (PartyResult eligibility = CheckPartyInviteEligibility(invitingPlayer, invitedPlayer); eligibility != ERR_PARTY_RESULT_OK)
    {
        SendPartyResult(PARTY_OP_INVITE, invitedPlayer->GetName(), eligibility);
        return;
    }

    Group* group = invitingPlayer->GetGroup(packet.PartyIndex);
    if (!group)
        group = invitingPlayer->GetGroupInvite();

    Group* group2 = invitedPlayer->GetGroup(packet.PartyIndex);
    // player already in another group or invited
    if (group2 || invitedPlayer->GetGroupInvite())
    {
        SendPartyResult(PARTY_OP_INVITE, invitedPlayer->GetName(), ERR_ALREADY_IN_GROUP_S);

        if (group2)
        {
            // tell the player that they were invited but it failed as they were already in a group
            WorldPackets::Party::PartyInvite partyInvite;
            partyInvite.Initialize(invitingPlayer, packet.ProposedRoles, false);
            invitedPlayer->SendDirectMessage(partyInvite.Write());
        }

        return;
    }

    if (group)
    {
        // not have permissions for invite
        if (!group->IsLeader(invitingPlayer->GetGUID()) && !group->IsAssistant(invitingPlayer->GetGUID()))
        {
            if (group->IsCreated())
            {
                // This is the "Suggest Invite" path: the client of a member who may not invite still
                // sends CMSG_PARTY_INVITE (Blizzard_UnitPopup/Mainline/UnitPopupUtils.lua:94-106,
                // GetDisplayedInviteType in Blizzard_LFGUtil/Mainline/LFGUtil.lua:88-111). Retail
                // forwards the suggestion to the leader; before this it was dropped on the floor.
                //
                // ERR_NOT_LEADER goes out only when the suggestion could NOT be handed on. The
                // member's own UI labels this button "Suggest Invite", so on the path where the
                // suggestion reaches the leader, telling him "you are not the leader" reports a
                // failure for an action that worked. Retail's answer to the suggester on that path
                // is GameError 82 ERR_SUGGEST_INVITE_PLAYER_S (SharedDefines.h, brief 7.3), which
                // carries a %s; SMSG_DISPLAY_GAME_ERROR has no string carrying ctor form (brief 1.4
                // F3), so it cannot be sent from this tree. The suggester therefore gets no
                // confirmation - silence where retail confirms, which is a named gap, rather than
                // an error where retail confirms, which was a wrong statement.
                if (!SendSuggestedInvite(group, invitingPlayer, invitedPlayer))
                    SendPartyResult(PARTY_OP_INVITE, "", ERR_NOT_LEADER);
            }
            return;
        }
        // not have place
        if (group->IsFull())
        {
            SendPartyResult(PARTY_OP_INVITE, "", ERR_GROUP_FULL);
            return;
        }
    }

    // ok, but group not exist, start a new group
    // but don't create and save the group to the DB until
    // at least one person joins
    if (!group)
    {
        group = new Group();
        // new group: if can't add then delete
        if (!group->AddLeaderInvite(invitingPlayer))
        {
            delete group;
            return;
        }
        if (!group->AddInvite(invitedPlayer))
        {
            group->RemoveAllInvites();
            delete group;
            return;
        }
    }
    else
    {
        // already existed group: if can't add then just leave
        if (!group->AddInvite(invitedPlayer))
        {
            return;
        }
    }

    WorldPackets::Party::PartyInvite partyInvite;
    partyInvite.Initialize(invitingPlayer, packet.ProposedRoles, true);
    invitedPlayer->SendDirectMessage(partyInvite.Write());

    SendPartyResult(PARTY_OP_INVITE, invitedPlayer->GetName(), ERR_PARTY_RESULT_OK);
}

void WorldSession::HandlePartyInviteResponseOpcode(WorldPackets::Party::PartyInviteResponse& packet)
{
    Group* group = GetPlayer()->GetGroupInvite();

    if (!group)
        return;

    if (packet.PartyIndex && group->GetGroupCategory() != GroupCategory(*packet.PartyIndex))
        return;

    if (packet.Accept)
    {
        // Remove player from invitees in any case
        group->RemoveInvite(GetPlayer());

        if (group->GetLeaderGUID() == GetPlayer()->GetGUID())
        {
            TC_LOG_ERROR("network", "HandleGroupAcceptOpcode: player {} {} tried to accept an invite to his own group", GetPlayer()->GetName(), GetPlayer()->GetGUID().ToString());
            return;
        }

        // Group is full
        if (group->IsFull())
        {
            SendPartyResult(PARTY_OP_INVITE, "", ERR_GROUP_FULL);
            return;
        }

        Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID());

        // Forming a new group, create it
        if (!group->IsCreated())
        {
            // This can happen if the leader is zoning. To be removed once delayed actions for zoning are implemented
            if (!leader)
            {
                group->RemoveAllInvites();
                return;
            }

            // If we're about to create a group there really should be a leader present
            ASSERT(leader);
            group->RemoveInvite(leader);
            group->Create(leader);
            sGroupMgr->AddGroup(group);
        }

        // Everything is fine, do it, PLAYER'S GROUP IS SET IN ADDMEMBER!!!
        if (!group->AddMember(GetPlayer()))
            return;

        group->BroadcastGroupUpdate();
    }
    else
    {
        // Remember leader if online (group pointer will be invalid if group gets disbanded)
        Player* leader = ObjectAccessor::FindConnectedPlayer(group->GetLeaderGUID());

        // uninvite, group can be deleted
        GetPlayer()->UninviteFromGroup();

        if (!leader || !leader->GetSession())
            return;

        // report
        WorldPackets::Party::GroupDecline decline(GetPlayer()->GetName());
        leader->SendDirectMessage(decline.Write());
    }
}

void WorldSession::HandlePartyUninviteOpcode(WorldPackets::Party::PartyUninvite& packet)
{
    // can't uninvite yourself
    if (packet.TargetGUID == GetPlayer()->GetGUID())
    {
        TC_LOG_ERROR("network", "WorldSession::HandleGroupUninviteGuidOpcode: leader {} {} tried to uninvite himself from the group.",
            GetPlayer()->GetName(), GetPlayer()->GetGUID().ToString());
        return;
    }

    PartyResult res = GetPlayer()->CanUninviteFromGroup(packet.TargetGUID, packet.PartyIndex);
    if (res != ERR_PARTY_RESULT_OK)
    {
        SendPartyResult(PARTY_OP_UNINVITE, "", res);
        return;
    }

    Group* grp = GetPlayer()->GetGroup(packet.PartyIndex);
    // grp is checked already above in CanUninviteFromGroup()
    ASSERT(grp);

    if (grp->IsMember(packet.TargetGUID))
    {
        Player::RemoveFromGroup(grp, packet.TargetGUID, GROUP_REMOVEMETHOD_KICK, GetPlayer()->GetGUID(), packet.Reason.c_str());
        return;
    }

    if (Player* player = grp->GetInvited(packet.TargetGUID))
    {
        player->UninviteFromGroup();
        return;
    }

    SendPartyResult(PARTY_OP_UNINVITE, "", ERR_TARGET_NOT_IN_GROUP_S);
}

void WorldSession::HandleSetPartyLeaderOpcode(WorldPackets::Party::SetPartyLeader& packet)
{
    Player* player = ObjectAccessor::FindConnectedPlayer(packet.TargetGUID);
    Group* group = GetPlayer()->GetGroup(packet.PartyIndex);

    if (!group || !player)
        return;

    if (!group->IsLeader(GetPlayer()->GetGUID()) || player->GetGroup() != group)
        return;

    // Everything's fine, accepted.
    group->ChangeLeader(packet.TargetGUID);
    group->SendUpdate();
}

void WorldSession::HandleSetRoleOpcode(WorldPackets::Party::SetRole& packet)
{
    WorldPackets::Party::RoleChangedInform roleChangedInform;

    Group* group = GetPlayer()->GetGroup(packet.PartyIndex);
    uint8 oldRole = group ? group->GetLfgRoles(packet.TargetGUID) : 0;
    if (oldRole == packet.Role)
        return;

    roleChangedInform.From = GetPlayer()->GetGUID();
    roleChangedInform.ChangedUnit = packet.TargetGUID;
    roleChangedInform.OldRole = oldRole;
    roleChangedInform.NewRole = packet.Role;

    if (group)
    {
        roleChangedInform.PartyIndex = group->GetGroupCategory();
        group->BroadcastPacket(roleChangedInform.Write(), false);
        group->SetLfgRoles(packet.TargetGUID, packet.Role);
    }
    else
        SendPacket(roleChangedInform.Write());
}

void WorldSession::HandleLeaveGroupOpcode(WorldPackets::Party::LeaveGroup& packet)
{
    Group* grp = GetPlayer()->GetGroup(packet.PartyIndex);
    Group* grpInvite = GetPlayer()->GetGroupInvite();
    if (!grp && !grpInvite)
        return;

    if (_player->InBattleground())
        return;

    /** error handling **/
    /********************/

    // everything's fine, do it
    if (grp)
    {
        SendPartyResult(PARTY_OP_LEAVE, GetPlayer()->GetName(), ERR_PARTY_RESULT_OK);
        GetPlayer()->RemoveFromGroup(GROUP_REMOVEMETHOD_LEAVE);
    }
    else if (grpInvite && grpInvite->GetLeaderGUID() == GetPlayer()->GetGUID())
    { // pending group creation being cancelled
        SendPartyResult(PARTY_OP_LEAVE, GetPlayer()->GetName(), ERR_PARTY_RESULT_OK);
        grpInvite->Disband();
    }
}

void WorldSession::HandleSetLootMethodOpcode(WorldPackets::Party::SetLootMethod& /*packet*/)
{
    // not allowed to change
    /*
    Group* group = GetPlayer()->GetGroup(packet.PartyIndex);
    if (!group)
        return;

    if (!group->IsLeader(GetPlayer()->GetGUID()))
        return;

    if (group->isLFGGroup())
        return;

    switch (packet.LootMethod)
    {
        case FREE_FOR_ALL:
        case MASTER_LOOT:
        case GROUP_LOOT:
        case PERSONAL_LOOT:
            break;
        default:
            return;
    }

    if (packet.LootThreshold < ITEM_QUALITY_UNCOMMON || packet.LootThreshold > ITEM_QUALITY_ARTIFACT)
        return;

    if (packet.LootMethod == MASTER_LOOT && !group->IsMember(packet.LootMasterGUID))
        return;

    // everything's fine, do it
    group->SetLootMethod(static_cast<LootMethod>(packet.LootMethod));
    group->SetMasterLooterGuid(packet.LootMasterGUID);
    group->SetLootThreshold(static_cast<ItemQualities>(packet.LootThreshold));
    group->SendUpdate();
    */
}

void WorldSession::HandleMinimapPingOpcode(WorldPackets::Party::MinimapPingClient& packet)
{
    Group const* group = GetPlayer()->GetGroup(packet.PartyIndex);
    if (!group)
        return;

    WorldPackets::Party::MinimapPing minimapPing;
    minimapPing.Sender = GetPlayer()->GetGUID();
    minimapPing.PositionX = packet.PositionX;
    minimapPing.PositionY = packet.PositionY;
    group->BroadcastPacket(minimapPing.Write(), true, -1, GetPlayer()->GetGUID());
}

void WorldSession::HandleRandomRollOpcode(WorldPackets::Misc::RandomRollClient& packet)
{
    /** error handling **/
    if (packet.Min > packet.Max || packet.Max > 1000000)
        return;
    /********************/

    GetPlayer()->DoRandomRoll(packet.Min, packet.Max);
}

void WorldSession::HandleUpdateRaidTargetOpcode(WorldPackets::Party::UpdateRaidTarget& packet)
{
    Group* group = GetPlayer()->GetGroup(packet.PartyIndex);
    if (!group)
        return;

    if (packet.Symbol == -1)                  // target icon request
        group->SendTargetIconList(this);
    else                                        // target icon update
    {
        if (group->isRaidGroup() && !group->IsLeader(GetPlayer()->GetGUID()) && !group->IsAssistant(GetPlayer()->GetGUID()))
            return;

        if (packet.Target.IsPlayer())
        {
            Player* target = ObjectAccessor::FindConnectedPlayer(packet.Target);
            if (!target || target->IsHostileTo(GetPlayer()))
                return;
        }

        group->SetTargetIcon(packet.Symbol, packet.Target, GetPlayer()->GetGUID());
    }
}

void WorldSession::HandleConvertRaidOpcode(WorldPackets::Party::ConvertRaid& packet)
{
    Group* group = GetPlayer()->GetGroup();
    if (!group)
        return;

    if (_player->InBattleground())
        return;

    // error handling
    if (!group->IsLeader(GetPlayer()->GetGUID()) || group->GetMembersCount() < 2)
        return;

    // everything's fine, do it (is it 0 (PARTY_OP_INVITE) correct code)
    SendPartyResult(PARTY_OP_INVITE, "", ERR_PARTY_RESULT_OK);

    // New 4.x: it is now possible to convert a raid to a group if member count is 5 or less
    if (packet.Raid)
        group->ConvertToRaid();
    else
        group->ConvertToGroup();
}

void WorldSession::HandleRequestPartyJoinUpdates(WorldPackets::Party::RequestPartyJoinUpdates& packet)
{
    Group* group = GetPlayer()->GetGroup(packet.PartyIndex);
    if (!group)
        return;

    group->SendTargetIconList(this);
    group->SendRaidMarkersChanged(this);
}

void WorldSession::HandleChangeSubGroupOpcode(WorldPackets::Party::ChangeSubGroup& packet)
{
    // we will get correct pointer for group here, so we don't have to check if group is BG raid
    Group* group = GetPlayer()->GetGroup(packet.PartyIndex);
    if (!group)
        return;

    if (packet.NewSubGroup >= MAX_RAID_SUBGROUPS)
        return;

    ObjectGuid senderGuid = GetPlayer()->GetGUID();
    if (!group->IsLeader(senderGuid) && !group->IsAssistant(senderGuid))
        return;

    if (!group->HasFreeSlotSubGroup(packet.NewSubGroup))
        return;

    group->ChangeMembersGroup(packet.TargetGUID, packet.NewSubGroup);
}

void WorldSession::HandleSwapSubGroupsOpcode(WorldPackets::Party::SwapSubGroups& packet)
{
    Group* group = GetPlayer()->GetGroup(packet.PartyIndex);
    if (!group)
        return;

    ObjectGuid senderGuid = GetPlayer()->GetGUID();
    if (!group->IsLeader(senderGuid) && !group->IsAssistant(senderGuid))
        return;

    group->SwapMembersGroups(packet.FirstTarget, packet.SecondTarget);
}

void WorldSession::HandleSetAssistantLeaderOpcode(WorldPackets::Party::SetAssistantLeader& packet)
{
    Group* group = GetPlayer()->GetGroup(packet.PartyIndex);
    if (!group)
        return;

    if (!group->IsLeader(GetPlayer()->GetGUID()))
        return;

    group->SetGroupMemberFlag(packet.Target, packet.Apply, MEMBER_FLAG_ASSISTANT);
}

void WorldSession::HandleSetPartyAssignment(WorldPackets::Party::SetPartyAssignment& packet)
{
    Group* group = GetPlayer()->GetGroup(packet.PartyIndex);
    if (!group)
        return;

    ObjectGuid senderGuid = GetPlayer()->GetGUID();
    if (!group->IsLeader(senderGuid) && !group->IsAssistant(senderGuid))
        return;

    switch (packet.Assignment)
    {
        case GROUP_ASSIGN_MAINASSIST:
            group->RemoveUniqueGroupMemberFlag(MEMBER_FLAG_MAINASSIST);
            group->SetGroupMemberFlag(packet.Target, packet.Set, MEMBER_FLAG_MAINASSIST);
            break;
        case GROUP_ASSIGN_MAINTANK:
            group->RemoveUniqueGroupMemberFlag(MEMBER_FLAG_MAINTANK);           // Remove main assist flag from current if any.
            group->SetGroupMemberFlag(packet.Target, packet.Set, MEMBER_FLAG_MAINTANK);
            break;
        default:
            break;
    }

    group->SendUpdate();
}

void WorldSession::HandleDoReadyCheckOpcode(WorldPackets::Party::DoReadyCheck& packet)
{
    Group* group = GetPlayer()->GetGroup(packet.PartyIndex);
    if (!group)
        return;

    /** error handling **/
    if (!group->IsLeader(GetPlayer()->GetGUID()) && !group->IsAssistant(GetPlayer()->GetGUID()))
        return;
    /********************/

    // everything's fine, do it
    group->StartReadyCheck(GetPlayer()->GetGUID());
}

void WorldSession::HandleReadyCheckResponseOpcode(WorldPackets::Party::ReadyCheckResponseClient& packet)
{
    Group* group = GetPlayer()->GetGroup(packet.PartyIndex);
    if (!group)
        return;

    // everything's fine, do it
    group->SetMemberReadyCheck(GetPlayer()->GetGUID(), packet.IsReady);
}

void WorldSession::HandleRequestPartyMemberStatsOpcode(WorldPackets::Party::RequestPartyMemberStats& packet)
{
    for (ObjectGuid const& target : packet.Targets)
    {
        WorldPackets::Party::PartyMemberFullState partyMemberStats;
        Player* player = ObjectAccessor::FindConnectedPlayer(target);
        if (!player || !GetPlayer()->IsInSameRaidWith(player))
        {
            partyMemberStats.MemberGuid = target;
            partyMemberStats.MemberStats.Status = MEMBER_STATUS_OFFLINE;
        }
        else
        {
            partyMemberStats.Initialize(player);
        }
        SendPacket(partyMemberStats.Write());
    }
}

void WorldSession::HandleRequestRaidInfoOpcode(WorldPackets::Party::RequestRaidInfo& /*packet*/)
{
    // every time the player checks the character screen
    _player->SendRaidInfo();
}

void WorldSession::HandleOptOutOfLootOpcode(WorldPackets::Party::OptOutOfLoot& packet)
{
    // ignore if player not loaded
    if (!GetPlayer())                                        // needed because STATUS_AUTHED
    {
        if (packet.PassOnLoot)
            TC_LOG_ERROR("network", "CMSG_OPT_OUT_OF_LOOT value<>0 for not-loaded character!");
        return;
    }

    GetPlayer()->SetPassOnGroupLoot(packet.PassOnLoot);
}

void WorldSession::HandleInitiateRolePoll(WorldPackets::Party::InitiateRolePoll& packet)
{
    Group const* group = GetPlayer()->GetGroup(packet.PartyIndex);
    if (!group)
        return;

    ObjectGuid guid = GetPlayer()->GetGUID();
    if (!group->IsLeader(guid) && !group->IsAssistant(guid))
        return;

    WorldPackets::Party::RolePollInform rolePollInform;
    rolePollInform.From = GetPlayer()->GetGUID();
    rolePollInform.PartyIndex = group->GetGroupCategory();
    group->BroadcastPacket(rolePollInform.Write(), true);
}

void WorldSession::HandleSetEveryoneIsAssistant(WorldPackets::Party::SetEveryoneIsAssistant& packet)
{
    Group* group = GetPlayer()->GetGroup(packet.PartyIndex);
    if (!group)
        return;

    if (!group->IsLeader(GetPlayer()->GetGUID()))
        return;

    group->SetEveryoneIsAssistant(packet.EveryoneIsAssistant);
}

void WorldSession::HandleClearRaidMarker(WorldPackets::Party::ClearRaidMarker& packet)
{
    Group* group = GetPlayer()->GetGroup();
    if (!group)
        return;

    if (group->isRaidGroup() && !group->IsLeader(GetPlayer()->GetGUID()) && !group->IsAssistant(GetPlayer()->GetGUID()))
        return;

    group->DeleteRaidMarker(packet.MarkerId);
}

namespace
{
bool CanSendPing(Player const& player, PingSubjectType type, Group const*& group)
{
    if (type >= PingSubjectType::Max)
        return false;

    if (!player.GetSession()->CanSpeak())
        return false;

    group = player.GetGroup();
    if (!group)
        return false;

    if (group->IsLeader(player.GetGUID()))
        return true;

    switch (group->GetRestrictPings())
    {
        case RestrictPingsTo::None:
            return true;
        case RestrictPingsTo::Lead:
            return false;
        case RestrictPingsTo::Assist:
            if (!group->IsAssistant(player.GetGUID()))
                return false;
            break;
        case RestrictPingsTo::TankHealer:
            if (!(group->GetLfgRoles(player.GetGUID()) & (lfg::PLAYER_ROLE_TANK | lfg::PLAYER_ROLE_HEALER)))
                return false;
            break;
    }

    return true;
}
}

void WorldSession::HandleSetRestrictPingsToAssistants(WorldPackets::Party::SetRestrictPingsToAssistants const& setRestrictPingsToAssistants)
{
    Group* group = GetPlayer()->GetGroup(setRestrictPingsToAssistants.PartyIndex);
    if (!group)
        return;

    if (!group->IsLeader(GetPlayer()->GetGUID()))
        return;

    group->SetRestrictPingsTo(setRestrictPingsToAssistants.RestrictTo);
}

void WorldSession::HandleSendPingUnit(WorldPackets::Party::SendPingUnit const& pingUnit)
{
    Group const* group = nullptr;
    if (!CanSendPing(*_player, pingUnit.Type, group))
        return;

    Unit const* target = ObjectAccessor::GetUnit(*_player, pingUnit.TargetGUID);
    if (!target || !_player->HaveAtClient(target))
        return;

    WorldPackets::Party::ReceivePingUnit broadcastPingUnit;
    broadcastPingUnit.SenderGUID = _player->GetGUID();
    broadcastPingUnit.TargetGUID = pingUnit.TargetGUID;
    broadcastPingUnit.Type = pingUnit.Type;
    broadcastPingUnit.PinFrameID = pingUnit.PinFrameID;
    broadcastPingUnit.PingDuration = pingUnit.PingDuration;
    broadcastPingUnit.Health = pingUnit.Health;
    broadcastPingUnit.Mana = pingUnit.Mana;
    broadcastPingUnit.IsUnitFrameStatusTextPing = pingUnit.IsUnitFrameStatusTextPing;
    broadcastPingUnit.CreatureID = pingUnit.CreatureID;
    broadcastPingUnit.SpellOverrideNameID = pingUnit.SpellOverrideNameID;
    broadcastPingUnit.Write();

    for (GroupReference const& itr : group->GetMembers())
    {
        Player const* member = itr.GetSource();
        if (_player == member || !_player->IsInMap(member))
            continue;

        member->SendDirectMessage(broadcastPingUnit.GetRawPacket());
    }
}

void WorldSession::HandleSendPingWorldPoint(WorldPackets::Party::SendPingWorldPoint const& pingWorldPoint)
{
    Group const* group = nullptr;
    if (!CanSendPing(*_player, pingWorldPoint.Type, group))
        return;

    if (_player->GetMapId() != pingWorldPoint.MapID)
        return;

    WorldPackets::Party::ReceivePingWorldPoint broadcastPingWorldPoint;
    broadcastPingWorldPoint.SenderGUID = _player->GetGUID();
    broadcastPingWorldPoint.MapID = pingWorldPoint.MapID;
    broadcastPingWorldPoint.Point = pingWorldPoint.Point;
    broadcastPingWorldPoint.Type = pingWorldPoint.Type;
    broadcastPingWorldPoint.PinFrameID = pingWorldPoint.PinFrameID;
    broadcastPingWorldPoint.Transport = pingWorldPoint.Transport;
    broadcastPingWorldPoint.PingDuration = pingWorldPoint.PingDuration;
    broadcastPingWorldPoint.Write();

    for (GroupReference const& itr : group->GetMembers())
    {
        Player const* member = itr.GetSource();
        if (_player == member || !_player->IsInMap(member))
            continue;

        member->SendDirectMessage(broadcastPingWorldPoint.GetRawPacket());
    }
}

void WorldSession::HandleSendPingCooldown(WorldPackets::Party::SendPingCooldown const& pingCooldown)
{
    Group const* group = nullptr;
    if (!CanSendPing(*_player, pingCooldown.Type, group))
        return;

    WorldPackets::Party::ReceivePingCooldown broadcastPingCooldown;
    broadcastPingCooldown.SenderGUID = _player->GetGUID();
    broadcastPingCooldown.PinFrameID = pingCooldown.PinFrameID;
    broadcastPingCooldown.SpellID = pingCooldown.SpellID;
    broadcastPingCooldown.ItemID = pingCooldown.ItemID;
    broadcastPingCooldown.Duration = pingCooldown.Duration;
    broadcastPingCooldown.Remaining = pingCooldown.Remaining;
    broadcastPingCooldown.Type = pingCooldown.Type;
    broadcastPingCooldown.SpellCategoryID = pingCooldown.SpellCategoryID;
    broadcastPingCooldown.Write();

    for (GroupReference const& itr : group->GetMembers())
    {
        Player const* member = itr.GetSource();
        if (_player == member || !_player->IsInMap(member))
            continue;

        member->SendDirectMessage(broadcastPingCooldown.GetRawPacket());
    }
}
