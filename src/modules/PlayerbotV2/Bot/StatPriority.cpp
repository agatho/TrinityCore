#include "StatPriority.h"

#include "ItemTemplate.h"   // ITEM_MOD_*

#include <unordered_map>

namespace Playerbot {

namespace {

// Helper: build a row inline. The verbose syntax is intentional — each
// (class, spec) row is independently auditable, no clever DSL hiding the
// weights. Caster casters score by ilvl × intellect dominance; physical
// DPS by main-stat × secondary mix; tanks bias toward Stamina + survival.
//
// All weights cap at 1.0 (item level) and float down to ~0.5 at the
// least-useful secondary. The exact shape comes from V1
// EquipmentManager::InitializeStatPriorities — kept verbatim so the
// upgrade decision matches V1's behavior in the overlap content.

constexpr float W_ILVL_PRIMARY   = 0.95f;
constexpr float W_TANK_STAMINA   = 1.00f;

StatPriority MakeRow(float str, float agi, float sta, float intellect, float spi,
                     float crit, float haste, float mastery, float versa,
                     float leech, float avoidance, float speed,
                     float weapon_dps)
{
    StatPriority p{};
    p.weights[static_cast<size_t>(StatIndex::Strength)]    = str;
    p.weights[static_cast<size_t>(StatIndex::Agility)]     = agi;
    p.weights[static_cast<size_t>(StatIndex::Stamina)]     = sta;
    p.weights[static_cast<size_t>(StatIndex::Intellect)]   = intellect;
    p.weights[static_cast<size_t>(StatIndex::Spirit)]      = spi;
    p.weights[static_cast<size_t>(StatIndex::Crit)]        = crit;
    p.weights[static_cast<size_t>(StatIndex::Haste)]       = haste;
    p.weights[static_cast<size_t>(StatIndex::Mastery)]     = mastery;
    p.weights[static_cast<size_t>(StatIndex::Versatility)] = versa;
    p.weights[static_cast<size_t>(StatIndex::Leech)]       = leech;
    p.weights[static_cast<size_t>(StatIndex::Avoidance)]   = avoidance;
    p.weights[static_cast<size_t>(StatIndex::Speed)]       = speed;
    p.weapon_dps_weight = weapon_dps;
    return p;
}

// Default rows for fully-uniform fallback. Used when class_id / spec_id
// don't match any curated entry. 1.0 across everything → items rank by
// raw stat sum, which is "less wrong" than 0-weight everywhere.
StatPriority const& UniformRow()
{
    static const StatPriority row = MakeRow(
        /*str*/   1.0f,  /*agi*/   1.0f,  /*sta*/  1.0f,  /*int*/  1.0f,  /*spi*/  0.5f,
        /*crit*/  0.6f,  /*haste*/ 0.6f,  /*mast*/ 0.6f,  /*vers*/ 0.6f,
        /*leech*/ 0.3f,  /*avoid*/ 0.3f,  /*speed*/ 0.3f,
        /*dps*/   8.0f);
    return row;
}

// Built once on first call; the map is read-only after init.
std::unordered_map<uint32, StatPriority> const& SpecTable()
{
    static const std::unordered_map<uint32, StatPriority> table = {
        // ===== Warrior =====
        // Arms (71): Strength + ilvl, Crit primary secondary, Haste + Mast + Versa lower.
        { 71,  MakeRow(W_ILVL_PRIMARY, 0.0f, 0.85f, 0.0f, 0.0f, 0.75f, 0.70f, 0.65f, 0.60f, 0.30f, 0.30f, 0.30f, 12.0f) },
        // Fury (72): Haste-driven DPS. DPS weight elevated for dual-wield 2H Titan's Grip.
        { 72,  MakeRow(W_ILVL_PRIMARY, 0.0f, 0.85f, 0.0f, 0.0f, 0.65f, 0.85f, 0.70f, 0.60f, 0.30f, 0.30f, 0.30f, 12.0f) },
        // Protection (73): Stamina top, Versa for damage-take, Mastery for block, Strength for threat.
        { 73,  MakeRow(0.85f, 0.0f, W_TANK_STAMINA, 0.0f, 0.0f, 0.55f, 0.65f, 0.75f, 0.80f, 0.55f, 0.55f, 0.35f, 8.0f) },

        // ===== Paladin =====
        // Holy (65): Int caster healer; Crit + Haste + Mastery balanced.
        { 65,  MakeRow(0.0f, 0.0f, 0.85f, W_ILVL_PRIMARY, 0.55f, 0.75f, 0.80f, 0.80f, 0.60f, 0.40f, 0.30f, 0.30f, 4.0f) },
        // Protection (66): Plate tank. Stamina top.
        { 66,  MakeRow(0.85f, 0.0f, W_TANK_STAMINA, 0.0f, 0.0f, 0.55f, 0.65f, 0.75f, 0.80f, 0.55f, 0.55f, 0.35f, 8.0f) },
        // Retribution (70): Strength DPS. Haste + Mastery primary secondaries.
        { 70,  MakeRow(W_ILVL_PRIMARY, 0.0f, 0.85f, 0.0f, 0.0f, 0.70f, 0.80f, 0.75f, 0.65f, 0.30f, 0.30f, 0.30f, 12.0f) },

        // ===== Hunter =====
        // Beast Mastery (253): Agility + Crit/Haste/Mast/Vers spread.
        { 253, MakeRow(0.0f, W_ILVL_PRIMARY, 0.85f, 0.0f, 0.0f, 0.75f, 0.75f, 0.70f, 0.60f, 0.30f, 0.30f, 0.30f, 11.0f) },
        // Marksmanship (254): Crit-favored ranged DPS.
        { 254, MakeRow(0.0f, W_ILVL_PRIMARY, 0.85f, 0.0f, 0.0f, 0.85f, 0.75f, 0.70f, 0.60f, 0.30f, 0.30f, 0.30f, 11.0f) },
        // Survival (255): Melee Hunter, weapon-DPS for 2H spear.
        { 255, MakeRow(0.0f, W_ILVL_PRIMARY, 0.85f, 0.0f, 0.0f, 0.70f, 0.80f, 0.75f, 0.65f, 0.30f, 0.30f, 0.30f, 12.0f) },

        // ===== Rogue =====
        // Assassination (259), Outlaw (260), Subtlety (261): all Agility, weapon DPS critical.
        { 259, MakeRow(0.0f, W_ILVL_PRIMARY, 0.85f, 0.0f, 0.0f, 0.75f, 0.75f, 0.85f, 0.65f, 0.30f, 0.30f, 0.30f, 12.0f) },
        { 260, MakeRow(0.0f, W_ILVL_PRIMARY, 0.85f, 0.0f, 0.0f, 0.80f, 0.75f, 0.65f, 0.70f, 0.30f, 0.30f, 0.30f, 12.0f) },
        { 261, MakeRow(0.0f, W_ILVL_PRIMARY, 0.85f, 0.0f, 0.0f, 0.85f, 0.70f, 0.75f, 0.65f, 0.30f, 0.30f, 0.30f, 12.0f) },

        // ===== Priest =====
        // Discipline (256): Int healer; Haste primary, Crit secondary.
        { 256, MakeRow(0.0f, 0.0f, 0.85f, W_ILVL_PRIMARY, 0.55f, 0.70f, 0.85f, 0.70f, 0.65f, 0.40f, 0.30f, 0.30f, 4.0f) },
        // Holy (257): Mastery-favored healer.
        { 257, MakeRow(0.0f, 0.0f, 0.85f, W_ILVL_PRIMARY, 0.55f, 0.70f, 0.75f, 0.85f, 0.65f, 0.40f, 0.30f, 0.30f, 4.0f) },
        // Shadow (258): Caster DPS; Haste + Crit.
        { 258, MakeRow(0.0f, 0.0f, 0.80f, W_ILVL_PRIMARY, 0.0f, 0.75f, 0.85f, 0.75f, 0.65f, 0.30f, 0.30f, 0.30f, 4.0f) },

        // ===== Death Knight =====
        // Blood (250): Strength tank, Stamina top.
        { 250, MakeRow(0.85f, 0.0f, W_TANK_STAMINA, 0.0f, 0.0f, 0.65f, 0.70f, 0.75f, 0.70f, 0.55f, 0.55f, 0.35f, 10.0f) },
        // Frost (251): Strength dual-wield/2H DPS.
        { 251, MakeRow(W_ILVL_PRIMARY, 0.0f, 0.85f, 0.0f, 0.0f, 0.75f, 0.75f, 0.70f, 0.65f, 0.30f, 0.30f, 0.30f, 12.0f) },
        // Unholy (252): Strength DPS, Haste primary, Mastery for ghoul.
        { 252, MakeRow(W_ILVL_PRIMARY, 0.0f, 0.85f, 0.0f, 0.0f, 0.65f, 0.85f, 0.75f, 0.65f, 0.30f, 0.30f, 0.30f, 12.0f) },

        // ===== Shaman =====
        // Elemental (262): Int caster.
        { 262, MakeRow(0.0f, 0.0f, 0.80f, W_ILVL_PRIMARY, 0.0f, 0.75f, 0.85f, 0.75f, 0.65f, 0.30f, 0.30f, 0.30f, 4.0f) },
        // Enhancement (263): Agi melee dual-wield.
        { 263, MakeRow(0.0f, W_ILVL_PRIMARY, 0.85f, 0.0f, 0.0f, 0.75f, 0.85f, 0.75f, 0.65f, 0.30f, 0.30f, 0.30f, 12.0f) },
        // Restoration (264): Int healer, Haste-favored.
        { 264, MakeRow(0.0f, 0.0f, 0.85f, W_ILVL_PRIMARY, 0.55f, 0.65f, 0.85f, 0.75f, 0.65f, 0.40f, 0.30f, 0.30f, 4.0f) },

        // ===== Mage =====
        // Arcane (62), Fire (63), Frost (64): all Int. Mastery-Crit-Haste shifts per spec.
        { 62,  MakeRow(0.0f, 0.0f, 0.80f, W_ILVL_PRIMARY, 0.0f, 0.70f, 0.65f, 0.90f, 0.60f, 0.30f, 0.30f, 0.30f, 4.0f) },
        { 63,  MakeRow(0.0f, 0.0f, 0.80f, W_ILVL_PRIMARY, 0.0f, 0.95f, 0.80f, 0.65f, 0.60f, 0.30f, 0.30f, 0.30f, 4.0f) },
        { 64,  MakeRow(0.0f, 0.0f, 0.80f, W_ILVL_PRIMARY, 0.0f, 0.75f, 0.70f, 0.85f, 0.60f, 0.30f, 0.30f, 0.30f, 4.0f) },

        // ===== Warlock =====
        // Affliction (265), Demo (266), Destro (267): Int caster DPS.
        { 265, MakeRow(0.0f, 0.0f, 0.80f, W_ILVL_PRIMARY, 0.0f, 0.75f, 0.85f, 0.70f, 0.65f, 0.30f, 0.30f, 0.30f, 4.0f) },
        { 266, MakeRow(0.0f, 0.0f, 0.80f, W_ILVL_PRIMARY, 0.0f, 0.70f, 0.85f, 0.75f, 0.65f, 0.30f, 0.30f, 0.30f, 4.0f) },
        { 267, MakeRow(0.0f, 0.0f, 0.80f, W_ILVL_PRIMARY, 0.0f, 0.85f, 0.75f, 0.70f, 0.65f, 0.30f, 0.30f, 0.30f, 4.0f) },

        // ===== Monk =====
        // Brewmaster (268): Agi tank, Stamina dominant.
        { 268, MakeRow(0.0f, 0.85f, W_TANK_STAMINA, 0.0f, 0.0f, 0.55f, 0.65f, 0.75f, 0.80f, 0.55f, 0.55f, 0.35f, 10.0f) },
        // Windwalker (269): Agi melee.
        { 269, MakeRow(0.0f, W_ILVL_PRIMARY, 0.85f, 0.0f, 0.0f, 0.75f, 0.75f, 0.70f, 0.65f, 0.30f, 0.30f, 0.30f, 12.0f) },
        // Mistweaver (270): Int healer.
        { 270, MakeRow(0.0f, 0.0f, 0.85f, W_ILVL_PRIMARY, 0.55f, 0.70f, 0.80f, 0.75f, 0.65f, 0.40f, 0.30f, 0.30f, 4.0f) },

        // ===== Druid =====
        // Balance (102): Int caster.
        { 102, MakeRow(0.0f, 0.0f, 0.80f, W_ILVL_PRIMARY, 0.0f, 0.75f, 0.85f, 0.75f, 0.65f, 0.30f, 0.30f, 0.30f, 4.0f) },
        // Feral (103): Agi cat melee.
        { 103, MakeRow(0.0f, W_ILVL_PRIMARY, 0.85f, 0.0f, 0.0f, 0.85f, 0.70f, 0.75f, 0.65f, 0.30f, 0.30f, 0.30f, 12.0f) },
        // Guardian (104): Agi tank, Stamina dominant.
        { 104, MakeRow(0.0f, 0.85f, W_TANK_STAMINA, 0.0f, 0.0f, 0.55f, 0.65f, 0.75f, 0.80f, 0.55f, 0.55f, 0.35f, 10.0f) },
        // Restoration (105): Int healer, Mastery-Haste.
        { 105, MakeRow(0.0f, 0.0f, 0.85f, W_ILVL_PRIMARY, 0.55f, 0.65f, 0.85f, 0.90f, 0.65f, 0.40f, 0.30f, 0.30f, 4.0f) },

        // ===== Demon Hunter =====
        // Havoc (577): Agi melee, fast Haste.
        { 577, MakeRow(0.0f, W_ILVL_PRIMARY, 0.85f, 0.0f, 0.0f, 0.75f, 0.85f, 0.70f, 0.65f, 0.30f, 0.30f, 0.30f, 12.0f) },
        // Vengeance (581): Agi tank.
        { 581, MakeRow(0.0f, 0.85f, W_TANK_STAMINA, 0.0f, 0.0f, 0.55f, 0.65f, 0.75f, 0.80f, 0.55f, 0.55f, 0.35f, 10.0f) },

        // ===== Evoker =====
        // Devastation (1467): Int caster DPS.
        { 1467, MakeRow(0.0f, 0.0f, 0.80f, W_ILVL_PRIMARY, 0.0f, 0.85f, 0.75f, 0.70f, 0.65f, 0.30f, 0.30f, 0.30f, 4.0f) },
        // Preservation (1468): Int healer.
        { 1468, MakeRow(0.0f, 0.0f, 0.85f, W_ILVL_PRIMARY, 0.55f, 0.75f, 0.80f, 0.75f, 0.65f, 0.40f, 0.30f, 0.30f, 4.0f) },
        // Augmentation (1473): Int support DPS.
        { 1473, MakeRow(0.0f, 0.0f, 0.80f, W_ILVL_PRIMARY, 0.0f, 0.75f, 0.80f, 0.75f, 0.65f, 0.30f, 0.30f, 0.30f, 4.0f) },
    };
    return table;
}

// Class-default rows: physical / caster / hybrid. Used when spec_id
// doesn't match a known spec (fresh bot, freshly-introduced spec we
// haven't tabulated yet).
StatPriority const& ClassDefaultFor(uint8 class_id)
{
    // Plate physical: Warrior(1), Paladin(2), Death Knight(6).
    static const StatPriority plate_dps =
        MakeRow(W_ILVL_PRIMARY, 0.0f, 0.85f, 0.0f, 0.0f, 0.75f, 0.75f, 0.70f, 0.65f, 0.30f, 0.30f, 0.30f, 12.0f);
    // Mail/leather physical: Hunter(3), Rogue(4), Shaman(7) Enh, Monk(10), Druid(11), DH(12).
    static const StatPriority agi_dps =
        MakeRow(0.0f, W_ILVL_PRIMARY, 0.85f, 0.0f, 0.0f, 0.75f, 0.80f, 0.70f, 0.65f, 0.30f, 0.30f, 0.30f, 12.0f);
    // Cloth caster: Mage(8), Warlock(9), Priest(5).
    static const StatPriority caster =
        MakeRow(0.0f, 0.0f, 0.80f, W_ILVL_PRIMARY, 0.55f, 0.75f, 0.80f, 0.75f, 0.65f, 0.30f, 0.30f, 0.30f, 4.0f);
    // Evoker(13) — mail caster.
    static const StatPriority evoker =
        MakeRow(0.0f, 0.0f, 0.80f, W_ILVL_PRIMARY, 0.55f, 0.75f, 0.80f, 0.75f, 0.65f, 0.30f, 0.30f, 0.30f, 4.0f);

    switch (class_id)
    {
        case 1: case 2: case 6: return plate_dps;
        case 3: case 4: case 7: case 10: case 11: case 12: return agi_dps;
        case 5: case 8: case 9: return caster;
        case 13: return evoker;
        default: return UniformRow();
    }
}

} // namespace

StatIndex StatIndexForItemMod(uint32 item_mod_type)
{
    switch (item_mod_type)
    {
        case ITEM_MOD_STRENGTH:                 return StatIndex::Strength;
        case ITEM_MOD_AGILITY:                  return StatIndex::Agility;
        case ITEM_MOD_STAMINA:                  return StatIndex::Stamina;
        case ITEM_MOD_INTELLECT:                return StatIndex::Intellect;
        case ITEM_MOD_SPIRIT:                   return StatIndex::Spirit;
        case ITEM_MOD_CRIT_RATING:
        case ITEM_MOD_CRIT_MELEE_RATING:
        case ITEM_MOD_CRIT_RANGED_RATING:
        case ITEM_MOD_CRIT_SPELL_RATING:        return StatIndex::Crit;
        case ITEM_MOD_HASTE_RATING:
        case ITEM_MOD_HASTE_MELEE_RATING:
        case ITEM_MOD_HASTE_RANGED_RATING:
        case ITEM_MOD_HASTE_SPELL_RATING:       return StatIndex::Haste;
        case ITEM_MOD_MASTERY_RATING:           return StatIndex::Mastery;
        case ITEM_MOD_VERSATILITY:              return StatIndex::Versatility;
        case ITEM_MOD_CR_LIFESTEAL:             return StatIndex::Leech;
        case ITEM_MOD_CR_AVOIDANCE:             return StatIndex::Avoidance;
        case ITEM_MOD_CR_SPEED:                 return StatIndex::Speed;
        // AGI_STR_INT / AGI_STR / AGI_INT / STR_INT — flexible primary stats
        // that resolve at equip time to whichever of Agi/Str/Int the spec
        // wants. The snapshot can't know that without re-resolving spec at
        // bag-walk time; for ranking, treat them as Strength so plate-Agi-
        // hybrid items still surface (Versa-fit will match by other stats).
        // The actual primary-stat selection happens server-side on equip.
        case ITEM_MOD_AGI_STR_INT:
        case ITEM_MOD_AGI_STR:
        case ITEM_MOD_AGI_INT:
        case ITEM_MOD_STR_INT:                  return StatIndex::Strength;
        default:                                 return StatIndex::Count;     // unweighted
    }
}

StatPriority const& StatPriorityFor(uint8 class_id, uint32 spec_id)
{
    auto const& tbl = SpecTable();
    auto const it = tbl.find(spec_id);
    if (it != tbl.end()) return it->second;
    return ClassDefaultFor(class_id);
}

} // namespace Playerbot
