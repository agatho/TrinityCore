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

#include "BattlegroundMgr.h"
#include "Arena.h"
#include "BattlegroundPackets.h"
#include "Containers.h"
#include "DB2Stores.h"
#include "DatabaseEnv.h"
#include "DisableMgr.h"
#include "GameEventMgr.h"
#include "GossipDef.h"
#include "Language.h"
#include "Log.h"
#include "MapManager.h"
#include "MapUtils.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SharedDefines.h"
#include "World.h"
#include "WorldSession.h"

bool BattlegroundTemplate::IsArena() const
{
    return BattlemasterEntry->GetType() == BattlemasterType::Arena;
}

uint16 BattlegroundTemplate::GetMinPlayersPerTeam() const
{
    return BattlemasterEntry->MinPlayers;
}

uint16 BattlegroundTemplate::GetMaxPlayersPerTeam() const
{
    return BattlemasterEntry->MaxPlayers;
}

uint8 BattlegroundTemplate::GetMinLevel() const
{
    return BattlemasterEntry->MinLevel;
}

uint8 BattlegroundTemplate::GetMaxLevel() const
{
    return BattlemasterEntry->MaxLevel;
}

/*********************************************************/
/***            BATTLEGROUND MANAGER                   ***/
/*********************************************************/

BattlegroundMgr::BattlegroundMgr() :
    m_NextRatedArenaUpdate(sWorld->getIntConfig(CONFIG_ARENA_RATED_UPDATE_TIMER)),
    m_UpdateTimer(0), m_ArenaTesting(0), m_Testing(false)
{ }

BattlegroundMgr::~BattlegroundMgr()
{
    DeleteAllBattlegrounds();
}

void BattlegroundMgr::DeleteAllBattlegrounds()
{
    for (auto& [_, data] : m_BGFreeSlotQueue)
        for (Battleground* battleground : data)
            battleground->RemoveFromBGFreeSlotQueueOnShutdown();

    bgDataStore.clear();
    m_BGFreeSlotQueue.clear();
}

BattlegroundMgr* BattlegroundMgr::instance()
{
    static BattlegroundMgr instance;
    return &instance;
}

// used to update running battlegrounds, and delete finished ones
void BattlegroundMgr::Update(uint32 diff)
{
    m_UpdateTimer += diff;
    if (m_UpdateTimer > BATTLEGROUND_OBJECTIVE_UPDATE_INTERVAL)
    {
        for (BattlegroundDataContainer::iterator itr1 = bgDataStore.begin(); itr1 != bgDataStore.end(); ++itr1)
        {
            BattlegroundContainer& bgs = itr1->second.m_Battlegrounds;
            BattlegroundContainer::iterator itrDelete = bgs.begin();
            for (BattlegroundContainer::iterator itr = itrDelete; itr != bgs.end();)
            {
                itrDelete = itr++;
                Battleground* bg = itrDelete->second.get();

                bg->Update(m_UpdateTimer);
                if (bg->ToBeDeleted())
                {
                    BattlegroundClientIdsContainer& clients = itr1->second.m_ClientBattlegroundIds[bg->GetBracketId()];
                    if (!clients.empty())
                        clients.erase(bg->GetClientInstanceID());

                    // move out unique_ptr to delete after erasing
                    Trinity::unique_trackable_ptr<Battleground> bgPtr = std::move(itrDelete->second);

                    bgs.erase(itrDelete);
                }
            }
        }

        m_UpdateTimer = 0;
    }

    // update events timer
    for (std::pair<BattlegroundQueueTypeId const, BattlegroundQueue>& pair : m_BattlegroundQueues)
        pair.second.UpdateEvents(diff);

    // update scheduled queues
    if (!m_QueueUpdateScheduler.empty())
    {
        std::vector<ScheduledQueueUpdate> scheduled;
        std::swap(scheduled, m_QueueUpdateScheduler);

        for (auto& [arenaMMRating, bgQueueTypeId, bracket_id] : scheduled)
            GetBattlegroundQueue(bgQueueTypeId).BattlegroundQueueUpdate(diff, bracket_id, arenaMMRating);
    }

    // The periodic sweep of the rated queues. Arena.RatedUpdateTimer is the tick for all three of them; a
    // realm that sets it to 0 has switched the sweep off altogether and every rated queue is then moved only
    // by the ScheduleQueueUpdate a join or a leave triggers.
    if (uint32 const ratedUpdateTimer = sWorld->getIntConfig(CONFIG_ARENA_RATED_UPDATE_TIMER))
    {
        // it's time to force update
        if (m_NextRatedArenaUpdate < diff)
        {
            // The arena scan keeps the extra condition it has carried upstream for years. That condition is
            // odd - GetMaxRatingDifference() turns a configured 0 into 5000, so Arena.MaxRatingDifference = 0
            // does not actually disable rated arena matching, it only widens it - but rated arena is not this
            // unit's subject and its sweep is left exactly as it was found.
            if (sWorld->getIntConfig(CONFIG_ARENA_MAX_RATING_DIFFERENCE))
            {
                // forced update for rated arenas (scan all, but skipped non rated)
                TC_LOG_TRACE("bg.arena", "BattlegroundMgr: UPDATING ARENA QUEUES");
                for (uint8 teamSize : { ARENA_TYPE_2v2, ARENA_TYPE_3v3, ARENA_TYPE_5v5 })
                {
                    BattlegroundQueueTypeId ratedArenaQueueId = BGQueueTypeId(BATTLEGROUND_AA, BattlegroundQueueIdType::Arena, true, teamSize);
                    for (int bracket = BG_BRACKET_ID_FIRST; bracket < MAX_BATTLEGROUND_BRACKETS; ++bracket)
                        GetBattlegroundQueue(ratedArenaQueueId).BattlegroundQueueUpdate(diff, BattlegroundBracketId(bracket), 0);
                }
            }

            // Battleground Blitz is a rated queue too and needs the same periodic sweep. Without it the
            // queue is only ever re-evaluated by the ScheduleQueueUpdate a join or a leave triggers, so a
            // queue that could not form a match at join time (for example the healer quota was not met yet)
            // would sit idle until the next player happens to join, rather than retrying on its own.
            // Deliberately NOT under the Arena.MaxRatingDifference condition above: CheckSoloQueueMatch
            // reads no rating at all, it orders by JoinTime and fills a role quota, so that option says
            // nothing about this queue and switching it off must not switch this sweep off with it.
            BattlegroundQueueTypeId blitzQueueId = BGQueueTypeId(BATTLEGROUND_BLITZ, BattlegroundQueueIdType::RatedBattlegroundBlitz, true, 0);
            for (int bracket = BG_BRACKET_ID_FIRST; bracket < MAX_BATTLEGROUND_BRACKETS; ++bracket)
                GetBattlegroundQueue(blitzQueueId).BattlegroundQueueUpdate(diff, BattlegroundBracketId(bracket), 0);

            // Rated battlegrounds are matched by CheckPremadeMatch, which only runs when something
            // schedules an update. Sweep it on the same timer so a queue that could not pair two
            // premades at join time retries instead of sitting idle. CheckPremadeMatch reads no rating
            // either, so the same reasoning as for Blitz applies.
            BattlegroundQueueTypeId ratedBgQueueId = BGQueueTypeId(BATTLEGROUND_RATED_10_VS_10, BattlegroundQueueIdType::Battleground, true, 0);
            for (int bracket = BG_BRACKET_ID_FIRST; bracket < MAX_BATTLEGROUND_BRACKETS; ++bracket)
                GetBattlegroundQueue(ratedBgQueueId).BattlegroundQueueUpdate(diff, BattlegroundBracketId(bracket), 0);

            m_NextRatedArenaUpdate = ratedUpdateTimer;
        }
        else
            m_NextRatedArenaUpdate -= diff;
    }
}

void BattlegroundMgr::BuildBattlegroundStatusHeader(WorldPackets::Battleground::BattlefieldStatusHeader* header, Player const* player, uint32 ticketId, uint32 joinTime, BattlegroundQueueTypeId queueId)
{
    header->Ticket.RequesterGuid = player->GetGUID();
    header->Ticket.Id = ticketId;
    header->Ticket.Type = WorldPackets::LFG::RideType::Battlegrounds;
    header->Ticket.Time = joinTime;
    header->QueueID.push_back(queueId.GetPacked());
    header->RangeMin = 0; // seems to always be 0
    header->RangeMax = GetMaxLevelForExpansion(CURRENT_EXPANSION); // alwyas max level of current expansion. Might be limited to account
    header->TeamSize = queueId.TeamSize;
    header->InstanceID = 0; // seems to always be 0
    header->RegisteredMatch = queueId.Rated;
    header->TournamentRules = false;
}

void BattlegroundMgr::BuildBattlegroundStatusNone(WorldPackets::Battleground::BattlefieldStatusNone* battlefieldStatus, Player const* player, uint32 ticketId, uint32 joinTime)
{
    battlefieldStatus->Ticket.RequesterGuid = player->GetGUID();
    battlefieldStatus->Ticket.Id = ticketId;
    battlefieldStatus->Ticket.Type = WorldPackets::LFG::RideType::Battlegrounds;
    battlefieldStatus->Ticket.Time = joinTime;
}

void BattlegroundMgr::BuildBattlegroundStatusNeedConfirmation(WorldPackets::Battleground::BattlefieldStatusNeedConfirmation* battlefieldStatus, Battleground const* bg, Player const* player, uint32 ticketId, uint32 joinTime, uint32 timeout, BattlegroundQueueTypeId queueId)
{
    BuildBattlegroundStatusHeader(&battlefieldStatus->Hdr, player, ticketId, joinTime, queueId);
    battlefieldStatus->Mapid = bg->GetMapId();
    battlefieldStatus->Timeout = timeout;
    battlefieldStatus->Role = 0;
}

void BattlegroundMgr::BuildBattlegroundStatusActive(WorldPackets::Battleground::BattlefieldStatusActive* battlefieldStatus, Battleground const* bg, Player const* player, uint32 ticketId, uint32 joinTime, BattlegroundQueueTypeId queueId)
{
    BuildBattlegroundStatusHeader(&battlefieldStatus->Hdr, player, ticketId, joinTime, queueId);
    battlefieldStatus->ShutdownTimer = bg->GetRemainingTime();
    battlefieldStatus->ArenaFaction = player->GetBGTeam() == HORDE ? PVP_TEAM_HORDE : PVP_TEAM_ALLIANCE;
    battlefieldStatus->LeftEarly = false;
    battlefieldStatus->StartTimer = bg->GetElapsedTime();
    battlefieldStatus->Mapid = bg->GetMapId();
}

void BattlegroundMgr::BuildBattlegroundStatusQueued(WorldPackets::Battleground::BattlefieldStatusQueued* battlefieldStatus, Player const* player, uint32 ticketId, uint32 joinTime, BattlegroundQueueTypeId queueId, uint32 avgWaitTime, bool asGroup)
{
    BuildBattlegroundStatusHeader(&battlefieldStatus->Hdr, player, ticketId, joinTime, queueId);
    battlefieldStatus->AverageWaitTime = avgWaitTime;
    battlefieldStatus->AsGroup = asGroup;
    battlefieldStatus->SuspendedQueue = false;
    battlefieldStatus->EligibleForMatchmaking = true;
    battlefieldStatus->WaitTime = GetMSTimeDiffToNow(joinTime);
    // Retail populates this: the sniffed reply to a Battleground Blitz join carried SpecSelected 257
    // (Holy Priest) for a healer-flagged queuer. It was previously left at 0 for every queue.
    battlefieldStatus->SpecSelected = AsUnderlyingType(player->GetPrimarySpecialization());
}

void BattlegroundMgr::BuildBattlegroundStatusFailed(WorldPackets::Battleground::BattlefieldStatusFailed* battlefieldStatus, BattlegroundQueueTypeId queueId, Player const* player, uint32 ticketId, GroupJoinBattlegroundResult result, ObjectGuid const* errorGuid /*= nullptr*/)
{
    battlefieldStatus->Ticket.RequesterGuid = player->GetGUID();
    battlefieldStatus->Ticket.Id = ticketId;
    battlefieldStatus->Ticket.Type = WorldPackets::LFG::RideType::Battlegrounds;
    battlefieldStatus->Ticket.Time = player->GetBattlegroundQueueJoinTime(queueId);
    battlefieldStatus->QueueID = queueId.GetPacked();
    battlefieldStatus->Reason = result;
    if (errorGuid && (result == ERR_BATTLEGROUND_NOT_IN_BATTLEGROUND || result == ERR_BATTLEGROUND_JOIN_TIMED_OUT))
        battlefieldStatus->ClientID = *errorGuid;
}

void BattlegroundMgr::BuildBattlegroundStatusWaitForGroups(WorldPackets::Battleground::BattlefieldStatusWaitForGroups* battlefieldStatus, Player const* player, uint32 ticketId, uint32 joinTime, BattlegroundQueueTypeId queueId,
    uint32 mapId, uint32 timeout, std::array<uint8, 2> const& slotsPerSide, std::array<uint8, 2> const& awaitedPerSide, WorldPackets::Battleground::PvpRoleQueueCounts const& roles)
{
    BuildBattlegroundStatusHeader(&battlefieldStatus->Hdr, player, ticketId, joinTime, queueId);
    battlefieldStatus->Mapid = mapId;
    battlefieldStatus->Timeout = timeout;
    battlefieldStatus->SlotsPerSide = slotsPerSide;
    battlefieldStatus->AwaitedPerSide = awaitedPerSide;
    battlefieldStatus->Roles = roles;
}

void BattlegroundMgr::BuildBattlegroundStatusGroupProposalFailed(WorldPackets::Battleground::BattlefieldStatusGroupProposalFailed* battlefieldStatus, Player const* player, uint32 ticketId, uint32 joinTime, BattlegroundQueueTypeId queueId,
    WorldPackets::Battleground::PvpRoleQueueCounts const& roles)
{
    BuildBattlegroundStatusHeader(&battlefieldStatus->Hdr, player, ticketId, joinTime, queueId);
    battlefieldStatus->Roles = roles;
}

void BattlegroundMgr::PortPlayerToBattleground(Player* player, Battleground* bg, Team team, BattlegroundQueueTypeId queueId, uint32 ticketId)
{
    if (!player->InBattleground())
        player->SetBattlegroundEntryPoint();

    // Resurrect the player. Both callers reach this - the ordinary accept in WorldSession::HandleBattleFieldPortOpcode
    // and BattlegroundQueue::ResolveProposal for the solo queue - so a corpse entering is handled the same way
    // whichever queue it came from. See the note in HandleBattleFieldPortOpcode for why no path refuses the port
    // of a dead player instead.
    if (!player->IsAlive())
    {
        player->ResurrectPlayer(1.0f);
        player->SpawnCorpseBones();
    }
    // stop taxi flight at port
    player->FinishTaxiFlight();

    WorldPackets::Battleground::BattlefieldStatusActive battlefieldStatus;
    BuildBattlegroundStatusActive(&battlefieldStatus, bg, player, ticketId, player->GetBattlegroundQueueJoinTime(queueId), queueId);
    player->SendDirectMessage(battlefieldStatus.Write());

    // remove battleground queue status from BGmgr
    sBattlegroundMgr->GetBattlegroundQueue(queueId).RemovePlayer(player->GetGUID(), false);
    // this is still needed here if battleground "jumping" shouldn't add deserter debuff
    // also this is required to prevent stuck at old battleground after SetBattlegroundId set to new
    if (Battleground* currentBg = player->GetBattleground())
        currentBg->RemovePlayerAtLeave(player->GetGUID(), false, true);

    // set the destination instance id
    player->SetBattlegroundId(bg->GetInstanceID(), bg->GetTypeID(), queueId);
    // set the destination team
    player->SetBGTeam(team);

    SendToBattleground(player, bg);

    TC_LOG_DEBUG("bg.battleground", "Battleground: player {} ({}) joined battle for bg {}, bgtype {}, queue {{ BattlemasterListId: {}, Type: {}, Rated: {}, TeamSize: {} }}.",
        player->GetName(), player->GetGUID().ToString(), bg->GetInstanceID(), bg->GetTypeID(),
        queueId.BattlemasterListId, uint32(queueId.Type), queueId.Rated ? "true" : "false", uint32(queueId.TeamSize));
}

Battleground* BattlegroundMgr::GetBattleground(uint32 instanceId, BattlegroundTypeId bgTypeId)
{
    if (!instanceId)
        return nullptr;

    BattlegroundDataContainer::const_iterator begin, end;

    // An aggregate BattlemasterList (Blitz 1101, Rated Battleground 100, the random-BG pools, the multi-map
    // brawls, All Arenas) is never the key its own instances are stored under: CreateNewBattleground resolves
    // it through GetRandomBG to one of its maps and AddBattleground files the instance under
    // bg->GetTypeID(), which is that map's single-map template id. Callers that only have the queue id -
    // WorldSession::HandleBattleFieldPortOpcode, BGQueueInviteEvent/BGQueueRemoveEvent::Execute,
    // BattlegroundQueue::RemovePlayer - would otherwise look in bgDataStore[1101], which
    // CreateClientVisibleInstanceId creates but nothing ever fills, and get a null back. The instance id is
    // generated by MapManager::GenerateInstanceId and is unique across all types, so the wider search cannot
    // return the wrong battleground.
    if (bgTypeId == BATTLEGROUND_TYPE_NONE || IsRandomBattleground(bgTypeId) || IsAggregateBattleground(bgTypeId))
    {
        begin = bgDataStore.begin();
        end = bgDataStore.end();
    }
    else
    {
        end = bgDataStore.find(bgTypeId);
        if (end == bgDataStore.end())
            return nullptr;
        begin = end++;
    }

    for (BattlegroundDataContainer::const_iterator it = begin; it != end; ++it)
    {
        BattlegroundContainer const& bgs = it->second.m_Battlegrounds;
        BattlegroundContainer::const_iterator itr = bgs.find(instanceId);
        if (itr != bgs.end())
           return itr->second.get();
    }

    return nullptr;
}

void BattlegroundMgr::LoadBattlegroundScriptTemplate()
{
    uint32 oldMSTime = getMSTime();
    //                                               0      1                   2
    QueryResult result = WorldDatabase.Query("SELECT MapId, BattlemasterListId, ScriptName FROM battleground_scripts");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 battleground scripts. DB table `battleground_scripts` is empty!");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 mapID = fields[0].GetUInt32();

        MapEntry const* mapEntry = sMapStore.LookupEntry(mapID);
        if (!mapEntry || !mapEntry->IsBattlegroundOrArena())
        {
            TC_LOG_ERROR("sql.sql", "BattlegroundMgr::LoadBattlegroundScriptTemplate: bad mapid {}! Map doesn't exist or is not a battleground/arena!", mapID);
            continue;
        }

        BattlegroundTypeId bgTypeId = static_cast<BattlegroundTypeId>(fields[1].GetUInt32());
        if (bgTypeId != BATTLEGROUND_TYPE_NONE && !Trinity::Containers::MapGetValuePtr(_battlegroundTemplates, bgTypeId))
        {
            TC_LOG_ERROR("sql.sql", "BattlegroundMgr::LoadBattlegroundScriptTemplate: bad battlemasterlist id {}! Battleground doesn't exist or is not supported in battleground_template!", bgTypeId);
            continue;
        }

        BattlegroundScriptTemplate& scriptTemplate = _battlegroundScriptTemplates[{ mapID, bgTypeId }];
        scriptTemplate.MapId = mapID;
        scriptTemplate.Id = bgTypeId;
        scriptTemplate.ScriptId = sObjectMgr->GetScriptId(fields[2].GetStringView());

        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} battleground scripts in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

BattlegroundScriptTemplate const* BattlegroundMgr::FindBattlegroundScriptTemplate(uint32 mapId, BattlegroundTypeId bgTypeId) const
{
    if (BattlegroundScriptTemplate const* scriptTemplate = Trinity::Containers::MapGetValuePtr(_battlegroundScriptTemplates, { mapId, bgTypeId }))
        return scriptTemplate;

    // fall back to 0 for no specific battleground type id
    return Trinity::Containers::MapGetValuePtr(_battlegroundScriptTemplates, { mapId, BATTLEGROUND_TYPE_NONE });
}

void BattlegroundMgr::QueuePlayerForArena(Player const* player, uint8 teamSize, uint8 roles)
{
    WorldPackets::Battleground::BattlemasterJoinArena packet((WorldPacket(CMSG_BATTLEMASTER_JOIN_ARENA)));
    packet.TeamSizeIndex = teamSize;
    packet.Roles = roles;

    player->GetSession()->HandleBattlemasterJoinArena(packet);
}

uint32 BattlegroundMgr::CreateClientVisibleInstanceId(BattlegroundTypeId bgTypeId, BattlegroundBracketId bracket_id)
{
    if (IsArenaType(bgTypeId))
        return 0;                                           //arenas don't have client-instanceids

    // we create here an instanceid, which is just for
    // displaying this to the client and without any other use..
    // the client-instanceIds are unique for each battleground-type
    // the instance-id just needs to be as low as possible, beginning with 1
    // the following works, because std::set is default ordered with "<"
    // the optimalization would be to use as bitmask std::vector<uint32> - but that would only make code unreadable

    BattlegroundClientIdsContainer& clientIds = bgDataStore[bgTypeId].m_ClientBattlegroundIds[bracket_id];
    uint32 lastId = 0;
    for (BattlegroundClientIdsContainer::const_iterator itr = clientIds.begin(); itr != clientIds.end();)
    {
        if ((++lastId) != *itr)                             //if there is a gap between the ids, we will break..
            break;
        lastId = *itr;
    }

    clientIds.insert(++lastId);
    return lastId;
}

// create a new battleground that will really be used to play
Battleground* BattlegroundMgr::CreateNewBattleground(BattlegroundQueueTypeId queueId, BattlegroundBracketId bracketId)
{
    // The bracket belongs to the QUEUE, not to the map that is about to be drawn. Every join handler derives
    // it with GetBattlegroundBracketByLevel(queueTemplate->MapIDs.front(), level), the queue stores its groups
    // under that id, and BattlegroundQueue reads bg->GetBracketId() back as if it were the same number - in
    // the free-slot loop (`bg->GetBracketId() == bracket_id`), in PlayerInvitedToBGUpdateAverageWaitTime and
    // in both ScheduleQueueUpdate calls. Resolving it a second time on the drawn map, as this used to do,
    // breaks that identity for every aggregate whose maps have unequal PVPDifficulty ladders, and fails
    // outright when the drawn map has no such RangeIndex at all. So: look the bracket up once, on the same
    // map the queue used, and let GetRandomBG restrict the draw to maps that can host it.
    BattlegroundTypeId const queueTypeId = BattlegroundTypeId(queueId.BattlemasterListId);
    BattlegroundTemplate const* queueTemplate = GetBattlegroundTemplateByTypeId(queueTypeId);
    if (!queueTemplate || queueTemplate->MapIDs.empty())
    {
        TC_LOG_ERROR("bg.battleground", "Battleground: CreateNewBattleground - queue template not found (or has no maps) for {}", queueTypeId);
        return nullptr;
    }

    PVPDifficultyEntry const* bracketEntry = DB2Manager::GetBattlegroundBracketById(queueTemplate->MapIDs.front(), bracketId);
    if (!bracketEntry)
    {
        TC_LOG_ERROR("bg.battleground", "Battleground: CreateNewBattleground: bg bracket entry not found for map {} bracket id {}", queueTemplate->MapIDs.front(), bracketId);
        return nullptr;
    }

    BattlegroundTypeId bgTypeId = GetRandomBG(queueTypeId, bracketEntry);

    // get the template BG
    BattlegroundTemplate const* bg_template = GetBattlegroundTemplateByTypeId(bgTypeId);

    if (!bg_template)
    {
        TC_LOG_ERROR("bg.battleground", "Battleground: CreateNewBattleground - no runnable map for {} in level range {}-{} (bracket id {})",
            queueTypeId, uint32(bracketEntry->MinLevel), uint32(bracketEntry->MaxLevel), bracketId);
        return nullptr;
    }

    Battleground* bg = nullptr;
    if (bg_template->IsArena())
        bg = new Arena(bg_template);
    else
        bg = new Battleground(bg_template);

    bg->SetBracket(bracketEntry);
    bg->SetInstanceID(sMapMgr->GenerateInstanceId());
    // Numbered under the DRAWN type, not under the queue's aggregate id, because that is the key the number is
    // given back under: BattlegroundMgr::Update frees it from itr1->second.m_ClientBattlegroundIds, and itr1 is
    // the bgDataStore entry the instance is FILED in - bgDataStore[bg->GetTypeID()], set by AddBattleground.
    // Allocating from bgDataStore[1101] and releasing into bgDataStore[2106] was wrong twice over: the
    // aggregate's own pool never shrank, so its numbers climbed forever, and the release erased a number that
    // belonged to the drawn map's own queue, which could then hand the same "Warsong Gulch 3" to two live
    // instances. Not one of the audited findings - found while making the bracket above queue-side - and it is
    // inherited, Random Battleground (32) has behaved this way all along. The number is also what the client
    // shows next to the map name, so the map's own series is the one it belongs in.
    bg->SetClientInstanceID(CreateClientVisibleInstanceId(bgTypeId, bracketEntry->GetBracketId()));
    // reset the new bg (set status to status_wait_queue from status_none)
    // this shouldn't be needed anymore as a new Battleground instance is created each time. But some bg sub classes still depend on it.
    bg->Reset();
    bg->SetStatus(STATUS_WAIT_JOIN); // start the joining of the bg
    bg->SetArenaType(queueId.TeamSize);
    bg->SetRated(queueId.Rated);

    return bg;
}

void BattlegroundMgr::LoadBattlegroundTemplates()
{
    uint32 oldMSTime = getMSTime();

    //                                               0   1                 2              3             4       5
    QueryResult result = WorldDatabase.Query("SELECT ID, AllianceStartLoc, HordeStartLoc, StartMaxDist, Weight, ScriptName FROM battleground_template");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 battlegrounds. DB table `battleground_template` is empty.");
        return;
    }

    std::unordered_map<BattlegroundTypeId, std::vector<int32>> mapsByBattleground;
    for (BattlemasterListXMapEntry const* battlemasterListXMap : sBattlemasterListXMapStore)
        if (sBattlemasterListStore.HasRecord(battlemasterListXMap->BattlemasterListID) && sMapStore.HasRecord(battlemasterListXMap->MapID))
            mapsByBattleground[BattlegroundTypeId(battlemasterListXMap->BattlemasterListID)].push_back(battlemasterListXMap->MapID);

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        BattlegroundTypeId bgTypeId = BattlegroundTypeId(fields[0].GetUInt32());

        if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, bgTypeId, nullptr))
            continue;

        // can be overwrite by values from DB
        BattlemasterListEntry const* bl = sBattlemasterListStore.LookupEntry(bgTypeId);
        if (!bl)
        {
            TC_LOG_ERROR("bg.battleground", "Battleground ID {} could not be found in BattlemasterList.dbc. The battleground was not created.", bgTypeId);
            continue;
        }

        BattlegroundTemplate& bgTemplate = _battlegroundTemplates[bgTypeId];
        bgTemplate.Id                = bgTypeId;
        float dist                   = fields[3].GetFloat();
        bgTemplate.MaxStartDistSq    = dist * dist;
        bgTemplate.Weight            = fields[4].GetUInt8();
        bgTemplate.ScriptId          = sObjectMgr->GetScriptId(fields[5].GetStringView());
        bgTemplate.BattlemasterEntry = bl;
        bgTemplate.MapIDs            = std::move(mapsByBattleground[bgTypeId]);

        // Aggregate templates (All Arenas, the random-BG pools, Battleground Blitz, multi-map brawls) never
        // spawn anyone themselves: CreateNewBattleground resolves their BattlemasterListXMap maps back to the
        // individual single-map templates, which carry the real start locations. Requiring a WorldSafeLocs id
        // here would mean inventing one that is never used, and a wrong id silently drops the whole template.
        //
        // A brawl only qualifies when it really is multi-map - the single-map brawl rows (PvpBrawl 12-16, the
        // per-map Deep Six entries) point at one map each and would then register themselves over that map's own
        // template two lines below, hijacking normal queues for it.
        bool const isMultiMapBrawl = bgTemplate.MapIDs.size() > 1 && bl->GetFlags().HasFlag(BattlemasterListFlags::IsBrawl);

        if (bgTemplate.Id != BATTLEGROUND_AA && bgTemplate.Id != BATTLEGROUND_BLITZ
            && bgTemplate.Id != BATTLEGROUND_RATED_10_VS_10 && !IsRandomBattleground(bgTemplate.Id) && !isMultiMapBrawl)
        {
            uint32 startId = fields[1].GetUInt32();
            if (WorldSafeLocsEntry const* start = sObjectMgr->GetWorldSafeLoc(startId))
                bgTemplate.StartLocation[TEAM_ALLIANCE] = start;
            else if (bgTemplate.StartLocation[TEAM_ALLIANCE]) // reload case
                TC_LOG_ERROR("sql.sql", "Table `battleground_template` for id {} contains a non-existing WorldSafeLocs.dbc id {} in field `AllianceStartLoc`. Ignoring.", bgTemplate.Id, startId);
            else
            {
                TC_LOG_ERROR("sql.sql", "Table `battleground_template` for id {} contains a non-existing WorldSafeLocs.dbc id {} in field `AllianceStartLoc`. BG not created.", bgTemplate.Id, startId);
                _battlegroundTemplates.erase(bgTypeId);
                continue;
            }

            startId = fields[2].GetUInt32();
            if (WorldSafeLocsEntry const* start = sObjectMgr->GetWorldSafeLoc(startId))
                bgTemplate.StartLocation[TEAM_HORDE] = start;
            else if (bgTemplate.StartLocation[TEAM_HORDE]) // reload case
                TC_LOG_ERROR("sql.sql", "Table `battleground_template` for id {} contains a non-existing WorldSafeLocs.dbc id {} in field `HordeStartLoc`. Ignoring.", bgTemplate.Id, startId);
            else
            {
                TC_LOG_ERROR("sql.sql", "Table `battleground_template` for id {} contains a non-existing WorldSafeLocs.dbc id {} in field `HordeStartLoc`. BG not created.", bgTemplate.Id, startId);
                _battlegroundTemplates.erase(bgTypeId);
                continue;
            }
        }

        if (bgTemplate.MapIDs.size() == 1)
            _battlegroundMapTemplates[bgTemplate.MapIDs.front()] = &_battlegroundTemplates[bgTypeId];

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} battlegrounds in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

// Which brawl is running is a server-side decision on TrinityCore, and that is not a shortcut - it is what the
// client dictates. PvpBrawl.db2 has exactly seven columns (Name_lang, Description_lang, Objective_lang, ID,
// BattlemasterListID, LFGDungeonsID, RewardsQuestID); there is no holiday reference, no date and no flags in it,
// and no other client table schedules brawls. The client learns the running brawl only from
// SMSG_REQUEST_SCHEDULED_PVP_INFO_RESPONSE, whose handler (client VA 0x7FF72AAC2120) is the sole writer of the
// two globals the entire brawl UI reads. So the rotation has to come from configuration.
//
// Everything after the config read is a refusal to advertise a brawl we could not actually run: the whole point
// of gating here is that HandleRequestScheduledPvpInfo and HandleBattlemasterJoinBrawl consult the same function,
// so the client is never shown a queueable Brawl button for a mode the queue would drop.
Optional<BattlegroundMgr::ActiveBrawl> BattlegroundMgr::GetActiveBrawl()
{
    if (!sWorld->getBoolConfig(CONFIG_BRAWL_ENABLED))
        return {};

    ActiveBrawl brawl;
    brawl.PvpBrawlId = sWorld->getIntConfig(CONFIG_BRAWL_PVP_BRAWL_ID);
    brawl.BattlemasterListId = sWorld->getIntConfig(CONFIG_BRAWL_BATTLEMASTER_LIST_ID);

    if (!brawl.PvpBrawlId || !brawl.BattlemasterListId)
        return {};

    // This runs once per client login, so a bad config would otherwise repeat its complaint forever. Say it once.
    static uint32 loggedBadConfigFor = 0;
    auto complainOnce = [&](char const* what)
    {
        if (loggedBadConfigFor == brawl.BattlemasterListId)
            return;

        loggedBadConfigFor = brawl.BattlemasterListId;
        TC_LOG_ERROR("bg.battleground", "Brawl.BattlemasterListID {} {} - no brawl will be advertised.", brawl.BattlemasterListId, what);
    };

    BattlemasterListEntry const* battlemasterList = sBattlemasterListStore.LookupEntry(brawl.BattlemasterListId);
    if (!battlemasterList)
    {
        complainOnce("does not exist in BattlemasterList.db2");
        return {};
    }

    // BattlemasterList.Flags bit 0x20 is IsBrawl. Refusing anything else keeps a typo in the config from turning
    // an ordinary battleground into "the brawl" and mislabelling it in every client that asks.
    if (!battlemasterList->GetFlags().HasFlag(BattlemasterListFlags::IsBrawl))
    {
        complainOnce("is not flagged IsBrawl in BattlemasterList.db2");
        return {};
    }

    // These two are deliberate operator choices rather than mistakes, so they are not errors - but they still
    // have to be findable, because from the player's side the only symptom is a Brawl button that never appears.
    if (battlemasterList->GetFlags().HasFlag(BattlemasterListFlags::InternalOnly))
    {
        TC_LOG_DEBUG("bg.battleground", "Brawl {} not advertised: BattlemasterList is flagged InternalOnly.", brawl.BattlemasterListId);
        return {};
    }

    if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, brawl.BattlemasterListId, nullptr))
    {
        TC_LOG_DEBUG("bg.battleground", "Brawl {} not advertised: disabled via the `disables` table.", brawl.BattlemasterListId);
        return {};
    }

    // No battleground_template row means CreateNewBattleground would fail: the queue could accept players and
    // never pop. Do not advertise it.
    BattlegroundTemplate const* bgTemplate = GetBattlegroundTemplateByTypeId(BattlegroundTypeId(brawl.BattlemasterListId));
    if (!bgTemplate || bgTemplate->MapIDs.empty())
    {
        complainOnce("has no `battleground_template` row (or the row resolves to no maps)");
        return {};
    }

    // GetRandomBG resolves the brawl's BattlemasterListXMap maps back to the single-map templates that own the
    // start locations. If none of them resolves there is no map to send anyone to.
    if (GetRandomBG(BattlegroundTypeId(brawl.BattlemasterListId)) == BATTLEGROUND_TYPE_NONE)
    {
        complainOnce("has no runnable map - none of its BattlemasterListXMap maps has a single-map template here");
        return {};
    }

    TC_LOG_DEBUG("bg.battleground", "Brawl advertised: PvpBrawl {} on BattlemasterList {}.", brawl.PvpBrawlId, brawl.BattlemasterListId);
    return brawl;
}

void BattlegroundMgr::SendBattlegroundList(Player* player, ObjectGuid const& guid, BattlegroundTypeId bgTypeId)
{
    BattlegroundTemplate const* bgTemplate = GetBattlegroundTemplateByTypeId(bgTypeId);
    if (!bgTemplate)
        return;

    player->PlayerTalkClass->GetInteractionData().StartInteraction(guid, PlayerInteractionType::BattleMaster);

    WorldPackets::Battleground::BattlefieldList battlefieldList;
    battlefieldList.BattlemasterGuid = guid;
    battlefieldList.BattlemasterListID = bgTypeId;
    battlefieldList.MinLevel = bgTemplate->GetMinLevel();
    battlefieldList.MaxLevel = bgTemplate->GetMaxLevel();
    battlefieldList.PvpAnywhere = guid.IsEmpty();
    battlefieldList.HasRandomWinToday = player->GetRandomWinner();
    player->SendDirectMessage(battlefieldList.Write());
}

/*static*/ void BattlegroundMgr::SendToBattleground(Player* player, Battleground const* battleground)
{
    if (!battleground)
    {
        TC_LOG_ERROR("bg.battleground", "BattlegroundMgr::SendToBattleground: Battleground not found while trying to teleport player {}", player->GetName());
        return;
    }

    uint32 mapid = battleground->GetMapId();
    Team team = player->GetBGTeam();

    WorldSafeLocsEntry const* pos = battleground->GetTeamStartPosition(Battleground::GetTeamIndexByTeamId(team));
    TC_LOG_DEBUG("bg.battleground", "BattlegroundMgr::SendToBattleground: Sending {} to map {}, {} (bgType {})", player->GetName(), mapid, pos->Loc, battleground->GetTypeID());
    player->TeleportTo({ .Location = pos->Loc, .TransportGuid = pos->TransportSpawnId ? ObjectGuid::Create<HighGuid::Transport>(*pos->TransportSpawnId) : ObjectGuid::Empty });
}

bool BattlegroundMgr::IsArenaType(BattlegroundTypeId bgTypeId)
{
    return bgTypeId == BATTLEGROUND_AA
            || bgTypeId == BATTLEGROUND_BE
            || bgTypeId == BATTLEGROUND_NA
            || bgTypeId == BATTLEGROUND_DS
            || bgTypeId == BATTLEGROUND_RV
            || bgTypeId == BATTLEGROUND_RL;
}

bool BattlegroundMgr::IsRandomBattleground(uint32 battlemasterListId)
{
    return battlemasterListId == BATTLEGROUND_RB || battlemasterListId == BATTLEGROUND_RANDOM_EPIC;
}

bool BattlegroundMgr::IsAggregateBattleground(BattlegroundTypeId bgTypeId)
{
    BattlegroundTemplate const* bgTemplate = GetBattlegroundTemplateByTypeId(bgTypeId);
    return bgTemplate && bgTemplate->MapIDs.size() > 1;
}

BattlegroundQueueTypeId BattlegroundMgr::BGQueueTypeId(uint16 battlemasterListId, BattlegroundQueueIdType type, bool rated, uint8 teamSize)
{
    return { .BattlemasterListId = battlemasterListId, .Type = AsUnderlyingType(type), .Rated = rated, .TeamSize = teamSize };
}

void BattlegroundMgr::ToggleTesting()
{
    m_Testing = !m_Testing;
    sWorld->SendWorldText(m_Testing ? LANG_DEBUG_BG_ON : LANG_DEBUG_BG_OFF);
}

bool BattlegroundMgr::ToggleArenaTesting(uint32 battlemasterListId)
{
    if (battlemasterListId != 0)
    {
        BattlegroundTemplate const* bgTemplate = GetBattlegroundTemplateByTypeId(static_cast<BattlegroundTypeId>(battlemasterListId));
        if (!bgTemplate)
            return false;

        if (!bgTemplate->IsArena())
            return false;
    }

    if (m_ArenaTesting != battlemasterListId)
        sWorld->SendWorldText((battlemasterListId != 0) ? LANG_DEBUG_ARENA_ON : LANG_DEBUG_ARENA_OFF);

    m_ArenaTesting = battlemasterListId;
    return true;
}

bool BattlegroundMgr::IsValidQueueId(BattlegroundQueueTypeId bgQueueTypeId)
{
    BattlemasterListEntry const* battlemasterList = sBattlemasterListStore.LookupEntry(bgQueueTypeId.BattlemasterListId);
    if (!battlemasterList)
        return false;

    switch (BattlegroundQueueIdType(bgQueueTypeId.Type))
    {
        case BattlegroundQueueIdType::Battleground:
            if (battlemasterList->GetType() != BattlemasterType::Battleground)
                return false;
            if (bgQueueTypeId.TeamSize)
                return false;
            break;
        case BattlegroundQueueIdType::Arena:
            if (battlemasterList->GetType() != BattlemasterType::Arena)
                return false;
            if (!bgQueueTypeId.Rated)
                return false;
            if (!bgQueueTypeId.TeamSize)
                return false;
            break;
        case BattlegroundQueueIdType::Wargame:
            if (bgQueueTypeId.Rated)
                return false;
            break;
        case BattlegroundQueueIdType::ArenaSkirmish:
            if (battlemasterList->GetType() != BattlemasterType::Arena)
                return false;
            // A skirmish is UNRATED. This test used to be `if (!Rated) return false;`, which made the case
            // self-contradictory and unreachable in practice: the only skirmish matchmaker,
            // CheckSkirmishForSameFaction, is called exclusively from the `if (!m_queueId.Rated)` branch of
            // BattlegroundQueueUpdate, so a queue id that satisfied this check could never be matched.
            if (bgQueueTypeId.Rated)
                return false;
            if (bgQueueTypeId.TeamSize != ARENA_TYPE_3v3)
                return false;
            break;
        case BattlegroundQueueIdType::RatedBattlegroundBlitz:
            // BattlemasterList 1101 ("Battleground Blitz") carries PvpType 0, so GetType() is Battleground and
            // not Arena, even though the mode is rated. Rated must be set and TeamSize must be 0 - both read
            // straight off the packed QueueID retail sent back to the client (0x1F1000000019044D).
            if (battlemasterList->GetType() != BattlemasterType::Battleground)
                return false;
            if (!bgQueueTypeId.Rated)
                return false;
            if (bgQueueTypeId.TeamSize)
                return false;
            break;
        default:
            return false;
    }

    return true;
}

void BattlegroundMgr::ScheduleQueueUpdate(uint32 arenaMatchmakerRating, BattlegroundQueueTypeId bgQueueTypeId, BattlegroundBracketId bracket_id)
{
    //This method must be atomic, @todo add mutex
    //we will use only 1 number created of bgTypeId and bracket_id
    ScheduledQueueUpdate scheduleId{ arenaMatchmakerRating, bgQueueTypeId, bracket_id };
    if (std::find(m_QueueUpdateScheduler.begin(), m_QueueUpdateScheduler.end(), scheduleId) == m_QueueUpdateScheduler.end())
        m_QueueUpdateScheduler.push_back(scheduleId);
}

uint32 BattlegroundMgr::GetMaxRatingDifference() const
{
    // this is for stupid people who can't use brain and set max rating difference to 0
    uint32 diff = sWorld->getIntConfig(CONFIG_ARENA_MAX_RATING_DIFFERENCE);
    if (diff == 0)
        diff = 5000;
    return diff;
}

uint32 BattlegroundMgr::GetRatingDiscardTimer() const
{
    return sWorld->getIntConfig(CONFIG_ARENA_RATING_DISCARD_TIMER);
}

uint32 BattlegroundMgr::GetPrematureFinishTime() const
{
    return sWorld->getIntConfig(CONFIG_BATTLEGROUND_PREMATURE_FINISH_TIMER);
}

void BattlegroundMgr::LoadBattleMastersEntry()
{
    uint32 oldMSTime = getMSTime();

    mBattleMastersMap.clear();                                  // need for reload case

    QueryResult result = WorldDatabase.Query("SELECT entry, bg_template FROM battlemaster_entry");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 battlemaster entries. DB table `battlemaster_entry` is empty!");
        return;
    }

    uint32 count = 0;

    do
    {
        ++count;

        Field* fields = result->Fetch();

        uint32 entry = fields[0].GetUInt32();
        if (CreatureTemplate const* cInfo = sObjectMgr->GetCreatureTemplate(entry))
        {
            if ((cInfo->npcflag & UNIT_NPC_FLAG_BATTLEMASTER) == 0)
                TC_LOG_ERROR("sql.sql", "Creature (Entry: {}) listed in `battlemaster_entry` is not a battlemaster.", entry);
        }
        else
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: {}) listed in `battlemaster_entry` does not exist.", entry);
            continue;
        }

        uint32 bgTypeId  = fields[1].GetUInt32();
        if (!sBattlemasterListStore.LookupEntry(bgTypeId))
        {
            TC_LOG_ERROR("sql.sql", "Table `battlemaster_entry` contains entry {} for a non-existing battleground type {}, ignored.", entry, bgTypeId);
            continue;
        }

        mBattleMastersMap[entry] = BattlegroundTypeId(bgTypeId);
    }
    while (result->NextRow());

    CheckBattleMasters();

    TC_LOG_INFO("server.loading", ">> Loaded {} battlemaster entries in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void BattlegroundMgr::CheckBattleMasters()
{
    CreatureTemplateContainer const& ctc = sObjectMgr->GetCreatureTemplates();
    for (auto const& creatureTemplatePair : ctc)
    {
        if ((creatureTemplatePair.second.npcflag & UNIT_NPC_FLAG_BATTLEMASTER) && !mBattleMastersMap.count(creatureTemplatePair.first))
        {
            TC_LOG_ERROR("sql.sql", "Creature_Template Entry: {} has UNIT_NPC_FLAG_BATTLEMASTER, but no data in the `battlemaster_entry` table. Removing flag.", creatureTemplatePair.first);
            const_cast<CreatureTemplate&>(creatureTemplatePair.second).npcflag &= ~UNIT_NPC_FLAG_BATTLEMASTER;
        }
    }
}

HolidayIds BattlegroundMgr::BGTypeToWeekendHolidayId(BattlegroundTypeId bgTypeId)
{
    switch (bgTypeId)
    {
        case BATTLEGROUND_AV:  return HOLIDAY_CALL_TO_ARMS_AV;
        case BATTLEGROUND_EY:  return HOLIDAY_CALL_TO_ARMS_ES;
        case BATTLEGROUND_WS:  return HOLIDAY_CALL_TO_ARMS_WG;
        case BATTLEGROUND_SA:  return HOLIDAY_CALL_TO_ARMS_SA;
        case BATTLEGROUND_AB:  return HOLIDAY_CALL_TO_ARMS_AB;
        case BATTLEGROUND_IC:  return HOLIDAY_CALL_TO_ARMS_IC;
        case BATTLEGROUND_TP:  return HOLIDAY_CALL_TO_ARMS_TP;
        case BATTLEGROUND_BFG: return HOLIDAY_CALL_TO_ARMS_BG;
        default: return HOLIDAY_NONE;
    }
}

BattlegroundTypeId BattlegroundMgr::WeekendHolidayIdToBGType(HolidayIds holiday)
{
    switch (holiday)
    {
        case HOLIDAY_CALL_TO_ARMS_AV: return BATTLEGROUND_AV;
        case HOLIDAY_CALL_TO_ARMS_ES: return BATTLEGROUND_EY;
        case HOLIDAY_CALL_TO_ARMS_WG: return BATTLEGROUND_WS;
        case HOLIDAY_CALL_TO_ARMS_SA: return BATTLEGROUND_SA;
        case HOLIDAY_CALL_TO_ARMS_AB: return BATTLEGROUND_AB;
        case HOLIDAY_CALL_TO_ARMS_IC: return BATTLEGROUND_IC;
        case HOLIDAY_CALL_TO_ARMS_TP: return BATTLEGROUND_TP;
        case HOLIDAY_CALL_TO_ARMS_BG: return BATTLEGROUND_BFG;
        default: return BATTLEGROUND_TYPE_NONE;
    }
}

bool BattlegroundMgr::IsBGWeekend(BattlegroundTypeId bgTypeId)
{
    return IsHolidayActive(BGTypeToWeekendHolidayId(bgTypeId));
}

// PVPDifficulty.RangeIndex is numbered per map, and the ladders of the maps inside one aggregate
// BattlemasterList are NOT aligned. Battleground Blitz (1101) and Rated Battleground (100) both list nine
// maps: seven of them carry the full ten-step ladder 10-19 .. 90-90, but map 968 (Silvershard Mines) starts
// at 50-59 and has six steps, and map 2656 starts at 71-79 and has four. A queue bracket computed on the
// aggregate's first map - which is what every join handler does, and what the queue is keyed by - therefore
// means a different level range on those two maps, or none at all: for levels 60-70 map 2656 has no bracket
// whatsoever, so DB2Manager::GetBattlegroundBracketByLevel returns null there. Random Epic (901) has the
// same shape, with map 2799 holding only 90-90.
//
// A map that cannot host the queue's level range must not be drawn. Otherwise CreateNewBattleground fails
// after the matchmaker has already picked the teams and rewritten their factions, and - once the instance
// does come up on such a map - WorldSession::HandleBattleFieldPortOpcode looks the bracket up again on
// bg->GetMapId() and returns in silence when it finds none, stranding the accepted invitation.
static bool MapCanHostBracket(uint32 mapId, PVPDifficultyEntry const* queueBracket)
{
    // GetBattlegroundBracketByLevel falls back to the highest bracket below the level, so test containment
    // explicitly instead of trusting a non-null result.
    PVPDifficultyEntry const* entry = DB2Manager::GetBattlegroundBracketByLevel(mapId, queueBracket->MinLevel);
    return entry && entry->MinLevel <= queueBracket->MinLevel && entry->MaxLevel >= queueBracket->MaxLevel;
}

BattlegroundTypeId BattlegroundMgr::GetRandomBG(BattlegroundTypeId bgTypeId, PVPDifficultyEntry const* queueBracket /*= nullptr*/)
{
    if (BattlegroundTemplate const* bgTemplate = GetBattlegroundTemplateByTypeId(bgTypeId))
    {
        if (bgTemplate->IsArena() && isArenaTesting())
            return static_cast<BattlegroundTypeId>(m_ArenaTesting);

        std::vector<BattlegroundTemplate const*> ids;
        ids.reserve(bgTemplate->MapIDs.size());
        for (int32 mapId : bgTemplate->MapIDs)
        {
            if (queueBracket && !MapCanHostBracket(mapId, queueBracket))
                continue;

            if (BattlegroundTemplate const* bg = GetBattlegroundTemplateByMapId(mapId))
                ids.push_back(bg);
        }

        if (!ids.empty())
            return (*Trinity::Containers::SelectRandomWeightedContainerElement(ids, [](BattlegroundTemplate const* bg) { return bg->Weight; }))->Id;
    }

    return BATTLEGROUND_TYPE_NONE;
}

BGFreeSlotQueueContainer& BattlegroundMgr::GetBGFreeSlotQueueStore(uint32 mapId)
{
    return m_BGFreeSlotQueue[mapId];
}

void BattlegroundMgr::AddToBGFreeSlotQueue(Battleground* bg)
{
    m_BGFreeSlotQueue[bg->GetMapId()].push_front(bg);
}

void BattlegroundMgr::RemoveFromBGFreeSlotQueue(uint32 mapId, uint32 instanceId)
{
    if (BGFreeSlotQueueContainer* freeSlotQueue = Trinity::Containers::MapGetValuePtr(m_BGFreeSlotQueue, mapId))
    {
        auto itr = std::ranges::find(*freeSlotQueue, instanceId, [](Battleground const* bg) { return bg->GetInstanceID(); });
        if (itr != freeSlotQueue->end())
            freeSlotQueue->erase(itr);
    }
}

void BattlegroundMgr::AddBattleground(Battleground* bg)
{
    if (bg)
    {
        Trinity::unique_trackable_ptr<Battleground>& ptr = bgDataStore[bg->GetTypeID()].m_Battlegrounds[bg->GetInstanceID()];
        ptr.reset(bg);
        bg->SetWeakPtr(ptr);
    }
}
