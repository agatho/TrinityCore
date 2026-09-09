// Assassination Rogue - WoW 12.1.0.69587 (Midnight) enterprise rotation.
// Bleed-driven melee with Garrote/Rupture maintenance, Envenom spend at
// 5+ CP, Slice and Dice attack-speed buff, Crimson Tempest AoE generator
// (copies Garrote/Rupture to nearby enemies), Fan of Knives AoE generator
// / poison spread, Deathmark + Kingsbane damage windows, Shiv poison amp.
// Weapon poisons are kept up out of combat: Deadly Poison (+ Amplifying
// Poison with Dragon-Tempered Blades) lethal, Atrophic / Crippling
// non-lethal. Thistle Tea restores Energy when starved.
//
// Survival: Crimson Vial, Evasion, Cloak of Shadows (magic immune), Feint
// (DR), Vanish (combat drop). Group utility: Tricks of the Trade (tank
// threat). CC: Kick interrupt, Kidney Shot (CP stun), Blind (panic CC).
// Gap-close via Shadowstep (36554, learned through spec spell 394932).
//
// Validated spell IDs (SpellName.csv lookup, WoW 12.1.0.69587):
//   1329    Mutilate           - CP gen, melee (spec override of 1752)
//   32645   Envenom            - finisher (poison amp; overrides Eviscerate)
//   703     Garrote            - bleed (stealth opener + maintained)
//   1943    Rupture            - bleed finisher
//   360194  Deathmark          - talent [R] burst CD
//   385627  Kingsbane          - talent [R] Nature DoT CD
//   1247227 Crimson Tempest    - talent [M] AoE generator (was 121411)
//   315496  Slice and Dice     - attack-speed self-buff finisher
//   185565  Poisoned Knife     - ranged poison applicator
//   51723   Fan of Knives      - AoE generator + poison spread
//   5938    Shiv               - talent [R] poison amp
//   2823    Deadly Poison      - talent [R] lethal poison
//   381664  Amplifying Poison  - talent [R] second lethal poison
//   315584  Instant Poison     - baseline lethal poison (fallback)
//   381637  Atrophic Poison    - talent [R] non-lethal poison
//   3408    Crippling Poison   - baseline non-lethal poison (fallback)
//   381623  Thistle Tea        - taught by class passive 469779 [R]
//   1298826 Thistle Tea        - active class talent variant (same effect)
//   36554   Shadowstep         - cast id; 394932 is the Sin spec learn-spell
//   1766    Kick / 408 Kidney Shot / 2094 Blind / 57934 Tricks
//   114018  Shroud of Concealment (declared, no combat rule)
//   185311  Crimson Vial / 5277 Evasion / 31224 Cloak / 1966 Feint / 1856 Vanish
//   1784    Stealth / 115191 Stealth (Subterfuge) / 115192 Subterfuge window
//   Passive gate: 381801 Dragon-Tempered Blades (allows two lethal poisons)
//
// Skipped (with reason):
//   79140   Vendetta           - not learnable in 12.1 (replaced by Deathmark).
//   385408  Sepsis             - not learnable in 12.1.
//   137619  Marked for Death   - not learnable in 12.1.
//   385616  Echoing Reprimand  - 12.1 version (470669) is a passive.
//   381802  Indiscriminate Carnage - passive/removed in 12.1.
//   76577   Smoke Bomb         - not learnable in 12.1.
//   1776    Gouge              - class talent not in the curated Sin builds; needs facing.
//   1229376 Single-Button Assistant - client convenience macro, not a rotation ability.
//   1293340 Mark for Death (Deathstalker) - hero-tree passive-driven, no cast rule.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated) ----
constexpr uint32 MUTILATE             = 1329;
constexpr uint32 ENVENOM              = 32645;
constexpr uint32 GARROTE              = 703;
constexpr uint32 RUPTURE              = 1943;
constexpr uint32 DEATHMARK            = 360194;       // talent [R] burst CD
constexpr uint32 CRIMSON_TEMPEST      = 1247227;      // talent [M] AoE generator (was 121411)
constexpr uint32 SLICE_AND_DICE       = 315496;
constexpr uint32 POISONED_KNIFE       = 185565;       // ranged
constexpr uint32 FAN_OF_KNIVES        = 51723;        // AoE generator + poison spread
constexpr uint32 SHIV                 = 5938;         // talent [R] poison amplifier
constexpr uint32 KINGSBANE            = 385627;       // talent [R] Nature DoT CD
// Weapon poisons: the self-buff aura carries the cast spell's id.
constexpr uint32 DEADLY_POISON        = 2823;         // talent [R] lethal
constexpr uint32 AMPLIFYING_POISON    = 381664;       // talent [R] lethal (second, via 381801)
constexpr uint32 INSTANT_POISON       = 315584;       // baseline lethal fallback
constexpr uint32 ATROPHIC_POISON      = 381637;       // talent [R] non-lethal
constexpr uint32 CRIPPLING_POISON     = 3408;         // baseline non-lethal fallback
constexpr uint32 DRAGON_TEMPERED_BLADES = 381801;     // passive [R]: two lethal + two non-lethal
// Thistle Tea: the [R] class passive 469779 teaches 381623; the active
// talent node variant is 1298826. Same effect (restore Energy) - cast
// whichever is in the spellbook.
constexpr uint32 THISTLE_TEA          = 381623;
constexpr uint32 THISTLE_TEA_ALT      = 1298826;
constexpr uint32 KICK                 = 1766;
constexpr uint32 KIDNEY_SHOT          = 408;
constexpr uint32 BLIND                = 2094;
constexpr uint32 TRICKS_OF_TRADE      = 57934;
// Shadowstep: 36554 is the castable. 394932 is the Assassination spec
// spell that teaches it (SpellLearnSpell 394932 -> 36554); gate on either
// being known, always cast 36554.
constexpr uint32 SHADOWSTEP           = 36554;
constexpr uint32 SHADOWSTEP_SIN       = 394932;
constexpr uint32 CRIMSON_VIAL         = 185311;
constexpr uint32 EVASION              = 5277;
constexpr uint32 CLOAK_OF_SHADOWS     = 31224;
constexpr uint32 FEINT                = 1966;
constexpr uint32 VANISH               = 1856;
constexpr uint32 SHROUD_OF_CONCEAL    = 114018;
constexpr uint32 STEALTH              = 1784;
constexpr uint32 STEALTH_AURA         = 115191;       // Subterfuge improved
constexpr uint32 SUBTERFUGE_AURA      = 115192;       // post-stealth-break talent window

constexpr uint8 POWER_ENERGY_IDX       = 3;
constexpr uint8 POWER_COMBO_POINTS_IDX = 4;

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

uint8 ComboPoints(ApPredicateContext const& ctx)
{
    return static_cast<uint8>(ctx.bot.power(POWER_COMBO_POINTS_IDX));
}

int32 Energy(ApPredicateContext const& ctx)
{
    return ctx.bot.power(POWER_ENERGY_IDX);
}

bool InStealth(ApPredicateContext const& ctx)
{
    return ctx.bot.has_aura(STEALTH)
        || ctx.bot.has_aura(STEALTH_AURA)
        || ctx.bot.has_aura(SUBTERFUGE_AURA);
}

// ---- Weapon poisons (OOC upkeep, 1.5s cast, 1h buff) ----
// Lethal: Deadly Poison when talented, else baseline Instant Poison. With
// Dragon-Tempered Blades a second lethal (Amplifying) is layered on top -
// without that passive the second application would replace the first
// and the two rules would flip-flop, so Amplifying is gated on 381801
// AND Deadly already being up.
uint32 PickLethalPoison(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(DEADLY_POISON))
    {
        if (!ctx.bot.has_aura(DEADLY_POISON)) return DEADLY_POISON;
        if (ctx.bot.knows_spell(AMPLIFYING_POISON) && ctx.bot.knows_spell(DRAGON_TEMPERED_BLADES)
            && !ctx.bot.has_aura(AMPLIFYING_POISON))
            return AMPLIFYING_POISON;
        return 0;
    }
    if (ctx.bot.knows_spell(INSTANT_POISON) && !ctx.bot.has_aura(INSTANT_POISON))
        return INSTANT_POISON;
    return 0;
}
uint32 PickNonLethalPoison(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(ATROPHIC_POISON))
        return ctx.bot.has_aura(ATROPHIC_POISON) ? 0 : ATROPHIC_POISON;
    if (ctx.bot.knows_spell(CRIPPLING_POISON) && !ctx.bot.has_aura(CRIPPLING_POISON))
        return CRIPPLING_POISON;
    return 0;
}
bool PoisonCastWindow(ApPredicateContext const& ctx)
{
    // Poisons are a 1.5s cast: only out of combat, standing still, and
    // not mounted (the cast would dismount mid-travel).
    if (ctx.bot.in_combat()) return false;
    if (ctx.bot.is_moving()) return false;
    if (ctx.bot.is_mounted()) return false;
    return true;
}
bool ShouldLethalPoison(ApPredicateContext const& ctx)
{
    if (!PoisonCastWindow(ctx)) return false;
    const uint32 sid = PickLethalPoison(ctx);
    return sid != 0 && ctx.bot.is_ready(sid);
}
void DoLethalPoison(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const uint32 sid = PickLethalPoison(ctx);
    if (sid != 0) e.cast(sid);
}
bool ShouldNonLethalPoison(ApPredicateContext const& ctx)
{
    if (!PoisonCastWindow(ctx)) return false;
    const uint32 sid = PickNonLethalPoison(ctx);
    return sid != 0 && ctx.bot.is_ready(sid);
}
void DoNonLethalPoison(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const uint32 sid = PickNonLethalPoison(ctx);
    if (sid != 0) e.cast(sid);
}

// ---- Stealth (OOC opener prep) ----
bool ShouldStealthOOC(ApPredicateContext const& ctx)
{
    if (ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(STEALTH)) return false;
    return !InStealth(ctx);
}
void DoStealth(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(STEALTH); }

// ---- Thistle Tea (Energy restore, charge-based, off-GCD) ----
uint32 BestThistleTeaSpell(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(THISTLE_TEA) && ctx.bot.is_ready(THISTLE_TEA))
        return THISTLE_TEA;
    if (ctx.bot.knows_spell(THISTLE_TEA_ALT) && ctx.bot.is_ready(THISTLE_TEA_ALT))
        return THISTLE_TEA_ALT;
    return 0;
}
bool ShouldThistleTea(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.in_combat()) return false;
    if (BestThistleTeaSpell(ctx) == 0) return false;
    // Energy-starved during a burst window (Deathmark/Kingsbane up) or
    // plainly empty - the restore is wasted above ~40 Energy.
    if (Energy(ctx) > 40) return false;
    return ctx.bot.has_aura(DEATHMARK, ctx.bot.victim()) || ctx.bot.has_aura(KINGSBANE, ctx.bot.victim())
        || Energy(ctx) <= 20;
}
void DoThistleTea(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const uint32 sid = BestThistleTeaSpell(ctx);
    if (sid != 0) e.cast(sid);
}

// ---- Survival ----
bool ShouldCrimsonVial(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(CRIMSON_VIAL)) return false;
    if (!ctx.bot.is_ready(CRIMSON_VIAL)) return false;
    return ctx.bot.hp_pct() <= 60;
}
void DoCrimsonVial(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CRIMSON_VIAL); }

bool ShouldEvasion(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(EVASION)) return false;
    if (!ctx.bot.is_ready(EVASION)) return false;
    return ctx.bot.hp_pct() <= 40;
}
void DoEvasion(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(EVASION); }

bool ShouldCloakOfShadows(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(CLOAK_OF_SHADOWS)) return false;
    if (!ctx.bot.is_ready(CLOAK_OF_SHADOWS)) return false;
    for (auto const& a : ctx.bot.raw().auras.own_auras)
        if (a.is_harmful && a.dispel_type == DispelType::Magic)
            return true;
    return false;
}
void DoCloakOfShadows(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CLOAK_OF_SHADOWS); }

bool ShouldFeint(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(FEINT)) return false;
    if (!ctx.bot.is_ready(FEINT)) return false;
    return ctx.bot.hp_pct() <= 60;
}
void DoFeint(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FEINT); }

bool ShouldVanish(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(VANISH)) return false;
    if (!ctx.bot.is_ready(VANISH)) return false;
    return ctx.bot.hp_pct() <= 25 && ctx.bot.attackers_count() >= 1;
}
void DoVanish(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(VANISH); }

// ---- Group utility ----
bool ShouldTricks(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(TRICKS_OF_TRADE)) return false;
    if (!ctx.bot.is_ready(TRICKS_OF_TRADE)) return false;
    auto const* tank = ctx.group.tank();
    return tank && tank->online && tank->guid != ctx.bot.raw().guid;
}
void DoTricks(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* tank = ctx.group.tank())
        e.cast(TRICKS_OF_TRADE, tank->guid);
}

// ---- Interrupt / CC ----
bool ShouldKick(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(KICK)) return false;
    if (!ctx.bot.is_ready(KICK)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    return ctx.bot.kick_target(pvp, 5.0f) != nullptr;
}
void DoKick(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 5.0f))
        e.cast(KICK, c->guid);
}

bool ShouldKidneyShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(KIDNEY_SHOT)) return false;
    if (!ctx.bot.is_ready(KIDNEY_SHOT)) return false;
    if (ComboPoints(ctx) < 5) return false;
    if (ctx.bot.is_ready(KICK)) return false;
    return ctx.bot.interruptible_caster() != nullptr;
}
void DoKidneyShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(KIDNEY_SHOT, ctx.bot.victim());
}

bool ShouldBlind(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BLIND)) return false;
    if (!ctx.bot.is_ready(BLIND)) return false;
    if (ctx.bot.attackers_count() < 2) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoBlind(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BLIND, ctx.bot.victim());
}

// ---- Maintenance ----
bool ShouldSliceAndDice(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SLICE_AND_DICE)) return false;
    if (ComboPoints(ctx) < 4) return false;
    AuraEntry const* a = ctx.bot.find_aura(SLICE_AND_DICE);
    return !a || a->remaining.count() <= 5000;
}
void DoSliceAndDice(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SLICE_AND_DICE); }

// ---- Major offensive cooldowns ----
bool ShouldDeathmark(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEATHMARK)) return false;
    if (!ctx.bot.is_ready(DEATHMARK)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoDeathmark(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DEATHMARK, ctx.bot.victim());
}

bool ShouldKingsbane(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(KINGSBANE)) return false;
    if (!ctx.bot.is_ready(KINGSBANE)) return false;
    // Kingsbane wants Rupture already ticking so its amp uptime is paid out;
    // gate on Rupture-up OR boss-tier so we never delay it past the burst.
    AuraEntry const* r = ctx.bot.find_aura(RUPTURE, ctx.bot.victim());
    return (r && r->remaining.count() >= 4000) || BossLikeTargetEngaged(ctx);
}
void DoKingsbane(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(KINGSBANE, ctx.bot.victim());
}

bool ShouldShiv(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHIV)) return false;
    if (!ctx.bot.is_ready(SHIV)) return false;
    // Shiv amps poison damage on the current victim — pop on cooldown
    // during Envenom windows. Gate on Rupture being present so the amp
    // window overlaps the highest-value DoT.
    AuraEntry const* r = ctx.bot.find_aura(RUPTURE, ctx.bot.victim());
    return r != nullptr;
}
void DoShiv(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHIV, ctx.bot.victim());
}

// Shadowstep: 36554 is the castable in 12.1; the Assassination spec spell
// 394932 only teaches it. Gate on either id being in the spellbook, cast
// 36554. Teleports behind the victim - used as a gap-closer when OoR.
bool KnowsShadowstep(ApPredicateContext const& ctx)
{
    return ctx.bot.knows_spell(SHADOWSTEP) || ctx.bot.knows_spell(SHADOWSTEP_SIN);
}
bool ShouldShadowstep(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!KnowsShadowstep(ctx)) return false;
    if (!ctx.bot.is_ready(SHADOWSTEP)) return false;
    return ctx.bot.enemies_within(8.0f) == 0;
}
void DoShadowstep(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHADOWSTEP, ctx.bot.victim());
}

// ---- Bleeds + spender ----
// Stealth-opener Garrote: from stealth Garrote silences for 3s + applies
// a full-duration bleed. We want this BEFORE Mutilate spam consumes the
// stealth break (Mutilate from stealth has no stealth-only effect for Sin).
bool ShouldGarroteOpener(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(GARROTE)) return false;
    if (!InStealth(ctx)) return false;
    return true;
}
void DoGarroteOpener(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(GARROTE, ctx.bot.victim());
}

bool ShouldGarrote(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(GARROTE)) return false;
    AuraEntry const* a = ctx.bot.find_aura(GARROTE, ctx.bot.victim());
    return !a || a->remaining.count() <= 4500;
}
void DoGarrote(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(GARROTE, ctx.bot.victim());
}

bool ShouldRupture(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RUPTURE)) return false;
    if (ComboPoints(ctx) < 4) return false;
    AuraEntry const* a = ctx.bot.find_aura(RUPTURE, ctx.bot.victim());
    return !a || a->remaining.count() <= 4500;
}
void DoRupture(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RUPTURE, ctx.bot.victim());
}

bool ShouldEnvenom(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ENVENOM)) return false;
    if (!ctx.bot.is_ready(ENVENOM)) return false;
    return ComboPoints(ctx) >= 5;
}
void DoEnvenom(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ENVENOM, ctx.bot.victim());
}

// Crimson Tempest (12.1): no longer a finisher - a 60 Energy AoE
// GENERATOR that copies the longest Garrote and Rupture on the enemies it
// hits onto nearby enemies. Fire in AoE with CP headroom once the victim
// carries Rupture (so there is a bleed worth copying); preferred over Fan
// of Knives while a bleed is up.
bool ShouldCrimsonTempest(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CRIMSON_TEMPEST)) return false;
    if (!ctx.bot.is_ready(CRIMSON_TEMPEST)) return false;
    if (ComboPoints(ctx) >= 4) return false;
    if (!(ctx.aoe_preference || ctx.bot.enemies_within(8.0f) >= 2)) return false;
    return ctx.bot.has_aura(RUPTURE, ctx.bot.victim()) || ctx.bot.has_aura(GARROTE, ctx.bot.victim());
}
void DoCrimsonTempest(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(CRIMSON_TEMPEST);
}

// Fan of Knives: AoE CP generator that also spreads Deadly/Wound Poison
// from the current victim to nearby enemies. Use whenever we see 2+ enemies
// and have CP headroom (don't spam past 5 — it's wasted resource gen).
bool ShouldFanOfKnives(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FAN_OF_KNIVES)) return false;
    if (!ctx.bot.is_ready(FAN_OF_KNIVES)) return false;
    if (ComboPoints(ctx) >= 5) return false;
    return ctx.bot.enemies_within(10.0f) >= 2;
}
void DoFanOfKnives(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(FAN_OF_KNIVES);
}

bool ShouldMutilate(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ComboPoints(ctx) >= 5) return false;
    return ctx.bot.knows_spell(MUTILATE);
}
void DoMutilate(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MUTILATE, ctx.bot.victim());
}

// Poisoned Knife: ranged poison applicator. Fires when target is out of
// melee range (no other enemies on us so Shadowstep would be smarter, but
// PK is a no-cooldown filler so it stays useful when stepping is on CD).
bool ShouldPoisonedKnife(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(POISONED_KNIFE)) return false;
    if (ComboPoints(ctx) >= 5) return false;
    return ctx.bot.enemies_within(8.0f) == 0;
}
void DoPoisonedKnife(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(POISONED_KNIFE, ctx.bot.victim());
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

// Rule order:
//   Cloak (magic emerg) -> Vanish (panic) -> Evasion (defensive)
//   -> Crimson Vial (heal) -> Feint (DR) -> Kick -> Kidney Shot (kick fb)
//   -> Blind -> poisons (OOC upkeep) -> Stealth (OOC) -> Stealth-opener Garrote
//   -> Tricks -> Shadowstep (gap close) -> Thistle Tea -> major CDs (Deathmark,
//   Kingsbane, Shiv) -> maintenance (SnD, Garrote, Rupture) -> Envenom
//   -> AoE generators (Crimson Tempest, Fan of Knives) -> Mutilate -> Poisoned Knife -> AA.
ApRule const kRules[] = {
    { ShouldCloakOfShadows,        DoCloakOfShadows,        "Cloak of Shadows (magic emergency)" },
    { ShouldVanish,                DoVanish,                "Vanish (panic <=25%)"               },
    { ShouldEvasion,               DoEvasion,               "Evasion (<=40%)"                    },
    { ShouldCrimsonVial,           DoCrimsonVial,           "Crimson Vial (<=60%)"               },
    { ShouldFeint,                 DoFeint,                 "Feint (<=60% DR)"                   },
    { ShouldKick,                  DoKick,                  "Kick (interrupt)"                   },
    { ShouldKidneyShot,            DoKidneyShot,            "Kidney Shot (interrupt fb)"         },
    { ShouldBlind,                 DoBlind,                 "Blind (panic CC)"                   },
    { ShouldLethalPoison,          DoLethalPoison,          "Lethal poison (OOC upkeep)"         },
    { ShouldNonLethalPoison,       DoNonLethalPoison,       "Non-lethal poison (OOC upkeep)"     },
    { ShouldStealthOOC,            DoStealth,               "Stealth (OOC opener prep)"          },
    { ShouldGarroteOpener,         DoGarroteOpener,         "Garrote (stealth opener)"           },
    { ShouldTricks,                DoTricks,                "Tricks of the Trade"                },
    { ShouldShadowstep,            DoShadowstep,            "Shadowstep (gap close)"             },
    { ShouldThistleTea,            DoThistleTea,            "Thistle Tea (energy restore)"       },
    { ShouldDeathmark,             DoDeathmark,             "Deathmark"                          },
    { ShouldKingsbane,             DoKingsbane,             "Kingsbane (Nature DoT CD)"          },
    { ShouldShiv,                  DoShiv,                  "Shiv (poison amp)"                  },
    { ShouldSliceAndDice,          DoSliceAndDice,          "Slice and Dice (refresh)"           },
    { ShouldGarrote,               DoGarrote,               "Garrote (refresh bleed)"            },
    { ShouldRupture,               DoRupture,               "Rupture (refresh 4 CP)"             },
    { ShouldEnvenom,               DoEnvenom,               "Envenom (5 CP spend)"               },
    { ShouldCrimsonTempest,        DoCrimsonTempest,        "Crimson Tempest (AoE bleed copy)"   },
    { ShouldFanOfKnives,           DoFanOfKnives,           "Fan of Knives (AoE gen + spread)"   },
    { ShouldMutilate,              DoMutilate,              "Mutilate (filler)"                  },
    { ShouldPoisonedKnife,         DoPoisonedKnife,         "Poisoned Knife (ranged filler)"     },
    { AlwaysInCombat,              DoAutoAttack,            "Engage auto attack"                 },
};

} // anonymous

void RegisterApl_Rogue_Assassination()
{
    constexpr uint32 SPEC_ROGUE_ASSASSINATION = 259;
    RegisterRotation(CLASS_ROGUE, SPEC_ROGUE_ASSASSINATION, ApRotation{kRules});
}

} // namespace Playerbot::Combat
