// Devastation Evoker - WoW 12.0 enterprise rotation. Caster DPS resourced
// by Essence (max 5–6 with talents). Empower spells (Fire Breath, Eternity
// Surge) charge to higher ranks the longer they're held — we treat them as
// instants since the action queue does not expose empower-rank cancellation.
//
// Rule ORDER (per APL convention):
//   1) Mobility / Hover (cast-while-moving + speed)
//   2) Defensives: Obsidian Scales, Renewing Blaze, Verdant Embrace self,
//                  Zephyr, Emerald Blossom self-heal (<=50% HP), Emerald
//                  Communion OOC
//   3) Interrupt / CC: Quell, Sleep Walk off-target, Tail Swipe / Wing
//                       Buffet panic knockbacks
//   4) Group utility:  Rescue peel, Cauterizing Flame cleanse, Source of
//                       Magic
//   5) Lust:           Fury of the Aspects (Sated guarded)
//   6) Major CDs:      Dragonrage, Tip the Scales, Time Spiral
//   7) AoE windows:    Eternity Surge (Empower AoE), Fire Breath (Empower
//                       cone), Firestorm, Deep Breath, Pyre (3+ AoE spend)
//   8) ST burst:       Shattering Star (debuff)
//   9) Channel:        Disintegrate (3+ Essence)
//  10) Filler:         Azure Strike, Living Flame
//
// Validated IDs (cross-referenced against SpellName.csv +
// SpecializationSpells.csv + SkillLineAbility.csv on 2026-05-27):
//   361469 Living Flame           — class baseline
//   362969 Azure Strike           — class baseline
//   356995 Disintegrate           — class baseline
//   382266 Fire Breath            — Empower (Devastation specialization
//                                   override; 357208 is the class-baseline
//                                   variant — we use the spec ID)
//   359073 Eternity Surge         — Empower (Devastation)
//   357211 Pyre                   — Devastation Essence spender
//   233269 Shattering Star        — CORRECTED from 370452 (which does not
//                                   exist in SpellName.csv — 233269 is the
//                                   canonical Devastation talent)
//   368847 Firestorm              — talent
//   357210 Deep Breath            — class baseline
//   375087 Dragonrage             — Devastation CD
//   370553 Tip the Scales         — CD
//   374968 Time Spiral            — talent
//   351338 Quell                  — interrupt
//   360806 Sleep Walk             — off-target CC
//   368970 Tail Swipe             — knockback
//   357214 Wing Buffet            — knockback
//   363916 Obsidian Scales        — CORRECTED from 235450 (which is
//                                   Prismatic Barrier / Mage)
//   374348 Renewing Blaze         — self HoT
//   374227 Zephyr                 — talent group magic DR
//   360995 Verdant Embrace        — heal
//   358267 Hover                  — mobility + cast-while-moving
//   370665 Rescue                 — peel
//   369459 Source of Magic        — mana regen on caster
//   374251 Cauterizing Flame      — cleanse
//   390386 Fury of the Aspects    — lust
//   365261 Emerald Blossom        — self-heal at ≤50% HP (Initial Evoker
//                                   grant — class baseline as of 12.0)
//   370960 Emerald Communion      — talent OOC heal+essence
//
// Skipped spells (and why):
//   355913 Emerald Blossom        — older class-baseline ID; 365261 is the
//                                   modern Initial Evoker grant. Keep one.
//   355627 Azure Strike           — alternate ID; class baseline uses
//                                   362969.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "ApDispelHelpers.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated) ----
constexpr uint32 LIVING_FLAME           = 361469;
constexpr uint32 AZURE_STRIKE           = 362969;
constexpr uint32 FIRE_BREATH            = 382266;
constexpr uint32 DISINTEGRATE           = 356995;
constexpr uint32 ETERNITY_SURGE         = 359073;
constexpr uint32 PYRE                   = 357211;
constexpr uint32 SHATTERING_STAR        = 233269;       // talent — debuff + chargen
constexpr uint32 FIRESTORM              = 368847;       // talent — ground AoE
constexpr uint32 DEEP_BREATH            = 357210;       // big AoE flyover
constexpr uint32 DRAGONRAGE             = 375087;       // 30s burst CD
constexpr uint32 TIP_THE_SCALES         = 370553;       // free max-rank empower CD
constexpr uint32 TIME_SPIRAL            = 374968;       // talent — group blink CD
constexpr uint32 QUELL                  = 351338;
constexpr uint32 SLEEP_WALK             = 360806;       // off-target incap
constexpr uint32 TAIL_SWIPE             = 368970;       // 8yd cone knockback
constexpr uint32 WING_BUFFET            = 357214;       // frontal cone knockback
constexpr uint32 OBSIDIAN_SCALES        = 363916;       // CORRECTED — 235450 is Mage Prismatic Barrier
constexpr uint32 RENEWING_BLAZE         = 374348;
constexpr uint32 ZEPHYR                 = 374227;       // talent — magic DR group
constexpr uint32 VERDANT_EMBRACE        = 360995;       // self/friendly heal
constexpr uint32 HOVER                  = 358267;       // cast-while-moving + speed
constexpr uint32 RESCUE                 = 370665;       // friendly pull peel
constexpr uint32 SOURCE_OF_MAGIC        = 369459;       // mana regen on caster
constexpr uint32 CAUTERIZING_FLAME      = 374251;       // friendly cleanse Disease+Poison+Bleed
constexpr uint32 FURY_OF_THE_ASPECTS    = 390386;
constexpr uint32 EMERALD_BLOSSOM        = 365261;       // Initial Evoker grant — self-heal at <=50% HP
constexpr uint32 EMERALD_COMMUNION      = 370960;       // talent — full mana/essence channel

// Lust debuffs
constexpr uint32 SATED_DEBUFF           = 57724;
constexpr uint32 TEMPORAL_DISPL_DEBUFF  = 80354;
constexpr uint32 INSANITY_HUNTER_DEBUFF = 95809;
constexpr uint32 FATIGUED_DEBUFF        = 264689;

// Aura tracker
constexpr uint32 SHATTERING_STAR_DEBUFF = 233269;       // same id

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

GroupMemberSummary const* DispelTarget(ApPredicateContext const& ctx)
{
    return DispelTargetWithPriority(ctx, [](GroupSnapshotView const& g)
        -> GroupMemberSummary const*
    {
        if (auto const* m = g.dispel_candidate(DispelType::Disease)) return m;
        if (auto const* m = g.dispel_candidate(DispelType::Poison))  return m;
        return nullptr;
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

bool ShouldWingBuffet(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(WING_BUFFET)) return false;
    if (!ctx.bot.is_ready(WING_BUFFET)) return false;
    if (ctx.bot.is_ready(TAIL_SWIPE)) return false;
    return ctx.bot.attackers_count() >= 2 && ctx.bot.hp_pct() <= 60;
}
void DoWingBuffet(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(WING_BUFFET); }

// ---- Survival ----
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

bool ShouldEmeraldCommunion(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(EMERALD_COMMUNION)) return false;
    if (!ctx.bot.is_ready(EMERALD_COMMUNION)) return false;
    if (ctx.bot.in_combat()) return false;
    if (ctx.bot.max_power(0) <= 0) return false;
    return ctx.bot.power_pct(0) <= 30 || ctx.bot.hp_pct() <= 50;
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
    if (ctx.bot.has_aura(SOURCE_OF_MAGIC, m->guid)) return false;
    return true;
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
    // Group blink CD reset — pop on hard wipes.
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
bool ShouldShatteringStar(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHATTERING_STAR)) return false;
    if (!ctx.bot.is_ready(SHATTERING_STAR)) return false;
    AuraEntry const* a = ctx.bot.find_aura(SHATTERING_STAR_DEBUFF, ctx.bot.victim());
    return !a || a->remaining.count() <= 1500;
}
void DoShatteringStar(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHATTERING_STAR, ctx.bot.victim());
}

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

bool ShouldFirestorm(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FIRESTORM)) return false;
    if (!ctx.bot.is_ready(FIRESTORM)) return false;
    return ctx.bot.enemies_within(15.0f) >= 2;
}
void DoFirestorm(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.victim_info())
        e.cast_at(FIRESTORM, t->x, t->y, t->z);
    else
        e.cast(FIRESTORM, ctx.bot.victim());
}

// ---- Essence spending ----
bool ShouldPyre(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(PYRE)) return false;
    if (ctx.bot.power(POWER_ESSENCE) < 2) return false;
    // aoe_preference still requires ≥2 enemies — stale `.aoe on` from
    // prior pack shouldn't waste 2 Essence on a single-target Pyre.
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
    // Disintegrate is a 3s+ channel that breaks on movement —
    // starting it while the bot is moving wastes the cast immediately.
    // Hover (Evoker baseline movement ability) sets can_cast_while_moving
    // on most casts; honor that. Without this check, the rotation
    // queued Disintegrate during repositioning and lost ticks.
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(DISINTEGRATE))
        return false;
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
//        Major CDs -> AoE empowers -> ST burst (Shattering Star) ->
//        Channel (Disintegrate) -> Filler (Azure Strike / Living Flame).
ApRule const kRules[] = {
    // 1) Mobility — always first so we keep casting while repositioning.
    { ShouldHover,             DoHover,             "Hover (cast-while-moving)"    },

    // 2) Defensives — fire as soon as HP/mana thresholds trigger.
    { ShouldObsidianScales,    DoObsidianScales,    "Obsidian Scales (<=50%)"      },
    { ShouldRenewingBlaze,     DoRenewingBlaze,     "Renewing Blaze (<=60%)"       },
    { ShouldVerdantEmbraceSelf,DoVerdantEmbraceSelf,"Verdant Embrace (<=55%)"      },
    { ShouldEmeraldBlossomSelf,DoEmeraldBlossomSelf,"Emerald Blossom (self <=50%)" },
    { ShouldZephyr,            DoZephyr,            "Zephyr (caster <=55%)"        },
    { ShouldEmeraldCommunion,  DoEmeraldCommunion,  "Emerald Communion (OOC heal)" },

    // 3) Interrupt / CC — Quell first, then off-target Sleep Walk, then
    //    panic knockbacks if we're getting swarmed.
    { ShouldQuell,             DoQuell,             "Quell (interrupt)"            },
    { ShouldSleepWalk,         DoSleepWalk,         "Sleep Walk (off-target CC)"   },
    { ShouldTailSwipe,         DoTailSwipe,         "Tail Swipe (3+ knockback)"    },
    { ShouldWingBuffet,        DoWingBuffet,        "Wing Buffet (2+ knockback)"   },

    // 4) Group utility.
    { ShouldRescueLowestAlly,  DoRescueLowestAlly,  "Rescue (peel ally <=25%)"     },
    { ShouldCauterizingFlame,  DoCauterizingFlame,  "Cauterizing Flame (cleanse)"  },
    { ShouldSourceOfMagic,     DoSourceOfMagic,     "Source of Magic (caster)"     },

    // 5) Lust.
    { ShouldFuryOfTheAspects,  DoFuryOfTheAspects,  "Fury of the Aspects (boss)"   },

    // 6) Major CDs — pop on boss-tier engagements.
    { ShouldDragonrage,        DoDragonrage,        "Dragonrage (burst CD)"        },
    { ShouldTipTheScales,      DoTipTheScales,      "Tip the Scales (free max)"    },
    { ShouldTimeSpiral,        DoTimeSpiral,        "Time Spiral (group bail)"     },

    // 7) AoE windows — Eternity Surge + Fire Breath are Empowers, Firestorm
    //    + Deep Breath + Pyre are AoE spenders. Eternity Surge first since
    //    it scales hardest at high empower ranks; Fire Breath cone next;
    //    Firestorm / Deep Breath area; Pyre as Essence sink in 3+ packs.
    { ShouldEternitySurge,     DoEternitySurge,     "Eternity Surge (empower)"     },
    { ShouldFireBreath,        DoFireBreath,        "Fire Breath (empower)"        },
    { ShouldFirestorm,         DoFirestorm,         "Firestorm (2+ AoE)"           },
    { ShouldDeepBreath,        DoDeepBreath,        "Deep Breath (3+ AoE)"         },
    { ShouldPyre,              DoPyre,              "Pyre (3+ AoE spend)"          },

    // 8) ST burst.
    { ShouldShatteringStar,    DoShatteringStar,    "Shattering Star"              },

    // 9) Channel.
    { ShouldDisintegrate,      DoDisintegrate,      "Disintegrate (3 essence)"     },

    // 10) Filler.
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
