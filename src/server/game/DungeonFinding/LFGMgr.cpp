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

#include "LFGMgr.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "DisableMgr.h"
#include "GameEventMgr.h"
#include "GameTime.h"
#include "Group.h"
#include "GroupMgr.h"
#include "BattlegroundMgr.h"
#include "BattlegroundPackets.h"
#include "BattlegroundQueue.h"
#include "InstanceLockMgr.h"
#include "LFGGroupData.h"
#include "LFGPlayerData.h"
#include "LFGQueue.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "SharedDefines.h"
#include "SocialMgr.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>
#include <sstream>

namespace lfg
{

LFGDungeonData::LFGDungeonData() : id(0), name(), map(0), type(0), expansion(0), group(0), contentTuningId(0),
    difficulty(DIFFICULTY_NONE), seasonal(false), x(0.0f), y(0.0f), z(0.0f), o(0.0f), requiredItemLevel(0), finalDungeonEncounterId(0)
{
}

LFGDungeonData::LFGDungeonData(LFGDungeonsEntry const* dbc) : id(dbc->ID), name(dbc->Name[sWorld->GetDefaultDbcLocale()]), map(dbc->MapID),
    type(uint8(dbc->TypeID)), expansion(uint8(dbc->ExpansionLevel)), group(uint8(dbc->GroupID)),
    contentTuningId(uint32(dbc->ContentTuningID)), difficulty(Difficulty(dbc->DifficultyID)),
    seasonal((dbc->Flags[0] & LFG_FLAG_SEASONAL) != 0), x(0.0f), y(0.0f), z(0.0f), o(0.0f),
    requiredItemLevel(0), finalDungeonEncounterId(0)
{
    if (JournalEncounterEntry const* journalEncounter = sJournalEncounterStore.LookupEntry(dbc->FinalEncounterID))
        finalDungeonEncounterId = journalEncounter->DungeonEncounterID;
}

LFGMgr::LFGMgr() : m_QueueTimer(0), m_lfgProposalId(1),
    m_options(sWorld->getIntConfig(CONFIG_LFG_OPTIONSMASK))
{
}

LFGMgr::~LFGMgr()
{
    for (LfgRewardContainer::iterator itr = RewardMapStore.begin(); itr != RewardMapStore.end(); ++itr)
        delete itr->second;
}

void LFGMgr::_LoadFromDB(Field* fields, ObjectGuid guid)
{
    if (!fields)
        return;

    if (!guid.IsParty())
        return;

    SetLeader(guid, ObjectGuid::Create<HighGuid::Player>(fields[0].GetUInt64()));

    uint32 dungeon = fields[19].GetUInt32();
    uint8 state = fields[20].GetUInt8();

    if (!dungeon || !state)
        return;

    SetDungeon(guid, dungeon);

    switch (state)
    {
        case LFG_STATE_DUNGEON:
        case LFG_STATE_FINISHED_DUNGEON:
            SetState(guid, (LfgState)state);
            break;
        default:
            break;
    }
}

void LFGMgr::_SaveToDB(ObjectGuid guid, uint32 db_guid)
{
    if (!guid.IsParty())
        return;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_LFG_DATA);
    stmt->setUInt32(0, db_guid);
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_LFG_DATA);
    stmt->setUInt32(0, db_guid);
    stmt->setUInt32(1, GetDungeon(guid));
    stmt->setUInt32(2, GetState(guid));
    trans->Append(stmt);

    CharacterDatabase.CommitTransaction(trans);
}

/// Load rewards for completing dungeons
void LFGMgr::LoadRewards()
{
    uint32 oldMSTime = getMSTime();

    for (LfgRewardContainer::iterator itr = RewardMapStore.begin(); itr != RewardMapStore.end(); ++itr)
        delete itr->second;
    RewardMapStore.clear();

    // ORDER BY is very important for GetRandomDungeonReward!
    QueryResult result = WorldDatabase.Query("SELECT dungeonId, maxLevel, firstQuestId, otherQuestId FROM lfg_dungeon_rewards ORDER BY dungeonId, maxLevel ASC");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 lfg dungeon rewards. DB table `lfg_dungeon_rewards` is empty!");
        return;
    }

    uint32 count = 0;

    Field* fields = nullptr;
    do
    {
        fields = result->Fetch();
        uint32 dungeonId = fields[0].GetUInt32();
        uint32 maxLevel = fields[1].GetUInt8();
        uint32 firstQuestId = fields[2].GetUInt32();
        uint32 otherQuestId = fields[3].GetUInt32();

        if (!GetLFGDungeonEntry(dungeonId))
        {
            TC_LOG_ERROR("sql.sql", "Dungeon {} specified in table `lfg_dungeon_rewards` does not exist!", dungeonId);
            continue;
        }

        if (!maxLevel || maxLevel > sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
        {
            TC_LOG_ERROR("sql.sql", "Level {} specified for dungeon {} in table `lfg_dungeon_rewards` can never be reached!", maxLevel, dungeonId);
            maxLevel = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);
        }

        if (!firstQuestId || !sObjectMgr->GetQuestTemplate(firstQuestId))
        {
            TC_LOG_ERROR("sql.sql", "First quest {} specified for dungeon {} in table `lfg_dungeon_rewards` does not exist!", firstQuestId, dungeonId);
            continue;
        }

        if (otherQuestId && !sObjectMgr->GetQuestTemplate(otherQuestId))
        {
            TC_LOG_ERROR("sql.sql", "Other quest {} specified for dungeon {} in table `lfg_dungeon_rewards` does not exist!", otherQuestId, dungeonId);
            otherQuestId = 0;
        }

        RewardMapStore.insert(LfgRewardContainer::value_type(dungeonId, new LfgReward(maxLevel, firstQuestId, otherQuestId)));
        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} lfg dungeon rewards in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

LFGDungeonData const* LFGMgr::GetLFGDungeon(uint32 id)
{
    LFGDungeonContainer::const_iterator itr = LfgDungeonStore.find(id);
    if (itr != LfgDungeonStore.end())
        return &(itr->second);

    return nullptr;
}

void LFGMgr::LoadLFGDungeons()
{
    uint32 oldMSTime = getMSTime();

    LfgDungeonStore.clear();

    // Initialize Dungeon map with data from dbcs
    for (uint32 i = 0; i < sLFGDungeonsStore.GetNumRows(); ++i)
    {
        LFGDungeonsEntry const* dungeon = sLFGDungeonsStore.LookupEntry(i);
        if (!dungeon)
            continue;

        if (!sDB2Manager.GetMapDifficultyData(dungeon->MapID, Difficulty(dungeon->DifficultyID)))
            continue;

        switch (dungeon->TypeID)
        {
            case LFG_TYPE_DUNGEON:
            case LFG_TYPE_HEROIC:
            case LFG_TYPE_RAID:
            case LFG_TYPE_RANDOM:
                LfgDungeonStore[dungeon->ID] = LFGDungeonData(dungeon);
                break;
        }
    }

    // Fill teleport locations from DB
    QueryResult result = WorldDatabase.Query("SELECT dungeonId, position_x, position_y, position_z, orientation, requiredItemLevel FROM lfg_dungeon_template");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 lfg dungeon templates. DB table `lfg_dungeon_template` is empty!");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();
        uint32 dungeonId = fields[0].GetUInt32();
        LFGDungeonContainer::iterator dungeonItr = LfgDungeonStore.find(dungeonId);
        if (dungeonItr == LfgDungeonStore.end())
        {
            TC_LOG_ERROR("sql.sql", "table `lfg_dungeon_template` contains coordinates for wrong dungeon {}", dungeonId);
            continue;
        }

        LFGDungeonData& data    = dungeonItr->second;
        data.x                  = fields[1].GetFloat();
        data.y                  = fields[2].GetFloat();
        data.z                  = fields[3].GetFloat();
        data.o                  = fields[4].GetFloat();
        data.requiredItemLevel  = fields[5].GetUInt16();

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} lfg dungeon templates in {} ms", count, GetMSTimeDiffToNow(oldMSTime));

    CachedDungeonMapStore.clear();

    // Fill all other teleport coords from areatriggers
    for (LFGDungeonContainer::iterator itr = LfgDungeonStore.begin(); itr != LfgDungeonStore.end(); ++itr)
    {
        LFGDungeonData& dungeon = itr->second;

        // No teleport coords in database, load from areatriggers
        if (dungeon.type != LFG_TYPE_RANDOM && dungeon.x == 0.0f && dungeon.y == 0.0f && dungeon.z == 0.0f)
        {
            AreaTriggerTeleport const* at = sObjectMgr->GetMapEntranceTrigger(dungeon.map);
            if (!at)
            {
                TC_LOG_ERROR("sql.sql", "Failed to load dungeon {} (Id: {}), cant find areatrigger for map {}", dungeon.name, dungeon.id, dungeon.map);
                continue;
            }

            dungeon.map = at->Loc.GetMapId();
            dungeon.x = at->Loc.GetPositionX();
            dungeon.y = at->Loc.GetPositionY();
            dungeon.z = at->Loc.GetPositionZ();
            dungeon.o = at->Loc.GetOrientation();
        }

        if (dungeon.type != LFG_TYPE_RANDOM)
            CachedDungeonMapStore[dungeon.group].insert(dungeon.id);
        CachedDungeonMapStore[0].insert(dungeon.id);
    }
}

LFGMgr* LFGMgr::instance()
{
    static LFGMgr instance;
    return &instance;
}

void LFGMgr::Update(uint32 diff)
{
    time_t currTime = GameTime::GetGameTime();

    // Remove obsolete readiness checks. A timeout is status 3, which the client turns into
    // ERR_LFG_READY_CHECK_FAILED_TIMEOUT (804) and closes LFGReadyCheckPopup with.
    //
    // This sweep sits AHEAD of the LFG option gate below, and that is deliberate: a readiness check is not
    // a dungeon-finder queue, and StartReadyCheck does not consult the option mask either. Behind the gate
    // the lifecycle would be asymmetric - a check could be created on a realm with the dungeon finder
    // switched off, but never expire and never be aborted. The popup would then stay open on every
    // member's screen (only an update ever closes it), the entry would live in ReadyChecksStore until the
    // next restart, and StartReadyCheck would refuse that group forever because the stale entry blocks it.
    for (LfgReadyCheckContainer::iterator it = ReadyChecksStore.begin(); it != ReadyChecksStore.end();)
    {
        LfgReadyCheckContainer::iterator itReadyCheck = it++;
        if (currTime < itReadyCheck->second.cancelTime)
            continue;

        FinishReadyCheck(itReadyCheck, LFG_READYCHECK_MISSING_ANSWER);
    }

    if (!isOptionEnabled(LFG_OPTION_ENABLE_DUNGEON_FINDER | LFG_OPTION_ENABLE_RAID_BROWSER))
        return;

    // Remove obsolete role checks
    for (LfgRoleCheckContainer::iterator it = RoleChecksStore.begin(); it != RoleChecksStore.end();)
    {
        LfgRoleCheckContainer::iterator itRoleCheck = it++;
        LfgRoleCheck& roleCheck = itRoleCheck->second;
        if (currTime < roleCheck.cancelTime)
            continue;
        roleCheck.state = LFG_ROLECHECK_MISSING_ROLE;

        for (LfgRolesMap::const_iterator itRoles = roleCheck.roles.begin(); itRoles != roleCheck.roles.end(); ++itRoles)
        {
            ObjectGuid guid = itRoles->first;
            RestoreState(guid, "Remove Obsolete RoleCheck");
            SendLfgRoleCheckUpdate(guid, roleCheck);
            if (guid == roleCheck.leader)
                SendLfgJoinResult(guid, LfgJoinResultData(LFG_JOIN_ROLE_CHECK_FAILED, LFG_ROLECHECK_MISSING_ROLE));
        }

        RestoreState(itRoleCheck->first, "Remove Obsolete RoleCheck");
        RoleChecksStore.erase(itRoleCheck);
    }

    // Remove obsolete proposals
    for (LfgProposalContainer::iterator it = ProposalsStore.begin(); it != ProposalsStore.end();)
    {
        LfgProposalContainer::iterator itRemove = it++;
        if (itRemove->second.cancelTime < currTime)
            RemoveProposal(itRemove, LFG_UPDATETYPE_PROPOSAL_FAILED);
    }

    // Remove obsolete kicks
    for (LfgPlayerBootContainer::iterator it = BootsStore.begin(); it != BootsStore.end();)
    {
        LfgPlayerBootContainer::iterator itBoot = it++;
        LfgPlayerBoot& boot = itBoot->second;
        if (boot.cancelTime < currTime)
        {
            boot.inProgress = false;
            for (LfgAnswerContainer::const_iterator itVotes = boot.votes.begin(); itVotes != boot.votes.end(); ++itVotes)
            {
                ObjectGuid pguid = itVotes->first;
                if (pguid != boot.victim)
                    SendLfgBootProposalUpdate(pguid, boot);
            }
            SetVoteKick(itBoot->first, false);
            BootsStore.erase(itBoot);
        }
    }

    uint32 lastProposalId = m_lfgProposalId;
    // Check if a proposal can be formed with the new groups being added
    for (LfgQueueContainer::iterator it = QueuesStore.begin(); it != QueuesStore.end(); ++it)
        if (uint8 newProposals = it->second.FindGroups())
            TC_LOG_DEBUG("lfg.update", "Found {} new groups in queue {}", newProposals, it->first);

    if (lastProposalId != m_lfgProposalId)
    {
        // FIXME lastProposalId ? lastProposalId +1 ?
        for (LfgProposalContainer::const_iterator itProposal = ProposalsStore.find(m_lfgProposalId); itProposal != ProposalsStore.end(); ++itProposal)
        {
            uint32 proposalId = itProposal->first;
            LfgProposal& proposal = ProposalsStore[proposalId];

            ObjectGuid guid;
            for (LfgProposalPlayerContainer::const_iterator itPlayers = proposal.players.begin(); itPlayers != proposal.players.end(); ++itPlayers)
            {
                guid = itPlayers->first;
                SetState(guid, LFG_STATE_PROPOSAL);
                ObjectGuid gguid = GetGroup(guid);
                if (!gguid.IsEmpty())
                {
                    SetState(gguid, LFG_STATE_PROPOSAL);
                    SendLfgUpdateStatus(guid, LfgUpdateData(LFG_UPDATETYPE_PROPOSAL_BEGIN, GetSelectedDungeons(guid)), true);
                }
                else
                    SendLfgUpdateStatus(guid, LfgUpdateData(LFG_UPDATETYPE_PROPOSAL_BEGIN, GetSelectedDungeons(guid)), false);
                SendLfgUpdateProposal(guid, proposal);
            }

            if (proposal.state == LFG_PROPOSAL_SUCCESS)
                UpdateProposal(proposalId, guid, true);
        }
    }

    // Update all players status queue info
    if (m_QueueTimer > LFG_QUEUEUPDATE_INTERVAL)
    {
        m_QueueTimer = 0;
        for (LfgQueueContainer::iterator it = QueuesStore.begin(); it != QueuesStore.end(); ++it)
            it->second.UpdateQueueTimers(it->first, currTime);

        // Piggy-backed on the queue-status sweep on purpose: this is the one place that already runs over
        // every live queue on a timer, and the prompt is a "you have been waiting a while" notification,
        // so LFG_QUEUEUPDATE_INTERVAL granularity is the right resolution for it.
        UpdateExpandSearchPrompts(currTime);
    }
    else
        m_QueueTimer += diff;
}

/**
    Translates a TC LfgLockStatusType into the client's Enum.LFGSlotInvalidReason, which is what
    SMSG_LFG_SLOT_INVALID's first dword is indexed by client side (LFG_INSTANCE_INVALID_CODES).

    Two deliberate divergences from a straight cast:
      - LFG_LOCKSTATUS_NO_SPEC is 14 in TC but NoSpec is 13 in the client enum, where 14 is
        CannotRunAnyChildDungeon. A straight cast would tell the player the wrong thing.
      - TC folds PlayerCondition failures into 1000 + the client's condition code. Retail sends
        PlayerConditionFailed (19) with the condition code in SubReason1, which is what the client's
        LFG_INSTANCE_CONDITION_FAILED_CODES table actually reads.
*/
static LfgSlotInvalidReason LockStatusToSlotInvalidReason(LfgLockInfoData const& info, int32& subReason1, int32& subReason2)
{
    subReason1 = 0;
    subReason2 = 0;

    switch (info.lockStatus)
    {
        case LFG_LOCKSTATUS_INSUFFICIENT_EXPANSION:
            return LFG_SLOT_INVALID_EXPANSION_TOO_LOW;
        case LFG_LOCKSTATUS_TOO_LOW_LEVEL:
            return LFG_SLOT_INVALID_LEVEL_TOO_LOW;
        case LFG_LOCKSTATUS_TOO_HIGH_LEVEL:
            return LFG_SLOT_INVALID_LEVEL_TOO_HIGH;
        case LFG_LOCKSTATUS_TOO_LOW_GEAR_SCORE:
            subReason1 = int32(info.requiredItemLevel);
            subReason2 = int32(info.currentItemLevel);
            return LFG_SLOT_INVALID_GEAR_TOO_LOW;
        case LFG_LOCKSTATUS_TOO_HIGH_GEAR_SCORE:
            subReason1 = int32(info.requiredItemLevel);
            subReason2 = int32(info.currentItemLevel);
            return LFG_SLOT_INVALID_GEAR_TOO_HIGH;
        case LFG_LOCKSTATUS_RAID_LOCKED:
            return LFG_SLOT_INVALID_RAID_LOCKED;
        case LFG_LOCKSTATUS_NO_SPEC:
            return LFG_SLOT_INVALID_NO_SPEC;
        case LFG_LOCKSTATUS_HAS_RESTRICTION:
            return LFG_SLOT_INVALID_RESTRICTED;
        default:
            break;
    }

    if (info.lockStatus > 1000)
    {
        subReason1 = int32(info.lockStatus - 1000);
        return LFG_SLOT_INVALID_PLAYER_CONDITION_FAILED;
    }

    return LFG_SLOT_INVALID_NONE;
}

/**
    Builds the dungeon set an already queued entry would be widened to.

    Expanding the search is not a free-form widening: it is the set the same queue would have covered had
    it been joined as a random for the same LFGDungeons group, minus anything the requester cannot enter
    right now. Both halves are data TC already owns - CachedDungeonMapStore, which LoadLFGDungeons fills
    per dungeon group, and GetLockedDungeons, which is recomputed live.

    Randoms are already expanded to their whole group by JoinLfg, so they have nothing to gain here; this
    is for a specific-dungeon queue.

   @param[in]     pguid      Player whose eligibility bounds the expansion
   @param[in]     current    Dungeons the entry is queued for right now
   @param[out]    nowInvalid If given, receives the currently queued dungeons that have since become locked
   @returns The widened dungeon set (never contains anything locked for pguid)
*/
LfgDungeonSet LFGMgr::BuildExpandedDungeons(ObjectGuid pguid, LfgDungeonSet const& current, LfgLockMap* nowInvalid)
{
    LfgDungeonSet expanded = current;

    std::set<uint8> groups;
    for (uint32 dungeonId : current)
        if (LFGDungeonData const* dungeon = GetLFGDungeon(dungeonId))
            groups.insert(dungeon->group);

    for (uint8 group : groups)
    {
        LfgCachedDungeonContainer::const_iterator itr = CachedDungeonMapStore.find(group);
        if (itr != CachedDungeonMapStore.end())
            expanded.insert(itr->second.begin(), itr->second.end());
    }

    for (auto const& lock : GetLockedDungeons(pguid))
    {
        uint32 const dungeonId = lock.first & 0x00FFFFFF;
        expanded.erase(dungeonId);
        if (nowInvalid && current.find(dungeonId) != current.end())
            (*nowInvalid)[lock.first] = lock.second;
    }

    return expanded;
}

/**
    Offers SMSG_LFG_EXPAND_SEARCH_PROMPT to the owner of every queue entry that has been waiting longer
    than LFG_TIME_EXPAND_SEARCH_PROMPT and that would actually gain dungeons by being widened. Prompted
    owners are remembered so the popup is offered once per queue entry rather than once per sweep, and
    that memory is pruned in the same pass against the set of owners still queued.
*/
void LFGMgr::UpdateExpandSearchPrompts(time_t currTime)
{
    GuidSet stillQueued;

    for (auto const& itr : PlayersStore)
    {
        ObjectGuid const pguid = itr.first;
        if (itr.second.GetState() != LFG_STATE_QUEUED)
            continue;

        Player* player = ObjectAccessor::FindConnectedPlayer(pguid);
        if (!player)
            continue;

        ObjectGuid const gguid = GetGroup(pguid);
        if (!gguid.IsEmpty())
        {
            // Only the leader can act on the prompt, so only the leader is asked.
            Group const* group = sGroupMgr->GetGroupByGUID(gguid);
            if (!group || group->GetLeaderGUID() != pguid)
                continue;
        }

        ObjectGuid const owner = gguid.IsEmpty() ? pguid : gguid;
        stillQueued.insert(owner);

        if (ExpandSearchPromptedStore.find(owner) != ExpandSearchPromptedStore.end())
            continue;

        LFGQueue& queue = GetQueue(owner);
        time_t const joinTime = queue.GetJoinTime(owner);
        if (!joinTime || currTime - joinTime < LFG_TIME_EXPAND_SEARCH_PROMPT)
            continue;

        LfgDungeonSet const* queued = queue.GetQueuedDungeons(owner);
        if (!queued || queued->empty())
            continue;

        LfgDungeonSet const expanded = BuildExpandedDungeons(pguid, *queued, nullptr);
        bool const wouldGain = std::any_of(expanded.begin(), expanded.end(),
            [queued](uint32 dungeonId) { return queued->find(dungeonId) == queued->end(); });
        if (!wouldGain)
            continue;

        WorldPackets::LFG::RideTicket const* ticket = GetTicket(pguid);
        if (!ticket)
            continue;

        ExpandSearchPromptedStore.insert(owner);
        TC_LOG_DEBUG("lfg.queue.expand", "Offering search expansion to {} (owner {}): queued for {} dungeons, {} available",
            pguid.ToString(), owner.ToString(), uint32(queued->size()), uint32(expanded.size()));
        player->GetSession()->SendLfgExpandSearchPrompt(*ticket);
    }

    std::erase_if(ExpandSearchPromptedStore, [&stillQueued](ObjectGuid const& guid)
    {
        return stillQueued.find(guid) == stillQueued.end();
    });
}

/**
    Handles CMSG_DF_CONFIRM_EXPAND_SEARCH: the player accepted the "expand your search?" popup.

    The entry is rebuilt with the widened dungeon set but the *original* join time, so accepting costs
    nothing in queue position - the join time is what wait-time averages and ordering key off. Any dungeon
    the entry was already queued for that has since become unenterable is reported through
    SMSG_LFG_SLOT_INVALID before the swap, so the player is told why his selection shrank rather than
    silently losing slots.

   @param[in]     player Player that accepted the prompt (must be the queue owner or the group leader)
   @param[in]     ticket Ticket echoed back by the client; must match the one we issued
*/
void LFGMgr::ConfirmExpandSearch(Player* player, WorldPackets::LFG::RideTicket const& ticket)
{
    if (!player)
        return;

    ObjectGuid const pguid = player->GetGUID();

    if (GetState(pguid) != LFG_STATE_QUEUED)
    {
        TC_LOG_DEBUG("lfg.queue.expand", "{} confirmed a search expansion while not queued - ignored", pguid.ToString());
        return;
    }

    WorldPackets::LFG::RideTicket const* ourTicket = GetTicket(pguid);
    if (!ourTicket || ourTicket->RequesterGuid != ticket.RequesterGuid || ourTicket->Id != ticket.Id)
    {
        TC_LOG_DEBUG("lfg.queue.expand", "{} confirmed a search expansion with a ticket we did not issue - ignored", pguid.ToString());
        return;
    }

    ObjectGuid const gguid = GetGroup(pguid);
    if (!gguid.IsEmpty())
    {
        Group const* group = sGroupMgr->GetGroupByGUID(gguid);
        if (!group || group->GetLeaderGUID() != pguid)
        {
            TC_LOG_DEBUG("lfg.queue.expand", "{} is not the leader of {} - search expansion ignored", pguid.ToString(), gguid.ToString());
            return;
        }
    }

    ObjectGuid const owner = gguid.IsEmpty() ? pguid : gguid;
    LFGQueue& queue = GetQueue(owner);

    time_t const joinTime = queue.GetJoinTime(owner);
    LfgDungeonSet const* queuedPtr = queue.GetQueuedDungeons(owner);
    LfgRolesMap const* rolesPtr = queue.GetQueuedRoles(owner);
    if (!joinTime || !queuedPtr || !rolesPtr)
    {
        TC_LOG_DEBUG("lfg.queue.expand", "No queue data for owner {} - search expansion ignored", owner.ToString());
        return;
    }

    LfgDungeonSet const queued = *queuedPtr;
    LfgRolesMap const roles = *rolesPtr;

    LfgLockMap nowInvalid;
    LfgDungeonSet expanded = BuildExpandedDungeons(pguid, queued, &nowInvalid);

    GuidSet players;
    if (gguid.IsEmpty())
        players.insert(pguid);
    else
        players = GetPlayers(gguid);

    // Trim to what the whole party can actually enter - the same routine JoinLfg uses, so an expanded
    // queue can never end up holding a dungeon one member is locked out of.
    LfgLockPartyMap lockMap;
    std::vector<std::string_view> playersMissingRequirement;
    GetCompatibleDungeons(&expanded, players, &lockMap, &playersMissingRequirement, false);

    for (auto const& invalid : nowInvalid)
    {
        int32 subReason1 = 0;
        int32 subReason2 = 0;
        LfgSlotInvalidReason const reason = LockStatusToSlotInvalidReason(invalid.second, subReason1, subReason2);
        TC_LOG_DEBUG("lfg.queue.expand", "Queued dungeon {} is no longer valid for {} (lock status {} -> reason {})",
            invalid.first & 0x00FFFFFF, pguid.ToString(), invalid.second.lockStatus, uint32(reason));
        player->GetSession()->SendLfgSlotInvalid(reason, subReason1, subReason2);
    }

    if (expanded.empty())
    {
        // Everything we could have offered is locked. Leaving the entry queued for nothing would be a lie;
        // the SMSG_LFG_SLOT_INVALID lines above have already told the player why.
        TC_LOG_DEBUG("lfg.queue.expand", "Search expansion for owner {} left no valid dungeon - leaving queue", owner.ToString());
        LeaveLfg(owner);
        return;
    }

    if (expanded == queued)
    {
        // Nothing left to widen into (the queue can move between prompt and answer). Restate the queue so
        // the client's popup closes against the real selection rather than against nothing.
        for (GuidSet::const_iterator it = players.begin(); it != players.end(); ++it)
            SendLfgUpdateStatus(*it, LfgUpdateData(LFG_UPDATETYPE_ADDED_TO_QUEUE, queued), !gguid.IsEmpty());
        return;
    }

    // Re-add with the original join time. AddToQueue(reAdd = false) is deliberate: the widened selection
    // has to be re-tested against everyone already queued, and the ordering that actually matters - the
    // join time, which drives wait-time averages - is carried across explicitly.
    queue.RemoveFromQueue(owner);
    queue.AddQueueData(owner, joinTime, expanded, roles);
    queue.AddToQueue(owner, false);

    for (GuidSet::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        SetSelectedDungeons(*it, expanded);
        SendLfgUpdateStatus(*it, LfgUpdateData(LFG_UPDATETYPE_ADDED_TO_QUEUE, expanded), !gguid.IsEmpty());
    }

    TC_LOG_INFO("lfg.queue.expand", "Owner {} expanded search from {} to {} dungeons, join time preserved",
        owner.ToString(), uint32(queued.size()), uint32(expanded.size()));
}

/**
    Adds the player/group to lfg queue. If player is in a group then it is the leader
    of the group tying to join the group. Join conditions are checked before adding
    to the new queue.

   @param[in]     player Player trying to join (or leader of group trying to join)
   @param[in]     roles Player selected roles
   @param[in]     dungeons Dungeons the player/group is applying for
*/
void LFGMgr::JoinLfg(Player* player, uint8 roles, LfgDungeonSet& dungeons)
{
    if (!player || !player->GetSession() || dungeons.empty())
        return;

    // Sanitize input roles
    roles &= PLAYER_ROLE_ANY;
    roles = FilterClassRoles(player, roles);

    // At least 1 role must be selected
    if (!(roles & (PLAYER_ROLE_TANK | PLAYER_ROLE_HEALER | PLAYER_ROLE_DAMAGE)))
        return;

    Group* grp = player->GetGroup();
    ObjectGuid guid = player->GetGUID();
    ObjectGuid gguid = grp ? grp->GetGUID() : guid;
    LfgJoinResultData joinData;
    GuidSet players;
    uint32 rDungeonId = 0;
    bool isContinue = grp && grp->isLFGGroup() && GetState(gguid) != LFG_STATE_FINISHED_DUNGEON;

    // Do not allow to change dungeon in the middle of a current dungeon
    if (isContinue)
    {
        dungeons.clear();
        dungeons.insert(GetDungeon(gguid));
    }

    // Already in queue?
    LfgState state = GetState(gguid);
    if (state == LFG_STATE_QUEUED)
    {
        LFGQueue& queue = GetQueue(gguid);
        queue.RemoveFromQueue(gguid);
    }

    // Check player or group member restrictions
    if (!player->GetSession()->HasPermission(rbac::RBAC_PERM_JOIN_DUNGEON_FINDER))
        joinData.result = LFG_JOIN_NO_SLOTS;
    else if (player->InBattleground() || player->InArena() || player->InBattlegroundQueue())
        joinData.result = LFG_JOIN_CANT_USE_DUNGEONS;
    else if (player->HasAura(LFG_SPELL_DUNGEON_DESERTER))
        joinData.result = LFG_JOIN_DESERTER_PLAYER;
    else if (!isContinue && player->HasAura(LFG_SPELL_DUNGEON_COOLDOWN))
        joinData.result = LFG_JOIN_RANDOM_COOLDOWN_PLAYER;
    else if (dungeons.empty())
        joinData.result = LFG_JOIN_NO_SLOTS;
    else if (player->HasAura(9454)) // check Freeze debuff
        joinData.result = LFG_JOIN_NO_SLOTS;
    else if (grp)
    {
        if (grp->GetMembersCount() > MAX_GROUP_SIZE)
            joinData.result = LFG_JOIN_TOO_MANY_MEMBERS;
        else
        {
            uint8 memberCount = 0;
            for (GroupReference const& itr : grp->GetMembers())
            {
                Player* plrg = itr.GetSource();
                if (!plrg->GetSession()->HasPermission(rbac::RBAC_PERM_JOIN_DUNGEON_FINDER))
                    joinData.result = LFG_JOIN_NO_LFG_OBJECT;
                if (plrg->HasAura(LFG_SPELL_DUNGEON_DESERTER))
                    joinData.result = LFG_JOIN_DESERTER_PARTY;
                else if (!isContinue && plrg->HasAura(LFG_SPELL_DUNGEON_COOLDOWN))
                    joinData.result = LFG_JOIN_RANDOM_COOLDOWN_PARTY;
                else if (plrg->InBattleground() || plrg->InArena() || plrg->InBattlegroundQueue())
                    joinData.result = LFG_JOIN_CANT_USE_DUNGEONS;
                else if (plrg->HasAura(9454)) // check Freeze debuff
                {
                    joinData.result = LFG_JOIN_NO_SLOTS;
                    joinData.playersMissingRequirement.push_back(plrg->GetName());
                }
                ++memberCount;
                players.insert(plrg->GetGUID());

                if (joinData.result != LFG_JOIN_OK)
                    break;
            }

            if (joinData.result == LFG_JOIN_OK && memberCount != grp->GetMembersCount())
                joinData.result = LFG_JOIN_MEMBERS_NOT_PRESENT;
        }
    }
    else
        players.insert(player->GetGUID());

    // Check if all dungeons are valid
    bool isRaid = false;
    if (joinData.result == LFG_JOIN_OK)
    {
        bool isDungeon = false;
        for (LfgDungeonSet::const_iterator it = dungeons.begin(); it != dungeons.end() && joinData.result == LFG_JOIN_OK; ++it)
        {
            LfgType type = GetDungeonType(*it);
            switch (type)
            {
                case LFG_TYPE_RANDOM:
                    if (dungeons.size() > 1)               // Only allow 1 random dungeon
                        joinData.result = LFG_JOIN_INVALID_SLOT;
                    else
                        rDungeonId = (*dungeons.begin());
                    [[fallthrough]]; // Random can only be dungeon or heroic dungeon
                case LFG_TYPE_HEROIC:
                case LFG_TYPE_DUNGEON:
                    if (isRaid)
                        joinData.result = LFG_JOIN_MISMATCHED_SLOTS;
                    isDungeon = true;
                    break;
                case LFG_TYPE_RAID:
                    if (isDungeon)
                        joinData.result = LFG_JOIN_MISMATCHED_SLOTS;
                    isRaid = true;
                    break;
                default:
                    joinData.result = LFG_JOIN_INVALID_SLOT;
                    break;
            }
        }

        // it could be changed
        if (joinData.result == LFG_JOIN_OK)
        {
            // Expand random dungeons and check restrictions
            if (rDungeonId)
                dungeons = GetDungeonsByRandom(rDungeonId);

            // if we have lockmap then there are no compatible dungeons
            GetCompatibleDungeons(&dungeons, players, &joinData.lockmap, &joinData.playersMissingRequirement, isContinue);
            if (dungeons.empty())
                joinData.result = LFG_JOIN_NO_SLOTS;
        }
    }

    // Can't join. Send result
    if (joinData.result != LFG_JOIN_OK)
    {
        TC_LOG_DEBUG("lfg.join", "{} joining with {} members. Result: {}, Dungeons: {}",
            guid.ToString(), grp ? grp->GetMembersCount() : 1, joinData.result, ConcatenateDungeons(dungeons));

        if (!dungeons.empty())                             // Only should show lockmap when have no dungeons available
            joinData.lockmap.clear();
        player->GetSession()->SendLfgJoinResult(joinData);
        return;
    }

    if (isRaid)
    {
        TC_LOG_DEBUG("lfg.join", "{} trying to join raid browser and it's disabled.", guid.ToString());
        return;
    }

    WorldPackets::LFG::RideTicket ticket;
    ticket.RequesterGuid = guid;
    ticket.Id = GetQueueId(gguid);
    ticket.Type = WorldPackets::LFG::RideType::Lfg;
    ticket.Time = int32(GameTime::GetGameTime());
    std::string debugNames = "";
    if (grp)                                               // Begin rolecheck
    {
        // Create new rolecheck
        LfgRoleCheck& roleCheck = RoleChecksStore[gguid];
        roleCheck.cancelTime = GameTime::GetGameTime() + LFG_TIME_ROLECHECK;
        roleCheck.state = LFG_ROLECHECK_INITIALITING;
        roleCheck.leader = guid;
        roleCheck.dungeons = dungeons;
        roleCheck.rDungeonId = rDungeonId;

        if (rDungeonId)
        {
            dungeons.clear();
            dungeons.insert(rDungeonId);
        }

        SetState(gguid, LFG_STATE_ROLECHECK);
        // Send update to player
        LfgUpdateData updateData = LfgUpdateData(LFG_UPDATETYPE_JOIN_QUEUE, dungeons);
        for (GroupReference const& itr : grp->GetMembers())
        {
            Player* plrg = itr.GetSource();
            ObjectGuid pguid = plrg->GetGUID();
            plrg->GetSession()->SendLfgUpdateStatus(updateData, true);
            SetState(pguid, LFG_STATE_ROLECHECK);
            SetTicket(pguid, ticket);
            if (!isContinue)
                SetSelectedDungeons(pguid, dungeons);
            roleCheck.roles[pguid] = 0;
            if (!debugNames.empty())
                debugNames.append(", ");
            debugNames.append(plrg->GetName());
        }
        // Update leader role
        UpdateRoleCheck(gguid, guid, roles);
    }
    else                                                   // Add player to queue
    {
        LfgRolesMap rolesMap;
        rolesMap[guid] = roles;
        LFGQueue& queue = GetQueue(guid);
        queue.AddQueueData(guid, GameTime::GetGameTime(), dungeons, rolesMap);

        if (!isContinue)
        {
            if (rDungeonId)
            {
                dungeons.clear();
                dungeons.insert(rDungeonId);
            }
            SetSelectedDungeons(guid, dungeons);
        }
        // Send update to player
        SetTicket(guid, ticket);
        SetRoles(guid, roles);
        player->GetSession()->SendLfgUpdateStatus(LfgUpdateData(LFG_UPDATETYPE_JOIN_QUEUE_INITIAL, dungeons), false);
        SetState(guid, LFG_STATE_QUEUED);
        player->GetSession()->SendLfgUpdateStatus(LfgUpdateData(LFG_UPDATETYPE_ADDED_TO_QUEUE, dungeons), false);
        player->GetSession()->SendLfgJoinResult(joinData);
        debugNames.append(player->GetName());
    }

    TC_LOG_DEBUG("lfg.join", "{} joined ({}), Members: {}. Dungeons ({}): {}", guid.ToString(),
        grp ? "group" : "player", debugNames, uint32(dungeons.size()), ConcatenateDungeons(dungeons));
}

/**
    Leaves Dungeon System. Player/Group is removed from queue, rolechecks, proposals
    or votekicks. Player or group needs to be not NULL and using Dungeon System

   @param[in]     guid Player or group guid
*/
void LFGMgr::LeaveLfg(ObjectGuid guid, bool disconnected)
{
    ObjectGuid gguid = guid.IsParty() ? guid : GetGroup(guid);

    TC_LOG_DEBUG("lfg.leave", "{} left ({})", guid.ToString(), guid == gguid ? "group" : "player");

    LfgState state = GetState(guid);
    switch (state)
    {
        case LFG_STATE_QUEUED:
            if (!gguid.IsEmpty())
            {
                LfgState newState = LFG_STATE_NONE;
                LfgState oldState = GetOldState(gguid);

                // Set the new state to LFG_STATE_DUNGEON/LFG_STATE_FINISHED_DUNGEON if the group is already in a dungeon
                // This is required in case a LFG group vote-kicks a player in a dungeon, queues, then leaves the queue (maybe to queue later again)
                if (Group* group = sGroupMgr->GetGroupByGUID(gguid))
                    if (group->isLFGGroup() && GetDungeon(gguid) && (oldState == LFG_STATE_DUNGEON || oldState == LFG_STATE_FINISHED_DUNGEON))
                        newState = oldState;

                LFGQueue& queue = GetQueue(gguid);
                queue.RemoveFromQueue(gguid);
                SetState(gguid, newState);
                GuidSet const& players = GetPlayers(gguid);
                for (GuidSet::const_iterator it = players.begin(); it != players.end(); ++it)
                {
                    SetState(*it, newState);
                    SendLfgUpdateStatus(*it, LfgUpdateData(LFG_UPDATETYPE_REMOVED_FROM_QUEUE), true);
                }
            }
            else
            {
                LFGQueue& queue = GetQueue(guid);
                queue.RemoveFromQueue(guid);
                SendLfgUpdateStatus(guid, LfgUpdateData(LFG_UPDATETYPE_REMOVED_FROM_QUEUE), false);
                SetState(guid, LFG_STATE_NONE);
            }
            break;
        case LFG_STATE_ROLECHECK:
            if (!gguid.IsEmpty())
                UpdateRoleCheck(gguid);                    // No player to update role = LFG_ROLECHECK_ABORTED
            break;
        case LFG_STATE_PROPOSAL:
        {
            // Remove from Proposals
            LfgProposalContainer::iterator it = ProposalsStore.begin();
            ObjectGuid pguid = gguid == guid ? GetLeader(gguid) : guid;
            while (it != ProposalsStore.end())
            {
                LfgProposalPlayerContainer::iterator itPlayer = it->second.players.find(pguid);
                if (itPlayer != it->second.players.end())
                {
                    // Mark the player/leader of group who left as didn't accept the proposal
                    itPlayer->second.accept = LFG_ANSWER_DENY;
                    break;
                }
                ++it;
            }

            // Remove from queue - if proposal is found, RemoveProposal will call RemoveFromQueue
            if (it != ProposalsStore.end())
                RemoveProposal(it, LFG_UPDATETYPE_PROPOSAL_DECLINED);
            break;
        }
        case LFG_STATE_NONE:
        case LFG_STATE_RAIDBROWSER:
            break;
        case LFG_STATE_DUNGEON:
        case LFG_STATE_FINISHED_DUNGEON:
            if (guid != gguid && !disconnected) // Player
                SetState(guid, LFG_STATE_NONE);
            break;
    }
}

WorldPackets::LFG::RideTicket const* LFGMgr::GetTicket(ObjectGuid guid) const
{
    auto itr = PlayersStore.find(guid);
    if (itr != PlayersStore.end())
        return &itr->second.GetTicket();

    return nullptr;
}

/**
   Update the Role check info with the player selected role.

   @param[in]     grp Group guid to update rolecheck
   @param[in]     guid Player guid (0 = rolecheck failed)
   @param[in]     roles Player selected roles
*/
void LFGMgr::UpdateRoleCheck(ObjectGuid gguid, ObjectGuid guid /* = ObjectGuid::Empty */, uint8 roles /* = PLAYER_ROLE_NONE */)
{
    if (!gguid)
        return;

    LfgRolesMap check_roles;
    LfgRoleCheckContainer::iterator itRoleCheck = RoleChecksStore.find(gguid);
    if (itRoleCheck == RoleChecksStore.end())
        return;

    // Sanitize input roles
    roles &= PLAYER_ROLE_ANY;

    if (!guid.IsEmpty())
    {
        if (Player* player = ObjectAccessor::FindPlayer(guid))
            roles = FilterClassRoles(player, roles);
        else
            return;
    }

    LfgRoleCheck& roleCheck = itRoleCheck->second;
    bool sendRoleChosen = roleCheck.state != LFG_ROLECHECK_DEFAULT && !guid.IsEmpty();

    if (!guid)
        roleCheck.state = LFG_ROLECHECK_ABORTED;
    else if (roles < PLAYER_ROLE_TANK)                            // Player selected no role.
        roleCheck.state = LFG_ROLECHECK_NO_ROLE;
    else
    {
        roleCheck.roles[guid] = roles;

        // Check if all players have selected a role
        LfgRolesMap::const_iterator itRoles = roleCheck.roles.begin();
        while (itRoles != roleCheck.roles.end() && itRoles->second != PLAYER_ROLE_NONE)
            ++itRoles;

        if (itRoles == roleCheck.roles.end())
        {
            // use temporal var to check roles, CheckGroupRoles modifies the roles
            check_roles = roleCheck.roles;
            roleCheck.state = CheckGroupRoles(check_roles) ? LFG_ROLECHECK_FINISHED : LFG_ROLECHECK_WRONG_ROLES;
        }
    }

    LfgDungeonSet dungeons;
    if (roleCheck.rDungeonId)
        dungeons.insert(roleCheck.rDungeonId);
    else
        dungeons = roleCheck.dungeons;

    LfgJoinResultData joinData = LfgJoinResultData(LFG_JOIN_ROLE_CHECK_FAILED, roleCheck.state);
    for (LfgRolesMap::const_iterator it = roleCheck.roles.begin(); it != roleCheck.roles.end(); ++it)
    {
        ObjectGuid pguid = it->first;

        if (sendRoleChosen)
            SendLfgRoleChosen(pguid, guid, roles);

        SendLfgRoleCheckUpdate(pguid, roleCheck);
        switch (roleCheck.state)
        {
            case LFG_ROLECHECK_INITIALITING:
                continue;
            case LFG_ROLECHECK_FINISHED:
                SetState(pguid, LFG_STATE_QUEUED);
                SetRoles(pguid, it->second);
                SendLfgUpdateStatus(pguid, LfgUpdateData(LFG_UPDATETYPE_ADDED_TO_QUEUE, dungeons), true);
                break;
            default:
                if (roleCheck.leader == pguid)
                    SendLfgJoinResult(pguid, joinData);
                SendLfgUpdateStatus(pguid, LfgUpdateData(LFG_UPDATETYPE_ROLECHECK_FAILED), true);
                RestoreState(pguid, "Rolecheck Failed");
                break;
        }
    }

    if (roleCheck.state == LFG_ROLECHECK_FINISHED)
    {
        SetState(gguid, LFG_STATE_QUEUED);
        LFGQueue& queue = GetQueue(gguid);
        queue.AddQueueData(gguid, time_t(GameTime::GetGameTime()), roleCheck.dungeons, roleCheck.roles);
        RoleChecksStore.erase(itRoleCheck);
    }
    else if (roleCheck.state != LFG_ROLECHECK_INITIALITING)
    {
        RestoreState(gguid, "Rolecheck Failed");
        RoleChecksStore.erase(itRoleCheck);
    }
}

/**
   Starts an LFG readiness check for a group.

   Wire contract (client 12.1.0.69382): SMSG_LFG_READY_CHECK_UPDATE (0x5A0006) drives LFGReadyCheckPopup.
   The consumer @ RVA 0x24C2920 shows the dialog only while ReadyCheckStatus == 2 AND the receiver's own
   member entry still has IsReady == 0; every other combination hides it. The four terminal states each
   carry their own GameError (829 / 804 / 844 / 803), so a check must always be closed out explicitly -
   dropping it silently would leave the popup on screen.

   UNVERIFIED - THE RETAIL TRIGGER, and this function HAS NO CALLER IN-TREE. That is a deliberate state,
   not an oversight, and the next reader is asked not to "fix" it by attaching it to a working path.

   What the client settles: the dialog only resolves a real queue name when readyCheckIsBattleground is
   true (Blizzard_GroupFinder/Shared/LFGReadyCheck.lua:13-17), QueueStatusFrame.lua:671-680 creates its
   status entry only then, and readyCheckIsBattleground is exactly "BgQueueIDs is not empty"
   (GetLFGReadyCheckUpdate @ RVA 0x24CF010). So the check belongs to a PvP queue.

   What the client also settles is WHICH one, and it is not one this tree has: Blizzard_PVPMatch/
   PVPMatchResults.lua:227-229 gates its requeue button on LFG_READY_CHECK_SHOW / LFG_READY_CHECK_DECLINED,
   and that button calls RequeueSkirmish() (PVPMatchResults.lua:492) = CMSG_BATTLEMASTER_JOIN_SKIRMISH
   (0x3E00C1) - an opcode TrinityCore does not handle at all, in family 0x3E, i.e. another unit's work.

   A previous revision of this branch borrowed WorldSession::HandleBattlemasterJoinArena as a stand-in
   trigger. That was withdrawn on 2026-08-27 (audit finding 1 on lfg_5A) and must stay withdrawn: the
   substitution is destructive, because FinishReadyCheck calls LeaveReadyCheckQueues for every outcome
   except LFG_READYCHECK_FINISHED. Attached to a live rated queue that means every group whose members do
   not all answer within LFG_TIME_ROLECHECK (45 s) is thrown out of the arena queue - a working, entirely
   server-side path made dependent on client interaction, on a guess. Any future caller has to accept that
   consequence for the queues it passes in, so only the queue whose OWN retail flow contains the check may
   ever be wired here.

   The same consequence has a second entrance, and a caller has to know about it: a member who LEAVES the
   group during the check. Its LFG_ANSWER_PENDING would be unanswerable, the check would run into the 45 s
   timeout and LeaveReadyCheckQueues would throw the remaining members - all of whom agreed - out of the
   queues. LFGGroupScript::OnRemoveMember therefore calls RemoveReadyCheckMember, which drops the answer and
   re-evaluates the check exactly as an answer does. Only OnDisband aborts the whole check. That path is not
   optional bookkeeping; it is what keeps the two failure modes of this function to ONE - a real decline and
   a real timeout.

   ACCEPTANCE, held open on purpose (audit 2026-08-27, lfg_5A round 4, finding 2, D2 gap on
   SMSG_LFG_READY_CHECK_UPDATE 0x5A0006, CMSG_DF_READY_CHECK_RESPONSE 0x430048 and
   SMSG_LFG_READY_CHECK_RESULT 0x5A001E): this function has NO caller anywhere in the tree, and neither
   does HasReadyCheck. ReadyChecksStore therefore stays empty for good, both SMSG are unreachable, and
   WorldSession::HandleDFReadyCheckResponse returns from UpdateReadyCheck's first `if` for every packet it
   ever receives. The wire is complete and correct (D1), the state machine is complete including all four
   end states and their GameErrors (D3), but D2 is OPEN for all three opcodes and they are NOT acceptable.
   The gap is recorded in orchestrierung/status/lfg_5A.json under dod_luecken.D2.

   Do NOT close it with a substitute trigger - see the withdrawn arena trigger above. The one belonging
   trigger, CMSG_BATTLEMASTER_JOIN_SKIRMISH (0x3E00C1), sits in family 0x3E, which is outside this unit
   and, as of 2026-08-27, is not assigned to ANY unit in orchestrierung/familien.json. The link is carried
   there under offene_serverarbeit, id lfg_bereitschaftscheck_ausloeser: whoever takes on 0x3E inherits
   this gap together with the requeue capture named in lfg_5A.json, aufnahme_noetig.

   @param[in]     gguid Group guid to start the readycheck for
   @param[in]     bgQueueIDs Battleground queue ids; non-empty selects the battleground dialog variant
   @param[in]     isRequeue Payload of LFG_READY_CHECK_SHOW(isRequeue)
   @param[in]     partyIndex Value the client echoes back in CMSG_DF_READY_CHECK_RESPONSE
   @return true if a check was started
*/
bool LFGMgr::StartReadyCheck(ObjectGuid gguid, std::vector<uint64> bgQueueIDs /*= {}*/, bool isRequeue /*= false*/, uint8 partyIndex /*= 127*/)
{
    if (!gguid.IsParty())
        return false;

    if (ReadyChecksStore.find(gguid) != ReadyChecksStore.end())
        return false;

    Group const* group = sGroupMgr->GetGroupByGUID(gguid);
    if (!group)
        return false;

    LfgReadyCheck& readyCheck = ReadyChecksStore[gguid];
    readyCheck.cancelTime = GameTime::GetGameTime() + LFG_TIME_ROLECHECK;
    readyCheck.state = LFG_READYCHECK_INITIALITING;
    readyCheck.leader = group->GetLeaderGUID();
    readyCheck.bgQueueIDs = std::move(bgQueueIDs);
    readyCheck.partyIndex = partyIndex;
    readyCheck.isRequeue = isRequeue;

    for (Group::MemberSlot const& slot : group->GetMemberSlots())
        readyCheck.answers[slot.guid] = LFG_ANSWER_PENDING;

    if (readyCheck.answers.empty())
    {
        ReadyChecksStore.erase(gguid);
        return false;
    }

    SendReadyCheckUpdate(readyCheck);
    return true;
}

bool LFGMgr::HasReadyCheck(ObjectGuid gguid) const
{
    return ReadyChecksStore.find(gguid) != ReadyChecksStore.end();
}

/**
   Records one player's answer to a running readiness check (CMSG_DF_READY_CHECK_RESPONSE, 0x430048).

   Every answer is broadcast as SMSG_LFG_READY_CHECK_RESULT, and BOTH flanks matter (D3):
     Ready == true  -> LFG_READY_CHECK_PLAYER_IS_READY(name)
     Ready == false -> GameError 831 ERR_LFG_PLAYER_DECLINED_READY_CHECK *and* LFG_READY_CHECK_DECLINED(name)
   A decline fails the whole check (status 5), mirroring how a refused role fails a role check.

   @param[in]     gguid Group guid the check belongs to
   @param[in]     guid Player that answered
   @param[in]     isReady The answer
*/
void LFGMgr::UpdateReadyCheck(ObjectGuid gguid, ObjectGuid guid, bool isReady)
{
    LfgReadyCheckContainer::iterator itReadyCheck = ReadyChecksStore.find(gguid);
    if (itReadyCheck == ReadyChecksStore.end())
        return;

    LfgReadyCheck& readyCheck = itReadyCheck->second;
    LfgAnswerContainer::iterator itAnswer = readyCheck.answers.find(guid);
    if (itAnswer == readyCheck.answers.end())
        return;

    // An answer is final - a second CMSG for the same player must not restart the state machine.
    if (itAnswer->second != LFG_ANSWER_PENDING)
        return;

    itAnswer->second = isReady ? LFG_ANSWER_AGREE : LFG_ANSWER_DENY;

    // SMSG_LFG_READY_CHECK_RESULT goes to everyone in the check, including the answering player -
    // the consumer needs it to print the "<name> is ready" / "<name> declined" line.
    SendReadyCheckResult(readyCheck, guid, isReady);

    if (!isReady)
    {
        FinishReadyCheck(itReadyCheck, LFG_READYCHECK_FAILED);
        return;
    }

    bool const allReady = std::all_of(readyCheck.answers.begin(), readyCheck.answers.end(),
        [](LfgAnswerContainer::value_type const& answer) { return answer.second == LFG_ANSWER_AGREE; });

    if (allReady)
    {
        FinishReadyCheck(itReadyCheck, LFG_READYCHECK_FINISHED);
        return;
    }

    // Still running: refresh everyone's dialog so members who already answered see their own popup
    // disappear (the consumer hides it as soon as the receiver's own IsReady flag is set).
    SendReadyCheckUpdate(readyCheck);
}

/**
   Aborts a running readiness check -> status 4, ERR_LFG_READY_CHECK_ABORTED (844).
*/
void LFGMgr::AbortReadyCheck(ObjectGuid gguid)
{
    LfgReadyCheckContainer::iterator itReadyCheck = ReadyChecksStore.find(gguid);
    if (itReadyCheck == ReadyChecksStore.end())
        return;

    FinishReadyCheck(itReadyCheck, LFG_READYCHECK_ABORTED);
}

/**
   A member left the group while the check was running - take its answer out and carry on with the rest.

   StartReadyCheck fills `answers` once from the member list and nothing used to maintain it afterwards, so a
   departure left a permanently PENDING entry: std::all_of in UpdateReadyCheck could never come out true, the
   check ran to its LFG_TIME_ROLECHECK timeout and FinishReadyCheck, seeing a state other than
   LFG_READYCHECK_FINISHED, called LeaveReadyCheckQueues - the remaining members were dropped from every
   queue in bgQueueIDs even though every one of them had agreed. That is the same destructive outcome the
   withdrawn arena trigger was found for, reached without any trigger at all.

   The three outcomes here are the three the state machine already has, no new ones:
     - nobody left in the check   -> abort (the dialog has to be closed on the clients that still show it)
     - everyone remaining agreed  -> the check is finished, which is what it would have been had the departed
                                     member simply answered yes
     - still someone pending      -> keep running, and refresh the dialog so the leaver disappears from it
   A member that had already DECLINED cannot be reached here: that answer ends the check on the spot.

   What this function deliberately does NOT do is fix up readyCheck.leader when the departing player was the
   leader. Re-reading the group's leader here would be wrong, not merely useless: Group::RemoveMember fires
   this hook at its very top (Group.cpp:551), long before it promotes the new leader (Group.cpp:626), so
   GetLeaderGUID() still returns the player that is leaving. The promotion is picked up one step later, by
   SetReadyCheckLeader from LFGGroupScript::OnChangeLeader - Group::RemoveMember reaches the new leader
   through Group::ChangeLeader (Group.cpp:626), which fires OnGroupChangeLeader (Group.cpp:676) before it
   touches anything else. So the update this function sends can still be missing slot 0, and the one
   SetReadyCheckLeader sends right afterwards has it again.

   @param[in]     gguid Group guid the check belongs to
   @param[in]     guid Player that left the group
*/
void LFGMgr::RemoveReadyCheckMember(ObjectGuid gguid, ObjectGuid guid)
{
    LfgReadyCheckContainer::iterator itReadyCheck = ReadyChecksStore.find(gguid);
    if (itReadyCheck == ReadyChecksStore.end())
        return;

    LfgReadyCheck& readyCheck = itReadyCheck->second;
    if (!readyCheck.answers.erase(guid))
        return;

    if (readyCheck.answers.empty())
    {
        FinishReadyCheck(itReadyCheck, LFG_READYCHECK_ABORTED);
        return;
    }

    bool const allReady = std::all_of(readyCheck.answers.begin(), readyCheck.answers.end(),
        [](LfgAnswerContainer::value_type const& answer) { return answer.second == LFG_ANSWER_AGREE; });

    if (allReady)
    {
        FinishReadyCheck(itReadyCheck, LFG_READYCHECK_FINISHED);
        return;
    }

    SendReadyCheckUpdate(readyCheck);
}

/**
   The group got a new leader while the check was running - carry that into the check and refresh the dialog.

   readyCheck.leader is filled once by StartReadyCheck and decides ONE thing: which member
   SendLfgReadyCheckUpdate puts into slot 0 of the member list, because the client reads that list
   positionally and expects the leader first (LFGHandler.cpp, SendLfgReadyCheckUpdate; the same requirement
   is spelled out in the tree for the sister message, LFGHandler.cpp:515-517 "Leader info MUST be sent 1st").
   Nothing else in the check consults the field, so a promotion cannot break the state machine - it can only
   make every following update name the wrong player as leader.

   Both ways into a promotion end up here, and the second is the more common one:
     - the leader LEFT       -> Group::RemoveMember erases the member slot and then picks a new leader via
                                Group::ChangeLeader (Group.cpp:626). RemoveReadyCheckMember has already run
                                at that point (OnGroupRemoveMember, Group.cpp:551) and its update went out
                                without slot 0; this one puts it back.
     - a plain LEADER CHANGE -> no member leaves, nobody is erased from `answers`, and without this hook the
                                OLD leader would stay in the field and keep occupying slot 0 for the rest of
                                the check. Four paths in the tree reach it: CMSG_SET_PARTY_LEADER
                                (GroupHandler.cpp:306), the offline-leader timer (Group::Update ->
                                Group::SelectNewPartyOrRaidLeader, Group.cpp:97/136), the `.group leader`
                                command (cs_group.cpp:267) and the battleground leader pick
                                (Battleground.cpp:1112).
   Group::ChangeLeader fires OnGroupChangeLeader (Group.cpp:676) before it updates m_leaderGuid, so the hook
   passes the new guid explicitly and GetLeaderGUID() must NOT be used to obtain it.

   The refresh is skipped when the new leader does not take part in the check (nothing in the tree adds a
   member to a running check, but a caller could): SendLfgReadyCheckUpdate leaves slot 0 out for an unknown
   leader, so the message would be byte-identical to the previous one.

   @param[in]     gguid Group guid the check belongs to
   @param[in]     leaderGuid The new leader, as handed over by the hook
*/
void LFGMgr::SetReadyCheckLeader(ObjectGuid gguid, ObjectGuid leaderGuid)
{
    LfgReadyCheckContainer::iterator itReadyCheck = ReadyChecksStore.find(gguid);
    if (itReadyCheck == ReadyChecksStore.end())
        return;

    LfgReadyCheck& readyCheck = itReadyCheck->second;
    if (readyCheck.leader == leaderGuid)
        return;

    readyCheck.leader = leaderGuid;

    if (readyCheck.answers.find(leaderGuid) == readyCheck.answers.end())
        return;

    SendReadyCheckUpdate(readyCheck);
}

/// Sends the current state of a readiness check to every member still in it.
void LFGMgr::SendReadyCheckUpdate(LfgReadyCheck const& readyCheck) const
{
    for (LfgAnswerContainer::value_type const& answer : readyCheck.answers)
        if (Player* player = ObjectAccessor::FindConnectedPlayer(answer.first))
            player->GetSession()->SendLfgReadyCheckUpdate(readyCheck);
}

/// Sends one player's answer to every member of the check.
void LFGMgr::SendReadyCheckResult(LfgReadyCheck const& readyCheck, ObjectGuid guid, bool isReady) const
{
    for (LfgAnswerContainer::value_type const& answer : readyCheck.answers)
        if (Player* player = ObjectAccessor::FindConnectedPlayer(answer.first))
            player->GetSession()->SendLfgReadyCheckResult(guid, isReady);
}

/// Pushes the terminal state out and drops the check. Erasing without sending would leave
/// LFGReadyCheckPopup open on every client - only an update ever closes that dialog.
/// A check that did NOT end in LFG_READYCHECK_FINISHED also has to undo what it was guarding: the group
/// leaves the queues named in bgQueueIDs. Without that the check would be a dialog with no consequence -
/// members could decline and the group would sit in the queue anyway.
void LFGMgr::FinishReadyCheck(LfgReadyCheckContainer::iterator itReadyCheck, LfgReadyCheckState state)
{
    itReadyCheck->second.state = state;
    SendReadyCheckUpdate(itReadyCheck->second);

    if (state != LFG_READYCHECK_FINISHED)
        LeaveReadyCheckQueues(itReadyCheck->second);

    ReadyChecksStore.erase(itReadyCheck);
}

/**
   Drops every member of a failed readiness check out of the battleground queues the check was about.

   The consequence is not invented: Blizzard_PVPMatch/PVPMatchResults.lua:227-229 re-enables the requeue
   button on LFG_READY_CHECK_DECLINED, i.e. after a refusal the group is NOT in the queue and may try
   again. The sequence used here is the one CMSG_BATTLEFIELD_PORT already uses to leave a queue
   (BattleGroundHandler.cpp): SMSG_BATTLEFIELD_STATUS_NONE, then Player::RemoveBattlegroundQueueId, then
   BattlegroundQueue::RemovePlayer - in that order, because moving the first removal into the queue causes
   known bugs (comment at the original site).

   UNVERIFIED: that retail resolves a refusal by dropping the queue entry rather than by never creating it.
   Both end with "the group is not queued"; which of the two retail does is not visible from the client.
*/
void LFGMgr::LeaveReadyCheckQueues(LfgReadyCheck const& readyCheck)
{
    for (uint64 packedQueueId : readyCheck.bgQueueIDs)
    {
        BattlegroundQueueTypeId const bgQueueTypeId = BattlegroundQueueTypeId::FromPacked(packedQueueId);
        BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);

        for (LfgAnswerContainer::value_type const& answer : readyCheck.answers)
        {
            Player* player = ObjectAccessor::FindConnectedPlayer(answer.first);
            if (!player || !player->InBattlegroundQueueForBattlegroundQueueType(bgQueueTypeId))
                continue;

            WorldPackets::Battleground::BattlefieldStatusNone battlefieldStatus;
            BattlegroundMgr::BuildBattlegroundStatusNone(&battlefieldStatus, player,
                player->GetBattlegroundQueueIndex(bgQueueTypeId), player->GetBattlegroundQueueJoinTime(bgQueueTypeId));
            player->SendDirectMessage(battlefieldStatus.Write());

            player->RemoveBattlegroundQueueId(bgQueueTypeId);
            bgQueue.RemovePlayer(player->GetGUID(), true);
        }
    }
}

/**
   Given a list of dungeons remove the dungeons players have restrictions.

   @param[in, out] dungeons Dungeons to check restrictions
   @param[in]     players Set of players to check their dungeon restrictions
   @param[out]    lockMap Map of players Lock status info of given dungeons (Empty if dungeons is not empty)
*/
void LFGMgr::GetCompatibleDungeons(LfgDungeonSet* dungeons, GuidSet const& players, LfgLockPartyMap* lockMap, std::vector<std::string_view>* playersMissingRequirement, bool isContinue)
{
    lockMap->clear();

    std::map<uint32, uint32> lockedDungeons;
    std::unordered_set<uint32> dungeonsToRemove;

    for (GuidSet::const_iterator it = players.begin(); it != players.end() && !dungeons->empty(); ++it)
    {
        ObjectGuid guid = (*it);
        LfgLockMap cachedLockMap = GetLockedDungeons(guid);
        Player* player = ObjectAccessor::FindConnectedPlayer(guid);
        for (LfgLockMap::const_iterator it2 = cachedLockMap.begin(); it2 != cachedLockMap.end() && !dungeons->empty(); ++it2)
        {
            uint32 dungeonId = (it2->first & 0x00FFFFFF); // Compare dungeon ids
            LfgDungeonSet::iterator itDungeon = dungeons->find(dungeonId);
            if (itDungeon != dungeons->end())
            {
                bool eraseDungeon = true;

                // Don't remove the dungeon if team members are trying to continue a locked instance
                if (it2->second.lockStatus == LFG_LOCKSTATUS_RAID_LOCKED && isContinue)
                {
                    LFGDungeonData const* dungeon = GetLFGDungeon(dungeonId);
                    ASSERT(dungeon);
                    ASSERT(player);
                    MapDb2Entries entries{ dungeon->map, Difficulty(dungeon->difficulty) };
                    if (InstanceLock* playerBind = sInstanceLockMgr.FindActiveInstanceLock(guid, entries))
                    {
                        uint32 dungeonInstanceId = playerBind->GetInstanceId();
                        auto itLockedDungeon = lockedDungeons.find(dungeonId);
                        if (itLockedDungeon == lockedDungeons.end() || itLockedDungeon->second == dungeonInstanceId)
                            eraseDungeon = false;

                        lockedDungeons[dungeonId] = dungeonInstanceId;
                    }
                }

                if (eraseDungeon)
                    dungeonsToRemove.insert(dungeonId);

                (*lockMap)[guid][dungeonId] = it2->second;
                playersMissingRequirement->push_back(player->GetName());
            }
        }
    }

    for (uint32 dungeonIdToRemove : dungeonsToRemove)
        dungeons->erase(dungeonIdToRemove);

    if (!dungeons->empty())
        lockMap->clear();
}

/**
   Check if a group can be formed with the given group roles

   @param[in]     groles Map of roles to check
   @return True if roles are compatible
*/
bool LFGMgr::CheckGroupRoles(LfgRolesMap& groles)
{
    if (groles.empty())
        return false;

    uint8 damage = 0;
    uint8 tank = 0;
    uint8 healer = 0;

    for (LfgRolesMap::iterator it = groles.begin(); it != groles.end(); ++it)
    {
        uint8 role = it->second & ~PLAYER_ROLE_LEADER;
        if (role == PLAYER_ROLE_NONE)
            return false;

        if (role & PLAYER_ROLE_DAMAGE)
        {
            if (role != PLAYER_ROLE_DAMAGE)
            {
                it->second -= PLAYER_ROLE_DAMAGE;
                if (CheckGroupRoles(groles))
                    return true;
                it->second += PLAYER_ROLE_DAMAGE;
            }
            else if (damage == LFG_DPS_NEEDED)
                return false;
            else
                damage++;
        }

        if (role & PLAYER_ROLE_HEALER)
        {
            if (role != PLAYER_ROLE_HEALER)
            {
                it->second -= PLAYER_ROLE_HEALER;
                if (CheckGroupRoles(groles))
                    return true;
                it->second += PLAYER_ROLE_HEALER;
            }
            else if (healer == LFG_HEALERS_NEEDED)
                return false;
            else
                healer++;
        }

        if (role & PLAYER_ROLE_TANK)
        {
            if (role != PLAYER_ROLE_TANK)
            {
                it->second -= PLAYER_ROLE_TANK;
                if (CheckGroupRoles(groles))
                    return true;
                it->second += PLAYER_ROLE_TANK;
            }
            else if (tank == LFG_TANKS_NEEDED)
                return false;
            else
                tank++;
        }
    }
    return (tank + healer + damage) == uint8(groles.size());
}

/**
   Makes a new group given a proposal
   @param[in]     proposal Proposal to get info from
*/
void LFGMgr::MakeNewGroup(LfgProposal const& proposal)
{
    GuidList players, tankPlayers, healPlayers, dpsPlayers;
    GuidList playersToTeleport;

    for (LfgProposalPlayerContainer::const_iterator it = proposal.players.begin(); it != proposal.players.end(); ++it)
    {
        ObjectGuid guid = it->first;
        if (guid == proposal.leader)
            players.push_back(guid);
        else
            switch (it->second.role & ~PLAYER_ROLE_LEADER)
            {
                case PLAYER_ROLE_TANK:
                    tankPlayers.push_back(guid);
                    break;
                case PLAYER_ROLE_HEALER:
                    healPlayers.push_back(guid);
                    break;
                case PLAYER_ROLE_DAMAGE:
                    dpsPlayers.push_back(guid);
                    break;
                default:
                    ABORT_MSG("Invalid LFG role %u", it->second.role);
                    break;
            }

        if (proposal.isNew || GetGroup(guid) != proposal.group)
            playersToTeleport.push_back(guid);
    }

    players.splice(players.end(), tankPlayers);
    players.splice(players.end(), healPlayers);
    players.splice(players.end(), dpsPlayers);

    // Set the dungeon difficulty
    LFGDungeonData const* dungeon = GetLFGDungeon(proposal.dungeonId);
    ASSERT(dungeon);

    Group* grp = !proposal.group.IsEmpty() ? sGroupMgr->GetGroupByGUID(proposal.group) : nullptr;
    for (GuidList::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        ObjectGuid pguid = (*it);
        Player* player = ObjectAccessor::FindConnectedPlayer(pguid);
        if (!player)
            continue;

        Group* group = player->GetGroup();
        if (group && group != grp)
            group->RemoveMember(player->GetGUID());

        if (!grp)
        {
            grp = new Group();
            grp->ConvertToLFG();
            grp->Create(player);
            ObjectGuid gguid = grp->GetGUID();
            SetState(gguid, LFG_STATE_PROPOSAL);
            sGroupMgr->AddGroup(grp);
        }
        else if (group != grp)
            grp->AddMember(player);

        grp->SetLfgRoles(pguid, proposal.players.find(pguid)->second.role);

        // Add the cooldown spell if queued for a random dungeon
        LfgDungeonSet const& dungeons = GetSelectedDungeons(player->GetGUID());
        if (!dungeons.empty())
        {
            uint32 rDungeonId = (*dungeons.begin());
            LFGDungeonData const* dungeonEntry = GetLFGDungeon(rDungeonId);
            if (dungeonEntry && dungeonEntry->type == LFG_TYPE_RANDOM)
                player->CastSpell(player, LFG_SPELL_DUNGEON_COOLDOWN, false);
        }
    }

    ASSERT(grp);
    grp->SetDungeonDifficultyID(Difficulty(dungeon->difficulty));
    ObjectGuid gguid = grp->GetGUID();
    SetDungeon(gguid, dungeon->Entry());
    SetState(gguid, LFG_STATE_DUNGEON);

    _SaveToDB(gguid, grp->GetDbStoreId());

    // Teleport Player
    for (GuidList::const_iterator it = playersToTeleport.begin(); it != playersToTeleport.end(); ++it)
        if (Player* player = ObjectAccessor::FindPlayer(*it))
            TeleportPlayer(player, false);

    // Update group info
    grp->SendUpdate();
}

uint32 LFGMgr::AddProposal(LfgProposal& proposal)
{
    proposal.id = ++m_lfgProposalId;
    ProposalsStore[m_lfgProposalId] = proposal;
    return m_lfgProposalId;
}

/**
   Update Proposal info with player answer

   @param[in]     proposalId Proposal id to be updated
   @param[in]     guid Player guid to update answer
   @param[in]     accept Player answer
*/
void LFGMgr::UpdateProposal(uint32 proposalId, ObjectGuid guid, bool accept)
{
    // Check if the proposal exists
    LfgProposalContainer::iterator itProposal = ProposalsStore.find(proposalId);
    if (itProposal == ProposalsStore.end())
        return;

    LfgProposal& proposal = itProposal->second;

    // Check if proposal have the current player
    LfgProposalPlayerContainer::iterator itProposalPlayer = proposal.players.find(guid);
    if (itProposalPlayer == proposal.players.end())
        return;

    LfgProposalPlayer& player = itProposalPlayer->second;
    player.accept = LfgAnswer(accept);

    TC_LOG_DEBUG("lfg.proposal.update", "{}, Proposal {}, Selection: {}", guid.ToString(), proposalId, accept);
    if (!accept)
    {
        RemoveProposal(itProposal, LFG_UPDATETYPE_PROPOSAL_DECLINED);
        return;
    }

    // check if all have answered and reorder players (leader first)
    bool allAnswered = true;
    for (LfgProposalPlayerContainer::const_iterator itPlayers = proposal.players.begin(); itPlayers != proposal.players.end(); ++itPlayers)
        if (itPlayers->second.accept != LFG_ANSWER_AGREE)   // No answer (-1) or not accepted (0)
            allAnswered = false;

    if (!allAnswered)
    {
        for (LfgProposalPlayerContainer::const_iterator it = proposal.players.begin(); it != proposal.players.end(); ++it)
            SendLfgUpdateProposal(it->first, proposal);

        return;
    }

    bool sendUpdate = proposal.state != LFG_PROPOSAL_SUCCESS;
    proposal.state = LFG_PROPOSAL_SUCCESS;
    time_t joinTime = GameTime::GetGameTime();

    LFGQueue& queue = GetQueue(guid);
    LfgUpdateData updateData = LfgUpdateData(LFG_UPDATETYPE_GROUP_FOUND);
    for (LfgProposalPlayerContainer::const_iterator it = proposal.players.begin(); it != proposal.players.end(); ++it)
    {
        ObjectGuid pguid = it->first;
        ObjectGuid gguid = it->second.group;
        uint32 dungeonId = (*GetSelectedDungeons(pguid).begin());
        int32 waitTime = -1;
        if (sendUpdate)
           SendLfgUpdateProposal(pguid, proposal);

        if (!gguid.IsEmpty())
        {
            waitTime = int32((joinTime - queue.GetJoinTime(gguid)) / IN_MILLISECONDS);
            SendLfgUpdateStatus(pguid, updateData, false);
        }
        else
        {
            waitTime = int32((joinTime - queue.GetJoinTime(pguid)) / IN_MILLISECONDS);
            SendLfgUpdateStatus(pguid, updateData, false);
        }
        updateData.updateType = LFG_UPDATETYPE_REMOVED_FROM_QUEUE;
        SendLfgUpdateStatus(pguid, updateData, true);
        SendLfgUpdateStatus(pguid, updateData, false);

        // Update timers
        uint8 role = GetRoles(pguid);
        role &= ~PLAYER_ROLE_LEADER;
        switch (role)
        {
            case PLAYER_ROLE_DAMAGE:
                queue.UpdateWaitTimeDps(waitTime, dungeonId);
                break;
            case PLAYER_ROLE_HEALER:
                queue.UpdateWaitTimeHealer(waitTime, dungeonId);
                break;
            case PLAYER_ROLE_TANK:
                queue.UpdateWaitTimeTank(waitTime, dungeonId);
                break;
            default:
                queue.UpdateWaitTimeAvg(waitTime, dungeonId);
                break;
        }

        // Store the number of players that were present in group when joining RFD, used for achievement purposes
        if (Player* player = ObjectAccessor::FindConnectedPlayer(pguid))
            if (Group* group = player->GetGroup())
                PlayersStore[pguid].SetNumberOfPartyMembersAtJoin(group->GetMembersCount());

        SetState(pguid, LFG_STATE_DUNGEON);
    }

    // Remove players/groups from Queue
    for (GuidList::const_iterator it = proposal.queues.begin(); it != proposal.queues.end(); ++it)
        queue.RemoveFromQueue(*it);

    MakeNewGroup(proposal);
    ProposalsStore.erase(itProposal);
}

/**
   Remove a proposal from the pool, remove the group that didn't accept (if needed) and readd the other members to the queue

   @param[in]     itProposal Iterator to the proposal to remove
   @param[in]     type Type of removal (LFG_UPDATETYPE_PROPOSAL_FAILED, LFG_UPDATETYPE_PROPOSAL_DECLINED)
*/
void LFGMgr::RemoveProposal(LfgProposalContainer::iterator itProposal, LfgUpdateType type)
{
    LfgProposal& proposal = itProposal->second;
    proposal.state = LFG_PROPOSAL_FAILED;

    TC_LOG_DEBUG("lfg.proposal.remove", "Proposal {}, state FAILED, UpdateType {}", itProposal->first, type);
    // Mark all people that didn't answered as no accept
    if (type == LFG_UPDATETYPE_PROPOSAL_FAILED)
        for (LfgProposalPlayerContainer::iterator it = proposal.players.begin(); it != proposal.players.end(); ++it)
            if (it->second.accept == LFG_ANSWER_PENDING)
                it->second.accept = LFG_ANSWER_DENY;

    // Mark players/groups to be removed
    GuidSet toRemove;
    for (LfgProposalPlayerContainer::iterator it = proposal.players.begin(); it != proposal.players.end(); ++it)
    {
        if (it->second.accept == LFG_ANSWER_AGREE)
            continue;

        ObjectGuid guid = !it->second.group.IsEmpty() ? it->second.group : it->first;
        // Player didn't accept or still pending when no secs left
        if (it->second.accept == LFG_ANSWER_DENY || type == LFG_UPDATETYPE_PROPOSAL_FAILED)
        {
            it->second.accept = LFG_ANSWER_DENY;
            toRemove.insert(guid);
        }
    }

    // Notify players
    for (LfgProposalPlayerContainer::const_iterator it = proposal.players.begin(); it != proposal.players.end(); ++it)
    {
        ObjectGuid guid = it->first;
        ObjectGuid gguid = !it->second.group.IsEmpty() ? it->second.group : guid;

        SendLfgUpdateProposal(guid, proposal);

        if (toRemove.find(gguid) != toRemove.end())         // Didn't accept or in same group that someone that didn't accept
        {
            LfgUpdateData updateData;
            if (it->second.accept == LFG_ANSWER_DENY)
            {
                updateData.updateType = type;
                TC_LOG_DEBUG("lfg.proposal.remove", "{} didn't accept. Removing from queue and compatible cache", guid.ToString());
            }
            else
            {
                updateData.updateType = LFG_UPDATETYPE_REMOVED_FROM_QUEUE;
                TC_LOG_DEBUG("lfg.proposal.remove", "{} in same group that someone that didn't accept. Removing from queue and compatible cache", guid.ToString());
            }

            RestoreState(guid, "Proposal Fail (didn't accepted or in group with someone that didn't accept");
            if (gguid != guid)
            {
                RestoreState(it->second.group, "Proposal Fail (someone in group didn't accepted)");
                SendLfgUpdateStatus(guid, updateData, true);
            }
            else
                SendLfgUpdateStatus(guid, updateData, false);
        }
        else
        {
            TC_LOG_DEBUG("lfg.proposal.remove", "Readding {} to queue.", guid.ToString());
            SetState(guid, LFG_STATE_QUEUED);
            if (gguid != guid)
            {
                SetState(gguid, LFG_STATE_QUEUED);
                SendLfgUpdateStatus(guid, LfgUpdateData(LFG_UPDATETYPE_ADDED_TO_QUEUE, GetSelectedDungeons(guid)), true);
            }
            else
                SendLfgUpdateStatus(guid, LfgUpdateData(LFG_UPDATETYPE_ADDED_TO_QUEUE, GetSelectedDungeons(guid)), false);
        }
    }

    LFGQueue& queue = GetQueue(proposal.players.begin()->first);
    // Remove players/groups from queue
    for (GuidSet::const_iterator it = toRemove.begin(); it != toRemove.end(); ++it)
    {
        ObjectGuid guid = *it;
        queue.RemoveFromQueue(guid);
        proposal.queues.remove(guid);
    }

    // Readd to queue
    for (GuidList::const_iterator it = proposal.queues.begin(); it != proposal.queues.end(); ++it)
    {
        ObjectGuid guid = *it;
        queue.AddToQueue(guid, true);
    }

    ProposalsStore.erase(itProposal);
}

/**
   Initialize a boot kick vote

   @param[in]     gguid Group the vote kicks belongs to
   @param[in]     kicker Kicker guid
   @param[in]     victim Victim guid
   @param[in]     reason Kick reason
*/
void LFGMgr::InitBoot(ObjectGuid gguid, ObjectGuid kicker, ObjectGuid victim, std::string const& reason)
{
    SetVoteKick(gguid, true);

    LfgPlayerBoot& boot = BootsStore[gguid];
    boot.inProgress = true;
    boot.cancelTime = time_t(GameTime::GetGameTime()) + LFG_TIME_BOOT;
    boot.reason = reason;
    boot.victim = victim;

    GuidSet const& players = GetPlayers(gguid);

    // Set votes
    for (GuidSet::const_iterator itr = players.begin(); itr != players.end(); ++itr)
    {
        ObjectGuid guid = (*itr);
        boot.votes[guid] = LFG_ANSWER_PENDING;
    }

    boot.votes[victim] = LFG_ANSWER_DENY;                  // Victim auto vote NO
    boot.votes[kicker] = LFG_ANSWER_AGREE;                 // Kicker auto vote YES

    // Notify players
    for (GuidSet::const_iterator it = players.begin(); it != players.end(); ++it)
        SendLfgBootProposalUpdate(*it, boot);
}

/**
   Update Boot info with player answer

   @param[in]     guid Player who has answered
   @param[in]     player answer
*/
void LFGMgr::UpdateBoot(ObjectGuid guid, bool accept)
{
    ObjectGuid gguid = GetGroup(guid);
    if (!gguid)
        return;

    LfgPlayerBootContainer::iterator itBoot = BootsStore.find(gguid);
    if (itBoot == BootsStore.end())
        return;

    LfgPlayerBoot& boot = itBoot->second;

    if (boot.votes[guid] != LFG_ANSWER_PENDING)    // Cheat check: Player can't vote twice
        return;

    boot.votes[guid] = LfgAnswer(accept);

    uint8 agreeNum = 0;
    uint8 denyNum = 0;
    for (LfgAnswerContainer::const_iterator itVotes = boot.votes.begin(); itVotes != boot.votes.end(); ++itVotes)
    {
        switch (itVotes->second)
        {
            case LFG_ANSWER_PENDING:
                break;
            case LFG_ANSWER_AGREE:
                ++agreeNum;
                break;
            case LFG_ANSWER_DENY:
                ++denyNum;
                break;
        }
    }

    // if we don't have enough votes (agree or deny) do nothing
    if (agreeNum < LFG_GROUP_KICK_VOTES_NEEDED && (boot.votes.size() - denyNum) >= LFG_GROUP_KICK_VOTES_NEEDED)
        return;

    // Send update info to all players
    boot.inProgress = false;
    for (LfgAnswerContainer::const_iterator itVotes = boot.votes.begin(); itVotes != boot.votes.end(); ++itVotes)
    {
        ObjectGuid pguid = itVotes->first;
        if (pguid != boot.victim)
            SendLfgBootProposalUpdate(pguid, boot);
    }

    SetVoteKick(gguid, false);
    if (agreeNum == LFG_GROUP_KICK_VOTES_NEEDED)           // Vote passed - Kick player
    {
        if (Group* group = sGroupMgr->GetGroupByGUID(gguid))
            Player::RemoveFromGroup(group, boot.victim, GROUP_REMOVEMETHOD_KICK_LFG);
        DecreaseKicksLeft(gguid);
    }
    BootsStore.erase(itBoot);
}

/**
   Teleports the player in or out the dungeon

   @param[in]     player Player to teleport
   @param[in]     out Teleport out (true) or in (false)
   @param[in]     fromOpcode Function called from opcode handlers? (Default false)
*/
void LFGMgr::TeleportPlayer(Player* player, bool out, bool fromOpcode /*= false*/)
{
    LFGDungeonData const* dungeon = nullptr;
    Group* group = player->GetGroup();

    if (group && group->isLFGGroup())
        dungeon = GetLFGDungeon(GetDungeon(group->GetGUID()));

    if (!dungeon)
    {
        TC_LOG_DEBUG("lfg.teleport", "Player {} not in group/lfggroup or dungeon not found!",
            player->GetName());
        player->GetSession()->SendLfgTeleportError(LFG_TELEPORT_RESULT_NO_RETURN_LOCATION);
        return;
    }

    if (out)
    {
        TC_LOG_DEBUG("lfg.teleport", "Player {} is being teleported out. Current Map {} - Expected Map {}",
            player->GetName(), player->GetMapId(), uint32(dungeon->map));
        if (player->GetMapId() == uint32(dungeon->map))
            player->TeleportToBGEntryPoint();

        return;
    }

    LfgTeleportResult error = LFG_TELEPORT_RESULT_NONE;

    if (!player->IsAlive())
        error = LFG_TELEPORT_RESULT_DEAD;
    else if (player->IsFalling() || player->HasUnitState(UNIT_STATE_JUMPING))
        error = LFG_TELEPORT_RESULT_FALLING;
    else if (player->IsMirrorTimerActive(FATIGUE_TIMER))
        error = LFG_TELEPORT_RESULT_EXHAUSTION;
    else if (player->GetVehicle())
        error = LFG_TELEPORT_RESULT_ON_TRANSPORT;
    else if (!player->GetCharmedGUID().IsEmpty())
        error = LFG_TELEPORT_RESULT_IMMUNE_TO_SUMMONS;
    else if (player->HasAura(9454)) // check Freeze debuff
        error = LFG_TELEPORT_RESULT_NO_RETURN_LOCATION;
    else if (player->GetMapId() != uint32(dungeon->map))  // Do not teleport players in dungeon to the entrance
    {
        uint32 mapid = dungeon->map;
        float x = dungeon->x;
        float y = dungeon->y;
        float z = dungeon->z;
        float orientation = dungeon->o;

        if (!fromOpcode)
        {
            // Select a player inside to be teleported to
            for (GroupReference const& itr : group->GetMembers())
            {
                Player* plrg = itr.GetSource();
                if (plrg != player && plrg->GetMapId() == uint32(dungeon->map))
                {
                    mapid = plrg->GetMapId();
                    x = plrg->GetPositionX();
                    y = plrg->GetPositionY();
                    z = plrg->GetPositionZ();
                    orientation = plrg->GetOrientation();
                    break;
                }
            }
        }

        if (!player->GetMap()->IsDungeon())
            player->SetBattlegroundEntryPoint();

        player->FinishTaxiFlight();

        if (!player->TeleportTo({ .Location = WorldLocation(mapid, x, y, z, orientation), .LfgDungeonsId = dungeon->id }))
            error = LFG_TELEPORT_RESULT_NO_RETURN_LOCATION;
    }
    else
        error = LFG_TELEPORT_RESULT_NO_RETURN_LOCATION;

    if (error != LFG_TELEPORT_RESULT_NONE)
        player->GetSession()->SendLfgTeleportError(error);

    TC_LOG_DEBUG("lfg.teleport", "Player {} is being teleported in to map {} "
        "(x: {}, y: {}, z: {}) Result: {}", player->GetName(), dungeon->map,
        dungeon->x, dungeon->y, dungeon->z, error);
}

/**
   Check if dungeon can be rewarded, if any.

   @param[in]     gguid Group guid
   @param[in]     dungeonEncounters DungeonEncounter that was just completed
   @param[in]     currMap Map of the instance where encounter was completed
*/
void LFGMgr::OnDungeonEncounterDone(ObjectGuid gguid, std::span<uint32 const> dungeonEncounters, Map const* currMap)
{
    if (GetState(gguid) == LFG_STATE_FINISHED_DUNGEON) // Shouldn't happen. Do not reward multiple times
    {
        TC_LOG_DEBUG("lfg.dungeon.finish", "Group: {} already rewarded", gguid.ToString());
        return;
    }

    uint32 gDungeonId = GetDungeon(gguid);
    LFGDungeonData const* dungeonDone = GetLFGDungeon(gDungeonId);
    // LFGDungeons can point to a DungeonEncounter from any difficulty so we need this kind of lenient check
    if (!advstd::ranges::contains(dungeonEncounters, dungeonDone->finalDungeonEncounterId))
        return;

    FinishDungeon(gguid, gDungeonId, currMap);
}

/**
   Finish a dungeon and give reward, if any.

   @param[in]     gguid Group guid
   @param[in]     dungeonId Dungeonid
*/
void LFGMgr::FinishDungeon(ObjectGuid gguid, const uint32 dungeonId, Map const* currMap)
{
    uint32 gDungeonId = GetDungeon(gguid);
    if (gDungeonId != dungeonId)
    {
        TC_LOG_DEBUG("lfg.dungeon.finish", "Group {} finished dungeon {} but queued for {}", gguid.ToString(), dungeonId, gDungeonId);
        return;
    }

    if (GetState(gguid) == LFG_STATE_FINISHED_DUNGEON) // Shouldn't happen. Do not reward multiple times
    {
        TC_LOG_DEBUG("lfg.dungeon.finish", "Group: {} already rewarded", gguid.ToString());
        return;
    }

    SetState(gguid, LFG_STATE_FINISHED_DUNGEON);

    GuidSet const& players = GetPlayers(gguid);
    for (GuidSet::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        ObjectGuid guid = (*it);
        if (GetState(guid) == LFG_STATE_FINISHED_DUNGEON)
        {
            TC_LOG_DEBUG("lfg.dungeon.finish", "Group: {}, Player: {} already rewarded", gguid.ToString(), guid.ToString());
            continue;
        }

        uint32 rDungeonId = 0;
        LfgDungeonSet const& dungeons = GetSelectedDungeons(guid);
        if (!dungeons.empty())
            rDungeonId = (*dungeons.begin());

        SetState(guid, LFG_STATE_FINISHED_DUNGEON);

        // Give rewards only if its a random dungeon
        LFGDungeonData const* dungeon = GetLFGDungeon(rDungeonId);

        if (!dungeon || (dungeon->type != LFG_TYPE_RANDOM && !dungeon->seasonal))
        {
            TC_LOG_DEBUG("lfg.dungeon.finish", "Group: {}, Player: {} dungeon {} is not random or seasonal", gguid.ToString(), guid.ToString(), rDungeonId);
            continue;
        }

        Player* player = ObjectAccessor::FindPlayer(guid);
        if (!player)
        {
            TC_LOG_DEBUG("lfg.dungeon.finish", "Group: {}, Player: {} not found in world", gguid.ToString(), guid.ToString());
            continue;
        }

        if (player->FindMap() != currMap)
        {
            TC_LOG_DEBUG("lfg.dungeon.finish", "Group: {}, Player: {} is in a different map", gguid.ToString(), guid.ToString());
            continue;
        }

        player->RemoveAurasDueToSpell(LFG_SPELL_DUNGEON_COOLDOWN);

        LFGDungeonData const* dungeonDone = GetLFGDungeon(dungeonId);
        uint32 mapId = dungeonDone ? uint32(dungeonDone->map) : 0;

        if (player->GetMapId() != mapId)
        {
            TC_LOG_DEBUG("lfg.dungeon.finish", "Group: {}, Player: {} is in map {} and should be in {} to get reward", gguid.ToString(), guid.ToString(), player->GetMapId(), mapId);
            continue;
        }

        player->UpdateCriteria(CriteriaType::CompletedLFGDungeon, 1);

        // Update achievements
        if (dungeon->difficulty == DIFFICULTY_HEROIC)
        {
            uint8 lfdRandomPlayers = 0;
            if (uint8 numParty = PlayersStore[guid].GetNumberOfPartyMembersAtJoin())
                lfdRandomPlayers = 5 - numParty;
            else
                lfdRandomPlayers = 4;
            player->UpdateCriteria(CriteriaType::CompletedLFGDungeonWithStrangers, lfdRandomPlayers);
        }

        LfgReward const* reward = GetRandomDungeonReward(rDungeonId, player->GetLevel());
        if (!reward)
            continue;

        bool done = false;
        Quest const* quest = sObjectMgr->GetQuestTemplate(reward->firstQuest);
        if (!quest)
            continue;

        // if we can take the quest, means that we haven't done this kind of "run", IE: First Heroic Random of Day.
        if (player->CanRewardQuest(quest, false))
            player->RewardQuest(quest, LootItemType::Item, 0, nullptr, false);
        else
        {
            done = true;
            quest = sObjectMgr->GetQuestTemplate(reward->otherQuest);
            if (!quest)
                continue;
            // we give reward without informing client (retail does this)
            player->RewardQuest(quest, LootItemType::Item, 0, nullptr, false);
        }

        // Give rewards
        TC_LOG_DEBUG("lfg.dungeon.finish", "Group: {}, Player: {} done dungeon {}, {} previously done.", gguid.ToString(), guid.ToString(), GetDungeon(gguid), done ? " " : " not");
        LfgPlayerRewardData data = LfgPlayerRewardData(dungeon->Entry(), GetDungeon(gguid, false), done, quest);
        player->GetSession()->SendLfgPlayerReward(data);
    }
}

// --------------------------------------------------------------------------//
// Auxiliar Functions
// --------------------------------------------------------------------------//

/**
   Get the dungeon list that can be done given a random dungeon entry.

   @param[in]     randomdungeon Random dungeon id (if value = 0 will return all dungeons)
   @returns Set of dungeons that can be done.
*/
LfgDungeonSet const& LFGMgr::GetDungeonsByRandom(uint32 randomdungeon)
{
    LFGDungeonData const* dungeon = GetLFGDungeon(randomdungeon);
    uint32 group = dungeon ? dungeon->group : 0;
    return CachedDungeonMapStore[group];
}

/**
   Get the reward of a given random dungeon at a certain level

   @param[in]     dungeon dungeon id
   @param[in]     level Player level
   @returns Reward
*/
LfgReward const* LFGMgr::GetRandomDungeonReward(uint32 dungeon, uint8 level)
{
    LfgReward const* rew = nullptr;
    LfgRewardContainerBounds bounds = RewardMapStore.equal_range(dungeon & 0x00FFFFFF);
    for (LfgRewardContainer::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        rew = itr->second;
        // ordered properly at loading
        if (itr->second->maxLevel >= level)
            break;
    }

    return rew;
}

/**
   Given a Dungeon id returns the dungeon Type

   @param[in]     dungeon dungeon id
   @returns Dungeon type
*/
LfgType LFGMgr::GetDungeonType(uint32 dungeonId)
{
    LFGDungeonData const* dungeon = GetLFGDungeon(dungeonId);
    if (!dungeon)
        return LFG_TYPE_NONE;

    return LfgType(dungeon->type);
}

LfgState LFGMgr::GetState(ObjectGuid guid)
{
    LfgState state;
    if (guid.IsParty())
    {
        state = GroupsStore[guid].GetState();
        TC_LOG_TRACE("lfg.data.group.state.get", "Group: {}, State: {}", guid.ToString(), GetStateString(state));
    }
    else
    {
        state = PlayersStore[guid].GetState();
        TC_LOG_TRACE("lfg.data.player.state.get", "Player: {}, State: {}", guid.ToString(), GetStateString(state));
    }

    return state;
}

LfgState LFGMgr::GetOldState(ObjectGuid guid)
{
    LfgState state;
    if (guid.IsParty())
    {
        state = GroupsStore[guid].GetOldState();
        TC_LOG_TRACE("lfg.data.group.oldstate.get", "Group: {}, Old state: {}", guid.ToString(), state);
    }
    else
    {
        state = PlayersStore[guid].GetOldState();
        TC_LOG_TRACE("lfg.data.player.oldstate.get", "Player: {}, Old state: {}", guid.ToString(), state);
    }

    return state;
}

bool LFGMgr::IsVoteKickActive(ObjectGuid gguid)
{
    ASSERT(gguid.IsParty());

    bool active = GroupsStore[gguid].IsVoteKickActive();
    TC_LOG_TRACE("lfg.data.group.votekick.get", "Group: {}, Active: {}", gguid.ToString(), active);

    return active;
}

uint32 LFGMgr::GetDungeon(ObjectGuid guid, bool asId /*= true */)
{
    uint32 dungeon = GroupsStore[guid].GetDungeon(asId);
    TC_LOG_TRACE("lfg.data.group.dungeon.get", "Group: {}, asId: {}, Dungeon: {}", guid.ToString(), asId, dungeon);
    return dungeon;
}

uint32 LFGMgr::GetDungeonMapId(ObjectGuid guid)
{
    uint32 dungeonId = GroupsStore[guid].GetDungeon(true);
    uint32 mapId = 0;
    if (dungeonId)
        if (LFGDungeonData const* dungeon = GetLFGDungeon(dungeonId))
            mapId = dungeon->map;

    TC_LOG_TRACE("lfg.data.group.dungeon.map", "Group: {}, MapId: {} (DungeonId: {})", guid.ToString(), mapId, dungeonId);

    return mapId;
}

uint8 LFGMgr::GetRoles(ObjectGuid guid)
{
    uint8 roles = PlayersStore[guid].GetRoles();
    TC_LOG_TRACE("lfg.data.player.role.get", "Player: {}, Role: {}", guid.ToString(), roles);
    return roles;
}

LfgDungeonSet const& LFGMgr::GetSelectedDungeons(ObjectGuid guid)
{
    TC_LOG_TRACE("lfg.data.player.dungeons.selected.get", "Player: {}, Selected Dungeons: {}", guid.ToString(), ConcatenateDungeons(PlayersStore[guid].GetSelectedDungeons()));
    return PlayersStore[guid].GetSelectedDungeons();
}

uint32 LFGMgr::GetSelectedRandomDungeon(ObjectGuid guid)
{
    if (GetState(guid) != LFG_STATE_NONE)
    {
        LfgDungeonSet const& dungeons = GetSelectedDungeons(guid);
        if (!dungeons.empty())
        {
            LFGDungeonData const* dungeon = GetLFGDungeon(*dungeons.begin());
            if (dungeon && dungeon->type == LFG_TYPE_RANDOM)
                return *dungeons.begin();
        }
    }

    return 0;
}

LfgLockMap LFGMgr::GetLockedDungeons(ObjectGuid guid)
{
    TC_LOG_TRACE("lfg.data.player.dungeons.locked.get", "Player: {}, LockedDungeons.", guid.ToString());
    LfgLockMap lock;
    Player* player = ObjectAccessor::FindConnectedPlayer(guid);
    if (!player)
    {
        TC_LOG_WARN("lfg.data.player.dungeons.locked.get", "Player: {} not ingame while retrieving his LockedDungeons.", guid.ToString());
        return lock;
    }

    uint8 level = player->GetLevel();
    uint8 expansion = player->GetSession()->GetExpansion();
    LfgDungeonSet const& dungeons = GetDungeonsByRandom(0);
    bool denyJoin = !player->GetSession()->HasPermission(rbac::RBAC_PERM_JOIN_DUNGEON_FINDER);

    for (LfgDungeonSet::const_iterator it = dungeons.begin(); it != dungeons.end(); ++it)
    {
        LFGDungeonData const* dungeon = GetLFGDungeon(*it);
        if (!dungeon) // should never happen - We provide a list from sLfgDungeonsStore
            continue;

        uint32 lockStatus = [&]() -> uint32
        {
            if (denyJoin)
                return LFG_LOCKSTATUS_RAID_LOCKED;
            if (dungeon->expansion > expansion)
                return LFG_LOCKSTATUS_INSUFFICIENT_EXPANSION;
            if (DisableMgr::IsDisabledFor(DISABLE_TYPE_MAP, dungeon->map, player))
                return LFG_LOCKSTATUS_NOT_IN_SEASON;
            if (DisableMgr::IsDisabledFor(DISABLE_TYPE_LFG_MAP, dungeon->map, player))
                return LFG_LOCKSTATUS_RAID_LOCKED;
            if (sInstanceLockMgr.FindActiveInstanceLock(guid, { dungeon->map, Difficulty(dungeon->difficulty) }))
                return LFG_LOCKSTATUS_RAID_LOCKED;
            if (Optional<ContentTuningLevels> levels = sDB2Manager.GetContentTuningData(dungeon->contentTuningId, player->m_playerData->CtrOptions->ConditionalFlags))
            {
                if (levels->MinLevel > level)
                    return LFG_LOCKSTATUS_TOO_LOW_LEVEL;
                if (levels->MaxLevel < level)
                    return LFG_LOCKSTATUS_TOO_HIGH_LEVEL;
            }
            if (dungeon->seasonal && !IsSeasonActive(dungeon->id))
                return LFG_LOCKSTATUS_NOT_IN_SEASON;
            if (dungeon->requiredItemLevel > player->GetAverageItemLevel())
                return LFG_LOCKSTATUS_TOO_LOW_GEAR_SCORE;
            if (AccessRequirement const* ar = sObjectMgr->GetAccessRequirement(dungeon->map, Difficulty(dungeon->difficulty)))
            {
                if (ar->achievement && !player->HasAchieved(ar->achievement))
                    return LFG_LOCKSTATUS_MISSING_ACHIEVEMENT;
                if (player->GetTeam() == ALLIANCE && ar->quest_A && !player->GetQuestRewardStatus(ar->quest_A))
                    return LFG_LOCKSTATUS_QUEST_NOT_COMPLETED;
                if (player->GetTeam() == HORDE && ar->quest_H && !player->GetQuestRewardStatus(ar->quest_H))
                    return LFG_LOCKSTATUS_QUEST_NOT_COMPLETED;

                if (ar->item)
                {
                    if (!player->HasItemCount(ar->item) && (!ar->item2 || !player->HasItemCount(ar->item2)))
                        return LFG_LOCKSTATUS_MISSING_ITEM;
                }
                else if (ar->item2 && !player->HasItemCount(ar->item2))
                    return LFG_LOCKSTATUS_MISSING_ITEM;
            }

            /* @todo VoA closed if WG is not under team control (LFG_LOCKSTATUS_RAID_LOCKED)
            lockData = LFG_LOCKSTATUS_TOO_HIGH_GEAR_SCORE;
            lockData = LFG_LOCKSTATUS_ATTUNEMENT_TOO_LOW_LEVEL;
            lockData = LFG_LOCKSTATUS_ATTUNEMENT_TOO_HIGH_LEVEL;
            */
            return 0;
        }();

        if (lockStatus)
            lock[dungeon->Entry()] = LfgLockInfoData(lockStatus, dungeon->requiredItemLevel, player->GetAverageItemLevel());
    }

    return lock;
}

uint8 LFGMgr::GetKicksLeft(ObjectGuid guid)
{
    uint8 kicks = GroupsStore[guid].GetKicksLeft();
    TC_LOG_TRACE("lfg.data.group.kickleft.get", "Group: {}, Kicks left: {}", guid.ToString(), kicks);
    return kicks;
}

void LFGMgr::RestoreState(ObjectGuid guid, char const* debugMsg)
{
    if (guid.IsParty())
    {
        LfgGroupData& data = GroupsStore[guid];
        TC_LOG_TRACE("lfg.data.group.state.restore", "Group: {} ({}), State: {}, Old state: {}",
            guid.ToString(), debugMsg, GetStateString(data.GetState()),
            GetStateString(data.GetOldState()));

        data.RestoreState();
    }
    else
    {
        LfgPlayerData& data = PlayersStore[guid];
        TC_LOG_TRACE("lfg.data.player.state.restore", "Player: {} ({}), State: {}, Old state: {}",
            guid.ToString(), debugMsg, GetStateString(data.GetState()),
            GetStateString(data.GetOldState()));

        data.RestoreState();
    }
}

void LFGMgr::SetState(ObjectGuid guid, LfgState state)
{
    if (guid.IsParty())
    {
        LfgGroupData& data = GroupsStore[guid];
        TC_LOG_TRACE("lfg.data.group.state.set", "Group: {}, New state: {}, Previous: {}, Old state: {}",
            guid.ToString(), GetStateString(state), GetStateString(data.GetState()),
            GetStateString(data.GetOldState()));

        data.SetState(state);
    }
    else
    {
        LfgPlayerData& data = PlayersStore[guid];
        TC_LOG_TRACE("lfg.data.player.state.set", "Player: {}, New state: {}, Previous: {}, OldState: {}",
            guid.ToString(), GetStateString(state), GetStateString(data.GetState()),
            GetStateString(data.GetOldState()));

        data.SetState(state);
    }
}

void LFGMgr::SetVoteKick(ObjectGuid gguid, bool active)
{
    ASSERT(gguid.IsParty());

    LfgGroupData& data = GroupsStore[gguid];
    TC_LOG_TRACE("lfg.data.group.votekick.set", "Group: {}, New state: {}, Previous: {}",
        gguid.ToString(), active, data.IsVoteKickActive());

    data.SetVoteKick(active);
}

void LFGMgr::SetDungeon(ObjectGuid guid, uint32 dungeon)
{
    TC_LOG_TRACE("lfg.data.group.dungeon.set", "Group: {}, Dungeon: {}", guid.ToString(), dungeon);
    GroupsStore[guid].SetDungeon(dungeon);
}

void LFGMgr::SetRoles(ObjectGuid guid, uint8 roles)
{
    TC_LOG_TRACE("lfg.data.player.role.set", "Player: {}, Roles: {}", guid.ToString(), roles);
    PlayersStore[guid].SetRoles(roles);
}

void LFGMgr::SetSelectedDungeons(ObjectGuid guid, LfgDungeonSet const& dungeons)
{
    TC_LOG_TRACE("lfg.data.player.dungeon.selected.set", "Player: {}, Dungeons: {}", guid.ToString(), ConcatenateDungeons(dungeons));
    PlayersStore[guid].SetSelectedDungeons(dungeons);
}

void LFGMgr::DecreaseKicksLeft(ObjectGuid guid)
{
    GroupsStore[guid].DecreaseKicksLeft();
    TC_LOG_TRACE("lfg.data.group.kicksleft.decrease", "Group: {}, Kicks: {}", guid.ToString(), GroupsStore[guid].GetKicksLeft());
}

void LFGMgr::SetTicket(ObjectGuid guid, WorldPackets::LFG::RideTicket const& ticket)
{
    PlayersStore[guid].SetTicket(ticket);
}

void LFGMgr::RemovePlayerData(ObjectGuid guid)
{
    TC_LOG_TRACE("lfg.data.player.remove", "Player: {}", guid.ToString());
    LfgPlayerDataContainer::iterator it = PlayersStore.find(guid);
    if (it != PlayersStore.end())
        PlayersStore.erase(it);
}

void LFGMgr::RemoveGroupData(ObjectGuid guid)
{
    TC_LOG_TRACE("lfg.data.group.remove", "Group: {}", guid.ToString());

    // A readiness check must never outlive its group: the dialog is only ever closed by an update, so
    // dropping the check silently would leave LFGReadyCheckPopup stuck on every member's screen.
    AbortReadyCheck(guid);

    LfgGroupDataContainer::iterator it = GroupsStore.find(guid);
    if (it == GroupsStore.end())
        return;

    LfgState state = GetState(guid);
    // If group is being formed after proposal success do nothing more
    GuidSet const& players = it->second.GetPlayers();
    for (ObjectGuid playerGUID : players)
    {
        SetGroup(playerGUID, ObjectGuid::Empty);
        if (state != LFG_STATE_PROPOSAL)
        {
            SetState(playerGUID, LFG_STATE_NONE);
            SendLfgUpdateStatus(playerGUID, LfgUpdateData(LFG_UPDATETYPE_REMOVED_FROM_QUEUE), true);
        }
    }
    GroupsStore.erase(it);
}

uint8 LFGMgr::GetTeam(ObjectGuid guid)
{
    uint8 team = PlayersStore[guid].GetTeam();
    TC_LOG_TRACE("lfg.data.player.team.get", "Player: {}, Team: {}", guid.ToString(), team);
    return team;
}

uint8 LFGMgr::FilterClassRoles(Player* player, uint8 roles)
{
    uint8 allowedRoles = PLAYER_ROLE_LEADER;
    for (uint32 i = 0; i < MAX_SPECIALIZATIONS; ++i)
        if (ChrSpecializationEntry const* specialization = sDB2Manager.GetChrSpecializationByIndex(player->GetClass(), i))
            allowedRoles |= 1 << (specialization->Role + 1);

    return roles & allowedRoles;
}

uint8 LFGMgr::RemovePlayerFromGroup(ObjectGuid gguid, ObjectGuid guid)
{
    return GroupsStore[gguid].RemovePlayer(guid);
}

void LFGMgr::AddPlayerToGroup(ObjectGuid gguid, ObjectGuid guid)
{
    GroupsStore[gguid].AddPlayer(guid);
}

void LFGMgr::SetLeader(ObjectGuid gguid, ObjectGuid leader)
{
    GroupsStore[gguid].SetLeader(leader);
}

void LFGMgr::SetTeam(ObjectGuid guid, uint8 team)
{
    if (sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GROUP))
        team = 0;

    PlayersStore[guid].SetTeam(team);
}

ObjectGuid LFGMgr::GetGroup(ObjectGuid guid)
{
    return PlayersStore[guid].GetGroup();
}

void LFGMgr::SetGroup(ObjectGuid guid, ObjectGuid group)
{
    PlayersStore[guid].SetGroup(group);
}

GuidSet const& LFGMgr::GetPlayers(ObjectGuid guid)
{
    return GroupsStore[guid].GetPlayers();
}

uint8 LFGMgr::GetPlayerCount(ObjectGuid guid)
{
    return GroupsStore[guid].GetPlayerCount();
}

ObjectGuid LFGMgr::GetLeader(ObjectGuid guid)
{
    return GroupsStore[guid].GetLeader();
}

bool LFGMgr::HasIgnore(ObjectGuid guid1, ObjectGuid guid2)
{
    Player* plr1 = ObjectAccessor::FindConnectedPlayer(guid1);
    Player* plr2 = ObjectAccessor::FindConnectedPlayer(guid2);
    return plr1 && plr2
        && (plr1->GetSocial()->HasIgnore(guid2, plr2->GetSession()->GetAccountGUID())
            || plr2->GetSocial()->HasIgnore(guid1, plr1->GetSession()->GetAccountGUID()));
}

/**
   SMSG_LFG_INSTANCE_SHUTDOWN_COUNTDOWN (0x5A0009) to every player in an LFG instance that is winding
   down while they are still inside it. (The opcode belongs to the dungeon finder, not to the premade
   group finder - there is no SMSG_LFG_LIST_ prefix on it; Opcodes.h is the authority.)

   The consumer (RVA 0x24C18C0) formats TimeLeft with INT_GENERAL_DURATION into the GlobalString
   INSTANCE_SHUTDOWN_MESSAGE and prints it to the system chat - no Lua event, no CVar, no UI state. So the
   whole effect is a chat line, and the unit of TimeLeft is seconds, proven by the duration formatting.

   The message carries the player's own LFG ride ticket. Only somebody who queued through the dungeon
   finder has one, which is also the filter: a player who walked into the same instance without queueing
   gets no ticket and therefore no message. That is not a shortcut, it is what the field means - the ticket
   names the queue entry the player rode in on.

   NO CALLER, and that is the honest state of this opcode - see the ACCEPTANCE note below.

   ACCEPTANCE, held open on purpose (audit 2026-08-27, lfg_5A round 4, finding 1, D2 gap on
   SMSG_LFG_INSTANCE_SHUTDOWN_COUNTDOWN): there is no shutdown in this tree that this message could
   truthfully announce, so it is built and byte-correct but has no trigger.
   Round 2/3 hung it on InstanceMap::Reset(InstanceResetMethod::Expire), HavePlayers() branch. That was
   wrong and is withdrawn: that branch does NOT shut the instance down. It ends in
   `return InstanceResetResult::NotEmpty` without a single line that unloads the map, its caller
   (InstanceMap::Update) only re-arms i_instanceExpireEvent, and the 60 s borrowed from the block below it
   are PendingRaidLock::TimeUntilLock - the binding window after which the player is bound (SetPendingBind)
   and the instance keeps running. Announcing a 60-second shutdown there tells the client something false.
   Every path in this tree that actually unloads a map is an empty-map path with nobody left to receive the
   message: InstanceMap::RemovePlayerFromMap arms m_unloadTimer only for the LAST player leaving
   (`!m_unloadTimer && m_mapRefManager.size() == 1`), InstanceMap::Reset's else branch runs only when
   !HavePlayers(), BattlegroundMap::SetUnload runs from the Battleground destructor, and
   MapManager::DestroyMap refuses outright while HavePlayers(). Map::RemoveAllPlayers is an emergency path
   that logs an error and teleports immediately - no countdown, and not a dungeon-finder concept.
   Do NOT close this gap with a substitute trigger. That was tried once in this unit (the arena trigger of
   round 2, withdrawn in round 3) and it damaged a working existing path. What retail winds down here is an
   LFG instance closing under the player - a mechanism TrinityCore does not have at all, not a message it
   is missing. Settling it needs the capture recorded in orchestrierung/status/lfg_5A.json under
   aufnahme_noetig, and the missing server mechanism is carried in familien.json under
   offene_serverarbeit (id lfg_instanz_abschaltung). The gap is recorded in dod_luecken.D2 and must not be
   dropped at acceptance.

   @param[in]     map Instance being shut down
   @param[in]     secondsRemaining Time left, in seconds
*/
void LFGMgr::SendInstanceShutdownCountdown(Map const* map, uint32 secondsRemaining) const
{
    if (!map)
        return;

    for (MapReference const& ref : map->GetPlayers())
    {
        Player* player = ref.GetSource();
        if (!player || !player->GetSession())
            continue;

        // The group's ticket if it queued as a group, otherwise the player's own.
        WorldPackets::LFG::RideTicket const* ticket = nullptr;
        if (Group const* group = player->GetGroup())
            ticket = GetTicket(group->GetGUID());
        if (!ticket)
            ticket = GetTicket(player->GetGUID());
        if (!ticket)
            continue;

        player->GetSession()->SendLfgInstanceShutdownCountdown(*ticket, secondsRemaining);
    }
}

void LFGMgr::SendLfgRoleChosen(ObjectGuid guid, ObjectGuid pguid, uint8 roles)
{
    if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
        player->GetSession()->SendLfgRoleChosen(pguid, roles);
}

void LFGMgr::SendLfgRoleCheckUpdate(ObjectGuid guid, LfgRoleCheck const& roleCheck)
{
    if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
        player->GetSession()->SendLfgRoleCheckUpdate(roleCheck);
}

void LFGMgr::SendLfgUpdateStatus(ObjectGuid guid, LfgUpdateData const& data, bool party)
{
    if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
        player->GetSession()->SendLfgUpdateStatus(data, party);
}

void LFGMgr::SendLfgJoinResult(ObjectGuid guid, LfgJoinResultData const& data)
{
    if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
        player->GetSession()->SendLfgJoinResult(data);
}

void LFGMgr::SendLfgBootProposalUpdate(ObjectGuid guid, LfgPlayerBoot const& boot)
{
    if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
        player->GetSession()->SendLfgBootProposalUpdate(boot);
}

void LFGMgr::SendLfgUpdateProposal(ObjectGuid guid, LfgProposal const& proposal)
{
    if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
        player->GetSession()->SendLfgUpdateProposal(proposal);
}

void LFGMgr::SendLfgQueueStatus(ObjectGuid guid, LfgQueueStatusData const& data)
{
    if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
        player->GetSession()->SendLfgQueueStatus(data);
}

bool LFGMgr::IsLfgGroup(ObjectGuid guid)
{
    return !guid.IsEmpty() && guid.IsParty() && GroupsStore[guid].IsLfgGroup();
}

uint8 LFGMgr::GetQueueId(ObjectGuid guid)
{
    if (guid.IsParty())
    {
        GuidSet const& players = GetPlayers(guid);
        ObjectGuid pguid = players.empty() ? ObjectGuid::Empty : (*players.begin());
        if (!pguid.IsEmpty())
            return GetTeam(pguid);
    }

    return GetTeam(guid);
}

LFGQueue& LFGMgr::GetQueue(ObjectGuid guid)
{
    uint8 queueId = GetQueueId(guid);
    return QueuesStore[queueId];
}

bool LFGMgr::AllQueued(GuidList const& check)
{
    if (check.empty())
        return false;

    for (GuidList::const_iterator it = check.begin(); it != check.end(); ++it)
    {
        LfgState state = GetState(*it);
        if (state != LFG_STATE_QUEUED)
        {
            if (state != LFG_STATE_PROPOSAL)
                TC_LOG_DEBUG("lfg.allqueued", "Unexpected state found while trying to form new group. Guid: {}, State: {}", (*it).ToString(), GetStateString(state));

            return false;
        }
    }
    return true;
}

time_t LFGMgr::GetQueueJoinTime(ObjectGuid guid)
{
    uint8 queueId = GetQueueId(guid);
    LfgQueueContainer::const_iterator itr = QueuesStore.find(queueId);
    if (itr != QueuesStore.end())
        return itr->second.GetJoinTime(guid);

    return 0;
}

// Only for debugging purposes
void LFGMgr::Clean()
{
    QueuesStore.clear();
}

bool LFGMgr::isOptionEnabled(uint32 option)
{
    return (m_options & option) != 0;
}

uint32 LFGMgr::GetOptions()
{
    return m_options;
}

void LFGMgr::SetOptions(uint32 options)
{
    m_options = options;
}

LfgUpdateData LFGMgr::GetLfgStatus(ObjectGuid guid)
{
    LfgPlayerData& playerData = PlayersStore[guid];
    return LfgUpdateData(LFG_UPDATETYPE_UPDATE_STATUS, playerData.GetState(), playerData.GetSelectedDungeons());
}

bool LFGMgr::IsSeasonActive(uint32 dungeonId)
{
    switch (dungeonId)
    {
        case 285: // The Headless Horseman
            return IsHolidayActive(HOLIDAY_HALLOWS_END);
        case 286: // The Frost Lord Ahune
            return IsHolidayActive(HOLIDAY_MIDSUMMER_FIRE_FESTIVAL);
        case 287: // Coren Direbrew
            return IsHolidayActive(HOLIDAY_BREWFEST);
        case 288: // The Crown Chemical Co.
            return IsHolidayActive(HOLIDAY_LOVE_IS_IN_THE_AIR);
    }
    return false;
}

std::string LFGMgr::DumpQueueInfo(bool full)
{
    uint32 size = uint32(QueuesStore.size());
    std::ostringstream o;

    o << "Number of Queues: " << size << "\n";
    for (LfgQueueContainer::const_iterator itr = QueuesStore.begin(); itr != QueuesStore.end(); ++itr)
    {
        std::string const& queued = itr->second.DumpQueueInfo();
        std::string const& compatibles = itr->second.DumpCompatibleInfo(full);
        o << queued << compatibles;
    }

    return o.str();
}

void LFGMgr::SetupGroupMember(ObjectGuid guid, ObjectGuid gguid)
{
    LfgDungeonSet dungeons;
    dungeons.insert(GetDungeon(gguid));
    SetSelectedDungeons(guid, dungeons);
    SetState(guid, GetState(gguid));
    SetGroup(guid, gguid);
    AddPlayerToGroup(gguid, guid);
}

bool LFGMgr::selectedRandomLfgDungeon(ObjectGuid guid)
{
    if (GetState(guid) != LFG_STATE_NONE)
    {
        LfgDungeonSet const& dungeons = GetSelectedDungeons(guid);
        if (!dungeons.empty())
        {
             LFGDungeonData const* dungeon = GetLFGDungeon(*dungeons.begin());
             if (dungeon && (dungeon->type == LFG_TYPE_RANDOM || dungeon->seasonal))
                 return true;
        }
    }

    return false;
}

bool LFGMgr::inLfgDungeonMap(ObjectGuid guid, uint32 map, Difficulty difficulty)
{
    if (!guid.IsParty())
        guid = GetGroup(guid);

    if (uint32 dungeonId = GetDungeon(guid, true))
        if (LFGDungeonData const* dungeon = GetLFGDungeon(dungeonId))
            if (uint32(dungeon->map) == map && dungeon->difficulty == difficulty)
                return true;

    return false;
}

uint32 LFGMgr::GetLFGDungeonEntry(uint32 id)
{
    if (id)
        if (LFGDungeonData const* dungeon = GetLFGDungeon(id))
            return dungeon->Entry();

    return 0;
}

LfgDungeonSet LFGMgr::GetRandomAndSeasonalDungeons(uint8 level, uint8 expansion, std::span<uint32 const> contentTuningReplacementConditionMask)
{
    LfgDungeonSet randomDungeons;
    for (lfg::LFGDungeonContainer::const_iterator itr = LfgDungeonStore.begin(); itr != LfgDungeonStore.end(); ++itr)
    {
        lfg::LFGDungeonData const& dungeon = itr->second;
        if (!(dungeon.type == lfg::LFG_TYPE_RANDOM || (dungeon.seasonal && sLFGMgr->IsSeasonActive(dungeon.id))))
            continue;

        if (dungeon.expansion > expansion)
            continue;

        if (Optional<ContentTuningLevels> levels = sDB2Manager.GetContentTuningData(dungeon.contentTuningId, contentTuningReplacementConditionMask))
            if (levels->MinLevel > level || level > levels->MaxLevel)
                continue;

        randomDungeons.insert(dungeon.Entry());
    }
    return randomDungeons;
}

} // namespace lfg
