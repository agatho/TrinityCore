// Destruction Warlock - WoW 12.0 baseline rotation. Direct-damage caster
// with Soul Shard fragments + Chaos Bolt spend. Cleave via Havoc on a
// secondary target. Major cooldowns: Summon Infernal, Cataclysm,
// Channel Demonfire.
//
// ---- Validated IDs (SpellName.csv, WoW 12.0) ----------------------------
//   29722  Incinerate                  filler builder
//   348    Immolate                    L1 generic DoT (baseline)
//   193541 Immolate (Destro variant)   modern Destro Immolate id
//   157736 Immolate (periodic aura)    DoT-tick component on target
//   17962  Conflagrate                 instant + Backdraft proc
//   117828 Backdraft                   proc — buffs next Incin / Chaos Bolt
//   116858 Chaos Bolt                  2-shard single-target spender
//   5740   Rain of Fire                3-shard AoE spender
//   1122   Summon Infernal             major CD AoE pet
//   80240  Havoc                       cleave debuff on secondary
//   17877  Shadowburn                  execute talent
//   152108 Cataclysm                   AoE + Immolate spread (talent)
//   196447 Channel Demonfire           channel; needs Immolated target
//   6353   Soul Fire                   high-damage shard generator (talent)
//   196586 Dimensional Rift            talent — opens portal, free damage
//   30283  Shadowfury                  AoE 3s stun
//   710    Banish                      CC Demon/Elemental
//   5782   Fear                        CC humanoid/beast
//   6789   Mortal Coil                 8s fear + 20% heal
//   5484   Howl of Terror              8s AoE fear (10y), 40s CD
//   108416 Dark Pact                   20% HP → 250% absorb
//   234153 Drain Life                  emergency self-heal channel
//   48018  Demonic Circle              utility teleport
//   231811 Soulstone (modern)          combat-rez
//   20707  Soulstone (legacy)          legacy fallback
//   104773 Unending Resolve            40% DR, 3min CD
//   19647  Spell Lock                  Felhunter pet interrupt
//   89766  Axe Toss                    Felguard pet stun
//   17012  Devour Magic                Felhunter pet dispel
//
// ---- Skipped spells (and why) -------------------------------------------
//   - Curse of Tongues (1714) / Exhaustion (334275): situational debuffs;
//     not a fixed-tick rotation slot.
//   - Banish (710), Fear (5782): handled by baseline.
//   - Roaring Blaze (205184): passive talent; no spell to cast.
//   - Summon Imp (688): baseline pet maintenance.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated against SpellName.csv) ----
constexpr uint32 INCINERATE         = 29722;
constexpr uint32 IMMOLATE_DESTRO    = 193541;     // Destro variant
constexpr uint32 IMMOLATE_GENERIC   = 348;        // L1 generic Immolate
constexpr uint32 IMMOLATE_DOT       = 157736;     // periodic aura applied to target
constexpr uint32 CONFLAGRATE        = 17962;
constexpr uint32 CHAOS_BOLT         = 116858;
constexpr uint32 RAIN_OF_FIRE       = 5740;
constexpr uint32 SUMMON_INFERNAL    = 1122;
constexpr uint32 HAVOC              = 80240;
constexpr uint32 SHADOWBURN         = 17877;
constexpr uint32 CATACLYSM          = 152108;
constexpr uint32 CHANNEL_DEMONFIRE  = 196447;
constexpr uint32 SOUL_FIRE          = 6353;
constexpr uint32 DIMENSIONAL_RIFT   = 196586;
constexpr uint32 SHADOWFURY         = 30283;
constexpr uint32 MORTAL_COIL        = 6789;
constexpr uint32 HOWL_OF_TERROR     = 5484;
constexpr uint32 DARK_PACT          = 108416;
constexpr uint32 DRAIN_LIFE         = 234153;
constexpr uint32 DEMONIC_CIRCLE     = 48018;
constexpr uint32 SOULSTONE_MODERN   = 231811;
constexpr uint32 SOULSTONE_LEGACY   = 20707;
constexpr uint32 UNENDING_RESOLVE   = 104773;
constexpr uint32 BACKDRAFT          = 117828;     // proc — buffs Incin/CB
constexpr uint32 PET_SPELL_LOCK     = 19647;      // Felhunter interrupt
constexpr uint32 PET_AXE_TOSS       = 89766;      // Felguard stun
constexpr uint32 PET_DEVOUR_MAGIC   = 17012;      // Felhunter dispel

bool HasLiveTarget(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && !ctx.bot.victim().IsEmpty();
}

bool TargetExecuteRange(ApPredicateContext const& ctx)
{
    NearbyUnit const* t = ctx.bot.victim_info();
    if (!t || t->max_hp <= 0 || t->hp <= 0) return false;
    return (t->hp * 100) / t->max_hp <= 20;
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

// Modern Soulstone preferred over legacy id.
uint32 KnownSoulstone(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(SOULSTONE_MODERN)) return SOULSTONE_MODERN;
    if (ctx.bot.knows_spell(SOULSTONE_LEGACY)) return SOULSTONE_LEGACY;
    return 0;
}

// Resolve which Immolate variant the bot knows. Destro-spec Immolate
// (193541) is preferred; falls back to the generic L1 Immolate (348).
uint32 KnownImmolate(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(IMMOLATE_DESTRO)) return IMMOLATE_DESTRO;
    if (ctx.bot.knows_spell(IMMOLATE_GENERIC)) return IMMOLATE_GENERIC;
    return 0;
}

// True when the bot's Immolate DoT is on the victim (any variant aura id).
bool VictimHasImmolate(ApPredicateContext const& ctx)
{
    if (ctx.bot.find_aura(IMMOLATE_DOT, ctx.bot.victim())) return true;
    if (ctx.bot.find_aura(IMMOLATE_DESTRO, ctx.bot.victim())) return true;
    if (ctx.bot.find_aura(IMMOLATE_GENERIC, ctx.bot.victim())) return true;
    return false;
}

// Pet interrupt — emit both Spell Lock (Felhunter) and Axe Toss (Felguard);
// only the matching pet's ability succeeds.
bool ShouldPetInterrupt(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.has_pet()) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (pvp) return ctx.bot.kick_target(true, 40.0f) != nullptr;
    auto const* c = ctx.bot.interruptible_caster();
    return c && c->guid == ctx.bot.victim();
}
void DoPetInterrupt(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 40.0f))
    {
        e.pet_cast(PET_SPELL_LOCK, c->guid);
        e.pet_cast(PET_AXE_TOSS, c->guid);
    }
}

// Felhunter Devour Magic — pet dispel on self for harmful Magic auras.
bool ShouldPetDevourMagic(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.has_pet()) return false;
    return ctx.bot.self_dispellable(DispelType::Magic);
}
void DoPetDevourMagic(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.pet_cast(PET_DEVOUR_MAGIC, ctx.bot.guid());
}

// ---- Survival ----
bool ShouldUnendingResolve(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(UNENDING_RESOLVE)) return false;
    if (!ctx.bot.is_ready(UNENDING_RESOLVE)) return false;
    return ctx.bot.hp_pct() <= 30;
}
void DoUnendingResolve(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(UNENDING_RESOLVE); }

bool ShouldMortalCoil(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(MORTAL_COIL)) return false;
    if (!ctx.bot.is_ready(MORTAL_COIL)) return false;
    if (ctx.bot.victim().IsEmpty()) return false;
    return ctx.bot.hp_pct() <= 40;
}
void DoMortalCoil(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(MORTAL_COIL, ctx.bot.victim()); }

bool ShouldDarkPact(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DARK_PACT)) return false;
    if (!ctx.bot.is_ready(DARK_PACT)) return false;
    if (ctx.bot.hp_pct() < 30 || ctx.bot.hp_pct() > 75) return false;
    return ctx.bot.attackers_count() >= 1;
}
void DoDarkPact(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DARK_PACT); }

bool ShouldDrainLifeEmergency(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DRAIN_LIFE)) return false;
    if (ctx.bot.hp_pct() > 50) return false;
    if (ctx.bot.knows_spell(UNENDING_RESOLVE) && ctx.bot.is_ready(UNENDING_RESOLVE)) return false;
    if (ctx.bot.knows_spell(MORTAL_COIL) && ctx.bot.is_ready(MORTAL_COIL)) return false;
    if (ctx.bot.knows_spell(DARK_PACT) && ctx.bot.is_ready(DARK_PACT)) return false;
    return true;
}
void DoDrainLifeEmergency(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(DRAIN_LIFE, ctx.bot.victim()); }

bool ShouldHowlOfTerror(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(HOWL_OF_TERROR)) return false;
    if (!ctx.bot.is_ready(HOWL_OF_TERROR)) return false;
    return ctx.bot.enemies_within(10.0f) >= 2 && ctx.bot.hp_pct() <= 70;
}
void DoHowlOfTerror(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HOWL_OF_TERROR); }

// ---- Group utility ----
bool ShouldSoulstone(ApPredicateContext const& ctx)
{
    uint32 sid = KnownSoulstone(ctx);
    if (sid == 0) return false;
    if (!ctx.bot.is_ready(sid)) return false;
    return ctx.group.dead_member(ctx.bot.map_id()) != nullptr;
}
void DoSoulstone(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    uint32 sid = KnownSoulstone(ctx);
    if (sid == 0) return;
    if (auto const* m = ctx.group.dead_member(ctx.bot.map_id()))
        e.cast(sid, m->guid);
}

// ---- CC ----
bool ShouldShadowfury(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOWFURY)) return false;
    if (!ctx.bot.is_ready(SHADOWFURY)) return false;
    return ctx.bot.enemies_within(8.0f) >= 3 && ctx.bot.hp_pct() <= 70;
}
void DoShadowfury(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(SHADOWFURY, v->x, v->y, v->z);
    else
        e.cast(SHADOWFURY);
}

// ---- Major offensive cooldowns ----
bool ShouldSummonInfernal(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SUMMON_INFERNAL)) return false;
    if (!ctx.bot.is_ready(SUMMON_INFERNAL)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(10.0f) >= 3;
}
void DoSummonInfernal(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(SUMMON_INFERNAL, v->x, v->y, v->z);
    else
        e.cast(SUMMON_INFERNAL);
}

bool ShouldCataclysm(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CATACLYSM)) return false;
    if (!ctx.bot.is_ready(CATACLYSM)) return false;
    // 30y AoE that also applies Immolate to every hit. Worth a 30s CD when
    // 2+ targets in range, or always on boss for the immolate refresh.
    return ctx.bot.enemies_within(8.0f) >= 2 || BossLikeTargetEngaged(ctx);
}
void DoCataclysm(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(CATACLYSM, v->x, v->y, v->z);
    else
        e.cast(CATACLYSM);
}

// Dimensional Rift — talent on a charge system; opens a portal that fires
// free damage at the target. Fire any time we're in combat with a live
// target (charges deplete naturally).
bool ShouldDimensionalRift(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DIMENSIONAL_RIFT)) return false;
    return ctx.bot.is_ready(DIMENSIONAL_RIFT);
}
void DoDimensionalRift(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(DIMENSIONAL_RIFT, ctx.bot.victim()); }

// ---- Havoc cleave ----
// Havoc copies Chaos Bolt / Incinerate damage to a secondary target. Apply
// when an OFF-target is alive within 30y so the cleave actually lands.
bool ShouldHavoc(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HAVOC)) return false;
    if (!ctx.bot.is_ready(HAVOC)) return false;
    // Find an off-target enemy in range that doesn't already have Havoc.
    return ctx.bot.enemy_without_my_aura(HAVOC, 30.0f) != nullptr;
}
void DoHavoc(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.enemy_without_my_aura(HAVOC, 30.0f))
        e.cast(HAVOC, t->guid);
}

// ---- DoT maintenance ----
bool ShouldImmolate(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    uint32 sid = KnownImmolate(ctx);
    if (sid == 0) return false;
    if (!ctx.bot.is_ready(sid)) return false;
    // Refresh aware: pandemic 4.5s window for an 18s base DoT.
    AuraEntry const* a = ctx.bot.find_aura(IMMOLATE_DOT, ctx.bot.victim());
    if (!a) a = ctx.bot.find_aura(sid, ctx.bot.victim());
    return !a || a->remaining.count() <= 4500;
}
void DoImmolate(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 sid = KnownImmolate(ctx))
        e.cast(sid, ctx.bot.victim());
}

// ---- Channel Demonfire (channel; only worth it with Immolate up) ----
bool ShouldChannelDemonfire(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CHANNEL_DEMONFIRE)) return false;
    if (!ctx.bot.is_ready(CHANNEL_DEMONFIRE)) return false;
    // Channel only worth firing with Immolate up on at least the victim.
    return VictimHasImmolate(ctx);
}
void DoChannelDemonfire(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(CHANNEL_DEMONFIRE, ctx.bot.victim()); }

// ---- Soul Fire — Destruction's mini-burst shard generator ----
bool ShouldSoulFire(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SOUL_FIRE)) return false;
    if (!ctx.bot.is_ready(SOUL_FIRE)) return false;
    return true;
}
void DoSoulFire(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SOUL_FIRE, ctx.bot.victim()); }

// Conflagrate fires Backdraft proc that speeds the next Incinerate/Chaos
// Bolt. Off-CD on victim — but skip when we already have a Backdraft stack
// so we don't waste the next charge.
bool ShouldConflagrate(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CONFLAGRATE)) return false;
    if (!ctx.bot.is_ready(CONFLAGRATE)) return false;
    // Skip if we already carry an unspent Backdraft proc — let the buff be
    // consumed first by Incinerate / Chaos Bolt.
    if (ctx.bot.has_aura(BACKDRAFT)) return false;
    return true;
}
void DoConflagrate(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(CONFLAGRATE, ctx.bot.victim()); }

bool ShouldShadowburn(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOWBURN)) return false;
    if (!ctx.bot.is_ready(SHADOWBURN)) return false;
    return TargetExecuteRange(ctx);
}
void DoShadowburn(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SHADOWBURN, ctx.bot.victim()); }

// ---- AoE shard spend ----
bool ShouldRainOfFire(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RAIN_OF_FIRE)) return false;
    if (ctx.bot.power(POWER_SOUL_SHARDS) < 3) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(20.0f) >= 3;
}
void DoRainOfFire(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(RAIN_OF_FIRE, v->x, v->y, v->z);
    else
        e.cast(RAIN_OF_FIRE);
}

// ---- Single-target shard spend ----
bool ShouldChaosBolt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CHAOS_BOLT)) return false;
    if (!ctx.bot.is_ready(CHAOS_BOLT)) return false;
    return ctx.bot.power(POWER_SOUL_SHARDS) >= 2;
}
void DoChaosBolt(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(CHAOS_BOLT, ctx.bot.victim()); }

bool ShouldIncinerate(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(INCINERATE)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(INCINERATE)) return false;
    return true;
}
void DoIncinerate(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(INCINERATE, ctx.bot.victim()); }

bool AlwaysAlive(ApPredicateContext const& ctx) { return ctx.bot.is_alive(); }
void DoNothing(ApPredicateContext const&, BotIntentEmitter&) {}

// ---- Rule table ----
// Order (per task spec):
//   1. Unending Resolve   — panic ≤30%
//   2. Drain Life         — emergency self-heal ≤50% when nothing else up
//   3. Howl of Terror     — multi-target panic fear
//   4. Felhunter Devour Magic — pet dispel on self
//   5. Pet interrupt      — Spell Lock / Axe Toss
//   6. Mortal Coil        — heal + horror
//   7. Dark Pact          — absorb shield 30-75% HP
//   8. Soulstone (battle rez)
//   9. Shadowfury (3+ surround stun)
//  10. Summon Infernal (boss / 3+ AoE)
//  11. Cataclysm (AoE + Immolate spread)
//  12. Havoc (cleave secondary)
//  13. Immolate (DoT maintenance — primary)
//  14. Channel Demonfire (Immolate up)
//  15. Dimensional Rift (talent — charges)
//  16. Shadowburn (execute talent)
//  17. Rain of Fire (3+ AoE shard spend)
//  18. Chaos Bolt (2+ shard ST spend)
//  19. Conflagrate (apply Backdraft if absent)
//  20. Soul Fire (shard generator)
//  21. Incinerate (filler)
ApRule const kRules[] = {
    { ShouldUnendingResolve,    DoUnendingResolve,    "Unending Resolve (<=30%)"      },
    { ShouldDrainLifeEmergency, DoDrainLifeEmergency, "Drain Life (emergency)"        },
    { ShouldHowlOfTerror,       DoHowlOfTerror,       "Howl of Terror (AoE fear)"     },
    { ShouldPetDevourMagic,     DoPetDevourMagic,     "Felhunter Devour Magic"        },
    { ShouldPetInterrupt,       DoPetInterrupt,       "Pet interrupt (Spell Lock/Axe Toss)" },
    { ShouldMortalCoil,         DoMortalCoil,         "Mortal Coil (heal+horror)"     },
    { ShouldDarkPact,           DoDarkPact,           "Dark Pact (absorb 30-75%)"     },
    { ShouldSoulstone,          DoSoulstone,          "Soulstone (battle rez)"        },
    { ShouldShadowfury,         DoShadowfury,         "Shadowfury (3+ surround stun)" },
    { ShouldSummonInfernal,     DoSummonInfernal,     "Summon Infernal"               },
    { ShouldCataclysm,          DoCataclysm,          "Cataclysm (AoE + Immolate)"    },
    { ShouldHavoc,              DoHavoc,              "Havoc (cleave secondary)"      },
    { ShouldImmolate,           DoImmolate,           "Immolate (refresh)"            },
    { ShouldChannelDemonfire,   DoChannelDemonfire,   "Channel Demonfire"             },
    { ShouldDimensionalRift,    DoDimensionalRift,    "Dimensional Rift (talent)"     },
    { ShouldShadowburn,         DoShadowburn,         "Shadowburn (<=20%)"            },
    { ShouldRainOfFire,         DoRainOfFire,         "Rain of Fire (3+ AoE)"         },
    { ShouldChaosBolt,          DoChaosBolt,          "Chaos Bolt (2-shard spend)"    },
    { ShouldConflagrate,        DoConflagrate,        "Conflagrate (apply Backdraft)" },
    { ShouldSoulFire,           DoSoulFire,           "Soul Fire (shard generator)"   },
    { ShouldIncinerate,         DoIncinerate,         "Incinerate (filler)"           },
    { AlwaysAlive,              DoNothing,            "Idle"                          },
};

} // anonymous

void RegisterApl_Warlock_Destruction()
{
    constexpr uint32 SPEC_WARLOCK_DESTRUCTION = 267;
    RegisterRotation(CLASS_WARLOCK, SPEC_WARLOCK_DESTRUCTION, ApRotation{kRules});
}

} // namespace Playerbot::Combat
