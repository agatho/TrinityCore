// StatPriority - per-(class, spec) stat weights for equip-upgrade scoring.
//
// The 12 compact StatIndex slots cover the core stats every modern (WoW
// 12.x) item carries. Profession-only stats (Inspiration / Resourcefulness
// / Finesse etc.) and resistances are intentionally not weighted —
// equip-upgrade scoring is a combat-gear decision; bots use vendor value
// as the floor for non-combat stats.
//
// Score formula (matches V1 EquipmentManager::CalculateItemScore):
//
//   score = item_level * 1.0
//         + Σ stat_value[i] * stat_weight[i]
//         + (weapon_dps_x10 * weapon_dps_weight)   // weapons only
//
// Weights are pulled from V1's curated table (well-tested over years of
// PvE questing). Unknown specs collapse to a class-fallback row so a
// fresh spec id (e.g., a future Augmentation tweak) still scores items
// sanely instead of treating every stat as 0-weight.

#pragma once

#include "Define.h"
#include <array>

namespace Playerbot {

// 12 compact stat slots. Order matters — both StatPriorityFor and the
// snapshot's ItemStatBlock index into the same array.
enum class StatIndex : uint8
{
    Strength    = 0,
    Agility     = 1,
    Stamina     = 2,
    Intellect   = 3,
    Spirit      = 4,    // legacy stat — vanilla/wrath gear; carry the slot for completeness
    Crit        = 5,
    Haste       = 6,
    Mastery     = 7,
    Versatility = 8,
    Leech       = 9,
    Avoidance   = 10,
    Speed       = 11,

    Count       = 12
};

// Map TC's ITEM_MOD_* (raw 0..82) onto our compact StatIndex. Returns
// StatIndex::Count for stats we don't weight (mana, health, defense
// rating from old expansions, profession stats, resistances). The score
// formula skips unweighted stats automatically.
StatIndex StatIndexForItemMod(uint32 item_mod_type);

// Compact per-spec stat weight row + weapon DPS weight. Float vector of
// 12 entries plus the weapon DPS coefficient. Resolved once at login
// per BotAI; cached on the snapshot so the equip-upgrade rule reads it
// without a table lookup per tick.
struct StatPriority
{
    std::array<float, static_cast<size_t>(StatIndex::Count)> weights{};
    // 0.0 for casters who never use weapon damage; >0 for melee/hunters.
    // Multiplied with weapon_dps_x10 (DPS rounded x10) — typical weight
    // is 8.0-12.0 so DPS dominates ilvl deltas for weapon swaps.
    float weapon_dps_weight = 0.0f;
};

// Resolve weights for a (class_id, spec_id) pair. spec_id is
// ChrSpecialization.db2 id (the same field SnapshotView::active_spec()
// surfaces). Unknown spec → falls back to a sensible class-default row.
// Unknown class (e.g., spec_id == 0 on a fresh bot pre-talent-apply) →
// returns a uniform 1.0-weight row so items still rank by ilvl + stats.
StatPriority const& StatPriorityFor(uint8 class_id, uint32 spec_id);

} // namespace Playerbot
