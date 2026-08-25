// Assassination Rogue - WoW 12.0 enterprise rotation. Bleed-driven melee
// with Garrote/Rupture maintenance via aura_stacks-aware refresh, Envenom
// spend at 5+ CP, Slice and Dice attack-speed buff, Crimson Tempest AoE,
// Fan of Knives AoE-generator/poison-spread, and Vendetta / Deathmark
// damage windows. Off-target Garrote/Rupture expansion via
// enemy_without_my_aura where the spec is on the multi-DoT scan list
// (255 only currently — Assassination not yet, so skipped).
//
// Survival: Crimson Vial, Evasion, Cloak of Shadows (magic immune), Feint
// (DR), Vanish (combat drop). Group utility: Tricks of the Trade (tank
// threat), Shroud of Concealment (group stealth), Smoke Bomb (talent —
// AoE no-LOS area). CC: Kick interrupt, Kidney Shot (CP stun), Blind sap,
// Sap (out-of-combat CC). Major CDs: Vendetta / Deathmark, Kingsbane
// (talent — Nature DoT + amp), Sepsis (talent — DoT + free Garrote),
// Indiscriminate Carnage (talent — next Garrote/Rupture multi-applies),
// Marked for Death (5 CP from execute), Echoing Reprimand. Gap-close via
// Shadowstep (Assassination variant 394932). Shiv as a Nature-poison
// amplifier on the current target.
//
// Validated spell IDs (SpellName.csv lookup, WoW 12.0):
//   1329    Mutilate           — CP gen, melee
//   32645   Envenom            — finisher (poison amp)
//   703     Garrote            — bleed (stealth opener + maintained)
//   1943    Rupture            — bleed finisher
//   79140   Vendetta           — CD burst
//   360194  Deathmark          — replaces Vendetta when talented
//   121411  Crimson Tempest    — AoE bleed finisher
//   315496  Slice and Dice     — attack-speed self-buff finisher
//   385408  Sepsis             — talent
//   137619  Marked for Death   — talent (instant 5 CP)
//   385616  Echoing Reprimand  — talent finisher
//   185565  Poisoned Knife     — ranged poison applicator
//   51723   Fan of Knives      — AoE generator + poison spread (NEW)
//   5938    Shiv               — non-lethal poison amp
//   385627  Kingsbane          — talent burst Nature DoT
//   381802  Indiscriminate Carnage — talent AoE bleed enabler
//   394932  Shadowstep (Sin)   — Assassination talent gap-closer (replaces 36554)
//   1766    Kick / 408 Kidney Shot / 2094 Blind / 6770 Sap / 57934 Tricks
//   76577   Smoke Bomb / 114018 Shroud of Concealment
//   185311  Crimson Vial / 5277 Evasion / 31224 Cloak / 1966 Feint / 1856 Vanish
//   1784    Stealth / 115191 Stealth (Subterfuge) / 115192 post-break aura
//
// Skipped (with reason):
//   51667   Cut to the Chase   — passive (Eviscerate/Envenom extends SnD); no cast rule needed.
//   200806  Exsanguinate       — NOT in SpellName.csv at WoW 12.0; removed.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated) ----
constexpr uint32 MUTILATE             = 1329;
constexpr uint32 ENVENOM              = 32645;
constexpr uint32 GARROTE              = 703;
constexpr uint32 RUPTURE              = 1943;
constexpr uint32 VENDETTA             = 79140;
constexpr uint32 DEATHMARK            = 360194;       // talent — replaces Vendetta
constexpr uint32 CRIMSON_TEMPEST      = 121411;
constexpr uint32 SLICE_AND_DICE       = 315496;
constexpr uint32 SEPSIS               = 385408;       // talent
constexpr uint32 MARKED_FOR_DEATH     = 137619;       // talent
constexpr uint32 ECHOING_REPRIMAND    = 385616;       // talent
constexpr uint32 POISONED_KNIFE       = 185565;       // ranged
constexpr uint32 FAN_OF_KNIVES        = 51723;        // AoE generator + poison spread
constexpr uint32 SHIV                 = 5938;         // poison amplifier
constexpr uint32 KINGSBANE            = 385627;       // talent — Nature DoT CD
constexpr uint32 INDISCRIMINATE_CARNAGE = 381802;     // talent — next Garrote/Rupture multi-applies
constexpr uint32 KICK                 = 1766;
constexpr uint32 KIDNEY_SHOT          = 408;
constexpr uint32 BLIND                = 2094;
constexpr uint32 SAP                  = 6770;
constexpr uint32 TRICKS_OF_TRADE      = 57934;
// Shadowstep, Assassination talent variant (replaces base 36554 once
// talented). `knows_spell` checks both — newer characters cap out on the
// variant, older still on 36554. Cast uses whichever is known.
constexpr uint32 SHADOWSTEP           = 36554;
constexpr uint32 SHADOWSTEP_SIN       = 394932;
constexpr uint32 CRIMSON_VIAL         = 185311;
constexpr uint32 EVASION              = 5277;
constexpr uint32 CLOAK_OF_SHADOWS     = 31224;
constexpr uint32 FEINT                = 1966;
constexpr uint32 VANISH               = 1856;
constexpr uint32 SMOKE_BOMB           = 76577;
constexpr uint32 SHROUD_OF_CONCEAL    = 114018;
constexpr uint32 STEALTH              = 1784;
constexpr uint32 STEALTH_AURA         = 115191;       // Subterfuge improved
constexpr uint32 SUBTERFUGE_AURA      = 115192;       // post-stealth-break talent window

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
        || ctx.bot.has_aura(SUBTERFUGE_AURA);
}

// ---- Stealth (OOC opener prep) ----
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

bool ShouldVendetta(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(DEATHMARK)) return false;
    if (!ctx.bot.knows_spell(VENDETTA)) return false;
    if (!ctx.bot.is_ready(VENDETTA)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoVendetta(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(VENDETTA, ctx.bot.victim());
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

bool ShouldIndiscriminateCarnage(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(INDISCRIMINATE_CARNAGE)) return false;
    if (!ctx.bot.is_ready(INDISCRIMINATE_CARNAGE)) return false;
    // Only fire as an AoE prelude — pointless on single-target.
    return ctx.bot.enemies_within(10.0f) >= 3;
}
void DoIndiscriminateCarnage(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(INDISCRIMINATE_CARNAGE);
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

// Shadowstep: prefer Assassination talent variant (394932) when known,
// fall back to base 36554. Either spell teleports behind victim and
// removes movement impair — used as a gap-closer when target is OoR.
uint32 BestShadowstepSpell(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(SHADOWSTEP_SIN) && ctx.bot.is_ready(SHADOWSTEP_SIN))
        return SHADOWSTEP_SIN;
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

bool ShouldCrimsonTempest(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CRIMSON_TEMPEST)) return false;
    if (!ctx.bot.is_ready(CRIMSON_TEMPEST)) return false;
    if (ComboPoints(ctx) < 4) return false;
    return ctx.bot.enemies_within(8.0f) >= 2;
}
void DoCrimsonTempest(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CRIMSON_TEMPEST, ctx.bot.victim());
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

// Rule order (per HANDOFF):
//   Cloak (magic emerg) → Vanish (panic) → Evasion (defensive)
//   → Crimson Vial (heal) → Feint (DR) → Kick → Kidney Shot (kick fb)
//   → Blind → Stealth (OOC) → Stealth-opener Garrote → Tricks → Smoke Bomb
//   → Shadowstep (gap close) → major CDs → maintenance → finishers → generators → AA.
ApRule const kRules[] = {
    { ShouldCloakOfShadows,        DoCloakOfShadows,        "Cloak of Shadows (magic emergency)" },
    { ShouldVanish,                DoVanish,                "Vanish (panic <=25%)"               },
    { ShouldEvasion,               DoEvasion,               "Evasion (<=40%)"                    },
    { ShouldCrimsonVial,           DoCrimsonVial,           "Crimson Vial (<=60%)"               },
    { ShouldFeint,                 DoFeint,                 "Feint (<=60% DR)"                   },
    { ShouldKick,                  DoKick,                  "Kick (interrupt)"                   },
    { ShouldKidneyShot,            DoKidneyShot,            "Kidney Shot (interrupt fb)"         },
    { ShouldBlind,                 DoBlind,                 "Blind (panic CC)"                   },
    { ShouldStealthOOC,            DoStealth,               "Stealth (OOC opener prep)"          },
    { ShouldGarroteOpener,         DoGarroteOpener,         "Garrote (stealth opener)"           },
    { ShouldTricks,                DoTricks,                "Tricks of the Trade"                },
    { ShouldSmokeBomb,             DoSmokeBomb,             "Smoke Bomb (3+ AoE)"                },
    { ShouldShadowstep,            DoShadowstep,            "Shadowstep (gap close)"             },
    { ShouldDeathmark,             DoDeathmark,             "Deathmark"                          },
    { ShouldVendetta,              DoVendetta,              "Vendetta"                           },
    { ShouldKingsbane,             DoKingsbane,             "Kingsbane (Nature DoT CD)"          },
    { ShouldIndiscriminateCarnage, DoIndiscriminateCarnage, "Indiscriminate Carnage (AoE prelude)"},
    { ShouldSepsis,                DoSepsis,                "Sepsis"                             },
    { ShouldMarkedForDeath,        DoMarkedForDeath,        "Marked for Death"                   },
    { ShouldEchoingReprimand,      DoEchoingReprimand,      "Echoing Reprimand"                  },
    { ShouldShiv,                  DoShiv,                  "Shiv (poison amp)"                  },
    { ShouldSliceAndDice,          DoSliceAndDice,          "Slice and Dice (refresh)"           },
    { ShouldGarrote,               DoGarrote,               "Garrote (refresh bleed)"            },
    { ShouldRupture,               DoRupture,               "Rupture (refresh 4 CP)"             },
    { ShouldCrimsonTempest,        DoCrimsonTempest,        "Crimson Tempest (2+ AoE)"           },
    { ShouldEnvenom,               DoEnvenom,               "Envenom (5 CP spend)"               },
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
