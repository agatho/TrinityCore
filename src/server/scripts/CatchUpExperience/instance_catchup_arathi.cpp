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

// Catch-Up Experience -- Arathi Highlands (server map 2796).
// Minimal InstanceMapScript scaffold for the map-2796 Catch-Up Experience,
// modeled directly on
// ExilesReach/DarkmaulCitadel/instance_darkmaul_citadel.cpp. Encounter
// tracking is intentionally empty for now -- this personal-phased leveling
// zone has no boss/door/encounter data authored yet. This CANNOT be compiled
// in the authoring environment -- compilation and realm testing are an
// explicit Phase-K step (see CATCHUP_BLIZZLIKE_IMPLEMENTATION_PLAN
// task-7-brief.md).

#include "ScriptMgr.h"
#include "InstanceScript.h"

// TODO Phase K: scenario step wiring. The `scenarios` table row for map 2796
// is currently a (2796, 0, 0) placeholder (content branch, Task 6) -- Arathi
// Catch-Up scenario progression is data-driven via the `scenarios` table +
// Scenario/ScenarioStep/CriteriaTree DB2 rows, NOT a scenario script class
// here. Once real DB2 rows exist, bind boss/door/encounter data the same way
// instance_darkmaul_citadel.cpp does: a static ObjectData[] / DoorData[] /
// DungeonEncounterData[] table above this class, and
// LoadObjectData(...)/LoadDoorData(...)/LoadDungeonEncounterData(...) calls
// in the InstanceScript subclass constructor below.

class instance_catchup_arathi : public InstanceMapScript
{
public:
    instance_catchup_arathi() : InstanceMapScript("instance_catchup_arathi", 2796) { }

    struct instance_catchup_arathi_InstanceMapScript : public InstanceScript
    {
        instance_catchup_arathi_InstanceMapScript(InstanceMap* map) : InstanceScript(map)
        {
            // TODO Phase K: scenario step wiring -- see file header comment above.
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
