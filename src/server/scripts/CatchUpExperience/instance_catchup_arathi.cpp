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

// Catch-Up Experience -- Arathi Highlands (server map 2927).
// Minimal InstanceMapScript scaffold for the map-2927 Catch-Up Experience,
// modeled directly on
// ExilesReach/DarkmaulCitadel/instance_darkmaul_citadel.cpp. Encounter
// tracking is intentionally empty for now -- this personal-phased leveling
// zone has no boss/door/encounter data authored yet. This CANNOT be compiled
// in the authoring environment -- compilation and realm testing are an
// explicit Phase-K step (see CATCHUP_BLIZZLIKE_IMPLEMENTATION_PLAN
// task-7-brief.md).

#include "ScriptMgr.h"
#include "InstanceScript.h"

// Map 2927 is a personally-phased instance-map, NOT a Scenario: a wire-level
// capture of the launch flow confirmed zero scenario objects. Phasing is
// driven by phase_area rows + conditions (real phase ids include 793), not
// by the Scenario/ScenarioStep/CriteriaTree system. An InstanceMapScript is
// still required here because a phased instance-map still binds an
// InstanceScript to the map instance; there is just no scenario step wiring
// to author. Once real boss/door/encounter data exists, bind it the same
// way instance_darkmaul_citadel.cpp does: a static ObjectData[] / DoorData[]
// / DungeonEncounterData[] table above this class, and
// LoadObjectData(...)/LoadDoorData(...)/LoadDungeonEncounterData(...) calls
// in the InstanceScript subclass constructor below.

class instance_catchup_arathi : public InstanceMapScript
{
public:
    instance_catchup_arathi() : InstanceMapScript("instance_catchup_arathi", 2927) { }

    struct instance_catchup_arathi_InstanceMapScript : public InstanceScript
    {
        instance_catchup_arathi_InstanceMapScript(InstanceMap* map) : InstanceScript(map)
        {
            // No scenario wiring: map 2927 is a phased instance-map, not a
            // Scenario -- see file header comment above.
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_catchup_arathi_InstanceMapScript(map);
    }
};

void AddSC_instance_catchup_arathi()
{
    new instance_catchup_arathi();
}
