// Fury Warrior - WoW 12.0 enterprise rotation. Dual-wield berserker with
// Enrage uptime via Bloodthirst / Rampage cycle, Raging Blow proc spending,
// Whirlwind cleave (Wrecking Ball / Meat Cleaver auras for AoE), Recklessness
// + Avatar burst, Odyn's Fury / Bladestorm AoE bursts, Execute under 20%
// (or any HP via Massacre talent).
//
// Survival: Enraged Regen, Die by the Sword (parry / DR), Rallying Cry
// (group HP buff), Spell Reflection, Berserker Rage anti-fear, Impending
// Victory / Victory Rush. Group utility: Battle Shout, Intervene (peel),
// Storm Bolt stun. CC: Pummel interrupt, Storm Bolt fallback, Intimidating
// Shout fear, Hamstring slow.
//
// Rule order (panic-first ladder, then offensive ladder):
//   1) Anti-CC immunity     — Berserker Rage (fear/sap/incap break)
//   2) Spec defensives      — Enraged Regen / Die by the Sword (<=45%)
//   3) Interrupts           — Pummel, Storm Bolt fallback
//   4) Magic absorb         — Spell Reflection (enemy casting)
//   5) Slow / CC            — Hamstring, Intimidating Shout
//   6) Group utility        — Battle Shout, Rallying Cry, Intervene
//   7) Self heals / shields — Victory Rush / Impending Victory / Ignore Pain
//   8) Major CDs            — Recklessness, Avatar, Ravager, Odyn's Fury, Bladestorm
//   9) Rotation             — Rampage (Enrage), Execute (Massacre), Bloodbath/Bloodthirst,
//                              Raging Blow, Onslaught, Whirlwind
//  10) Auto-attack          — engage fallback
//
// Validated spell IDs (WoW 12.0):
//   23881 Bloodthirst       | 335096 Bloodbath (HT) | 85288  Raging Blow
//   184367 Rampage          | 5308   Execute (Fury) | 190411 Whirlwind
//   385059 Odyn's Fury      | 1719   Recklessness   | 107574 Avatar
//   46924  Bladestorm (Fury)| 228920 Ravager        | 315720 Onslaught
//   184361 Enrage (aura)    | 184364 Enraged Regen  | 118038 Die by the Sword
//   6552   Pummel           | 107570 Storm Bolt     | 5246   Intimidating Shout
//   1715   Hamstring        | 18499  Berserker Rage | 23920  Spell Reflection
//   97462  Rallying Cry     | 202168 Impending Vict.| 34428  Victory Rush
//   6673   Battle Shout     | 3411   Intervene      | 190456 Ignore Pain
//
// Skipped spells (and why):
//   184362 Enrage (BaseLevel=1)  — Enrage talent grant aura, not the buff itself
//                                  (real buff = 184361, applied by Bloodthirst/Rampage).
//                                  We track 184361 (the actual damage buff).
//   231842 Dual Wield            — passive (auto-grant when 2x 1H equipped)
//   46917  Titan's Grip          — passive 2H dual-wield (no GCD)
//   76856  Mastery: Unshackled F.— passive mastery aura
//   86110  Plate Specialization  — passive stat aura
//   137050 Fury Warrior          — spec aura container (passive)
//   462117 Fury Warrior (modern) — modern aura container (passive)
//   162698 Stat Negation Aura    — internal stat-balance aura
//   354788 Slaughterhouse        — Hero talent PROC (applied by Bloodthirst/Execute,
//                                  NOT directly cast). The previous code referenced
//                                  393713 which was a phial item — removed.
//   100    Charge                — handled in baseline rotation

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated against SpellName.csv) ----
constexpr uint32 BLOODTHIRST         = 23881;
constexpr uint32 BLOODBATH           = 335096;       // hero talent — replaces BT during burst
constexpr uint32 RAGING_BLOW         = 85288;
constexpr uint32 RAMPAGE             = 184367;
constexpr uint32 EXECUTE_FURY        = 5308;
constexpr uint32 WHIRLWIND           = 190411;
constexpr uint32 ODYNS_FURY          = 385059;       // talent CD
constexpr uint32 RECKLESSNESS        = 1719;
constexpr uint32 AVATAR              = 107574;
constexpr uint32 BLADESTORM_FURY     = 46924;        // Fury bladestorm
constexpr uint32 RAVAGER             = 228920;       // talent
constexpr uint32 ONSLAUGHT           = 315720;       // talent — instant attack
// Enrage damage buff aura applied by Bloodthirst (crit) or Rampage. Used to
// gate Rampage refresh — IMPORTANT: 184361 is the damage buff itself, NOT
// 184362 (which is the BaseLevel=1 talent grant aura — same name, different
// effect). Previous code used 184362 which never matched.
constexpr uint32 ENRAGE_AURA         = 184361;
constexpr uint32 PUMMEL              = 6552;
constexpr uint32 STORM_BOLT          = 107570;
constexpr uint32 INTIMIDATING_SHOUT  = 5246;
constexpr uint32 HAMSTRING           = 1715;
constexpr uint32 BERSERKER_RAGE      = 18499;
constexpr uint32 ENRAGED_REGEN       = 184364;
constexpr uint32 RALLYING_CRY        = 97462;
constexpr uint32 SPELL_REFLECTION    = 23920;
constexpr uint32 IMPENDING_VICTORY   = 202168;
constexpr uint32 VICTORY_RUSH        = 34428;
constexpr uint32 BATTLE_SHOUT        = 6673;
constexpr uint32 INTERVENE           = 3411;
constexpr uint32 IGNORE_PAIN         = 190456;
constexpr uint32 DIE_BY_THE_SWORD    = 118038;       // 100% parry, 30% dmg reduction, 8s, 2 min CD

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

bool HasEnrage(ApPredicateContext const& ctx) { return ctx.bot.has_aura(ENRAGE_AURA); }

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
bool ShouldEnragedRegen(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ENRAGED_REGEN)) return false;
    if (!ctx.bot.is_ready(ENRAGED_REGEN)) return false;
    return ctx.bot.hp_pct() <= 35;
}
void DoEnragedRegen(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ENRAGED_REGEN); }

// Die by the Sword — Fury's only short-CD damage-reduction CD. 100% parry +
// 30% dmg reduction for 8s. Fires at <=45% HP with active fight. Skip when
// Enraged Regen is active (no need to double-CD) or Rallying Cry (group-wide
// buff is up).
bool ShouldDieByTheSword(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DIE_BY_THE_SWORD)) return false;
    if (!ctx.bot.is_ready(DIE_BY_THE_SWORD)) return false;
    if (ctx.bot.hp_pct() > 45) return false;
    if (ctx.bot.find_aura(ENRAGED_REGEN, ObjectGuid::Empty)) return false;
    if (ctx.bot.find_aura(RALLYING_CRY,  ObjectGuid::Empty)) return false;
    return true;
}
void DoDieByTheSword(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(DIE_BY_THE_SWORD);
}

bool ShouldRallyingCry(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(RALLYING_CRY)) return false;
    if (!ctx.bot.is_ready(RALLYING_CRY)) return false;
    auto const* low = ctx.group.lowest_hp_on_map(ctx.bot.map_id(), Role::Unknown, ctx.bot.raw().position.x, ctx.bot.raw().position.y, ctx.bot.raw().position.z, 45.0f);
    if (low && low->online && low->hp > 0)
        return (low->hp * 100) / low->max_hp <= 35;
    return ctx.bot.hp_pct() <= 50;
}
void DoRallyingCry(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(RALLYING_CRY); }

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

bool ShouldIgnorePain(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(IGNORE_PAIN)) return false;
    if (Rage(ctx) < 40) return false;
    if (ctx.bot.has_aura(IGNORE_PAIN)) return false;
    return ctx.bot.hp_pct() <= 60;
}
void DoIgnorePain(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(IGNORE_PAIN); }

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

bool ShouldIntimidatingShout(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(INTIMIDATING_SHOUT)) return false;
    if (!ctx.bot.is_ready(INTIMIDATING_SHOUT)) return false;
    return ctx.bot.enemies_within(8.0f) >= 2 && ctx.bot.hp_pct() <= 50;
}
void DoIntimidatingShout(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(INTIMIDATING_SHOUT); }

// FC peel: in a BG with a friendly carrier, prefer slowing an enemy on the
// carrier over slowing our own victim. See Apl_Warrior_Arms.cpp for the
// rationale (same pattern; kept duplicated rather than shared so each spec
// can tune its Rage gates independently).
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
bool ShouldRecklessness(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RECKLESSNESS)) return false;
    if (!ctx.bot.is_ready(RECKLESSNESS)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(10.0f) >= 3;
}
void DoRecklessness(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(RECKLESSNESS); }

bool ShouldAvatar(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(AVATAR)) return false;
    if (!ctx.bot.is_ready(AVATAR)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(10.0f) >= 3;
}
void DoAvatar(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(AVATAR); }

bool ShouldOdynsFury(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ODYNS_FURY)) return false;
    if (!ctx.bot.is_ready(ODYNS_FURY)) return false;
    return true;
}
void DoOdynsFury(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ODYNS_FURY); }

bool ShouldBladestormAoe(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BLADESTORM_FURY)) return false;
    if (!ctx.bot.is_ready(BLADESTORM_FURY)) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(8.0f) >= 3;
}
void DoBladestorm(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLADESTORM_FURY); }

bool ShouldRavager(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RAVAGER)) return false;
    if (!ctx.bot.is_ready(RAVAGER)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(10.0f) >= 2;
}
void DoRavager(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(RAVAGER, v->x, v->y, v->z);
    else
        e.cast(RAVAGER, ctx.bot.victim());
}

bool ShouldOnslaught(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ONSLAUGHT)) return false;
    if (!ctx.bot.is_ready(ONSLAUGHT)) return false;
    return Rage(ctx) >= 30;
}
void DoOnslaught(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ONSLAUGHT, ctx.bot.victim());
}

// ---- Damage ----
// Rampage: 80-rage spender that applies / refreshes Enrage. We pool to 100
// before spending unless Enrage is about to drop (<=1.5s) — refreshing it
// is worth more than the extra Rampage damage from pooling further.
bool ShouldRampage(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RAMPAGE)) return false;
    if (!ctx.bot.is_ready(RAMPAGE)) return false;
    if (Rage(ctx) < 80) return false;
    AuraEntry const* a = ctx.bot.find_aura(ENRAGE_AURA);
    return !a || a->remaining.count() <= 1500 || Rage(ctx) >= 100;
}
void DoRampage(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RAMPAGE, ctx.bot.victim());
}

bool ShouldExecute(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(EXECUTE_FURY)) return false;
    if (!ctx.bot.is_ready(EXECUTE_FURY)) return false;
    return TargetExecuteRange(ctx);
}
void DoExecute(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(EXECUTE_FURY, ctx.bot.victim());
}

bool ShouldBloodbath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BLOODBATH)) return false;
    return ctx.bot.is_ready(BLOODBATH);
}
void DoBloodbath(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BLOODBATH, ctx.bot.victim());
}

bool ShouldBloodthirst(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(BLOODBATH)) return false;       // BB replaces BT when picked
    if (!ctx.bot.knows_spell(BLOODTHIRST)) return false;
    return ctx.bot.is_ready(BLOODTHIRST);
}
void DoBloodthirst(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BLOODTHIRST, ctx.bot.victim());
}

bool ShouldRagingBlow(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RAGING_BLOW)) return false;
    return ctx.bot.is_ready(RAGING_BLOW);
}
void DoRagingBlow(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RAGING_BLOW, ctx.bot.victim());
}

// Whirlwind: AoE swing AND single-target buff (next abilities cleave). With
// Meat Cleaver talent the single-target buff is a meaningful DPS gain even
// vs one target, but it's a Rage cost — only spend when AoE is happening
// or we've got Rage flooding to dump.
bool ShouldWhirlwind(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(WHIRLWIND)) return false;
    return ctx.bot.enemies_within(8.0f) >= 2 || Rage(ctx) >= 30;
}
void DoWhirlwind(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(WHIRLWIND); }

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
    // 1) Anti-CC immunity
    { ShouldBerserkerRage,     DoBerserkerRage,     "Berserker Rage (anti-fear)"  },
    // 2) Spec defensive ladder
    { ShouldEnragedRegen,      DoEnragedRegen,      "Enraged Regen (<=35%)"       },
    { ShouldDieByTheSword,     DoDieByTheSword,     "Die by the Sword (<=45%)"    },
    // 3) Interrupts
    { ShouldPummel,            DoPummel,            "Pummel (interrupt)"          },
    { ShouldStormBolt,         DoStormBolt,         "Storm Bolt (interrupt fb)"   },
    // 4) Magic absorb
    { ShouldSpellReflection,   DoSpellReflection,   "Spell Reflection"            },
    // 5) Slow / CC
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
    // 8) Major offensive cooldowns
    { ShouldRecklessness,      DoRecklessness,      "Recklessness"                },
    { ShouldAvatar,            DoAvatar,            "Avatar"                      },
    { ShouldRavager,           DoRavager,           "Ravager"                     },
    { ShouldOdynsFury,         DoOdynsFury,         "Odyn's Fury"                 },
    { ShouldBladestormAoe,     DoBladestorm,        "Bladestorm (3+ AoE)"         },
    // 9) Rotation — Rampage (Enrage maintenance) → Execute window → Bloodbath/BT
    //               → Raging Blow → Onslaught → Whirlwind (filler/AoE)
    { ShouldRampage,           DoRampage,           "Rampage (Enrage)"            },
    { ShouldExecute,           DoExecute,           "Execute (<=20%)"             },
    { ShouldBloodbath,         DoBloodbath,         "Bloodbath"                   },
    { ShouldBloodthirst,       DoBloodthirst,       "Bloodthirst"                 },
    { ShouldRagingBlow,        DoRagingBlow,        "Raging Blow"                 },
    { ShouldOnslaught,         DoOnslaught,         "Onslaught"                   },
    { ShouldWhirlwind,         DoWhirlwind,         "Whirlwind (filler/AoE)"      },
    // 10) Auto-attack fallback
    { AlwaysInCombat,          DoAutoAttack,        "Engage auto attack"          },
};

} // anonymous

void RegisterApl_Warrior_Fury()
{
    constexpr uint32 SPEC_WARRIOR_FURY = 72;
    RegisterRotation(CLASS_WARRIOR, SPEC_WARRIOR_FURY, ApRotation{kRules});
}

} // namespace Playerbot::Combat
