// Marksmanship Hunter - WoW 12.1.0.69587 (Midnight) enterprise rotation.
// Pure ranged, PETLESS DPS (MM hunts with a Spotting Eagle since 11.1 - no
// beast companion) with Aimed Shot as the heavy hitter, Rapid Fire as a
// focus generator + spike, Explosive Shot on cooldown (Precision Detonation),
// Trueshot burst window, Precise Shots procs spent on Arcane Shot, and
// Kill Shot (or Black Arrow when the Dark Ranger hero talent replaces it)
// as the execute. Steady Shot is the cast-while-moving filler.
//
// Layered survival: Aspect of the Turtle -> Exhilaration -> Survival of the
// Fittest -> Disengage -> Feign Death. Group utility: Misdirection (tank
// threat redirect), Harrier's Cry (L48 spec spell: raid haste / Bloodlust
// equivalent, Sated-gated). CC: Counter Shot, Intimidation (eagle stun,
// interrupt fallback), Tar Trap, Binding Shot. Major CDs: Trueshot, Volley
// (ground AoE, also grants Trick Shots).
//
// Validated spell IDs (WoW 12.1.0.69587, kit + SpellName.csv):
//   19434  Aimed Shot       | 185358 Arcane Shot      | 257044 Rapid Fire
//   56641  Steady Shot      | 53351  Kill Shot        | 466930 Black Arrow
//   288613 Trueshot         | 260243 Volley           | 212431 Explosive Shot
//   257620 Multi-Shot       | 466904 Harrier's Cry    | 257284 Hunter's Mark
//   147362 Counter Shot     | 474421 Intimidation(MM) | 109248 Binding Shot
//   187698 Tar Trap         | 34477  Misdirection     | 264735 Survival o.t.Fit.
//   186265 Aspect of Turtle | 109304 Exhilaration     | 781    Disengage
//   5384   Feign Death      | 136    Mend Pet         | 982    Revive Pet
//   260242 Precise Shots (buff aura; talent passive is 260240)
//
// Skipped (deliberate, 12.1):
//   * Salvo            ( 400456) - PASSIVE in 12.1 (Volley applies Explosive
//                                  Shot); was wrongly cast as a spell.
//   * Wailing Arrow    ( 392060/355589) - no 12.1 learn path for MM (355589 is
//                                  the Shadowlands legendary-bow leftover).
//   * Death Chakram    ( 375891/325028) - covenant leftover, not learnable.
//   * Serpent Sting    ( 271788) - removed from MM.
//   * Aspect of the Wild (193530) - removed from the game.
//   * Primal Rage      ( 264667) - Ferocity PET ability; MM has no pet in 12.1.
//   * Lone Wolf        ( 155228) - not in the 12.1 MM kit (petless is baseline).
//   * Steady Focus     ( 193533) - passive, never cast.
//   * Tranquilizing Shot (19801) - class talent not in either curated build.
//   * Eagle Eye        (   6197) - scout-vision spell, removes bot control.
//   * Fetch: Eagle (1232995) / Air Superiority (470937) - loot / passive DR
//                                  granted by passives, no combat cast.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated against the 12.1 kit and
// SpellName.csv; see the header table) ----
constexpr uint32 AIMED_SHOT          = 19434;        // spec talent [R][M] (L10), 2.5s cast, 35 focus
constexpr uint32 ARCANE_SHOT         = 185358;       // baseline L2 - Precise Shots spender
constexpr uint32 RAPID_FIRE          = 257044;       // spec talent [R][M], 16s cd, focus generator
constexpr uint32 STEADY_SHOT         = 56641;        // baseline L1 filler, usable while moving
constexpr uint32 KILL_SHOT_MM        = 53351;        // spec talent [R] (L42), <20% execute
constexpr uint32 BLACK_ARROW         = 466930;       // Dark Ranger active; 466932 [R] REPLACES Kill Shot with it
constexpr uint32 TRUESHOT            = 288613;       // spec talent [R][M], 120s cd burst
constexpr uint32 PRECISE_SHOTS       = 260242;       // buff aura after Aimed Shot (talent passive 260240)
constexpr uint32 VOLLEY               = 260243;      // spec talent [R][M] - ground AoE, 45s cd
constexpr uint32 EXPLOSIVE_SHOT       = 212431;      // spec talent [R][M] - 30s cd, 20 focus, ST + AoE
constexpr uint32 MULTI_SHOT_MM        = 257620;      // spec spell L10, 30 focus
constexpr uint32 HUNTERS_MARK         = 257284;
constexpr uint32 COUNTER_SHOT         = 147362;      // class talent [R][M] (L18)
constexpr uint32 INTIMIDATION_MM      = 474421;      // class talent [R][M] - MM variant (Spotting Eagle stun, 40y)
constexpr uint32 MISDIRECTION         = 34477;
constexpr uint32 ASPECT_TURTLE        = 186265;
constexpr uint32 EXHILARATION         = 109304;
constexpr uint32 SURVIVAL_FITTEST     = 264735;
constexpr uint32 DISENGAGE            = 781;
constexpr uint32 MEND_PET             = 136;         // class baseline; inert for a petless MM (has_pet gate)
constexpr uint32 REVIVE_PET           = 982;         // class baseline; inert for a petless MM (pet_guid gate)
constexpr uint32 FEIGN_DEATH          = 5384;
constexpr uint32 TAR_TRAP             = 187698;      // class talent [R]
constexpr uint32 BINDING_SHOT         = 109248;      // class talent [R][M]
constexpr uint32 HARRIERS_CRY         = 466904;      // spec spell L48 - raid haste (Bloodlust-class, applies Sated)

constexpr uint8 POWER_FOCUS_IDX = 2;

constexpr int32 AIMED_SHOT_COST     = 35;
constexpr int32 MULTI_SHOT_COST     = 30;
constexpr int32 EXPLOSIVE_SHOT_COST = 20;
constexpr int32 BLACK_ARROW_COST    = 10;

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

int64 TargetHpPct(ApPredicateContext const& ctx)
{
    NearbyUnit const* t = ctx.bot.victim_info();
    if (!t || t->max_hp <= 0 || t->hp <= 0) return -1;
    return (int64_t(t->hp) * 100) / t->max_hp;
}

bool TargetExecuteRange(ApPredicateContext const& ctx)
{
    const int64 pct = TargetHpPct(ctx);
    return pct >= 0 && pct <= 20;
}

// Black Arrow window (466930): "Only usable on enemies above 80% health or
// below 20% health".
bool TargetBlackArrowWindow(ApPredicateContext const& ctx)
{
    const int64 pct = TargetHpPct(ctx);
    return pct >= 0 && (pct <= 20 || pct >= 80);
}

int32 FocusVal(ApPredicateContext const& ctx) { return ctx.bot.power(POWER_FOCUS_IDX); }

// ---- Pet maintenance ----
// MM is petless in 12.1 (Spotting Eagle). Both rules stay for the class
// baseline spells but self-gate on pet_guid()/has_pet(), so they are inert
// unless a pet somehow exists (e.g. spec swap mid-session).
bool ShouldRevivePet(ApPredicateContext const& ctx)
{
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

// Intimidation, MM variant (474421, [R][M]): the Spotting Eagle stuns the
// target for 5s at 40y - no pet required. Used as the interrupt fallback
// when Counter Shot is on cooldown (same slot Intimidation holds in BM).
bool ShouldIntimidation(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(INTIMIDATION_MM)) return false;
    if (!ctx.bot.is_ready(INTIMIDATION_MM)) return false;
    if (ctx.bot.is_ready(COUNTER_SHOT)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    return ctx.bot.kick_target(pvp, 40.0f) != nullptr;
}
void DoIntimidation(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 40.0f))
        e.cast(INTIMIDATION_MM, c->guid);
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

// Harrier's Cry (466904, L48 spec spell): "Increases haste by $s1% for all
// party and raid members for 40s ... Allies receiving this effect will
// become Sated". It is MM's Bloodlust, so it takes the Bloodlust gates:
// boss-like target only and never while the bot itself is Sated / Temporal
// Displacement / Fatigued (the cast would be wasted).
bool ShouldHarriersCry(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HARRIERS_CRY)) return false;
    if (!ctx.bot.is_ready(HARRIERS_CRY)) return false;
    if (BotHasSatedDebuff(ctx)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoHarriersCry(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HARRIERS_CRY); }

// Volley (260243, [R][M], 45s cd): 12.1 MM presses it on cooldown in single
// target too (it grants Trick Shots + Salvo's Explosive Shot spread and is
// a large chunk of damage on its own), so the only gate is readiness.
bool ShouldVolley(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(VOLLEY)) return false;
    return ctx.bot.is_ready(VOLLEY);
}
void DoVolley(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(VOLLEY, v->x, v->y, v->z);
    else
        e.cast(VOLLEY);
}

// Explosive Shot (212431, [R][M], 30s cd, 20 focus): single-target AND AoE
// button in 12.1 (Precision Detonation / Unstable Trigger build around it),
// so it fires whenever ready and affordable.
bool ShouldExplosiveShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(EXPLOSIVE_SHOT)) return false;
    if (!ctx.bot.is_ready(EXPLOSIVE_SHOT)) return false;
    return FocusVal(ctx) >= EXPLOSIVE_SHOT_COST;
}
void DoExplosiveShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(EXPLOSIVE_SHOT, ctx.bot.victim());
}

// ---- Execute ----
// Two-branch pattern (see Victory Rush / Impending Victory in Arms): the
// Dark Ranger passive 466932 [R] REPLACES Kill Shot with Black Arrow
// (466930). When the bot knows Black Arrow, fire it in its <20% / >80%
// window and keep Kill Shot silent; non-hero bots keep the plain execute.
bool ShouldBlackArrow(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BLACK_ARROW)) return false;
    if (!ctx.bot.is_ready(BLACK_ARROW)) return false;
    if (FocusVal(ctx) < BLACK_ARROW_COST) return false;
    return TargetBlackArrowWindow(ctx);
}
void DoBlackArrow(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BLACK_ARROW, ctx.bot.victim());
}

bool ShouldKillShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(BLACK_ARROW)) return false;   // replaced by Black Arrow
    if (!ctx.bot.knows_spell(KILL_SHOT_MM)) return false;
    if (!ctx.bot.is_ready(KILL_SHOT_MM)) return false;
    if (FocusVal(ctx) < BLACK_ARROW_COST) return false;   // Kill Shot also costs 10
    return TargetExecuteRange(ctx);
}
void DoKillShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(KILL_SHOT_MM, ctx.bot.victim());
}

// ---- Damage rotation ----
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
    if (FocusVal(ctx) < MULTI_SHOT_COST) return false;   // 30 focus in 12.1 (kit); under-costing let it claim ticks it couldn't pay for
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
    if (FocusVal(ctx) < AIMED_SHOT_COST) return false;
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
    { ShouldIntimidation,   DoIntimidation,   "Intimidation (interrupt fb)" },
    { ShouldBindingShot,    DoBindingShot,    "Binding Shot (3+ AoE)"       },
    { ShouldTarTrap,        DoTarTrap,        "Tar Trap (slow)"             },
    { ShouldHarriersCry,    DoHarriersCry,    "Harrier's Cry (raid haste)"  },
    { ShouldTrueshot,       DoTrueshot,       "Trueshot (boss)"             },
    { ShouldVolley,         DoVolley,         "Volley (on cd)"              },
    { ShouldExplosiveShot,  DoExplosiveShot,  "Explosive Shot (on cd)"      },
    { ShouldHuntersMark,    DoHuntersMark,    "Hunter's Mark (debuff)"      },
    { ShouldBlackArrow,     DoBlackArrow,     "Black Arrow (<20% / >80%)"   },
    { ShouldKillShot,       DoKillShot,       "Kill Shot (<=20%)"           },
    { ShouldRapidFire,      DoRapidFire,      "Rapid Fire"                  },
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
