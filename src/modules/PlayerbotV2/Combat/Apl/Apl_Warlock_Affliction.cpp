// Affliction Warlock - WoW 12.1.0.69587 rotation. Caster DPS, mana +
// soul shards. Multi-DoT spread + Unstable Affliction shard-spend cycle.
//
// Multi-DoT model: the bot maintains Agony / Corruption (Wither when the
// Hellcaller passive is known) on the primary target, then expands both to
// every visible add via the snapshot's `enemy_without_my_aura` helper.
// Unstable Affliction is the 12.1 shard SPENDER (1 shard, 8s, stacking
// overlaps) and stays on the primary target. The expansion rules sit
// between the primary refresh rules and the spend/filler rules so a fresh
// add is dotted before any Soul Shard is spent.
//
// Talent overrides (SpellEffect aura 332, OVERRIDE_ACTIONBAR_SPELLS): TC
// redirects a cast of the BASE id to the override while the passive is
// known, so the rules always emit the base id and gate/label on the talent:
//   388667 Drain Soul (passive [R][M])   686 Shadow Bolt -> 198590 Drain Soul
//   445465 Wither (Hellcaller passive)   172 Corruption  -> 445468 Wither
//
// ---- Validated IDs (SpellName.csv, WoW 12.1.0.69587) --------------------
//   686     Shadow Bolt                 L1 filler (redirected to Drain Soul)
//   172     Corruption                  L2 baseline DoT (cast id)
//   146739  Corruption                  periodic aura applied by 172
//   445465  Wither                      Hellcaller passive (gate only)
//   445474  Wither                      periodic aura applied by 445468
//   980     Agony                       spec talent [R][M], shard generator
//   1259790 Unstable Affliction         spec talent [R][M], 1-shard spender
//   48181   Haunt                       spec talent [R][M], 15s CD debuff
//   1257052 Dark Harvest                spec talent [R][M], 60s CD burst+heal
//   27243   Seed of Corruption          spec talent [R][M], AoE shard spend
//   205180  Summon Darkglare            spec talent [R][M], 2min CD
//   388667  Drain Soul                  spec passive [R][M] (gate only)
//   234153  Drain Life                  L9 emergency self-heal channel
//   6789    Mortal Coil                 class talent [R][M], horror + heal
//   5484    Howl of Terror              class talent, AoE fear, 40s CD
//   108416  Dark Pact                   class talent [R][M], absorb shield
//   231811  Soulstone                   modern combat-rez (preferred)
//   20707   Soulstone                   L14 baseline fallback id
//   104773  Unending Resolve            L4 baseline, 40% DR, 3min CD
//
// ---- Pet abilities (cast through pet_cast) ------------------------------
//   19647   Spell Lock                  Felhunter interrupt/silence
//   89766   Axe Toss                    Felguard stun (not Aff default but
//                                        supported when player swapped pet)
//   17012   Devour Magic                Felhunter dispel (cleanses bot's
//                                        harmful Magic auras)
//
// ---- Skipped spells (and why) -------------------------------------------
//   - Malefic Rapture (324536), Phantom Singularity (205179), Vile Taint
//     (278350), Soul Rot (236235), Dark Soul: Misery (113860): removed from
//     the Affliction 12.1 tree (ids still exist but are not learnable).
//   - Drain Soul 198590 / Wither 445468: never emitted directly - TC
//     redirects the base cast (see override note above).
//   - Demonic Circle (268358 -> 48018/48020), Demonic Gateway (111771),
//     Soulburn (385899), Burning Rush (111400): positioning / utility the
//     fixed-tick rotation cannot use sensibly.
//   - Banish (710), Fear (5782), Curse of Tongues (1714) / Exhaustion
//     (334275), Blight of Tongues (1271802): situational CC / debuffs.
//   - Grimoire of Sacrifice (108503): not in the curated builds; pet kept.
//   - Summon Imp / Felhunter (688 / 691): pet maintenance lives in
//     State_Idle (ooc pet-summon rule), not in the combat APL.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated against SpellName.csv) ----
constexpr uint32 SHADOW_BOLT          = 686;        // base filler; 388667 redirects to Drain Soul
constexpr uint32 CORRUPTION           = 172;        // base cast; 445465 redirects to Wither
constexpr uint32 CORRUPTION_DOT       = 146739;     // periodic aura applied by 172
constexpr uint32 WITHER_TALENT        = 445465;     // Hellcaller passive (gate only)
constexpr uint32 WITHER_DOT           = 445474;     // periodic aura applied by 445468
constexpr uint32 AGONY                = 980;
constexpr uint32 UNSTABLE_AFFLICTION  = 1259790;    // 1-shard spender, stacking
constexpr uint32 HAUNT                = 48181;
constexpr uint32 DARK_HARVEST         = 1257052;    // 60s CD burst + self-heal
constexpr uint32 DRAIN_SOUL_TALENT    = 388667;     // passive: Shadow Bolt cast becomes Drain Soul
constexpr uint32 SEED_OF_CORRUPTION   = 27243;
constexpr uint32 SUMMON_DARKGLARE     = 205180;
constexpr uint32 DRAIN_LIFE           = 234153;
constexpr uint32 MORTAL_COIL          = 6789;
constexpr uint32 HOWL_OF_TERROR       = 5484;
constexpr uint32 DARK_PACT            = 108416;
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

// Refresh threshold for DoTs: 30% of base duration is the standard pandemic
// window in modern WoW. Using 4s here covers the maintained Aff DoTs (Agony
// 18s, Corruption / Wither ~14s) without burning mana on early refreshes.
bool MissingDot(ApPredicateContext const& ctx, uint32 dot)
{
    AuraEntry const* a = ctx.bot.find_aura(dot, ctx.bot.victim());
    return !a || a->remaining.count() <= 4000;
}

// The Corruption cast (172) is redirected to Wither (445468) by TC while the
// Hellcaller passive is known, and the periodic aura on the target differs
// (146739 vs 445474). Resolve the aura id the bot's build actually applies.
uint32 CorruptionDotId(ApPredicateContext const& ctx)
{
    return ctx.bot.knows_spell(WITHER_TALENT) ? WITHER_DOT : CORRUPTION_DOT;
}

bool MissingCorruption(ApPredicateContext const& ctx)
{
    return MissingDot(ctx, CorruptionDotId(ctx));
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

// Pet interrupt - Felhunter's Spell Lock (24s CD, no school lockout in 12.0).
// We only fire when the bot has a pet summoned and the caster is targeting
// our victim or a nearby threat. Pet abilities cast via PetCastSpellIntent,
// NOT bot-side cast(); the API resolves the pet and issues the cast through
// pet->CastSpell. Emitting both Spell Lock + Axe Toss means whichever pet is
// out lands the matching one - the executor returns Locked on the mismatch.
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

// Felhunter Devour Magic - pet dispel. The bot's pet eats one Magic aura
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
    // Emergency self-heal channel - only when we're critically low AND no
    // instant defensive is up. The 5s channel pins the bot in place so we
    // gate harder than the baseline rule (<=50%).
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DRAIN_LIFE)) return false;
    // is_ready folds in cast-in-progress: re-emitting mid-channel would
    // cancel and restart the 5s drain every tick.
    if (!ctx.bot.is_ready(DRAIN_LIFE)) return false;
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

// Howl of Terror - 10y AoE fear, 40s CD. Multi-target panic CC: when we're
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

bool ShouldSummonDarkglare(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SUMMON_DARKGLARE)) return false;
    if (!ctx.bot.is_ready(SUMMON_DARKGLARE)) return false;
    // 2min CD - only blow on a boss with all DoTs ramped (Darkglare buffs
    // Agony / Corruption / UA damage while active; firing without DoTs up
    // wastes the cooldown).
    if (!BossLikeTargetEngaged(ctx)) return false;
    if (MissingDot(ctx, AGONY) || MissingCorruption(ctx)) return false;
    return ctx.bot.has_aura(UNSTABLE_AFFLICTION, ctx.bot.victim());
}
void DoSummonDarkglare(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SUMMON_DARKGLARE, ctx.bot.victim());
}

// Dark Harvest - 60s CD, self-centred (range 0): consumes the life force of
// every target carrying our periodic effects for 3s of Shadowflame damage
// and heals for a share of it. Fire once Agony + Corruption are ramped on
// the victim so the burst has DoTs to feed on; spreads reward more targets
// but a single ramped target is already worth the 60s.
bool ShouldDarkHarvest(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DARK_HARVEST)) return false;
    if (!ctx.bot.is_ready(DARK_HARVEST)) return false;
    return !MissingDot(ctx, AGONY) && !MissingCorruption(ctx);
}
void DoDarkHarvest(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DARK_HARVEST); }

// Haunt - 15s CD, 1.5s cast: +damage-taken debuff on the target and, with
// the curated Shadow of Nathreza / Improved Haunt passives, periodic +
// splash damage of its own. Cheap enough to run on cooldown; only wait for
// Agony + Corruption so the amplified window lands on ticking DoTs.
bool ShouldHaunt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HAUNT)) return false;
    if (!ctx.bot.is_ready(HAUNT)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(HAUNT)) return false;
    return !MissingDot(ctx, AGONY) && !MissingCorruption(ctx);
}
void DoHaunt(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(HAUNT, ctx.bot.victim()); }

// Unstable Affliction - the 12.1 shard SPENDER: 1 shard, 1.5s cast, 8s DoT;
// multiple casts overlap and every stack raises its damage. Dump shards on
// the primary target once Agony + Corruption are ramped (Seed of Corruption
// sits above this rule and claims the shard first when 3+ are clustered).
bool ShouldUnstableAffliction(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(UNSTABLE_AFFLICTION)) return false;
    if (!ctx.bot.is_ready(UNSTABLE_AFFLICTION)) return false;
    if (ctx.bot.power(POWER_SOUL_SHARDS) < 1) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(UNSTABLE_AFFLICTION)) return false;
    return !MissingDot(ctx, AGONY) && !MissingCorruption(ctx);
}
void DoUnstableAffliction(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(UNSTABLE_AFFLICTION, ctx.bot.victim()); }

bool ShouldAgonyPrimary(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(AGONY)) return false;
    return MissingDot(ctx, AGONY);
}
void DoAgonyPrimary(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(AGONY, ctx.bot.victim()); }

// Corruption / Wither primary: emit the base Corruption id - TC redirects it
// to Wither while the Hellcaller passive is known - and refresh off the
// aura the build actually applies.
bool ShouldCorruptionPrimary(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CORRUPTION)) return false;
    return MissingCorruption(ctx);
}
void DoCorruptionPrimary(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CORRUPTION, ctx.bot.victim());
}

// Multi-dot expansion: spread Agony + Corruption to off-target adds. Skips
// the primary victim (handled by the primary refresh rules above) and any
// add already carrying our own DoT. Returns nullptr when nothing to do -
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
    if (!ctx.bot.knows_spell(CORRUPTION)) return false;
    return OffDotExpansionTarget(ctx, CorruptionDotId(ctx)) != nullptr;
}
void DoCorruptionExpand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = OffDotExpansionTarget(ctx, CorruptionDotId(ctx)))
        e.cast(CORRUPTION, t->guid);
}

// Filler. With the Drain Soul passive (388667) known, TC redirects the
// Shadow Bolt emit to Drain Soul (198590, a channel) - same emit, separate
// rule so /whyidle labels it truthfully. Both are hard-casts: hold while
// moving, and is_ready keeps a running channel from being restarted.
bool ShouldDrainSoul(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DRAIN_SOUL_TALENT)) return false;
    if (!ctx.bot.knows_spell(SHADOW_BOLT)) return false;
    if (!ctx.bot.is_ready(SHADOW_BOLT)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(SHADOW_BOLT)) return false;
    return true;
}
void DoDrainSoul(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SHADOW_BOLT, ctx.bot.victim()); }

bool ShouldShadowBolt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(DRAIN_SOUL_TALENT)) return false;    // Drain Soul rule owns the filler slot
    if (!ctx.bot.knows_spell(SHADOW_BOLT)) return false;
    if (!ctx.bot.is_ready(SHADOW_BOLT)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(SHADOW_BOLT)) return false;
    return true;
}
void DoShadowBolt(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SHADOW_BOLT, ctx.bot.victim()); }

bool ShouldSeedOfCorruption(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SEED_OF_CORRUPTION)) return false;
    if (!ctx.bot.is_ready(SEED_OF_CORRUPTION)) return false;
    if (ctx.bot.power(POWER_SOUL_SHARDS) < 1) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(SEED_OF_CORRUPTION)) return false;
    // Seed costs a shard (2s cast) and explodes into Corruption / Wither on
    // everything nearby; worth it only when 3+ enemies cluster.
    return ctx.aoe_preference || ctx.bot.enemies_within(20.0f) >= 3;
}
void DoSeedOfCorruption(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SEED_OF_CORRUPTION, ctx.bot.victim()); }

bool AlwaysAlive(ApPredicateContext const& ctx) { return ctx.bot.is_alive(); }
void DoNothing(ApPredicateContext const&, BotIntentEmitter&) {}

// ---- Rule table ----
// Order (per task spec):
//   1. Unending Resolve   - panic <=30%
//   2. Drain Life         - emergency self-heal <=50% when nothing else up
//   3. Mortal Coil        - heal + horror CC
//   4. Howl of Terror     - multi-target panic fear
//   5. Felhunter Devour Magic - pet dispel on self (Magic auras)
//   6. Felhunter Spell Lock   - pet interrupt
//   7. Dark Pact          - absorb shield (30-75% HP)
//   8. Group utility      - Soulstone rez
//   9. Major offensive CDs - Summon Darkglare (boss, all DoTs), Dark
//      Harvest (DoTs ramped)
//  10. Single-target debuff CD - Haunt (DoTs ramped)
//  11. Primary DoT maintenance - Agony, Corruption / Wither
//  12. Off-target DoT expansion - Agony + Corruption / Wither to adds
//  13. AoE shard spend   - Seed of Corruption (3+ clustered)
//  14. Single-target shard spend - Unstable Affliction (1+ shard)
//  15. Filler            - Drain Soul (talent redirect) else Shadow Bolt
ApRule const kRules[] = {
    { ShouldUnendingResolve,    DoUnendingResolve,    "Unending Resolve (<=30%)"      },
    { ShouldDrainLifeEmergency, DoDrainLifeEmergency, "Drain Life (emergency)"        },
    { ShouldMortalCoil,         DoMortalCoil,         "Mortal Coil (heal+horror)"     },
    { ShouldHowlOfTerror,       DoHowlOfTerror,       "Howl of Terror (AoE fear)"     },
    { ShouldPetDevourMagic,     DoPetDevourMagic,     "Felhunter Devour Magic"        },
    { ShouldPetSpellLock,       DoPetSpellLock,       "Felhunter Spell Lock"          },
    { ShouldDarkPact,           DoDarkPact,           "Dark Pact (absorb 30-75%)"     },
    { ShouldSoulstone,          DoSoulstone,          "Soulstone (battle rez)"        },
    { ShouldSummonDarkglare,    DoSummonDarkglare,    "Summon Darkglare (boss+dots)"  },
    { ShouldDarkHarvest,        DoDarkHarvest,        "Dark Harvest (dots ramped)"    },
    { ShouldHaunt,              DoHaunt,              "Haunt (dots ramped)"           },
    { ShouldAgonyPrimary,       DoAgonyPrimary,       "Agony primary (refresh)"       },
    { ShouldCorruptionPrimary,  DoCorruptionPrimary,  "Corruption/Wither primary"     },
    { ShouldAgonyExpand,        DoAgonyExpand,        "Agony off-target spread"       },
    { ShouldCorruptionExpand,   DoCorruptionExpand,   "Corruption/Wither spread"      },
    { ShouldSeedOfCorruption,   DoSeedOfCorruption,   "Seed of Corruption (3+ AoE)"   },
    { ShouldUnstableAffliction, DoUnstableAffliction, "Unstable Affliction (spend)"   },
    { ShouldDrainSoul,          DoDrainSoul,          "Drain Soul (filler)"           },
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
