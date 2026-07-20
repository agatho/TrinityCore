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

// Target version: WoW 12.0.5+ (Midnight). Classic-era fallback IDs
// were removed 2026-05-21 per user directive ("we do not develop
// any legacy versions"). If a Midnight bot is observed without the
// retail ID in its spellbook, that's a data-layer issue (db2 /
// SkillLineAbility) to fix in the data, not by reintroducing
// classic fallbacks here.
constexpr uint32 ARCANE_SHOT_IDS[]     = { 185358 };
constexpr uint32 STEADY_SHOT_IDS[]     = { 56641 };
constexpr uint32 AIMED_SHOT_IDS[]      = { 19434 };
constexpr uint32 KILL_COMMAND_IDS[]    = { 34026 };
constexpr uint32 SERPENT_STING_IDS[]   = { 271788 };
constexpr uint32 HUNTERS_MARK_IDS[]    = { 257284 };
constexpr uint32 CONCUSSIVE_SHOT_IDS[] = { 5116 };
constexpr uint32 WING_CLIP_IDS[]       = { 195645 };
constexpr uint32 DISENGAGE_IDS[]       = { 781 };
constexpr uint32 FEIGN_DEATH_IDS[]     = { 5384 };
constexpr uint32 MEND_PET_IDS[]        = { 136 };
constexpr uint32 CALL_PET_1_IDS[]      = { 883 };
constexpr uint32 REVIVE_PET_IDS[]      = { 982 };
// Wago.tools gap-fill 2026-05-27: pre-L10 baseline coverage.
// Aspect of the Turtle (L8, 186265): 8s damage/CC immunity, panic-tier
// CD — the strongest defensive a baseline hunter owns before specs
// unlock. Exhilaration (L9, 109304): self+pet heal CD (~30% HP/2min).
// Binding Shot (L1, 117526): talent root AoE — fires only when ≥3
// enemies cluster within 15y, so it doesn't burn its CD on solo trash.
constexpr uint32 ASPECT_TURTLE_ID      = 186265;
constexpr uint32 EXHILARATION_ID       = 109304;
constexpr uint32 BINDING_SHOT_ID       = 117526;

// First candidate the bot knows + is ready to cast. 0 = none.
inline uint32 FirstReady(ApPredicateContext const& ctx, std::span<const uint32> ids)
{
    for (uint32 sid : ids) if (ctx.bot.is_ready(sid)) return sid;
    return 0;
}
// First candidate the bot has in its spellbook (ignores cooldown / GCD).
// Used for predicates that need to check aura presence (Hunter's Mark,
// Serpent Sting) to decide whether to refresh before we care if it's ready.
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
    if (!ctx.bot.knows_spell(883)) return false;
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
    return ctx.bot.is_ready(883);
}
void DoCallPet(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(883, ObjectGuid::Empty);
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
// Wing Clip: classic-era melee slow. Modern retail (BfA+) removes this
// from base spellbook, but the server's data layer may re-grant the
// classic 2974 via SkillLineAbility overlays. Fires when an enemy is
// sitting on top of the bot (≤5y) so the slow lets us back off.
bool ShouldWingClip(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.enemies_within(5.0f) == 0) return false;
    return FirstReady(ctx, WING_CLIP_IDS) != 0;
}
void DoWingClip(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 sid = FirstReady(ctx, WING_CLIP_IDS)) e.cast(sid, ctx.bot.victim());
}
// Concussive Shot: ranged 50% slow. Open with this when a target is
// closing the gap and we're below 80% HP (proxy for "actively being
// chased" — at full HP we don't bother).
bool ShouldConcussiveShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    uint32 sid = FirstReady(ctx, CONCUSSIVE_SHOT_IDS);
    if (!sid) return false;
    // Refresh-aware: don't re-apply while still slowing.
    if (ctx.bot.find_aura(sid, ctx.bot.victim())) return false;
    return ctx.bot.enemies_within(15.0f) > 0;
}
void DoConcussiveShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 sid = FirstReady(ctx, CONCUSSIVE_SHOT_IDS)) e.cast(sid, ctx.bot.victim());
}

// ---- Debuff / DoT openers ----
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
bool ShouldSerpentSting(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    uint32 known = FirstKnown(ctx, SERPENT_STING_IDS);
    if (!known) return false;
    AuraEntry const* a = ctx.bot.find_aura(known, ctx.bot.victim());
    if (a && a->remaining.count() > 3000) return false;   // still ticking
    return FirstReady(ctx, SERPENT_STING_IDS) != 0;
}
void DoSerpentSting(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 sid = FirstReady(ctx, SERPENT_STING_IDS)) e.cast(sid, ctx.bot.victim());
}

// ---- AoE control ----
// Binding Shot: talent-gated ground-targeted AoE root. Only fires when
// ≥3 enemies cluster within 15y so the CD isn't wasted on a single
// add. Cast at the victim's location (matches the spec idiom in
// Apl_Hunter_*: cast_at(x,y,z) when victim_info() resolves, else
// self-cast at the bot's feet as a last resort). Talent (not class
// baseline), so knows_spell() naturally gates non-talented bots out.
bool ShouldBindingShot(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(BINDING_SHOT_ID)) return false;
    if (!ctx.bot.is_ready(BINDING_SHOT_ID)) return false;
    return ctx.bot.enemies_within(15.0f) >= 3;
}
void DoBindingShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(BINDING_SHOT_ID, v->x, v->y, v->z);
    else
        e.cast(BINDING_SHOT_ID, ObjectGuid::Empty);
}

// ---- Damage rotation ----
// Each one is "have-target + ready + (resource check)". Falls through
// to the next on cooldown so the turn is never wasted on a queued spell.
bool ShouldKillCommand(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.has_pet()) return false;   // KC needs the pet to deliver it
    return FirstReady(ctx, KILL_COMMAND_IDS) != 0;
}
void DoKillCommand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 sid = FirstReady(ctx, KILL_COMMAND_IDS)) e.cast(sid, ctx.bot.victim());
}
bool ShouldAimedShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.is_moving()) return false;   // 2.5s cast — interrupts on move
    return FirstReady(ctx, AIMED_SHOT_IDS) != 0;
}
void DoAimedShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 sid = FirstReady(ctx, AIMED_SHOT_IDS)) e.cast(sid, ctx.bot.victim());
}
// Arcane Shot costs 40 focus. The bare is_ready check let this rule
// claim the tick at ANY focus level — the cast then bounced off
// SPELL_FAILED_NO_POWER server-side and (pre-B02) starved Steady Shot,
// the generator sitting one slot below. Gate on affordability, and keep
// a Kill Command reserve banked once KC is known so the pet's best
// button is never delayed by a filler shot.
bool ShouldArcaneShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    constexpr uint8 POWER_FOCUS_IDX = 2;
    constexpr int32 ARCANE_COST = 40;
    constexpr int32 KC_RESERVE  = 30;
    int32 const need = ARCANE_COST +
        (ctx.bot.knows_spell(KILL_COMMAND_IDS[0]) && ctx.bot.has_pet() ? KC_RESERVE : 0);
    if (ctx.bot.power(POWER_FOCUS_IDX) < need) return false;
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
    { ShouldConcussiveShot, DoConcussiveShot, "Concussive Shot (ranged slow)"},
    { ShouldHuntersMark,    DoHuntersMark,    "Hunter's Mark (debuff)"      },
    { ShouldSerpentSting,   DoSerpentSting,   "Serpent Sting (DoT)"         },
    // Binding Shot before single-target damage so a 3+ pack gets rooted
    // before we commit to a ST shot rotation.
    { ShouldBindingShot,    DoBindingShot,    "Binding Shot (3+ AoE root)"  },
    { ShouldKillCommand,    DoKillCommand,    "Kill Command"                },
    { ShouldAimedShot,      DoAimedShot,      "Aimed Shot"                  },
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
