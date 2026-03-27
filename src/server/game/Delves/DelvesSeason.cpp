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

#include "DelvesSeason.h"
#include "DB2Stores.h"
#include "DelveMgr.h"
#include "DelvesDefines.h"
#include "Log.h"
#include "Player.h"
#include "SpellAuras.h"
#include "SpellMgr.h"

namespace Delves
{

uint32 DelvesSeason::GetCurrentSeasonNumber()
{
    return sDelveMgr->GetActiveSeasonId();
}

uint32 DelvesSeason::GetFactionIdForSeason(uint32 seasonId)
{
    // DelvesSeason.Field_11_0_7_57361_000 is actually FactionID (from WoWDBDefs research)
    DelvesSeasonEntry const* entry = sDelvesSeasonStore.LookupEntry(seasonId);
    if (!entry)
        return 0;

    return static_cast<uint32>(entry->Field_11_0_7_57361_000);
}

std::vector<int32> DelvesSeason::GetSeasonSpellIds(uint32 seasonId)
{
    std::vector<int32> spellIds;

    DB2Manager::DelvesSeasonXSpellContainer const* spells = sDB2Manager.GetDelvesSeasonSpells(seasonId);
    if (!spells)
        return spellIds;

    for (DelvesSeasonXSpellEntry const* entry : *spells)
        spellIds.push_back(entry->SpellID);

    return spellIds;
}

void DelvesSeason::ApplySeasonSpells(Player* player)
{
    uint32 seasonId = GetCurrentSeasonNumber();
    if (seasonId == 0)
        return;

    std::vector<int32> spellIds = GetSeasonSpellIds(seasonId);
    for (int32 spellId : spellIds)
    {
        if (spellId > 0 && !player->HasAura(spellId))
        {
            player->CastSpell(player, spellId, true);
            TC_LOG_DEBUG("scripts.delves", "Applied season spell {} to {}", spellId, player->GetName());
        }
    }
}

void DelvesSeason::RemoveSeasonSpells(Player* player)
{
    uint32 seasonId = GetCurrentSeasonNumber();
    if (seasonId == 0)
        return;

    std::vector<int32> spellIds = GetSeasonSpellIds(seasonId);
    for (int32 spellId : spellIds)
    {
        if (spellId > 0)
            player->RemoveAura(spellId);
    }
}

bool DelvesSeason::MeetsMinimumLevelRequirement(Player* player)
{
    // ContentTuning ID 2677 defines the min level for delves
    // In retail, this resolves to level 70 (TWW) via the ContentTuning system
    // For now, check against a hardcoded minimum until ContentTuning evaluation is implemented
    ContentTuningEntry const* contentTuning = sContentTuningStore.LookupEntry(CONTENT_TUNING_DELVE_MIN_LEVEL);
    if (!contentTuning)
    {
        // Fallback: require max expansion level
        return player->GetLevel() >= 70;
    }

    return player->GetLevel() >= static_cast<uint32>(contentTuning->MinLevel);
}

} // namespace Delves
