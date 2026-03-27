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

// Order of Night - Hallowfall

#include "delves_common.h"
#include "InstanceMapScript.h"
#include "ScriptMgr.h"

namespace
{

static char const* const ScriptName = "nightfall_sanctum";

enum DataTypes
{
    BOSS_1 = 0,
    MAX_ENCOUNTER
};

class instance_nightfall_sanctum : public InstanceMapScript
{
public:
    instance_nightfall_sanctum() : InstanceMapScript(ScriptName, 2686) { }

    struct instance_nightfall_sanctum_InstanceScript : public Delves::DelveInstanceScript
    {
        instance_nightfall_sanctum_InstanceScript(InstanceMap* map)
            : DelveInstanceScript(map, 1 /* TODO: tier from context */)
        {
            SetBossNumber(MAX_ENCOUNTER);
        }

        void OnDelveStart() override { }
        void OnDelveComplete() override { }
        void OnDelveFailed() override { }

        void OnCreatureCreate(Creature* creature) override
        {
            Delves::DelveInstanceScript::OnCreatureCreate(creature);
        }

        void OnUnitDeath(Unit* unit) override
        {
            Delves::DelveInstanceScript::OnUnitDeath(unit);
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_nightfall_sanctum_InstanceScript(map);
    }
};

} // anonymous namespace

void AddSC_instance_nightfall_sanctum()
{
    new instance_nightfall_sanctum();
}
