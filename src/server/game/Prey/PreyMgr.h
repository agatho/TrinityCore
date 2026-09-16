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

#ifndef TRINITYCORE_PREY_MGR_H
#define TRINITYCORE_PREY_MGR_H

#include "Define.h"
#include <set>
#include <string>
#include <vector>

/*
    Prey (Midnight) - the weekly hunt rotation behind Astalor's Hunt Table.

    Measured from retail 12.1 captures (builds 69273 .. 69814) and the 12.1 client DB2:

    - The Hunt Table (creature 245824) offers one gossip option of type AdventureMap (GossipNPCOption 59105); the
      client opens the Midnight scouting map, asks CMSG_CHECK_IS_ADVENTURE_MAP_POI_VALID for every AdventureMapPOI and
      starts a hunt with CMSG_ADVENTURE_MAP_START_QUEST. Both handlers already exist in the core.
    - Every prey POI (quest sort QUEST_SORT_PREY) is gated by a PlayerCondition whose world state expression reads
      "world state W == slot". One world state belongs to one hunt target and is shared by the target's Normal, Hard
      and Nightmare quests; a POI exists per (target, slot). Slots 1..12 are four map positions times three
      difficulties, 13 and 14 two more Hard positions.
    - Once a week the realm sets those world states: 14 different targets fill slots 1..14 and 3 of the 4 special
      Nightmare targets (the targets that also have a slot-0 POI) fill 3 different Nightmare slots next to a regular
      target. Every other prey world state is 0. Six captured weeks all have exactly this shape; which target lands
      where follows no pattern across them, so the assignment is random.

    Everything is derived from DB2 at startup. The world states are realm-wide `world_state` rows persisted by
    WorldStateMgr, so a restart keeps the current week.
*/

struct PreyHuntTarget
{
    int32 WorldStateId = 0;
    std::set<int32> Slots;              // every slot one of this target's POIs can be shown in
    std::vector<uint32> QuestIds;       // the Normal / Hard / Nightmare quests sharing the world state
    std::string Name;                   // AdventureMapPOI title
    bool IsSpecial = false;             // has a slot-0 POI: joins a week only on a Nightmare slot
};

class TC_GAME_API PreyMgr
{
    PreyMgr();
    ~PreyMgr();

public:
    PreyMgr(PreyMgr const&) = delete;
    PreyMgr(PreyMgr&&) = delete;
    PreyMgr& operator=(PreyMgr const&) = delete;
    PreyMgr& operator=(PreyMgr&&) = delete;

    static PreyMgr* instance();

    // Builds the hunt targets from AdventureMapPOI, PlayerCondition, WorldStateExpression and the quest templates.
    // Requires quests and WorldStateMgr to be loaded; draws a rotation when the realm has none.
    void Initialize();

    // Weekly reset: a new rotation replaces the old one.
    void OnWeeklyReset();

    // Draws a new rotation and publishes it through the prey world states.
    void Rotate();

    std::vector<PreyHuntTarget> const& GetTargets() const { return _targets; }
    int32 GetSlot(PreyHuntTarget const& target) const;
    std::set<int32> const& GetRegularSlots() const { return _regularSlots; }
    std::set<int32> const& GetSpecialSlots() const { return _specialSlots; }

private:
    bool HasRotation() const;

    std::vector<PreyHuntTarget> _targets;
    std::set<int32> _regularSlots;
    std::set<int32> _specialSlots;
};

#define sPreyMgr PreyMgr::instance()

#endif // TRINITYCORE_PREY_MGR_H
