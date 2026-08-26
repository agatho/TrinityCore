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

#ifndef __BATTLEGROUNDMGR_H
#define __BATTLEGROUNDMGR_H

#include "Common.h"
#include "DBCEnums.h"
#include "Battleground.h"
#include "BattlegroundQueue.h"
#include "UniqueTrackablePtr.h"
#include <array>
#include <unordered_map>

class Battleground;
struct BattlemasterListEntry;

typedef std::map<uint32, Trinity::unique_trackable_ptr<Battleground>> BattlegroundContainer;
typedef std::set<uint32> BattlegroundClientIdsContainer;

typedef std::unordered_map<uint32, BattlegroundTypeId> BattleMastersMap;

enum BattlegroundMisc
{
    BATTLEGROUND_OBJECTIVE_UPDATE_INTERVAL      = 1000
};

struct BattlegroundData
{
    BattlegroundContainer m_Battlegrounds;
    BattlegroundClientIdsContainer m_ClientBattlegroundIds[MAX_BATTLEGROUND_BRACKETS];
};

struct BattlegroundTemplate
{
    BattlegroundTypeId Id;
    WorldSafeLocsEntry const* StartLocation[PVP_TEAMS_COUNT] = { };
    float MaxStartDistSq;
    uint8 Weight;
    uint32 ScriptId;
    BattlemasterListEntry const* BattlemasterEntry;
    std::vector<int32> MapIDs;

    bool IsArena() const;
    uint16 GetMinPlayersPerTeam() const;
    uint16 GetMaxPlayersPerTeam() const;
    uint8 GetMinLevel() const;
    uint8 GetMaxLevel() const;
};

struct BattlegroundScriptTemplate
{
    int32 MapId;
    BattlegroundTypeId Id;
    uint32 ScriptId;
};

namespace WorldPackets
{
    namespace Battleground
    {
        struct BattlefieldStatusHeader;
        struct PvpRoleQueueCounts;
        class BattlefieldStatusNone;
        class BattlefieldStatusNeedConfirmation;
        class BattlefieldStatusActive;
        class BattlefieldStatusQueued;
        class BattlefieldStatusFailed;
        class BattlefieldStatusWaitForGroups;
        class BattlefieldStatusGroupProposalFailed;
    }
}

class TC_GAME_API BattlegroundMgr
{
    private:
        BattlegroundMgr();
        ~BattlegroundMgr();

    public:
        BattlegroundMgr(BattlegroundMgr const& right) = delete;
        BattlegroundMgr(BattlegroundMgr&& right) = delete;
        BattlegroundMgr& operator=(BattlegroundMgr const& right) = delete;
        BattlegroundMgr& operator=(BattlegroundMgr&& right) = delete;

        static BattlegroundMgr* instance();

        void Update(uint32 diff);

        /* Packet Building */
        void SendBattlegroundList(Player* player, ObjectGuid const& guid, BattlegroundTypeId bgTypeId);
        static void BuildBattlegroundStatusHeader(WorldPackets::Battleground::BattlefieldStatusHeader* header, Player const* player, uint32 ticketId, uint32 joinTime, BattlegroundQueueTypeId queueId);
        static void BuildBattlegroundStatusNone(WorldPackets::Battleground::BattlefieldStatusNone* battlefieldStatus, Player const* player, uint32 ticketId, uint32 joinTime);
        static void BuildBattlegroundStatusNeedConfirmation(WorldPackets::Battleground::BattlefieldStatusNeedConfirmation* battlefieldStatus, Battleground const* bg, Player const* player, uint32 ticketId, uint32 joinTime, uint32 timeout, BattlegroundQueueTypeId queueId,
            ChrSpecializationRole role);
        static void BuildBattlegroundStatusActive(WorldPackets::Battleground::BattlefieldStatusActive* battlefieldStatus, Battleground const* bg, Player const* player, uint32 ticketId, uint32 joinTime, BattlegroundQueueTypeId queueId);
        static void BuildBattlegroundStatusQueued(WorldPackets::Battleground::BattlefieldStatusQueued* battlefieldStatus, Player const* player, uint32 ticketId, uint32 joinTime, BattlegroundQueueTypeId queueId, uint32 avgWaitTime, bool asGroup);
        static void BuildBattlegroundStatusFailed(WorldPackets::Battleground::BattlefieldStatusFailed* battlefieldStatus, BattlegroundQueueTypeId queueId, Player const* player, uint32 ticketId, GroupJoinBattlegroundResult result, ObjectGuid const* errorGuid = nullptr);
        static void BuildBattlegroundStatusWaitForGroups(WorldPackets::Battleground::BattlefieldStatusWaitForGroups* battlefieldStatus, Player const* player, uint32 ticketId, uint32 joinTime, BattlegroundQueueTypeId queueId,
            uint32 mapId, uint32 timeout, std::array<uint8, 2> const& slotsPerSide, std::array<uint8, 2> const& awaitedPerSide, WorldPackets::Battleground::PvpRoleQueueCounts const& roles);
        static void BuildBattlegroundStatusGroupProposalFailed(WorldPackets::Battleground::BattlefieldStatusGroupProposalFailed* battlefieldStatus, Player const* player, uint32 ticketId, uint32 joinTime, BattlegroundQueueTypeId queueId,
            WorldPackets::Battleground::PvpRoleQueueCounts const& roles);

        // Everything CMSG_BATTLEFIELD_PORT does once an invite is accepted and all the cheat checks have
        // passed: leave the queue, drop out of any current battleground and teleport in. Split out of
        // WorldSession::HandleBattleFieldPortOpcode because a group proposal has to run it for every member
        // at once, not only for the session that happened to send the packet.
        static void PortPlayerToBattleground(Player* player, Battleground* bg, Team team, BattlegroundQueueTypeId queueId, uint32 ticketId);

        /* Battlegrounds */
        Battleground* GetBattleground(uint32 InstanceID, BattlegroundTypeId bgTypeId);
        Battleground* CreateNewBattleground(BattlegroundQueueTypeId queueId, BattlegroundBracketId bracketId);

        void AddBattleground(Battleground* bg);
        void AddToBGFreeSlotQueue(Battleground* bg);
        void RemoveFromBGFreeSlotQueue(uint32 mapId, uint32 instanceId);
        BGFreeSlotQueueContainer& GetBGFreeSlotQueueStore(uint32 mapId);

        void LoadBattlegroundTemplates();
        void DeleteAllBattlegrounds();

        static void SendToBattleground(Player* player, Battleground const* battleground);

        /* PvP Brawl */
        struct ActiveBrawl
        {
            uint32 PvpBrawlId = 0;              // PvpBrawl.db2 row id - what the client is told
            uint32 BattlemasterListId = 0;      // PvpBrawl.db2 BattlemasterListID - what the server queues
        };

        // The running brawl, or nothing. Returns nothing unless the configured brawl can genuinely produce a
        // match, so callers may treat a value as "advertising this is honest".
        Optional<ActiveBrawl> GetActiveBrawl();

        /* Battleground queues */
        static bool IsValidQueueId(BattlegroundQueueTypeId bgQueueTypeId);
        BattlegroundQueue& GetBattlegroundQueue(BattlegroundQueueTypeId bgQueueTypeId) { return m_BattlegroundQueues.emplace(bgQueueTypeId, bgQueueTypeId).first->second; }
        void ScheduleQueueUpdate(uint32 arenaMatchmakerRating, BattlegroundQueueTypeId bgQueueTypeId, BattlegroundBracketId bracket_id);
        uint32 GetPrematureFinishTime() const;

        // Return whether toggling was successful. In case of a non-existing battlemasterListId, or this battlemasterListId is not an arena, this would return false.
        bool ToggleArenaTesting(uint32 battlemasterListId);
        void ToggleTesting();

        bool isArenaTesting() const { return m_ArenaTesting != 0; }
        bool isTesting() const { return m_Testing; }

        static bool IsRandomBattleground(uint32 battlemasterListId);

        // True for a BattlemasterList that never runs itself. Its `battleground_template` row carries more than
        // one BattlemasterListXMap map, so CreateNewBattleground draws one of them and builds the instance from
        // THAT map's single-map template: the instance is filed in bgDataStore under the drawn type id, never
        // under the queue's own id. Every lookup that starts from a queue id therefore has to search all types.
        // Random Battleground (32) and Random Epic (901) are the two this tree already knew about by name; the
        // 12.1 aggregates Battleground Blitz (1101), Rated Battleground (100) and the multi-map brawls behave
        // exactly the same way, and so does All Arenas (6).
        bool IsAggregateBattleground(BattlegroundTypeId bgTypeId);
        static BattlegroundQueueTypeId BGQueueTypeId(uint16 battlemasterListId, BattlegroundQueueIdType type, bool rated, uint8 teamSize);

        static HolidayIds BGTypeToWeekendHolidayId(BattlegroundTypeId bgTypeId);
        static BattlegroundTypeId WeekendHolidayIdToBGType(HolidayIds holiday);
        static bool IsBGWeekend(BattlegroundTypeId bgTypeId);

        uint32 GetMaxRatingDifference() const;
        uint32 GetRatingDiscardTimer()  const;
        void LoadBattleMastersEntry();
        void CheckBattleMasters();
        BattlegroundTypeId GetBattleMasterBG(uint32 entry) const
        {
            BattleMastersMap::const_iterator itr = mBattleMastersMap.find(entry);
            if (itr != mBattleMastersMap.end())
                return itr->second;
            return BATTLEGROUND_TYPE_NONE;
        }

        BattlegroundTemplate const* GetBattlegroundTemplateByTypeId(BattlegroundTypeId id)
        {
            BattlegroundTemplateMap::const_iterator itr = _battlegroundTemplates.find(id);
            if (itr != _battlegroundTemplates.end())
                return &itr->second;
            return nullptr;
        }

        void LoadBattlegroundScriptTemplate();
        BattlegroundScriptTemplate const* FindBattlegroundScriptTemplate(uint32 mapId, BattlegroundTypeId bgTypeId) const;

        static void QueuePlayerForArena(Player const* player, uint8 teamSize, uint8 roles);

    private:
        uint32 CreateClientVisibleInstanceId(BattlegroundTypeId bgTypeId, BattlegroundBracketId bracket_id);
        static bool IsArenaType(BattlegroundTypeId bgTypeId);
        // queueBracket, when given, restricts the draw to maps whose own PVPDifficulty ladder holds a single
        // bracket that fully contains the queue bracket's level range. See the definition for why.
        BattlegroundTypeId GetRandomBG(BattlegroundTypeId id, PVPDifficultyEntry const* queueBracket = nullptr);

        typedef std::map<BattlegroundTypeId, BattlegroundData> BattlegroundDataContainer;
        BattlegroundDataContainer bgDataStore;

        std::map<BattlegroundQueueTypeId, BattlegroundQueue> m_BattlegroundQueues;
        std::map<uint32 /*mapId*/, BGFreeSlotQueueContainer> m_BGFreeSlotQueue;

        struct ScheduledQueueUpdate
        {
            uint32 ArenaMatchmakerRating;
            BattlegroundQueueTypeId QueueId;
            BattlegroundBracketId BracketId;

            bool operator==(ScheduledQueueUpdate const& right) const = default;
        };

        std::vector<ScheduledQueueUpdate> m_QueueUpdateScheduler;
        uint32 m_NextRatedArenaUpdate;
        uint32 m_UpdateTimer;
        uint32 m_ArenaTesting;
        bool   m_Testing;
        BattleMastersMap mBattleMastersMap;

        BattlegroundTemplate const* GetBattlegroundTemplateByMapId(uint32 mapId)
        {
            BattlegroundMapTemplateContainer::const_iterator itr = _battlegroundMapTemplates.find(mapId);
            if (itr != _battlegroundMapTemplates.end())
                return itr->second;
            return nullptr;
        }

        typedef std::map<BattlegroundTypeId, BattlegroundTemplate> BattlegroundTemplateMap;
        typedef std::map<uint32 /*mapId*/, BattlegroundTemplate*> BattlegroundMapTemplateContainer;
        BattlegroundTemplateMap _battlegroundTemplates;
        BattlegroundMapTemplateContainer _battlegroundMapTemplates;

        std::map<std::pair<int32, uint32>, BattlegroundScriptTemplate> _battlegroundScriptTemplates;
};

#define sBattlegroundMgr BattlegroundMgr::instance()

#endif // __BATTLEGROUNDMGR_H
