// Retribution Paladin - WoW 12.1.0.69587 (Midnight) rotation. Two-handed
// melee with Holy Power as the spending resource. Generators: Judgment,
// Blade of Justice, Wake of Ashes, Divine Toll (5x Judgment), Hammer of
// Wrath (execute / during Avenging Wrath), Crusader Strike (only when
// Crusading Strikes is NOT taken - the [R] build turns CS into the auto
// attack). Spenders: Final Verdict (overrides Templar's Verdict, 3 HP),
// Divine Storm (AoE, 3 HP), Word of Glory / Eternal Flame (heal, 3 HP).
//
// Layered survival: Lay on Hands (full heal panic) -> Divine Shield
// (immunity bail) -> WoG/EF self -> Divine Protection (Ret spec 403876;
// with Shield of Vengeance [R] it also fires the absorb). Group utility:
// Blessing of Sacrifice, Blessing of Freedom, Blessing of Protection,
// Devotion Aura, Hand of Reckoning (peel). CC: Rebuke interrupt, Hammer of
// Justice stun, Blinding Light. Major CDs: Avenging Wrath (Crusade is a
// passive haste rider in 12.1), Execution Sentence, Divine Toll.
//
// =================================================================
// Validated spell IDs (WoW 12.1.0.69587, kit Apl_Paladin_Retribution.md)
// =================================================================
//    20271 - Judgment                  (baseline L3; no Ret spec override)
//    35395 - Crusader Strike           (baseline L1; replaced by Crusading Strikes [R])
//   404542 - Crusading Strikes         (passive [R] - knows_spell gate only)
//   184575 - Blade of Justice          (spec talent [R])
//    24275 - Hammer of Wrath           (class baseline)
//   255937 - Wake of Ashes             (spec talent [R])
//    85256 - Templar's Verdict         (spec spell L10; overridden by 383328)
//   383328 - Final Verdict             (spec talent [R]; replaces TV)
//    53385 - Divine Storm              (spec talent [R])
//   343527 - Execution Sentence        (spec talent [R]; no HP cost in 12.1)
//   375576 - Divine Toll               (class talent [R])
//    31884 - Avenging Wrath            (spec talent [R]; cast + buff aura)
//    96231 - Rebuke                    (class talent [R])
//      853 - Hammer of Justice         (baseline L5)
//   115750 - Blinding Light            (class talent [M])
//      633 - Lay on Hands              (class talent [R])
//      642 - Divine Shield             (baseline L10)
//   403876 - Divine Protection         (Ret spec spell L10)
//    85673 - Word of Glory             (baseline L7)
//   156322 - Eternal Flame             (Herald of the Sun [R] - overrides WoG)
//    25771 - Forbearance               (debuff only - never cast)
//     6940 - Blessing of Sacrifice     (class talent [R])
//     1022 - Blessing of Protection    (class talent [R])
//     1044 - Blessing of Freedom       (class talent [R])
//    62124 - Hand of Reckoning         (baseline L9)
//      465 - Devotion Aura             (Auras of the Resolute [R])
//
// =================================================================
// Skipped spells (and why)
// =================================================================
//   184662 - Shield of Vengeance: no longer a cast; 1261562 is a passive
//             [R] that makes Divine Protection fire the absorb.
//   343721 - Final Reckoning: only the Execution Sentence damage sub-spell
//             remains under this id in 12.1; not castable.
//   231895 - Crusade: the 12.1 Crusade is passive 1253598 (haste during
//             Avenging Wrath); Avenging Wrath 31884 is the cast.
//   384376 - Avenging Wrath rank passive / 342348 Crusader Strike rank
//             passive / 315867 Judgment passive: not castable, would hijack
//             a multi-id pick and stall the rule.
//   275773 - Judgment (Holy spec spell): not learnable by Ret.
//    20066 - Repentance / 203538 Blessing of Kings / 152262 Seraphim:
//             gone from the game or not available to Ret in 12.1.
//   404834 - Consecrated Blade: passive proc, not cast.
//      498 - Divine Protection (Holy): Ret casts the spec spell 403876.
//   213644 - Cleanse Toxins [M]: M+-only utility, not wired.
//   183435 - Retribution Aura: Devotion Aura is the maintained aura.
//   190784 - Divine Steed / 433568 Rites: movement / pre-pull imbue.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated against the kit) ----
constexpr uint32 JUDGMENT             = 20271;      // baseline; Ret has no spec override
constexpr uint32 CRUSADER_STRIKE      = 35395;      // baseline; replaced by Crusading Strikes [R]
constexpr uint32 CRUSADING_STRIKES    = 404542;     // passive [R] - CS becomes the auto attack
constexpr uint32 BLADE_OF_JUSTICE     = 184575;
constexpr uint32 HAMMER_OF_WRATH      = 24275;
constexpr uint32 WAKE_OF_ASHES        = 255937;
constexpr uint32 TEMPLARS_VERDICT     = 85256;
constexpr uint32 FINAL_VERDICT        = 383328;     // talent [R] - overrides Templar's Verdict
constexpr uint32 DIVINE_STORM         = 53385;
constexpr uint32 EXECUTION_SENTENCE   = 343527;
constexpr uint32 DIVINE_TOLL          = 375576;     // class talent [R] - 5x Judgment
constexpr uint32 AVENGING_WRATH       = 31884;      // cast + buff aura
constexpr uint32 REBUKE               = 96231;
constexpr uint32 HAMMER_OF_JUSTICE    = 853;        // 6sec stun
constexpr uint32 BLINDING_LIGHT       = 115750;    // talent [M] - AoE disorient
constexpr uint32 LAY_ON_HANDS         = 633;
constexpr uint32 DIVINE_SHIELD        = 642;
constexpr uint32 DIVINE_PROTECTION    = 403876;    // Ret spec spell (+Shield of Vengeance [R])
constexpr uint32 WORD_OF_GLORY        = 85673;
constexpr uint32 ETERNAL_FLAME        = 156322;    // Herald of the Sun [R] - overrides WoG
constexpr uint32 FORBEARANCE          = 25771;     // debuff only
constexpr uint32 BLESSING_SACRIFICE   = 6940;
constexpr uint32 BLESSING_PROTECTION  = 1022;
constexpr uint32 BLESSING_FREEDOM     = 1044;
constexpr uint32 HAND_OF_RECKONING    = 62124;
constexpr uint32 DEVOTION_AURA        = 465;

// Holy Power lives in POWER_HOLY_POWER index of the power array.
constexpr uint8 POWER_HOLY_POWER_IDX = 9;

// Mechanic ids (SharedDefines.h) for the Freedom self-cast.
constexpr uint32 MECHANIC_ROOT_ID  = 7;
constexpr uint32 MECHANIC_SNARE_ID = 11;

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

bool TargetExecuteRange(ApPredicateContext const& ctx)
{
    NearbyUnit const* t = ctx.bot.victim_info();
    if (!t || t->max_hp <= 0 || t->hp <= 0) return false;
    return (t->hp * 100) / t->max_hp <= 20;
}

uint8 HolyPower(ApPredicateContext const& ctx)
{
    return static_cast<uint8>(ctx.bot.power(POWER_HOLY_POWER_IDX));
}

// Talent overrides: cast whichever id the bot actually owns.
uint32 WogId(ApPredicateContext const& ctx)
{
    return ctx.bot.knows_spell(ETERNAL_FLAME) ? ETERNAL_FLAME : WORD_OF_GLORY;
}
uint32 VerdictId(ApPredicateContext const& ctx)
{
    return ctx.bot.knows_spell(FINAL_VERDICT) ? FINAL_VERDICT : TEMPLARS_VERDICT;
}

// ---- Aura maintenance ----
bool ShouldDevotionAura(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(DEVOTION_AURA)) return false;
    return !ctx.bot.has_aura(DEVOTION_AURA);
}
void DoDevotionAura(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DEVOTION_AURA); }

// ---- Survival ----
bool ShouldLayOnHands(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(LAY_ON_HANDS)) return false;
    if (!ctx.bot.is_ready(LAY_ON_HANDS)) return false;
    if (ctx.bot.has_aura(FORBEARANCE)) return false;
    return ctx.bot.hp_pct() <= 15;
}
void DoLayOnHands(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LAY_ON_HANDS, ctx.bot.raw().guid);
}

bool ShouldDivineShield(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DIVINE_SHIELD)) return false;
    if (!ctx.bot.is_ready(DIVINE_SHIELD)) return false;
    if (ctx.bot.has_aura(FORBEARANCE)) return false;
    // Bail at sub-15% if Lay on Hands is on cooldown.
    return ctx.bot.hp_pct() <= 15 && !ctx.bot.is_ready(LAY_ON_HANDS);
}
void DoDivineShield(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DIVINE_SHIELD); }

// Word of Glory / Eternal Flame self heal (3 HP).
bool ShouldWordOfGlorySelf(ApPredicateContext const& ctx)
{
    const uint32 id = WogId(ctx);
    if (!ctx.bot.knows_spell(id)) return false;
    if (!ctx.bot.is_ready(id)) return false;
    if (HolyPower(ctx) < 3) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoWordOfGlorySelf(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(WogId(ctx), ctx.bot.raw().guid);
}

// Divine Protection 403876 - all-school DR, usable while stunned; with
// the Shield of Vengeance passive [R] it also pops the absorb, so it is
// the Ret bot's main mid-fight defensive.
bool ShouldDivineProtection(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DIVINE_PROTECTION)) return false;
    if (!ctx.bot.is_ready(DIVINE_PROTECTION)) return false;
    if (ctx.bot.has_aura(DIVINE_PROTECTION)) return false;
    return ctx.bot.hp_pct() <= 60;
}
void DoDivineProtection(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DIVINE_PROTECTION); }

// Blessing of Freedom on self when rooted / snared - melee that cannot
// reach the target does no damage.
bool ShouldBlessingOfFreedom(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(BLESSING_FREEDOM)) return false;
    if (!ctx.bot.is_ready(BLESSING_FREEDOM)) return false;
    if (ctx.bot.has_aura(BLESSING_FREEDOM)) return false;
    return ctx.bot.has_mechanic(MECHANIC_ROOT_ID) || ctx.bot.has_mechanic(MECHANIC_SNARE_ID);
}
void DoBlessingOfFreedom(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BLESSING_FREEDOM, ctx.bot.raw().guid);
}

// ---- Group utility ----
bool ShouldBlessingOfSacrifice(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BLESSING_SACRIFICE)) return false;
    if (!ctx.bot.is_ready(BLESSING_SACRIFICE)) return false;
    if (ctx.bot.hp_pct() <= 50) return false;        // don't trade hp when ours is low
    if (auto const* tank = ctx.group.tank())
        return tank->online && tank->hp > 0 && (tank->hp * 100) / tank->max_hp <= 35;
    return false;
}
void DoBlessingOfSacrifice(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* tank = ctx.group.tank())
        e.cast(BLESSING_SACRIFICE, tank->guid);
}

bool ShouldBlessingOfProtection(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BLESSING_PROTECTION)) return false;
    if (!ctx.bot.is_ready(BLESSING_PROTECTION)) return false;
    // Pop on a non-tank ally taking heavy melee damage and at <=25%.
    if (auto const* low = ctx.group.lowest_hp_on_map(ctx.bot.map_id(), Role::Unknown, ctx.bot.raw().position.x, ctx.bot.raw().position.y, ctx.bot.raw().position.z, 45.0f)) {
        if (low->online && low->hp > 0 && low->role != Role::Tank) {
            int32 pct = (low->hp * 100) / low->max_hp;
            if (pct <= 25 && !ctx.bot.has_aura(FORBEARANCE, low->guid))
                return true;
        }
    }
    return false;
}
void DoBlessingOfProtection(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* low = ctx.group.lowest_hp_on_map(ctx.bot.map_id(), Role::Unknown, ctx.bot.raw().position.x, ctx.bot.raw().position.y, ctx.bot.raw().position.z, 45.0f))
        e.cast(BLESSING_PROTECTION, low->guid);
}

bool ShouldHandOfReckoning(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HAND_OF_RECKONING)) return false;
    if (!ctx.bot.is_ready(HAND_OF_RECKONING)) return false;
    // Use as a peel - pull an add off a low-HP healer.
    auto const* m = ctx.group.lowest_hp_on_map(ctx.bot.map_id(), Role::Healer, ctx.bot.raw().position.x, ctx.bot.raw().position.y, ctx.bot.raw().position.z, 45.0f);
    if (!m || !m->online || m->hp <= 0) return false;
    if ((m->hp * 100) / m->max_hp > 50) return false;
    return ctx.bot.untaunted_enemy(40.f) != nullptr;
}
void DoHandOfReckoning(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.untaunted_enemy(40.f))
        e.cast(HAND_OF_RECKONING, t->guid);
}

// ---- Interrupt / CC ----
bool ShouldRebuke(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(REBUKE)) return false;
    if (!ctx.bot.is_ready(REBUKE)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    return ctx.bot.kick_target(pvp, 5.0f) != nullptr;
}
void DoRebuke(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 5.0f))
        e.cast(REBUKE, c->guid);
}

// HoJ as PvP kick-with-stun: prefer enemy healer (friendly-target casts the
// regular interrupt selector misses), then any interruptible enemy caster.
NearbyUnit const* PickHammerOfJusticeTarget(ApPredicateContext const& ctx)
{
    if (ctx.pvp.in_battleground || ctx.pvp.in_arena)
        if (auto const* h = ctx.bot.enemy_healer_to_interrupt(10.0f))
            return h;
    return ctx.bot.interruptible_caster();
}

bool ShouldHammerOfJustice(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HAMMER_OF_JUSTICE)) return false;
    if (!ctx.bot.is_ready(HAMMER_OF_JUSTICE)) return false;
    if (ctx.bot.is_ready(REBUKE)) return false;
    return PickHammerOfJusticeTarget(ctx) != nullptr;
}
void DoHammerOfJustice(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* c = PickHammerOfJusticeTarget(ctx))
        e.cast(HAMMER_OF_JUSTICE, c->guid);
}

bool ShouldBlindingLight(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BLINDING_LIGHT)) return false;
    if (!ctx.bot.is_ready(BLINDING_LIGHT)) return false;
    return ctx.bot.enemies_within(10.0f) >= 3 && ctx.bot.hp_pct() <= 50;
}
void DoBlindingLight(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLINDING_LIGHT); }

// ---- Major offensive cooldowns ----
// Avenging Wrath 31884 is the cast AND the buff aura (384376 is only the
// rank passive; Crusade 1253598 is a passive haste rider in 12.1).
bool ShouldAvengingWrath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(AVENGING_WRATH)) return false;
    if (!ctx.bot.is_ready(AVENGING_WRATH)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(10.0f) >= 3;
}
void DoAvengingWrath(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(AVENGING_WRATH); }

// Execution Sentence: 60s cd, no Holy Power cost in 12.1 - fire on
// cooldown against the current target.
bool ShouldExecutionSentence(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(EXECUTION_SENTENCE)) return false;
    return ctx.bot.is_ready(EXECUTION_SENTENCE);
}
void DoExecutionSentence(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(EXECUTION_SENTENCE, ctx.bot.victim());
}

// Divine Toll - Judgment on up to 5 enemies, each generating Holy Power.
// Use it as a burst generator when low on HP.
bool ShouldDivineToll(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DIVINE_TOLL)) return false;
    if (!ctx.bot.is_ready(DIVINE_TOLL)) return false;
    return HolyPower(ctx) <= 2;
}
void DoDivineToll(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DIVINE_TOLL, ctx.bot.victim());
}

// ---- Generators / spenders ----
bool ShouldHammerOfWrath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HAMMER_OF_WRATH)) return false;
    if (!ctx.bot.is_ready(HAMMER_OF_WRATH)) return false;
    return TargetExecuteRange(ctx)
        || ctx.bot.has_aura(AVENGING_WRATH);
}
void DoHammerOfWrath(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HAMMER_OF_WRATH, ctx.bot.victim());
}

bool ShouldWakeOfAshes(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(WAKE_OF_ASHES)) return false;
    if (!ctx.bot.is_ready(WAKE_OF_ASHES)) return false;
    return HolyPower(ctx) <= 2;       // burst generator - don't overcap
}
void DoWakeOfAshes(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(WAKE_OF_ASHES, ctx.bot.victim());
}

bool ShouldDivineStorm(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DIVINE_STORM)) return false;
    if (!ctx.bot.is_ready(DIVINE_STORM)) return false;
    if (HolyPower(ctx) < 3) return false;
    return ctx.aoe_preference ||
           ctx.bot.attackers_count() >= 2 || ctx.bot.enemies_within(8.0f) >= 2;
}
void DoDivineStorm(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DIVINE_STORM); }

// Templar's Verdict / Final Verdict (3 HP single-target spender).
bool ShouldTemplarsVerdict(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    const uint32 id = VerdictId(ctx);
    if (!ctx.bot.knows_spell(id)) return false;
    if (!ctx.bot.is_ready(id)) return false;
    return HolyPower(ctx) >= 3;
}
void DoTemplarsVerdict(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(VerdictId(ctx), ctx.bot.victim());
}

bool ShouldBladeOfJustice(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BLADE_OF_JUSTICE)) return false;
    return ctx.bot.is_ready(BLADE_OF_JUSTICE);
}
void DoBladeOfJustice(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BLADE_OF_JUSTICE, ctx.bot.victim());
}

// Judgment 20271 - Ret has no spec override in 12.1.
bool ShouldJudgment(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(JUDGMENT)) return false;
    return ctx.bot.is_ready(JUDGMENT);
}
void DoJudgment(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(JUDGMENT, ctx.bot.victim());
}

// Crusader Strike 35395 - only for bots WITHOUT Crusading Strikes; the
// [R] build converts CS into the auto attack and the button disappears.
bool ShouldCrusaderStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(CRUSADING_STRIKES)) return false;
    if (!ctx.bot.knows_spell(CRUSADER_STRIKE)) return false;
    return ctx.bot.is_ready(CRUSADER_STRIKE);
}
void DoCrusaderStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CRUSADER_STRIKE, ctx.bot.victim());
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

// Rule order: cross-spec ladder first (LoH -> DS -> WoG self ->
// HandOfReckoning -> HoJ -> Judgment) then Ret-specific spenders /
// generators / CDs.
ApRule const kRules[] = {
    { ShouldDevotionAura,         DoDevotionAura,         "Devotion Aura"               },
    { ShouldLayOnHands,           DoLayOnHands,           "Lay on Hands (<=15%)"        },
    { ShouldDivineShield,         DoDivineShield,         "Divine Shield (panic)"       },
    { ShouldWordOfGlorySelf,      DoWordOfGlorySelf,      "Word of Glory (self heal)"   },
    { ShouldHandOfReckoning,      DoHandOfReckoning,      "Hand of Reckoning (peel)"    },
    { ShouldRebuke,               DoRebuke,               "Rebuke (interrupt)"          },
    { ShouldHammerOfJustice,      DoHammerOfJustice,      "Hammer of Justice (fb)"      },
    { ShouldBlindingLight,        DoBlindingLight,        "Blinding Light (3+ AoE)"     },
    { ShouldBlessingOfFreedom,    DoBlessingOfFreedom,    "Blessing of Freedom (self)"  },
    { ShouldDivineProtection,     DoDivineProtection,     "Divine Protection (<=60%)"   },
    { ShouldBlessingOfSacrifice,  DoBlessingOfSacrifice,  "Blessing of Sacrifice (tank)"},
    { ShouldBlessingOfProtection, DoBlessingOfProtection, "Blessing of Protection"      },
    { ShouldJudgment,             DoJudgment,             "Judgment"                    },
    { ShouldAvengingWrath,        DoAvengingWrath,        "Avenging Wrath"              },
    { ShouldExecutionSentence,    DoExecutionSentence,    "Execution Sentence"          },
    { ShouldHammerOfWrath,        DoHammerOfWrath,        "Hammer of Wrath"             },
    { ShouldWakeOfAshes,          DoWakeOfAshes,          "Wake of Ashes"               },
    { ShouldDivineToll,           DoDivineToll,           "Divine Toll (HP <= 2)"       },
    { ShouldDivineStorm,          DoDivineStorm,          "Divine Storm (2+ targets)"   },
    { ShouldTemplarsVerdict,      DoTemplarsVerdict,      "Templar's / Final Verdict"   },
    { ShouldBladeOfJustice,       DoBladeOfJustice,       "Blade of Justice"            },
    { ShouldCrusaderStrike,       DoCrusaderStrike,       "Crusader Strike"             },
    { AlwaysInCombat,             DoAutoAttack,           "Engage auto attack"          },
};

} // anonymous

void RegisterApl_Paladin_Retribution()
{
    constexpr uint32 SPEC_PALADIN_RETRIBUTION = 70;
    RegisterRotation(CLASS_PALADIN, SPEC_PALADIN_RETRIBUTION, ApRotation{kRules});
}

} // namespace Playerbot::Combat
