// Balance Druid (Boomkin) - WoW 12.1.0.69587 (Midnight) spec rotation (specId 102).
//
// Stance / form
// -------------
// Balance is a CASTER-form spec. The bot lives in Moonkin Form for the
// damage / haste auras and never voluntarily drops it. The baseline druid
// file is cat-form-centric (any-druid leveling default); the moment a
// druid is in the Balance spec we override that by parking in Moonkin
// Form. We do NOT cast Cat Form / Bear Form here — those are emergency
// drops the user can trigger manually or via other rules elsewhere.
//
// Astral Power loop (12.1)
// ------------------------
// Eclipse is an ACTIVE button in Midnight (talent 1239669 teaches
// 1233346): pressing it enters Solar Eclipse (Wrath empowered) or Lunar
// Eclipse (Starfire empowered); the mode follows the last filler cast and
// both modes share the button + cooldown. Wrath / Starfire are the AP
// builders (Wrath single-target, Starfire cleave / Lunar). Starsurge is
// the instant 40-AP single-target spender; Starfall is the 50-AP ground
// AoE spender. Moonfire + Sunfire are instant DoTs blanketed across every
// reachable enemy via the BotSnapshotBuilder outbound enemy scan.
//
// Cooldowns
// ---------
// Celestial Alignment (talent 395022 -> castable 194223) and Incarnation:
// Chosen of Elune (talent 394013 -> castable 102560, replaces CA) are the
// burst windows - fire on boss-like targets or 3+ enemy AoE. Fury of
// Elune [R] is the 1min AoE beam; Heart of the Wild [R] in Moonkin Form
// is a burst of empowered falling stars; Force of Nature is a talent the
// curated build does not take (rule kept, knows_spell-gated).
//
// Survival ladder
// ---------------
// Barkskin (off-GCD 20% DR), Regrowth (hard-cast, drops Moonkin Form,
// deep panic only), Typhoon (knockback peel). Renewal no longer exists in
// 12.1. No bear-form bail in Balance - we trust the CDs + raid healer.
//
// Group utility
// -------------
// Rebirth (battle rez), Innervate (ally mana), Mark of the Wild (group
// buff, OOC only), Soothe (enrage dispel), Remove Corruption (Curse +
// Poison dispel, class talent [R]). CC: Solar Beam (interrupt + silence)
// is the primary kick; Mighty Bash + Typhoon are emergency tools;
// Hibernate / Cyclone / Entangling Roots are situational CC options we
// leave for the baseline / non-rotational logic.
//
// Validated spell IDs (SpellName.csv, WoW 12.1.0.69587)
// ----------------------------------------------------
//    24858  Moonkin Form            (class talent [R])
//   194153  Starfire                (class talent [R])
//   190984  Wrath                   (Balance spec override of baseline 5176)
//    78674  Starsurge               (class talent [R], 40 AP)
//   191034  Starfall                (spec spell L15, 50 AP)
//     8921  Moonfire                (cast id; DoT aura is 164812)
//    93402  Sunfire                 (class talent [R]; DoT aura is 164815)
//   274281  New Moon                (spec talent, not in build; the button
//                                    morphs into Half / Full Moon - 274282 /
//                                    274283 are no longer learnable ids)
//   205636  Force of Nature         (spec talent, not in build)
//   202770  Fury of Elune           (spec talent [R])
//  1233346  Solar Eclipse           (Eclipse button, taught by talent 1239669;
//                                    1233272 Lunar Eclipse is the flipped face)
//    48517  Eclipse (Solar)         (aura while Wrath is empowered)
//    48518  Eclipse (Lunar)         (aura while Starfire is empowered)
//   194223  Celestial Alignment     (castable; talent id 395022 teaches it)
//   102560  Incarnation: Chosen of Elune (castable; talent id 394013 teaches it)
//  1261867  Heart of the Wild       (class talent [R]; Moonkin = empowered Starfall)
//    78675  Solar Beam              (spec talent [R]; interrupt + silence)
//     5211  Mighty Bash             (class talent, not in build)
//   132469  Typhoon                 (class talent [R])
//    20484  Rebirth                 (battle rez)
//    22812  Barkskin                (off-GCD 20% DR)
//    29166  Innervate               (class talent [R])
//     2908  Soothe                  (class talent [R])
//     2782  Remove Corruption       (class talent [R]; Curse + Poison)
//     8936  Regrowth                (hard-cast self-heal)
//     1126  Mark of the Wild        (group buff)
//
// Skipped spells (and why)
// ------------------------
//   * 202347  Stellar Flare, 202425 Warrior of Elune, 124974 Nature's
//     Vigil, 108238 Renewal - not learnable by any druid in 12.1 (removed
//     or reworked into passives); rules deleted.
//   * 274282 / 274283  Half Moon / Full Moon - no longer separate learnable
//     spells; New Moon's button morphs. Only 274281 is cast.
//   * 391528  Convoke the Spirits, 88747 Wild Mushroom - spec talents the
//     curated Balance builds do not take.
//   * 22842 Frenzied Regeneration, 106898 Stampeding Roar, 102401 Wild
//     Charge, 102793 Ursol's Vortex - [R] class talents that need Bear Form
//     or ground/ally positioning the caster rotation does not do.
//   * 1229376 Single-Button Assistant - client convenience macro.
//   * 197911  Astral Power - power resource, read via power(POWER_LUNAR_POWER_IDX).

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated against SpellName.csv) ----
constexpr uint32 MOONKIN_FORM      = 24858;      // class talent [R]
constexpr uint32 STARFIRE          = 194153;     // class talent [R]
constexpr uint32 WRATH             = 190984;     // Balance spec override of 5176
constexpr uint32 STARSURGE         = 78674;      // class talent [R] - 40 AP spender
constexpr uint32 STARFALL          = 191034;     // spec spell L15 - 50 AP AoE spender
constexpr uint32 MOONFIRE          = 8921;       // cast id
constexpr uint32 MOONFIRE_DOT      = 164812;     // periodic aura applied by 8921
constexpr uint32 SUNFIRE           = 93402;      // class talent [R]; cast id
constexpr uint32 SUNFIRE_DOT       = 164815;     // periodic aura applied by 93402
constexpr uint32 NEW_MOON          = 274281;     // spec talent (not in build); button morphs to Half/Full Moon
constexpr uint32 FORCE_OF_NATURE   = 205636;     // spec talent (not in build) - treant adds
constexpr uint32 FURY_OF_ELUNE     = 202770;     // spec talent [R] - ground AoE beam
constexpr uint32 ECLIPSE           = 1233346;    // Solar Eclipse button (taught by talent 1239669)
constexpr uint32 ECLIPSE_TALENT    = 1239669;    // Eclipse talent [R] (teaches 1233346)
constexpr uint32 ECLIPSE_LUNAR     = 48518;      // Lunar Eclipse aura (Starfire empowered)
constexpr uint32 ECLIPSE_SOLAR     = 48517;      // Solar Eclipse aura (Wrath empowered)
constexpr uint32 CELESTIAL_ALIGN   = 194223;     // castable CA (taught by talent 395022)
constexpr uint32 CELESTIAL_ALIGN_TALENT = 395022; // Celestial Alignment talent [R]
constexpr uint32 INCARNATION_CHOSEN_OF_ELUNE = 102560; // castable (taught by talent 394013)
constexpr uint32 INCARNATION_TALENT = 394013;    // Incarnation: Chosen of Elune talent [R]
constexpr uint32 HEART_OF_THE_WILD = 1261867;    // class talent [R] - Moonkin: empowered Starfall
constexpr uint32 SOLAR_BEAM        = 78675;      // spec talent [R] - interrupt + silence
constexpr uint32 MIGHTY_BASH       = 5211;       // class talent (not in build) - stun
constexpr uint32 TYPHOON           = 132469;     // class talent [R] - knockback
constexpr uint32 REBIRTH           = 20484;      // battle resurrection
constexpr uint32 BARKSKIN          = 22812;      // 20% DR, 8s, 1min CD
constexpr uint32 INNERVATE         = 29166;      // class talent [R] - ally mana
constexpr uint32 SOOTHE            = 2908;       // class talent [R] - enrage dispel
constexpr uint32 REMOVE_CORRUPTION = 2782;       // class talent [R] - Curse + Poison dispel
constexpr uint32 REGROWTH          = 8936;       // self heal (drops Moonkin Form)
constexpr uint32 MARK_OF_THE_WILD  = 1126;       // group buff

// Astral Power lives at POWER_LUNAR_POWER (8) in the WoW 12.0 power array.
constexpr uint8 POWER_LUNAR_POWER_IDX = 8;

// ---- Helpers ----
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

uint8 NearbyEnemiesInRange(ApPredicateContext const& ctx, float range)
{
    return static_cast<uint8>(ctx.bot.enemies_within(range));
}

bool CanShapeshiftNow(ApPredicateContext const& ctx)
{
    auto const& mv = ctx.bot.raw().movement;
    return !mv.is_mounted && !mv.is_flying;
}

// Several 12.1 talents are exposed as a talent spell that teaches the
// classic castable (kit: "teaches"). Depending on how the talent build was
// applied the bot may know either id, so pick whichever is in the spellbook
// - classic castable first, talent id as fallback - and gate is_ready() on
// that id (is_ready folds knows_spell). Returns 0 when neither is known.
uint32 KnownId(ApPredicateContext const& ctx, uint32 castable, uint32 talent)
{
    if (ctx.bot.knows_spell(castable)) return castable;
    if (ctx.bot.knows_spell(talent))   return talent;
    return 0;
}

// ---- Stance / buffs ----
// Moonkin Form is the spec's home stance — we enter it any time the aura
// is missing and we're not mounted/flying. The baseline druid file would
// otherwise drop us into Cat Form; this rule sits high enough that on a
// Balance bot the Moonkin re-entry beats the baseline's Cat Form rule
// (different rule tables, but the spec table runs first by design).
bool ShouldMoonkinForm(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(MOONKIN_FORM)) return false;
    if (ctx.bot.has_aura(MOONKIN_FORM)) return false;
    if (!CanShapeshiftNow(ctx)) return false;
    return true;
}
void DoMoonkinForm(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(MOONKIN_FORM); }

bool ShouldMarkOfTheWild(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(MARK_OF_THE_WILD)) return false;
    if (ctx.bot.in_combat()) return false;
    return !ctx.bot.has_aura(MARK_OF_THE_WILD);
}
void DoMarkOfTheWild(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MARK_OF_THE_WILD, ctx.bot.raw().guid);
}

// ---- Survival ----
bool ShouldBarkskin(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(BARKSKIN)) return false;
    if (!ctx.bot.is_ready(BARKSKIN)) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoBarkskin(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BARKSKIN); }

// Regrowth is a 1.5s hard-cast that DROPS Moonkin Form. Only fire at the
// deep-panic threshold (<=30%) and only when Barkskin is on CD -
// otherwise the lost Moonkin aura window costs more damage than the heal
// saves. Barkskin (above) runs first.
bool ShouldRegrowth(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(REGROWTH)) return false;
    if (ctx.bot.hp_pct() > 30) return false;
    // If the instant DR is up, prefer it.
    if (ctx.bot.knows_spell(BARKSKIN) && ctx.bot.is_ready(BARKSKIN)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(REGROWTH)) return false;
    return true;
}
void DoRegrowth(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(REGROWTH, ctx.bot.raw().guid);
}

// ---- Group utility ----
bool ShouldRebirth(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(REBIRTH)) return false;
    if (!ctx.bot.is_ready(REBIRTH)) return false;
    return ctx.group.dead_member_priority(ctx.bot.map_id()) != nullptr;
}
void DoRebirth(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* m = ctx.group.dead_member_priority(ctx.bot.map_id()))
        e.cast(REBIRTH, m->guid);
}

bool ShouldInnervate(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(INNERVATE)) return false;
    if (!ctx.bot.is_ready(INNERVATE)) return false;
    auto const* m = ctx.group.lowest_mana_caster();
    if (!m || !m->online || m->hp <= 0) return false;
    if (m->guid == ctx.bot.raw().guid) return false;
    return m->max_mana > 0 && (m->mana * 100) / m->max_mana <= 30;
}
void DoInnervate(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* m = ctx.group.lowest_mana_caster())
        e.cast(INNERVATE, m->guid);
}

bool ShouldSoothe(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SOOTHE)) return false;
    if (!ctx.bot.is_ready(SOOTHE)) return false;
    return ctx.bot.target_dispellable(Playerbot::DispelType::Enrage);
}
void DoSoothe(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SOOTHE, ctx.bot.victim());
}

// Remove Corruption: Curse + Poison dispel (class talent [R]). Castable in
// Moonkin Form. Group member first, self as fallback.
bool ShouldRemoveCorruption(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(REMOVE_CORRUPTION)) return false;
    if (!ctx.bot.is_ready(REMOVE_CORRUPTION)) return false;
    if (ctx.group.dispel_candidate(Playerbot::DispelType::Curse)  != nullptr) return true;
    if (ctx.group.dispel_candidate(Playerbot::DispelType::Poison) != nullptr) return true;
    return ctx.bot.self_dispellable(Playerbot::DispelType::Curse)
        || ctx.bot.self_dispellable(Playerbot::DispelType::Poison);
}
void DoRemoveCorruption(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* m = ctx.group.dispel_candidate(Playerbot::DispelType::Curse))
    { e.cast(REMOVE_CORRUPTION, m->guid); return; }
    if (auto const* m = ctx.group.dispel_candidate(Playerbot::DispelType::Poison))
    { e.cast(REMOVE_CORRUPTION, m->guid); return; }
    e.cast(REMOVE_CORRUPTION, ctx.bot.raw().guid);
}

// ---- Interrupt / CC ----
// Interrupt-aware gates: every Druid interrupt should check that the
// victim is actually casting an interruptible spell (saves CD for caster
// mobs instead of burning on melee trash). `kick_target()` already
// returns nullptr unless the candidate is mid-cast on an interruptible.
bool ShouldSolarBeam(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SOLAR_BEAM)) return false;
    if (!ctx.bot.is_ready(SOLAR_BEAM)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    auto const* c = ctx.bot.kick_target(pvp, 40.0f);
    return c && !c->guid.IsEmpty();
}
void DoSolarBeam(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 40.0f))
        e.cast_at(SOLAR_BEAM, c->x, c->y, c->z);
}

bool ShouldMightyBash(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MIGHTY_BASH)) return false;
    if (!ctx.bot.is_ready(MIGHTY_BASH)) return false;
    // Only fire when Solar Beam isn't a better option (Solar Beam is the
    // primary kick) AND a caster is actually interruptible nearby.
    if (ctx.bot.is_ready(SOLAR_BEAM) && ctx.bot.knows_spell(SOLAR_BEAM)) return false;
    return ctx.bot.interruptible_caster() != nullptr
        || ctx.bot.enemies_within(8.0f) >= 1;
}
void DoMightyBash(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* c = ctx.bot.interruptible_caster())
    { e.cast(MIGHTY_BASH, c->guid); return; }
    e.cast(MIGHTY_BASH, ctx.bot.victim());
}

bool ShouldTyphoon(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(TYPHOON)) return false;
    if (!ctx.bot.is_ready(TYPHOON)) return false;
    // Knockback to peel melee — at least 2 attackers in our face and we're
    // taking pressure.
    return ctx.bot.enemies_within(15.0f) >= 2 && ctx.bot.hp_pct() <= 50;
}
void DoTyphoon(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TYPHOON); }

// ---- Major offensive cooldowns ----
// Incarnation: Chosen of Elune replaces Celestial Alignment when talented;
// both are exposed as talent-teaches-castable pairs (see KnownId).
bool ShouldIncarnation(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    const uint32 id = KnownId(ctx, INCARNATION_CHOSEN_OF_ELUNE, INCARNATION_TALENT);
    if (!id || !ctx.bot.is_ready(id)) return false;
    return BossLikeTargetEngaged(ctx) || NearbyEnemiesInRange(ctx, 40.0f) >= 3;
}
void DoIncarnation(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (const uint32 id = KnownId(ctx, INCARNATION_CHOSEN_OF_ELUNE, INCARNATION_TALENT))
        e.cast(id);
}

bool ShouldCelestialAlign(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    // Incarnation owns the burst slot when it is talented.
    if (KnownId(ctx, INCARNATION_CHOSEN_OF_ELUNE, INCARNATION_TALENT)) return false;
    const uint32 id = KnownId(ctx, CELESTIAL_ALIGN, CELESTIAL_ALIGN_TALENT);
    if (!id || !ctx.bot.is_ready(id)) return false;
    return BossLikeTargetEngaged(ctx) || NearbyEnemiesInRange(ctx, 40.0f) >= 3;
}
void DoCelestialAlign(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (const uint32 id = KnownId(ctx, CELESTIAL_ALIGN, CELESTIAL_ALIGN_TALENT))
        e.cast(id);
}

// ---- Eclipse (12.1: an active button) ----
// The Eclipse talent turns Eclipse into a cast: the button enters Solar
// Eclipse (empowers Wrath) or Lunar Eclipse (empowers Starfire); the mode
// follows the last filler cast (Wrath -> Solar, Starfire -> Lunar) and both
// modes share the button and its cooldown. Press it whenever no Eclipse
// (or CA / Incarnation, which grant both) is active, after letting one
// filler arm the mode that fits the fight shape: Lunar on 2+ targets,
// Solar single-target.
bool ShouldEclipse(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    const uint32 id = KnownId(ctx, ECLIPSE, ECLIPSE_TALENT);
    if (!id || !ctx.bot.is_ready(id)) return false;
    if (ctx.bot.has_aura(ECLIPSE_SOLAR) || ctx.bot.has_aura(ECLIPSE_LUNAR)) return false;
    if (ctx.bot.has_aura(CELESTIAL_ALIGN) || ctx.bot.has_aura(INCARNATION_CHOSEN_OF_ELUNE)) return false;
    const bool aoe = ctx.aoe_preference || NearbyEnemiesInRange(ctx, 40.0f) >= 2;
    const uint32 last = ctx.bot.last_cast_spell_id();
    // Wrong mode armed for the fight shape - let one filler flip it first.
    if (aoe && last == WRATH) return false;
    if (!aoe && last == STARFIRE) return false;
    return true;
}
void DoEclipse(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (const uint32 id = KnownId(ctx, ECLIPSE, ECLIPSE_TALENT))
        e.cast(id);
}

// Heart of the Wild in Moonkin Form = a burst of empowered falling stars
// around the bot (2min CD). Treat it as an AoE / boss cooldown next to
// Fury of Elune; self-targeted since the Moonkin variant is caster-centred.
bool ShouldHeartOfTheWild(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HEART_OF_THE_WILD)) return false;
    if (!ctx.bot.is_ready(HEART_OF_THE_WILD)) return false;
    if (!ctx.bot.has_aura(MOONKIN_FORM)) return false;
    return BossLikeTargetEngaged(ctx) || NearbyEnemiesInRange(ctx, 40.0f) >= 2;
}
void DoHeartOfTheWild(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HEART_OF_THE_WILD, ctx.bot.raw().guid);
}

bool ShouldForceOfNature(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FORCE_OF_NATURE)) return false;
    if (!ctx.bot.is_ready(FORCE_OF_NATURE)) return false;
    return true;
}
void DoForceOfNature(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(FORCE_OF_NATURE, v->x, v->y, v->z);
    else
        e.cast(FORCE_OF_NATURE, ctx.bot.victim());
}

bool ShouldFuryOfElune(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FURY_OF_ELUNE)) return false;
    if (!ctx.bot.is_ready(FURY_OF_ELUNE)) return false;
    return true;
}
void DoFuryOfElune(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(FURY_OF_ELUNE, v->x, v->y, v->z);
    else
        e.cast(FURY_OF_ELUNE, ctx.bot.victim());
}

// ---- AoE ----
bool ShouldStarfall(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(STARFALL)) return false;
    if (!ctx.bot.is_ready(STARFALL)) return false;
    if (ctx.bot.power(POWER_LUNAR_POWER_IDX) < 50) return false;
    // 2+ targets makes it more total damage than Starsurge.
    return ctx.aoe_preference || NearbyEnemiesInRange(ctx, 40.0f) >= 2;
}
void DoStarfall(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(STARFALL); }

// ---- DoTs ----
// Moonfire / Sunfire casts apply separate periodic auras (164812 / 164815),
// so refresh and multi-dot checks look for the DoT ids, not the cast ids.
bool ShouldMoonfirePrimary(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MOONFIRE)) return false;
    AuraEntry const* a = ctx.bot.find_aura(MOONFIRE_DOT, ctx.bot.victim());
    return !a || a->remaining.count() <= 4000;
}
void DoMoonfirePrimary(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MOONFIRE, ctx.bot.victim());
}

bool ShouldSunfirePrimary(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SUNFIRE)) return false;
    AuraEntry const* a = ctx.bot.find_aura(SUNFIRE_DOT, ctx.bot.victim());
    return !a || a->remaining.count() <= 4000;
}
void DoSunfirePrimary(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SUNFIRE, ctx.bot.victim());
}

bool ShouldMoonfireExpand(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(MOONFIRE)) return false;
    return ctx.bot.enemy_without_my_aura(MOONFIRE_DOT, 40.0f) != nullptr;
}
void DoMoonfireExpand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* off = ctx.bot.enemy_without_my_aura(MOONFIRE_DOT, 40.0f))
        e.cast(MOONFIRE, off->guid);
}

bool ShouldSunfireExpand(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SUNFIRE)) return false;
    return ctx.bot.enemy_without_my_aura(SUNFIRE_DOT, 40.0f) != nullptr;
}
void DoSunfireExpand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* off = ctx.bot.enemy_without_my_aura(SUNFIRE_DOT, 40.0f))
        e.cast(SUNFIRE, off->guid);
}

// ---- New Moon (the button morphs into Half / Full Moon in 12.1) ----
bool ShouldNewMoon(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(NEW_MOON)) return false;
    return ctx.bot.is_ready(NEW_MOON);
}
void DoNewMoon(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(NEW_MOON, ctx.bot.victim()); }

// ---- AP spender ----
bool ShouldStarsurge(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(STARSURGE)) return false;
    if (!ctx.bot.is_ready(STARSURGE)) return false;
    if (ctx.bot.power(POWER_LUNAR_POWER_IDX) < 40) return false;
    // Instant in 12.1; the moving guard stays harmless if build data differs.
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(STARSURGE)) return false;
    // Single-target spender — defer to Starfall when 2+ enemies.
    return NearbyEnemiesInRange(ctx, 40.0f) <= 1 || BossLikeTargetEngaged(ctx);
}
void DoStarsurge(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(STARSURGE, ctx.bot.victim());
}

// ---- Eclipse fillers ----
// Starfire is the heavier filler - preferred in Lunar Eclipse (cleave
// bonus on secondary targets) and on 2+ enemy AoE. Wrath is the lighter
// filler - preferred in Solar Eclipse and on single-target sustained
// damage. In 12.1 the last filler cast also arms which mode the Eclipse
// button enters (see ShouldEclipse), so this split doubles as the mode
// selector.
bool ShouldStarfire(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(STARFIRE)) return false;
    if (!ctx.bot.is_ready(STARFIRE)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(STARFIRE)) return false;
    if (ctx.bot.has_aura(ECLIPSE_LUNAR)) return true;
    if (NearbyEnemiesInRange(ctx, 40.0f) >= 2) return true;
    // Otherwise, build toward Solar Eclipse via Wrath. Only fall through
    // to Starfire if Wrath isn't known (very low level / no spec).
    return !ctx.bot.knows_spell(WRATH);
}
void DoStarfire(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(STARFIRE, ctx.bot.victim()); }

bool ShouldWrath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(WRATH)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(WRATH)) return false;
    return true;
}
void DoWrath(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(WRATH, ctx.bot.victim()); }

bool AlwaysAlive(ApPredicateContext const& ctx) { return ctx.bot.is_alive(); }
void DoNothing(ApPredicateContext const&, BotIntentEmitter&) {}

// ---- Rule table (priority order top-down) ----
// Order rationale:
//   1.  Rebirth                - battle-rez has highest impact in group.
//   2.  Barkskin               - off-GCD survival, fire on damage spikes.
//   3.  Regrowth               - hard-cast panic only (DROPS Moonkin
//                                briefly) when Barkskin is down.
//   4.  Typhoon                - peel knockback when surrounded.
//   5.  Moonkin Form           - spec stance entry, sits above DPS rules
//                                so a stripped/dispelled form re-applies
//                                before any nuke fires.
//   6.  Mark of the Wild       - OOC group buff maintenance.
//   7.  Solar Beam             - primary interrupt (AoE silence too).
//   8.  Mighty Bash            - interrupt fallback when Solar Beam down.
//   9.  Soothe                 - enrage dispel on target.
//   10. Remove Corruption      - Curse / Poison dispel on group or self.
//   11. Innervate              - ally caster mana cooldown.
//   12. Incarnation            - major CD (replaces CA when talented).
//   13. Celestial Alignment    - major CD when Incarnation not talented.
//   14. Eclipse                - active Eclipse button (12.1) whenever no
//                                Eclipse / CA / Incarnation is up.
//   15. Heart of the Wild      - empowered Starfall burst (2+ / boss).
//   16. Force of Nature        - treant adds (talent, not in build).
//   17. Fury of Elune          - ground AoE beam (talent).
//   18. Starfall               - 50-AP AoE spender (2+ targets).
//   19. Sunfire primary        - instant DoT on victim.
//   20. Moonfire primary       - instant DoT on victim.
//   21. Sunfire expand         - blanket DoT on off-target enemies.
//   22. Moonfire expand        - blanket DoT on off-target enemies.
//   23. New Moon               - talent nuke (button morphs Half / Full).
//   24. Starsurge              - 40-AP ST spender.
//   25. Starfire               - heavier filler (Lunar / AoE).
//   26. Wrath                  - lighter filler (Solar build).
//   27. Idle                   - alive fallthrough.
ApRule const kRules[] = {
    { ShouldRebirth,          DoRebirth,          "Rebirth (battle rez)"       },
    { ShouldBarkskin,         DoBarkskin,         "Barkskin (<=50%)"           },
    { ShouldRegrowth,         DoRegrowth,         "Regrowth (<=30% deep panic)"},
    { ShouldTyphoon,          DoTyphoon,          "Typhoon (peel)"             },
    { ShouldMoonkinForm,      DoMoonkinForm,      "Moonkin Form"               },
    { ShouldMarkOfTheWild,    DoMarkOfTheWild,    "Mark of the Wild"           },
    { ShouldSolarBeam,        DoSolarBeam,        "Solar Beam (interrupt)"     },
    { ShouldMightyBash,       DoMightyBash,       "Mighty Bash (interrupt fb)" },
    { ShouldSoothe,           DoSoothe,           "Soothe (enrage)"            },
    { ShouldRemoveCorruption, DoRemoveCorruption, "Remove Corruption (dispel)" },
    { ShouldInnervate,        DoInnervate,        "Innervate (healer mana)"    },
    { ShouldIncarnation,      DoIncarnation,      "Incarnation"                },
    { ShouldCelestialAlign,   DoCelestialAlign,   "Celestial Alignment"        },
    { ShouldEclipse,          DoEclipse,          "Eclipse (enter)"            },
    { ShouldHeartOfTheWild,   DoHeartOfTheWild,   "Heart of the Wild (stars)"  },
    { ShouldForceOfNature,    DoForceOfNature,    "Force of Nature (treants)"  },
    { ShouldFuryOfElune,      DoFuryOfElune,      "Fury of Elune"              },
    { ShouldStarfall,         DoStarfall,         "Starfall (2+ AoE)"          },
    { ShouldSunfirePrimary,   DoSunfirePrimary,   "Sunfire (primary)"          },
    { ShouldMoonfirePrimary,  DoMoonfirePrimary,  "Moonfire (primary)"         },
    { ShouldSunfireExpand,    DoSunfireExpand,    "Sunfire (expand off-tgt)"   },
    { ShouldMoonfireExpand,   DoMoonfireExpand,   "Moonfire (expand off-tgt)"  },
    { ShouldNewMoon,          DoNewMoon,          "New Moon"                   },
    { ShouldStarsurge,        DoStarsurge,        "Starsurge (spend)"          },
    { ShouldStarfire,         DoStarfire,         "Starfire (lunar/cleave)"    },
    { ShouldWrath,            DoWrath,            "Wrath (solar build)"        },
    { AlwaysAlive,            DoNothing,          "Idle"                       },
};

} // anonymous

void RegisterApl_Druid_Balance()
{
    constexpr uint32 SPEC_DRUID_BALANCE = 102;
    RegisterRotation(CLASS_DRUID, SPEC_DRUID_BALANCE, ApRotation{kRules});
}

} // namespace Playerbot::Combat
