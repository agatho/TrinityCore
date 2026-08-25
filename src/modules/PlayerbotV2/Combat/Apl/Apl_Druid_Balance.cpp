// Balance Druid (Boomkin) — WoW 12.0 spec rotation (specId 102).
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
// Astral Power loop
// -----------------
// Wrath (filler) builds AP toward the Solar Eclipse entry; Starfire
// (heavier filler) builds AP toward the Lunar Eclipse entry. Two
// consecutive casts of the same nuke triggers the opposing Eclipse buff
// (Lunar after 2 Wraths, Solar after 2 Starfires). Starsurge is the
// hard-cast 30-AP single-target spender; Starfall is the 50-AP ground
// AoE spender. Moonfire + Sunfire are instant DoTs that should be
// blanketed across every reachable enemy via the BotSnapshotBuilder
// outbound enemy scan.
//
// Cooldowns
// ---------
// Celestial Alignment (3min) and Incarnation: Chosen of Elune (3min,
// talent override) are the burst windows — fire on boss-like targets
// or 3+ enemy AoE. Warrior of Elune grants 3 instant Starfires.
// Force of Nature + Fury of Elune are talent-only AoE pressure. Nature's
// Vigil is a heal+damage smear used on boss-like engagements.
//
// Survival ladder
// ---------------
// Barkskin (off-GCD 20% DR), Renewal (instant 30% self-heal talent),
// Regrowth (hard-cast, mana cost), Typhoon (knockback peel). No bear-
// form bail in Balance — we trust the survival CDs + raid healer.
//
// Group utility
// -------------
// Rebirth (battle rez), Innervate (ally mana), Mark of the Wild (group
// buff, OOC only), Soothe (enrage dispel). CC: Solar Beam (interrupt +
// 8s silence) is the primary kick; Mighty Bash + Typhoon are talented
// emergency tools; Hibernate / Cyclone / Entangling Roots are situational
// CC options we leave for the baseline / non-rotational logic.
//
// Validated spell IDs (SpellName.csv, WoW 12.0 client 11.x+ data)
// ---------------------------------------------------------------
//    24858  Moonkin Form
//   194153  Starfire
//   190984  Wrath                  (Balance learns the buffed 190984; baseline druid uses 5176)
//    78674  Starsurge
//   191034  Starfall
//     8921  Moonfire               (DoT cast / aura — Balance's modern damage spell variant is
//                                   326646, but the LEARNED spell is still 8921 in this build;
//                                   326646 is the periodic-damage SpellEffect handle and not
//                                   used directly by predicates)
//    93402  Sunfire
//   274281/274282/274283  New / Half / Full Moon (talent chain)
//   205636  Force of Nature        (talent)
//   202770  Fury of Elune          (talent)
//   202347  Stellar Flare          (talent)
//   202425  Warrior of Elune       (talent)
//    48518  Eclipse (Lunar)        (buff after 2 Wraths)
//    48517  Eclipse (Solar)        (buff after 2 Starfires)
//   194223  Celestial Alignment    (3min burst)
//   102560  Incarnation: Chosen of Elune
//    78675  Solar Beam             (interrupt+silence)
//     5211  Mighty Bash            (talent stun)
//   132469  Typhoon                (talent knockback)
//      339  Entangling Roots       (root CC)
//     2637  Hibernate              (beast/dragonkin CC)
//    33786  Cyclone                (banish-style CC)
//    20484  Rebirth                (battle rez)
//    22812  Barkskin                (off-GCD 20% DR)
//   108238  Renewal                (talent instant self-heal)
//    29166  Innervate              (ally mana cooldown)
//     2908  Soothe                 (enrage dispel)
//     8936  Regrowth               (hard-cast self-heal)
//     1126  Mark of the Wild       (group buff)
//   124974  Nature's Vigil         (talent heal+dmg smear)
//
// Skipped spells (and why)
// ------------------------
//   * 157228 / 231042  Owlkin Frenzy  — passive proc aura (gives Starfire
//     instant + damage), not directly cast. The Eclipse / Starfire rules
//     benefit from it implicitly when Starfire becomes instant.
//   * 197911  Astral Power  — power resource, not a cast. We read it via
//     ctx.bot.power(POWER_LUNAR_POWER_IDX).
//   * 197490  Feral Affinity  — passive talent that grants Rake/Shred/
//     Rip; Balance spec doesn't run a cat-form sub-rotation, so we skip
//     these spells. If the user wants cat damage they should re-spec.
//   * 405834  Improved Prowl  — Feral talent, not available to Balance.
//   * 326646  Moonfire (variant damage spell)  — applied automatically by
//     SpellMgr off the 8921 cast; not a separate predicate.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated against SpellName.csv) ----
constexpr uint32 MOONKIN_FORM      = 24858;
constexpr uint32 STARFIRE          = 194153;
constexpr uint32 WRATH             = 190984;
constexpr uint32 STARSURGE         = 78674;
constexpr uint32 STARFALL          = 191034;
constexpr uint32 MOONFIRE          = 8921;
constexpr uint32 SUNFIRE           = 93402;
constexpr uint32 NEW_MOON          = 274281;     // talent
constexpr uint32 HALF_MOON         = 274282;     // chained off New Moon
constexpr uint32 FULL_MOON         = 274283;     // chained off Half Moon
constexpr uint32 FORCE_OF_NATURE   = 205636;     // talent — treant adds
constexpr uint32 FURY_OF_ELUNE     = 202770;     // talent — ground AoE channel
constexpr uint32 STELLAR_FLARE     = 202347;     // talent — DoT
constexpr uint32 WARRIOR_OF_ELUNE  = 202425;     // talent — 3 instant Starfires
constexpr uint32 ECLIPSE_LUNAR     = 48518;      // buff after 2 Wraths
constexpr uint32 ECLIPSE_SOLAR     = 48517;      // buff after 2 Starfires
constexpr uint32 CELESTIAL_ALIGN   = 194223;     // 3min CD burst
constexpr uint32 INCARNATION_CHOSEN_OF_ELUNE = 102560; // talent replacement
constexpr uint32 SOLAR_BEAM        = 78675;      // interrupt + silence
constexpr uint32 MIGHTY_BASH       = 5211;       // talent stun
constexpr uint32 TYPHOON           = 132469;     // talent knockback
constexpr uint32 REBIRTH           = 20484;      // battle resurrection
constexpr uint32 BARKSKIN          = 22812;      // 20% DR, 8s, 1min CD
constexpr uint32 RENEWAL           = 108238;     // talent — instant 30% heal
constexpr uint32 INNERVATE         = 29166;      // mana cooldown for ally
constexpr uint32 SOOTHE            = 2908;       // enrage dispel
constexpr uint32 REGROWTH          = 8936;       // self heal
constexpr uint32 MARK_OF_THE_WILD  = 1126;       // group buff
constexpr uint32 NATURES_VIGIL     = 124974;     // talent — heal+dmg buff

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

bool ShouldRenewal(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(RENEWAL)) return false;
    if (!ctx.bot.is_ready(RENEWAL)) return false;
    return ctx.bot.hp_pct() <= 40;
}
void DoRenewal(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(RENEWAL); }

// Regrowth is a 1.5s hard-cast that DROPS Moonkin Form. Only fire at the
// deep-panic threshold (<=30%) and only when other instants are on CD —
// otherwise the lost Moonkin aura window costs more damage than the heal
// saves. Renewal (rule above) and Barkskin (above) run first.
bool ShouldRegrowth(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(REGROWTH)) return false;
    if (ctx.bot.hp_pct() > 30) return false;
    // If a faster panic option is up, prefer it.
    if (ctx.bot.knows_spell(RENEWAL) && ctx.bot.is_ready(RENEWAL)) return false;
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
bool ShouldIncarnation(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(INCARNATION_CHOSEN_OF_ELUNE)) return false;
    if (!ctx.bot.is_ready(INCARNATION_CHOSEN_OF_ELUNE)) return false;
    return BossLikeTargetEngaged(ctx) || NearbyEnemiesInRange(ctx, 40.0f) >= 3;
}
void DoIncarnation(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(INCARNATION_CHOSEN_OF_ELUNE); }

bool ShouldCelestialAlign(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CELESTIAL_ALIGN)) return false;
    if (!ctx.bot.is_ready(CELESTIAL_ALIGN)) return false;
    // Incarnation replaces Celestial Alignment via talent — they share a
    // CD only one will be known at any given time.
    return BossLikeTargetEngaged(ctx) || NearbyEnemiesInRange(ctx, 40.0f) >= 3;
}
void DoCelestialAlign(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CELESTIAL_ALIGN); }

bool ShouldNaturesVigil(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(NATURES_VIGIL)) return false;
    if (!ctx.bot.is_ready(NATURES_VIGIL)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoNaturesVigil(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(NATURES_VIGIL); }

bool ShouldWarriorOfElune(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(WARRIOR_OF_ELUNE)) return false;
    if (!ctx.bot.is_ready(WARRIOR_OF_ELUNE)) return false;
    if (ctx.bot.has_aura(WARRIOR_OF_ELUNE)) return false;
    return true;
}
void DoWarriorOfElune(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(WARRIOR_OF_ELUNE); }

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
bool ShouldStellarFlare(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(STELLAR_FLARE)) return false;
    AuraEntry const* a = ctx.bot.find_aura(STELLAR_FLARE, ctx.bot.victim());
    return !a || a->remaining.count() <= 4000;
}
void DoStellarFlare(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(STELLAR_FLARE, ctx.bot.victim());
}

bool ShouldMoonfirePrimary(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MOONFIRE)) return false;
    AuraEntry const* a = ctx.bot.find_aura(MOONFIRE, ctx.bot.victim());
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
    AuraEntry const* a = ctx.bot.find_aura(SUNFIRE, ctx.bot.victim());
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
    return ctx.bot.enemy_without_my_aura(MOONFIRE, 40.0f) != nullptr;
}
void DoMoonfireExpand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* off = ctx.bot.enemy_without_my_aura(MOONFIRE, 40.0f))
        e.cast(MOONFIRE, off->guid);
}

bool ShouldSunfireExpand(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SUNFIRE)) return false;
    return ctx.bot.enemy_without_my_aura(SUNFIRE, 40.0f) != nullptr;
}
void DoSunfireExpand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* off = ctx.bot.enemy_without_my_aura(SUNFIRE, 40.0f))
        e.cast(SUNFIRE, off->guid);
}

// ---- New Moon / Half Moon / Full Moon chain ----
bool ShouldFullMoon(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FULL_MOON)) return false;
    return ctx.bot.is_ready(FULL_MOON);
}
void DoFullMoon(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(FULL_MOON, ctx.bot.victim()); }

bool ShouldHalfMoon(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HALF_MOON)) return false;
    return ctx.bot.is_ready(HALF_MOON);
}
void DoHalfMoon(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(HALF_MOON, ctx.bot.victim()); }

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
    if (ctx.bot.power(POWER_LUNAR_POWER_IDX) < 30) return false;
    // Hard-cast spender — don't waste 30 AP on a moving cast that fails.
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(STARSURGE)) return false;
    // Single-target spender — defer to Starfall when 2+ enemies.
    return NearbyEnemiesInRange(ctx, 40.0f) <= 1 || BossLikeTargetEngaged(ctx);
}
void DoStarsurge(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(STARSURGE, ctx.bot.victim());
}

// ---- Eclipse fillers ----
// Starfire is the heavier filler — preferred in Lunar Eclipse (cleave
// bonus on secondary targets) and on 2+ enemy AoE. Wrath is the lighter
// filler — preferred in Solar Eclipse and on single-target sustained
// damage. Two consecutive casts of EITHER one triggers the opposing
// Eclipse aura, so the bot naturally alternates over a fight.
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
//   1.  Rebirth                — battle-rez has highest impact in group.
//   2.  Barkskin               — off-GCD survival, fire on damage spikes.
//   3.  Renewal                — instant 30% self-heal (talent).
//   4.  Regrowth               — hard-cast panic only (DROPS Moonkin
//                                briefly) when nothing else is up.
//   5.  Typhoon                — peel knockback when surrounded.
//   6.  Moonkin Form           — spec stance entry, sits above DPS rules
//                                so a stripped/dispelled form re-applies
//                                before any nuke fires.
//   7.  Mark of the Wild       — OOC group buff maintenance.
//   8.  Solar Beam             — primary interrupt (AoE silence too).
//   9.  Mighty Bash            — interrupt fallback when Solar Beam down.
//   10. Soothe                 — enrage dispel on target.
//   11. Innervate              — ally caster mana cooldown.
//   12. Nature's Vigil         — burst+heal smear on bosses.
//   13. Incarnation            — major CD pair (talent override of CA).
//   14. Celestial Alignment    — major CD when Incarnation not talented.
//   15. Warrior of Elune       — buff (3 instant Starfires).
//   16. Force of Nature        — treant adds (talent).
//   17. Fury of Elune          — ground AoE channel (talent).
//   18. Starfall               — 50-AP AoE spender (2+ targets).
//   19. Stellar Flare          — talent DoT refresh.
//   20. Sunfire primary        — instant DoT on victim.
//   21. Moonfire primary       — instant DoT on victim.
//   22. Sunfire expand         — blanket DoT on off-target enemies.
//   23. Moonfire expand        — blanket DoT on off-target enemies.
//   24. Full / Half / New Moon — talent chain finisher.
//   25. Starsurge              — 30-AP ST spender.
//   26. Starfire               — heavier filler (Lunar / AoE).
//   27. Wrath                  — lighter filler (Solar build).
//   28. Idle                   — alive fallthrough.
ApRule const kRules[] = {
    { ShouldRebirth,          DoRebirth,          "Rebirth (battle rez)"       },
    { ShouldBarkskin,         DoBarkskin,         "Barkskin (<=50%)"           },
    { ShouldRenewal,          DoRenewal,          "Renewal (<=40%)"            },
    { ShouldRegrowth,         DoRegrowth,         "Regrowth (<=30% deep panic)"},
    { ShouldTyphoon,          DoTyphoon,          "Typhoon (peel)"             },
    { ShouldMoonkinForm,      DoMoonkinForm,      "Moonkin Form"               },
    { ShouldMarkOfTheWild,    DoMarkOfTheWild,    "Mark of the Wild"           },
    { ShouldSolarBeam,        DoSolarBeam,        "Solar Beam (interrupt)"     },
    { ShouldMightyBash,       DoMightyBash,       "Mighty Bash (interrupt fb)" },
    { ShouldSoothe,           DoSoothe,           "Soothe (enrage)"            },
    { ShouldInnervate,        DoInnervate,        "Innervate (healer mana)"    },
    { ShouldNaturesVigil,     DoNaturesVigil,     "Nature's Vigil (boss)"      },
    { ShouldIncarnation,      DoIncarnation,      "Incarnation"                },
    { ShouldCelestialAlign,   DoCelestialAlign,   "Celestial Alignment"        },
    { ShouldWarriorOfElune,   DoWarriorOfElune,   "Warrior of Elune"           },
    { ShouldForceOfNature,    DoForceOfNature,    "Force of Nature (treants)"  },
    { ShouldFuryOfElune,      DoFuryOfElune,      "Fury of Elune"              },
    { ShouldStarfall,         DoStarfall,         "Starfall (2+ AoE)"          },
    { ShouldStellarFlare,     DoStellarFlare,     "Stellar Flare (DoT)"        },
    { ShouldSunfirePrimary,   DoSunfirePrimary,   "Sunfire (primary)"          },
    { ShouldMoonfirePrimary,  DoMoonfirePrimary,  "Moonfire (primary)"         },
    { ShouldSunfireExpand,    DoSunfireExpand,    "Sunfire (expand off-tgt)"   },
    { ShouldMoonfireExpand,   DoMoonfireExpand,   "Moonfire (expand off-tgt)"  },
    { ShouldFullMoon,         DoFullMoon,         "Full Moon"                  },
    { ShouldHalfMoon,         DoHalfMoon,         "Half Moon"                  },
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
