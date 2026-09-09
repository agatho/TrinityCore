// Apl_Baseline_Warlock.cpp - baseline rotation for class CLASS_WARLOCK
// (spec=0), used below level 10 and as the fallback when no spec rotation
// is registered. Extracted from the monolithic Apl_Baseline.cpp on the
// split refactor; future edits go here exclusively. See
// Apl_Baseline_Common.h for the shared helpers + rule macros.
//
// ---- Validated IDs (SkillLineAbility class baseline, WoW 12.1.0.69587) --
//   686     Shadow Bolt                 L1 filler
//   172     Corruption                  L2 DoT (cast id)
//   146739  Corruption                  periodic aura applied by 172
//   688     Summon Imp                  L3 pet
//   104773  Unending Resolve            L4 defensive, 40% DR
//   5782    Fear                        L5 CC
//   702     Curse of Weakness           L6 debuff
//   234153  Drain Life                  L9 emergency self-heal channel
//
// ---- Skipped spells (and why) -------------------------------------------
//   - Immolate (348) and Agony (980): Destruction spec spell / Affliction
//     talent in 12.1, not class baseline - the spec rotations own them.
//   - Create Healthstone (6201, L7): out-of-combat item creation, not a
//     rotation slot.
//   - Summon Voidwalker (697, L10+) and later pets: pet choice lives in
//     State_Idle's ooc pet-summon rule; the baseline only backstops the Imp.
//
// To audit coverage:
//   python src/modules/PlayerbotV2/tools/baseline_coverage_audit.py

#include "Apl_Baseline_Common.h"

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::HasLiveTarget;
using ::Playerbot::Combat::baseline_common::AlwaysInCombat;
using ::Playerbot::Combat::baseline_common::DoAutoAttack;

constexpr uint32 SHADOW_BOLT        = 686;          // L1 filler
constexpr uint32 CORRUPTION         = 172;          // L2 DoT (cast id)
constexpr uint32 CORRUPTION_DOT     = 146739;       // periodic aura applied by 172
constexpr uint32 FEAR               = 5782;         // L5 CC
constexpr uint32 SUMMON_IMP         = 688;          // L3 pet
constexpr uint32 CURSE_OF_WEAKNESS  = 702;          // L6 debuff
constexpr uint32 DRAIN_LIFE         = 234153;       // L9 emergency self-heal channel
constexpr uint32 UNENDING_RESOLVE   = 104773;       // L4 defensive

BASELINE_SPELL_RULE(ShadowBolt,  SHADOW_BOLT)

// Corruption: BASELINE_DEBUFF_RULE semantics, but the cast (172) applies a
// separately-numbered periodic aura (146739), so check that aura on the
// victim (and the cast id as a fallback) rather than re-rolling the DoT
// every GCD.
bool ShouldCorruption(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CORRUPTION)) return false;
    if (!ctx.bot.is_ready(CORRUPTION)) return false;
    if (ctx.bot.find_aura(CORRUPTION_DOT, ctx.bot.victim())) return false;
    return ctx.bot.find_aura(CORRUPTION, ctx.bot.victim()) == nullptr;
}
void DoCorruption(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CORRUPTION, ctx.bot.victim());
}

BASELINE_DEBUFF_RULE(CurseOfWeakness, CURSE_OF_WEAKNESS)

// Fear as emergency CC: 2+ enemies AND we're at <=50% HP. The baseline
// version is intentionally conservative - Fear breaks on damage and the
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
// bot in place - only fire when we're genuinely in trouble AND no other
// defensive is ready (Unending Resolve burned or not yet learned).
// Threshold is <=50% HP per spec.
bool ShouldDrainLifeEmergency(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DRAIN_LIFE)) return false;
    if (!ctx.bot.is_ready(DRAIN_LIFE)) return false;
    if (ctx.bot.hp_pct() > 50) return false;
    // Prefer Unending Resolve if it's available - it's instant, off-GCD,
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
//   1. Unending Resolve   - panic CD <=30%
//   2. Drain Life         - emergency self-heal <=50% when UR unavailable
//   3. Summon Imp         - pet maintenance (no live pet)
//   4. Corruption (172)   - DoT (aura 146739)
//   5. Curse of Weakness  - debuff
//   6. Fear (emergency)   - >=2 enemies AND <=50% HP
//   7. Shadow Bolt        - filler
//   8. Auto attack
ApRule const baseline_warlock_kRules[] = {
    { ShouldUnendingResolve,    DoUnendingResolve,    "Unending Resolve (<30%)"        },
    { ShouldDrainLifeEmergency, DoDrainLifeEmergency, "Drain Life (emergency heal)"    },
    { ShouldSummonImp,          DoSummonImp,          "Summon Imp (no pet)"            },
    { ShouldCorruption,         DoCorruption,         "Corruption (DoT)"               },
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
