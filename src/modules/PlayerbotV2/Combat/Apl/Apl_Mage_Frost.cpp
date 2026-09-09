// Frost Mage - WoW 12.1.0.69587 (Midnight) rotation. Caster DPS, mana
// resource. Midnight reworked the shatter loop around FREEZING stacks:
// Frostbolt / Flurry apply Freezing (debuff 1221389) to the target, Ice Lance
// Shatters those stacks for bonus damage, Fingers of Frost lets Ice Lance
// Shatter without consuming stacks, Brain Freeze resets Flurry. Icicles now
// auto-upgrade Frostbolt into Glacial Spike, so no separate Glacial Spike
// cast exists. Ray of Frost is a core rotational channel, Frozen Orb the
// AoE / proc engine, Blizzard the 3+ AoE.
//
// ---- Validated spell IDs (WoW 12.1.0.69587) ----
//   116    Frostbolt              431044 Frostfire Bolt (hero, overrides Frostbolt)
//   30455  Ice Lance (talent)     431177 Frostfire Empowerment (proc aura)
//   44614  Flurry (talent)        84714  Frozen Orb (talent)
//   205021 Ray of Frost (talent)  190356 Blizzard (talent)
//   11426  Ice Barrier (talent)   235219 Cold Snap (talent)
//   113724 Ring of Frost          120    Cone of Cold
//   157997 Ice Nova (overrides Cone of Cold)
//   190446 Brain Freeze (proc)    44544  Fingers of Frost (proc)
//   1221389 Freezing (debuff)     55342  Mirror Image (talent)
//   342245 Alter Time (talent)    45438  Ice Block (talent)
//   110959 Greater Invisibility   110960 Greater Invisibility (aura)
//   1953   Blink                  212653 Shimmer (talent, overrides Blink)
//   2139   Counterspell           475    Remove Curse
//   122    Frost Nova             118    Polymorph
//   80353  Time Warp
//
// ---- Skipped spells (and why) ----
//   199786 Glacial Spike          - no longer a castable; Icicles (1246832)
//                                   upgrade Frostbolt automatically.
//   153595 / 1247777 Comet Storm  - 12.1 Comet Storm is a PASSIVE that
//                                   replaces Ray of Frost; not in the
//                                   curated build. Ray of Frost rule covers.
//   257537 / 214634 Ebonbolt      - 214634 is a Legion artifact spell
//                                   (no learn level); never granted.
//   12472  Icy Veins              - removed from the Frost tree.
//   108839 Ice Floes              - removed from the Mage class tree.
//   228358 Winter's Chill         - replaced by Freezing stacks.
//   205473 Icicles (old aura)     - Glacial Spike is automatic now.
//   31687  Summon Water Elemental - Lonely Winter [R][M] forgoes the pet.
//   1248829 Blizzard (target)     - alt variant; we cast_at with 190356.
//   157980 Supernova / 383121 Mass Polymorph - not in the raid build.

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
constexpr uint32 FROSTBOLT          = 116;
constexpr uint32 FROSTFIRE_BOLT     = 431044;     // hero talent - replaces Frostbolt
constexpr uint32 ICE_LANCE          = 30455;      // Shatters Freezing stacks
constexpr uint32 FLURRY             = 44614;      // applies Freezing stacks
constexpr uint32 FROZEN_ORB         = 84714;
constexpr uint32 ICE_BARRIER        = 11426;
constexpr uint32 RAY_OF_FROST       = 205021;     // talent - 4s channel, applies Freezing
constexpr uint32 MIRROR_IMAGE       = 55342;      // 3x clones, 2min CD
constexpr uint32 COLD_SNAP          = 235219;     // resets Ice Barrier / Frost Nova / Ice Block
constexpr uint32 RING_OF_FROST      = 113724;     // 10s AoE incapacitate, 45s CD
constexpr uint32 POLYMORPH          = 118;        // CC - sheep humanoid/beast/etc.
constexpr uint32 ALTER_TIME         = 342245;     // snapshots HP/position; reverts after 10s

// Procs / triggers
constexpr uint32 BRAIN_FREEZE       = 190446;     // resets Flurry, empowers next Flurry
constexpr uint32 FINGERS_OF_FROST   = 44544;      // next Ice Lance Shatters without consuming Freezing
constexpr uint32 FREEZING           = 1221389;    // debuff on target - stacks, Shattered by Ice Lance
constexpr uint32 FROSTFIRE_EMPOWER  = 431177;     // proc aura - next Frostfire Bolt instant

constexpr uint32 COUNTERSPELL       = 2139;       // 40yd interrupt
constexpr uint32 REMOVE_CURSE       = 475;        // friendly curse dispel, 8s CD
constexpr uint32 BLIZZARD           = 190356;
constexpr uint32 CONE_OF_COLD       = 120;
constexpr uint32 ICE_NOVA           = 157997;     // talent - replaces Cone of Cold, targeted freeze
constexpr uint32 FROST_NOVA         = 122;
constexpr uint32 ICE_BLOCK          = 45438;      // 10s immunity, 4min CD
constexpr uint32 GREATER_INVIS      = 110959;     // threat wipe, 2min CD
constexpr uint32 GREATER_INVIS_AURA = 110960;     // active invisibility aura
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
    // Resets Ice Block / Ice Barrier / Frost Nova. Worth firing when
    // Ice Block is on CD AND we're going to need it (low HP).
    return ctx.bot.hp_pct() <= 25 && !ctx.bot.is_ready(ICE_BLOCK);
}
void DoColdSnap(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(COLD_SNAP); }

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
bool ShouldTimeWarp(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(TIME_WARP)) return false;
    if (!ctx.bot.is_ready(TIME_WARP)) return false;
    if (BotHasSatedDebuff(ctx)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoTimeWarp(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TIME_WARP); }

// Ray of Frost (12.1) is a core rotational 4s channel that stacks Freezing
// on the target and (Spellslinger) conjures Splinters. Fire it on cooldown
// whenever we can stand still; the channel drops on the first step.
bool ShouldRayOfFrost(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RAY_OF_FROST)) return false;
    if (!ctx.bot.is_ready(RAY_OF_FROST)) return false;
    return !ctx.bot.is_moving();
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

bool ShouldIceLance(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ICE_LANCE)) return false;
    if (!ctx.bot.is_ready(ICE_LANCE)) return false;
    // Spend Fingers of Frost charges (Shatter without consuming Freezing).
    // Otherwise Shatter once the target carries a Freezing stack pile from
    // Flurry / Frostbolt hits - 3 is roughly one Flurry's worth, so the bot
    // alternates builder and Shatter instead of lancing every other GCD.
    if (ctx.bot.has_aura(FINGERS_OF_FROST)) return true;
    if (ctx.bot.aura_stacks(FREEZING, ctx.bot.victim()) >= 3) return true;
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

// Ice Nova (talent) replaces Cone of Cold: targeted 40y freeze + AoE around
// the victim. Two-branch with Cone of Cold so untalented bots keep the cone.
bool ShouldIceNova(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ICE_NOVA)) return false;
    if (!ctx.bot.is_ready(ICE_NOVA)) return false;
    return ctx.bot.enemies_within(8.0f) >= 2;
}
void DoIceNova(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(ICE_NOVA, ctx.bot.victim()); }

bool ShouldConeOfCold(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(ICE_NOVA)) return false;   // overridden by Ice Nova
    if (!ctx.bot.knows_spell(CONE_OF_COLD)) return false;
    if (!ctx.bot.is_ready(CONE_OF_COLD)) return false;
    // Short cone in front - usable only when 3+ enemies in melee range.
    return ctx.bot.enemies_within(8.0f) >= 3;
}
void DoConeOfCold(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CONE_OF_COLD); }

// ---- Filler ----
// Frostfire Bolt (hero talent) replaces Frostbolt and still applies Freezing.
// Frostfire Empowerment makes the next one instant, so fire it even while
// moving under the proc.
bool ShouldFrostfireBolt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FROSTFIRE_BOLT)) return false;
    if (ctx.bot.has_aura(FROSTFIRE_EMPOWER)) return true;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(FROSTFIRE_BOLT)) return false;
    return true;
}
void DoFrostfireBolt(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(FROSTFIRE_BOLT, ctx.bot.victim()); }

bool ShouldFrostbolt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(FROSTFIRE_BOLT)) return false;   // overridden
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

// ---- Rule table ----
//
// Ordering matches the documented Mage spec ordering (most-urgent first):
//   1. Survival panic (Ice Block + Cold Snap to reset it / Greater Invis)
//   2. Defensives (Mirror Image / Alter Time / Ice Barrier / Shimmer or
//      Blink)
//   3. Interrupt (Counterspell) + group dispel (Remove Curse)
//   4. Kite (Frost Nova / Ring of Frost / Ice Nova or Cone of Cold)
//   5. CC (Polymorph - healer-pref off-target sheep)
//   6. Major offensive CDs (Time Warp / Frozen Orb / Ray of Frost)
//   7. Procs - Frost's Freezing / Shatter loop:
//      a. Flurry (Brain Freeze proc - stacks Freezing on the target)
//      b. Ice Lance (Fingers of Frost or 3+ Freezing stacks -> Shatter;
//         also moving-fallback when the bolt is locked)
//   8. AoE (Blizzard at 3+)
//   9. Filler (Frostfire Bolt or Frostbolt - auto-upgrades to Glacial
//      Spike at max Icicles)
//  10. Auto-attack fallback
//
// Critical Frost mechanic: Ice Lance is gated by Freezing / proc state
// (Fingers of Frost aura 44544, Freezing stacks 1221389 on the target, or
// moving-fallback). Never an unconditional ST spam - that would waste
// mana on a low-damage cast lacking the Shatter multiplier.
//
// Critical proc priority: Brain Freeze MUST be consumed by Flurry BEFORE
// Ice Lance so the Freezing stacks are on the target when the lance lands.
ApRule const kRules[] = {
    // 1. Survival panic
    { ShouldIceBlock,      DoIceBlock,      "Ice Block (panic <=20% / <=40% PvP)" },
    { ShouldColdSnap,      DoColdSnap,      "Cold Snap (reset Ice Block)" },
    { ShouldGreaterInvis,  DoGreaterInvis,  "Greater Invis (threat wipe)" },
    // 2. Defensives
    { ShouldMirrorImage,   DoMirrorImage,   "Mirror Image (threat / boss)" },
    { ShouldAlterTime,     DoAlterTime,     "Alter Time (snapshot HP)"   },
    { ShouldIceBarrier,    DoIceBarrier,    "Ice Barrier (defensive)"    },
    { ShouldShimmerAway,   DoShimmer,       "Shimmer (escape melee)"     },
    { ShouldBlinkAway,     DoBlink,         "Blink (escape melee)"       },
    // 3. Interrupt / dispel
    { ShouldCounterspell,  DoCounterspell,  "Counterspell (interrupt)"   },
    { ShouldRemoveCurse,   DoRemoveCurse,   "Remove Curse (group)"       },
    // 4. Kite (Ice Nova overrides Cone of Cold when talented)
    { ShouldFrostNova,     DoFrostNova,     "Frost Nova (root attackers)" },
    { ShouldRingOfFrost,   DoRingOfFrost,   "Ring of Frost (3+ approaching)" },
    { ShouldIceNova,       DoIceNova,       "Ice Nova (2+ freeze)"       },
    { ShouldConeOfCold,    DoConeOfCold,    "Cone of Cold (3+ melee)"    },
    // 5. CC
    { ShouldPolymorph,     DoPolymorph,     "Polymorph (off-target CC, healer-pref)" },
    // 6. Major offensive CDs
    { ShouldTimeWarp,      DoTimeWarp,      "Time Warp (boss)"           },
    { ShouldFrozenOrb,     DoFrozenOrb,     "Frozen Orb (cooldown)"      },
    { ShouldRayOfFrost,    DoRayOfFrost,    "Ray of Frost (channel)"     },
    // 7. Procs (Flurry first so Freezing stacks land before the lance)
    { ShouldFlurry,        DoFlurry,        "Flurry (Brain Freeze)"      },
    { ShouldIceLance,      DoIceLance,      "Ice Lance (FoF / Shatter)"  },
    // 8. AoE
    { ShouldBlizzard,      DoBlizzard,      "Blizzard (3+ targets)"      },
    // 9. Filler (Frostfire Bolt overrides Frostbolt when talented)
    { ShouldFrostfireBolt, DoFrostfireBolt, "Frostfire Bolt (filler)"    },
    { ShouldFrostbolt,     DoFrostbolt,     "Frostbolt (filler)"         },
    // 10. Auto attack fallback
    { AlwaysInCombat,      DoAutoAttack,    "Engage auto attack"         },
};

} // anonymous

void RegisterApl_Mage_Frost()
{
    constexpr uint32 SPEC_MAGE_FROST = 64;
    RegisterRotation(CLASS_MAGE, SPEC_MAGE_FROST, ApRotation{kRules});
}

} // namespace Playerbot::Combat
