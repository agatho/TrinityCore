#include "BotItemScorer.h"
#include "ItemTemplate.h"
#include "Item.h"
#include "Player.h"
#include "DBCEnums.h"
#include "SharedDefines.h"

namespace Playerbot::Gear {

int32 EffectiveItemLevelForLevel(ItemTemplate const* tpl, uint8 level)
{
    if (!tpl)
        return 0;
    // Mirror Item::GetItemLevel(owner) but without a live Item: initialize a
    // stack BonusData from the template (curve ids + base ilvl) and call the
    // static overload. fixedLevel/min/max/azerite all 0 + pvp false, matching a
    // normal sub-max wearer (the under-gearing case). Items with no scaling
    // curve fall through to the static base ilvl unchanged.
    BonusData bonus;
    bonus.Initialize(tpl);
    return int32(Item::GetItemLevel(tpl, bonus, level, /*fixedLevel*/ 0,
        /*minItemLevel*/ 0, /*minItemLevelCutoff*/ 0, /*maxItemLevel*/ 0,
        /*pvpBonus*/ false, /*azeriteLevel*/ 0));
}

namespace {

// Per-class preferred armor type. Hunter/Shaman switch from leather to mail
// at L40 - same rule as BotGearGenerator's PreferredArmorForClass.
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

// Stat-block scorer. Modern items can carry:
//   - Static primary (ITEM_MOD_STRENGTH/AGILITY/INTELLECT)
//   - "Smart" combined primaries that resolve to spec at runtime
//     (ITEM_MOD_AGI_STR_INT, AGI_STR, AGI_INT, STR_INT)
//   - Stamina, secondaries (Crit/Haste/Mastery/Versatility), Spirit, etc.
int32 ScoreStatBlock(ItemTemplate const* tpl, uint8 cls, uint16 spec)
{
    uint32 primary = PrimaryStatForBot(cls, spec);
    bool   healer  = IsHealerSpec(cls, spec);
    int32  total   = 0;

    for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        int32 stat_type = tpl->GetStatModifierBonusStat(i);
        if (stat_type < 0) continue;

        bool primary_match = false;
        if (uint32(stat_type) == primary)
            primary_match = true;
        else if (stat_type == ITEM_MOD_AGI_STR_INT)
            primary_match = true;  // game resolves to bot's spec
        else if (stat_type == ITEM_MOD_AGI_STR)
            primary_match = (primary == ITEM_MOD_AGILITY || primary == ITEM_MOD_STRENGTH);
        else if (stat_type == ITEM_MOD_AGI_INT)
            primary_match = (primary == ITEM_MOD_AGILITY || primary == ITEM_MOD_INTELLECT);
        else if (stat_type == ITEM_MOD_STR_INT)
            primary_match = (primary == ITEM_MOD_STRENGTH || primary == ITEM_MOD_INTELLECT);

        if (primary_match) { total += 500; continue; }

        // Wrong fixed primary - wasted on this bot.
        if (stat_type == ITEM_MOD_STRENGTH ||
            stat_type == ITEM_MOD_AGILITY  ||
            stat_type == ITEM_MOD_INTELLECT)
        {
            total -= 300;
            continue;
        }

        if (stat_type == ITEM_MOD_STAMINA) { total += 60; continue; }

        // Secondaries (rating). All flavors of crit/haste/mastery/vers
        // count as the same modern stat for scoring.
        if (stat_type == ITEM_MOD_CRIT_RATING       ||
            stat_type == ITEM_MOD_CRIT_MELEE_RATING ||
            stat_type == ITEM_MOD_CRIT_RANGED_RATING||
            stat_type == ITEM_MOD_CRIT_SPELL_RATING ||
            stat_type == ITEM_MOD_HASTE_RATING      ||
            stat_type == ITEM_MOD_HASTE_MELEE_RATING||
            stat_type == ITEM_MOD_HASTE_RANGED_RATING ||
            stat_type == ITEM_MOD_HASTE_SPELL_RATING ||
            stat_type == ITEM_MOD_MASTERY_RATING    ||
            stat_type == ITEM_MOD_VERSATILITY)
        {
            total += 40;
            continue;
        }

        if (stat_type == ITEM_MOD_SPIRIT)
        {
            total += healer ? 50 : -25;
            continue;
        }

        // Tank stats. Useful only for tank specs.
        if (stat_type == ITEM_MOD_DEFENSE_SKILL_RATING ||
            stat_type == ITEM_MOD_DODGE_RATING         ||
            stat_type == ITEM_MOD_PARRY_RATING         ||
            stat_type == ITEM_MOD_BLOCK_RATING)
        {
            bool tank_friendly = (cls == CLASS_WARRIOR && spec == 73)
                              || (cls == CLASS_PALADIN && spec == 66)
                              || (cls == CLASS_DEATH_KNIGHT && spec == 250)
                              || (cls == CLASS_DRUID && spec == 104)
                              || (cls == CLASS_MONK && spec == 268)
                              || (cls == CLASS_DEMON_HUNTER && spec == 581);
            total += tank_friendly ? 35 : 0;
            continue;
        }

        // Anything else is small / neutral.
    }
    return total;
}

} // anonymous

uint32 PrimaryStatForBot(uint8 cls, uint16 spec)
{
    switch (cls)
    {
        case CLASS_WARRIOR:
        case CLASS_DEATH_KNIGHT:
            return ITEM_MOD_STRENGTH;

        case CLASS_PALADIN:
            return spec == 65 ? ITEM_MOD_INTELLECT : ITEM_MOD_STRENGTH;  // Holy

        case CLASS_HUNTER:
        case CLASS_ROGUE:
        case CLASS_DEMON_HUNTER:
            return ITEM_MOD_AGILITY;

        case CLASS_DRUID:
            return (spec == 102 || spec == 105) ? ITEM_MOD_INTELLECT : ITEM_MOD_AGILITY;  // Bal/Resto vs Feral/Guardian

        case CLASS_MONK:
            return spec == 270 ? ITEM_MOD_INTELLECT : ITEM_MOD_AGILITY;  // MW vs BrM/WW

        case CLASS_SHAMAN:
            return spec == 263 ? ITEM_MOD_AGILITY : ITEM_MOD_INTELLECT;  // Enh vs Ele/Resto

        case CLASS_PRIEST:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
        case CLASS_EVOKER:
            return ITEM_MOD_INTELLECT;

        default:
            return ITEM_MOD_STRENGTH;
    }
}

bool IsHealerSpec(uint8 cls, uint16 spec)
{
    if (cls == CLASS_PALADIN && spec == 65)  return true;
    if (cls == CLASS_PRIEST && (spec == 256 || spec == 257)) return true;
    if (cls == CLASS_DRUID && spec == 105)   return true;
    if (cls == CLASS_SHAMAN && spec == 264)  return true;
    if (cls == CLASS_MONK && spec == 270)    return true;
    if (cls == CLASS_EVOKER && spec == 1468) return true;
    return false;
}

int32 ScoreItemForClass(ItemTemplate const* tpl,
                        uint8 cls, uint16 spec, uint8 level)
{
    if (!tpl) return -1000;

    int32 score = 0;
    uint32 cls_mask = (1u << (cls - 1));

    // AllowableClass: 0 / -1 = anyone, otherwise must include bot's class.
    int32 allowable = tpl->GetAllowableClass();
    if (allowable && allowable != -1 && !(uint32(allowable) & cls_mask))
        return -1000;

    // Armor type fit
    if (tpl->GetClass() == ITEM_CLASS_ARMOR)
    {
        ItemSubclassArmor sub = ItemSubclassArmor(tpl->GetSubClass());
        ItemSubclassArmor preferred = PreferredArmorForClass(cls, level);
        if (sub == preferred)               score += 200;
        else if (sub == ITEM_SUBCLASS_ARMOR_MISCELLANEOUS) score += 50;
        else if (sub == ITEM_SUBCLASS_ARMOR_COSMETIC)      score += 0;
        else                                 score -= 100;
    }
    else if (tpl->GetClass() == ITEM_CLASS_WEAPON)
    {
        uint8 inv = tpl->GetInventoryType();
        switch (cls)
        {
            case CLASS_WARRIOR: case CLASS_PALADIN: case CLASS_DEATH_KNIGHT:
                if (inv == INVTYPE_2HWEAPON) score += 150;
                else if (inv == INVTYPE_WEAPON || inv == INVTYPE_WEAPONMAINHAND) score += 120;
                else if (inv == INVTYPE_SHIELD) score += 100;
                break;
            case CLASS_HUNTER:
                if (inv == INVTYPE_RANGED || inv == INVTYPE_RANGEDRIGHT) score += 200;
                else if (inv == INVTYPE_2HWEAPON || inv == INVTYPE_WEAPON) score += 80;
                break;
            case CLASS_ROGUE: case CLASS_DEMON_HUNTER:
                if (inv == INVTYPE_WEAPON || inv == INVTYPE_WEAPONMAINHAND
                 || inv == INVTYPE_WEAPONOFFHAND) score += 150;
                break;
            case CLASS_DRUID: case CLASS_MONK:
                if (inv == INVTYPE_2HWEAPON || inv == INVTYPE_WEAPON) score += 130;
                break;
            case CLASS_PRIEST: case CLASS_MAGE: case CLASS_WARLOCK:
                if (inv == INVTYPE_2HWEAPON) score += 150;
                else if (inv == INVTYPE_WEAPONMAINHAND
                      || inv == INVTYPE_WEAPON) score += 100;
                else if (inv == INVTYPE_HOLDABLE) score += 100;
                else if (inv == INVTYPE_RANGEDRIGHT) score += 50;
                break;
            case CLASS_SHAMAN:
                if (inv == INVTYPE_2HWEAPON) score += 130;
                else if (inv == INVTYPE_WEAPONMAINHAND
                      || inv == INVTYPE_WEAPONOFFHAND
                      || inv == INVTYPE_WEAPON) score += 110;
                else if (inv == INVTYPE_SHIELD) score += 90;
                break;
            case CLASS_EVOKER:
                if (inv == INVTYPE_2HWEAPON) score += 130;
                else if (inv == INVTYPE_WEAPONMAINHAND
                      || inv == INVTYPE_WEAPON) score += 100;
                break;
        }
    }

    // Stat block: primary stat match dominates.
    score += ScoreStatBlock(tpl, cls, spec);

    // Tie-break: ilvl, then quality. Use the LEVEL-SCALED effective ilvl (what
    // the bot actually wears) not the static base — a base-120 piece that the
    // 12.0 scaling curve collapses to effective ilvl 5 at this level must not
    // out-score a static green that wears at its full ~20 (the under-gearing
    // root: bots wore curve-collapsed gear). Falls back to base for non-scaling
    // items. Callers (generator, quest-reward pickers) all pass the bot's real
    // level, so effective-at-level is the correct comparison metric.
    score += EffectiveItemLevelForLevel(tpl, level);
    score += int32(tpl->GetQuality()) * 5;

    return score;
}

int32 ScoreItemForBot(ItemTemplate const* tpl, Player* bot)
{
    if (!bot) return -1000;
    uint16 spec = uint16(AsUnderlyingType(bot->GetPrimarySpecialization()));
    return ScoreItemForClass(tpl, bot->GetClass(), spec, bot->GetLevel());
}

} // namespace Playerbot::Gear
