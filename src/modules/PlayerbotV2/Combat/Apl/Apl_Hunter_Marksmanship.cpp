// Marksmanship Hunter - WoW 12.0 enterprise rotation. Pure ranged DPS with
// Aimed Shot as the heavy hitter, Rapid Fire as a focus generator + spike,
// Trueshot burst window, and Precise Shots procs spent on Arcane Shot.
// Steady Shot is the cast-while-moving filler.
//
// Layered survival: Aspect of the Turtle -> Exhilaration -> Survival of the
// Fittest -> Disengage -> Feign Death. Group utility: Misdirection (tank
// threat redirect), Aspect of the Wild (group crit), Primal Rage
// (Bloodlust). CC: Counter Shot, Tar Trap, Binding Shot. Major CDs:
// Trueshot, Volley (talent ground AoE), Wailing Arrow (talent silence +
// damage), Salvo (talent — auto-explosive shot proc), Death Chakram
// (talent — focus gen + AoE), Harrier's Cry (L48 hero-talent group haste).
//
// Validated against wago.tools SpellName.csv 2026-05-27. Every ID below
// resolves to its expected name.
//
// Skipped spec spells (not rotation-relevant — intentional omissions):
//   * Spotter's Mark (1219616) — passive proc that buffs the next Aimed
//                                Shot; no active cast surface.
//   * Eagle Eye     (    6197) — vanilla scout-vision spell, removes bot
//                                control and has no combat effect.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated) ----
constexpr uint32 AIMED_SHOT          = 19434;
constexpr uint32 ARCANE_SHOT         = 185358;
constexpr uint32 RAPID_FIRE          = 257044;
constexpr uint32 STEADY_SHOT         = 56641;
constexpr uint32 KILL_SHOT_MM        = 53351;
constexpr uint32 TRUESHOT            = 288613;
constexpr uint32 PRECISE_SHOTS       = 260242;       // proc — buffs Arcane Shot
constexpr uint32 VOLLEY               = 260243;      // talent — ground AoE
constexpr uint32 WAILING_ARROW        = 392060;      // talent — AoE silence + damage
constexpr uint32 SALVO                = 400456;      // talent — auto explosive
constexpr uint32 EXPLOSIVE_SHOT       = 212431;      // talent — AoE bomb
constexpr uint32 DEATH_CHAKRAM        = 375891;      // talent — focus + AoE
constexpr uint32 SERPENT_STING        = 271788;      // MM Serpent Sting (different id from SV)
constexpr uint32 MULTI_SHOT_MM        = 257620;
constexpr uint32 HUNTERS_MARK         = 257284;
constexpr uint32 COUNTER_SHOT         = 147362;
constexpr uint32 MISDIRECTION         = 34477;
constexpr uint32 ASPECT_TURTLE        = 186265;
constexpr uint32 EXHILARATION         = 109304;
constexpr uint32 SURVIVAL_FITTEST     = 264735;
constexpr uint32 DISENGAGE            = 781;
constexpr uint32 MEND_PET             = 136;
constexpr uint32 REVIVE_PET           = 982;
constexpr uint32 FEIGN_DEATH          = 5384;
constexpr uint32 ASPECT_WILD          = 193530;
constexpr uint32 PRIMAL_RAGE          = 264667;
constexpr uint32 TAR_TRAP             = 187698;
constexpr uint32 BINDING_SHOT         = 109248;
constexpr uint32 LONE_WOLF            = 155228;       // (passive — no cast)
constexpr uint32 STEADY_FOCUS         = 193533;      // proc
constexpr uint32 HARRIERS_CRY         = 466904;      // L48 hero-talent self/raid haste CD

constexpr uint8 POWER_FOCUS_IDX = 2;

bool HasLiveTarget(ApPredicateContext const& ctx)
{
    return !ctx.bot.victim().IsEmpty();
}

bool BossLikeTargetEngaged(ApPredicateContext const& ctx)
{
    constexpr int32 kBossHpThreshold = 5'000'000;
    NearbyUnit const* t = ctx.bot.victim_info();
    if (t && t->max_hp >= kBossHpThreshold) return true;
    for (auto const& a : ctx.bot.raw().combat.attackers)
        if (a.max_hp >= kBossHpThreshold) return true;
    return false;
}

bool BotHasSatedDebuff(ApPredicateContext const& ctx)
{
    constexpr uint32 SATED_DEBUFF           = 57724;
    constexpr uint32 TEMPORAL_DISPL_DEBUFF  = 80354;
    constexpr uint32 INSANITY_HUNTER_DEBUFF = 95809;
    constexpr uint32 FATIGUED_DEBUFF        = 264689;
    return ctx.bot.has_aura(SATED_DEBUFF)
        || ctx.bot.has_aura(TEMPORAL_DISPL_DEBUFF)
        || ctx.bot.has_aura(INSANITY_HUNTER_DEBUFF)
        || ctx.bot.has_aura(FATIGUED_DEBUFF);
}

bool TargetExecuteRange(ApPredicateContext const& ctx)
{
    NearbyUnit const* t = ctx.bot.victim_info();
    if (!t || t->max_hp <= 0 || t->hp <= 0) return false;
    return (t->hp * 100) / t->max_hp <= 20;
}

int32 FocusVal(ApPredicateContext const& ctx) { return ctx.bot.power(POWER_FOCUS_IDX); }

// ---- Pet maintenance ----
bool ShouldRevivePet(ApPredicateContext const& ctx)
{
    if (ctx.bot.has_aura(LONE_WOLF)) return false;     // Lone Wolf — pet not used
    // See note on the BM version: only resurrect when there's actually a
    // dead pet to bring back. pet_guid().IsEmpty() means the bot never
    // tamed/summoned one, so Revive Pet is a no-op that would otherwise
    // starve the entire rotation by re-firing every tick.
    if (ctx.bot.pet_guid().IsEmpty()) return false;
    if (ctx.bot.has_pet()) return false;
    if (!ctx.bot.knows_spell(REVIVE_PET)) return false;
    return ctx.bot.is_ready(REVIVE_PET);
}
void DoRevivePet(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(REVIVE_PET); }

bool ShouldMendPet(ApPredicateContext const& ctx)
{
    if (!ctx.bot.has_pet()) return false;
    if (!ctx.bot.knows_spell(MEND_PET)) return false;
    if (!ctx.bot.is_ready(MEND_PET)) return false;
    return ctx.bot.pet_hp_pct() <= 50;
}
void DoMendPet(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(MEND_PET); }

// ---- Survival ----
bool ShouldAspectTurtle(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ASPECT_TURTLE)) return false;
    if (!ctx.bot.is_ready(ASPECT_TURTLE)) return false;
    // PvP: bump panic threshold so the immunity catches the burst window.
    const int32 threshold = ctx.pvp.under_player_attack ? 40 : 20;
    return ctx.bot.hp_pct() <= threshold;
}
void DoAspectTurtle(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ASPECT_TURTLE); }

bool ShouldSurvivalFittest(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SURVIVAL_FITTEST)) return false;
    if (!ctx.bot.is_ready(SURVIVAL_FITTEST)) return false;
    return ctx.bot.hp_pct() <= 60;
}
void DoSurvivalFittest(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SURVIVAL_FITTEST); }

bool ShouldExhilaration(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(EXHILARATION)) return false;
    if (!ctx.bot.is_ready(EXHILARATION)) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoExhilaration(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(EXHILARATION); }

bool ShouldDisengage(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DISENGAGE)) return false;
    if (!ctx.bot.is_ready(DISENGAGE)) return false;
    // "Kite 2+ melee": leap back only when 2+ enemies are actually ATTACKING
    // the bot in melee range — not merely near it. enemies_within(8) also
    // counted the PET's targets, making a full-HP hunter leap away from its
    // pet's fight every cooldown without ever engaging (see BeastMastery).
    return ctx.bot.melee_attackers_within(8.0f) >= 2;
}
void DoDisengage(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DISENGAGE); }

bool ShouldFeignDeath(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(FEIGN_DEATH)) return false;
    if (!ctx.bot.is_ready(FEIGN_DEATH)) return false;
    return ctx.bot.hp_pct() <= 30 && ctx.bot.attackers_count() >= 1;
}
void DoFeignDeath(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FEIGN_DEATH); }

// ---- Group utility ----
bool ShouldMisdirection(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(MISDIRECTION)) return false;
    if (!ctx.bot.is_ready(MISDIRECTION)) return false;
    auto const* tank = ctx.group.tank();
    return tank && tank->online && tank->guid != ctx.bot.raw().guid;
}
void DoMisdirection(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* tank = ctx.group.tank())
        e.cast(MISDIRECTION, tank->guid);
}

bool ShouldAspectWild(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ASPECT_WILD)) return false;
    if (!ctx.bot.is_ready(ASPECT_WILD)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoAspectWild(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ASPECT_WILD); }

bool ShouldPrimalRage(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.has_pet()) return false;
    if (!ctx.bot.knows_spell(PRIMAL_RAGE)) return false;
    if (!ctx.bot.is_ready(PRIMAL_RAGE)) return false;
    if (BotHasSatedDebuff(ctx)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoPrimalRage(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(PRIMAL_RAGE); }

// ---- Interrupt / CC ----
bool ShouldCounterShot(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(COUNTER_SHOT)) return false;
    if (!ctx.bot.is_ready(COUNTER_SHOT)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    return ctx.bot.kick_target(pvp, 40.0f) != nullptr;
}
void DoCounterShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 40.0f))
        e.cast(COUNTER_SHOT, c->guid);
}

bool ShouldBindingShot(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BINDING_SHOT)) return false;
    if (!ctx.bot.is_ready(BINDING_SHOT)) return false;
    return ctx.bot.enemies_within(15.0f) >= 3;
}
void DoBindingShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(BINDING_SHOT, v->x, v->y, v->z);
    else
        e.cast(BINDING_SHOT);
}

bool ShouldTarTrap(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(TAR_TRAP)) return false;
    if (!ctx.bot.is_ready(TAR_TRAP)) return false;
    return ctx.bot.enemies_within(15.0f) >= 2 && ctx.bot.hp_pct() <= 50;
}
void DoTarTrap(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(TAR_TRAP, v->x, v->y, v->z);
    else
        e.cast(TAR_TRAP);
}

// ---- Major offensive cooldowns ----
bool ShouldTrueshot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(TRUESHOT)) return false;
    if (!ctx.bot.is_ready(TRUESHOT)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoTrueshot(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TRUESHOT); }

// Harrier's Cry (466904, L48 hero-talent). Burst CD that buffs the
// hunter (and per the Sentinel/Dark Ranger hero-tree wording, allies
// near her) with attack speed for ~10s. Fire it on a boss-like target
// so the CD isn't wasted on trash. Gate on alive target + readiness.
bool ShouldHarriersCry(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HARRIERS_CRY)) return false;
    if (!ctx.bot.is_ready(HARRIERS_CRY)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoHarriersCry(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HARRIERS_CRY); }

bool ShouldVolley(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(VOLLEY)) return false;
    if (!ctx.bot.is_ready(VOLLEY)) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(40.0f) >= 2;
}
void DoVolley(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(VOLLEY, v->x, v->y, v->z);
    else
        e.cast(VOLLEY);
}

bool ShouldWailingArrow(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(WAILING_ARROW)) return false;
    if (!ctx.bot.is_ready(WAILING_ARROW)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    return ctx.bot.enemies_within(20.0f) >= 2 ||
           ctx.bot.kick_target(pvp, 40.0f) != nullptr;
}
void DoWailingArrow(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(WAILING_ARROW, ctx.bot.victim());
}

bool ShouldSalvo(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SALVO)) return false;
    if (!ctx.bot.is_ready(SALVO)) return false;
    return ctx.bot.enemies_within(40.0f) >= 2;
}
void DoSalvo(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SALVO); }

bool ShouldExplosiveShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(EXPLOSIVE_SHOT)) return false;
    if (!ctx.bot.is_ready(EXPLOSIVE_SHOT)) return false;
    return ctx.bot.enemies_within(40.0f) >= 2;
}
void DoExplosiveShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(EXPLOSIVE_SHOT, ctx.bot.victim());
}

bool ShouldDeathChakram(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEATH_CHAKRAM)) return false;
    if (!ctx.bot.is_ready(DEATH_CHAKRAM)) return false;
    return true;
}
void DoDeathChakram(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DEATH_CHAKRAM, ctx.bot.victim());
}

// ---- Execute ----
bool ShouldKillShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(KILL_SHOT_MM)) return false;
    if (!ctx.bot.is_ready(KILL_SHOT_MM)) return false;
    return TargetExecuteRange(ctx);
}
void DoKillShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(KILL_SHOT_MM, ctx.bot.victim());
}

// ---- Damage rotation ----
bool ShouldSerpentSting(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SERPENT_STING)) return false;
    AuraEntry const* a = ctx.bot.find_aura(SERPENT_STING, ctx.bot.victim());
    return !a || a->remaining.count() <= 4500;
}
void DoSerpentSting(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SERPENT_STING, ctx.bot.victim());
}

bool ShouldRapidFire(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RAPID_FIRE)) return false;
    return ctx.bot.is_ready(RAPID_FIRE);
}
void DoRapidFire(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RAPID_FIRE, ctx.bot.victim());
}

bool ShouldArcaneShotProc(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ARCANE_SHOT)) return false;
    return ctx.bot.has_aura(PRECISE_SHOTS);
}
void DoArcaneShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ARCANE_SHOT, ctx.bot.victim());
}

bool ShouldMultiShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MULTI_SHOT_MM)) return false;
    if (FocusVal(ctx) < 40) return false;   // real cost — 20 let it claim ticks it couldn't pay for
    // attackers_count (mobs actually fighting us), not "2 enemies anywhere
    // within 40y" — the old gate made MM bots Multi-Shot single targets
    // all through any populated camp.
    return ctx.aoe_preference || ctx.bot.attackers_count() >= 2;
}
void DoMultiShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MULTI_SHOT_MM, ctx.bot.victim());
}

bool ShouldAimedShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(AIMED_SHOT)) return false;
    if (!ctx.bot.is_ready(AIMED_SHOT)) return false;
    if (FocusVal(ctx) < 35) return false;
    // Don't cast Aimed Shot while moving — it's a 2.5s channel that gets
    // interrupted. Steady Shot fills the gap.
    if (ctx.bot.is_moving()) return false;
    return true;
}
void DoAimedShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(AIMED_SHOT, ctx.bot.victim());
}

bool ShouldSteadyShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(STEADY_SHOT)) return false;
    // is_ready folds in is_casting + GCD — without it the rule re-emitted
    // mid-cast and bounced off SPELL_FAILED_SPELL_IN_PROGRESS.
    return ctx.bot.is_ready(STEADY_SHOT);
}
void DoSteadyShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(STEADY_SHOT, ctx.bot.victim());
}

// Hunter's Mark — baseline ranged-damage-taken debuff (5%). Granted
// around L7 and persists across all three specs. One cast per target.
bool ShouldHuntersMark(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HUNTERS_MARK)) return false;
    if (!ctx.bot.is_ready(HUNTERS_MARK)) return false;
    return ctx.bot.find_aura(HUNTERS_MARK, ctx.bot.victim()) == nullptr;
}
void DoHuntersMark(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HUNTERS_MARK, ctx.bot.victim());
}

bool AlwaysInCombat(ApPredicateContext const& ctx) { return ctx.bot.in_combat(); }
void DoAutoAttack(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    ObjectGuid t = ctx.bot.victim();
    if (t.IsEmpty()) t = ctx.bot.current_target();
    // Retaliate fallback (2026-06-17): in combat with no victim/target the bot
    // looped "Engage auto attack" as a no-op while taking damage (the CombatLoop
    // wedge cluster). Engage whatever is meleeing us (attackers are adjacent and
    // reachable), else the nearest visible enemy, so combat actually resolves.
    // Strictly additive: only runs when no target was already selected.
    if (t.IsEmpty())
        for (auto const& u : ctx.bot.attackers())
            if (u.hp > 0) { t = u.guid; break; }
    if (t.IsEmpty())
        for (auto const& u : ctx.bot.nearby_enemies())
            if (u.hp > 0 && u.in_los) { t = u.guid; break; }
    if (!t.IsEmpty()) e.start_attack(t);
}

ApRule const kRules[] = {
    { ShouldRevivePet,      DoRevivePet,      "Revive Pet"                  },
    { ShouldMendPet,        DoMendPet,        "Mend Pet (<=50%)"            },
    { ShouldAspectTurtle,   DoAspectTurtle,   "Aspect of the Turtle (<=20%)"},
    { ShouldSurvivalFittest,DoSurvivalFittest,"Survival of the Fittest"     },
    { ShouldExhilaration,   DoExhilaration,   "Exhilaration (<=50%)"        },
    { ShouldDisengage,      DoDisengage,      "Disengage (kite 2+ melee)"   },
    { ShouldFeignDeath,     DoFeignDeath,     "Feign Death (drop aggro)"    },
    { ShouldMisdirection,   DoMisdirection,   "Misdirection (tank threat)"  },
    { ShouldCounterShot,    DoCounterShot,    "Counter Shot (interrupt)"    },
    { ShouldBindingShot,    DoBindingShot,    "Binding Shot (3+ AoE)"       },
    { ShouldTarTrap,        DoTarTrap,        "Tar Trap (slow)"             },
    { ShouldAspectWild,     DoAspectWild,     "Aspect of the Wild (boss)"   },
    { ShouldPrimalRage,     DoPrimalRage,     "Primal Rage (Bloodlust)"     },
    { ShouldTrueshot,       DoTrueshot,       "Trueshot (boss)"             },
    { ShouldHarriersCry,    DoHarriersCry,    "Harrier's Cry (boss haste)"  },
    { ShouldDeathChakram,   DoDeathChakram,   "Death Chakram"               },
    { ShouldWailingArrow,   DoWailingArrow,   "Wailing Arrow"               },
    { ShouldSalvo,          DoSalvo,          "Salvo (2+ AoE)"              },
    { ShouldVolley,         DoVolley,         "Volley (2+ AoE)"             },
    { ShouldExplosiveShot,  DoExplosiveShot,  "Explosive Shot (2+ AoE)"     },
    { ShouldHuntersMark,    DoHuntersMark,    "Hunter's Mark (debuff)"      },
    { ShouldKillShot,       DoKillShot,       "Kill Shot (<=20%)"           },
    { ShouldRapidFire,      DoRapidFire,      "Rapid Fire"                  },
    { ShouldSerpentSting,   DoSerpentSting,   "Serpent Sting (refresh)"     },
    { ShouldArcaneShotProc, DoArcaneShot,     "Arcane Shot (Precise Shots)" },
    { ShouldMultiShot,      DoMultiShot,      "Multi-Shot (2+ AoE)"         },
    { ShouldAimedShot,      DoAimedShot,      "Aimed Shot"                  },
    { ShouldSteadyShot,     DoSteadyShot,     "Steady Shot (filler)"        },
    { AlwaysInCombat,       DoAutoAttack,     "Engage auto attack"          },
};

} // anonymous

void RegisterApl_Hunter_Marksmanship()
{
    constexpr uint32 SPEC_HUNTER_MARKSMANSHIP = 254;
    RegisterRotation(CLASS_HUNTER, SPEC_HUNTER_MARKSMANSHIP, ApRotation{kRules});
}

} // namespace Playerbot::Combat
