// Brewmaster Monk - WoW 12.1.0.69587 (Midnight) enterprise rotation. Tank
// archetype built around Stagger management: damage taken is delayed via the
// Stagger debuff (Light/Moderate/Heavy bands), and Purifying Brew clears 50%
// of the remaining pool when cast. The active mitigation layer is Celestial
// Brew / Celestial Infusion (absorb shield, choice node); the passive rotation
// drives brew charge regeneration via Keg Smash + Tiger Palm.
//
// Survival ladder: Fortifying Brew (+HP / DR) -> Celestial Brew or Celestial
// Infusion -> Purifying Brew (stagger) -> Black Ox Brew (charge reset) ->
// Vivify panic -> Expel Harm self-heal. Dampen Harm, Diffuse Magic and Zen
// Meditation are gone from the 12.1 Monk kit (Diffuse Magic is now a passive
// rider on Fortifying Brew), so the ladder is shorter than in 11.x.
//
// Threat: Provoke (ranged taunt) + Keg Smash (huge initial threat + reduces
// brew CDs) + Spinning Crane Kick / Breath of Fire / Chi Burst AoE.
//
// Group utility: Ring of Peace (displacement), Leg Sweep (AoE stun),
// Paralysis (off-target CC), Spear Hand Strike (melee interrupt),
// Tiger's Lust (self root/snare break), Detox (Poison/Disease cleanse).
//
// Major CDs: Invoke Niuzao (tank pet, pulls stagger), Exploding Keg, Black
// Ox Brew. Weapons of Order, Bonedust Brew, Rising Sun Kick and Invoke Xuen
// are no longer Brewmaster abilities in 12.1.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated against SpellName.csv) ----
// Validated spell IDs (WoW 12.1.0.69587):
//   121253 Keg Smash           100784 Blackout Kick              100780 Tiger Palm
//   115181 Breath of Fire      101546 Spinning Crane Kick        322729 Spinning Crane Kick (BrM)
//   116847 Rushing Jade Wind   325153 Exploding Keg              115399 Black Ox Brew
//   123986 Chi Burst           322109 Touch of Death             322101 Expel Harm
//   116670 Vivify              119582 Purifying Brew             322507 Celestial Brew
//   1241059 Celestial Infusion 115203 Fortifying Brew (cast)     388917 Fortifying Brew (talent)
//   132578 Invoke Niuzao       115546 Provoke                    116705 Spear Hand Strike
//   119381 Leg Sweep           116844 Ring of Peace              115078 Paralysis
//   116841 Tiger's Lust        218164 Detox                      115178 Resuscitate
//   Aura-only: 124273 Heavy Stagger | 124274 Moderate Stagger | 123725 Breath of Fire DoT
//              116847 Rushing Jade Wind buff
//   Passive gates: 418359 Press the Advantage (removes Tiger Palm)
//
// Skipped (with reason):
//   388917 Fortifying Brew (talent) passive trait spell. TC learns the trait
//                              SpellID but never its VisibleSpellID 115203, so
//                              the rule gates on EITHER id and casts 115203.
//   1243287 Diffuse Magic      12.1 passive rider on Fortifying Brew; no cast.
//   122278 Dampen Harm / 115176 Zen Meditation / 387184 Weapons of Order /
//   386276 Bonedust Brew       removed from the 12.1 Monk kit.
//   107428 Rising Sun Kick / 123904 Invoke Xuen
//                              Windwalker/Mistweaver-only in 12.1.
//   205523 Blackout Kick (BrM) Shuffle variant with no learn link in 12.1 data;
//                              the generic 100784 is redirected server-side.
//   450391 Chi Wave            passive in 12.1 (auto-fires off Keg Smash).
//   115069 Stagger             passive damage-delay mechanic; only OBSERVED
//                              via the band auras (124273/124274) to gate
//                              Purifying Brew.
//   325092 Purified Chi / 215479 Shuffle buff / 124275 Light Stagger
//                              no rule reads them.
//   1229376 Single-Button Assistant client convenience macro.
//   115315 Summon Black Ox Statue / 115008 Chi Torpedo / 116095 Disable
//                              not in the curated builds; positioning/PvP tools.
constexpr uint32 KEG_SMASH            = 121253;
constexpr uint32 BLACKOUT_KICK_BRM    = 100784;       // generic id; Shuffle's BrM variant is a server-side override
constexpr uint32 TIGER_PALM           = 100780;
constexpr uint32 PRESS_THE_ADVANTAGE  = 418359;       // passive talent that REMOVES Tiger Palm
constexpr uint32 BREATH_OF_FIRE       = 115181;
constexpr uint32 SPINNING_CRANE_KICK  = 101546;       // class baseline id
constexpr uint32 SPINNING_CRANE_KICK_BRM = 322729;    // spec spell override (25 Energy, grants Shuffle)
constexpr uint32 RUSHING_JADE_WIND    = 116847;       // talent
constexpr uint32 EXPLODING_KEG        = 325153;       // talent - AoE + melee damage reduction
constexpr uint32 BLACK_OX_BREW        = 115399;       // talent - resets brew charges + energy
constexpr uint32 TOUCH_OF_DEATH       = 322109;       // execute (<=15% target HP or target.max_hp <= bot.max_hp)
constexpr uint32 EXPEL_HARM           = 322101;       // self heal that consumes Healing Spheres
constexpr uint32 VIVIFY               = 116670;       // emergency self heal
constexpr uint32 CHI_BURST            = 123986;       // talent - line AoE heal/dmg (does not block avoidance)

// Brews / mitigation
constexpr uint32 PURIFYING_BREW       = 119582;
constexpr uint32 CELESTIAL_BREW       = 322507;       // choice node with Celestial Infusion
constexpr uint32 CELESTIAL_INFUSION   = 1241059;      // [R][M] pick - %-based absorb variant
constexpr uint32 FORTIFYING_BREW      = 115203;       // cast id (VisibleSpellID of the talent)
constexpr uint32 FORTIFYING_BREW_TALENT = 388917;     // learned trait spell - knows_spell gate

// Major cooldowns
constexpr uint32 INVOKE_NIUZAO        = 132578;

// Threat / interrupt / CC
constexpr uint32 PROVOKE              = 115546;
constexpr uint32 SPEAR_HAND_STRIKE    = 116705;
constexpr uint32 LEG_SWEEP            = 119381;
constexpr uint32 RING_OF_PEACE        = 116844;
constexpr uint32 PARALYSIS            = 115078;
constexpr uint32 TIGERS_LUST          = 116841;

// Group utility
constexpr uint32 DETOX                = 218164;       // BrM cleanse: Poison + Disease only in 12.1
constexpr uint32 RESUSCITATE          = 115178;       // OOC rez

// Stagger debuffs - bot self-aura tracks current stagger band.
constexpr uint32 MODERATE_STAGGER     = 124274;
constexpr uint32 HEAVY_STAGGER        = 124273;

// Buff trackers
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

// Celestial Brew and Celestial Infusion share a choice node - resolve to
// whichever brew this bot actually learned so both builds work.
uint32 CelestialBrewSpell(ApPredicateContext const& ctx)
{
    return ctx.bot.knows_spell(CELESTIAL_INFUSION) ? CELESTIAL_INFUSION : CELESTIAL_BREW;
}

// Spinning Crane Kick: the Brewmaster spec spell 322729 overrides the class
// baseline 101546 once the spec is learned; fall back for pre-spec bots.
uint32 SpinningCraneKickSpell(ApPredicateContext const& ctx)
{
    return ctx.bot.knows_spell(SPINNING_CRANE_KICK_BRM) ? SPINNING_CRANE_KICK_BRM : SPINNING_CRANE_KICK;
}

// Fortifying Brew: TC learns the trait spell 388917, never its VisibleSpellID
// 115203 (the actual cast). Accept either id as proof the talent is known.
bool KnowsFortifyingBrew(ApPredicateContext const& ctx)
{
    return ctx.bot.knows_spell(FORTIFYING_BREW_TALENT) || ctx.bot.knows_spell(FORTIFYING_BREW);
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
    // Panic displace when overwhelmed at low HP -peels healer effectively.
    return ctx.bot.attackers_count() >= 4 && ctx.bot.hp_pct() <= 40;
}
void DoRingOfPeace(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    float bx, by, bz;
    ctx.bot.position(bx, by, bz);
    e.cast_at(RING_OF_PEACE, bx, by, bz);
}

// ---- Survival ladder ----
bool ShouldFortifyingBrew(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!KnowsFortifyingBrew(ctx)) return false;
    if (!ctx.bot.is_ready(FORTIFYING_BREW)) return false;
    // 12.1: the only big self CD left (Dampen Harm / Zen Meditation gone);
    // also carries Diffuse Magic's reflect when that passive is taken.
    if (ctx.bot.hp_pct() <= 35) return true;
    return BossLikeTargetEngaged(ctx) && ctx.bot.hp_pct() <= 50;
}
void DoFortifyingBrew(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FORTIFYING_BREW); }

bool ShouldTigersLust(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(TIGERS_LUST)) return false;
    if (!ctx.bot.is_ready(TIGERS_LUST)) return false;
    // Self root/snare break so the tank can keep the pack on it.
    return ctx.bot.has_mechanic(MECHANIC_ROOT) || ctx.bot.has_mechanic(MECHANIC_SNARE);
}
void DoTigersLust(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(TIGERS_LUST, ctx.bot.raw().guid);
}

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

// ---- Active mitigation: Celestial Brew / Infusion + Purifying Brew ----
bool ShouldCelestialBrew(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    const uint32 brew = CelestialBrewSpell(ctx);
    if (!ctx.bot.knows_spell(brew)) return false;
    if (!ctx.bot.is_ready(brew)) return false;
    if (ctx.bot.has_aura(brew)) return false;
    // Pop reactively at <=65%, but only if we don't already have an absorb.
    return ctx.bot.hp_pct() <= 65;
}
void DoCelestialBrew(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(CelestialBrewSpell(ctx)); }

bool ShouldPurifyingBrew(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(PURIFYING_BREW)) return false;
    if (!ctx.bot.is_ready(PURIFYING_BREW)) return false;
    // Only burn a charge when stagger is meaningful -clears 50% of pool.
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
    if (!ctx.bot.is_ready(CelestialBrewSpell(ctx)) && ctx.bot.hp_pct() <= 50) return true;
    return false;
}
void DoBlackOxBrew(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLACK_OX_BREW); }

// ---- Major offensive cooldowns ----
bool ShouldInvokeNiuzao(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(INVOKE_NIUZAO)) return false;
    if (!ctx.bot.is_ready(INVOKE_NIUZAO)) return false;
    // Niuzao stomps + drains stagger - pop on bosses or when overwhelmed.
    return BossLikeTargetEngaged(ctx) || ctx.bot.attackers_count() >= 4;
}
void DoInvokeNiuzao(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(INVOKE_NIUZAO); }

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
    // 12.1 Brewmaster Detox (218164) clears Poison + Disease only - no Magic.
    auto const* m = ctx.group.dispel_candidate(Playerbot::DispelType::Disease);
    if (!m) m = ctx.group.dispel_candidate(Playerbot::DispelType::Poison);
    return m != nullptr;
}
void DoDetox(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    auto const* m = ctx.group.dispel_candidate(Playerbot::DispelType::Disease);
    if (!m) m = ctx.group.dispel_candidate(Playerbot::DispelType::Poison);
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
    // Only refresh if DoT missing or expiring -keeps the BoF cone honest.
    AuraEntry const* a = ctx.bot.find_aura(BREATH_OF_FIRE_DOT, ctx.bot.victim());
    return !a || a->remaining.count() <= 3000;
}
void DoBreathOfFire(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BREATH_OF_FIRE, ctx.bot.victim());
}

bool ShouldChiBurst(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CHI_BURST)) return false;
    if (!ctx.bot.is_ready(CHI_BURST)) return false;
    // Line AoE that also heals the Monk; Brewmaster keeps avoidance while
    // casting it, so fire on packs or as a cheap self-heal top-up.
    return ctx.bot.attackers_count() >= 2 || ctx.bot.hp_pct() <= 70;
}
void DoChiBurst(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CHI_BURST, ctx.bot.victim());
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
    if (!ctx.bot.knows_spell(SpinningCraneKickSpell(ctx))) return false;
    if (ctx.bot.power(POWER_ENERGY_IDX) < 25) return false;
    return ctx.aoe_preference || ctx.bot.attackers_count() >= 3;
}
void DoSpinningCraneKick(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SpinningCraneKickSpell(ctx)); }

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
    // Press the Advantage (passive talent) removes Tiger Palm from the kit.
    if (ctx.bot.knows_spell(PRESS_THE_ADVANTAGE)) return false;
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
// Canonical Brewmaster priority order (12.1):
//   Survival ladder (Fortifying Brew -> Celestial Brew/Infusion -> Purifying
//     Brew on heavy stagger -> Black Ox Brew -> Vivify panic -> Expel Harm)
//     FIRST,
//   then Threat (Provoke), Interrupts (Spear Hand / Paralysis / Leg Sweep /
//     Ring of Peace), Tiger's Lust root break, Dispel (Detox),
//   then offensive CDs (Niuzao / Exploding Keg / Touch of Death),
//   then the steady rotation (Keg Smash -> Breath of Fire -> Chi Burst ->
//     AoE -> Blackout Kick -> Tiger Palm filler),
//   finally auto-attack.
ApRule const kRules[] = {
    { ShouldFortifyingBrew,    DoFortifyingBrew,    "Fortifying Brew (<=35%/boss)"    },
    { ShouldCelestialBrew,     DoCelestialBrew,     "Celestial Brew (<=65% absorb)"   },
    { ShouldPurifyingBrew,     DoPurifyingBrew,     "Purifying Brew (clear stagger)"  },
    { ShouldBlackOxBrew,       DoBlackOxBrew,       "Black Ox Brew (reset charges)"   },
    { ShouldVivifyPanic,       DoVivifyPanic,       "Vivify (panic heal)"             },
    { ShouldExpelHarm,         DoExpelHarm,         "Expel Harm (<=80% self heal)"    },
    { ShouldProvoke,           DoProvoke,           "Provoke (taunt)"                 },
    { ShouldSpearHandStrike,   DoSpearHandStrike,   "Spear Hand Strike (interrupt)"   },
    { ShouldParalysisOffTarget,DoParalysisOffTarget,"Paralysis (off-target caster)"   },
    { ShouldRingOfPeace,       DoRingOfPeace,       "Ring of Peace (panic peel)"      },
    { ShouldLegSweep,          DoLegSweep,          "Leg Sweep (3+ AoE stun)"         },
    { ShouldTigersLust,        DoTigersLust,        "Tiger's Lust (root break)"       },
    { ShouldDetox,             DoDetox,             "Detox (cleanse)"                 },
    { ShouldInvokeNiuzao,      DoInvokeNiuzao,      "Invoke Niuzao (boss/4+)"         },
    { ShouldExplodingKeg,      DoExplodingKeg,      "Exploding Keg (3+/boss AoE)"     },
    { ShouldTouchOfDeath,      DoTouchOfDeath,      "Touch of Death (<=15% or HP-cap)"},
    { ShouldKegSmash,          DoKegSmash,          "Keg Smash (priority + threat)"   },
    { ShouldBreathOfFire,      DoBreathOfFire,      "Breath of Fire (DoT refresh)"    },
    { ShouldChiBurst,          DoChiBurst,          "Chi Burst (2+ / <=70% heal)"     },
    { ShouldRushingJadeWind,   DoRushingJadeWind,   "Rushing Jade Wind (2+ AoE)"      },
    { ShouldSpinningCraneKick, DoSpinningCraneKick, "Spinning Crane Kick (3+ AoE)"    },
    { ShouldBlackoutKick,      DoBlackoutKick,      "Blackout Kick (Chi spender)"     },
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
