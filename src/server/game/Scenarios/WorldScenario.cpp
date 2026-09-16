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

#include "WorldScenario.h"
#include "DB2Stores.h"
#include "Map.h"
#include "Player.h"
#include "ScenarioMgr.h"
#include "ScenarioPackets.h"
#include "StringFormat.h"

WorldScenario::WorldScenario(Map* map, ScenarioData const* scenarioData, uint32 areaId) : Scenario(map, scenarioData),
    _areaId(areaId), _ended(false)
{
}

bool WorldScenario::IsInScenarioArea(WorldObject const* object) const
{
    if (!object->IsInWorld() || object->GetMap() != _map)
        return false;

    return DB2Manager::IsInArea(object->GetAreaId(), _areaId);
}

void WorldScenario::UpdatePlayerMembership(Player* player, uint32 playerAreaId)
{
    if (_ended)
        return;

    bool const inArea = player->IsInWorld() && player->GetMap() == _map && DB2Manager::IsInArea(playerAreaId, _areaId);
    bool const isMember = HasPlayer(player->GetGUID());
    if (inArea && !isMember)
        OnPlayerEnter(player);
    else if (!inArea && isMember)
        OnPlayerExit(player);
}

void WorldScenario::CompleteScenario()
{
    Scenario::CompleteScenario();

    // retail: the last step again, flagged complete and without spells, followed by the vacate in the same update
    ScenarioStepEntry const* lastStep = GetLastStep();
    DoForAllPlayers([&](Player const* receiver)
    {
        WorldPackets::Scenario::ScenarioState scenarioState;
        BuildScenarioStateFor(receiver, &scenarioState, StateSpells::None);
        if (lastStep)
            scenarioState.CurrentStep = lastStep->ID;
        receiver->SendDirectMessage(scenarioState.Write());

        SendBootPlayer(receiver, ScenarioVacateReason::Completed);
    });

    _players.clear();
    _ended = true;
}

void WorldScenario::Stop()
{
    if (_ended)
        return;

    DoForAllPlayers([&](Player const* receiver)
    {
        SendBootPlayer(receiver, ScenarioVacateReason::Left);
    });

    _players.clear();
    _ended = true;
}

std::string WorldScenario::GetOwnerInfo() const
{
    return Trinity::StringFormat("Map {} area {}", _map->GetId(), _areaId);
}
