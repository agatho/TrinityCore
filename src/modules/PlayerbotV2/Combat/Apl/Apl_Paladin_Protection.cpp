// Protection Paladin - WoW 12.1.0.69587 (Midnight) rotation. Plate tank
// with Holy Power as the active-mitigation resource (Shield of the
// Righteous), pull + interrupt + cleave from Avenger's Shield, Consecration
// ground tick for AoE threat, Divine Toll for burst Holy Power, and Word of
// Glory as the Holy Power self heal. Lightsmith hero tree adds Holy Bulwark
// / Sacred Weapon armaments.
//
// Survival ladder: Lay on Hands (full heal panic) -> Divine Shield
// (immunity bail) -> Word of Glory self -> Ardent Defender (cheat death)
// -> Guardian of Ancient Kings (50% DR) -> Holy Bulwark (absorb) -> Eye of
// Tyr (25% enemy damage DR). Group utility: Blessing of Sacrifice,
// Blessing of Protection, Blessing of Freedom. Tank duties: Hand of
// Reckoning taunt, Rebuke interrupt, Hammer of Justice fallback, Avenger's
// Shield ranged silence.
//
// =================================================================
// Validated spell IDs (WoW 12.1.0.69587, kit Apl_Paladin_Protection.md)
// =================================================================
//    31935 - Avenger's Shield             (spec talent [R])
//    53600 - Shield of the Righteous      (baseline L2)
//    53595 - Hammer of the Righteous      (spec talent, choice vs Blessed Hammer)
//   204019 - Blessed Hammer               (spec talent [R]; replaces HotR)
//   275779 - Judgment                     (Prot spec spell; overrides 20271)
//    20271 - Judgment                     (baseline L3; pre-spec fallback)
//    26573 - Consecration                 (baseline L6)
//   209202 - Eye of Tyr                   (class baseline; was 387174)
//    31850 - Ardent Defender              (spec talent [R])
//    86659 - Guardian of Ancient Kings    (spec talent [R])
//   432459 - Holy Bulwark                 (Lightsmith [R]; becomes Sacred Weapon)
//   432472 - Sacred Weapon                (Lightsmith; other half of the armament)
//      633 - Lay on Hands                 (class talent [R])
//    24275 - Hammer of Wrath              (class baseline)
//    96231 - Rebuke                       (class talent [R])
//      853 - Hammer of Justice            (baseline L5)
//    62124 - Hand of Reckoning            (baseline L9)
//      642 - Divine Shield                (baseline L10)
//     6940 - Blessing of Sacrifice        (class talent [R])
//     1022 - Blessing of Protection       (class talent [R])
//     1044 - Blessing of Freedom          (class talent [R])
//      465 - Devotion Aura                (Auras of the Resolute [R])
//    31884 - Avenging Wrath               (spec talent [R]; cast + buff aura)
//   389539 - Sentinel                     (spec talent [M]; overrides Avenging Wrath)
//   375576 - Divine Toll                  (class talent [R])
//    85673 - Word of Glory                (baseline L7)
//    25771 - Forbearance                  (debuff only - never cast)
//
// =================================================================
// Skipped spells (and why)
// =================================================================
//      498 - Divine Protection: Holy spec spell in 12.1; Prot cannot learn it.
//   353367 / 358934 - Aegis of Light: not an active Prot talent in 12.1
//             (only a passive of that name remains); no cast surface.
//    31821 - Aura Mastery: Holy spec talent only in 12.1.
//   203538 - Blessing of Kings / 152262 Seraphim: not in 12.1 SpellName.
//   327193 - Moment of Glory: removed from the Prot tree in 12.1.
//   384376 - Avenging Wrath rank passive, 327980 Consecration rank passive,
//             315921 Word of Glory rank passive, 315867 Judgment passive:
//             not castable - a multi-id pick that lists them first stalls
//             the rule, so only the real cast ids are used.
//   204018 - Blessing of Spellwarding [M]: shares the BoP cooldown; BoP
//             covers the ally-save slot for both builds.
//   213644 - Cleanse Toxins [M] / 115750 Blinding Light [M]: M+-only picks
//             the default build does not own; not wired to keep the ladder
//             readable.
//   433568 - Rite of Sanctification: pre-pull weapon imbue, not combat.
//    35395 - Crusader Strike: Prot generates HP with Blessed Hammer /
//             Hammer of the Righteous; CS stays out of the ladder.
//
// =================================================================
// Decision tree (top-down)
// =================================================================
//   0) Aura maintenance:       Devotion Aura
//   1) Survival ladder:        LoH -> DS -> WoG self -> Ardent Defender
//                                -> GoAK -> Holy Bulwark
//   2) Tank duty:              Hand of Reckoning
//   3) Interrupt:              Rebuke -> Hammer of Justice
//   4) Freedom self:           rooted / snared
//   5) Group utility:          BoSac, BoP
//   6) Major offensive CDs:    Avenging Wrath / Sentinel, Divine Toll
//   7) Active mitigation:      Shield of the Righteous (HP3)
//   8) Threat/pull:            Avenger's Shield (opener + ranged silence)
//   9) Damage:                 Judgment -> Eye of Tyr -> HoW -> Consecration
//                                -> Blessed Hammer / HotR
//  10) AutoAttack fallback

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

#include <initializer_list>

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated against the kit) ----
constexpr uint32 AVENGERS_SHIELD            = 31935;
constexpr uint32 SHIELD_OF_THE_RIGHTEOUS    = 53600;
constexpr uint32 HAMMER_OF_THE_RIGHTEOUS    = 53595;
constexpr uint32 BLESSED_HAMMER             = 204019;     // talent [R] - replaces HotR
constexpr uint32 JUDGMENT_PROT              = 275779;     // Prot spec spell (overrides baseline)
constexpr uint32 JUDGMENT_BASELINE          = 20271;
constexpr uint32 CONSECRATION               = 26573;      // cast + ground aura
constexpr uint32 EYE_OF_TYR                 = 209202;     // 12.1 id (was 387174)
constexpr uint32 ARDENT_DEFENDER            = 31850;
constexpr uint32 GUARDIAN_OF_ANCIENT_KINGS  = 86659;
constexpr uint32 HOLY_BULWARK               = 432459;     // Lightsmith [R] - self absorb armament
constexpr uint32 SACRED_WEAPON              = 432472;     // Lightsmith - Bulwark toggles into this
constexpr uint32 LAY_ON_HANDS               = 633;
constexpr uint32 HAMMER_OF_WRATH            = 24275;
constexpr uint32 REBUKE                     = 96231;
constexpr uint32 HAMMER_OF_JUSTICE          = 853;
constexpr uint32 HAND_OF_RECKONING          = 62124;
constexpr uint32 DIVINE_SHIELD              = 642;
constexpr uint32 BLESSING_OF_SACRIFICE      = 6940;
constexpr uint32 BLESSING_OF_PROTECTION     = 1022;
constexpr uint32 BLESSING_OF_FREEDOM        = 1044;
constexpr uint32 DEVOTION_AURA              = 465;
constexpr uint32 AVENGING_WRATH             = 31884;      // cast + buff aura
constexpr uint32 SENTINEL                   = 389539;     // talent [M] - overrides Avenging Wrath
constexpr uint32 DIVINE_TOLL                = 375576;     // class talent [R] - 5x Avenger's Shield
constexpr uint32 WORD_OF_GLORY              = 85673;
constexpr uint32 FORBEARANCE                = 25771;      // debuff only

constexpr uint8 POWER_HOLY_POWER_IDX = 9;

// Mechanic ids (SharedDefines.h) for the Freedom self-cast.
constexpr uint32 MECHANIC_ROOT_ID  = 7;
constexpr uint32 MECHANIC_SNARE_ID = 11;

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

// ---- Survival ladder ----
bool ShouldLayOnHands(ApPredicateContext const& ctx)
{
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
    if (ctx.bot.hp_pct() > 15) return false;
    return !ctx.bot.is_ready(LAY_ON_HANDS);
}
void DoDivineShield(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DIVINE_SHIELD); }

// Word of Glory self heal (85673; the Prot "rank 2" 315921 is a passive
// and must not be in the cast list).
bool ShouldWordOfGlorySelf(ApPredicateContext const& ctx)
{
    if (HolyPower(ctx) < 3) return false;
    // Emergency-only threshold (<=35%): both WoG-self and Shield of the
    // Righteous drain Holy Power, but SotR is ranked below WoG in kRules.
    // With the old <=60% gate, a Prot Paladin tanking at normal (<60%) HP
    // dumped all HP into WoG every tick and never cast SotR, dropping
    // physical active-mitigation uptime to ~zero. Reserve WoG for real
    // spikes so SotR wins the Holy Power during normal tanking.
    if (ctx.bot.hp_pct() > 35) return false;
    return ctx.bot.knows_spell(WORD_OF_GLORY) && ctx.bot.is_ready(WORD_OF_GLORY);
}
void DoWordOfGlorySelf(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(WORD_OF_GLORY, ctx.bot.raw().guid);
}

bool ShouldArdentDefender(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ARDENT_DEFENDER)) return false;
    if (!ctx.bot.is_ready(ARDENT_DEFENDER)) return false;
    return ctx.bot.hp_pct() <= 30;
}
void DoArdentDefender(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ARDENT_DEFENDER); }

bool ShouldGuardianOfAncientKings(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(GUARDIAN_OF_ANCIENT_KINGS)) return false;
    if (!ctx.bot.is_ready(GUARDIAN_OF_ANCIENT_KINGS)) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoGuardianOfAncientKings(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(GUARDIAN_OF_ANCIENT_KINGS); }

// Holy Bulwark (Lightsmith [R]) - self absorb armament; the button
// toggles into Sacred Weapon after use, so both ids are candidates and
// whichever is currently castable goes on the bot. Mid-tier defensive
// that fills the gap when AD/GoAK are down.
bool ShouldHolyArmament(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (PickKnownAndReady(ctx, { HOLY_BULWARK, SACRED_WEAPON }) == 0) return false;
    return ctx.bot.hp_pct() <= 75;
}
void DoHolyArmament(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    uint32 id = PickKnownAndReady(ctx, { HOLY_BULWARK, SACRED_WEAPON });
    if (id) e.cast(id, ctx.bot.raw().guid);
}

// Blessing of Freedom on self when rooted / snared - a tank that cannot
// reposition loses the pull.
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

// ---- Tank utility ----
bool ShouldHandOfReckoning(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(HAND_OF_RECKONING)) return false;
    if (!ctx.bot.is_ready(HAND_OF_RECKONING)) return false;
    return ctx.bot.untaunted_enemy() != nullptr;
}
void DoHandOfReckoning(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.untaunted_enemy())
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
    if (!HasLiveTarget(ctx)) return false;
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

// ---- Group utility ----
bool ShouldBlessingOfSacrifice(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BLESSING_OF_SACRIFICE)) return false;
    if (!ctx.bot.is_ready(BLESSING_OF_SACRIFICE)) return false;
    if (ctx.bot.hp_pct() <= 70) return false;
    auto const* low = ctx.group.lowest_hp_on_map(ctx.bot.map_id(), Role::Unknown, ctx.bot.raw().position.x, ctx.bot.raw().position.y, ctx.bot.raw().position.z, 45.0f);
    return low && low->online && low->max_hp > 0
        && (low->hp * 100) / low->max_hp <= 30
        && low->guid != ctx.bot.raw().guid;
}
void DoBlessingOfSacrifice(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* low = ctx.group.lowest_hp_on_map(ctx.bot.map_id(), Role::Unknown, ctx.bot.raw().position.x, ctx.bot.raw().position.y, ctx.bot.raw().position.z, 45.0f))
        e.cast(BLESSING_OF_SACRIFICE, low->guid);
}

bool ShouldBlessingOfProtection(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BLESSING_OF_PROTECTION)) return false;
    if (!ctx.bot.is_ready(BLESSING_OF_PROTECTION)) return false;
    if (auto const* low = ctx.group.lowest_hp_on_map(ctx.bot.map_id(), Role::Unknown, ctx.bot.raw().position.x, ctx.bot.raw().position.y, ctx.bot.raw().position.z, 45.0f)) {
        if (low->role != Role::Tank && low->online && low->hp > 0
            && (low->hp * 100) / low->max_hp <= 25
            && !ctx.bot.has_aura(FORBEARANCE, low->guid))
            return true;
    }
    return false;
}
void DoBlessingOfProtection(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* low = ctx.group.lowest_hp_on_map(ctx.bot.map_id(), Role::Unknown, ctx.bot.raw().position.x, ctx.bot.raw().position.y, ctx.bot.raw().position.z, 45.0f))
        e.cast(BLESSING_OF_PROTECTION, low->guid);
}

// ---- Major offensive cooldowns ----
// Avenging Wrath 31884 is overridden by Sentinel 389539 when that talent
// is known ([M] build); cast whichever the bot actually has. 384376 is
// only the rank passive and is never cast.
uint32 WingsId(ApPredicateContext const& ctx)
{
    return ctx.bot.knows_spell(SENTINEL) ? SENTINEL : AVENGING_WRATH;
}

bool ShouldAvengingWrath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    const uint32 id = WingsId(ctx);
    if (!ctx.bot.knows_spell(id) || !ctx.bot.is_ready(id)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.attackers_count() >= 3;
}
void DoAvengingWrath(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(WingsId(ctx));
}

// Divine Toll - Avenger's Shield on up to 5 enemies, each generating Holy
// Power. Fire on a pull with 2+ attackers or when starved for HP.
bool ShouldDivineToll(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DIVINE_TOLL)) return false;
    if (!ctx.bot.is_ready(DIVINE_TOLL)) return false;
    return ctx.bot.attackers_count() >= 2 || HolyPower(ctx) <= 1;
}
void DoDivineToll(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DIVINE_TOLL, ctx.bot.victim());
}

// ---- Active mitigation ----
bool ShouldShieldOfTheRighteous(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHIELD_OF_THE_RIGHTEOUS)) return false;
    if (!ctx.bot.is_ready(SHIELD_OF_THE_RIGHTEOUS)) return false;
    return HolyPower(ctx) >= 3;
}
void DoShieldOfTheRighteous(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SHIELD_OF_THE_RIGHTEOUS); }

// ---- Threat / damage ----
bool ShouldAvengersShield(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(AVENGERS_SHIELD)) return false;
    return ctx.bot.is_ready(AVENGERS_SHIELD);
}
void DoAvengersShield(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(AVENGERS_SHIELD, ctx.bot.victim());
}

// Eye of Tyr 209202 - AoE Holy damage + 25% less damage dealt to the
// bot by every enemy hit for 9s. Cleave threat on 2+, or a defensive
// when the tank is getting low even on a single target.
bool ShouldEyeOfTyr(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(EYE_OF_TYR)) return false;
    if (!ctx.bot.is_ready(EYE_OF_TYR)) return false;
    return ctx.bot.enemies_within(8.0f) >= 2 || ctx.bot.hp_pct() <= 60;
}
void DoEyeOfTyr(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(EYE_OF_TYR); }

// Hammer of Wrath: execute (<=20%) or any target during Wings.
bool ShouldHammerOfWrath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HAMMER_OF_WRATH)) return false;
    if (!ctx.bot.is_ready(HAMMER_OF_WRATH)) return false;
    NearbyUnit const* t = ctx.bot.victim_info();
    if (!t || t->max_hp <= 0 || t->hp <= 0) return false;
    return (t->hp * 100) / t->max_hp <= 20
        || ctx.bot.has_aura(AVENGING_WRATH)
        || ctx.bot.has_aura(SENTINEL);
}
void DoHammerOfWrath(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HAMMER_OF_WRATH, ctx.bot.victim());
}

// Judgment: Prot spec spell 275779 (overrides baseline) -> baseline 20271.
bool ShouldJudgment(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    return PickKnownAndReady(ctx, { JUDGMENT_PROT, JUDGMENT_BASELINE }) != 0;
}
void DoJudgment(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    uint32 id = PickKnownAndReady(ctx, { JUDGMENT_PROT, JUDGMENT_BASELINE });
    if (id) e.cast(id, ctx.bot.victim());
}

// Consecration 26573 - maintain the ground tick (the 12.1 Prot "rank 3"
// 327980 is a passive cooldown reduction, not a separate cast).
bool ShouldConsecration(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(CONSECRATION)) return false;
    if (!ctx.bot.is_ready(CONSECRATION)) return false;
    return !ctx.bot.has_aura(CONSECRATION);
}
void DoConsecration(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CONSECRATION); }

bool ShouldBlessedHammer(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BLESSED_HAMMER)) return false;
    return ctx.bot.is_ready(BLESSED_HAMMER);
}
void DoBlessedHammer(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLESSED_HAMMER); }

bool ShouldHammerOfTheRighteous(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(BLESSED_HAMMER)) return false;
    if (!ctx.bot.knows_spell(HAMMER_OF_THE_RIGHTEOUS)) return false;
    return ctx.bot.is_ready(HAMMER_OF_THE_RIGHTEOUS);
}
void DoHammerOfTheRighteous(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HAMMER_OF_THE_RIGHTEOUS, ctx.bot.victim());
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

// Rule order: cross-spec paladin ladder first (LoH -> DS -> WoG self ->
// HandOfReckoning -> HoJ -> Judgment) then Prot-specific (active
// mitigation + threat + AoE + filler).
ApRule const kRules[] = {
    { ShouldDevotionAura,          DoDevotionAura,          "Devotion Aura"             },
    { ShouldLayOnHands,            DoLayOnHands,            "Lay on Hands (<=15%)"      },
    { ShouldDivineShield,          DoDivineShield,          "Divine Shield (panic)"     },
    { ShouldWordOfGlorySelf,       DoWordOfGlorySelf,       "Word of Glory (self heal)" },
    { ShouldHandOfReckoning,       DoHandOfReckoning,       "Hand of Reckoning (taunt)" },
    { ShouldRebuke,                DoRebuke,                "Rebuke (interrupt)"        },
    { ShouldHammerOfJustice,       DoHammerOfJustice,       "Hammer of Justice (fb)"    },
    { ShouldArdentDefender,        DoArdentDefender,        "Ardent Defender (<=30%)"   },
    { ShouldGuardianOfAncientKings,DoGuardianOfAncientKings,"Guardian of Ancient Kings" },
    { ShouldHolyArmament,          DoHolyArmament,          "Holy Bulwark (<=75%)"      },
    { ShouldBlessingOfFreedom,     DoBlessingOfFreedom,     "Blessing of Freedom (self)"},
    { ShouldBlessingOfSacrifice,   DoBlessingOfSacrifice,   "BoSac (low ally)"          },
    { ShouldBlessingOfProtection,  DoBlessingOfProtection,  "Blessing of Protection"    },
    { ShouldAvengingWrath,         DoAvengingWrath,         "Avenging Wrath / Sentinel" },
    { ShouldDivineToll,            DoDivineToll,            "Divine Toll"               },
    { ShouldShieldOfTheRighteous,  DoShieldOfTheRighteous,  "Shield of the Righteous"   },
    { ShouldAvengersShield,        DoAvengersShield,        "Avenger's Shield"          },
    { ShouldJudgment,              DoJudgment,              "Judgment"                  },
    { ShouldEyeOfTyr,              DoEyeOfTyr,              "Eye of Tyr (AoE / DR)"     },
    { ShouldHammerOfWrath,         DoHammerOfWrath,         "Hammer of Wrath"           },
    { ShouldConsecration,          DoConsecration,          "Consecration (maintain)"   },
    { ShouldBlessedHammer,         DoBlessedHammer,         "Blessed Hammer"            },
    { ShouldHammerOfTheRighteous,  DoHammerOfTheRighteous,  "Hammer of the Righteous"   },
    { AlwaysInCombat,              DoAutoAttack,            "Engage auto attack"        },
};

} // anonymous

void RegisterApl_Paladin_Protection()
{
    constexpr uint32 SPEC_PALADIN_PROTECTION = 66;
    RegisterRotation(CLASS_PALADIN, SPEC_PALADIN_PROTECTION, ApRotation{kRules});
}

} // namespace Playerbot::Combat
