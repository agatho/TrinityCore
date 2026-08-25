// Augmentation Evoker - WoW 12.0 enterprise rotation. Support spec that
// buffs allies via Ebon Might (group buff applied to nearby DPS) and
// Prescience (single-target Mastery buff). Damage is secondary — its purpose
// is to feed Essence Burst procs that fuel Eruption (the buff-extender).
//
// Rule ORDER:
//   1) Mobility:              Hover (cast-while-moving)
//   2) Survival:              Renewing Blaze, Obsidian Scales, Verdant
//                             Embrace self, Emerald Blossom self-heal
//                             (<=50%), Zephyr, Emerald Communion (OOC)
//   3) Interrupt / CC:        Quell, Sleep Walk off-target
//   4) Group utility:         Rescue peel, Cauterizing Flame cleanse, Time
//                             Dilation (tank DR), Source of Magic (caster),
//                             Blessing of the Bronze
//   5) Lust:                  Fury of the Aspects (Sated guarded)
//   6) Ally maintenance       Ebon Might (refresh), Prescience on best DPS,
//      (Augmentation's        Blistering Scales on tank
//      whole purpose):
//   7) Major CDs:             Breath of Eons (extends Ebon Might per hit),
//                             Time Skip (talent — accelerates CDs)
//   8) Damage rotor:          Upheaval (3+ AoE), Eruption (Essence spender —
//                             extends Ebon Might/Prescience), Disintegrate
//                             channel (big essence sink), Living Flame
//                             filler (Essence Burst proc), Azure Strike
//
// Validated IDs (cross-referenced against SpellName.csv +
// SpecializationSpells.csv on 2026-05-27):
//   361469 Living Flame           — class baseline
//   362969 Azure Strike           — class baseline
//   395160 Eruption               — Aug Essence spender / Ebon Might extender
//   395152 Ebon Might             — group ally buff (Augmentation's job)
//   410089 Prescience             — single-target Mastery buff
//   403631 Breath of Eons         — Aug burst CD; extends EM per hit
//   360827 Blistering Scales      — tank buff
//   396286 Upheaval               — Empower AoE damage
//   356995 Disintegrate           — channel Essence sink
//   392268 Essence Burst (aura)   — proc aura, makes Eruption cheap/free
//   404977 Time Skip              — talent CD accelerator
//   351338 Quell, 360806 Sleep Walk
//   363916 Obsidian Scales        — CORRECTED from 235450 (Prismatic Barrier)
//   374348 Renewing Blaze
//   374227 Zephyr                 — talent group magic DR
//   360995 Verdant Embrace        — heal
//   365261 Emerald Blossom        — self-heal at <=50% HP (Initial Evoker)
//   358267 Hover, 370665 Rescue
//   369459 Source of Magic
//   374251 Cauterizing Flame
//   370960 Emerald Communion
//   357170 Time Dilation
//   364342 Blessing of the Bronze
//   390386 Fury of the Aspects
//
// Skipped spells (and why — Augmentation has an unusually high passive
// surface area; most of the spec's "abilities" are auras, not casts):
//   361021 Sense Power            — PASSIVE (reveals strongest enemy near
//                                   ally; no cast button)
//   395153 Sands of Time          — PASSIVE (extends Ebon Might / Prescience
//                                   via Eruption — already exposed via the
//                                   Eruption + EM/Prescience refresh rules)
//   396043 Close as Clutchmates   — PASSIVE (Versatility scaling on EM
//                                   targets)
//   406041 Nourishing Sands       — PASSIVE (extends EM heals)
//   406380 Mastery: Timewalker    — PASSIVE (Mastery aura)
//   396186 Augmentation Evoker    — PASSIVE (specialization-defining aura)
//   365262 Improved Emerald Blossom — Preservation passive, irrelevant here

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
constexpr uint32 LIVING_FLAME_AUG       = 361469;
constexpr uint32 AZURE_STRIKE_AUG       = 362969;
constexpr uint32 ERUPTION               = 395160;
constexpr uint32 EBON_MIGHT             = 395152;
constexpr uint32 PRESCIENCE             = 410089;
constexpr uint32 BREATH_OF_EONS         = 403631;
constexpr uint32 BLISTERING_SCALES      = 360827;
constexpr uint32 UPHEAVAL               = 396286;
constexpr uint32 DISINTEGRATE           = 356995;
constexpr uint32 ESSENCE_BURST_AUG      = 392268;       // proc aura — also reduces Eruption cost
constexpr uint32 TIME_SKIP              = 404977;       // talent — accelerates CDs

// Utility / CC
constexpr uint32 QUELL                  = 351338;
constexpr uint32 SLEEP_WALK             = 360806;
constexpr uint32 OBSIDIAN_SCALES        = 363916;       // CORRECTED — 235450 is Mage Prismatic Barrier
constexpr uint32 RENEWING_BLAZE         = 374348;
constexpr uint32 ZEPHYR                 = 374227;
constexpr uint32 VERDANT_EMBRACE        = 360995;
constexpr uint32 HOVER                  = 358267;
constexpr uint32 RESCUE                 = 370665;
constexpr uint32 SOURCE_OF_MAGIC        = 369459;
constexpr uint32 CAUTERIZING_FLAME      = 374251;
constexpr uint32 EMERALD_COMMUNION      = 370960;
constexpr uint32 EMERALD_BLOSSOM        = 365261;       // Initial Evoker — self-heal at <=50% HP
constexpr uint32 TIME_DILATION          = 357170;
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

// Pick the best Prescience recipient — first non-self DPS in the group,
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
    AuraEntry const* a = ctx.bot.find_aura(PRESCIENCE, target->guid);
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

// ---- Damage rotor ----
bool ShouldUpheaval(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(UPHEAVAL)) return false;
    if (!ctx.bot.is_ready(UPHEAVAL)) return false;
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

bool ShouldDisintegrate(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DISINTEGRATE)) return false;
    if (!ctx.bot.is_ready(DISINTEGRATE)) return false;
    return ctx.bot.power(POWER_ESSENCE) >= 4;
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
//        Major CDs -> Damage rotor (Upheaval AoE -> Eruption ES spender ->
//        Disintegrate channel -> Living Flame / Azure Strike fillers).
ApRule const kRules[] = {
    // 1) Mobility.
    { ShouldHover,                DoHover,                "Hover (cast-while-moving)"    },

    // 2) Defensives.
    { ShouldRenewingBlaze,        DoRenewingBlaze,        "Renewing Blaze (<=60%)"       },
    { ShouldObsidianScales,       DoObsidianScales,       "Obsidian Scales (<=50%)"      },
    { ShouldVerdantEmbraceSelf,   DoVerdantEmbraceSelf,   "Verdant Embrace (<=55%)"      },
    { ShouldEmeraldBlossomSelf,   DoEmeraldBlossomSelf,   "Emerald Blossom (self <=50%)" },
    { ShouldZephyr,               DoZephyr,               "Zephyr (caster <=55%)"        },
    { ShouldEmeraldCommunion,     DoEmeraldCommunion,     "Emerald Communion (OOC)"      },

    // 3) Interrupt / CC.
    { ShouldQuell,                DoQuell,                "Quell (interrupt)"            },
    { ShouldSleepWalk,            DoSleepWalk,            "Sleep Walk (off-target)"      },

    // 4) Group utility.
    { ShouldRescueLowestAlly,     DoRescueLowestAlly,     "Rescue (peel ally <=25%)"     },
    { ShouldCauterizingFlame,     DoCauterizingFlame,     "Cauterizing Flame (cleanse)"  },
    { ShouldTimeDilation,         DoTimeDilation,         "Time Dilation (tank <=45%)"   },
    { ShouldSourceOfMagic,        DoSourceOfMagic,        "Source of Magic (caster)"     },
    { ShouldBlessingOfTheBronze,  DoBlessingOfTheBronze,  "Blessing of the Bronze"       },

    // 5) Lust.
    { ShouldFuryOfTheAspects,     DoFuryOfTheAspects,     "Fury of the Aspects (boss)"   },

    // 6) Ally maintenance — this is Augmentation's whole job. Refresh
    //    Ebon Might and Prescience before doing anything else damaging,
    //    since the DPS-aspect of the spec is just a fuel pump for these
    //    two buffs.
    { ShouldEbonMight,            DoEbonMight,            "Ebon Might (refresh)"         },
    { ShouldPrescience,           DoPrescience,           "Prescience (best DPS)"        },
    { ShouldBlisteringScales,     DoBlisteringScales,     "Blistering Scales (tank)"     },

    // 7) Major CDs.
    { ShouldBreathOfEons,         DoBreathOfEons,         "Breath of Eons (boss/3+)"     },
    { ShouldTimeSkip,             DoTimeSkip,             "Time Skip (boss CD reset)"    },

    // 8) Damage rotor.
    { ShouldUpheaval,             DoUpheaval,             "Upheaval (3+ AoE)"            },
    { ShouldEruption,             DoEruption,             "Eruption (Essence spend)"     },
    { ShouldDisintegrate,         DoDisintegrate,         "Disintegrate (Essence sink)"  },
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
