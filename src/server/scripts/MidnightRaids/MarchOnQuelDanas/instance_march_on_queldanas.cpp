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
#include "march_on_queldanas.h"

// DungeonEncounter.db2 ids @68887 (CONFIRMED). Modern DB2-driven encounter seam.
static constexpr DungeonEncounterData const encounters[] =
{
    { DATA_BELOREN_CHILD_OF_ALAR, {{ 3182 }} },
    { DATA_MIDNIGHT_FALLS,        {{ 3183 }} }
};

class instance_march_on_queldanas : public InstanceMapScript
{
public:
    instance_march_on_queldanas() : InstanceMapScript(MarchOnQuelDanasScriptName, 2913) { }

    struct instance_march_on_queldanas_InstanceMapScript : public InstanceScript
    {
        instance_march_on_queldanas_InstanceMapScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(MarchOnQuelDanasDataHeader);
            SetBossNumber(MarchOnQuelDanasEncounterCount);
            LoadDungeonEncounterData(encounters);
            // TODO(CAPTURE-BLOCKED): LoadObjectData(creatureData, gameObjectData) once boss/door
            // creature + gameobject entries are captured from a live March on Quel'Danas run.
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_march_on_queldanas_InstanceMapScript(map);
    }
};

void AddSC_instance_march_on_queldanas()
{
    new instance_march_on_queldanas();
}
