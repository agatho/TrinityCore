// Apl_Baseline_Hunter.cpp — baseline rotation for class CLASS_HUNTER (spec=0). Extracted from the monolithic Apl_Baseline.cpp on the split refactor; future edits go
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

// Target version: WoW 12.1.0.69587 (Midnight). Classic-era fallback IDs
// were removed 2026-05-21 per user directive ("we do not develop
// any legacy versions"). If a Midnight bot is observed without the
// retail ID in its spellbook, that's a data-layer issue (db2 /
// SkillLineAbility) to fix in the data, not by reintroducing
// classic fallbacks here.
//
// Validated spell IDs (WoW 12.1.0.69587, Hunter class baseline
// SkillLineAbility list - every id here is class-wide, learn level <= 9):
//   56641  Steady Shot (L1)   | 185358 Arcane Shot (L2)   | 195645 Wing Clip (L3)
//   781    Disengage (L4)     | 136    Mend Pet (L5)      | 883    Call Pet 1 (L5)
//   982    Revive Pet (L5)    | 5384   Feign Death (L6)   | 257284 Hunter's Mark (L7)
//   186265 Aspect of Turtle(L8)| 109304 Exhilaration (L9)
//
// Skipped (deliberate, 12.1): Aimed Shot (19434) is a Marksmanship spec
// talent, Kill Command (34026) a Beast Mastery spec talent, Serpent Sting
// (271788) no longer exists for any Hunter spec, Concussive Shot (5116)
// and Binding Shot (109248; the old 117526 was its stun aura) are class
// TALENTS - none of them is class baseline, so they live in the spec
// rotations only. Freezing Trap (187650) / Aspect of the Cheetah (186257)
// are L10 / movement utility outside the combat ladder.
constexpr uint32 ARCANE_SHOT_IDS[]     = { 185358 };
constexpr uint32 STEADY_SHOT_IDS[]     = { 56641 };
constexpr uint32 HUNTERS_MARK_IDS[]    = { 257284 };
constexpr uint32 WING_CLIP_IDS[]       = { 195645 };
constexpr uint32 DISENGAGE_IDS[]       = { 781 };
constexpr uint32 FEIGN_DEATH_IDS[]     = { 5384 };
constexpr uint32 MEND_PET_IDS[]        = { 136 };
constexpr uint32 CALL_PET_1_IDS[]      = { 883 };
constexpr uint32 REVIVE_PET_IDS[]      = { 982 };
// Pre-L10 baseline coverage.
// Aspect of the Turtle (L8, 186265): 8s damage/CC immunity, panic-tier
// CD - the strongest defensive a baseline hunter owns before specs
// unlock. Exhilaration (L9, 109304): self+pet heal CD (~30% HP/2min).
constexpr uint32 ASPECT_TURTLE_ID      = 186265;
constexpr uint32 EXHILARATION_ID       = 109304;

// First candidate the bot knows + is ready to cast. 0 = none.
inline uint32 FirstReady(ApPredicateContext const& ctx, std::span<const uint32> ids)
{
    for (uint32 sid : ids) if (ctx.bot.is_ready(sid)) return sid;
    return 0;
}
// First candidate the bot has in its spellbook (ignores cooldown / GCD).
// Used for predicates that need to check aura presence (Hunter's Mark)
// to decide whether to refresh before we care if it's ready.
inline uint32 FirstKnown(ApPredicateContext const& ctx, std::span<const uint32> ids)
{
    for (uint32 sid : ids) if (ctx.bot.knows_spell(sid)) return sid;
    return 0;
}

// ---- Pet maintenance ----
// Revive Pet: only when the bot HAD a pet that's now dead. If pet_guid is
// empty the bot never tamed/summoned one, and casting Revive Pet does
// nothing — the rule then fires every tick (~5Hz) and starves every
// downstream rule of evaluation. See log audit 2026-05-21: spell 982
// attempted 22,819 times across the fleet, vs 0 Arcane Shot.
bool ShouldRevivePet(ApPredicateContext const& ctx)
{
    if (ctx.bot.pet_guid().IsEmpty()) return false;   // no pet at all
    if (ctx.bot.has_pet()) return false;              // pet alive
    return FirstReady(ctx, REVIVE_PET_IDS) != 0;
}
void DoRevivePet(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 sid = FirstReady(ctx, REVIVE_PET_IDS)) e.cast(sid, ObjectGuid::Empty);
}
// Call Pet 1: out-of-combat summon when an empty pet slot points at a
// stabled beast. Only fires if there's actually a pet entry in the
// active stable slot (slot_kind==0 with a non-zero entry id).
// has_stabled_pets() returns true for ANY non-active slot too, which
// for low-level Hunters that never tamed a beast was firing Call Pet
// every OOC tick — server rejected with SPELL_FAILED_NO_PET (32), the
// rule re-fired, 367 wasted casts observed pre-fix. Per-bot cooldown
// on retry prevents log spam after the gate tightens.
bool ShouldCallPet(ApPredicateContext const& ctx)
{
    if (ctx.bot.in_combat()) return false;
    if (!ctx.bot.pet_guid().IsEmpty()) return false;
    if (!ctx.bot.knows_spell(CALL_PET_1_IDS[0])) return false;
    // Require a stable_pets entry that *can* be summoned — slot_kind 0
    // (active slot) with a real creature entry. Anything else is a
    // dead record from a prior pet that got dismissed/abandoned.
    bool has_summonable = false;
    for (auto const& sp : ctx.bot.stable_pets())
    {
        if (sp.slot_kind == 0 && sp.creature_id != 0)
        { has_summonable = true; break; }
    }
    if (!has_summonable) return false;
    return ctx.bot.is_ready(CALL_PET_1_IDS[0]);
}
void DoCallPet(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(CALL_PET_1_IDS[0], ObjectGuid::Empty);
}
bool ShouldMendPet(ApPredicateContext const& ctx)
{
    if (!ctx.bot.has_pet()) return false;
    if (ctx.bot.pet_hp_pct() >= 60) return false;
    return FirstReady(ctx, MEND_PET_IDS) != 0;
}
void DoMendPet(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 sid = FirstReady(ctx, MEND_PET_IDS)) e.cast(sid, ObjectGuid::Empty);
}

// ---- Survival ----
// Aspect of the Turtle: 8s total damage + CC immunity, ~3min CD. This
// is the strongest panic button a hunter has — it stops everything
// (PvE crit spike, mob enrage, PvP burst) for 8 seconds. Fires at
// ≤20% HP. Placed FIRST in the rule list so it pre-empts every other
// decision (including Disengage / Feign Death which both leave the
// bot vulnerable mid-animation). Spell 186265 is granted at L8.
bool ShouldAspectTurtle(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (ctx.bot.hp_pct() > 20) return false;
    if (!ctx.bot.knows_spell(ASPECT_TURTLE_ID)) return false;
    return ctx.bot.is_ready(ASPECT_TURTLE_ID);
}
void DoAspectTurtle(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(ASPECT_TURTLE_ID, ObjectGuid::Empty);
}

// Exhilaration: ~30% HP self+pet heal on a ~2min CD. Sits one tier
// below Aspect of the Turtle — fires at ≤50% HP so it lands before
// the bot enters the panic-CD bracket. Pet co-heal is a bonus (saves
// a Mend Pet GCD when both are wounded). Spell 109304 is granted at L9.
bool ShouldExhilaration(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (ctx.bot.hp_pct() > 50) return false;
    if (!ctx.bot.knows_spell(EXHILARATION_ID)) return false;
    return ctx.bot.is_ready(EXHILARATION_ID);
}
void DoExhilaration(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(EXHILARATION_ID, ObjectGuid::Empty);
}

// Disengage: panic-kite. Fires when a melee-range enemy is adjacent AND
// bot is below 70% HP. Once per CD (~20s). Without this gate L1-9
// hunters never used Disengage — the spec rotations only triggered on
// 2+ melee within 8y, almost never true in starter zones.
bool ShouldDisengage(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (ctx.bot.enemies_within(5.0f) == 0) return false;
    if (ctx.bot.hp_pct() >= 70) return false;
    return FirstReady(ctx, DISENGAGE_IDS) != 0;
}
void DoDisengage(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 sid = FirstReady(ctx, DISENGAGE_IDS)) e.cast(sid, ObjectGuid::Empty);
}

// Feign Death: emergency aggro drop. Hunters' panic-bail when Disengage
// is on cooldown OR doesn't help (kited backwards into wall, multiple
// ranged attackers). Drops player from all PvE creature threat tables,
// making it the strongest survivability tool a low-level hunter has.
// Fires at 25% HP with at least one nearby enemy + Disengage NOT
// available (so the two CDs are alternated, not blown simultaneously).
bool ShouldFeignDeath(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (ctx.bot.hp_pct() >= 25) return false;
    if (ctx.bot.enemies_within(40.0f) == 0) return false;
    // Skip in PvP — Feign Death is broken by player damage so it just
    // wastes the cooldown. Battlegrounds + duels gated here.
    if (ctx.bot.is_pvp()) return false;
    if (FirstReady(ctx, DISENGAGE_IDS) != 0) return false;   // prefer Disengage if up
    return FirstReady(ctx, FEIGN_DEATH_IDS) != 0;
}
void DoFeignDeath(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 sid = FirstReady(ctx, FEIGN_DEATH_IDS)) e.cast(sid, ObjectGuid::Empty);
}

// ---- Slow / utility ----
// Wing Clip (195645, L3, 20 focus, 5y): melee slow, still class baseline
// in 12.1. Fires when an enemy is sitting on top of the bot (<=5y) and
// the slow isn't already running, so the bot can back off to shot range.
bool ShouldWingClip(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.enemies_within(5.0f) == 0) return false;
    uint32 known = FirstKnown(ctx, WING_CLIP_IDS);
    if (!known) return false;
    if (ctx.bot.find_aura(known, ctx.bot.victim())) return false;   // still slowed
    constexpr uint8 POWER_FOCUS_IDX = 2;
    if (ctx.bot.power(POWER_FOCUS_IDX) < 20) return false;
    return FirstReady(ctx, WING_CLIP_IDS) != 0;
}
void DoWingClip(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 sid = FirstReady(ctx, WING_CLIP_IDS)) e.cast(sid, ctx.bot.victim());
}

// ---- Debuff opener ----
bool ShouldHuntersMark(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    uint32 known = FirstKnown(ctx, HUNTERS_MARK_IDS);
    if (!known) return false;
    if (ctx.bot.find_aura(known, ctx.bot.victim())) return false;   // already marked
    return FirstReady(ctx, HUNTERS_MARK_IDS) != 0;
}
void DoHuntersMark(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 sid = FirstReady(ctx, HUNTERS_MARK_IDS)) e.cast(sid, ctx.bot.victim());
}

// ---- Damage rotation ----
// Each one is "have-target + ready + (resource check)". Falls through
// to the next on cooldown so the turn is never wasted on a queued spell.
// Arcane Shot costs 40 focus. The bare is_ready check let this rule
// claim the tick at ANY focus level - the cast then bounced off
// SPELL_FAILED_NO_POWER server-side and (pre-B02) starved Steady Shot,
// the generator sitting one slot below. Gate on affordability. (Spec
// spenders such as Kill Command are spec talents, not class baseline,
// so the pre-L10 ladder has no reserve to keep.)
bool ShouldArcaneShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    constexpr uint8 POWER_FOCUS_IDX = 2;
    constexpr int32 ARCANE_COST = 40;
    if (ctx.bot.power(POWER_FOCUS_IDX) < ARCANE_COST) return false;
    return FirstReady(ctx, ARCANE_SHOT_IDS) != 0;
}
void DoArcaneShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 sid = FirstReady(ctx, ARCANE_SHOT_IDS)) e.cast(sid, ctx.bot.victim());
}
bool ShouldSteadyShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    return FirstReady(ctx, STEADY_SHOT_IDS) != 0;
}
void DoSteadyShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 sid = FirstReady(ctx, STEADY_SHOT_IDS)) e.cast(sid, ctx.bot.victim());
}

ApRule const baseline_hunter_kRules[] = {
    // Top-priority panic: 8s full immunity pre-empts every other decision
    // so the bot doesn't bleed through a CD animation at ≤20% HP.
    { ShouldAspectTurtle,   DoAspectTurtle,   "Aspect of the Turtle (<=20%)"},
    { ShouldCallPet,        DoCallPet,        "Call Pet (stabled, OOC)"     },
    { ShouldRevivePet,      DoRevivePet,      "Revive Pet (dead pet only)"  },
    { ShouldMendPet,        DoMendPet,        "Mend Pet (<60%)"             },
    { ShouldDisengage,      DoDisengage,      "Disengage (panic kite)"      },
    // Exhilaration sits in the Disengage/Feign Death tier — fires at
    // ≤50% HP so it lands before the bot enters the deeper panic CDs.
    { ShouldExhilaration,   DoExhilaration,   "Exhilaration (<=50%)"        },
    { ShouldFeignDeath,     DoFeignDeath,     "Feign Death (emergency)"     },
    { ShouldWingClip,       DoWingClip,       "Wing Clip (melee slow)"      },
    { ShouldHuntersMark,    DoHuntersMark,    "Hunter's Mark (debuff)"      },
    { ShouldArcaneShot,     DoArcaneShot,     "Arcane Shot"                 },
    { ShouldSteadyShot,     DoSteadyShot,     "Steady Shot (filler)"        },
    { AlwaysInCombat,       DoAutoAttack,     "Auto attack"                 },
};

} // anonymous

void RegisterApl_Baseline_Hunter()
{
    RegisterRotation(CLASS_HUNTER, 0, ApRotation{baseline_hunter_kRules});
}

} // namespace Playerbot::Combat
