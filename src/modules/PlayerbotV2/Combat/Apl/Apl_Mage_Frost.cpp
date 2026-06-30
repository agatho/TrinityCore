// Frost Mage - WoW 12.0 baseline rotation. Caster DPS, mana resource.
// Validates a third combat archetype: cast-time spells with proc-based
// instant spenders. Shatter combo: Winter's Chill / Fingers of Frost lets
// Ice Lance crit; Brain Freeze procs make Flurry instant.
//
// ---- Validated IDs (verified against wago.tools SpellName.csv 12.0) ----
//   116    Frostbolt              30455  Ice Lance
//   44614  Flurry                 84714  Frozen Orb
//   11426  Ice Barrier            199786 Glacial Spike (talent)
//   153595 Comet Storm (talent)   205021 Ray of Frost (talent)
//   257537 Ebonbolt (talent)      55342  Mirror Image
//   235219 Cold Snap              113724 Ring of Frost
//   118    Polymorph              342245 Alter Time
//   190446 Brain Freeze (proc)    44544  Fingers of Frost (proc)
//   228358 Winter's Chill (deb.)  205473 Icicles (stack tracker)
//   2139   Counterspell           190356 Blizzard
//   120    Cone of Cold           122    Frost Nova
//   45438  Ice Block              12472  Icy Veins
//   108839 Ice Floes (talent)     80353  Time Warp
//
// ---- Skipped spells (and why) ----
//   1246769 Shatter               — passive crit-multiplier on frozen
//                                   targets. Granted automatically; no
//                                   active aura to query, no GCD to
//                                   spend. The whole rotation already
//                                   leverages it implicitly via the
//                                   Fingers of Frost / Winter's Chill
//                                   Ice Lance gates.
//   1247775 Winter's End          — passive talent (modern Frost). No
//                                   active proc, no rule wiring needed.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "ApCrowdControl.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated) ----
constexpr uint32 FROSTBOLT          = 116;
constexpr uint32 ICE_LANCE          = 30455;
constexpr uint32 FLURRY             = 44614;
constexpr uint32 FROZEN_ORB         = 84714;
constexpr uint32 ICE_BARRIER        = 11426;
constexpr uint32 GLACIAL_SPIKE      = 199786;     // talent — big-hit spike at 5 Icicles
constexpr uint32 COMET_STORM        = 153595;     // talent — single-target burst, 30s CD
constexpr uint32 RAY_OF_FROST       = 205021;     // talent — channeled high damage
constexpr uint32 EBONBOLT           = 257537;     // talent — generates Brain Freeze
constexpr uint32 MIRROR_IMAGE       = 55342;      // 3x clones, 40s duration, 2min CD
constexpr uint32 COLD_SNAP          = 235219;     // resets Frost Nova / Cone of Cold / Ice Block
constexpr uint32 RING_OF_FROST      = 113724;     // 10s AoE root, 45s CD
constexpr uint32 POLYMORPH          = 118;        // CC — sheep humanoid/beast/etc.
constexpr uint32 ALTER_TIME         = 342245;     // snapshots HP/position; reverts after 10s

// Procs / triggers
constexpr uint32 BRAIN_FREEZE       = 190446;     // makes next Flurry instant
constexpr uint32 FINGERS_OF_FROST   = 44544;      // next Ice Lance crits
constexpr uint32 WINTERS_CHILL      = 228358;     // debuff on target — guarantees shatter for next 2 spells
constexpr uint32 ICICLES_AURA       = 205473;     // Icicles stack tracker (5 = Glacial Spike ready)

constexpr uint32 COUNTERSPELL       = 2139;       // 40yd interrupt
constexpr uint32 BLIZZARD           = 190356;
constexpr uint32 CONE_OF_COLD       = 120;
constexpr uint32 FROST_NOVA         = 122;
constexpr uint32 ICE_BLOCK          = 45438;     // 10s immunity, 4min CD
constexpr uint32 ICY_VEINS          = 12472;     // 25s haste burst, 3min CD
constexpr uint32 ICE_FLOES           = 108839;    // talent — 3 charges, next hard-cast can move
constexpr uint32 TIME_WARP              = 80353;
constexpr uint32 SATED_DEBUFF           = 57724;
constexpr uint32 TEMPORAL_DISPL_DEBUFF  = 80354;
constexpr uint32 INSANITY_HUNTER_DEBUFF = 95809;
constexpr uint32 FATIGUED_DEBUFF        = 264689;

// ---- Helpers ----
bool HasLiveTarget(ApPredicateContext const& ctx)
{
    return !ctx.bot.victim().IsEmpty();
}

bool BotHasSatedDebuff(ApPredicateContext const& ctx)
{
    return ctx.bot.has_aura(SATED_DEBUFF)
        || ctx.bot.has_aura(TEMPORAL_DISPL_DEBUFF)
        || ctx.bot.has_aura(INSANITY_HUNTER_DEBUFF)
        || ctx.bot.has_aura(FATIGUED_DEBUFF);
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

// ---- Survival ----
bool ShouldIceBlock(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ICE_BLOCK)) return false;
    if (!ctx.bot.is_ready(ICE_BLOCK)) return false;
    // PvP burst kills clothies from 40% in <2s. The 20% raid threshold
    // is too late once a real player is on you — by the time HP touches
    // 20 the next instant has already landed. Bump the trigger when
    // under_player_attack so the iceblock catches the danger window.
    const int32 threshold = ctx.pvp.under_player_attack ? 40 : 20;
    return ctx.bot.hp_pct() <= threshold;
}
void DoIceBlock(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ICE_BLOCK); }

// ---- CC (PvP-aware Polymorph) ----
// Sheep an off-target enemy via the shared PickOffTargetCC gate: PvE fires
// only on a genuine 2+ ATTACKER pull (never on a 40y scan bystander while
// solo-questing) and skips already-sheeped mobs through NearbyUnit::
// is_cc_locked; PvP escalates to enemy Healer > caster. See ApCrowdControl.h
// for the full rationale (this replaced the old nearby_enemies.size()>=2 +
// has_aura gate that made the bot Polymorph every GCD and never DPS).
bool ShouldPolymorph(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(POLYMORPH)) return false;
    if (!ctx.bot.is_ready(POLYMORPH)) return false;
    return !PickOffTargetCC(ctx, POLYMORPH, ApInPvp(ctx)).IsEmpty();
}
void DoPolymorph(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    ObjectGuid const t = PickOffTargetCC(ctx, POLYMORPH, ApInPvp(ctx));
    if (!t.IsEmpty()) e.cast(POLYMORPH, t);
}

bool ShouldAlterTime(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ALTER_TIME)) return false;
    if (!ctx.bot.is_ready(ALTER_TIME)) return false;
    // Snapshot HP at 80%+; if we drop below 35% within 10s, the revert
    // restores us to the snapshot. Effectively a delayed self-heal cooldown.
    // Predicate: only snapshot when HP is high (no point recording a low
    // snapshot we'd never want to revert to).
    return ctx.bot.hp_pct() >= 80 && !ctx.bot.has_aura(ALTER_TIME);
}
void DoAlterTime(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ALTER_TIME); }

bool ShouldIceBarrier(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(ICE_BARRIER)) return false;
    if (!ctx.bot.is_ready(ICE_BARRIER)) return false;
    if (ctx.bot.has_aura(ICE_BARRIER)) return false;   // already up
    return ctx.bot.in_combat() && ctx.bot.hp_pct() <= 90;
}
void DoIceBarrier(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ICE_BARRIER); }

bool ShouldMirrorImage(ApPredicateContext const& ctx)
{
    // Mirror Image is both a damage cooldown AND a threat dump. Use it on
    // pull (early in combat) or when low HP.
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(MIRROR_IMAGE)) return false;
    if (!ctx.bot.is_ready(MIRROR_IMAGE)) return false;
    return ctx.bot.hp_pct() <= 70 || BossLikeTargetEngaged(ctx);
}
void DoMirrorImage(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(MIRROR_IMAGE); }

bool ShouldColdSnap(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(COLD_SNAP)) return false;
    if (!ctx.bot.is_ready(COLD_SNAP)) return false;
    // Resets Ice Block / Frost Nova / Cone of Cold. Worth firing when
    // Ice Block is on CD AND we're going to need it (low HP).
    return ctx.bot.hp_pct() <= 25 && !ctx.bot.is_ready(ICE_BLOCK);
}
void DoColdSnap(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(COLD_SNAP); }

// ---- Interrupt + CC ----
bool ShouldCounterspell(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(COUNTERSPELL)) return false;
    if (!ctx.bot.is_ready(COUNTERSPELL)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    return ctx.bot.kick_target(pvp, 40.0f) != nullptr;
}
void DoCounterspell(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 40.0f))
        e.cast(COUNTERSPELL, c->guid);
}

// Frost Nova used both defensively (root attackers in melee) and as a
// shatter-combo setup (Frozen target → Ice Lance crits guaranteed).
// Conservative gate: only kite when WE are the one being beaten on in melee
// (an enemy is attacking us inside 8y) AND there's no group tank to hold the
// pack — rooting the tank's pack mid-pull peels mobs off the tank and
// scatters them. Solo (no tank) keeps the original defensive behavior.
bool ShouldFrostNova(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(FROST_NOVA)) return false;
    if (!ctx.bot.is_ready(FROST_NOVA)) return false;
    // Must be personally under melee attack: something is on our threat list
    // AND a hostile is within Frost Nova's 8y radius.
    if (ctx.bot.attackers_count() < 1) return false;
    if (ctx.bot.enemies_within(8.0f) < 1) return false;
    // If a living group tank exists, let them hold aggro — don't root the
    // pack. tank() returns nullptr solo / tankless, preserving the kite.
    GroupMemberSummary const* tank = ctx.group.tank();
    if (tank && tank->online && tank->hp > 0)
        return false;
    return true;
}
void DoFrostNova(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FROST_NOVA); }

bool ShouldRingOfFrost(ApPredicateContext const& ctx)
{
    // Ground-target AoE root — useful when leader hasn't pulled and we
    // need to lock down adds before they reach us. Skip if already
    // surrounded (the cast time is too long to land).
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RING_OF_FROST)) return false;
    if (!ctx.bot.is_ready(RING_OF_FROST)) return false;
    return ctx.bot.enemies_within(20.0f) >= 3 && ctx.bot.enemies_within(8.0f) == 0;
}
void DoRingOfFrost(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(RING_OF_FROST, v->x, v->y, v->z);
    else
        e.cast(RING_OF_FROST);
}

// ---- Major offensive cooldowns ----
bool ShouldIcyVeins(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ICY_VEINS)) return false;
    if (!ctx.bot.is_ready(ICY_VEINS)) return false;
    if (ctx.bot.victim() == ObjectGuid::Empty) return false;
    // 3min major burst — gate to boss-tier or 3+ enemy AoE cluster.
    // Was unconditionally firing on first victim; trash pull would
    // burn the CD seconds before a boss pull. Same gate as TimeWarp.
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(8.0f) >= 3;
}
void DoIcyVeins(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ICY_VEINS); }

bool ShouldTimeWarp(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(TIME_WARP)) return false;
    if (!ctx.bot.is_ready(TIME_WARP)) return false;
    if (BotHasSatedDebuff(ctx)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoTimeWarp(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TIME_WARP); }

bool ShouldCometStorm(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(COMET_STORM)) return false;
    if (!ctx.bot.is_ready(COMET_STORM)) return false;
    return true;
}
void DoCometStorm(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(COMET_STORM, v->x, v->y, v->z);
    else
        e.cast(COMET_STORM);
}

bool ShouldRayOfFrost(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RAY_OF_FROST)) return false;
    if (!ctx.bot.is_ready(RAY_OF_FROST)) return false;
    // Channel — only on a boss where we can stand still safely.
    return BossLikeTargetEngaged(ctx);
}
void DoRayOfFrost(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(RAY_OF_FROST, ctx.bot.victim()); }

bool ShouldFrozenOrb(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FROZEN_ORB)) return false;
    return ctx.bot.is_ready(FROZEN_ORB);
}
void DoFrozenOrb(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info()) e.cast_at(FROZEN_ORB, v->x, v->y, v->z);
    else                                       e.cast(FROZEN_ORB);
}

// ---- Proc spending ----
bool ShouldFlurry(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FLURRY)) return false;
    if (!ctx.bot.is_ready(FLURRY)) return false;
    // Only spend Flurry when Brain Freeze procs (otherwise it has a long
    // cast — we'd rather Frostbolt).
    return ctx.bot.has_aura(BRAIN_FREEZE);
}
void DoFlurry(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(FLURRY, ctx.bot.victim()); }

bool ShouldGlacialSpike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(GLACIAL_SPIKE)) return false;
    if (!ctx.bot.is_ready(GLACIAL_SPIKE)) return false;
    // Glacial Spike requires 5 Icicle stacks. The Icicles aura tracks the
    // current stack count.
    return ctx.bot.aura_stacks(ICICLES_AURA) >= 5;
}
void DoGlacialSpike(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(GLACIAL_SPIKE, ctx.bot.victim()); }

bool ShouldIceLance(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ICE_LANCE)) return false;
    if (!ctx.bot.is_ready(ICE_LANCE)) return false;
    // Spend Fingers of Frost charges; otherwise save it for shatter combos.
    // Also spend free if the target has Winter's Chill (debuff applied by
    // Flurry hits — guarantees the next 2 spells crit).
    if (ctx.bot.has_aura(FINGERS_OF_FROST)) return true;
    if (ctx.bot.find_aura(WINTERS_CHILL, ctx.bot.victim()) != nullptr) return true;
    // Moving-fallback: when the bot is moving and Frostbolt is locked
    // out (can_cast_while_moving=false), Ice Lance maintains DPS
    // uptime even without proc. Real Frost Mages refuse to lose
    // global cooldowns to movement — the GCD is too valuable to drop.
    // Skipped when Ice Floes / Slipstream lets Frostbolt cast on the
    // move (those still resolve via ShouldFrostbolt).
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(FROSTBOLT))
        return true;
    return false;
}
void DoIceLance(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(ICE_LANCE, ctx.bot.victim()); }

bool ShouldEbonbolt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(EBONBOLT)) return false;
    if (!ctx.bot.is_ready(EBONBOLT)) return false;
    // Generates Brain Freeze. Only worth firing when we don't already have
    // BF up (otherwise we'd overwrite it).
    return !ctx.bot.has_aura(BRAIN_FREEZE);
}
void DoEbonbolt(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(EBONBOLT, ctx.bot.victim()); }

// ---- AoE ----
bool ShouldBlizzard(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BLIZZARD)) return false;
    if (!ctx.bot.is_ready(BLIZZARD)) return false;
    // aoe_preference is a soft hint; still require ≥2 enemies so stale
    // `.aoe on` doesn't fire on a boss pull.
    const int near = ctx.bot.enemies_within(20.0f);
    return near >= 3 || (ctx.aoe_preference && near >= 2);
}
void DoBlizzard(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info()) e.cast_at(BLIZZARD, v->x, v->y, v->z);
    else                                       e.cast(BLIZZARD);
}

bool ShouldConeOfCold(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CONE_OF_COLD)) return false;
    if (!ctx.bot.is_ready(CONE_OF_COLD)) return false;
    // Short cone in front — usable only when 3+ enemies in melee range.
    return ctx.bot.enemies_within(8.0f) >= 3;
}
void DoConeOfCold(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CONE_OF_COLD); }

// ---- Filler ----
bool ShouldFrostbolt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FROSTBOLT)) return false;
    // Frostbolt is hard-cast; bots in motion would silently fail the cast.
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(FROSTBOLT)) return false;
    return true;
}
void DoFrostbolt(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(FROSTBOLT, ctx.bot.victim()); }

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

// Ice Floes — preemptive cast-while-moving enabler. Off-GCD instant with 3
// charges. Pop when we're moving and the next-priority spell is a hard cast
// (Frostbolt is the most common). Charges regen 20s each, so never holds
// the bot back from spending them when needed.
bool ShouldIceFloes(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ICE_FLOES)) return false;
    if (!ctx.bot.is_ready(ICE_FLOES)) return false;
    if (!ctx.bot.is_moving()) return false;
    if (ctx.bot.has_aura(ICE_FLOES)) return false;
    // Only useful if we'd actually want to cast Frostbolt now — gate on
    // having a victim. Avoids burning a charge while we're repositioning
    // OOC or out-of-range.
    return !ctx.bot.victim().IsEmpty();
}
void DoIceFloes(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ICE_FLOES); }

// ---- Rule table ----
//
// Ordering matches the documented Mage spec ordering (most-urgent first):
//   1. Survival panic (Ice Block + Cold Snap to reset Ice Block)
//   2. Defensives (Mirror Image / Alter Time / Ice Barrier)
//   3. Interrupt (Counterspell)
//   4. Kite (Frost Nova / Ring of Frost / Cone of Cold)
//   5. CC (Polymorph — healer-pref off-target sheep)
//   6. Cast-while-moving prep (Ice Floes)
//   7. Major offensive CDs (Time Warp / Icy Veins / Frozen Orb /
//      Comet Storm / Ray of Frost)
//   8. Procs — Frost's signature combo:
//      a. Flurry (Brain Freeze proc — applies Winter's Chill debuff)
//      b. Glacial Spike (5 Icicle stacks)
//      c. Ice Lance (Fingers of Frost / Winter's Chill / Frost Nova
//         shatter — also moving-fallback when Frostbolt is locked)
//      d. Ebonbolt (Brain Freeze generator when none up)
//   9. AoE (Blizzard at 3+)
//  10. Filler (Frostbolt)
//  11. Auto-attack fallback
//
// Critical Frost mechanic: Ice Lance is gated by frozen-target / proc
// state (Fingers of Frost aura 44544, Winter's Chill debuff on target,
// or moving-fallback). Never an unconditional ST spam — that would
// waste mana on a low-damage cast lacking the shatter multiplier.
//
// Critical proc priority: Brain Freeze MUST be consumed by Flurry
// BEFORE Glacial Spike. Otherwise the bot wastes the BF proc and the
// follow-up Frostbolt that shattered the next mob loses its Winter's
// Chill window. (Audit 2026-05-22 swap preserved.)
ApRule const kRules[] = {
    // 1. Survival panic
    { ShouldIceBlock,      DoIceBlock,      "Ice Block (panic <=20% / <=40% PvP)" },
    { ShouldColdSnap,      DoColdSnap,      "Cold Snap (reset Ice Block)" },
    // 2. Defensives
    { ShouldMirrorImage,   DoMirrorImage,   "Mirror Image (threat / boss)" },
    { ShouldAlterTime,     DoAlterTime,     "Alter Time (snapshot HP)"   },
    { ShouldIceBarrier,    DoIceBarrier,    "Ice Barrier (defensive)"    },
    // 3. Interrupt
    { ShouldCounterspell,  DoCounterspell,  "Counterspell (interrupt)"   },
    // 4. Kite
    { ShouldFrostNova,     DoFrostNova,     "Frost Nova (root attackers)" },
    { ShouldRingOfFrost,   DoRingOfFrost,   "Ring of Frost (3+ approaching)" },
    { ShouldConeOfCold,    DoConeOfCold,    "Cone of Cold (3+ melee)"    },
    // 5. CC
    { ShouldPolymorph,     DoPolymorph,     "Polymorph (off-target CC, healer-pref)" },
    // 6. Movement prep
    { ShouldIceFloes,      DoIceFloes,      "Ice Floes (moving prep)"    },
    // 7. Major offensive CDs
    { ShouldTimeWarp,      DoTimeWarp,      "Time Warp (boss)"           },
    { ShouldIcyVeins,      DoIcyVeins,      "Icy Veins (burst)"          },
    { ShouldFrozenOrb,     DoFrozenOrb,     "Frozen Orb (cooldown)"      },
    { ShouldCometStorm,    DoCometStorm,    "Comet Storm"                },
    { ShouldRayOfFrost,    DoRayOfFrost,    "Ray of Frost (boss channel)" },
    // 8. Procs (signature shatter combo — Flurry first to apply
    //    Winter's Chill before subsequent spenders fire)
    { ShouldFlurry,        DoFlurry,        "Flurry (Brain Freeze)"      },
    { ShouldGlacialSpike,  DoGlacialSpike,  "Glacial Spike (5 Icicles)"  },
    { ShouldIceLance,      DoIceLance,      "Ice Lance (FoF / Shatter)"  },
    { ShouldEbonbolt,      DoEbonbolt,      "Ebonbolt (BF generator)"    },
    // 9. AoE
    { ShouldBlizzard,      DoBlizzard,      "Blizzard (3+ targets)"      },
    // 10. Filler
    { ShouldFrostbolt,     DoFrostbolt,     "Frostbolt (filler)"         },
    // 11. Auto attack fallback
    { AlwaysInCombat,      DoAutoAttack,    "Engage auto attack"         },
};

} // anonymous

void RegisterApl_Mage_Frost()
{
    constexpr uint32 SPEC_MAGE_FROST = 64;
    RegisterRotation(CLASS_MAGE, SPEC_MAGE_FROST, ApRotation{kRules});
}

} // namespace Playerbot::Combat
