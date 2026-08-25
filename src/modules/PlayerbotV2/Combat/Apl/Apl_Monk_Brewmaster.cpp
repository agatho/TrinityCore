// Brewmaster Monk - WoW 12.0 enterprise rotation. Tank archetype built around
// Stagger management: damage taken is delayed via Stagger debuff (Light/Mod/
// Heavy), and Purifying Brew clears 50% of remaining stagger when cast. The
// active mitigation layer is Celestial Brew (absorb shield); the passive
// rotation drives brew charge regeneration via Keg Smash + Tiger Palm.
//
// Survival ladder: Zen Meditation (90% spell DR) -> Fortifying Brew (20% HP
// +20% DR) -> Dampen Harm (large-hit reducer) -> Diffuse Magic (60% magic DR
// + reflect) -> Expel Harm self-heal.
//
// Threat: Provoke (ranged taunt) + Keg Smash (huge initial threat + reduces
// brew CDs) + Spinning Crane Kick / Breath of Fire AoE.
//
// Group utility: Ring of Peace (displacement), Leg Sweep (AoE stun),
// Paralysis (off-target CC), Spear Hand Strike (melee interrupt),
// Tiger's Lust (friendly speed), Detox (Magic/Disease/Poison cleanse).
//
// Major CDs: Invoke Niuzao (tank pet, pulls stagger), Weapons of Order
// (mastery + CD reduction), Exploding Keg / Bonedust Brew / Black Ox Brew.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated against SpellName.csv) ----
// Validated IDs:
//   121253 Keg Smash           107428 Rising Sun Kick           115181 Breath of Fire
//   205523 Blackout Kick (BrM) 100780 Tiger Palm                 101546 Spinning Crane Kick
//   116847 Rushing Jade Wind   325153 Exploding Keg              386276 Bonedust Brew
//   115399 Black Ox Brew       322109 Touch of Death (generic)   322101 Expel Harm (generic)
//   119582 Purifying Brew      322507 Celestial Brew             325092 Purified Chi
//   115203 Fortifying Brew     122278 Dampen Harm                122783 Diffuse Magic
//   115176 Zen Meditation      132578 Invoke Niuzao              387184 Weapons of Order
//   123904 Invoke Xuen         115546 Provoke                    116705 Spear Hand Strike
//   119381 Leg Sweep           116844 Ring of Peace              115078 Paralysis
//   116841 Tiger's Lust        218164 Detox (BrM)                115178 Resuscitate
//   124273 Heavy Stagger       124274 Moderate Stagger           124275 Light Stagger
//   215479 Shuffle (proc'd)    123725 Breath of Fire DoT         116670 Vivify
//   115098 Chi Wave            123986 Chi Burst
//
// Skipped (with reason):
//   115069 Stagger             passive damage-delay mechanic; not a castable —
//                              the rotation only OBSERVES stagger via the band
//                              auras (124273/4/5) to gate Purifying Brew.
//   322729 Spinning Crane Kick BrM spec-variant id; player gets the generic
//                              101546 in spellbook, so generic suffices.
//   231602 Improved Vivify     passive talent (cast-while-moving + heal-on-self),
//                              not castable.
//   322102 Expel Harm          alias of 322101 (same name + effect); using
//                              the canonical 322101 the spec already validates.
//   216519 Celestial Fortune   passive crit-heal-on-tank proc.
//   325095 Touch of Death      BrM spec-variant; generic 322109 routes correctly
//                              through the cast handler.
constexpr uint32 KEG_SMASH            = 121253;
constexpr uint32 BLACKOUT_KICK_BRM    = 205523;       // Brewmaster variant (auto-resets via Blackout Combo)
constexpr uint32 TIGER_PALM           = 100780;
constexpr uint32 RISING_SUN_KICK_BRM  = 107428;       // Brewmaster talent variant id
constexpr uint32 BREATH_OF_FIRE       = 115181;
constexpr uint32 SPINNING_CRANE_KICK  = 101546;
constexpr uint32 RUSHING_JADE_WIND    = 116847;       // talent
constexpr uint32 EXPLODING_KEG        = 325153;       // talent — AoE blind + dot
constexpr uint32 BONEDUST_BREW        = 386276;       // talent — cleave
constexpr uint32 BLACK_OX_BREW        = 115399;       // talent — resets brew charges + energy
constexpr uint32 TOUCH_OF_DEATH       = 322109;       // execute (<=15% target HP or target.max_hp <= bot.max_hp)
constexpr uint32 EXPEL_HARM           = 322101;       // self heal that consumes Healing Spheres
constexpr uint32 VIVIFY               = 116670;       // emergency self heal
constexpr uint32 CHI_WAVE             = 115098;       // talent — light heal/dmg
constexpr uint32 CHI_BURST            = 123986;       // talent — AoE heal/dmg

// Brews / mitigation
constexpr uint32 PURIFYING_BREW       = 119582;
constexpr uint32 CELESTIAL_BREW       = 322507;
constexpr uint32 PURIFIED_CHI_AURA    = 325092;       // Celestial Brew shield magnitude buff
constexpr uint32 FORTIFYING_BREW      = 115203;
constexpr uint32 DAMPEN_HARM          = 122278;
constexpr uint32 DIFFUSE_MAGIC        = 122783;
constexpr uint32 ZEN_MEDITATION       = 115176;

// Major cooldowns
constexpr uint32 INVOKE_NIUZAO        = 132578;
constexpr uint32 WEAPONS_OF_ORDER     = 387184;       // talent
constexpr uint32 INVOKE_XUEN          = 123904;       // optional offensive talent

// Threat / interrupt / CC
constexpr uint32 PROVOKE              = 115546;
constexpr uint32 SPEAR_HAND_STRIKE    = 116705;
constexpr uint32 LEG_SWEEP            = 119381;
constexpr uint32 RING_OF_PEACE        = 116844;
constexpr uint32 PARALYSIS            = 115078;
constexpr uint32 TIGERS_LUST          = 116841;

// Group utility
constexpr uint32 DETOX                = 218164;       // BrM cleanse (Magic via talent + Disease + Poison)
constexpr uint32 RESUSCITATE          = 115178;       // OOC rez

// Stagger debuffs — bot self-aura tracks current stagger band.
constexpr uint32 LIGHT_STAGGER        = 124275;
constexpr uint32 MODERATE_STAGGER     = 124274;
constexpr uint32 HEAVY_STAGGER        = 124273;

// Buff trackers
constexpr uint32 SHUFFLE_AURA         = 215479;       // proc'd by Keg Smash + BoK — extends stagger
constexpr uint32 BREATH_OF_FIRE_DOT   = 123725;
constexpr uint32 RUSHING_JADE_WIND_AURA = 116847;

constexpr uint8  POWER_ENERGY_IDX = 3;

// ---- Helpers ----
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

bool InHeavyOrModStagger(ApPredicateContext const& ctx)
{
    return ctx.bot.has_aura(HEAVY_STAGGER) || ctx.bot.has_aura(MODERATE_STAGGER);
}

// ---- Threat ----
bool ShouldProvoke(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(PROVOKE)) return false;
    if (!ctx.bot.is_ready(PROVOKE)) return false;
    return ctx.bot.untaunted_enemy() != nullptr;
}
void DoProvoke(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.untaunted_enemy())
        e.cast(PROVOKE, t->guid);
}

// ---- Interrupt cascade ----
bool ShouldSpearHandStrike(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SPEAR_HAND_STRIKE)) return false;
    if (!ctx.bot.is_ready(SPEAR_HAND_STRIKE)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (pvp) return ctx.bot.kick_target(true, 5.0f) != nullptr;
    auto const* c = ctx.bot.interruptible_caster();
    return c && c->guid == ctx.bot.victim();
}
void DoSpearHandStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 5.0f))
        e.cast(SPEAR_HAND_STRIKE, c->guid);
}

bool ShouldParalysisOffTarget(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(PARALYSIS)) return false;
    if (!ctx.bot.is_ready(PARALYSIS)) return false;
    // CC a non-target caster (e.g. healer add) when interrupts are unavailable.
    auto const* c = ctx.bot.interruptible_caster();
    if (!c || c->guid == ctx.bot.victim()) return false;
    return !ctx.bot.is_ready(SPEAR_HAND_STRIKE);
}
void DoParalysisOffTarget(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* c = ctx.bot.interruptible_caster())
        e.cast(PARALYSIS, c->guid);
}

// ---- AoE CC ----
bool ShouldLegSweep(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(LEG_SWEEP)) return false;
    if (!ctx.bot.is_ready(LEG_SWEEP)) return false;
    return ctx.bot.attackers_count() >= 3;
}
void DoLegSweep(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(LEG_SWEEP); }

bool ShouldRingOfPeace(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(RING_OF_PEACE)) return false;
    if (!ctx.bot.is_ready(RING_OF_PEACE)) return false;
    // Panic displace when overwhelmed at low HP — peels healer effectively.
    return ctx.bot.attackers_count() >= 4 && ctx.bot.hp_pct() <= 40;
}
void DoRingOfPeace(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    float bx, by, bz;
    ctx.bot.position(bx, by, bz);
    e.cast_at(RING_OF_PEACE, bx, by, bz);
}

// ---- Survival ladder ----
bool ShouldZenMeditation(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ZEN_MEDITATION)) return false;
    if (!ctx.bot.is_ready(ZEN_MEDITATION)) return false;
    // 90% spell DR but breaks on melee — only useful when an incoming spell
    // would otherwise crush us. Gate strictly on caster + low HP to avoid
    // wasting a 5min CD.
    if (ctx.bot.hp_pct() > 45) return false;
    return ctx.bot.interruptible_caster() != nullptr;
}
void DoZenMeditation(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ZEN_MEDITATION); }

bool ShouldFortifyingBrew(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(FORTIFYING_BREW)) return false;
    if (!ctx.bot.is_ready(FORTIFYING_BREW)) return false;
    return ctx.bot.hp_pct() <= 30;
}
void DoFortifyingBrew(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FORTIFYING_BREW); }

bool ShouldDampenHarm(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DAMPEN_HARM)) return false;
    if (!ctx.bot.is_ready(DAMPEN_HARM)) return false;
    // Save it for either pre-emptive boss damage or sub-50% reactive use.
    if (ctx.bot.hp_pct() <= 50) return true;
    return BossLikeTargetEngaged(ctx) && ctx.bot.hp_pct() <= 75;
}
void DoDampenHarm(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DAMPEN_HARM); }

bool ShouldDiffuseMagic(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DIFFUSE_MAGIC)) return false;
    if (!ctx.bot.is_ready(DIFFUSE_MAGIC)) return false;
    if (ctx.bot.hp_pct() > 55) return false;
    return ctx.bot.interruptible_caster() != nullptr;
}
void DoDiffuseMagic(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DIFFUSE_MAGIC); }

bool ShouldExpelHarm(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(EXPEL_HARM)) return false;
    if (!ctx.bot.is_ready(EXPEL_HARM)) return false;
    if (ctx.bot.power(POWER_ENERGY_IDX) < 15) return false;
    return ctx.bot.hp_pct() <= 80;
}
void DoExpelHarm(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(EXPEL_HARM); }

bool ShouldVivifyPanic(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(VIVIFY)) return false;
    if (!ctx.bot.is_ready(VIVIFY)) return false;
    return ctx.bot.hp_pct() <= 25;
}
void DoVivifyPanic(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(VIVIFY, ctx.bot.raw().guid);
}

// ---- Active mitigation: Celestial Brew + Purifying Brew ----
bool ShouldCelestialBrew(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(CELESTIAL_BREW)) return false;
    if (!ctx.bot.is_ready(CELESTIAL_BREW)) return false;
    if (ctx.bot.has_aura(CELESTIAL_BREW)) return false;
    // Pop reactively at <=65%, but only if we don't already have an absorb.
    return ctx.bot.hp_pct() <= 65;
}
void DoCelestialBrew(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CELESTIAL_BREW); }

bool ShouldPurifyingBrew(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(PURIFYING_BREW)) return false;
    if (!ctx.bot.is_ready(PURIFYING_BREW)) return false;
    // Only burn a charge when stagger is meaningful — clears 50% of pool.
    // Heavy stagger always; Moderate stagger when below 75% HP.
    if (ctx.bot.has_aura(HEAVY_STAGGER)) return true;
    if (ctx.bot.has_aura(MODERATE_STAGGER) && ctx.bot.hp_pct() <= 75) return true;
    return false;
}
void DoPurifyingBrew(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(PURIFYING_BREW); }

bool ShouldBlackOxBrew(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(BLACK_OX_BREW)) return false;
    if (!ctx.bot.is_ready(BLACK_OX_BREW)) return false;
    // Resets brew charges + refunds energy. Use when out of brews and taking
    // damage, OR when energy-starved and Keg Smash is on CD.
    if (!ctx.bot.is_ready(PURIFYING_BREW) && InHeavyOrModStagger(ctx)) return true;
    if (!ctx.bot.is_ready(CELESTIAL_BREW) && ctx.bot.hp_pct() <= 50) return true;
    return false;
}
void DoBlackOxBrew(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLACK_OX_BREW); }

// ---- Major offensive cooldowns ----
bool ShouldInvokeNiuzao(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(INVOKE_NIUZAO)) return false;
    if (!ctx.bot.is_ready(INVOKE_NIUZAO)) return false;
    // Niuzao stomps + drains stagger — pop on bosses or when overwhelmed.
    return BossLikeTargetEngaged(ctx) || ctx.bot.attackers_count() >= 4;
}
void DoInvokeNiuzao(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(INVOKE_NIUZAO); }

bool ShouldWeaponsOfOrder(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(WEAPONS_OF_ORDER)) return false;
    if (!ctx.bot.is_ready(WEAPONS_OF_ORDER)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.attackers_count() >= 3;
}
void DoWeaponsOfOrder(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(WEAPONS_OF_ORDER); }

bool ShouldInvokeXuen(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(INVOKE_XUEN)) return false;
    if (!ctx.bot.is_ready(INVOKE_XUEN)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoInvokeXuen(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(INVOKE_XUEN); }

bool ShouldBonedustBrew(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BONEDUST_BREW)) return false;
    if (!ctx.bot.is_ready(BONEDUST_BREW)) return false;
    return ctx.bot.attackers_count() >= 2 || BossLikeTargetEngaged(ctx);
}
void DoBonedustBrew(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.victim_info())
    {
        e.cast_at(BONEDUST_BREW, t->x, t->y, t->z);
    }
    else
    {
        float bx, by, bz;
        ctx.bot.position(bx, by, bz);
        e.cast_at(BONEDUST_BREW, bx, by, bz);
    }
}

bool ShouldExplodingKeg(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(EXPLODING_KEG)) return false;
    if (!ctx.bot.is_ready(EXPLODING_KEG)) return false;
    return ctx.bot.attackers_count() >= 3 || BossLikeTargetEngaged(ctx);
}
void DoExplodingKeg(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.victim_info())
        e.cast_at(EXPLODING_KEG, t->x, t->y, t->z);
    else
        e.cast(EXPLODING_KEG, ctx.bot.victim());
}

// ---- Execute ----
// Modern Touch of Death fires when EITHER the target is below 15% HP OR the
// target's max HP is no greater than the bot's max HP (the "instant kill" cap
// against small mobs). Both branches are valid execute windows.
bool ShouldTouchOfDeath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(TOUCH_OF_DEATH)) return false;
    if (!ctx.bot.is_ready(TOUCH_OF_DEATH)) return false;
    NearbyUnit const* t = ctx.bot.victim_info();
    if (!t || t->max_hp <= 0) return false;
    const int32 hp_pct = static_cast<int32>((int64_t(t->hp) * 100) / t->max_hp);
    if (hp_pct <= 15) return true;
    // HP-cap branch: bot can ToD anything its own max HP exceeds.
    if (t->max_hp <= ctx.bot.max_hp()) return true;
    return false;
}
void DoTouchOfDeath(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(TOUCH_OF_DEATH, ctx.bot.victim());
}

// ---- Group utility ----
bool ShouldDetox(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(DETOX)) return false;
    if (!ctx.bot.is_ready(DETOX)) return false;
    auto const* m = ctx.group.dispel_candidate(Playerbot::DispelType::Disease);
    if (!m) m = ctx.group.dispel_candidate(Playerbot::DispelType::Poison);
    if (!m) m = ctx.group.dispel_candidate(Playerbot::DispelType::Magic);
    return m != nullptr;
}
void DoDetox(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    auto const* m = ctx.group.dispel_candidate(Playerbot::DispelType::Disease);
    if (!m) m = ctx.group.dispel_candidate(Playerbot::DispelType::Poison);
    if (!m) m = ctx.group.dispel_candidate(Playerbot::DispelType::Magic);
    if (m) e.cast(DETOX, m->guid);
}

// ---- Damage / threat rotation ----
bool ShouldKegSmash(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(KEG_SMASH)) return false;
    if (!ctx.bot.is_ready(KEG_SMASH)) return false;
    return ctx.bot.power(POWER_ENERGY_IDX) >= 40;
}
void DoKegSmash(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(KEG_SMASH, ctx.bot.victim());
}

bool ShouldBreathOfFire(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BREATH_OF_FIRE)) return false;
    if (!ctx.bot.is_ready(BREATH_OF_FIRE)) return false;
    // Only refresh if DoT missing or expiring — keeps the BoF cone honest.
    AuraEntry const* a = ctx.bot.find_aura(BREATH_OF_FIRE_DOT, ctx.bot.victim());
    return !a || a->remaining.count() <= 3000;
}
void DoBreathOfFire(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BREATH_OF_FIRE, ctx.bot.victim());
}

bool ShouldRisingSunKick(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RISING_SUN_KICK_BRM)) return false;
    return ctx.bot.is_ready(RISING_SUN_KICK_BRM);
}
void DoRisingSunKick(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RISING_SUN_KICK_BRM, ctx.bot.victim());
}

bool ShouldRushingJadeWind(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RUSHING_JADE_WIND)) return false;
    if (!ctx.bot.is_ready(RUSHING_JADE_WIND)) return false;
    if (ctx.bot.attackers_count() < 2) return false;
    return !ctx.bot.has_aura(RUSHING_JADE_WIND_AURA);
}
void DoRushingJadeWind(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(RUSHING_JADE_WIND); }

bool ShouldSpinningCraneKick(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SPINNING_CRANE_KICK)) return false;
    if (ctx.bot.power(POWER_ENERGY_IDX) < 25) return false;
    return ctx.aoe_preference || ctx.bot.attackers_count() >= 3;
}
void DoSpinningCraneKick(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SPINNING_CRANE_KICK); }

bool ShouldBlackoutKick(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BLACKOUT_KICK_BRM)) return false;
    return ctx.bot.is_ready(BLACKOUT_KICK_BRM);
}
void DoBlackoutKick(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BLACKOUT_KICK_BRM, ctx.bot.victim());
}

bool ShouldTigerPalm(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(TIGER_PALM)) return false;
    return ctx.bot.power(POWER_ENERGY_IDX) >= 25;
}
void DoTigerPalm(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(TIGER_PALM, ctx.bot.victim());
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
// Canonical Brewmaster priority order:
//   Survival ladder (Fortifying Brew -> Celestial Brew -> Purifying Brew on
//     heavy stagger -> Zen Meditation against casters -> Diffuse Magic ->
//     Dampen Harm -> Vivify panic -> Expel Harm) FIRST,
//   then Threat (Provoke), Interrupts (Spear Hand / Paralysis / Leg Sweep /
//     Ring of Peace), Dispel (Detox),
//   then offensive CDs (Niuzao / Weapons of Order / Xuen / Exploding Keg /
//     Bonedust Brew / Black Ox Brew / Touch of Death),
//   then the steady rotation (Keg Smash -> Breath of Fire -> AoE -> Blackout
//     Kick -> Rising Sun Kick -> Tiger Palm filler),
//   finally auto-attack.
ApRule const kRules[] = {
    { ShouldFortifyingBrew,    DoFortifyingBrew,    "Fortifying Brew (<=30%)"         },
    { ShouldCelestialBrew,     DoCelestialBrew,     "Celestial Brew (<=65% absorb)"   },
    { ShouldPurifyingBrew,     DoPurifyingBrew,     "Purifying Brew (clear stagger)"  },
    { ShouldBlackOxBrew,       DoBlackOxBrew,       "Black Ox Brew (reset charges)"   },
    { ShouldZenMeditation,     DoZenMeditation,     "Zen Meditation (caster <=45%)"   },
    { ShouldDiffuseMagic,      DoDiffuseMagic,      "Diffuse Magic (caster <=55%)"    },
    { ShouldDampenHarm,        DoDampenHarm,        "Dampen Harm (boss/<=50%)"        },
    { ShouldVivifyPanic,       DoVivifyPanic,       "Vivify (panic heal)"             },
    { ShouldExpelHarm,         DoExpelHarm,         "Expel Harm (<=80% self heal)"    },
    { ShouldProvoke,           DoProvoke,           "Provoke (taunt)"                 },
    { ShouldSpearHandStrike,   DoSpearHandStrike,   "Spear Hand Strike (interrupt)"   },
    { ShouldParalysisOffTarget,DoParalysisOffTarget,"Paralysis (off-target caster)"   },
    { ShouldRingOfPeace,       DoRingOfPeace,       "Ring of Peace (panic peel)"      },
    { ShouldLegSweep,          DoLegSweep,          "Leg Sweep (3+ AoE stun)"         },
    { ShouldDetox,             DoDetox,             "Detox (cleanse)"                 },
    { ShouldInvokeNiuzao,      DoInvokeNiuzao,      "Invoke Niuzao (boss/4+)"         },
    { ShouldWeaponsOfOrder,    DoWeaponsOfOrder,    "Weapons of Order (CD)"           },
    { ShouldInvokeXuen,        DoInvokeXuen,        "Invoke Xuen (boss CD)"           },
    { ShouldExplodingKeg,      DoExplodingKeg,      "Exploding Keg (3+/boss AoE)"     },
    { ShouldBonedustBrew,      DoBonedustBrew,      "Bonedust Brew (cleave)"          },
    { ShouldTouchOfDeath,      DoTouchOfDeath,      "Touch of Death (<=15% or HP-cap)"},
    { ShouldKegSmash,          DoKegSmash,          "Keg Smash (priority + threat)"   },
    { ShouldBreathOfFire,      DoBreathOfFire,      "Breath of Fire (DoT refresh)"    },
    { ShouldRushingJadeWind,   DoRushingJadeWind,   "Rushing Jade Wind (2+ AoE)"      },
    { ShouldSpinningCraneKick, DoSpinningCraneKick, "Spinning Crane Kick (3+ AoE)"    },
    { ShouldBlackoutKick,      DoBlackoutKick,      "Blackout Kick (Chi spender)"     },
    { ShouldRisingSunKick,     DoRisingSunKick,     "Rising Sun Kick"                 },
    { ShouldTigerPalm,         DoTigerPalm,         "Tiger Palm (filler / brew CDR)"  },
    { AlwaysInCombat,          DoAutoAttack,        "Engage auto attack"              },
};

} // anonymous

void RegisterApl_Monk_Brewmaster()
{
    constexpr uint32 SPEC_MONK_BREWMASTER = 268;
    RegisterRotation(CLASS_MONK, SPEC_MONK_BREWMASTER, ApRotation{kRules});
}

} // namespace Playerbot::Combat
