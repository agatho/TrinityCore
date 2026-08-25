// Protection Paladin - WoW 12.0 enterprise rotation. Plate tank with Holy
// Power as the active-mitigation resource (Shield of the Righteous), pull
// + interrupt + cleave from Avenger's Shield, Consecration ground tick for
// AoE threat, and Word of Glory as a Holy Power heal alternative.
//
// Survival ladder: Lay on Hands (full heal panic) -> Divine Shield
// (immunity bail) -> Ardent Defender (50% DR + cheat death) -> Guardian of
// Ancient Kings (50% DR) -> Aegis of Light (talent, group ranged DR) ->
// Word of Glory (HP self heal). Group utility: Blessing of Sacrifice,
// Blessing of Protection, Blessing of Freedom, Aura Mastery (50% magic DR
// group buff). Tank duties: Hand of Reckoning taunt, Rebuke interrupt,
// Hammer of Justice fallback, Avenger's Shield for ranged silence.
//
// =================================================================
// Validated IDs (cross-checked vs SpellName.csv + SpellLevels.csv,
// Wago dump, WoW 12.0 client)
// =================================================================
//    31935 — Avenger's Shield
//    53600 — Shield of the Righteous           (baseline)
//   415091 — Shield of the Righteous           (Holy talent variant — not
//                                                used by Prot; documented)
//    53595 — Hammer of the Righteous
//   204019 — Blessed Hammer                     (talent — replaces HotR)
//    20271 — Judgment                            (baseline)
//   275779 — Judgment                            (Prot spec variant; primary)
//   315867 — Judgment                            (modern unified spec variant)
//    26573 — Consecration                         (baseline)
//   327980 — Consecration                         (Prot spec variant; primary)
//   387174 — Eye of Tyr                            (talent — frontal cone fear)
//    31850 — Ardent Defender
//    86659 — Guardian of Ancient Kings
//      633 — Lay on Hands
//    24275 — Hammer of Wrath
//    96231 — Rebuke
//      853 — Hammer of Justice
//    62124 — Hand of Reckoning
//      642 — Divine Shield
//      498 — Divine Protection                       (defensive 20% DR)
//   353367 — Aegis of Light                          (talent — group DR
//                                                       channel)
//   358934 — Aegis of Light                          (talent rank/variant)
//     6940 — Blessing of Sacrifice
//     1022 — Blessing of Protection
//     1044 — Blessing of Freedom
//    31821 — Aura Mastery
//      465 — Devotion Aura
//   203538 — Blessing of Kings
//    31884 — Avenging Wrath                          (classic)
//   384376 — Avenging Wrath                          (modern spec variant)
//   152262 — Seraphim                                  (talent)
//   327193 — Moment of Glory                           (talent — AS reset)
//    85673 — Word of Glory                              (baseline ID)
//   315921 — Word of Glory                              (Prot spec variant —
//                                                          self-heal flavour)
//    25771 — Forbearance
//    35395 — Crusader Strike                            (baseline)
//   342348 — Crusader Strike                            (Prot/Ret spec
//                                                          variant)
//   105805 — Sanctuary                                    (PASSIVE — talent
//                                                          mitigation aura;
//                                                          not cast)
//    25780 — Righteous Fury                               (PASSIVE — talent
//                                                          threat aura;
//                                                          not cast)
//
// =================================================================
// Skipped spells (and why)
// =================================================================
//   376996 — Seasoned Warhorse: mount, not a combat rotation spell.
//   105805 — Sanctuary: passive talent (mitigation/threat aura); no cast
//             surface.
//    25780 — Righteous Fury: passive (legacy threat aura, persistent
//             passive on modern Prot); not cast.
//   415091 — Shield of the Righteous (Holy talent variant): Prot uses the
//             baseline 53600. The 415091 ID is a Holy-specific talent
//             that converts WoG into a SotR-like absorb.
//   342348 — Crusader Strike (Prot variant): Prot's HP generator is
//             Hammer of the Righteous / Blessed Hammer, not Crusader
//             Strike. Documented but not in the rotation; if a future
//             talent path brings CS back, wire here.
//
// =================================================================
// Decision tree (top-down)
// =================================================================
//   0) Aura/buff maintenance: Devotion Aura, Blessing of Kings
//   1) Survival ladder:        LoH -> DS -> WoG self -> Ardent Defender
//                                -> GoAK -> Divine Protection -> Aegis
//   2) Tank duty:              Hand of Reckoning
//   3) Interrupt:              Rebuke -> Hammer of Justice
//   4) Group utility:          Aura Mastery, BoSac, BoP
//   5) Major offensive CDs:    Avenging Wrath, Seraphim, Moment of Glory
//   6) Active mitigation:      Shield of the Righteous (HP3)
//   7) Threat/pull:            Avenger's Shield (opener + ranged silence)
//   8) AoE:                    Eye of Tyr
//   9) Damage:                 Judgment -> HoW -> Consecration -> HotR /
//                                Blessed Hammer
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

// ---- Spell IDs (WoW 12.0, validated) ----
constexpr uint32 AVENGERS_SHIELD            = 31935;
constexpr uint32 SHIELD_OF_THE_RIGHTEOUS    = 53600;
constexpr uint32 HAMMER_OF_THE_RIGHTEOUS    = 53595;
constexpr uint32 BLESSED_HAMMER             = 204019;     // talent — replaces HotR
constexpr uint32 JUDGMENT_PROT              = 275779;     // Prot judgment variant (primary)
constexpr uint32 JUDGMENT_UNIFIED           = 315867;     // modern unified spec variant
constexpr uint32 JUDGMENT_BASELINE          = 20271;
constexpr uint32 CONSECRATION_PROT          = 327980;     // Prot spec variant
constexpr uint32 CONSECRATION_BASELINE      = 26573;
constexpr uint32 EYE_OF_TYR                 = 387174;
constexpr uint32 ARDENT_DEFENDER            = 31850;
constexpr uint32 GUARDIAN_OF_ANCIENT_KINGS  = 86659;
constexpr uint32 LAY_ON_HANDS               = 633;
constexpr uint32 HAMMER_OF_WRATH            = 24275;
constexpr uint32 REBUKE                     = 96231;
constexpr uint32 HAMMER_OF_JUSTICE          = 853;
constexpr uint32 HAND_OF_RECKONING          = 62124;
constexpr uint32 DIVINE_SHIELD              = 642;
constexpr uint32 DIVINE_PROTECTION          = 498;        // 20% all-school DR
constexpr uint32 AEGIS_OF_LIGHT_A           = 353367;     // talent — group DR channel
constexpr uint32 AEGIS_OF_LIGHT_B           = 358934;     // talent variant
constexpr uint32 BLESSING_OF_SACRIFICE      = 6940;
constexpr uint32 BLESSING_OF_PROTECTION     = 1022;
constexpr uint32 BLESSING_OF_FREEDOM        = 1044;
constexpr uint32 AURA_MASTERY               = 31821;
constexpr uint32 DEVOTION_AURA              = 465;
constexpr uint32 BLESSING_OF_KINGS          = 203538;
constexpr uint32 AVENGING_WRATH             = 31884;
constexpr uint32 AVENGING_WRATH_MODERN      = 384376;     // Prot spec variant
constexpr uint32 SERAPHIM                   = 152262;
constexpr uint32 MOMENT_OF_GLORY            = 327193;     // talent — Avenger's Shield reset
constexpr uint32 WORD_OF_GLORY              = 85673;
constexpr uint32 WORD_OF_GLORY_PROT         = 315921;     // Prot variant (self-heal flavour)
constexpr uint32 FORBEARANCE                = 25771;

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
    if (!ctx.bot.knows_spell(BLESSING_OF_KINGS)) return false;
    return !ctx.bot.has_aura(BLESSING_OF_KINGS);
}
void DoKings(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BLESSING_OF_KINGS, ctx.bot.raw().guid);
}

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

// Word of Glory self heal — try the Prot spec variant first.
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
    return PickKnownAndReady(ctx, { WORD_OF_GLORY_PROT, WORD_OF_GLORY }) != 0;
}
void DoWordOfGlorySelf(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    uint32 id = PickKnownAndReady(ctx, { WORD_OF_GLORY_PROT, WORD_OF_GLORY });
    if (id) e.cast(id, ctx.bot.raw().guid);
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

// Divine Protection — 20% all-school DR. Mid-tier defensive that
// alternates with AD/GoAK so we always have *something* up under
// pressure.
bool ShouldDivineProtection(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DIVINE_PROTECTION)) return false;
    if (!ctx.bot.is_ready(DIVINE_PROTECTION)) return false;
    if (ctx.bot.has_aura(DIVINE_PROTECTION)) return false;
    return ctx.bot.hp_pct() <= 60;
}
void DoDivineProtection(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DIVINE_PROTECTION); }

// Aegis of Light — talent: channelled 65% DR for the bot + allies in a
// frontal cone. Heavy AoE damage save; gated on multiple wounded
// allies to avoid wasting a 5-min CD on solo damage.
bool ShouldAegisOfLight(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (PickKnownAndReady(ctx, { AEGIS_OF_LIGHT_A, AEGIS_OF_LIGHT_B }) == 0) return false;
    int wounded = 0;
    auto const* members = ctx.group.members();
    if (members) {
        for (auto const& m : *members) {
            if (!m.online || m.max_hp <= 0 || m.hp <= 0) continue;
            if ((m.hp * 100) / m.max_hp <= 60) ++wounded;
        }
    } else {
        wounded = ctx.bot.hp_pct() <= 60 ? 1 : 0;
    }
    return wounded >= 2 && ctx.bot.hp_pct() <= 70;
}
void DoAegisOfLight(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    uint32 id = PickKnownAndReady(ctx, { AEGIS_OF_LIGHT_A, AEGIS_OF_LIGHT_B });
    if (id) e.cast(id);
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
bool ShouldAuraMastery(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(AURA_MASTERY)) return false;
    if (!ctx.bot.is_ready(AURA_MASTERY)) return false;
    return BossLikeTargetEngaged(ctx) && ctx.bot.hp_pct() <= 70;
}
void DoAuraMastery(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(AURA_MASTERY); }

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
bool ShouldAvengingWrath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (PickKnownAndReady(ctx, { AVENGING_WRATH_MODERN, AVENGING_WRATH }) == 0) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.attackers_count() >= 3;
}
void DoAvengingWrath(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    uint32 id = PickKnownAndReady(ctx, { AVENGING_WRATH_MODERN, AVENGING_WRATH });
    if (id) e.cast(id);
}

bool ShouldSeraphim(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SERAPHIM)) return false;
    if (!ctx.bot.is_ready(SERAPHIM)) return false;
    return HolyPower(ctx) >= 3;
}
void DoSeraphim(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SERAPHIM); }

bool ShouldMomentOfGlory(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MOMENT_OF_GLORY)) return false;
    if (!ctx.bot.is_ready(MOMENT_OF_GLORY)) return false;
    return ctx.bot.attackers_count() >= 3;
}
void DoMomentOfGlory(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(MOMENT_OF_GLORY); }

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

bool ShouldEyeOfTyr(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(EYE_OF_TYR)) return false;
    if (!ctx.bot.is_ready(EYE_OF_TYR)) return false;
    return ctx.bot.enemies_within(8.0f) >= 2;
}
void DoEyeOfTyr(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(EYE_OF_TYR); }

bool ShouldHammerOfWrath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HAMMER_OF_WRATH)) return false;
    if (!ctx.bot.is_ready(HAMMER_OF_WRATH)) return false;
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

// Judgment: Prot spec variant 275779 -> unified 315867 -> baseline 20271.
bool ShouldJudgment(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    return PickKnownAndReady(ctx, { JUDGMENT_PROT, JUDGMENT_UNIFIED, JUDGMENT_BASELINE }) != 0;
}
void DoJudgment(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    uint32 id = PickKnownAndReady(ctx, { JUDGMENT_PROT, JUDGMENT_UNIFIED, JUDGMENT_BASELINE });
    if (id) e.cast(id, ctx.bot.victim());
}

// Consecration: Prot spec variant 327980 -> baseline 26573. Maintain
// the ground tick by checking the *baseline* aura — both variants
// apply the same player aura on cast (TC checks by spell-effect, not
// spell-ID, but we keep the simple baseline-aura check for safety).
bool ShouldConsecration(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (PickKnownAndReady(ctx, { CONSECRATION_PROT, CONSECRATION_BASELINE }) == 0) return false;
    return !ctx.bot.has_aura(CONSECRATION_PROT) && !ctx.bot.has_aura(CONSECRATION_BASELINE);
}
void DoConsecration(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    uint32 id = PickKnownAndReady(ctx, { CONSECRATION_PROT, CONSECRATION_BASELINE });
    if (id) e.cast(id);
}

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
    { ShouldKings,                 DoKings,                 "Blessing of Kings"         },
    { ShouldLayOnHands,            DoLayOnHands,            "Lay on Hands (<=15%)"      },
    { ShouldDivineShield,          DoDivineShield,          "Divine Shield (panic)"     },
    { ShouldWordOfGlorySelf,       DoWordOfGlorySelf,       "Word of Glory (self heal)" },
    { ShouldHandOfReckoning,       DoHandOfReckoning,       "Hand of Reckoning (taunt)" },
    { ShouldRebuke,                DoRebuke,                "Rebuke (interrupt)"        },
    { ShouldHammerOfJustice,       DoHammerOfJustice,       "Hammer of Justice (fb)"    },
    { ShouldArdentDefender,        DoArdentDefender,        "Ardent Defender (<=30%)"   },
    { ShouldGuardianOfAncientKings,DoGuardianOfAncientKings,"Guardian of Ancient Kings" },
    { ShouldDivineProtection,      DoDivineProtection,      "Divine Protection (<=60%)" },
    { ShouldAegisOfLight,          DoAegisOfLight,          "Aegis of Light"            },
    { ShouldAuraMastery,           DoAuraMastery,           "Aura Mastery"              },
    { ShouldBlessingOfSacrifice,   DoBlessingOfSacrifice,   "BoSac (low ally)"          },
    { ShouldBlessingOfProtection,  DoBlessingOfProtection,  "Blessing of Protection"    },
    { ShouldAvengingWrath,         DoAvengingWrath,         "Avenging Wrath"            },
    { ShouldSeraphim,              DoSeraphim,              "Seraphim"                  },
    { ShouldMomentOfGlory,         DoMomentOfGlory,         "Moment of Glory"           },
    { ShouldShieldOfTheRighteous,  DoShieldOfTheRighteous,  "Shield of the Righteous"   },
    { ShouldAvengersShield,        DoAvengersShield,        "Avenger's Shield"          },
    { ShouldJudgment,              DoJudgment,              "Judgment"                  },
    { ShouldEyeOfTyr,              DoEyeOfTyr,              "Eye of Tyr (2+ AoE)"       },
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
