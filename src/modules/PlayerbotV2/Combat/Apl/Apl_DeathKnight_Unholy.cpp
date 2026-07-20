// Unholy Death Knight - WoW 12.0 enterprise rotation. Disease-driven melee
// DPS with Festering Wound stacking, ghoul minion uptime, Army of the Dead /
// Apocalypse burst, and multi-target Virulent Plague spread via Outbreak +
// the BotSnapshotBuilder enemy outbound scan that already covers spec 252.
//
// Layered survival: Icebound Fortitude (30% DR) -> Anti-Magic Shell (magic
// absorb) -> Death Strike (self-heal via Runic Power, free + bigger heal on
// Dark Succor proc) -> Lichborne (talent). Group utility: Raise Ally
// (combat rez), Anti-Magic Zone (10s group magic soak), Death's Advance
// (movement), Path of Frost. CC: Mind Freeze interrupt, Asphyxiate (talent
// stun), Strangulate (talent silence), Death Grip (peel/pull). Major CDs:
// Army of the Dead, Apocalypse, Dark Transformation, Summon Gargoyle,
// Unholy Assault.
//
// ---- Validated spell IDs (wago.tools SpellName.csv + SpellLevels.csv, 2026-05) ----
//   85948  Festering Strike
//   197147 Festering Wound              (FIXED 2026-05: was 194310 which does NOT resolve in SpellName.csv;
//                                        197147 is the actual debuff stack id, BaseLevel 1)
//   55090  Scourge Strike
//   207311 Clawing Shadows              (talent — replaces Scourge Strike)
//   47541  Death Coil
//   49998  Death Strike
//   77575  Outbreak
//   191587 Virulent Plague
//   275699 Apocalypse
//   42650  Army of the Dead
//   325554 Dark Transformation          (FIXED 2026-05: was 63560 legacy with no SpellLevels row;
//                                        325554 is BaseLevel 52, the modern pet-CD cast)
//   207289 Unholy Assault               (talent)
//   49206  Summon Gargoyle              (talent / replaced by Dark Arbiter row)
//   81340  Sudden Doom                  (proc — free Death Coil)
//   195621 Frost Fever                  (added 2026-05 — applied alongside Virulent Plague by Outbreak;
//                                        tracked passively, not gated explicitly because Outbreak
//                                        already maintains both diseases together)
//   178819 Dark Succor                  (added 2026-05 — L18 proc, makes next Death Strike free + 20% heal)
//   47528  Mind Freeze
//   108194 Asphyxiate                   (talent)
//   47476  Strangulate                  (talent)
//   47482  Leap                         (pet ghoul ability — fallback interrupt)
//   49576  Death Grip
//   61999  Raise Ally
//   48792  Icebound Fortitude
//   48707  Anti-Magic Shell
//   51052  Anti-Magic Zone
//   49039  Lichborne                    (talent)
//   43265  Death and Decay
//   207317 Epidemic
//   48265  Death's Advance
//   46584  Raise Dead
//
// ---- Skipped (with reason) ----
//   Runic Corruption (51462)            — PASSIVE proc that speeds rune regen on Festering Wound
//                                          consumption. Not castable; auto-engages from the rotation's
//                                          Scourge/Clawing strikes. No rule needed.
//   Lesser Ghoul (1255830)              — pet creature identity, not a player spell. Skipped per
//                                          SpecializationSpells audit note.
//   Soul Reaper (343294)                — execute-window talent, needs <35% HP gate. Deferred.
//   Reanimation (210128)                — out-of-combat utility, not rotation.
//   Festering Wound legacy id 194310    — does NOT resolve in modern SpellName.csv; replaced
//                                          with the correct 197147 above.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated) ----
constexpr uint32 FESTERING_STRIKE     = 85948;
constexpr uint32 FESTERING_WOUND      = 197147;       // FIXED 2026-05: legacy 194310 does not exist in modern SpellName.csv; 197147 is the correct stack-tracked debuff
constexpr uint32 SCOURGE_STRIKE       = 55090;
constexpr uint32 CLAWING_SHADOWS      = 207311;       // talent — replaces Scourge Strike
constexpr uint32 DEATH_COIL           = 47541;
constexpr uint32 DEATH_STRIKE         = 49998;        // RP -> self heal
constexpr uint32 OUTBREAK             = 77575;
constexpr uint32 VIRULENT_PLAGUE      = 191587;
constexpr uint32 APOCALYPSE           = 275699;
constexpr uint32 ARMY_OF_THE_DEAD     = 42650;
constexpr uint32 DARK_TRANSFORMATION  = 325554;       // FIXED 2026-05: legacy 63560 has no SpellLevels row; 325554 is the modern BaseLevel-52 pet CD
constexpr uint32 UNHOLY_ASSAULT       = 207289;       // talent — burst CD
constexpr uint32 SUMMON_GARGOYLE      = 49206;        // talent — replaced by Dark Arbiter line
constexpr uint32 SUDDEN_DOOM          = 81340;
constexpr uint32 RUNIC_CORRUPTION     = 51462;        // passive proc — speeds rune regen on FW consumption; NOT cast directly (kept for documentation)
constexpr uint32 DARK_SUCCOR          = 178819;       // L18 — proc, next Death Strike free + 20% heal
constexpr uint32 MIND_FREEZE          = 47528;
constexpr uint32 ASPHYXIATE           = 108194;       // talent — 5sec stun
constexpr uint32 STRANGULATE          = 47476;        // talent — silence
constexpr uint32 GHOUL_LEAP           = 47482;        // pet ghoul leap — 2sec stun fallback
constexpr uint32 DEATH_GRIP           = 49576;        // pull / peel
constexpr uint32 RAISE_ALLY           = 61999;
constexpr uint32 ICEBOUND_FORTITUDE   = 48792;
constexpr uint32 ANTI_MAGIC_SHELL     = 48707;
constexpr uint32 ANTI_MAGIC_ZONE      = 51052;        // group magic soak
constexpr uint32 LICHBORNE            = 49039;        // talent — undead self heal
constexpr uint32 DEATH_AND_DECAY      = 43265;
constexpr uint32 EPIDEMIC             = 207317;
constexpr uint32 DEATHS_ADVANCE       = 48265;        // movement
constexpr uint32 RAISE_DEAD           = 46584;        // permanent ghoul (talent)

// Runic Power index in WoW 12.0 power array.
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

uint8 FesteringWoundStacks(ApPredicateContext const& ctx)
{
    return ctx.bot.aura_stacks(FESTERING_WOUND, ctx.bot.victim());
}

// ---- Pet maintenance ----
bool ShouldRaiseDead(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(RAISE_DEAD)) return false;
    if (ctx.bot.has_pet()) return false;
    if (!ctx.bot.is_ready(RAISE_DEAD)) return false;
    return true;
}
void DoRaiseDead(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(RAISE_DEAD); }

// ---- Survival ----
bool ShouldIceboundFortitude(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ICEBOUND_FORTITUDE)) return false;
    if (!ctx.bot.is_ready(ICEBOUND_FORTITUDE)) return false;
    return ctx.bot.hp_pct() <= 35;
}
void DoIceboundFortitude(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ICEBOUND_FORTITUDE); }

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
    // Pop when fear/charm/sleep about to land or HP is dipping; we can't
    // see incoming CC types, so we gate on HP only.
    return ctx.bot.hp_pct() <= 60;
}
void DoLichborne(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(LICHBORNE); }

bool ShouldDeathStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEATH_STRIKE)) return false;
    if (!ctx.bot.is_ready(DEATH_STRIKE)) return false;
    if (ctx.bot.power(POWER_RUNIC_POWER_IDX) < 45) return false;
    // Bumped 60->70%: Death Strike is the #1 DK survival button. Bank
    // the heal earlier — it scales on damage taken in last 5s, so a
    // slightly proactive cast absorbs more burst than waiting.
    return ctx.bot.hp_pct() <= 70;
}
void DoDeathStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DEATH_STRIKE, ctx.bot.victim());
}

// Dark Succor — proc that makes the next Death Strike free + heal 20%.
// Spend it whenever it's up and we're below full HP. 8s window so we
// don't have to be picky about timing.
bool ShouldDeathStrikeDarkSuccor(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEATH_STRIKE)) return false;
    if (!ctx.bot.is_ready(DEATH_STRIKE)) return false;
    if (!ctx.bot.has_aura(DARK_SUCCOR)) return false;
    return ctx.bot.hp_pct() <= 85;
}

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
    // Drop when group is taking magic damage en masse or boss is up.
    return BossLikeTargetEngaged(ctx);
}
void DoAntiMagicZone(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    float bx, by, bz;
    ctx.bot.position(bx, by, bz);
    e.cast_at(ANTI_MAGIC_ZONE, bx, by, bz);
}

// Pet ghoul Leap — fallback interrupt when bot's own Mind Freeze /
// Asphyxiate / Strangulate are all on CD. Fired through PetCastSpellIntent
// since the leap belongs to the ghoul's spellbook, not the DK's.
bool ShouldGhoulLeap(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.has_pet()) return false;
    if (ctx.bot.is_ready(MIND_FREEZE)) return false;
    if (ctx.bot.is_ready(ASPHYXIATE)) return false;
    if (ctx.bot.is_ready(STRANGULATE)) return false;
    auto const* c = ctx.bot.interruptible_caster();
    return c && c->guid == ctx.bot.victim();
}
void DoGhoulLeap(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* c = ctx.bot.interruptible_caster())
        e.pet_cast(GHOUL_LEAP, c->guid);
}

// ---- Interrupt / CC ----
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

bool ShouldStrangulate(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(STRANGULATE)) return false;
    if (!ctx.bot.is_ready(STRANGULATE)) return false;
    if (ctx.bot.is_ready(MIND_FREEZE)) return false;
    if (ctx.bot.is_ready(ASPHYXIATE)) return false;
    return ctx.bot.interruptible_caster() != nullptr;
}
void DoStrangulate(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* c = ctx.bot.interruptible_caster())
        e.cast(STRANGULATE, c->guid);
}

bool ShouldDeathGrip(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(DEATH_GRIP)) return false;
    if (!ctx.bot.is_ready(DEATH_GRIP)) return false;
    // Use as a peel — pull a caster off our healer or self.
    if (auto const* c = ctx.bot.interruptible_caster())
        return c->guid != ctx.bot.victim();
    return false;
}
void DoDeathGrip(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* c = ctx.bot.interruptible_caster())
        e.cast(DEATH_GRIP, c->guid);
}

// ---- Major offensive cooldowns ----
bool ShouldArmyOfTheDead(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ARMY_OF_THE_DEAD)) return false;
    if (!ctx.bot.is_ready(ARMY_OF_THE_DEAD)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoArmyOfTheDead(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ARMY_OF_THE_DEAD); }

bool ShouldUnholyAssault(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(UNHOLY_ASSAULT)) return false;
    if (!ctx.bot.is_ready(UNHOLY_ASSAULT)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoUnholyAssault(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(UNHOLY_ASSAULT, ctx.bot.victim());
}

bool ShouldSummonGargoyle(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SUMMON_GARGOYLE)) return false;
    if (!ctx.bot.is_ready(SUMMON_GARGOYLE)) return false;
    return true;
}
void DoSummonGargoyle(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SUMMON_GARGOYLE, ctx.bot.victim());
}

bool ShouldApocalypse(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(APOCALYPSE)) return false;
    if (!ctx.bot.is_ready(APOCALYPSE)) return false;
    return FesteringWoundStacks(ctx) >= 4;
}
void DoApocalypse(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(APOCALYPSE, ctx.bot.victim());
}

bool ShouldDarkTransformation(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.has_pet()) return false;
    if (!ctx.bot.knows_spell(DARK_TRANSFORMATION)) return false;
    return ctx.bot.is_ready(DARK_TRANSFORMATION);
}
void DoDarkTransformation(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DARK_TRANSFORMATION); }

// ---- DoT (primary + multi-target expand) ----
bool ShouldOutbreakPrimary(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(OUTBREAK)) return false;
    AuraEntry const* a = ctx.bot.find_aura(VIRULENT_PLAGUE, ctx.bot.victim());
    return !a || a->remaining.count() <= 4000;
}
void DoOutbreakPrimary(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(OUTBREAK, ctx.bot.victim());
}

bool ShouldOutbreakExpand(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(OUTBREAK)) return false;
    return ctx.bot.enemy_without_my_aura(VIRULENT_PLAGUE, 30.0f) != nullptr;
}
void DoOutbreakExpand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* off = ctx.bot.enemy_without_my_aura(VIRULENT_PLAGUE, 30.0f))
        e.cast(OUTBREAK, off->guid);
}

// ---- AoE ----
bool ShouldDeathAndDecay(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEATH_AND_DECAY)) return false;
    if (!ctx.bot.is_ready(DEATH_AND_DECAY)) return false;
    // aoe_preference still requires ≥2 enemies; stale `.aoe on` from
    // prior pack would otherwise drop D&D on a single boss pull.
    const int near = ctx.bot.enemies_within(10.0f);
    return near >= 3 || (ctx.aoe_preference && near >= 2);
}
void DoDeathAndDecay(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(DEATH_AND_DECAY, v->x, v->y, v->z);
    else
        e.cast(DEATH_AND_DECAY);
}

bool ShouldEpidemic(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(EPIDEMIC)) return false;
    if (ctx.bot.power(POWER_RUNIC_POWER_IDX) < 30) return false;
    const int near = ctx.bot.enemies_within(20.0f);
    return near >= 3 || (ctx.aoe_preference && near >= 2);
}
void DoEpidemic(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(EPIDEMIC, ctx.bot.victim());
}

// ---- Proc spend ----
bool ShouldSuddenDoomCoil(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEATH_COIL)) return false;
    return ctx.bot.has_aura(SUDDEN_DOOM);
}
void DoDeathCoil(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DEATH_COIL, ctx.bot.victim());
}

// ---- Wound burst / build ----
bool ShouldClawingShadows(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CLAWING_SHADOWS)) return false;
    if (!ctx.bot.is_ready(CLAWING_SHADOWS)) return false;
    return FesteringWoundStacks(ctx) >= 1;
}
void DoClawingShadows(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CLAWING_SHADOWS, ctx.bot.victim());
}

bool ShouldScourgeStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(CLAWING_SHADOWS)) return false;
    if (!ctx.bot.knows_spell(SCOURGE_STRIKE)) return false;
    if (!ctx.bot.is_ready(SCOURGE_STRIKE)) return false;
    return FesteringWoundStacks(ctx) >= 1;
}
void DoScourgeStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SCOURGE_STRIKE, ctx.bot.victim());
}

bool ShouldFesteringStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FESTERING_STRIKE)) return false;
    if (!ctx.bot.is_ready(FESTERING_STRIKE)) return false;
    return FesteringWoundStacks(ctx) < 4;
}
void DoFesteringStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FESTERING_STRIKE, ctx.bot.victim());
}

bool ShouldDeathCoilDump(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEATH_COIL)) return false;
    return ctx.bot.power(POWER_RUNIC_POWER_IDX) >= 80;
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
    { ShouldRaiseAlly,         DoRaiseAlly,         "Raise Ally (battle rez)"      },
    { ShouldIceboundFortitude, DoIceboundFortitude, "Icebound Fortitude (<=35%)"   },
    { ShouldAntiMagicShell,    DoAntiMagicShell,    "AMS (incoming cast)"          },
    { ShouldLichborne,         DoLichborne,         "Lichborne"                    },
    { ShouldDeathStrikeDarkSuccor, DoDeathStrike,   "Death Strike (Dark Succor)"   },
    { ShouldDeathStrike,       DoDeathStrike,       "Death Strike (<=70% heal)"    },
    { ShouldRaiseDead,         DoRaiseDead,         "Raise Dead (ghoul)"           },
    { ShouldMindFreeze,        DoMindFreeze,        "Mind Freeze (interrupt)"      },
    { ShouldAsphyxiate,        DoAsphyxiate,        "Asphyxiate (interrupt fb)"    },
    { ShouldStrangulate,       DoStrangulate,       "Strangulate (silence fb)"     },
    { ShouldGhoulLeap,         DoGhoulLeap,         "Ghoul Leap (interrupt fb)"    },
    { ShouldDeathGrip,         DoDeathGrip,         "Death Grip (peel)"            },
    { ShouldAntiMagicZone,     DoAntiMagicZone,     "Anti-Magic Zone (boss)"       },
    { ShouldArmyOfTheDead,     DoArmyOfTheDead,     "Army of the Dead"             },
    { ShouldApocalypse,        DoApocalypse,        "Apocalypse (4+ wounds)"       },
    { ShouldDarkTransformation,DoDarkTransformation,"Dark Transformation"          },
    { ShouldUnholyAssault,     DoUnholyAssault,     "Unholy Assault"               },
    { ShouldSummonGargoyle,    DoSummonGargoyle,    "Summon Gargoyle"              },
    { ShouldOutbreakPrimary,   DoOutbreakPrimary,   "Outbreak (primary)"           },
    { ShouldOutbreakExpand,    DoOutbreakExpand,    "Outbreak (expand off-target)" },
    { ShouldDeathAndDecay,     DoDeathAndDecay,     "Death and Decay (3+ AoE)"     },
    { ShouldEpidemic,          DoEpidemic,          "Epidemic (3+ AoE)"            },
    { ShouldSuddenDoomCoil,    DoDeathCoil,         "Death Coil (Sudden Doom)"     },
    { ShouldClawingShadows,    DoClawingShadows,    "Clawing Shadows (burst wound)"},
    { ShouldScourgeStrike,     DoScourgeStrike,     "Scourge Strike (burst wound)" },
    { ShouldFesteringStrike,   DoFesteringStrike,   "Festering Strike (build)"     },
    { ShouldDeathCoilDump,     DoDeathCoil,         "Death Coil (RP dump)"         },
    { AlwaysInCombat,          DoAutoAttack,        "Engage auto attack"           },
};

} // anonymous

void RegisterApl_DeathKnight_Unholy()
{
    constexpr uint32 SPEC_DEATHKNIGHT_UNHOLY = 252;
    RegisterRotation(CLASS_DEATH_KNIGHT, SPEC_DEATHKNIGHT_UNHOLY, ApRotation{kRules});
}

} // namespace Playerbot::Combat
