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

/*
 * Interaction between core and LFGScripts
 */

#include "LFGScripts.h"
#include "Common.h"
#include "Group.h"
#include "LFGListMgr.h"
#include "LFGMgr.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "PartyPackets.h"
#include "Player.h"
#include "QueryPackets.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "WorldSession.h"

namespace lfg
{

LFGPlayerScript::LFGPlayerScript() : PlayerScript("LFGPlayerScript") { }

void LFGPlayerScript::OnLogout(Player* player)
{
    if (!sLFGMgr->isOptionEnabled(LFG_OPTION_ENABLE_DUNGEON_FINDER | LFG_OPTION_ENABLE_RAID_BROWSER))
        return;

    if (!player->GetGroup())
        sLFGMgr->LeaveLfg(player->GetGUID());
    else if (player->GetSession()->PlayerDisconnected())
        sLFGMgr->LeaveLfg(player->GetGUID(), true);
}

void LFGPlayerScript::OnLogin(Player* player, bool /*loginFirst*/)
{
    if (!sLFGMgr->isOptionEnabled(LFG_OPTION_ENABLE_DUNGEON_FINDER | LFG_OPTION_ENABLE_RAID_BROWSER))
        return;

    // Temporal: Trying to determine when group data and LFG data gets desynched
    ObjectGuid guid = player->GetGUID();
    ObjectGuid gguid = sLFGMgr->GetGroup(guid);

    if (Group const* group = player->GetGroup())
    {
        ObjectGuid gguid2 = group->GetGUID();
        if (gguid != gguid2)
        {
            TC_LOG_ERROR("lfg", "{} on group {} but LFG has group {} saved... Fixing.",
                player->GetSession()->GetPlayerInfo(), gguid2.ToString(), gguid.ToString());
            sLFGMgr->SetupGroupMember(guid, group->GetGUID());
        }
    }

    sLFGMgr->SetTeam(player->GetGUID(), player->GetTeam());
    /// @todo - Restore LfgPlayerData and send proper status to player if it was in a group
}

void LFGPlayerScript::OnMapChanged(Player* player)
{
    Map const* map = player->GetMap();

    if (sLFGMgr->inLfgDungeonMap(player->GetGUID(), map->GetId(), map->GetDifficultyID()))
    {
        Group* group = player->GetGroup();
        // This function is also called when players log in
        // if for some reason the LFG system recognises the player as being in a LFG dungeon,
        // but the player was loaded without a valid group, we'll teleport to homebind to prevent
        // crashes or other undefined behaviour
        if (!group)
        {
            sLFGMgr->LeaveLfg(player->GetGUID());
            player->RemoveAurasDueToSpell(LFG_SPELL_LUCK_OF_THE_DRAW);
            player->TeleportTo(player->m_homebind);
            TC_LOG_ERROR("lfg", "LFGPlayerScript::OnMapChanged, Player {} {} is in LFG dungeon map but does not have a valid group! "
                "Teleporting to homebind.", player->GetName(), player->GetGUID().ToString());
            return;
        }

        WorldPackets::Query::QueryPlayerNamesResponse response;
        for (Group::MemberSlot const& memberSlot : group->GetMemberSlots())
            player->GetSession()->BuildNameQueryData(memberSlot.guid, response.Players.emplace_back());

        player->SendDirectMessage(response.Write());

        if (sLFGMgr->selectedRandomLfgDungeon(player->GetGUID()))
            player->CastSpell(player, LFG_SPELL_LUCK_OF_THE_DRAW, true);
    }
    else
    {
        Group* group = player->GetGroup();
        if (group && group->GetMembersCount() == 1)
        {
            sLFGMgr->LeaveLfg(group->GetGUID());
            group->Disband();
            TC_LOG_DEBUG("lfg", "LFGPlayerScript::OnMapChanged, Player {}({}) is last in the lfggroup so we disband the group.",
                player->GetName(), player->GetGUID().ToString());
        }
        player->RemoveAurasDueToSpell(LFG_SPELL_LUCK_OF_THE_DRAW);
    }
}

LFGGroupScript::LFGGroupScript() : GroupScript("LFGGroupScript") { }

void LFGGroupScript::OnAddMember(Group* group, ObjectGuid guid)
{
    // Ahead of the option gate, same reasoning as in the three hooks below: the premade group finder is not
    // the dungeon finder. A search-result row lists the members of the LIVE group
    // (LFGListMgr::FillSearchRow), so every open browser showing this listing is now one member out of date
    // - and it stays that way, because the push used to happen only when the LEADER edited the listing.
    // This hook rather than the group-finder handler alone: a listed party also grows through an ordinary
    // party invite and through an LFG proposal, and the row is just as wrong then. Group::AddMember fires it
    // after the member slot is in place (Group.cpp:494-495), so the roster this reads is the new one.
    sLFGListMgr.NotifyGroupMemberJoined(group->GetGUID());

    if (!sLFGMgr->isOptionEnabled(LFG_OPTION_ENABLE_DUNGEON_FINDER | LFG_OPTION_ENABLE_RAID_BROWSER))
        return;

    ObjectGuid gguid = group->GetGUID();
    ObjectGuid leader = group->GetLeaderGUID();

    if (leader == guid)
    {
        TC_LOG_DEBUG("lfg", "LFGScripts::OnAddMember [{}]: added [{}] leader [{}]", gguid.ToString(), guid.ToString(), leader.ToString());
        sLFGMgr->SetLeader(gguid, guid);
    }
    else
    {
        LfgState gstate = sLFGMgr->GetState(gguid);
        LfgState state = sLFGMgr->GetState(guid);
        TC_LOG_DEBUG("lfg", "LFGScripts::OnAddMember [{}]: added [{}] leader [{}] gstate: {}, state: {}", gguid.ToString(), guid.ToString(), leader.ToString(), gstate, state);

        if (state == LFG_STATE_QUEUED)
            sLFGMgr->LeaveLfg(guid);

        if (gstate == LFG_STATE_QUEUED)
            sLFGMgr->LeaveLfg(gguid);
    }

    sLFGMgr->SetGroup(guid, gguid);
    sLFGMgr->AddPlayerToGroup(gguid, guid);
}

void LFGGroupScript::OnRemoveMember(Group* group, ObjectGuid guid, RemoveMethod method, ObjectGuid kicker, char const* reason)
{
    // Ahead of the option gate, for the same reason as in OnDisband: a readiness check does not depend on
    // the dungeon-finder options, and it must not keep waiting for an answer from a player who is gone. Left
    // in place, that answer stays PENDING forever, the check times out after 45 s and FinishReadyCheck drops
    // the REMAINING members out of every queue it was guarding - although all of them agreed. Unlike
    // OnDisband this does not abort the check: the group is still there and the rest of it can still be
    // ready. RemoveReadyCheckMember is a no-op when no check is running or the player is not in one.
    sLFGMgr->RemoveReadyCheckMember(group->GetGUID(), guid);

    // Ahead of the option gate for a second, independent reason: the premade group finder is NOT the
    // dungeon finder. It has its own manager, its own opcode family and no option of its own, and a realm
    // that has turned the dungeon finder off still lets players publish and join listings. Gating this on
    // LFG_OPTION_ENABLE_DUNGEON_FINDER would leave the listing system without its only member-departure
    // signal on exactly those realms.
    sLFGListMgr.NotifyGroupMemberLeft(group->GetGUID(), guid);

    if (!sLFGMgr->isOptionEnabled(LFG_OPTION_ENABLE_DUNGEON_FINDER | LFG_OPTION_ENABLE_RAID_BROWSER))
        return;

    ObjectGuid gguid = group->GetGUID();
    TC_LOG_DEBUG("lfg", "LFGScripts::OnRemoveMember [{}]: remove [{}] Method: {} Kicker: [{}] Reason: {}",
        gguid.ToString(), guid.ToString(), method, kicker.ToString(), (reason ? reason : ""));

    bool isLFG = group->isLFGGroup();

    if (isLFG && method == GROUP_REMOVEMETHOD_KICK)        // Player have been kicked
    {
        /// @todo - Update internal kick cooldown of kicker
        std::string str_reason = "";
        if (reason)
            str_reason = std::string(reason);
        sLFGMgr->InitBoot(gguid, kicker, guid, str_reason);
        return;
    }

    LfgState state = sLFGMgr->GetState(gguid);

    // If group is being formed after proposal success do nothing more
    if (state == LFG_STATE_PROPOSAL && method == GROUP_REMOVEMETHOD_DEFAULT)
    {
        // LfgData: Remove player from group
        sLFGMgr->SetGroup(guid, ObjectGuid::Empty);
        sLFGMgr->RemovePlayerFromGroup(gguid, guid);
        return;
    }

    sLFGMgr->LeaveLfg(guid);
    sLFGMgr->SetGroup(guid, ObjectGuid::Empty);
    uint8 players = sLFGMgr->RemovePlayerFromGroup(gguid, guid);

    if (Player* player = ObjectAccessor::FindPlayer(guid))
    {
        if (method == GROUP_REMOVEMETHOD_LEAVE && state == LFG_STATE_DUNGEON &&
            players >= LFG_GROUP_KICK_VOTES_NEEDED)
            player->CastSpell(player, LFG_SPELL_DUNGEON_DESERTER, true);
        else if (method == GROUP_REMOVEMETHOD_KICK_LFG)
            player->RemoveAurasDueToSpell(LFG_SPELL_DUNGEON_COOLDOWN);
        //else if (state == LFG_STATE_BOOT)
            // Update internal kick cooldown of kicked

        player->GetSession()->SendLfgUpdateStatus(LfgUpdateData(LFG_UPDATETYPE_LEADER_UNK1), true);
        if (isLFG && player->GetMap()->IsDungeon())            // Teleport player out the dungeon
            sLFGMgr->TeleportPlayer(player, true);
    }

    if (isLFG && state != LFG_STATE_FINISHED_DUNGEON) // Need more players to finish the dungeon
        if (Player* leader = ObjectAccessor::FindConnectedPlayer(sLFGMgr->GetLeader(gguid)))
            leader->GetSession()->SendLfgOfferContinue(sLFGMgr->GetDungeon(gguid, false));
}

void LFGGroupScript::OnDisband(Group* group)
{
    // Ahead of the option gate on purpose: a readiness check does not depend on the dungeon-finder options
    // (StartReadyCheck never consults them), and it must not outlive its group. LFGReadyCheckPopup is only
    // ever closed by an update, so a check dropped together with the group would leave the dialog standing
    // on every member's screen. AbortReadyCheck is a no-op when no check is running - LFGMgr::RemoveGroupData
    // calls it again, which covers any other caller of that function.
    sLFGMgr->AbortReadyCheck(group->GetGUID());

    // Ahead of the option gate, same reasoning as in OnRemoveMember. A listing whose party has been
    // disbanded went on being advertised and applied to for the rest of its 30-minute expiry window,
    // because the leader stays logged in and the logout path was the only thing that could have caught it.
    // Group::Disband fires this hook as its FIRST statement (Group.cpp:710), so the member slots are still
    // populated here - which is what the delist needs, since it addresses everyone holding an active entry.
    sLFGListMgr.RemoveListingsByGroup(group->GetGUID());

    if (!sLFGMgr->isOptionEnabled(LFG_OPTION_ENABLE_DUNGEON_FINDER | LFG_OPTION_ENABLE_RAID_BROWSER))
        return;

    ObjectGuid gguid = group->GetGUID();
    TC_LOG_DEBUG("lfg", "LFGScripts::OnDisband [{}]", gguid.ToString());

    sLFGMgr->RemoveGroupData(gguid);
}

void LFGGroupScript::OnChangeLeader(Group* group, ObjectGuid newLeaderGuid, ObjectGuid oldLeaderGuid)
{
    // Ahead of the option gate, for the same reason as in OnDisband and OnRemoveMember: a readiness check
    // does not depend on the dungeon-finder options. The check stores its own leader and that guid decides
    // who lands in slot 0 of SMSG_LFG_READY_CHECK_UPDATE, which the client reads positionally - so a
    // promotion has to be carried into it, or every following update names the wrong player as leader.
    // newLeaderGuid, not group->GetLeaderGUID(): Group::ChangeLeader fires this hook before it updates
    // m_leaderGuid (Group.cpp:676). SetReadyCheckLeader is a no-op when no check is running.
    sLFGMgr->SetReadyCheckLeader(group->GetGUID(), newLeaderGuid);

    // Ahead of the option gate, same reasoning as in OnRemoveMember. newLeaderGuid rather than
    // group->GetLeaderGUID() for the same reason as the line above: Group::ChangeLeader fires this hook
    // before it updates m_leaderGuid (Group.cpp:676).
    sLFGListMgr.TransferListingLeadership(group->GetGUID(), newLeaderGuid);

    if (!sLFGMgr->isOptionEnabled(LFG_OPTION_ENABLE_DUNGEON_FINDER | LFG_OPTION_ENABLE_RAID_BROWSER))
        return;

    ObjectGuid gguid = group->GetGUID();

    TC_LOG_DEBUG("lfg", "LFGScripts::OnChangeLeader [{}]: old [{}] new [{}]",
        gguid.ToString(), newLeaderGuid.ToString(), oldLeaderGuid.ToString());

    sLFGMgr->SetLeader(gguid, newLeaderGuid);

    // A group listing does not survive its leader changing hands. The client (consumer RVA
    // 0x24DEB10 at build 12.1.0.69382) fires LFG_GROUP_DELISTED_LEADERSHIP_CHANGE, shows
    // PREMADE_GROUP_LEADER_CHANGE_DELIST_WARNING and drops the listing unless the new leader
    // actively keeps it; listing name and the 60 second grace are produced by the client itself.
    // The message therefore has to reach every member, not only the new leader - otherwise a single
    // client delists while the rest keep showing the entry.
    //
    // UNVERIFIED: the precondition. Retail keys this on an active premade group finder listing
    // (C_LFGList), which does not exist in this tree, so the closest state we can actually observe
    // is used instead: the group is queued or browsing. Sending it for a group that holds no
    // listing at all would pop the delist warning for nothing.
    LfgState state = sLFGMgr->GetState(gguid);
    if (state == LFG_STATE_QUEUED || state == LFG_STATE_RAIDBROWSER)
    {
        WorldPackets::Party::PartyNotifyLFGLeaderChange notifyLeaderChange;
        notifyLeaderChange.NewLeaderGUID = newLeaderGuid;
        group->BroadcastPacket(notifyLeaderChange.Write(), false);
    }
}

void LFGGroupScript::OnInviteMember(Group* group, ObjectGuid guid)
{
    if (!sLFGMgr->isOptionEnabled(LFG_OPTION_ENABLE_DUNGEON_FINDER | LFG_OPTION_ENABLE_RAID_BROWSER))
        return;

    ObjectGuid gguid = group->GetGUID();
    ObjectGuid leader = group->GetLeaderGUID();
    TC_LOG_DEBUG("lfg", "LFGScripts::OnInviteMember [{}]: invite [{}] leader [{}]",
        gguid.ToString(), guid.ToString(), leader.ToString());

    // No gguid ==  new group being formed
    // No leader == after group creation first invite is new leader
    // leader and no gguid == first invite after leader is added to new group (this is the real invite)
    if (!leader.IsEmpty() && gguid.IsEmpty())
        sLFGMgr->LeaveLfg(leader);
}

void AddSC_LFGScripts()
{
    new LFGPlayerScript();
    new LFGGroupScript();
}

} // namespace lfg
