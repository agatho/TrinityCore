// Arms Warrior - WoW 12.0 enterprise rotation. Two-handed melee with Rage
// resource, Mortal Strike single-target focus, Cleave / Sweeping Strikes
// for AoE, Colossus Smash / Warbreaker damage window, Avatar / Thunderous
// Roar burst. Execute under 20% (or any HP with Massacre talent). Rend
// bleed for Deep Wounds upkeep.
//
// Survival ladder: Die by the Sword (parry chance + 30% DR) -> Defensive
// Stance (talent — DR toggle) -> Spell Reflection (magic absorb) ->
// Victory Rush (heal). Group utility: Battle Shout (group buff), Rallying
// Cry (group HP buff), Berserker Shout (anti-fear), Intervene (peel +
// damage redirect). CC: Pummel interrupt, Storm Bolt (talent stun),
// Hamstring slow, Intimidating Shout fear, Shockwave AoE stun.
//
// Rule order (panic-first ladder, then offensive ladder):
//   1) Anti-CC immunity     — Berserker Rage (fear/sap/incap break)
//   2) Spec defensive       — Die by the Sword (<=35% HP)
//   3) Interrupt            — Pummel, Storm Bolt fallback
//   4) Magic absorb         — Spell Reflection (enemy casting)
//   5) Slow / CC            — Hamstring, Intimidating Shout, Shockwave
//   6) Group utility        — Battle Shout, Rallying Cry, Intervene
//   7) Self heals / shields — Victory Rush / Impending Victory / Ignore Pain
//   8) Major CDs            — Avatar, Thunderous Roar, Warbreaker, Colossus Smash,
//                              Bladestorm, Sweeping Strikes
//   9) Rotation             — Execute, Skullsplitter, Mortal Strike, Overpower,
//                              Rend, Cleave, Slam
//  10) Auto-attack          — engage fallback
//
// Validated spell IDs (WoW 12.0):
//   12294 Mortal Strike    | 7384 Overpower         | 163201 Execute (Arms variant)
//   1464 Slam              | 227847 Bladestorm      | 845 Cleave (talent)
//   167105 Colossus Smash  | 260708 Sweeping Strikes| 107574 Avatar
//   260643 Skullsplitter   | 388539 Rend            | 384318 Thunderous Roar
//   262161 Warbreaker      | 46968 Shockwave        | 107570 Storm Bolt
//   1715 Hamstring         | 5246 Intimidating Shout| 18499 Berserker Rage
//   118038 Die by the Sword| 190456 Ignore Pain     | 34428 Victory Rush
//   202168 Impending Victory| 6552 Pummel           | 23920 Spell Reflection
//   6673 Battle Shout      | 97462 Rallying Cry     | 3411 Intervene
//
// Skipped spells (and why):
//   279423 Seasoned Soldier      — passive 2H damage bonus (no GCD)
//   137049 Arms Warrior          — spec auras (passive container)
//   462115 Arms Warrior (modern) — same, modern aura container
//   86101  Plate Specialization  — passive stat aura
//   76856  Mastery: Unshackled F.— Fury mastery, wrong spec anyway
//   162698 Stat Negation Aura    — internal stat-balance aura
//   386208 Defensive Stance      — stance toggle (talent; we don't manage stance dance)
//   262115 Deep Wounds debuff    — MS proc bleed (NOT cast directly; tracked via Rend instead)
//   100    Charge                — handled in baseline rotation
//   1680   Whirlwind (baseline)  — covered by Cleave (spec talent) for Arms AoE

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated against SpellName.csv) ----
constexpr uint32 MORTAL_STRIKE       = 12294;
constexpr uint32 OVERPOWER           = 7384;
constexpr uint32 EXECUTE             = 163201;       // Arms-flagged Execute (Massacre-aware)
constexpr uint32 SLAM                = 1464;
constexpr uint32 BLADESTORM          = 227847;       // Arms Bladestorm (channel)
constexpr uint32 CLEAVE              = 845;          // talent AoE
constexpr uint32 COLOSSUS_SMASH      = 167105;
constexpr uint32 SWEEPING_STRIKES    = 260708;
constexpr uint32 AVATAR              = 107574;
constexpr uint32 SKULLSPLITTER       = 260643;       // talent rage gen
constexpr uint32 REND                = 388539;       // talent bleed
constexpr uint32 THUNDEROUS_ROAR     = 384318;
constexpr uint32 WARBREAKER          = 262161;       // talent — replaces Colossus Smash AoE
constexpr uint32 SHOCKWAVE           = 46968;        // AoE stun
constexpr uint32 STORM_BOLT          = 107570;       // talent stun (interrupt fallback)
constexpr uint32 HAMSTRING           = 1715;
constexpr uint32 INTIMIDATING_SHOUT  = 5246;
constexpr uint32 BERSERKER_RAGE      = 18499;        // anti-fear / sap / incap immunity
constexpr uint32 DIE_BY_THE_SWORD    = 118038;       // 100% parry + 30% DR, 8s
constexpr uint32 IGNORE_PAIN         = 190456;       // talent — absorb shield
constexpr uint32 VICTORY_RUSH        = 34428;
constexpr uint32 IMPENDING_VICTORY   = 202168;       // talent replacement
constexpr uint32 PUMMEL              = 6552;
constexpr uint32 SPELL_REFLECTION    = 23920;
constexpr uint32 BATTLE_SHOUT        = 6673;
constexpr uint32 RALLYING_CRY        = 97462;
constexpr uint32 INTERVENE           = 3411;

constexpr uint8 POWER_RAGE_IDX = 1;

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

int32 Rage(ApPredicateContext const& ctx) { return ctx.bot.power(POWER_RAGE_IDX); }

// ---- Anti-CC immunity (highest panic priority) ----
bool ShouldBerserkerRage(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BERSERKER_RAGE)) return false;
    if (!ctx.bot.is_ready(BERSERKER_RAGE)) return false;
    // Berserker Rage clears (and grants 6s immunity to) Fear / Sap /
    // Incapacitate / Disorient effects. MECHANIC_FEAR=5,
    // MECHANIC_DISORIENTED=2, MECHANIC_HORROR=24, MECHANIC_SAPPED=30.
    return ctx.bot.has_mechanic(5)  || ctx.bot.has_mechanic(2) ||
           ctx.bot.has_mechanic(24) || ctx.bot.has_mechanic(30);
}
void DoBerserkerRage(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BERSERKER_RAGE); }

// ---- Survival ----
bool ShouldDieByTheSword(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DIE_BY_THE_SWORD)) return false;
    if (!ctx.bot.is_ready(DIE_BY_THE_SWORD)) return false;
    return ctx.bot.hp_pct() <= 35;
}
void DoDieByTheSword(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DIE_BY_THE_SWORD); }

bool ShouldIgnorePain(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(IGNORE_PAIN)) return false;
    if (Rage(ctx) < 40) return false;
    if (ctx.bot.has_aura(IGNORE_PAIN)) return false;
    return ctx.bot.hp_pct() <= 60;
}
void DoIgnorePain(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(IGNORE_PAIN); }

bool ShouldImpendingVictory(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(IMPENDING_VICTORY)) return false;
    if (!ctx.bot.is_ready(IMPENDING_VICTORY)) return false;
    return ctx.bot.hp_pct() <= 70;
}
void DoImpendingVictory(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(IMPENDING_VICTORY, ctx.bot.victim());
}

bool ShouldVictoryRush(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(IMPENDING_VICTORY)) return false;
    if (!ctx.bot.knows_spell(VICTORY_RUSH)) return false;
    return ctx.bot.hp_pct() <= 80;
}
void DoVictoryRush(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(VICTORY_RUSH, ctx.bot.victim());
}

bool ShouldSpellReflection(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SPELL_REFLECTION)) return false;
    if (!ctx.bot.is_ready(SPELL_REFLECTION)) return false;
    return ctx.bot.interruptible_caster() != nullptr;
}
void DoSpellReflection(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SPELL_REFLECTION); }

// ---- Maintenance ----
bool ShouldBattleShout(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BATTLE_SHOUT)) return false;
    return !ctx.bot.has_aura(BATTLE_SHOUT);
}
void DoBattleShout(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BATTLE_SHOUT); }

// ---- Group utility ----
bool ShouldRallyingCry(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(RALLYING_CRY)) return false;
    if (!ctx.bot.is_ready(RALLYING_CRY)) return false;
    auto const* low = ctx.group.lowest_hp_on_map(ctx.bot.map_id(), Role::Unknown, ctx.bot.raw().position.x, ctx.bot.raw().position.y, ctx.bot.raw().position.z, 45.0f);
    if (low && low->online && low->hp > 0)
        return (low->hp * 100) / low->max_hp <= 35;
    return false;
}
void DoRallyingCry(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(RALLYING_CRY); }

bool ShouldIntervene(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(INTERVENE)) return false;
    if (!ctx.bot.is_ready(INTERVENE)) return false;
    auto const* low = ctx.group.lowest_hp_on_map(ctx.bot.map_id(), Role::Unknown, ctx.bot.raw().position.x, ctx.bot.raw().position.y, ctx.bot.raw().position.z, 45.0f);
    if (!low || !low->online || low->hp <= 0) return false;
    if (low->guid == ctx.bot.raw().guid) return false;
    return (low->hp * 100) / low->max_hp <= 30;
}
void DoIntervene(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* low = ctx.group.lowest_hp_on_map(ctx.bot.map_id(), Role::Unknown, ctx.bot.raw().position.x, ctx.bot.raw().position.y, ctx.bot.raw().position.z, 45.0f))
        e.cast(INTERVENE, low->guid);
}

// ---- Interrupt / CC ----
bool ShouldPummel(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(PUMMEL)) return false;
    if (!ctx.bot.is_ready(PUMMEL)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    return ctx.bot.kick_target(pvp, 5.0f) != nullptr;
}
void DoPummel(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 5.0f))
        e.cast(PUMMEL, c->guid);
}

bool ShouldStormBolt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(STORM_BOLT)) return false;
    if (!ctx.bot.is_ready(STORM_BOLT)) return false;
    if (ctx.bot.is_ready(PUMMEL)) return false;
    return ctx.bot.interruptible_caster() != nullptr;
}
void DoStormBolt(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* c = ctx.bot.interruptible_caster())
        e.cast(STORM_BOLT, c->guid);
}

bool ShouldShockwave(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHOCKWAVE)) return false;
    if (!ctx.bot.is_ready(SHOCKWAVE)) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(10.0f) >= 3;
}
void DoShockwave(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SHOCKWAVE); }

bool ShouldIntimidatingShout(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(INTIMIDATING_SHOUT)) return false;
    if (!ctx.bot.is_ready(INTIMIDATING_SHOUT)) return false;
    return ctx.bot.enemies_within(8.0f) >= 2 && ctx.bot.hp_pct() <= 50;
}
void DoIntimidatingShout(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(INTIMIDATING_SHOUT); }

// Hamstring fires either on the bot's current victim OR — in BGs where we
// have a friendly flag carrier — on an enemy meleeing the carrier (the FC
// peel pattern). The carrier-peel branch is evaluated first because slowing
// the FC's attacker is higher leverage than slowing whatever the bot is on.
NearbyUnit const* PickHamstringTarget(ApPredicateContext const& ctx)
{
    if (ctx.pvp.in_battleground && !ctx.pvp.friendly_flag_carrier.IsEmpty())
    {
        if (NearbyUnit const* peel = ctx.bot.enemy_near_friendly_carrier(8.0f))
        {
            if (!ctx.bot.has_aura(HAMSTRING, peel->guid)) return peel;
        }
    }
    return ctx.bot.victim_info();
}

bool ShouldHamstring(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HAMSTRING)) return false;
    if (!ctx.bot.is_ready(HAMSTRING)) return false;
    if (Rage(ctx) < 30) return false;
    NearbyUnit const* t = PickHamstringTarget(ctx);
    if (!t || t->hp <= 0) return false;
    if (ctx.bot.has_aura(HAMSTRING, t->guid)) return false;
    // Original gate: only fire when not in AoE territory (saves Rage for
    // Whirlwind / Sweeping Strikes). FC-peel ignores that — peeling for the
    // carrier is worth the Rage trade.
    const bool fc_peel = ctx.pvp.in_battleground &&
                         !ctx.pvp.friendly_flag_carrier.IsEmpty() &&
                         t != ctx.bot.victim_info();
    return fc_peel || ctx.bot.enemies_within(8.0f) == 0;
}
void DoHamstring(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (NearbyUnit const* t = PickHamstringTarget(ctx))
        e.cast(HAMSTRING, t->guid);
}

// ---- Major offensive cooldowns ----
bool ShouldAvatar(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(AVATAR)) return false;
    if (!ctx.bot.is_ready(AVATAR)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(10.0f) >= 3;
}
void DoAvatar(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(AVATAR); }

bool ShouldThunderousRoar(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(THUNDEROUS_ROAR)) return false;
    if (!ctx.bot.is_ready(THUNDEROUS_ROAR)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(12.0f) >= 2;
}
void DoThunderousRoar(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(THUNDEROUS_ROAR); }

bool ShouldWarbreaker(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(WARBREAKER)) return false;
    if (!ctx.bot.is_ready(WARBREAKER)) return false;
    return ctx.bot.enemies_within(10.0f) >= 2;
}
void DoWarbreaker(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(WARBREAKER); }

bool ShouldColossusSmash(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(WARBREAKER)) return false;
    if (!ctx.bot.knows_spell(COLOSSUS_SMASH)) return false;
    if (!ctx.bot.is_ready(COLOSSUS_SMASH)) return false;
    return true;
}
void DoColossusSmash(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(COLOSSUS_SMASH, ctx.bot.victim());
}

bool ShouldBladestormAoe(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BLADESTORM)) return false;
    if (!ctx.bot.is_ready(BLADESTORM)) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(8.0f) >= 3;
}
void DoBladestorm(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLADESTORM); }

bool ShouldSweepingStrikes(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SWEEPING_STRIKES)) return false;
    if (!ctx.bot.is_ready(SWEEPING_STRIKES)) return false;
    if (ctx.bot.has_aura(SWEEPING_STRIKES)) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(8.0f) >= 2;
}
void DoSweepingStrikes(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SWEEPING_STRIKES); }

// ---- Damage ----
bool ShouldExecute(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(EXECUTE)) return false;
    if (!ctx.bot.is_ready(EXECUTE)) return false;
    if (Rage(ctx) < 20) return false;
    return TargetExecuteRange(ctx);
}
void DoExecute(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(EXECUTE, ctx.bot.victim());
}

bool ShouldSkullsplitter(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SKULLSPLITTER)) return false;
    if (!ctx.bot.is_ready(SKULLSPLITTER)) return false;
    return Rage(ctx) <= 70;
}
void DoSkullsplitter(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SKULLSPLITTER, ctx.bot.victim());
}

bool ShouldMortalStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MORTAL_STRIKE)) return false;
    return ctx.bot.is_ready(MORTAL_STRIKE);
}
void DoMortalStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MORTAL_STRIKE, ctx.bot.victim());
}

bool ShouldOverpower(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(OVERPOWER)) return false;
    return ctx.bot.is_ready(OVERPOWER);
}
void DoOverpower(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(OVERPOWER, ctx.bot.victim());
}

bool ShouldRend(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(REND)) return false;
    if (!ctx.bot.is_ready(REND)) return false;
    // Track Rend itself, NOT Deep Wounds. Deep Wounds (262115) is the
    // MS-proc bleed that re-applies every ~5s, so it almost never
    // expires — gating Rend on DW caused Rend to drop ~30-40% of its
    // intended uptime. Track the Rend aura (388539) at 30% pandemic
    // window (~4.5s of a 15s bleed).
    AuraEntry const* a = ctx.bot.find_aura(REND, ctx.bot.victim());
    return !a || a->remaining.count() <= 4500;
}
void DoRend(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(REND, ctx.bot.victim());
}

bool ShouldCleave(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CLEAVE)) return false;
    if (!ctx.bot.is_ready(CLEAVE)) return false;
    if (Rage(ctx) < 20) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(8.0f) >= 3;
}
void DoCleave(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CLEAVE); }

bool ShouldSlam(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SLAM)) return false;
    return Rage(ctx) >= 20;
}
void DoSlam(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SLAM, ctx.bot.victim());
}

bool AlwaysInCombat(ApPredicateContext const& ctx) { return ctx.bot.in_combat(); }
void DoAutoAttack(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    ObjectGuid t = ctx.bot.victim();
    if (t.IsEmpty()) t = ctx.bot.current_target();
    // Retaliate fallback (2026-06-17): in combat with no victim/target the bot
    // looped "Engage auto attack" as a no-op while taking damage (the CombatLoop
    // wedge cluster). Engage whatever is meleeing us (attackers are adjacent and
    // reachable), else the nearest visible enemy, so combat actually resolves.
    // Strictly additive: only runs when no target was already selected.
    if (t.IsEmpty())
        for (auto const& u : ctx.bot.attackers())
            if (u.hp > 0) { t = u.guid; break; }
    if (t.IsEmpty())
        for (auto const& u : ctx.bot.nearby_enemies())
            if (u.hp > 0 && u.in_los) { t = u.guid; break; }
    if (!t.IsEmpty()) e.start_attack(t);
}

// Rule table — order is meaningful. See the file header comment for the
// canonical ladder; the comments below restate each tier for quick scanning.
ApRule const kRules[] = {
    // 1) Anti-CC immunity (must clear fear/sap before anything else can fire)
    { ShouldBerserkerRage,     DoBerserkerRage,     "Berserker Rage (anti-fear)"  },
    // 2) Spec defensive ladder
    { ShouldDieByTheSword,     DoDieByTheSword,     "Die by the Sword (<=35%)"    },
    // 3) Interrupts
    { ShouldPummel,            DoPummel,            "Pummel (interrupt)"          },
    { ShouldStormBolt,         DoStormBolt,         "Storm Bolt (interrupt fb)"   },
    // 4) Magic absorb
    { ShouldSpellReflection,   DoSpellReflection,   "Spell Reflection"            },
    // 5) Slow / CC
    { ShouldShockwave,         DoShockwave,         "Shockwave (3+ AoE stun)"     },
    { ShouldIntimidatingShout, DoIntimidatingShout, "Intimidating Shout (panic)"  },
    { ShouldHamstring,         DoHamstring,         "Hamstring (slow)"            },
    // 6) Maintenance / group utility
    { ShouldBattleShout,       DoBattleShout,       "Battle Shout (group buff)"   },
    { ShouldRallyingCry,       DoRallyingCry,       "Rallying Cry"                },
    { ShouldIntervene,         DoIntervene,         "Intervene"                   },
    // 7) Self-heal / shield (cheap, GCD-locked, fire before CDs)
    { ShouldIgnorePain,        DoIgnorePain,        "Ignore Pain"                 },
    { ShouldVictoryRush,       DoVictoryRush,       "Victory Rush"                },
    { ShouldImpendingVictory,  DoImpendingVictory,  "Impending Victory"           },
    // 8) Major offensive cooldowns (boss / multi-target gated)
    { ShouldAvatar,            DoAvatar,            "Avatar"                      },
    { ShouldThunderousRoar,    DoThunderousRoar,    "Thunderous Roar"             },
    { ShouldSweepingStrikes,   DoSweepingStrikes,   "Sweeping Strikes (2+)"       },
    { ShouldBladestormAoe,     DoBladestorm,        "Bladestorm (3+ AoE)"         },
    { ShouldWarbreaker,        DoWarbreaker,        "Warbreaker (2+ AoE)"         },
    { ShouldColossusSmash,     DoColossusSmash,     "Colossus Smash"              },
    // 9) Rotation (signature MS-window first, then proc spends, then fillers)
    { ShouldExecute,           DoExecute,           "Execute (<=20%)"             },
    { ShouldMortalStrike,      DoMortalStrike,      "Mortal Strike (signature)"   },
    { ShouldOverpower,         DoOverpower,         "Overpower (proc)"            },
    { ShouldSkullsplitter,     DoSkullsplitter,     "Skullsplitter (rage gen)"    },
    { ShouldRend,              DoRend,              "Rend (refresh bleed)"        },
    { ShouldCleave,            DoCleave,            "Cleave (3+ AoE)"             },
    { ShouldSlam,              DoSlam,              "Slam (rage spender)"         },
    // 10) Auto-attack fallback
    { AlwaysInCombat,          DoAutoAttack,        "Engage auto attack"          },
};

} // anonymous

void RegisterApl_Warrior_Arms()
{
    constexpr uint32 SPEC_WARRIOR_ARMS = 71;
    RegisterRotation(CLASS_WARRIOR, SPEC_WARRIOR_ARMS, ApRotation{kRules});
}

} // namespace Playerbot::Combat
