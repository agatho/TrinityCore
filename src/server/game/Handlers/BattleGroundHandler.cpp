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
#include "ArenaTeam.h"
#include "ArenaTeamMgr.h"
#include "Battlefield.h"
#include "BattlefieldMgr.h"
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "BattlegroundPackets.h"
#include "Chat.h"
#include "Common.h"
#include "Creature.h"
#include "DB2Stores.h"
#include "DisableMgr.h"
#include "GameTime.h"
#include "Group.h"
#include "Language.h"
#include "Log.h"
#include "NPCPackets.h"
#include "Object.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "World.h"

// Sliding window plus burst, modelled on AuctionHouseMgr::CheckThrottle: the check itself answers the client
// rather than letting the caller return in silence. Returns false when the request must be refused.
// The window is a std::chrono TimePoint taken from GameTime::Now(), not a GameTime::GetGameTimeMS() millisecond
// counter, so there is no wrap to be safe about in the first place.
// The client's own registrar for SMSG_BATTLEGROUND_INFO_THROTTLED sits in its NPC interaction code (0x1E2F5D0),
// not in the PvP queue registrar block, which is what places the throttle here on battlemaster gossip rather
// than on the queue. What it displays is settled: consumer 0x1E20180 tail-calls ShowSystemMessage(0x2F8), and
// entry 760 of the client error table at 0x43D55C0 carries the string key ERR_BATTLEGROUND_INFO_THROTTLED -
// the one entry in all 1243 that names this opcode.
// UNVERIFIED: the window and burst are ours. No capture of any build contains this message, so retail's own
// limits are unknown; the defaults are picked to stay silent during normal play.
bool WorldSession::CheckBattlegroundInfoThrottle()
{
    uint32 period = sWorld->getIntConfig(CONFIG_BATTLEGROUND_INFO_THROTTLE_PERIOD);
    if (!period)
        return true;

    // A window is opened by the first request and by the first request after the previous one expired. The
    // initial TimePoint::min() makes the very first call take that branch, so the budget starts full.
    TimePoint now = GameTime::Now();
    if (now > _battlegroundInfoThrottlePeriodEnd)
    {
        _battlegroundInfoThrottlePeriodEnd = now + Milliseconds(period);
        _battlegroundInfoRequestsRemaining = sWorld->getIntConfig(CONFIG_BATTLEGROUND_INFO_THROTTLE_BURST);
    }

    if (!_battlegroundInfoRequestsRemaining)
    {
        SendPacket(WorldPackets::Battleground::BattlegroundInfoThrottled().Write());
        return false;
    }

    --_battlegroundInfoRequestsRemaining;
    return true;
}

void WorldSession::HandleBattlemasterHelloOpcode(WorldPackets::NPC::Hello& hello)
{
    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(hello.Unit, UNIT_NPC_FLAG_BATTLEMASTER, UNIT_NPC_FLAG_2_NONE);
    if (!unit)
        return;

    // Stop the npc if moving
    if (uint32 pause = unit->GetMovementTemplate().GetInteractionPauseTimer())
        unit->PauseMovement(pause);
    unit->SetHomePosition(unit->GetPosition());

    BattlegroundTypeId bgTypeId = sBattlegroundMgr->GetBattleMasterBG(unit->GetEntry());

    if (!_player->GetBGAccessByLevel(bgTypeId))
    {
                                                            // temp, must be gossip message...
        SendNotification(LANG_YOUR_BG_LEVEL_REQ_ERROR);
        return;
    }

    // Last gate before the list is rebuilt: on refusal the client gets the red ERR_BATTLEGROUND_INFO_THROTTLED
    // text instead of a silently missing window.
    if (!CheckBattlegroundInfoThrottle())
        return;

    sBattlegroundMgr->SendBattlegroundList(_player, hello.Unit, bgTypeId);
}

void WorldSession::HandleBattlemasterJoinOpcode(WorldPackets::Battleground::BattlemasterJoin& battlemasterJoin)
{
    bool isPremade = false;
    if (battlemasterJoin.QueueIDs.empty())
    {
        TC_LOG_ERROR("network", "Battleground: no bgtype received. possible cheater? {}", _player->GetGUID().ToString());
        return;
    }

    BattlegroundQueueTypeId bgQueueTypeId = BattlegroundQueueTypeId::FromPacked(battlemasterJoin.QueueIDs[0]);
    if (!BattlegroundMgr::IsValidQueueId(bgQueueTypeId))
    {
        TC_LOG_ERROR("network", "Battleground: invalid bg queue {{ BattlemasterListId: {}, Type: {}, Rated: {}, TeamSize: {} }} received. possible cheater? {}",
            bgQueueTypeId.BattlemasterListId, uint32(bgQueueTypeId.Type), bgQueueTypeId.Rated ? "true" : "false", uint32(bgQueueTypeId.TeamSize),
            _player->GetGUID().ToString());
        return;
    }

    BattlemasterListEntry const* battlemasterListEntry = sBattlemasterListStore.AssertEntry(bgQueueTypeId.BattlemasterListId);

    if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, bgQueueTypeId.BattlemasterListId, nullptr) || battlemasterListEntry->GetFlags().HasFlag(BattlemasterListFlags::InternalOnly))
    {
        ChatHandler(this).PSendSysMessage(LANG_BG_DISABLED);
        return;
    }

    BattlegroundTypeId bgTypeId = BattlegroundTypeId(bgQueueTypeId.BattlemasterListId);

    // ignore if player is already in BG
    if (_player->InBattleground())
        return;

    BattlegroundTemplate const* bgTemplate = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(bgTypeId);
    if (!bgTemplate)
        return;

    // expected bracket entry
    PVPDifficultyEntry const* bracketEntry = DB2Manager::GetBattlegroundBracketByLevel(bgTemplate->MapIDs.front(), _player->GetLevel());
    if (!bracketEntry)
        return;

    GroupJoinBattlegroundResult err = ERR_BATTLEGROUND_NONE;

    Group const* grp = _player->GetGroup();

    auto getQueueTeam = [&]() -> Team
    {
        // mercenary applies only to unrated battlegrounds
        if (!bgQueueTypeId.Rated && !bgTemplate->IsArena())
        {
            if (_player->HasAura(SPELL_MERCENARY_CONTRACT_HORDE))
                return HORDE;

            if (_player->HasAura(SPELL_MERCENARY_CONTRACT_ALLIANCE))
                return ALLIANCE;
        }

        return Team(_player->GetTeam());
    };

    // check queue conditions
    if (!grp)
    {
        if (GetPlayer()->isUsingLfg())
        {
            WorldPackets::Battleground::BattlefieldStatusFailed battlefieldStatus;
            BattlegroundMgr::BuildBattlegroundStatusFailed(&battlefieldStatus, bgQueueTypeId, _player, 0, ERR_LFG_CANT_USE_BATTLEGROUND);
            SendPacket(battlefieldStatus.Write());
            return;
        }

        // check RBAC permissions
        if (!_player->CanJoinToBattleground(bgTemplate))
        {
            WorldPackets::Battleground::BattlefieldStatusFailed battlefieldStatus;
            BattlegroundMgr::BuildBattlegroundStatusFailed(&battlefieldStatus, bgQueueTypeId, _player, 0, ERR_BATTLEGROUND_JOIN_TIMED_OUT);
            SendPacket(battlefieldStatus.Write());
            return;
        }

        // check Deserter debuff
        if (_player->IsDeserter())
        {
            WorldPackets::Battleground::BattlefieldStatusFailed battlefieldStatus;
            BattlegroundMgr::BuildBattlegroundStatusFailed(&battlefieldStatus, bgQueueTypeId, _player, 0, ERR_GROUP_JOIN_BATTLEGROUND_DESERTERS);
            SendPacket(battlefieldStatus.Write());
            return;
        }

        bool isInRandomBgQueue = _player->InBattlegroundQueueForBattlegroundQueueType(BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_RB, BattlegroundQueueIdType::Battleground, false, 0))
            || _player->InBattlegroundQueueForBattlegroundQueueType(BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_RANDOM_EPIC, BattlegroundQueueIdType::Battleground, false, 0));
        if (!BattlegroundMgr::IsRandomBattleground(bgTypeId) && isInRandomBgQueue)
        {
            // player is already in random queue
            WorldPackets::Battleground::BattlefieldStatusFailed battlefieldStatus;
            BattlegroundMgr::BuildBattlegroundStatusFailed(&battlefieldStatus, bgQueueTypeId, _player, 0, ERR_IN_RANDOM_BG);
            SendPacket(battlefieldStatus.Write());
            return;
        }

        if (_player->InBattlegroundQueue(true) && !isInRandomBgQueue && BattlegroundMgr::IsRandomBattleground(bgTypeId))
        {
            // player is already in queue, can't start random queue
            WorldPackets::Battleground::BattlefieldStatusFailed battlefieldStatus;
            BattlegroundMgr::BuildBattlegroundStatusFailed(&battlefieldStatus, bgQueueTypeId, _player, 0, ERR_IN_NON_RANDOM_BG);
            SendPacket(battlefieldStatus.Write());
            return;
        }

        // check if already in queue
        if (_player->GetBattlegroundQueueIndex(bgQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES)
            // player is already in this queue
            return;

        // check if has free queue slots
        if (!_player->HasFreeBattlegroundQueueId())
        {
            WorldPackets::Battleground::BattlefieldStatusFailed battlefieldStatus;
            BattlegroundMgr::BuildBattlegroundStatusFailed(&battlefieldStatus, bgQueueTypeId, _player, 0, ERR_BATTLEGROUND_TOO_MANY_QUEUES);
            SendPacket(battlefieldStatus.Write());
            return;
        }

        // check Freeze debuff
        if (_player->HasAura(9454))
            return;

        BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);
        // The parameter order is (…, bool isPremade, uint32 ArenaRating, uint32 MatchmakerRating, uint8 roles).
        // This call used to read (…, false, isPremade, 0): isPremade was landing in ArenaRating while the real
        // isPremade parameter was hardcoded false. Because AddGroup buckets with
        // `if (!m_queueId.Rated && !isPremade) index += PVP_TEAMS_COUNT`, every unrated group was filed under
        // BG_QUEUE_NORMAL_* and CheckPremadeMatch could never fire.
        GroupQueueInfo* ginfo = bgQueue.AddGroup(_player, nullptr, getQueueTeam(), bracketEntry, isPremade, 0, 0, battlemasterJoin.Roles);
        uint32 avgTime = bgQueue.GetAverageQueueWaitTime(ginfo, bracketEntry->GetBracketId());
        uint32 queueSlot = _player->AddBattlegroundQueueId(bgQueueTypeId);

        WorldPackets::Battleground::BattlefieldStatusQueued battlefieldStatus;
        BattlegroundMgr::BuildBattlegroundStatusQueued(&battlefieldStatus, _player, queueSlot, ginfo->JoinTime, bgQueueTypeId, avgTime, false);
        SendPacket(battlefieldStatus.Write());

        TC_LOG_DEBUG("bg.battleground", "Battleground: player joined queue for bg queue {{ BattlemasterListId: {}, Type: {}, Rated: {}, TeamSize: {} }}, {}, NAME {}",
            bgQueueTypeId.BattlemasterListId, uint32(bgQueueTypeId.Type), bgQueueTypeId.Rated ? "true" : "false", uint32(bgQueueTypeId.TeamSize),
            _player->GetGUID().ToString(), _player->GetName());
    }
    else
    {
        if (grp->GetLeaderGUID() != _player->GetGUID())
            return;

        ObjectGuid errorGuid;
        err = grp->CanJoinBattlegroundQueue(bgTemplate, bgQueueTypeId, 0, bgTemplate->GetMaxPlayersPerTeam(), false, 0, errorGuid);
        isPremade = (grp->GetMembersCount() >= bgTemplate->GetMinPlayersPerTeam());

        BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);
        GroupQueueInfo* ginfo = nullptr;
        uint32 avgTime = 0;

        if (!err)
        {
            TC_LOG_DEBUG("bg.battleground", "Battleground: the following players are joining as group:");
            // Same argument-order fix as the solo path above.
            ginfo = bgQueue.AddGroup(_player, grp, getQueueTeam(), bracketEntry, isPremade, 0, 0, battlemasterJoin.Roles);
            avgTime = bgQueue.GetAverageQueueWaitTime(ginfo, bracketEntry->GetBracketId());
        }

        for (GroupReference const& itr : grp->GetMembers())
        {
            Player* member = itr.GetSource();
            if (err)
            {
                WorldPackets::Battleground::BattlefieldStatusFailed battlefieldStatus;
                BattlegroundMgr::BuildBattlegroundStatusFailed(&battlefieldStatus, bgQueueTypeId, _player, 0, err, &errorGuid);
                member->SendDirectMessage(battlefieldStatus.Write());
                continue;
            }

            // add to queue
            uint32 queueSlot = member->AddBattlegroundQueueId(bgQueueTypeId);

            WorldPackets::Battleground::BattlefieldStatusQueued battlefieldStatus;
            BattlegroundMgr::BuildBattlegroundStatusQueued(&battlefieldStatus, member, queueSlot, ginfo->JoinTime, bgQueueTypeId, avgTime, true);
            member->SendDirectMessage(battlefieldStatus.Write());
            TC_LOG_DEBUG("bg.battleground", "Battleground: player joined queue for bg queue {{ BattlemasterListId: {}, Type: {}, Rated: {}, TeamSize: {} }}, {}, NAME {}",
                bgQueueTypeId.BattlemasterListId, uint32(bgQueueTypeId.Type), bgQueueTypeId.Rated ? "true" : "false", uint32(bgQueueTypeId.TeamSize),
                member->GetGUID().ToString(), member->GetName());
        }
        TC_LOG_DEBUG("bg.battleground", "Battleground: group end");
    }

    sBattlegroundMgr->ScheduleQueueUpdate(0, bgQueueTypeId, bracketEntry->GetBracketId());
}

// CMSG_BATTLEMASTER_JOIN_RATED_BG_BLITZ (0x3B00BE) - rated 8v8 solo/duo queue.
//
// Unlike CMSG_BATTLEMASTER_JOIN, this packet carries NO queue identity: the client sends a single role-mask
// byte and the mode is implied entirely by the opcode. The server therefore builds the queue id itself. The
// value used here is not invented - a live 12.0.7.68275 capture shows retail replying to this exact opcode
// with SMSG_BATTLEFIELD_STATUS_QUEUED carrying packed QueueID 0x1F1000000019044D, which decodes to
// { BattlemasterListId = 1101, Type = 9, Rated = true, TeamSize = 0 }.
void WorldSession::HandleBattlemasterJoinRatedBGBlitz(WorldPackets::Battleground::BattlemasterJoinRatedBGBlitz& packet)
{
    BattlegroundQueueTypeId bgQueueTypeId =
        BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_BLITZ, BattlegroundQueueIdType::RatedBattlegroundBlitz, true, 0);

    if (!BattlegroundMgr::IsValidQueueId(bgQueueTypeId))
    {
        TC_LOG_ERROR("network", "Battleground Blitz: queue id rejected by IsValidQueueId - BattlemasterList {} is missing from the client DB2 the server loaded.",
            uint32(BATTLEGROUND_BLITZ));
        return;
    }

    BattlemasterListEntry const* battlemasterListEntry = sBattlemasterListStore.AssertEntry(bgQueueTypeId.BattlemasterListId);

    if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, bgQueueTypeId.BattlemasterListId, nullptr) || battlemasterListEntry->GetFlags().HasFlag(BattlemasterListFlags::InternalOnly))
    {
        ChatHandler(this).PSendSysMessage(LANG_BG_DISABLED);
        return;
    }

    if (_player->InBattleground())
        return;

    BattlegroundTemplate const* bgTemplate = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BATTLEGROUND_BLITZ);
    if (!bgTemplate)
    {
        TC_LOG_ERROR("bg.battleground", "Battleground Blitz: no battleground_template row for {} - apply the Blitz world migration.", uint32(BATTLEGROUND_BLITZ));
        return;
    }

    PVPDifficultyEntry const* bracketEntry = DB2Manager::GetBattlegroundBracketByLevel(bgTemplate->MapIDs.front(), _player->GetLevel());
    if (!bracketEntry)
        return;

    auto sendFailed = [&](GroupJoinBattlegroundResult result)
    {
        WorldPackets::Battleground::BattlefieldStatusFailed battlefieldStatus;
        BattlegroundMgr::BuildBattlegroundStatusFailed(&battlefieldStatus, bgQueueTypeId, _player, 0, result);
        SendPacket(battlefieldStatus.Write());
    };

    // The retail client refuses to send this opcode with no role selected, so a zero mask means either a
    // modified client or a UI state we do not model. Either way the matchmaker cannot place the player.
    if (!packet.Roles)
    {
        sendFailed(ERR_BATTLEGROUND_JOIN_FAILED);
        return;
    }

    Group const* grp = _player->GetGroup();
    if (grp)
    {
        if (grp->GetLeaderGUID() != _player->GetGUID())
            return;

        // BattlemasterList 1101 caps the queueing party at MaxGroupSize (2 on retail - solo or duo).
        if (grp->GetMembersCount() > battlemasterListEntry->MaxGroupSize)
        {
            sendFailed(ERR_BATTLEGROUND_JOIN_FAILED);
            return;
        }
    }

    if (GetPlayer()->isUsingLfg())
    {
        sendFailed(ERR_LFG_CANT_USE_BATTLEGROUND);
        return;
    }

    if (!_player->CanJoinToBattleground(bgTemplate))
    {
        sendFailed(ERR_BATTLEGROUND_JOIN_TIMED_OUT);
        return;
    }

    if (_player->IsDeserter())
    {
        sendFailed(ERR_GROUP_JOIN_BATTLEGROUND_DESERTERS);
        return;
    }

    // already queued for Blitz
    if (_player->GetBattlegroundQueueIndex(bgQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES)
        return;

    if (!_player->HasFreeBattlegroundQueueId())
    {
        sendFailed(ERR_BATTLEGROUND_TOO_MANY_QUEUES);
        return;
    }

    // check Freeze debuff
    if (_player->HasAura(9454))
        return;

    BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);
    GroupQueueInfo* ginfo = bgQueue.AddGroup(_player, grp, Team(_player->GetTeam()), bracketEntry, false, 0, 0, packet.Roles);
    uint32 avgTime = bgQueue.GetAverageQueueWaitTime(ginfo, bracketEntry->GetBracketId());

    if (grp)
    {
        for (GroupReference const& itr : grp->GetMembers())
        {
            Player* member = itr.GetSource();
            uint32 memberSlot = member->AddBattlegroundQueueId(bgQueueTypeId);

            WorldPackets::Battleground::BattlefieldStatusQueued battlefieldStatus;
            BattlegroundMgr::BuildBattlegroundStatusQueued(&battlefieldStatus, member, memberSlot, ginfo->JoinTime, bgQueueTypeId, avgTime, true);
            member->SendDirectMessage(battlefieldStatus.Write());
        }
    }
    else
    {
        uint32 queueSlot = _player->AddBattlegroundQueueId(bgQueueTypeId);

        WorldPackets::Battleground::BattlefieldStatusQueued battlefieldStatus;
        BattlegroundMgr::BuildBattlegroundStatusQueued(&battlefieldStatus, _player, queueSlot, ginfo->JoinTime, bgQueueTypeId, avgTime, false);
        SendPacket(battlefieldStatus.Write());
    }

    TC_LOG_DEBUG("bg.battleground", "Battleground Blitz: {} ({}) queued with roles {:#x}",
        _player->GetName(), _player->GetGUID().ToString(), packet.Roles);

    sBattlegroundMgr->ScheduleQueueUpdate(0, bgQueueTypeId, bracketEntry->GetBracketId());
}

// CMSG_BATTLEMASTER_JOIN_SKIRMISH (0x3B00BF) - unrated 3v3 arena, solo or small group.
//
// Like the Blitz join this packet carries no queue identity; the mode is implied by the opcode. It queues
// against BattlemasterList 6 ("All Arenas"), which already has a battleground_template row and whose 15
// arena maps all have templates - so this phase needs no SQL.
//
// Matchmaking needs no new code either: the queue is unrated, so entries land in BG_QUEUE_NORMAL_* and the
// existing `!m_queueId.Rated` branch of BattlegroundQueueUpdate already runs
// CheckNormalMatch(...) || (IsArena && CheckSkirmishForSameFaction(...)).
void WorldSession::HandleBattlemasterJoinSkirmish(WorldPackets::Battleground::BattlemasterJoinSkirmish& packet)
{
    // JoinSkirmish only ever sends Bracket 4 and hard-rejects anything else client-side; RequeueSkirmish
    // sends 255 with the Requeue bit set. Accept exactly those two shapes. The value is NOT mapped onto a
    // BattlegroundBracketId - its enum identity is unknown (see BattlemasterJoinSkirmish's comment).
    bool const validBracket = (packet.Bracket == 4) || (packet.Bracket == 255 && packet.Requeue);

    BattlegroundQueueTypeId bgQueueTypeId =
        BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_AA, BattlegroundQueueIdType::ArenaSkirmish, false, ARENA_TYPE_3v3);

    if (!BattlegroundMgr::IsValidQueueId(bgQueueTypeId))
        return;

    auto sendFailed = [&](GroupJoinBattlegroundResult result)
    {
        WorldPackets::Battleground::BattlefieldStatusFailed battlefieldStatus;
        BattlegroundMgr::BuildBattlegroundStatusFailed(&battlefieldStatus, bgQueueTypeId, _player, 0, result);
        SendPacket(battlefieldStatus.Write());
    };

    if (!validBracket)
    {
        TC_LOG_DEBUG("bg.battleground", "Skirmish: {} sent an unexpected Bracket {} (Requeue {})",
            _player->GetName(), uint32(packet.Bracket), packet.Requeue ? "true" : "false");
        sendFailed(ERR_BATTLEGROUND_JOIN_FAILED);
        return;
    }

    BattlemasterListEntry const* battlemasterListEntry = sBattlemasterListStore.AssertEntry(bgQueueTypeId.BattlemasterListId);

    if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, bgQueueTypeId.BattlemasterListId, nullptr) || battlemasterListEntry->GetFlags().HasFlag(BattlemasterListFlags::InternalOnly))
    {
        ChatHandler(this).PSendSysMessage(LANG_ARENA_DISABLED);
        return;
    }

    if (_player->InBattleground())
        return;

    BattlegroundTemplate const* bgTemplate = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BATTLEGROUND_AA);
    if (!bgTemplate)
    {
        TC_LOG_ERROR("bg.battleground", "Skirmish: template bg (all arenas) not found");
        return;
    }

    PVPDifficultyEntry const* bracketEntry = DB2Manager::GetBattlegroundBracketByLevel(bgTemplate->MapIDs.front(), _player->GetLevel());
    if (!bracketEntry)
        return;

    Group* grp = _player->GetGroup();
    if (grp && grp->GetLeaderGUID() != _player->GetGUID())
        return;

    if (GetPlayer()->isUsingLfg())
    {
        sendFailed(ERR_LFG_CANT_USE_BATTLEGROUND);
        return;
    }

    if (!_player->CanJoinToBattleground(bgTemplate))
    {
        sendFailed(ERR_BATTLEGROUND_JOIN_TIMED_OUT);
        return;
    }

    if (_player->IsDeserter())
    {
        sendFailed(ERR_GROUP_JOIN_BATTLEGROUND_DESERTERS);
        return;
    }

    if (_player->GetBattlegroundQueueIndex(bgQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES)
        return;

    if (!_player->HasFreeBattlegroundQueueId())
    {
        sendFailed(ERR_BATTLEGROUND_TOO_MANY_QUEUES);
        return;
    }

    if (_player->HasAura(9454))
        return;

    ObjectGuid errorGuid;
    GroupJoinBattlegroundResult err = ERR_BATTLEGROUND_NONE;
    if (grp && !sBattlegroundMgr->isArenaTesting())
        err = grp->CanJoinBattlegroundQueue(bgTemplate, bgQueueTypeId, ARENA_TYPE_3v3, ARENA_TYPE_3v3, false, 0, errorGuid);

    if (err)
    {
        WorldPackets::Battleground::BattlefieldStatusFailed battlefieldStatus;
        BattlegroundMgr::BuildBattlegroundStatusFailed(&battlefieldStatus, bgQueueTypeId, _player, 0, err, &errorGuid);
        SendPacket(battlefieldStatus.Write());
        return;
    }

    BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);
    // Unrated, so no rating is carried. Roles are stored but no skirmish matchmaker consults them - the wire
    // only ever tells us the queuer's own mask, never the other party members'.
    GroupQueueInfo* ginfo = bgQueue.AddGroup(_player, grp, Team(_player->GetTeam()), bracketEntry, false, 0, 0, packet.Roles);
    uint32 avgTime = bgQueue.GetAverageQueueWaitTime(ginfo, bracketEntry->GetBracketId());

    if (grp)
    {
        for (GroupReference const& itr : grp->GetMembers())
        {
            Player* member = itr.GetSource();
            uint32 memberSlot = member->AddBattlegroundQueueId(bgQueueTypeId);

            WorldPackets::Battleground::BattlefieldStatusQueued battlefieldStatus;
            BattlegroundMgr::BuildBattlegroundStatusQueued(&battlefieldStatus, member, memberSlot, ginfo->JoinTime, bgQueueTypeId, avgTime, true);
            member->SendDirectMessage(battlefieldStatus.Write());
        }
    }
    else
    {
        uint32 queueSlot = _player->AddBattlegroundQueueId(bgQueueTypeId);

        WorldPackets::Battleground::BattlefieldStatusQueued battlefieldStatus;
        BattlegroundMgr::BuildBattlegroundStatusQueued(&battlefieldStatus, _player, queueSlot, ginfo->JoinTime, bgQueueTypeId, avgTime, false);
        SendPacket(battlefieldStatus.Write());
    }

    TC_LOG_DEBUG("bg.battleground", "Skirmish: {} ({}) queued with roles {:#x} (bracket {}, requeue {})",
        _player->GetName(), _player->GetGUID().ToString(), packet.Roles, uint32(packet.Bracket), packet.Requeue ? "true" : "false");

    sBattlegroundMgr->ScheduleQueueUpdate(0, bgQueueTypeId, bracketEntry->GetBracketId());
}

// CMSG_JOIN_RATED_BATTLEGROUND (0x3A0025) - the classic 10v10 rated battleground, premade-group only.
//
// Like the Blitz and Skirmish joins this carries no queue identity, only the role mask; the mode is implied
// by the opcode. The queue id is { BattlemasterListId = 100, Type = 0 (BATTLEGROUND), Rated = true,
// TeamSize = 0 }.
//
// Type 0 rather than a dedicated "rated bg" nibble is not a guess. The client decodes the nibble through a
// pure switch (VA 0x7FF72AAB59E0) whose cases are 0 BATTLEGROUND, 1 ARENA, 2 WARGAME, 3 CHEAT,
// 4 ARENASKIRMISH, 6 BRAWLSHUFFLE, 7 RATEDSHUFFLE, 8 BRAWLSOLORBG, 9 RATEDSOLORBG - there is no
// RATEDBATTLEGROUND value, because rated-ness lives in bit 20, not in the nibble. The client's own
// SMSG_BATTLEFIELD_STATUS_FAILED handler (VA 0x7FF72AABA380) tests exactly
// "QueueID != 0 && bit20 && nibble == 0" as its notion of a rated battleground.
//
// The client only sends this from a full 10-man group with the leader pressing the button, so the same
// constraint is enforced here rather than trusted.
void WorldSession::HandleJoinRatedBattleground(WorldPackets::Battleground::JoinRatedBattleground& packet)
{
    BattlegroundQueueTypeId bgQueueTypeId =
        BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_RATED_BG, BattlegroundQueueIdType::Battleground, true, 0);

    if (!BattlegroundMgr::IsValidQueueId(bgQueueTypeId))
    {
        TC_LOG_ERROR("network", "Rated Battleground: queue id rejected by IsValidQueueId - BattlemasterList {} missing from the client DB2.",
            uint32(BATTLEGROUND_RATED_BG));
        return;
    }

    BattlemasterListEntry const* battlemasterListEntry = sBattlemasterListStore.AssertEntry(bgQueueTypeId.BattlemasterListId);

    if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, bgQueueTypeId.BattlemasterListId, nullptr) || battlemasterListEntry->GetFlags().HasFlag(BattlemasterListFlags::InternalOnly))
    {
        ChatHandler(this).PSendSysMessage(LANG_BG_DISABLED);
        return;
    }

    if (_player->InBattleground())
        return;

    BattlegroundTemplate const* bgTemplate = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BATTLEGROUND_RATED_BG);
    if (!bgTemplate)
    {
        TC_LOG_ERROR("bg.battleground", "Rated Battleground: no battleground_template row for {} - apply the rated-BG world migration.", uint32(BATTLEGROUND_RATED_BG));
        return;
    }

    PVPDifficultyEntry const* bracketEntry = DB2Manager::GetBattlegroundBracketByLevel(bgTemplate->MapIDs.front(), _player->GetLevel());
    if (!bracketEntry)
        return;

    auto sendFailed = [&](GroupJoinBattlegroundResult result, ObjectGuid const* errorGuid = nullptr)
    {
        WorldPackets::Battleground::BattlefieldStatusFailed battlefieldStatus;
        BattlegroundMgr::BuildBattlegroundStatusFailed(&battlefieldStatus, bgQueueTypeId, _player, 0, result, errorGuid);
        SendPacket(battlefieldStatus.Write());
    };

    if (!packet.Roles)
    {
        sendFailed(ERR_BATTLEGROUND_JOIN_FAILED);
        return;
    }

    Group* grp = _player->GetGroup();
    if (!grp)
    {
        sendFailed(ERR_BATTLEGROUND_JOIN_FAILED);
        return;
    }

    if (grp->GetLeaderGUID() != _player->GetGUID())
        return;

    // Rated battlegrounds are a full-roster mode: BattlemasterList 100 is 10v10 and the client's own UI
    // refuses to send unless the group is full.
    if (grp->GetMembersCount() != bgTemplate->GetMaxPlayersPerTeam())
    {
        sendFailed(ERR_ARENA_TEAM_PARTY_SIZE);
        return;
    }

    if (GetPlayer()->isUsingLfg())
    {
        sendFailed(ERR_LFG_CANT_USE_BATTLEGROUND);
        return;
    }

    if (_player->GetBattlegroundQueueIndex(bgQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES)
        return;

    if (!_player->HasFreeBattlegroundQueueId())
    {
        sendFailed(ERR_BATTLEGROUND_TOO_MANY_QUEUES);
        return;
    }

    ObjectGuid errorGuid;
    GroupJoinBattlegroundResult err = grp->CanJoinBattlegroundQueue(bgTemplate, bgQueueTypeId, 0, bgTemplate->GetMaxPlayersPerTeam(), true, 0, errorGuid);
    if (err)
    {
        sendFailed(err, &errorGuid);
        return;
    }

    BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);
    GroupQueueInfo* ginfo = bgQueue.AddGroup(_player, grp, Team(_player->GetTeam()), bracketEntry, true, 0, 0, packet.Roles);
    uint32 avgTime = bgQueue.GetAverageQueueWaitTime(ginfo, bracketEntry->GetBracketId());

    for (GroupReference const& itr : grp->GetMembers())
    {
        Player* member = itr.GetSource();
        uint32 memberSlot = member->AddBattlegroundQueueId(bgQueueTypeId);

        WorldPackets::Battleground::BattlefieldStatusQueued battlefieldStatus;
        BattlegroundMgr::BuildBattlegroundStatusQueued(&battlefieldStatus, member, memberSlot, ginfo->JoinTime, bgQueueTypeId, avgTime, true);
        member->SendDirectMessage(battlefieldStatus.Write());
    }

    TC_LOG_DEBUG("bg.battleground", "Rated Battleground: {} queued a {}-man group with roles {:#x}",
        _player->GetName(), grp->GetMembersCount(), packet.Roles);

    sBattlegroundMgr->ScheduleQueueUpdate(0, bgQueueTypeId, bracketEntry->GetBracketId());
}

// CMSG_BATTLEMASTER_JOIN_BRAWL (0x3B00C2) - the rotating PvP Brawl. Body is uint8 Roles + one bit
// IsSpecialBrawl (see BattlemasterJoinBrawl's comment for the serializer that says so).
//
// Like the other three joins in this file the packet carries no queue identity, but here that is not just a
// convention: the client already knows which brawl is running because the server told it, in
// SMSG_REQUEST_SCHEDULED_PVP_INFO_RESPONSE. C_PvP.JoinBrawl (client RVA 0x1277770) resolves the brawl by reading
// back the very global that packet's handler wrote (dword_7FF72F082BB8, or dword_7FF72F082BBC when
// isSpecialBrawl is set) and looking it up in PvpBrawl.db2. So the authoritative queue identity is whatever this
// server last advertised, which is exactly what GetActiveBrawl returns - asking it again here rather than
// trusting anything in the packet keeps the two in step.
//
// Queue identity: { BattlemasterListId = the brawl's, Type = 0 (BATTLEGROUND), Rated = false, TeamSize = 0 }.
// Type 0 is not a placeholder for a missing "brawl" nibble - the client's nibble decoder (VA 0x7FF72AAB59E0) has
// no brawl case at all; its nine values are 0 BATTLEGROUND, 1 ARENA, 2 WARGAME, 3 CHEAT, 4 ARENASKIRMISH,
// 6 BRAWLSHUFFLE, 7 RATEDSHUFFLE, 8 BRAWLSOLORBG, 9 RATEDSOLORBG. Brawl-ness is carried by the
// BattlemasterList row (Flags & 0x20 = IsBrawl), which is what the client itself tests when it decides to render
// a queue as a brawl (0x7FF72AAB9DBB, and the isBrawl field of QueueSpecificInfo at 0x7FF72AA8D7D5). Unrated,
// because a brawl has no rating; that also puts the queue on the existing CheckNormalMatch path.
void WorldSession::HandleBattlemasterJoinBrawl(WorldPackets::Battleground::BattlemasterJoinBrawl& packet)
{
    // We never advertise a special-event brawl (HandleRequestScheduledPvpInfo leaves that block out), so the
    // client has nothing to set this bit for. Retail's captured special-event brawl is an LFGDungeons brawl with
    // no BattlemasterList at all, which this queue could not serve even if it were advertised.
    if (packet.IsSpecialBrawl)
        return;

    Optional<BattlegroundMgr::ActiveBrawl> brawl = sBattlegroundMgr->GetActiveBrawl();
    if (!brawl)
        return;

    BattlegroundQueueTypeId bgQueueTypeId =
        BattlegroundMgr::BGQueueTypeId(uint16(brawl->BattlemasterListId), BattlegroundQueueIdType::Battleground, false, 0);

    if (!BattlegroundMgr::IsValidQueueId(bgQueueTypeId))
        return;

    BattlemasterListEntry const* battlemasterListEntry = sBattlemasterListStore.AssertEntry(bgQueueTypeId.BattlemasterListId);

    if (_player->InBattleground())
        return;

    // GetActiveBrawl already refused to advertise a brawl without a template, so this is a reload race, not a
    // configuration error.
    BattlegroundTemplate const* bgTemplate = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BattlegroundTypeId(bgQueueTypeId.BattlemasterListId));
    if (!bgTemplate)
        return;

    PVPDifficultyEntry const* bracketEntry = DB2Manager::GetBattlegroundBracketByLevel(bgTemplate->MapIDs.front(), _player->GetLevel());
    if (!bracketEntry)
        return;

    auto sendFailed = [&](GroupJoinBattlegroundResult result, ObjectGuid const* errorGuid = nullptr)
    {
        WorldPackets::Battleground::BattlefieldStatusFailed battlefieldStatus;
        BattlegroundMgr::BuildBattlegroundStatusFailed(&battlefieldStatus, bgQueueTypeId, _player, 0, result, errorGuid);
        SendPacket(battlefieldStatus.Write());
    };

    // C_PvP.JoinBrawl refuses to emit the packet when the selected roles and the class's allowed roles do not
    // intersect (client error 0x33A), so a zero mask means a client we do not model.
    if (!packet.Roles)
    {
        sendFailed(ERR_BATTLEGROUND_JOIN_FAILED);
        return;
    }

    Group* grp = _player->GetGroup();
    if (grp)
    {
        if (grp->GetLeaderGUID() != _player->GetGUID())
            return;

        // BattlemasterList.MaxGroupSize is the brawl's own party cap (5 for Deep Six).
        if (grp->GetMembersCount() > battlemasterListEntry->MaxGroupSize)
        {
            sendFailed(ERR_BATTLEGROUND_JOIN_FAILED);
            return;
        }
    }

    if (GetPlayer()->isUsingLfg())
    {
        sendFailed(ERR_LFG_CANT_USE_BATTLEGROUND);
        return;
    }

    if (!_player->CanJoinToBattleground(bgTemplate))
    {
        sendFailed(ERR_BATTLEGROUND_JOIN_TIMED_OUT);
        return;
    }

    if (_player->IsDeserter())
    {
        sendFailed(ERR_GROUP_JOIN_BATTLEGROUND_DESERTERS);
        return;
    }

    if (_player->GetBattlegroundQueueIndex(bgQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES)
        return;

    if (!_player->HasFreeBattlegroundQueueId())
    {
        sendFailed(ERR_BATTLEGROUND_TOO_MANY_QUEUES);
        return;
    }

    // check Freeze debuff
    if (_player->HasAura(9454))
        return;

    ObjectGuid errorGuid;
    GroupJoinBattlegroundResult err = ERR_BATTLEGROUND_NONE;
    if (grp)
        err = grp->CanJoinBattlegroundQueue(bgTemplate, bgQueueTypeId, 0, bgTemplate->GetMaxPlayersPerTeam(), false, 0, errorGuid);

    if (err)
    {
        sendFailed(err, &errorGuid);
        return;
    }

    BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);

    // isPremade true only for a group that already fills a side, matching HandleBattlemasterJoinOpcode.
    bool const isPremade = grp && grp->GetMembersCount() >= bgTemplate->GetMinPlayersPerTeam();

    GroupQueueInfo* ginfo = bgQueue.AddGroup(_player, grp, Team(_player->GetTeam()), bracketEntry, isPremade, 0, 0, packet.Roles);
    uint32 avgTime = bgQueue.GetAverageQueueWaitTime(ginfo, bracketEntry->GetBracketId());

    if (grp)
    {
        for (GroupReference const& itr : grp->GetMembers())
        {
            Player* member = itr.GetSource();
            uint32 memberSlot = member->AddBattlegroundQueueId(bgQueueTypeId);

            WorldPackets::Battleground::BattlefieldStatusQueued battlefieldStatus;
            BattlegroundMgr::BuildBattlegroundStatusQueued(&battlefieldStatus, member, memberSlot, ginfo->JoinTime, bgQueueTypeId, avgTime, true);
            member->SendDirectMessage(battlefieldStatus.Write());
        }
    }
    else
    {
        uint32 queueSlot = _player->AddBattlegroundQueueId(bgQueueTypeId);

        WorldPackets::Battleground::BattlefieldStatusQueued battlefieldStatus;
        BattlegroundMgr::BuildBattlegroundStatusQueued(&battlefieldStatus, _player, queueSlot, ginfo->JoinTime, bgQueueTypeId, avgTime, false);
        SendPacket(battlefieldStatus.Write());
    }

    TC_LOG_DEBUG("bg.battleground", "Brawl: {} ({}) queued for PvpBrawl {} (BattlemasterList {}) with roles {:#x}",
        _player->GetName(), _player->GetGUID().ToString(), brawl->PvpBrawlId, bgQueueTypeId.BattlemasterListId, packet.Roles);

    sBattlegroundMgr->ScheduleQueueUpdate(0, bgQueueTypeId, bracketEntry->GetBracketId());
}

void WorldSession::HandlePVPLogDataOpcode(WorldPackets::Battleground::PVPLogDataRequest& /*pvpLogDataRequest*/)
{
    Battleground* bg = _player->GetBattleground();
    if (!bg)
        return;

    // Prevent players from sending BuildPvpLogDataPacket in an arena except for when sent in BattleGround::EndBattleGround.
    if (bg->isArena())
        return;

    WorldPackets::Battleground::PVPMatchStatisticsMessage pvpMatchStatistics;
    bg->BuildPvPLogDataPacket(pvpMatchStatistics.Data);
    SendPacket(pvpMatchStatistics.Write());
}

void WorldSession::HandleBattlefieldListOpcode(WorldPackets::Battleground::BattlefieldListRequest& battlefieldList)
{
    BattlemasterListEntry const* battlemasterListEntry = sBattlemasterListStore.LookupEntry(battlefieldList.ListID);
    if (!battlemasterListEntry)
    {
        TC_LOG_DEBUG("bg.battleground", "BattlegroundHandler: invalid bgtype ({}) with player (Name: {}, {}) received.", battlefieldList.ListID, _player->GetName(), _player->GetGUID().ToString());
        return;
    }

    sBattlegroundMgr->SendBattlegroundList(_player, ObjectGuid::Empty, BattlegroundTypeId(battlefieldList.ListID));
}

void WorldSession::HandleBattleFieldPortOpcode(WorldPackets::Battleground::BattlefieldPort& battlefieldPort)
{
    if (!_player->InBattlegroundQueue())
    {
        TC_LOG_DEBUG("bg.battleground", "CMSG_BATTLEFIELD_PORT {} Slot: {}, Unk: {}, Time: {}, AcceptedInvite: {}. Player not in queue!",
            GetPlayerInfo(), battlefieldPort.Ticket.Id, uint32(battlefieldPort.Ticket.Type), battlefieldPort.Ticket.Time.AsUnderlyingType(), uint32(battlefieldPort.AcceptedInvite));
        return;
    }

    BattlegroundQueueTypeId bgQueueTypeId = _player->GetBattlegroundQueueTypeId(battlefieldPort.Ticket.Id);
    if (bgQueueTypeId == BATTLEGROUND_QUEUE_NONE)
    {
        TC_LOG_DEBUG("bg.battleground", "CMSG_BATTLEFIELD_PORT {} Slot: {}, Unk: {}, Time: {}, AcceptedInvite: {}. Invalid queueSlot!",
            GetPlayerInfo(), battlefieldPort.Ticket.Id, uint32(battlefieldPort.Ticket.Type), battlefieldPort.Ticket.Time.AsUnderlyingType(), uint32(battlefieldPort.AcceptedInvite));
        return;
    }

    BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);

    //we must use temporary variable, because GroupQueueInfo pointer can be deleted in BattlegroundQueue::RemovePlayer() function
    GroupQueueInfo ginfo;
    if (!bgQueue.GetPlayerGroupInfoData(_player->GetGUID(), &ginfo))
    {
        TC_LOG_DEBUG("bg.battleground", "CMSG_BATTLEFIELD_PORT {} Slot: {}, Unk: {}, Time: {}, AcceptedInvite: {}. Player not in queue (No player Group Info)!",
            GetPlayerInfo(), battlefieldPort.Ticket.Id, uint32(battlefieldPort.Ticket.Type), battlefieldPort.Ticket.Time.AsUnderlyingType(), uint32(battlefieldPort.AcceptedInvite));
        return;
    }
    // if action == 1, then player must have been invited to join
    if (!ginfo.IsInvitedToBGInstanceGUID && battlefieldPort.AcceptedInvite)
    {
        TC_LOG_DEBUG("bg.battleground", "CMSG_BATTLEFIELD_PORT {} Slot: {}, Unk: {}, Time: {}, AcceptedInvite: {}. Player is not invited to any bg!",
            GetPlayerInfo(), battlefieldPort.Ticket.Id, uint32(battlefieldPort.Ticket.Type), battlefieldPort.Ticket.Time.AsUnderlyingType(), uint32(battlefieldPort.AcceptedInvite));
        return;
    }

    BattlegroundTypeId bgTypeId = BattlegroundTypeId(bgQueueTypeId.BattlemasterListId);
    BattlegroundTemplate const* bgTemplate = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(bgTypeId);
    if (!bgTemplate)
    {
        TC_LOG_ERROR("network", "BattlegroundHandle: BattlegroundTemplate not found for type id {}.", bgTypeId);
        return;
    }

    uint32 mapId = bgTemplate->MapIDs.front();

    // BGTemplateId returns BATTLEGROUND_AA when it is arena queue.
    // Do instance id search as there is no AA bg instances.
    Battleground* bg = sBattlegroundMgr->GetBattleground(ginfo.IsInvitedToBGInstanceGUID, bgTypeId == BATTLEGROUND_AA ? BATTLEGROUND_TYPE_NONE : bgTypeId);
    if (!bg && battlefieldPort.AcceptedInvite)
    {
        TC_LOG_DEBUG("bg.battleground", "CMSG_BATTLEFIELD_PORT {} Slot: {}, Unk: {}, Time: {}, AcceptedInvite: {}. Cant find BG with id {}!",
            GetPlayerInfo(), battlefieldPort.Ticket.Id, uint32(battlefieldPort.Ticket.Type), battlefieldPort.Ticket.Time.AsUnderlyingType(), uint32(battlefieldPort.AcceptedInvite), ginfo.IsInvitedToBGInstanceGUID);

        // The one refusal on this path that is a fact rather than a rule of ours: the player accepted a real
        // invitation, and the battleground instance it pointed at is gone. Upstream returned in silence here, so
        // the client kept a ready dialog that could never resolve. SMSG_BATTLEFIELD_PORT_DENIED is the only
        // channel the client has for "your port did not happen": it reads nothing from the body and shows one
        // system message, see the BattlefieldPortDenied class comment for the consumer.
        // UNVERIFIED: which refusals retail answers with this message. The displayed text is settled
        // (ERR_PLAYER_DEAD, table entry 171) and does not describe a vanished instance, but the alternative is
        // the silence that leaves the dialog hanging, and no capture of any build contains this opcode. Needs a
        // recording of a port into an instance that was torn down between invitation and accept.
        SendPacket(WorldPackets::Battleground::BattlefieldPortDenied().Write());
        return;
    }
    else if (bg)
        mapId = bg->GetMapId();

    TC_LOG_DEBUG("bg.battleground", "CMSG_BATTLEFIELD_PORT {} Slot: {}, Unk: {}, Time: {}, AcceptedInvite: {}.",
        GetPlayerInfo(), battlefieldPort.Ticket.Id, uint32(battlefieldPort.Ticket.Type), battlefieldPort.Ticket.Time.AsUnderlyingType(), uint32(battlefieldPort.AcceptedInvite));

    // expected bracket entry
    PVPDifficultyEntry const* bracketEntry = DB2Manager::GetBattlegroundBracketByLevel(mapId, _player->GetLevel());
    if (!bracketEntry)
        return;

    //some checks if player isn't cheating - it is not exactly cheating, but we cannot allow it
    if (battlefieldPort.AcceptedInvite && bgQueue.GetQueueId().TeamSize == 0)
    {
        //if player is trying to enter battleground (not arena!) and he has deserter debuff, we must just remove him from queue
        if (_player->IsDeserter())
        {
            //send bg command result to show nice message
            WorldPackets::Battleground::BattlefieldStatusFailed battlefieldStatus;
            BattlegroundMgr::BuildBattlegroundStatusFailed(&battlefieldStatus, bgQueueTypeId, _player, battlefieldPort.Ticket.Id, ERR_GROUP_JOIN_BATTLEGROUND_DESERTERS);
            SendPacket(battlefieldStatus.Write());
            battlefieldPort.AcceptedInvite = false;
            TC_LOG_DEBUG("bg.battleground", "Player {} {} has a deserter debuff, do not port him to battleground!", _player->GetName(), _player->GetGUID().ToString());
        }
        //if player don't match battleground max level, then do not allow him to enter! (this might happen when player leveled up during his waiting in queue
        if (_player->GetLevel() > bg->GetMaxLevel())
        {
            TC_LOG_ERROR("network", "Player {} {} has level ({}) higher than maxlevel ({}) of battleground ({})! Do not port him to battleground!",
                _player->GetName(), _player->GetGUID().ToString(), _player->GetLevel(), bg->GetMaxLevel(), bg->GetTypeID());
            battlefieldPort.AcceptedInvite = false;
        }
    }

    if (battlefieldPort.AcceptedInvite)
    {
        // No liveness gate here, and that is a decision rather than an omission. A dead player who accepts is
        // ported and resurrected by BattlegroundMgr::PortPlayerToBattleground, which is what Blizzard's own UI
        // expects: CONFIRM_BATTLEFIELD_ENTRY is declared whileDead = 1 in GameDialogDefs.lua, and both accept
        // paths (that dialog and PVPReadyDialogEnterButtonMixin:OnClick in PVPHelper.lua) close the release-spirit
        // popup with StaticPopup_Hide("DEATH") once the accept goes through - only meaningful if a corpse can
        // enter. Retail's refusal for a dead player is a JOIN-time one and has its own code on its own packet:
        // ERR_GROUP_JOIN_BATTLEGROUND_DEAD, GroupJoinBattlegroundResult 57, client error table entry 588,
        // travelling in SMSG_BATTLEFIELD_STATUS_FAILED. SMSG_BATTLEFIELD_PORT_DENIED shows the generic
        // ERR_PLAYER_DEAD instead (table entry 171 via ShowSystemMessage(0xAB) in consumer 0x21C23E0), so it is
        // not that refusal. An earlier revision of this branch denied the port here; that turned the resurrect
        // in PortPlayerToBattleground into dead code for this path while the group-proposal path kept using it,
        // and it stranded the invitation for the full INVITE_ACCEPT_WAIT_TIME.

        // check Freeze debuff
        if (_player->HasAura(9454))
            return;

        if (!_player->IsInvitedForBattlegroundQueueType(bgQueueTypeId))
            return;                                 // cheating?

        // Solo-queue modes hold the whole lobby together: the accept is recorded, the client is told who is
        // still deciding, and nobody ports until everyone has answered. ProposalAccept does the porting - for
        // every member at once - when this accept is the last one.
        if (bgQueue.ProposalAccept(_player->GetGUID()))
            return;

        // bg->HandleBeforeTeleportToBattleground(_player);
        // add only in HandleMoveWorldPortAck()
        // bg->AddPlayer(_player, team);
        BattlegroundMgr::PortPlayerToBattleground(_player, bg, ginfo.Team, bgQueueTypeId, battlefieldPort.Ticket.Id);
    }
    else // leave queue
    {
        // Under a group proposal a decline is not just this player leaving: the whole proposal collapses, the
        // others get SMSG_BATTLEFIELD_STATUS_GROUP_PROPOSAL_FAILED and those who had already accepted are put
        // back in the queue at the position they held. This player's own removal is the code below.
        bgQueue.ProposalDecline(_player->GetGUID());

        // if player leaves rated arena match before match start, it is counted as he played but he lost
        if (bgQueue.GetQueueId().Rated && ginfo.IsInvitedToBGInstanceGUID)
        {
            ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(ginfo.Team);
            if (at)
            {
                TC_LOG_DEBUG("bg.battleground", "UPDATING memberLost's personal arena rating for {} by opponents rating: {}, because he has left queue!", _player->GetGUID().ToString(), ginfo.OpponentsTeamRating);
                at->MemberLost(_player, ginfo.OpponentsMatchmakerRating);
                at->SaveToDB();
            }
        }

        WorldPackets::Battleground::BattlefieldStatusNone battlefieldStatus;
        battlefieldStatus.Ticket = battlefieldPort.Ticket;
        SendPacket(battlefieldStatus.Write());

        _player->RemoveBattlegroundQueueId(bgQueueTypeId);  // must be called this way, because if you move this call to queue->removeplayer, it causes bugs
        bgQueue.RemovePlayer(_player->GetGUID(), true);
        // player left queue, we should update it - do not update Arena Queue
        if (!bgQueue.GetQueueId().TeamSize)
            sBattlegroundMgr->ScheduleQueueUpdate(ginfo.ArenaMatchmakerRating, bgQueueTypeId, bracketEntry->GetBracketId());

        TC_LOG_DEBUG("bg.battleground", "Battleground: player {} ({}) left queue for bgtype {}, queue {{ BattlemasterListId: {}, Type: {}, Rated: {}, TeamSize: {} }}.",
            _player->GetName(), _player->GetGUID().ToString(), bg->GetTypeID(),
            bgQueueTypeId.BattlemasterListId, uint32(bgQueueTypeId.Type), bgQueueTypeId.Rated ? "true" : "false", uint32(bgQueueTypeId.TeamSize));
    }
}

void WorldSession::HandleBattlefieldLeaveOpcode(WorldPackets::Battleground::BattlefieldLeave& /*battlefieldLeave*/)
{
    // not allow leave battleground in combat
    if (_player->IsInCombat())
        if (Battleground* bg = _player->GetBattleground())
            if (bg->GetStatus() != STATUS_WAIT_LEAVE)
                return;

    _player->LeaveBattleground();
}

void WorldSession::HandleRequestBattlefieldStatusOpcode(WorldPackets::Battleground::RequestBattlefieldStatus& /*requestBattlefieldStatus*/)
{
    // we must update all queues here
    Battleground* bg = nullptr;
    for (uint8 i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
    {
        BattlegroundQueueTypeId bgQueueTypeId = _player->GetBattlegroundQueueTypeId(i);
        if (bgQueueTypeId == BATTLEGROUND_QUEUE_NONE)
            continue;
        BattlegroundTypeId bgTypeId = BattlegroundTypeId(bgQueueTypeId.BattlemasterListId);
        bg = _player->GetBattleground();
        if (bg)
        {
            BattlegroundPlayer const* bgPlayer = bg->GetBattlegroundPlayerData(_player->GetGUID());
            if (bgPlayer && bgPlayer->queueTypeId == bgQueueTypeId)
            {
                //i cannot check any variable from player class because player class doesn't know if player is in 2v2 / 3v3 or 5v5 arena
                //so i must use bg pointer to get that information
                WorldPackets::Battleground::BattlefieldStatusActive battlefieldStatus;
                BattlegroundMgr::BuildBattlegroundStatusActive(&battlefieldStatus, bg, _player, i, _player->GetBattlegroundQueueJoinTime(bgQueueTypeId), bgQueueTypeId);
                SendPacket(battlefieldStatus.Write());
                continue;
            }
        }

        //we are sending update to player about queue - he can be invited there!
        //get GroupQueueInfo for queue status
        BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);
        GroupQueueInfo ginfo;
        if (!bgQueue.GetPlayerGroupInfoData(_player->GetGUID(), &ginfo))
            continue;
        if (ginfo.IsInvitedToBGInstanceGUID)
        {
            bg = sBattlegroundMgr->GetBattleground(ginfo.IsInvitedToBGInstanceGUID, bgTypeId);
            if (!bg)
                continue;

            WorldPackets::Battleground::BattlefieldStatusNeedConfirmation battlefieldStatus;
            BattlegroundMgr::BuildBattlegroundStatusNeedConfirmation(&battlefieldStatus, bg, _player, i, _player->GetBattlegroundQueueJoinTime(bgQueueTypeId), getMSTimeDiff(GameTime::GetGameTimeMS(), ginfo.RemoveInviteTime), bgQueueTypeId);
            SendPacket(battlefieldStatus.Write());
        }
        else
        {
            BattlegroundTemplate const* bgTemplate  = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(bgTypeId);
            if (!bgTemplate)
                continue;

            // expected bracket entry
            PVPDifficultyEntry const* bracketEntry = DB2Manager::GetBattlegroundBracketByLevel(bgTemplate->MapIDs.front(), _player->GetLevel());
            if (!bracketEntry)
                continue;

            uint32 avgTime = bgQueue.GetAverageQueueWaitTime(&ginfo, bracketEntry->GetBracketId());
            WorldPackets::Battleground::BattlefieldStatusQueued battlefieldStatus;
            BattlegroundMgr::BuildBattlegroundStatusQueued(&battlefieldStatus, _player, i, _player->GetBattlegroundQueueJoinTime(bgQueueTypeId), bgQueueTypeId, avgTime, ginfo.Players.size() > 1);
            SendPacket(battlefieldStatus.Write());
        }
    }
}

void WorldSession::HandleBattlemasterJoinArena(WorldPackets::Battleground::BattlemasterJoinArena& packet)
{
    // ignore if we already in BG or BG queue
    if (_player->InBattleground())
        return;

    uint8 arenatype = ArenaTeam::GetTypeBySlot(packet.TeamSizeIndex);

    //check existence
    BattlegroundTemplate const* bgTemplate = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BATTLEGROUND_AA);
    if (!bgTemplate)
    {
        TC_LOG_ERROR("network", "Battleground: template bg (all arenas) not found");
        return;
    }

    if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, BATTLEGROUND_AA, nullptr))
    {
        ChatHandler(this).PSendSysMessage(LANG_ARENA_DISABLED);
        return;
    }

    BattlegroundTypeId bgTypeId = bgTemplate->Id;
    BattlegroundQueueTypeId bgQueueTypeId = BattlegroundMgr::BGQueueTypeId(bgTypeId, BattlegroundQueueIdType::Arena, true, arenatype);
    PVPDifficultyEntry const* bracketEntry = DB2Manager::GetBattlegroundBracketByLevel(bgTemplate->MapIDs.front(), _player->GetLevel());
    if (!bracketEntry)
        return;

    Group* grp = _player->GetGroup();
    if (!grp)
    {
        grp = new Group();
        grp->Create(_player);
    }

    // no group found, error
    if (!grp)
        return;

    if (grp->GetLeaderGUID() != _player->GetGUID())
        return;

    // get the team rating for queuing
    uint32 arenaRating = 1; //at->GetRating();
    uint32 matchmakerRating = 1; //at->GetAverageMMR(grp);

    if (arenaRating <= 0)
        arenaRating = 1;

    BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);

    uint32 avgTime = 0;
    GroupQueueInfo* ginfo = nullptr;

    ObjectGuid errorGuid;
    GroupJoinBattlegroundResult err = ERR_BATTLEGROUND_NONE;
    if (!sBattlegroundMgr->isArenaTesting())
        err = grp->CanJoinBattlegroundQueue(bgTemplate, bgQueueTypeId, arenatype, arenatype, true, packet.TeamSizeIndex, errorGuid);

    if (!err)
    {
        TC_LOG_DEBUG("bg.battleground", "Battleground: arena team id {}, leader {} queued with matchmaker rating {} for type {}", _player->GetArenaTeamId(packet.TeamSizeIndex), _player->GetName(), matchmakerRating, arenatype);

        ginfo = bgQueue.AddGroup(_player, grp, Team(_player->GetTeam()), bracketEntry, false, arenaRating, matchmakerRating, packet.Roles);
        avgTime = bgQueue.GetAverageQueueWaitTime(ginfo, bracketEntry->GetBracketId());
    }

    for (GroupReference const& itr : grp->GetMembers())
    {
        Player* member = itr.GetSource();
        if (err)
        {
            WorldPackets::Battleground::BattlefieldStatusFailed battlefieldStatus;
            BattlegroundMgr::BuildBattlegroundStatusFailed(&battlefieldStatus, bgQueueTypeId, _player, 0, err, &errorGuid);
            member->SendDirectMessage(battlefieldStatus.Write());
            continue;
        }

        if (!_player->CanJoinToBattleground(bgTemplate))
        {
            WorldPackets::Battleground::BattlefieldStatusFailed battlefieldStatus;
            BattlegroundMgr::BuildBattlegroundStatusFailed(&battlefieldStatus, bgQueueTypeId, _player, 0, ERR_BATTLEGROUND_JOIN_FAILED, &errorGuid);
            member->SendDirectMessage(battlefieldStatus.Write());
            return;
        }

        // add to queue
        uint32 queueSlot = member->AddBattlegroundQueueId(bgQueueTypeId);

        WorldPackets::Battleground::BattlefieldStatusQueued battlefieldStatus;
        BattlegroundMgr::BuildBattlegroundStatusQueued(&battlefieldStatus, member, queueSlot, ginfo->JoinTime, bgQueueTypeId, avgTime, true);
        member->SendDirectMessage(battlefieldStatus.Write());

        TC_LOG_DEBUG("bg.battleground", "Battleground: player joined queue for arena as group bg queue {{ BattlemasterListId: {}, Type: {}, Rated: {}, TeamSize: {} }}, {}, NAME {}",
            bgQueueTypeId.BattlemasterListId, uint32(bgQueueTypeId.Type), bgQueueTypeId.Rated ? "true" : "false", uint32(bgQueueTypeId.TeamSize),
            member->GetGUID().ToString(), member->GetName());
    }

    sBattlegroundMgr->ScheduleQueueUpdate(matchmakerRating, bgQueueTypeId, bracketEntry->GetBracketId());
}

void WorldSession::HandleReportPvPAFK(WorldPackets::Battleground::ReportPvPPlayerAFK& reportPvPPlayerAFK)
{
    Player* reportedPlayer = ObjectAccessor::FindPlayer(reportPvPPlayerAFK.Offender);
    if (!reportedPlayer)
    {
        TC_LOG_INFO("bg.reportpvpafk", "WorldSession::HandleReportPvPAFK: {} [IP: {}] reported {}", _player->GetName(), _player->GetSession()->GetRemoteAddress(), reportPvPPlayerAFK.Offender.ToString());
        return;
    }

    TC_LOG_DEBUG("bg.battleground", "WorldSession::HandleReportPvPAFK: {} reported {}", _player->GetName(), reportedPlayer->GetName());

    reportedPlayer->ReportedAfkBy(_player);
}

void WorldSession::HandleRequestRatedPvpInfo(WorldPackets::Battleground::RequestRatedPvpInfo& /*packet*/)
{
    WorldPackets::Battleground::RatedPvpInfo ratedPvpInfo;
    SendPacket(ratedPvpInfo.Write());
}

// This is the packet that publishes the running PvP Brawl. It used to answer all-inactive because nothing here
// scheduled a brawl; now it answers with whatever BattlegroundMgr::GetActiveBrawl() vouches for, and with nothing
// when it vouches for nothing. GetActiveBrawl only returns a brawl whose BattlemasterList exists, is flagged
// IsBrawl, is not disabled, and has a battleground_template that resolves to a real map - so the Brawl button can
// only ever light up for a queue that can actually pop.
//
// The special-event slot is left empty on purpose. Retail's captured value for it is PvpBrawl 155 "Decor Duel",
// whose PvpBrawl.db2 BattlemasterListID is 0 - it is an LFGDungeons brawl, not a battleground, and there is no
// queue here that could serve it.
void WorldSession::HandleRequestScheduledPvpInfo(WorldPackets::Battleground::RequestScheduledPvpInfo& /*packet*/)
{
    WorldPackets::Battleground::RequestScheduledPvpInfoResponse response;

    if (Optional<BattlegroundMgr::ActiveBrawl> brawl = sBattlegroundMgr->GetActiveBrawl())
    {
        WorldPackets::Battleground::RequestScheduledPvpInfoResponse::BrawlInfo& info = response.Brawl.emplace();
        info.BrawlID = brawl->PvpBrawlId;
        info.CanQueue = true;

        // The client turns this into `timeLeftUntilNextChange` by adding it to the moment this packet arrived,
        // and hides the brawl entirely once that deadline passes - so it must be a real future instant, not a
        // decoration. That much is read off the client.
        //
        // UNVERIFIED: the LENGTH of the window. Retail's own value is not in any capture we hold, and the
        // client accepts any future instant, so this is our policy and not a decoded field: the brawl here is
        // a fixed configuration rather than a rotation, and the next weekly reset is the point at which an
        // operator changing that configuration would take effect. Re-asking after the reset gets a fresh
        // window. A capture of retail's SMSG_REQUEST_SCHEDULED_PVP_INFO_RESPONSE would replace this choice
        // with a measurement - see aufnahme_noetig.
        time_t const now = GameTime::GetGameTime();
        time_t const nextChange = sWorld->GetNextWeeklyQuestsResetTime();
        info.SecondsUntilNextChange = nextChange > now ? uint32(nextChange - now) : uint32(WEEK);
    }

    SendPacket(response.Write());
}

void WorldSession::HandleGetPVPOptionsEnabled(WorldPackets::Battleground::GetPVPOptionsEnabled& /*getPvPOptionsEnabled*/)
{
    WorldPackets::Battleground::PVPOptionsEnabled pvpOptionsEnabled;
    // Flipped: HandleJoinRatedBattleground exists and queues for real.
    pvpOptionsEnabled.RatedBattlegrounds = true;
    pvpOptionsEnabled.PugBattlegrounds = true;
    pvpOptionsEnabled.WargameBattlegrounds = false;
    pvpOptionsEnabled.WargameArenas = false;
    pvpOptionsEnabled.RatedArenas = true;
    // Flipped because HandleBattlemasterJoinSkirmish now exists: it builds a real
    // BattlegroundQueueIdType::ArenaSkirmish queue id and the existing unrated matchmaker
    // (CheckSkirmishForSameFaction) pairs the entries up. This bit gates the client's Skirmish button.
    pvpOptionsEnabled.ArenaSkirmish = true;
    pvpOptionsEnabled.SoloShuffle = false;
    pvpOptionsEnabled.RatedSoloShuffle = false;
    pvpOptionsEnabled.BattlegroundBlitz = false;
    // Flipped because HandleBattlemasterJoinRatedBGBlitz now exists and really queues: this bit gates the
    // client's "Battleground Blitz" button, and retail sets it (sniffed SMSG_PVP_OPTIONS_ENABLED body = FF C0,
    // all ten bits). The remaining four stay false until their own handlers land - enabling a bit whose opcode
    // the server drops just produces a button that silently does nothing.
    pvpOptionsEnabled.RatedBattlegroundBlitz = true;
    SendPacket(pvpOptionsEnabled.Write());
}

void WorldSession::HandleRequestPvpReward(WorldPackets::Battleground::RequestPVPRewards& /*packet*/)
{
    _player->SendPvpRewards();
}

void WorldSession::HandleAreaSpiritHealerQueryOpcode(WorldPackets::Battleground::AreaSpiritHealerQuery& areaSpiritHealerQuery)
{
    Player* player = GetPlayer();
    Creature* spiritHealer = ObjectAccessor::GetCreature(*player, areaSpiritHealerQuery.HealerGuid);
    if (!spiritHealer)
        return;

    if (!spiritHealer->IsAreaSpiritHealer())
        return;

    if (!_player->IsWithinDistInMap(spiritHealer, MAX_AREA_SPIRIT_HEALER_RANGE))
        return;

    if (spiritHealer->IsAreaSpiritHealerIndividual())
    {
        if (Aura* aura = player->GetAura(SPELL_SPIRIT_HEAL_PLAYER_AURA))
        {
            player->SendAreaSpiritHealerTime(spiritHealer->GetGUID(), aura->GetDuration());
        }
        else if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(SPELL_SPIRIT_HEAL_PLAYER_AURA, DIFFICULTY_NONE))
        {
            spiritHealer->CastSpell(player, SPELL_SPIRIT_HEAL_PLAYER_AURA);
            player->SendAreaSpiritHealerTime(spiritHealer->GetGUID(), spellInfo->GetDuration());
            spiritHealer->CastSpell(nullptr, SPELL_SPIRIT_HEAL_CHANNEL_SELF);
        }
    }
    else
        _player->SendAreaSpiritHealerTime(spiritHealer);
}

void WorldSession::HandleAreaSpiritHealerQueueOpcode(WorldPackets::Battleground::AreaSpiritHealerQueue& areaSpiritHealerQueue)
{
    Creature* spiritHealer = ObjectAccessor::GetCreature(*GetPlayer(), areaSpiritHealerQueue.HealerGuid);
    if (!spiritHealer)
        return;

    if (!spiritHealer->IsAreaSpiritHealer())
        return;

    if (!_player->IsWithinDistInMap(spiritHealer, MAX_AREA_SPIRIT_HEALER_RANGE))
        return;

    _player->SetAreaSpiritHealer(spiritHealer);
}

void WorldSession::HandleHearthAndResurrect(WorldPackets::Battleground::HearthAndResurrect& /*hearthAndResurrect*/)
{
    if (_player->IsInFlight())
        return;

    if (Battlefield* bf = sBattlefieldMgr->GetBattlefieldToZoneId(_player->GetMap(), _player->GetZoneId()))
    {
        bf->PlayerAskToLeave(_player);
        return;
    }

    AreaTableEntry const* atEntry = sAreaTableStore.LookupEntry(_player->GetAreaId());
    if (!atEntry || !(atEntry->GetFlags().HasFlag(AreaFlags::AllowHearthAndRessurectFromArea)))
        return;

    _player->BuildPlayerRepop();
    _player->ResurrectPlayer(1.0f);
    _player->TeleportTo(_player->m_homebind);
}
