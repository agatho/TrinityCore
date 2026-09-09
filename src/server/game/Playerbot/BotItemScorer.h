// BotItemScorer - Score how well an item suits a bot.
//
// Used by:
//   - PlayerbotAPI::complete_quest -> ScoreQuestReward: pick best reward
//     index when the bot calls complete_quest with kRewardChoiceAuto.
//   - PlayerbotV2 Bot/Gear/BotGearGenerator: pick the best in-slot item
//     from the per-class candidate pool when distributing gear.
//   - PlayerbotV2 Fleet/StarterQuestAutocomplete: pick the best reward
//     index for a class/race starter quest auto-completion.
//
// Score components (rough magnitude, highest-impact first):
//   - Class can equip       (-1000 / 0)   hard block on mis-class items
//   - Primary stat match    (+500)        decisive at endgame for ilvl-tied gear
//   - Wrong primary stat    (-300)        wasted item for the spec
//   - Armor type match      (+200)        cloth on a mage = good, on a paladin = bad
//   - Wrong armor type      (-100)        but neutral for misc/cosmetic accessories
//   - Weapon proficiency    (+100..+200)  per-class inventory-type alignment
//   - Stamina               (+60)         universal HP buff
//   - Secondaries           (+40)         crit/haste/mastery/versatility
//   - Spirit                (+50/-25)     healer perk, dead weight elsewhere
//   - Tank stats            (+35/+0)      defense/dodge/parry/block (tanks only)
//   - Item level            (+ilvl)       tie-break
//   - Quality               (+5 x Q)      Epic narrowly beats Rare at same ilvl

#pragma once

#include "Define.h"

struct ItemTemplate;
class Player;

namespace Playerbot::Gear {

// Score a candidate item for the bot's class+spec+level. Higher = better fit.
// Negative values are possible (mis-equipped or wrong primary). The class
// can-equip check returns -1000 to effectively rule out items the bot
// physically can't wear.
int32 ScoreItemForBot(ItemTemplate const* tpl, Player* bot);

// Same as above but takes class/spec/level explicitly - used by call sites
// without a Player* (e.g. pre-distribution scoring).
int32 ScoreItemForClass(ItemTemplate const* tpl,
                        uint8 cls, uint16 spec, uint8 level);

// Effective (level-scaled) item level a wearer of `level` would actually have
// equipped, computed WITHOUT a live Item via BonusData::Initialize(tpl) +
// the static Item::GetItemLevel overload. TC 12.0 scales many items at
// wear-time via PlayerLevelToItemLevelCurveId clamped to ContentTuning, so the
// STATIC GetBaseItemLevel() can be wildly different from what the bot wears
// (e.g. a base-120 item collapsing to effective ilvl 5 at L16). This is the
// value the snapshot / .playerbot inspect reports (Item::GetItemLevel(owner)).
// For items with no scaling curve it returns the static base ilvl unchanged.
// NOT for the synchronous snapshot-build thread (DB2 curve lookups); use at
// setup / hygiene / OOC only.
int32 EffectiveItemLevelForLevel(ItemTemplate const* tpl, uint8 level);

// Returns the primary stat (ITEM_MOD_STRENGTH/AGILITY/INTELLECT) for the
// (class, spec) pair. Hybrid classes (Druid/Monk/Pally/Sham) have spec-
// dependent primaries; pure classes return their fixed stat.
uint32 PrimaryStatForBot(uint8 cls, uint16 spec);

// True if this (class, spec) is a healer spec - used to flip Spirit
// scoring from waste to perk.
bool IsHealerSpec(uint8 cls, uint16 spec);

} // namespace Playerbot::Gear
