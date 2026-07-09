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

// Special boss delve - Azj-Kahet

#include "delves_common.h"
#include "ScriptMgr.h"

namespace
{

static char const* const ScriptName = "zekvirs_lair";

enum DataTypes
{
    BOSS_1 = 0,
    MAX_ENCOUNTER
};

class instance_zekvirs_lair : public InstanceMapScript
{
public:
    instance_zekvirs_lair() : InstanceMapScript(ScriptName, 2682) { }

    struct instance_zekvirs_lair_InstanceScript : public Delves::DelveInstanceScript
    {
        instance_zekvirs_lair_InstanceScript(InstanceMap* map)
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
        return new instance_zekvirs_lair_InstanceScript(map);
    }
};

} // anonymous namespace

void AddSC_instance_zekvirs_lair()
{
    new instance_zekvirs_lair();
}
