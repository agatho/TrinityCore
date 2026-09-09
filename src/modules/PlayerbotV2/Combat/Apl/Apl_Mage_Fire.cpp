// Fire Mage - WoW 12.1.0.69587 (Midnight) rotation. Hot Streak proc-driven
// Pyroblast / Flamestrike, Fire Blast converts Heating Up into Hot Streak,
// Combustion burst window, Meteor on cooldown for bosses / clusters, Scorch
// as the moving + execute filler (instant under Heat Shimmer).
//
// ---- Validated spell IDs (WoW 12.1.0.69587) ----
//   133    Fireball               431044 Frostfire Bolt (hero, overrides Fireball)
//   11366  Pyroblast (talent)     431177 Frostfire Empowerment (proc aura)
//   108853 Fire Blast (talent)    48108  Hot Streak! (proc aura)
//   2948   Scorch (talent)        48107  Heating Up (half-proc aura)
//   190319 Combustion (talent)    458964 Heat Shimmer (instant Scorch aura)
//   2120   Flamestrike (talent)   153561 Meteor (talent)
//   31661  Dragon's Breath        235313 Blazing Barrier
//   45438  Ice Block              110959 Greater Invisibility
//   122    Frost Nova             110960 Greater Invisibility (aura)
//   55342  Mirror Image           342245 Alter Time
//   2139   Counterspell           475    Remove Curse
//   1953   Blink                  212653 Shimmer (talent, overrides Blink)
//   118    Polymorph              80353  Time Warp
//
// ---- Skipped spells (and why) ----
//   257541 Phoenix Flames         - removed from the Fire tree in Midnight.
//   44457  Living Bomb            - removed from the Fire tree in Midnight.
//   157981 Blast Wave             - removed from the Mage class tree.
//   108839 Ice Floes              - removed from the Mage class tree.
//   194466 Phoenix's Flames       - Legion artifact spell (no learn level);
//                                   never granted to a Midnight character.
//   1254851 Flamestrike (target)  - alt Flamestrike variant that lands on
//                                   the target; not in the curated build,
//                                   we cast_at the victim with 2120.
//   195283 Hot Streak (passive)   - proc driver; the runtime aura is 48108.
//   319836 Fire Blast (baseline)  - Fire uses the spec talent id 108853.
//   157980 Supernova / 383121 Mass Polymorph / 157997 Ice Nova - not in
//                                   the curated raid build.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "ApCrowdControl.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated) ----
constexpr uint32 FIREBALL          = 133;
constexpr uint32 FROSTFIRE_BOLT    = 431044;     // hero talent - replaces Fireball
constexpr uint32 FROSTFIRE_EMPOWER = 431177;     // proc aura - next Frostfire Bolt instant
constexpr uint32 PYROBLAST         = 11366;
constexpr uint32 FIRE_BLAST        = 108853;     // charge-based, castable while casting
constexpr uint32 SCORCH            = 2948;       // cast-while-moving filler, guaranteed crit sub-30
constexpr uint32 HEAT_SHIMMER      = 458964;     // proc aura - next Scorch instant + execute
constexpr uint32 COMBUSTION        = 190319;     // burst window - crit chance + damage
constexpr uint32 METEOR            = 153561;     // talent - delayed AoE, big single-hit
constexpr uint32 HOT_STREAK        = 48108;      // proc - instant Pyroblast / Flamestrike
constexpr uint32 HEATING_UP        = 48107;      // half-proc - next crit becomes Hot Streak
constexpr uint32 COUNTERSPELL      = 2139;
constexpr uint32 FLAMESTRIKE       = 2120;       // ground AoE
constexpr uint32 DRAGONS_BREATH    = 31661;      // frontal cone disorient
constexpr uint32 ICE_BLOCK         = 45438;      // 10s immunity, 4min CD
constexpr uint32 FROST_NOVA        = 122;        // 8yd root
constexpr uint32 BLINK             = 1953;       // 20yd reposition
constexpr uint32 SHIMMER           = 212653;     // talent - replaces Blink, off-GCD
constexpr uint32 BLAZING_BARRIER   = 235313;     // absorb shield, burns melee attackers
constexpr uint32 GREATER_INVIS     = 110959;     // threat wipe, 2min CD
constexpr uint32 GREATER_INVIS_AURA= 110960;     // active invisibility aura
constexpr uint32 REMOVE_CURSE      = 475;        // friendly curse dispel, 8s CD
constexpr uint32 MIRROR_IMAGE      = 55342;      // threat dump + DPS
constexpr uint32 ALTER_TIME        = 342245;
constexpr uint32 POLYMORPH         = 118;        // CC
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

bool TargetExecuteRange(ApPredicateContext const& ctx)
{
    NearbyUnit const* t = ctx.bot.victim_info();
    if (!t || t->max_hp <= 0 || t->hp <= 0) return false;
    return (t->hp * 100) / t->max_hp <= 30;
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

// ---- CC (PvP-aware Polymorph) ----
// Off-target sheep via the shared PickOffTargetCC gate (PvE: only on a 2+
// ATTACKER pull, skipping already-sheeped mobs; PvP: enemy Healer > caster).
// See ApCrowdControl.h — this replaced the old nearby_enemies.size()>=2 +
// has_aura gate that CC-spammed every GCD during open-world questing.
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

// ---- Survival ----
bool ShouldIceBlock(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ICE_BLOCK)) return false;
    if (!ctx.bot.is_ready(ICE_BLOCK)) return false;
    // PvP burst kills clothies from 40% in <2s. See Apl_Mage_Frost for
    // rationale; bumped under_player_attack.
    const int32 threshold = ctx.pvp.under_player_attack ? 40 : 20;
    return ctx.bot.hp_pct() <= threshold;
}
void DoIceBlock(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ICE_BLOCK); }

// Greater Invisibility - instant threat wipe. The invisibility breaks on our
// next action, but the threat reset already happened on cast, so it is the
// "get the pack off me" button once Ice Block is gone.
bool ShouldGreaterInvis(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(GREATER_INVIS)) return false;
    if (!ctx.bot.is_ready(GREATER_INVIS)) return false;
    if (ctx.bot.has_aura(GREATER_INVIS_AURA)) return false;
    if (ctx.bot.attackers_count() < 1) return false;
    return ctx.bot.hp_pct() <= 35;
}
void DoGreaterInvis(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(GREATER_INVIS); }

bool ShouldBlazingBarrier(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(BLAZING_BARRIER)) return false;
    if (!ctx.bot.is_ready(BLAZING_BARRIER)) return false;
    if (ctx.bot.has_aura(BLAZING_BARRIER)) return false;   // already up
    return ctx.bot.hp_pct() <= 90;
}
void DoBlazingBarrier(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLAZING_BARRIER); }

bool ShouldAlterTime(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ALTER_TIME)) return false;
    if (!ctx.bot.is_ready(ALTER_TIME)) return false;
    return ctx.bot.hp_pct() >= 80 && !ctx.bot.has_aura(ALTER_TIME);
}
void DoAlterTime(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ALTER_TIME); }

bool ShouldMirrorImage(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(MIRROR_IMAGE)) return false;
    if (!ctx.bot.is_ready(MIRROR_IMAGE)) return false;
    return ctx.bot.hp_pct() <= 70 || BossLikeTargetEngaged(ctx);
}
void DoMirrorImage(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(MIRROR_IMAGE); }

// Emergency reposition when low HP and a melee is on us. Shimmer (talent)
// replaces Blink; two-branch so both talented and untalented bots escape.
bool ShouldShimmerAway(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SHIMMER)) return false;
    if (!ctx.bot.is_ready(SHIMMER)) return false;
    return ctx.bot.hp_pct() <= 35 && ctx.bot.enemies_within(8.0f) >= 1;
}
void DoShimmer(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SHIMMER); }

bool ShouldBlinkAway(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(SHIMMER)) return false;
    if (!ctx.bot.knows_spell(BLINK)) return false;
    if (!ctx.bot.is_ready(BLINK)) return false;
    return ctx.bot.hp_pct() <= 35 && ctx.bot.enemies_within(8.0f) >= 1;
}
void DoBlink(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLINK); }

bool ShouldFrostNova(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(FROST_NOVA)) return false;
    if (!ctx.bot.is_ready(FROST_NOVA)) return false;
    // Personal defensive — only fire if attackers are in melee range.
    return ctx.bot.enemies_within(8.0f) >= 1;
}
void DoFrostNova(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FROST_NOVA); }

// ---- Interrupt / CC ----
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

bool ShouldDragonsBreath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DRAGONS_BREATH)) return false;
    if (!ctx.bot.is_ready(DRAGONS_BREATH)) return false;
    return ctx.bot.enemies_within(12.0f) >= 2;
}
void DoDragonsBreath(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DRAGONS_BREATH); }

// Remove Curse - group utility. Prefer a cursed group member, else self.
bool ShouldRemoveCurse(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(REMOVE_CURSE)) return false;
    if (!ctx.bot.is_ready(REMOVE_CURSE)) return false;
    if (ctx.group.dispel_candidate(DispelType::Curse) != nullptr) return true;
    return ctx.bot.self_dispellable(DispelType::Curse);
}
void DoRemoveCurse(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* m = ctx.group.dispel_candidate(DispelType::Curse)) { e.cast(REMOVE_CURSE, m->guid); return; }
    e.cast(REMOVE_CURSE, ctx.bot.raw().guid);
}

// ---- Major offensive cooldowns ----
bool ShouldTimeWarp(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(TIME_WARP)) return false;
    if (!ctx.bot.is_ready(TIME_WARP)) return false;
    if (BotHasSatedDebuff(ctx)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoTimeWarp(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TIME_WARP); }

bool ShouldCombustion(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(COMBUSTION)) return false;
    if (!ctx.bot.is_ready(COMBUSTION)) return false;
    // Major 2min burst — gate to boss-tier or 3+ enemy AoE cluster.
    // Previous unconditional fire wasted the CD on first-encountered
    // trash mob; missed entire boss windows ~30% of pulls. Matches the
    // synergy pattern in ShouldTimeWarp / ShouldAvatar / ShouldThunderousRoar.
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(8.0f) >= 3;
}
void DoCombustion(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(COMBUSTION); }

bool ShouldMeteor(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(METEOR)) return false;
    if (!ctx.bot.is_ready(METEOR)) return false;
    // 45s CD with delayed landing — wasted on trash mobs that die before
    // it lands. Gate to boss-tier targets or a 2+ enemy cluster (the
    // ground AoE benefits from any extra body in the splash).
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(8.0f) >= 2;
}
void DoMeteor(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(METEOR, v->x, v->y, v->z);
    else
        e.cast(METEOR);
}

// ---- AoE ----
bool ShouldFlamestrikeHotStreak(ApPredicateContext const& ctx)
{
    // Hot Streak Pyroblast vs Flamestrike: prefer Flamestrike when 3+ in
    // its 8yd ground area. The Hot Streak proc gets consumed regardless.
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FLAMESTRIKE)) return false;
    if (!ctx.bot.has_aura(HOT_STREAK)) return false;
    // aoe_preference is a soft owner hint — still require 2+ enemies in
    // range so a stale `.aoe on` from the prior pack doesn't waste Hot
    // Streak on Flamestrike during a boss pull (single-target loss ~30%).
    const int near = ctx.bot.enemies_within(8.0f);
    return near >= 3 || (ctx.aoe_preference && near >= 2);
}
void DoFlamestrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info()) e.cast_at(FLAMESTRIKE, v->x, v->y, v->z);
    else                                       e.cast(FLAMESTRIKE);
}

// ---- Proc spending ----
bool ShouldPyroblastHotStreak(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(PYROBLAST)) return false;
    return ctx.bot.has_aura(HOT_STREAK);
}
void DoPyroblast(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(PYROBLAST, ctx.bot.victim()); }

// Fire Blast — burns a charge to instantly proc Heating Up → Hot Streak.
// Save charges for Combustion window if it's coming up soon (less than
// ~20s remaining); otherwise spend on cooldown.
bool ShouldFireBlast(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FIRE_BLAST)) return false;
    if (!ctx.bot.is_ready(FIRE_BLAST)) return false;
    // If we already have Hot Streak, no point burning a Fire Blast charge
    // (it'd just generate Heating Up which can't stack with Hot Streak).
    if (ctx.bot.has_aura(HOT_STREAK)) return false;
    // Combustion lookahead: when Combustion is < 20s from ready and we
    // already know the spell, hoard Fire Blast charges so the burst
    // window opens with 3 instant-Pyro chains queued. Skip during the
    // Combustion buff itself — every Fire Blast crits guaranteed under
    // the buff, so spend freely.
    if (ctx.bot.knows_spell(COMBUSTION) && !ctx.bot.has_aura(COMBUSTION))
    {
        const int64_t cd_ms = ctx.bot.cd_remaining(COMBUSTION).count();
        if (cd_ms > 0 && cd_ms <= 20000) return false;
    }
    // Spend the proc trigger when Heating Up is up — converts to Hot Streak.
    return ctx.bot.has_aura(HEATING_UP);
}
void DoFireBlast(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(FIRE_BLAST, ctx.bot.victim()); }

// ---- Filler ----
bool ShouldScorch(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SCORCH)) return false;
    // Heat Shimmer proc: next Scorch is instant and counts as execute -
    // always worth the GCD.
    if (ctx.bot.has_aura(HEAT_SHIMMER)) return true;
    // Scorch is the cast-while-moving filler + guaranteed crit sub-30%. Use
    // it when moving (no cast-time penalty) OR in the execute window.
    return TargetExecuteRange(ctx) || ctx.bot.is_moving();
}
void DoScorch(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SCORCH, ctx.bot.victim()); }

// Frostfire Bolt (hero talent) replaces Fireball. Frostfire Empowerment makes
// the next one instant, so fire it even while moving under the proc.
bool ShouldFrostfireBolt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FROSTFIRE_BOLT)) return false;
    if (ctx.bot.has_aura(FROSTFIRE_EMPOWER)) return true;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(FROSTFIRE_BOLT)) return false;
    return true;
}
void DoFrostfireBolt(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(FROSTFIRE_BOLT, ctx.bot.victim()); }

bool ShouldFireball(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(FROSTFIRE_BOLT)) return false;   // overridden
    if (!ctx.bot.knows_spell(FIREBALL)) return false;
    // Hard-cast - defer to Scorch when moving so we don't silently fail.
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(FIREBALL)) return false;
    return true;
}
void DoFireball(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(FIREBALL, ctx.bot.victim()); }

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

// ---- Rule table ----
//
// Ordering matches the documented Mage spec ordering (most-urgent first):
//   1. Survival panic (Ice Block / Greater Invisibility)
//   2. Defensives (Blazing Barrier / Mirror Image / Alter Time /
//      Shimmer or Blink)
//   3. Interrupt (Counterspell) + group dispel (Remove Curse)
//   4. Kite (Frost Nova / Dragon's Breath)
//   5. CC (Polymorph)
//   6. Major offensive CDs (Time Warp / Combustion / Meteor)
//   7. Procs (Hot Streak: Flamestrike-AoE > Pyroblast-ST; then
//      Fire Blast to convert Heating Up into the next Hot Streak)
//   8. Filler (Scorch / Frostfire Bolt or Fireball)
//   9. Auto-attack fallback
//
// Critical Fire mechanic: Pyroblast MUST be gated on the Hot Streak
// proc aura (48108). Without the proc it is a 4s hardcast that gets
// interrupted; with it, instant + huge crit. ShouldPyroblastHotStreak
// enforces this - no unconditional Pyroblast rule exists.
ApRule const kRules[] = {
    // 1. Survival panic
    { ShouldIceBlock,            DoIceBlock,            "Ice Block (panic <=20% / <=40% PvP)" },
    { ShouldGreaterInvis,        DoGreaterInvis,        "Greater Invis (threat wipe)" },
    // 2. Defensives
    { ShouldBlazingBarrier,      DoBlazingBarrier,      "Blazing Barrier (shield)"   },
    { ShouldMirrorImage,         DoMirrorImage,         "Mirror Image (threat / boss)" },
    { ShouldAlterTime,           DoAlterTime,           "Alter Time (snapshot HP)"   },
    { ShouldShimmerAway,         DoShimmer,             "Shimmer (escape melee)"     },
    { ShouldBlinkAway,           DoBlink,               "Blink (escape melee)"       },
    // 3. Interrupt / dispel
    { ShouldCounterspell,        DoCounterspell,        "Counterspell (interrupt)"   },
    { ShouldRemoveCurse,         DoRemoveCurse,         "Remove Curse (group)"       },
    // 4. Kite
    { ShouldFrostNova,           DoFrostNova,           "Frost Nova (kite melee)"    },
    { ShouldDragonsBreath,       DoDragonsBreath,       "Dragon's Breath (2+ AoE)"   },
    // 5. CC
    { ShouldPolymorph,           DoPolymorph,           "Polymorph (off-target CC)"  },
    // 6. Major offensive CDs
    { ShouldTimeWarp,            DoTimeWarp,            "Time Warp (boss)"           },
    { ShouldCombustion,          DoCombustion,          "Combustion (burst window)"  },
    { ShouldMeteor,              DoMeteor,              "Meteor (boss / 2+ AoE)"     },
    // 7. Procs (Hot Streak consumption - AoE first, then ST)
    { ShouldFlamestrikeHotStreak,DoFlamestrike,         "Flamestrike (HS + 3 AoE)"   },
    { ShouldPyroblastHotStreak,  DoPyroblast,           "Pyroblast (Hot Streak)"     },
    { ShouldFireBlast,           DoFireBlast,           "Fire Blast (HU -> HS)"      },
    // 8. Filler (Frostfire Bolt overrides Fireball when talented)
    { ShouldScorch,              DoScorch,              "Scorch (execute / moving)"  },
    { ShouldFrostfireBolt,       DoFrostfireBolt,       "Frostfire Bolt (filler)"    },
    { ShouldFireball,            DoFireball,            "Fireball (filler)"          },
    // 9. Auto attack fallback
    { AlwaysInCombat,            DoAutoAttack,          "Engage auto attack"         },
};

} // anonymous

void RegisterApl_Mage_Fire()
{
    constexpr uint32 SPEC_MAGE_FIRE = 63;
    RegisterRotation(CLASS_MAGE, SPEC_MAGE_FIRE, ApRotation{kRules});
}

} // namespace Playerbot::Combat
