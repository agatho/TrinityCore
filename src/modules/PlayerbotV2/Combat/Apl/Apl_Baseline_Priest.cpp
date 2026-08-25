// Apl_Baseline_Priest.cpp — baseline rotation for class CLASS_PRIEST (spec=0). Extracted from the monolithic Apl_Baseline.cpp on the split refactor; future edits go
// here exclusively. See Apl_Baseline_Common.h for the
// shared helpers + rule macros.
//
// Coverage focus: L1-9 leveling + unspecced. Priest's hallmark at low level is
// self-sustain — Shadow Mend (L1 instant heal), Power Word: Shield (L1 absorb),
// Renew (L3 HoT), and Fade (L8 threat drop). Combine with Power Word: Fortitude
// (L6 stamina buff) and the SW:P/Smite damage pair.
//
// To audit coverage:
//   python src/modules/PlayerbotV2/tools/baseline_coverage_audit.py

#include "Apl_Baseline_Common.h"

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::HasLiveTarget;
using ::Playerbot::Combat::baseline_common::AlwaysInCombat;
using ::Playerbot::Combat::baseline_common::DoAutoAttack;

constexpr uint32 SMITE              = 585;
constexpr uint32 SHADOW_WORD_PAIN   = 589;
constexpr uint32 POWER_WORD_SHIELD  = 17;
constexpr uint32 WEAKENED_SOUL      = 6788;    // PW:Shield re-cast debuff (~6-7.5s)
constexpr uint32 FLASH_HEAL         = 2061;
constexpr uint32 SHADOW_MEND        = 186440;  // L1 instant self-heal w/ small caster DoT
constexpr uint32 RENEW              = 139;
constexpr uint32 POWER_WORD_FORTITUDE = 21562; // L6 group stamina buff
constexpr uint32 FADE               = 586;

// ---- Damage ----
BASELINE_SPELL_RULE(Smite,            SMITE)
BASELINE_DEBUFF_RULE(ShadowWordPain,  SHADOW_WORD_PAIN)

// ---- Self-buff (refresh when not up) ----
BASELINE_SELFBUFF_RULE(PowerWordFortitude, POWER_WORD_FORTITUDE)

// ---- Survival ----
// Fade: drop threat when we're being beaten on AND wounded — the threat
// drop helps the tank recover aggro instead of us tanking by accident.
bool ShouldFade(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(FADE)) return false;
    if (!ctx.bot.is_ready(FADE)) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoFade(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(FADE, ObjectGuid::Empty);
}

// PW: Shield self — proactive absorb at <=70%. Weakened Soul gate
// prevents wasted GCDs while the re-cast lockout is up; aura gate
// avoids stacking a fresh shield over an existing one. Doubles as
// pre-pull when an enemy is nearby (proxy: any attackers OR victim).
bool ShouldPowerWordShieldSelf(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(POWER_WORD_SHIELD)) return false;
    if (!ctx.bot.is_ready(POWER_WORD_SHIELD)) return false;
    if (ctx.bot.hp_pct() > 70) return false;
    if (ctx.bot.find_aura(WEAKENED_SOUL, ObjectGuid::Empty)) return false;
    if (ctx.bot.find_aura(POWER_WORD_SHIELD, ObjectGuid::Empty)) return false;
    return true;
}
void DoPowerWordShieldSelf(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(POWER_WORD_SHIELD, ObjectGuid::Empty);
}

// Shadow Mend — L1 instant heal at the cost of a small self DoT. The DoT
// is acceptable for the off-GCD spike heal it provides; baseline priests
// have nothing else this fast pre-Flash Heal training.
BASELINE_DEFENSIVE_RULE(ShadowMendSelf, SHADOW_MEND, 50)

// Flash Heal — slower cast fallback when Shadow Mend isn't known/ready.
BASELINE_DEFENSIVE_RULE(FlashHealSelf,  FLASH_HEAL,  50)

// Renew self — top off with the HoT when wounded but not panicking, and
// only when the HoT isn't already ticking on us (avoid clipping).
bool ShouldRenewSelf(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(RENEW)) return false;
    if (!ctx.bot.is_ready(RENEW)) return false;
    if (ctx.bot.hp_pct() > 80) return false;
    if (ctx.bot.find_aura(RENEW, ObjectGuid::Empty)) return false;
    return true;
}
void DoRenewSelf(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(RENEW, ObjectGuid::Empty);
}

ApRule const baseline_priest_kRules[] = {
    { ShouldFade,                  DoFade,                  "Fade (in combat, <=50%)"     },
    { ShouldPowerWordShieldSelf,   DoPowerWordShieldSelf,   "PW: Shield self (<=70%)"     },
    { ShouldShadowMendSelf,        DoShadowMendSelf,        "Shadow Mend self (<=50%)"    },
    { ShouldFlashHealSelf,         DoFlashHealSelf,         "Flash Heal self (<=50%)"     },
    { ShouldPowerWordFortitude,    DoPowerWordFortitude,    "PW: Fortitude (self-buff)"   },
    { ShouldRenewSelf,             DoRenewSelf,             "Renew self (<=80%)"          },
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
