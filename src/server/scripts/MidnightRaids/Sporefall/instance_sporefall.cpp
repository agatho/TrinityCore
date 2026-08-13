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
#include "sporefall.h"

// DungeonEncounter.db2 id @68887 (CONFIRMED). Modern DB2-driven encounter seam.
// NOTE: Map 1592 is the "DevMapE" dev-shell map -- world data may not load (see sporefall.h).
static constexpr DungeonEncounterData const encounters[] =
{
    { DATA_ROTMIRE, {{ 3159 }} }
};

class instance_sporefall : public InstanceMapScript
{
public:
    instance_sporefall() : InstanceMapScript(SporefallScriptName, 1592) { }

    struct instance_sporefall_InstanceMapScript : public InstanceScript
    {
        instance_sporefall_InstanceMapScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(SporefallDataHeader);
            SetBossNumber(SporefallEncounterCount);
            LoadDungeonEncounterData(encounters);
            // TODO(CAPTURE-BLOCKED): LoadObjectData(creatureData, gameObjectData) once boss/door
            // creature + gameobject entries are captured from a live Sporefall run (dev-shell map).
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_sporefall_InstanceMapScript(map);
    }
};

void AddSC_instance_sporefall()
{
    new instance_sporefall();
}
