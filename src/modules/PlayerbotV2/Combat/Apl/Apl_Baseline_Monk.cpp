// Apl_Baseline_Monk.cpp — baseline rotation for class CLASS_MONK (spec=0). Extracted from the monolithic Apl_Baseline.cpp on the split refactor; future edits go
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

constexpr uint32 TIGER_PALM              = 100780;
constexpr uint32 BLACKOUT_KICK           = 100784;
constexpr uint32 SPINNING_CRANE_KICK     = 101546;
constexpr uint32 SPEAR_HAND_STRIKE       = 116705;
constexpr uint32 PARALYSIS               = 115078;
constexpr uint32 FORTIFYING_BREW         = 115203;
constexpr uint32 LEG_SWEEP               = 119381;    // AoE stun, 5y radius
constexpr uint32 PROVOKE                 = 115546;    // BrM/MW taunt
constexpr uint32 VIVIFY                  = 116670;    // L4 core self-heal (~50% panic)
constexpr uint32 EXPEL_HARM              = 322101;    // L8 self-heal + small AoE damage
constexpr uint32 CRACKLING_JADE_LIGHTNING= 117952;    // L5 ranged channel
constexpr uint32 CHI_BURST               = 130654;    // Talent — AoE damage (line)
constexpr uint32 CHI_WAVE                = 132463;    // Talent — bouncing heal/dmg chain

BASELINE_SPELL_RULE(TigerPalm,     TIGER_PALM)
BASELINE_SPELL_RULE(BlackoutKick,  BLACKOUT_KICK)
BASELINE_INTERRUPT_RULE(SpearHandStrike, SPEAR_HAND_STRIKE)

BASELINE_SELF_RULE(SpinningCraneKick, SPINNING_CRANE_KICK)
BASELINE_DEFENSIVE_RULE(FortifyingBrew, FORTIFYING_BREW, 40)
// Expel Harm: self-heal + minor AoE damage. Cheap, low CD — fire as soon
// as we drop below 70%. Baseline always uses it as a defensive layer
// (the offensive splash is just a side effect at this level).
BASELINE_DEFENSIVE_RULE(ExpelHarm, EXPEL_HARM, 70)

BASELINE_SPELL_RULE(Provoke, PROVOKE)

// Vivify self panic — core L4 instant heal. Fire on self when HP <= 50%.
// Sits between Expel Harm (<=70%, small) and Fortifying Brew (<=40%, CD)
// in the survival ladder, giving baseline monks a real spike-heal button.
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

// Paralysis emergency CC — baseline picker can't pick non-victim casters,
// so we fire it on the current victim ONLY as a panic CC: 3+ attackers
// AND we're below half HP. Buys ~30s peel against one of the mobs while
// the bot tries to recover. Spec rotations override this with smarter
// off-target casting against actual caster mobs.
bool ShouldParalysisEmergency(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(PARALYSIS)) return false;
    if (!ctx.bot.is_ready(PARALYSIS)) return false;
    if (ctx.bot.attackers_count() < 3) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoParalysisEmergency(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(PARALYSIS, ctx.bot.victim());
}

// Leg Sweep: PBAoE 3s stun. Only fires when 2+ enemies are inside
// the 5y radius — otherwise Paralysis is the cheaper single-target
// CC. is_ready handles the 60s CD; no need to refresh.
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

// Chi Burst: talent line-AoE — fires on victim direction, hits everything
// in a line. Gate on 2+ enemies near the victim (it bounces but only line).
bool ShouldChiBurst(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CHI_BURST)) return false;
    if (!ctx.bot.is_ready(CHI_BURST)) return false;
    return ctx.bot.enemies_within(15.0f) >= 2;
}
void DoChiBurst(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CHI_BURST, ctx.bot.victim());
}

// Chi Wave: talent — bouncing 7-target heal/damage chain. Only worth a
// GCD when there's somewhere to bounce (2+ enemies in chain range).
bool ShouldChiWave(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CHI_WAVE)) return false;
    if (!ctx.bot.is_ready(CHI_WAVE)) return false;
    return ctx.bot.enemies_within(25.0f) >= 2;
}
void DoChiWave(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CHI_WAVE, ctx.bot.victim());
}

// Crackling Jade Lightning: ranged channel — useful when the victim is
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
    { ShouldFortifyingBrew,        DoFortifyingBrew,        "Fortifying Brew (<40%)"          },
    { ShouldExpelHarm,             DoExpelHarm,             "Expel Harm (<=70% self heal)"    },
    { ShouldVivifySelfPanic,       DoVivifySelfPanic,       "Vivify (<=50% panic heal)"       },
    { ShouldSpearHandStrike,       DoSpearHandStrike,       "Spear Hand (interrupt)"          },
    { ShouldParalysisEmergency,    DoParalysisEmergency,    "Paralysis (3+ enemies, <=50% HP)"},
    { ShouldLegSweep,              DoLegSweep,              "Leg Sweep (2+ AoE stun)"         },
    { ShouldProvoke,               DoProvoke,               "Provoke (taunt)"                 },
    { ShouldChiBurst,              DoChiBurst,              "Chi Burst (2+ AoE)"              },
    { ShouldChiWave,               DoChiWave,               "Chi Wave (2+ chain)"             },
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
