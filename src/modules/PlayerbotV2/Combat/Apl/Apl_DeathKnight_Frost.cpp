// Frost Death Knight - WoW 12.0 enterprise rotation. Two-handed melee DPS
// with runes + runic power, Killing Machine proc spending on Obliterate,
// Rime proc free Howling Blasts, Pillar of Frost burst window, Breath of
// Sindragosa channel option, Remorseless Winter for AoE.
//
// Layered survival: Icebound Fortitude (30% DR) -> Anti-Magic Shell ->
// Death Strike self-heal (Dark Succor proc = free + bigger heal) -> Lichborne.
// Group utility: Raise Ally (combat rez), Anti-Magic Zone, Path of Frost.
// CC: Mind Freeze, Asphyxiate (talent stun), Death Grip (peel/pull), Chains
// of Ice (slow). Major CDs: Pillar of Frost, Empower Rune Weapon,
// Frostwyrm's Fury, Breath of Sindragosa, Obliteration.
//
// ---- Validated spell IDs (wago.tools SpellName.csv + SpellLevels.csv, 2026-05) ----
//   49143  Frost Strike
//   49184  Howling Blast
//   49020  Obliterate
//   194913 Glacial Advance               (talent)
//   196770 Remorseless Winter
//   51271  Pillar of Frost
//   152279 Breath of Sindragosa          (talent channel)
//   279302 Frostwyrm's Fury              (3min)
//   281238 Obliteration                  (talent — KM uptime burst)
//   47568  Empower Rune Weapon
//   57330  Horn of Winter                (talent — instant runes + RP)
//   45524  Chains of Ice
//   195621 Frost Fever                   (FIXED 2026-05: was 55095 which is the LEGACY id with no SpellLevels row; 195621 is BaseLevel 23 active)
//   51128  Killing Machine               (proc buff)
//   59057  Rime                          (FIXED 2026-05: was 59052 which is the LEGACY id with no SpellLevels row; 59057 is BaseLevel 21 active)
//   178819 Dark Succor                   (added 2026-05 — L18 proc, makes next Death Strike free + 20% heal)
//   47528  Mind Freeze
//   108194 Asphyxiate                    (talent)
//   49576  Death Grip
//   61999  Raise Ally
//   48792  Icebound Fortitude
//   48707  Anti-Magic Shell
//   51052  Anti-Magic Zone
//   49039  Lichborne
//   49998  Death Strike
//
// ---- Skipped (with reason) ----
//   Might of the Frozen Wastes (81333)   — passive 2H damage bonus, not castable. Auto-applied
//                                          by the spec when wielding a 2H weapon, no rotation choice.
//   Frostwyrm Roar (8th Tier talent)     — too specialized for talent-blind rotation.
//   Soul Reaper (343294)                 — execute talent, needs <35% HP gate. Deferred.
//   Path of Frost (3714)                 — water-walking utility, out-of-combat.
//   Runic Empowerment (81229) passive    — proc-only, no cast.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated) ----
constexpr uint32 FROST_STRIKE          = 49143;
constexpr uint32 HOWLING_BLAST         = 49184;
constexpr uint32 OBLITERATE            = 49020;
constexpr uint32 GLACIAL_ADVANCE       = 194913;     // talent — AoE spender
constexpr uint32 REMORSELESS_WINTER    = 196770;
constexpr uint32 PILLAR_OF_FROST       = 51271;
constexpr uint32 BREATH_SINDRAGOSA     = 152279;     // talent burst channel
constexpr uint32 FROSTWYRMS_FURY       = 279302;     // 3min CD
constexpr uint32 OBLITERATION          = 281238;     // talent — KM uptime burst
constexpr uint32 EMPOWER_RUNE_WEAPON   = 47568;
constexpr uint32 HORN_OF_WINTER        = 57330;      // talent — instant runes + RP
constexpr uint32 CHAINS_OF_ICE         = 45524;
constexpr uint32 FROST_FEVER           = 195621;     // FIXED: legacy 55095 -> modern 195621 (BaseLevel 23, validated 2026-05)
constexpr uint32 KILLING_MACHINE       = 51128;
constexpr uint32 RIME                  = 59057;      // FIXED: legacy 59052 -> modern 59057 (BaseLevel 21, validated 2026-05)
constexpr uint32 DARK_SUCCOR           = 178819;     // L18 — proc makes next Death Strike free + heal 20%
constexpr uint32 MIND_FREEZE           = 47528;
constexpr uint32 ASPHYXIATE            = 108194;
constexpr uint32 DEATH_GRIP            = 49576;
constexpr uint32 RAISE_ALLY            = 61999;
constexpr uint32 ICEBOUND_FORTITUDE    = 48792;
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
    if (ctx.bot.power(POWER_RUNIC_POWER_IDX) < 50) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoBreathSindragosa(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BREATH_SINDRAGOSA, ctx.bot.victim());
}

bool ShouldObliterationCD(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(OBLITERATION)) return false;
    if (!ctx.bot.is_ready(OBLITERATION)) return false;
    return ctx.bot.has_aura(PILLAR_OF_FROST);
}
void DoObliterationCD(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(OBLITERATION); }

bool ShouldEmpowerRuneWeapon(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(EMPOWER_RUNE_WEAPON)) return false;
    if (!ctx.bot.is_ready(EMPOWER_RUNE_WEAPON)) return false;
    return ctx.bot.has_aura(PILLAR_OF_FROST) || ctx.bot.power(POWER_RUNIC_POWER_IDX) <= 30;
}
void DoEmpowerRuneWeapon(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(EMPOWER_RUNE_WEAPON); }

bool ShouldHornOfWinter(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HORN_OF_WINTER)) return false;
    if (!ctx.bot.is_ready(HORN_OF_WINTER)) return false;
    return ctx.bot.power(POWER_RUNIC_POWER_IDX) <= 50;
}
void DoHornOfWinter(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HORN_OF_WINTER); }

// ---- AoE ----
bool ShouldRemorselessWinter(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(REMORSELESS_WINTER)) return false;
    if (!ctx.bot.is_ready(REMORSELESS_WINTER)) return false;
    // Single-target value too: also generates Frost Fever via Frigid Executioner.
    return true;
}
void DoRemorselessWinter(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(REMORSELESS_WINTER); }

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
    { ShouldAntiMagicShell,       DoAntiMagicShell,       "AMS (incoming cast)"       },
    { ShouldLichborne,            DoLichborne,            "Lichborne"                 },
    { ShouldDeathStrikeDarkSuccor,DoDeathStrike,          "Death Strike (Dark Succor)"},
    { ShouldDeathStrike,          DoDeathStrike,          "Death Strike (<=70% heal)" },
    { ShouldMindFreeze,           DoMindFreeze,           "Mind Freeze (interrupt)"   },
    { ShouldAsphyxiate,           DoAsphyxiate,           "Asphyxiate (interrupt fb)" },
    { ShouldDeathGrip,            DoDeathGrip,            "Death Grip (peel)"         },
    { ShouldChainsOfIce,          DoChainsOfIce,          "Chains of Ice"             },
    { ShouldAntiMagicZone,        DoAntiMagicZone,        "Anti-Magic Zone (boss)"    },
    { ShouldPillarOfFrost,        DoPillarOfFrost,        "Pillar of Frost"           },
    { ShouldFrostwyrmsFury,       DoFrostwyrmsFury,       "Frostwyrm's Fury"          },
    { ShouldBreathSindragosa,     DoBreathSindragosa,     "Breath of Sindragosa"      },
    { ShouldObliterationCD,       DoObliterationCD,       "Obliteration"              },
    { ShouldEmpowerRuneWeapon,    DoEmpowerRuneWeapon,    "Empower Rune Weapon"       },
    { ShouldHornOfWinter,         DoHornOfWinter,         "Horn of Winter"            },
    { ShouldRemorselessWinter,    DoRemorselessWinter,    "Remorseless Winter"        },
    { ShouldGlacialAdvance,       DoGlacialAdvance,       "Glacial Advance (2+ AoE)"  },
    { ShouldHowlingBlastFreeProc, DoHowlingBlast,         "Howling Blast (Rime)"      },
    { ShouldFrostFever,           DoHowlingBlast,         "Frost Fever (refresh)"     },
    { ShouldObliterateProc,       DoObliterate,           "Obliterate (KM proc)"      },
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
