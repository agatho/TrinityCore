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

#ifndef TRINITY_WORLDQUESTMGR_H
#define TRINITY_WORLDQUESTMGR_H

#include "Define.h"
#include <ctime>
#include <unordered_map>
#include <vector>

namespace WorldPackets::Quest
{
    struct WorldQuestUpdateInfo;
}

// A world quest definition, loaded from `world_quest_template`.
struct WorldQuestTemplate
{
    uint32 QuestID       = 0;
    uint32 Duration      = 0;   // seconds the quest stays active once activated
    int32  VariableID    = 0;   // WorldState variable reported to the client (0 = none)
    int32  Value         = 0;   // WorldState value
};

// A currently-active world quest instance.
struct ActiveWorldQuest
{
    uint32 QuestID   = 0;
    time_t StartTime = 0;                // when it became active (client's LastUpdate)
    time_t EndTime   = 0;                // StartTime + template Duration
    int32  VariableID = 0;
    int32  Value      = 0;
};

// Activates world quests from `world_quest_template`, tracks their expiry, refreshes them on a
// rotation, and feeds SMSG_WORLD_QUEST_UPDATE_RESPONSE. Data-driven: activates whatever the table
// contains (validated against quest_template). Empty table => no active world quests.
class TC_GAME_API WorldQuestMgr
{
    private:
        WorldQuestMgr();
        ~WorldQuestMgr();

    public:
        WorldQuestMgr(WorldQuestMgr const&) = delete;
        WorldQuestMgr(WorldQuestMgr&&) = delete;
        WorldQuestMgr& operator=(WorldQuestMgr const&) = delete;
        WorldQuestMgr& operator=(WorldQuestMgr&&) = delete;

        static WorldQuestMgr* instance();

        // Loads `world_quest_template` (must run after quest templates) and activates every valid entry.
        void LoadFromDB();

        // Periodic tick: re-activates world quests whose timer has expired (rotation).
        void Update(uint32 diff);

        // Appends every currently-active world quest to the SMSG_WORLD_QUEST_UPDATE_RESPONSE payload.
        void FillActiveWorldQuests(std::vector<WorldPackets::Quest::WorldQuestUpdateInfo>& updates) const;

        bool IsWorldQuestActive(uint32 questId) const { return _active.find(questId) != _active.end(); }

    private:
        void Activate(WorldQuestTemplate const& tmpl, time_t now);

        std::unordered_map<uint32, WorldQuestTemplate> _templates;   // questId -> template
        std::unordered_map<uint32, ActiveWorldQuest> _active;        // questId -> active instance
        uint32 _updateAccumulator = 0;                               // ms since last expiry sweep
};

#define sWorldQuestMgr WorldQuestMgr::instance()

#endif // TRINITY_WORLDQUESTMGR_H
