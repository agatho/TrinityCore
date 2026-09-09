// Apl_Baseline_Rogue.cpp — baseline rotation for class CLASS_ROGUE (spec=0). Extracted from the monolithic Apl_Baseline.cpp on the split refactor; future edits go
// here exclusively. See Apl_Baseline_Common.h for the
// shared helpers + rule macros.
//
// To audit coverage:
//   python src/modules/PlayerbotV2/tools/baseline_coverage_audit.py

#include "Apl_Baseline_Common.h"

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::HasLiveTarget;
using ::Playerbot::Combat::baseline_common::AlwaysInCombat;
using ::Playerbot::Combat::baseline_common::DoAutoAttack;

// Class baseline ids, WoW 12.1.0.69587 (SkillLineAbility, all specs).
constexpr uint32 SINISTER_STRIKE       = 1752;    // L1 - builder (spec overrides keep it castable)
constexpr uint32 EVISCERATE            = 196819;  // L2 - CP finisher
constexpr uint32 SLICE_AND_DICE        = 315496;  // L9 - attack-speed finisher
constexpr uint32 KIDNEY_SHOT           = 408;     // L13 - CP stun
constexpr uint32 EVASION               = 5277;    // L21 - 100% dodge vs physical, 10s
constexpr uint32 VANISH                = 1856;    // L17 - drop combat, restealth
constexpr uint32 CLOAK_OF_SHADOWS      = 31224;   // L47 - magic immunity
constexpr uint32 STEALTH               = 1784;    // L3 - OOC stealth; aura id == spell id
constexpr uint32 CHEAP_SHOT            = 1833;    // L3 - stealth-only 4s stun opener (2 CP)
constexpr uint32 AMBUSH                = 8676;    // L7 - stealth-only opener (2 CP)
constexpr uint32 KICK                  = 1766;    // L6 - interrupt
constexpr uint32 CRIMSON_VIAL          = 185311;  // L8 - self HoT

constexpr uint8 POWER_COMBO_POINTS_IDX = 4;

uint8 ComboPoints(ApPredicateContext const& ctx)
{
    return static_cast<uint8>(ctx.bot.power(POWER_COMBO_POINTS_IDX));
}

BASELINE_INTERRUPT_RULE(Kick,       KICK)
BASELINE_DEFENSIVE_RULE(CrimsonVial, CRIMSON_VIAL, 60)

// Finishers need combo points: firing Eviscerate / Kidney Shot / Slice
// and Dice on 0 CP fails the cast and wastes the tick, so every finisher
// below is CP-gated instead of using the plain BASELINE_SPELL_RULE.
bool ShouldEviscerate(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.is_ready(EVISCERATE)) return false;
    return ComboPoints(ctx) >= 3;
}
void DoEviscerate(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(EVISCERATE, ctx.bot.victim());
}

// Kidney Shot (L13): CP stun. Spend it on a casting mob (interrupt
// fallback when Kick is down) or when the bot is getting low.
bool ShouldKidneyShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(KIDNEY_SHOT) || !ctx.bot.is_ready(KIDNEY_SHOT)) return false;
    if (ComboPoints(ctx) < 3) return false;
    NearbyUnit const* v = ctx.bot.victim_info();
    const bool caster = v && v->is_casting && !ctx.bot.is_ready(KICK);
    return caster || ctx.bot.hp_pct() <= 50;
}
void DoKidneyShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(KIDNEY_SHOT, ctx.bot.victim());
}

// Slice and Dice (L9): attack-speed buff. Put it up at 2+ CP when missing
// or about to fall off; Eviscerate takes the CP otherwise.
bool ShouldSliceAndDice(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SLICE_AND_DICE) || !ctx.bot.is_ready(SLICE_AND_DICE)) return false;
    if (ComboPoints(ctx) < 2) return false;
    AuraEntry const* a = ctx.bot.find_aura(SLICE_AND_DICE, ObjectGuid::Empty);
    return !a || a->remaining.count() <= 3000;
}
void DoSliceAndDice(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(SLICE_AND_DICE, ObjectGuid::Empty);
}

// Evasion: 100% dodge vs physical, 10s. Panic for melee-range trouble.
// Useless vs caster damage so gated on a nearby melee threat.
bool ShouldEvasion(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (ctx.bot.hp_pct() >= 50) return false;
    if (ctx.bot.enemies_within(5.0f) == 0) return false;
    return ctx.bot.knows_spell(EVASION) && ctx.bot.is_ready(EVASION);
}
void DoEvasion(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(EVASION, ObjectGuid::Empty);
}

// Vanish: drop combat. Last-resort below 20% HP when Evasion isn't up.
// Alternates with Evasion (different CDs) so the bot has TWO panic
// pulls per fight cluster. Doesn't fire in PvP arenas where it's
// tactical rather than emergency.
bool ShouldVanish(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (ctx.bot.hp_pct() >= 20) return false;
    if (ctx.bot.is_ready(EVASION) && ctx.bot.knows_spell(EVASION)) return false;
    return ctx.bot.knows_spell(VANISH) && ctx.bot.is_ready(VANISH);
}
void DoVanish(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(VANISH, ObjectGuid::Empty);
}

// Cloak of Shadows: ~5s magic immunity, dispels existing harmful magic
// auras. Fires on low HP / silence (proxy for magic-debuff pressure).
bool ShouldCloakOfShadows(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (ctx.bot.hp_pct() < 40 || ctx.bot.is_silenced())
        return ctx.bot.knows_spell(CLOAK_OF_SHADOWS) && ctx.bot.is_ready(CLOAK_OF_SHADOWS);
    return false;
}
void DoCloakOfShadows(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(CLOAK_OF_SHADOWS, ObjectGuid::Empty);
}

// Stealth (L3): OOC opener prep. Aura ID matches spell ID. Don't restealth
// while mounted (the cast would auto-dismount and bots have no reason to
// blow a mount during travel). The two improved-stealth talent auras
// (115191 Subterfuge, 115192 post-break) only exist for talented specs
// and don't apply to baseline; checking STEALTH alone is sufficient at
// spec=0 since the bot can't pick those talents pre-L10.
bool ShouldStealth(ApPredicateContext const& ctx)
{
    if (ctx.bot.in_combat()) return false;
    if (ctx.bot.is_mounted()) return false;
    if (!ctx.bot.knows_spell(STEALTH)) return false;
    if (!ctx.bot.is_ready(STEALTH)) return false;
    return ctx.bot.find_aura(STEALTH, ObjectGuid::Empty) == nullptr;
}
void DoStealth(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(STEALTH, ObjectGuid::Empty);
}

// Cheap Shot (L3): stealth-only 4s stun, 2 CP. Use when stealthed and
// in melee range — Ambush is the ranged-opener counterpart but Cheap
// Shot's stun is more valuable at melee distance because it locks the
// mob down while the bot stacks CPs.
bool ShouldCheapShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CHEAP_SHOT) || !ctx.bot.is_ready(CHEAP_SHOT)) return false;
    if (ctx.bot.find_aura(STEALTH, ObjectGuid::Empty) == nullptr) return false;
    return ctx.bot.enemies_within(5.0f) >= 1;
}
void DoCheapShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CHEAP_SHOT, ctx.bot.victim());
}

// Ambush (L7): stealth-only opener, 2 CP, big up-front damage. Fires
// when stealthed but not in Cheap Shot's melee window — i.e. as the
// ranged-opener form. Falling through to Ambush from any stealth state
// is fine since both spells break stealth and the rule ordering puts
// Cheap Shot first when adjacent.
bool ShouldAmbush(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(AMBUSH) || !ctx.bot.is_ready(AMBUSH)) return false;
    return ctx.bot.find_aura(STEALTH, ObjectGuid::Empty) != nullptr;
}
void DoAmbush(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(AMBUSH, ctx.bot.victim());
}

// Sinister Strike (L1): the only baseline builder. Spec overrides
// (193315 Outlaw / 1329 Mutilate / 53 Backstab) replace it on the action
// bar but the base id stays in the spellbook and castable.
bool ShouldSinisterStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ComboPoints(ctx) >= 5) return false;
    return ctx.bot.knows_spell(SINISTER_STRIKE) && ctx.bot.is_ready(SINISTER_STRIKE);
}
void DoSinisterStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SINISTER_STRIKE, ctx.bot.victim());
}

// NOTE on omitted baseline spells (WoW 12.1.0.69587):
//   1776 Gouge - no longer a class baseline spell (class talent in 12.1);
//     removed from the baseline ladder.
//   2983 Sprint (L5) - pure movement CD; idle-rule territory, not a
//     combat rotation rule.
//   1966 Feint (L12) / 3408 Crippling Poison / 315584 Instant Poison (L10)
//     - above the L<10 window this rotation targets; spec rotations own them.
//   6770 Sap (L11) / 1725 Distract - hidden OOC utility, not a combat rule.

ApRule const baseline_rogue_kRules[] = {
    { ShouldCloakOfShadows, DoCloakOfShadows,"Cloak of Shadows (magic emergency <=40%)"},
    { ShouldVanish,         DoVanish,        "Vanish (<20% last resort)"      },
    { ShouldEvasion,        DoEvasion,       "Evasion (<50% panic dodge)"     },
    { ShouldCrimsonVial,    DoCrimsonVial,   "Crimson Vial (<=60% self HoT)"  },
    { ShouldKick,           DoKick,          "Kick (interrupt)"               },
    { ShouldStealth,        DoStealth,       "Stealth (OOC opener prep)"      },
    { ShouldCheapShot,      DoCheapShot,     "Cheap Shot (stealth melee 4s stun)"},
    { ShouldAmbush,         DoAmbush,        "Ambush (stealth opener)"        },
    { ShouldKidneyShot,     DoKidneyShot,    "Kidney Shot (CP stun)"          },
    { ShouldSliceAndDice,   DoSliceAndDice,  "Slice and Dice (2+ CP)"         },
    { ShouldEviscerate,     DoEviscerate,    "Eviscerate (3+ CP finisher)"    },
    { ShouldSinisterStrike, DoSinisterStrike,"Sinister Strike (builder)"      },
    { AlwaysInCombat,       DoAutoAttack,    "Auto attack"                    },
};

} // anonymous

void RegisterApl_Baseline_Rogue()
{
    RegisterRotation(CLASS_ROGUE, 0, ApRotation{baseline_rogue_kRules});
}

} // namespace Playerbot::Combat
