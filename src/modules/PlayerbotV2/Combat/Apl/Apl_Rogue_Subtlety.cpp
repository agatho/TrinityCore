// Subtlety Rogue - WoW 12.0 enterprise rotation. Stealth-driven melee with
// Symbols of Death damage window, Shadow Dance multi-Shadowstrike burst,
// Shadow Blades, Eviscerate at 5+ CP, Shuriken Storm + Shuriken Tornado
// AoE, Black Powder AoE finisher, Rupture bleed finisher, Secret Technique
// burst finisher, Sepsis utility CD. Stealth-opener cascade: Garrote
// (silence + bleed) and Cheap Shot (stun) before Shadowstrike.
//
// Survival: Crimson Vial, Evasion, Cloak of Shadows, Feint, Vanish.
// Group utility: Tricks of the Trade, Shroud of Concealment, Smoke Bomb.
// CC: Kick, Cheap Shot (stealth opener), Kidney Shot stun, Blind, Sap.
// Major CDs: Symbols of Death, Shadow Dance, Shadow Blades, Sepsis,
// Marked for Death, Shuriken Tornado (talent — auto Shuriken Storm).
//
// Validated spell IDs (SpellName.csv lookup, WoW 12.0):
//   53      Backstab              — CP gen behind target
//   185438  Shadowstrike          — Shadow Dance / Vanish CP opener
//   196819  Eviscerate            — CP spender
//   319175  Black Powder          — AoE finisher
//   212283  Symbols of Death      — CD self-buff
//   185313  Shadow Dance          — stealth CD
//   197835  Shuriken Storm        — Sub AoE generator (legacy id)
//   1279401 Shuriken Storm        — Sub AoE generator (modern variant)
//   114014  Shuriken Toss         — ranged CP gen
//   277925  Shuriken Tornado      — talent AoE CD
//   280719  Secret Technique      — talent finisher
//   1833    Cheap Shot            — stealth stun opener
//   703     Garrote               — stealth opener silence + bleed
//   1943    Rupture               — bleed finisher
//   200758  Gloomblade            — talent (replaces Backstab)
//   121471  Shadow Blades         — CD
//   394935  Shadowstep (Sub)      — Sub talent gap-closer (was 36554)
//   385408  Sepsis / 137619 Marked for Death / 385616 Echoing Reprimand
//   1766/408/1833/2094/6770/57934/76577 Kick/Kidney/Cheap/Blind/Sap/Tricks/Smoke
//   185311/5277/31224/1966/1856 Crimson Vial / Evasion / Cloak / Feint / Vanish
//   1784/115191                  Stealth / Subterfuge aura
//
// Skipped (with reason):
//   51667   Cut to the Chase      — passive (Eviscerate extends Slice and Dice).
//   196912  Shadow Techniques     — passive (auto-grants CP every few attacks).
//   245687  Dark Shadow           — talent passive (amplifies SoD).
//   199736  Find Treasure         — Outlaw-only passive; not Sub.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated) ----
constexpr uint32 BACKSTAB             = 53;
constexpr uint32 SHADOWSTRIKE         = 185438;
constexpr uint32 EVISCERATE           = 196819;
constexpr uint32 SYMBOLS_OF_DEATH     = 212283;
constexpr uint32 SHADOW_BLADES        = 121471;
constexpr uint32 SHADOW_DANCE         = 185313;
// Shuriken Storm: classic (197835) and modern (1279401) cast ids both
// real at WoW 12.0; bot picks whichever is in the spellbook.
constexpr uint32 SHURIKEN_STORM       = 197835;
constexpr uint32 SHURIKEN_STORM_MOD   = 1279401;
constexpr uint32 SHURIKEN_TORNADO     = 277925;       // talent
constexpr uint32 BLACK_POWDER         = 319175;
constexpr uint32 SECRET_TECHNIQUE     = 280719;       // talent — multi-attack finisher
constexpr uint32 SHURIKEN_TOSS        = 114014;
constexpr uint32 GLOOMBLADE           = 200758;       // talent — replaces Backstab
constexpr uint32 GARROTE              = 703;          // stealth opener bleed + silence
constexpr uint32 RUPTURE              = 1943;         // bleed finisher
constexpr uint32 SEPSIS               = 385408;
constexpr uint32 MARKED_FOR_DEATH     = 137619;
constexpr uint32 ECHOING_REPRIMAND    = 385616;
constexpr uint32 KICK                 = 1766;
constexpr uint32 KIDNEY_SHOT          = 408;
constexpr uint32 CHEAP_SHOT           = 1833;
constexpr uint32 BLIND                = 2094;
constexpr uint32 SAP                  = 6770;
constexpr uint32 STEALTH              = 1784;
constexpr uint32 STEALTH_AURA         = 115191;       // Subterfuge improved version
constexpr uint32 SUBTERFUGE_AURA      = 115192;       // post-stealth-break talent window
constexpr uint32 TRICKS_OF_TRADE      = 57934;
// Shadowstep: Subtlety talent variant (394935) replaces base 36554 when
// talented. Bot probes both — same gap-close behavior either way.
constexpr uint32 SHADOWSTEP           = 36554;
constexpr uint32 SHADOWSTEP_SUB       = 394935;
constexpr uint32 CRIMSON_VIAL         = 185311;
constexpr uint32 EVASION              = 5277;
constexpr uint32 CLOAK_OF_SHADOWS     = 31224;
constexpr uint32 FEINT                = 1966;
constexpr uint32 VANISH               = 1856;
constexpr uint32 SMOKE_BOMB           = 76577;

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

bool InStealth(ApPredicateContext const& ctx)
{
    return ctx.bot.has_aura(STEALTH)
        || ctx.bot.has_aura(STEALTH_AURA)
        || ctx.bot.has_aura(SUBTERFUGE_AURA)
        || ctx.bot.has_aura(SHADOW_DANCE);
}

// ---- Stealth ----
bool ShouldStealthOOC(ApPredicateContext const& ctx)
{
    if (ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(STEALTH)) return false;
    return !InStealth(ctx);
}
void DoStealth(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(STEALTH); }

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
    // Sub uses Vanish as a damage CD (re-stealth -> Shadowstrike) too —
    // pop offensively when symbols is up, defensively when low. In PvP,
    // bump the defensive threshold so the stealth catches the burst.
    const int32 panic = ctx.pvp.under_player_attack ? 40 : 25;
    return ctx.bot.has_aura(SYMBOLS_OF_DEATH) || ctx.bot.hp_pct() <= panic;
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

bool ShouldSmokeBomb(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SMOKE_BOMB)) return false;
    if (!ctx.bot.is_ready(SMOKE_BOMB)) return false;
    return ctx.bot.attackers_count() >= 3;
}
void DoSmokeBomb(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SMOKE_BOMB); }

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

bool ShouldCheapShotOpener(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CHEAP_SHOT)) return false;
    if (!ctx.bot.is_ready(CHEAP_SHOT)) return false;
    return InStealth(ctx);
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

// Shadowstep: prefer Sub talent variant (394935) when known, fall back to
// base 36554. Same behavior — teleport behind victim + clear movement
// impair — gating on no nearby enemy keeps it as a pure gap-close.
uint32 BestShadowstepSpell(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(SHADOWSTEP_SUB) && ctx.bot.is_ready(SHADOWSTEP_SUB))
        return SHADOWSTEP_SUB;
    if (ctx.bot.knows_spell(SHADOWSTEP) && ctx.bot.is_ready(SHADOWSTEP))
        return SHADOWSTEP;
    return 0;
}
bool ShouldShadowstep(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (BestShadowstepSpell(ctx) == 0) return false;
    return ctx.bot.enemies_within(8.0f) == 0;
}
void DoShadowstep(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const uint32 sid = BestShadowstepSpell(ctx);
    if (sid != 0) e.cast(sid, ctx.bot.victim());
}

// ---- Major offensive cooldowns ----
bool ShouldSymbolsOfDeath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SYMBOLS_OF_DEATH)) return false;
    if (!ctx.bot.is_ready(SYMBOLS_OF_DEATH)) return false;
    return true;
}
void DoSymbolsOfDeath(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SYMBOLS_OF_DEATH); }

bool ShouldShadowBlades(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOW_BLADES)) return false;
    if (!ctx.bot.is_ready(SHADOW_BLADES)) return false;
    return ctx.bot.has_aura(SYMBOLS_OF_DEATH) || BossLikeTargetEngaged(ctx);
}
void DoShadowBlades(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SHADOW_BLADES); }

bool ShouldShadowDance(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOW_DANCE)) return false;
    if (!ctx.bot.is_ready(SHADOW_DANCE)) return false;
    return ctx.bot.has_aura(SYMBOLS_OF_DEATH);
}
void DoShadowDance(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SHADOW_DANCE); }

bool ShouldShurikenTornado(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHURIKEN_TORNADO)) return false;
    if (!ctx.bot.is_ready(SHURIKEN_TORNADO)) return false;
    return ctx.bot.enemies_within(15.0f) >= 2;
}
void DoShurikenTornado(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SHURIKEN_TORNADO); }

bool ShouldSepsis(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SEPSIS)) return false;
    if (!ctx.bot.is_ready(SEPSIS)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoSepsis(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SEPSIS, ctx.bot.victim());
}

bool ShouldMarkedForDeath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MARKED_FOR_DEATH)) return false;
    if (!ctx.bot.is_ready(MARKED_FOR_DEATH)) return false;
    return ComboPoints(ctx) <= 1;
}
void DoMarkedForDeath(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MARKED_FOR_DEATH, ctx.bot.victim());
}

bool ShouldEchoingReprimand(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ECHOING_REPRIMAND)) return false;
    if (!ctx.bot.is_ready(ECHOING_REPRIMAND)) return false;
    return ComboPoints(ctx) <= 2;
}
void DoEchoingReprimand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ECHOING_REPRIMAND, ctx.bot.victim());
}

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
// Shuriken Storm: AoE CP generator. Bot might know either the classic
// (197835) or modern variant (1279401). Pick the ready known one.
uint32 BestShurikenStormSpell(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(SHURIKEN_STORM_MOD) && ctx.bot.is_ready(SHURIKEN_STORM_MOD))
        return SHURIKEN_STORM_MOD;
    if (ctx.bot.knows_spell(SHURIKEN_STORM) && ctx.bot.is_ready(SHURIKEN_STORM))
        return SHURIKEN_STORM;
    return 0;
}
bool ShouldShurikenStorm(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (BestShurikenStormSpell(ctx) == 0) return false;
    if (ComboPoints(ctx) >= 5) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(15.0f) >= 2;
}
void DoShurikenStorm(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const uint32 sid = BestShurikenStormSpell(ctx);
    if (sid != 0) e.cast(sid);
}

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

// Stealth-opener Garrote: from stealth Garrote silences for 3s and applies
// a full-duration bleed. Useful even for Sub (which doesn't normally
// maintain bleeds) because the silence locks down a caster mob during
// the opener. Skipped post-opener; refresh duty owned by Rupture.
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

// Rupture: bleed finisher. Sub uses Rupture as a free Find Weakness
// extender at 4+ CP when it's missing or about to fall off. Eviscerate
// is still the top priority at 5 CP — Rupture only fires at 4 CP or
// when no bleed is up.
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

// Rule order (per HANDOFF):
//   Cloak (magic emerg) → Vanish (panic/offensive) → Evasion → Crimson Vial
//   → Feint → Kick → Kidney Shot (kick fb) → Sap (OOC CC) → Blind
//   → Stealth (OOC) → Stealth-opener Garrote → Cheap Shot (stealth open)
//   → Shadowstrike (stealth) → Tricks → Smoke Bomb → Shadowstep (gap close)
//   → major CDs (SoD, SD, SB, Tornado, Sepsis, MfD, ER) → Secret Tech
//   → Black Powder (AoE finisher) → Eviscerate (5 CP) → Rupture (4 CP bleed)
//   → Shuriken Storm (AoE gen) → Gloomblade / Backstab → Shuriken Toss → AA.
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
    { ShouldStealthOOC,         DoStealth,          "Stealth (OOC opener prep)"         },
    { ShouldGarroteOpener,      DoGarroteOpener,    "Garrote (stealth silence opener)"  },
    { ShouldCheapShotOpener,    DoCheapShot,        "Cheap Shot (stealth stun opener)"  },
    { ShouldShadowstrike,       DoShadowstrike,     "Shadowstrike (stealth CP gen)"     },
    { ShouldTricks,             DoTricks,           "Tricks of the Trade"               },
    { ShouldSmokeBomb,          DoSmokeBomb,        "Smoke Bomb (3+ AoE)"               },
    { ShouldShadowstep,         DoShadowstep,       "Shadowstep (gap close)"            },
    { ShouldSymbolsOfDeath,     DoSymbolsOfDeath,   "Symbols of Death"                  },
    { ShouldShadowBlades,       DoShadowBlades,     "Shadow Blades"                     },
    { ShouldShadowDance,        DoShadowDance,      "Shadow Dance"                      },
    { ShouldShurikenTornado,    DoShurikenTornado,  "Shuriken Tornado (2+ AoE)"         },
    { ShouldSepsis,             DoSepsis,           "Sepsis"                            },
    { ShouldMarkedForDeath,     DoMarkedForDeath,   "Marked for Death"                  },
    { ShouldEchoingReprimand,   DoEchoingReprimand, "Echoing Reprimand"                 },
    { ShouldSecretTechnique,    DoSecretTechnique,  "Secret Technique (5 CP)"           },
    { ShouldBlackPowder,        DoBlackPowder,      "Black Powder (3+ AoE 5 CP)"        },
    { ShouldEviscerate,         DoEviscerate,       "Eviscerate (5 CP finisher)"        },
    { ShouldRupture,            DoRupture,          "Rupture (4 CP bleed refresh)"      },
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
