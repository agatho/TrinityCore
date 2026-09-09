// Outlaw Rogue - WoW 12.1.0.69587 (Midnight) enterprise rotation.
// Pistol-style melee with the reworked Roll the Bones (dice "sets":
// One of a Kind / Double Trouble / Triple Threat / Jackpot) driving DPS,
// Keep It Rolling to extend a strong roll, Adrenaline Rush burst phase
// with Preparation resetting the burst kit, Blade Flurry cleave, Killing
// Spree finisher, Between the Eyes / Dispatch spend, Pistol Shot proc
// spend (Opportunity), Sinister Strike builder, Ambush stealth opener,
// Slice and Dice upkeep, Instant + Atrophic poison upkeep, Thistle Tea.
//
// Survival: Crimson Vial, Evasion, Cloak of Shadows, Feint (DR), Vanish.
// Group utility: Tricks of the Trade. CC: Kick interrupt, Kidney Shot stun
// (CP), Blind (panic CC). Gap-close: Grappling Hook, Blade Rush.
//
// Validated spell IDs (SpellName.csv lookup, WoW 12.1.0.69587):
//   193315  Sinister Strike       (spec override of 1752)
//   185763  Pistol Shot
//   315341  Between the Eyes
//   2098    Dispatch              (spec override of Eviscerate 196819)
//   1214909 Roll the Bones        (12.1 cast id; 315508 no longer learnable)
//   1214933 One of a Kind         (RtB 1-set buff)
//   1214934 Double Trouble        (RtB 2-set buff)
//   1214935 Triple Threat         (RtB 3-set buff)
//   1214937 Jackpot               (RtB top buff)
//   381989  Keep It Rolling       (talent [R] - extends RtB)
//   13750   Adrenaline Rush       (talent [R])
//   1277933 Preparation           (talent [R] - resets AR/BtE/BF/Blade Rush/KS)
//   13877   Blade Flurry
//   195627  Opportunity           (proc aura - gates Pistol Shot)
//   51690   Killing Spree         (talent [R] - now a CP finisher)
//   271877  Blade Rush            (talent, not in curated build - gated)
//   195457  Grappling Hook        (gap-close)
//   8676    Ambush                (stealth opener)
//   315496  Slice and Dice
//   315584  Instant Poison / 8679 Wound Poison (lethal, baseline)
//   381637  Atrophic Poison       (talent [R] non-lethal) / 3408 Crippling Poison fallback
//   381623  Thistle Tea           (taught by class passive 469779 [R]) / 1298826 active variant
//   1766/408/2094            Kick / Kidney Shot / Blind
//   57934                    Tricks of the Trade
//   185311/5277/31224/1966/1856 Crimson Vial / Evasion / Cloak / Feint / Vanish
//   1784/115191/115192          Stealth / Subterfuge stealth / Subterfuge window
//
// Skipped (with reason):
//   196937  Ghostly Strike        - removed from the game (not in 12.1 SpellName).
//   385408  Sepsis / 137619 Marked for Death / 76577 Smoke Bomb - not learnable in 12.1.
//   385616  Echoing Reprimand     - 12.1 version (470669) is a passive.
//   315508  Roll the Bones (old)  - not learnable in 12.1; 1214909 is the cast.
//   193356/193357/193358/193359/199600/199603 - pre-12.1 RtB buffs, replaced by dice sets.
//   79096   Restless Blades / 256170 Loaded Dice / 279876 Opportunity - passives.
//   199736  Find Treasure         - passive utility (minimap treasure); no cast rule.
//   1776    Gouge                 - [R] but needs facing and Kidney Shot covers the CC slot.
//   1229376 Single-Button Assistant - client convenience macro, not a rotation ability.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated) ----
constexpr uint32 SINISTER_STRIKE      = 193315;
constexpr uint32 PISTOL_SHOT          = 185763;
constexpr uint32 BETWEEN_THE_EYES     = 315341;
constexpr uint32 DISPATCH             = 2098;
// Roll the Bones (12.1 rework): 25 Energy, 45s CD, no CP cost. Rolls dice
// and applies ONE of four tiered buffs instead of the old six named ones.
constexpr uint32 ROLL_THE_BONES       = 1214909;
constexpr uint32 RTB_ONE_OF_A_KIND    = 1214933;      // 1 set  - reroll
constexpr uint32 RTB_DOUBLE_TROUBLE   = 1214934;      // 2 sets - keep
constexpr uint32 RTB_TRIPLE_THREAT    = 1214935;      // 3 sets - keep, Keep It Rolling
constexpr uint32 RTB_JACKPOT          = 1214937;      // top    - keep, Keep It Rolling
constexpr uint32 KEEP_IT_ROLLING      = 381989;       // talent [R] - extend active roll
constexpr uint32 ADRENALINE_RUSH      = 13750;        // talent [R]
constexpr uint32 PREPARATION          = 1277933;      // talent [R] - resets AR/BtE/BF/Blade Rush/KS
constexpr uint32 BLADE_FLURRY         = 13877;
constexpr uint32 OPPORTUNITY          = 195627;       // proc aura (from passive 279876)
constexpr uint32 KILLING_SPREE        = 51690;        // talent [R] - CP finisher burst
constexpr uint32 BLADE_RUSH           = 271877;       // talent - gap close + AoE
constexpr uint32 SLICE_AND_DICE       = 315496;
// Weapon poisons: the self-buff aura carries the cast spell's id.
constexpr uint32 INSTANT_POISON       = 315584;       // baseline lethal
constexpr uint32 ATROPHIC_POISON      = 381637;       // talent [R] non-lethal
constexpr uint32 CRIPPLING_POISON     = 3408;         // baseline non-lethal fallback
// Thistle Tea: class passive 469779 [R] teaches 381623; the active talent
// node variant is 1298826. Cast whichever is in the spellbook.
constexpr uint32 THISTLE_TEA          = 381623;
constexpr uint32 THISTLE_TEA_ALT      = 1298826;
constexpr uint32 KICK                 = 1766;
constexpr uint32 KIDNEY_SHOT          = 408;
constexpr uint32 BLIND                = 2094;
constexpr uint32 TRICKS_OF_TRADE      = 57934;
constexpr uint32 CRIMSON_VIAL         = 185311;
constexpr uint32 EVASION              = 5277;
constexpr uint32 CLOAK_OF_SHADOWS     = 31224;
constexpr uint32 FEINT                = 1966;
constexpr uint32 VANISH               = 1856;
constexpr uint32 GRAPPLING_HOOK       = 195457;       // gap-close
constexpr uint32 STEALTH              = 1784;
constexpr uint32 STEALTH_AURA         = 115191;       // Subterfuge improved
constexpr uint32 SUBTERFUGE_AURA      = 115192;       // post-stealth-break talent window
constexpr uint32 AMBUSH               = 8676;         // stealth-only opener

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
bool PoisonCastWindow(ApPredicateContext const& ctx)
{
    if (ctx.bot.in_combat()) return false;
    if (ctx.bot.is_moving()) return false;
    if (ctx.bot.is_mounted()) return false;
    return true;
}
bool ShouldInstantPoison(ApPredicateContext const& ctx)
{
    if (!PoisonCastWindow(ctx)) return false;
    if (!ctx.bot.knows_spell(INSTANT_POISON)) return false;
    if (!ctx.bot.is_ready(INSTANT_POISON)) return false;
    return !ctx.bot.has_aura(INSTANT_POISON);
}
void DoInstantPoison(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(INSTANT_POISON); }

uint32 PickNonLethalPoison(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(ATROPHIC_POISON))
        return ctx.bot.has_aura(ATROPHIC_POISON) ? 0 : ATROPHIC_POISON;
    if (ctx.bot.knows_spell(CRIPPLING_POISON) && !ctx.bot.has_aura(CRIPPLING_POISON))
        return CRIPPLING_POISON;
    return 0;
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

// ---- Stealth opener: Stealth OOC, Ambush from stealth ----
bool ShouldStealthOOC(ApPredicateContext const& ctx)
{
    if (ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(STEALTH)) return false;
    return !InStealth(ctx);
}
void DoStealthOOC(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(STEALTH); }

bool ShouldAmbushOpener(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(AMBUSH)) return false;
    if (!ctx.bot.is_ready(AMBUSH)) return false;
    return InStealth(ctx);
}
void DoAmbushOpener(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(AMBUSH, ctx.bot.victim());
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
        if (a.is_harmful && a.dispel_type == DispelType::Magic) return true;
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

bool ShouldGrapplingHook(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(GRAPPLING_HOOK)) return false;
    if (!ctx.bot.is_ready(GRAPPLING_HOOK)) return false;
    return ctx.bot.enemies_within(8.0f) == 0;       // gap close
}
void DoGrapplingHook(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(GRAPPLING_HOOK, v->x, v->y, v->z);
    else
        e.cast(GRAPPLING_HOOK);
}

// ---- Major offensive cooldowns ----
bool ShouldAdrenalineRush(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ADRENALINE_RUSH)) return false;
    if (!ctx.bot.is_ready(ADRENALINE_RUSH)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(10.0f) >= 3;
}
void DoAdrenalineRush(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ADRENALINE_RUSH); }

// Killing Spree (12.1): a CP FINISHER (45 Energy + CP) on a 180s CD that
// gains strikes per combo point. Spend it at 5+ CP on boss-tier targets or
// dense packs so the 3-minute CD is not wasted on a single trash mob.
bool ShouldKillingSpree(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(KILLING_SPREE)) return false;
    if (!ctx.bot.is_ready(KILLING_SPREE)) return false;
    if (ComboPoints(ctx) < 5) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(8.0f) >= 3;
}
void DoKillingSpree(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(KILLING_SPREE, ctx.bot.victim());
}

// Preparation: resets Adrenaline Rush, Between the Eyes, Blade Flurry,
// Blade Rush and Killing Spree. Worth it once AR is deep on cooldown and
// the target will still be around (boss-tier) - the reset re-opens a
// full burst window.
bool ShouldPreparation(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(PREPARATION)) return false;
    if (!ctx.bot.is_ready(PREPARATION)) return false;
    if (!ctx.bot.knows_spell(ADRENALINE_RUSH)) return false;
    if (ctx.bot.is_ready(ADRENALINE_RUSH)) return false;
    if (ctx.bot.cd_remaining(ADRENALINE_RUSH).count() < 60000) return false;
    if (ctx.bot.knows_spell(KILLING_SPREE) && ctx.bot.is_ready(KILLING_SPREE)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoPreparation(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(PREPARATION); }

bool ShouldBladeRush(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BLADE_RUSH)) return false;
    if (!ctx.bot.is_ready(BLADE_RUSH)) return false;
    return ctx.bot.enemies_within(8.0f) == 0 || ctx.bot.enemies_within(10.0f) >= 2;
}
void DoBladeRush(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLADE_RUSH); }

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
    if (Energy(ctx) > 40) return false;
    // Starved inside Adrenaline Rush, or plainly empty.
    return ctx.bot.has_aura(ADRENALINE_RUSH) || Energy(ctx) <= 20;
}
void DoThistleTea(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const uint32 sid = BestThistleTeaSpell(ctx);
    if (sid != 0) e.cast(sid);
}

// ---- Maintenance ----
// Roll the Bones (12.1): the cast applies exactly ONE tiered buff -
// One of a Kind (1 set), Double Trouble (2), Triple Threat (3) or
// Jackpot. Returns the active tier's aura, or nullptr when no roll is up.
// `tier` is 1..4 for the four buffs.
AuraEntry const* ActiveRtbBuff(ApPredicateContext const& ctx, int& tier)
{
    constexpr uint32 kTiers[] = { RTB_ONE_OF_A_KIND, RTB_DOUBLE_TROUBLE, RTB_TRIPLE_THREAT, RTB_JACKPOT };
    for (int i = 3; i >= 0; --i)
        if (AuraEntry const* a = ctx.bot.find_aura(kTiers[i]))
        {
            tier = i + 1;
            return a;
        }
    tier = 0;
    return nullptr;
}
bool ShouldRollTheBones(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ROLL_THE_BONES)) return false;
    if (!ctx.bot.is_ready(ROLL_THE_BONES)) return false;
    if (Energy(ctx) < 25) return false;
    int tier = 0;
    AuraEntry const* a = ActiveRtbBuff(ctx, tier);
    // No roll -> roll. A 1-set roll (One of a Kind) -> reroll; Loaded Dice
    // [R] guarantees an upgrade right after Adrenaline Rush. 2+ sets ->
    // keep, refresh only inside the last 6s so uptime is not lost.
    if (!a) return true;
    if (tier <= 1) return true;
    return a->remaining.count() <= 6000;
}
void DoRollTheBones(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ROLL_THE_BONES); }

// Keep It Rolling: 6-minute CD that extends the current roll. Only worth
// spending on a Triple Threat / Jackpot roll with enough time left that
// the extension is not immediately overwritten.
bool ShouldKeepItRolling(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(KEEP_IT_ROLLING)) return false;
    if (!ctx.bot.is_ready(KEEP_IT_ROLLING)) return false;
    int tier = 0;
    AuraEntry const* a = ActiveRtbBuff(ctx, tier);
    if (!a || tier < 3) return false;
    return a->remaining.count() >= 8000;
}
void DoKeepItRolling(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(KEEP_IT_ROLLING); }

// Slice and Dice: Outlaw keeps SnD up manually (Ruthlessness/Fatal
// Flourish feed CP). Refresh at 4+ CP when missing or in the last 5s.
bool ShouldSliceAndDice(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SLICE_AND_DICE)) return false;
    if (!ctx.bot.is_ready(SLICE_AND_DICE)) return false;
    if (ComboPoints(ctx) < 4) return false;
    AuraEntry const* a = ctx.bot.find_aura(SLICE_AND_DICE);
    return !a || a->remaining.count() <= 5000;
}
void DoSliceAndDice(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SLICE_AND_DICE); }

bool ShouldBladeFlurry(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BLADE_FLURRY)) return false;
    if (!ctx.bot.is_ready(BLADE_FLURRY)) return false;
    if (ctx.bot.has_aura(BLADE_FLURRY)) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(8.0f) >= 2;
}
void DoBladeFlurry(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLADE_FLURRY); }

// ---- Spenders / generators ----
bool ShouldBetweenTheEyes(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BETWEEN_THE_EYES)) return false;
    if (!ctx.bot.is_ready(BETWEEN_THE_EYES)) return false;
    return ComboPoints(ctx) >= 5;
}
void DoBetweenTheEyes(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BETWEEN_THE_EYES, ctx.bot.victim());
}

bool ShouldDispatch(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DISPATCH)) return false;
    if (!ctx.bot.is_ready(DISPATCH)) return false;
    return ComboPoints(ctx) >= 5;
}
void DoDispatch(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DISPATCH, ctx.bot.victim());
}

bool ShouldPistolShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(PISTOL_SHOT)) return false;
    if (ComboPoints(ctx) >= 5) return false;
    return ctx.bot.has_aura(OPPORTUNITY);
}
void DoPistolShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(PISTOL_SHOT, ctx.bot.victim());
}

bool ShouldSinisterStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ComboPoints(ctx) >= 5) return false;        // never overcap CP
    return ctx.bot.knows_spell(SINISTER_STRIKE);
}
void DoSinisterStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SINISTER_STRIKE, ctx.bot.victim());
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
//   Cloak (magic emerg) -> Vanish (panic) -> Evasion -> Crimson Vial -> Feint
//   -> Kick -> Kidney Shot (kick fb) -> Blind -> poisons (OOC upkeep)
//   -> Stealth (OOC) -> Ambush opener -> Tricks -> Grappling Hook (gap close)
//   -> Thistle Tea -> major CDs (Adrenaline Rush, Preparation, Blade Rush)
//   -> Roll the Bones / Keep It Rolling -> Blade Flurry -> Slice and Dice
//   -> finishers (Killing Spree, BtE, Dispatch) -> Pistol Shot proc -> Sinister Strike -> AA.
ApRule const kRules[] = {
    { ShouldCloakOfShadows,   DoCloakOfShadows,   "Cloak of Shadows (magic emergency)" },
    { ShouldVanish,           DoVanish,           "Vanish (panic <=25%)"               },
    { ShouldEvasion,          DoEvasion,          "Evasion (<=40%)"                    },
    { ShouldCrimsonVial,      DoCrimsonVial,      "Crimson Vial (<=60%)"               },
    { ShouldFeint,            DoFeint,            "Feint (<=60% DR)"                   },
    { ShouldKick,             DoKick,             "Kick (interrupt)"                   },
    { ShouldKidneyShot,       DoKidneyShot,       "Kidney Shot (interrupt fb)"         },
    { ShouldBlind,            DoBlind,            "Blind (panic CC)"                   },
    { ShouldInstantPoison,    DoInstantPoison,    "Instant Poison (OOC upkeep)"        },
    { ShouldNonLethalPoison,  DoNonLethalPoison,  "Non-lethal poison (OOC upkeep)"     },
    { ShouldStealthOOC,       DoStealthOOC,       "Stealth (OOC opener)"               },
    { ShouldAmbushOpener,     DoAmbushOpener,     "Ambush (stealth opener)"            },
    { ShouldTricks,           DoTricks,           "Tricks of the Trade"                },
    { ShouldGrapplingHook,    DoGrapplingHook,    "Grappling Hook (gap close)"         },
    { ShouldThistleTea,       DoThistleTea,       "Thistle Tea (energy restore)"       },
    { ShouldAdrenalineRush,   DoAdrenalineRush,   "Adrenaline Rush"                    },
    { ShouldPreparation,      DoPreparation,      "Preparation (reset burst CDs)"      },
    { ShouldBladeRush,        DoBladeRush,        "Blade Rush"                         },
    { ShouldRollTheBones,     DoRollTheBones,     "Roll the Bones (reroll/refresh)"    },
    { ShouldKeepItRolling,    DoKeepItRolling,    "Keep It Rolling (3+ sets)"          },
    { ShouldBladeFlurry,      DoBladeFlurry,      "Blade Flurry (2+ AoE)"              },
    { ShouldSliceAndDice,     DoSliceAndDice,     "Slice and Dice (refresh)"           },
    { ShouldKillingSpree,     DoKillingSpree,     "Killing Spree (5 CP finisher)"      },
    { ShouldBetweenTheEyes,   DoBetweenTheEyes,   "Between the Eyes (5 CP finisher)"   },
    { ShouldDispatch,         DoDispatch,         "Dispatch (5 CP spend)"              },
    { ShouldPistolShot,       DoPistolShot,       "Pistol Shot (Opportunity)"          },
    { ShouldSinisterStrike,   DoSinisterStrike,   "Sinister Strike (filler)"           },
    { AlwaysInCombat,         DoAutoAttack,       "Engage auto attack"                 },
};

} // anonymous

void RegisterApl_Rogue_Outlaw()
{
    constexpr uint32 SPEC_ROGUE_OUTLAW = 260;
    RegisterRotation(CLASS_ROGUE, SPEC_ROGUE_OUTLAW, ApRotation{kRules});
}

} // namespace Playerbot::Combat
