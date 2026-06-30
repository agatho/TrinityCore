// Demonology Warlock - WoW 12.0 baseline rotation. Pet-driven caster: Hand
// of Gul'dan summons imps, Demonbolt spends shards via Demonic Core procs.
// Major cooldowns are Demonic Tyrant (extends imps), Grimoire: Felguard,
// Summon Vilefiend.
//
// ---- Validated IDs (SpellName.csv, WoW 12.0) ----------------------------
//   686    Shadow Bolt                 filler shard generator
//   264178 Demonbolt                   proc spender (Demonic Core)
//   105174 Hand of Gul'dan             imp summon, 1-3 shards
//   104316 Call Dreadstalkers          2 shards / 2 stalkers
//   265187 Summon Demonic Tyrant       major CD (extend imps)
//   264119 Summon Vilefiend            talent / 1min CD
//   111898 Grimoire: Felguard          talent / 2min CD
//   196277 Implosion                   detonates Wild Imps (AoE)
//   264130 Power Siphon                sacrifice imps → Demonic Core
//   603    Doom                        30s instant DoT (talent)
//   267217 Nether Portal               3min CD demon spawn
//   267211 Bilescourge Bombers         talent ground-AoE
//   267171 Demonic Strength            talent / empower Felguard
//   264057 Soul Strike                 Felguard nuke (talent)
//   264173 Demonic Core                proc id (read-only)
//   30283  Shadowfury                  AoE 3s stun, 1min CD
//   89766  Axe Toss                    Felguard pet stun (interrupt)
//   19647  Spell Lock                  Felhunter pet interrupt
//   17012  Devour Magic                Felhunter pet dispel
//   234153 Drain Life                  emergency self-heal channel
//   6789   Mortal Coil                 8s fear + 20% heal
//   5484   Howl of Terror              8s AoE fear (10y), 40s CD
//   108416 Dark Pact                   absorb shield
//   48018  Demonic Circle              utility teleport
//   231811 Soulstone (modern)          combat-rez
//   20707  Soulstone (legacy)          legacy fallback
//   104773 Unending Resolve            40% DR, 3min CD
//
// ---- Skipped spells (and why) -------------------------------------------
//   - Fel Firebolt (334591): Wild Imp PET auto-attack — never directly
//     cast by the warlock. The imp's AI handles it server-side.
//   - Track Pets (1245325): hunter/utility OOC tracking buff, not a
//     combat ability.
//   - Banish (710), Fear (5782), Curse of Tongues (1714):
//     situational utility / handled by baseline.
//   - Felstorm (89751): the Felguard PET's own AoE; auto-used by the pet's
//     class AI when commanded to attack, not by the warlock APL.
//   - Summon Felguard / Imp (688/etc): baseline pet maintenance.

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
constexpr uint32 DEMONBOLT            = 264178;
constexpr uint32 HAND_OF_GULDAN       = 105174;
constexpr uint32 CALL_DREADSTALKERS   = 104316;
constexpr uint32 SUMMON_DEMONIC_TYRANT= 265187;
constexpr uint32 SUMMON_VILEFIEND     = 264119;
constexpr uint32 GRIMOIRE_FELGUARD    = 111898;
constexpr uint32 IMPLOSION            = 196277;
constexpr uint32 POWER_SIPHON         = 264130;
constexpr uint32 DOOM                 = 603;
constexpr uint32 NETHER_PORTAL        = 267217;
constexpr uint32 BILESCOURGE_BOMBERS  = 267211;     // talent ground-AoE
constexpr uint32 DEMONIC_STRENGTH     = 267171;     // talent Felguard nuke
constexpr uint32 SOUL_STRIKE          = 264057;
constexpr uint32 SHADOWFURY           = 30283;
constexpr uint32 AXE_TOSS             = 89766;
constexpr uint32 SPELL_LOCK           = 19647;
constexpr uint32 DEMONIC_CORE         = 264173;
constexpr uint32 DRAIN_LIFE           = 234153;
constexpr uint32 MORTAL_COIL          = 6789;
constexpr uint32 HOWL_OF_TERROR       = 5484;
constexpr uint32 DARK_PACT            = 108416;
constexpr uint32 DEMONIC_CIRCLE       = 48018;
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

// Demo defaults to Felguard, so Axe Toss (4s stun) is the primary "interrupt"
// — no silence, but the stun stops a cast cold. Spell Lock falls back when
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

// Felhunter Devour Magic — fires only if the pet is a Felhunter and the bot
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
    // 3+ adds clustered around us — emergency stun.
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
bool ShouldNetherPortal(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(NETHER_PORTAL)) return false;
    if (!ctx.bot.is_ready(NETHER_PORTAL)) return false;
    if (ctx.bot.power(POWER_SOUL_SHARDS) < 1) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoNetherPortal(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(NETHER_PORTAL, v->x, v->y, v->z);
    else
        e.cast(NETHER_PORTAL);
}

bool ShouldDemonicTyrant(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SUMMON_DEMONIC_TYRANT)) return false;
    return ctx.bot.is_ready(SUMMON_DEMONIC_TYRANT);
}
void DoDemonicTyrant(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SUMMON_DEMONIC_TYRANT, ctx.bot.victim()); }

bool ShouldGrimoireFelguard(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(GRIMOIRE_FELGUARD)) return false;
    return ctx.bot.is_ready(GRIMOIRE_FELGUARD);
}
void DoGrimoireFelguard(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(GRIMOIRE_FELGUARD, ctx.bot.victim()); }

bool ShouldVilefiend(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SUMMON_VILEFIEND)) return false;
    return ctx.bot.is_ready(SUMMON_VILEFIEND);
}
void DoVilefiend(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SUMMON_VILEFIEND, ctx.bot.victim()); }

bool ShouldCallDreadstalkers(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CALL_DREADSTALKERS)) return false;
    if (!ctx.bot.is_ready(CALL_DREADSTALKERS)) return false;
    return ctx.bot.power(POWER_SOUL_SHARDS) >= 2;
}
void DoCallDreadstalkers(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(CALL_DREADSTALKERS, ctx.bot.victim()); }

// Demonic Strength — talent. Empower next Felguard Felstorm. Fires off-CD
// against any combat target.
bool ShouldDemonicStrength(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEMONIC_STRENGTH)) return false;
    if (!ctx.bot.is_ready(DEMONIC_STRENGTH)) return false;
    return ctx.bot.has_pet();
}
void DoDemonicStrength(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(DEMONIC_STRENGTH, ctx.bot.victim()); }

// Bilescourge Bombers — ground-target AoE bombing. 2+ enemies in 10y or boss.
bool ShouldBilescourgeBombers(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BILESCOURGE_BOMBERS)) return false;
    if (!ctx.bot.is_ready(BILESCOURGE_BOMBERS)) return false;
    if (ctx.bot.power(POWER_SOUL_SHARDS) < 2) return false;
    return ctx.bot.enemies_within(10.0f) >= 2 || BossLikeTargetEngaged(ctx);
}
void DoBilescourgeBombers(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(BILESCOURGE_BOMBERS, v->x, v->y, v->z);
    else
        e.cast(BILESCOURGE_BOMBERS);
}

// ---- DoT maintenance ----
bool ShouldDoom(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DOOM)) return false;
    AuraEntry const* a = ctx.bot.find_aura(DOOM, ctx.bot.victim());
    return !a || a->remaining.count() <= 5000;
}
void DoDoom(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(DOOM, ctx.bot.victim()); }

// ---- AoE Implosion ----
// When 3+ Wild Imps are out and 3+ enemies clustered, detonate them.
// Tracked indirectly: imps sit out for ~12s after Hand of Gul'dan. We can't
// count imp-pets in the snapshot, so we use a proxy — fire when in AoE
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
    // Spend at 3+ shards for max imp count (3 imps per cast at 3 shards).
    return ctx.bot.power(POWER_SOUL_SHARDS) >= 3;
}
void DoHandOfGuldan(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(HAND_OF_GULDAN, ctx.bot.victim()); }

bool ShouldPowerSiphon(ApPredicateContext const& ctx)
{
    // Sacrifices up to 2 imps to gain 2 stacks of Demonic Core. Ideally
    // fired before a damage burst window; a 2nd-tier readiness check —
    // skip when we already have Demonic Core stacks ready.
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(POWER_SIPHON)) return false;
    if (!ctx.bot.is_ready(POWER_SIPHON)) return false;
    if (ctx.bot.aura_stacks(DEMONIC_CORE) >= 2) return false;
    return ctx.bot.in_combat();
}
void DoPowerSiphon(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(POWER_SIPHON); }

bool ShouldSoulStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SOUL_STRIKE)) return false;
    if (!ctx.bot.is_ready(SOUL_STRIKE)) return false;
    // Only when our pet is a Felguard (the one that can use this ability).
    return ctx.bot.has_pet();
}
void DoSoulStrike(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SOUL_STRIKE, ctx.bot.victim()); }

bool ShouldDemonbolt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEMONBOLT)) return false;
    // Spend Demonic Core procs — instant cast, 2-shard generator on Demo.
    return ctx.bot.has_aura(DEMONIC_CORE);
}
void DoDemonbolt(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(DEMONBOLT, ctx.bot.victim()); }

bool ShouldShadowBolt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOW_BOLT)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(SHADOW_BOLT)) return false;
    return true;
}
void DoShadowBolt(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SHADOW_BOLT, ctx.bot.victim()); }

bool AlwaysAlive(ApPredicateContext const& ctx) { return ctx.bot.is_alive(); }
void DoNothing(ApPredicateContext const&, BotIntentEmitter&) {}

// ---- Rule table ----
// Order (per task spec):
//   1. Unending Resolve   — panic ≤30%
//   2. Drain Life         — emergency self-heal ≤50% when nothing else up
//   3. Howl of Terror     — multi-target panic fear
//   4. Felhunter Devour Magic — pet dispel
//   5. Pet interrupt      — Axe Toss / Spell Lock
//   6. Mortal Coil        — heal + horror
//   7. Dark Pact          — absorb shield (30-75%)
//   8. Group utility — Soulstone rez
//   9. Emergency CC — Shadowfury (3+ surround)
//  10. Major offensive CDs — Nether Portal → Tyrant → Grimoire: Felguard
//      → Vilefiend → Dreadstalkers → Demonic Strength
//  11. Ground AoE — Bilescourge Bombers, Implosion (3+ clustered)
//  12. DoT — Doom (talent refresh)
//  13. Soul Strike (Felguard nuke off-CD)
//  14. Demonbolt — Demonic Core spending
//  15. Power Siphon — generate cores when none
//  16. Hand of Gul'dan (3+ shards)
//  17. Filler Shadow Bolt
ApRule const kRules[] = {
    { ShouldUnendingResolve,    DoUnendingResolve,    "Unending Resolve (<=30%)"      },
    { ShouldDrainLifeEmergency, DoDrainLifeEmergency, "Drain Life (emergency)"        },
    { ShouldHowlOfTerror,       DoHowlOfTerror,       "Howl of Terror (AoE fear)"     },
    { ShouldPetDevourMagic,     DoPetDevourMagic,     "Felhunter Devour Magic"        },
    { ShouldPetInterrupt,       DoPetInterrupt,       "Pet interrupt (Axe Toss/Spell Lock)" },
    { ShouldMortalCoil,         DoMortalCoil,         "Mortal Coil (heal+horror)"     },
    { ShouldDarkPact,           DoDarkPact,           "Dark Pact (absorb 30-75%)"     },
    { ShouldSoulstone,          DoSoulstone,          "Soulstone (battle rez)"        },
    { ShouldShadowfury,         DoShadowfury,         "Shadowfury (3+ surround stun)" },
    { ShouldNetherPortal,       DoNetherPortal,       "Nether Portal (boss CD)"       },
    { ShouldDemonicTyrant,      DoDemonicTyrant,      "Summon Demonic Tyrant"         },
    { ShouldGrimoireFelguard,   DoGrimoireFelguard,   "Grimoire: Felguard"            },
    { ShouldVilefiend,          DoVilefiend,          "Summon Vilefiend"              },
    { ShouldCallDreadstalkers,  DoCallDreadstalkers,  "Call Dreadstalkers"            },
    { ShouldDemonicStrength,    DoDemonicStrength,    "Demonic Strength (Felguard)"   },
    { ShouldBilescourgeBombers, DoBilescourgeBombers, "Bilescourge Bombers"           },
    { ShouldImplosion,          DoImplosion,          "Implosion (3+ AoE)"            },
    { ShouldDoom,               DoDoom,               "Doom (talent refresh)"         },
    { ShouldSoulStrike,         DoSoulStrike,         "Soul Strike (Felguard nuke)"   },
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
