// Apl_Baseline_Priest.cpp — baseline rotation for class CLASS_PRIEST (spec=0). Extracted from the monolithic Apl_Baseline.cpp on the split refactor; future edits go
// here exclusively. See Apl_Baseline_Common.h for the
// shared helpers + rule macros.
//
// Coverage focus: L1-9 leveling + unspecced. WoW 12.1.0.69587: the Priest
// class baseline below L10 is Smite (L1), Shadow Word: Pain (L2), Flash
// Heal (L3), Power Word: Shield (L4) and Power Word: Fortitude (L6). Shadow
// Mend (now a Discipline talent passive), Renew (removed) and Fade (a class
// talent from L10) are no longer class baseline spells, so self-sustain is
// PW: Shield (7.5s category cooldown - Weakened Soul is gone) + Flash Heal.
//
// To audit coverage:
//   python src/modules/PlayerbotV2/tools/baseline_coverage_audit.py

#include "Apl_Baseline_Common.h"

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::HasLiveTarget;
using ::Playerbot::Combat::baseline_common::AlwaysInCombat;
using ::Playerbot::Combat::baseline_common::DoAutoAttack;

// ---- Spell IDs (WoW 12.1.0.69587 class baseline, SkillLineAbility) ----
constexpr uint32 SMITE                = 585;     // L1
constexpr uint32 SHADOW_WORD_PAIN     = 589;     // L2
constexpr uint32 FLASH_HEAL           = 2061;    // L3
constexpr uint32 POWER_WORD_SHIELD    = 17;      // L4 - 7.5s category CD (Weakened Soul 6788 no longer exists)
constexpr uint32 POWER_WORD_FORTITUDE = 21562;   // L6 group stamina buff

// ---- Damage ----
BASELINE_SPELL_RULE(Smite,            SMITE)
BASELINE_DEBUFF_RULE(ShadowWordPain,  SHADOW_WORD_PAIN)

// ---- Self-buff (refresh when not up) ----
BASELINE_SELFBUFF_RULE(PowerWordFortitude, POWER_WORD_FORTITUDE)

// ---- Survival ----
// PW: Shield self - proactive absorb at <=70%. is_ready covers the 7.5s
// category cooldown; the aura gate avoids stacking a fresh shield over an
// existing one. Doubles as pre-pull when an enemy is nearby.
bool ShouldPowerWordShieldSelf(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(POWER_WORD_SHIELD)) return false;
    if (!ctx.bot.is_ready(POWER_WORD_SHIELD)) return false;
    if (ctx.bot.hp_pct() > 70) return false;
    if (ctx.bot.find_aura(POWER_WORD_SHIELD, ObjectGuid::Empty)) return false;
    return true;
}
void DoPowerWordShieldSelf(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(POWER_WORD_SHIELD, ObjectGuid::Empty);
}

// Flash Heal - the only direct heal in the 12.1 class baseline (Shadow Mend
// and Renew are gone below L10).
BASELINE_DEFENSIVE_RULE(FlashHealSelf,  FLASH_HEAL,  50)

ApRule const baseline_priest_kRules[] = {
    { ShouldPowerWordShieldSelf,   DoPowerWordShieldSelf,   "PW: Shield self (<=70%)"     },
    { ShouldFlashHealSelf,         DoFlashHealSelf,         "Flash Heal self (<=50%)"     },
    { ShouldPowerWordFortitude,    DoPowerWordFortitude,    "PW: Fortitude (self-buff)"   },
    { ShouldShadowWordPain,        DoShadowWordPain,        "Shadow Word: Pain (debuff)"  },
    { ShouldSmite,                 DoSmite,                 "Smite (filler)"              },
    { AlwaysInCombat,              DoAutoAttack,            "Auto attack"                 },
};

} // anonymous

void RegisterApl_Baseline_Priest()
{
    RegisterRotation(CLASS_PRIEST, 0, ApRotation{baseline_priest_kRules});
}

} // namespace Playerbot::Combat
