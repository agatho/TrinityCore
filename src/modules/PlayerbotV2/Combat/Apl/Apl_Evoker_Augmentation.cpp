// Augmentation Evoker - WoW 12.1.0.69587 (Midnight) rotation. Support spec
// that buffs allies via Ebon Might (group buff applied to nearby DPS) and
// Prescience (single-target crit buff). Damage is secondary - Fire Breath,
// Upheaval and Eruption all extend the active Ebon Might, and Essence Burst
// procs make Eruption free.
//
// Rule ORDER:
//   1) Mobility:              Hover (cast-while-moving)
//   2) Survival:              Obsidian Scales (Renewing Blaze rides on it
//                             passively), Verdant Embrace self, Emerald
//                             Blossom self-heal (<=50%), Zephyr
//   3) Interrupt / CC:        Quell, Sleep Walk off-target, Landslide panic
//                             root
//   4) Group utility:         Rescue peel, Expunge / Cauterizing Flame
//                             cleanse, Source of Magic (caster), Blessing
//                             of the Bronze
//   5) Lust:                  Fury of the Aspects (Sated guarded)
//   6) Ally maintenance       Ebon Might (refresh), Prescience on best DPS,
//      (Augmentation's        Blistering Scales on tank
//      whole purpose):
//   7) Major CDs:             Breath of Eons (Ebon Might + Temporal Wound),
//                             Time Skip, Tip the Scales (free max empower),
//                             Time Spiral (group bail)
//   8) Damage rotor:          Fire Breath (Empower, extends EM), Upheaval
//                             (Empower, extends EM), Eruption (Essence
//                             spender / EM extender), Disintegrate (only
//                             without the Eruption override), Living Flame,
//                             Azure Strike
//
// Validated IDs (WoW 12.1.0.69587, cross-referenced against the 12.1 kit:
// SkillLineAbility + SpecializationSpells + simc trait data, 2026-09-09):
//   361469 Living Flame           - class baseline
//   362969 Azure Strike           - class baseline
//   357208 Fire Breath            - class baseline Empower cone (extends EM)
//   395160 Eruption               - spec talent Essence spender [R]
//                                   (overrides 356995 Disintegrate)
//   395152 Ebon Might             - spec talent group buff [R]
//   409311 Prescience             - spec talent crit buff castable [R]
//   410089 Prescience             - aura on the ally (duration tracker)
//   403631 Breath of Eons         - spec talent burst CD [R]
//   360827 Blistering Scales      - spec talent tank buff (not in the
//                                   curated builds; knows_spell-gated)
//   396286 Upheaval               - spec talent Empower AoE [R]
//   356995 Disintegrate           - class baseline, only for bots without
//                                   Eruption
//   392268 Essence Burst          - aura only: next Eruption costs no Essence
//   404977 Time Skip              - spec talent CD accelerator [R]
//   370553 Tip the Scales         - class talent free max empower [R]
//   374968 Time Spiral            - class talent [R]
//   351338 Quell                  - spec talent interrupt [R]
//   360806 Sleep Walk             - class talent off-target CC [M]
//   358385 Landslide              - class talent ground root [R]
//   363916 Obsidian Scales        - class talent 30% DR [R]
//   374227 Zephyr                 - class talent AoE DR [R]
//   360995 Verdant Embrace        - class talent heal [R]
//   355913 Emerald Blossom        - class baseline castable (365261 is the
//                                   passive rank marker)
//   358267 Hover                  - class baseline mobility
//   370665 Rescue                 - class talent peel [R]
//   369459 Source of Magic        - class talent [R]
//   365585 Expunge                - class talent Poison dispel [R]
//   374251 Cauterizing Flame      - class talent cleanse [R]
//   364342 Blessing of the Bronze - class baseline (L30)
//   390386 Fury of the Aspects    - class baseline lust (L48)
//
// Skipped spells (and why - Augmentation has an unusually high passive
// surface area; most of the spec's "abilities" are auras, not casts):
//   374348 Renewing Blaze         - passive in 12.1: rides on Obsidian
//                                   Scales, no cast
//   357170 Time Dilation          - Preservation-only spec talent in 12.1
//   370960 Emerald Communion      - removed from the class tree
//   361021 Sense Power            - spec spell, UI-only reveal, no combat
//                                   value
//   408233 Bestow Weyrnstone      - not in the curated builds; needs a
//                                   bearer-activated teleport
//   412710 Timelessness           - not in the curated builds; threat drop
//   412713 Interwoven Threads     - passive that replaces Time Skip when
//                                   taken (not in the curated builds)
//   372048 Oppressing Roar / 406732 Spatial Paradox - not in either build
//   1229376 Single-Button Assistant - client rotation helper

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "ApDispelHelpers.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated) ----
constexpr uint32 LIVING_FLAME_AUG       = 361469;
constexpr uint32 AZURE_STRIKE_AUG       = 362969;
constexpr uint32 FIRE_BREATH            = 357208;       // empower cone - extends Ebon Might
constexpr uint32 ERUPTION               = 395160;       // 3 Essence - overrides Disintegrate
constexpr uint32 EBON_MIGHT             = 395152;
constexpr uint32 PRESCIENCE             = 409311;       // castable
constexpr uint32 PRESCIENCE_BUFF        = 410089;       // aura on the ally
constexpr uint32 BREATH_OF_EONS         = 403631;
constexpr uint32 BLISTERING_SCALES      = 360827;
constexpr uint32 UPHEAVAL               = 396286;       // empower - extends Ebon Might
constexpr uint32 DISINTEGRATE           = 356995;       // only without the Eruption override
constexpr uint32 ESSENCE_BURST_AUG      = 392268;       // proc aura - next Eruption costs no Essence
constexpr uint32 TIME_SKIP              = 404977;       // talent - accelerates CDs
constexpr uint32 TIP_THE_SCALES         = 370553;       // talent - free max-rank empower
constexpr uint32 TIME_SPIRAL            = 374968;       // talent - group blink CD

// Utility / CC
constexpr uint32 QUELL                  = 351338;
constexpr uint32 SLEEP_WALK             = 360806;
constexpr uint32 LANDSLIDE              = 358385;       // talent - ground root at target location
constexpr uint32 OBSIDIAN_SCALES        = 363916;       // 30% DR; Renewing Blaze rides on it passively
constexpr uint32 ZEPHYR                 = 374227;
constexpr uint32 VERDANT_EMBRACE        = 360995;
constexpr uint32 HOVER                  = 358267;
constexpr uint32 RESCUE                 = 370665;
constexpr uint32 SOURCE_OF_MAGIC        = 369459;
constexpr uint32 EXPUNGE                = 365585;       // talent - Poison dispel
constexpr uint32 CAUTERIZING_FLAME      = 374251;       // talent - Bleed/Poison/Curse/Disease
constexpr uint32 EMERALD_BLOSSOM        = 355913;       // class baseline castable - self-heal at <=50% HP
constexpr uint32 BLESSING_OF_THE_BRONZE = 364342;

// Lust
constexpr uint32 FURY_OF_THE_ASPECTS    = 390386;
constexpr uint32 SATED_DEBUFF           = 57724;
constexpr uint32 TEMPORAL_DISPL_DEBUFF  = 80354;
constexpr uint32 INSANITY_HUNTER_DEBUFF = 95809;
constexpr uint32 FATIGUED_DEBUFF        = 264689;

// ---- Helpers ----
bool HasLiveTarget(ApPredicateContext const& ctx)
{
    return !ctx.bot.victim().IsEmpty();
}

bool BotHasSatedDebuff(ApPredicateContext const& ctx)
{
    return ctx.bot.has_aura(SATED_DEBUFF)
        || ctx.bot.has_aura(TEMPORAL_DISPL_DEBUFF)
        || ctx.bot.has_aura(INSANITY_HUNTER_DEBUFF)
        || ctx.bot.has_aura(FATIGUED_DEBUFF);
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

// Cauterizing Flame covers Bleed, Poison, Curse and Disease in 12.1.
GroupMemberSummary const* DispelTarget(ApPredicateContext const& ctx)
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

// Expunge only clears Poison.
GroupMemberSummary const* PoisonDispelTarget(ApPredicateContext const& ctx)
{
    return DispelTargetWithPriority(ctx, [](GroupSnapshotView const& g)
        -> GroupMemberSummary const*
    {
        return g.dispel_candidate(DispelType::Poison);
    });
}

// Pick the best Prescience recipient - first non-self DPS in the group,
// fallback to tank, fallback to self.
GroupMemberSummary const* PrescienceTarget(ApPredicateContext const& ctx)
{
    auto const* members = ctx.group.members();
    if (members)
        for (auto const& m : *members)
        {
            if (!m.online || m.hp <= 0) continue;
            if (m.guid == ctx.bot.raw().guid) continue;
            if (m.role == Role::Dps) return &m;
        }
    if (auto const* tank = ctx.group.tank()) return tank;
    return nullptr;
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

// Landslide - ground root in a line towards the target. Panic peel when
// we're being swarmed.
bool ShouldLandslide(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(LANDSLIDE)) return false;
    if (!ctx.bot.is_ready(LANDSLIDE)) return false;
    return ctx.bot.attackers_count() >= 2 && ctx.bot.hp_pct() <= 60;
}
void DoLandslide(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.victim_info())
        e.cast_at(LANDSLIDE, t->x, t->y, t->z);
    else
        e.cast(LANDSLIDE, ctx.bot.victim());
}

// ---- Survival ----
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
    if (ctx.bot.hp_pct() > 55) return false;
    return ctx.bot.interruptible_caster() != nullptr;
}
void DoZephyr(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ZEPHYR); }

bool ShouldVerdantEmbraceSelf(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(VERDANT_EMBRACE)) return false;
    if (!ctx.bot.is_ready(VERDANT_EMBRACE)) return false;
    return ctx.bot.hp_pct() <= 55;
}
void DoVerdantEmbraceSelf(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(VERDANT_EMBRACE, ctx.bot.raw().guid);
}

bool ShouldEmeraldBlossomSelf(ApPredicateContext const& ctx)
{
    // Self-rescue: Augmentation has no real healer-target context here, so
    // Emerald Blossom is gated as a personal panic heal (delayed AoE HoT
    // on our own location).
    if (!ctx.bot.knows_spell(EMERALD_BLOSSOM)) return false;
    if (!ctx.bot.is_ready(EMERALD_BLOSSOM)) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoEmeraldBlossomSelf(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(EMERALD_BLOSSOM, ctx.bot.raw().guid);
}

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

bool ShouldExpunge(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(EXPUNGE)) return false;
    if (!ctx.bot.is_ready(EXPUNGE)) return false;
    return PoisonDispelTarget(ctx) != nullptr;
}
void DoExpunge(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = PoisonDispelTarget(ctx))
        e.cast(EXPUNGE, t->guid);
}

bool ShouldCauterizingFlame(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(CAUTERIZING_FLAME)) return false;
    if (!ctx.bot.is_ready(CAUTERIZING_FLAME)) return false;
    return DispelTarget(ctx) != nullptr;
}
void DoCauterizingFlame(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = DispelTarget(ctx))
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

// ---- Ally maintenance (Augmentation's whole purpose) ----
bool ShouldEbonMight(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(EBON_MIGHT)) return false;
    if (!ctx.bot.is_ready(EBON_MIGHT)) return false;
    AuraEntry const* a = ctx.bot.find_aura(EBON_MIGHT);
    return !a || a->remaining.count() <= 4000;
}
void DoEbonMight(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(EBON_MIGHT); }

bool ShouldPrescience(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(PRESCIENCE)) return false;
    if (!ctx.bot.is_ready(PRESCIENCE)) return false;
    auto const* target = PrescienceTarget(ctx);
    if (!target) return false;
    AuraEntry const* a = ctx.bot.find_aura(PRESCIENCE_BUFF, target->guid);
    return !a || a->remaining.count() <= 4000;
}
void DoPrescience(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* target = PrescienceTarget(ctx))
        e.cast(PRESCIENCE, target->guid);
}

bool ShouldBlisteringScales(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BLISTERING_SCALES)) return false;
    if (!ctx.bot.is_ready(BLISTERING_SCALES)) return false;
    GroupMemberSummary const* tank = ctx.group.tank();
    if (!tank || !tank->online || tank->hp <= 0) return false;
    AuraEntry const* a = ctx.bot.find_aura(BLISTERING_SCALES, tank->guid);
    return !a || a->remaining.count() <= 5000;
}
void DoBlisteringScales(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* tank = ctx.group.tank())
        e.cast(BLISTERING_SCALES, tank->guid);
}

// ---- Major CDs ----
bool ShouldBreathOfEons(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BREATH_OF_EONS)) return false;
    if (!ctx.bot.is_ready(BREATH_OF_EONS)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.attackers_count() >= 3;
}
void DoBreathOfEons(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BREATH_OF_EONS, ctx.bot.victim());
}

bool ShouldTimeSkip(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(TIME_SKIP)) return false;
    if (!ctx.bot.is_ready(TIME_SKIP)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoTimeSkip(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TIME_SKIP); }

bool ShouldTipTheScales(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(TIP_THE_SCALES)) return false;
    if (!ctx.bot.is_ready(TIP_THE_SCALES)) return false;
    // Only worth it when an empower is ready to consume it, and while Ebon
    // Might is up (the max-rank empower extends it the most) or on bosses.
    if (!ctx.bot.is_ready(FIRE_BREATH) && !ctx.bot.is_ready(UPHEAVAL)) return false;
    if (ctx.bot.has_aura(EBON_MIGHT)) return true;
    return BossLikeTargetEngaged(ctx);
}
void DoTipTheScales(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TIP_THE_SCALES); }

bool ShouldTimeSpiral(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(TIME_SPIRAL)) return false;
    if (!ctx.bot.is_ready(TIME_SPIRAL)) return false;
    // Group movement-CD reset - pop on hard wipes.
    return BossLikeTargetEngaged(ctx) && ctx.bot.hp_pct() <= 50;
}
void DoTimeSpiral(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TIME_SPIRAL); }

// ---- Damage rotor ----
// Fire Breath - Empower cone; every cast extends the active Ebon Might, so
// it is used on cooldown regardless of target count.
bool ShouldFireBreath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FIRE_BREATH)) return false;
    return ctx.bot.is_ready(FIRE_BREATH);
}
void DoFireBreath(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FIRE_BREATH); }

bool ShouldUpheaval(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(UPHEAVAL)) return false;
    if (!ctx.bot.is_ready(UPHEAVAL)) return false;
    // Upheaval also extends Ebon Might - use it whenever the buff is up,
    // otherwise save it for packs / bosses.
    if (ctx.bot.has_aura(EBON_MIGHT)) return true;
    return ctx.bot.enemies_within(10.0f) >= 3 || BossLikeTargetEngaged(ctx);
}
void DoUpheaval(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(UPHEAVAL, ctx.bot.victim());
}

bool ShouldEruption(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ERUPTION)) return false;
    if (!ctx.bot.is_ready(ERUPTION)) return false;
    // Eruption is the Ebon Might extender. Cast freely with Essence Burst,
    // otherwise only when we have 3+ Essence to spare.
    if (ctx.bot.has_aura(ESSENCE_BURST_AUG)) return true;
    return ctx.bot.power(POWER_ESSENCE) >= 3;
}
void DoEruption(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ERUPTION, ctx.bot.victim());
}

// Disintegrate is overridden by Eruption once the talent is taken - this
// branch only serves bots without Eruption (two-branch override pattern).
bool ShouldDisintegrate(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(ERUPTION)) return false;
    if (!ctx.bot.knows_spell(DISINTEGRATE)) return false;
    if (!ctx.bot.is_ready(DISINTEGRATE)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(DISINTEGRATE))
        return false;
    return ctx.bot.power(POWER_ESSENCE) >= 3;
}
void DoDisintegrate(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DISINTEGRATE, ctx.bot.victim());
}

bool ShouldLivingFlame(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    return ctx.bot.knows_spell(LIVING_FLAME_AUG);
}
void DoLivingFlame(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LIVING_FLAME_AUG, ctx.bot.victim());
}

bool ShouldAzureStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(AZURE_STRIKE_AUG)) return false;
    return ctx.bot.is_ready(AZURE_STRIKE_AUG);
}
void DoAzureStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(AZURE_STRIKE_AUG, ctx.bot.victim());
}

bool AlwaysAlive(ApPredicateContext const& ctx) { return ctx.bot.is_alive(); }
void DoNothing(ApPredicateContext const&, BotIntentEmitter&) {}

// ---- Rule table ----
// Order: Mobility -> Defensives -> Interrupt/CC -> Group utility -> Lust ->
//        Ally maintenance (Ebon Might / Prescience / Blistering Scales) ->
//        Major CDs -> Damage rotor (Fire Breath / Upheaval empowers ->
//        Eruption ES spender -> Disintegrate (no Eruption) -> Living Flame
//        / Azure Strike fillers).
ApRule const kRules[] = {
    // 1) Mobility.
    { ShouldHover,                DoHover,                "Hover (cast-while-moving)"    },

    // 2) Defensives. Renewing Blaze is a passive rider on Obsidian Scales
    //    in 12.1.
    { ShouldObsidianScales,       DoObsidianScales,       "Obsidian Scales (<=50%)"      },
    { ShouldVerdantEmbraceSelf,   DoVerdantEmbraceSelf,   "Verdant Embrace (<=55%)"      },
    { ShouldEmeraldBlossomSelf,   DoEmeraldBlossomSelf,   "Emerald Blossom (self <=50%)" },
    { ShouldZephyr,               DoZephyr,               "Zephyr (caster <=55%)"        },

    // 3) Interrupt / CC.
    { ShouldQuell,                DoQuell,                "Quell (interrupt)"            },
    { ShouldSleepWalk,            DoSleepWalk,            "Sleep Walk (off-target)"      },
    { ShouldLandslide,            DoLandslide,            "Landslide (2+ root)"          },

    // 4) Group utility.
    { ShouldRescueLowestAlly,     DoRescueLowestAlly,     "Rescue (peel ally <=25%)"     },
    { ShouldExpunge,              DoExpunge,              "Expunge (poison)"             },
    { ShouldCauterizingFlame,     DoCauterizingFlame,     "Cauterizing Flame (cleanse)"  },
    { ShouldSourceOfMagic,        DoSourceOfMagic,        "Source of Magic (caster)"     },
    { ShouldBlessingOfTheBronze,  DoBlessingOfTheBronze,  "Blessing of the Bronze"       },

    // 5) Lust.
    { ShouldFuryOfTheAspects,     DoFuryOfTheAspects,     "Fury of the Aspects (boss)"   },

    // 6) Ally maintenance - this is Augmentation's whole job. Refresh
    //    Ebon Might and Prescience before doing anything else damaging,
    //    since the DPS-aspect of the spec is just a fuel pump for these
    //    two buffs.
    { ShouldEbonMight,            DoEbonMight,            "Ebon Might (refresh)"         },
    { ShouldPrescience,           DoPrescience,           "Prescience (best DPS)"        },
    { ShouldBlisteringScales,     DoBlisteringScales,     "Blistering Scales (tank)"     },

    // 7) Major CDs.
    { ShouldBreathOfEons,         DoBreathOfEons,         "Breath of Eons (boss/3+)"     },
    { ShouldTimeSkip,             DoTimeSkip,             "Time Skip (boss CD reset)"    },
    { ShouldTipTheScales,         DoTipTheScales,         "Tip the Scales (free max)"    },
    { ShouldTimeSpiral,           DoTimeSpiral,           "Time Spiral (group bail)"     },

    // 8) Damage rotor - empowers first (both extend Ebon Might), then the
    //    Essence spender, then fillers.
    { ShouldFireBreath,           DoFireBreath,           "Fire Breath (empower)"        },
    { ShouldUpheaval,             DoUpheaval,             "Upheaval (empower)"           },
    { ShouldEruption,             DoEruption,             "Eruption (Essence spend)"     },
    { ShouldDisintegrate,         DoDisintegrate,         "Disintegrate (no Eruption)"   },
    { ShouldLivingFlame,          DoLivingFlame,          "Living Flame (filler)"        },
    { ShouldAzureStrike,          DoAzureStrike,          "Azure Strike (filler)"        },

    { AlwaysAlive,                DoNothing,              "Idle"                         },
};

} // anonymous

void RegisterApl_Evoker_Augmentation()
{
    constexpr uint32 SPEC_EVOKER_AUGMENTATION = 1473;
    RegisterRotation(CLASS_EVOKER, SPEC_EVOKER_AUGMENTATION, ApRotation{kRules});
}

} // namespace Playerbot::Combat
