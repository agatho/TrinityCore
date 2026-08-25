// Retribution Paladin - WoW 12.0 enterprise rotation. Two-handed melee
// with Holy Power as the spending resource. Generators: Crusader Strike,
// Judgment, Blade of Justice, Wake of Ashes (5 HP burst), Hammer of Wrath
// (free during Avenging Wrath / execute). Spenders: Templar's Verdict
// (single-target, 3 HP), Divine Storm (AoE, 3 HP), Final Reckoning (talent
// burst).
//
// Layered survival: Lay on Hands (full heal panic) -> Divine Shield
// (immunity bail) -> Shield of Vengeance (passive absorb) -> Divine
// Protection (Ret variant 403876, 20% DR self CD) -> Word of Glory (3 HP
// heal). Group utility: Blessing of Sacrifice (split damage), Blessing of
// Freedom, Blessing of Protection, Devotion Aura, Hand of Reckoning
// (taunt-on-demand). CC: Rebuke interrupt, Hammer of Justice stun,
// Repentance sap, Blinding Light AoE blind. Major CDs: Avenging Wrath,
// Crusade (talent — note 231895 is its own ID), Execution Sentence, Final
// Reckoning.
//
// =================================================================
// Validated IDs (cross-checked vs SpellName.csv + SpellLevels.csv,
// Wago dump, WoW 12.0 client)
// =================================================================
//    20271 — Judgment                            (baseline)
//   275773 — Judgment                            (Holy spec variant — kept
//                                                  in fallback list for
//                                                  unspecced edge cases)
//   315867 — Judgment                            (Ret/modern unified spec
//                                                  variant; primary)
//    35395 — Crusader Strike                     (baseline)
//   342348 — Crusader Strike                     (Ret/Prot spec variant)
//   184575 — Blade of Justice
//    24275 — Hammer of Wrath
//   255937 — Wake of Ashes
//    85256 — Templar's Verdict
//    53385 — Divine Storm
//   343527 — Execution Sentence
//   343721 — Final Reckoning                       (talent burst)
//   404834 — Consecrated Blade                      (talent — CS proc:
//                                                     consecrates the
//                                                     ground briefly)
//   184662 — Shield of Vengeance
//    31884 — Avenging Wrath                          (classic)
//   384376 — Avenging Wrath                          (modern Ret spec
//                                                     variant)
//   231895 — Avenging Wrath (Crusade)                (Crusade talent, also
//                                                     named "Avenging
//                                                     Wrath" in the DB2)
//    96231 — Rebuke
//      853 — Hammer of Justice
//    20066 — Repentance
//   115750 — Blinding Light
//      633 — Lay on Hands
//      642 — Divine Shield
//      498 — Divine Protection                        (baseline)
//   403876 — Divine Protection                        (Ret spec variant —
//                                                     20% magic DR)
//    85673 — Word of Glory
//    25771 — Forbearance
//     6940 — Blessing of Sacrifice
//     1022 — Blessing of Protection
//     1044 — Blessing of Freedom
//    62124 — Hand of Reckoning
//      465 — Devotion Aura
//   183435 — Retribution Aura                         (optional aura)
//   203538 — Blessing of Kings
//   152262 — Seraphim                                  (talent — temp stat)
//
// =================================================================
// Skipped spells (and why)
// =================================================================
//   376996 — Seasoned Warhorse: mount, not a combat rotation spell.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

#include <initializer_list>

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated) ----
constexpr uint32 JUDGMENT_RET         = 315867;     // Ret / unified spec variant (primary)
constexpr uint32 JUDGMENT_HOLY        = 275773;     // Holy spec variant (fallback)
constexpr uint32 JUDGMENT_BASELINE    = 20271;      // baseline
constexpr uint32 CRUSADER_STRIKE_RET  = 342348;     // Ret/Prot spec variant
constexpr uint32 CRUSADER_STRIKE      = 35395;      // baseline
constexpr uint32 BLADE_OF_JUSTICE     = 184575;
constexpr uint32 HAMMER_OF_WRATH      = 24275;
constexpr uint32 WAKE_OF_ASHES        = 255937;
constexpr uint32 TEMPLARS_VERDICT     = 85256;
constexpr uint32 DIVINE_STORM         = 53385;
constexpr uint32 EXECUTION_SENTENCE   = 343527;
constexpr uint32 FINAL_RECKONING      = 343721;     // talent burst
constexpr uint32 CONSECRATED_BLADE    = 404834;     // talent — CS empowerment
constexpr uint32 SHIELD_OF_VENGEANCE  = 184662;
constexpr uint32 AVENGING_WRATH       = 31884;      // classic
constexpr uint32 AVENGING_WRATH_RET   = 384376;     // modern Ret variant
constexpr uint32 CRUSADE              = 231895;     // talent — replaces AW
constexpr uint32 REBUKE               = 96231;
constexpr uint32 HAMMER_OF_JUSTICE    = 853;        // 6sec stun
constexpr uint32 REPENTANCE           = 20066;     // talent — sleep
constexpr uint32 BLINDING_LIGHT       = 115750;    // talent — 5yd disorient
constexpr uint32 LAY_ON_HANDS         = 633;
constexpr uint32 DIVINE_SHIELD        = 642;
constexpr uint32 DIVINE_PROTECTION    = 498;        // baseline
constexpr uint32 DIVINE_PROTECTION_RET= 403876;    // Ret spec variant — magic DR
constexpr uint32 WORD_OF_GLORY        = 85673;
constexpr uint32 FORBEARANCE          = 25771;     // shared CD with DS / Hand of Protection
constexpr uint32 BLESSING_SACRIFICE   = 6940;
constexpr uint32 BLESSING_PROTECTION  = 1022;
constexpr uint32 BLESSING_FREEDOM     = 1044;
constexpr uint32 HAND_OF_RECKONING    = 62124;
constexpr uint32 DEVOTION_AURA        = 465;
constexpr uint32 RETRIBUTION_AURA     = 183435;    // optional aura
constexpr uint32 BLESSING_KINGS       = 203538;    // major buff
constexpr uint32 SERAPHIM             = 152262;    // talent — temp stat buff

// Holy Power lives in POWER_HOLY_POWER index in the WoW 12.0 array.
constexpr uint8 POWER_HOLY_POWER_IDX = 9;

// ---- Multi-ID helpers ----
uint32 PickKnownAndReady(ApPredicateContext const& ctx, std::initializer_list<uint32> ids)
{
    for (uint32 id : ids)
        if (ctx.bot.knows_spell(id) && ctx.bot.is_ready(id)) return id;
    return 0;
}

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

// ---- Aura maintenance ----
bool ShouldDevotionAura(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(DEVOTION_AURA)) return false;
    return !ctx.bot.has_aura(DEVOTION_AURA);
}
void DoDevotionAura(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DEVOTION_AURA); }

bool ShouldKings(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BLESSING_KINGS)) return false;
    return !ctx.bot.has_aura(BLESSING_KINGS);
}
void DoKings(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BLESSING_KINGS, ctx.bot.raw().guid);
}

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

bool ShouldWordOfGlorySelf(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(WORD_OF_GLORY)) return false;
    if (HolyPower(ctx) < 3) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoWordOfGlorySelf(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(WORD_OF_GLORY, ctx.bot.raw().guid);
}

bool ShouldShieldOfVengeance(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SHIELD_OF_VENGEANCE)) return false;
    if (!ctx.bot.is_ready(SHIELD_OF_VENGEANCE)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.attackers_count() >= 1;
}
void DoShieldOfVengeance(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SHIELD_OF_VENGEANCE); }

// Divine Protection — 20% magic DR; Ret spec variant 403876 (magic-only)
// preferred over the baseline 498 (all-school) when learned.
bool ShouldDivineProtection(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (PickKnownAndReady(ctx, { DIVINE_PROTECTION_RET, DIVINE_PROTECTION }) == 0) return false;
    if (ctx.bot.has_aura(DIVINE_PROTECTION_RET) || ctx.bot.has_aura(DIVINE_PROTECTION)) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoDivineProtection(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    uint32 id = PickKnownAndReady(ctx, { DIVINE_PROTECTION_RET, DIVINE_PROTECTION });
    if (id) e.cast(id);
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
    // Use as a peel — pull an add off a low-HP healer.
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
bool ShouldAvengingWrath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(CRUSADE)) return false;       // Crusade replaces AW
    if (PickKnownAndReady(ctx, { AVENGING_WRATH_RET, AVENGING_WRATH }) == 0) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(10.0f) >= 3;
}
void DoAvengingWrath(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    uint32 id = PickKnownAndReady(ctx, { AVENGING_WRATH_RET, AVENGING_WRATH });
    if (id) e.cast(id);
}

bool ShouldCrusade(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CRUSADE)) return false;
    if (!ctx.bot.is_ready(CRUSADE)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(10.0f) >= 3;
}
void DoCrusade(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CRUSADE); }

bool ShouldSeraphim(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SERAPHIM)) return false;
    if (!ctx.bot.is_ready(SERAPHIM)) return false;
    return HolyPower(ctx) >= 3;
}
void DoSeraphim(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SERAPHIM); }

bool ShouldExecutionSentence(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(EXECUTION_SENTENCE)) return false;
    if (!ctx.bot.is_ready(EXECUTION_SENTENCE)) return false;
    return HolyPower(ctx) >= 3;
}
void DoExecutionSentence(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(EXECUTION_SENTENCE, ctx.bot.victim());
}

bool ShouldFinalReckoning(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FINAL_RECKONING)) return false;
    if (!ctx.bot.is_ready(FINAL_RECKONING)) return false;
    return HolyPower(ctx) >= 3;
}
void DoFinalReckoning(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(FINAL_RECKONING, v->x, v->y, v->z);
    else
        e.cast(FINAL_RECKONING, ctx.bot.victim());
}

// ---- Generators / spenders ----
bool ShouldHammerOfWrath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HAMMER_OF_WRATH)) return false;
    if (!ctx.bot.is_ready(HAMMER_OF_WRATH)) return false;
    return TargetExecuteRange(ctx)
        || ctx.bot.has_aura(AVENGING_WRATH)
        || ctx.bot.has_aura(AVENGING_WRATH_RET)
        || ctx.bot.has_aura(CRUSADE);
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
    return HolyPower(ctx) <= 2;       // generates 5 HP — don't waste
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

bool ShouldTemplarsVerdict(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(TEMPLARS_VERDICT)) return false;
    if (!ctx.bot.is_ready(TEMPLARS_VERDICT)) return false;
    return HolyPower(ctx) >= 3;
}
void DoTemplarsVerdict(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(TEMPLARS_VERDICT, ctx.bot.victim());
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

// Judgment: Ret/unified variant 315867 -> Holy 275773 -> baseline 20271.
bool ShouldJudgment(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    return PickKnownAndReady(ctx, { JUDGMENT_RET, JUDGMENT_HOLY, JUDGMENT_BASELINE }) != 0;
}
void DoJudgment(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    uint32 id = PickKnownAndReady(ctx, { JUDGMENT_RET, JUDGMENT_HOLY, JUDGMENT_BASELINE });
    if (id) e.cast(id, ctx.bot.victim());
}

// Crusader Strike: Ret variant 342348 -> baseline 35395.
bool ShouldCrusaderStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    return PickKnownAndReady(ctx, { CRUSADER_STRIKE_RET, CRUSADER_STRIKE }) != 0;
}
void DoCrusaderStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    uint32 id = PickKnownAndReady(ctx, { CRUSADER_STRIKE_RET, CRUSADER_STRIKE });
    if (id) e.cast(id, ctx.bot.victim());
}

// Consecrated Blade (talent 404834): fires passively from Crusader
// Strike as a proc; not a direct cast. No rule needed — the proc auto-
// triggers when CS is used. Listed in the validated-IDs block so future
// maintainers don't think it's missing.

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
    { ShouldKings,                DoKings,                "Blessing of Kings"           },
    { ShouldLayOnHands,           DoLayOnHands,           "Lay on Hands (<=15%)"        },
    { ShouldDivineShield,         DoDivineShield,         "Divine Shield (panic)"       },
    { ShouldWordOfGlorySelf,      DoWordOfGlorySelf,      "Word of Glory (self heal)"   },
    { ShouldHandOfReckoning,      DoHandOfReckoning,      "Hand of Reckoning (peel)"    },
    { ShouldRebuke,               DoRebuke,               "Rebuke (interrupt)"          },
    { ShouldHammerOfJustice,      DoHammerOfJustice,      "Hammer of Justice (fb)"      },
    { ShouldBlindingLight,        DoBlindingLight,        "Blinding Light (3+ AoE)"     },
    { ShouldShieldOfVengeance,    DoShieldOfVengeance,    "Shield of Vengeance"         },
    { ShouldDivineProtection,     DoDivineProtection,     "Divine Protection (<=50%)"   },
    { ShouldBlessingOfSacrifice,  DoBlessingOfSacrifice,  "Blessing of Sacrifice (tank)"},
    { ShouldBlessingOfProtection, DoBlessingOfProtection, "Blessing of Protection"      },
    { ShouldJudgment,             DoJudgment,             "Judgment"                    },
    { ShouldAvengingWrath,        DoAvengingWrath,        "Avenging Wrath"              },
    { ShouldCrusade,              DoCrusade,              "Crusade"                     },
    { ShouldSeraphim,             DoSeraphim,             "Seraphim (3 HP)"             },
    { ShouldFinalReckoning,       DoFinalReckoning,       "Final Reckoning"             },
    { ShouldExecutionSentence,    DoExecutionSentence,    "Execution Sentence"          },
    { ShouldHammerOfWrath,        DoHammerOfWrath,        "Hammer of Wrath"             },
    { ShouldWakeOfAshes,          DoWakeOfAshes,          "Wake of Ashes"               },
    { ShouldDivineStorm,          DoDivineStorm,          "Divine Storm (2+ targets)"   },
    { ShouldTemplarsVerdict,      DoTemplarsVerdict,      "Templar's Verdict"           },
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
