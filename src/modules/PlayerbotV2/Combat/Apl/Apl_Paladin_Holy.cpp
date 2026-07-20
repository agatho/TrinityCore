// Holy Paladin - WoW 12.0 enterprise rotation. Holy Power healer with
// Beacon of Light persistent transfer, Holy Shock proc engine, Word of
// Glory burst heal, Light of Dawn cone AoE, and Avenging Wrath / Holy
// Avenger throughput cooldowns. Major raid CDs include Aura Mastery (50%
// magic DR group buff), Divine Toll (cleave heal + HP gen).
//
// =================================================================
// Validated IDs (cross-checked vs SpellName.csv + SpellLevels.csv,
// Wago dump, WoW 12.0 client)
// =================================================================
//   275773 — Judgment           (Holy spec variant; primary)
//   315867 — Judgment           (modern unified spec variant; fallback)
//    20271 — Judgment           (baseline; final fallback)
//    20473 — Holy Shock
//    19750 — Flash of Light
//    82326 — Holy Light
//    85222 — Light of Dawn
//      633 — Lay on Hands
//      642 — Divine Shield
//      498 — Divine Protection            (defensive CD, 20% all-school DR)
//      465 — Devotion Aura
//   203538 — Blessing of Kings
//     6940 — Blessing of Sacrifice
//     1022 — Blessing of Protection
//     1044 — Blessing of Freedom
//    31821 — Aura Mastery
//    31884 — Avenging Wrath                (classic ID)
//   384376 — Avenging Wrath                (modern spec variant; primary)
//   105809 — Holy Avenger                  (talent — 3x HP gen)
//   304971 — Divine Toll                   (3 HP gen + cleave heal)
//   200652 — Tyr's Deliverance              (talent — channel cone heal)
//    53563 — Beacon of Light
//   156910 — Beacon of Faith                (talent — 2nd beacon)
//   200025 — Beacon of Virtue               (talent — temporary 4-target beacon)
//     4987 — Cleanse                         (magic + poison + disease)
//   440013 — Cleanse Toxins                  (talent — poison + disease only,
//                                              instant + no GCD when learned)
//   212056 — Absolution                       (combat rez)
//    85673 — Word of Glory
//    96231 — Rebuke                            (interrupt)
//      853 — Hammer of Justice                  (60s stun)
//   115750 — Blinding Light                     (talent AoE disorient)
//    20066 — Repentance                          (talent — long CC, OOC)
//    25771 — Forbearance                          (shared CD debuff)
//    35395 — Crusader Strike
//    24275 — Hammer of Wrath
//    53651 — Light's Beacon                       (PASSIVE — linked to Beacon
//                                                    of Light 53563; no cast)
//   231642 — Tower of Radiance                    (PASSIVE talent — no cast)
//   415091 — Shield of the Righteous              (Holy talent variant —
//                                                    rarely used by Holy spec)
//
// =================================================================
// Skipped spells (and why)
// =================================================================
//   376996 — Seasoned Warhorse: mount, not a combat rotation spell.
//    53651 — Light's Beacon: passive aura granted with Beacon of Light;
//             never cast directly.
//   231642 — Tower of Radiance: passive talent (Holy Shock generates
//             extra HP on beacon target); never cast directly.
//   415091 — Shield of the Righteous (Holy variant): Holy paladins
//             spend HP on Word of Glory / Light of Dawn for healing
//             throughput; SotR is a Prot active-mitigation tool and is
//             never the right Holy spender. Documented in case a
//             defensive-leaning talent loadout wants it later.
//   212056 — Absolution: combat resurrection. Handled by out-of-combat
//             revive logic, not by the APL combat loop.
//
// =================================================================
// Decision tree (top-down)
// =================================================================
//   0) Aura/buff maintenance: Devotion Aura, Blessing of Kings
//   1) Cast-swap shim:        cancel slow heal if better target appears
//   2) Emergency self:        Lay on Hands -> Divine Shield -> WoG self
//   3) Tank duty:             Hand of Reckoning, HoJ (interrupt fallback)
//   4) Interrupt:             Rebuke -> Hammer of Justice -> Blinding Light
//   5) BoP/BoSac group save:  ally <=25% / tank <=30%
//   6) Beacon maintenance:    Beacon of Light tank, Beacon of Faith 2nd
//   7) Big raid CDs:          Aura Mastery, Avenging Wrath, Holy Avenger,
//                              Divine Toll, Tyr's Deliverance, BoVirtue
//   8) Defensive CD:          Divine Protection (self DR)
//   9) Dispel:                Cleanse Toxins (poison/disease prio) -> Cleanse
//  10) AoE heal:              Light of Dawn (HP3)
//  11) Spike heal:             Holy Shock, Word of Glory, Flash of Light
//  12) Filler heal:           Holy Light
//  13) Offensive filler:       Holy Shock dmg, Judgment, HoW, Crusader Strike

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "ApDispelHelpers.h"
#include "ApHealHelpers.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

#include <initializer_list>

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated) ----
constexpr uint32 HOLY_SHOCK             = 20473;
constexpr uint32 WORD_OF_GLORY          = 85673;
constexpr uint32 FLASH_OF_LIGHT         = 19750;
constexpr uint32 HOLY_LIGHT             = 82326;
constexpr uint32 LAY_ON_HANDS           = 633;
constexpr uint32 BLESSING_OF_PROTECTION = 1022;
constexpr uint32 BLESSING_OF_FREEDOM    = 1044;
constexpr uint32 LIGHT_OF_DAWN          = 85222;
constexpr uint32 AVENGING_WRATH         = 31884;
constexpr uint32 AVENGING_WRATH_MODERN  = 384376;     // spec-variant ID
constexpr uint32 HOLY_AVENGER           = 105809;     // talent — 3x HP gen
constexpr uint32 AURA_MASTERY           = 31821;
constexpr uint32 DIVINE_TOLL            = 304971;     // 3 HP gen + cleave heal
constexpr uint32 TYRS_DELIVERANCE       = 200652;     // talent — channel cone heal
constexpr uint32 BEACON_OF_LIGHT        = 53563;
constexpr uint32 BEACON_OF_FAITH        = 156910;     // talent — 2nd beacon
constexpr uint32 BEACON_OF_VIRTUE       = 200025;     // talent — temporary 4-target beacon
constexpr uint32 CLEANSE                = 4987;
constexpr uint32 CLEANSE_TOXINS         = 440013;     // talent — poison/disease only
constexpr uint32 DIVINE_SHIELD          = 642;
constexpr uint32 DIVINE_PROTECTION      = 498;        // defensive — 20% DR
constexpr uint32 BLESSING_OF_SACRIFICE  = 6940;
constexpr uint32 HAMMER_OF_JUSTICE      = 853;
constexpr uint32 REBUKE                 = 96231;
constexpr uint32 BLINDING_LIGHT         = 115750;
constexpr uint32 REPENTANCE             = 20066;
constexpr uint32 DEVOTION_AURA          = 465;
constexpr uint32 BLESSING_OF_KINGS      = 203538;
constexpr uint32 FORBEARANCE            = 25771;
constexpr uint32 CRUSADER_STRIKE        = 35395;
constexpr uint32 HAND_OF_RECKONING      = 62124;
constexpr uint32 JUDGMENT_HOLY          = 275773;     // Holy spec variant (primary)
constexpr uint32 JUDGMENT_UNIFIED       = 315867;     // modern unified spec variant
constexpr uint32 JUDGMENT_BASELINE      = 20271;
constexpr uint32 HAMMER_OF_WRATH        = 24275;

constexpr uint8 POWER_HOLY_POWER_IDX = 9;

// ---- Multi-ID helpers (try spec variant first, fall back to baseline) ----
// Why this matters: a level-30 Holy paladin has the baseline 20271
// Judgment but not 275773. A level-60 Holy paladin has the spec variant.
// Picking the first known + ready spell is the cleanest way to support
// the entire 1-80 level band without per-level branching.
uint32 PickKnownAndReady(ApPredicateContext const& ctx, std::initializer_list<uint32> ids)
{
    for (uint32 id : ids)
        if (ctx.bot.knows_spell(id) && ctx.bot.is_ready(id)) return id;
    return 0;
}

// ---- Heal target picker ----
struct HealTarget
{
    ObjectGuid guid;
    int32      hp_pct;
    bool       valid;
};

HealTarget PickHealTarget(ApPredicateContext const& ctx)
{
    HealTarget t{};
    t.guid    = ctx.bot.raw().guid;
    t.hp_pct  = ctx.bot.hp_pct();
    t.valid   = true;

    GroupMemberSummary const* low = ctx.group.heal_assignment(ctx.bot.raw().guid, ctx.bot.map_id(), ctx.bot.raw().position.x, ctx.bot.raw().position.y, ctx.bot.raw().position.z, 45.0f);
    if (low && low->online && low->max_hp > 0 && low->hp > 0) {
        const int32 pct = (low->hp * 100) / low->max_hp;
        if (pct < t.hp_pct) {
            t.guid   = low->guid;
            t.hp_pct = pct;
        }
    }
    return t;
}

int WoundedFriendCount(ApPredicateContext const& ctx, int below_pct)
{
    int n = 0;
    auto const* members = ctx.group.members();
    // SOLO (audit B22): ungrouped, "wounded friend" used to collapse to
    // "my own HP <= below_pct" — the 92% GroupTopped gates then froze ALL
    // damage the moment a questing healer took two melee hits, degenerating
    // solo healer-spec leveling into heal-regen-nuke loops (3-10x kill
    // time). Cap the solo threshold at a 45% survival floor: topped-style
    // gates (92) stay open while merely scratched, true emergency heals
    // (<=45) keep their thresholds.
    if (!members) return ctx.bot.hp_pct() <= std::min(below_pct, 45) ? 1 : 0;
    for (auto const& m : *members) {
        if (!m.online || m.max_hp <= 0 || m.hp <= 0) continue;
        if ((m.hp * 100) / m.max_hp <= below_pct) ++n;
    }
    return n;
}

bool GroupTopped(ApPredicateContext const& ctx)
{
    return WoundedFriendCount(ctx, 92) == 0;
}

uint8 HolyPower(ApPredicateContext const& ctx)
{
    return static_cast<uint8>(ctx.bot.power(POWER_HOLY_POWER_IDX));
}

GroupMemberSummary const* DispelTarget(ApPredicateContext const& ctx)
{
    return DispelTargetWithPriority(ctx, [](GroupSnapshotView const& g)
        -> GroupMemberSummary const*
    {
        if (auto const* m = g.dispel_candidate(DispelType::Magic))   return m;
        if (auto const* m = g.dispel_candidate(DispelType::Disease)) return m;
        if (auto const* m = g.dispel_candidate(DispelType::Poison))  return m;
        return nullptr;
    });
}

GroupMemberSummary const* PoisonDiseaseTarget(ApPredicateContext const& ctx)
{
    return DispelTargetWithPriority(ctx, [](GroupSnapshotView const& g)
        -> GroupMemberSummary const*
    {
        if (auto const* m = g.dispel_candidate(DispelType::Disease)) return m;
        if (auto const* m = g.dispel_candidate(DispelType::Poison))  return m;
        return nullptr;
    });
}

bool SelfNeedsDispel(ApPredicateContext const& ctx)
{
    return ctx.bot.self_dispellable(DispelType::Magic)
        || ctx.bot.self_dispellable(DispelType::Disease)
        || ctx.bot.self_dispellable(DispelType::Poison);
}

bool SelfNeedsToxinDispel(ApPredicateContext const& ctx)
{
    return ctx.bot.self_dispellable(DispelType::Disease)
        || ctx.bot.self_dispellable(DispelType::Poison);
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
    if (!ctx.bot.knows_spell(BLESSING_OF_KINGS)) return false;
    return !ctx.bot.has_aura(BLESSING_OF_KINGS);
}
void DoKings(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BLESSING_OF_KINGS, ctx.bot.raw().guid);
}

// ---- Emergency / survival (top of ladder) ----
bool ShouldLayOnHands(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(LAY_ON_HANDS)) return false;
    if (!ctx.bot.is_ready(LAY_ON_HANDS)) return false;
    HealTarget t = PickHealTarget(ctx);
    if (t.guid != ctx.bot.raw().guid && ctx.bot.has_aura(FORBEARANCE, t.guid)) return false;
    if (t.guid == ctx.bot.raw().guid && ctx.bot.has_aura(FORBEARANCE)) return false;
    return t.hp_pct <= 15;
}
void DoLayOnHands(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LAY_ON_HANDS, PickHealTarget(ctx).guid);
}

bool ShouldDivineShield(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DIVINE_SHIELD)) return false;
    if (!ctx.bot.is_ready(DIVINE_SHIELD)) return false;
    if (ctx.bot.has_aura(FORBEARANCE)) return false;
    if (ctx.bot.hp_pct() > 15) return false;
    return !ctx.bot.is_ready(LAY_ON_HANDS);
}
void DoDivineShield(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DIVINE_SHIELD); }

// Self Word of Glory at <=50% HP. The wider heal-target loop picks WoG
// for allies; this rule is the ordered self-preservation step.
bool ShouldWordOfGlorySelf(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(WORD_OF_GLORY)) return false;
    if (!ctx.bot.is_ready(WORD_OF_GLORY)) return false;
    if (HolyPower(ctx) < 3) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoWordOfGlorySelf(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(WORD_OF_GLORY, ctx.bot.raw().guid);
}

// Divine Protection — 20% all-school DR self CD; complements the ladder
// when Forbearance is up (no DS) or DS+LoH are on CD.
bool ShouldDivineProtection(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DIVINE_PROTECTION)) return false;
    if (!ctx.bot.is_ready(DIVINE_PROTECTION)) return false;
    if (ctx.bot.has_aura(DIVINE_PROTECTION)) return false;
    return ctx.bot.hp_pct() <= 45;
}
void DoDivineProtection(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DIVINE_PROTECTION); }

// ---- Tank duty (Holy paladins are sometimes the only ranged taunt
// available in 5-mans when the pull goes sideways) ----
bool ShouldHandOfReckoning(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(HAND_OF_RECKONING)) return false;
    if (!ctx.bot.is_ready(HAND_OF_RECKONING)) return false;
    // Only peel for self — Holy paladins should not be steady-state tanking.
    if (ctx.bot.hp_pct() > 40) return false;
    return ctx.bot.untaunted_enemy(40.0f) != nullptr;
}
void DoHandOfReckoning(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.untaunted_enemy(40.0f))
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

bool ShouldHammerOfJustice(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HAMMER_OF_JUSTICE)) return false;
    if (!ctx.bot.is_ready(HAMMER_OF_JUSTICE)) return false;
    if (ctx.bot.is_ready(REBUKE)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    return ctx.bot.kick_target(pvp, 10.0f) != nullptr;
}
void DoHammerOfJustice(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 10.0f))
        e.cast(HAMMER_OF_JUSTICE, c->guid);
}

bool ShouldBlindingLight(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BLINDING_LIGHT)) return false;
    if (!ctx.bot.is_ready(BLINDING_LIGHT)) return false;
    return ctx.bot.enemies_within(10.0f) >= 3 && ctx.bot.hp_pct() <= 50;
}
void DoBlindingLight(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLINDING_LIGHT); }

// ---- Group saves ----
bool ShouldBlessingOfProtection(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BLESSING_OF_PROTECTION)) return false;
    if (!ctx.bot.is_ready(BLESSING_OF_PROTECTION)) return false;
    HealTarget t = PickHealTarget(ctx);
    if (!t.valid || t.guid == ctx.bot.raw().guid) return false;
    if (ctx.bot.has_aura(FORBEARANCE, t.guid)) return false;
    return t.hp_pct <= 25;
}
void DoBlessingOfProtection(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BLESSING_OF_PROTECTION, PickHealTarget(ctx).guid);
}

bool ShouldBlessingOfSacrifice(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BLESSING_OF_SACRIFICE)) return false;
    if (!ctx.bot.is_ready(BLESSING_OF_SACRIFICE)) return false;
    if (ctx.bot.hp_pct() <= 80) return false;
    GroupMemberSummary const* tank = ctx.group.tank();
    if (!tank || !tank->online || tank->max_hp <= 0 || tank->hp <= 0) return false;
    return (tank->hp * 100) / tank->max_hp <= 30;
}
void DoBlessingOfSacrifice(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* tank = ctx.group.tank())
        e.cast(BLESSING_OF_SACRIFICE, tank->guid);
}

// ---- Beacon maintenance ----
bool ShouldBeaconOfLight(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BEACON_OF_LIGHT)) return false;
    GroupMemberSummary const* tank = ctx.group.tank();
    if (!tank || !tank->online) return false;
    AuraEntry const* a = ctx.bot.find_aura(BEACON_OF_LIGHT, tank->guid);
    return a == nullptr;
}
void DoBeaconOfLight(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* tank = ctx.group.tank())
        e.cast(BEACON_OF_LIGHT, tank->guid);
}

bool ShouldBeaconOfFaith(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BEACON_OF_FAITH)) return false;
    // Place the second beacon on the second-most-vulnerable melee tank/dps.
    auto const* members = ctx.group.members();
    if (!members) return false;
    for (auto const& m : *members) {
        if (!m.online || m.hp <= 0) continue;
        if (m.guid == ctx.bot.raw().guid) continue;
        if (ctx.bot.has_aura(BEACON_OF_LIGHT, m.guid)) continue;
        if (m.role == Role::Tank || m.role == Role::Dps) {
            if (!ctx.bot.has_aura(BEACON_OF_FAITH, m.guid)) return true;
        }
    }
    return false;
}
void DoBeaconOfFaith(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    auto const* members = ctx.group.members();
    if (!members) return;
    for (auto const& m : *members) {
        if (!m.online || m.hp <= 0) continue;
        if (m.guid == ctx.bot.raw().guid) continue;
        if (ctx.bot.has_aura(BEACON_OF_LIGHT, m.guid)) continue;
        if (m.role != Role::Tank && m.role != Role::Dps) continue;
        if (ctx.bot.has_aura(BEACON_OF_FAITH, m.guid)) continue;
        e.cast(BEACON_OF_FAITH, m.guid);
        return;
    }
}

bool ShouldBeaconOfVirtue(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BEACON_OF_VIRTUE)) return false;
    if (!ctx.bot.is_ready(BEACON_OF_VIRTUE)) return false;
    if (HolyPower(ctx) < 3) return false;
    return WoundedFriendCount(ctx, 70) >= 3;
}
void DoBeaconOfVirtue(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BEACON_OF_VIRTUE, PickHealTarget(ctx).guid);
}

// ---- Major CDs ----
bool ShouldAuraMastery(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(AURA_MASTERY)) return false;
    if (!ctx.bot.is_ready(AURA_MASTERY)) return false;
    return WoundedFriendCount(ctx, 50) >= 2;
}
void DoAuraMastery(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(AURA_MASTERY); }

// Avenging Wrath: try the modern 384376 first then the classic 31884.
// Both exist in retail; some talent loadouts surface one vs the other.
bool ShouldAvengingWrath(ApPredicateContext const& ctx)
{
    uint32 id = PickKnownAndReady(ctx, { AVENGING_WRATH_MODERN, AVENGING_WRATH });
    if (id == 0) return false;
    return WoundedFriendCount(ctx, 60) >= 2;
}
void DoAvengingWrath(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    uint32 id = PickKnownAndReady(ctx, { AVENGING_WRATH_MODERN, AVENGING_WRATH });
    if (id) e.cast(id);
}

bool ShouldHolyAvenger(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HOLY_AVENGER)) return false;
    if (!ctx.bot.is_ready(HOLY_AVENGER)) return false;
    return WoundedFriendCount(ctx, 70) >= 2;
}
void DoHolyAvenger(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HOLY_AVENGER); }

bool ShouldDivineToll(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(DIVINE_TOLL)) return false;
    if (!ctx.bot.is_ready(DIVINE_TOLL)) return false;
    return WoundedFriendCount(ctx, 70) >= 2;
}
void DoDivineToll(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DIVINE_TOLL, PickHealTarget(ctx).guid);
}

bool ShouldTyrsDeliverance(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(TYRS_DELIVERANCE)) return false;
    if (!ctx.bot.is_ready(TYRS_DELIVERANCE)) return false;
    return WoundedFriendCount(ctx, 80) >= 3;
}
void DoTyrsDeliverance(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TYRS_DELIVERANCE); }

// ---- Dispel ----
// Cleanse Toxins (talent, instant + no GCD) is preferred for poison/
// disease because it doesn't disrupt the heal cadence. Falls back to
// regular Cleanse which also handles Magic.
bool ShouldCleanseToxins(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(CLEANSE_TOXINS)) return false;
    if (!ctx.bot.is_ready(CLEANSE_TOXINS)) return false;
    return PoisonDiseaseTarget(ctx) != nullptr || SelfNeedsToxinDispel(ctx);
}
void DoCleanseToxins(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = PoisonDiseaseTarget(ctx)) {
        e.cast(CLEANSE_TOXINS, t->guid);
        return;
    }
    if (SelfNeedsToxinDispel(ctx))
        e.cast(CLEANSE_TOXINS, ctx.bot.raw().guid);
}

bool ShouldCleanse(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(CLEANSE)) return false;
    if (!ctx.bot.is_ready(CLEANSE)) return false;
    return DispelTarget(ctx) != nullptr || SelfNeedsDispel(ctx);
}
void DoCleanse(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = DispelTarget(ctx)) {
        e.cast(CLEANSE, t->guid);
        return;
    }
    if (SelfNeedsDispel(ctx))
        e.cast(CLEANSE, ctx.bot.raw().guid);
}

// ---- AoE / cone heal ----
bool ShouldLightOfDawn(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(LIGHT_OF_DAWN)) return false;
    if (!ctx.bot.is_ready(LIGHT_OF_DAWN)) return false;
    if (HolyPower(ctx) < 3) return false;
    return WoundedFriendCount(ctx, 75) >= 2;
}
void DoLightOfDawn(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(LIGHT_OF_DAWN); }

// ---- Spike heals ----
bool ShouldHolyShockHeal(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HOLY_SHOCK)) return false;
    if (!ctx.bot.is_ready(HOLY_SHOCK)) return false;
    return PickHealTarget(ctx).hp_pct < 100;
}
void DoHolyShockHeal(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HOLY_SHOCK, PickHealTarget(ctx).guid);
}

bool ShouldWordOfGlory(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(WORD_OF_GLORY)) return false;
    if (!ctx.bot.is_ready(WORD_OF_GLORY)) return false;
    if (HolyPower(ctx) < 3) return false;
    return PickHealTarget(ctx).hp_pct <= 80;
}
void DoWordOfGlory(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(WORD_OF_GLORY, PickHealTarget(ctx).guid);
}

bool ShouldFlashOfLight(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(FLASH_OF_LIGHT)) return false;
    return PickHealTarget(ctx).hp_pct <= 50;
}
void DoFlashOfLight(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FLASH_OF_LIGHT, PickHealTarget(ctx).guid);
}

bool ShouldHolyLight(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HOLY_LIGHT)) return false;
    if (PickHealTarget(ctx).hp_pct > 85) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(HOLY_LIGHT)) return false;
    return true;
}
void DoHolyLight(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HOLY_LIGHT, PickHealTarget(ctx).guid);
}

// ---- Offensive filler when group is topped (Glimmer / HP gen) ----
bool ShouldHolyShockDamage(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!GroupTopped(ctx)) return false;
    if (!ctx.bot.knows_spell(HOLY_SHOCK)) return false;
    if (!ctx.bot.is_ready(HOLY_SHOCK)) return false;
    return !ctx.bot.victim().IsEmpty();
}
void DoHolyShockDamage(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HOLY_SHOCK, ctx.bot.victim());
}

// Judgment: spec variant 275773 (Holy) -> unified 315867 -> baseline 20271.
bool ShouldJudgment(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (ctx.bot.victim().IsEmpty()) return false;
    return PickKnownAndReady(ctx, { JUDGMENT_HOLY, JUDGMENT_UNIFIED, JUDGMENT_BASELINE }) != 0;
}
void DoJudgment(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    uint32 id = PickKnownAndReady(ctx, { JUDGMENT_HOLY, JUDGMENT_UNIFIED, JUDGMENT_BASELINE });
    if (id) e.cast(id, ctx.bot.victim());
}

bool ShouldHammerOfWrath(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!GroupTopped(ctx)) return false;
    if (!ctx.bot.knows_spell(HAMMER_OF_WRATH)) return false;
    if (!ctx.bot.is_ready(HAMMER_OF_WRATH)) return false;
    if (ctx.bot.victim().IsEmpty()) return false;
    NearbyUnit const* t = ctx.bot.victim_info();
    if (!t || t->max_hp <= 0 || t->hp <= 0) return false;
    return (t->hp * 100) / t->max_hp <= 20
        || ctx.bot.has_aura(AVENGING_WRATH)
        || ctx.bot.has_aura(AVENGING_WRATH_MODERN);
}
void DoHammerOfWrath(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HAMMER_OF_WRATH, ctx.bot.victim());
}

bool ShouldCrusaderStrike(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!GroupTopped(ctx)) return false;
    if (!ctx.bot.knows_spell(CRUSADER_STRIKE)) return false;
    if (!ctx.bot.is_ready(CRUSADER_STRIKE)) return false;
    return !ctx.bot.victim().IsEmpty();
}
void DoCrusaderStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CRUSADER_STRIKE, ctx.bot.victim());
}

bool AlwaysAlive(ApPredicateContext const& ctx) { return ctx.bot.is_alive(); }
void DoNothing(ApPredicateContext const&, BotIntentEmitter&) {}

// Cast-swap shim — Holy Paladin's slow single-target heals. Holy Light
// 2.5s, Flash of Light 1.5s. See ApHealHelpers.h.
bool ShouldCancelHealForSwap(ApPredicateContext const& ctx)
{
    return ShouldCancelHealForSwapImpl(ctx, { HOLY_LIGHT, FLASH_OF_LIGHT });
}

// Rule order (top-down). Lay on Hands -> Divine Shield -> Word of Glory
// self -> Hand of Reckoning -> Hammer of Justice -> Judgment ->
// spec-specific heals -> AutoAttack — exactly per the cross-spec
// paladin contract.
ApRule const kRules[] = {
    { ShouldCancelHealForSwap,    DoCancelHealForSwap,    "Cancel heal — swap to lower target" },
    { ShouldDevotionAura,         DoDevotionAura,         "Devotion Aura"                  },
    { ShouldKings,                DoKings,                "Blessing of Kings"              },
    { ShouldLayOnHands,           DoLayOnHands,           "Lay on Hands (<=15%)"           },
    { ShouldDivineShield,         DoDivineShield,         "Divine Shield (panic)"          },
    { ShouldWordOfGlorySelf,      DoWordOfGlorySelf,      "Word of Glory self (<=50%)"     },
    { ShouldHandOfReckoning,      DoHandOfReckoning,      "Hand of Reckoning (peel)"       },
    { ShouldRebuke,               DoRebuke,               "Rebuke (interrupt)"             },
    { ShouldHammerOfJustice,      DoHammerOfJustice,      "Hammer of Justice (fb)"         },
    { ShouldBlindingLight,        DoBlindingLight,        "Blinding Light (3+ AoE)"        },
    { ShouldDivineProtection,     DoDivineProtection,     "Divine Protection (<=45%)"      },
    { ShouldBlessingOfProtection, DoBlessingOfProtection, "Blessing of Protection"         },
    { ShouldBlessingOfSacrifice,  DoBlessingOfSacrifice,  "BoSac (tank <=30%)"             },
    { ShouldBeaconOfLight,        DoBeaconOfLight,        "Beacon of Light (tank)"         },
    { ShouldBeaconOfFaith,        DoBeaconOfFaith,        "Beacon of Faith (2nd tank)"     },
    { ShouldAuraMastery,          DoAuraMastery,          "Aura Mastery"                   },
    { ShouldAvengingWrath,        DoAvengingWrath,        "Avenging Wrath"                 },
    { ShouldHolyAvenger,          DoHolyAvenger,          "Holy Avenger"                   },
    { ShouldDivineToll,           DoDivineToll,           "Divine Toll"                    },
    { ShouldTyrsDeliverance,      DoTyrsDeliverance,      "Tyr's Deliverance"              },
    { ShouldBeaconOfVirtue,       DoBeaconOfVirtue,       "Beacon of Virtue (3+ wounded)"  },
    { ShouldCleanseToxins,        DoCleanseToxins,        "Cleanse Toxins (poison/disease)"},
    { ShouldCleanse,              DoCleanse,              "Cleanse (dispel)"               },
    { ShouldLightOfDawn,          DoLightOfDawn,          "Light of Dawn (2+ wounded)"     },
    { ShouldHolyShockHeal,        DoHolyShockHeal,        "Holy Shock (heal)"              },
    { ShouldWordOfGlory,          DoWordOfGlory,          "Word of Glory (<=80%)"          },
    { ShouldFlashOfLight,         DoFlashOfLight,         "Flash of Light (<=50%)"         },
    { ShouldHolyLight,            DoHolyLight,            "Holy Light (<=85%)"             },
    { ShouldJudgment,             DoJudgment,             "Judgment (filler)"              },
    { ShouldHolyShockDamage,      DoHolyShockDamage,      "Holy Shock damage (filler)"     },
    { ShouldHammerOfWrath,        DoHammerOfWrath,        "Hammer of Wrath (filler)"       },
    { ShouldCrusaderStrike,       DoCrusaderStrike,       "Crusader Strike (filler)"       },
    { AlwaysAlive,                DoNothing,              "Idle"                           },
};

} // anonymous

void RegisterApl_Paladin_Holy()
{
    constexpr uint32 SPEC_PALADIN_HOLY = 65;
    RegisterRotation(CLASS_PALADIN, SPEC_PALADIN_HOLY, ApRotation{kRules});
}

} // namespace Playerbot::Combat
