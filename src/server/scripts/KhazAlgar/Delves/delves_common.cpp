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

    if (!_delveInstance)
        return;

    // First entrant locks the run's tier from their gossip selection.
    if (_delveInstance->GetState() == DelveState::Entering &&
        player->m_delveSelectedTier > 0 && player->m_delveSelectedTier <= MAX_DELVE_TIER)
    {
        _delveInstance->SetTier(player->m_delveSelectedTier);
    }

    _delveInstance->OnPlayerEnter(player);

    // Apply the tier scaling/affix aura. The TIER_SPELL_IDS auras are the
    // retail mechanism that drives mob HP/damage scaling and tier-specific
    // affixes inside the instance (verified via sniff 12.0.1.66527).
    uint8 tier = _delveInstance->GetTier();
    if (uint32 spellId = GetTierSpellId(tier))
    {
        // Refresh — strip any prior tier aura before applying the current one.
        for (uint32 sid : TIER_SPELL_IDS)
            if (sid && sid != spellId && player->HasAura(sid))
                player->RemoveAurasDueToSpell(sid);

        if (!player->HasAura(spellId))
            player->CastSpell(player, spellId, true);
    }

    // Re-send the WorldStates so the in-delve HUD reflects the active tier.
    player->SendUpdateWorldState(WS_DELVE_TIER, tier);
    player->SendUpdateWorldState(WS_DELVE_IN_DELVE_FLAG, 2);
    player->SendUpdateWorldState(WS_DELVE_MAP_ID, instance->GetId());
    player->SendUpdateWorldState(WS_DELVE_TIER_SPELL, GetTierSpellId(tier));
    if (DelveTemplate const* tmpl = _delveInstance->GetTemplate())
    {
        if (tmpl->WorldState26903)
            player->SendUpdateWorldState(WS_DELVE_UNKNOWN_26903, tmpl->WorldState26903);
        if (tmpl->LfgDungeonsId)
            player->SendUpdateWorldState(WS_DELVE_LFG_DUNGEONS_ID, tmpl->LfgDungeonsId);
    }
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
