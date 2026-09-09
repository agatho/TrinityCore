// Blood Death Knight - WoW 12.1.0.69587 (Midnight) enterprise rotation. Tank
// with Bone Shield active mitigation, Death Strike self-heal driven by Runic
// Power, Heart Strike for rune dump and threat. Major CDs layered: Dancing
// Rune Weapon (parry burst + cleave), Vampiric Blood (HP buff + healing),
// Reaper's Mark (Deathbringer hero burst), Consumption (talent - DR window +
// Blood Plague burst), Lichborne (undead self heal), Anti-Magic Zone (group
// magic soak), Asphyxiate / Blinding Sleet CC, Raise Dead ghoul.
//
// Survival ladder: Icebound Fortitude -> Vampiric Blood -> Anti-Magic Shell
// -> Lichborne -> Blinding Sleet (swarmed) -> Death Strike. Tank duties: Dark
// Command taunt, Mind Freeze interrupt, Death Grip pull, Gorefiend's Grasp,
// Death and Decay zone for AoE threat.
//
// ---- Validated spell IDs (WoW 12.1.0.69587 SpellName.csv / SkillLineAbility / trait data) ----
//   49998   Death Strike              (class talent [R])
//   195182  Marrowrend                (spec talent [R])
//   206930  Heart Strike              (spec talent [R])
//   50842   Blood Boil                (spec talent [R])
//   55233   Vampiric Blood            (spec talent [R])
//   49028   Dancing Rune Weapon       (spec talent [R])
//   1263824 Consumption               (spec talent, not in default build; was 274156)
//   195181  Bone Shield               (buff stack tracker)
//   195292  Death's Caress            (spec spell L23 - ranged opener / disease)
//   195621  Frost Fever               (disease debuff carried by Blood Plague)
//   439843  Reaper's Mark             (Deathbringer hero talent [R])
//   46585   Raise Dead                (class talent [R] - 60s ghoul)
//   47528   Mind Freeze               (class talent [R])
//   56222   Dark Command              (baseline L9)
//   221562  Asphyxiate                (class talent [R]; was 108194)
//   207167  Blinding Sleet            (class talent [R] - cone disorient)
//   49576   Death Grip                (baseline L5)
//   61999   Raise Ally                (baseline L19)
//   48792   Icebound Fortitude        (class talent [R])
//   48707   Anti-Magic Shell          (baseline L14)
//   51052   Anti-Magic Zone           (class talent [R])
//   49039   Lichborne                 (baseline L9)
//   43265   Death and Decay           (baseline L3)
//   108199  Gorefiend's Grasp         (spec talent [R])
//   433895  Vampiric Strike           (San'layn hero [R] - aura-driven override of Heart Strike;
//                                      NOT in the spellbook, so gated on the 433899 proc buff only)
//   433899  Vampiric Strike proc      (buff granted by 433901 passive)
//
// ---- Skipped (with reason) ----
//   Rune Tap (194679), Bonestorm (194844), Tombstone (219809), Blooddrinker (206931)
//                                     - removed from the Blood kit in Midnight; not learnable in 12.1.
//   Consumption 205223 / 205224       - Legion artifact remnant rows; 1263824 is the 12.1 talent cast.
//   Abomination Limb (1263569)        - not in the curated builds; 20y pull is disruptive for a bot tank.
//   Death Pact (48743), Wraith Walk (212552), Control Undead (111673)
//                                     - not selected in the default build / movement-only.
//   Runeforging (53428) and runes     - out-of-combat weapon-enchant, not rotation.
//   Bone Shield passive ticks         - auto-driven by Marrowrend / Death's Caress; stack count
//                                       tracked only (see ShouldMarrowrend).

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated) ----
constexpr uint32 DEATH_STRIKE         = 49998;
constexpr uint32 MARROWREND           = 195182;
constexpr uint32 HEART_STRIKE         = 206930;
constexpr uint32 BLOOD_BOIL           = 50842;
constexpr uint32 VAMPIRIC_BLOOD       = 55233;
constexpr uint32 DANCING_RUNE_WEAPON  = 49028;
constexpr uint32 BONE_SHIELD          = 195181;
constexpr uint32 REAPERS_MARK         = 439843;     // Deathbringer hero talent - 2 runes, 45s CD
constexpr uint32 RAISE_DEAD           = 46585;      // class talent - 60s ghoul, 120s CD
constexpr uint32 MIND_FREEZE          = 47528;
constexpr uint32 DARK_COMMAND         = 56222;
constexpr uint32 ASPHYXIATE           = 221562;     // 12.1 id (was 108194)
constexpr uint32 BLINDING_SLEET       = 207167;     // class talent - cone disorient, 60s CD
constexpr uint32 DEATH_GRIP           = 49576;
constexpr uint32 RAISE_ALLY           = 61999;
constexpr uint32 ICEBOUND_FORTITUDE   = 48792;
constexpr uint32 ANTI_MAGIC_SHELL     = 48707;
constexpr uint32 ANTI_MAGIC_ZONE      = 51052;
constexpr uint32 LICHBORNE            = 49039;
constexpr uint32 DEATH_AND_DECAY      = 43265;
constexpr uint32 CONSUMPTION          = 1263824;    // spec talent - DR window + Blood Plague burst (was 274156)
constexpr uint32 GOREFIENDS_GRASP     = 108199;     // mass DR-grip
constexpr uint32 VAMPIRIC_STRIKE      = 433895;     // San'layn — aura override of Heart Strike (never in spellbook)
constexpr uint32 VAMPIRIC_STRIKE_BUFF = 433899;     // proc buff that swaps Heart Strike -> Vampiric Strike
constexpr uint32 DEATHS_CARESS        = 195292;     // L23 — ranged opener / out-of-melee filler, Blood-only
constexpr uint32 FROST_FEVER          = 195621;     // disease applied by Blood Plague/Death's Caress; Blood keeps this rolling for the proc engine

constexpr uint8 POWER_RUNIC_POWER_IDX = 6;

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

// ---- Tank utility ----
bool ShouldDarkCommand(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DARK_COMMAND)) return false;
    if (!ctx.bot.is_ready(DARK_COMMAND)) return false;
    return ctx.bot.untaunted_enemy() != nullptr;
}
void DoDarkCommand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.untaunted_enemy())
        e.cast(DARK_COMMAND, t->guid);
}

bool ShouldDeathGrip(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(DEATH_GRIP)) return false;
    if (!ctx.bot.is_ready(DEATH_GRIP)) return false;
    // Pull a missing add to us — untaunted enemy at range.
    auto const* t = ctx.bot.untaunted_enemy(40.f);
    return t != nullptr;
}
void DoDeathGrip(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.untaunted_enemy(40.f))
        e.cast(DEATH_GRIP, t->guid);
}

bool ShouldGorefiendsGrasp(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(GOREFIENDS_GRASP)) return false;
    if (!ctx.bot.is_ready(GOREFIENDS_GRASP)) return false;
    return ctx.bot.enemies_within(20.0f) >= 4;
}
void DoGorefiendsGrasp(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(GOREFIENDS_GRASP, v->x, v->y, v->z);
    else
        e.cast(GOREFIENDS_GRASP);
}

// ---- Interrupt ----
bool ShouldMindFreeze(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(MIND_FREEZE)) return false;
    if (!ctx.bot.is_ready(MIND_FREEZE)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    return ctx.bot.kick_target(pvp, 15.0f) != nullptr;
}
void DoMindFreeze(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 15.0f))
        e.cast(MIND_FREEZE, c->guid);
}

bool ShouldAsphyxiate(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ASPHYXIATE)) return false;
    if (!ctx.bot.is_ready(ASPHYXIATE)) return false;
    if (ctx.bot.is_ready(MIND_FREEZE)) return false;
    return ctx.bot.interruptible_caster() != nullptr;
}
void DoAsphyxiate(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* c = ctx.bot.interruptible_caster())
        e.cast(ASPHYXIATE, c->guid);
}

// Blinding Sleet — cone disorient (damage breaks it). Only worth it as an
// emergency breather when a trash pack is beating on us and we're dipping;
// never on a boss (immune) and never while the pack is still stacked for
// Heart Strike cleave on a healthy tank.
bool ShouldBlindingSleet(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(BLINDING_SLEET)) return false;
    if (!ctx.bot.is_ready(BLINDING_SLEET)) return false;
    if (BossLikeTargetEngaged(ctx)) return false;
    if (ctx.bot.hp_pct() > 45) return false;
    return ctx.bot.melee_attackers_within(8.0f) >= 3;
}
void DoBlindingSleet(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLINDING_SLEET); }

// ---- Survival ladder ----
bool ShouldIceboundFortitude(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ICEBOUND_FORTITUDE)) return false;
    if (!ctx.bot.is_ready(ICEBOUND_FORTITUDE)) return false;
    return ctx.bot.hp_pct() <= 35;
}
void DoIceboundFortitude(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ICEBOUND_FORTITUDE); }

bool ShouldVampiricBlood(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(VAMPIRIC_BLOOD)) return false;
    if (!ctx.bot.is_ready(VAMPIRIC_BLOOD)) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoVampiricBlood(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(VAMPIRIC_BLOOD); }

bool ShouldAntiMagicShell(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ANTI_MAGIC_SHELL)) return false;
    if (!ctx.bot.is_ready(ANTI_MAGIC_SHELL)) return false;
    return ctx.bot.interruptible_caster() != nullptr;
}
void DoAntiMagicShell(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ANTI_MAGIC_SHELL); }

bool ShouldLichborne(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(LICHBORNE)) return false;
    if (!ctx.bot.is_ready(LICHBORNE)) return false;
    if (ctx.bot.has_aura(LICHBORNE)) return false;
    return ctx.bot.hp_pct() <= 60;
}
void DoLichborne(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(LICHBORNE); }

bool ShouldDeathStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEATH_STRIKE)) return false;
    if (!ctx.bot.is_ready(DEATH_STRIKE)) return false;
    if (ctx.bot.power(POWER_RUNIC_POWER_IDX) < 45) return false;
    // Either we're hurting OR we'd cap RP — both are good times to spend.
    return ctx.bot.hp_pct() <= 75 || ctx.bot.power(POWER_RUNIC_POWER_IDX) >= 100;
}
void DoDeathStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DEATH_STRIKE, ctx.bot.victim());
}

// Consumption (12.1) — 2s damage-reduction window that ends in a frontal
// Shadow burst consuming Blood Plague from the targets. Pop it when hurting
// or when a pack is stacked in front of us (plague on 2+ = big burst).
bool ShouldConsumption(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CONSUMPTION)) return false;
    if (!ctx.bot.is_ready(CONSUMPTION)) return false;
    return ctx.bot.enemies_within(8.0f) >= 2 || ctx.bot.hp_pct() <= 60;
}
void DoConsumption(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CONSUMPTION); }

// Raise Dead — class talent in 12.1: a 60s ghoul on a 120s CD. Keep it out
// whenever we're fighting and don't have one (extra threat + damage; the
// San'layn build also feeds Vampiric procs off pet damage).
bool ShouldRaiseDead(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(RAISE_DEAD)) return false;
    if (ctx.bot.has_pet()) return false;
    return ctx.bot.is_ready(RAISE_DEAD);
}
void DoRaiseDead(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(RAISE_DEAD); }

// ---- Group utility ----
bool ShouldRaiseAlly(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(RAISE_ALLY)) return false;
    if (!ctx.bot.is_ready(RAISE_ALLY)) return false;
    return ctx.group.dead_member_priority(ctx.bot.map_id()) != nullptr;
}
void DoRaiseAlly(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* m = ctx.group.dead_member_priority(ctx.bot.map_id()))
        e.cast(RAISE_ALLY, m->guid);
}

bool ShouldAntiMagicZone(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ANTI_MAGIC_ZONE)) return false;
    if (!ctx.bot.is_ready(ANTI_MAGIC_ZONE)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoAntiMagicZone(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    float bx, by, bz;
    ctx.bot.position(bx, by, bz);
    e.cast_at(ANTI_MAGIC_ZONE, bx, by, bz);
}

// ---- Major offensive cooldowns ----
bool ShouldDancingRuneWeapon(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DANCING_RUNE_WEAPON)) return false;
    if (!ctx.bot.is_ready(DANCING_RUNE_WEAPON)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.attackers_count() >= 2 || ctx.bot.hp_pct() <= 60;
}
void DoDancingRuneWeapon(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DANCING_RUNE_WEAPON); }

// Reaper's Mark — Deathbringer hero active (2 runes, 45s CD). Marks the
// target; every Shadow/Frost hit stacks it and it explodes for heavy
// Shadowfrost damage. Simply on cooldown against the current victim —
// Blood's Heart Strike/Blood Boil/Death's Caress feed the stacks anyway.
bool ShouldReapersMark(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(REAPERS_MARK)) return false;
    return ctx.bot.is_ready(REAPERS_MARK);
}
void DoReapersMark(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(REAPERS_MARK, ctx.bot.victim());
}

// ---- Active mitigation ----
bool ShouldMarrowrend(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MARROWREND)) return false;
    if (!ctx.bot.is_ready(MARROWREND)) return false;
    AuraEntry const* a = ctx.bot.find_aura(BONE_SHIELD);
    // Refresh on low stacks OR when the buff is about to fall off — letting
    // Bone Shield expire drops all active mitigation regardless of stacks.
    if (!a) return true;
    return a->stacks <= 4 || a->remaining.count() <= 6000;
}
void DoMarrowrend(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MARROWREND, ctx.bot.victim());
}

// ---- AoE / fillers ----
bool ShouldDeathAndDecay(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEATH_AND_DECAY)) return false;
    if (!ctx.bot.is_ready(DEATH_AND_DECAY)) return false;
    return ctx.aoe_preference ||
           ctx.bot.attackers_count() >= 2 || ctx.bot.enemies_within(8.0f) >= 2;
}
void DoDeathAndDecay(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(DEATH_AND_DECAY, v->x, v->y, v->z);
    else
        e.cast(DEATH_AND_DECAY);
}

bool ShouldBloodBoil(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BLOOD_BOIL)) return false;
    return ctx.bot.is_ready(BLOOD_BOIL);
}
void DoBloodBoil(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLOOD_BOIL); }

// Vampiric Strike — San'layn proc: Death Coil / Death Strike turn the next
// Heart Strike into Vampiric Strike (Shadow damage + % max-HP heal + Essence
// of the Blood Queen haste). The override spell is granted by the aura, not
// learned, so knows_spell/is_ready would never pass — gate on the proc buff
// and cast the override id directly (core Spell system honours it).
bool ShouldVampiricStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HEART_STRIKE)) return false;
    if (!ctx.bot.has_aura(VAMPIRIC_STRIKE_BUFF)) return false;
    return !ctx.bot.gcd_active();
}
void DoVampiricStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(VAMPIRIC_STRIKE, ctx.bot.victim());
}

bool ShouldHeartStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    return ctx.bot.knows_spell(HEART_STRIKE);
}
void DoHeartStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HEART_STRIKE, ctx.bot.victim());
}

// Death's Caress — ranged Blood opener and out-of-melee filler. Applies
// Blood Plague (which carries Frost Fever uptime in Blood's kit) and
// is the answer to "I have runes but my victim is 15y away" without
// Death-Gripping when DG is on CD. Skip when we're already in melee.
bool ShouldDeathsCaress(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEATHS_CARESS)) return false;
    if (!ctx.bot.is_ready(DEATHS_CARESS)) return false;
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v) return false;
    // Only use when out of melee (>8y); in melee, Heart Strike is the
    // rune dump.
    float bx, by, bz; ctx.bot.position(bx, by, bz);
    const float dx = v->x - bx, dy = v->y - by;
    if ((dx*dx + dy*dy) <= 64.0f) return false;
    // Also fire when Frost Fever is missing on the victim — this is the
    // Blood disease-maintenance step (Blood Plague applies Frost Fever).
    return true;
}
void DoDeathsCaress(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DEATHS_CARESS, ctx.bot.victim());
}

// Frost Fever maintenance — if the disease falls off (kited, target
// swap, etc.) and we're in melee with Death's Caress not yet learned
// or out of range, fall back to Blood Boil which also applies Blood
// Plague (which carries Frost Fever). Belt-and-suspenders so the
// disease never times out.
bool ShouldFrostFeverRefresh(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BLOOD_BOIL)) return false;
    if (!ctx.bot.is_ready(BLOOD_BOIL)) return false;
    AuraEntry const* ff = ctx.bot.find_aura(FROST_FEVER, ctx.bot.victim());
    return !ff || ff->remaining.count() <= 4000;
}
void DoFrostFeverRefresh(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(BLOOD_BOIL);
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
    { ShouldRaiseAlly,           DoRaiseAlly,           "Raise Ally (battle rez)"     },
    { ShouldDarkCommand,         DoDarkCommand,         "Dark Command (taunt)"        },
    { ShouldDeathGrip,           DoDeathGrip,           "Death Grip (pull add)"       },
    { ShouldGorefiendsGrasp,     DoGorefiendsGrasp,     "Gorefiend's Grasp (4+ AoE)"  },
    { ShouldMindFreeze,          DoMindFreeze,          "Mind Freeze (interrupt)"     },
    { ShouldAsphyxiate,          DoAsphyxiate,          "Asphyxiate (interrupt fb)"   },
    { ShouldIceboundFortitude,   DoIceboundFortitude,   "Icebound Fortitude (<=35%)"  },
    { ShouldVampiricBlood,       DoVampiricBlood,       "Vampiric Blood (<=50%)"      },
    { ShouldAntiMagicShell,      DoAntiMagicShell,      "AMS (incoming cast)"         },
    { ShouldLichborne,           DoLichborne,           "Lichborne"                   },
    { ShouldBlindingSleet,       DoBlindingSleet,       "Blinding Sleet (swarmed)"    },
    { ShouldAntiMagicZone,       DoAntiMagicZone,       "Anti-Magic Zone (boss)"      },
    { ShouldDeathStrike,         DoDeathStrike,         "Death Strike (heal/cap)"     },
    { ShouldConsumption,         DoConsumption,         "Consumption"                 },
    { ShouldRaiseDead,           DoRaiseDead,           "Raise Dead (ghoul)"          },
    { ShouldDancingRuneWeapon,   DoDancingRuneWeapon,   "Dancing Rune Weapon"         },
    { ShouldReapersMark,         DoReapersMark,         "Reaper's Mark"               },
    { ShouldMarrowrend,          DoMarrowrend,          "Marrowrend (Bone Shield<=4)" },
    { ShouldVampiricStrike,      DoVampiricStrike,      "Vampiric Strike (proc)"      },
    { ShouldDeathAndDecay,       DoDeathAndDecay,       "Death and Decay (2+ AoE)"    },
    { ShouldFrostFeverRefresh,   DoFrostFeverRefresh,   "Blood Boil (refresh disease)"},
    { ShouldBloodBoil,           DoBloodBoil,           "Blood Boil"                  },
    { ShouldDeathsCaress,        DoDeathsCaress,        "Death's Caress (ranged)"     },
    { ShouldHeartStrike,         DoHeartStrike,         "Heart Strike (filler)"       },
    { AlwaysInCombat,            DoAutoAttack,          "Engage auto attack"          },
};

} // anonymous

void RegisterApl_DeathKnight_Blood()
{
    constexpr uint32 SPEC_DEATHKNIGHT_BLOOD = 250;
    RegisterRotation(CLASS_DEATH_KNIGHT, SPEC_DEATHKNIGHT_BLOOD, ApRotation{kRules});
}

} // namespace Playerbot::Combat
