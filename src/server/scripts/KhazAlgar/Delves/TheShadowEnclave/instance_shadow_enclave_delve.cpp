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

#include "delves_common.h"
#include "ScriptMgr.h"

namespace
{

static char const* const ShadowEnclaveScriptName = "instance_shadow_enclave_delve";

enum ShadowEnclaveData
{
    BOSS_LORD_ANTENORIAN = 0,

    MAX_ENCOUNTER
};

// DungeonEncounter.db2 3368 "Antenorian", MapID 2952 - SMSG_ENCOUNTER_START 2493067 / SMSG_BOSS_KILL 3368 2611872
// (eversong 12.1.0.69497; deatholme 698731 / 742616). Binding it here is what makes SetBossState(DONE) send
// SMSG_BOSS_KILL and the encounter frames.
DungeonEncounterData const encounters[] =
{
    { BOSS_LORD_ANTENORIAN, {{ 3368 }} }
};

class instance_shadow_enclave_delve : public InstanceMapScript
{
public:
    instance_shadow_enclave_delve() : InstanceMapScript(ShadowEnclaveScriptName, 2952) { }

    struct instance_shadow_enclave_delve_InstanceScript : public Delves::DelveInstanceScript
    {
        instance_shadow_enclave_delve_InstanceScript(InstanceMap* map)
            : DelveInstanceScript(map, 1 /* tier resolved at OnPlayerEnter from m_delveSelectedTier */)
        {
            SetBossNumber(MAX_ENCOUNTER);
            LoadDungeonEncounterData(encounters);
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_shadow_enclave_delve_InstanceScript(map);
    }
};

} // anonymous namespace

void AddSC_instance_shadow_enclave_delve()
{
    new instance_shadow_enclave_delve();
}
