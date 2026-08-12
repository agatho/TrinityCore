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

#ifndef TRINITYCORE_OMNIUM_FOLIO_MGR_H
#define TRINITYCORE_OMNIUM_FOLIO_MGR_H

#include "Define.h"

class Player;

// ---------------------------------------------------------------------------
// Omnium Folio  --  Midnight (patch 12.0.7 "Revelations") Season 1 seasonal
// "rune ledger" power system.  Characters are measurably weaker without it.
//
// Evidence anchors (client build 12.0.7.68887, wago.tools DB2 + fork source):
//   * TraitTree 1186 / TraitSystem 48 (WidgetSetID 2200) = the folio rune tree.
//         5 nodes = 5 weekly rows; node 110273 is a no-choice (type 0) row.
//   * TraitCurrency 4230 ("Mote of Omnial Inquiry", Type 2 = TraitSourced) =
//         the ledger currency. It is COMPUTED, never stored: TraitMgr derives
//         the balance live from TraitCurrencySource rows 37084/37158..37162:
//         PlayerLevel-1 base + 1 mote per completed achievement 62606..62610
//         (the five weekly-unlock gates).
//   * Spell 1279717 carries SPELL_EFFECT_CREATE_TRAIT_TREE_CONFIG (303) with
//         MiscValue_0 = 1186, i.e. casting it mints the generic TraitConfig
//         (Spell::EffectCreateTraitTreeConfig -> Player::CreateTraitConfig).
//   * Rune node spells (DB2 TraitDefinition -> SpellName): 1279596 / 1279599
//         (core), 1279603-05 (defensive), 1287555 (lingering), 1279609 / 1279613
//         / 1287771 / 1287774 (stat), 1279614-16 (capstone).
//   * UI strings: GlobalStrings 58702 MIDNIGHT_LANDING_PAGE_TITLE / 58704
//         RUNES_OF_POWER / 58849 OMNIUM_FOLIO_UNLOCKED / 59538 unspent-points.
//
// Architecture: the folio rides TrinityCore's EXISTING generic-trait machinery
// (TraitMgr + Player trait config: create / apply / persist / wire are all
// already implemented for TraitConfigType::Generic).  This manager is therefore
// a thin coordinator / seam covering only what the stock trait system does not:
//   (1) per-character eligibility + config-ensure on login,
//   (2) the seasonal-reset hook (wipe & re-mint the generic config on rollover).
// The heavy lifting (power grant, persistence, opcodes) stays in TraitMgr.
//
// Realm-safety: LoadFromDB() tolerates an absent seasonal table (no-op), so this
// branch is safe against the shared realm exactly like the zone-events skeleton.
// ---------------------------------------------------------------------------
class TC_GAME_API OmniumFolioMgr
{
public:
    static OmniumFolioMgr* instance();

    // --- DB2 / client anchors (build 12.0.7.68887) ---
    static constexpr int32  TRAIT_TREE_ID    = 1186;
    static constexpr int32  TRAIT_SYSTEM_ID  = 48;
    static constexpr int32  CURRENCY_ID      = 4230;   // "Mote of Omnial Inquiry" (TraitSourced)
    static constexpr uint32 UNLOCK_SPELL_ID  = 1279717; // SPELL_EFFECT_CREATE_TRAIT_TREE_CONFIG, MiscValue=1186

    // Loads the (optional) seasonal schedule for this fork. Realm-safe: absent
    // table/row => system stays idle and definitions still ride TraitMgr.
    void LoadFromDB();

    // Seasonal-reset tick. No-op until a seasonal schedule row is present.
    void Update(uint32 diff);

    // Player load-path seam (called from Player::LoadFromDB after traits apply).
    void OnPlayerLogin(Player* player);

    // Power-grant seam. Delegates to the trait system: when the player is
    // eligible (unlock content completed) and has no folio config yet, casting
    // UNLOCK_SPELL_ID mints the generic config. Currently a documented no-op
    // because the unlock content is CAPTURE-BLOCKED / not yet seeded.
    void EnsureFolioForPlayer(Player* player);

    // Seasonal-reset hook (stub): drop & re-mint the generic folio config.
    void ResetForNewSeason(Player* player);

    uint32 GetCurrentSeasonId() const { return _seasonId; }
    bool IsEnabled() const { return _enabled; }

private:
    OmniumFolioMgr() = default;
    ~OmniumFolioMgr() = default;
    OmniumFolioMgr(OmniumFolioMgr const&) = delete;
    OmniumFolioMgr(OmniumFolioMgr&&) = delete;
    OmniumFolioMgr& operator=(OmniumFolioMgr const&) = delete;
    OmniumFolioMgr& operator=(OmniumFolioMgr&&) = delete;

    bool   _enabled    = false;
    uint32 _seasonId   = 0;
    uint32 _resetTimer = 0;
};

#define sOmniumFolioMgr OmniumFolioMgr::instance()

#endif // TRINITYCORE_OMNIUM_FOLIO_MGR_H
