// Apl_Baseline_Dh.cpp - baseline rotation for class CLASS_DEMON_HUNTER (spec=0).
// WoW 12.1.0.69587: DHs begin play at L8 and pick a spec at L10. Before that
// the class-wide kit (SkillLineAbility, skill line 1848) is only the generic
// pre-spec Demon's Bite 344859 / Chaos Strike 344862 (which the spec spells
// 162243 / 162794 / Soul Cleave / Reap override), Torment (L9), Sigil of
// Flame (L10), Immolation Aura (L11), Throw Glaive (L12), Disrupt (L19),
// Metamorphosis (L20) plus the class-wide Fury of the Illidari / Soul Carver
// rows. Blur, Blade Dance, Eye Beam are SPEC spells in 12.1 and left to the
// spec rotations. This baseline covers L8-9 and any corrupt-spec fallback for
// DHs whose spec slot is missing/invalid. See Apl_Baseline_Common.h.
//
// To audit coverage:
//   python src/modules/PlayerbotV2/tools/baseline_coverage_audit.py

#include "Apl_Baseline_Common.h"

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::HasLiveTarget;
using ::Playerbot::Combat::baseline_common::AlwaysInCombat;
using ::Playerbot::Combat::baseline_common::DoAutoAttack;

// ---- Spell IDs (WoW 12.1.0.69587, class baseline SkillLineAbility 1848) ----
constexpr uint32 DEMONS_BITE       = 344859;   // pre-spec generic Fury generator
constexpr uint32 CHAOS_STRIKE      = 344862;   // pre-spec generic spender (40 Fury)
constexpr uint32 TORMENT           = 185245;   // L9 taunt - gated on knows_spell
constexpr uint32 SIGIL_OF_FLAME    = 204596;   // L10 ground AoE + Fury
constexpr uint32 IMMOLATION_AURA   = 258920;   // L11 self-buff AoE + Fury
constexpr uint32 THROW_GLAIVE      = 185123;   // L12 ranged opener / gap filler
constexpr uint32 DISRUPT           = 183752;   // L19 interrupt
constexpr uint32 METAMORPHOSIS     = 191427;   // L20 leap + demon form
constexpr uint32 FURY_OF_ILLIDARI  = 201467;   // class-wide 40y AoE, 60s CD
constexpr uint32 SOUL_CARVER       = 207407;   // class-wide melee CD, 60s

constexpr uint8 POWER_FURY_IDX = 17;

// Standard rules.
BASELINE_SPELL_RULE(DemonsBite,    DEMONS_BITE)
BASELINE_SPELL_RULE(SoulCarver,    SOUL_CARVER)
BASELINE_INTERRUPT_RULE(Disrupt,   DISRUPT)
BASELINE_SELF_RULE(ImmolationAura, IMMOLATION_AURA)

// Fury of the Illidari - glaive whirlwind on the target area; cheap 2+
// cleave with a long cooldown.
BASELINE_AOE_RULE(FuryOfIllidari, FURY_OF_ILLIDARI, 8.0f, 2)

// Chaos Strike - 40 Fury spender; the macro has no resource gate.
bool ShouldChaosStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CHAOS_STRIKE)) return false;
    if (!ctx.bot.is_ready(CHAOS_STRIKE)) return false;
    return ctx.bot.power(POWER_FURY_IDX) >= 40;
}
void DoChaosStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CHAOS_STRIKE, ctx.bot.victim());
}

// Metamorphosis - the L20 class version (leap + stun + demon form). Used
// as the baseline's only big button: burst on packs or survival when low.
bool ShouldMetamorphosis(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(METAMORPHOSIS)) return false;
    if (!ctx.bot.is_ready(METAMORPHOSIS)) return false;
    return ctx.bot.hp_pct() <= 50 || ctx.bot.enemies_within(8.0f) >= 3;
}
void DoMetamorphosis(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(METAMORPHOSIS, v->x, v->y, v->z);
    else
        e.cast(METAMORPHOSIS, ctx.bot.victim());
}

// Sigil of Flame - ground sigil under the target; damage + Fury.
bool ShouldSigilOfFlame(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SIGIL_OF_FLAME)) return false;
    return ctx.bot.is_ready(SIGIL_OF_FLAME);
}
void DoSigilOfFlame(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(SIGIL_OF_FLAME, v->x, v->y, v->z);
    else
        e.cast(SIGIL_OF_FLAME);
}

// Throw Glaive - ranged opener. DHs are melee; firing this every GCD would
// waste it and starve the core melee filler. Only fire when there is no
// enemy in melee range (gap-close / pre-pull / kiting), matching the Havoc
// spec's idiom.
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

// Torment - taunt. Only useful for tanks, but baseline covers the L9
// unlock window before spec is committed. Gated on knows_spell so it
// silently no-ops for bots that never learned it.
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
    { ShouldDisrupt,        DoDisrupt,        "Disrupt (interrupt)"        },
    { ShouldTorment,        DoTorment,        "Torment (taunt, L9)"        },
    { ShouldMetamorphosis,  DoMetamorphosis,  "Metamorphosis (burst/low)"  },
    { ShouldFuryOfIllidari, DoFuryOfIllidari, "Fury of the Illidari (2+)"  },
    { ShouldSoulCarver,     DoSoulCarver,     "Soul Carver (on CD)"        },
    { ShouldSigilOfFlame,   DoSigilOfFlame,   "Sigil of Flame (on CD)"     },
    { ShouldImmolationAura, DoImmolationAura, "Immolation Aura (on CD)"    },
    { ShouldThrowGlaive,    DoThrowGlaive,    "Throw Glaive (ranged open)" },
    { ShouldChaosStrike,    DoChaosStrike,    "Chaos Strike (Fury>=40)"    },
    { ShouldDemonsBite,     DoDemonsBite,     "Demon's Bite (builder)"     },
    { AlwaysInCombat,       DoAutoAttack,     "Auto attack"                },
};

} // anonymous

void RegisterApl_Baseline_DH()
{
    RegisterRotation(CLASS_DEMON_HUNTER, 0, ApRotation{baseline_dh_kRules});
}

} // namespace Playerbot::Combat
