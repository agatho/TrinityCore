// Arms Warrior - WoW 12.1.0.69587 (Midnight) enterprise rotation. Two-handed
// melee with Rage resource, Mortal Strike single-target focus, Cleave /
// Sweeping Strikes for AoE, Colossus Smash damage window followed by
// Demolish (Colossus hero tree), Avatar burst. Execute under 20% (35% with
// Massacre, any HP on a Sudden Death proc). Rend bleed for Deep Wounds upkeep.
//
// Survival ladder: Die by the Sword (parry chance + 30% DR) -> Defensive
// Stance (<=30% HP, swapped back to Battle Stance once recovered) -> Spell
// Reflection (magic absorb) -> Victory Rush / Impending Victory (heal).
// Group utility: Battle Shout (group buff), Rallying Cry (group HP buff),
// Berserker Rage / Berserker Shout (anti-fear), Intervene (peel + damage
// redirect). CC: Pummel interrupt, Storm Bolt (talent stun), Hamstring slow,
// Piercing Howl AoE snare (PvP), Intimidating Shout fear, Shockwave AoE stun.
//
// Rule order (panic-first ladder, then offensive ladder):
//   1) Anti-CC immunity     - Berserker Rage / Berserker Shout (fear/sap/incap break)
//   2) Spec defensive       - Die by the Sword (<=35% HP), Defensive Stance (<=30%)
//   3) Interrupt            - Pummel, Storm Bolt fallback
//   4) Magic absorb         - Spell Reflection (enemy casting)
//   5) Slow / CC            - Shockwave, Intimidating Shout, Piercing Howl, Hamstring
//   6) Group utility        - Battle Stance, Battle Shout, Rallying Cry, Intervene
//   7) Self heals / shields - Ignore Pain / Victory Rush / Impending Victory
//   8) Major CDs            - Avatar, Sweeping Strikes, Bladestorm, Colossus Smash,
//                              Demolish
//   9) Rotation             - Execute, Mortal Strike, Overpower, Rend, Cleave,
//                              Wrecking Throw, Slam
//  10) Auto-attack          - engage fallback
//
// Validated spell IDs (WoW 12.1.0.69587):
//   12294 Mortal Strike    | 7384 Overpower         | 163201 Execute (Arms variant)
//   1464 Slam              | 227847 Bladestorm      | 845 Cleave (talent)
//   167105 Colossus Smash  | 260708 Sweeping Strikes| 107574 Avatar
//   772 Rend               | 436358 Demolish        | 384110 Wrecking Throw
//   46968 Shockwave        | 107570 Storm Bolt      | 12323 Piercing Howl
//   1715 Hamstring         | 5246 Intimidating Shout| 18499 Berserker Rage
//   384100 Berserker Shout | 386164 Battle Stance   | 386208 Defensive Stance
//   118038 Die by the Sword| 1277297 Ignore Pain    | 34428 Victory Rush
//   202168 Impending Victory| 6552 Pummel           | 23920 Spell Reflection
//   6673 Battle Shout      | 97462 Rallying Cry     | 3411 Intervene
//   Aura-only: 388539 Rend bleed | 208086 Colossus Smash debuff | 52437 Sudden Death
//   Passive gates: 281001 Massacre
//
// Skipped spells (and why):
//   260643 Skullsplitter        - removed from the Arms tree in 12.1
//   384318 Thunderous Roar      - removed from the Arms tree in 12.1
//   262161 Warbreaker           - removed in 12.1 (209577 is the Legion artifact spell)
//   209577 Warbreaker (artifact)- unlearnable artifact ability, not a 12.1 talent
//   1244088 Interpose / 3411 Intervene-target-location, 6544 Heroic Leap
//                               - ground-target positioning the bot does not do
//   376079 Champion's Spear     - not in the curated Arms builds (gated anyway if added)
//   228920 Ravager              - Arms tree option not in the curated builds
//   1229376 Single-Button Assistant - client convenience macro, not a rotation ability
//   262115 Deep Wounds debuff   - MS proc bleed (NOT cast directly; tracked via Rend instead)
//   100    Charge               - handled in baseline rotation
//   1680   Whirlwind (baseline) - covered by Cleave (spec talent) for Arms AoE

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated against SpellName.csv) ----
constexpr uint32 MORTAL_STRIKE       = 12294;
constexpr uint32 OVERPOWER           = 7384;
constexpr uint32 EXECUTE             = 163201;       // Arms-flagged Execute (Massacre-aware)
constexpr uint32 SLAM                = 1464;
constexpr uint32 BLADESTORM          = 227847;       // Arms Bladestorm (channel)
constexpr uint32 CLEAVE              = 845;          // talent AoE
constexpr uint32 COLOSSUS_SMASH      = 167105;
constexpr uint32 SWEEPING_STRIKES    = 260708;
constexpr uint32 AVATAR              = 107574;
constexpr uint32 DEMOLISH            = 436358;       // Colossus hero tree - 2s channel burst
constexpr uint32 WRECKING_THROW      = 384110;       // talent - 25y armor-ignoring throw
constexpr uint32 REND                = 772;          // talent bleed (12.1 cast id)
constexpr uint32 SHOCKWAVE           = 46968;        // AoE stun
constexpr uint32 STORM_BOLT          = 107570;       // talent stun (interrupt fallback)
constexpr uint32 HAMSTRING           = 1715;
constexpr uint32 PIERCING_HOWL       = 12323;        // talent AoE snare (PvP peel)
constexpr uint32 INTIMIDATING_SHOUT  = 5246;
constexpr uint32 BERSERKER_RAGE      = 18499;        // anti-fear / sap / incap immunity
constexpr uint32 BERSERKER_SHOUT     = 384100;       // talent - overrides Berserker Rage
constexpr uint32 BATTLE_STANCE       = 386164;       // talent - default DPS stance
constexpr uint32 DEFENSIVE_STANCE    = 386208;       // talent - DR stance (panic swap)
constexpr uint32 DIE_BY_THE_SWORD    = 118038;       // 100% parry + 30% DR, 8s
constexpr uint32 IGNORE_PAIN         = 1277297;      // Arms spec-tree talent - absorb shield
constexpr uint32 VICTORY_RUSH        = 34428;
constexpr uint32 IMPENDING_VICTORY   = 202168;       // talent replacement
constexpr uint32 PUMMEL              = 6552;
constexpr uint32 SPELL_REFLECTION    = 23920;
constexpr uint32 BATTLE_SHOUT        = 6673;
constexpr uint32 RALLYING_CRY        = 97462;
constexpr uint32 INTERVENE           = 3411;
// Aura / passive ids (never cast; predicates only).
constexpr uint32 REND_BLEED_AURA     = 388539;       // Rend DoT on the target (772 applies it)
constexpr uint32 COLOSSUS_SMASH_DEBUFF = 208086;     // damage-taken window on the target
constexpr uint32 SUDDEN_DEATH_AURA   = 52437;        // proc: Execute usable at any HP
constexpr uint32 MASSACRE            = 281001;       // passive: Execute usable below 35%

constexpr uint8 POWER_RAGE_IDX = 1;

bool HasLiveTarget(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && !ctx.bot.victim().IsEmpty();
}

// Execute window: 20% baseline, 35% with the Massacre passive, any HP while
// a Sudden Death proc is up (both are [R] in the curated Arms build).
bool TargetExecuteRange(ApPredicateContext const& ctx)
{
    NearbyUnit const* t = ctx.bot.victim_info();
    if (!t || t->max_hp <= 0 || t->hp <= 0) return false;
    if (ctx.bot.has_aura(SUDDEN_DEATH_AURA)) return true;
    const int32 threshold = ctx.bot.knows_spell(MASSACRE) ? 35 : 20;
    return (t->hp * 100) / t->max_hp <= threshold;
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

// Berserker Shout (talent) overrides Berserker Rage in the spellbook, so
// resolve which of the two the bot actually owns (same two-branch idiom as
// Victory Rush / Impending Victory).
uint32 BerserkerRageId(ApPredicateContext const& ctx)
{
    return ctx.bot.knows_spell(BERSERKER_SHOUT) ? BERSERKER_SHOUT : BERSERKER_RAGE;
}

// ---- Anti-CC immunity (highest panic priority) ----
bool ShouldBerserkerRage(ApPredicateContext const& ctx)
{
    const uint32 id = BerserkerRageId(ctx);
    if (!ctx.bot.knows_spell(id)) return false;
    if (!ctx.bot.is_ready(id)) return false;
    // Berserker Rage clears (and grants 6s immunity to) Fear / Sap /
    // Incapacitate / Disorient effects. MECHANIC_FEAR=5,
    // MECHANIC_DISORIENTED=2, MECHANIC_HORROR=24, MECHANIC_SAPPED=30.
    return ctx.bot.has_mechanic(5)  || ctx.bot.has_mechanic(2) ||
           ctx.bot.has_mechanic(24) || ctx.bot.has_mechanic(30);
}
void DoBerserkerRage(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(BerserkerRageId(ctx)); }

// ---- Survival ----
bool ShouldDieByTheSword(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DIE_BY_THE_SWORD)) return false;
    if (!ctx.bot.is_ready(DIE_BY_THE_SWORD)) return false;
    return ctx.bot.hp_pct() <= 35;
}
void DoDieByTheSword(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DIE_BY_THE_SWORD); }

// Defensive Stance: flat DR toggle. Swap in at <=30% HP; ShouldBattleStance
// swaps back once HP recovers (>=70%) or combat ends. The 3s stance
// category cooldown prevents ping-pong.
bool ShouldDefensiveStance(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DEFENSIVE_STANCE)) return false;
    if (!ctx.bot.is_ready(DEFENSIVE_STANCE)) return false;
    if (ctx.bot.has_aura(DEFENSIVE_STANCE)) return false;
    return ctx.bot.hp_pct() <= 30;
}
void DoDefensiveStance(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DEFENSIVE_STANCE); }

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
// Battle Stance is the Arms default. Re-enter it when no stance is up, or
// when Defensive Stance was taken as a panic swap and HP has recovered.
bool ShouldBattleStance(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BATTLE_STANCE)) return false;
    if (!ctx.bot.is_ready(BATTLE_STANCE)) return false;
    if (ctx.bot.has_aura(BATTLE_STANCE)) return false;
    if (!ctx.bot.has_aura(DEFENSIVE_STANCE)) return true;
    return !ctx.bot.in_combat() || ctx.bot.hp_pct() >= 70;
}
void DoBattleStance(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BATTLE_STANCE); }

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

// Piercing Howl: 8s AoE snare on a 90s CD. PvP-only - in PvE a mass snare
// on melee-range mobs is a wasted GCD, in BGs it peels a whole pack off a
// carrier / kites a chase.
bool ShouldPiercingHowl(ApPredicateContext const& ctx)
{
    if (!ctx.pvp.in_battleground && !ctx.pvp.in_arena) return false;
    if (!ctx.bot.knows_spell(PIERCING_HOWL)) return false;
    if (!ctx.bot.is_ready(PIERCING_HOWL)) return false;
    return ctx.bot.enemies_within(10.0f) >= 2;
}
void DoPiercingHowl(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(PIERCING_HOWL); }

// Hamstring fires either on the bot's current victim OR - in BGs where we
// have a friendly flag carrier - on an enemy meleeing the carrier (the FC
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
    // Whirlwind / Sweeping Strikes). FC-peel ignores that - peeling for the
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

// Colossus Smash is an 8y ground AoE in 12.1 (no target needed); fire on
// cooldown when we are in melee so the debuff lands on the victim.
bool ShouldColossusSmash(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(COLOSSUS_SMASH)) return false;
    if (!ctx.bot.is_ready(COLOSSUS_SMASH)) return false;
    return ctx.bot.enemies_within(8.0f) >= 1;
}
void DoColossusSmash(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(COLOSSUS_SMASH); }

// Demolish (Colossus hero tree): 2s channel, biggest single hit. Prefer to
// land it inside the Colossus Smash window; the CS rule sits above this one
// so when CS is ready it fires first and Demolish follows the next tick.
// Do not hold it forever - if CS is on cooldown, fire anyway.
bool ShouldDemolish(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEMOLISH)) return false;
    if (!ctx.bot.is_ready(DEMOLISH)) return false;
    if (ctx.bot.enemies_within(5.0f) == 0) return false;
    if (ctx.bot.has_aura(COLOSSUS_SMASH_DEBUFF, ctx.bot.victim())) return true;
    return !ctx.bot.knows_spell(COLOSSUS_SMASH) || !ctx.bot.is_ready(COLOSSUS_SMASH);
}
void DoDemolish(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DEMOLISH, ctx.bot.victim());
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
    // expires - gating Rend on DW caused Rend to drop ~30-40% of its
    // intended uptime. In 12.1 the cast is 772 but the bleed it applies is
    // still aura 388539; track that at the 30% pandemic window (~4.5s of a
    // 15s bleed).
    AuraEntry const* a = ctx.bot.find_aura(REND_BLEED_AURA, ctx.bot.victim());
    return !a || a->remaining.count() <= 4500;
}
void DoRend(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(REND, ctx.bot.victim());
}

// Wrecking Throw: 25y, 45s CD, ignores armor and shreds absorbs. Low in the
// ladder so it only fills a GCD nothing better wants (also our only ranged
// hit while closing on a target).
bool ShouldWreckingThrow(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(WRECKING_THROW)) return false;
    return ctx.bot.is_ready(WRECKING_THROW);
}
void DoWreckingThrow(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(WRECKING_THROW, ctx.bot.victim());
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

// Rule table - order is meaningful. See the file header comment for the
// canonical ladder; the comments below restate each tier for quick scanning.
ApRule const kRules[] = {
    // 1) Anti-CC immunity (must clear fear/sap before anything else can fire)
    { ShouldBerserkerRage,     DoBerserkerRage,     "Berserker Rage (anti-fear)"  },
    // 2) Spec defensive ladder
    { ShouldDieByTheSword,     DoDieByTheSword,     "Die by the Sword (<=35%)"    },
    { ShouldDefensiveStance,   DoDefensiveStance,   "Defensive Stance (<=30%)"    },
    // 3) Interrupts
    { ShouldPummel,            DoPummel,            "Pummel (interrupt)"          },
    { ShouldStormBolt,         DoStormBolt,         "Storm Bolt (interrupt fb)"   },
    // 4) Magic absorb
    { ShouldSpellReflection,   DoSpellReflection,   "Spell Reflection"            },
    // 5) Slow / CC
    { ShouldShockwave,         DoShockwave,         "Shockwave (3+ AoE stun)"     },
    { ShouldIntimidatingShout, DoIntimidatingShout, "Intimidating Shout (panic)"  },
    { ShouldPiercingHowl,      DoPiercingHowl,      "Piercing Howl (PvP snare)"   },
    { ShouldHamstring,         DoHamstring,         "Hamstring (slow)"            },
    // 6) Maintenance / group utility
    { ShouldBattleStance,      DoBattleStance,      "Battle Stance"               },
    { ShouldBattleShout,       DoBattleShout,       "Battle Shout (group buff)"   },
    { ShouldRallyingCry,       DoRallyingCry,       "Rallying Cry"                },
    { ShouldIntervene,         DoIntervene,         "Intervene"                   },
    // 7) Self-heal / shield (cheap, GCD-locked, fire before CDs)
    { ShouldIgnorePain,        DoIgnorePain,        "Ignore Pain"                 },
    { ShouldVictoryRush,       DoVictoryRush,       "Victory Rush"                },
    { ShouldImpendingVictory,  DoImpendingVictory,  "Impending Victory"           },
    // 8) Major offensive cooldowns (boss / multi-target gated)
    { ShouldAvatar,            DoAvatar,            "Avatar"                      },
    { ShouldSweepingStrikes,   DoSweepingStrikes,   "Sweeping Strikes (2+)"       },
    { ShouldBladestormAoe,     DoBladestorm,        "Bladestorm (3+ AoE)"         },
    { ShouldColossusSmash,     DoColossusSmash,     "Colossus Smash"              },
    { ShouldDemolish,          DoDemolish,          "Demolish (CS window)"        },
    // 9) Rotation (signature MS-window first, then proc spends, then fillers)
    { ShouldExecute,           DoExecute,           "Execute (window)"            },
    { ShouldMortalStrike,      DoMortalStrike,      "Mortal Strike (signature)"   },
    { ShouldOverpower,         DoOverpower,         "Overpower (proc)"            },
    { ShouldRend,              DoRend,              "Rend (refresh bleed)"        },
    { ShouldCleave,            DoCleave,            "Cleave (3+ AoE)"             },
    { ShouldWreckingThrow,     DoWreckingThrow,     "Wrecking Throw"              },
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
