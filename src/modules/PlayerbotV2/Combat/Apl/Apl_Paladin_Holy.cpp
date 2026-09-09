// Holy Paladin - WoW 12.1.0.69587 (Midnight) rotation. Holy Power healer
// with Beacon of Light / Beacon of Virtue transfer, Holy Shock as the
// primary heal + HP generator, Word of Glory (or Eternal Flame with the
// Herald of the Sun hero tree) burst heal, Light of Dawn AoE, and
// Avenging Wrath / Divine Toll throughput cooldowns. Aura Mastery is the
// raid-wide DR (Ringing of the Heavens also fires a Divine Toll).
//
// =================================================================
// Validated spell IDs (WoW 12.1.0.69587, kit Apl_Paladin_Holy.md)
// =================================================================
//   275773 - Judgment                 (Holy spec spell; overrides 20271)
//    20271 - Judgment                 (baseline L3; pre-spec fallback)
//    20473 - Holy Shock               (spec talent [R]; replaces Crusader Strike)
//    19750 - Flash of Light           (baseline L4)
//    82326 - Holy Light               (spec spell L11)
//    85222 - Light of Dawn            (spec talent [R])
//    85673 - Word of Glory            (baseline L7)
//   156322 - Eternal Flame            (Herald of the Sun [R] - overrides WoG)
//      633 - Lay on Hands             (class talent [R])
//      642 - Divine Shield            (baseline L10)
//      498 - Divine Protection        (spec spell L26 - 20% self DR)
//      465 - Devotion Aura            (Auras of the Resolute [R])
//     6940 - Blessing of Sacrifice    (class talent [R])
//     1022 - Blessing of Protection   (class talent [R])
//     1044 - Blessing of Freedom      (class talent [R])
//    31821 - Aura Mastery             (spec talent [R])
//    31884 - Avenging Wrath           (spec talent [R]; buff aura is the same id)
//   375576 - Divine Toll              (class talent [R]; was 304971)
//   200652 - Tyr's Deliverance        (class baseline; 90s cd cone heal)
//    53563 - Beacon of Light          (spec spell L16; overridden by 200025)
//   156910 - Beacon of Faith          (spec talent [M])
//   200025 - Beacon of Virtue         (spec talent [R]; replaces Beacon of Light)
//     4987 - Cleanse                  (spec spell L10; Magic, +Poison/Disease
//                                       with Improved Cleanse 393024 [R])
//      853 - Hammer of Justice        (baseline L5)
//   115750 - Blinding Light           (class talent [R])
//    62124 - Hand of Reckoning        (baseline L9)
//    24275 - Hammer of Wrath          (class baseline)
//    25771 - Forbearance              (debuff only - never cast)
//   393024 - Improved Cleanse         (passive [R] - knows_spell gate only)
//
// =================================================================
// Skipped spells (and why)
// =================================================================
//    96231 - Rebuke: Retribution/Protection class-tree node only in 12.1;
//             Holy cannot learn it. Hammer of Justice is the interrupt.
//   105809 - Holy Avenger: removed from the Holy tree in 12.1.
//   203538 - Blessing of Kings: does not exist in 12.1 SpellName.
//    20066 - Repentance: not available to Holy in 12.1.
//   384376 - Avenging Wrath (rank passive): not castable; 31884 is both
//             the cast and the buff aura.
//   315867 - Judgment (Holy Power passive): not castable; would hijack the
//             multi-id pick and stall the rule.
//   440013 - Cleanse Toxins: overridden by Cleanse 4987 the moment Holy
//             spec is chosen (L10); Improved Cleanse folds poison/disease
//             into Cleanse for the [R] build.
//    35395 - Crusader Strike: overridden by Holy Shock for Holy in 12.1.
//   415091 - Shield of the Righteous (Holy): 5y melee spender; the healer
//             bot does not hold melee position, so not wired.
//   114165 - Holy Prism: choice node vs Divine Toll; [R]/[M] both pick Toll.
//   212056 - Absolution / 391054 Intercession: resurrections, handled by
//             the out-of-combat revive logic, not the combat APL.
//   190784 - Divine Steed / 433568 Rites: movement / pre-pull weapon imbue.
//
// =================================================================
// Decision tree (top-down)
// =================================================================
//   0) Aura maintenance:      Devotion Aura
//   1) Cast-swap shim:        cancel slow heal if better target appears
//   2) Emergency self:        Lay on Hands -> Divine Shield -> WoG/EF self
//   3) Tank duty:             Hand of Reckoning (self peel)
//   4) Interrupt / CC:        Hammer of Justice -> Blinding Light
//   5) Freedom self:          rooted / snared
//   6) Defensive CD:          Divine Protection (self DR)
//   7) BoP/BoSac group save:  ally <=25% / tank <=30%
//   8) Beacon maintenance:    Beacon of Light tank (non-BoV), Beacon of Faith
//   9) Big raid CDs:          Aura Mastery, Avenging Wrath, Divine Toll,
//                              Tyr's Deliverance, Beacon of Virtue
//  10) Dispel:                Cleanse (Magic; +Poison/Disease when improved)
//  11) AoE heal:              Light of Dawn (HP3)
//  12) Spike heal:            Holy Shock, Word of Glory / Eternal Flame,
//                              Flash of Light
//  13) Filler heal:           Holy Light
//  14) Offensive filler:      Judgment, Holy Shock dmg, Hammer of Wrath

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

// ---- Spell IDs (WoW 12.1.0.69587, validated against the kit) ----
constexpr uint32 HOLY_SHOCK             = 20473;
constexpr uint32 WORD_OF_GLORY          = 85673;
constexpr uint32 ETERNAL_FLAME          = 156322;     // Herald of the Sun - overrides WoG
constexpr uint32 FLASH_OF_LIGHT         = 19750;
constexpr uint32 HOLY_LIGHT             = 82326;
constexpr uint32 LAY_ON_HANDS           = 633;
constexpr uint32 BLESSING_OF_PROTECTION = 1022;
constexpr uint32 BLESSING_OF_FREEDOM    = 1044;
constexpr uint32 LIGHT_OF_DAWN          = 85222;
constexpr uint32 AVENGING_WRATH         = 31884;      // cast + buff aura
constexpr uint32 AURA_MASTERY           = 31821;
constexpr uint32 DIVINE_TOLL            = 375576;     // 12.1 id (was 304971)
constexpr uint32 TYRS_DELIVERANCE       = 200652;     // 90s cd cone heal
constexpr uint32 BEACON_OF_LIGHT        = 53563;
constexpr uint32 BEACON_OF_FAITH        = 156910;     // talent [M] - 2nd beacon
constexpr uint32 BEACON_OF_VIRTUE       = 200025;     // talent [R] - replaces Beacon of Light
constexpr uint32 CLEANSE                = 4987;
constexpr uint32 IMPROVED_CLEANSE       = 393024;     // passive [R] - Cleanse also strips poison/disease
constexpr uint32 DIVINE_SHIELD          = 642;
constexpr uint32 DIVINE_PROTECTION      = 498;        // defensive - 20% DR
constexpr uint32 BLESSING_OF_SACRIFICE  = 6940;
constexpr uint32 HAMMER_OF_JUSTICE      = 853;
constexpr uint32 BLINDING_LIGHT         = 115750;
constexpr uint32 DEVOTION_AURA          = 465;
constexpr uint32 FORBEARANCE            = 25771;      // debuff only
constexpr uint32 HAND_OF_RECKONING      = 62124;
constexpr uint32 JUDGMENT_HOLY          = 275773;     // Holy spec spell (overrides baseline)
constexpr uint32 JUDGMENT_BASELINE      = 20271;
constexpr uint32 HAMMER_OF_WRATH        = 24275;

constexpr uint8 POWER_HOLY_POWER_IDX = 9;

// Mechanic ids (SharedDefines.h) for the Freedom self-cast.
constexpr uint32 MECHANIC_ROOT_ID  = 7;
constexpr uint32 MECHANIC_SNARE_ID = 11;

// ---- Multi-ID helpers (try spec variant first, fall back to baseline) ----
// Why this matters: a level-5 Holy paladin has the baseline 20271
// Judgment but not 275773. A level-10+ Holy paladin has the spec spell.
// Picking the first known + ready spell is the cleanest way to support
// the entire 1-90 level band without per-level branching.
uint32 PickKnownAndReady(ApPredicateContext const& ctx, std::initializer_list<uint32> ids)
{
    for (uint32 id : ids)
        if (ctx.bot.knows_spell(id) && ctx.bot.is_ready(id)) return id;
    return 0;
}

// Word of Glory is overridden by Eternal Flame when the Herald of the Sun
// hero talent is known ([R] build). Cast whichever the bot actually has.
uint32 WogId(ApPredicateContext const& ctx)
{
    return ctx.bot.knows_spell(ETERNAL_FLAME) ? ETERNAL_FLAME : WORD_OF_GLORY;
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
    // "my own HP <= below_pct" - the 92% GroupTopped gates then froze ALL
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

// Cleanse strips Magic baseline; Poison/Disease only once Improved Cleanse
// (393024, [R]) is known. Without the gate a non-talented bot would burn
// the 8s Cleanse cooldown on a poison it cannot remove.
GroupMemberSummary const* DispelTarget(ApPredicateContext const& ctx)
{
    const bool toxins = ctx.bot.knows_spell(IMPROVED_CLEANSE);
    return DispelTargetWithPriority(ctx, [toxins](GroupSnapshotView const& g)
        -> GroupMemberSummary const*
    {
        if (auto const* m = g.dispel_candidate(DispelType::Magic))   return m;
        if (!toxins) return nullptr;
        if (auto const* m = g.dispel_candidate(DispelType::Disease)) return m;
        if (auto const* m = g.dispel_candidate(DispelType::Poison))  return m;
        return nullptr;
    });
}

bool SelfNeedsDispel(ApPredicateContext const& ctx)
{
    if (ctx.bot.self_dispellable(DispelType::Magic)) return true;
    if (!ctx.bot.knows_spell(IMPROVED_CLEANSE)) return false;
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

// Self Word of Glory / Eternal Flame at <=50% HP. The wider heal-target
// loop picks WoG for allies; this rule is the ordered self-preservation
// step.
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

// Blessing of Freedom on self when rooted / snared - a healer that cannot
// reposition out of a puddle or back to range dies to it.
bool ShouldBlessingOfFreedom(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(BLESSING_OF_FREEDOM)) return false;
    if (!ctx.bot.is_ready(BLESSING_OF_FREEDOM)) return false;
    if (ctx.bot.has_aura(BLESSING_OF_FREEDOM)) return false;
    return ctx.bot.has_mechanic(MECHANIC_ROOT_ID) || ctx.bot.has_mechanic(MECHANIC_SNARE_ID);
}
void DoBlessingOfFreedom(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BLESSING_OF_FREEDOM, ctx.bot.raw().guid);
}

// Divine Protection - 20% all-school DR self CD; complements the ladder
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
    // Only peel for self - Holy paladins should not be steady-state tanking.
    if (ctx.bot.hp_pct() > 40) return false;
    return ctx.bot.untaunted_enemy(40.0f) != nullptr;
}
void DoHandOfReckoning(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.untaunted_enemy(40.0f))
        e.cast(HAND_OF_RECKONING, t->guid);
}

// ---- Interrupt / CC ----
// Holy has no Rebuke in 12.1 (Ret/Prot class-tree node only); Hammer of
// Justice's stun is the healer's only interrupt.
bool ShouldHammerOfJustice(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HAMMER_OF_JUSTICE)) return false;
    if (!ctx.bot.is_ready(HAMMER_OF_JUSTICE)) return false;
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
// Beacon of Light is overridden by Beacon of Virtue in the [R] build -
// the permanent tank beacon only exists for non-Virtue loadouts.
bool ShouldBeaconOfLight(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BEACON_OF_LIGHT)) return false;
    if (ctx.bot.knows_spell(BEACON_OF_VIRTUE)) return false;
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

// Beacon of Virtue (mana, 15s cd, 9s duration): beacon the heal target
// plus injured allies so the following Holy Shock / WoG / LoD transfer.
// Fire whenever two or more allies are hurt so the transfer window
// actually overlaps the heals below it in the ladder.
bool ShouldBeaconOfVirtue(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BEACON_OF_VIRTUE)) return false;
    if (!ctx.bot.is_ready(BEACON_OF_VIRTUE)) return false;
    return WoundedFriendCount(ctx, 80) >= 2;
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

// Avenging Wrath 31884 - the spec-tree talent is the cast AND the buff
// aura (384376 is only the rank passive and must not be cast).
bool ShouldAvengingWrath(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(AVENGING_WRATH)) return false;
    if (!ctx.bot.is_ready(AVENGING_WRATH)) return false;
    return WoundedFriendCount(ctx, 60) >= 2;
}
void DoAvengingWrath(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(AVENGING_WRATH); }

// Divine Toll 375576 - Holy Shock on up to 5 targets around the heal
// target (each generating Holy Power).
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
// Cleanse (Holy spec spell, replaces Cleanse Toxins): Magic always,
// Poison/Disease once Improved Cleanse is known - see DispelTarget().
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

// Word of Glory / Eternal Flame (3 HP) on the heal target.
bool ShouldWordOfGlory(ApPredicateContext const& ctx)
{
    const uint32 id = WogId(ctx);
    if (!ctx.bot.knows_spell(id)) return false;
    if (!ctx.bot.is_ready(id)) return false;
    if (HolyPower(ctx) < 3) return false;
    return PickHealTarget(ctx).hp_pct <= 80;
}
void DoWordOfGlory(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(WogId(ctx), PickHealTarget(ctx).guid);
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

// Judgment: Holy spec spell 275773 (overrides baseline) -> baseline 20271.
bool ShouldJudgment(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (ctx.bot.victim().IsEmpty()) return false;
    return PickKnownAndReady(ctx, { JUDGMENT_HOLY, JUDGMENT_BASELINE }) != 0;
}
void DoJudgment(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    uint32 id = PickKnownAndReady(ctx, { JUDGMENT_HOLY, JUDGMENT_BASELINE });
    if (id) e.cast(id, ctx.bot.victim());
}

// Hammer of Wrath: execute (<=20%) or any target during Avenging Wrath.
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
        || ctx.bot.has_aura(AVENGING_WRATH);
}
void DoHammerOfWrath(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HAMMER_OF_WRATH, ctx.bot.victim());
}

bool AlwaysAlive(ApPredicateContext const& ctx) { return ctx.bot.is_alive(); }
void DoNothing(ApPredicateContext const&, BotIntentEmitter&) {}

// Cast-swap shim - Holy Paladin's slow single-target heals. Holy Light
// 2.5s, Flash of Light 1.5s. See ApHealHelpers.h.
bool ShouldCancelHealForSwap(ApPredicateContext const& ctx)
{
    return ShouldCancelHealForSwapImpl(ctx, { HOLY_LIGHT, FLASH_OF_LIGHT });
}

// Rule order (top-down). Lay on Hands -> Divine Shield -> Word of Glory
// self -> Hand of Reckoning -> Hammer of Justice -> group saves ->
// beacons -> raid CDs -> dispel -> heals -> offensive filler -> Idle.
ApRule const kRules[] = {
    { ShouldCancelHealForSwap,    DoCancelHealForSwap,    "Cancel heal - swap to lower target" },
    { ShouldDevotionAura,         DoDevotionAura,         "Devotion Aura"                  },
    { ShouldLayOnHands,           DoLayOnHands,           "Lay on Hands (<=15%)"           },
    { ShouldDivineShield,         DoDivineShield,         "Divine Shield (panic)"          },
    { ShouldWordOfGlorySelf,      DoWordOfGlorySelf,      "Word of Glory self (<=50%)"     },
    { ShouldHandOfReckoning,      DoHandOfReckoning,      "Hand of Reckoning (peel)"       },
    { ShouldHammerOfJustice,      DoHammerOfJustice,      "Hammer of Justice (kick)"       },
    { ShouldBlindingLight,        DoBlindingLight,        "Blinding Light (3+ AoE)"        },
    { ShouldBlessingOfFreedom,    DoBlessingOfFreedom,    "Blessing of Freedom (self)"     },
    { ShouldDivineProtection,     DoDivineProtection,     "Divine Protection (<=45%)"      },
    { ShouldBlessingOfProtection, DoBlessingOfProtection, "Blessing of Protection"         },
    { ShouldBlessingOfSacrifice,  DoBlessingOfSacrifice,  "BoSac (tank <=30%)"             },
    { ShouldBeaconOfLight,        DoBeaconOfLight,        "Beacon of Light (tank)"         },
    { ShouldBeaconOfFaith,        DoBeaconOfFaith,        "Beacon of Faith (2nd tank)"     },
    { ShouldAuraMastery,          DoAuraMastery,          "Aura Mastery"                   },
    { ShouldAvengingWrath,        DoAvengingWrath,        "Avenging Wrath"                 },
    { ShouldDivineToll,           DoDivineToll,           "Divine Toll"                    },
    { ShouldTyrsDeliverance,      DoTyrsDeliverance,      "Tyr's Deliverance"              },
    { ShouldBeaconOfVirtue,       DoBeaconOfVirtue,       "Beacon of Virtue (2+ wounded)"  },
    { ShouldCleanse,              DoCleanse,              "Cleanse (dispel)"               },
    { ShouldLightOfDawn,          DoLightOfDawn,          "Light of Dawn (2+ wounded)"     },
    { ShouldHolyShockHeal,        DoHolyShockHeal,        "Holy Shock (heal)"              },
    { ShouldWordOfGlory,          DoWordOfGlory,          "Word of Glory (<=80%)"          },
    { ShouldFlashOfLight,         DoFlashOfLight,         "Flash of Light (<=50%)"         },
    { ShouldHolyLight,            DoHolyLight,            "Holy Light (<=85%)"             },
    { ShouldJudgment,             DoJudgment,             "Judgment (filler)"              },
    { ShouldHolyShockDamage,      DoHolyShockDamage,      "Holy Shock damage (filler)"     },
    { ShouldHammerOfWrath,        DoHammerOfWrath,        "Hammer of Wrath (filler)"       },
    { AlwaysAlive,                DoNothing,              "Idle"                           },
};

} // anonymous

void RegisterApl_Paladin_Holy()
{
    constexpr uint32 SPEC_PALADIN_HOLY = 65;
    RegisterRotation(CLASS_PALADIN, SPEC_PALADIN_HOLY, ApRotation{kRules});
}

} // namespace Playerbot::Combat
