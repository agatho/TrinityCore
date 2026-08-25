// Apl_Baseline_Dh.cpp — baseline rotation for class CLASS_DEMON_HUNTER (spec=0).
// DHs are unique: they begin play at L8 in Midnight and unlock most of the
// baseline kit (Demon's Bite, Chaos Strike, Blade Dance, Eye Beam, Immolation
// Aura, Disrupt, Blur, Throw Glaive) immediately, with Torment landing at L9.
// This baseline covers L8-9 and any corrupt-spec fallback for DHs whose
// spec slot is missing/invalid. See Apl_Baseline_Common.h for shared macros.
//
// To audit coverage:
//   python src/modules/PlayerbotV2/tools/baseline_coverage_audit.py

#include "Apl_Baseline_Common.h"

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::HasLiveTarget;
using ::Playerbot::Combat::baseline_common::AlwaysInCombat;
using ::Playerbot::Combat::baseline_common::DoAutoAttack;

// ---- Spell IDs (WoW 12.0, validated against wago.tools) ----
constexpr uint32 DEMONS_BITE       = 162243;   // core Fury generator
constexpr uint32 CHAOS_STRIKE      = 162794;   // core Fury spender
constexpr uint32 BLADE_DANCE       = 188499;   // AoE spender
constexpr uint32 THROW_GLAIVE      = 185123;   // ranged opener / gap filler
constexpr uint32 EYE_BEAM          = 198013;   // channelled AoE
constexpr uint32 IMMOLATION_AURA   = 258920;   // self-buff AoE + Painbringer heal
constexpr uint32 DISRUPT           = 183752;   // interrupt
constexpr uint32 BLUR              = 198589;   // -50% damage taken defensive
constexpr uint32 TORMENT           = 185245;   // L9 taunt — Vengeance utility,
                                               // gated on knows_spell so Havoc
                                               // bots simply never fire it.

// Standard rules.
BASELINE_SPELL_RULE(DemonsBite,    DEMONS_BITE)
BASELINE_SPELL_RULE(ChaosStrike,   CHAOS_STRIKE)
BASELINE_INTERRUPT_RULE(Disrupt,   DISRUPT)
BASELINE_SELF_RULE(ImmolationAura, IMMOLATION_AURA)

// Blur fires at <=50% HP per the survival-first baseline policy. The macro
// fires when hp_pct < threshold, so pass 51 to capture exactly 50% and below.
BASELINE_DEFENSIVE_RULE(Blur, BLUR, 51)

// AoE rules — Eye Beam needs >=3 targets (it's a long-cooldown burst channel),
// Blade Dance needs >=2 (cheap 35-Fury cleave, ~9s CD).
BASELINE_AOE_RULE(EyeBeam,    EYE_BEAM,    10.0f, 3)
BASELINE_AOE_RULE(BladeDance, BLADE_DANCE, 8.0f,  2)

// Throw Glaive — ranged opener. DHs are melee; firing this every GCD would
// waste it on a 9s CD and starve the core melee filler. Only fire when there
// is no enemy in melee range (gap-close / pre-pull / kiting), matching the
// Havoc spec's idiom.
bool ShouldThrowGlaive(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(THROW_GLAIVE)) return false;
    if (!ctx.bot.is_ready(THROW_GLAIVE)) return false;
    return ctx.bot.enemies_within(8.0f) == 0;
}
void DoThrowGlaive(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(THROW_GLAIVE, ctx.bot.victim());
}

// Torment — taunt. Only useful for Vengeance tanks, but baseline covers
// the L9 unlock window before spec is committed. Gated on knows_spell so it
// silently no-ops for Havoc / spec=0 Havoc-leaning bots.
bool ShouldTorment(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(TORMENT)) return false;
    if (!ctx.bot.is_ready(TORMENT)) return false;
    return ctx.bot.untaunted_enemy() != nullptr;
}
void DoTorment(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.untaunted_enemy())
        e.cast(TORMENT, t->guid);
}

ApRule const baseline_dh_kRules[] = {
    { ShouldBlur,           DoBlur,           "Blur (<=50% defensive)"     },
    { ShouldDisrupt,        DoDisrupt,        "Disrupt (interrupt)"        },
    { ShouldTorment,        DoTorment,        "Torment (taunt, L9)"        },
    { ShouldImmolationAura, DoImmolationAura, "Immolation Aura (on CD)"    },
    { ShouldEyeBeam,        DoEyeBeam,        "Eye Beam (>=3 AoE)"         },
    { ShouldBladeDance,     DoBladeDance,     "Blade Dance (>=2 AoE)"      },
    { ShouldThrowGlaive,    DoThrowGlaive,    "Throw Glaive (ranged open)" },
    { ShouldChaosStrike,    DoChaosStrike,    "Chaos Strike (spender)"     },
    { ShouldDemonsBite,     DoDemonsBite,     "Demon's Bite (builder)"     },
    { AlwaysInCombat,       DoAutoAttack,     "Auto attack"                },
};

} // anonymous

void RegisterApl_Baseline_DH()
{
    RegisterRotation(CLASS_DEMON_HUNTER, 0, ApRotation{baseline_dh_kRules});
}

} // namespace Playerbot::Combat
