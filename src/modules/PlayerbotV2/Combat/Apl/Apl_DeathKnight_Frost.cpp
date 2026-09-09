// Frost Death Knight - WoW 12.1.0.69587 (Midnight) enterprise rotation.
// Melee DPS with runes + runic power, Killing Machine proc spending on
// Obliterate (Frostscythe on packs), Rime proc free Howling Blasts, Pillar
// of Frost burst window (Frozen Dominion makes Pillar summon Remorseless
// Winter itself), Breath of Sindragosa 8s burst, Reaper's Mark hero active.
//
// Layered survival: Icebound Fortitude (30% DR) -> Death Pact -> Anti-Magic
// Shell -> Death Strike self-heal (Dark Succor proc = free + bigger heal) ->
// Lichborne. Group utility: Raise Ally (combat rez), Anti-Magic Zone.
// CC: Mind Freeze, Asphyxiate (talent stun), Death Grip (peel/pull), Chains
// of Ice (slow). Major CDs: Pillar of Frost, Reaper's Mark, Breath of
// Sindragosa, Frostwyrm's Fury, Empower Rune Weapon.
//
// ---- Validated spell IDs (WoW 12.1.0.69587 SpellName.csv / SkillLineAbility / trait data) ----
//   49143   Frost Strike              (spec talent [R])
//   49184   Howling Blast             (spec talent [R])
//   49020   Obliterate                (spec talent [R])
//   207230  Frostscythe               (spec talent [R] - frontal AoE, KM 4x crit)
//   194913  Glacial Advance           (spec spell - AoE RP spender)
//   196770  Remorseless Winter        (spec spell L19; not castable with Frozen Dominion)
//   377226  Frozen Dominion           (passive talent [R] - Pillar summons RW, gates the RW rule)
//   51271   Pillar of Frost           (spec talent [R], 45s CD / 12s)
//   1249658 Breath of Sindragosa      (spec talent [R]; was 152279 - now 8s burst, 60 RP)
//   279302  Frostwyrm's Fury          (spec talent [R], 90s CD)
//   281238  Obliteration              (passive talent [R] - Frost Strike/HB grant KM during Pillar)
//   47568   Empower Rune Weapon       (spec talent [R] - damage + RP + KM)
//   439843  Reaper's Mark             (Deathbringer hero talent [R])
//   46585   Raise Dead                (class talent [M] - 60s ghoul)
//   45524   Chains of Ice             (baseline L13)
//   195621  Frost Fever               (disease debuff, tracked on victim)
//   51128   Killing Machine           (proc buff)
//   59057   Rime                      (proc buff)
//   178819  Dark Succor               (proc buff - next Death Strike free + heal)
//   47528   Mind Freeze               (class talent [M])
//   221562  Asphyxiate                (class talent; was 108194)
//   49576   Death Grip                (baseline L5)
//   61999   Raise Ally                (baseline L19)
//   48792   Icebound Fortitude        (class talent [R])
//   48743   Death Pact                (class talent [M] - 50% heal)
//   48707   Anti-Magic Shell          (baseline L14)
//   51052   Anti-Magic Zone           (class talent [M])
//   49039   Lichborne                 (baseline L9)
//   49998   Death Strike              (class talent [M])
//
// ---- Skipped (with reason) ----
//   Horn of Winter (57330)             - passive in 12.1 (triggers when all runes are spent); no cast.
//   Exterminate (441378 -> 443564)     - passive: makes the next Obliterates free after Reaper's Mark
//                                        explodes; the regular Obliterate rules consume it.
//   Blinding Sleet (207167), Wraith Walk (212552), Control Undead (111673)
//                                      - M+-only / movement / niche; not in the default build.
//   Sindragosa's Fury (190778), Consumption (205223), Apocalypse (220143)
//                                      - Legion artifact remnant rows in SkillLineAbility, not real 12.1 kit.
//   Path of Frost (3714)               - water-walking utility, out-of-combat.
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
constexpr uint32 FROST_STRIKE          = 49143;
constexpr uint32 HOWLING_BLAST         = 49184;
constexpr uint32 OBLITERATE            = 49020;
constexpr uint32 FROSTSCYTHE           = 207230;     // talent — frontal AoE, 2 runes, KM -> 4x crit
constexpr uint32 GLACIAL_ADVANCE       = 194913;     // spec spell — AoE RP spender
constexpr uint32 REMORSELESS_WINTER    = 196770;
constexpr uint32 FROZEN_DOMINION       = 377226;     // passive talent — Pillar of Frost summons RW itself
constexpr uint32 PILLAR_OF_FROST       = 51271;
constexpr uint32 BREATH_SINDRAGOSA     = 1249658;    // talent — 8s burst, 60 RP (was 152279)
constexpr uint32 FROSTWYRMS_FURY       = 279302;     // 90s CD
constexpr uint32 OBLITERATION          = 281238;     // passive talent — FS/HB grant KM during Pillar
constexpr uint32 EMPOWER_RUNE_WEAPON   = 47568;
constexpr uint32 REAPERS_MARK          = 439843;     // Deathbringer hero talent — 2 runes, 45s CD
constexpr uint32 RAISE_DEAD            = 46585;      // class talent — 60s ghoul, 120s CD
constexpr uint32 CHAINS_OF_ICE         = 45524;
constexpr uint32 FROST_FEVER           = 195621;     // disease debuff tracked on the victim
constexpr uint32 KILLING_MACHINE       = 51128;
constexpr uint32 RIME                  = 59057;      // proc buff — free Howling Blast
constexpr uint32 DARK_SUCCOR           = 178819;     // proc makes next Death Strike free + heal 20%
constexpr uint32 MIND_FREEZE           = 47528;
constexpr uint32 ASPHYXIATE            = 221562;     // 12.1 id (was 108194)
constexpr uint32 DEATH_GRIP            = 49576;
constexpr uint32 RAISE_ALLY            = 61999;
constexpr uint32 ICEBOUND_FORTITUDE    = 48792;
constexpr uint32 DEATH_PACT            = 48743;      // class talent — 50% heal, healing absorb after
constexpr uint32 ANTI_MAGIC_SHELL      = 48707;
constexpr uint32 ANTI_MAGIC_ZONE       = 51052;
constexpr uint32 LICHBORNE             = 49039;
constexpr uint32 DEATH_STRIKE          = 49998;

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

// ---- Survival ----
bool ShouldIceboundFortitude(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ICEBOUND_FORTITUDE)) return false;
    if (!ctx.bot.is_ready(ICEBOUND_FORTITUDE)) return false;
    return ctx.bot.hp_pct() <= 35;
}
void DoIceboundFortitude(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ICEBOUND_FORTITUDE); }

// Death Pact — big instant self-heal with a healing-absorb tail. Panic
// button below IBF's threshold-ish band so a healer isn't wasting casts
// into the absorb at moderate HP.
bool ShouldDeathPact(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DEATH_PACT)) return false;
    if (!ctx.bot.is_ready(DEATH_PACT)) return false;
    return ctx.bot.hp_pct() <= 35;
}
void DoDeathPact(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DEATH_PACT); }

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
    // Bumped from 60->70%: Death Strike is the #1 DK survival button. The
    // heal scales on damage taken in the last 5s, so banking it slightly
    // earlier (while still being hit) absorbs more burst than waiting.
    return ctx.bot.hp_pct() <= 70;
}
void DoDeathStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DEATH_STRIKE, ctx.bot.victim());
}

// Dark Succor — proc from Remorseless Winter / Howling Blast / Frost
// Strike that makes the next Death Strike free AND heal for 20% of max
// HP. Always spend the buff before it falls off (8s window), gated on
// HP <= 85% so we don't burn the proc at full health for nothing.
bool ShouldDeathStrikeDarkSuccor(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEATH_STRIKE)) return false;
    if (!ctx.bot.is_ready(DEATH_STRIKE)) return false;
    if (!ctx.bot.has_aura(DARK_SUCCOR)) return false;
    return ctx.bot.hp_pct() <= 85;
}

// Raise Dead — class talent (M+ build): 60s ghoul on a 120s CD. Keep one out
// whenever we're fighting without a pet.
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
    if (auto const* c = ctx.bot.interruptible_caster())
        return c->guid != ctx.bot.victim();
    return false;
}
void DoDeathGrip(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* c = ctx.bot.interruptible_caster())
        e.cast(DEATH_GRIP, c->guid);
}

bool ShouldChainsOfIce(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CHAINS_OF_ICE)) return false;
    if (!ctx.bot.is_ready(CHAINS_OF_ICE)) return false;
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v) return false;
    if (ctx.bot.has_aura(CHAINS_OF_ICE, v->guid)) return false;
    // Use only on something that's not already in melee — slowing a melee
    // mid-stand-still doesn't help.
    return ctx.bot.enemies_within(8.0f) == 0;
}
void DoChainsOfIce(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CHAINS_OF_ICE, ctx.bot.victim());
}

// ---- Major offensive cooldowns ----
bool ShouldPillarOfFrost(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(PILLAR_OF_FROST)) return false;
    if (!ctx.bot.is_ready(PILLAR_OF_FROST)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(10.0f) >= 3;
}
void DoPillarOfFrost(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(PILLAR_OF_FROST); }

// Reaper's Mark — Deathbringer hero active (2 runes, 45s CD). Mark stacks
// on every Frost/Shadow hit and explodes; Exterminate then makes the next
// Obliterates free. Fire on cooldown, ideally inside Pillar.
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

bool ShouldFrostwyrmsFury(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FROSTWYRMS_FURY)) return false;
    if (!ctx.bot.is_ready(FROSTWYRMS_FURY)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(10.0f) >= 3;
}
void DoFrostwyrmsFury(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FROSTWYRMS_FURY); }

bool ShouldBreathSindragosa(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BREATH_SINDRAGOSA)) return false;
    if (!ctx.bot.is_ready(BREATH_SINDRAGOSA)) return false;
    // 12.1 Breath is an 8s burst costing 60 RP up front (KM/Rime consumption
    // extends it), not the old RP-drain channel.
    if (ctx.bot.power(POWER_RUNIC_POWER_IDX) < 60) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(10.0f) >= 3;
}
void DoBreathSindragosa(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BREATH_SINDRAGOSA, ctx.bot.victim());
}

bool ShouldEmpowerRuneWeapon(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(EMPOWER_RUNE_WEAPON)) return false;
    if (!ctx.bot.is_ready(EMPOWER_RUNE_WEAPON)) return false;
    return ctx.bot.has_aura(PILLAR_OF_FROST) || ctx.bot.power(POWER_RUNIC_POWER_IDX) <= 30;
}
void DoEmpowerRuneWeapon(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(EMPOWER_RUNE_WEAPON); }

// ---- AoE ----
bool ShouldRemorselessWinter(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(REMORSELESS_WINTER)) return false;
    // Frozen Dominion (default build): Pillar of Frost summons Remorseless
    // Winter itself and the button is removed from the bar — don't spend a
    // rune trying to press it.
    if (ctx.bot.knows_spell(FROZEN_DOMINION)) return false;
    if (!ctx.bot.is_ready(REMORSELESS_WINTER)) return false;
    // Single-target value too (Gathering Storm / Frost Fever pressure).
    return true;
}
void DoRemorselessWinter(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(REMORSELESS_WINTER); }

// Frostscythe — frontal cone for 2 runes; with Killing Machine it crits for
// 4x, so on a pack it beats Obliterate for the proc. 8y reach.
bool ShouldFrostscythe(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FROSTSCYTHE)) return false;
    if (!ctx.bot.is_ready(FROSTSCYTHE)) return false;
    const size_t near = ctx.bot.enemies_within(8.0f);
    return near >= 3 || (ctx.aoe_preference && near >= 2);
}
void DoFrostscythe(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FROSTSCYTHE, ctx.bot.victim());
}

bool ShouldGlacialAdvance(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(GLACIAL_ADVANCE)) return false;
    if (ctx.bot.power(POWER_RUNIC_POWER_IDX) < 30) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(15.0f) >= 2;
}
void DoGlacialAdvance(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(GLACIAL_ADVANCE, ctx.bot.victim());
}

// ---- Procs / DoT / spenders / fillers ----
bool ShouldHowlingBlastFreeProc(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HOWLING_BLAST)) return false;
    return ctx.bot.has_aura(RIME);
}
void DoHowlingBlast(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HOWLING_BLAST, ctx.bot.victim());
}

bool ShouldFrostFever(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HOWLING_BLAST)) return false;
    AuraEntry const* a = ctx.bot.find_aura(FROST_FEVER, ctx.bot.victim());
    return !a || a->remaining.count() <= 4000;
}

bool ShouldObliterateProc(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(OBLITERATE)) return false;
    return ctx.bot.has_aura(KILLING_MACHINE);
}
void DoObliterate(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(OBLITERATE, ctx.bot.victim());
}

// Obliteration (passive, default build): during Pillar of Frost every Frost
// Strike / Howling Blast grants Killing Machine. So inside Pillar, whenever
// KM is NOT up, a Frost Strike (35 RP) is the way to fish the next
// guaranteed-crit Obliterate rather than pressing a plain Obliterate.
bool ShouldFrostStrikeObliteration(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(OBLITERATION)) return false;
    if (!ctx.bot.has_aura(PILLAR_OF_FROST)) return false;
    if (ctx.bot.has_aura(KILLING_MACHINE)) return false;
    if (!ctx.bot.knows_spell(FROST_STRIKE)) return false;
    if (!ctx.bot.is_ready(FROST_STRIKE)) return false;
    return ctx.bot.power(POWER_RUNIC_POWER_IDX) >= 35;
}

bool ShouldFrostStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FROST_STRIKE)) return false;
    if (!ctx.bot.is_ready(FROST_STRIKE)) return false;
    // Cap-spend at >=80 RP, otherwise prefer Obliterate when up.
    return ctx.bot.power(POWER_RUNIC_POWER_IDX) >= 80;
}
void DoFrostStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FROST_STRIKE, ctx.bot.victim());
}

bool ShouldObliterate(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(OBLITERATE)) return false;
    return ctx.bot.is_ready(OBLITERATE);
}

bool ShouldFrostStrikeFiller(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FROST_STRIKE)) return false;
    if (!ctx.bot.is_ready(FROST_STRIKE)) return false;
    return ctx.bot.power(POWER_RUNIC_POWER_IDX) >= 25;
}

bool ShouldHowlingBlast(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HOWLING_BLAST)) return false;
    return ctx.bot.is_ready(HOWLING_BLAST);
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
    { ShouldRaiseAlly,            DoRaiseAlly,            "Raise Ally (battle rez)"   },
    { ShouldIceboundFortitude,    DoIceboundFortitude,    "Icebound Fortitude (<=35%)"},
    { ShouldDeathPact,            DoDeathPact,            "Death Pact (<=35%)"        },
    { ShouldAntiMagicShell,       DoAntiMagicShell,       "AMS (incoming cast)"       },
    { ShouldLichborne,            DoLichborne,            "Lichborne"                 },
    { ShouldDeathStrikeDarkSuccor,DoDeathStrike,          "Death Strike (Dark Succor)"},
    { ShouldDeathStrike,          DoDeathStrike,          "Death Strike (<=70% heal)" },
    { ShouldMindFreeze,           DoMindFreeze,           "Mind Freeze (interrupt)"   },
    { ShouldAsphyxiate,           DoAsphyxiate,           "Asphyxiate (interrupt fb)" },
    { ShouldDeathGrip,            DoDeathGrip,            "Death Grip (peel)"         },
    { ShouldChainsOfIce,          DoChainsOfIce,          "Chains of Ice"             },
    { ShouldAntiMagicZone,        DoAntiMagicZone,        "Anti-Magic Zone (boss)"    },
    { ShouldRaiseDead,            DoRaiseDead,            "Raise Dead (ghoul)"        },
    { ShouldPillarOfFrost,        DoPillarOfFrost,        "Pillar of Frost"           },
    { ShouldReapersMark,          DoReapersMark,          "Reaper's Mark"             },
    { ShouldBreathSindragosa,     DoBreathSindragosa,     "Breath of Sindragosa"      },
    { ShouldFrostwyrmsFury,       DoFrostwyrmsFury,       "Frostwyrm's Fury"          },
    { ShouldEmpowerRuneWeapon,    DoEmpowerRuneWeapon,    "Empower Rune Weapon"       },
    { ShouldRemorselessWinter,    DoRemorselessWinter,    "Remorseless Winter"        },
    { ShouldFrostscythe,          DoFrostscythe,          "Frostscythe (3+ AoE)"      },
    { ShouldGlacialAdvance,       DoGlacialAdvance,       "Glacial Advance (2+ AoE)"  },
    { ShouldHowlingBlastFreeProc, DoHowlingBlast,         "Howling Blast (Rime)"      },
    { ShouldFrostFever,           DoHowlingBlast,         "Frost Fever (refresh)"     },
    { ShouldObliterateProc,       DoObliterate,           "Obliterate (KM proc)"      },
    { ShouldFrostStrikeObliteration, DoFrostStrike,       "Frost Strike (Obliteration)"},
    { ShouldFrostStrike,          DoFrostStrike,          "Frost Strike (cap-spend)"  },
    { ShouldObliterate,           DoObliterate,           "Obliterate"                },
    { ShouldFrostStrikeFiller,    DoFrostStrike,          "Frost Strike (RP filler)"  },
    { ShouldHowlingBlast,         DoHowlingBlast,         "Howling Blast (filler)"    },
    { AlwaysInCombat,             DoAutoAttack,           "Engage auto attack"        },
};

} // anonymous

void RegisterApl_DeathKnight_Frost()
{
    constexpr uint32 SPEC_DEATHKNIGHT_FROST = 251;
    RegisterRotation(CLASS_DEATH_KNIGHT, SPEC_DEATHKNIGHT_FROST, ApRotation{kRules});
}

} // namespace Playerbot::Combat
