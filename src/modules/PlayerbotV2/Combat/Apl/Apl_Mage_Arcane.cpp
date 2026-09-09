// Arcane Mage - WoW 12.1.0.69587 (Midnight) enterprise rotation. Charge-driven
// mana caster: build Arcane Charges with Arcane Blast / Arcane Missiles /
// Arcane Orb, dump with Arcane Barrage at 4 stacks. Touch of the Magi window
// centers burst, Arcane Surge (spends all mana for one nuke + regen buff) and
// Presence of Mind layer on top, Evocation refills mana when Clearcasting goes
// long. Spellsteal pulls offensive enemy buffs, Polymorph keeps an off-target
// sapped while we focus the kill target.
//
// Charge state lives on the bot as the ARCANE_CHARGE aura with stack count.
// We use BotSnapshotView::aura_stacks() so the spender ticks at 4 charges
// every tick - no relying on next-tick heuristics.
//
// ---- Validated spell IDs (WoW 12.1.0.69587) ----
//   30451  Arcane Blast            321507 Touch of the Magi (talent)
//   44425  Arcane Barrage          365350 Arcane Surge (talent, 90s)
//   5143   Arcane Missiles         365362 Arcane Surge buff (aura)
//   153626 Arcane Orb (talent)     1449   Arcane Explosion
//   12051  Evocation (talent)      1241462 Arcane Pulse (talent, overrides AE)
//   205025 Presence of Mind        30449  Spellsteal (talent)
//   263725 Clearcasting (proc)     55342  Mirror Image (talent)
//   36032  Arcane Charge (stack)   342245 Alter Time (talent)
//   235450 Prismatic Barrier       2139   Counterspell
//   110959 Greater Invisibility    110960 Greater Invisibility (aura)
//   45438  Ice Block (talent)      122    Frost Nova
//   1953   Blink                   212653 Shimmer (talent, overrides Blink)
//   118    Polymorph               80353  Time Warp
//
// ---- Skipped spells (and why) ----
//   12042  Arcane Power            - removed in Midnight; Arcane Surge
//                                    (365350) is the burst cooldown.
//   114923 Nether Tempest          - no longer in the Arcane tree.
//   31589  Slow                    - no longer in the Mage class tree.
//   108839 Ice Floes               - no longer in the Mage class tree.
//   314791 Shifting Power          - no longer in the Mage class tree.
//   79684  Clearcasting passive    - proc driver; we read the active
//                                    aura (263725) via has_aura().
//   190427 Arcane Charge (alt id)  - empty helper; 36032 is the stacked
//                                    aura the client shows.
//   157980 Supernova / 383121 Mass Polymorph / 157997 Ice Nova - not in
//                                    the curated raid build; Supernova is
//                                    [M]-only and a knockback we cannot
//                                    aim safely in a group.
//   475    Remove Curse            - not selected in the Arcane builds.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "ApCrowdControl.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated) ----
constexpr uint32 ARCANE_BLAST       = 30451;
constexpr uint32 ARCANE_BARRAGE     = 44425;
constexpr uint32 ARCANE_MISSILES    = 5143;
constexpr uint32 ARCANE_ORB         = 153626;     // talent - single target builder + AoE
constexpr uint32 ARCANE_SURGE       = 365350;     // talent - 90s CD, spends all mana
constexpr uint32 ARCANE_SURGE_BUFF  = 365362;     // aura - damage + mana regen window
constexpr uint32 EVOCATION          = 12051;      // talent - 3s mana regen burst
constexpr uint32 PRESENCE_OF_MIND   = 205025;     // next 2 Arcane Blasts instant
constexpr uint32 CLEARCASTING       = 263725;     // proc - free Arcane Missiles
constexpr uint32 ARCANE_CHARGE      = 36032;      // tracked aura with stacks
constexpr uint32 TOUCH_OF_THE_MAGI  = 321507;     // damage accumulate / explode
constexpr uint32 ARCANE_EXPLOSION   = 1449;       // PBAoE charge builder
constexpr uint32 ARCANE_PULSE       = 1241462;    // talent - targeted AoE, overrides AE
constexpr uint32 SPELLSTEAL         = 30449;      // grab a buff off enemy
constexpr uint32 MIRROR_IMAGE       = 55342;      // threat dump + DPS cooldown
constexpr uint32 ALTER_TIME         = 342245;     // HP/position snapshot + return
constexpr uint32 PRISMATIC_BARRIER  = 235450;     // absorb + magic DR shield
constexpr uint32 GREATER_INVIS      = 110959;     // threat wipe, 2min CD
constexpr uint32 GREATER_INVIS_AURA = 110960;     // active invisibility aura
constexpr uint32 COUNTERSPELL       = 2139;
constexpr uint32 POLYMORPH          = 118;        // CC - sheep
constexpr uint32 ICE_BLOCK          = 45438;      // 10s immunity, 4min CD
constexpr uint32 FROST_NOVA         = 122;        // 8yd PBAoE root
constexpr uint32 BLINK              = 1953;       // 20yd reposition
constexpr uint32 SHIMMER            = 212653;     // talent - replaces Blink, off-GCD
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

// Greater Invisibility - instant threat wipe. The invisibility itself breaks
// on our next action, but the threat reset already happened on cast, so it
// is a usable "get the pack off me" button when Ice Block is gone.
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

bool ShouldPrismaticBarrier(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(PRISMATIC_BARRIER)) return false;
    if (!ctx.bot.is_ready(PRISMATIC_BARRIER)) return false;
    if (ctx.bot.has_aura(PRISMATIC_BARRIER)) return false;   // already up
    return ctx.bot.hp_pct() <= 90;
}
void DoPrismaticBarrier(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(PRISMATIC_BARRIER); }

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

// Emergency reposition when low HP and a melee is on us - Blink / Shimmer
// gets us 20yd of breathing room before the Frost Nova / Ice Block decision
// tree fires next tick. Shimmer (talent) replaces Blink; two-branch so both
// talented and untalented bots keep the escape.
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

// Touch of the Magi - applied to the victim, accumulates damage, then explodes
// for the stored amount. We want to land it ASAP (window centers Arcane Surge)
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

// Arcane Surge - 2.5s hard cast that spends ALL current mana for one big
// nuke (damage scales with mana spent) and then grants a damage + mana regen
// window (365362). Cast it with a full tank and only while standing still;
// the follow-up rotation runs on the regen buff plus Barrage refunds.
bool ShouldArcaneSurge(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ARCANE_SURGE)) return false;
    if (!ctx.bot.is_ready(ARCANE_SURGE)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(ARCANE_SURGE)) return false;
    // Damage is proportional to mana spent - never fire it half empty.
    return ctx.bot.power_pct(0) >= 60;
}
void DoArcaneSurge(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ARCANE_SURGE, ctx.bot.victim());
}

bool ShouldPresenceOfMind(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(PRESENCE_OF_MIND)) return false;
    if (!ctx.bot.is_ready(PRESENCE_OF_MIND)) return false;
    // Stack with the Arcane Surge window, or use to clip movement. Either
    // condition fires it - the buff lingers until 2 Arcane Blasts are used.
    return ctx.bot.has_aura(ARCANE_SURGE_BUFF) || ctx.bot.is_moving();
}
void DoPresenceOfMind(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(PRESENCE_OF_MIND); }

// Evocation (12.1) is an instant 3s mana-regen burst, not a channel. Fire
// it when the tank is low - including right after Arcane Surge emptied it.
bool ShouldEvocation(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(EVOCATION)) return false;
    if (!ctx.bot.is_ready(EVOCATION)) return false;
    return ctx.bot.power_pct(0) <= 30;
}
void DoEvocation(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(EVOCATION); }

// ---- AoE ----
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

// AoE cluster gate shared by Arcane Pulse / Arcane Explosion. aoe_preference
// is a soft owner hint; still require >= 2 enemies so a stale `.aoe on`
// doesn't fire on a single boss pull.
bool AoeClusterNear(ApPredicateContext const& ctx, float range)
{
    const int near = static_cast<int>(ctx.bot.enemies_within(range));
    return near >= 3 || (ctx.aoe_preference && near >= 2);
}

// Arcane Pulse (talent) replaces Arcane Explosion: 2s cast, 15s CD, AoE
// around the TARGET that generates a charge per enemy hit. Two-branch with
// Arcane Explosion below so untalented bots keep their PBAoE builder.
bool ShouldArcanePulse(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ARCANE_PULSE)) return false;
    if (!ctx.bot.is_ready(ARCANE_PULSE)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(ARCANE_PULSE)) return false;
    return AoeClusterNear(ctx, 10.0f);
}
void DoArcanePulse(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ARCANE_PULSE, ctx.bot.victim());
}

bool ShouldArcaneExplosion(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(ARCANE_PULSE)) return false;   // overridden by Pulse
    if (!ctx.bot.knows_spell(ARCANE_EXPLOSION)) return false;
    // PBAoE charge builder - only when 3+ enemies are in its 10yd hit box,
    // so we don't waste mana on single-target.
    return AoeClusterNear(ctx, 10.0f);
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

// ---- Rule table ----
//
// Ordering matches the documented Mage spec ordering (most-urgent first):
//   1. Survival panic (Ice Block / Greater Invisibility)
//   2. Defensives (Prismatic Barrier / Mirror Image / Alter Time /
//      Shimmer or Blink)
//   3. Interrupt (Counterspell)
//   4. Kite (Frost Nova)
//   5. CC (Polymorph) / utility (Spellsteal)
//   6. Major offensive CDs (Time Warp / Touch of the Magi / Arcane Surge
//      / Presence of Mind / Evocation)
//   7. Procs (Arcane Missiles via Clearcasting)
//   8. AoE (Arcane Pulse or Arcane Explosion)
//   9. Charge spender (Arcane Barrage)
//  10. ST builder (Arcane Orb / Arcane Blast)
//  11. Filler (Auto Attack)
ApRule const kRules[] = {
    // 1. Survival panic
    { ShouldIceBlock,        DoIceBlock,        "Ice Block (panic <=20% / <=40% PvP)" },
    { ShouldGreaterInvis,    DoGreaterInvis,    "Greater Invis (threat wipe)"   },
    // 2. Defensives
    { ShouldPrismaticBarrier,DoPrismaticBarrier,"Prismatic Barrier (shield)"    },
    { ShouldMirrorImage,     DoMirrorImage,     "Mirror Image (threat / boss)"  },
    { ShouldAlterTime,       DoAlterTime,       "Alter Time (snapshot HP)"      },
    { ShouldShimmerAway,     DoShimmer,         "Shimmer (escape melee)"        },
    { ShouldBlinkAway,       DoBlink,           "Blink (escape melee)"          },
    // 3. Interrupt
    { ShouldCounterspell,    DoCounterspell,    "Counterspell (interrupt)"      },
    // 4. Kite
    { ShouldFrostNova,       DoFrostNova,       "Frost Nova (kite melee)"       },
    // 5. CC / utility
    { ShouldPolymorph,       DoPolymorph,       "Polymorph (off-target CC)"     },
    { ShouldSpellsteal,      DoSpellsteal,      "Spellsteal (Magic buff)"       },
    // 6. Major offensive CDs
    { ShouldTimeWarp,        DoTimeWarp,        "Time Warp (boss)"              },
    { ShouldTouchOfTheMagi,  DoTouchOfTheMagi,  "Touch of the Magi (window)"    },
    { ShouldArcaneSurge,     DoArcaneSurge,     "Arcane Surge (burst)"          },
    { ShouldPresenceOfMind,  DoPresenceOfMind,  "Presence of Mind (instant)"    },
    { ShouldEvocation,       DoEvocation,       "Evocation (<=30% mana)"        },
    // 7. Procs (free spender - fire ASAP before it expires)
    { ShouldArcaneMissiles,  DoArcaneMissiles,  "Arcane Missiles (Clearcast)"   },
    // 8. AoE (Arcane Pulse overrides Arcane Explosion when talented)
    { ShouldArcanePulse,     DoArcanePulse,     "Arcane Pulse (3+ AoE)"         },
    { ShouldArcaneExplosion, DoArcaneExplosion, "Arcane Explosion (3+ AoE)"     },
    // 9. Charge spender (4-stack dump / mana-low refund)
    { ShouldArcaneBarrage,   DoArcaneBarrage,   "Arcane Barrage (spend 4)"      },
    // 10. ST builders / filler
    { ShouldArcaneOrb,       DoArcaneOrb,       "Arcane Orb (charge gen)"       },
    { ShouldArcaneBlast,     DoArcaneBlast,     "Arcane Blast (build)"          },
    // 11. Auto attack fallback
    { AlwaysInCombat,        DoAutoAttack,      "Engage auto attack"            },
};

} // anonymous

void RegisterApl_Mage_Arcane()
{
    constexpr uint32 SPEC_MAGE_ARCANE = 62;
    RegisterRotation(CLASS_MAGE, SPEC_MAGE_ARCANE, ApRotation{kRules});
}

} // namespace Playerbot::Combat
