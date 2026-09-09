// Apl_Baseline_Monk.cpp -baseline rotation for class CLASS_MONK (spec=0). Extracted from the monolithic Apl_Baseline.cpp on the split refactor; future edits go
// here exclusively. See Apl_Baseline_Common.h for the
// shared helpers + rule macros.
//
// To audit coverage:
//   python src/modules/PlayerbotV2/tools/baseline_coverage_audit.py

#include "Apl_Baseline_Common.h"

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::HasLiveTarget;
using ::Playerbot::Combat::baseline_common::AlwaysInCombat;
using ::Playerbot::Combat::baseline_common::DoAutoAttack;

// ---- Spell IDs (WoW 12.1.0.69587, Monk class baseline / SkillLineAbility) ----
// Validated spell IDs (WoW 12.1.0.69587):
//   100780 Tiger Palm (L1)        100784 Blackout Kick (L2)     116670 Vivify (L4)
//   117952 Crackling Jade Lightning (L5)                        119381 Leg Sweep (L6)
//   101546 Spinning Crane Kick (L7) 322101 Expel Harm (L8)      115546 Provoke (L9)
//   322109 Touch of Death (L10)
//
// Skipped (with reason):
//   115203 Fortifying Brew / 116705 Spear Hand Strike / 115078 Paralysis /
//   123986 Chi Burst              class-tree TALENTS in 12.1, not baseline
//                                 spells; the spec rotations own them.
//   450391 Chi Wave               passive in 12.1.
//   109132 Roll / 209525 Soothing Mist / 169340 Touch of Fatality
//                                 positioning, healer channel, PvP-only.
constexpr uint32 TIGER_PALM              = 100780;
constexpr uint32 BLACKOUT_KICK           = 100784;
constexpr uint32 SPINNING_CRANE_KICK     = 101546;
constexpr uint32 LEG_SWEEP               = 119381;    // AoE stun, 5y radius
constexpr uint32 PROVOKE                 = 115546;    // L9 taunt
constexpr uint32 VIVIFY                  = 116670;    // L4 core self-heal (~50% panic)
constexpr uint32 EXPEL_HARM              = 322101;    // L8 self-heal + small AoE damage
constexpr uint32 CRACKLING_JADE_LIGHTNING= 117952;    // L5 ranged channel
constexpr uint32 TOUCH_OF_DEATH          = 322109;    // L10 execute (<=15% or HP-cap)

BASELINE_SPELL_RULE(TigerPalm,     TIGER_PALM)
BASELINE_SPELL_RULE(BlackoutKick,  BLACKOUT_KICK)

BASELINE_SELF_RULE(SpinningCraneKick, SPINNING_CRANE_KICK)
// Expel Harm: self-heal + minor AoE damage. Cheap, low CD - fire as soon
// as we drop below 70%. Baseline always uses it as a defensive layer
// (the offensive splash is just a side effect at this level).
BASELINE_DEFENSIVE_RULE(ExpelHarm, EXPEL_HARM, 70)

BASELINE_SPELL_RULE(Provoke, PROVOKE)

// Vivify self panic - core L4 instant heal. Fire on self when HP <= 50%.
// Sits below Expel Harm (<=70%, small) in the survival ladder, giving
// baseline monks a real spike-heal button (Fortifying Brew is a talent).
bool ShouldVivifySelfPanic(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(VIVIFY)) return false;
    if (!ctx.bot.is_ready(VIVIFY)) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoVivifySelfPanic(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(VIVIFY, ctx.bot.raw().guid);
}

// Leg Sweep: PBAoE 3s stun. Only fires when 2+ enemies are inside
// the 5y radius. is_ready handles the 60s CD; no need to refresh.
bool ShouldLegSweep(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(LEG_SWEEP)) return false;
    if (!ctx.bot.is_ready(LEG_SWEEP)) return false;
    return ctx.bot.enemies_within(5.0f) >= 2;
}
void DoLegSweep(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(LEG_SWEEP, ObjectGuid::Empty);
}

// Touch of Death: L10 execute. Two windows - target HP <= 15%, or the
// target's max HP is no greater than ours (instant kill on small mobs).
bool ShouldTouchOfDeath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(TOUCH_OF_DEATH)) return false;
    if (!ctx.bot.is_ready(TOUCH_OF_DEATH)) return false;
    NearbyUnit const* t = ctx.bot.victim_info();
    if (!t || t->max_hp <= 0 || t->hp <= 0) return false;
    const int32 hp_pct = static_cast<int32>((int64_t(t->hp) * 100) / t->max_hp);
    if (hp_pct <= 15) return true;
    return t->max_hp <= ctx.bot.max_hp();
}
void DoTouchOfDeath(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(TOUCH_OF_DEATH, ctx.bot.victim());
}

// Crackling Jade Lightning: ranged channel -useful when the victim is
// out of melee but within 30y. Acts as a tag/pull/gap-cover filler so
// baseline monks aren't dead air when they can't reach the mob.
bool ShouldCracklingJadeLightning(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CRACKLING_JADE_LIGHTNING)) return false;
    if (!ctx.bot.is_ready(CRACKLING_JADE_LIGHTNING)) return false;
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v) return false;
    float bx, by, bz;
    ctx.bot.position(bx, by, bz);
    const float dx = v->x - bx;
    const float dy = v->y - by;
    const float dz = v->z - bz;
    const float d2 = dx * dx + dy * dy + dz * dz;
    // > 10y AND <= 30y from the bot.
    return d2 > (10.0f * 10.0f) && d2 <= (30.0f * 30.0f);
}
void DoCracklingJadeLightning(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CRACKLING_JADE_LIGHTNING, ctx.bot.victim());
}

ApRule const baseline_monk_kRules[] = {
    { ShouldExpelHarm,             DoExpelHarm,             "Expel Harm (<=70% self heal)"    },
    { ShouldVivifySelfPanic,       DoVivifySelfPanic,       "Vivify (<=50% panic heal)"       },
    { ShouldLegSweep,              DoLegSweep,              "Leg Sweep (2+ AoE stun)"         },
    { ShouldProvoke,               DoProvoke,               "Provoke (taunt)"                 },
    { ShouldTouchOfDeath,          DoTouchOfDeath,          "Touch of Death (<=15%/HP-cap)"   },
    { ShouldSpinningCraneKick,     DoSpinningCraneKick,     "Spinning Crane Kick (AoE)"      },
    { ShouldBlackoutKick,          DoBlackoutKick,          "Blackout Kick (spender)"         },
    { ShouldTigerPalm,             DoTigerPalm,             "Tiger Palm (builder)"            },
    { ShouldCracklingJadeLightning,DoCracklingJadeLightning,"Crackling Jade (ranged filler)"  },
    { AlwaysInCombat,              DoAutoAttack,            "Auto attack"                     },
};

} // anonymous

void RegisterApl_Baseline_Monk()
{
    RegisterRotation(CLASS_MONK, 0, ApRotation{baseline_monk_kRules});
}

} // namespace Playerbot::Combat
