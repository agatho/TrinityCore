// Arcane Mage - WoW 12.0 enterprise rotation. Charge-driven mana caster:
// build Arcane Charges with Arcane Blast / Arcane Missiles / Arcane Orb, dump
// with Arcane Barrage at 4 stacks. Touch of the Magi window centers burst,
// Arcane Power / Presence of Mind layer on top, Evocation refills mana when
// Clearcasting goes long. Nether Tempest provides multi-dot pressure, Slow
// kites dangerous melee, Spellsteal pulls offensive enemy buffs, Polymorph
// keeps an off-target sapped while we focus the kill target.
//
// Charge state lives on the bot as the ARCANE_CHARGE aura with stack count.
// We use BotSnapshotView::aura_stacks() so the spender ticks at 4 charges
// every tick — no relying on next-tick heuristics.
//
// ---- Validated IDs (verified against wago.tools SpellName.csv 12.0) ----
//   30451  Arcane Blast            321507 Touch of the Magi
//   44425  Arcane Barrage          114923 Nether Tempest
//   5143   Arcane Missiles         1449   Arcane Explosion
//   153626 Arcane Orb (talent)     31589  Slow
//   12042  Arcane Power            30449  Spellsteal
//   12051  Evocation               55342  Mirror Image
//   205025 Presence of Mind        342245 Alter Time
//   263725 Clearcasting (proc)     2139   Counterspell
//   36032  Arcane Charge (stack)   108839 Ice Floes (talent)
//   314791 Shifting Power (talent) 118    Polymorph
//   45438  Ice Block               122    Frost Nova
//   1953   Blink                   80353  Time Warp
//
// ---- Skipped spells (and why) ----
//   79684  Clearcasting passive    — procc-driver. We consume the active
//                                    aura (263725 — same name, distinct
//                                    ID) via has_aura(); the passive
//                                    driver itself is never read.
//   30625  Arcane Surge (legacy)   — old Mage-spec talent; modern
//                                    spec uses Arcane Power (12042)
//                                    until Dragonflight+ trees.
//   307443 Radiant Spark           — niche covenant ability; not
//                                    granted in classic-style data.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "ApCrowdControl.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated) ----
constexpr uint32 ARCANE_BLAST       = 30451;
constexpr uint32 ARCANE_BARRAGE     = 44425;
constexpr uint32 ARCANE_MISSILES    = 5143;
constexpr uint32 ARCANE_ORB         = 153626;     // talent — single target builder + AoE
constexpr uint32 ARCANE_POWER       = 12042;      // 90s CD burst window
constexpr uint32 EVOCATION          = 12051;      // mana refill channel
constexpr uint32 PRESENCE_OF_MIND   = 205025;     // next 2 Arcane Blasts instant
constexpr uint32 CLEARCASTING       = 263725;     // proc — free Arcane Missiles
constexpr uint32 ARCANE_CHARGE      = 36032;      // tracked aura with stacks
constexpr uint32 TOUCH_OF_THE_MAGI  = 321507;     // 45s CD damage absorb / explode
constexpr uint32 NETHER_TEMPEST     = 114923;     // 12s DoT, 1 target at a time
constexpr uint32 ARCANE_EXPLOSION   = 1449;       // PBAoE charge builder
constexpr uint32 SLOW               = 31589;      // 60% movement debuff
constexpr uint32 SPELLSTEAL         = 30449;      // grab a buff off enemy
constexpr uint32 MIRROR_IMAGE       = 55342;      // threat dump + DPS cooldown
constexpr uint32 ALTER_TIME         = 342245;     // HP/position snapshot + return
constexpr uint32 COUNTERSPELL       = 2139;
constexpr uint32 ICE_FLOES          = 108839;     // talent — 3-charge cast-while-moving enabler
constexpr uint32 POLYMORPH          = 118;        // CC — sheep
constexpr uint32 ICE_BLOCK          = 45438;     // 10s immunity, 4min CD
constexpr uint32 FROST_NOVA         = 122;       // 8yd PBAoE root
constexpr uint32 BLINK              = 1953;       // 20yd reposition
constexpr uint32 SHIFTING_POWER     = 314791;     // talent — channel that
                                                  // reduces all Arcane CDs
                                                  // (Arcane Power, Touch of
                                                  // the Magi, Evocation).
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

uint8 ArcaneCharges(ApPredicateContext const& ctx)
{
    return ctx.bot.aura_stacks(ARCANE_CHARGE);
}

// ---- Survival ----
bool ShouldIceBlock(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ICE_BLOCK)) return false;
    if (!ctx.bot.is_ready(ICE_BLOCK)) return false;
    // PvP burst kills cloth from 40% in <2s; the 20% threshold trips
    // after the killshot has already landed. Bump under_player_attack
    // for parity with Fire / Frost specs.
    const int32 threshold = ctx.pvp.under_player_attack ? 40 : 20;
    return ctx.bot.hp_pct() <= threshold;
}
void DoIceBlock(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ICE_BLOCK); }

bool ShouldAlterTime(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ALTER_TIME)) return false;
    if (!ctx.bot.is_ready(ALTER_TIME)) return false;
    // Snapshot at high HP / safe position so the return jump rescues us
    // when the inevitable damage spike comes during the burst window.
    return ctx.bot.hp_pct() >= 80 && !ctx.bot.has_aura(ALTER_TIME);
}
void DoAlterTime(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ALTER_TIME); }

bool ShouldMirrorImage(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(MIRROR_IMAGE)) return false;
    if (!ctx.bot.is_ready(MIRROR_IMAGE)) return false;
    // Threat-dump trigger when something locked onto us, or proactively
    // before a boss burst. Either condition fires the cooldown.
    return ctx.bot.hp_pct() <= 70 || BossLikeTargetEngaged(ctx);
}
void DoMirrorImage(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(MIRROR_IMAGE); }

bool ShouldFrostNova(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(FROST_NOVA)) return false;
    if (!ctx.bot.is_ready(FROST_NOVA)) return false;
    // Personal defensive — fire only with an attacker in melee range.
    return ctx.bot.enemies_within(8.0f) >= 1;
}
void DoFrostNova(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FROST_NOVA); }

bool ShouldBlinkAway(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BLINK)) return false;
    if (!ctx.bot.is_ready(BLINK)) return false;
    // Emergency reposition when low HP and a melee is on us — Blink breaks
    // most roots and gets us 20yd of breathing room before Frost Nova / Ice
    // Block decision tree fires next tick.
    return ctx.bot.hp_pct() <= 35 && ctx.bot.enemies_within(8.0f) >= 1;
}
void DoBlink(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLINK); }

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

// Polymorph an off-target add via the shared PickOffTargetCC gate. PvE only
// fires on a genuine 2+ ATTACKER pull (never a 40y scan bystander while
// solo-questing) and skips already-sheeped mobs via NearbyUnit::is_cc_locked;
// PvP escalates to enemy Healer > caster. See ApCrowdControl.h for why the
// old nearby_enemies.size()>=2 + has_aura gate CC-spammed every GCD.
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

// Spellsteal — pull a useful buff off the enemy. We approximate "useful"
// by checking that an aura is dispellable as Magic. The snapshot view's
// target_dispellable() inspects the current target's aura list.
bool ShouldSpellsteal(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SPELLSTEAL)) return false;
    if (!ctx.bot.is_ready(SPELLSTEAL)) return false;
    // Mana cost is significant (~21%); only spend when we have headroom.
    if (ctx.bot.power_pct(0) < 35) return false;
    return ctx.bot.target_dispellable(Playerbot::DispelType::Magic);
}
void DoSpellsteal(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SPELLSTEAL, ctx.bot.victim());
}

bool ShouldSlow(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SLOW)) return false;
    if (!ctx.bot.is_ready(SLOW)) return false;
    // Only apply when victim is melee-class threat (<= 12yd) and lacks Slow.
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v) return false;
    if (ctx.bot.has_aura(SLOW, v->guid)) return false;
    return ctx.bot.enemies_within(12.0f) >= 1;
}
void DoSlow(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SLOW, ctx.bot.victim());
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

// Touch of the Magi — applied to the victim, absorbs damage, then explodes
// for stored damage. We want to land it ASAP (window centers Arcane Power)
// and only re-apply when the previous instance has expired.
bool ShouldTouchOfTheMagi(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(TOUCH_OF_THE_MAGI)) return false;
    if (!ctx.bot.is_ready(TOUCH_OF_THE_MAGI)) return false;
    // No point applying to a corpse; victim_info()->hp guards that.
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v || v->hp <= 0) return false;
    // Don't re-apply if our debuff is still ticking on this target.
    return !ctx.bot.has_aura(TOUCH_OF_THE_MAGI, v->guid);
}
void DoTouchOfTheMagi(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(TOUCH_OF_THE_MAGI, ctx.bot.victim());
}

bool ShouldArcanePower(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ARCANE_POWER)) return false;
    if (!ctx.bot.is_ready(ARCANE_POWER)) return false;
    // Want at least 3 charges so the burst window has spenders queued up,
    // and enough mana to actually channel hard during the buff.
    return ArcaneCharges(ctx) >= 3 && ctx.bot.power_pct(0) >= 40;
}
void DoArcanePower(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ARCANE_POWER); }

bool ShouldPresenceOfMind(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(PRESENCE_OF_MIND)) return false;
    if (!ctx.bot.is_ready(PRESENCE_OF_MIND)) return false;
    // Stack with Arcane Power, or use to clip movement. Either condition
    // fires it — the buff lingers until 2 Arcane Blasts are used.
    return ctx.bot.has_aura(ARCANE_POWER) || ctx.bot.is_moving();
}
void DoPresenceOfMind(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(PRESENCE_OF_MIND); }

bool ShouldEvocation(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(EVOCATION)) return false;
    if (!ctx.bot.is_ready(EVOCATION)) return false;
    // Don't break a burst window to channel Evocation.
    if (ctx.bot.has_aura(ARCANE_POWER)) return false;
    return ctx.bot.power_pct(0) <= 30;
}
void DoEvocation(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(EVOCATION); }

// Shifting Power — short channel that AoEs around the mage AND shaves
// the cooldown off our major CDs (Arcane Power / Touch of the Magi /
// Evocation). Only valuable in combat with a real target and when at
// least one of those CDs is actually on cooldown to compress — otherwise
// it's just a weak AoE that interrupts the rotation.
bool ShouldShiftingPower(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHIFTING_POWER)) return false;
    if (!ctx.bot.is_ready(SHIFTING_POWER)) return false;
    // Don't channel while Arcane Power is up — burst window should not be
    // wasted on a slow channel that reduces a CD already active.
    if (ctx.bot.has_aura(ARCANE_POWER)) return false;
    // Pause when moving (channeled spell drops on first step).
    if (ctx.bot.is_moving()) return false;
    // Compress at least one big CD — otherwise the GCD spend isn't worth
    // the lost Blast / Barrage time.
    const bool ap_on_cd  = ctx.bot.knows_spell(ARCANE_POWER)      && !ctx.bot.is_ready(ARCANE_POWER);
    const bool tom_on_cd = ctx.bot.knows_spell(TOUCH_OF_THE_MAGI) && !ctx.bot.is_ready(TOUCH_OF_THE_MAGI);
    const bool ev_on_cd  = ctx.bot.knows_spell(EVOCATION)         && !ctx.bot.is_ready(EVOCATION);
    return ap_on_cd || tom_on_cd || ev_on_cd;
}
void DoShiftingPower(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SHIFTING_POWER); }

// ---- AoE / DoT ----
bool ShouldArcaneOrb(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ARCANE_ORB)) return false;
    if (!ctx.bot.is_ready(ARCANE_ORB)) return false;
    // Free charge generator and decent damage — fire on cooldown when we
    // are not yet at max charges (no overflow).
    return ArcaneCharges(ctx) < 4;
}
void DoArcaneOrb(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ARCANE_ORB, ctx.bot.victim());
}

bool ShouldNetherTempest(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(NETHER_TEMPEST)) return false;
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v || v->hp <= 0) return false;
    // Refresh in pandemic window (~3s before expiry); also apply when absent.
    AuraEntry const* a = ctx.bot.find_aura(NETHER_TEMPEST, v->guid);
    return !a || a->remaining.count() <= 3000;
}
void DoNetherTempest(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(NETHER_TEMPEST, ctx.bot.victim());
}

bool ShouldArcaneExplosion(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ARCANE_EXPLOSION)) return false;
    // PBAoE charge builder — only when 3+ enemies are in its 10yd hit box,
    // so we don't waste mana on single-target. aoe_preference is a soft
    // owner hint; still require ≥2 enemies so stale `.aoe on` doesn't
    // fire on a single boss pull.
    const int near = ctx.bot.enemies_within(10.0f);
    return near >= 3 || (ctx.aoe_preference && near >= 2);
}
void DoArcaneExplosion(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(ARCANE_EXPLOSION);
}

// ---- Charge spender ----
bool ShouldArcaneBarrage(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ARCANE_BARRAGE)) return false;
    if (!ctx.bot.is_ready(ARCANE_BARRAGE)) return false;
    // Spend at 4 charges, OR when low mana (Barrage costs much less than
    // Blast and refunds mana on hit), OR when we need to dump charges
    // before they overflow at the cap.
    if (ArcaneCharges(ctx) >= 4) return true;
    if (ctx.bot.power_pct(0) <= 35 && ArcaneCharges(ctx) >= 1) return true;
    return false;
}
void DoArcaneBarrage(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ARCANE_BARRAGE, ctx.bot.victim());
}

// ---- Proc spender ----
bool ShouldArcaneMissiles(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ARCANE_MISSILES)) return false;
    // Free missiles (no mana cost while Clearcasting), so spend immediately.
    return ctx.bot.has_aura(CLEARCASTING);
}
void DoArcaneMissiles(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ARCANE_MISSILES, ctx.bot.victim());
}

// ---- Charge builder / filler ----
bool ShouldArcaneBlast(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ARCANE_BLAST)) return false;
    // Mana floor — Arcane Blast cost scales with charges; refuse when too
    // low so the next tick falls through to Barrage / Evocation instead.
    if (ctx.bot.power_pct(0) <= 20) return false;
    // Hard-cast — defer to instant Arcane Barrage when moving.
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(ARCANE_BLAST)) return false;
    return true;
}
void DoArcaneBlast(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ARCANE_BLAST, ctx.bot.victim());
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
//   2. Defensives (Mirror Image / Alter Time / Blink)
//   3. Interrupt (Counterspell)
//   4. Kite (Frost Nova)
//   5. CC (Polymorph) / utility (Spellsteal, Slow)
//   6. Cast-while-moving prep (Ice Floes)
//   7. Major offensive CDs (Time Warp / Touch of the Magi / Arcane Power
//      / Presence of Mind / Shifting Power / Evocation)
//   8. Procs (Arcane Missiles via Clearcasting)
//   9. AoE (Arcane Explosion)
//  10. Charge spender (Arcane Barrage)
//  11. DoT (Nether Tempest)
//  12. ST builder (Arcane Orb / Arcane Blast)
//  13. Filler (Auto Attack)
ApRule const kRules[] = {
    // 1. Survival panic
    { ShouldIceBlock,        DoIceBlock,        "Ice Block (panic <=20% / <=40% PvP)" },
    // 2. Defensives
    { ShouldMirrorImage,     DoMirrorImage,     "Mirror Image (threat / boss)"  },
    { ShouldAlterTime,       DoAlterTime,       "Alter Time (snapshot HP)"      },
    { ShouldBlinkAway,       DoBlink,           "Blink (escape melee)"          },
    // 3. Interrupt
    { ShouldCounterspell,    DoCounterspell,    "Counterspell (interrupt)"      },
    // 4. Kite
    { ShouldFrostNova,       DoFrostNova,       "Frost Nova (kite melee)"       },
    // 5. CC / utility
    { ShouldPolymorph,       DoPolymorph,       "Polymorph (off-target CC)"     },
    { ShouldSpellsteal,      DoSpellsteal,      "Spellsteal (Magic buff)"       },
    { ShouldSlow,            DoSlow,            "Slow (melee debuff)"           },
    // 6. Movement prep
    { ShouldIceFloes,        DoIceFloes,        "Ice Floes (moving prep)"       },
    // 7. Major offensive CDs
    { ShouldTimeWarp,        DoTimeWarp,        "Time Warp (boss)"              },
    { ShouldTouchOfTheMagi,  DoTouchOfTheMagi,  "Touch of the Magi (window)"    },
    { ShouldArcanePower,     DoArcanePower,     "Arcane Power (burst)"          },
    { ShouldPresenceOfMind,  DoPresenceOfMind,  "Presence of Mind (instant)"    },
    { ShouldShiftingPower,   DoShiftingPower,   "Shifting Power (CD compress)"  },
    { ShouldEvocation,       DoEvocation,       "Evocation (<=30% mana)"        },
    // 8. Procs (free spender — fire ASAP before it expires)
    { ShouldArcaneMissiles,  DoArcaneMissiles,  "Arcane Missiles (Clearcast)"   },
    // 9. AoE
    { ShouldArcaneExplosion, DoArcaneExplosion, "Arcane Explosion (3+ AoE)"     },
    // 10. Charge spender (4-stack dump / mana-low refund)
    { ShouldArcaneBarrage,   DoArcaneBarrage,   "Arcane Barrage (spend 4)"      },
    // 11. DoT refresh
    { ShouldNetherTempest,   DoNetherTempest,   "Nether Tempest (DoT refresh)"  },
    // 12. ST builders / filler
    { ShouldArcaneOrb,       DoArcaneOrb,       "Arcane Orb (charge gen)"       },
    { ShouldArcaneBlast,     DoArcaneBlast,     "Arcane Blast (build)"          },
    // 13. Auto attack fallback
    { AlwaysInCombat,        DoAutoAttack,      "Engage auto attack"            },
};

} // anonymous

void RegisterApl_Mage_Arcane()
{
    constexpr uint32 SPEC_MAGE_ARCANE = 62;
    RegisterRotation(CLASS_MAGE, SPEC_MAGE_ARCANE, ApRotation{kRules});
}

} // namespace Playerbot::Combat
