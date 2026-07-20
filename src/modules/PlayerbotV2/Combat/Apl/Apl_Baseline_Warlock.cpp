// Apl_Baseline_Warlock.cpp — baseline rotation for class CLASS_WARLOCK (spec=0). Extracted from the monolithic Apl_Baseline.cpp on the split refactor; future edits go
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

constexpr uint32 SHADOW_BOLT        = 686;
constexpr uint32 IMMOLATE           = 348;          // L1 Destro DoT
constexpr uint32 AGONY              = 980;          // L10 Aff DoT (kept; auto-skipped pre-L10 via knows_spell)
constexpr uint32 FEAR               = 5782;         // L8 CC
constexpr uint32 SUMMON_IMP         = 688;
constexpr uint32 CREATE_HEALTHSTONE = 6201;
constexpr uint32 CURSE_OF_WEAKNESS  = 702;          // L6 debuff
constexpr uint32 DRAIN_LIFE         = 234153;       // L9 emergency self-heal channel
constexpr uint32 UNENDING_RESOLVE   = 104773;       // L4+ defensive

// Corruption: 146739 is the modern Aff Corruption aura (per wago.tools L3).
// 172 is the legacy ID some clients/specs still resolve. Try the modern
// one first; baseline_warlock will fall through to the legacy ID when
// the modern spell isn't known (e.g. some pre-talent learnsets).
constexpr uint32 CORRUPTION_IDS[] = { 146739, 172 };

// Resolve which Corruption spell the bot actually knows, preferring the
// modern ID. Returns 0 when neither is known.
uint32 KnownCorruption(ApPredicateContext const& ctx)
{
    for (uint32 id : CORRUPTION_IDS)
        if (ctx.bot.knows_spell(id))
            return id;
    return 0;
}

BASELINE_SPELL_RULE(ShadowBolt,  SHADOW_BOLT)

// Corruption: BASELINE_DEBUFF_RULE semantics, but with the multi-ID fallback.
// Fires whichever Corruption variant the bot knows, only if the target
// doesn't already have that aura applied by us.
bool ShouldCorruption(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    uint32 sid = KnownCorruption(ctx);
    if (!sid) return false;
    if (!ctx.bot.is_ready(sid)) return false;
    return ctx.bot.find_aura(sid, ctx.bot.victim()) == nullptr;
}
void DoCorruption(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 sid = KnownCorruption(ctx))
        e.cast(sid, ctx.bot.victim());
}

// Immolate as a refresh-aware debuff (overrides the basic spell rule above
// when defining the rule entry). Avoids re-rolling the DoT every GCD.
bool ShouldImmolateDoT(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(IMMOLATE)) return false;
    if (!ctx.bot.is_ready(IMMOLATE)) return false;
    return ctx.bot.find_aura(IMMOLATE, ctx.bot.victim()) == nullptr;
}
void DoImmolateDoT(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(IMMOLATE, ctx.bot.victim());
}

// Agony as a refresh-aware debuff (same reason as Immolate above).
bool ShouldAgonyDoT(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(AGONY)) return false;
    if (!ctx.bot.is_ready(AGONY)) return false;
    return ctx.bot.find_aura(AGONY, ctx.bot.victim()) == nullptr;
}
void DoAgonyDoT(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(AGONY, ctx.bot.victim());
}

BASELINE_DEBUFF_RULE(CurseOfWeakness, CURSE_OF_WEAKNESS)

// Fear as emergency CC: 2+ enemies AND we're at ≤50% HP. The baseline
// version is intentionally conservative — Fear breaks on damage and the
// baseline bot has no follow-up CC, so we only burn it when it actually
// buys breathing room.
bool ShouldFearEmergency(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FEAR)) return false;
    if (!ctx.bot.is_ready(FEAR)) return false;
    if (ctx.bot.hp_pct() > 50) return false;
    return ctx.bot.enemies_within(15.0f) >= 2;
}
void DoFearEmergency(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FEAR, ctx.bot.victim());
}

// Drain Life as emergency heal channel (L9). Channels for ~5s, locks the
// bot in place — only fire when we're genuinely in trouble AND no other
// defensive is ready (Unending Resolve burned or not yet learned).
// Threshold is ≤50% HP per spec.
bool ShouldDrainLifeEmergency(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DRAIN_LIFE)) return false;
    if (!ctx.bot.is_ready(DRAIN_LIFE)) return false;
    if (ctx.bot.hp_pct() > 50) return false;
    // Prefer Unending Resolve if it's available — it's instant, off-GCD,
    // and doesn't pin the bot in a channel. Only channel Drain Life when
    // UR is unavailable (not learned yet, or on cooldown).
    if (ctx.bot.knows_spell(UNENDING_RESOLVE) && ctx.bot.is_ready(UNENDING_RESOLVE))
        return false;
    return true;
}
void DoDrainLifeEmergency(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DRAIN_LIFE, ctx.bot.victim());
}

// Summon Imp: skip when bot already has a live pet. Without this
// the rule fires every tick at OOC walking-around, server processes
// a dismiss+resummon cycle and the pet ends up perpetually getting
// re-summoned (also burns 60% of the bot's mana repeatedly).
bool ShouldSummonImp(ApPredicateContext const& ctx)
{
    if (ctx.bot.has_pet()) return false;
    return ctx.bot.knows_spell(SUMMON_IMP) && ctx.bot.is_ready(SUMMON_IMP);
}
void DoSummonImp(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(SUMMON_IMP, ObjectGuid::Empty);
}

BASELINE_DEFENSIVE_RULE(UnendingResolve, UNENDING_RESOLVE, 30)

// Rule order (per spec):
//   1. Unending Resolve   — panic CD ≤30%
//   2. Drain Life         — emergency self-heal ≤50% when UR unavailable
//   3. Summon Imp         — pet maintenance (no live pet)
//   4. Corruption (146739 preferred, 172 fallback) — DoT
//   5. Immolate (348)     — DoT
//   6. Agony (980)        — DoT
//   7. Curse of Weakness  — debuff
//   8. Fear (emergency)   — ≥2 enemies AND ≤50% HP
//   9. Shadow Bolt        — filler
//  10. Auto attack
ApRule const baseline_warlock_kRules[] = {
    { ShouldUnendingResolve,    DoUnendingResolve,    "Unending Resolve (<30%)"        },
    { ShouldDrainLifeEmergency, DoDrainLifeEmergency, "Drain Life (emergency heal)"    },
    { ShouldSummonImp,          DoSummonImp,          "Summon Imp (no pet)"            },
    { ShouldCorruption,         DoCorruption,         "Corruption (DoT)"               },
    { ShouldImmolateDoT,        DoImmolateDoT,        "Immolate (DoT)"                 },
    { ShouldAgonyDoT,           DoAgonyDoT,           "Agony (DoT)"                    },
    { ShouldCurseOfWeakness,    DoCurseOfWeakness,    "Curse of Weakness (debuff)"     },
    { ShouldFearEmergency,      DoFearEmergency,      "Fear (emergency CC)"            },
    { ShouldShadowBolt,         DoShadowBolt,         "Shadow Bolt"                    },
    { AlwaysInCombat,           DoAutoAttack,         "Auto attack"                    },
};

} // anonymous

void RegisterApl_Baseline_Warlock()
{
    RegisterRotation(CLASS_WARLOCK, 0, ApRotation{baseline_warlock_kRules});
}

} // namespace Playerbot::Combat
