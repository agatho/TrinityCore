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
#include "DelveMgr.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"

namespace Delves
{

DelveInstanceScript::DelveInstanceScript(InstanceMap* map, uint8 tier)
    : InstanceScript(map)
{
    DelveTemplate const* tmpl = sDelveMgr->GetDelveTemplate(map->GetId());
    _delveInstance = std::make_unique<DelveInstance>(map, tier, tmpl);
}

DelveInstanceScript::~DelveInstanceScript() = default;

void DelveInstanceScript::OnPlayerEnter(Player* player)
{
    InstanceScript::OnPlayerEnter(player);

    if (_delveInstance)
        _delveInstance->OnPlayerEnter(player);
}

void DelveInstanceScript::OnPlayerLeave(Player* player)
{
    if (_delveInstance)
        _delveInstance->OnPlayerExit(player);

    InstanceScript::OnPlayerLeave(player);
}

void DelveInstanceScript::Update(uint32 diff)
{
    InstanceScript::Update(diff);

    if (_delveInstance)
        _delveInstance->Update(diff);
}

void DelveInstanceScript::OnScenarioComplete()
{
    if (_delveInstance)
    {
        _delveInstance->OnScenarioComplete();

        if (_delveInstance->GetState() == DelveState::Completed)
            OnDelveComplete();
        else if (_delveInstance->GetState() == DelveState::Failed)
            OnDelveFailed();
    }
}

void DelveInstanceScript::OnPlayerDeath(Player* player)
{
    if (_delveInstance)
        _delveInstance->OnPlayerDeath(player);
}

} // namespace Delves
