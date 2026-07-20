// Protection Warrior - WoW 12.0 enterprise rotation. Shield-and-board tank
// with Shield Block active mitigation, Ignore Pain rage absorb, and the
// classic Shield Slam / Thunder Clap / Revenge rotation. Survival ladder:
// Shield Wall (40% DR) -> Last Stand (40% HP buff) -> Spell Reflection
// (magic absorb) -> Spell Block (talent, magic immunity) -> Victory Rush /
// Impending Victory (heal). Group utility: Battle Shout, Rallying Cry,
// Intervene (peel), Demoralizing Shout (damage debuff). CC: Pummel,
// Storm Bolt, Shockwave AoE stun, Intimidating Shout fear. Ranged engage:
// Heroic Throw (pull), Intercept (gap close + ally peel),
// Champion's Spear (talent ground AoE).
//
// Rule order (tank panic-first ladder, then threat/mitigation, then offense):
//   1) Anti-CC immunity     — Berserker Rage (fear/sap/incap break)
//   2) Panic CDs            — Shield Wall (<=25%), Last Stand (<=40%)
//   3) Interrupts           — Pummel, Storm Bolt fallback
//   4) Magic absorb         — Spell Reflection, Spell Block (talent)
//   5) Threat / peel        — Taunt (mob on healer), Challenging Shout (4+),
//                              Intercept (peel ally)
//   6) Active mitigation    — Shield Block (rage), Ignore Pain (rage absorb),
//                              Demoralizing Shout (incoming reduction)
//   7) Self heals           — Victory Rush / Impending Victory
//   8) Slow / CC            — Shockwave, Intimidating Shout, Hamstring(see baseline)
//   9) Group utility        — Battle Shout, Rallying Cry, Intervene
//  10) Major CDs            — Avatar, Ravager, Champion's Spear, Shield Charge
//  11) Rotation             — Execute (<=20%), Shield Slam, Thunder Clap,
//                              Revenge, Heroic Throw, Devastate
//  12) Auto-attack          — engage fallback
//
// Validated spell IDs (WoW 12.0):
//   23922 Shield Slam        | 6343 Thunder Clap     | 6572 Revenge
//   20243 Devastate          | 163201 Execute (Prot) | 2565 Shield Block (CASTABLE)
//   190456 Ignore Pain       | 12975 Last Stand      | 871  Shield Wall
//   107574 Avatar            | 385952 Shield Charge  | 228920 Ravager
//   1160  Demoralizing Shout | 46968 Shockwave       | 107570 Storm Bolt
//   5246  Intimidating Shout | 18499 Berserker Rage  | 23920 Spell Reflection
//   6552  Pummel             | 355   Taunt           | 1161  Challenging Shout
//   3411  Intervene          | 6673  Battle Shout    | 97462 Rallying Cry
//   202168 Impending Victory | 34428 Victory Rush    | 57755 Heroic Throw
//   198304 Intercept         | 392966 Spell Block    | 376079 Champion's Spear
//
// Skipped spells (and why):
//   71     Vanguard               — passive shield-wearing stamina aura
//   231847 Shield Block (passive) — Prot SPEC-grant aura adding rage on block;
//                                   distinct from the castable 2565 we use here
//   397708 Improved Execute       — passive rage-cost reduction (no GCD)
//   5301   Revenge!                — passive proc that makes Revenge free /
//                                   instant; we use the base Revenge cast (6572)
//   86535  Plate Specialization   — passive stat aura
//   76857  Mastery: Critical Block— passive mastery aura
//   137048 Protection Warrior     — spec aura container (passive)
//   462119 Protection Warrior     — modern aura container (passive)
//   162702 Stat Negation Aura     — internal stat-balance aura
//   161798 Riposte                — passive parry-to-crit conversion
//   401150 Avatar (Prot variant)  — NOT A VALID SPELL ID in 12.0 DB2 (previously
//                                   referenced — removed; we use the shared 107574)
//   100    Charge                 — handled in baseline rotation

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated against SpellName.csv) ----
constexpr uint32 SHIELD_SLAM         = 23922;
constexpr uint32 THUNDER_CLAP        = 6343;
constexpr uint32 REVENGE             = 6572;
constexpr uint32 DEVASTATE           = 20243;
constexpr uint32 EXECUTE_PROT        = 163201;
// Castable Shield Block (the active mitigation we manage). Distinct from the
// Prot spec passive Shield Block (231847) which is auto-granted and only adds
// rage gain on block — it doesn't replace the castable.
constexpr uint32 SHIELD_BLOCK        = 2565;
constexpr uint32 IGNORE_PAIN         = 190456;
constexpr uint32 LAST_STAND          = 12975;
constexpr uint32 SHIELD_WALL         = 871;
// Avatar uses the shared 107574 id. The previous value 401150 was bogus —
// no row in SpellName.csv 12.0 — so the rule never fired.
constexpr uint32 AVATAR              = 107574;
constexpr uint32 SHIELD_CHARGE       = 385952;       // talent — gap close + dmg
constexpr uint32 RAVAGER             = 228920;       // talent ground AoE
constexpr uint32 DEMORALIZING_SHOUT  = 1160;         // damage-dealt debuff
constexpr uint32 SHOCKWAVE           = 46968;
constexpr uint32 STORM_BOLT          = 107570;
constexpr uint32 INTIMIDATING_SHOUT  = 5246;
constexpr uint32 BERSERKER_RAGE      = 18499;
constexpr uint32 SPELL_REFLECTION    = 23920;
constexpr uint32 SPELL_BLOCK         = 392966;       // talent — full spell immunity 4s
constexpr uint32 PUMMEL              = 6552;
constexpr uint32 TAUNT               = 355;
constexpr uint32 CHALLENGING_SHOUT   = 1161;
constexpr uint32 INTERVENE           = 3411;
constexpr uint32 INTERCEPT           = 198304;       // gap close + ally damage transfer
constexpr uint32 HEROIC_THROW        = 57755;        // ranged pull (~30y)
constexpr uint32 CHAMPIONS_SPEAR     = 376079;       // talent ground AoE root
constexpr uint32 BATTLE_SHOUT        = 6673;
constexpr uint32 RALLYING_CRY        = 97462;
constexpr uint32 IMPENDING_VICTORY   = 202168;
constexpr uint32 VICTORY_RUSH        = 34428;

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

// ---- Anti-CC immunity ----
bool ShouldBerserkerRage(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BERSERKER_RAGE)) return false;
    if (!ctx.bot.is_ready(BERSERKER_RAGE)) return false;
    // MECHANIC_FEAR=5, MECHANIC_DISORIENTED=2, MECHANIC_HORROR=24,
    // MECHANIC_SAPPED=30. 6s immunity window.
    return ctx.bot.has_mechanic(5)  || ctx.bot.has_mechanic(2) ||
           ctx.bot.has_mechanic(24) || ctx.bot.has_mechanic(30);
}
void DoBerserkerRage(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BERSERKER_RAGE); }

// ---- Panic CDs ----
// PRE-EMPTIVE-ON-PILE-ON (2026-06-27). A large simultaneous melee pile-on
// (e.g. the Deadmines harbor deck: ~24 Defias Pirates LEAP-CLEAVE on aggro,
// taking the L30 tank 87%->dead in ~15s) blows past the old purely-reactive
// HP gates (Shield Wall <=25%, Last Stand <=40%) before the cooldown can
// land. So: pop Last Stand the instant a heavy pile engages while HP is still
// high, and RESERVE Shield Wall as the staggered follow-up (with a hard <=20%
// floor that can never be withheld into a death). Keyed on
// fightable_attackers_count() (stalker-free) so the untargetable 49521
// Lightning Stalker flood can't trivially trip these every tick. K=4
// attackers; the HP co-gate keeps it from burning on trivial pulls.
constexpr size_t kPileOnAttackers = 4;

bool ShouldShieldWall(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SHIELD_WALL)) return false;
    if (!ctx.bot.is_ready(SHIELD_WALL)) return false;
    // Unconditional death-floor: never withhold Shield Wall into a death.
    if (ctx.bot.hp_pct() <= 20) return true;
    // Pre-emptive (2026-06-27): with Last Stand UNAVAILABLE at L30 (live diag
    // cd=LS0 — the pre-emptive pile-on lever previously lived ONLY in
    // ShouldLastStand, line 188, which never runs), Shield Wall is the only major
    // DR the tank still has. Fire it pre-emptively when a HEAVY pile engages while
    // HP is still high so the 40% DR covers the burst window instead of reacting at
    // 35% (one heal-tick from death vs the ~930 HP/s harbor pull). Key on
    // enemies_within (NOT only fightable_attackers_count): at the Deadmines harbor
    // the killing ring is untargetable 49521 stalkers so fightable reads ~0-2,
    // while the real leap-cleave Defias register as raw nearby hostiles. The high
    // threshold (6) + in_combat gate keep it off trivial pulls.
    if ((ctx.bot.enemies_within(10.0f) >= 6 ||
         ctx.bot.fightable_attackers_count() >= kPileOnAttackers) &&
        ctx.bot.hp_pct() <= 80)
        return true;
    // Reactive panic raised 25 -> 35 so it lands before a fast burst kills.
    if (ctx.bot.hp_pct() <= 35)
    {
        // Stagger behind Last Stand: if Last Stand is already up for this same
        // pile-on and HP is still in the 20-35% band, hold a beat so a single
        // burst doesn't waste both DRs at once (the <=20% floor still catches
        // a continued free-fall).
        if (ctx.bot.has_aura(LAST_STAND) &&
            ctx.bot.fightable_attackers_count() >= kPileOnAttackers)
            return false;
        return true;
    }
    return false;
}
void DoShieldWall(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SHIELD_WALL); }

bool ShouldLastStand(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(LAST_STAND)) return false;
    if (!ctx.bot.is_ready(LAST_STAND)) return false;
    // Pre-emptive: pop the +30% max-HP/EHP buff when a heavy pile engages while
    // HP is still high (covers the whole burst window instead of the last ~2s),
    // staggered ahead of Shield Wall (don't fire if Shield Wall is already up).
    if (!ctx.bot.has_aura(SHIELD_WALL) &&
        ctx.bot.fightable_attackers_count() >= kPileOnAttackers &&
        ctx.bot.hp_pct() <= 75)
        return true;
    return ctx.bot.hp_pct() <= 40;
}
void DoLastStand(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(LAST_STAND); }

// ---- Magic absorb ----
bool ShouldSpellReflection(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SPELL_REFLECTION)) return false;
    if (!ctx.bot.is_ready(SPELL_REFLECTION)) return false;
    return ctx.bot.interruptible_caster() != nullptr;
}
void DoSpellReflection(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SPELL_REFLECTION); }

// Spell Block: talent — 4s of spell immunity. Cheaper CD than Spell
// Reflection, fire when we're being cast on but Spell Reflection is down
// (or talent-only). Don't double-stack.
bool ShouldSpellBlock(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SPELL_BLOCK)) return false;
    if (!ctx.bot.is_ready(SPELL_BLOCK)) return false;
    if (ctx.bot.has_aura(SPELL_REFLECTION)) return false;
    return ctx.bot.interruptible_caster() != nullptr;
}
void DoSpellBlock(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SPELL_BLOCK); }

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

bool ShouldChallengingShout(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(CHALLENGING_SHOUT)) return false;
    if (!ctx.bot.is_ready(CHALLENGING_SHOUT)) return false;
    int n = 0;
    for (auto const& u : ctx.bot.raw().combat.nearby_enemies)
        if (u.hp > 0) ++n;
    return n >= 4;
}
void DoChallengingShout(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CHALLENGING_SHOUT); }

// Intercept: short-CD gap close that ALSO redirects damage off an ally
// for 10s. Use it as a peel — a tank's Intercept onto a low ally is a
// mini-Intervene with a damage component. We let the existing Intervene
// rule (group helper) handle the low-HP case and reserve Intercept for
// situations where we're out of melee with our current victim (gap close
// to the fight).
bool ShouldIntercept(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(INTERCEPT)) return false;
    if (!ctx.bot.is_ready(INTERCEPT)) return false;
    return ctx.bot.enemies_within(8.0f) == 0;
}
void DoIntercept(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(INTERCEPT, ctx.bot.victim());
}

// ---- Active mitigation ----
bool ShouldShieldBlock(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SHIELD_BLOCK)) return false;
    if (!ctx.bot.is_ready(SHIELD_BLOCK)) return false;
    // Rage gate lowered 30 -> 20 (2026-06-27): on a fresh big pull the tank
    // enters combat rage-starved, so the old >=30 gate locked active mitigation
    // out for the first several seconds — exactly the burst window. Always-
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

// ---- Maintenance / group utility ----
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

bool ShouldShieldCharge(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHIELD_CHARGE)) return false;
    if (!ctx.bot.is_ready(SHIELD_CHARGE)) return false;
    return ctx.bot.enemies_within(8.0f) == 0;       // gap close when out of melee
}
void DoShieldCharge(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHIELD_CHARGE, ctx.bot.victim());
}

// ---- Damage / threat ----
// Heroic Throw: ranged pull (~30y). Fires only when we're out of melee
// range — otherwise Shield Slam / Devastate is strictly higher DPS.
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

bool ShouldDevastate(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
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

// Rule table — order is meaningful. See file header for the canonical
// tank ladder; comments below restate each tier for quick scanning.
ApRule const kRules[] = {
    // 1) Anti-CC immunity
    { ShouldBerserkerRage,     DoBerserkerRage,     "Berserker Rage (anti-fear)" },
    // 2) Panic CDs
    { ShouldShieldWall,        DoShieldWall,        "Shield Wall (<=25%)"        },
    { ShouldLastStand,         DoLastStand,         "Last Stand (<=40%)"         },
    // 3) Interrupts
    { ShouldPummel,            DoPummel,            "Pummel (interrupt)"         },
    { ShouldStormBolt,         DoStormBolt,         "Storm Bolt (interrupt fb)"  },
    // 4) Magic absorb
    { ShouldSpellReflection,   DoSpellReflection,   "Spell Reflection"           },
    { ShouldSpellBlock,        DoSpellBlock,        "Spell Block (talent)"       },
    // 5) Threat / peel
    { ShouldTaunt,             DoTaunt,             "Taunt (threat-grab)"        },
    { ShouldChallengingShout,  DoChallengingShout,  "Challenging Shout (4+)"     },
    { ShouldIntervene,         DoIntervene,         "Intervene (peel low ally)"  },
    { ShouldIntercept,         DoIntercept,         "Intercept (gap close)"      },
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
    // 9) Group utility
    { ShouldBattleShout,       DoBattleShout,       "Battle Shout (group buff)"  },
    { ShouldRallyingCry,       DoRallyingCry,       "Rallying Cry"               },
    // 10) Major offensive cooldowns
    { ShouldAvatar,            DoAvatar,            "Avatar"                     },
    { ShouldRavager,           DoRavager,           "Ravager"                    },
    { ShouldChampionsSpear,    DoChampionsSpear,    "Champion's Spear"           },
    { ShouldShieldCharge,      DoShieldCharge,      "Shield Charge (gap close)"  },
    // 11) Rotation — Execute window → Shield Slam (signature) → Thunder Clap (AoE/rage)
    //                → Revenge (proc) → Heroic Throw (ranged) → Devastate (filler)
    { ShouldExecute,           DoExecute,           "Execute (<=20%)"            },
    { ShouldShieldSlam,        DoShieldSlam,        "Shield Slam"                },
    { ShouldThunderClap,       DoThunderClap,       "Thunder Clap"               },
    { ShouldRevenge,           DoRevenge,           "Revenge (proc)"             },
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
