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
#include <unordered_map>

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
        // Argument list unchanged from upstream; only the role mask is new. See the group branch below for why
        // the premade flag is deliberately left where upstream put it. (Here isPremade is still false anyway -
        // The parameter order is (ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦, bool isPremade, uint32 ArenaRating, uint32 MatchmakerRating, uint8 roles).
        // This call used to read (ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦, false, isPremade, 0): isPremade was landing in ArenaRating while the real
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
            // The premade flag is NOT promoted into AddGroup's isPremade parameter, and that is a decision, not
            // an oversight. AddGroup buckets with `if (!m_queueId.Rated && !isPremade) index += PVP_TEAMS_COUNT`,
            // so a true flag files the entry in BG_QUEUE_PREMADE_*. For an UNRATED queue exactly one matchmaker
            // reads those lists - CheckPremadeMatch - and its first condition needs an uninvited premade on BOTH
            // factions at once; CheckNormalMatch and FillPlayersToBG read BG_QUEUE_NORMAL_* only. A group that
            // reaches BattlemasterList.MinPlayers would therefore drop out of ordinary matchmaking until an
            // opposing premade turned up, or until the demotion loop at the end of CheckPremadeMatch released it
            // - and that loop fires only on `JoinTime < now - CONFIG_BATTLEGROUND_PREMADE_GROUP_WAIT_FOR_MATCH`
            // (Battleground.PremadeGroupWaitForMatch, 30 min by default) or on `Players.size() < MinPlayersPerTeam`,
            // which is false for this group by construction. That is not a rare shape: BattlemasterList carries
            // MinPlayers 5 at MaxGroupSize 5 for Random Battleground (32), Classic Warsong Gulch (2) and Twin
            // Peaks (108), so ANY full party would be parked. CMSG_BATTLEMASTER_JOIN is not one of this unit's
            // opcodes and nothing in the 12.1 client asks for a different queue layout, so the call is left
            // byte-for-byte as upstream wrote it (`false, isPremade, 0` - the flag lands in ArenaRating, which no
            // unrated code path reads) and only gains the role mask that the 12.1 role-aware queue needs.
            ginfo = bgQueue.AddGroup(_player, grp, getQueueTeam(), bracketEntry, false, isPremade, 0, battlemasterJoin.Roles);
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

// CMSG_BATTLEMASTER_JOIN_RATED_BG_BLITZ (0x3E00C0) - rated 8v8 solo/duo queue.
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

    auto sendFailed = [&](GroupJoinBattlegroundResult result, ObjectGuid const* errorGuid = nullptr)
    {
        WorldPackets::Battleground::BattlefieldStatusFailed battlefieldStatus;
        BattlegroundMgr::BuildBattlegroundStatusFailed(&battlefieldStatus, bgQueueTypeId, _player, 0, result, errorGuid);
        SendPacket(battlefieldStatus.Write());
    };

    // LOWER LEVEL BOUND. This is not redundant with the bracket lookup below, and taking it for redundant is
    // exactly how it came to be missing here: DB2Manager::GetBattlegroundBracketByLevel returns nullptr only
    // below the lowest MinLevel of the MAP, and PVPDifficulty gives map 2107 - MapIDs.front() for both
    // BattlemasterList 1101 and 100 - a ladder that starts at 10-19, while BattlemasterList.MinLevel is 60
    // for Battleground Blitz and 50 for Rated Battleground. Without this gate a level-10 character is filed
    // into the 10-19 bracket of a rated 8v8 queue, and CreateNewBattleground even finds maps for it
    // (2107, 2245, 727, 998, 761, 726 and 2106 all carry 10-19). Nothing further down catches it either:
    // HandleBattleFieldPortOpcode checks only the UPPER bound, Player::CanJoinToBattleground is a pure RBAC
    // check, and Group::CanJoinBattlegroundQueue compares members only against EACH OTHER - which a solo
    // join never even reaches. The arena sibling never exposed the hole because BattlemasterList 6 carries
    // MinLevel 20 and map 1505's ladder likewise begins at 20-29; the analogy does not carry to the three
    // aggregates, whose ladders sit 40 to 50 levels below their declared minimum.
    //
    // Player::GetBGAccessByLevel is the tree's own gate for this question - it reads BattlemasterList
    // MinLevel/MaxLevel through BattlegroundTemplate - and HandleBattlemasterHelloOpcode already uses it.
    //
    // On the Reason: the client's Reason -> GlobalString table (read out of 0x21C13A0, tabulated in ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â§10.2 of
    // AGENT_BRIEF_BATTLEFIELD_4B.md) has no code meaning "below this queue's minimum level". 58,
    // ERR_BATTLEGROUND_JOIN_REQUIRES_LEVEL, is the tournament-realm all-participants-at-max-level rule, not a
    // minimum, and every value the table does not list falls into the client's default branch, which prints
    // nothing and still tears the queue slot down. 35 is that table's generic "you cannot join", so it is the
    // only value here that leaves the player with a message rather than a silently vanishing queue. What
    // retail sends is not claimed: retail's own UI gates these queues by level, so no capture can hold it.
    if (!_player->GetBGAccessByLevel(BattlegroundTypeId(bgQueueTypeId.BattlemasterListId)))
    {
        sendFailed(ERR_BATTLEGROUND_JOIN_FAILED);
        return;
    }

    PVPDifficultyEntry const* bracketEntry = DB2Manager::GetBattlegroundBracketByLevel(bgTemplate->MapIDs.front(), _player->GetLevel());
    if (!bracketEntry)
        return;

    // The retail client refuses to send this opcode with no role selected, so a zero mask means either a
    // modified client or a UI state we do not model. Either way the matchmaker cannot place the player.
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

        // BattlemasterList 1101 caps the queueing party at MaxGroupSize (2 on retail - solo or duo).
        if (int32(grp->GetMembersCount()) > battlemasterListEntry->MaxGroupSize)
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

    // The exclusion against the random-battleground queue, on the solo path. A party is already covered:
    // Group::CanJoinBattlegroundQueue tests every member with
    // "bgOrTemplate->Id != BATTLEGROUND_AA && isInRandomBgQueue -> ERR_IN_RANDOM_BG" (Group.cpp), and
    // BattlemasterList 1101 is not BATTLEGROUND_AA - but that function is only called under "if (grp)"
    // further down, so without this a player standing in the random queue may stack Blitz on top of it
    // ALONE while the same player in a duo is refused. That asymmetry is not a retail question, it is an
    // invariant of this tree; HandleBattlemasterJoinOpcode, which these handlers were written along, carries
    // the same test on its own solo path.
    // Only this half of the pair is needed. Its mirror, ERR_IN_NON_RANDOM_BG, guards joining the RANDOM
    // queue while already queued elsewhere, and BattlegroundMgr::IsRandomBattleground is true only for
    // BATTLEGROUND_RB (32) and BATTLEGROUND_RANDOM_EPIC (901) - never for 1101.
    if (!grp
        && (_player->InBattlegroundQueueForBattlegroundQueueType(BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_RB, BattlegroundQueueIdType::Battleground, false, 0))
            || _player->InBattlegroundQueueForBattlegroundQueueType(BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_RANDOM_EPIC, BattlegroundQueueIdType::Battleground, false, 0))))
    {
        sendFailed(ERR_IN_RANDOM_BG);
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

    // Every check above tests the leader only, but AddGroup below enqueues EVERY member. Group::CanJoinBattlegroundQueue
    // is the single place in the tree that re-runs them per member, and the one it adds is the one that matters here:
    // member->InBattlegroundQueueForBattlegroundQueueType(bgQueueTypeId). Without it a player who is already queued solo
    // for Blitz and then joins someone else's party gets a SECOND GroupQueueInfo while the first keeps a pointer to his
    // BattlegroundQueue::PlayerQueueInfo - AddGroup (BattlegroundQueue.cpp) reuses the existing m_QueuedPlayers node and
    // only repoints pl_info.GroupInfo, so the stale ginfo stays in m_QueuedGroups holding a pointer that the next
    // RemovePlayer frees. CheckSoloQueueMatch then counts and selects that dead entry and StartProposal dereferences it.
    // It also supplies the two gates the leader-only checks cannot: an equal bracket for every member (bracketEntry here
    // is derived from the LEADER's level alone) and a free queue slot per member, without which AddBattlegroundQueueId
    // returns PLAYER_MAX_BATTLEGROUND_QUEUES and the member silently never accepts, timing the proposal out every time.
    //
    // isRated = true because Blitz is a rated mode; the only thing that flag gates inside is the arena-team comparison,
    // which is inert on this tree (Player::GetArenaTeamId always returns 0). MinPlayerCount is likewise only read behind
    // bgOrTemplate->IsArena(), which is false for BattlemasterList 1101 - it carries PvpType 0, so GetType() is
    // Battleground (see BattlegroundMgr::IsValidQueueId's RatedBattlegroundBlitz case).
    ObjectGuid errorGuid;
    GroupJoinBattlegroundResult err = ERR_BATTLEGROUND_NONE;
    if (grp)
        err = grp->CanJoinBattlegroundQueue(bgTemplate, bgQueueTypeId, 0, bgTemplate->GetMaxPlayersPerTeam(), true, 0, errorGuid);

    if (err)
    {
        sendFailed(err, &errorGuid);
        return;
    }

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

// CMSG_BATTLEMASTER_JOIN_RATED_SOLO_SHUFFLE (0x3E00BF) - rated 3v3-rounds solo queue. Like Blitz the packet
// carries no queue identity (one Roles byte); the server builds the queue id. Strictly solo (BattlemasterList
// 1065 GroupsAllowed=0), role-balanced into a 6-player lobby by CheckSoloQueueMatch(3,0,healers). This is the
// queue/join half (P0); the 6-round shuffle controller is BattlegroundSoloShuffle.
void WorldSession::HandleBattlemasterJoinRatedSoloShuffle(WorldPackets::Battleground::BattlemasterJoinRatedSoloShuffle& packet)
{
    BattlegroundQueueTypeId bgQueueTypeId =
        BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_SOLO_SHUFFLE, BattlegroundQueueIdType::RatedSoloShuffle, true, 0);

    if (!BattlegroundMgr::IsValidQueueId(bgQueueTypeId))
    {
        TC_LOG_ERROR("network", "Rated Solo Shuffle: queue id rejected by IsValidQueueId - BattlemasterList {} is missing from the client DB2.",
            uint32(BATTLEGROUND_SOLO_SHUFFLE));
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

    BattlegroundTemplate const* bgTemplate = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BATTLEGROUND_SOLO_SHUFFLE);
    if (!bgTemplate)
    {
        TC_LOG_ERROR("bg.battleground", "Rated Solo Shuffle: no battleground_template row for {} - apply the Solo Shuffle world migration.", uint32(BATTLEGROUND_SOLO_SHUFFLE));
        return;
    }

    auto sendFailed = [&](GroupJoinBattlegroundResult result, ObjectGuid const* errorGuid = nullptr)
    {
        WorldPackets::Battleground::BattlefieldStatusFailed battlefieldStatus;
        BattlegroundMgr::BuildBattlegroundStatusFailed(&battlefieldStatus, bgQueueTypeId, _player, 0, result, errorGuid);
        SendPacket(battlefieldStatus.Write());
    };

    if (!_player->GetBGAccessByLevel(BattlegroundTypeId(bgQueueTypeId.BattlemasterListId)))
    {
        sendFailed(ERR_BATTLEGROUND_JOIN_FAILED);
        return;
    }

    PVPDifficultyEntry const* bracketEntry = DB2Manager::GetBattlegroundBracketByLevel(bgTemplate->MapIDs.front(), _player->GetLevel());
    if (!bracketEntry)
        return;

    if (!packet.Roles)
    {
        sendFailed(ERR_BATTLEGROUND_JOIN_FAILED);
        return;
    }

    // Strictly solo: BattlemasterList 1065 carries GroupsAllowed=0, so a premade cannot queue Solo Shuffle.
    if (_player->GetGroup())
    {
        sendFailed(ERR_BATTLEGROUND_JOIN_FAILED);
        return;
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

    // random-BG exclusion on the solo path (same invariant Blitz/HandleBattlemasterJoin carry).
    if (_player->InBattlegroundQueueForBattlegroundQueueType(BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_RB, BattlegroundQueueIdType::Battleground, false, 0))
        || _player->InBattlegroundQueueForBattlegroundQueueType(BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_RANDOM_EPIC, BattlegroundQueueIdType::Battleground, false, 0)))
    {
        sendFailed(ERR_IN_RANDOM_BG);
        return;
    }

    if (_player->GetBattlegroundQueueIndex(bgQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES)
        return;

    if (!_player->HasFreeBattlegroundQueueId())
    {
        sendFailed(ERR_BATTLEGROUND_TOO_MANY_QUEUES);
        return;
    }

    if (_player->HasAura(9454))     // Freeze debuff
        return;

    BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);
    GroupQueueInfo* ginfo = bgQueue.AddGroup(_player, nullptr, Team(_player->GetTeam()), bracketEntry, false, 0, 0, packet.Roles);
    uint32 avgTime = bgQueue.GetAverageQueueWaitTime(ginfo, bracketEntry->GetBracketId());

    uint32 queueSlot = _player->AddBattlegroundQueueId(bgQueueTypeId);
    WorldPackets::Battleground::BattlefieldStatusQueued battlefieldStatus;
    BattlegroundMgr::BuildBattlegroundStatusQueued(&battlefieldStatus, _player, queueSlot, ginfo->JoinTime, bgQueueTypeId, avgTime, false);
    SendPacket(battlefieldStatus.Write());

    TC_LOG_DEBUG("bg.battleground", "Rated Solo Shuffle: {} ({}) queued with roles {:#x}",
        _player->GetName(), _player->GetGUID().ToString(), packet.Roles);

    sBattlegroundMgr->ScheduleQueueUpdate(0, bgQueueTypeId, bracketEntry->GetBracketId());
}

// CMSG_BATTLEMASTER_JOIN_SKIRMISH (0x3E00C1) - unrated 3v3 arena, solo or small group.
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

    // Lower level bound. Unlike its three siblings this queue had no gap to close - BattlemasterList 6
    // carries MinLevel 20 and map 1505's ladder begins at 20-29, so the bracket lookup below already refuses
    // everyone this refuses. It is stated explicitly all the same, for two reasons: it stops the invariant
    // from resting on two DB2 tables happening to agree (the very assumption that produced the hole in the
    // three aggregates), and below level 20 the player now gets a message instead of the lookup's silent
    // return. See HandleBattlemasterJoinRatedBGBlitz for the account of the Reason.
    if (!_player->GetBGAccessByLevel(BattlegroundTypeId(bgQueueTypeId.BattlemasterListId)))
    {
        sendFailed(ERR_BATTLEGROUND_JOIN_FAILED);
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
    // Unrated, so no rating is carried. The role mask narrows the queuer's own PlayerQueueInfo::Role in
    // AddGroup and is not kept beyond that - no skirmish matchmaker consults it, and the wire only ever
    // tells us the queuer's own mask, never the other party members'.
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

// CMSG_JOIN_RATED_BATTLEGROUND (0x3D0025) - the classic 10v10 rated battleground, premade-group only.
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
        BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_RATED_10_VS_10, BattlegroundQueueIdType::Battleground, true, 0);

    if (!BattlegroundMgr::IsValidQueueId(bgQueueTypeId))
    {
        TC_LOG_ERROR("network", "Rated Battleground: queue id rejected by IsValidQueueId - BattlemasterList {} missing from the client DB2.",
            uint32(BATTLEGROUND_RATED_10_VS_10));
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

    BattlegroundTemplate const* bgTemplate = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BATTLEGROUND_RATED_10_VS_10);
    if (!bgTemplate)
    {
        TC_LOG_ERROR("bg.battleground", "Rated Battleground: no battleground_template row for {} - apply the rated-BG world migration.", uint32(BATTLEGROUND_RATED_10_VS_10));
        return;
    }

    auto sendFailed = [&](GroupJoinBattlegroundResult result, ObjectGuid const* errorGuid = nullptr)
    {
        WorldPackets::Battleground::BattlefieldStatusFailed battlefieldStatus;
        BattlegroundMgr::BuildBattlegroundStatusFailed(&battlefieldStatus, bgQueueTypeId, _player, 0, result, errorGuid);
        SendPacket(battlefieldStatus.Write());
    };

    // Lower level bound - BattlemasterList 100 declares MinLevel 50 while map 2107's ladder starts at 10-19.
    // The full account of why the bracket lookup below does not cover this, and why the Reason is 35, is in
    // HandleBattlemasterJoinRatedBGBlitz; do not restate it here.
    if (!_player->GetBGAccessByLevel(BattlegroundTypeId(bgQueueTypeId.BattlemasterListId)))
    {
        sendFailed(ERR_BATTLEGROUND_JOIN_FAILED);
        return;
    }

    PVPDifficultyEntry const* bracketEntry = DB2Manager::GetBattlegroundBracketByLevel(bgTemplate->MapIDs.front(), _player->GetLevel());
    if (!bracketEntry)
        return;

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

    // Rated battlegrounds are a full-roster mode, and the equality - not a range - is what the client itself
    // enforces. Blizzard_PVPUI.lua ConquestFrame_UpdateJoinButton only enables the join button on the
    // 'neededSize == groupSize' branch, with neededSize = CONQUEST_SIZES[RATED_BG_BUTTON_ID]; RATED_BG_BUTTON_ID
    // is 5 and Constants.lua has CONQUEST_SIZES = { 1, 1, 2, 3, 10 }, so the client sends this opcode only for
    // a group of exactly 10. A short group gets a tooltip instead of a packet.
    //
    // 10 is also what the read below resolves to. BattlemasterList 100 'Rated Battleground' in build
    // 12.1.0.69382 (wago.tools, confirmed against c:/dumps/sr_scratch/BattlemasterList.csv) is
    // RatedPlayers 10, MinPlayers 5, MaxPlayers 10, GroupsAllowed 1, MaxGroupSize 10, Flags 0x2 -
    // and GetMaxPlayersPerTeam() reads MaxPlayers. MinPlayers 5 is the number the match may START with once
    // formed, not a queueing size; RatedPlayers and MaxGroupSize both agree on 10, so the gate matches the
    // client whichever of the three the field order is read against.
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

    // check Freeze debuff - same gate as HandleBattlemasterJoinOpcode, HandleBattlemasterJoinRatedBGBlitz,
    // HandleBattlemasterJoinSkirmish, HandleBattlemasterJoinBrawl and HandleBattleFieldPortOpcode.
    // Group::CanJoinBattlegroundQueue below does not cover it - it tests CanJoinToBattleground, faction,
    // bracket, arena team, double queue, random queue, deserter and free slots, no aura. Without this a
    // GM-frozen player is the only one on this tree who can still enter a rated battleground queue.
    if (_player->HasAura(9454))
        return;

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

// CMSG_BATTLEMASTER_JOIN_BRAWL (0x3E00C4) - the rotating PvP Brawl. Body is uint8 Roles + one bit
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
    // The next two rejections happen before any queue identity exists, so they answer with
    // BATTLEGROUND_QUEUE_NONE. That is not a truncated answer: the client's SMSG_BATTLEFIELD_STATUS_FAILED
    // consumer (RVA 0x21C13A0) never reads QueueID at all. It switches on Reason to pick the ERR_* system
    // message and then tail-calls the SMSG_BATTLEFIELD_STATUS_NONE teardown (RVA 0x21BFA80) with NO
    // arguments; that teardown resolves the slot through RVA 0x21BF930, which compares the FULL ticket -
    // RequesterGuid, Type, Id and Time. Ticket.Time comes from Player::GetBattlegroundQueueJoinTime, which
    // returns 0 for a queue the player never joined, so no slot matches, nothing is torn down, and the
    // queues the player really holds are untouched. He just gets the error text.
    // (Decompiled from the 12.1 client this session; see status/battlefield_4B.json beleg for the RVAs.)
    auto sendFailedBeforeQueueKnown = [&](GroupJoinBattlegroundResult result)
    {
        WorldPackets::Battleground::BattlefieldStatusFailed battlefieldStatus;
        BattlegroundMgr::BuildBattlegroundStatusFailed(&battlefieldStatus, BATTLEGROUND_QUEUE_NONE, _player, 0, result);
        SendPacket(battlefieldStatus.Write());
    };

    // We never advertise a special-event brawl (HandleRequestScheduledPvpInfo leaves that block out), so the
    // client has nothing to set this bit for. Retail's captured special-event brawl is an LFGDungeons brawl with
    // no BattlemasterList at all, which this queue could not serve even if it were advertised.
    if (packet.IsSpecialBrawl)
    {
        // UNVERIFIED: that ERR_BATTLEGROUND_JOIN_FAILED is the code retail picks here. No capture holds a
        // rejected brawl join, so only the fact that an answer is owed is established, not its Reason. 35 is
        // the generic "you cannot join" of the client's table (ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â§10.2 of AGENT_BRIEF_BATTLEFIELD_4B.md) and the
        // one every other unmodelled-client rejection in this file already uses.
        sendFailedBeforeQueueKnown(ERR_BATTLEGROUND_JOIN_FAILED);
        return;
    }

    Optional<BattlegroundMgr::ActiveBrawl> brawl = sBattlegroundMgr->GetActiveBrawl();
    if (!brawl)
    {
        // Reachable in normal operation: Brawl.Enabled set to 0 at runtime (worldserver.conf.dist) makes
        // GetActiveBrawl return nothing while the client still holds the last advertised brawl id in
        // dword_7FF72F082BB8 and C_PvP.JoinBrawl keeps sending. Staying silent leaves the player pressing a
        // button that does nothing.
        // UNVERIFIED: the Reason, for the same reason as above.
        sendFailedBeforeQueueKnown(ERR_BATTLEGROUND_JOIN_FAILED);
        return;
    }

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

    auto sendFailed = [&](GroupJoinBattlegroundResult result, ObjectGuid const* errorGuid = nullptr)
    {
        WorldPackets::Battleground::BattlefieldStatusFailed battlefieldStatus;
        BattlegroundMgr::BuildBattlegroundStatusFailed(&battlefieldStatus, bgQueueTypeId, _player, 0, result, errorGuid);
        SendPacket(battlefieldStatus.Write());
    };

    // Lower level bound - Deep Six (BattlemasterList 879) declares MinLevel 50 while map 2106's ladder starts
    // at 10-19. The full account of why the bracket lookup below does not cover this, and why the Reason is
    // 35, is in HandleBattlemasterJoinRatedBGBlitz; do not restate it here. Note that this reads the RUNNING
    // brawl's own BattlemasterList, so a future brawl with a different minimum is gated by its own number.
    if (!_player->GetBGAccessByLevel(BattlegroundTypeId(bgQueueTypeId.BattlemasterListId)))
    {
        sendFailed(ERR_BATTLEGROUND_JOIN_FAILED);
        return;
    }

    PVPDifficultyEntry const* bracketEntry = DB2Manager::GetBattlegroundBracketByLevel(bgTemplate->MapIDs.front(), _player->GetLevel());
    if (!bracketEntry)
        return;

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

        // BattlemasterList.MaxGroupSize is the brawl's own party cap - 6 for Deep Six, which carries
        // MinPlayers 5, MaxPlayers 6, RatedPlayers 6 and MaxGroupSize 6 in build 12.1.0.69382
        // (c:/dumps/sr_scratch/BattlemasterList.csv row 879, wago.tools). The 5 an earlier revision of this
        // comment named is MinPlayers, not the cap; the read below always used the right field.
        if (int32(grp->GetMembersCount()) > battlemasterListEntry->MaxGroupSize)
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

    // Exclusion against the random-battleground queue on the solo path, exactly as in
    // HandleBattlemasterJoinRatedBGBlitz; the account of why the group path already has it and this one did
    // not is there, do not restate it here. It applies unchanged to a brawl: BattlemasterList 879 is neither
    // BATTLEGROUND_AA nor a random battleground, so Group::CanJoinBattlegroundQueue does refuse a party
    // member who is in the random queue, and nothing refused the same player queueing alone.
    if (!grp
        && (_player->InBattlegroundQueueForBattlegroundQueueType(BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_RB, BattlegroundQueueIdType::Battleground, false, 0))
            || _player->InBattlegroundQueueForBattlegroundQueueType(BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_RANDOM_EPIC, BattlegroundQueueIdType::Battleground, false, 0))))
    {
        sendFailed(ERR_IN_RANDOM_BG);
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

    // isPremade stays false, for the same reason it stays false in HandleBattlemasterJoinOpcode: a brawl queue is
    // unrated, so a true flag would file the entry in BG_QUEUE_PREMADE_*, where only CheckPremadeMatch looks and
    // only when both factions have a premade waiting. Deep Six (BattlemasterList 879) carries MinPlayers 5 at
    // MaxGroupSize 6, so a full party would otherwise be invisible to CheckNormalMatch for up to
    // Battleground.PremadeGroupWaitForMatch (30 min).
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

    TC_LOG_DEBUG("bg.battleground", "Brawl: {} ({}) queued for PvpBrawl {} (BattlemasterList {}) with roles {:#x}",
        _player->GetName(), _player->GetGUID().ToString(), brawl->PvpBrawlId, bgQueueTypeId.BattlemasterListId, packet.Roles);

    sBattlegroundMgr->ScheduleQueueUpdate(0, bgQueueTypeId, bracketEntry->GetBracketId());
}

// CMSG_BATTLEMASTER_JOIN_RATED_BG_BLITZ (0x3B00BE) - rated 8v8 solo/duo queue.
//
// Unlike CMSG_BATTLEMASTER_JOIN, this packet carries NO queue identity: the client sends a single role-mask
// byte and the mode is implied entirely by the opcode. The server therefore builds the queue id itself. The
// value used here is not invented - a live 12.0.7.68275 capture shows retail replying to this exact opcode
// with SMSG_BATTLEFIELD_STATUS_QUEUED carrying packed QueueID 0x1F1000000019044D, which decodes to
// { BattlemasterListId = 1101, Type = 9, Rated = true, TeamSize = 0 }.
// CMSG_BATTLEMASTER_JOIN_SKIRMISH (0x3B00BF) - unrated 3v3 arena, solo or small group.
//
// Like the Blitz join this packet carries no queue identity; the mode is implied by the opcode. It queues
// against BattlemasterList 6 ("All Arenas"), which already has a battleground_template row and whose 15
// arena maps all have templates - so this phase needs no SQL.
//
// Matchmaking needs no new code either: the queue is unrated, so entries land in BG_QUEUE_NORMAL_* and the
// existing `!m_queueId.Rated` branch of BattlegroundQueueUpdate already runs
// CheckNormalMatch(...) || (IsArena && CheckSkirmishForSameFaction(...)).
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

void WorldSession::HandleSurrenderArena(WorldPackets::Battleground::SurrenderArena& /*surrenderArena*/)
{
    // Arena forfeit: the sender concedes an in-progress arena match. Their team is marked as the loser and the
    // match ends through the standard arena end path (winner = opposing team), which applies the normal rating
    // change / MMR update for both teams exactly like a fought-out loss.
    Battleground* bg = _player->GetBattleground();
    if (!bg || !bg->isArena() || bg->GetStatus() != STATUS_IN_PROGRESS)
        return;

    Team const surrenderingTeam = bg->GetPlayerTeam(_player->GetGUID());
    if (surrenderingTeam != ALLIANCE && surrenderingTeam != HORDE)
        return;

    bg->EndBattleground(GetOtherTeam(surrenderingTeam));
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

    // The queue template's first map, and NOT bg->GetMapId(), which is what upstream substituted here once the
    // instance was known. bracketEntry below is used for exactly one thing - the ScheduleQueueUpdate on the
    // leave path - and that call has to name the bracket the QUEUE is keyed by. For an aggregate
    // BattlemasterList the drawn map is a different map with its own PVPDifficulty numbering, so resolving the
    // bracket there would schedule an update for a bracket this queue holds nobody in, and the leave would
    // never re-evaluate the bracket it actually emptied. BattlegroundMgr::CreateNewBattleground now settles the
    // same question the same way. For a single-map queue this is bit-for-bit the map upstream used.
    uint32 const mapId = bgTemplate->MapIDs.front();

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
            BattlegroundMgr::BuildBattlegroundStatusNeedConfirmation(&battlefieldStatus, bg, _player, i, _player->GetBattlegroundQueueJoinTime(bgQueueTypeId), getMSTimeDiff(GameTime::GetGameTimeMS(), ginfo.RemoveInviteTime), bgQueueTypeId,
                bgQueue.GetPlayerRole(_player->GetGUID()));
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
        // The LENGTH of the window is retail's, measured, not chosen. 70 bodies of this opcode across nine
        // builds in C:/sniff were resolved to absolute instants (PKT startedTime + (tick - startedTickCount)),
        // and every single one lands on the next weekly reset of its own region: builds 67186, 67314, 68275,
        // 68453, 68974 and 69273 all target Tuesday 15:00-15:01 UTC (US), builds 69299, 69382 and 69404 all
        // target Wednesday 05:52-05:54 UTC (EU). The minute of jitter is the sniffer's tick-to-wallclock
        // drift, not the field: within one capture the value counts down second for second toward one fixed
        // instant - e.g. build 68275, 18 bodies between 18:11 and 23:40 on 2026-07-06, secs 74903 down to
        // 55200, all pointing at 2026-07-07 15:00. So SecondsUntilNextChange is "seconds until the weekly
        // reset", which is exactly what GetNextWeeklyQuestsResetTime yields.
        // The brawl id rotates with that window in retail (8, 11, 120, 6, 10, 9 over the nine builds); ours
        // is a fixed configuration instead, so re-asking after the reset gets a fresh window on the same
        // brawl. That is a content decision, not a wire one.
        time_t const now = GameTime::GetGameTime();
        time_t const nextChange = sWorld->GetNextWeeklyQuestsResetTime();
        info.SecondsUntilNextChange = nextChange > now ? uint32(nextChange - now) : uint32(WEEK);
    }

    SendPacket(response.Write());
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
    pvpOptionsEnabled.RatedSoloShuffle = true;      // HandleBattlemasterJoinRatedSoloShuffle now really queues
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

namespace
{
    // A pending war-game challenge, keyed by the opposing group leader who must accept it. War games are
    // ephemeral (no persistence): the challenge lives only until the opponent answers or logs off. These
    // handlers run on the world update thread (PROCESS_THREADUNSAFE), so a plain map needs no locking.
    struct WargamePendingRequest
    {
        ObjectGuid Initiator;
        uint64 QueueID = 0;
        uint32 BattlemasterListID = 0;
        uint16 Bracket = 0;
        bool TournamentRules = false;
    };

    std::unordered_map<ObjectGuid /*opponentLeader*/, WargamePendingRequest> g_wargameRequests;
}

void WorldSession::HandleStartWarGame(WorldPackets::Battleground::StartWarGame& packet)
{
    Player* initiator = _player;

    // Only a party leader may issue a war-game challenge on behalf of their group.
    Group* group = initiator->GetGroup();
    if (!group || group->GetLeaderGUID() != initiator->GetGUID())
        return;

    // Resolve the opposing group from the named member, then its leader (who receives the prompt).
    Player* opposingMember = ObjectAccessor::FindConnectedPlayer(packet.OpposingPartyMember);
    if (!opposingMember)
        return;

    Group* opposingGroup = opposingMember->GetGroup();
    if (!opposingGroup)
        return;

    ObjectGuid opposingLeaderGuid = opposingGroup->GetLeaderGUID();
    if (opposingGroup == group)
        return; // cannot war-game your own group

    Player* opposingLeader = ObjectAccessor::FindConnectedPlayer(opposingLeaderGuid);
    if (!opposingLeader)
        return;

    // Register the pending challenge so the opponent's acceptance can be correlated back.
    WargamePendingRequest& req = g_wargameRequests[opposingLeaderGuid];
    req.Initiator = initiator->GetGUID();
    req.QueueID = packet.QueueID;
    req.BattlemasterListID = packet.BattlemasterListID;
    req.Bracket = packet.Bracket;
    req.TournamentRules = packet.TournamentRules;

    // Prompt the opposing leader to accept or decline.
    WorldPackets::Battleground::CheckWargameEntry check;
    check.OpposingPartyMember = initiator->GetGUID();
    check.QueueID = packet.QueueID;
    check.Time = 0;
    check.TournamentRules = packet.TournamentRules;
    opposingLeader->SendDirectMessage(check.Write());

    // Confirm to the initiator that the challenge was delivered.
    WorldPackets::Battleground::WargameRequestSuccessfullySentToOpponent sent;
    sent.OpposingPartyMember = opposingLeaderGuid;
    initiator->SendDirectMessage(sent.Write());
}

void WorldSession::HandleAcceptWargameInvite(WorldPackets::Battleground::AcceptWargameInvite& packet)
{
    Player* responder = _player;

    auto itr = g_wargameRequests.find(responder->GetGUID());
    if (itr == g_wargameRequests.end())
        return; // no outstanding challenge for this player

    WargamePendingRequest const req = itr->second;
    g_wargameRequests.erase(itr);

    // The invite being answered must match the challenger we recorded.
    if (packet.OpposingPartyMember != req.Initiator || packet.QueueID != req.QueueID)
        return;

    // Report the outcome to the challenger.
    Player* initiator = ObjectAccessor::FindConnectedPlayer(req.Initiator);
    if (initiator)
    {
        WorldPackets::Battleground::WargameRequestOpponentResponse response;
        response.OpposingPartyMember = responder->GetGUID();
        response.Accepted = packet.Accept;
        initiator->SendDirectMessage(response.Write());
    }

    // P1: on acceptance, spin up a war-game battleground/arena instance and port both groups in. This mirrors the
    // rated-arena start path (create bg -> queue each side -> invite -> start), but the two premade groups are
    // known up front so no matchmaking is involved; the challenger's side is forced ALLIANCE, the opponent HORDE.
    if (!packet.Accept || !initiator)
        return;

    Group* initiatorGroup = initiator->GetGroup();
    Group* responderGroup = responder->GetGroup();
    if (!initiatorGroup || !responderGroup || initiatorGroup == responderGroup)
        return;
    // Both must still be led by the players who arranged the match.
    if (initiatorGroup->GetLeaderGUID() != initiator->GetGUID() || responderGroup->GetLeaderGUID() != responder->GetGUID())
        return;
    // Neither side may already be in a battleground.
    if (initiator->InBattleground() || responder->InBattleground())
        return;

    // Resolve the chosen battleground/arena from the recorded challenge.
    uint16 battlemasterListId = uint16(req.BattlemasterListID);
    BattlemasterListEntry const* battlemasterList = sBattlemasterListStore.LookupEntry(battlemasterListId);
    if (!battlemasterList)
        return;

    BattlegroundTypeId bgTypeId = BattlegroundTypeId(battlemasterListId);
    BattlegroundTemplate const* bgTemplate = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(bgTypeId);
    if (!bgTemplate)
        return;

    // Arena war games carry a team size (derived from the challenging group); battleground war games use 0.
    uint8 teamSize = 0;
    if (bgTemplate->IsArena())
        teamSize = uint8(std::max<uint32>(1, initiatorGroup->GetMembersCount()));

    BattlegroundQueueTypeId queueId = BattlegroundMgr::BGQueueTypeId(battlemasterListId, BattlegroundQueueIdType::Wargame, false, teamSize);

    PVPDifficultyEntry const* bracketEntry = DB2Manager::GetBattlegroundBracketByLevel(bgTemplate->MapIDs.front(), initiator->GetLevel());
    if (!bracketEntry)
        return;

    Battleground* bg = sBattlegroundMgr->CreateNewBattleground(queueId, bracketEntry->GetBracketId());
    if (!bg)
        return;

    BattlegroundQueue& queue = sBattlegroundMgr->GetBattlegroundQueue(queueId);
    queue.AddWargameSide(initiator, initiatorGroup, bg, bracketEntry, ALLIANCE);
    queue.AddWargameSide(responder, responderGroup, bg, bracketEntry, HORDE);

    // Register the instance and open it; both sides now hold an enter-confirmation and port in via the existing
    // CMSG_BATTLEFIELD_PORT -> SendToBattleground handshake.
    bg->StartBattleground();
}
