// Windwalker Monk - WoW 12.0 enterprise rotation. Energy + Chi melee with
// Tiger Palm Chi gen, Rising Sun Kick / Fists of Fury / Blackout Kick
// spenders, Spinning Crane Kick AoE, Storm/Earth/Fire burst phase, Touch
// of Death execute, Whirling Dragon Punch (talent burst), Strike of the
// Windlord (talent — frontal cone), Invoke Xuen (talent pet).
//
// Survival: Touch of Karma (damage redirect), Diffuse Magic, Fortifying
// Brew (DR + HP), Expel Harm (self heal). Group utility: Mystic Touch
// (passive 5% physical), Ring of Peace (silence/disarm). CC: Spear Hand
// Strike, Leg Sweep (PBAoE stun), Paralysis (incapacitate).

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated against SpellName.csv) ----
// Validated IDs:
//   100780 Tiger Palm              100784 Blackout Kick (generic)  107428 Rising Sun Kick
//   113656 Fists of Fury           137639 Storm, Earth, and Fire   152173 Serenity
//   152175 Whirling Dragon Punch   392983 Strike of the Windlord   101546 Spinning Crane Kick
//   123904 Invoke Xuen             322109 Touch of Death (generic) 122470 Touch of Karma
//   122783 Diffuse Magic           115203 Fortifying Brew          322101 Expel Harm
//   116705 Spear Hand Strike       115078 Paralysis                119381 Leg Sweep
//   116844 Ring of Peace           109132 Roll                     101545 Flying Serpent Kick
//
// Skipped (with reason):
//   261916 Blackout Kick (WW spec) WW-grant spell-variant id; the player
//                                  also has the generic 100784 in spellbook
//                                  via class spell, so generic suffices.
//   343730 Spinning Crane Kick     WW spec-variant id (same as above).
//   325215 Touch of Death          WW spec-variant id; generic 322109
//                                  routes correctly through the cast handler.
//   323999 Empowered Tiger Lightning passive — adds Xuen damage echo from
//                                  WW abilities; not castable.
//   343731 Disable                 short slow on a movement target — used
//                                  primarily in PvP kiting, not a rotation
//                                  primitive; left out of the auto rotation.
//   128595 Combat Conditioning     passive — Blackout Kick adds Mortal
//                                  Wounds equivalent; not castable.
//   116092 / 322719 Afterlife      passive proc — Healing Sphere drops
//                                  on kill; not castable.
//   274909 Rising Mist             MW talent passive, not WW.
//
// Combo Strikes (mastery): WW must alternate melee abilities — same ability
// twice in a row loses the bonus. BotSnapshotView publishes last_cast_spell_id()
// (populated by IntentVisitor on Result::Ok). The Tiger Palm / Blackout Kick /
// Rising Sun Kick / Fists of Fury / Spinning Crane Kick predicates all gate on
// ComboStrikesAllows(ctx, SELF) below — see the helper at line 234.
constexpr uint32 TIGER_PALM             = 100780;
constexpr uint32 BLACKOUT_KICK          = 100784;
constexpr uint32 RISING_SUN_KICK        = 107428;
constexpr uint32 FISTS_OF_FURY          = 113656;
constexpr uint32 STORM_EARTH_FIRE       = 137639;
constexpr uint32 SPEAR_HAND_STRIKE      = 116705;
constexpr uint32 SPINNING_CRANE_KICK    = 101546;
constexpr uint32 TOUCH_OF_DEATH         = 322109;
constexpr uint32 TOUCH_OF_KARMA         = 122470;
constexpr uint32 DIFFUSE_MAGIC          = 122783;
constexpr uint32 FORTIFYING_BREW        = 115203;
constexpr uint32 EXPEL_HARM             = 322101;
constexpr uint32 WHIRLING_DRAGON_PUNCH  = 152175;
constexpr uint32 STRIKE_OF_THE_WINDLORD = 392983;
constexpr uint32 INVOKE_XUEN            = 123904;
constexpr uint32 SERENITY               = 152173;       // talent burst (replaces SEF)
constexpr uint32 RISING_MIST            = 274909;
constexpr uint32 PARALYSIS              = 115078;
constexpr uint32 LEG_SWEEP              = 119381;
constexpr uint32 RING_OF_PEACE          = 116844;
constexpr uint32 ROLL                   = 109132;
constexpr uint32 FLYING_SERPENT_KICK    = 101545;

constexpr uint8 POWER_CHI_IDX    = 12;
constexpr uint8 POWER_ENERGY_IDX = 3;

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

// Touch of Death has two execute windows: target HP <= 15% OR
// target.max_hp <= bot.max_hp (the small-mob instant-kill cap). Both branches
// are valid against open-world quest mobs, dungeon trash and bosses.
bool TargetExecuteRange(ApPredicateContext const& ctx)
{
    NearbyUnit const* t = ctx.bot.victim_info();
    if (!t || t->max_hp <= 0 || t->hp <= 0) return false;
    const int32 hp_pct = static_cast<int32>((int64_t(t->hp) * 100) / t->max_hp);
    if (hp_pct <= 15) return true;
    if (t->max_hp <= ctx.bot.max_hp()) return true;
    return false;
}

int32 Chi(ApPredicateContext const& ctx) { return ctx.bot.power(POWER_CHI_IDX); }

// ---- Survival ----
bool ShouldFortifyingBrew(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(FORTIFYING_BREW)) return false;
    if (!ctx.bot.is_ready(FORTIFYING_BREW)) return false;
    return ctx.bot.hp_pct() <= 30;
}
void DoFortifyingBrew(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FORTIFYING_BREW); }

bool ShouldTouchOfKarma(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(TOUCH_OF_KARMA)) return false;
    if (!ctx.bot.is_ready(TOUCH_OF_KARMA)) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoTouchOfKarma(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(TOUCH_OF_KARMA, ctx.bot.victim());
}

bool ShouldDiffuseMagic(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DIFFUSE_MAGIC)) return false;
    if (!ctx.bot.is_ready(DIFFUSE_MAGIC)) return false;
    for (auto const& a : ctx.bot.raw().auras.own_auras)
        if (a.is_harmful && a.dispel_type == DispelType::Magic) return true;
    return false;
}
void DoDiffuseMagic(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DIFFUSE_MAGIC); }

bool ShouldExpelHarm(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(EXPEL_HARM)) return false;
    if (!ctx.bot.is_ready(EXPEL_HARM)) return false;
    return ctx.bot.hp_pct() <= 60;
}
void DoExpelHarm(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(EXPEL_HARM); }

// ---- Interrupt / CC ----
bool ShouldSpearHandStrike(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SPEAR_HAND_STRIKE)) return false;
    if (!ctx.bot.is_ready(SPEAR_HAND_STRIKE)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    return ctx.bot.kick_target(pvp, 5.0f) != nullptr;
}
void DoSpearHandStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 5.0f))
        e.cast(SPEAR_HAND_STRIKE, c->guid);
}

bool ShouldLegSweep(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(LEG_SWEEP)) return false;
    if (!ctx.bot.is_ready(LEG_SWEEP)) return false;
    return ctx.bot.enemies_within(5.0f) >= 3;
}
void DoLegSweep(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(LEG_SWEEP); }

bool ShouldRingOfPeace(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(RING_OF_PEACE)) return false;
    if (!ctx.bot.is_ready(RING_OF_PEACE)) return false;
    return ctx.bot.enemies_within(8.0f) >= 3 && ctx.bot.hp_pct() <= 50;
}
void DoRingOfPeace(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    float bx, by, bz;
    ctx.bot.position(bx, by, bz);
    e.cast_at(RING_OF_PEACE, bx, by, bz);
}

// ---- Major offensive cooldowns ----
bool ShouldInvokeXuen(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(INVOKE_XUEN)) return false;
    if (!ctx.bot.is_ready(INVOKE_XUEN)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoInvokeXuen(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(INVOKE_XUEN, ctx.bot.victim());
}

bool ShouldStormEarthFire(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(SERENITY)) return false;
    if (!ctx.bot.knows_spell(STORM_EARTH_FIRE)) return false;
    if (!ctx.bot.is_ready(STORM_EARTH_FIRE)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(10.0f) >= 2;
}
void DoStormEarthFire(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(STORM_EARTH_FIRE); }

bool ShouldSerenity(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SERENITY)) return false;
    if (!ctx.bot.is_ready(SERENITY)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoSerenity(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SERENITY); }

bool ShouldStrikeOfTheWindlord(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(STRIKE_OF_THE_WINDLORD)) return false;
    if (!ctx.bot.is_ready(STRIKE_OF_THE_WINDLORD)) return false;
    return true;
}
void DoStrikeOfTheWindlord(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(STRIKE_OF_THE_WINDLORD); }

bool ShouldWhirlingDragonPunch(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(WHIRLING_DRAGON_PUNCH)) return false;
    if (!ctx.bot.is_ready(WHIRLING_DRAGON_PUNCH)) return false;
    // Requires both Rising Sun Kick and Fists of Fury on cooldown.
    return !ctx.bot.is_ready(RISING_SUN_KICK) && !ctx.bot.is_ready(FISTS_OF_FURY);
}
void DoWhirlingDragonPunch(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(WHIRLING_DRAGON_PUNCH); }

bool ShouldTouchOfDeath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(TOUCH_OF_DEATH)) return false;
    if (!ctx.bot.is_ready(TOUCH_OF_DEATH)) return false;
    return TargetExecuteRange(ctx);
}
void DoTouchOfDeath(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(TOUCH_OF_DEATH, ctx.bot.victim());
}

bool ShouldFlyingSerpentKick(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FLYING_SERPENT_KICK)) return false;
    if (!ctx.bot.is_ready(FLYING_SERPENT_KICK)) return false;
    return ctx.bot.enemies_within(8.0f) == 0;
}
void DoFlyingSerpentKick(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FLYING_SERPENT_KICK, ctx.bot.victim());
}

// ---- Combo Strikes mastery helper ----
// Windwalker Mastery: Combo Strikes — using the same melee ability
// twice in a row loses the bonus damage. Each spender / generator
// gates on `last_cast != self` to enforce alternation. Snapshot
// publishes `last_cast_spell_id` (set by IntentVisitor on Result::Ok).
// Returns true if the rotation is allowed to fire `spell_id` from a
// Combo-Strikes standpoint.
bool ComboStrikesAllows(ApPredicateContext const& ctx, uint32 spell_id)
{
    return ctx.bot.last_cast_spell_id() != spell_id;
}

// ---- Damage rotation ----
bool ShouldFistsOfFury(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FISTS_OF_FURY)) return false;
    if (!ctx.bot.is_ready(FISTS_OF_FURY)) return false;
    if (!ComboStrikesAllows(ctx, FISTS_OF_FURY)) return false;
    return Chi(ctx) >= 3;
}
void DoFistsOfFury(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FISTS_OF_FURY); }

bool ShouldRisingSunKick(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RISING_SUN_KICK)) return false;
    if (!ctx.bot.is_ready(RISING_SUN_KICK)) return false;
    if (!ComboStrikesAllows(ctx, RISING_SUN_KICK)) return false;
    return Chi(ctx) >= 2;
}
void DoRisingSunKick(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RISING_SUN_KICK, ctx.bot.victim());
}

bool ShouldSpinningCraneKick(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SPINNING_CRANE_KICK)) return false;
    if (Chi(ctx) < 2) return false;
    if (!ComboStrikesAllows(ctx, SPINNING_CRANE_KICK)) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(8.0f) >= 3;
}
void DoSpinningCraneKick(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SPINNING_CRANE_KICK); }

bool ShouldBlackoutKick(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BLACKOUT_KICK)) return false;
    if (!ctx.bot.is_ready(BLACKOUT_KICK)) return false;
    if (!ComboStrikesAllows(ctx, BLACKOUT_KICK)) return false;
    return Chi(ctx) >= 1;
}
void DoBlackoutKick(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BLACKOUT_KICK, ctx.bot.victim());
}

bool ShouldTigerPalm(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(TIGER_PALM)) return false;
    // Tiger Palm is the no-cost generator filler — the rotation often
    // alternates Tiger Palm ↔ Blackout Kick / RSK. Combo Strikes still
    // applies: refuse Tiger Palm if it was the last ability cast.
    return ComboStrikesAllows(ctx, TIGER_PALM);
}
void DoTigerPalm(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(TIGER_PALM, ctx.bot.victim());
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

// Canonical Windwalker priority order:
//   Survival: Touch of Karma (DR + damage redirect) -> Fortifying Brew ->
//             Diffuse Magic -> Expel Harm
//   Interrupts / CC: Spear Hand Strike -> Leg Sweep -> Ring of Peace
//   Burst CDs: SEF or Serenity (mutually exclusive talent) -> Invoke Xuen
//   Execute: Touch of Death
//   Gap / movement: Flying Serpent Kick
//   High-priority damage: Whirling Dragon Punch -> Fists of Fury ->
//                         Strike of the Windlord -> Rising Sun Kick
//   AoE: Spinning Crane Kick (3+) -> Blackout Kick (combo-strikes aware) ->
//        Tiger Palm (combo-strikes aware filler / Chi gen)
//   Fallback: auto-attack
ApRule const kRules[] = {
    { ShouldTouchOfKarma,       DoTouchOfKarma,       "Touch of Karma (<=50%)"        },
    { ShouldFortifyingBrew,     DoFortifyingBrew,     "Fortifying Brew (<=30%)"       },
    { ShouldDiffuseMagic,       DoDiffuseMagic,       "Diffuse Magic"                 },
    { ShouldExpelHarm,          DoExpelHarm,          "Expel Harm (<=60%)"            },
    { ShouldSpearHandStrike,    DoSpearHandStrike,    "Spear Hand Strike (interrupt)" },
    { ShouldLegSweep,           DoLegSweep,           "Leg Sweep (3+ AoE stun)"       },
    { ShouldRingOfPeace,        DoRingOfPeace,        "Ring of Peace (panic)"         },
    { ShouldSerenity,           DoSerenity,           "Serenity"                      },
    { ShouldStormEarthFire,     DoStormEarthFire,     "Storm, Earth, and Fire"        },
    { ShouldInvokeXuen,         DoInvokeXuen,         "Invoke Xuen"                   },
    { ShouldTouchOfDeath,       DoTouchOfDeath,       "Touch of Death (<=15%/HP-cap)" },
    { ShouldFlyingSerpentKick,  DoFlyingSerpentKick,  "Flying Serpent Kick (gap)"     },
    { ShouldWhirlingDragonPunch,DoWhirlingDragonPunch,"Whirling Dragon Punch"         },
    { ShouldFistsOfFury,        DoFistsOfFury,        "Fists of Fury"                 },
    { ShouldStrikeOfTheWindlord,DoStrikeOfTheWindlord,"Strike of the Windlord"        },
    { ShouldRisingSunKick,      DoRisingSunKick,      "Rising Sun Kick"               },
    { ShouldSpinningCraneKick,  DoSpinningCraneKick,  "Spinning Crane Kick (3+ AoE)"  },
    { ShouldBlackoutKick,       DoBlackoutKick,       "Blackout Kick (combo strikes)" },
    { ShouldTigerPalm,          DoTigerPalm,          "Tiger Palm (combo strikes)"    },
    { AlwaysInCombat,           DoAutoAttack,         "Engage auto attack"            },
};

} // anonymous

void RegisterApl_Monk_Windwalker()
{
    constexpr uint32 SPEC_MONK_WINDWALKER = 269;
    RegisterRotation(CLASS_MONK, SPEC_MONK_WINDWALKER, ApRotation{kRules});
}

} // namespace Playerbot::Combat
