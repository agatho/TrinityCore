// Apl_Baseline_Warrior.cpp — baseline rotation for class CLASS_WARRIOR (spec=0). Extracted from the monolithic Apl_Baseline.cpp on the split refactor; future edits go
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

constexpr uint32 CHARGE         = 100;
constexpr uint32 SLAM           = 1464;
constexpr uint32 BATTLE_SHOUT   = 6673;        // DB2 SpellLevel=10 — gated on knows_spell, harmless when unknown
constexpr uint32 PUMMEL         = 6552;
constexpr uint32 VICTORY_RUSH   = 34428;
constexpr uint32 HAMSTRING      = 1715;
constexpr uint32 EXECUTE        = 5308;        // DB2 SpellLevel=10 — kept for early talent grants; gated on knows_spell
constexpr uint32 SHIELD_SLAM    = 23922;       // L5 Protection in spellbook audit
constexpr uint32 BERSERKER_RAGE = 18499;       // L1 panic — immunity to fear/sap/incap (DB2 SpellLevel=12 — gated)
constexpr uint32 SHIELD_BLOCK   = 2565;        // L6 — active block mitigation, requires shield equipped
constexpr uint32 TAUNT          = 355;         // L8 — taunt; included even on non-tank baseline since it's cheap and useful when a mob targets a healer/caster
constexpr uint32 WHIRLWIND      = 1680;        // L9 — AoE melee swing (≥2 enemies in 8y)

BASELINE_SPELL_RULE(Charge, CHARGE)
BASELINE_INTERRUPT_RULE(Pummel, PUMMEL)
BASELINE_SPELL_RULE(Execute, EXECUTE)
BASELINE_SPELL_RULE(Slam, SLAM)
BASELINE_SPELL_RULE(ShieldSlam, SHIELD_SLAM)
// Battle Shout is a self-buff aura — must use SELFBUFF (find_aura gate) so we
// re-cast only after it falls off, not every tick. Using BASELINE_SELF_RULE
// here was a bug that burned a GCD every tick the bot was idle.
BASELINE_SELFBUFF_RULE(BattleShout, BATTLE_SHOUT)
BASELINE_SPELL_RULE(VictoryRush, VICTORY_RUSH)
// Whirlwind: baseline AoE. 8y radius, hits all nearby enemies. Gate on
// ≥2 targets (not 3) since baseline warriors don't have Cleave/Sweeping
// Strikes and Whirlwind is their only AoE option pre-spec.
BASELINE_AOE_RULE(Whirlwind, WHIRLWIND, 8.0f, 2)

// Berserker Rage: panic CD. Grants immunity to fear/sap/incapacitate
// and a brief damage buff. L1 baseline so always available to fresh
// warriors. Fires below 35% HP with a nearby enemy — no alternation
// because Victory Rush requires a recent kill (not a true panic CD).
bool ShouldBerserkerRage(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (ctx.bot.hp_pct() >= 35) return false;
    if (ctx.bot.enemies_within(40.0f) == 0) return false;
    return ctx.bot.knows_spell(BERSERKER_RAGE) && ctx.bot.is_ready(BERSERKER_RAGE);
}
void DoBerserkerRage(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(BERSERKER_RAGE, ObjectGuid::Empty);
}

// Shield Block: active mitigation CD (L6 baseline). Requires a shield
// equipped — we can't reliably probe slot contents from the snapshot,
// but the cast will silently fail if no shield is on and is_ready gates
// the CD. Fire at ≤70% HP so it overlaps real incoming damage instead
// of being wasted on the first trash mob. No aura check needed: the
// CD itself (~16s) is longer than the buff (~6s), so is_ready prevents
// double-emit anyway.
bool ShouldShieldBlock(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (ctx.bot.hp_pct() > 70) return false;
    if (!ctx.bot.knows_spell(SHIELD_BLOCK)) return false;
    if (!ctx.bot.is_ready(SHIELD_BLOCK)) return false;
    if (ctx.bot.has_aura(SHIELD_BLOCK)) return false;
    return true;
}
void DoShieldBlock(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(SHIELD_BLOCK, ObjectGuid::Empty);
}

// Taunt: a tank tool, but kept in the baseline because pre-L10 dungeon
// runs frequently feature undirected warriors who CAN peel a caster off
// the healer. Cheap (no GCD, short CD), gated on an enemy not currently
// targeting us. The Prot spec rotation has the same idiom (untaunted_enemy).
bool ShouldTaunt(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(TAUNT)) return false;
    if (!ctx.bot.is_ready(TAUNT)) return false;
    return ctx.bot.untaunted_enemy(8.0f) != nullptr;
}
void DoTaunt(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.untaunted_enemy(8.0f))
        e.cast(TAUNT, t->guid);
}

// Hamstring: melee 50% slow. Fires whenever a target's adjacent and
// the bot has rage to spend; cheap reapply since the debuff stacks
// a fresh duration. is_ready gates GCD; the cast-emit dedup prevents
// re-emitting within ~1.5s, so we just lean on those for cadence.
bool ShouldHamstring(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HAMSTRING)) return false;
    if (!ctx.bot.is_ready(HAMSTRING)) return false;
    // Don't bother refreshing if the bot's already in melee with a
    // slowed target — track aura presence and skip while up.
    if (ctx.bot.find_aura(HAMSTRING, ctx.bot.victim())) return false;
    return ctx.bot.enemies_within(5.0f) > 0;
}
void DoHamstring(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HAMSTRING, ctx.bot.victim());
}

// Execute only when victim HP <= 20% (the rule's "below" trigger).
bool ShouldExecuteLow(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(EXECUTE)) return false;
    if (auto const* t = ctx.bot.victim_info())
        return t->max_hp > 0 && (t->hp * 100 / t->max_hp) <= 20;
    return false;
}

ApRule const baseline_warrior_kRules[] = {
    // 1) Panic / immunity
    { ShouldBerserkerRage,DoBerserkerRage,"Berserker Rage (<35% panic)"},
    // 2) Active defensive
    { ShouldShieldBlock,  DoShieldBlock,  "Shield Block (<=70% mit)"   },
    // 3) Self-buff maintenance (refreshes via SELFBUFF aura check)
    { ShouldBattleShout,  DoBattleShout,  "Battle Shout (self buff)"   },
    // 4) Gap closer
    { ShouldCharge,       DoCharge,       "Charge (gap close)"         },
    // 5) Interrupt (gated on victim casting interruptible)
    { ShouldPummel,       DoPummel,       "Pummel (interrupt)"         },
    // 6) Threat / peel — useful when a mob breaks off to a squishy
    { ShouldTaunt,        DoTaunt,        "Taunt (peel caster)"        },
    // 7) Slow
    { ShouldHamstring,    DoHamstring,    "Hamstring (melee slow)"     },
    // 8) Execute window
    { ShouldExecuteLow,   DoExecute,      "Execute (<=20% target)"     },
    // 9) Post-kill heal
    { ShouldVictoryRush,  DoVictoryRush,  "Victory Rush (post-kill)"   },
    // 10) AoE (>=2 enemies in 8y)
    { ShouldWhirlwind,    DoWhirlwind,    "Whirlwind (>=2 AoE)"        },
    // 11) Single-target damage
    { ShouldShieldSlam,   DoShieldSlam,   "Shield Slam"                },
    { ShouldSlam,         DoSlam,         "Slam (rage spender)"        },
    // 12) Auto-attack fallback
    { AlwaysInCombat,     DoAutoAttack,   "Auto attack"                },
};

} // anonymous

void RegisterApl_Baseline_Warrior()
{
    RegisterRotation(CLASS_WARRIOR, 0, ApRotation{baseline_warrior_kRules});
}

} // namespace Playerbot::Combat
