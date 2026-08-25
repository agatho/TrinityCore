#include "BotGearGenerator.h"
#include "BotItemScorer.h"
#include "ObjectMgr.h"
#include "ItemTemplate.h"
#include "Player.h"
#include "Log.h"
#include <array>
#include <limits>
#include <unordered_map>

namespace Playerbot::V2::Gear {

namespace {

// Equipment slot indices we generate for. Bag slots (19-22) come from the
// starter kit, not from gear generation. Tabard/Body/Shirt are cosmetic
// and skipped.
constexpr std::array<uint8, 16> kSlots = {
    EQUIPMENT_SLOT_HEAD,
    EQUIPMENT_SLOT_NECK,
    EQUIPMENT_SLOT_SHOULDERS,
    EQUIPMENT_SLOT_CHEST,
    EQUIPMENT_SLOT_WAIST,
    EQUIPMENT_SLOT_LEGS,
    EQUIPMENT_SLOT_FEET,
    EQUIPMENT_SLOT_WRISTS,
    EQUIPMENT_SLOT_HANDS,
    EQUIPMENT_SLOT_FINGER1,
    EQUIPMENT_SLOT_FINGER2,
    EQUIPMENT_SLOT_TRINKET1,
    EQUIPMENT_SLOT_TRINKET2,
    EQUIPMENT_SLOT_BACK,
    EQUIPMENT_SLOT_MAINHAND,
    EQUIPMENT_SLOT_OFFHAND,
};

// Per-class preferred armor type. Each class is restricted by Blizzard's
// armor proficiency rules; this picks the highest tier the class can wear
// at 80 (e.g. Druids wear leather even though they can technically wear
// cloth in early levels). Hunters/Shamans switch from leather to mail at L40.
ItemSubclassArmor PreferredArmorForClass(uint8 cls, uint8 level)
{
    switch (cls)
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
            return ITEM_SUBCLASS_ARMOR_PLATE;
        case CLASS_HUNTER:
        case CLASS_SHAMAN:
            return level >= 40 ? ITEM_SUBCLASS_ARMOR_MAIL : ITEM_SUBCLASS_ARMOR_LEATHER;
        case CLASS_ROGUE:
        case CLASS_DRUID:
        case CLASS_MONK:
        case CLASS_DEMON_HUNTER:
            return ITEM_SUBCLASS_ARMOR_LEATHER;
        case CLASS_PRIEST:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            return ITEM_SUBCLASS_ARMOR_CLOTH;
        case CLASS_EVOKER:
            return ITEM_SUBCLASS_ARMOR_MAIL;
        default:
            return ITEM_SUBCLASS_ARMOR_CLOTH;
    }
}

// Quick "is armor inventory type"
bool IsArmorSlot(uint8 slot)
{
    switch (slot)
    {
        case EQUIPMENT_SLOT_HEAD: case EQUIPMENT_SLOT_SHOULDERS:
        case EQUIPMENT_SLOT_CHEST: case EQUIPMENT_SLOT_WAIST:
        case EQUIPMENT_SLOT_LEGS: case EQUIPMENT_SLOT_FEET:
        case EQUIPMENT_SLOT_WRISTS: case EQUIPMENT_SLOT_HANDS:
            return true;
        default:
            return false;
    }
}

// Inventory types acceptable per equipment slot. A slot can match multiple
// inventory types (e.g. CHEST accepts INVTYPE_CHEST and INVTYPE_ROBE).
bool InventoryTypeFitsSlot(uint8 slot, uint8 inv)
{
    switch (slot)
    {
        case EQUIPMENT_SLOT_HEAD:      return inv == INVTYPE_HEAD;
        case EQUIPMENT_SLOT_NECK:      return inv == INVTYPE_NECK;
        case EQUIPMENT_SLOT_SHOULDERS: return inv == INVTYPE_SHOULDERS;
        case EQUIPMENT_SLOT_CHEST:     return inv == INVTYPE_CHEST || inv == INVTYPE_ROBE;
        case EQUIPMENT_SLOT_WAIST:     return inv == INVTYPE_WAIST;
        case EQUIPMENT_SLOT_LEGS:      return inv == INVTYPE_LEGS;
        case EQUIPMENT_SLOT_FEET:      return inv == INVTYPE_FEET;
        case EQUIPMENT_SLOT_WRISTS:    return inv == INVTYPE_WRISTS;
        case EQUIPMENT_SLOT_HANDS:     return inv == INVTYPE_HANDS;
        case EQUIPMENT_SLOT_FINGER1:
        case EQUIPMENT_SLOT_FINGER2:   return inv == INVTYPE_FINGER;
        case EQUIPMENT_SLOT_TRINKET1:
        case EQUIPMENT_SLOT_TRINKET2:  return inv == INVTYPE_TRINKET;
        case EQUIPMENT_SLOT_BACK:      return inv == INVTYPE_CLOAK;
        case EQUIPMENT_SLOT_MAINHAND:  return inv == INVTYPE_WEAPON || inv == INVTYPE_2HWEAPON
                                           || inv == INVTYPE_WEAPONMAINHAND
                                           || inv == INVTYPE_RANGED || inv == INVTYPE_RANGEDRIGHT
                                           || inv == INVTYPE_THROWN;
        case EQUIPMENT_SLOT_OFFHAND:   return inv == INVTYPE_WEAPON || inv == INVTYPE_WEAPONOFFHAND
                                           || inv == INVTYPE_SHIELD || inv == INVTYPE_HOLDABLE;
        default:                       return false;
    }
}

// Per-class candidate pool: for each slot, sorted-by-ilvl items the class
// can wear. Built once at Initialize().
struct ClassPool
{
    // [slot] -> sorted vec by ItemLevel ASC. Entry = (ItemTemplate*).
    std::array<std::vector<ItemTemplate const*>, 19> slots;
};

// Indexed by class id (1..13). Class 0 unused.
std::array<ClassPool, MAX_CLASSES> g_pools;
bool g_initialized = false;

uint64 fnv1a64(uint64 v)
{
    uint64 h = 0xcbf29ce484222325ULL;
    while (v) { h ^= (v & 0xff); h *= 0x100000001b3ULL; v >>= 8; }
    return h;
}

} // anonymous

// Approximate ilvl target for a level. Linear from L1=1 to L80=600.
// Exposed (declared in the header) for the hygiene under-gear detector.
uint16 TargetIlvlForLevel(uint8 level)
{
    if (level <= 1) return 1;
    if (level >= 80) return 600;
    return uint16((uint32(level) * 600u) / 80u);
}

void Initialize()
{
    if (g_initialized) return;
    g_initialized = true;

    auto const& store = sObjectMgr->GetItemTemplateStore();
    uint32 examined = 0, indexed = 0;

    for (auto const& [entry, tpl] : store)
    {
        ++examined;
        // Allow NoBind, BoE, and BoP for Rare+ (Blue) and Epic. BoP common /
        // uncommon are usually quest items or vendor trash, not equippable
        // upgrades. BoP Rare+ matches what real players loot from dungeons,
        // so distribution bots may as well start in similar gear.
        if (tpl.GetBonding() == BIND_ON_ACQUIRE && tpl.GetQuality() < ITEM_QUALITY_RARE)
            continue;
        // Skip items with quality > Epic (Legendary / Artifact require special unlock)
        if (tpl.GetQuality() > ITEM_QUALITY_EPIC) continue;
        // Skip items with too-high required level for our brackets
        if (tpl.GetBaseRequiredLevel() > 80) continue;
        // Skip items with no required level set BUT high ilvl (raid drops typically)
        if (tpl.GetBaseRequiredLevel() == 0 && tpl.GetBaseItemLevel() > 50) continue;

        uint8 inv = tpl.GetInventoryType();
        // Find matching slot(s) — most items map to 1 slot, but rings/trinkets
        // map to 2 (FINGER1/2, TRINKET1/2). We only index FINGER1/TRINKET1
        // and AutoEquip later picks 2nd slot.
        uint8 target_slot = 255;
        switch (inv)
        {
            case INVTYPE_HEAD:        target_slot = EQUIPMENT_SLOT_HEAD; break;
            case INVTYPE_NECK:        target_slot = EQUIPMENT_SLOT_NECK; break;
            case INVTYPE_SHOULDERS:   target_slot = EQUIPMENT_SLOT_SHOULDERS; break;
            case INVTYPE_CHEST:
            case INVTYPE_ROBE:        target_slot = EQUIPMENT_SLOT_CHEST; break;
            case INVTYPE_WAIST:       target_slot = EQUIPMENT_SLOT_WAIST; break;
            case INVTYPE_LEGS:        target_slot = EQUIPMENT_SLOT_LEGS; break;
            case INVTYPE_FEET:        target_slot = EQUIPMENT_SLOT_FEET; break;
            case INVTYPE_WRISTS:      target_slot = EQUIPMENT_SLOT_WRISTS; break;
            case INVTYPE_HANDS:       target_slot = EQUIPMENT_SLOT_HANDS; break;
            case INVTYPE_FINGER:      target_slot = EQUIPMENT_SLOT_FINGER1; break;
            case INVTYPE_TRINKET:     target_slot = EQUIPMENT_SLOT_TRINKET1; break;
            case INVTYPE_CLOAK:       target_slot = EQUIPMENT_SLOT_BACK; break;
            case INVTYPE_WEAPON:
            case INVTYPE_2HWEAPON:
            case INVTYPE_WEAPONMAINHAND:
            case INVTYPE_RANGED:
            case INVTYPE_RANGEDRIGHT:
            case INVTYPE_THROWN:      target_slot = EQUIPMENT_SLOT_MAINHAND; break;
            case INVTYPE_WEAPONOFFHAND:
            case INVTYPE_SHIELD:
            case INVTYPE_HOLDABLE:    target_slot = EQUIPMENT_SLOT_OFFHAND; break;
            default:                  continue;  // unsupported slot
        }

        // Class weapon-proficiency mask, bit = ItemSubclassWeapon value.
        // GetAllowableClass on weapons is usually -1 (all classes), so
        // without THIS filter the generator handed DKs bows and rogues
        // staves: the equip then failed Player::CanUseItem silently, the
        // pipeline latched its AutoEquip bit, and 82% of DKs (plus 27% of
        // DHs, 14% of Evokers) fought BARE-HANDED — a bigger DPS loss than
        // every rotation bug combined; weapon-requiring abilities hard-
        // failed with SPELL_FAILED_EQUIPPED_ITEM_CLASS (audit B15).
        auto weapon_mask_for_class = [](uint8 c) -> uint32 {
            constexpr uint32 AXE=1u<<0, AXE2=1u<<1, BOW=1u<<2, GUN=1u<<3,
                MACE=1u<<4, MACE2=1u<<5, POLEARM=1u<<6, SWORD=1u<<7,
                SWORD2=1u<<8, GLAIVE=1u<<9, STAFF=1u<<10, FIST=1u<<13,
                DAGGER=1u<<15, XBOW=1u<<18, WAND=1u<<19;
            switch (c)
            {
                case 1:  return AXE|AXE2|MACE|MACE2|POLEARM|SWORD|SWORD2|STAFF|DAGGER|FIST; // Warrior
                case 2:  return AXE|AXE2|MACE|MACE2|POLEARM|SWORD|SWORD2;                   // Paladin
                case 3:  return BOW|GUN|XBOW|AXE|AXE2|SWORD|SWORD2|POLEARM|STAFF|DAGGER|FIST; // Hunter
                case 4:  return DAGGER|SWORD|AXE|MACE|FIST;                                 // Rogue
                case 5:  return MACE|DAGGER|STAFF|WAND;                                     // Priest
                case 6:  return AXE|AXE2|MACE|MACE2|SWORD|SWORD2|POLEARM;                   // Death Knight
                case 7:  return AXE|AXE2|MACE|MACE2|STAFF|DAGGER|FIST;                      // Shaman
                case 8:  return DAGGER|SWORD|STAFF|WAND;                                    // Mage
                case 9:  return DAGGER|SWORD|STAFF|WAND;                                    // Warlock
                case 10: return FIST|AXE|MACE|SWORD|STAFF|POLEARM;                          // Monk
                case 11: return MACE|MACE2|STAFF|DAGGER|FIST|POLEARM;                       // Druid
                case 12: return GLAIVE|SWORD|AXE|FIST;                                      // Demon Hunter
                case 13: return DAGGER|FIST|MACE|SWORD|AXE|STAFF;                           // Evoker
                default: return 0;
            }
        };
        // For each class, check if it can wear this item.
        for (uint8 cls = 1; cls < MAX_CLASSES; ++cls)
        {
            uint32 cls_mask = (1u << (cls - 1));
            uint32 allowable = tpl.GetAllowableClass();
            // 0 / -1 means usable by all classes.
            if (allowable && allowable != uint32(-1) && !(allowable & cls_mask))
                continue;

            // Weapon proficiency (mask table above).
            if (tpl.GetClass() == ITEM_CLASS_WEAPON)
            {
                const uint32 sub = tpl.GetSubClass();
                if (sub > 31 || !(weapon_mask_for_class(cls) & (1u << sub)))
                    continue;
            }
            // Shields: Warrior / Paladin / Shaman only.
            if (inv == INVTYPE_SHIELD && cls != 1 && cls != 2 && cls != 7)
                continue;

            // For armor slots, filter by class's preferred armor type.
            if (IsArmorSlot(target_slot) && tpl.GetClass() == ITEM_CLASS_ARMOR)
            {
                ItemSubclassArmor preferred = PreferredArmorForClass(cls, /*level=*/80);
                // Allow items at or below preferred (cloth-wearing under-leather is bad,
                // but accept for low-level brackets where higher armor unavailable).
                ItemSubclassArmor item_armor = ItemSubclassArmor(tpl.GetSubClass());
                if (item_armor != preferred &&
                    item_armor != ITEM_SUBCLASS_ARMOR_MISCELLANEOUS &&
                    item_armor != ITEM_SUBCLASS_ARMOR_COSMETIC)
                {
                    // Allow lighter armor (cloth ≤ leather ≤ mail ≤ plate) for filler
                    // but prefer same. We'll let GenerateGearFor pick highest matching.
                    if (item_armor > preferred) continue;
                }
            }

            g_pools[cls].slots[target_slot].push_back(&tpl);
        }
        ++indexed;
    }

    // Sort each pool by static base ItemLevel ascending. This is ENUMERATION
    // ORDER ONLY — GenerateGearFor scores every candidate (it does not binary-
    // search), so the sort key does not affect the pick. Do NOT switch this to
    // effective/level-scaled ilvl: the pool is built once at init with no
    // wearer level, while effective ilvl is level-dependent (that scaling is
    // applied per-pick in GenerateGearFor).
    for (auto& pool : g_pools)
        for (auto& slot_vec : pool.slots)
            std::sort(slot_vec.begin(), slot_vec.end(),
                [](ItemTemplate const* a, ItemTemplate const* b) {
                    return a->GetBaseItemLevel() < b->GetBaseItemLevel();
                });

    TC_LOG_INFO("playerbot.v2", "[BotGearGenerator] indexed {} items (of {} examined) into per-class pools",
                indexed, examined);
}

std::vector<GearItem> GenerateGearFor(GearGenerationContext const& ctx)
{
    std::vector<GearItem> out;
    if (!g_initialized || ctx.cls < 1 || ctx.cls >= MAX_CLASSES) return out;

    uint16 const target_ilvl = TargetIlvlForLevel(ctx.level);
    auto const& pool = g_pools[ctx.cls];

    // Shield-tank specs (Protection Warrior 73, Protection Paladin 66) MUST wield
    // a ONE-HAND weapon so they can also carry a shield: a 2H locks out the
    // offhand, and with no shield the tank loses block, ~half its armor, and its
    // entire Shield Slam / Shield Block kit. The raw-stat scorer ranks a 2H above
    // a 1H, so without this guard the generator handed prot tanks a 2H mainhand
    // plus a shield it could NEVER equip (EQUIP_ERR_2HANDED_EQUIPPED) — observed
    // live 2026-06-28: the Deadmines prot-warrior tank ran a 2H with an empty
    // offhand and died 38x in the harbor, casting Shield Slam/Block with no
    // shield. For these specs we skip INVTYPE_2HWEAPON mainhand candidates so the
    // pick falls to the best 1H, leaving the offhand free for the shield.
    const bool shield_tank = (ctx.spec == 73 || ctx.spec == 66);

    out.reserve(kSlots.size());
    uint32 slot_idx = 0;
    for (uint8 slot : kSlots)
    {
        // Pick from pool slot.
        // For FINGER2/TRINKET2, reuse FINGER1/TRINKET1 pool (same items wearable in both).
        uint8 lookup_slot = (slot == EQUIPMENT_SLOT_FINGER2)  ? EQUIPMENT_SLOT_FINGER1
                          : (slot == EQUIPMENT_SLOT_TRINKET2) ? EQUIPMENT_SLOT_TRINKET1
                          : slot;
        auto const& candidates = pool.slots[lookup_slot];
        if (candidates.empty()) { ++slot_idx; continue; }

        // Pick the candidate that best fits the bot's class+spec and lands
        // near the target ilvl. Combined score = fit - 2 * |ilvl - target|;
        // the distance penalty keeps us in the bracket's ilvl band, while
        // the shared Gear scorer (primary stat, armor type, secondaries)
        // breaks ties so a wrong-primary item never beats a right-primary
        // alternative within the same band. ~250 ilvl-distance points is
        // worth one primary-stat match (+500 in the scorer).
        ItemTemplate const* best = nullptr;
        int32 best_score = std::numeric_limits<int32>::min();
        int32 const target = int32(target_ilvl);
        for (auto const* tpl : candidates)
        {
            if (tpl->GetBaseRequiredLevel() > ctx.level) continue;
            // Shield-tank mainhand: never a 2H (see shield_tank note above).
            if (shield_tank && slot == EQUIPMENT_SLOT_MAINHAND &&
                tpl->GetInventoryType() == INVTYPE_2HWEAPON)
                continue;
            // Shield-tank offhand: must be a SHIELD — not an offhand weapon /
            // holdable a prot warrior/paladin can't dual-wield (it would fail
            // EQUIP_ERR_2HSKILLNOTFOUND and leave the tank shield-less).
            if (shield_tank && slot == EQUIPMENT_SLOT_OFFHAND &&
                tpl->GetInventoryType() != INVTYPE_SHIELD)
                continue;
            // Use the LEVEL-SCALED effective ilvl, not the static base. TC 12.0
            // scales many items at wear-time (PlayerLevelToItemLevelCurveId);
            // ranking by static base made the generator pick a base-120 piece
            // that collapses to effective ilvl 5 when worn at L16 over a static
            // green that wears at ~20 — the under-gearing root (Varethon L16
            // ItemLevel 5, Loroyn L45 ItemLevel 15). dist is now in effective
            // units so the pick lands near what the bot actually wears.
            int32 ilvl = ::Playerbot::Gear::EffectiveItemLevelForLevel(tpl, ctx.level);
            int32 dist = ilvl > target ? ilvl - target : target - ilvl;
            int32 fit = ::Playerbot::Gear::ScoreItemForClass(
                tpl, ctx.cls, ctx.spec, ctx.level);
            int32 combined = fit - 2 * dist;
            if (combined > best_score) { best_score = combined; best = tpl; }
        }

        if (best)
            out.push_back({slot, best->GetId()});
        ++slot_idx;
    }

    // Deterministic shuffle of choice for FINGER2/TRINKET2 — pick a different
    // item than FINGER1/TRINKET1 if multiple candidates exist. Skip for now
    // (acceptable to wear matching pairs).

    return out;
}

} // namespace Playerbot::V2::Gear
