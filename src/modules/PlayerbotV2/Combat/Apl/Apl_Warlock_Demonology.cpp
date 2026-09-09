// Demonology Warlock - WoW 12.1.0.69587 rotation. Pet-driven caster: Hand
// of Gul'dan summons Wild Imps, Demonbolt spends Demonic Core procs, Call
// Dreadstalkers (which in 12.1 also brings the Vilefiend) runs on cooldown.
// Major cooldowns: Summon Demonic Tyrant (after the stalkers are out),
// Grimoire: Imp Lord / Fel Ravager, Summon Doomguard, Power Siphon.
//
// ---- Validated IDs (SpellName.csv, WoW 12.1.0.69587) --------------------
//   686     Shadow Bolt                 L1 filler shard generator
//   264178  Demonbolt                   granted by Demoniac 426115 [R][M]
//   105174  Hand of Gul'dan             granted by talent 1250273 [R][M]
//   104316  Call Dreadstalkers          spec talent [R][M], 2 shards, 20s CD
//   265187  Summon Demonic Tyrant       spec talent [R][M], 60s CD
//   1276452 Grimoire: Imp Lord          spec talent [R][M], 1 shard, 2min CD
//   1276467 Grimoire: Fel Ravager       choice-node alternative (same node)
//   1276672 Summon Doomguard            spec talent (not curated), 2min CD
//   196277  Implosion                   spec talent [M], detonate Wild Imps
//   264130  Power Siphon                spec talent [R], imps -> Demonic Core
//   264173  Demonic Core                proc aura (read-only)
//   30283   Shadowfury                  class talent [M], AoE 3s stun
//   234153  Drain Life                  L9 emergency self-heal channel
//   6789    Mortal Coil                 class talent [R][M], horror + heal
//   5484    Howl of Terror              class talent, AoE fear, 40s CD
//   108416  Dark Pact                   class talent [R][M], absorb shield
//   231811  Soulstone                   modern combat-rez (preferred)
//   20707   Soulstone                   L14 baseline fallback id
//   104773  Unending Resolve            L4 baseline, 40% DR, 3min CD
//
// ---- Pet abilities (cast through pet_cast) ------------------------------
//   89766   Axe Toss                    Felguard stun / interrupt
//   19647   Spell Lock                  Felhunter interrupt/silence
//   17012   Devour Magic                Felhunter dispel
//
// ---- Skipped spells (and why) -------------------------------------------
//   - Grimoire: Felguard (111898), Nether Portal (267217), Bilescourge
//     Bombers (267211), Demonic Strength (267171), Soul Strike (264057):
//     removed from the Demonology 12.1 tree (ids exist, not learnable).
//   - Summon Vilefiend (264119): the 12.1 talent 1251778 is a passive that
//     makes Call Dreadstalkers summon the Vilefiend - nothing to cast.
//   - Doom (603): 12.1 Doom 460551 is a passive applied by Demonbolt.
//   - Fel Firebolt / Felstorm (89751): Wild Imp / Felguard PET abilities,
//     handled by the pet AI server-side, never cast by the warlock.
//   - Summon Felguard (30146) / Imp (688): pet maintenance lives in
//     State_Idle (ooc pet-summon rule), not in the combat APL.
//   - Banish (710), Fear (5782), Curse of Tongues (1714 [R]) / Exhaustion
//     (334275): situational CC / debuffs.
//   - Demonic Circle (268358), Demonic Gateway (111771), Soulburn (385899),
//     Burning Rush (111400): positioning / utility.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated against SpellName.csv) ----
constexpr uint32 SHADOW_BOLT          = 686;
constexpr uint32 DEMONBOLT            = 264178;     // granted by Demoniac 426115
constexpr uint32 HAND_OF_GULDAN       = 105174;     // granted by talent 1250273
constexpr uint32 CALL_DREADSTALKERS   = 104316;
constexpr uint32 SUMMON_DEMONIC_TYRANT= 265187;
constexpr uint32 GRIMOIRE_IMP_LORD    = 1276452;    // [R][M] choice node
constexpr uint32 GRIMOIRE_FEL_RAVAGER = 1276467;    // alternative on the same node
constexpr uint32 SUMMON_DOOMGUARD     = 1276672;    // spec talent, 1 shard, 2min CD
constexpr uint32 IMPLOSION            = 196277;
constexpr uint32 POWER_SIPHON         = 264130;
constexpr uint32 SHADOWFURY           = 30283;
constexpr uint32 DEMONIC_CORE         = 264173;     // proc aura (read-only)
constexpr uint32 DRAIN_LIFE           = 234153;
constexpr uint32 MORTAL_COIL          = 6789;
constexpr uint32 HOWL_OF_TERROR       = 5484;
constexpr uint32 DARK_PACT            = 108416;
constexpr uint32 SOULSTONE_MODERN     = 231811;
constexpr uint32 SOULSTONE_LEGACY     = 20707;
constexpr uint32 UNENDING_RESOLVE     = 104773;

// Pet abilities (cast through PetCastSpellIntent).
constexpr uint32 PET_SPELL_LOCK       = 19647;      // Felhunter
constexpr uint32 PET_AXE_TOSS         = 89766;      // Felguard (Demo default)
constexpr uint32 PET_DEVOUR_MAGIC     = 17012;      // Felhunter dispel

// ---- Helpers ----
bool HasLiveTarget(ApPredicateContext const& ctx)
{
    return !ctx.bot.victim().IsEmpty();
}

// Modern Soulstone preferred over legacy id.
uint32 KnownSoulstone(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(SOULSTONE_MODERN)) return SOULSTONE_MODERN;
    if (ctx.bot.knows_spell(SOULSTONE_LEGACY)) return SOULSTONE_LEGACY;
    return 0;
}

// Demo defaults to Felguard, so Axe Toss (4s stun) is the primary "interrupt"
// - no silence, but the stun stops a cast cold. Spell Lock falls back when
// the player swapped to Felhunter. Emit both; only the matching pet's
// ability succeeds, the other returns Locked harmlessly.
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
    auto const* c = ctx.bot.kick_target(pvp, 40.0f);
    if (!c) return;
    e.pet_cast(PET_AXE_TOSS, c->guid);
    e.pet_cast(PET_SPELL_LOCK, c->guid);
}

// Felhunter Devour Magic - fires only if the pet is a Felhunter and the bot
// is carrying a harmful Magic aura. Mismatched-pet calls no-op server-side.
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
    if (!ctx.bot.is_ready(DRAIN_LIFE)) return false;    // don't restart a running channel
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
    return ctx.group.dead_member_priority(ctx.bot.map_id()) != nullptr;
}
void DoSoulstone(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    uint32 sid = KnownSoulstone(ctx);
    if (sid == 0) return;
    if (auto const* m = ctx.group.dead_member_priority(ctx.bot.map_id()))
        e.cast(sid, m->guid);
}

// ---- CC ----
bool ShouldShadowfury(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOWFURY)) return false;
    if (!ctx.bot.is_ready(SHADOWFURY)) return false;
    // 3+ adds clustered around us - emergency stun.
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
// Call Dreadstalkers - 2 shards, 20s CD; in 12.1 also summons the Vilefiend
// (passive 1251778 [R][M]). Runs on cooldown ahead of the Tyrant so the
// stalkers are out for the empowerment window.
bool ShouldCallDreadstalkers(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CALL_DREADSTALKERS)) return false;
    if (!ctx.bot.is_ready(CALL_DREADSTALKERS)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(CALL_DREADSTALKERS)) return false;
    return ctx.bot.power(POWER_SOUL_SHARDS) >= 2;
}
void DoCallDreadstalkers(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(CALL_DREADSTALKERS, ctx.bot.victim()); }

// Summon Demonic Tyrant - 60s CD; damage scales with every Wild Imp and
// Dreadstalker active. Hold it while Call Dreadstalkers is still off
// cooldown (nothing to empower yet); the Dreadstalkers rule above fires
// first, then the Tyrant follows on the next tick.
bool ShouldDemonicTyrant(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SUMMON_DEMONIC_TYRANT)) return false;
    if (!ctx.bot.is_ready(SUMMON_DEMONIC_TYRANT)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(SUMMON_DEMONIC_TYRANT)) return false;
    if (ctx.bot.knows_spell(CALL_DREADSTALKERS) && ctx.bot.cd_remaining(CALL_DREADSTALKERS).count() <= 0)
        return false;
    return true;
}
void DoDemonicTyrant(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SUMMON_DEMONIC_TYRANT, ctx.bot.victim()); }

// Grimoire: Imp Lord [R][M] / Grimoire: Fel Ravager - choice node, 20s
// guardian, 1 shard, 2min CD. Both are pure damage (Imp Lord also strips
// one harmful effect from the bot on summon), so fire whichever the build
// owns off cooldown with a shard in hand.
uint32 ReadyGrimoire(ApPredicateContext const& ctx)
{
    for (uint32 sid : { GRIMOIRE_IMP_LORD, GRIMOIRE_FEL_RAVAGER })
        if (ctx.bot.knows_spell(sid) && ctx.bot.is_ready(sid))
            return sid;
    return 0;
}
bool ShouldGrimoire(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.power(POWER_SOUL_SHARDS) < 1) return false;
    return ReadyGrimoire(ctx) != 0;
}
void DoGrimoire(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 sid = ReadyGrimoire(ctx))
        e.cast(sid, ctx.bot.victim());
}

// Summon Doomguard - spec talent (not in the curated builds, knows_spell
// gated): 12s guardian, 1 shard, 2min CD shortened by Demonic Core use.
bool ShouldSummonDoomguard(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SUMMON_DOOMGUARD)) return false;
    if (!ctx.bot.is_ready(SUMMON_DOOMGUARD)) return false;
    return ctx.bot.power(POWER_SOUL_SHARDS) >= 1;
}
void DoSummonDoomguard(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SUMMON_DOOMGUARD, ctx.bot.victim()); }

// ---- AoE Implosion ----
// When 3+ Wild Imps are out and 3+ enemies clustered, detonate them.
// Tracked indirectly: imps sit out for ~12s after Hand of Gul'dan. We can't
// count imp-pets in the snapshot, so we use a proxy - fire when in AoE
// situation (3+ enemies in 8yd).
bool ShouldImplosion(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(IMPLOSION)) return false;
    if (!ctx.bot.is_ready(IMPLOSION)) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(8.0f) >= 3;
}
void DoImplosion(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(IMPLOSION, ctx.bot.victim()); }

// ---- Shard generators / spenders ----
bool ShouldHandOfGuldan(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HAND_OF_GULDAN)) return false;
    if (!ctx.bot.is_ready(HAND_OF_GULDAN)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(HAND_OF_GULDAN)) return false;
    // Spend at 3+ shards for max imp count (3 imps per cast at 3 shards).
    return ctx.bot.power(POWER_SOUL_SHARDS) >= 3;
}
void DoHandOfGuldan(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(HAND_OF_GULDAN, ctx.bot.victim()); }

bool ShouldPowerSiphon(ApPredicateContext const& ctx)
{
    // Sacrifices up to 2 imps to gain 2 stacks of Demonic Core. Ideally
    // fired before a damage burst window; a 2nd-tier readiness check -
    // skip when we already have Demonic Core stacks ready.
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(POWER_SIPHON)) return false;
    if (!ctx.bot.is_ready(POWER_SIPHON)) return false;
    if (ctx.bot.aura_stacks(DEMONIC_CORE) >= 2) return false;
    return ctx.bot.in_combat();
}
void DoPowerSiphon(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(POWER_SIPHON); }

bool ShouldDemonbolt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEMONBOLT)) return false;
    if (!ctx.bot.is_ready(DEMONBOLT)) return false;
    // Spend Demonic Core procs - instant cast, 2-shard generator on Demo
    // (and with the Doom passive 460551 it also plants Doom on the target).
    return ctx.bot.has_aura(DEMONIC_CORE);
}
void DoDemonbolt(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(DEMONBOLT, ctx.bot.victim()); }

bool ShouldShadowBolt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOW_BOLT)) return false;
    if (!ctx.bot.is_ready(SHADOW_BOLT)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(SHADOW_BOLT)) return false;
    return true;
}
void DoShadowBolt(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SHADOW_BOLT, ctx.bot.victim()); }

bool AlwaysAlive(ApPredicateContext const& ctx) { return ctx.bot.is_alive(); }
void DoNothing(ApPredicateContext const&, BotIntentEmitter&) {}

// ---- Rule table ----
// Order (per task spec):
//   1. Unending Resolve   - panic <=30%
//   2. Drain Life         - emergency self-heal <=50% when nothing else up
//   3. Howl of Terror     - multi-target panic fear
//   4. Felhunter Devour Magic - pet dispel
//   5. Pet interrupt      - Axe Toss / Spell Lock
//   6. Mortal Coil        - heal + horror
//   7. Dark Pact          - absorb shield (30-75%)
//   8. Group utility      - Soulstone rez
//   9. Emergency CC       - Shadowfury (3+ surround)
//  10. Major offensive CDs - Call Dreadstalkers -> Demonic Tyrant (stalkers
//      out) -> Grimoire: Imp Lord / Fel Ravager -> Summon Doomguard
//  11. AoE               - Implosion (3+ clustered)
//  12. Demonbolt         - Demonic Core spending
//  13. Power Siphon      - generate cores when none
//  14. Hand of Gul'dan   - 3+ shards
//  15. Filler            - Shadow Bolt
ApRule const kRules[] = {
    { ShouldUnendingResolve,    DoUnendingResolve,    "Unending Resolve (<=30%)"      },
    { ShouldDrainLifeEmergency, DoDrainLifeEmergency, "Drain Life (emergency)"        },
    { ShouldHowlOfTerror,       DoHowlOfTerror,       "Howl of Terror (AoE fear)"     },
    { ShouldPetDevourMagic,     DoPetDevourMagic,     "Felhunter Devour Magic"        },
    { ShouldPetInterrupt,       DoPetInterrupt,       "Pet interrupt (Axe Toss/Lock)" },
    { ShouldMortalCoil,         DoMortalCoil,         "Mortal Coil (heal+horror)"     },
    { ShouldDarkPact,           DoDarkPact,           "Dark Pact (absorb 30-75%)"     },
    { ShouldSoulstone,          DoSoulstone,          "Soulstone (battle rez)"        },
    { ShouldShadowfury,         DoShadowfury,         "Shadowfury (3+ surround stun)" },
    { ShouldCallDreadstalkers,  DoCallDreadstalkers,  "Call Dreadstalkers"            },
    { ShouldDemonicTyrant,      DoDemonicTyrant,      "Summon Demonic Tyrant"         },
    { ShouldGrimoire,           DoGrimoire,           "Grimoire: Imp Lord/Ravager"    },
    { ShouldSummonDoomguard,    DoSummonDoomguard,    "Summon Doomguard"              },
    { ShouldImplosion,          DoImplosion,          "Implosion (3+ AoE)"            },
    { ShouldDemonbolt,          DoDemonbolt,          "Demonbolt (Demonic Core proc)" },
    { ShouldPowerSiphon,        DoPowerSiphon,        "Power Siphon (build cores)"    },
    { ShouldHandOfGuldan,       DoHandOfGuldan,       "Hand of Gul'dan (imps)"        },
    { ShouldShadowBolt,         DoShadowBolt,         "Shadow Bolt (filler)"          },
    { AlwaysAlive,              DoNothing,            "Idle"                          },
};

} // anonymous

void RegisterApl_Warlock_Demonology()
{
    constexpr uint32 SPEC_WARLOCK_DEMONOLOGY = 266;
    RegisterRotation(CLASS_WARLOCK, SPEC_WARLOCK_DEMONOLOGY, ApRotation{kRules});
}

} // namespace Playerbot::Combat
