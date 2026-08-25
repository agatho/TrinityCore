// Outlaw Rogue - WoW 12.0 enterprise rotation. Pistol-style melee with
// Roll the Bones buff window driving DPS, Adrenaline Rush burst phase,
// Blade Flurry cleave, Ghostly Strike debuff, Pistol Shot proc spend
// (Opportunity), Sinister Strike + Ambush opener.
//
// Survival: Crimson Vial, Evasion, Cloak of Shadows, Feint (DR), Vanish.
// Group utility: Tricks of the Trade, Smoke Bomb, Shroud of Concealment.
// CC: Kick interrupt, Cheap Shot (stealth opener), Kidney Shot stun (CP),
// Blind sap, Sap (OOC), Gouge incapacitate. Major CDs: Adrenaline Rush,
// Roll the Bones (refresh under threshold), Killing Spree (talent burst),
// Sepsis (talent), Marked for Death, Ghostly Strike maintenance.
//
// Validated spell IDs (SpellName.csv lookup, WoW 12.0):
//   193315  Sinister Strike       (Outlaw replaces 1752; both real)
//   185763  Pistol Shot
//   315341  Between the Eyes      (modern ID — replaces old 199804)
//   2098    Dispatch
//   315508  Roll the Bones        (classic cast id)
//   1214909 Roll the Bones        (modern variant — both supported)
//   13750   Adrenaline Rush
//   13877   Blade Flurry
//   196937  Ghostly Strike        (talent)
//   195627  Opportunity           (proc aura — gates Pistol Shot)
//   51690   Killing Spree
//   385408  Sepsis
//   137619  Marked for Death
//   385616  Echoing Reprimand
//   271877  Blade Rush            (talent)
//   195457  Grappling Hook        (gap-close)
//   8676    Ambush                (stealth opener)
//   1766/408/2094/6770/1776  Kick / Kidney Shot / Blind / Sap / Gouge
//   57934/76577/114018/195457   Tricks / Smoke Bomb / Shroud / Grappling Hook
//   185311/5277/31224/1966/1856 Crimson Vial / Evasion / Cloak / Feint / Vanish
//   1784/115191/115192          Stealth / Subterfuge / post-break aura
//
// Skipped (with reason):
//   51667   Cut to the Chase      — passive (Sinister Strike auto-extends Slice and Dice / RtB).
//   79096   Restless Blades       — passive (finishers reduce CD on RtB / AR / KS / GS / SS / Vanish / BR / Sepsis).
//   199736  Find Treasure         — passive utility (auto-loots coin); no cast rule.
//   199804  Between the Eyes (old) — pre-12.0 cast id; SpellName.csv at 12.0 has 315341 only.
//   1752    Sinister Strike (old) — baseline-only; spec uses 193315.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated) ----
constexpr uint32 SINISTER_STRIKE      = 193315;
constexpr uint32 PISTOL_SHOT          = 185763;
constexpr uint32 BETWEEN_THE_EYES     = 315341;       // modern ID (was 199804 pre-12.0)
constexpr uint32 DISPATCH             = 2098;
// Roll the Bones: TWO valid cast ids in WoW 12.0 — 315508 (classic) and
// 1214909 (modern variant). Bots probe both via `knows_spell` and cast
// whichever is known so the rotation works pre/post tree migration.
constexpr uint32 ROLL_THE_BONES       = 315508;
constexpr uint32 ROLL_THE_BONES_MOD   = 1214909;
constexpr uint32 ADRENALINE_RUSH      = 13750;
constexpr uint32 BLADE_FLURRY         = 13877;
constexpr uint32 GHOSTLY_STRIKE       = 196937;
constexpr uint32 OPPORTUNITY          = 195627;
constexpr uint32 KILLING_SPREE        = 51690;       // talent burst
constexpr uint32 SEPSIS               = 385408;
constexpr uint32 MARKED_FOR_DEATH     = 137619;
constexpr uint32 ECHOING_REPRIMAND    = 385616;
constexpr uint32 BLADE_RUSH           = 271877;       // talent — gap close + AoE
constexpr uint32 KICK                 = 1766;
constexpr uint32 KIDNEY_SHOT          = 408;
constexpr uint32 BLIND                = 2094;
constexpr uint32 SAP                  = 6770;
constexpr uint32 GOUGE                = 1776;
constexpr uint32 TRICKS_OF_TRADE      = 57934;
constexpr uint32 CRIMSON_VIAL         = 185311;
constexpr uint32 EVASION              = 5277;
constexpr uint32 CLOAK_OF_SHADOWS     = 31224;
constexpr uint32 FEINT                = 1966;
constexpr uint32 VANISH               = 1856;
constexpr uint32 SMOKE_BOMB           = 76577;
constexpr uint32 GRAPPLING_HOOK       = 195457;       // gap-close
constexpr uint32 STEALTH              = 1784;
constexpr uint32 STEALTH_AURA         = 115191;       // Subterfuge improved
constexpr uint32 SUBTERFUGE_AURA      = 115192;       // post-stealth-break talent window
constexpr uint32 AMBUSH               = 8676;         // stealth-only opener (5 CP gen)

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

bool ShouldKillingSpree(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(KILLING_SPREE)) return false;
    if (!ctx.bot.is_ready(KILLING_SPREE)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoKillingSpree(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(KILLING_SPREE, ctx.bot.victim());
}

bool ShouldBladeRush(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BLADE_RUSH)) return false;
    if (!ctx.bot.is_ready(BLADE_RUSH)) return false;
    return ctx.bot.enemies_within(8.0f) == 0 || ctx.bot.enemies_within(10.0f) >= 2;
}
void DoBladeRush(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLADE_RUSH); }

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

// ---- Maintenance ----
// Roll the Bones cast id resolver — bot might know either the classic
// (315508) or modern (1214909) cast spell. Return whichever is known
// AND ready; 0 means neither is castable this tick.
uint32 BestRollTheBonesSpell(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(ROLL_THE_BONES_MOD) && ctx.bot.is_ready(ROLL_THE_BONES_MOD))
        return ROLL_THE_BONES_MOD;
    if (ctx.bot.knows_spell(ROLL_THE_BONES) && ctx.bot.is_ready(ROLL_THE_BONES))
        return ROLL_THE_BONES;
    return 0;
}
bool ShouldRollTheBones(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (BestRollTheBonesSpell(ctx) == 0) return false;
    if (ComboPoints(ctx) < 5) return false;
    // The CAST spell id 315508 is NOT applied as an aura — server
    // instead applies one or more of the six RtB-family buffs:
    //   Broadside 193356, Ruthless Precision 193357, Grand Melee 193358,
    //   True Bearing 193359, Buried Treasure 199600, Skull and
    //   Crossbones 199603. Walk the aura set to count how many are
    //   active and find the shortest remaining duration.
    constexpr uint32 BROADSIDE            = 193356;
    constexpr uint32 RUTHLESS_PRECISION   = 193357;
    constexpr uint32 GRAND_MELEE          = 193358;
    constexpr uint32 TRUE_BEARING         = 193359;
    constexpr uint32 BURIED_TREASURE      = 199600;
    constexpr uint32 SKULL_AND_CROSSBONES = 199603;
    constexpr uint32 kRtbBuffs[] = {
        BROADSIDE, RUTHLESS_PRECISION, GRAND_MELEE, TRUE_BEARING,
        BURIED_TREASURE, SKULL_AND_CROSSBONES,
    };
    int active = 0;
    int64_t shortest_ms = INT64_MAX;
    for (uint32 bid : kRtbBuffs)
    {
        if (AuraEntry const* a = ctx.bot.find_aura(bid))
        {
            ++active;
            if (a->remaining.count() < shortest_ms)
                shortest_ms = a->remaining.count();
        }
    }
    // No buffs → reroll. Single-buff roll → reroll (suboptimal).
    // 2+ buffs ("multi-roll" — top tier) → only reroll when within
    // 6s of expiry to avoid losing uptime mid-execute. WoW 12.0
    // Outlaw guidance: keep rolls with 2+ buffs, reroll otherwise.
    if (active == 0) return true;
    if (active < 2) return true;
    return shortest_ms <= 6000;
}
void DoRollTheBones(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const uint32 sid = BestRollTheBonesSpell(ctx);
    if (sid != 0) e.cast(sid);
}

bool ShouldBladeFlurry(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BLADE_FLURRY)) return false;
    if (!ctx.bot.is_ready(BLADE_FLURRY)) return false;
    if (ctx.bot.has_aura(BLADE_FLURRY)) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(8.0f) >= 2;
}
void DoBladeFlurry(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLADE_FLURRY); }

bool ShouldGhostlyStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(GHOSTLY_STRIKE)) return false;
    if (!ctx.bot.is_ready(GHOSTLY_STRIKE)) return false;
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v) return false;
    return !ctx.bot.has_aura(GHOSTLY_STRIKE, v->guid);
}
void DoGhostlyStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(GHOSTLY_STRIKE, ctx.bot.victim());
}

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

// Rule order (per HANDOFF):
//   Cloak (magic emerg) → Vanish (panic) → Evasion → Crimson Vial → Feint
//   → Kick → Kidney Shot (kick fb) → Blind → Stealth (OOC) → Ambush opener
//   → Tricks → Smoke Bomb → Grappling Hook (gap close) → major CDs
//   → Roll the Bones (maintenance) → Blade Flurry → Ghostly Strike
//   → finishers (BtE, Dispatch) → Pistol Shot proc → Sinister Strike → AA.
ApRule const kRules[] = {
    { ShouldCloakOfShadows,   DoCloakOfShadows,   "Cloak of Shadows (magic emergency)" },
    { ShouldVanish,           DoVanish,           "Vanish (panic <=25%)"               },
    { ShouldEvasion,          DoEvasion,          "Evasion (<=40%)"                    },
    { ShouldCrimsonVial,      DoCrimsonVial,      "Crimson Vial (<=60%)"               },
    { ShouldFeint,            DoFeint,            "Feint (<=60% DR)"                   },
    { ShouldKick,             DoKick,             "Kick (interrupt)"                   },
    { ShouldKidneyShot,       DoKidneyShot,       "Kidney Shot (interrupt fb)"         },
    { ShouldBlind,            DoBlind,            "Blind (panic CC)"                   },
    { ShouldStealthOOC,       DoStealthOOC,       "Stealth (OOC opener)"               },
    { ShouldAmbushOpener,     DoAmbushOpener,     "Ambush (stealth opener)"            },
    { ShouldTricks,           DoTricks,           "Tricks of the Trade"                },
    { ShouldSmokeBomb,        DoSmokeBomb,        "Smoke Bomb (3+ AoE)"                },
    { ShouldGrapplingHook,    DoGrapplingHook,    "Grappling Hook (gap close)"         },
    { ShouldAdrenalineRush,   DoAdrenalineRush,   "Adrenaline Rush"                    },
    { ShouldKillingSpree,     DoKillingSpree,     "Killing Spree"                      },
    { ShouldBladeRush,        DoBladeRush,        "Blade Rush"                         },
    { ShouldSepsis,           DoSepsis,           "Sepsis"                             },
    { ShouldMarkedForDeath,   DoMarkedForDeath,   "Marked for Death"                   },
    { ShouldEchoingReprimand, DoEchoingReprimand, "Echoing Reprimand"                  },
    { ShouldRollTheBones,     DoRollTheBones,     "Roll the Bones (5 CP refresh)"      },
    { ShouldBladeFlurry,      DoBladeFlurry,      "Blade Flurry (2+ AoE)"              },
    { ShouldGhostlyStrike,    DoGhostlyStrike,    "Ghostly Strike (debuff)"            },
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
