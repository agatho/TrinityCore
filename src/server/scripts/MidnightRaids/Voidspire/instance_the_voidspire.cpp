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
#include "the_voidspire.h"

// DungeonEncounter.db2 ids @68887 (CONFIRMED). Modern DB2-driven encounter seam:
// sDungeonEncounterStore is queried by LoadDungeonEncounterData; no world-DB table needed.
static constexpr DungeonEncounterData const encounters[] =
{
    { DATA_IMPERATOR_AVERZIAN,    {{ 3176 }} },
    { DATA_VORASIUS,              {{ 3177 }} },
    { DATA_VAELGOR_EZZORAK,       {{ 3178 }} },
    { DATA_FALLEN_KING_SALHADAAR, {{ 3179 }} },
    { DATA_LIGHTBLINDED_VANGUARD, {{ 3180 }} },
    { DATA_CROWN_OF_THE_COSMOS,   {{ 3181 }} }
};

class instance_the_voidspire : public InstanceMapScript
{
public:
    instance_the_voidspire() : InstanceMapScript(TheVoidspireScriptName, 2912) { }

    struct instance_the_voidspire_InstanceMapScript : public InstanceScript
    {
        instance_the_voidspire_InstanceMapScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(VoidspireDataHeader);
            SetBossNumber(VoidspireEncounterCount);
            LoadDungeonEncounterData(encounters);
            // TODO(CAPTURE-BLOCKED): LoadObjectData(creatureData, gameObjectData) once boss/door
            // creature + gameobject entries are captured from a live The Voidspire run.
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_the_voidspire_InstanceMapScript(map);
    }
};

void AddSC_instance_the_voidspire()
{
    new instance_the_voidspire();
}
