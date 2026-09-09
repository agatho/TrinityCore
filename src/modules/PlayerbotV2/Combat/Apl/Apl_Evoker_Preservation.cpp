// Preservation Evoker - WoW 12.1.0.69587 (Midnight) rotation. Empower-spell
// healer (Dream Breath empowered to higher ranks the longer held - treated
// as an instant here since rank-cancellation isn't exposed). Spiritbloom is
// gone from the 12.1 Preservation tree.
//
// Rule ORDER:
//   1) Battle / OOC rez:     Resurrection (Return) + Mass Return combat rez
//   2) Personal survival:    Obsidian Scales (Renewing Blaze rides on it
//                            passively), Hover (cast-while-moving)
//   3) CC:                   Sleep Walk off-target (Quell is Devastation-only
//                            in 12.1)
//   4) Dispel:               Naturalize (Magic+Poison), Cauterizing Flame
//                            (Bleed+Poison+Curse+Disease)
//   5) Group utility:        Time Dilation (tank), Rescue peel, Blessing of
//                            the Bronze, Zephyr
//   6) Lust:                 Fury of the Aspects (Sated guarded)
//   7) Hard panic / pre-burst: Stasis (bank pre-burst), Rewind (raid spike),
//                              Time Spiral, Temporal Anomaly (group absorb +
//                              Echo), Temporal Barrier (M+ absorb)
//   8) Major heal (raid spike): Dream Breath empowered cone
//   9) HoTs:                 Echo (loader), Reversion (refresh)
//  10) Sustained heal:       Verdant Embrace lowest (Lifebind passive rides
//                            on it), Emerald Blossom AoE, Living Flame heal
//  11) Offensive filler:     Disintegrate / Living Flame when group full
//
// Validated IDs (WoW 12.1.0.69587, cross-referenced against the 12.1 kit:
// SkillLineAbility + SpecializationSpells + simc trait data, 2026-09-09):
//   361469 Living Flame           - class baseline (heal + offensive cast)
//   366155 Reversion              - spec talent HoT [R]
//   355936 Dream Breath           - spec talent Empower cone heal [R]
//   355913 Emerald Blossom        - class baseline delayed AoE heal
//   364343 Echo                   - spec talent HoT loader [R]
//   363534 Rewind                 - spec talent raid rewind [R]
//   370537 Stasis                 - spec talent banked heals [R]
//   373861 Temporal Anomaly       - spec talent forward absorb + Echo [R]
//   1291636 Temporal Barrier      - spec talent targeted absorb + Echo [M]
//   357170 Time Dilation          - spec talent friendly DR [R]
//   361178 Mass Return            - spec spell combat rez (L38)
//   361227 Return                 - class baseline OOC rez (L12)
//   360806 Sleep Walk             - class talent off-target CC (not in the
//                                   curated builds; knows_spell-gated)
//   360823 Naturalize             - spec spell Magic+Poison dispel
//                                   (overrides 365585 Expunge)
//   363916 Obsidian Scales        - class talent 30% DR [R]
//   374227 Zephyr                 - class talent AoE DR [R]
//   360995 Verdant Embrace        - class talent heal [R]
//   358267 Hover                  - class baseline mobility
//   370665 Rescue                 - class talent peel [R]
//   369459 Source of Magic        - class talent [R]
//   374251 Cauterizing Flame      - class talent cleanse [R]
//   374968 Time Spiral            - class talent [M]
//   364342 Blessing of the Bronze - class baseline (L30)
//   390386 Fury of the Aspects    - class baseline lust (L48)
//   356995 Disintegrate           - class baseline offensive filler
//
// Skipped spells (and why):
//   367226 Spiritbloom            - not in the 12.1 Preservation tree
//                                   (1244312 same-name stub, no description)
//   351338 Quell                  - Devastation-only spec talent in 12.1
//   374348 Renewing Blaze         - passive in 12.1: rides on Obsidian
//                                   Scales, no cast
//   373270 Lifebind               - passive: triggered by Verdant Embrace
//   370960 Emerald Communion      - removed from the class tree
//   365585 Expunge                - always overridden by Naturalize for
//                                   Preservation
//   359816 Dream Flight           - not in either curated build; needs a
//                                   flight line the bot cannot plan
//   358385 Landslide / 372048 Oppressing Roar - [M]-only CC, healer has no
//                                   spare GCD for them
//   1229376 Single-Button Assistant - client rotation helper

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

// ---- Spell IDs (WoW 12.1.0.69587, validated) ----
constexpr uint32 LIVING_FLAME_PRES      = 361469;
constexpr uint32 REVERSION              = 366155;
constexpr uint32 DREAM_BREATH           = 355936;       // empower cone heal
constexpr uint32 EMERALD_BLOSSOM        = 355913;
constexpr uint32 ECHO                   = 364343;
constexpr uint32 REWIND                 = 363534;
constexpr uint32 STASIS                 = 370537;
constexpr uint32 TEMPORAL_ANOMALY       = 373861;       // talent - forward absorb vortex + Echo
constexpr uint32 TEMPORAL_BARRIER       = 1291636;      // talent (M+) - targeted absorb + Echo
constexpr uint32 SLEEP_WALK             = 360806;
constexpr uint32 NATURALIZE             = 360823;       // spec spell - overrides Expunge
constexpr uint32 OBSIDIAN_SCALES        = 363916;       // 30% DR; Renewing Blaze rides on it passively
constexpr uint32 ZEPHYR                 = 374227;
constexpr uint32 VERDANT_EMBRACE        = 360995;       // Lifebind passive triggers off this
constexpr uint32 HOVER                  = 358267;
constexpr uint32 RESCUE                 = 370665;
constexpr uint32 SOURCE_OF_MAGIC        = 369459;
constexpr uint32 CAUTERIZING_FLAME      = 374251;
constexpr uint32 TIME_SPIRAL            = 374968;
constexpr uint32 TIME_DILATION          = 357170;       // talent - friendly DR over time
constexpr uint32 BLESSING_OF_THE_BRONZE = 364342;
constexpr uint32 FURY_OF_THE_ASPECTS    = 390386;
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
    // "my own HP <= below_pct" - the 92% GroupTopped gates then froze ALL
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

// Cauterizing Flame covers Bleed, Poison, Curse and Disease in 12.1.
GroupMemberSummary const* DispelTargetCleanseDPB(ApPredicateContext const& ctx)
{
    return DispelTargetWithPriority(ctx, [](GroupSnapshotView const& g)
        -> GroupMemberSummary const*
    {
        if (auto const* m = g.dispel_candidate(DispelType::Disease)) return m;
        if (auto const* m = g.dispel_candidate(DispelType::Poison))  return m;
        if (auto const* m = g.dispel_candidate(DispelType::Curse))   return m;
        if (auto const* m = g.dispel_candidate(DispelType::Bleed))   return m;
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

// Combat rez - Mass Return is Preservation's signature combat rez. Use it
// when at least 2 allies on our map are down (single dead -> cheaper Return
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

// ---- CC ----
// Quell is a Devastation-only spec talent in 12.1 - Preservation has no
// interrupt. Sleep Walk an off-target caster instead when talented.
bool ShouldSleepWalk(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SLEEP_WALK)) return false;
    if (!ctx.bot.is_ready(SLEEP_WALK)) return false;
    auto const* c = ctx.bot.interruptible_caster();
    return c && c->guid != ctx.bot.victim();
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
    // Bank 3 spells for burst window - pop pre-emptively at boss start.
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

// Temporal Anomaly - 1.5s cast, sends an absorb vortex forward through the
// group and applies Echo to the first allies it passes. Pre-shield when the
// group is taking sustained damage or a boss is engaged.
bool ShouldTemporalAnomaly(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(TEMPORAL_ANOMALY)) return false;
    if (!ctx.bot.is_ready(TEMPORAL_ANOMALY)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(TEMPORAL_ANOMALY))
        return false;
    return BossLikeTargetEngaged(ctx) || WoundedFriendCount(ctx, 85) >= 2;
}
void DoTemporalAnomaly(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TEMPORAL_ANOMALY); }

// Temporal Barrier (M+ build) - targeted absorb + Echo on the target and
// nearby allies. Put it on whoever is lowest before they need the heal.
bool ShouldTemporalBarrier(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(TEMPORAL_BARRIER)) return false;
    if (!ctx.bot.is_ready(TEMPORAL_BARRIER)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(TEMPORAL_BARRIER))
        return false;
    HealTarget t = LowestFriendOrSelf(ctx);
    if (t.hp_pct > 70) return false;
    return !ctx.bot.has_aura(TEMPORAL_BARRIER, t.guid);
}
void DoTemporalBarrier(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(TEMPORAL_BARRIER, LowestFriendOrSelf(ctx).guid);
}

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

// Cast-swap shim - Preservation Evoker slow empower. Dream Breath is the
// only empower heal left in 12.1 (1-3.25s scaling). See ApHealHelpers.h.
bool ShouldCancelHealForSwap(ApPredicateContext const& ctx)
{
    return ShouldCancelHealForSwapImpl(ctx,
        { DREAM_BREATH });
}

// ---- Rule table ----
// Order: Heal-cancel swap -> Rez (OOC + combat) -> Personal survival ->
//        CC -> Dispel -> Group utility -> Lust -> Hard panic / pre-shield
//        (Stasis, Rewind, Time Spiral, Temporal Anomaly / Barrier) ->
//        Major heal (Dream Breath) -> HoTs (Echo, Reversion) ->
//        Sustained heals -> Offensive filler.
ApRule const kRules[] = {
    // Internal: cancel a heal in flight to retarget onto someone lower.
    { ShouldCancelHealForSwap,    DoCancelHealForSwap,    "Cancel heal - swap to lower target" },

    // 1) Rez. OOC = Return single-target; in-combat = Mass Return.
    { ShouldResurrection,         DoResurrection,         "Resurrection (OOC)"             },
    { ShouldMassReturn,           DoMassReturn,           "Mass Return (combat rez)"       },

    // 2) Personal survival. Renewing Blaze is a passive rider on Obsidian
    //    Scales in 12.1.
    { ShouldObsidianScales,       DoObsidianScales,       "Obsidian Scales (<=50%)"        },
    { ShouldHover,                DoHover,                "Hover (cast-while-moving)"      },

    // 3) CC (no interrupt: Quell is Devastation-only in 12.1).
    { ShouldSleepWalk,            DoSleepWalk,            "Sleep Walk (off-target)"        },

    // 4) Dispel.
    { ShouldNaturalize,           DoNaturalize,           "Naturalize (dispel)"            },
    { ShouldCauterizingFlame,     DoCauterizingFlame,     "Cauterizing Flame (cleanse)"    },

    // 5) Group utility.
    { ShouldTimeDilation,         DoTimeDilation,         "Time Dilation (tank <=45%)"     },
    { ShouldRescueLowestAlly,     DoRescueLowestAlly,     "Rescue (peel ally <=25%)"       },
    { ShouldBlessingOfTheBronze,  DoBlessingOfTheBronze,  "Blessing of the Bronze (buff)"  },
    { ShouldZephyr,               DoZephyr,               "Zephyr (group AoE DR)"          },

    // 6) Lust.
    { ShouldFuryOfTheAspects,     DoFuryOfTheAspects,     "Fury of the Aspects (boss)"     },

    // 7) Hard panic + pre-burst banking + pre-shields.
    { ShouldStasis,               DoStasis,               "Stasis (bank pre-burst)"        },
    { ShouldRewind,               DoRewind,               "Rewind (3+ at <=30%)"           },
    { ShouldTimeSpiral,           DoTimeSpiral,           "Time Spiral (group bail)"       },
    { ShouldTemporalAnomaly,      DoTemporalAnomaly,      "Temporal Anomaly (absorb)"      },
    { ShouldTemporalBarrier,      DoTemporalBarrier,      "Temporal Barrier (<=70%)"       },

    // 8) Major heal - Dream Breath is the only Empower spike heal in 12.1.
    { ShouldDreamBreath,          DoDreamBreath,          "Dream Breath (3+ at 80%)"       },

    // 9) HoTs - preload + refresh.
    { ShouldEcho,                 DoEcho,                 "Echo (preload)"                 },
    { ShouldReversion,            DoReversion,            "Reversion (HoT refresh)"        },

    // 10) Sustained heals. Lifebind is a passive that rides on Verdant
    //     Embrace.
    { ShouldVerdantEmbraceLowest, DoVerdantEmbraceLowest, "Verdant Embrace (<=60%)"        },
    { ShouldEmeraldBlossom,       DoEmeraldBlossom,       "Emerald Blossom (2+ at 80%)"    },
    { ShouldLivingFlame,          DoLivingFlame,          "Living Flame (heal lowest)"     },

    // 11) Offensive filler - only when the group is topped (>=92%).
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
