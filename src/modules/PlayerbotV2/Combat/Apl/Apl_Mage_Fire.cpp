// Fire Mage - WoW 12.0 baseline rotation. Hot Streak proc-driven Pyroblast
// + Fire Blast / Phoenix Flames proc generation, Combustion burst window.
//
// ---- Validated IDs (verified against wago.tools SpellName.csv 12.0) ----
//   133    Fireball               48108  Hot Streak! (proc aura)
//   11366  Pyroblast              48107  Heating Up (half-proc aura)
//   108853 Fire Blast (Fire spec) 190319 Combustion
//   257541 Phoenix Flames         44457  Living Bomb (talent)
//   2948   Scorch                 153561 Meteor (talent)
//   2120   Flamestrike            157981 Blast Wave (talent)
//   31661  Dragon's Breath        45438  Ice Block
//   122    Frost Nova             55342  Mirror Image
//   342245 Alter Time             2139   Counterspell
//   108839 Ice Floes (talent)     80353  Time Warp
//   118    Polymorph
//
// ---- Skipped spells (and why) ----
//   195283 Hot Streak (passive)   — talent/passive that grants the proc
//                                   mechanic. The runtime proc aura is
//                                   48108 ("Hot Streak!" — exclamation
//                                   in name); we check the proc, not
//                                   the underlying passive.
//   44448  Pyroblast Clearcasting — internal proc driver, not a player
//          Driver                   facing aura. Never read.
//   333313 Sun King's Blessing    — talent that buffs Pyroblast via a
//                                   stacking driver. The runtime proc
//                                   aura ID is currently unstable
//                                   between client builds; integrating
//                                   would require build-specific
//                                   probing. Pyroblast (gated on Hot
//                                   Streak) already fires the empowered
//                                   variant automatically when the buff
//                                   is up. Skipped at rule level —
//                                   spell engine handles the upgrade.
//   319836 Fire Blast (baseline)  — baseline Mage instant; Fire spec
//                                   uses the spec-specific charge
//                                   variant (108853) which generates
//                                   Heating Up / Hot Streak.

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
constexpr uint32 FIREBALL          = 133;
constexpr uint32 PYROBLAST         = 11366;
constexpr uint32 FIRE_BLAST        = 108853;     // charge-based, generates Hot Streak procs
constexpr uint32 PHOENIX_FLAMES    = 257541;     // charge-based, AoE flavor
constexpr uint32 SCORCH            = 2948;       // cast-while-moving filler, exec scaling sub-30
constexpr uint32 COMBUSTION        = 190319;     // burst window — every Fire Blast crits
constexpr uint32 LIVING_BOMB       = 44457;      // talent — DoT that explodes for AoE
constexpr uint32 METEOR            = 153561;     // talent — delayed AoE, big single-hit
constexpr uint32 BLAST_WAVE        = 157981;     // talent — short-CD knock + AoE damage
constexpr uint32 HOT_STREAK        = 48108;      // proc — instant Pyroblast
constexpr uint32 HEATING_UP        = 48107;      // half-proc — next crit becomes Hot Streak
constexpr uint32 COUNTERSPELL      = 2139;
constexpr uint32 ICE_FLOES         = 108839;     // talent — 3-charge cast-while-moving enabler
constexpr uint32 FLAMESTRIKE       = 2120;       // ground AoE
constexpr uint32 DRAGONS_BREATH    = 31661;      // melee AoE stun
constexpr uint32 ICE_BLOCK         = 45438;      // 10s immunity, 4min CD
constexpr uint32 FROST_NOVA        = 122;        // 8yd root, 30s CD
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

bool ShouldBlastWave(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BLAST_WAVE)) return false;
    if (!ctx.bot.is_ready(BLAST_WAVE)) return false;
    return ctx.bot.enemies_within(8.0f) >= 2;
}
void DoBlastWave(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLAST_WAVE); }

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

// ---- DoT ----
bool ShouldLivingBomb(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(LIVING_BOMB)) return false;
    if (!ctx.bot.is_ready(LIVING_BOMB)) return false;
    AuraEntry const* a = ctx.bot.find_aura(LIVING_BOMB, ctx.bot.victim());
    return !a || a->remaining.count() <= 3000;
}
void DoLivingBomb(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(LIVING_BOMB, ctx.bot.victim()); }

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

bool ShouldPhoenixFlames(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(PHOENIX_FLAMES)) return false;
    if (!ctx.bot.is_ready(PHOENIX_FLAMES)) return false;
    // Generates Heating Up on hit — fire when we don't have it yet, so we
    // build a Hot Streak. Skip when we already have Hot Streak.
    if (ctx.bot.has_aura(HOT_STREAK)) return false;
    return true;
}
void DoPhoenixFlames(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(PHOENIX_FLAMES, ctx.bot.victim()); }

// ---- Filler ----
bool ShouldScorch(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SCORCH)) return false;
    // Scorch is the cast-while-moving filler + execute scaling. Use it when
    // moving (no cast-time penalty) OR in the execute window (sub-30%).
    return TargetExecuteRange(ctx) || ctx.bot.is_moving();
}
void DoScorch(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SCORCH, ctx.bot.victim()); }

bool ShouldFireball(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FIREBALL)) return false;
    // Hard-cast — defer to Scorch when moving so we don't silently fail.
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

// Ice Floes — preemptive cast-while-moving enabler. Off-GCD instant.
bool ShouldIceFloes(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ICE_FLOES)) return false;
    if (!ctx.bot.is_ready(ICE_FLOES)) return false;
    if (!ctx.bot.is_moving()) return false;
    if (ctx.bot.has_aura(ICE_FLOES)) return false;
    return !ctx.bot.victim().IsEmpty();
}
void DoIceFloes(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ICE_FLOES); }

// ---- Rule table ----
//
// Ordering matches the documented Mage spec ordering (most-urgent first):
//   1. Survival panic (Ice Block)
//   2. Defensives (Mirror Image / Alter Time)
//   3. Interrupt (Counterspell)
//   4. Kite (Frost Nova / Dragon's Breath / Blast Wave)
//   5. CC (Polymorph)
//   6. Cast-while-moving prep (Ice Floes)
//   7. Major offensive CDs (Time Warp / Combustion / Meteor)
//   8. Procs (Hot Streak: Flamestrike-AoE > Pyroblast-ST; then
//      Fire Blast / Phoenix Flames to build the next Hot Streak)
//   9. DoT (Living Bomb)
//  10. Filler (Scorch / Fireball)
//  11. Auto-attack fallback
//
// Critical Fire mechanic: Pyroblast MUST be gated on the Hot Streak
// proc aura (48108). Without the proc it is a 4.5s hardcast that gets
// interrupted; with it, instant + huge crit. ShouldPyroblastHotStreak
// enforces this — no unconditional Pyroblast rule exists.
ApRule const kRules[] = {
    // 1. Survival panic
    { ShouldIceBlock,            DoIceBlock,            "Ice Block (panic <=20% / <=40% PvP)" },
    // 2. Defensives
    { ShouldMirrorImage,         DoMirrorImage,         "Mirror Image (threat / boss)" },
    { ShouldAlterTime,           DoAlterTime,           "Alter Time (snapshot HP)"   },
    // 3. Interrupt
    { ShouldCounterspell,        DoCounterspell,        "Counterspell (interrupt)"   },
    // 4. Kite
    { ShouldFrostNova,           DoFrostNova,           "Frost Nova (kite melee)"    },
    { ShouldDragonsBreath,       DoDragonsBreath,       "Dragon's Breath (2+ AoE)"   },
    { ShouldBlastWave,           DoBlastWave,           "Blast Wave (2+ AoE)"        },
    // 5. CC
    { ShouldPolymorph,           DoPolymorph,           "Polymorph (off-target CC)"  },
    // 6. Movement prep
    { ShouldIceFloes,            DoIceFloes,            "Ice Floes (moving prep)"    },
    // 7. Major offensive CDs
    { ShouldTimeWarp,            DoTimeWarp,            "Time Warp (boss)"           },
    { ShouldCombustion,          DoCombustion,          "Combustion (burst window)"  },
    { ShouldMeteor,              DoMeteor,              "Meteor (boss / 2+ AoE)"     },
    // 8. Procs (Hot Streak consumption — AoE first, then ST)
    { ShouldFlamestrikeHotStreak,DoFlamestrike,         "Flamestrike (HS + 3 AoE)"   },
    { ShouldPyroblastHotStreak,  DoPyroblast,           "Pyroblast (Hot Streak)"     },
    { ShouldFireBlast,           DoFireBlast,           "Fire Blast (HU -> HS)"      },
    { ShouldPhoenixFlames,       DoPhoenixFlames,       "Phoenix Flames (build HU)"  },
    // 9. DoT refresh
    { ShouldLivingBomb,          DoLivingBomb,          "Living Bomb (DoT refresh)"  },
    // 10. Filler
    { ShouldScorch,              DoScorch,              "Scorch (execute / moving)"  },
    { ShouldFireball,            DoFireball,            "Fireball (filler)"          },
    // 11. Auto attack fallback
    { AlwaysInCombat,            DoAutoAttack,          "Engage auto attack"         },
};

} // anonymous

void RegisterApl_Mage_Fire()
{
    constexpr uint32 SPEC_MAGE_FIRE = 63;
    RegisterRotation(CLASS_MAGE, SPEC_MAGE_FIRE, ApRotation{kRules});
}

} // namespace Playerbot::Combat
