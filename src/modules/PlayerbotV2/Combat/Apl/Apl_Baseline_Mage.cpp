// Apl_Baseline_Mage.cpp — baseline rotation for class CLASS_MAGE (spec=0). Extracted from the monolithic Apl_Baseline.cpp on the split refactor; future edits go
// here exclusively. See Apl_Baseline_Common.h for the
// shared helpers + rule macros.
//
// To audit coverage:
//   python src/modules/PlayerbotV2/tools/baseline_coverage_audit.py
//
// ---- Validated spell IDs (WoW 12.1.0.69587, Mage class baseline) ----
//   116    Frostbolt (L1)         319836 Fire Blast (L2)
//   122    Frost Nova (L3)        1953   Blink (L4)
//   1449   Arcane Explosion (L6)  2139   Counterspell (L7)
//   1459   Arcane Intellect (L8)  120    Cone of Cold (L18)
//   61025 / 61780 / 126819 / 161353 / 161354 / 161355 Polymorph (L1)
//   118    Polymorph (L10)
//
// ---- Skipped spells (and why) ----
//   133    Fireball      - Fire SPEC spell (overrides Frostbolt at L10);
//                          not in the class baseline list.
//   45438  Ice Block     - class-tree talent, not baseline.
//   55342  Mirror Image  - class-tree talent, not baseline.
//   80353  Time Warp     - L49 baseline; boss-gated group CD lives in the
//                          spec rotations, baseline stays lean.

#include "Apl_Baseline_Common.h"
#include "ApCrowdControl.h"          // PickOffTargetCC / CanBeCCd / ApInPvp

#include <span>

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::HasLiveTarget;
using ::Playerbot::Combat::baseline_common::AlwaysInCombat;
using ::Playerbot::Combat::baseline_common::DoAutoAttack;

constexpr uint32 FROSTBOLT         = 116;         // L1
constexpr uint32 ARCANE_EXPLOSION  = 1449;        // L6 - PBAoE
constexpr uint32 FROST_NOVA        = 122;         // L3 - 8y root
constexpr uint32 COUNTERSPELL      = 2139;        // L7
constexpr uint32 BLINK             = 1953;        // L4
constexpr uint32 FIRE_BLAST        = 319836;      // L2 - core instant (baseline ID)
constexpr uint32 ARCANE_INTELLECT  = 1459;        // L8 - group/self intellect buff
constexpr uint32 CONE_OF_COLD      = 120;         // L18 - frontal cone + slow

// Polymorph variants (12.1.0.69587 class baseline). Different spell IDs
// encode different visuals (serpent / turkey / porcupine / etc.) - all
// share the L1 unlock + functionally identical CC; 118 (sheep) is the L10
// form. Pick the first ID the bot has trained; that's what their data
// layer (SkillLineAbility) granted. Pattern lifted from the hunter
// baseline *_IDS arrays.
constexpr uint32 POLYMORPH_IDS[] = { 118, 61025, 61780, 126819, 161353, 161354, 161355 };

// First candidate the bot knows + is ready to cast. 0 = none.
// Same helper shape as Apl_Baseline_Hunter.cpp's FirstReady.
inline uint32 FirstReadyPoly(ApPredicateContext const& ctx, std::span<const uint32> ids)
{
    for (uint32 sid : ids)
        if (ctx.bot.knows_spell(sid) && ctx.bot.is_ready(sid))
            return sid;
    return 0;
}

BASELINE_SPELL_RULE(Frostbolt,    FROSTBOLT)
BASELINE_INTERRUPT_RULE(Counterspell, COUNTERSPELL)

BASELINE_SELF_RULE(FrostNova, FROST_NOVA)
BASELINE_SELF_RULE(Blink,     BLINK)
BASELINE_SELF_RULE(ArcaneExplosion, ARCANE_EXPLOSION)
BASELINE_SELF_RULE(ConeOfCold, CONE_OF_COLD)

// L8 group/self buff. Retail 1459 is a self-aura that auto-shares to
// party members via Magic-school intellect aura — casting on self
// satisfies group coverage. Refresh-aware via the macro's find_aura
// gate (re-fires only when the aura drops).
BASELINE_SELFBUFF_RULE(ArcaneIntellect, ARCANE_INTELLECT)

bool ShouldBlinkLow(ApPredicateContext const& ctx)
{
    if (ctx.bot.hp_pct() >= 30) return false;
    return ctx.bot.knows_spell(BLINK) && ctx.bot.is_ready(BLINK);
}

// Frost Nova: defensive root. Original baseline rule fires on any
// melee-adjacent enemy when CD is ready. Override the SELF_RULE
// gate with a melee-range check so we don't burn a 30s CD with no
// enemies inside the 8y nova radius.
bool ShouldFrostNovaMelee(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(FROST_NOVA)) return false;
    if (!ctx.bot.is_ready(FROST_NOVA)) return false;
    return ctx.bot.enemies_within(8.0f) >= 1;
}

// Arcane Explosion: 8y PBAoE. Fires at 2+ enemies in range. Override
// the SELF_RULE-generated predicate so we don't waste mana on a single
// target. (Spec rotations apply tighter thresholds; baseline 2 is
// permissive because low-level mages have no other AoE option.)
bool ShouldArcaneExplosionAoE(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(ARCANE_EXPLOSION)) return false;
    if (!ctx.bot.is_ready(ARCANE_EXPLOSION)) return false;
    return ctx.bot.enemies_within(8.0f) >= 2;
}

// Cone of Cold (L18): frontal cone damage + slow, 25s CD. Fire on a 2+
// melee cluster so the slow buys kiting room; Frost Nova's root already
// fired above when only one attacker is on us.
bool ShouldConeOfColdMelee(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(CONE_OF_COLD)) return false;
    if (!ctx.bot.is_ready(CONE_OF_COLD)) return false;
    return ctx.bot.enemies_within(8.0f) >= 2;
}

// Fire Blast: instant ranged nuke. Core baseline filler whenever
// off-GCD/instant is needed (e.g. mid-move). Simple have-target +
// ready + knows-spell gate; the engine handles the actual GCD/CD.
bool ShouldFireBlast(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FIRE_BLAST)) return false;
    return ctx.bot.is_ready(FIRE_BLAST);
}
void DoFireBlast(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FIRE_BLAST, ctx.bot.victim());
}

// Polymorph — baseline emergency CC.
//
// Trigger model (intentionally narrow for baseline):
//   * 2+ enemies engaged AND
//   * (a) a non-victim caster/dps enemy exists  -> sheep that one off, OR
//   * (b) bot HP ≤ 50%                          -> emergency sheep on the
//                                                  victim (last resort,
//                                                  bot kites + heals)
//
// Variant resolution: walks POLYMORPH_IDS and picks the first known +
// ready ID. Different morph visuals (sheep/turtle/penguin/etc.) all
// share the L1 unlock — whichever the data layer trained on this
// character is what we cast.
//
// Refresh-aware: skips an off-target candidate that's already sheeped
// (avoids re-applying a 60s CC and resetting its diminishing-returns
// counter on PvP targets).
// Emergency victim-sheep candidate: the unit we're fighting, when we're
// about to die, not already CC'd and CC-able by type. A last-resort reset
// for a low-level mage with no heal. Returns true when it should fire.
inline bool CanPanicSheepVictim(ApPredicateContext const& ctx, uint32 sid)
{
    if (ctx.bot.hp_pct() > 50) return false;
    NearbyUnit const* vi = ctx.bot.victim_info();
    return vi && !vi->is_cc_locked && CanBeCCd(sid, vi->creature_type);
}
bool ShouldPolymorph(ApPredicateContext const& ctx)
{
    const uint32 sid = FirstReadyPoly(ctx, POLYMORPH_IDS);
    if (sid == 0) return false;
    // (a) Off-target sheep — only on a genuine 2+ ATTACKER pull, skipping
    // already-CC'd mobs (shared gate; see ApCrowdControl.h). The old
    // nearby_enemies.size()>=2 + has_aura path sheeped a 40y scan
    // bystander every GCD and never let the damage rules run.
    if (!PickOffTargetCC(ctx, sid, ApInPvp(ctx)).IsEmpty()) return true;
    // (b) Emergency CC the victim itself when HP critical.
    return CanPanicSheepVictim(ctx, sid);
}
void DoPolymorph(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const uint32 sid = FirstReadyPoly(ctx, POLYMORPH_IDS);
    if (sid == 0) return;
    ObjectGuid t = PickOffTargetCC(ctx, sid, ApInPvp(ctx));
    if (t.IsEmpty() && CanPanicSheepVictim(ctx, sid)) t = ctx.bot.victim();
    if (!t.IsEmpty()) e.cast(sid, t);
}

ApRule const baseline_mage_kRules[] = {
    // Survival first.
    { ShouldBlinkLow,           DoBlink,           "Blink (<30% escape)"           },
    // Self-buff (refresh).
    { ShouldArcaneIntellect,    DoArcaneIntellect, "Arcane Intellect (self-buff)"  },
    // Interrupt (caster-victim gated by macro).
    { ShouldCounterspell,       DoCounterspell,    "Counterspell (interrupt)"      },
    // Melee root / slow.
    { ShouldFrostNovaMelee,     DoFrostNova,       "Frost Nova (root melee)"       },
    { ShouldConeOfColdMelee,    DoConeOfCold,      "Cone of Cold (2+ melee)"       },
    // Emergency CC.
    { ShouldPolymorph,          DoPolymorph,       "Polymorph (CC off-target / panic)"},
    // AoE.
    { ShouldArcaneExplosionAoE, DoArcaneExplosion, "Arcane Explosion (PBAoE)"      },
    // Damage.
    { ShouldFireBlast,          DoFireBlast,       "Fire Blast (instant)"          },
    { ShouldFrostbolt,          DoFrostbolt,       "Frostbolt"                     },
    // Fallback.
    { AlwaysInCombat,           DoAutoAttack,      "Auto attack"                   },
};

} // anonymous

void RegisterApl_Baseline_Mage()
{
    RegisterRotation(CLASS_MAGE, 0, ApRotation{baseline_mage_kRules});
}

} // namespace Playerbot::Combat
