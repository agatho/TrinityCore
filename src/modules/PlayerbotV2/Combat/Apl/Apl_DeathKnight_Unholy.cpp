// Unholy Death Knight - WoW 12.1.0.69587 (Midnight) enterprise rotation.
// Midnight removed Festering Wounds: Festering Strike now blights the weapon
// so the next 2-3 Scourge Strikes each summon a Lesser Ghoul, Army of the
// Dead empowers those ghouls into an "Orders" army, Putrefy summons and
// detonates one, and Soul Reaper (<35%) consumes them. Diseases: Outbreak
// applies Virulent Plague + Dread Plague; Death Coil / Epidemic extend both
// and Dark Transformation (Eternal Agony); Sudden Doom procs cheap Coils.
//
// Layered survival: Icebound Fortitude (30% DR) -> Anti-Magic Shell (magic
// absorb) -> Death Strike (self-heal via Runic Power, free + bigger heal on
// Dark Succor proc) -> Lichborne. Group utility: Raise Ally (combat rez),
// Anti-Magic Zone (group magic soak). CC: Mind Freeze interrupt, Asphyxiate
// (stun fallback), Death Grip (peel/pull). Major CDs: Army of the Dead,
// Dark Transformation, Soul Reaper execute.
//
// ---- Validated spell IDs (WoW 12.1.0.69587 SpellName.csv / SkillLineAbility / trait data) ----
//   85948   Festering Strike          (spec spell L10, overrides Rune Strike - 2 runes, blights weapon)
//   1240994 Festering Strike buff     (blight: next Scourge Strikes summon a Lesser Ghoul)
//   55090   Scourge Strike            (spec talent [R] - 1 rune, plagues erupt)
//   47541   Death Coil                (baseline L2 - 30 RP)
//   49998   Death Strike              (class talent [R])
//   77575   Outbreak                  (spec talent [R] - Virulent + Dread Plague)
//   191587  Virulent Plague           (disease debuff tracked on enemies)
//   42650   Army of the Dead          (spec talent [R], 90s CD)
//   1233448 Dark Transformation       (spec talent [R], 45s CD; was 325554 = Rank 2 passive)
//   1247378 Putrefy                   (spec talent [R] - 1 rune, 40y, summon + detonate ghoul)
//   343294  Soul Reaper               (spec talent [R] - execute <35%, 15s CD)
//   81340   Sudden Doom proc          (buff from 49530 passive - cheap Death Coil / Epidemic)
//   178819  Dark Succor               (proc buff - next Death Strike free + heal)
//   433895  Vampiric Strike           (San'layn hero [R][M] - aura-driven override of Scourge Strike;
//                                      NOT in the spellbook, so gated on the 433899 proc buff only)
//   433899  Vampiric Strike proc      (buff granted by 433901 passive)
//   47528   Mind Freeze               (class talent [R])
//   221562  Asphyxiate                (class talent; was 108194)
//   49576   Death Grip                (baseline L5)
//   61999   Raise Ally                (baseline L19)
//   48792   Icebound Fortitude        (class talent [R])
//   48707   Anti-Magic Shell          (baseline L14)
//   51052   Anti-Magic Zone           (class talent [R])
//   49039   Lichborne                 (baseline L9)
//   43265   Death and Decay           (baseline L3)
//   207317  Epidemic                  (spec spell L18 - 30 RP AoE spender)
//   46584   Raise Dead                (spec spell, overrides 46585 - permanent ghoul, 30s CD)
//   46585   Raise Dead                (class talent [R] - fallback when the spec override is absent)
//
// ---- Skipped (with reason) ----
//   Festering Wound (197147)           - mechanic removed from Unholy in Midnight; no 12.1 ability
//                                        applies or bursts wounds. Old wound-count gating deleted.
//   Clawing Shadows (207311 -> 1241567) - passive in 12.1 (Scourge Strike damage + chain stacks);
//                                        Scourge Strike stays the cast.
//   Summon Gargoyle (49206 -> 1242147) - passive in 12.1 (Army of the Dead summons the Gargoyle).
//   Apocalypse (275699 / 220143)       - talent gone; 220143 is a Legion artifact remnant row.
//   Unholy Assault (207289), Strangulate (47476 PvP talent) - not learnable by Unholy in 12.1.
//   Ghoul Leap (47482)                 - pet gap-closer, not an interrupt/stun; fallback rule removed.
//   Runic Corruption (51462)           - passive, no cast.
//   Wraith Walk (212552), Death's Advance (48265) - movement utility, not rotation.
//   Dread Plague (1240996)             - host-limited (few targets); not gated to avoid Outbreak spam.
//   Runeforging (53428) and runes      - out-of-combat weapon-enchant, not rotation.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated) ----
constexpr uint32 FESTERING_STRIKE     = 85948;        // 2 runes — blights weapon (next Scourge Strikes summon ghouls)
constexpr uint32 FESTERING_BLIGHT     = 1240994;      // buff from Festering Strike (stacks = ghoul-summoning Scourge Strikes left)
constexpr uint32 SCOURGE_STRIKE       = 55090;        // 1 rune — plagues erupt, spreads Virulent Plague
constexpr uint32 DEATH_COIL           = 47541;
constexpr uint32 DEATH_STRIKE         = 49998;        // RP -> self heal
constexpr uint32 OUTBREAK             = 77575;
constexpr uint32 VIRULENT_PLAGUE      = 191587;
constexpr uint32 ARMY_OF_THE_DEAD     = 42650;        // 90s CD — empowers Lesser Ghouls into Orders
constexpr uint32 DARK_TRANSFORMATION  = 1233448;      // 45s CD (was 325554 = Rank 2 passive)
constexpr uint32 PUTREFY              = 1247378;      // 1 rune, 40y — summon a Lesser Ghoul that strikes + explodes
constexpr uint32 SOUL_REAPER          = 343294;       // execute <35% — consumes Lesser Ghouls, 15s CD
constexpr uint32 SUDDEN_DOOM          = 81340;        // proc buff — cheap Death Coil / Epidemic
constexpr uint32 DARK_SUCCOR          = 178819;       // proc, next Death Strike free + 20% heal
constexpr uint32 VAMPIRIC_STRIKE      = 433895;       // San'layn — aura override of Scourge Strike (never in spellbook)
constexpr uint32 VAMPIRIC_STRIKE_BUFF = 433899;       // proc buff that swaps Scourge Strike -> Vampiric Strike
constexpr uint32 MIND_FREEZE          = 47528;
constexpr uint32 ASPHYXIATE           = 221562;       // 12.1 id (was 108194) — 5sec stun
constexpr uint32 DEATH_GRIP           = 49576;        // pull / peel
constexpr uint32 RAISE_ALLY           = 61999;
constexpr uint32 ICEBOUND_FORTITUDE   = 48792;
constexpr uint32 ANTI_MAGIC_SHELL     = 48707;
constexpr uint32 ANTI_MAGIC_ZONE      = 51052;        // group magic soak
constexpr uint32 LICHBORNE            = 49039;        // undead self heal
constexpr uint32 DEATH_AND_DECAY      = 43265;
constexpr uint32 EPIDEMIC             = 207317;
constexpr uint32 RAISE_DEAD           = 46584;        // spec override — permanent ghoul, 30s CD
constexpr uint32 RAISE_DEAD_BASE      = 46585;        // class talent — 60s ghoul, fallback without the override

// Runic Power index in the power array.
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

// ---- Pet maintenance ----
// Raise Dead: the Unholy spec spell 46584 (permanent ghoul) overrides the
// class talent 46585 (60s ghoul). Two-branch like Victory Rush / Impending
// Victory so both the override and the plain talent bots keep a pet out.
bool ShouldRaiseDead(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(RAISE_DEAD)) return false;
    if (ctx.bot.has_pet()) return false;
    if (!ctx.bot.is_ready(RAISE_DEAD)) return false;
    return true;
}
void DoRaiseDead(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(RAISE_DEAD); }

bool ShouldRaiseDeadBase(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(RAISE_DEAD)) return false;
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(RAISE_DEAD_BASE)) return false;
    if (ctx.bot.has_pet()) return false;
    return ctx.bot.is_ready(RAISE_DEAD_BASE);
}
void DoRaiseDeadBase(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(RAISE_DEAD_BASE); }

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
// Army of the Dead (12.1): 90s CD, 30s window that turns every Lesser Ghoul
// summon into an "Order" (cleave / strike / etc.). Short enough to use on
// bosses and on any real pack, not just the 3-minute boss-only opener.
bool ShouldArmyOfTheDead(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ARMY_OF_THE_DEAD)) return false;
    if (!ctx.bot.is_ready(ARMY_OF_THE_DEAD)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(10.0f) >= 3;
}
void DoArmyOfTheDead(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ARMY_OF_THE_DEAD); }

// Soul Reaper — execute: only usable below 35% target HP, 15s CD, 1 rune;
// consumes Lesser Ghouls for extra hits and amps disease/minion damage.
bool ShouldSoulReaper(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SOUL_REAPER)) return false;
    if (!ctx.bot.is_ready(SOUL_REAPER)) return false;
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v || v->max_hp <= 0) return false;
    return (int64_t(v->hp) * 100) < (int64_t(v->max_hp) * 35);
}
void DoSoulReaper(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SOUL_REAPER, ctx.bot.victim());
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

// Putrefy — 1 rune, 40y: a Lesser Ghoul strikes the target and explodes
// on nearby enemies. Rune spender of choice on packs (explosion cleave +
// an Order during Army), and the ranged filler when the victim is out of
// melee reach instead of walking with runes banked.
bool ShouldPutrefy(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(PUTREFY)) return false;
    if (!ctx.bot.is_ready(PUTREFY)) return false;
    const size_t near = ctx.bot.enemies_within(10.0f);
    if (near >= 2 || ctx.aoe_preference) return true;
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v) return false;
    float bx, by, bz; ctx.bot.position(bx, by, bz);
    const float dx = v->x - bx, dy = v->y - by;
    return (dx*dx + dy*dy) > 64.0f;   // >8y: out of melee, use the ranged summon
}
void DoPutrefy(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(PUTREFY, ctx.bot.victim());
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

// ---- Blight build / Scourge spend ----
// Vampiric Strike — San'layn proc: Death Coil / Epidemic / Death Strike turn
// the next Scourge Strike into Vampiric Strike (Shadow damage + % max-HP
// heal + Essence of the Blood Queen haste). The override spell is granted
// by the aura, not learned, so knows_spell/is_ready would never pass — gate
// on the proc buff and cast the override id directly.
bool ShouldVampiricStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SCOURGE_STRIKE)) return false;
    if (!ctx.bot.has_aura(VAMPIRIC_STRIKE_BUFF)) return false;
    return !ctx.bot.gcd_active();
}
void DoVampiricStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(VAMPIRIC_STRIKE, ctx.bot.victim());
}

// Scourge Strike while the weapon is blighted: each strike summons a
// Lesser Ghoul (the 12.1 damage engine) and erupts the plagues.
bool ShouldScourgeStrikeBlighted(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SCOURGE_STRIKE)) return false;
    if (!ctx.bot.is_ready(SCOURGE_STRIKE)) return false;
    return ctx.bot.has_aura(FESTERING_BLIGHT);
}
void DoScourgeStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SCOURGE_STRIKE, ctx.bot.victim());
}

// Festering Strike (2 runes) re-blights the weapon once the charges are
// spent. Without the buff, this is the rune priority.
bool ShouldFesteringStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FESTERING_STRIKE)) return false;
    if (!ctx.bot.is_ready(FESTERING_STRIKE)) return false;
    return !ctx.bot.has_aura(FESTERING_BLIGHT);
}
void DoFesteringStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FESTERING_STRIKE, ctx.bot.victim());
}

// Plain Scourge Strike filler: only 1 rune available (Festering needs 2)
// or the blight buff id does not resolve on this build — never sit on runes.
bool ShouldScourgeStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SCOURGE_STRIKE)) return false;
    return ctx.bot.is_ready(SCOURGE_STRIKE);
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
    { ShouldRaiseDeadBase,     DoRaiseDeadBase,     "Raise Dead (talent ghoul)"    },
    { ShouldMindFreeze,        DoMindFreeze,        "Mind Freeze (interrupt)"      },
    { ShouldAsphyxiate,        DoAsphyxiate,        "Asphyxiate (interrupt fb)"    },
    { ShouldDeathGrip,         DoDeathGrip,         "Death Grip (peel)"            },
    { ShouldAntiMagicZone,     DoAntiMagicZone,     "Anti-Magic Zone (boss)"       },
    { ShouldArmyOfTheDead,     DoArmyOfTheDead,     "Army of the Dead"             },
    { ShouldDarkTransformation,DoDarkTransformation,"Dark Transformation"          },
    { ShouldSoulReaper,        DoSoulReaper,        "Soul Reaper (<35% execute)"   },
    { ShouldOutbreakPrimary,   DoOutbreakPrimary,   "Outbreak (primary)"           },
    { ShouldOutbreakExpand,    DoOutbreakExpand,    "Outbreak (expand off-target)" },
    { ShouldDeathAndDecay,     DoDeathAndDecay,     "Death and Decay (3+ AoE)"     },
    { ShouldEpidemic,          DoEpidemic,          "Epidemic (3+ AoE)"            },
    { ShouldSuddenDoomCoil,    DoDeathCoil,         "Death Coil (Sudden Doom)"     },
    { ShouldVampiricStrike,    DoVampiricStrike,    "Vampiric Strike (proc)"       },
    { ShouldPutrefy,           DoPutrefy,           "Putrefy (AoE / ranged)"       },
    { ShouldScourgeStrikeBlighted, DoScourgeStrike, "Scourge Strike (blighted)"    },
    { ShouldFesteringStrike,   DoFesteringStrike,   "Festering Strike (blight)"    },
    { ShouldDeathCoilDump,     DoDeathCoil,         "Death Coil (RP dump)"         },
    { ShouldScourgeStrike,     DoScourgeStrike,     "Scourge Strike (filler)"      },
    { AlwaysInCombat,          DoAutoAttack,        "Engage auto attack"           },
};

} // anonymous

void RegisterApl_DeathKnight_Unholy()
{
    constexpr uint32 SPEC_DEATHKNIGHT_UNHOLY = 252;
    RegisterRotation(CLASS_DEATH_KNIGHT, SPEC_DEATHKNIGHT_UNHOLY, ApRotation{kRules});
}

} // namespace Playerbot::Combat
