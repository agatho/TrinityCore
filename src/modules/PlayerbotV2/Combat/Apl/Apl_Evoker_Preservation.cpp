// Preservation Evoker - WoW 12.0 enterprise rotation. Empower-spell healer
// (Spiritbloom + Dream Breath empowered to higher ranks the longer held —
// treated as instants here since rank-cancellation isn't exposed).
//
// Rule ORDER:
//   1) Battle / OOC rez:     Resurrection (Return) + Mass Return combat rez
//   2) Personal survival:    Renewing Blaze (self emergency), Obsidian
//                            Scales, Verdant Embrace self if dropping, Hover
//                            (cast-while-moving), Emerald Communion OOC
//   3) Interrupt / CC:       Quell, Sleep Walk off-target
//   4) Dispel:               Naturalize (Magic+Poison), Cauterizing Flame
//                            (Disease+Poison+Bleed)
//   5) Group utility:        Time Dilation (tank), Source of Magic, Rescue
//                            peel, Blessing of the Bronze, Zephyr
//   6) Lust:                 Fury of the Aspects (Sated guarded)
//   7) Hard panic / pre-burst: Stasis (bank pre-burst), Rewind (raid spike),
//                              Time Spiral
//   8) Major heals (raid spike): Spiritbloom split, Dream Breath empowered
//                                AoE
//   9) HoTs:                 Echo (loader), Reversion (refresh)
//  10) Sustained heal:       Emerald Blossom AoE, Verdant Embrace lowest,
//                            Lifebind, Living Flame heal lowest
//  11) Offensive filler:     Disintegrate / Living Flame when group full
//                            (auto-converts proc to heal)
//
// Validated IDs (cross-referenced against SpellName.csv +
// SpecializationSpells.csv on 2026-05-27):
//   361469 Living Flame           — class baseline (used for both heal +
//                                   offensive cast)
//   366155 Reversion              — Preservation HoT
//   355936 Dream Breath           — Empower AoE heal
//   367226 Spiritbloom            — Empower split heal
//   355913 Emerald Blossom        — group HoT proc heal (class baseline)
//   364343 Echo                   — HoT loader
//   363534 Rewind                 — raid HP rewind
//   370537 Stasis                 — banked heals
//   361178 Mass Return            — Preservation COMBAT rez (added)
//   361227 Return                 — Preservation OOC rez
//   351338 Quell, 360806 Sleep Walk
//   360823 Naturalize             — Magic+Poison dispel
//   363916 Obsidian Scales        — CORRECTED from 235450 (Prismatic Barrier)
//   374348 Renewing Blaze         — self HoT
//   374227 Zephyr                 — talent group magic DR
//   360995 Verdant Embrace        — heal
//   358267 Hover, 370665 Rescue
//   369459 Source of Magic        — caster mana regen
//   374251 Cauterizing Flame      — cleanse
//   370960 Emerald Communion      — OOC mana
//   374968 Time Spiral
//   357170 Time Dilation          — talent friendly DR
//   364342 Blessing of the Bronze
//   373270 Lifebind               — talent heal link
//   390386 Fury of the Aspects
//   356995 Disintegrate           — offensive filler when group is topped
//
// Skipped spells (and why):
//   365262 Improved Emerald Blossom — passive, no cast
//   373514 Verdant Embrace        — duplicate ID variant; 360995 is the
//                                   actual castable spell

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "ApDispelHelpers.h"
#include "ApHealHelpers.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated) ----
constexpr uint32 LIVING_FLAME_PRES      = 361469;
constexpr uint32 REVERSION              = 366155;
constexpr uint32 DREAM_BREATH           = 355936;
constexpr uint32 SPIRITBLOOM            = 367226;
constexpr uint32 EMERALD_BLOSSOM        = 355913;
constexpr uint32 ECHO                   = 364343;
constexpr uint32 REWIND                 = 363534;
constexpr uint32 STASIS                 = 370537;
constexpr uint32 QUELL                  = 351338;
constexpr uint32 SLEEP_WALK             = 360806;
constexpr uint32 NATURALIZE             = 360823;
constexpr uint32 OBSIDIAN_SCALES        = 363916;       // CORRECTED — 235450 is Mage Prismatic Barrier
constexpr uint32 RENEWING_BLAZE         = 374348;
constexpr uint32 ZEPHYR                 = 374227;
constexpr uint32 VERDANT_EMBRACE        = 360995;
constexpr uint32 HOVER                  = 358267;
constexpr uint32 RESCUE                 = 370665;
constexpr uint32 SOURCE_OF_MAGIC        = 369459;
constexpr uint32 CAUTERIZING_FLAME      = 374251;
constexpr uint32 EMERALD_COMMUNION      = 370960;
constexpr uint32 TIME_SPIRAL            = 374968;
constexpr uint32 TIME_DILATION          = 357170;       // talent — friendly DR over time
constexpr uint32 BLESSING_OF_THE_BRONZE = 364342;
constexpr uint32 FURY_OF_THE_ASPECTS    = 390386;
constexpr uint32 LIFEBIND               = 373270;       // talent — links healing
constexpr uint32 DISINTEGRATE           = 356995;
constexpr uint32 RESURRECTION_PRES      = 361227;       // OOC rez (Return)
constexpr uint32 MASS_RETURN            = 361178;       // Preservation COMBAT rez

// Lust debuffs
constexpr uint32 SATED_DEBUFF           = 57724;
constexpr uint32 TEMPORAL_DISPL_DEBUFF  = 80354;
constexpr uint32 INSANITY_HUNTER_DEBUFF = 95809;
constexpr uint32 FATIGUED_DEBUFF        = 264689;

// ---- Helpers ----
struct HealTarget
{
    ObjectGuid guid;
    int32      hp_pct;
};

HealTarget LowestFriendOrSelf(ApPredicateContext const& ctx)
{
    HealTarget t{ ctx.bot.raw().guid, ctx.bot.hp_pct() };
    if (auto const* low = ctx.group.heal_assignment(ctx.bot.raw().guid, ctx.bot.map_id(), ctx.bot.raw().position.x, ctx.bot.raw().position.y, ctx.bot.raw().position.z, 45.0f))
    {
        if (low->online && low->max_hp > 0)
        {
            const int32 pct = (low->hp * 100) / low->max_hp;
            if (pct < t.hp_pct) { t.guid = low->guid; t.hp_pct = pct; }
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
    for (auto const& m : *members)
    {
        if (!m.online || m.max_hp <= 0 || m.hp <= 0) continue;
        if ((m.hp * 100) / m.max_hp <= below_pct) ++n;
    }
    return n;
}

bool GroupTopped(ApPredicateContext const& ctx)
{
    return WoundedFriendCount(ctx, 92) == 0;
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

bool BotHasSatedDebuff(ApPredicateContext const& ctx)
{
    return ctx.bot.has_aura(SATED_DEBUFF)
        || ctx.bot.has_aura(TEMPORAL_DISPL_DEBUFF)
        || ctx.bot.has_aura(INSANITY_HUNTER_DEBUFF)
        || ctx.bot.has_aura(FATIGUED_DEBUFF);
}

GroupMemberSummary const* DispelTarget(ApPredicateContext const& ctx)
{
    return DispelTargetWithPriority(ctx, [](GroupSnapshotView const& g)
        -> GroupMemberSummary const*
    {
        if (auto const* m = g.dispel_candidate(DispelType::Magic))  return m;
        if (auto const* m = g.dispel_candidate(DispelType::Poison)) return m;
        return nullptr;
    });
}

GroupMemberSummary const* DispelTargetCleanseDPB(ApPredicateContext const& ctx)
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
        || ctx.bot.self_dispellable(DispelType::Poison);
}

bool HasLiveTargetInline(ApPredicateContext const& ctx)
{
    return !ctx.bot.victim().IsEmpty();
}

// ---- OOC rez ----
bool ShouldResurrection(ApPredicateContext const& ctx)
{
    if (ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(RESURRECTION_PRES)) return false;
    if (!ctx.bot.is_ready(RESURRECTION_PRES)) return false;
    return ctx.group.dead_member(ctx.bot.map_id()) != nullptr;
}
void DoResurrection(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* m = ctx.group.dead_member(ctx.bot.map_id()))
        e.cast(RESURRECTION_PRES, m->guid);
}

// Combat rez — Mass Return is Preservation's signature combat rez. Use it
// when at least 2 allies on our map are down (single dead → cheaper Return
// equivalent via Battle Rez normally, but Pres ONLY has Mass Return, which
// raises every fallen ally near the target). Only fire in-combat, since
// the OOC path above uses single-target Return.
bool ShouldMassReturn(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(MASS_RETURN)) return false;
    if (!ctx.bot.is_ready(MASS_RETURN)) return false;
    return ctx.group.dead_member(ctx.bot.map_id()) != nullptr;
}
void DoMassReturn(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* m = ctx.group.dead_member(ctx.bot.map_id()))
        e.cast(MASS_RETURN, m->guid);
}

// ---- Personal survival ----
bool ShouldRenewingBlaze(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(RENEWING_BLAZE)) return false;
    if (!ctx.bot.is_ready(RENEWING_BLAZE)) return false;
    return ctx.bot.hp_pct() <= 60;
}
void DoRenewingBlaze(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(RENEWING_BLAZE); }

bool ShouldObsidianScales(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(OBSIDIAN_SCALES)) return false;
    if (!ctx.bot.is_ready(OBSIDIAN_SCALES)) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoObsidianScales(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(OBSIDIAN_SCALES); }

bool ShouldZephyr(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ZEPHYR)) return false;
    if (!ctx.bot.is_ready(ZEPHYR)) return false;
    return WoundedFriendCount(ctx, 70) >= 3;
}
void DoZephyr(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ZEPHYR); }

bool ShouldHover(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(HOVER)) return false;
    if (!ctx.bot.is_ready(HOVER)) return false;
    if (ctx.bot.has_aura(HOVER)) return false;
    return ctx.bot.is_moving();
}
void DoHover(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HOVER); }

bool ShouldEmeraldCommunion(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(EMERALD_COMMUNION)) return false;
    if (!ctx.bot.is_ready(EMERALD_COMMUNION)) return false;
    if (ctx.bot.in_combat()) return false;
    if (ctx.bot.max_power(0) <= 0) return false;
    return ctx.bot.power_pct(0) <= 35 || ctx.bot.hp_pct() <= 50;
}
void DoEmeraldCommunion(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(EMERALD_COMMUNION); }

// ---- Group utility ----
bool ShouldSourceOfMagic(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SOURCE_OF_MAGIC)) return false;
    if (!ctx.bot.is_ready(SOURCE_OF_MAGIC)) return false;
    auto const* m = ctx.group.lowest_mana_caster();
    if (!m || !m->online || m->hp <= 0) return false;
    if (m->guid == ctx.bot.raw().guid) return false;
    return !ctx.bot.has_aura(SOURCE_OF_MAGIC, m->guid);
}
void DoSourceOfMagic(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* m = ctx.group.lowest_mana_caster())
        e.cast(SOURCE_OF_MAGIC, m->guid);
}

bool ShouldCauterizingFlame(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(CAUTERIZING_FLAME)) return false;
    if (!ctx.bot.is_ready(CAUTERIZING_FLAME)) return false;
    return DispelTargetCleanseDPB(ctx) != nullptr;
}
void DoCauterizingFlame(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = DispelTargetCleanseDPB(ctx))
        e.cast(CAUTERIZING_FLAME, t->guid);
}

bool ShouldRescueLowestAlly(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(RESCUE)) return false;
    if (!ctx.bot.is_ready(RESCUE)) return false;
    auto const* low = ctx.group.lowest_hp_on_map(ctx.bot.map_id(), Role::Unknown, ctx.bot.raw().position.x, ctx.bot.raw().position.y, ctx.bot.raw().position.z, 45.0f);
    if (!low || !low->online || low->hp <= 0) return false;
    if (low->max_hp <= 0) return false;
    if (low->guid == ctx.bot.raw().guid) return false;
    return (low->hp * 100) / low->max_hp <= 25;
}
void DoRescueLowestAlly(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* low = ctx.group.lowest_hp_on_map(ctx.bot.map_id(), Role::Unknown, ctx.bot.raw().position.x, ctx.bot.raw().position.y, ctx.bot.raw().position.z, 45.0f))
        e.cast(RESCUE, low->guid);
}

bool ShouldBlessingOfTheBronze(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BLESSING_OF_THE_BRONZE)) return false;
    return !ctx.bot.has_aura(BLESSING_OF_THE_BRONZE);
}
void DoBlessingOfTheBronze(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLESSING_OF_THE_BRONZE); }

bool ShouldTimeDilation(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(TIME_DILATION)) return false;
    if (!ctx.bot.is_ready(TIME_DILATION)) return false;
    GroupMemberSummary const* tank = ctx.group.tank();
    if (!tank || !tank->online || tank->hp <= 0) return false;
    if (tank->max_hp <= 0) return false;
    return (tank->hp * 100) / tank->max_hp <= 45;
}
void DoTimeDilation(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* tank = ctx.group.tank())
        e.cast(TIME_DILATION, tank->guid);
}

// ---- Dispel ----
bool ShouldNaturalize(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(NATURALIZE)) return false;
    if (!ctx.bot.is_ready(NATURALIZE)) return false;
    return DispelTarget(ctx) != nullptr || SelfNeedsDispel(ctx);
}
void DoNaturalize(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = DispelTarget(ctx)) { e.cast(NATURALIZE, t->guid); return; }
    if (SelfNeedsDispel(ctx))                e.cast(NATURALIZE, ctx.bot.raw().guid);
}

// ---- Interrupt / CC ----
bool ShouldQuell(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(QUELL)) return false;
    if (!ctx.bot.is_ready(QUELL)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (pvp) return ctx.bot.kick_target(true, 30.0f) != nullptr;
    auto const* c = ctx.bot.interruptible_caster();
    return c && c->guid == ctx.bot.victim();
}
void DoQuell(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 30.0f))
        e.cast(QUELL, c->guid);
}

bool ShouldSleepWalk(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SLEEP_WALK)) return false;
    if (!ctx.bot.is_ready(SLEEP_WALK)) return false;
    auto const* c = ctx.bot.interruptible_caster();
    if (!c || c->guid == ctx.bot.victim()) return false;
    return !ctx.bot.is_ready(QUELL);
}
void DoSleepWalk(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* c = ctx.bot.interruptible_caster())
        e.cast(SLEEP_WALK, c->guid);
}

// ---- Hard panic ----
bool ShouldRewind(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(REWIND)) return false;
    if (!ctx.bot.is_ready(REWIND)) return false;
    return WoundedFriendCount(ctx, 30) >= 3;
}
void DoRewind(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(REWIND); }

bool ShouldStasis(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(STASIS)) return false;
    if (!ctx.bot.is_ready(STASIS)) return false;
    // Bank 3 spells for burst window — pop pre-emptively at boss start.
    return BossLikeTargetEngaged(ctx) && WoundedFriendCount(ctx, 95) <= 1;
}
void DoStasis(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(STASIS); }

bool ShouldTimeSpiral(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(TIME_SPIRAL)) return false;
    if (!ctx.bot.is_ready(TIME_SPIRAL)) return false;
    return BossLikeTargetEngaged(ctx) && WoundedFriendCount(ctx, 50) >= 2;
}
void DoTimeSpiral(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TIME_SPIRAL); }

// ---- Lust ----
bool ShouldFuryOfTheAspects(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(FURY_OF_THE_ASPECTS)) return false;
    if (!ctx.bot.is_ready(FURY_OF_THE_ASPECTS)) return false;
    if (BotHasSatedDebuff(ctx)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoFuryOfTheAspects(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FURY_OF_THE_ASPECTS); }

// ---- Major heals ----
bool ShouldDreamBreath(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(DREAM_BREATH)) return false;
    if (!ctx.bot.is_ready(DREAM_BREATH)) return false;
    return WoundedFriendCount(ctx, 80) >= 3;
}
void DoDreamBreath(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DREAM_BREATH); }

bool ShouldSpiritbloom(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SPIRITBLOOM)) return false;
    if (!ctx.bot.is_ready(SPIRITBLOOM)) return false;
    return WoundedFriendCount(ctx, 65) >= 2;
}
void DoSpiritbloom(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SPIRITBLOOM, LowestFriendOrSelf(ctx).guid);
}

// ---- Sustained heal ----
bool ShouldEmeraldBlossom(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(EMERALD_BLOSSOM)) return false;
    if (!ctx.bot.is_ready(EMERALD_BLOSSOM)) return false;
    return WoundedFriendCount(ctx, 80) >= 2;
}
void DoEmeraldBlossom(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(EMERALD_BLOSSOM, LowestFriendOrSelf(ctx).guid);
}

bool ShouldEcho(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(ECHO)) return false;
    if (!ctx.bot.is_ready(ECHO)) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 75;
}
void DoEcho(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ECHO, LowestFriendOrSelf(ctx).guid);
}

bool ShouldVerdantEmbraceLowest(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(VERDANT_EMBRACE)) return false;
    if (!ctx.bot.is_ready(VERDANT_EMBRACE)) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 60;
}
void DoVerdantEmbraceLowest(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(VERDANT_EMBRACE, LowestFriendOrSelf(ctx).guid);
}

bool ShouldLifebind(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(LIFEBIND)) return false;
    if (!ctx.bot.is_ready(LIFEBIND)) return false;
    return WoundedFriendCount(ctx, 80) >= 2;
}
void DoLifebind(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LIFEBIND, LowestFriendOrSelf(ctx).guid);
}

// ---- HoT maintenance ----
bool ShouldReversion(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(REVERSION)) return false;
    HealTarget t = LowestFriendOrSelf(ctx);
    if (t.hp_pct >= 95) return false;
    AuraEntry const* a = ctx.bot.find_aura(REVERSION, t.guid);
    return !a || a->remaining.count() <= 3000;
}
void DoReversion(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(REVERSION, LowestFriendOrSelf(ctx).guid);
}

// ---- Filler ----
bool ShouldLivingFlame(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(LIVING_FLAME_PRES)) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 90;
}
void DoLivingFlame(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LIVING_FLAME_PRES, LowestFriendOrSelf(ctx).guid);
}

// ---- Offensive filler ----
bool ShouldDisintegrateFiller(ApPredicateContext const& ctx)
{
    if (!HasLiveTargetInline(ctx)) return false;
    if (!GroupTopped(ctx)) return false;
    if (!ctx.bot.knows_spell(DISINTEGRATE)) return false;
    if (!ctx.bot.is_ready(DISINTEGRATE)) return false;
    return ctx.bot.power(POWER_ESSENCE) >= 3;
}
void DoDisintegrateFiller(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DISINTEGRATE, ctx.bot.victim());
}

bool ShouldLivingFlameOffensive(ApPredicateContext const& ctx)
{
    if (!HasLiveTargetInline(ctx)) return false;
    if (!GroupTopped(ctx)) return false;
    return ctx.bot.knows_spell(LIVING_FLAME_PRES);
}
void DoLivingFlameOffensive(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LIVING_FLAME_PRES, ctx.bot.victim());
}

bool AlwaysAlive(ApPredicateContext const& ctx) { return ctx.bot.is_alive(); }
void DoNothing(ApPredicateContext const&, BotIntentEmitter&) {}

// Cast-swap shim — Preservation Evoker slow empowers. Dream Breath
// and Spiritbloom are empowers (1-3.25s scaling). See ApHealHelpers.h.
bool ShouldCancelHealForSwap(ApPredicateContext const& ctx)
{
    return ShouldCancelHealForSwapImpl(ctx,
        { DREAM_BREATH, SPIRITBLOOM });
}

// ---- Rule table ----
// Order: Heal-cancel swap -> Rez (OOC + combat) -> Personal survival ->
//        Interrupt/CC -> Dispel -> Group utility -> Lust -> Hard panic ->
//        Stasis pre-burst -> Major heals (Spiritbloom/Dream Breath) ->
//        HoTs (Echo, Reversion) -> Sustained heals -> Offensive filler.
ApRule const kRules[] = {
    // Internal: cancel a heal in flight to retarget onto someone lower.
    { ShouldCancelHealForSwap,    DoCancelHealForSwap,    "Cancel heal — swap to lower target" },

    // 1) Rez. OOC = Return single-target; in-combat = Mass Return.
    { ShouldResurrection,         DoResurrection,         "Resurrection (OOC)"             },
    { ShouldMassReturn,           DoMassReturn,           "Mass Return (combat rez)"       },

    // 2) Personal survival.
    { ShouldRenewingBlaze,        DoRenewingBlaze,        "Renewing Blaze (<=60%)"         },
    { ShouldObsidianScales,       DoObsidianScales,       "Obsidian Scales (<=50%)"        },
    { ShouldHover,                DoHover,                "Hover (cast-while-moving)"      },
    { ShouldEmeraldCommunion,     DoEmeraldCommunion,     "Emerald Communion (OOC)"        },

    // 3) Interrupt / CC.
    { ShouldQuell,                DoQuell,                "Quell (interrupt)"              },
    { ShouldSleepWalk,            DoSleepWalk,            "Sleep Walk (off-target)"        },

    // 4) Dispel.
    { ShouldNaturalize,           DoNaturalize,           "Naturalize (dispel)"            },
    { ShouldCauterizingFlame,     DoCauterizingFlame,     "Cauterizing Flame (cleanse)"    },

    // 5) Group utility.
    { ShouldTimeDilation,         DoTimeDilation,         "Time Dilation (tank <=45%)"     },
    { ShouldRescueLowestAlly,     DoRescueLowestAlly,     "Rescue (peel ally <=25%)"       },
    { ShouldBlessingOfTheBronze,  DoBlessingOfTheBronze,  "Blessing of the Bronze (buff)"  },
    { ShouldZephyr,               DoZephyr,               "Zephyr (group magic DR)"        },

    // 6) Lust.
    { ShouldFuryOfTheAspects,     DoFuryOfTheAspects,     "Fury of the Aspects (boss)"     },

    // 7) Hard panic + pre-burst banking.
    { ShouldStasis,               DoStasis,               "Stasis (bank pre-burst)"        },
    { ShouldRewind,               DoRewind,               "Rewind (3+ at <=30%)"           },
    { ShouldTimeSpiral,           DoTimeSpiral,           "Time Spiral (group bail)"       },

    // 8) Major heals — Spiritbloom (raid spike) before Dream Breath; both
    //    are Empowers and the only spike-heal tools we have.
    { ShouldSpiritbloom,          DoSpiritbloom,          "Spiritbloom (2+ at 65%)"        },
    { ShouldDreamBreath,          DoDreamBreath,          "Dream Breath (3+ at 80%)"       },

    // 9) HoTs — preload + refresh.
    { ShouldEcho,                 DoEcho,                 "Echo (preload)"                 },
    { ShouldReversion,            DoReversion,            "Reversion (HoT refresh)"        },

    // 10) Sustained heals.
    { ShouldVerdantEmbraceLowest, DoVerdantEmbraceLowest, "Verdant Embrace (<=60%)"        },
    { ShouldLifebind,             DoLifebind,             "Lifebind (heal share)"          },
    { ShouldEmeraldBlossom,       DoEmeraldBlossom,       "Emerald Blossom (2+ at 80%)"    },
    { ShouldLivingFlame,          DoLivingFlame,          "Living Flame (heal lowest)"     },

    // 11) Offensive filler — only when the group is topped (>=92%).
    { ShouldDisintegrateFiller,   DoDisintegrateFiller,   "Disintegrate (DPS filler)"      },
    { ShouldLivingFlameOffensive, DoLivingFlameOffensive, "Living Flame (DPS filler)"      },

    { AlwaysAlive,                DoNothing,              "Idle"                           },
};

} // anonymous

void RegisterApl_Evoker_Preservation()
{
    constexpr uint32 SPEC_EVOKER_PRESERVATION = 1468;
    RegisterRotation(CLASS_EVOKER, SPEC_EVOKER_PRESERVATION, ApRotation{kRules});
}

} // namespace Playerbot::Combat
