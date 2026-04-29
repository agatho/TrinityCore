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

static char const* const AtalAmanScriptName = "instance_atal_aman_delve";

enum AtalAmanData
{
    BOSS_ATAL_AMAN_FINAL = 0,

    MAX_ENCOUNTER
};

class instance_atal_aman_delve : public InstanceMapScript
{
public:
    instance_atal_aman_delve() : InstanceMapScript(AtalAmanScriptName, 2962) { }

    struct instance_atal_aman_delve_InstanceScript : public Delves::DelveInstanceScript
    {
        instance_atal_aman_delve_InstanceScript(InstanceMap* map)
            : DelveInstanceScript(map, 1 /* tier resolved at OnPlayerEnter from m_delveSelectedTier */)
        {
            SetBossNumber(MAX_ENCOUNTER);
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_atal_aman_delve_InstanceScript(map);
    }
};

} // anonymous namespace

void AddSC_instance_atal_aman_delve()
{
    new instance_atal_aman_delve();
}
