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
#include "Duration.h"
#include "Position.h"
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

/*
    Prey (Midnight) - the weekly hunt rotation behind Astalor's Hunt Table and the hunts themselves.

    Measured from retail 12.1 captures (builds 69273 .. 69933) and the 12.1 client DB2:

    - The Hunt Table (creature 245824) offers one gossip option of type AdventureMap (GossipNPCOption 59105); the
      client opens the Midnight scouting map, asks CMSG_CHECK_IS_ADVENTURE_MAP_POI_VALID for every AdventureMapPOI and
      starts a hunt with CMSG_ADVENTURE_MAP_START_QUEST. Both handlers already exist in the core.
    - Every prey POI (quest sort QUEST_SORT_PREY) is gated by a PlayerCondition whose world state expression reads
      "world state W == slot". One world state belongs to one hunt target and is shared by the target's Normal, Hard
      and Nightmare quests; a POI exists per (target, slot). Slots 1..12 are the four Midnight zones times three
      difficulties (1/4/7/10 Normal, 2/5/8/11 Hard, 3/6/9/12 Nightmare), 13 and 14 two more Hard positions.
    - Once a week the realm sets those world states: 14 different targets fill slots 1..14 and 3 of the 4 special
      Nightmare targets (the targets that also have a slot-0 POI) fill 3 different Nightmare slots next to a regular
      target. Every other prey world state is 0. Seven captured weeks all have exactly this shape; which target lands
      where follows no pattern across them, so the assignment is random.
    - A hunt runs in the zone of the slot it was started from. The hunt quest carries one "Hunt your Prey" quest POI
      and one "<prey> slain" quest POI per zone, each gated by "PlayerDataElementCharacter 141 == zone index"; the
      slain POI marks where the prey waits once the hunt turns final.

    The rotation is derived from DB2 at startup and kept in realm-wide `world_state` rows persisted by WorldStateMgr, so
    a restart keeps the current week. The hunt data the client does not carry lives in `prey_hunt_*` world tables.
*/

enum class PreyHuntDifficulty : uint8
{
    None        = 0,
    Normal      = 1,
    Hard        = 2,
    Nightmare   = 3
};

struct PreyHuntZone
{
    uint8 ZoneIndex = 0;                // value of PlayerDataElementCharacter 141 while the hunt runs
    uint32 AreaId = 0;                  // the hunt counts as "in zone" inside this area and its children
    uint32 UiMapId = 0;                 // selects the zone's blobs in the hunt quest's POIs
    std::string Name;
};

enum class PreyHuntSpellRole : uint8
{
    Remnant     = 0,                    // cast by the Hunted Remnant (246605) during an ambush
    Prey        = 1,                    // cast by the prey itself in the final fight
};

struct PreyHuntSpell
{
    uint32 SpellId = 0;
    Milliseconds InitialMin = 0ms;
    Milliseconds InitialMax = 0ms;
    Milliseconds RepeatMin = 0ms;
    Milliseconds RepeatMax = 0ms;
};

struct PreyHuntTarget
{
    int32 WorldStateId = 0;
    std::set<int32> Slots;              // every slot one of this target's POIs can be shown in
    std::vector<uint32> QuestIds;       // the Normal / Hard / Nightmare quests sharing the world state
    std::string Name;                   // AdventureMapPOI title
    bool IsSpecial = false;             // has a slot-0 POI: joins a week only on a Nightmare slot

    uint8 TargetIndex = 0;              // value of PlayerDataElementCharacter 137 while the hunt runs
    uint32 PursuitSpellId = 0;          // summons the fleeing prey after an ambush was beaten
    std::vector<PreyHuntSpell> RemnantSpells;
    std::vector<PreyHuntSpell> PreySpells;
};

struct PreyHunt
{
    uint32 QuestId = 0;
    PreyHuntTarget const* Target = nullptr;
    PreyHuntDifficulty Difficulty = PreyHuntDifficulty::None;
    uint32 PreyEntry = 0;               // creature fought at the end of the hunt
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

    // Builds the hunt targets from AdventureMapPOI, PlayerCondition, WorldStateExpression and the quest templates and
    // loads the `prey_hunt_*` tables. Requires quests and WorldStateMgr to be loaded; draws a rotation when the realm
    // has none.
    void Initialize();

    // Weekly reset: a new rotation replaces the old one.
    void OnWeeklyReset();

    // Draws a new rotation and publishes it through the prey world states.
    void Rotate();

    std::vector<PreyHuntTarget> const& GetTargets() const { return _targets; }
    int32 GetSlot(PreyHuntTarget const& target) const;
    std::set<int32> const& GetRegularSlots() const { return _regularSlots; }
    std::set<int32> const& GetSpecialSlots() const { return _specialSlots; }

    PreyHunt const* GetHunt(uint32 questId) const;
    PreyHunt const* GetHuntByPreyEntry(uint32 creatureEntry) const;
    PreyHuntZone const* GetZone(uint8 zoneIndex) const;

    // The zone a hunt quest runs in when it is started now: the zone of its target's current slot.
    PreyHuntZone const* GetZoneForNewHunt(PreyHunt const& hunt) const;

    // Where the prey waits in the given zone: the hunt quest's "<prey> slain" POI for that zone.
    bool GetPreyLocation(PreyHunt const& hunt, PreyHuntZone const& zone, uint32& mapId, Position& position) const;

private:
    bool HasRotation() const;
    void LoadZones();
    void LoadSlots();
    void LoadTargets();
    void LoadHunts();
    void LoadSpells();

    std::vector<PreyHuntTarget> _targets;
    std::set<int32> _regularSlots;
    std::set<int32> _specialSlots;

    std::map<uint8, PreyHuntZone> _zones;
    std::map<int32, uint8> _slotZones;                          // slot -> zone index
    std::unordered_map<uint32, PreyHunt> _hunts;                // quest -> hunt
    std::unordered_map<uint32, PreyHunt const*> _huntsByPrey;   // prey creature -> hunt
};

#define sPreyMgr PreyMgr::instance()

#endif // TRINITYCORE_PREY_MGR_H
