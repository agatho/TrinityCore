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

#include "OmniumFolioMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"

// The five weekly-unlock achievements that each source +1 "Mote of Omnial Inquiry"
// (TraitCurrency 4230) via DB2 TraitCurrencySource. Kept here for reference; the
// actual mote accrual is done live by TraitMgr from TraitCurrencySource, so this
// manager never touches currency directly.
namespace
{
    [[maybe_unused]] constexpr uint32 MoteAchievements[5] = { 62606, 62607, 62608, 62609, 62610 };
}

OmniumFolioMgr* OmniumFolioMgr::instance()
{
    static OmniumFolioMgr instance;
    return &instance;
}

void OmniumFolioMgr::LoadFromDB()
{
    _enabled    = false;
    _seasonId   = 0;
    _resetTimer = 0;

    // The folio's static definition data lives entirely in client DB2s
    // (TraitTree 1186 / TraitSystem 48 / TraitCurrency 4230) already loaded by
    // TraitMgr::Load(). This optional world table only carries the per-fork
    // seasonal schedule. It may be absent on the shared realm -> tolerate as
    // a no-op so the branch is realm-safe (mirrors the zone-events skeleton).
    QueryResult result = WorldDatabase.Query("SELECT SeasonId, Enabled FROM omnium_folio_season ORDER BY SeasonId DESC LIMIT 1");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Omnium Folio: no seasonal schedule row; system idle (definitions ride TraitMgr / DB2).");
        return;
    }

    Field* fields = result->Fetch();
    _seasonId = fields[0].GetUInt32();
    _enabled  = fields[1].GetBool();
    TC_LOG_INFO("server.loading", ">> Omnium Folio: season {} loaded (enabled={}).", _seasonId, uint32(_enabled));
}

void OmniumFolioMgr::Update(uint32 diff)
{
    if (!_enabled)
        return;

    _resetTimer += diff;
    // TODO(season-reset): on a weekly/season boundary, iterate online players and
    // call ResetForNewSeason(). Wired once the seasonal schedule table is seeded.
}

void OmniumFolioMgr::OnPlayerLogin(Player* player)
{
    if (!player || !_enabled)
        return;

    EnsureFolioForPlayer(player);
}

void OmniumFolioMgr::EnsureFolioForPlayer(Player* /*player*/)
{
    // POWER-GRANT SEAM.
    //
    // The folio's power is delivered by TrinityCore's generic-trait machinery:
    //   * Spell 1279717 has SPELL_EFFECT_CREATE_TRAIT_TREE_CONFIG (MiscValue = tree
    //     1186) -> Spell::EffectCreateTraitTreeConfig -> Player::CreateTraitConfig,
    //     which mints the generic TraitConfig for TraitSystem 48.
    //   * Node runes apply through Player::ApplyTraitConfig -> LearnSpell on each
    //     node's TraitDefinition.SpellID (already implemented).
    //   * Motes (TraitCurrency 4230) are computed live from TraitCurrencySource
    //     (achievements 62606..62610 + level-1 base) inside TraitMgr -- no store.
    //
    // CAPTURE-BLOCKED / content-gated: the unlock questline ("The Magisters' Call"
    // -> "The Omnium Reawakens", Magister Umbric / Grand Magister Rommath) and the
    // weekly achievement cadence are world-DB content not yet seeded on this branch.
    // Until it lands, this seam intentionally does nothing so we never cast on the
    // shared realm. Intended action once enabled + eligibility content exists:
    //
    //     if (playerHasCompletedUnlockQuest && !playerHasFolioConfig)
    //         player->CastSpell(player, UNLOCK_SPELL_ID, true);
}

void OmniumFolioMgr::ResetForNewSeason(Player* /*player*/)
{
    // TODO(season-reset): drop the player's generic folio config and re-mint it for
    // the new season (re-cast UNLOCK_SPELL_ID). No-op until the schedule is seeded.
}
