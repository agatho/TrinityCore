// Subtlety Rogue - WoW 12.1.0.69587 (Midnight) enterprise rotation.
// Stealth-driven melee: Shadow Dance multi-Shadowstrike burst (2 charges
// with Double Dance), Shadow Blades burst window, Vanish as an offensive
// re-stealth inside Shadow Blades, Goremaw's Bite CP burst, Eviscerate at
// 5+ CP, Secret Technique burst finisher, Black Powder AoE finisher,
// Shuriken Storm AoE generator, Slice and Dice upkeep, Backstab /
// Gloomblade filler, Shuriken Toss ranged filler. Instant + Atrophic
// poison upkeep out of combat, Thistle Tea when Energy-starved.
//
// Survival: Crimson Vial, Evasion, Cloak of Shadows, Feint, Vanish.
// Group utility: Tricks of the Trade. CC: Kick, Cheap Shot (stealth
// opener vs casters / PvP), Kidney Shot stun, Blind, Sap (OOC).
// Gap-close: Shadowstep (36554, learned through spec spell 394935).
//
// Validated spell IDs (SpellName.csv lookup, WoW 12.1.0.69587):
//   53      Backstab              - CP gen (spec override of 1752)
//   200758  Gloomblade            - talent (replaces Backstab; not in curated build)
//   185438  Shadowstrike          - stealth / Shadow Dance CP gen (overrides Ambush)
//   196819  Eviscerate            - CP spender
//   319175  Black Powder          - AoE finisher
//   280719  Secret Technique      - spec finisher (L25)
//   185313  Shadow Dance          - stealth CD (charges)
//   121471  Shadow Blades         - talent [R] burst CD
//   426591  Goremaw's Bite        - talent [R] 45s CP burst + bleed
//   197835  Shuriken Storm        - AoE generator
//   114014  Shuriken Toss         - ranged CP gen
//   315496  Slice and Dice        - attack-speed self-buff finisher
//   1833    Cheap Shot            - stealth stun opener
//   315584  Instant Poison        - baseline lethal poison
//   381637  Atrophic Poison       - talent [R] non-lethal / 3408 Crippling Poison fallback
//   381623  Thistle Tea           - taught by class passive 469779 / 1298826 active variant
//   36554   Shadowstep            - cast id; 394935 is the Sub spec learn-spell
//   1766/408/2094/6770/57934     Kick / Kidney Shot / Blind / Sap / Tricks
//   185311/5277/31224/1966/1856  Crimson Vial / Evasion / Cloak / Feint / Vanish
//   1784/115191/115192           Stealth / Subterfuge stealth / Subterfuge window
//
// Skipped (with reason):
//   212283  Symbols of Death      - not learnable in 12.1 (removed from Sub).
//   703     Garrote / 1943 Rupture - Assassination-only in 12.1; Sub cannot learn them.
//   277925  Shuriken Tornado      - 12.1 version (1264764) is a passive (Shadow Clone proc).
//   1279401 Shuriken Storm Rank 2 - passive damage modifier, not a cast.
//   385408  Sepsis / 137619 Marked for Death / 76577 Smoke Bomb - not learnable in 12.1.
//   385616  Echoing Reprimand     - 12.1 version (470669) is a passive.
//   196912  Shadow Techniques / 91023 Find Weakness / 245687 Dark Shadow - passives.
//   1776    Gouge                 - class talent not in the curated Sub build; needs facing.
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
constexpr uint32 BACKSTAB             = 53;
constexpr uint32 SHADOWSTRIKE         = 185438;
constexpr uint32 EVISCERATE           = 196819;
constexpr uint32 SHADOW_BLADES        = 121471;       // talent [R] burst CD
constexpr uint32 SHADOW_DANCE         = 185313;       // charges (Double Dance [R])
constexpr uint32 GOREMAWS_BITE        = 426591;       // talent [R] CP burst + bleed
constexpr uint32 SHURIKEN_STORM       = 197835;
constexpr uint32 BLACK_POWDER         = 319175;
constexpr uint32 SECRET_TECHNIQUE     = 280719;       // spec finisher (L25)
constexpr uint32 SHURIKEN_TOSS        = 114014;
constexpr uint32 GLOOMBLADE           = 200758;       // talent - replaces Backstab
constexpr uint32 SLICE_AND_DICE       = 315496;
// Weapon poisons: the self-buff aura carries the cast spell's id.
constexpr uint32 INSTANT_POISON       = 315584;       // baseline lethal
constexpr uint32 ATROPHIC_POISON      = 381637;       // talent [R] non-lethal
constexpr uint32 CRIPPLING_POISON     = 3408;         // baseline non-lethal fallback
// Thistle Tea: class passive 469779 teaches 381623; the active talent
// node variant is 1298826. Cast whichever is in the spellbook.
constexpr uint32 THISTLE_TEA          = 381623;
constexpr uint32 THISTLE_TEA_ALT      = 1298826;
constexpr uint32 KICK                 = 1766;
constexpr uint32 KIDNEY_SHOT          = 408;
constexpr uint32 CHEAP_SHOT           = 1833;
constexpr uint32 BLIND                = 2094;
constexpr uint32 SAP                  = 6770;
constexpr uint32 STEALTH              = 1784;
constexpr uint32 STEALTH_AURA         = 115191;       // Subterfuge improved version
constexpr uint32 SUBTERFUGE_AURA      = 115192;       // post-stealth-break talent window
constexpr uint32 TRICKS_OF_TRADE      = 57934;
// Shadowstep: 36554 is the castable. 394935 is the Subtlety spec spell
// that teaches it (SpellLearnSpell 394935 -> 36554); gate on either being
// known, always cast 36554.
constexpr uint32 SHADOWSTEP           = 36554;
constexpr uint32 SHADOWSTEP_SUB       = 394935;
constexpr uint32 CRIMSON_VIAL         = 185311;
constexpr uint32 EVASION              = 5277;
constexpr uint32 CLOAK_OF_SHADOWS     = 31224;
constexpr uint32 FEINT                = 1966;
constexpr uint32 VANISH               = 1856;

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
        || ctx.bot.has_aura(SUBTERFUGE_AURA)
        || ctx.bot.has_aura(SHADOW_DANCE);
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

// ---- Stealth ----
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
    if (Energy(ctx) > 40) return false;
    // Starved inside a Shadow Dance / Shadow Blades window, or plainly empty.
    return ctx.bot.has_aura(SHADOW_DANCE) || ctx.bot.has_aura(SHADOW_BLADES) || Energy(ctx) <= 20;
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
    // Sub uses Vanish as a damage CD (re-stealth -> Shadowstrike) too -
    // pop offensively inside Shadow Blades when not already stealthed /
    // dancing, defensively when low. In PvP, bump the defensive threshold
    // so the stealth catches the burst.
    const int32 panic = ctx.pvp.under_player_attack ? 40 : 25;
    const bool offensive = ctx.bot.has_aura(SHADOW_BLADES) && !InStealth(ctx) && ComboPoints(ctx) <= 3;
    return offensive || ctx.bot.hp_pct() <= panic;
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

// Cheap Shot from stealth: only worth the stealth break (and 40 Energy)
// when it locks down a caster or a player - otherwise Shadowstrike is the
// DPS opener and Kidney Shot covers stuns later.
bool ShouldCheapShotOpener(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CHEAP_SHOT)) return false;
    if (!ctx.bot.is_ready(CHEAP_SHOT)) return false;
    if (!InStealth(ctx)) return false;
    if (ctx.pvp.in_battleground || ctx.pvp.in_arena) return true;
    NearbyUnit const* v = ctx.bot.victim_info();
    return v && v->is_casting;
}
void DoCheapShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CHEAP_SHOT, ctx.bot.victim());
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

// Shadowstep: 36554 is the castable in 12.1; the Subtlety spec spell
// 394935 only teaches it. Gate on either id being in the spellbook, cast
// 36554. Gating on no nearby enemy keeps it as a pure gap-close.
bool KnowsShadowstep(ApPredicateContext const& ctx)
{
    return ctx.bot.knows_spell(SHADOWSTEP) || ctx.bot.knows_spell(SHADOWSTEP_SUB);
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

// ---- Major offensive cooldowns ----
// Shadow Blades (90s): burst window - double CP generation. Boss-tier or
// a real pack; trash singles do not deserve the 90s CD.
bool ShouldShadowBlades(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOW_BLADES)) return false;
    if (!ctx.bot.is_ready(SHADOW_BLADES)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(10.0f) >= 3;
}
void DoShadowBlades(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SHADOW_BLADES); }

// Shadow Dance (charges): enables Shadowstrike spam. Enter it with CP
// headroom so the empowered generators are not wasted, never while a
// stealth state is already active.
bool ShouldShadowDance(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOW_DANCE)) return false;
    if (!ctx.bot.is_ready(SHADOW_DANCE)) return false;
    if (InStealth(ctx)) return false;
    if (Energy(ctx) < 50) return false;         // need at least one Shadowstrike
    return ComboPoints(ctx) <= 3 || ctx.bot.has_aura(SHADOW_BLADES);
}
void DoShadowDance(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SHADOW_DANCE); }

// Goremaw's Bite (45s): hits the target + 2 nearby, generates CP and a
// bleed, and echoes finisher damage as Shadow. Use with CP headroom.
bool ShouldGoremawsBite(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(GOREMAWS_BITE)) return false;
    if (!ctx.bot.is_ready(GOREMAWS_BITE)) return false;
    return ComboPoints(ctx) <= 2;
}
void DoGoremawsBite(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(GOREMAWS_BITE, ctx.bot.victim());
}

// ---- Maintenance ----
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

// ---- Spenders ----
bool ShouldSecretTechnique(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SECRET_TECHNIQUE)) return false;
    if (!ctx.bot.is_ready(SECRET_TECHNIQUE)) return false;
    return ComboPoints(ctx) >= 5;
}
void DoSecretTechnique(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SECRET_TECHNIQUE, ctx.bot.victim());
}

bool ShouldBlackPowder(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BLACK_POWDER)) return false;
    if (ComboPoints(ctx) < 5) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(10.0f) >= 3;
}
void DoBlackPowder(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLACK_POWDER); }

bool ShouldEviscerate(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(EVISCERATE)) return false;
    if (!ctx.bot.is_ready(EVISCERATE)) return false;
    return ComboPoints(ctx) >= 5;
}
void DoEviscerate(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(EVISCERATE, ctx.bot.victim());
}

// ---- Generators ----
// Shuriken Storm: AoE CP generator (10y). 1279401 "Rank 2" is a passive
// stealth damage modifier, not a second cast id.
bool ShouldShurikenStorm(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHURIKEN_STORM)) return false;
    if (!ctx.bot.is_ready(SHURIKEN_STORM)) return false;
    if (ComboPoints(ctx) >= 5) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(10.0f) >= 2;
}
void DoShurikenStorm(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SHURIKEN_STORM); }

bool ShouldShadowstrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOWSTRIKE)) return false;
    if (!ctx.bot.is_ready(SHADOWSTRIKE)) return false;
    if (ComboPoints(ctx) >= 5) return false;
    // Shadowstrike requires stealth/Shadow Dance — gate accordingly.
    return InStealth(ctx);
}
void DoShadowstrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHADOWSTRIKE, ctx.bot.victim());
}

bool ShouldGloomblade(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(GLOOMBLADE)) return false;
    if (ComboPoints(ctx) >= 5) return false;
    return ctx.bot.is_ready(GLOOMBLADE);
}
void DoGloomblade(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(GLOOMBLADE, ctx.bot.victim());
}

bool ShouldBackstab(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(GLOOMBLADE)) return false;
    if (ComboPoints(ctx) >= 5) return false;
    return ctx.bot.knows_spell(BACKSTAB);
}
void DoBackstab(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BACKSTAB, ctx.bot.victim());
}

bool ShouldShurikenToss(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHURIKEN_TOSS)) return false;
    if (ComboPoints(ctx) >= 5) return false;
    return ctx.bot.enemies_within(8.0f) == 0;
}
void DoShurikenToss(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHURIKEN_TOSS, ctx.bot.victim());
}

// Sap: OOC CC. Stealthed, premium-target CC for pulls. Fires only when
// OOC + stealthed + an enemy is within 10y AND isn't already our
// engagement target (we don't want to Sap the mob we just opened on).
bool ShouldSap(ApPredicateContext const& ctx)
{
    if (ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SAP)) return false;
    if (!ctx.bot.is_ready(SAP)) return false;
    if (!InStealth(ctx)) return false;
    // Don't Sap our combat target — that would break a planned pull.
    // Only useful when we have a second nearby enemy.
    return ctx.bot.enemies_within(10.0f) >= 2;
}
void DoSap(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SAP, ctx.bot.victim());
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
//   Cloak (magic emerg) -> Vanish (panic/offensive) -> Evasion -> Crimson Vial
//   -> Feint -> Kick -> Kidney Shot (kick fb) -> Sap (OOC CC) -> Blind
//   -> poisons (OOC upkeep) -> Stealth (OOC) -> Cheap Shot (stealth open vs caster)
//   -> Shadowstrike (stealth) -> Tricks -> Shadowstep (gap close) -> Thistle Tea
//   -> major CDs (Shadow Blades, Shadow Dance, Goremaw's Bite) -> Slice and Dice
//   -> Secret Tech -> Black Powder (AoE finisher) -> Eviscerate (5 CP)
//   -> Shuriken Storm (AoE gen) -> Gloomblade / Backstab -> Shuriken Toss -> AA.
ApRule const kRules[] = {
    { ShouldCloakOfShadows,     DoCloakOfShadows,   "Cloak of Shadows (magic emergency)"},
    { ShouldVanish,             DoVanish,           "Vanish (offensive/panic)"          },
    { ShouldEvasion,            DoEvasion,          "Evasion (<=40%)"                   },
    { ShouldCrimsonVial,        DoCrimsonVial,      "Crimson Vial (<=60%)"              },
    { ShouldFeint,              DoFeint,            "Feint (<=60% DR)"                  },
    { ShouldKick,               DoKick,             "Kick (interrupt)"                  },
    { ShouldKidneyShot,         DoKidneyShot,       "Kidney Shot (interrupt fb)"        },
    { ShouldSap,                DoSap,              "Sap (OOC CC second target)"        },
    { ShouldBlind,              DoBlind,            "Blind (panic CC)"                  },
    { ShouldInstantPoison,      DoInstantPoison,    "Instant Poison (OOC upkeep)"       },
    { ShouldNonLethalPoison,    DoNonLethalPoison,  "Non-lethal poison (OOC upkeep)"    },
    { ShouldStealthOOC,         DoStealth,          "Stealth (OOC opener prep)"         },
    { ShouldCheapShotOpener,    DoCheapShot,        "Cheap Shot (stealth stun opener)"  },
    { ShouldShadowstrike,       DoShadowstrike,     "Shadowstrike (stealth CP gen)"     },
    { ShouldTricks,             DoTricks,           "Tricks of the Trade"               },
    { ShouldShadowstep,         DoShadowstep,       "Shadowstep (gap close)"            },
    { ShouldThistleTea,         DoThistleTea,       "Thistle Tea (energy restore)"      },
    { ShouldShadowBlades,       DoShadowBlades,     "Shadow Blades"                     },
    { ShouldShadowDance,        DoShadowDance,      "Shadow Dance"                      },
    { ShouldGoremawsBite,       DoGoremawsBite,     "Goremaw's Bite (CP burst)"         },
    { ShouldSliceAndDice,       DoSliceAndDice,     "Slice and Dice (refresh)"          },
    { ShouldSecretTechnique,    DoSecretTechnique,  "Secret Technique (5 CP)"           },
    { ShouldBlackPowder,        DoBlackPowder,      "Black Powder (3+ AoE 5 CP)"        },
    { ShouldEviscerate,         DoEviscerate,       "Eviscerate (5 CP finisher)"        },
    { ShouldShurikenStorm,      DoShurikenStorm,    "Shuriken Storm (AoE gen)"          },
    { ShouldGloomblade,         DoGloomblade,       "Gloomblade (filler)"               },
    { ShouldBackstab,           DoBackstab,         "Backstab (filler)"                 },
    { ShouldShurikenToss,       DoShurikenToss,     "Shuriken Toss (range filler)"      },
    { AlwaysInCombat,           DoAutoAttack,       "Engage auto attack"                },
};

} // anonymous

void RegisterApl_Rogue_Subtlety()
{
    constexpr uint32 SPEC_ROGUE_SUBTLETY = 261;
    RegisterRotation(CLASS_ROGUE, SPEC_ROGUE_SUBTLETY, ApRotation{kRules});
}

} // namespace Playerbot::Combat
