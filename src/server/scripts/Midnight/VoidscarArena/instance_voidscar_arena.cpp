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

#include "InstanceScript.h"
#include "ScriptMgr.h"
#include "voidscar_arena.h"

// DungeonEncounter.db2 ids @68887 (CONFIRMED). This is the modern DB2-driven "instance_encounters"
// seam: sDungeonEncounterStore is queried by LoadDungeonEncounterData; no world-DB table is needed.
static constexpr DungeonEncounterData const encounters[] =
{
    { DATA_TAZRAH,   {{ 3285 }} },
    { DATA_ATROXUS,  {{ 3286 }} },
    { DATA_CHARONUS, {{ 3287 }} }
};

class instance_voidscar_arena : public InstanceMapScript
{
public:
    instance_voidscar_arena() : InstanceMapScript(VoidscarArenaScriptName, 2923) { }

    struct instance_voidscar_arena_InstanceMapScript : public InstanceScript
    {
        instance_voidscar_arena_InstanceMapScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(DataHeader);
            SetBossNumber(EncounterCount);
            LoadDungeonEncounterData(encounters);
            // TODO(CAPTURE-BLOCKED): LoadObjectData(creatureData, gameObjectData) once boss/door
            // creature + gameobject entries are captured from a live Voidscar Arena run.
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_voidscar_arena_InstanceMapScript(map);
    }
};

void AddSC_instance_voidscar_arena()
{
    new instance_voidscar_arena();
}
