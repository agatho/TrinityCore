// Affliction Warlock - WoW 12.0 baseline rotation. Caster DPS, mana +
// soul shards. DoT spread + Malefic Rapture spend cycle.
//
// Multi-DoT model: the bot maintains Agony / Corruption / Unstable Affliction
// on the primary target, then expands Agony + Corruption to every visible
// add via the snapshot's `enemy_without_my_aura` helper. UA stays single-
// target (it's a punishing recast on multi). The expansion rules sit
// between the primary refresh rules and the spend/filler rules so a fresh
// add is dotted before any Soul Shard is spent on Malefic Rapture.
//
// ---- Validated IDs (SpellName.csv, WoW 12.0) ----------------------------
//   686    Shadow Bolt                 filler builder
//   172    Corruption                  legacy id (fallback)
//   146739 Corruption                  modern Aff aura (preferred)
//   980    Agony                       primary shard-generator DoT
//   316099 Unstable Affliction         single-target burst DoT
//   48181  Haunt                       single-target debuff CD
//   324536 Malefic Rapture             shard spender
//   198590 Drain Soul                  channel filler / execute
//   27243  Seed of Corruption          AoE bomb spender
//   205179 Phantom Singularity         ground DoT, talent
//   278350 Vile Taint                  ground Agony AoE, talent
//   205180 Summon Darkglare            major CD (DoT extender)
//   236235 Soul Rot                    talent CD; DoT on target + spreads
//   113860 Dark Soul: Misery           talent CD buff (haste)
//   234153 Drain Life                  emergency self-heal channel
//   6789   Mortal Coil                 8s fear + 20% heal
//   5484   Howl of Terror              8s AoE fear (10y), 40s CD
//   108416 Dark Pact                   absorb shield
//   48018  Demonic Circle              utility teleport
//   111771 Demonic Gateway             group teleport
//   231811 Soulstone                   modern combat-rez (preferred)
//   20707  Soulstone (legacy)          legacy fallback id
//   104773 Unending Resolve            40% DR, 3min CD
//
// ---- Pet abilities (cast through pet_cast) ------------------------------
//   19647  Spell Lock                  Felhunter interrupt/silence
//   89766  Axe Toss                    Felguard stun (not Aff default but
//                                       supported when player swapped pet)
//   17012  Devour Magic                Felhunter dispel (cleanses bot's
//                                       harmful Magic auras)
//
// ---- Skipped spells (and why) -------------------------------------------
//   - Banish (710), Fear (5782): handled by baseline Warlock rotation.
//   - Curse of Tongues (1714) / Exhaustion (334275): situational debuffs;
//     not on a fixed-tick rotation.
//   - Summon Imp (688): handled by baseline (pet maintenance).

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated against SpellName.csv) ----
constexpr uint32 SHADOW_BOLT          = 686;
constexpr uint32 CORRUPTION_MODERN    = 146739;     // modern Aff Corruption
constexpr uint32 CORRUPTION_LEGACY    = 172;        // legacy fallback
constexpr uint32 AGONY                = 980;
constexpr uint32 UNSTABLE_AFFLICTION  = 316099;
constexpr uint32 HAUNT                = 48181;
constexpr uint32 MALEFIC_RAPTURE      = 324536;
constexpr uint32 DRAIN_SOUL           = 198590;
constexpr uint32 SEED_OF_CORRUPTION   = 27243;
constexpr uint32 PHANTOM_SINGULARITY  = 205179;
constexpr uint32 VILE_TAINT           = 278350;
constexpr uint32 SUMMON_DARKGLARE     = 205180;
constexpr uint32 SOUL_ROT             = 236235;     // talent CD DoT (60s CD)
constexpr uint32 DARK_SOUL_MISERY     = 113860;     // talent 60s haste CD
constexpr uint32 DRAIN_LIFE           = 234153;
constexpr uint32 MORTAL_COIL          = 6789;
constexpr uint32 HOWL_OF_TERROR       = 5484;
constexpr uint32 DARK_PACT            = 108416;
constexpr uint32 DEMONIC_CIRCLE       = 48018;
constexpr uint32 DEMONIC_GATEWAY      = 111771;
constexpr uint32 SOULSTONE_MODERN     = 231811;     // modern combat-rez
constexpr uint32 SOULSTONE_LEGACY     = 20707;      // legacy fallback
constexpr uint32 UNENDING_RESOLVE     = 104773;

// ---- Pet abilities (cast through PetCastSpellIntent) ----
constexpr uint32 PET_SPELL_LOCK       = 19647;      // Felhunter
constexpr uint32 PET_AXE_TOSS         = 89766;      // Felguard
constexpr uint32 PET_DEVOUR_MAGIC     = 17012;      // Felhunter dispel

// ---- Helpers ----
bool HasLiveTarget(ApPredicateContext const& ctx)
{
    return !ctx.bot.victim().IsEmpty();
}

bool TargetExecuteRange(ApPredicateContext const& ctx)
{
    NearbyUnit const* t = ctx.bot.victim_info();
    if (!t || t->max_hp <= 0 || t->hp <= 0) return false;
    return (t->hp * 100) / t->max_hp <= 20;
}

// Refresh threshold for DoTs: 30% of base duration is the standard pandemic
// window in modern WoW. Using 4s here covers all the Aff DoTs (Agony 18s,
// Corruption 14s, UA 16s) without burning mana on early refreshes.
bool MissingDot(ApPredicateContext const& ctx, uint32 dot)
{
    AuraEntry const* a = ctx.bot.find_aura(dot, ctx.bot.victim());
    return !a || a->remaining.count() <= 4000;
}

// Resolve which Corruption variant the bot actually knows. Modern wins.
uint32 KnownCorruption(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(CORRUPTION_MODERN)) return CORRUPTION_MODERN;
    if (ctx.bot.knows_spell(CORRUPTION_LEGACY)) return CORRUPTION_LEGACY;
    return 0;
}

// Resolve which Soulstone variant the bot actually knows. Modern (231811)
// is preferred; older learnsets still resolve only 20707.
uint32 KnownSoulstone(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(SOULSTONE_MODERN)) return SOULSTONE_MODERN;
    if (ctx.bot.knows_spell(SOULSTONE_LEGACY)) return SOULSTONE_LEGACY;
    return 0;
}

// Boss-tier target for major-CD timing. Avoids blowing Darkglare on trash.
bool BossLikeTargetEngaged(ApPredicateContext const& ctx)
{
    constexpr int32 kBossHpThreshold = 5'000'000;
    NearbyUnit const* t = ctx.bot.victim_info();
    if (t && t->max_hp >= kBossHpThreshold) return true;
    for (auto const& a : ctx.bot.raw().combat.attackers)
        if (a.max_hp >= kBossHpThreshold) return true;
    return false;
}

// Pet interrupt — Felhunter's Spell Lock (24s CD, no school lockout in 12.0).
// We only fire when the bot has a pet summoned and the caster is targeting
// our victim or a nearby threat. Pet abilities cast via PetCastSpellIntent,
// NOT bot-side cast(); the API resolves the pet and issues the cast through
// pet->CastSpell. Emitting both Spell Lock + Axe Toss means whichever pet is
// out lands the matching one — the executor returns Locked on the mismatch.
bool ShouldPetSpellLock(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.has_pet()) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (pvp) return ctx.bot.kick_target(true, 40.0f) != nullptr;
    auto const* c = ctx.bot.interruptible_caster();
    return c && c->guid == ctx.bot.victim();
}
void DoPetSpellLock(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 40.0f))
    {
        e.pet_cast(PET_SPELL_LOCK, c->guid);
        e.pet_cast(PET_AXE_TOSS, c->guid);
    }
}

// Felhunter Devour Magic — pet dispel. The bot's pet eats one Magic aura
// off the bot, healing the pet for ~10% of its max HP. Fire when the bot
// carries a harmful Magic aura. Cheap to attempt: the call no-ops if the
// summoned pet isn't a Felhunter.
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

// ---- Predicates ----
bool ShouldUnendingResolve(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(UNENDING_RESOLVE)) return false;
    if (!ctx.bot.is_ready(UNENDING_RESOLVE)) return false;
    return ctx.bot.hp_pct() <= 30;
}
void DoUnendingResolve(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(UNENDING_RESOLVE); }

bool ShouldDarkPact(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DARK_PACT)) return false;
    if (!ctx.bot.is_ready(DARK_PACT)) return false;
    // Burns 20% of current HP for an absorb shield. Only fire when we have
    // headroom AND we're being beaten on. Not on full HP (waste) and not
    // sub-30% (the 20% sacrifice would take us into Mortal Coil territory).
    if (ctx.bot.hp_pct() < 30 || ctx.bot.hp_pct() > 75) return false;
    return ctx.bot.attackers_count() >= 1;
}
void DoDarkPact(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DARK_PACT); }

bool ShouldDrainLifeEmergency(ApPredicateContext const& ctx)
{
    // Emergency self-heal channel — only when we're critically low AND no
    // instant defensive is up. The 5s channel pins the bot in place so we
    // gate harder than the baseline rule (≤50%).
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DRAIN_LIFE)) return false;
    if (ctx.bot.hp_pct() > 50) return false;
    if (ctx.bot.knows_spell(UNENDING_RESOLVE) && ctx.bot.is_ready(UNENDING_RESOLVE)) return false;
    if (ctx.bot.knows_spell(MORTAL_COIL) && ctx.bot.is_ready(MORTAL_COIL)) return false;
    if (ctx.bot.knows_spell(DARK_PACT) && ctx.bot.is_ready(DARK_PACT)) return false;
    return true;
}
void DoDrainLifeEmergency(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(DRAIN_LIFE, ctx.bot.victim()); }

bool ShouldMortalCoil(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(MORTAL_COIL)) return false;
    if (!ctx.bot.is_ready(MORTAL_COIL)) return false;
    if (ctx.bot.victim().IsEmpty()) return false;
    // Use as emergency self-heal when low. Fear is incidental.
    return ctx.bot.hp_pct() <= 40;
}
void DoMortalCoil(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(MORTAL_COIL, ctx.bot.victim()); }

// Howl of Terror — 10y AoE fear, 40s CD. Multi-target panic CC: when we're
// surrounded AND taking damage, fear the pack so we can reposition. Gated
// on HP < 70% to avoid wasting the CD opening on a single pack of trash.
bool ShouldHowlOfTerror(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(HOWL_OF_TERROR)) return false;
    if (!ctx.bot.is_ready(HOWL_OF_TERROR)) return false;
    return ctx.bot.enemies_within(10.0f) >= 2 && ctx.bot.hp_pct() <= 70;
}
void DoHowlOfTerror(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HOWL_OF_TERROR); }

bool ShouldSoulstone(ApPredicateContext const& ctx)
{
    if (KnownSoulstone(ctx) == 0) return false;
    uint32 sid = KnownSoulstone(ctx);
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

// Dark Soul: Misery — 60s burst CD; massive haste buff for 20s. Fire on a
// boss with all DoTs ramped so the buffed window lands on full ticks.
bool ShouldDarkSoulMisery(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DARK_SOUL_MISERY)) return false;
    if (!ctx.bot.is_ready(DARK_SOUL_MISERY)) return false;
    if (!BossLikeTargetEngaged(ctx)) return false;
    uint32 cid = KnownCorruption(ctx);
    return !MissingDot(ctx, AGONY) && (cid && !MissingDot(ctx, cid));
}
void DoDarkSoulMisery(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DARK_SOUL_MISERY); }

// Soul Rot — 60s CD; single-target spread DoT (8 sec, splashes to nearby
// enemies). Fire on boss or when 2+ enemies clustered.
bool ShouldSoulRot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SOUL_ROT)) return false;
    if (!ctx.bot.is_ready(SOUL_ROT)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(15.0f) >= 2;
}
void DoSoulRot(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SOUL_ROT, ctx.bot.victim()); }

bool ShouldSummonDarkglare(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SUMMON_DARKGLARE)) return false;
    if (!ctx.bot.is_ready(SUMMON_DARKGLARE)) return false;
    // 3min CD — only blow on a boss with all DoTs ramped (Darkglare extends
    // existing DoT durations; firing without DoTs up wastes the cooldown).
    if (!BossLikeTargetEngaged(ctx)) return false;
    uint32 cid = KnownCorruption(ctx);
    if (cid == 0) return false;
    return !MissingDot(ctx, AGONY) && !MissingDot(ctx, cid) && !MissingDot(ctx, UNSTABLE_AFFLICTION);
}
void DoSummonDarkglare(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SUMMON_DARKGLARE, ctx.bot.victim());
}

bool ShouldVileTaint(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(VILE_TAINT)) return false;
    if (!ctx.bot.is_ready(VILE_TAINT)) return false;
    if (ctx.bot.power(POWER_SOUL_SHARDS) < 1) return false;
    // Ground-target Agony AoE — worth a shard only when 3+ in range.
    return ctx.bot.enemies_within(10.0f) >= 3;
}
void DoVileTaint(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(VILE_TAINT, v->x, v->y, v->z);
    else
        e.cast(VILE_TAINT);
}

bool ShouldPhantomSingularity(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(PHANTOM_SINGULARITY)) return false;
    if (!ctx.bot.is_ready(PHANTOM_SINGULARITY)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(15.0f) >= 2;
}
void DoPhantomSingularity(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(PHANTOM_SINGULARITY, v->x, v->y, v->z);
    else
        e.cast(PHANTOM_SINGULARITY);
}

bool ShouldHaunt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HAUNT)) return false;
    if (!ctx.bot.is_ready(HAUNT)) return false;
    // Single-target damage-amp CD — only worth it on a boss-tier target with
    // the primary DoTs already ramped (same gate as Darkglare / Dark Soul).
    // Firing on trash wastes the cooldown and the amplification window.
    if (!BossLikeTargetEngaged(ctx)) return false;
    uint32 cid = KnownCorruption(ctx);
    if (cid == 0) return false;
    return !MissingDot(ctx, AGONY) && !MissingDot(ctx, cid);
}
void DoHaunt(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(HAUNT, ctx.bot.victim()); }

bool ShouldUnstableAffliction(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(UNSTABLE_AFFLICTION)) return false;
    return MissingDot(ctx, UNSTABLE_AFFLICTION);
}
void DoUnstableAffliction(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(UNSTABLE_AFFLICTION, ctx.bot.victim()); }

bool ShouldAgonyPrimary(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(AGONY)) return false;
    return MissingDot(ctx, AGONY);
}
void DoAgonyPrimary(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(AGONY, ctx.bot.victim()); }

bool ShouldCorruptionPrimary(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    uint32 cid = KnownCorruption(ctx);
    if (cid == 0) return false;
    return MissingDot(ctx, cid);
}
void DoCorruptionPrimary(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 cid = KnownCorruption(ctx))
        e.cast(cid, ctx.bot.victim());
}

// Multi-dot expansion: spread Agony + Corruption to off-target adds. Skips
// the primary victim (handled by the primary refresh rules above) and any
// add already carrying our own DoT. Returns nullptr when nothing to do —
// which means the spec falls through to spend/filler rules. Range capped at
// 40yd to match the cast range of both DoTs.
NearbyUnit const* OffDotExpansionTarget(ApPredicateContext const& ctx, uint32 dot)
{
    return ctx.bot.enemy_without_my_aura(dot, 40.0f);
}

bool ShouldAgonyExpand(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(AGONY)) return false;
    return OffDotExpansionTarget(ctx, AGONY) != nullptr;
}
void DoAgonyExpand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = OffDotExpansionTarget(ctx, AGONY))
        e.cast(AGONY, t->guid);
}

bool ShouldCorruptionExpand(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    uint32 cid = KnownCorruption(ctx);
    if (cid == 0) return false;
    return OffDotExpansionTarget(ctx, cid) != nullptr;
}
void DoCorruptionExpand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    uint32 cid = KnownCorruption(ctx);
    if (cid == 0) return;
    if (auto const* t = OffDotExpansionTarget(ctx, cid))
        e.cast(cid, t->guid);
}

bool ShouldMaleficRapture(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MALEFIC_RAPTURE)) return false;
    if (!ctx.bot.is_ready(MALEFIC_RAPTURE)) return false;
    if (ctx.bot.power(POWER_SOUL_SHARDS) < 3) return false;
    // Spender; require Agony + Corruption up so MR ticks at least 2 dots.
    uint32 cid = KnownCorruption(ctx);
    if (cid == 0) return false;
    return !MissingDot(ctx, AGONY) && !MissingDot(ctx, cid);
}
void DoMaleficRapture(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(MALEFIC_RAPTURE, ctx.bot.victim()); }

bool ShouldDrainSoul(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DRAIN_SOUL)) return false;
    return TargetExecuteRange(ctx);
}
void DoDrainSoul(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(DRAIN_SOUL, ctx.bot.victim()); }

bool ShouldShadowBolt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOW_BOLT)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(SHADOW_BOLT)) return false;
    return true;
}
void DoShadowBolt(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SHADOW_BOLT, ctx.bot.victim()); }

bool ShouldSeedOfCorruption(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SEED_OF_CORRUPTION)) return false;
    if (ctx.bot.power(POWER_SOUL_SHARDS) < 1) return false;
    // Seed costs a shard and seeds a Corruption-style DoT that explodes;
    // worth it only when 3+ enemies cluster so the explosion procs.
    return ctx.aoe_preference || ctx.bot.enemies_within(20.0f) >= 3;
}
void DoSeedOfCorruption(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SEED_OF_CORRUPTION, ctx.bot.victim()); }

bool AlwaysAlive(ApPredicateContext const& ctx) { return ctx.bot.is_alive(); }
void DoNothing(ApPredicateContext const&, BotIntentEmitter&) {}

// ---- Rule table ----
// Order (per task spec):
//   1. Unending Resolve   — panic ≤30%
//   2. Drain Life         — emergency self-heal ≤50% when nothing else up
//   3. Mortal Coil        — heal + horror CC
//   4. Howl of Terror     — multi-target panic fear
//   5. Felhunter Devour Magic — pet dispel on self (Magic auras)
//   6. Felhunter Spell Lock   — pet interrupt
//   7. Dark Pact          — absorb shield (30-75% HP)
//   8. Group utility — Soulstone rez
//   9. Major offensive CDs — Dark Soul: Misery, Summon Darkglare,
//      Soul Rot (ramped/clustered)
//  10. Off-GCD ground AoE — Vile Taint, Phantom Singularity
//  11. Single-target debuff CD — Haunt
//  12. Primary DoT maintenance — UA, Agony, Corruption
//  13. Off-target DoT expansion — Agony + Corruption to adds
//  14. AoE shard spend — Seed of Corruption (3+ clustered)
//  15. Single-target shard spend — Malefic Rapture (3+ shards)
//  16. Execute filler — Drain Soul
//  17. Filler — Shadow Bolt
ApRule const kRules[] = {
    { ShouldUnendingResolve,    DoUnendingResolve,    "Unending Resolve (<=30%)"      },
    { ShouldDrainLifeEmergency, DoDrainLifeEmergency, "Drain Life (emergency)"        },
    { ShouldMortalCoil,         DoMortalCoil,         "Mortal Coil (heal+horror)"     },
    { ShouldHowlOfTerror,       DoHowlOfTerror,       "Howl of Terror (AoE fear)"     },
    { ShouldPetDevourMagic,     DoPetDevourMagic,     "Felhunter Devour Magic"        },
    { ShouldPetSpellLock,       DoPetSpellLock,       "Felhunter Spell Lock"          },
    { ShouldDarkPact,           DoDarkPact,           "Dark Pact (absorb 30-75%)"     },
    { ShouldSoulstone,          DoSoulstone,          "Soulstone (battle rez)"        },
    { ShouldDarkSoulMisery,     DoDarkSoulMisery,     "Dark Soul: Misery (burst CD)"  },
    { ShouldSummonDarkglare,    DoSummonDarkglare,    "Summon Darkglare (boss+dots)"  },
    { ShouldSoulRot,            DoSoulRot,            "Soul Rot (talent CD)"          },
    { ShouldVileTaint,          DoVileTaint,          "Vile Taint (3+ AoE)"           },
    { ShouldPhantomSingularity, DoPhantomSingularity, "Phantom Singularity"           },
    { ShouldHaunt,              DoHaunt,              "Haunt (cooldown burst)"        },
    { ShouldUnstableAffliction, DoUnstableAffliction, "UA primary (refresh)"          },
    { ShouldAgonyPrimary,       DoAgonyPrimary,       "Agony primary (refresh)"       },
    { ShouldCorruptionPrimary,  DoCorruptionPrimary,  "Corruption primary (refresh)"  },
    { ShouldAgonyExpand,        DoAgonyExpand,        "Agony off-target spread"       },
    { ShouldCorruptionExpand,   DoCorruptionExpand,   "Corruption off-target spread"  },
    { ShouldSeedOfCorruption,   DoSeedOfCorruption,   "Seed of Corruption (3+ AoE)"   },
    { ShouldMaleficRapture,     DoMaleficRapture,     "Malefic Rapture (3+ shards)"   },
    { ShouldDrainSoul,          DoDrainSoul,          "Drain Soul (execute)"          },
    { ShouldShadowBolt,         DoShadowBolt,         "Shadow Bolt (filler)"          },
    { AlwaysAlive,              DoNothing,            "Idle"                          },
};

} // anonymous

void RegisterApl_Warlock_Affliction()
{
    constexpr uint32 SPEC_WARLOCK_AFFLICTION = 265;
    RegisterRotation(CLASS_WARLOCK, SPEC_WARLOCK_AFFLICTION, ApRotation{kRules});
}

} // namespace Playerbot::Combat
