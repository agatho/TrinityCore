// Survival Hunter - WoW 12.0 enterprise rotation. Melee Hunter with bleeds,
// pet uptime, focus economy, and ground-target Wildfire Bomb. Multi-DoT
// cycling spreads Serpent Sting to every nearby enemy via the
// BotSnapshotBuilder enemy outbound scan that already covers spec 255.
//
// Layered survival: Aspect of the Turtle (immunity) -> Exhilaration ->
// Survival of the Fittest -> Feign Death threat dump. Group utility:
// Misdirection (tank threat redirect), Aspect of the Cheetah (movement),
// Aspect of the Wild (group buff). Pet maintenance: Mend Pet, Revive Pet,
// Bestial Wrath. CC: Tar Trap (slow), Freezing Trap (sap), Intimidation
// (talent stun), Binding Shot (talent root). Major CDs: Coordinated
// Assault, Spearhead, Aspect of the Eagle (range extender), Death Chakram.
// Engage: Harpoon (gap close, L14) / Hatchet Toss (ranged opener, L12).
// Execute: Kill Shot.
//
// Validated against wago.tools SpellName.csv 2026-05-27. Every ID below
// resolves to its expected name.
//
// Skipped spec spells (not rotation-relevant — intentional omissions):
//   * Track Pets         (1244920) — minimap-utility, never a damage cast.
//   * Dual Wield         (1277760) — passive (allows dual-wield melee).
//   * Eyes of the Beast  (  321297) — pet-vision toy, breaks bot AI control.
//   * Poison Injection   (  378014) — passive that converts Serpent Sting
//                                     into a Kill Command proc generator;
//                                     casting it as a spell is a no-op
//                                     (was previously mis-labeled
//                                     "Flayed Shot" — Flayed Shot is the
//                                     Shadow Priest Necrolord spell 324149).

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated) ----
constexpr uint32 RAPTOR_STRIKE        = 186270;
constexpr uint32 MONGOOSE_BITE        = 259387;
constexpr uint32 KILL_COMMAND_SV      = 259489;
constexpr uint32 KILL_SHOT_SV         = 53351;
constexpr uint32 WILDFIRE_BOMB        = 259495;
constexpr uint32 CARVE                = 187708;       // melee AoE
constexpr uint32 BUTCHERY             = 212436;       // talent — replaces Carve
constexpr uint32 SERPENT_STING        = 259491;
constexpr uint32 COORDINATED_ASSAULT  = 360952;
constexpr uint32 SPEARHEAD            = 360966;       // talent — bleed CD
constexpr uint32 DEATH_CHAKRAM        = 375891;       // talent — focus gen + AoE
constexpr uint32 HATCHET_TOSS         = 193265;       // L12 ranged opener (30y)
constexpr uint32 HARPOON              = 190925;       // L14 gap closer (8-30y)
constexpr uint32 ASPECT_EAGLE         = 186289;       // L24 burst CD — 40y range
constexpr uint32 INTIMIDATION         = 19577;        // 5sec stun via pet
constexpr uint32 BINDING_SHOT         = 109248;       // talent root cluster
constexpr uint32 MUZZLE               = 187707;       // melee interrupt
constexpr uint32 COUNTER_SHOT         = 147362;       // ranged interrupt
constexpr uint32 MISDIRECTION         = 34477;
constexpr uint32 ASPECT_TURTLE        = 186265;
constexpr uint32 EXHILARATION         = 109304;
constexpr uint32 SURVIVAL_FITTEST     = 264735;       // talent — 20% DR self
constexpr uint32 FEIGN_DEATH          = 5384;
constexpr uint32 MEND_PET             = 136;
constexpr uint32 REVIVE_PET           = 982;          // OOC + combat rez of pet
constexpr uint32 BESTIAL_WRATH        = 19574;        // SV uses too via talent
constexpr uint32 ASPECT_CHEETAH       = 186257;
constexpr uint32 ASPECT_WILD          = 193530;       // group crit buff
constexpr uint32 TAR_TRAP             = 187698;
constexpr uint32 FREEZING_TRAP        = 187650;
constexpr uint32 STEEL_TRAP           = 162488;       // talent ground bleed
constexpr uint32 HUNTERS_MARK         = 257284;
constexpr uint32 PRIMAL_RAGE          = 264667;       // pet bloodlust

constexpr uint8 POWER_FOCUS_IDX = 2;

bool HasLiveTarget(ApPredicateContext const& ctx)
{
    return !ctx.bot.victim().IsEmpty();
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

bool ShouldAspectWild(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ASPECT_WILD)) return false;
    if (!ctx.bot.is_ready(ASPECT_WILD)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoAspectWild(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ASPECT_WILD); }

bool ShouldPrimalRage(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.has_pet()) return false;
    if (!ctx.bot.knows_spell(PRIMAL_RAGE)) return false;
    if (!ctx.bot.is_ready(PRIMAL_RAGE)) return false;
    if (BotHasSatedDebuff(ctx)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoPrimalRage(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(PRIMAL_RAGE); }

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

bool ShouldCounterShot(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(COUNTER_SHOT)) return false;
    if (!ctx.bot.is_ready(COUNTER_SHOT)) return false;
    if (ctx.bot.is_ready(MUZZLE)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    return ctx.bot.kick_target(pvp, 40.0f) != nullptr;
}
void DoCounterShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 40.0f))
        e.cast(COUNTER_SHOT, c->guid);
}

bool ShouldIntimidation(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(INTIMIDATION)) return false;
    if (!ctx.bot.is_ready(INTIMIDATION)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
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

// ---- Major offensive CDs ----
bool ShouldCoordinatedAssault(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(COORDINATED_ASSAULT)) return false;
    if (!ctx.bot.is_ready(COORDINATED_ASSAULT)) return false;
    return ctx.bot.has_pet();
}
void DoCoordinatedAssault(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(COORDINATED_ASSAULT); }

bool ShouldSpearhead(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SPEARHEAD)) return false;
    if (!ctx.bot.is_ready(SPEARHEAD)) return false;
    return ctx.bot.has_pet();
}
void DoSpearhead(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SPEARHEAD, ctx.bot.victim());
}

bool ShouldBestialWrath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BESTIAL_WRATH)) return false;
    if (!ctx.bot.is_ready(BESTIAL_WRATH)) return false;
    return ctx.bot.has_pet();
}
void DoBestialWrath(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BESTIAL_WRATH); }

bool ShouldDeathChakram(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEATH_CHAKRAM)) return false;
    if (!ctx.bot.is_ready(DEATH_CHAKRAM)) return false;
    return true;
}
void DoDeathChakram(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DEATH_CHAKRAM, ctx.bot.victim());
}

// Aspect of the Eagle (186289, L24, 1.5min CD). Extends auto-attack and
// the SV ranged-tagged shots (Kill Shot, Hatchet Toss, Serpent Sting) to
// 40y for 15s. Use as a burst window — fires on boss-like targets so the
// CD isn't blown on a 3-second trash pull. Self-cast (no target).
bool ShouldAspectEagle(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ASPECT_EAGLE)) return false;
    if (!ctx.bot.is_ready(ASPECT_EAGLE)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoAspectEagle(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ASPECT_EAGLE); }

// Compute squared-distance between bot and a NearbyUnit. NearbyUnit
// itself doesn't carry a pre-computed distance — match the cheap inline
// idiom used by enemies_within() in BotSnapshotView.cpp.
inline float TargetDistSq(ApPredicateContext const& ctx, NearbyUnit const& t)
{
    float bx, by, bz; ctx.bot.position(bx, by, bz);
    const float dx = t.x - bx, dy = t.y - by, dz = t.z - bz;
    return dx*dx + dy*dy + dz*dz;
}

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

// Hatchet Toss (193265, L12, 6s CD). 30y ranged opener and the only
// damage cast SV has at range outside Aspect of the Eagle. Use when the
// bot is outside melee + Harpoon is on CD or the target is too close for
// Harpoon (≤8y) but the bot has been pushed back. Also fires while
// Aspect of the Eagle is active (ranged window). Cheap (no focus cost),
// so it slots in any time the bot is too far to swing.
bool ShouldHatchetToss(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HATCHET_TOSS)) return false;
    if (!ctx.bot.is_ready(HATCHET_TOSS)) return false;
    NearbyUnit const* t = ctx.bot.victim_info();
    if (!t) return false;
    const float d2 = TargetDistSq(ctx, *t);
    return d2 > 8.0f * 8.0f && d2 <= 30.0f * 30.0f;
}
void DoHatchetToss(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HATCHET_TOSS, ctx.bot.victim());
}

// ---- Execute / kill ----
bool ShouldKillShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(KILL_SHOT_SV)) return false;
    if (!ctx.bot.is_ready(KILL_SHOT_SV)) return false;
    return TargetExecuteRange(ctx);
}
void DoKillShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(KILL_SHOT_SV, ctx.bot.victim());
}

// ---- Bombs / DoT ----
bool ShouldWildfireBomb(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(WILDFIRE_BOMB)) return false;
    if (!ctx.bot.is_ready(WILDFIRE_BOMB)) return false;
    return true;
}
void DoWildfireBomb(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(WILDFIRE_BOMB, v->x, v->y, v->z);
    else
        e.cast(WILDFIRE_BOMB, ctx.bot.victim());
}

bool ShouldSteelTrap(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(STEEL_TRAP)) return false;
    if (!ctx.bot.is_ready(STEEL_TRAP)) return false;
    return true;
}
void DoSteelTrap(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(STEEL_TRAP, v->x, v->y, v->z);
    else
        e.cast(STEEL_TRAP);
}

bool ShouldSerpentStingPrimary(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SERPENT_STING)) return false;
    AuraEntry const* a = ctx.bot.find_aura(SERPENT_STING, ctx.bot.victim());
    return !a || a->remaining.count() <= 4500;
}
void DoSerpentStingPrimary(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SERPENT_STING, ctx.bot.victim());
}

bool ShouldSerpentStingExpand(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SERPENT_STING)) return false;
    return ctx.bot.enemy_without_my_aura(SERPENT_STING, 30.0f) != nullptr;
}
void DoSerpentStingExpand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* off = ctx.bot.enemy_without_my_aura(SERPENT_STING, 30.0f))
        e.cast(SERPENT_STING, off->guid);
}

// ---- Generators / spenders / AoE ----
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

bool ShouldButchery(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BUTCHERY)) return false;
    if (!ctx.bot.is_ready(BUTCHERY)) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(8.0f) >= 2;
}
void DoButchery(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BUTCHERY); }

bool ShouldCarve(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CARVE)) return false;
    if (ctx.bot.knows_spell(BUTCHERY)) return false;       // Butchery replaces Carve
    return ctx.aoe_preference || ctx.bot.enemies_within(8.0f) >= 2;
}
void DoCarve(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CARVE); }

bool ShouldMongooseBite(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MONGOOSE_BITE)) return false;
    if (!ctx.bot.is_ready(MONGOOSE_BITE)) return false;
    return ctx.bot.power(POWER_FOCUS_IDX) >= 30;
}
void DoMongooseBite(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MONGOOSE_BITE, ctx.bot.victim());
}

bool ShouldRaptorStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RAPTOR_STRIKE)) return false;
    if (ctx.bot.knows_spell(MONGOOSE_BITE)) return false; // Mongoose replaces Raptor
    return ctx.bot.power(POWER_FOCUS_IDX) >= 30;
}
void DoRaptorStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RAPTOR_STRIKE, ctx.bot.victim());
}

// Hunter's Mark — baseline ranged-damage-taken debuff (5%). Granted
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
    { ShouldCounterShot,       DoCounterShot,       "Counter Shot (interrupt fb)" },
    { ShouldIntimidation,      DoIntimidation,      "Intimidation (pet stun)"     },
    { ShouldBindingShot,       DoBindingShot,       "Binding Shot (3+ AoE)"       },
    { ShouldTarTrap,           DoTarTrap,           "Tar Trap (slow)"             },
    { ShouldAspectWild,        DoAspectWild,        "Aspect of the Wild (boss)"   },
    { ShouldPrimalRage,        DoPrimalRage,        "Primal Rage (Bloodlust)"     },
    { ShouldCoordinatedAssault,DoCoordinatedAssault,"Coordinated Assault"         },
    { ShouldSpearhead,         DoSpearhead,         "Spearhead"                   },
    { ShouldBestialWrath,      DoBestialWrath,      "Bestial Wrath"               },
    { ShouldAspectEagle,       DoAspectEagle,       "Aspect of the Eagle (range)" },
    { ShouldDeathChakram,      DoDeathChakram,      "Death Chakram"               },
    { ShouldHuntersMark,       DoHuntersMark,       "Hunter's Mark (debuff)"      },
    // Harpoon BEFORE Kill Shot: if the bot is too far away to melee, we
    // need to close the gap before any GCD spender (including Kill Shot,
    // which is melee-tagged in SV outside Aspect of the Eagle).
    { ShouldHarpoon,           DoHarpoon,           "Harpoon (gap close 8-30y)"   },
    { ShouldKillShot,          DoKillShot,          "Kill Shot (<=20%)"           },
    { ShouldWildfireBomb,      DoWildfireBomb,      "Wildfire Bomb"               },
    { ShouldSteelTrap,         DoSteelTrap,         "Steel Trap"                  },
    { ShouldSerpentStingPrimary, DoSerpentStingPrimary, "Serpent Sting (primary)" },
    { ShouldSerpentStingExpand, DoSerpentStingExpand,  "Serpent Sting (expand)"   },
    { ShouldKillCommand,       DoKillCommand,       "Kill Command"                },
    { ShouldButchery,          DoButchery,          "Butchery (2+ AoE)"           },
    { ShouldCarve,             DoCarve,             "Carve (2+ AoE)"              },
    // Hatchet Toss sits just above the melee fillers so an out-of-range
    // bot keeps damaging while Harpoon is on CD instead of falling all
    // the way to AutoAttack (which won't reach).
    { ShouldHatchetToss,       DoHatchetToss,       "Hatchet Toss (ranged 8-30y)" },
    { ShouldMongooseBite,      DoMongooseBite,      "Mongoose Bite"               },
    { ShouldRaptorStrike,      DoRaptorStrike,      "Raptor Strike (filler)"      },
    { AlwaysInCombat,          DoAutoAttack,        "Engage auto attack"          },
};

} // anonymous

void RegisterApl_Hunter_Survival()
{
    constexpr uint32 SPEC_HUNTER_SURVIVAL = 255;
    RegisterRotation(CLASS_HUNTER, SPEC_HUNTER_SURVIVAL, ApRotation{kRules});
}

} // namespace Playerbot::Combat
