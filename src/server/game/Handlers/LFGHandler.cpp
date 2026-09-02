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

#include "CharacterCache.h"
#include "Chat.h"
#include "Config.h"
#include "DB2Stores.h"
#include "WorldSession.h"
#include "GameTime.h"
#include "Group.h"
#include "LFGMgr.h"
#include "LFGPackets.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Warfront.h"
#include "WarfrontMgr.h"

namespace
{
    // The BfA war-table "Join Battle" button queues through the LFG system, not BattlemasterList: the client calls
    // JoinSingleLFG(LE_LFG_CATEGORY_SCENARIO, lfgDungeonID), which sends CMSG_DF_JOIN with
    // slot = dungeonID | (TypeID << 24) (warfronts are TypeID 1). A warfront is one faction versus NPCs, so the
    // normal LFG group-forming/proposal flow is wrong for it - there is nobody to match against and no role check to
    // run. Instead we hand the request to WarfrontMgr's single-team enrollment queue, which at the configured
    // min-player floor forms the battle group and teleports the enrolled players into the instanced battle map.
    //
    // Returns true when the join was consumed as a warfront join (so the caller must NOT continue into JoinLfg).
    // Robust against an empty slot list and against a request that mixes warfront and normal dungeons: the first
    // warfront slot wins (a warfront cannot be co-queued with anything else).
    bool TryHandleWarfrontLfgJoin(WorldSession* session, WorldPackets::LFG::DFJoin const& dfJoin)
    {
        Player* player = session->GetPlayer();
        if (!player)
            return false;

        for (uint32 slot : dfJoin.Slots)
        {
            // Same decode the normal path uses below: the low 24 bits are the LFGDungeons.db2 id.
            uint32 const dungeonId = slot & 0x00FFFFFF;

            uint32 battleMapId = 0;
            uint32 const warfrontId = WarfrontMgr::GetWarfrontForLfgDungeon(dungeonId, &battleMapId);
            if (!warfrontId)
                continue;   // an ordinary LFG slot - keep looking

            // TODO: the Heroic warfront dungeon ids (2007/1982/2032/2031) resolve to the same warfront and battle
            // map as their Normal twins; per-difficulty scaling/lockouts are a later phase.
            std::string reason;
            bool const joined = sWarfrontMgr->EnqueuePlayer(player, warfrontId, &reason);

            Warfront const* wf = sWarfrontMgr->GetWarfront(warfrontId);
            TC_LOG_INFO("warfront", "CMSG_DF_JOIN intercepted as warfront join: {} slot 0x{:X} (LFGDungeon {}) -> "
                "warfront {} '{}', battle map {} : {}",
                session->GetPlayerInfo(), slot, dungeonId, warfrontId, wf ? wf->Name : "<unknown>", battleMapId,
                joined ? "ACCEPTED" : (reason.empty() ? "REJECTED (no reason given)" : "REJECTED - " + reason));

            // Never let the button fail silently - always tell the player what happened.
            if (joined)
                ChatHandler(session).SendSysMessage("You have joined the assault. Stand ready - you will be summoned when the war party musters.");
            else
                ChatHandler(session).SendSysMessage(reason.empty() ? "You cannot join the assault right now." : reason.c_str());

            return true;
        }

        return false;
    }
}

void WorldSession::HandleLfgJoinOpcode(WorldPackets::LFG::DFJoin& dfJoin)
{
    // Warfront interception runs BEFORE the dungeon-finder option / group-leader gates below: a warfront assault is
    // not a dungeon-finder queue, so it must work regardless of how LFG itself is configured, and a non-leader in a
    // party may still enroll himself for the assault.
    if (TryHandleWarfrontLfgJoin(this, dfJoin))
        return;

    if (!sLFGMgr->isOptionEnabled(lfg::LFG_OPTION_ENABLE_DUNGEON_FINDER | lfg::LFG_OPTION_ENABLE_RAID_BROWSER) ||
        (GetPlayer()->GetGroup() && GetPlayer()->GetGroup()->GetLeaderGUID() != GetPlayer()->GetGUID() &&
        (GetPlayer()->GetGroup()->GetMembersCount() == MAX_GROUP_SIZE || !GetPlayer()->GetGroup()->isLFGGroup())))
        return;

    if (dfJoin.Slots.empty())
    {
        TC_LOG_DEBUG("lfg", "CMSG_DF_JOIN {} no dungeons selected", GetPlayerInfo());
        return;
    }

    lfg::LfgDungeonSet newDungeons;
    for (uint32 slot : dfJoin.Slots)
    {
        uint32 dungeon = slot & 0x00FFFFFF;
        if (sLFGDungeonsStore.LookupEntry(dungeon))
            newDungeons.insert(dungeon);
    }

    TC_LOG_DEBUG("lfg", "CMSG_DF_JOIN {} roles: {}, Dungeons: {}", GetPlayerInfo(), dfJoin.Roles, uint8(newDungeons.size()));

    sLFGMgr->JoinLfg(GetPlayer(), uint8(dfJoin.Roles), newDungeons);
}

void WorldSession::HandleLfgLeaveOpcode(WorldPackets::LFG::DFLeave& dfLeave)
{
    Group* group = GetPlayer()->GetGroup();

    TC_LOG_DEBUG("lfg", "CMSG_DF_LEAVE {} in group: {} sent guid {}.",
        GetPlayerInfo(), group ? 1 : 0, dfLeave.Ticket.RequesterGuid.ToString());

    // Check cheating - only leader can leave the queue
    if (!group || group->GetLeaderGUID() == dfLeave.Ticket.RequesterGuid)
        sLFGMgr->LeaveLfg(dfLeave.Ticket.RequesterGuid);
}

void WorldSession::HandleLfgProposalResultOpcode(WorldPackets::LFG::DFProposalResponse& dfProposalResponse)
{
    TC_LOG_DEBUG("lfg", "CMSG_LFG_PROPOSAL_RESULT {} proposal: {} accept: {}",
        GetPlayerInfo(), dfProposalResponse.ProposalID, dfProposalResponse.Accepted ? 1 : 0);
    sLFGMgr->UpdateProposal(dfProposalResponse.ProposalID, GetPlayer()->GetGUID(), dfProposalResponse.Accepted);
}

void WorldSession::HandleLfgSetRolesOpcode(WorldPackets::LFG::DFSetRoles& dfSetRoles)
{
    ObjectGuid guid = GetPlayer()->GetGUID();
    Group* group = GetPlayer()->GetGroup();
    if (!group)
    {
        TC_LOG_DEBUG("lfg", "CMSG_DF_SET_ROLES {} Not in group",
            GetPlayerInfo());
        return;
    }
    ObjectGuid gguid = group->GetGUID();
    TC_LOG_DEBUG("lfg", "CMSG_DF_SET_ROLES: Group {}, Player {}, Roles: {}",
        gguid.ToString(), GetPlayerInfo(), dfSetRoles.RolesDesired);
    sLFGMgr->UpdateRoleCheck(gguid, guid, dfSetRoles.RolesDesired);
}

void WorldSession::HandleLfgSetBootVoteOpcode(WorldPackets::LFG::DFBootPlayerVote& dfBootPlayerVote)
{
    ObjectGuid guid = GetPlayer()->GetGUID();
    TC_LOG_DEBUG("lfg", "CMSG_LFG_SET_BOOT_VOTE {} agree: {}",
        GetPlayerInfo(), dfBootPlayerVote.Vote ? 1 : 0);
    sLFGMgr->UpdateBoot(guid, dfBootPlayerVote.Vote);
}

void WorldSession::HandleLfgTeleportOpcode(WorldPackets::LFG::DFTeleport& dfTeleport)
{
    TC_LOG_DEBUG("lfg", "CMSG_DF_TELEPORT {} out: {}",
        GetPlayerInfo(), dfTeleport.TeleportOut ? 1 : 0);
    sLFGMgr->TeleportPlayer(GetPlayer(), dfTeleport.TeleportOut, true);
}

void WorldSession::HandleDFGetSystemInfo(WorldPackets::LFG::DFGetSystemInfo& dfGetSystemInfo)
{
    TC_LOG_DEBUG("lfg", "CMSG_DF_GET_SYSTEM_INFO {} for {}", GetPlayerInfo(), (dfGetSystemInfo.Player ? "player" : "party"));

    if (dfGetSystemInfo.Player)
        SendLfgPlayerLockInfo();
    else
        SendLfgPartyLockInfo();

    // SMSG_SET_DF_FAST_LAUNCH_RESULT is an unsolicited push with no request opcode, so the server has to
    // pick its own moment. This one is the earliest point at which the group finder UI is provably up and
    // listening: the client only issues CMSG_DF_GET_SYSTEM_INFO when it is building the LFD panel.
    // Sent only when the option is on - the client global defaults to 0, so staying quiet is the correct
    // no-op and matches the state of every character today.
    // UNVERIFIED: retail's own trigger for this push. Nothing in the client image reveals it.
    if (dfGetSystemInfo.Player && sConfigMgr->GetBoolDefault("DungeonFinder.FastLaunch", false))
        SendSetDFFastLaunchResult(true);
}

void WorldSession::HandleDFConfirmExpandSearch(WorldPackets::LFG::DFConfirmExpandSearch& dfConfirmExpandSearch)
{
    TC_LOG_DEBUG("lfg", "CMSG_DF_CONFIRM_EXPAND_SEARCH {} accepted: {}", GetPlayerInfo(), dfConfirmExpandSearch.Accepted);

    // A decline never reaches us - the client's LFG_QUEUE_EXPAND popup wires only its accept button to
    // C_LFGInfo.ConfirmLfgExpandSearch (LFGFrame.lua:88-97). Honour the bit anyway rather than assuming it,
    // and do nothing on a false: the prompt is one-shot per queue entry, so a decline simply leaves the
    // queue as it was.
    // UNVERIFIED: "one-shot per queue entry" is this server's send policy, not a measured one - see the
    // docblock of LFGMgr::UpdateExpandSearchPrompts. Reading the bit is not affected by that; the bit is on
    // the wire either way (client serializer RVA 0x6A40E0).
    if (!dfConfirmExpandSearch.Accepted)
        return;

    sLFGMgr->ConfirmExpandSearch(GetPlayer(), dfConfirmExpandSearch.Ticket);
}

// CMSG_DF_READY_CHECK_RESPONSE (0x430048). Sent by the global Lua binding CompleteLFGReadyCheck(isReady)
// (thunk RVA 0x24CCA50 -> body 0x24CC200), and only while a readiness check is actually running - the
// client guards the send with `cmp byte [0x4D195C9], 2`, i.e. against the ReadyCheckStatus it last got
// from SMSG_LFG_READY_CHECK_UPDATE. PartyIndex is that message's first uint8 echoed straight back.
//
// INERT TODAY, and knowingly so (audit 2026-08-27, lfg_5A round 4, finding 2): nothing in this tree starts
// a readiness check, so no client is ever in the state that lets it send this, and UpdateReadyCheck below
// returns from its first `if` for anything that arrives anyway. That is a D2 gap, not a finished opcode.
// The reasoning, the prohibition on inventing a trigger, and the hand-off to family 0x3E
// (CMSG_BATTLEMASTER_JOIN_SKIRMISH 0x3E00C1) are in the docblock of LFGMgr::StartReadyCheck.
void WorldSession::HandleDFReadyCheckResponse(WorldPackets::LFG::DFReadyCheckResponse& dfReadyCheckResponse)
{
    TC_LOG_DEBUG("lfg", "CMSG_DF_READY_CHECK_RESPONSE {} isReady: {}", GetPlayerInfo(), dfReadyCheckResponse.IsReady);

    Player* player = GetPlayer();
    if (!player)
        return;

    Group const* group = player->GetGroup();
    if (!group)
        return;

    sLFGMgr->UpdateReadyCheck(group->GetGUID(), player->GetGUID(), dfReadyCheckResponse.IsReady);
}

// CMSG_LFG_LOREWALKING_UPDATE_REQUEST (0x3D0259) -> SMSG_LFG_SUSPEND_LOREWALKING (0x5A0021).
//
// The client sends this with no Lua call at all: it arises inside the queue-status path (send site
// RVA 0x24C15B9 in 0x24C0A70), which looks up the LFGDungeons record for Slots[0] & 0xFFFFF and puts a
// 16-bit column of it on the wire. The request means "I want to queue for this dungeon - is Lorewalking
// in my way?".
//
// The answer bit is INVERTED relative to the opcode name (consumer RVA 0x24C50A0, four instructions):
//   Suspend == 1 -> silence, the player may queue
//   Suspend == 0 -> GameError 832 ERR_LFG_LOREWALKING, the queue attempt is refused
//
// TrinityCore has no Lorewalking content, and - this is the honest version of an earlier claim here - it
// has nowhere to keep the state either. CHARACTER_FLAG_4_LOGGED_OUT_WHILE_LOREWALKING (0x40) exists as an
// enumerator in SharedDefines.h and nothing more: no CharacterFlags4 field on Player, no column on the
// characters table, not one reference anywhere under src/server. So there is no flag to read, and the
// refusal path CANNOT be exercised on this tree today. Only the success path is live.
// What is built is the branch, funnelled through IsLorewalkingActive() below so that a later Lorewalking
// implementation gives that one predicate a body and changes nothing else in this handler.
//
// Where the data for such an implementation would live: NOT in a Lorewalking DB2 and not in a column of
// LFGDungeons, LFGDungeonGroup, GroupFinderCategory or ContentTuning (all four checked against the 12.1
// layouts; a case-insensitive sweep of every .dbd finds no lorewalking field anywhere). The single data
// anchor in the client is `lorewalkingCampaignID` at RVA 0x3C06260 - and that is not a CVar either: it
// has no code xref and appears only in the 18-entry field-name table of the JAM telemetry struct
// JamTelemetryCharacter, right beside chromieTimeID and timeRunningSeasonID. Lorewalking is a
// Campaign.db2 id, reported per character alongside the two other alternate-progression modes.
namespace
{
    // Is this character currently Lorewalking, so that queueing for `mapId` has to be refused?
    // Always false, and not by oversight: the tree has no storage for the state at all (see above). This is
    // the single place a Lorewalking implementation has to fill in - it would test the campaign
    // (`lorewalkingCampaignID`, a Campaign.db2 id) against the dungeon's map.
    // UNVERIFIED: retail's actual condition. No client evidence exists for it either - the client only reads
    // the answer bit, it never computes the state itself.
    //
    // ACCEPTANCE, held open on purpose (audit 2026-08-27, lfg_5A, D3 gap on SMSG_LFG_SUSPEND_LOREWALKING):
    // because this predicate returns a constant, only the SUCCESS answer
    // (Suspend = 1) can currently go out. The refusal (Suspend = 0 -> GameError 832 ERR_LFG_LOREWALKING) is
    // built and byte-correct but unreachable, and it stays that way until Lorewalking itself exists on this
    // tree. Do NOT close the gap by inventing a condition here: a wrong predicate would refuse legitimate
    // queue attempts, which is strictly worse than never refusing. The gap is recorded in
    // orchestrierung/status/lfg_5A.json under dod_luecken.D3 and must not be dropped at acceptance.
    bool IsLorewalkingActive(Player const* /*player*/, int32 /*mapId*/)
    {
        return false;
    }
}

void WorldSession::HandleLFGLorewalkingUpdateRequest(WorldPackets::LFG::LFGLorewalkingUpdateRequest& lfgLorewalkingUpdateRequest)
{
    TC_LOG_DEBUG("lfg", "CMSG_LFG_LOREWALKING_UPDATE_REQUEST {} mapId: {}", GetPlayerInfo(), lfgLorewalkingUpdateRequest.MapID);

    Player const* player = GetPlayer();
    if (!player)
        return;

    // The payload is LFGDungeons.MapID of the slot the player wants to queue for (see LFGPackets.h for the
    // DB2Meta walk that establishes it) - the map a Lorewalking implementation has to test the campaign
    // against. Suspend is INVERTED: true lets the player queue, false raises GameError 832.
    SendLfgSuspendLorewalking(!IsLorewalkingActive(player, lfgLorewalkingUpdateRequest.MapID));
}

// SMSG_LFG_SUSPEND_LOREWALKING (0x5A0021) - one byte, bit 7.
void WorldSession::SendLfgSuspendLorewalking(bool suspend)
{
    TC_LOG_DEBUG("lfg", "SMSG_LFG_SUSPEND_LOREWALKING {} suspend: {}", GetPlayerInfo(), suspend);

    WorldPackets::LFG::LFGSuspendLorewalking lfgSuspendLorewalking;
    lfgSuspendLorewalking.Suspend = suspend;
    SendPacket(lfgSuspendLorewalking.Write());
}

// SMSG_SET_DF_FAST_LAUNCH_RESULT (0x5A0012) - one byte, bit 7. Unsolicited server push; there is no
// CMSG_SET_DF_FAST_LAUNCH, neither in TrinityCore nor in the client (see LFGPackets.h).
// The flag bypasses an unlock gate on LFG activity entries client-side
// (`(flags & 0x1000) == 0 || lfgFastLaunch || ...` in the Lua getter binding RVA 0x24D4ED0).
void WorldSession::SendSetDFFastLaunchResult(bool lfgFastLaunch)
{
    TC_LOG_DEBUG("lfg", "SMSG_SET_DF_FAST_LAUNCH_RESULT {} lfgFastLaunch: {}", GetPlayerInfo(), lfgFastLaunch);

    WorldPackets::LFG::SetDFFastLaunchResult setDFFastLaunchResult;
    setDFFastLaunchResult.LfgFastLaunch = lfgFastLaunch;
    SendPacket(setDFFastLaunchResult.Write());
}

// SMSG_LFG_READY_CHECK_UPDATE (0x5A0006).
// Unreachable today: its only sender, LFGMgr::StartReadyCheck, has no caller. D2 is OPEN - see that
// function's docblock and lfg_5A.json, dod_luecken.D2.
void WorldSession::SendLfgReadyCheckUpdate(lfg::LfgReadyCheck const& readyCheck)
{
    TC_LOG_DEBUG("lfg", "SMSG_LFG_READY_CHECK_UPDATE {} state: {}", GetPlayerInfo(), readyCheck.state);

    WorldPackets::LFG::LFGReadyCheckUpdate lfgReadyCheckUpdate;
    // The client only ever echoes this byte back in CMSG_DF_READY_CHECK_RESPONSE; TrinityCore uses 127 as
    // the "no party index" value for the sister opcode SMSG_LFG_ROLE_CHECK_UPDATE and StartReadyCheck
    // defaults to the same.
    lfgReadyCheckUpdate.PartyIndex = readyCheck.partyIndex;
    lfgReadyCheckUpdate.ReadyCheckStatus = readyCheck.state;
    lfgReadyCheckUpdate.BgQueueIDs = readyCheck.bgQueueIDs;
    lfgReadyCheckUpdate.IsRequeue = readyCheck.isRequeue;

    // Leader first, mirroring SendLfgRoleCheckUpdate - the client indexes the member list positionally
    // in several places and expects the leader at slot 0.
    lfg::LfgAnswerContainer::const_iterator itLeader = readyCheck.answers.find(readyCheck.leader);
    if (itLeader != readyCheck.answers.end())
        lfgReadyCheckUpdate.Members.emplace_back(itLeader->first, itLeader->second == lfg::LFG_ANSWER_AGREE);

    for (lfg::LfgAnswerContainer::value_type const& answer : readyCheck.answers)
    {
        if (answer.first == readyCheck.leader)
            continue;

        lfgReadyCheckUpdate.Members.emplace_back(answer.first, answer.second == lfg::LFG_ANSWER_AGREE);
    }

    SendPacket(lfgReadyCheckUpdate.Write());
}

// SMSG_LFG_READY_CHECK_RESULT (0x5A001E). Unreachable today for the same reason as its sister message
// above: no running check means no answer to report. D2 is OPEN - see LFGMgr::StartReadyCheck.
// Both flanks are required (D3): a false additionally raises
// GameError 831 ERR_LFG_PLAYER_DECLINED_READY_CHECK client-side and fires LFG_READY_CHECK_DECLINED
// instead of LFG_READY_CHECK_PLAYER_IS_READY.
void WorldSession::SendLfgReadyCheckResult(ObjectGuid guid, bool ready)
{
    TC_LOG_DEBUG("lfg", "SMSG_LFG_READY_CHECK_RESULT {} player: {} ready: {}", GetPlayerInfo(), guid.ToString(), ready);

    WorldPackets::LFG::LFGReadyCheckResult lfgReadyCheckResult;
    lfgReadyCheckResult.Player = guid;
    lfgReadyCheckResult.Ready = ready;
    SendPacket(lfgReadyCheckResult.Write());
}

// SMSG_LFG_INSTANCE_SHUTDOWN_COUNTDOWN (0x5A0009).
//
// The consumer (RVA 0x24C18C0) is fully decoded: it formats TimeLeft with INT_GENERAL_DURATION into the
// GlobalString INSTANCE_SHUTDOWN_MESSAGE and prints it to the system chat.
//
// The only sender is LFGMgr::SendInstanceShutdownCountdown, and that one has NO caller: nothing in this
// tree shuts an instance down while players are still inside it, so there is nothing this message could
// truthfully announce. The full reasoning, the withdrawn round-2/3 trigger and the standing prohibition on
// inventing a substitute are in the docblock of LFGMgr::SendInstanceShutdownCountdown.
//
// UNVERIFIED - the retail trigger. That retail sends this message when an LFG instance closes under its
// occupants is inference from the consumer (a chat line built from INSTANCE_SHUTDOWN_MESSAGE), not from a
// capture. D2 is consequently OPEN for this opcode; see lfg_5A.json, dod_luecken.D2 and aufnahme_noetig.
void WorldSession::SendLfgInstanceShutdownCountdown(WorldPackets::LFG::RideTicket const& ticket, uint32 timeLeftSeconds)
{
    TC_LOG_DEBUG("lfg", "SMSG_LFG_INSTANCE_SHUTDOWN_COUNTDOWN {} timeLeft: {}", GetPlayerInfo(), timeLeftSeconds);

    WorldPackets::LFG::LFGInstanceShutdownCountdown lfgInstanceShutdownCountdown;
    lfgInstanceShutdownCountdown.Ticket = ticket;
    lfgInstanceShutdownCountdown.TimeLeft = timeLeftSeconds;
    SendPacket(lfgInstanceShutdownCountdown.Write());
}

void WorldSession::HandleDFGetJoinStatus(WorldPackets::LFG::DFGetJoinStatus& /*dfGetJoinStatus*/)
{
    TC_LOG_DEBUG("lfg", "CMSG_DF_GET_JOIN_STATUS {}", GetPlayerInfo());

    if (!GetPlayer()->isUsingLfg())
        return;

    ObjectGuid guid = GetPlayer()->GetGUID();
    lfg::LfgUpdateData updateData = sLFGMgr->GetLfgStatus(guid);

    if (GetPlayer()->GetGroup())
    {
        SendLfgUpdateStatus(updateData, true);
        updateData.dungeons.clear();
        SendLfgUpdateStatus(updateData, false);
    }
    else
    {
        SendLfgUpdateStatus(updateData, false);
        updateData.dungeons.clear();
        SendLfgUpdateStatus(updateData, true);
    }
}

void WorldSession::SendLfgPlayerLockInfo()
{
    TC_LOG_DEBUG("lfg", "SMSG_LFG_PLAYER_INFO {}", GetPlayerInfo());

    // Get Random dungeons that can be done at a certain level and expansion
    uint8 level = GetPlayer()->GetLevel();
    std::span<uint32 const> contentTuningReplacementConditionMask = GetPlayer()->m_playerData->CtrOptions->ConditionalFlags;
    lfg::LfgDungeonSet const& randomDungeons = sLFGMgr->GetRandomAndSeasonalDungeons(level, GetExpansion(), contentTuningReplacementConditionMask,
        uint32(GetPlayer()->m_playerData->CtrOptions->ChromieTimeExpansionMask));

    WorldPackets::LFG::LfgPlayerInfo lfgPlayerInfo;

    // Get player locked Dungeons
    for (auto const& lock : sLFGMgr->GetLockedDungeons(_player->GetGUID()))
        lfgPlayerInfo.BlackList.Slot.emplace_back(lock.first, lock.second.lockStatus, lock.second.requiredItemLevel, lock.second.currentItemLevel, 0);

    for (uint32 slot : randomDungeons)
    {
        lfgPlayerInfo.Dungeon.emplace_back();
        WorldPackets::LFG::LfgPlayerDungeonInfo& playerDungeonInfo = lfgPlayerInfo.Dungeon.back();
        playerDungeonInfo.Slot = slot;
        playerDungeonInfo.CompletionQuantity = 1;
        playerDungeonInfo.CompletionLimit = 1;
        playerDungeonInfo.CompletionCurrencyID = 0;
        playerDungeonInfo.SpecificQuantity = 0;
        playerDungeonInfo.SpecificLimit = 1;
        playerDungeonInfo.OverallQuantity = 0;
        playerDungeonInfo.OverallLimit = 1;
        playerDungeonInfo.PurseWeeklyQuantity = 0;
        playerDungeonInfo.PurseWeeklyLimit = 0;
        playerDungeonInfo.PurseQuantity = 0;
        playerDungeonInfo.PurseLimit = 0;
        playerDungeonInfo.Quantity = 1;
        playerDungeonInfo.CompletedMask = 0;
        playerDungeonInfo.EncounterMask = 0;

        if (lfg::LfgReward const* reward = sLFGMgr->GetRandomDungeonReward(slot, level))
        {
            if (Quest const* quest = sObjectMgr->GetQuestTemplate(reward->firstQuest))
            {
                playerDungeonInfo.FirstReward = GetPlayer()->CanRewardQuest(quest, false);
                if (!playerDungeonInfo.FirstReward)
                    quest = sObjectMgr->GetQuestTemplate(reward->otherQuest);

                if (quest)
                {
                    playerDungeonInfo.Rewards.RewardMoney = _player->GetQuestMoneyReward(quest);
                    playerDungeonInfo.Rewards.RewardXP = _player->GetQuestXPReward(quest);
                    for (uint8 i = 0; i < QUEST_REWARD_ITEM_COUNT; ++i)
                        if (uint32 itemId = quest->RewardItemId[i])
                            playerDungeonInfo.Rewards.Item.emplace_back(itemId, quest->RewardItemCount[i]);

                    for (uint32 i = 0; i < QUEST_REWARD_CURRENCY_COUNT; ++i)
                        if (uint32 curencyId = quest->RewardCurrencyId[i])
                            playerDungeonInfo.Rewards.Currency.emplace_back(curencyId, quest->RewardCurrencyCount[i]);
                }
            }
        }
    }

    SendPacket(lfgPlayerInfo.Write());;
}

void WorldSession::SendLfgPartyLockInfo()
{
    ObjectGuid guid = GetPlayer()->GetGUID();
    Group* group = GetPlayer()->GetGroup();
    if (!group)
        return;

    WorldPackets::LFG::LfgPartyInfo lfgPartyInfo;

    // Get the locked dungeons of the other party members
    for (GroupReference const& itr : group->GetMembers())
    {
        Player* plrg = itr.GetSource();
        ObjectGuid pguid = plrg->GetGUID();
        if (pguid == guid)
            continue;

        lfgPartyInfo.Player.emplace_back();
        WorldPackets::LFG::LFGBlackList& lfgBlackList = lfgPartyInfo.Player.back();
        lfgBlackList.PlayerGuid = pguid;
        for (auto const& lock : sLFGMgr->GetLockedDungeons(pguid))
            lfgBlackList.Slot.emplace_back(lock.first, lock.second.lockStatus, lock.second.requiredItemLevel, lock.second.currentItemLevel, 0);
    }

    TC_LOG_DEBUG("lfg", "SMSG_LFG_PARTY_INFO {}", GetPlayerInfo());
    SendPacket(lfgPartyInfo.Write());;
}

void WorldSession::SendLfgUpdateStatus(lfg::LfgUpdateData const& updateData, bool party)
{
    bool join = false;
    bool queued = false;

    switch (updateData.updateType)
    {
        case lfg::LFG_UPDATETYPE_JOIN_QUEUE_INITIAL:            // Joined queue outside the dungeon
            join = true;
            break;
        case lfg::LFG_UPDATETYPE_JOIN_QUEUE:
        case lfg::LFG_UPDATETYPE_ADDED_TO_QUEUE:                // Rolecheck Success
            join = true;
            queued = true;
            break;
        case lfg::LFG_UPDATETYPE_PROPOSAL_BEGIN:
            join = true;
            break;
        case lfg::LFG_UPDATETYPE_UPDATE_STATUS:
            join = updateData.state != lfg::LFG_STATE_ROLECHECK && updateData.state != lfg::LFG_STATE_NONE;
            queued = updateData.state == lfg::LFG_STATE_QUEUED;
            break;
        default:
            break;
    }

    TC_LOG_DEBUG("lfg", "SMSG_LFG_UPDATE_STATUS {} updatetype: {}, party {}",
        GetPlayerInfo(), updateData.updateType, party ? "true" : "false");

    WorldPackets::LFG::LFGUpdateStatus lfgUpdateStatus;
    if (WorldPackets::LFG::RideTicket const* ticket = sLFGMgr->GetTicket(_player->GetGUID()))
        lfgUpdateStatus.Ticket = *ticket;

    lfgUpdateStatus.SubType = lfg::LFG_QUEUE_DUNGEON; // other types not implemented
    lfgUpdateStatus.Reason = updateData.updateType;
    std::transform(updateData.dungeons.begin(), updateData.dungeons.end(), std::back_inserter(lfgUpdateStatus.Slots), [](uint32 dungeonId)
    {
        return sLFGMgr->GetLFGDungeonEntry(dungeonId);
    });
    lfgUpdateStatus.RequestedRoles = sLFGMgr->GetRoles(_player->GetGUID());
    //lfgUpdateStatus.SuspendedPlayers;
    lfgUpdateStatus.IsParty = party;
    lfgUpdateStatus.NotifyUI = true;
    lfgUpdateStatus.Joined = join;
    lfgUpdateStatus.LfgJoined = updateData.updateType != lfg::LFG_UPDATETYPE_REMOVED_FROM_QUEUE;
    lfgUpdateStatus.Queued = queued;
    lfgUpdateStatus.QueueMapID = sLFGMgr->GetDungeonMapId(_player->GetGUID());

    SendPacket(lfgUpdateStatus.Write());
}

void WorldSession::SendLfgRoleChosen(ObjectGuid guid, uint8 roles)
{
    TC_LOG_DEBUG("lfg", "SMSG_LFG_ROLE_CHOSEN {} guid: {} roles: {}",
        GetPlayerInfo(), guid.ToString(), roles);

    WorldPackets::LFG::RoleChosen roleChosen;
    roleChosen.Player = guid;
    roleChosen.RoleMask = roles;
    roleChosen.Accepted = roles > 0;
    SendPacket(roleChosen.Write());
}

void WorldSession::SendLfgRoleCheckUpdate(lfg::LfgRoleCheck const& roleCheck)
{
    lfg::LfgDungeonSet dungeons;
    if (roleCheck.rDungeonId)
        dungeons.insert(roleCheck.rDungeonId);
    else
        dungeons = roleCheck.dungeons;

    TC_LOG_DEBUG("lfg", "SMSG_LFG_ROLE_CHECK_UPDATE {}", GetPlayerInfo());
    WorldPackets::LFG::LFGRoleCheckUpdate lfgRoleCheckUpdate;
    lfgRoleCheckUpdate.PartyIndex = 127;
    lfgRoleCheckUpdate.RoleCheckStatus = roleCheck.state;
    std::transform(dungeons.begin(), dungeons.end(), std::back_inserter(lfgRoleCheckUpdate.JoinSlots), [](uint32 dungeonId)
    {
        return sLFGMgr->GetLFGDungeonEntry(dungeonId);
    });
    lfgRoleCheckUpdate.GroupFinderActivityID = 0;
    if (!roleCheck.roles.empty())
    {
        // Leader info MUST be sent 1st :S
        uint8 roles = roleCheck.roles.find(roleCheck.leader)->second;
        lfgRoleCheckUpdate.Members.emplace_back(roleCheck.leader, roles, ASSERT_NOTNULL(sCharacterCache->GetCharacterCacheByGuid(roleCheck.leader))->Level, roles > 0);

        for (lfg::LfgRolesMap::const_iterator it = roleCheck.roles.begin(); it != roleCheck.roles.end(); ++it)
        {
            if (it->first == roleCheck.leader)
                continue;

            roles = it->second;
            lfgRoleCheckUpdate.Members.emplace_back(it->first, roles, ASSERT_NOTNULL(sCharacterCache->GetCharacterCacheByGuid(it->first))->Level, roles > 0);
        }
    }

    SendPacket(lfgRoleCheckUpdate.Write());
}

void WorldSession::SendLfgJoinResult(lfg::LfgJoinResultData const& joinData)
{
    TC_LOG_DEBUG("lfg", "SMSG_LFG_JOIN_RESULT {} checkResult: {} checkValue: {}",
        GetPlayerInfo(), joinData.result, joinData.state);

    WorldPackets::LFG::LFGJoinResult lfgJoinResult;
    if (WorldPackets::LFG::RideTicket const* ticket = sLFGMgr->GetTicket(GetPlayer()->GetGUID()))
        lfgJoinResult.Ticket = *ticket;
    lfgJoinResult.Result = joinData.result;
    if (joinData.result == lfg::LFG_JOIN_ROLE_CHECK_FAILED)
        lfgJoinResult.ResultDetail = joinData.state;
    else if (joinData.result == lfg::LFG_JOIN_NO_SLOTS)
        lfgJoinResult.BlackListNames = joinData.playersMissingRequirement;

    for (lfg::LfgLockPartyMap::const_iterator it = joinData.lockmap.begin(); it != joinData.lockmap.end(); ++it)
    {
        lfgJoinResult.BlackList.emplace_back();
        WorldPackets::LFG::LFGBlackList& blackList = lfgJoinResult.BlackList.back();
        blackList.PlayerGuid = it->first;

        for (lfg::LfgLockMap::const_iterator itr = it->second.begin(); itr != it->second.end(); ++itr)
        {
            TC_LOG_TRACE("lfg", "SendLfgJoinResult:: {} DungeonID: {} Lock status: {} Required itemLevel: {} Current itemLevel: {}",
                it->first.ToString(), (itr->first & 0x00FFFFFF), itr->second.lockStatus, itr->second.requiredItemLevel, itr->second.currentItemLevel);

            blackList.Slot.emplace_back(itr->first, itr->second.lockStatus, itr->second.requiredItemLevel, itr->second.currentItemLevel, 0);
        }
    }

    SendPacket(lfgJoinResult.Write());
}

void WorldSession::SendLfgQueueStatus(lfg::LfgQueueStatusData const& queueData)
{
    TC_LOG_DEBUG("lfg", "SMSG_LFG_QUEUE_STATUS {} state: {}, dungeon: {}, waitTime: {}, "
        "avgWaitTime: {}, waitTimeTanks: {}, waitTimeHealer: {}, waitTimeDps: {}, "
        "queuedTime: {}, tanks: {}, healers: {}, dps: {}",
        GetPlayerInfo(), lfg::GetStateString(sLFGMgr->GetState(GetPlayer()->GetGUID())), queueData.dungeonId, queueData.waitTime, queueData.waitTimeAvg,
        queueData.waitTimeTank, queueData.waitTimeHealer, queueData.waitTimeDps,
        queueData.queuedTime, queueData.tanks, queueData.healers, queueData.dps);

    WorldPackets::LFG::LFGQueueStatus lfgQueueStatus;
    if (WorldPackets::LFG::RideTicket const* ticket = sLFGMgr->GetTicket(GetPlayer()->GetGUID()))
        lfgQueueStatus.Ticket = *ticket;
    lfgQueueStatus.Slot = sLFGMgr->GetLFGDungeonEntry(queueData.dungeonId);
    lfgQueueStatus.AvgWaitTimeMe = queueData.waitTime;
    lfgQueueStatus.AvgWaitTime = queueData.waitTimeAvg;
    lfgQueueStatus.AvgWaitTimeByRole[0] = queueData.waitTimeTank;
    lfgQueueStatus.AvgWaitTimeByRole[1] = queueData.waitTimeHealer;
    lfgQueueStatus.AvgWaitTimeByRole[2] = queueData.waitTimeDps;
    lfgQueueStatus.LastNeeded[0] = queueData.tanks;
    lfgQueueStatus.LastNeeded[1] = queueData.healers;
    lfgQueueStatus.LastNeeded[2] = queueData.dps;
    lfgQueueStatus.QueuedTime = queueData.queuedTime;
    SendPacket(lfgQueueStatus.Write());
}

void WorldSession::SendLfgPlayerReward(lfg::LfgPlayerRewardData const& rewardData)
{
    if (!rewardData.rdungeonEntry || !rewardData.sdungeonEntry || !rewardData.quest)
        return;

    TC_LOG_DEBUG("lfg", "SMSG_LFG_PLAYER_REWARD {} rdungeonEntry: {}, sdungeonEntry: {}, done: {}",
        GetPlayerInfo(), rewardData.rdungeonEntry, rewardData.sdungeonEntry, rewardData.done);

    WorldPackets::LFG::LFGPlayerReward lfgPlayerReward;
    lfgPlayerReward.QueuedSlot = rewardData.rdungeonEntry;
    lfgPlayerReward.ActualSlot = rewardData.sdungeonEntry;
    lfgPlayerReward.RewardMoney = GetPlayer()->GetQuestMoneyReward(rewardData.quest);
    lfgPlayerReward.AddedXP = GetPlayer()->GetQuestXPReward(rewardData.quest);

    for (uint8 i = 0; i < QUEST_REWARD_ITEM_COUNT; ++i)
        if (uint32 itemId = rewardData.quest->RewardItemId[i])
            lfgPlayerReward.Rewards.emplace_back(itemId, rewardData.quest->RewardItemCount[i], 0, false);

    for (uint32 i = 0; i < QUEST_REWARD_CURRENCY_COUNT; ++i)
        if (uint32 curencyId = rewardData.quest->RewardCurrencyId[i])
            lfgPlayerReward.Rewards.emplace_back(curencyId, rewardData.quest->RewardCurrencyCount[i], 0, true);

    SendPacket(lfgPlayerReward.Write());
}

void WorldSession::SendLfgBootProposalUpdate(lfg::LfgPlayerBoot const& boot)
{
    lfg::LfgAnswer playerVote = boot.votes.find(GetPlayer()->GetGUID())->second;
    uint8 votesNum = 0;
    uint8 agreeNum = 0;
    int32 secsleft = int32((boot.cancelTime - GameTime::GetGameTime()) / 1000);
    for (const auto& vote : boot.votes)
    {
        if (vote.second != lfg::LFG_ANSWER_PENDING)
        {
            ++votesNum;
            if (vote.second == lfg::LFG_ANSWER_AGREE)
                ++agreeNum;
        }
    }
    TC_LOG_DEBUG("lfg", "SMSG_LFG_BOOT_PROPOSAL_UPDATE {} inProgress: {} - "
        "didVote: {} - agree: {} - victim: {} votes: {} - agrees: {} - left: {} - "
        "needed: {} - reason {}",
        GetPlayerInfo(), uint8(boot.inProgress), uint8(playerVote != lfg::LFG_ANSWER_PENDING),
        uint8(playerVote == lfg::LFG_ANSWER_AGREE), boot.victim.ToString(), votesNum, agreeNum,
        secsleft, lfg::LFG_GROUP_KICK_VOTES_NEEDED, boot.reason);

    WorldPackets::LFG::LfgBootPlayer lfgBootPlayer;
    lfgBootPlayer.Info.VoteInProgress = boot.inProgress;
    lfgBootPlayer.Info.VotePassed = agreeNum >= lfg::LFG_GROUP_KICK_VOTES_NEEDED;
    lfgBootPlayer.Info.MyVoteCompleted = playerVote != lfg::LFG_ANSWER_PENDING;
    lfgBootPlayer.Info.MyVote = playerVote == lfg::LFG_ANSWER_AGREE;
    lfgBootPlayer.Info.Target = boot.victim;
    lfgBootPlayer.Info.TotalVotes = votesNum;
    lfgBootPlayer.Info.BootVotes = agreeNum;
    lfgBootPlayer.Info.TimeLeft = secsleft;
    lfgBootPlayer.Info.VotesNeeded = lfg::LFG_GROUP_KICK_VOTES_NEEDED;
    lfgBootPlayer.Info.Reason = boot.reason;
    SendPacket(lfgBootPlayer.Write());
}

void WorldSession::SendLfgUpdateProposal(lfg::LfgProposal const& proposal)
{
    ObjectGuid guid = GetPlayer()->GetGUID();
    ObjectGuid gguid = proposal.players.find(guid)->second.group;
    bool silent = !proposal.isNew && gguid == proposal.group;
    uint32 dungeonEntry = proposal.dungeonId;

    TC_LOG_DEBUG("lfg", "SMSG_LFG_PROPOSAL_UPDATE {} state: {}",
        GetPlayerInfo(), proposal.state);

    // show random dungeon if player selected random dungeon and it's not lfg group
    if (!silent)
    {
        lfg::LfgDungeonSet const& playerDungeons = sLFGMgr->GetSelectedDungeons(guid);
        if (playerDungeons.find(proposal.dungeonId) == playerDungeons.end())
            dungeonEntry = (*playerDungeons.begin());
    }

    WorldPackets::LFG::LFGProposalUpdate lfgProposalUpdate;
    if (WorldPackets::LFG::RideTicket const* ticket = sLFGMgr->GetTicket(GetPlayer()->GetGUID()))
        lfgProposalUpdate.Ticket = *ticket;
    lfgProposalUpdate.InstanceID = 0;
    lfgProposalUpdate.ProposalID = proposal.id;
    lfgProposalUpdate.Slot = sLFGMgr->GetLFGDungeonEntry(dungeonEntry);
    lfgProposalUpdate.State = proposal.state;
    lfgProposalUpdate.CompletedMask = proposal.encounters;
    lfgProposalUpdate.ValidCompletedMask = true;
    lfgProposalUpdate.ProposalSilent = silent;
    lfgProposalUpdate.FailedByMyParty = !proposal.isNew;

    for (auto const& player : proposal.players)
    {
        lfgProposalUpdate.Players.emplace_back();
        auto& proposalPlayer = lfgProposalUpdate.Players.back();
        proposalPlayer.Roles = player.second.role;
        proposalPlayer.Me = player.first == guid;
        proposalPlayer.MyParty = !player.second.group.IsEmpty() && player.second.group == proposal.group;
        proposalPlayer.SameParty = !player.second.group.IsEmpty() && player.second.group == gguid;
        proposalPlayer.Responded = player.second.accept != lfg::LFG_ANSWER_PENDING;
        proposalPlayer.Accepted = player.second.accept == lfg::LFG_ANSWER_AGREE;
    }

    SendPacket(lfgProposalUpdate.Write());
}

void WorldSession::SendLfgDisabled()
{
    TC_LOG_DEBUG("lfg", "SMSG_LFG_DISABLED {}", GetPlayerInfo());
    SendPacket(WorldPackets::LFG::LFGDisabled().Write());
}

void WorldSession::SendLfgOfferContinue(uint32 dungeonEntry)
{
    TC_LOG_DEBUG("lfg", "SMSG_LFG_OFFER_CONTINUE {} dungeon entry: {}",
        GetPlayerInfo(), dungeonEntry);
    SendPacket(WorldPackets::LFG::LFGOfferContinue(sLFGMgr->GetLFGDungeonEntry(dungeonEntry)).Write());
}

void WorldSession::SendLfgTeleportError(lfg::LfgTeleportResult err)
{
    TC_LOG_DEBUG("lfg", "SMSG_LFG_TELEPORT_DENIED {} reason: {}",
        GetPlayerInfo(), err);
    SendPacket(WorldPackets::LFG::LFGTeleportDenied(err).Write());
}

void WorldSession::SendLfgExpandSearchPrompt(WorldPackets::LFG::RideTicket const& ticket)
{
    TC_LOG_DEBUG("lfg", "SMSG_LFG_EXPAND_SEARCH_PROMPT {}", GetPlayerInfo());

    WorldPackets::LFG::LFGExpandSearchPrompt expandSearchPrompt;
    expandSearchPrompt.Ticket = ticket;
    SendPacket(expandSearchPrompt.Write());
}

void WorldSession::SendLfgSlotInvalid(lfg::LfgSlotInvalidReason reason, int32 subReason1, int32 subReason2)
{
    TC_LOG_DEBUG("lfg", "SMSG_LFG_SLOT_INVALID {} reason: {}, sub: {}/{}",
        GetPlayerInfo(), uint32(reason), subReason1, subReason2);

    WorldPackets::LFG::LFGSlotInvalid slotInvalid;
    slotInvalid.Reason = uint32(reason);
    slotInvalid.SubReason1 = subReason1;
    slotInvalid.SubReason2 = subReason2;
    SendPacket(slotInvalid.Write());
}
