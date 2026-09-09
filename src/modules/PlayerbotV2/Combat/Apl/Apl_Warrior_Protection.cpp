// Protection Warrior - WoW 12.1.0.69587 (Midnight) enterprise rotation.
// Shield-and-board tank with Shield Block active mitigation, Ignore Pain rage
// absorb, and the Shield Slam / Thunder Clap / Revenge rotation with Demolish
// (Colossus hero tree) as the burst hit. Survival ladder: Shield Wall (40% DR;
// Last Stand is a passive rider on it in 12.1) -> Spell Reflection (magic
// absorb) -> Victory Rush / Impending Victory (heal). Defensive Stance kept
// up as the tank's default stance. Group utility: Battle Shout, Rallying Cry,
// Intervene (peel), Demoralizing Shout (damage debuff). CC: Pummel, Storm
// Bolt, Shockwave AoE stun, Intimidating Shout fear, Piercing Howl snare
// (PvP). Ranged engage: Heroic Throw (pull), Wrecking Throw, Shield Charge
// (gap close), Champion's Spear (talent ground AoE).
//
// Rule order (tank panic-first ladder, then threat/mitigation, then offense):
//   1) Anti-CC immunity     - Berserker Rage / Berserker Shout (fear/sap/incap break)
//   2) Panic CDs            - Shield Wall (<=35%, pre-emptive on pile-on)
//   3) Interrupts           - Pummel, Storm Bolt fallback
//   4) Magic absorb         - Spell Reflection
//   5) Threat / peel        - Taunt (mob on healer), Challenging / Disrupting Shout (4+),
//                              Intervene (peel ally)
//   6) Active mitigation    - Shield Block (rage), Ignore Pain (rage absorb),
//                              Demoralizing Shout (incoming reduction)
//   7) Self heals           - Victory Rush / Impending Victory
//   8) Slow / CC            - Shockwave, Intimidating Shout, Piercing Howl
//   9) Group utility        - Defensive Stance, Battle Shout, Rallying Cry
//  10) Major CDs            - Avatar, Ravager, Champion's Spear, Shield Charge, Demolish
//  11) Rotation             - Execute (window), Shield Slam, Thunder Clap,
//                              Revenge, Wrecking Throw, Heroic Throw, Devastate
//  12) Auto-attack          - engage fallback
//
// Validated spell IDs (WoW 12.1.0.69587):
//   23922 Shield Slam        | 6343 Thunder Clap     | 6572 Revenge
//   20243 Devastate          | 163201 Execute (Prot) | 2565 Shield Block (CASTABLE)
//   190456 Ignore Pain       | 871  Shield Wall      | 386208 Defensive Stance
//   107574 Avatar            | 385952 Shield Charge  | 228920 Ravager
//   436358 Demolish          | 384110 Wrecking Throw | 376079 Champion's Spear
//   1160  Demoralizing Shout | 46968 Shockwave       | 107570 Storm Bolt
//   5246  Intimidating Shout | 12323 Piercing Howl   | 18499 Berserker Rage
//   384100 Berserker Shout   | 23920 Spell Reflection| 6552  Pummel
//   355   Taunt              | 1161  Challenging Shout| 386071 Disrupting Shout
//   3411  Intervene          | 6673  Battle Shout    | 97462 Rallying Cry
//   202168 Impending Victory | 34428 Victory Rush    | 57755 Heroic Throw
//   Aura-only: 52437 Sudden Death
//   Passive gates: 236279 Devastator | 281001 Massacre
//
// Skipped spells (and why):
//   12975  Last Stand (cast)      - no longer castable; 12.1 Last Stand (1243659) is
//                                   a passive that rides on Shield Wall (max HP + heal)
//   392966 Spell Block            - removed from the Protection tree in 12.1
//   198304 Intercept              - removed in 12.1 (Intervene 3411 remains)
//   386164 Battle Stance          - DPS stance; the tank stays in Defensive Stance
//   6544   Heroic Leap            - ground-target positioning the bot does not do
//   231847 Shield Block (passive) - Prot SPEC-grant aura adding rage on block;
//                                   distinct from the castable 2565 we use here
//   5301   Revenge!                - passive proc that makes Revenge free /
//                                   instant; we use the base Revenge cast (6572)
//   137048 Protection Warrior     - spec aura container (passive)
//   1229376 Single-Button Assistant - client convenience macro, not a rotation ability
//   100    Charge                 - handled in baseline rotation

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated against SpellName.csv) ----
constexpr uint32 SHIELD_SLAM         = 23922;
constexpr uint32 THUNDER_CLAP        = 6343;
constexpr uint32 REVENGE             = 6572;
constexpr uint32 DEVASTATE           = 20243;        // replaced by the Devastator passive when taken
constexpr uint32 EXECUTE_PROT        = 163201;
// Castable Shield Block (the active mitigation we manage). Distinct from the
// Prot spec passive Shield Block (231847) which is auto-granted and only adds
// rage gain on block - it doesn't replace the castable.
constexpr uint32 SHIELD_BLOCK        = 2565;
constexpr uint32 IGNORE_PAIN         = 190456;
constexpr uint32 SHIELD_WALL         = 871;          // Last Stand (1243659) is a passive rider on this in 12.1
constexpr uint32 DEFENSIVE_STANCE    = 386208;       // talent - tank default stance
// Avatar uses the shared 107574 id. The previous value 401150 was bogus -
// no row in SpellName.csv 12.0 - so the rule never fired.
constexpr uint32 AVATAR              = 107574;
constexpr uint32 SHIELD_CHARGE       = 385952;       // talent - gap close + dmg + Shield Block
constexpr uint32 RAVAGER             = 228920;       // talent ground AoE
constexpr uint32 DEMOLISH            = 436358;       // Colossus hero tree - 2s channel burst
constexpr uint32 WRECKING_THROW      = 384110;       // talent - 25y armor-ignoring throw
constexpr uint32 DEMORALIZING_SHOUT  = 1160;         // damage-dealt debuff
constexpr uint32 SHOCKWAVE           = 46968;
constexpr uint32 STORM_BOLT          = 107570;
constexpr uint32 INTIMIDATING_SHOUT  = 5246;
constexpr uint32 PIERCING_HOWL       = 12323;        // talent AoE snare (PvP peel)
constexpr uint32 BERSERKER_RAGE      = 18499;
constexpr uint32 BERSERKER_SHOUT     = 384100;       // talent - overrides Berserker Rage
constexpr uint32 SPELL_REFLECTION    = 23920;
constexpr uint32 PUMMEL              = 6552;
constexpr uint32 TAUNT               = 355;
constexpr uint32 CHALLENGING_SHOUT   = 1161;
constexpr uint32 DISRUPTING_SHOUT    = 386071;       // talent - overrides Challenging Shout (+AoE interrupt)
constexpr uint32 INTERVENE           = 3411;
constexpr uint32 HEROIC_THROW        = 57755;        // ranged pull (~30y)
constexpr uint32 CHAMPIONS_SPEAR     = 376079;       // talent ground AoE root
constexpr uint32 BATTLE_SHOUT        = 6673;
constexpr uint32 RALLYING_CRY        = 97462;
constexpr uint32 IMPENDING_VICTORY   = 202168;
constexpr uint32 VICTORY_RUSH        = 34428;
// Aura / passive ids (never cast; predicates only).
constexpr uint32 SUDDEN_DEATH_AURA   = 52437;        // proc: Execute usable at any HP
constexpr uint32 MASSACRE            = 281001;       // passive: Execute usable below 35%
constexpr uint32 DEVASTATOR          = 236279;       // passive: auto-attacks replace Devastate

constexpr uint8 POWER_RAGE_IDX = 1;

bool HasLiveTarget(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && !ctx.bot.victim().IsEmpty();
}

// Execute window: 20% baseline, 35% with the Massacre passive, any HP while
// a Sudden Death proc is up (Sudden Death is [R] in the curated Prot build).
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

// ---- Anti-CC immunity ----
bool ShouldBerserkerRage(ApPredicateContext const& ctx)
{
    const uint32 id = BerserkerRageId(ctx);
    if (!ctx.bot.knows_spell(id)) return false;
    if (!ctx.bot.is_ready(id)) return false;
    // MECHANIC_FEAR=5, MECHANIC_DISORIENTED=2, MECHANIC_HORROR=24,
    // MECHANIC_SAPPED=30. 6s immunity window.
    return ctx.bot.has_mechanic(5)  || ctx.bot.has_mechanic(2) ||
           ctx.bot.has_mechanic(24) || ctx.bot.has_mechanic(30);
}
void DoBerserkerRage(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(BerserkerRageId(ctx)); }

// ---- Panic CDs ----
// PRE-EMPTIVE-ON-PILE-ON (2026-06-27). A large simultaneous melee pile-on
// (e.g. the Deadmines harbor deck: ~24 Defias Pirates LEAP-CLEAVE on aggro,
// taking the L30 tank 87%->dead in ~15s) blows past a purely-reactive HP gate
// before the cooldown can land. So: pop Shield Wall the instant a heavy pile
// engages while HP is still high, with a hard <=20% floor that can never be
// withheld into a death. Keyed on fightable_attackers_count() (stalker-free)
// so the untargetable 49521 Lightning Stalker flood can't trivially trip it
// every tick. K=4 attackers; the HP co-gate keeps it from burning on trivial
// pulls. In 12.1 Last Stand is no longer a separate button - the passive
// (1243659) adds its max-HP boost + heal to every Shield Wall - so Shield Wall
// is the single major DR and carries both the pre-emptive and reactive gates.
constexpr size_t kPileOnAttackers = 4;

bool ShouldShieldWall(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SHIELD_WALL)) return false;
    if (!ctx.bot.is_ready(SHIELD_WALL)) return false;
    // Unconditional death-floor: never withhold Shield Wall into a death.
    if (ctx.bot.hp_pct() <= 20) return true;
    // Pre-emptive (2026-06-27): fire when a HEAVY pile engages while HP is still
    // high so the 40% DR covers the burst window instead of reacting at 35% (one
    // heal-tick from death vs the ~930 HP/s harbor pull). Key on enemies_within
    // (NOT only fightable_attackers_count): at the Deadmines harbor the killing
    // ring is untargetable 49521 stalkers so fightable reads ~0-2, while the real
    // leap-cleave Defias register as raw nearby hostiles. The high threshold (6)
    // + in_combat gate keep it off trivial pulls.
    if ((ctx.bot.enemies_within(10.0f) >= 6 ||
         ctx.bot.fightable_attackers_count() >= kPileOnAttackers) &&
        ctx.bot.hp_pct() <= 80)
        return true;
    // Reactive panic raised 25 -> 35 so it lands before a fast burst kills.
    return ctx.bot.hp_pct() <= 35;
}
void DoShieldWall(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SHIELD_WALL); }

// ---- Magic absorb ----
bool ShouldSpellReflection(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SPELL_REFLECTION)) return false;
    if (!ctx.bot.is_ready(SPELL_REFLECTION)) return false;
    return ctx.bot.interruptible_caster() != nullptr;
}
void DoSpellReflection(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SPELL_REFLECTION); }

// ---- Threat / peel ----
bool ShouldTaunt(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(TAUNT)) return false;
    if (!ctx.bot.is_ready(TAUNT)) return false;
    return ctx.bot.untaunted_enemy() != nullptr;
}
void DoTaunt(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.untaunted_enemy())
        e.cast(TAUNT, t->guid);
}

// Disrupting Shout (talent) overrides Challenging Shout in the spellbook and
// adds an AoE interrupt on top of the AoE taunt. Resolve which one is owned.
uint32 ChallengingShoutId(ApPredicateContext const& ctx)
{
    return ctx.bot.knows_spell(DISRUPTING_SHOUT) ? DISRUPTING_SHOUT : CHALLENGING_SHOUT;
}

bool ShouldChallengingShout(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    const uint32 id = ChallengingShoutId(ctx);
    if (!ctx.bot.knows_spell(id)) return false;
    if (!ctx.bot.is_ready(id)) return false;
    int n = 0;
    for (auto const& u : ctx.bot.raw().combat.nearby_enemies)
        if (u.hp > 0) ++n;
    return n >= 4;
}
void DoChallengingShout(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(ChallengingShoutId(ctx)); }

// ---- Active mitigation ----
bool ShouldShieldBlock(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SHIELD_BLOCK)) return false;
    if (!ctx.bot.is_ready(SHIELD_BLOCK)) return false;
    // Rage gate lowered 30 -> 20 (2026-06-27): on a fresh big pull the tank
    // enters combat rage-starved, so the old >=30 gate locked active mitigation
    // out for the first several seconds - exactly the burst window. Always-
    // valuable cooldown, so a slightly earlier fire is strictly better.
    if (Rage(ctx) < 20) return false;
    return !ctx.bot.has_aura(SHIELD_BLOCK);
}
void DoShieldBlock(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SHIELD_BLOCK); }

bool ShouldIgnorePain(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(IGNORE_PAIN)) return false;
    if (!ctx.bot.is_ready(IGNORE_PAIN)) return false;
    if (ctx.bot.has_aura(IGNORE_PAIN)) return false;
    // Under a heavy pile-on, accept the absorb at >=20 rage so it is usable in
    // the rage-starved opening seconds (otherwise the >=40 gate whiffs the
    // burst window); for normal pulls keep the standard >=40 gate.
    const int32 rage = Rage(ctx);
    // enemies_within co-signal (2026-06-27): at the stalker-dominated harbor the
    // pile-on fightable count reads ~0-2 (untargetable 49521 flood), so the
    // rage-starve relief branch never fired and Ignore Pain whiffed the burst.
    // Count raw nearby hostiles too so the >=20-rage early-absorb path triggers.
    if (ctx.bot.fightable_attackers_count() >= kPileOnAttackers ||
        ctx.bot.enemies_within(10.0f) >= 6)
    {
        if (rage < 20) return false;
    }
    else if (rage < 40)
        return false;
    return ctx.bot.hp_pct() < 90;
}
void DoIgnorePain(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(IGNORE_PAIN); }

bool ShouldDemoralizingShout(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DEMORALIZING_SHOUT)) return false;
    if (!ctx.bot.is_ready(DEMORALIZING_SHOUT)) return false;
    return ctx.bot.attackers_count() >= 1;
}
void DoDemoralizingShout(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DEMORALIZING_SHOUT); }

// ---- Self heal ----
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

// Piercing Howl: 8s AoE snare on a 90s CD. PvP-only - in PvE a tank wants
// mobs ON it, in BGs it peels a whole pack off a carrier / kites a chase.
bool ShouldPiercingHowl(ApPredicateContext const& ctx)
{
    if (!ctx.pvp.in_battleground && !ctx.pvp.in_arena) return false;
    if (!ctx.bot.knows_spell(PIERCING_HOWL)) return false;
    if (!ctx.bot.is_ready(PIERCING_HOWL)) return false;
    return ctx.bot.enemies_within(10.0f) >= 2;
}
void DoPiercingHowl(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(PIERCING_HOWL); }

// ---- Maintenance / group utility ----
// Defensive Stance is the tank default (flat DR). Keep it up; no stance
// dance into Battle Stance for damage - survivability first for a bot tank.
bool ShouldDefensiveStance(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(DEFENSIVE_STANCE)) return false;
    if (!ctx.bot.is_ready(DEFENSIVE_STANCE)) return false;
    return !ctx.bot.has_aura(DEFENSIVE_STANCE);
}
void DoDefensiveStance(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DEFENSIVE_STANCE); }

bool ShouldBattleShout(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BATTLE_SHOUT)) return false;
    return !ctx.bot.has_aura(BATTLE_SHOUT);
}
void DoBattleShout(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BATTLE_SHOUT); }

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

// ---- Major offensive cooldowns ----
bool ShouldAvatar(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(AVATAR)) return false;
    if (!ctx.bot.is_ready(AVATAR)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.attackers_count() >= 3;
}
void DoAvatar(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(AVATAR); }

bool ShouldRavager(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RAVAGER)) return false;
    if (!ctx.bot.is_ready(RAVAGER)) return false;
    return ctx.bot.enemies_within(10.0f) >= 2 || BossLikeTargetEngaged(ctx);
}
void DoRavager(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(RAVAGER, v->x, v->y, v->z);
    else
        e.cast(RAVAGER, ctx.bot.victim());
}

bool ShouldChampionsSpear(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CHAMPIONS_SPEAR)) return false;
    if (!ctx.bot.is_ready(CHAMPIONS_SPEAR)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(8.0f) >= 2;
}
void DoChampionsSpear(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(CHAMPIONS_SPEAR, v->x, v->y, v->z);
    else
        e.cast(CHAMPIONS_SPEAR, ctx.bot.victim());
}

// Shield Charge: gap close AND a big hit + Shield Block + 20 Rage in 12.1, so
// it is worth pressing on cooldown once engaged, not only when out of melee.
bool ShouldShieldCharge(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHIELD_CHARGE)) return false;
    return ctx.bot.is_ready(SHIELD_CHARGE);
}
void DoShieldCharge(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHIELD_CHARGE, ctx.bot.victim());
}

// Demolish (Colossus hero tree): 2s channel, the tank's biggest hit and a
// 5y AoE around the target. Fire when in melee and off cooldown.
bool ShouldDemolish(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEMOLISH)) return false;
    if (!ctx.bot.is_ready(DEMOLISH)) return false;
    return ctx.bot.enemies_within(5.0f) >= 1;
}
void DoDemolish(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DEMOLISH, ctx.bot.victim());
}

// ---- Damage / threat ----
// Wrecking Throw: 25y, 45s CD, ignores armor and shreds absorbs. Sits below
// the core rotation so it only fills a GCD nothing better wants, but above
// Heroic Throw so it is the preferred ranged hit while closing.
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

// Heroic Throw: ranged pull (~30y). Fires only when we're out of melee
// range - otherwise Shield Slam / Devastate is strictly higher DPS.
// Cheap CD (6s) so it's also a free off-GCD threat top-up in many cases,
// but we keep the out-of-melee gate so it doesn't usurp the actual
// rotation.
bool ShouldHeroicThrow(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HEROIC_THROW)) return false;
    if (!ctx.bot.is_ready(HEROIC_THROW)) return false;
    return ctx.bot.enemies_within(5.0f) == 0;
}
void DoHeroicThrow(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HEROIC_THROW, ctx.bot.victim());
}

bool ShouldShieldSlam(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHIELD_SLAM)) return false;
    return ctx.bot.is_ready(SHIELD_SLAM);
}
void DoShieldSlam(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHIELD_SLAM, ctx.bot.victim());
}

bool ShouldThunderClap(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(THUNDER_CLAP)) return false;
    return ctx.bot.is_ready(THUNDER_CLAP);
}
void DoThunderClap(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(THUNDER_CLAP); }

bool ShouldExecute(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(EXECUTE_PROT)) return false;
    if (!ctx.bot.is_ready(EXECUTE_PROT)) return false;
    return TargetExecuteRange(ctx);
}
void DoExecute(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(EXECUTE_PROT, ctx.bot.victim());
}

bool ShouldRevenge(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(REVENGE)) return false;
    return ctx.bot.is_ready(REVENGE);
}
void DoRevenge(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(REVENGE, ctx.bot.victim());
}

// Devastate filler. The Devastator passive ([R] in the curated build) folds
// Devastate into auto-attacks and removes the button, so skip when known.
bool ShouldDevastate(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(DEVASTATOR)) return false;
    return ctx.bot.knows_spell(DEVASTATE);
}
void DoDevastate(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DEVASTATE, ctx.bot.victim());
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

// Rule table - order is meaningful. See file header for the canonical
// tank ladder; comments below restate each tier for quick scanning.
ApRule const kRules[] = {
    // 1) Anti-CC immunity
    { ShouldBerserkerRage,     DoBerserkerRage,     "Berserker Rage (anti-fear)" },
    // 2) Panic CDs (Last Stand rides on Shield Wall as a passive in 12.1)
    { ShouldShieldWall,        DoShieldWall,        "Shield Wall (<=35%/pile)"   },
    // 3) Interrupts
    { ShouldPummel,            DoPummel,            "Pummel (interrupt)"         },
    { ShouldStormBolt,         DoStormBolt,         "Storm Bolt (interrupt fb)"  },
    // 4) Magic absorb
    { ShouldSpellReflection,   DoSpellReflection,   "Spell Reflection"           },
    // 5) Threat / peel
    { ShouldTaunt,             DoTaunt,             "Taunt (threat-grab)"        },
    { ShouldChallengingShout,  DoChallengingShout,  "Challenging Shout (4+)"     },
    { ShouldIntervene,         DoIntervene,         "Intervene (peel low ally)"  },
    // 6) Active mitigation
    { ShouldShieldBlock,       DoShieldBlock,       "Shield Block (mitigate)"    },
    { ShouldIgnorePain,        DoIgnorePain,        "Ignore Pain (rage)"         },
    { ShouldDemoralizingShout, DoDemoralizingShout, "Demoralizing Shout"         },
    // 7) Self heals
    { ShouldVictoryRush,       DoVictoryRush,       "Victory Rush"               },
    { ShouldImpendingVictory,  DoImpendingVictory,  "Impending Victory"          },
    // 8) Slow / CC
    { ShouldShockwave,         DoShockwave,         "Shockwave (3+ AoE stun)"    },
    { ShouldIntimidatingShout, DoIntimidatingShout, "Intimidating Shout (panic)" },
    { ShouldPiercingHowl,      DoPiercingHowl,      "Piercing Howl (PvP snare)"  },
    // 9) Group utility / stance
    { ShouldDefensiveStance,   DoDefensiveStance,   "Defensive Stance"           },
    { ShouldBattleShout,       DoBattleShout,       "Battle Shout (group buff)"  },
    { ShouldRallyingCry,       DoRallyingCry,       "Rallying Cry"               },
    // 10) Major offensive cooldowns
    { ShouldAvatar,            DoAvatar,            "Avatar"                     },
    { ShouldRavager,           DoRavager,           "Ravager"                    },
    { ShouldChampionsSpear,    DoChampionsSpear,    "Champion's Spear"           },
    { ShouldShieldCharge,      DoShieldCharge,      "Shield Charge"              },
    { ShouldDemolish,          DoDemolish,          "Demolish (burst)"           },
    // 11) Rotation - Execute window -> Shield Slam (signature) -> Thunder Clap (AoE/rage)
    //                -> Revenge (proc) -> Wrecking Throw -> Heroic Throw (ranged) -> Devastate
    { ShouldExecute,           DoExecute,           "Execute (window)"           },
    { ShouldShieldSlam,        DoShieldSlam,        "Shield Slam"                },
    { ShouldThunderClap,       DoThunderClap,       "Thunder Clap"               },
    { ShouldRevenge,           DoRevenge,           "Revenge (proc)"             },
    { ShouldWreckingThrow,     DoWreckingThrow,     "Wrecking Throw"             },
    { ShouldHeroicThrow,       DoHeroicThrow,       "Heroic Throw (ranged)"      },
    { ShouldDevastate,         DoDevastate,         "Devastate (filler)"         },
    // 12) Auto-attack fallback
    { AlwaysInCombat,          DoAutoAttack,        "Engage auto attack"         },
};

} // anonymous

void RegisterApl_Warrior_Protection()
{
    constexpr uint32 SPEC_WARRIOR_PROTECTION = 73;
    RegisterRotation(CLASS_WARRIOR, SPEC_WARRIOR_PROTECTION, ApRotation{kRules});
}

} // namespace Playerbot::Combat
