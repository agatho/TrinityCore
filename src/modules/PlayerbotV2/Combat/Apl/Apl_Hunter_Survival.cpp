// Survival Hunter - WoW 12.1.0.69587 (Midnight) enterprise rotation. Melee
// Hunter built around Kill Command (focus generator, Tip of the Spear),
// Raptor Strike (spender, Mongoose Fury stacks / Raptor Swipe cleave are
// PASSIVE upgrades in 12.1), Wildfire Bomb (charges, cone + DoT) and two
// new 12.1 buttons: Takedown (90s cd leap-and-strike burst, replaces
// Coordinated Assault / Spearhead) and Boomstick (60s cd shotgun cone,
// 50 focus, replaces Butchery / Carve as the AoE spender).
//
// Layered survival: Aspect of the Turtle (immunity) -> Exhilaration ->
// Survival of the Fittest -> Feign Death threat dump. Group utility:
// Misdirection (tank threat redirect), Primal Rage (pet-cast Bloodlust,
// Ferocity pets). Pet maintenance: Mend Pet, Revive Pet. CC: Muzzle
// (melee interrupt), Intimidation (pet stun, interrupt fallback), Binding
// Shot (root cluster), Tar Trap (slow). Engage: Harpoon (gap close, L14) /
// Hatchet Toss (ranged opener, L12; overrides Arcane Shot). Aspect of the
// Eagle extends Raptor Strike to 40y as a boss burst window.
//
// Validated spell IDs (WoW 12.1.0.69587, kit + SpellName.csv):
//   186270 Raptor Strike    | 259489 Kill Command (SV) | 259495 Wildfire Bomb
//   1250646 Takedown        | 1261193 Boomstick        | 193265 Hatchet Toss
//   190925 Harpoon          | 186289 Aspect of the Eagle| 187707 Muzzle
//   19577  Intimidation     | 109248 Binding Shot      | 187698 Tar Trap
//   34477  Misdirection     | 264735 Survival o.t.Fit. | 186265 Aspect of the Turtle
//   109304 Exhilaration     | 5384   Feign Death       | 136    Mend Pet
//   982    Revive Pet       | 257284 Hunter's Mark
//   264667 Primal Rage (PET ability, pet_cast)
//
// Skipped (deliberate, 12.1):
//   * Mongoose Bite    ( 259387) - gone; Mongoose Fury (1252708) is a passive
//                                  Raptor Strike self-buff now.
//   * Carve / Butchery (187708 / 212436) - gone; Boomstick + Raptor Swipe
//                                  passive (1259003) cover melee AoE.
//   * Serpent Sting    ( 259491) - removed from SV (no outbound DoT to cycle).
//   * Coordinated Assault / Spearhead (360952 / 360966) - gone; Takedown is
//                                  the 12.1 burst CD.
//   * Kill Shot        (  53351) - Marksmanship-only talent in 12.1.
//   * Counter Shot     ( 147362) - BM/MM only; SV interrupts with Muzzle.
//   * Bestial Wrath    (  19574) - BM-only talent.
//   * Aspect of the Wild (193530) / Death Chakram (375891) / Steel Trap
//                      ( 162488) - removed from the game / no SV learn path.
//   * Freezing Trap    ( 187650) / Aspect of the Cheetah (186257) / Wing
//                      Clip (195645) - out-of-combat or positional utility.
//   * Tranquilizing Shot (19801) / Camouflage (199483) - [M]-only class
//                      talents, dispel / stealth logic lives outside the DPS ladder.
//   * Eyes of the Beast ( 321297) - pet-vision toy, breaks bot AI control.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated against the 12.1 kit and
// SpellName.csv; see the header table) ----
constexpr uint32 RAPTOR_STRIKE        = 186270;       // spec talent [R][M] (L10), 30 focus, 5y
constexpr uint32 KILL_COMMAND_SV      = 259489;       // spec talent [R][M] (L11), 50y, GENERATES focus
constexpr uint32 WILDFIRE_BOMB        = 259495;       // spec talent [R][M] (L20), 10 focus, cone + DoT
constexpr uint32 TAKEDOWN             = 1250646;      // spec talent [R][M] - 90s cd, 15y leap-and-strike burst
constexpr uint32 BOOMSTICK            = 1261193;      // spec talent [R][M] - 60s cd, 50 focus, 20y shotgun cone
constexpr uint32 HATCHET_TOSS         = 193265;       // spec spell L12 ranged opener (40y), 30 focus; overrides Arcane Shot
constexpr uint32 HARPOON              = 190925;       // spec spell L14 gap closer (30y)
constexpr uint32 ASPECT_EAGLE         = 186289;       // spec spell L24 burst CD - Raptor Strike at 40y for 15s
constexpr uint32 INTIMIDATION         = 19577;        // class talent [R][M] - 5s stun via pet
constexpr uint32 BINDING_SHOT         = 109248;       // class talent [R] root cluster
constexpr uint32 MUZZLE               = 187707;       // class talent [R][M] melee interrupt
constexpr uint32 MISDIRECTION         = 34477;
constexpr uint32 ASPECT_TURTLE        = 186265;
constexpr uint32 EXHILARATION         = 109304;
constexpr uint32 SURVIVAL_FITTEST     = 264735;       // class talent [R][M] - DR self + pet
constexpr uint32 FEIGN_DEATH          = 5384;
constexpr uint32 MEND_PET             = 136;
constexpr uint32 REVIVE_PET           = 982;          // OOC + combat rez of pet
constexpr uint32 TAR_TRAP             = 187698;       // class talent [R][M]
constexpr uint32 HUNTERS_MARK         = 257284;
constexpr uint32 PRIMAL_RAGE          = 264667;       // PET ability (Ferocity, via Command Pet 272651) - pet_cast only

constexpr uint8 POWER_FOCUS_IDX = 2;

constexpr int32 RAPTOR_STRIKE_COST = 30;
constexpr int32 HATCHET_TOSS_COST  = 30;
constexpr int32 WILDFIRE_BOMB_COST = 10;
constexpr int32 BOOMSTICK_COST     = 50;

bool HasLiveTarget(ApPredicateContext const& ctx)
{
    return !ctx.bot.victim().IsEmpty();
}

int32 FocusVal(ApPredicateContext const& ctx) { return ctx.bot.power(POWER_FOCUS_IDX); }

bool BossLikeTargetEngaged(ApPredicateContext const& ctx)
{
    constexpr int32 kBossHpThreshold = 5'000'000;
    NearbyUnit const* t = ctx.bot.victim_info();
    if (t && t->max_hp >= kBossHpThreshold) return true;
    for (auto const& a : ctx.bot.raw().combat.attackers)
        if (a.max_hp >= kBossHpThreshold) return true;
    return false;
}

bool BotHasSatedDebuff(ApPredicateContext const& ctx)
{
    constexpr uint32 SATED_DEBUFF           = 57724;
    constexpr uint32 TEMPORAL_DISPL_DEBUFF  = 80354;
    constexpr uint32 INSANITY_HUNTER_DEBUFF = 95809;
    constexpr uint32 FATIGUED_DEBUFF        = 264689;
    return ctx.bot.has_aura(SATED_DEBUFF)
        || ctx.bot.has_aura(TEMPORAL_DISPL_DEBUFF)
        || ctx.bot.has_aura(INSANITY_HUNTER_DEBUFF)
        || ctx.bot.has_aura(FATIGUED_DEBUFF);
}

// ---- Pet maintenance ----
bool ShouldRevivePet(ApPredicateContext const& ctx)
{
    // Only resurrect when there's actually a dead pet to bring back.
    // pet_guid().IsEmpty() means the bot never tamed/summoned one; Revive
    // Pet would silently no-op and re-fire every tick, starving the rest
    // of the rotation. See log audit 2026-05-21 (22.8k spell-982 emits).
    if (ctx.bot.pet_guid().IsEmpty()) return false;
    if (ctx.bot.has_pet()) return false;
    if (!ctx.bot.knows_spell(REVIVE_PET)) return false;
    return ctx.bot.is_ready(REVIVE_PET);
}
void DoRevivePet(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(REVIVE_PET); }

bool ShouldMendPet(ApPredicateContext const& ctx)
{
    if (!ctx.bot.has_pet()) return false;
    if (!ctx.bot.knows_spell(MEND_PET)) return false;
    if (!ctx.bot.is_ready(MEND_PET)) return false;
    return ctx.bot.pet_hp_pct() <= 50;
}
void DoMendPet(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(MEND_PET); }

// ---- Survival ----
bool ShouldAspectTurtle(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ASPECT_TURTLE)) return false;
    if (!ctx.bot.is_ready(ASPECT_TURTLE)) return false;
    return ctx.bot.hp_pct() <= 20;
}
void DoAspectTurtle(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ASPECT_TURTLE); }

bool ShouldExhilaration(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(EXHILARATION)) return false;
    if (!ctx.bot.is_ready(EXHILARATION)) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoExhilaration(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(EXHILARATION); }

bool ShouldSurvivalFittest(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SURVIVAL_FITTEST)) return false;
    if (!ctx.bot.is_ready(SURVIVAL_FITTEST)) return false;
    return ctx.bot.hp_pct() <= 60;
}
void DoSurvivalFittest(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SURVIVAL_FITTEST); }

bool ShouldFeignDeath(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(FEIGN_DEATH)) return false;
    if (!ctx.bot.is_ready(FEIGN_DEATH)) return false;
    return ctx.bot.hp_pct() <= 30 && ctx.bot.attackers_count() >= 1;
}
void DoFeignDeath(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FEIGN_DEATH); }

// ---- Group utility ----
bool ShouldMisdirection(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(MISDIRECTION)) return false;
    if (!ctx.bot.is_ready(MISDIRECTION)) return false;
    auto const* tank = ctx.group.tank();
    return tank && tank->online && tank->guid != ctx.bot.raw().guid;
}
void DoMisdirection(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* tank = ctx.group.tank())
        e.cast(MISDIRECTION, tank->guid);
}

// Primal Rage is a PET ability (Ferocity family, surfaced to the hunter via
// Command Pet 272651), not a hunter spell - knows_spell()/is_ready() query
// the BOT's spellbook and always fail, and e.cast() would bounce off
// NOT_KNOWN. Route through pet_cast (API::pet_cast validates the pet knows
// it and checks the pet's own SpellHistory). Same pattern as the BM file.
bool ShouldPrimalRage(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.has_pet()) return false;
    if (!ctx.bot.pet_can_bloodlust()) return false;   // Ferocity pets only
    if (BotHasSatedDebuff(ctx)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoPrimalRage(ApPredicateContext const&, BotIntentEmitter& e) { e.pet_cast(PRIMAL_RAGE); }

// ---- Interrupt / CC ----
bool ShouldMuzzle(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(MUZZLE)) return false;
    if (!ctx.bot.is_ready(MUZZLE)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    return ctx.bot.kick_target(pvp, 5.0f) != nullptr;
}
void DoMuzzle(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 5.0f))
        e.cast(MUZZLE, c->guid);
}

// Intimidation (pet stun, 100y) doubles as the ranged interrupt fallback:
// Muzzle is melee-only (5y), so a caster that is out of reach - or casting
// while Muzzle is on cooldown - gets the pet stun instead.
bool ShouldIntimidation(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(INTIMIDATION)) return false;
    if (!ctx.bot.is_ready(INTIMIDATION)) return false;
    if (!ctx.bot.has_pet()) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (ctx.bot.is_ready(MUZZLE) && ctx.bot.kick_target(pvp, 5.0f) != nullptr) return false;
    return ctx.bot.kick_target(pvp, 40.0f) != nullptr;
}
void DoIntimidation(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 40.0f))
        e.cast(INTIMIDATION, c->guid);
}

bool ShouldBindingShot(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BINDING_SHOT)) return false;
    if (!ctx.bot.is_ready(BINDING_SHOT)) return false;
    return ctx.bot.enemies_within(15.0f) >= 3;
}
void DoBindingShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(BINDING_SHOT, v->x, v->y, v->z);
    else
        e.cast(BINDING_SHOT);
}

bool ShouldTarTrap(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(TAR_TRAP)) return false;
    if (!ctx.bot.is_ready(TAR_TRAP)) return false;
    return ctx.bot.enemies_within(20.0f) >= 2 && ctx.bot.hp_pct() <= 50;
}
void DoTarTrap(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(TAR_TRAP, v->x, v->y, v->z);
    else
        e.cast(TAR_TRAP);
}

// Compute squared-distance between bot and a NearbyUnit. NearbyUnit
// itself doesn't carry a pre-computed distance - match the cheap inline
// idiom used by enemies_within() in BotSnapshotView.cpp.
inline float TargetDistSq(ApPredicateContext const& ctx, NearbyUnit const& t)
{
    float bx, by, bz; ctx.bot.position(bx, by, bz);
    const float dx = t.x - bx, dy = t.y - by, dz = t.z - bz;
    return dx*dx + dy*dy + dz*dz;
}

// ---- Major offensive CDs ----
// Takedown (1250646, [R][M], 90s cd, 15y): "You and your pet leap to your
// target and strike as one ... For the next 8s the damage dealt by you
// [and your pet is increased]". It is SV's single burst CD in 12.1
// (Coordinated Assault / Spearhead are gone). Same leveling-aware gate as
// BM's Bestial Wrath: any fight that can absorb a 90s CD - boss-like,
// multi-pull, or a target meatier than the bot - not raid bosses only.
// The leap also closes a 15y gap, so it doubles as a mini gap-closer.
bool ShouldTakedown(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(TAKEDOWN)) return false;
    if (!ctx.bot.is_ready(TAKEDOWN)) return false;
    NearbyUnit const* t = ctx.bot.victim_info();
    if (!t) return false;
    if (TargetDistSq(ctx, *t) > 15.0f * 15.0f) return false;   // out of leap range
    if (BossLikeTargetEngaged(ctx)) return true;
    if (ctx.bot.attackers_count() >= 2 || ctx.bot.enemies_within(10.0f) >= 2) return true;
    return t->max_hp >= ctx.bot.max_hp();
}
void DoTakedown(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(TAKEDOWN, ctx.bot.victim());
}

// Aspect of the Eagle (186289, L24, 90s CD). Extends Raptor Strike (and
// Mastery: Spirit Bond) to 40y for 15s. Use as a burst window - fires on
// boss-like targets so the CD isn't blown on a 3-second trash pull.
// Self-cast (no target).
bool ShouldAspectEagle(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ASPECT_EAGLE)) return false;
    if (!ctx.bot.is_ready(ASPECT_EAGLE)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoAspectEagle(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ASPECT_EAGLE); }

// Harpoon (190925, L14, 30s CD). 8-30y leap that roots the target for 3s.
// SV is a melee spec, so Harpoon is the primary gap-closer when the bot
// is OOC-starting a pull or has been kited out. Gate on:
//   * an enemy that's actually FURTHER than melee (>8y) but within the
//     leap window (≤30y),
//   * no enemies already adjacent (otherwise we'd waste it pulling toward
//     someone next to us — leaping to a far target while glued in melee
//     pulls aggro on a new mob).
bool ShouldHarpoon(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HARPOON)) return false;
    if (!ctx.bot.is_ready(HARPOON)) return false;
    NearbyUnit const* t = ctx.bot.victim_info();
    if (!t) return false;
    const float d2 = TargetDistSq(ctx, *t);
    if (d2 <= 8.0f * 8.0f) return false;          // already in melee
    if (d2 >  30.0f * 30.0f) return false;         // out of leap range
    if (ctx.bot.enemies_within(5.0f) >= 1) return false; // someone on top of us
    return true;
}
void DoHarpoon(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HARPOON, ctx.bot.victim());
}

// Hatchet Toss (193265, L12, 30 focus, 40y; overrides Arcane Shot on the
// bar). The only damage cast SV has at range outside Aspect of the Eagle.
// Use when the bot is outside melee + Harpoon is on CD, or the target is
// too close for Harpoon (<=8y) but the bot has been pushed back. It costs
// the same 30 focus as Raptor Strike, so it slots in any time the bot is
// too far to swing and can afford a spender.
bool ShouldHatchetToss(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HATCHET_TOSS)) return false;
    if (!ctx.bot.is_ready(HATCHET_TOSS)) return false;
    if (FocusVal(ctx) < HATCHET_TOSS_COST) return false;
    NearbyUnit const* t = ctx.bot.victim_info();
    if (!t) return false;
    const float d2 = TargetDistSq(ctx, *t);
    return d2 > 8.0f * 8.0f && d2 <= 40.0f * 40.0f;
}
void DoHatchetToss(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HATCHET_TOSS, ctx.bot.victim());
}

// ---- Bombs ----
// Wildfire Bomb (259495, [R][M], 10 focus, charges via Quick Reload /
// Guerrilla Tactics). Cone + DoT on everything at the impact point, 12.1's
// top SV priority whenever a charge is up. is_ready() folds in charges.
bool ShouldWildfireBomb(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(WILDFIRE_BOMB)) return false;
    if (!ctx.bot.is_ready(WILDFIRE_BOMB)) return false;
    return FocusVal(ctx) >= WILDFIRE_BOMB_COST;
}
void DoWildfireBomb(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(WILDFIRE_BOMB, v->x, v->y, v->z);
    else
        e.cast(WILDFIRE_BOMB, ctx.bot.victim());
}

// ---- Generators / spenders / AoE ----
// Kill Command (259489): SV's version GENERATES focus and drives Tip of the
// Spear, so it is pressed on cooldown ahead of every focus spender.
bool ShouldKillCommand(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.has_pet()) return false;
    if (!ctx.bot.knows_spell(KILL_COMMAND_SV)) return false;
    return ctx.bot.is_ready(KILL_COMMAND_SV);
}
void DoKillCommand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(KILL_COMMAND_SV, ctx.bot.victim());
}

// Boomstick (1261193, [R][M], 60s cd, 50 focus, 20y cone): "Unload a
// series of shotgun blasts in front of you ... reduced damage beyond N
// targets". The AoE spender that replaced Butchery / Carve; a cone, so it
// needs the target roughly in front (the bot faces its victim while
// attacking) and inside 20y. Fires on 2+ attackers or owner /aoe pin, and
// on boss-like targets as a plain big hit so the CD is not wasted in ST.
bool ShouldBoomstick(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BOOMSTICK)) return false;
    if (!ctx.bot.is_ready(BOOMSTICK)) return false;
    if (FocusVal(ctx) < BOOMSTICK_COST) return false;
    NearbyUnit const* t = ctx.bot.victim_info();
    if (!t || TargetDistSq(ctx, *t) > 20.0f * 20.0f) return false;
    return ctx.aoe_preference || ctx.bot.attackers_count() >= 2 || BossLikeTargetEngaged(ctx);
}
void DoBoomstick(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BOOMSTICK, ctx.bot.victim());
}

// Raptor Strike (186270, 30 focus, 5y): the focus spender / filler. In
// 12.1 Mongoose Bite is gone - Mongoose Fury (1252708) and Raptor Swipe
// (1259003) are passive upgrades to Raptor Strike itself, so this is the
// only melee spender and needs no talent branch.
bool ShouldRaptorStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RAPTOR_STRIKE)) return false;
    if (!ctx.bot.is_ready(RAPTOR_STRIKE)) return false;
    return FocusVal(ctx) >= RAPTOR_STRIKE_COST;
}
void DoRaptorStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RAPTOR_STRIKE, ctx.bot.victim());
}

// Hunter's Mark - baseline ranged-damage-taken debuff (5%). Granted
// around L7 and persists across all three specs. One cast per target.
bool ShouldHuntersMark(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HUNTERS_MARK)) return false;
    if (!ctx.bot.is_ready(HUNTERS_MARK)) return false;
    return ctx.bot.find_aura(HUNTERS_MARK, ctx.bot.victim()) == nullptr;
}
void DoHuntersMark(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HUNTERS_MARK, ctx.bot.victim());
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

ApRule const kRules[] = {
    { ShouldRevivePet,         DoRevivePet,         "Revive Pet"                  },
    { ShouldMendPet,           DoMendPet,           "Mend Pet (<=50%)"            },
    { ShouldAspectTurtle,      DoAspectTurtle,      "Aspect of the Turtle (<=20%)"},
    { ShouldSurvivalFittest,   DoSurvivalFittest,   "Survival of the Fittest"     },
    { ShouldExhilaration,      DoExhilaration,      "Exhilaration (<=50%)"        },
    { ShouldFeignDeath,        DoFeignDeath,        "Feign Death (drop aggro)"    },
    { ShouldMisdirection,      DoMisdirection,      "Misdirection (tank threat)"  },
    { ShouldMuzzle,            DoMuzzle,            "Muzzle (interrupt)"          },
    { ShouldIntimidation,      DoIntimidation,      "Intimidation (interrupt fb)" },
    { ShouldBindingShot,       DoBindingShot,       "Binding Shot (3+ AoE)"       },
    { ShouldTarTrap,           DoTarTrap,           "Tar Trap (slow)"             },
    { ShouldPrimalRage,        DoPrimalRage,        "Primal Rage (Bloodlust)"     },
    { ShouldTakedown,          DoTakedown,          "Takedown (burst)"            },
    { ShouldAspectEagle,       DoAspectEagle,       "Aspect of the Eagle (range)" },
    { ShouldHuntersMark,       DoHuntersMark,       "Hunter's Mark (debuff)"      },
    // Harpoon BEFORE the spenders: if the bot is too far away to melee, we
    // need to close the gap before any GCD spender.
    { ShouldHarpoon,           DoHarpoon,           "Harpoon (gap close 8-30y)"   },
    { ShouldWildfireBomb,      DoWildfireBomb,      "Wildfire Bomb"               },
    { ShouldKillCommand,       DoKillCommand,       "Kill Command (focus gen)"    },
    { ShouldBoomstick,         DoBoomstick,         "Boomstick (cone AoE)"        },
    // Hatchet Toss sits just above the melee filler so an out-of-range
    // bot keeps damaging while Harpoon is on CD instead of falling all
    // the way to AutoAttack (which won't reach).
    { ShouldHatchetToss,       DoHatchetToss,       "Hatchet Toss (ranged 8-40y)" },
    { ShouldRaptorStrike,      DoRaptorStrike,      "Raptor Strike (spender)"     },
    { AlwaysInCombat,          DoAutoAttack,        "Engage auto attack"          },
};

} // anonymous

void RegisterApl_Hunter_Survival()
{
    constexpr uint32 SPEC_HUNTER_SURVIVAL = 255;
    RegisterRotation(CLASS_HUNTER, SPEC_HUNTER_SURVIVAL, ApRotation{kRules});
}

} // namespace Playerbot::Combat
