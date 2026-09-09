// Windwalker Monk - WoW 12.1.0.69587 (Midnight) enterprise rotation. Energy
// + Chi melee with Tiger Palm Chi gen, Rising Sun Kick / Fists of Fury /
// Blackout Kick spenders, Spinning Crane Kick AoE, Zenith burst window (the
// 12.1 replacement for Storm, Earth, and Fire / Serenity), Touch of Death
// execute, Whirling Dragon Punch (talent burst), Strike of the Windlord
// (talent - frontal cone), Invoke Xuen (Conduit of the Celestials hero pet).
//
// Survival: Touch of Karma (damage redirect), Fortifying Brew (DR + HP),
// Expel Harm (self heal, gone once Combat Wisdom makes it passive). Group
// utility: Tiger's Lust (root break), Ring of Peace (displacement). CC:
// Spear Hand Strike, Leg Sweep (PBAoE stun), Paralysis (incapacitate).
// Diffuse Magic is a passive rider on Fortifying Brew in 12.1 - no cast.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated against SpellName.csv) ----
// Validated spell IDs (WoW 12.1.0.69587):
//   100780 Tiger Palm              100784 Blackout Kick            107428 Rising Sun Kick
//   113656 Fists of Fury           1249625 Zenith                  152175 Whirling Dragon Punch
//   392983 Strike of the Windlord  101546 Spinning Crane Kick      123904 Invoke Xuen
//   322109 Touch of Death          122470 Touch of Karma           115203 Fortifying Brew (cast)
//   388917 Fortifying Brew (talent)322101 Expel Harm               116705 Spear Hand Strike
//   115078 Paralysis               119381 Leg Sweep                116844 Ring of Peace
//   116841 Tiger's Lust            101545 Flying Serpent Kick      1217413 Slicing Winds
//   Passive gates: 121817 Combat Wisdom (makes Expel Harm passive)
//
// Skipped (with reason):
//   388917 Fortifying Brew (talent) passive trait spell. TC learns the trait
//                                  SpellID but never its VisibleSpellID 115203,
//                                  so the rule gates on EITHER id, casts 115203.
//   1243287 Diffuse Magic          12.1 passive rider on Fortifying Brew.
//   137639 Storm, Earth, and Fire  removed from the 12.1 Windwalker tree
//                                  (Zenith is the burst window now).
//   152173 Serenity                no longer exists in 12.1 SpellName.
//   274909 Rising Mist             Mistweaver passive, never castable here.
//   109132 Roll / 115008 Chi Torpedo positioning tools the rotation does not use.
//   218164 Detox                   M+-only pick for Windwalker; not in the
//                                  default build (Poison/Disease only).
//   1261703 Tigereye Brew          passive stack generator consumed by Zenith.
//   457974 Jadefire Stomp          passive in 12.1 (fires off Fists of Fury).
//   1229376 Single-Button Assistant client convenience macro.
//
// Combo Strikes (mastery): WW must alternate melee abilities - same ability
// twice in a row loses the bonus. BotSnapshotView publishes last_cast_spell_id()
// (populated by IntentVisitor on Result::Ok). The Tiger Palm / Blackout Kick /
// Rising Sun Kick / Fists of Fury / Spinning Crane Kick predicates all gate on
// ComboStrikesAllows(ctx, SELF) below.
constexpr uint32 TIGER_PALM             = 100780;
constexpr uint32 BLACKOUT_KICK          = 100784;
constexpr uint32 RISING_SUN_KICK        = 107428;
constexpr uint32 FISTS_OF_FURY          = 113656;
constexpr uint32 ZENITH                 = 1249625;      // 12.1 burst window: -Chi costs, BoK CDR, resets RSK
constexpr uint32 SPEAR_HAND_STRIKE      = 116705;
constexpr uint32 SPINNING_CRANE_KICK    = 101546;
constexpr uint32 TOUCH_OF_DEATH         = 322109;
constexpr uint32 TOUCH_OF_KARMA         = 122470;
constexpr uint32 FORTIFYING_BREW        = 115203;       // cast id (VisibleSpellID of the talent)
constexpr uint32 FORTIFYING_BREW_TALENT = 388917;       // learned trait spell - knows_spell gate
constexpr uint32 EXPEL_HARM             = 322101;
constexpr uint32 COMBAT_WISDOM          = 121817;       // passive [R][M]: Expel Harm becomes automatic
constexpr uint32 WHIRLING_DRAGON_PUNCH  = 152175;
constexpr uint32 STRIKE_OF_THE_WINDLORD = 392983;
constexpr uint32 INVOKE_XUEN            = 123904;       // Conduit of the Celestials hero active
constexpr uint32 PARALYSIS              = 115078;
constexpr uint32 LEG_SWEEP              = 119381;
constexpr uint32 RING_OF_PEACE          = 116844;
constexpr uint32 TIGERS_LUST            = 116841;
constexpr uint32 FLYING_SERPENT_KICK    = 101545;
constexpr uint32 SLICING_WINDS          = 1217413;      // talent override of Flying Serpent Kick (2 Chi lunge)

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
// Fortifying Brew: TC learns the trait spell 388917, never its VisibleSpellID
// 115203 (the actual cast). Accept either id as proof the talent is known.
bool ShouldFortifyingBrew(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(FORTIFYING_BREW_TALENT) && !ctx.bot.knows_spell(FORTIFYING_BREW)) return false;
    if (!ctx.bot.is_ready(FORTIFYING_BREW)) return false;
    return ctx.bot.hp_pct() <= 30;
}
void DoFortifyingBrew(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FORTIFYING_BREW); }

bool ShouldTigersLust(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(TIGERS_LUST)) return false;
    if (!ctx.bot.is_ready(TIGERS_LUST)) return false;
    return ctx.bot.has_mechanic(MECHANIC_ROOT) || ctx.bot.has_mechanic(MECHANIC_SNARE);
}
void DoTigersLust(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(TIGERS_LUST, ctx.bot.raw().guid);
}

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

bool ShouldExpelHarm(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(EXPEL_HARM)) return false;
    // Combat Wisdom (default build) turns Expel Harm into a passive Tiger
    // Palm rider and removes the button.
    if (ctx.bot.knows_spell(COMBAT_WISDOM)) return false;
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

bool ShouldParalysisOffTarget(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(PARALYSIS)) return false;
    if (!ctx.bot.is_ready(PARALYSIS)) return false;
    // CC a non-target caster (e.g. healer add) when the kick is unavailable.
    auto const* c = ctx.bot.interruptible_caster();
    if (!c || c->guid == ctx.bot.victim()) return false;
    return !ctx.bot.is_ready(SPEAR_HAND_STRIKE);
}
void DoParalysisOffTarget(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* c = ctx.bot.interruptible_caster())
        e.cast(PARALYSIS, c->guid);
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

bool ShouldZenith(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ZENITH)) return false;
    if (!ctx.bot.is_ready(ZENITH)) return false;
    // 16s CD / 15s window: keep it rolling whenever it is down - it resets
    // Rising Sun Kick and discounts every Chi spender for the duration.
    return !ctx.bot.has_aura(ZENITH);
}
void DoZenith(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ZENITH); }

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

// Slicing Winds (talent) overrides Flying Serpent Kick: same gap-closer
// slot, but it costs 2 Chi and deals damage along the lunge.
bool ShouldSlicingWinds(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SLICING_WINDS)) return false;
    if (!ctx.bot.is_ready(SLICING_WINDS)) return false;
    if (Chi(ctx) < 2) return false;
    return ctx.bot.enemies_within(8.0f) == 0;
}
void DoSlicingWinds(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SLICING_WINDS, ctx.bot.victim());
}

bool ShouldFlyingSerpentKick(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(SLICING_WINDS)) return false;
    if (!ctx.bot.knows_spell(FLYING_SERPENT_KICK)) return false;
    if (!ctx.bot.is_ready(FLYING_SERPENT_KICK)) return false;
    return ctx.bot.enemies_within(8.0f) == 0;
}
void DoFlyingSerpentKick(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FLYING_SERPENT_KICK, ctx.bot.victim());
}

// ---- Combo Strikes mastery helper ----
// Windwalker Mastery: Combo Strikes -using the same melee ability
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
    // Tiger Palm is the no-cost generator filler -the rotation often
    // alternates Tiger Palm <-> Blackout Kick / RSK. Combo Strikes still
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

// Canonical Windwalker priority order (12.1):
//   Survival: Touch of Karma (DR + damage redirect) -> Fortifying Brew ->
//             Expel Harm (non-Combat-Wisdom builds)
//   Interrupts / CC: Spear Hand Strike -> Leg Sweep -> Ring of Peace ->
//             Tiger's Lust (self root break)
//   Burst CDs: Invoke Xuen (boss) -> Zenith (rolling burst window)
//   Execute: Touch of Death
//   Gap / movement: Slicing Winds or Flying Serpent Kick (override pair)
//   High-priority damage: Whirling Dragon Punch -> Fists of Fury ->
//                         Strike of the Windlord -> Rising Sun Kick
//   AoE: Spinning Crane Kick (3+) -> Blackout Kick (combo-strikes aware) ->
//        Tiger Palm (combo-strikes aware filler / Chi gen)
//   Fallback: auto-attack
ApRule const kRules[] = {
    { ShouldTouchOfKarma,       DoTouchOfKarma,       "Touch of Karma (<=50%)"        },
    { ShouldFortifyingBrew,     DoFortifyingBrew,     "Fortifying Brew (<=30%)"       },
    { ShouldExpelHarm,          DoExpelHarm,          "Expel Harm (<=60%)"            },
    { ShouldSpearHandStrike,    DoSpearHandStrike,    "Spear Hand Strike (interrupt)" },
    { ShouldParalysisOffTarget, DoParalysisOffTarget, "Paralysis (off-target caster)" },
    { ShouldLegSweep,           DoLegSweep,           "Leg Sweep (3+ AoE stun)"       },
    { ShouldRingOfPeace,        DoRingOfPeace,        "Ring of Peace (panic)"         },
    { ShouldTigersLust,         DoTigersLust,         "Tiger's Lust (root break)"     },
    { ShouldInvokeXuen,         DoInvokeXuen,         "Invoke Xuen"                   },
    { ShouldZenith,             DoZenith,             "Zenith (burst window)"         },
    { ShouldTouchOfDeath,       DoTouchOfDeath,       "Touch of Death (<=15%/HP-cap)" },
    { ShouldSlicingWinds,       DoSlicingWinds,       "Slicing Winds (gap)"           },
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
