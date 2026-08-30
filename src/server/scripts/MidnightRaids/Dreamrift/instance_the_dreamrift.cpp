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
#include "the_dreamrift.h"

// DungeonEncounter.db2 id @68887 (CONFIRMED). Modern DB2-driven encounter seam.
static constexpr DungeonEncounterData const encounters[] =
{
    { DATA_CHIMAERUS, {{ 3306 }} }
};

class instance_the_dreamrift : public InstanceMapScript
{
public:
    instance_the_dreamrift() : InstanceMapScript(TheDreamriftScriptName, 2939) { }

    struct instance_the_dreamrift_InstanceMapScript : public InstanceScript
    {
        instance_the_dreamrift_InstanceMapScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(DreamriftDataHeader);
            SetBossNumber(DreamriftEncounterCount);
            LoadDungeonEncounterData(encounters);
            // TODO(CAPTURE-BLOCKED): LoadObjectData(creatureData, gameObjectData) once boss/door
            // creature + gameobject entries are captured from a live The Dreamrift run.
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_the_dreamrift_InstanceMapScript(map);
    }
};

void AddSC_instance_the_dreamrift()
{
    new instance_the_dreamrift();
}
