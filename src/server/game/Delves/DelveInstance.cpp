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

#include "DelveInstance.h"
#include "DelvesCompanion.h"
#include "DelveMgr.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "UpdateFields.h"

namespace Delves
{

DelveInstance::DelveInstance(InstanceMap* map, uint8 tier, DelveTemplate const* tmpl)
    : _map(map)
    , _template(tmpl)
    , _tier(tier)
    , _remainingRevives(GetMaxRevivesForTier(tier))
    , _state(DelveState::Entering)
    , _bountiful(tmpl ? sDelveMgr->IsDelveCurrentlyBountiful(tmpl->MapChallengeModeId) : false)
{
    TC_LOG_DEBUG("scripts.delves", "DelveInstance created for map {} tier {} (bountiful: {})",
        map->GetId(), tier, _bountiful);
}

DelveInstance::~DelveInstance() = default;

void DelveInstance::OnPlayerEnter(Player* player)
{
    if (_state == DelveState::Entering || _state == DelveState::InProgress)
    {
        _owners.insert(player->GetGUID());
        PopulateDelveData(player);

        // Spawn companion for the first player if group size <= MAX_COMPANION_GROUP_SIZE (4)
        if (_state == DelveState::Entering)
        {
            _state = DelveState::InProgress;

            uint32 groupSize = 1;
            if (Group const* group = player->GetGroup())
                groupSize = group->GetMembersCount();

            if (groupSize <= MAX_COMPANION_GROUP_SIZE && _template)
            {
                CompanionState companionState;
                DelvesCompanion::LoadFromDB(player->GetBattlenetAccountId(), companionState);

                Position spawnPos(_template->CompanionSpawnX, _template->CompanionSpawnY,
                    _template->CompanionSpawnZ, _template->CompanionSpawnO);

                if (Creature* companion = DelvesCompanion::SpawnCompanion(_map, player, companionState, spawnPos))
                {
                    _companionGuid = companion->GetGUID();
                    companion->AI()->SetData(0, static_cast<uint32>(companionState.SelectedRole));
                }
            }
        }

        TC_LOG_DEBUG("scripts.delves", "Player {} entered delve (map {}, tier {}, revives: {}, owners: {})",
            player->GetName(), _map->GetId(), _tier, _remainingRevives, _owners.size());
    }
}

void DelveInstance::OnPlayerExit(Player* player)
{
    ClearDelveData(player);

    TC_LOG_DEBUG("scripts.delves", "Player {} exited delve (map {})",
        player->GetName(), _map->GetId());
}

void DelveInstance::OnPlayerDeath(Player* player)
{
    if (_remainingRevives == DELVE_REVIVES_UNLIMITED)
    {
        TC_LOG_DEBUG("scripts.delves", "Player {} died in delve (tier {}, unlimited revives)",
            player->GetName(), _tier);
        return;
    }

    if (_remainingRevives > 0)
        --_remainingRevives;

    TC_LOG_DEBUG("scripts.delves", "Player {} died in delve (tier {}, revives remaining: {})",
        player->GetName(), _tier, _remainingRevives);

    if (_remainingRevives == 0)
    {
        _state = DelveState::Failed;
        TC_LOG_DEBUG("scripts.delves", "Delve failed - no revives remaining (map {}, tier {})",
            _map->GetId(), _tier);
    }
}

void DelveInstance::OnScenarioComplete()
{
    if (_state == DelveState::Failed)
    {
        TC_LOG_DEBUG("scripts.delves", "Delve scenario completed but marked as failed (no revives) - map {}, tier {}",
            _map->GetId(), _tier);
        // Player can still complete but gets reduced rewards
    }
    else
    {
        _state = DelveState::Completed;
        TC_LOG_DEBUG("scripts.delves", "Delve completed successfully - map {}, tier {}, revives remaining: {}",
            _map->GetId(), _tier, _remainingRevives);
    }

    // TODO Phase 4: Award rewards via DelvesRewards
    // TODO Phase 3: Award companion XP
}

void DelveInstance::Update(uint32 /*diff*/)
{
    // TODO: Check for timeout, periodic updates
}

void DelveInstance::PopulateDelveData(Player* player)
{
    // Set the DelveData optional update field on the player's ActivePlayerData
    auto& delveData = player->m_values.ModifyValue(&Player::m_activePlayerData, 0)
        .ModifyValue(&UF::ActivePlayerData::DelveData, 0);

    // Set SpellID (season spell if available)
    DelvesSeasonEntry const* season = sDelveMgr->GetActiveSeason();
    if (season)
    {
        if (DB2Manager::DelvesSeasonXSpellContainer const* spells = sDB2Manager.GetDelvesSeasonSpells(season->ID))
        {
            if (!spells->empty())
                delveData.ModifyValue(&UF::DelveData::SpellID) = (*spells)[0]->SpellID;
        }
    }

    // Add this player as an owner
    // Note: the Owners vector in DelveData tracks who started the delve for reward eligibility
    // The Started flag restricts rewards to only these players

    player->SetUpdateFieldValue(
        player->m_values.ModifyValue(&Player::m_activePlayerData, 0)
            .ModifyValue(&UF::ActivePlayerData::DelveData, 0)
            .ModifyValue(&UF::DelveData::Started),
        1u);
}

void DelveInstance::ClearDelveData(Player* player)
{
    player->RemoveOptionalUpdateFieldValue(
        player->m_values.ModifyValue(&Player::m_activePlayerData)
            .ModifyValue(&UF::ActivePlayerData::DelveData));
}

} // namespace Delves
