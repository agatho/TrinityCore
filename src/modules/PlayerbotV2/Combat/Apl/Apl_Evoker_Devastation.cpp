// Devastation Evoker - WoW 12.1.0.69587 (Midnight) rotation. Caster DPS
// resourced by Essence (max 5-6 with talents). Empower spells (Fire Breath,
// Eternity Surge) charge to higher ranks the longer they're held - we treat
// them as instants since the action queue does not expose empower-rank
// cancellation.
//
// Rule ORDER (per APL convention):
//   1) Mobility / Hover (cast-while-moving + speed)
//   2) Defensives: Obsidian Scales (Renewing Blaze rides on it passively),
//                  Verdant Embrace self, Zephyr, Emerald Blossom self-heal
//                  (<=50% HP)
//   3) Interrupt / CC: Quell, Sleep Walk off-target, Tail Swipe panic
//                       knockback, Landslide panic root
//   4) Group utility:  Rescue peel, Expunge / Cauterizing Flame cleanse,
//                       Source of Magic
//   5) Lust:           Fury of the Aspects (Sated guarded)
//   6) Major CDs:      Dragonrage, Tip the Scales, Time Spiral
//   7) AoE windows:    Eternity Surge (Empower, fires Shattering Star via
//                       the passive), Fire Breath (Empower cone), Deep
//                       Breath, Pyre (3+ AoE spend)
//   8) Channel:        Disintegrate (3 Essence or Essence Burst)
//   9) Filler:         Azure Strike, Living Flame
//
// Validated IDs (WoW 12.1.0.69587, cross-referenced against the 12.1 kit:
// SkillLineAbility + SpecializationSpells + simc trait data, 2026-09-09):
//   361469 Living Flame           - class baseline
//   362969 Azure Strike           - class baseline
//   356995 Disintegrate           - class baseline (3 Essence channel)
//   357208 Fire Breath            - class baseline Empower cone (382266 is
//                                   the Font of Magic variant, not castable)
//   359073 Eternity Surge         - spec talent Empower [R]
//   357211 Pyre                   - spec talent Essence spender [R]
//   357210 Deep Breath            - class baseline flyover AoE
//   375087 Dragonrage             - spec talent 120s burst CD [R]
//   370553 Tip the Scales         - class talent, free max empower [R]
//   374968 Time Spiral            - class talent [R]
//   351338 Quell                  - spec talent interrupt (Devastation-only
//                                   in 12.1) [R]
//   360806 Sleep Walk             - class talent off-target CC (not in the
//                                   default build; knows_spell-gated)
//   368970 Tail Swipe             - class baseline knockback
//   358385 Landslide              - class talent ground root [R]
//   363916 Obsidian Scales        - class talent 30% DR [R]
//   374227 Zephyr                 - class talent AoE DR [R]
//   360995 Verdant Embrace        - class talent heal [R]
//   358267 Hover                  - class baseline mobility
//   370665 Rescue                 - class talent peel [R]
//   369459 Source of Magic        - class talent [R]
//   365585 Expunge                - class talent Poison dispel [R]
//   374251 Cauterizing Flame      - class talent Bleed/Poison/Curse/Disease
//                                   cleanse [M]
//   390386 Fury of the Aspects    - class baseline lust (L48)
//   355913 Emerald Blossom        - class baseline castable (365261 is the
//                                   passive rank marker)
//   359618 Essence Burst          - aura only: next Disintegrate/Pyre free
//
// Skipped spells (and why):
//   374348 Renewing Blaze         - passive in 12.1: rides on Obsidian
//                                   Scales, no cast
//   233269 Shattering Star        - no longer castable; 12.1 passive
//                                   Shattering Stars (1265802) fires it from
//                                   Eternity Surge (bolt 1265804)
//   368847 Firestorm              - removed from the Devastation tree
//   357214 Wing Buffet            - removed from the class kit
//   370960 Emerald Communion      - removed from the class tree
//   372048 Oppressing Roar        - not in either curated build
//   406732 Spatial Paradox        - not in either curated build
//   364342 Blessing of the Bronze - movement-CD buff, no combat value
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
constexpr uint32 LIVING_FLAME           = 361469;
constexpr uint32 AZURE_STRIKE           = 362969;
constexpr uint32 FIRE_BREATH            = 357208;       // empower cone (382266 = Font of Magic variant)
constexpr uint32 DISINTEGRATE           = 356995;       // 3 Essence channel
constexpr uint32 ETERNITY_SURGE         = 359073;       // empower - also fires Shattering Star (passive 1265802)
constexpr uint32 PYRE                   = 357211;       // 3 Essence AoE spender
constexpr uint32 DEEP_BREATH            = 357210;       // big AoE flyover
constexpr uint32 DRAGONRAGE             = 375087;       // 120s burst CD
constexpr uint32 TIP_THE_SCALES         = 370553;       // free max-rank empower CD
constexpr uint32 TIME_SPIRAL            = 374968;       // talent - group blink CD
constexpr uint32 QUELL                  = 351338;       // Devastation-only interrupt in 12.1
constexpr uint32 SLEEP_WALK             = 360806;       // off-target incap (not in default build)
constexpr uint32 TAIL_SWIPE             = 368970;       // 8yd knockback
constexpr uint32 LANDSLIDE              = 358385;       // talent - ground root at target location
constexpr uint32 OBSIDIAN_SCALES        = 363916;       // 30% DR; Renewing Blaze rides on it passively
constexpr uint32 ZEPHYR                 = 374227;       // talent - AoE DR group
constexpr uint32 VERDANT_EMBRACE        = 360995;       // self/friendly heal
constexpr uint32 HOVER                  = 358267;       // cast-while-moving + speed
constexpr uint32 RESCUE                 = 370665;       // friendly pull peel
constexpr uint32 SOURCE_OF_MAGIC        = 369459;       // mana regen on caster
constexpr uint32 EXPUNGE                = 365585;       // talent - Poison dispel (default build)
constexpr uint32 CAUTERIZING_FLAME      = 374251;       // talent - Bleed/Poison/Curse/Disease (M+ build)
constexpr uint32 FURY_OF_THE_ASPECTS    = 390386;
constexpr uint32 EMERALD_BLOSSOM        = 355913;       // class baseline castable - self-heal at <=50% HP

// Lust debuffs
constexpr uint32 SATED_DEBUFF           = 57724;
constexpr uint32 TEMPORAL_DISPL_DEBUFF  = 80354;
constexpr uint32 INSANITY_HUNTER_DEBUFF = 95809;
constexpr uint32 FATIGUED_DEBUFF        = 264689;

// Aura tracker
constexpr uint32 ESSENCE_BURST_BUFF     = 359618;       // next Disintegrate / Pyre costs no Essence

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

// Expunge (default build) only clears Poison.
GroupMemberSummary const* PoisonDispelTarget(ApPredicateContext const& ctx)
{
    return DispelTargetWithPriority(ctx, [](GroupSnapshotView const& g)
        -> GroupMemberSummary const*
    {
        return g.dispel_candidate(DispelType::Poison);
    });
}

// ---- Interrupt / CC ----
bool ShouldQuell(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(QUELL)) return false;
    if (!ctx.bot.is_ready(QUELL)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    // PvP: kick any healer/caster in range, even if not our victim. PvE:
    // keep the "must be my victim" gate so dragons don't quell random adds.
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

bool ShouldTailSwipe(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(TAIL_SWIPE)) return false;
    if (!ctx.bot.is_ready(TAIL_SWIPE)) return false;
    return ctx.bot.attackers_count() >= 3 && ctx.bot.hp_pct() <= 50;
}
void DoTailSwipe(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TAIL_SWIPE); }

// Landslide - ground root in a line towards the target. Panic peel when
// we're being swarmed and Tail Swipe is spent (or the pack is at range).
bool ShouldLandslide(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(LANDSLIDE)) return false;
    if (!ctx.bot.is_ready(LANDSLIDE)) return false;
    if (ctx.bot.is_ready(TAIL_SWIPE)) return false;
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
    // Self-rescue: Emerald Blossom is a delayed AoE HoT on the target's
    // location. Devastation doesn't run healer heal-target logic, so we
    // only fire it on ourselves when our own HP drops below half.
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
    if (ctx.bot.has_aura(SOURCE_OF_MAGIC, m->guid)) return false;
    return true;
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

// ---- Major CDs ----
bool ShouldDragonrage(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DRAGONRAGE)) return false;
    if (!ctx.bot.is_ready(DRAGONRAGE)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.attackers_count() >= 3;
}
void DoDragonrage(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DRAGONRAGE); }

bool ShouldTipTheScales(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(TIP_THE_SCALES)) return false;
    if (!ctx.bot.is_ready(TIP_THE_SCALES)) return false;
    // Only worth it when an empower is actually ready to consume it.
    if (!ctx.bot.is_ready(FIRE_BREATH) && !ctx.bot.is_ready(ETERNITY_SURGE)) return false;
    // Pair with Dragonrage burst (ready or active) or boss-tier targets.
    if (ctx.bot.has_aura(DRAGONRAGE)) return true;
    return BossLikeTargetEngaged(ctx);
}
void DoTipTheScales(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TIP_THE_SCALES); }

bool ShouldTimeSpiral(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(TIME_SPIRAL)) return false;
    if (!ctx.bot.is_ready(TIME_SPIRAL)) return false;
    // Group blink CD reset - pop on hard wipes.
    return BossLikeTargetEngaged(ctx) && ctx.bot.hp_pct() <= 50;
}
void DoTimeSpiral(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TIME_SPIRAL); }

bool ShouldDeepBreath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEEP_BREATH)) return false;
    if (!ctx.bot.is_ready(DEEP_BREATH)) return false;
    return ctx.bot.enemies_within(15.0f) >= 3;
}
void DoDeepBreath(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.victim_info())
        e.cast_at(DEEP_BREATH, t->x, t->y, t->z);
    else
        e.cast(DEEP_BREATH, ctx.bot.victim());
}

// ---- Empower windows ----
bool ShouldFireBreath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FIRE_BREATH)) return false;
    return ctx.bot.is_ready(FIRE_BREATH);
}
void DoFireBreath(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FIRE_BREATH); }

bool ShouldEternitySurge(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ETERNITY_SURGE)) return false;
    return ctx.bot.is_ready(ETERNITY_SURGE);
}
void DoEternitySurge(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ETERNITY_SURGE, ctx.bot.victim());
}

// ---- Essence spending ----
bool ShouldPyre(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(PYRE)) return false;
    // Pyre costs 3 Essence in 12.1 unless Essence Burst makes it free.
    if (!ctx.bot.has_aura(ESSENCE_BURST_BUFF) && ctx.bot.power(POWER_ESSENCE) < 3) return false;
    // aoe_preference still requires >=2 enemies - stale `.aoe on` from
    // prior pack shouldn't waste 3 Essence on a single-target Pyre.
    const int near = ctx.bot.enemies_within(15.0f);
    return near >= 3 || (ctx.aoe_preference && near >= 2);
}
void DoPyre(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(PYRE, ctx.bot.victim());
}

bool ShouldDisintegrate(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DISINTEGRATE)) return false;
    if (!ctx.bot.is_ready(DISINTEGRATE)) return false;
    // Disintegrate is a 3s+ channel that breaks on movement -
    // starting it while the bot is moving wastes the cast immediately.
    // Hover (Evoker baseline movement ability) sets can_cast_while_moving
    // on most casts; honor that. Without this check, the rotation
    // queued Disintegrate during repositioning and lost ticks.
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(DISINTEGRATE))
        return false;
    // Essence Burst (359618) makes the next Disintegrate free - spend it.
    if (ctx.bot.has_aura(ESSENCE_BURST_BUFF)) return true;
    return ctx.bot.power(POWER_ESSENCE) >= 3;
}
void DoDisintegrate(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DISINTEGRATE, ctx.bot.victim());
}

// ---- Filler ----
bool ShouldAzureStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(AZURE_STRIKE)) return false;
    return ctx.bot.is_ready(AZURE_STRIKE);
}
void DoAzureStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(AZURE_STRIKE, ctx.bot.victim());
}

bool ShouldLivingFlame(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    return ctx.bot.knows_spell(LIVING_FLAME);
}
void DoLivingFlame(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LIVING_FLAME, ctx.bot.victim());
}

bool AlwaysAlive(ApPredicateContext const& ctx) { return ctx.bot.is_alive(); }
void DoNothing(ApPredicateContext const&, BotIntentEmitter&) {}

// ---- Rule table ----
// Order: Hover -> Defensives -> Interrupt/CC -> Group utility -> Lust ->
//        Major CDs -> AoE empowers -> Channel (Disintegrate) ->
//        Filler (Azure Strike / Living Flame).
ApRule const kRules[] = {
    // 1) Mobility - always first so we keep casting while repositioning.
    { ShouldHover,             DoHover,             "Hover (cast-while-moving)"    },

    // 2) Defensives - fire as soon as HP thresholds trigger. Renewing
    //    Blaze is a passive rider on Obsidian Scales in 12.1.
    { ShouldObsidianScales,    DoObsidianScales,    "Obsidian Scales (<=50%)"      },
    { ShouldVerdantEmbraceSelf,DoVerdantEmbraceSelf,"Verdant Embrace (<=55%)"      },
    { ShouldEmeraldBlossomSelf,DoEmeraldBlossomSelf,"Emerald Blossom (self <=50%)" },
    { ShouldZephyr,            DoZephyr,            "Zephyr (caster <=55%)"        },

    // 3) Interrupt / CC - Quell first, then off-target Sleep Walk, then
    //    panic knockback / root if we're getting swarmed.
    { ShouldQuell,             DoQuell,             "Quell (interrupt)"            },
    { ShouldSleepWalk,         DoSleepWalk,         "Sleep Walk (off-target CC)"   },
    { ShouldTailSwipe,         DoTailSwipe,         "Tail Swipe (3+ knockback)"    },
    { ShouldLandslide,         DoLandslide,         "Landslide (2+ root)"          },

    // 4) Group utility.
    { ShouldRescueLowestAlly,  DoRescueLowestAlly,  "Rescue (peel ally <=25%)"     },
    { ShouldExpunge,           DoExpunge,           "Expunge (poison)"             },
    { ShouldCauterizingFlame,  DoCauterizingFlame,  "Cauterizing Flame (cleanse)"  },
    { ShouldSourceOfMagic,     DoSourceOfMagic,     "Source of Magic (caster)"     },

    // 5) Lust.
    { ShouldFuryOfTheAspects,  DoFuryOfTheAspects,  "Fury of the Aspects (boss)"   },

    // 6) Major CDs - pop on boss-tier engagements.
    { ShouldDragonrage,        DoDragonrage,        "Dragonrage (burst CD)"        },
    { ShouldTipTheScales,      DoTipTheScales,      "Tip the Scales (free max)"    },
    { ShouldTimeSpiral,        DoTimeSpiral,        "Time Spiral (group bail)"     },

    // 7) Empowers + AoE - Eternity Surge first (fires Shattering Star via
    //    the 12.1 passive and scales hardest at high ranks); Fire Breath
    //    cone next; Deep Breath area; Pyre as Essence sink in 3+ packs.
    { ShouldEternitySurge,     DoEternitySurge,     "Eternity Surge (empower)"     },
    { ShouldFireBreath,        DoFireBreath,        "Fire Breath (empower)"        },
    { ShouldDeepBreath,        DoDeepBreath,        "Deep Breath (3+ AoE)"         },
    { ShouldPyre,              DoPyre,              "Pyre (3+ AoE spend)"          },

    // 8) Channel.
    { ShouldDisintegrate,      DoDisintegrate,      "Disintegrate (3 essence)"     },

    // 9) Filler.
    { ShouldAzureStrike,       DoAzureStrike,       "Azure Strike"                 },
    { ShouldLivingFlame,       DoLivingFlame,       "Living Flame (filler)"        },

    { AlwaysAlive,             DoNothing,           "Idle"                         },
};

} // anonymous

void RegisterApl_Evoker_Devastation()
{
    constexpr uint32 SPEC_EVOKER_DEVASTATION = 1467;
    RegisterRotation(CLASS_EVOKER, SPEC_EVOKER_DEVASTATION, ApRotation{kRules});
}

} // namespace Playerbot::Combat
