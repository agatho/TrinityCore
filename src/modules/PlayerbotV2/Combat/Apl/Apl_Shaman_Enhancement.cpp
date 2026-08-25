// Enhancement Shaman - WoW 12.0 enterprise rotation. Dual-wield melee with
// Maelstrom Weapon (5-stack proc) gating instant Lightning Bolt / Chain
// Lightning / Healing Surge. Stormstrike windows + Lava Lash spread Flame
// Shock. Major CDs: Feral Spirit (wolves), Doom Winds (talent burst),
// Ascendance, Sundering (talent — line stun + dmg), Fire Nova (talent —
// detonates Flame Shocks for AoE), Primordial Wave (Flame Shock + Lava
// Burst hybrid), Ashen Catalyst. Gap closer: Feral Lunge (talent).
//
// Group utility: Bloodlust/Heroism, Healing Stream Totem, Spirit Link
// Totem, Cleanse Spirit, Wind Rush Totem (group sprint), Earth Shield.
// CC: Wind Shear, Capacitor Totem, Hex, Earthbind. Survival: Astral Shift,
// Stone Bulwark, Healing Surge (MW-instant self heal), Earth Shield.
//
// Validated IDs (SpellName.csv 2026-05):
//   17364  Stormstrike          60103  Lava Lash         187874 Crash Lightning
//   196840 Frost Shock          188389 Flame Shock       188196 Lightning Bolt
//   188443 Chain Lightning      117014 Elemental Blast   51533  Feral Spirit
//   384352 Doom Winds           114051 Ascendance (enh)  197214 Sundering
//   333974 Fire Nova            375982 Primordial Wave   342240 Ice Strike
//   196884 Feral Lunge          344179 Maelstrom Weapon  57994  Wind Shear
//   108271 Astral Shift         108270 Stone Bulwark     974    Earth Shield
//   8004   Healing Surge        5394   Healing Stream    98008  Spirit Link
//   192077 Wind Rush Totem      192058 Capacitor Totem   51514  Hex
//   370    Purge                51886  Cleanse Spirit    2008   Ancestral Spirit
//   2825   Bloodlust            32182  Heroism
//
// Skipped spells (and why):
//   • 201845 Stormsurge (passive proc — buffs Stormstrike off-GCD, no APL)
//   • 86629  Dual Wield (passive, no APL)
//   • 157444 Critical Strikes (passive, no APL)
//   • 192106 Lightning Shield (handled by baseline self-buff macro)
//   • 79206  Spiritwalker's Grace (Enhancement is melee — hard-cast windows
//     gated by MW proc instead; SWG is owned by Elemental / Restoration)

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "ApCrowdControl.h"
#include "ApDispelHelpers.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated) ----
constexpr uint32 STORMSTRIKE          = 17364;
constexpr uint32 LAVA_LASH            = 60103;
constexpr uint32 CRASH_LIGHTNING      = 187874;
constexpr uint32 FROST_SHOCK_ENH      = 196840;
constexpr uint32 FLAME_SHOCK          = 188389;
constexpr uint32 LIGHTNING_BOLT_ENH   = 188196;
constexpr uint32 CHAIN_LIGHTNING_ENH  = 188443;
constexpr uint32 ELEMENTAL_BLAST      = 117014;
constexpr uint32 FERAL_SPIRIT         = 51533;
constexpr uint32 DOOM_WINDS           = 384352;
constexpr uint32 ASCENDANCE_ENH       = 114051;
constexpr uint32 SUNDERING            = 197214;       // talent
constexpr uint32 FIRE_NOVA            = 333974;       // talent
constexpr uint32 PRIMORDIAL_WAVE      = 375982;       // talent
constexpr uint32 ICE_STRIKE           = 342240;       // talent
constexpr uint32 FERAL_LUNGE          = 196884;       // talent — gap closer
constexpr uint32 MAELSTROM_WEAPON     = 344179;
constexpr uint32 WIND_SHEAR           = 57994;
constexpr uint32 ASTRAL_SHIFT         = 108271;
constexpr uint32 STONE_BULWARK_TOTEM  = 108270;
constexpr uint32 EARTH_SHIELD         = 974;
constexpr uint32 HEALING_SURGE        = 8004;
constexpr uint32 HEALING_STREAM_TOTEM = 5394;
constexpr uint32 SPIRIT_LINK_TOTEM    = 98008;
constexpr uint32 WIND_RUSH_TOTEM      = 192077;
constexpr uint32 CAPACITOR_TOTEM      = 192058;
constexpr uint32 HEX                  = 51514;
constexpr uint32 PURGE                = 370;
constexpr uint32 CLEANSE_SPIRIT       = 51886;
constexpr uint32 ANCESTRAL_SPIRIT     = 2008;
constexpr uint32 BLOODLUST            = 2825;
constexpr uint32 HEROISM              = 32182;
constexpr uint32 SATED_DEBUFF         = 57724;
constexpr uint32 TEMPORAL_DISPL_DEBUFF= 80354;
constexpr uint32 INSANITY_HUNTER_DEBUFF = 95809;
constexpr uint32 FATIGUED_DEBUFF      = 264689;

bool HasLiveTarget(ApPredicateContext const& ctx)
{
    return !ctx.bot.victim().IsEmpty();
}

bool BotHasSatedDebuff(ApPredicateContext const& ctx)
{
    return ctx.bot.has_aura(SATED_DEBUFF)
        || ctx.bot.has_aura(TEMPORAL_DISPL_DEBUFF)
        || ctx.bot.has_aura(INSANITY_HUNTER_DEBUFF)
        || ctx.bot.has_aura(FATIGUED_DEBUFF);
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

uint8 MaelstromStacks(ApPredicateContext const& ctx)
{
    return ctx.bot.aura_stacks(MAELSTROM_WEAPON);
}

// ---- Group utility ----
bool ShouldBloodlust(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    const uint32 sid = ctx.bot.knows_spell(BLOODLUST) ? BLOODLUST
                     : ctx.bot.knows_spell(HEROISM)   ? HEROISM
                     : 0;
    if (!sid) return false;
    if (!ctx.bot.is_ready(sid)) return false;
    if (BotHasSatedDebuff(ctx)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoBloodlust(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const uint32 sid = ctx.bot.knows_spell(BLOODLUST) ? BLOODLUST : HEROISM;
    e.cast(sid);
}

bool ShouldAncestralSpirit(ApPredicateContext const& ctx)
{
    if (ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ANCESTRAL_SPIRIT)) return false;
    return ctx.group.dead_member(ctx.bot.map_id()) != nullptr;
}
void DoAncestralSpirit(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* m = ctx.group.dead_member(ctx.bot.map_id()))
        e.cast(ANCESTRAL_SPIRIT, m->guid);
}

bool ShouldHealingStreamTotem(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(HEALING_STREAM_TOTEM)) return false;
    if (!ctx.bot.is_ready(HEALING_STREAM_TOTEM)) return false;
    return !ctx.bot.has_aura(HEALING_STREAM_TOTEM);
}
void DoHealingStreamTotem(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HEALING_STREAM_TOTEM); }

bool ShouldSpiritLinkTotem(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SPIRIT_LINK_TOTEM)) return false;
    if (!ctx.bot.is_ready(SPIRIT_LINK_TOTEM)) return false;
    auto const* low = ctx.group.lowest_hp_on_map(ctx.bot.map_id(), Role::Unknown, ctx.bot.raw().position.x, ctx.bot.raw().position.y, ctx.bot.raw().position.z, 45.0f);
    if (!low || !low->online || low->hp <= 0) return false;
    return (low->hp * 100) / low->max_hp <= 30;
}
void DoSpiritLinkTotem(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SPIRIT_LINK_TOTEM); }

bool ShouldCleanseSpirit(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(CLEANSE_SPIRIT)) return false;
    if (!ctx.bot.is_ready(CLEANSE_SPIRIT)) return false;
    if (auto const* m = ctx.group.dispel_candidate(DispelType::Curse)) return true;
    return ctx.bot.self_dispellable(DispelType::Curse);
}
void DoCleanseSpirit(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    GroupMemberSummary const* tgt = DispelTargetWithPriority(ctx,
        [](GroupSnapshotView const& g) -> GroupMemberSummary const*
        {
            return g.dispel_candidate(DispelType::Curse);
        });
    if (tgt) { e.cast(CLEANSE_SPIRIT, tgt->guid); return; }
    e.cast(CLEANSE_SPIRIT, ctx.bot.raw().guid);
}

// ---- Survival ----
bool ShouldAstralShift(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ASTRAL_SHIFT)) return false;
    if (!ctx.bot.is_ready(ASTRAL_SHIFT)) return false;
    return ctx.bot.hp_pct() <= 40;
}
void DoAstralShift(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ASTRAL_SHIFT); }

bool ShouldStoneBulwark(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(STONE_BULWARK_TOTEM)) return false;
    if (!ctx.bot.is_ready(STONE_BULWARK_TOTEM)) return false;
    return ctx.bot.hp_pct() <= 70;
}
void DoStoneBulwark(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(STONE_BULWARK_TOTEM); }

bool ShouldEarthShield(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(EARTH_SHIELD)) return false;
    return !ctx.bot.has_aura(EARTH_SHIELD);
}
void DoEarthShield(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(EARTH_SHIELD, ctx.bot.raw().guid);
}

bool ShouldHealingSurgeProc(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HEALING_SURGE)) return false;
    if (MaelstromStacks(ctx) < 5) return false;
    return ctx.bot.hp_pct() <= 60;
}
void DoHealingSurgeSelf(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HEALING_SURGE, ctx.bot.raw().guid);
}

// ---- Interrupt / CC ----
bool ShouldWindShear(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(WIND_SHEAR)) return false;
    if (!ctx.bot.is_ready(WIND_SHEAR)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    return ctx.bot.kick_target(pvp, 30.0f) != nullptr;
}
void DoWindShear(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 30.0f))
        e.cast(WIND_SHEAR, c->guid);
}

bool ShouldCapacitorTotem(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(CAPACITOR_TOTEM)) return false;
    if (!ctx.bot.is_ready(CAPACITOR_TOTEM)) return false;
    return ctx.bot.enemies_within(8.0f) >= 3;
}
void DoCapacitorTotem(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CAPACITOR_TOTEM); }

// Off-target Hex via the shared PickOffTargetCC gate (PvE: only on a 2+
// ATTACKER pull, skipping already-CC'd mobs; PvP: enemy Healer > caster).
// See ApCrowdControl.h — replaced the old nearby_enemies.size()>=2 + has_aura
// gate that fired every GCD on a 40y scan bystander during questing.
bool ShouldHex(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HEX)) return false;
    if (!ctx.bot.is_ready(HEX)) return false;
    if (MaelstromStacks(ctx) < 5) return false;        // costs MW stacks
    return !PickOffTargetCC(ctx, HEX, ApInPvp(ctx)).IsEmpty();
}
void DoHex(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    ObjectGuid const t = PickOffTargetCC(ctx, HEX, ApInPvp(ctx));
    if (!t.IsEmpty()) e.cast(HEX, t);
}

bool ShouldPurge(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(PURGE)) return false;
    if (MaelstromStacks(ctx) < 5) return false;
    return ctx.bot.target_dispellable(Playerbot::DispelType::Magic);
}
void DoPurge(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(PURGE, ctx.bot.victim());
}

// ---- Major offensive cooldowns ----
bool ShouldAscendance(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ASCENDANCE_ENH)) return false;
    if (!ctx.bot.is_ready(ASCENDANCE_ENH)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoAscendance(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ASCENDANCE_ENH); }

bool ShouldFeralSpirit(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FERAL_SPIRIT)) return false;
    if (!ctx.bot.is_ready(FERAL_SPIRIT)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(10.0f) >= 3;
}
void DoFeralSpirit(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FERAL_SPIRIT); }

bool ShouldDoomWinds(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DOOM_WINDS)) return false;
    if (!ctx.bot.is_ready(DOOM_WINDS)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoDoomWinds(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DOOM_WINDS); }

bool ShouldPrimordialWave(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(PRIMORDIAL_WAVE)) return false;
    if (!ctx.bot.is_ready(PRIMORDIAL_WAVE)) return false;
    return true;
}
void DoPrimordialWave(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(PRIMORDIAL_WAVE, ctx.bot.victim());
}

bool ShouldSundering(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SUNDERING)) return false;
    if (!ctx.bot.is_ready(SUNDERING)) return false;
    return ctx.bot.enemies_within(10.0f) >= 2;
}
void DoSundering(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SUNDERING); }

bool ShouldFireNova(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FIRE_NOVA)) return false;
    if (!ctx.bot.is_ready(FIRE_NOVA)) return false;
    // Best when multiple Flame Shocks are out (multi-target). Approximated
    // by enemy count since we don't track per-enemy aura inventories.
    // Threshold raised 2026-05-22 (≥2 → ≥3): Fire Nova is a Maelstrom
    // spender; on 2 targets it generates roughly break-even Maelstrom
    // vs spending the GCD on a Stormstrike. ≥3 keeps the AoE pivot
    // strictly Maelstrom-positive.
    return ctx.bot.enemies_within(12.0f) >= 3;
}
void DoFireNova(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FIRE_NOVA); }

// ---- Damage rotation ----
bool ShouldFlameShock(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FLAME_SHOCK)) return false;
    AuraEntry const* a = ctx.bot.find_aura(FLAME_SHOCK, ctx.bot.victim());
    return !a || a->remaining.count() <= 4000;
}
void DoFlameShock(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FLAME_SHOCK, ctx.bot.victim());
}

bool ShouldElementalBlastMW(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ELEMENTAL_BLAST)) return false;
    if (!ctx.bot.is_ready(ELEMENTAL_BLAST)) return false;
    return MaelstromStacks(ctx) >= 5;
}
void DoElementalBlast(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ELEMENTAL_BLAST, ctx.bot.victim());
}

bool ShouldChainLightningMW(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CHAIN_LIGHTNING_ENH)) return false;
    if (MaelstromStacks(ctx) < 5) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(20.0f) >= 2;
}
void DoChainLightning(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CHAIN_LIGHTNING_ENH, ctx.bot.victim());
}

bool ShouldLightningBoltMW(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(LIGHTNING_BOLT_ENH)) return false;
    return MaelstromStacks(ctx) >= 5;
}
void DoLightningBolt(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LIGHTNING_BOLT_ENH, ctx.bot.victim());
}

// Feral Lunge — 196884, 25y leap to a target. Enhancement gap-closer.
// Fires when the bot's victim sits outside melee range so Stormstrike /
// Lava Lash / Crash Lightning don't fall through to ranged fillers.
// Uses victim_info() position; bails when the victim isn't a tracked
// nearby unit (e.g. fresh tab-target the snapshot hasn't seen yet).
bool ShouldFeralLunge(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FERAL_LUNGE)) return false;
    if (!ctx.bot.is_ready(FERAL_LUNGE)) return false;
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v) return false;
    float bx, by, bz;
    ctx.bot.position(bx, by, bz);
    const float dx = v->x - bx, dy = v->y - by, dz = v->z - bz;
    const float d2 = dx*dx + dy*dy + dz*dz;
    // Trigger past melee swing range (~5y) but inside the spell's 25y
    // cap. Squared so we avoid sqrt: 8y² = 64, 25y² = 625.
    return d2 > 64.0f && d2 <= 625.0f;
}
void DoFeralLunge(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FERAL_LUNGE, ctx.bot.victim());
}

bool ShouldStormstrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(STORMSTRIKE)) return false;
    return ctx.bot.is_ready(STORMSTRIKE);
}
void DoStormstrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(STORMSTRIKE, ctx.bot.victim());
}

bool ShouldCrashLightning(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CRASH_LIGHTNING)) return false;
    if (!ctx.bot.is_ready(CRASH_LIGHTNING)) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(8.0f) >= 2;
}
void DoCrashLightning(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CRASH_LIGHTNING); }

bool ShouldLavaLash(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(LAVA_LASH)) return false;
    return ctx.bot.is_ready(LAVA_LASH);
}
void DoLavaLash(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LAVA_LASH, ctx.bot.victim());
}

bool ShouldIceStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ICE_STRIKE)) return false;
    if (!ctx.bot.is_ready(ICE_STRIKE)) return false;
    return true;
}
void DoIceStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ICE_STRIKE, ctx.bot.victim());
}

bool ShouldFrostShock(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    return ctx.bot.knows_spell(FROST_SHOCK_ENH);
}
void DoFrostShock(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FROST_SHOCK_ENH, ctx.bot.victim());
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

// Rule ORDER (spec): Astral Shift → Healing Surge self (MW proc gates) →
// Wind Shear → Feral Lunge gap-close → Flame Shock DoT → Maelstrom Weapon
// 5-stack spenders (Elemental Blast / Chain Lightning AoE / Lightning Bolt)
// → Stormstrike (signature) → Crash Lightning (AoE 2+) → Lava Lash →
// talent AoE builders (Sundering / Fire Nova) → Ice Strike → Frost Shock
// filler / snare → AutoAttack.
// The MW spenders sit ABOVE the melee builders because the evaluator fires
// only the first matching rule per tick: the builders gate only on
// knows_spell+is_ready (essentially always true off-GCD) and would otherwise
// consume the tick at 5+ MW stacks, wasting Enhancement's instant procs.
// The spenders gate on MaelstromStacks>=5, so below 5 stacks they fall
// through to the builders unchanged. Major CDs (Ascendance, Feral Spirit,
// Doom Winds, Primordial Wave) slot above Flame Shock so they fire on
// cooldown but don't displace the GCD-cheap core.
ApRule const kRules[] = {
    { ShouldAncestralSpirit,  DoAncestralSpirit,  "Ancestral Spirit (rez OOC)" },
    { ShouldEarthShield,      DoEarthShield,      "Earth Shield (self)"        },
    { ShouldAstralShift,      DoAstralShift,      "Astral Shift (<=40%)"       },
    { ShouldStoneBulwark,     DoStoneBulwark,     "Stone Bulwark Totem"        },
    { ShouldHealingSurgeProc, DoHealingSurgeSelf, "Healing Surge (MW self)"    },
    { ShouldWindShear,        DoWindShear,        "Wind Shear (interrupt)"     },
    { ShouldCapacitorTotem,   DoCapacitorTotem,   "Capacitor Totem (3+ AoE)"   },
    { ShouldHex,              DoHex,              "Hex (off-target CC)"        },
    { ShouldPurge,            DoPurge,            "Purge (Magic dispel)"       },
    { ShouldCleanseSpirit,    DoCleanseSpirit,    "Cleanse Spirit (Curse)"     },
    { ShouldSpiritLinkTotem,  DoSpiritLinkTotem,  "Spirit Link Totem"          },
    { ShouldHealingStreamTotem,DoHealingStreamTotem,"Healing Stream Totem"     },
    { ShouldBloodlust,        DoBloodlust,        "Bloodlust/Heroism (boss)"   },
    { ShouldAscendance,       DoAscendance,       "Ascendance"                 },
    { ShouldFeralSpirit,      DoFeralSpirit,      "Feral Spirit"               },
    { ShouldDoomWinds,        DoDoomWinds,        "Doom Winds"                 },
    { ShouldPrimordialWave,   DoPrimordialWave,   "Primordial Wave"            },
    { ShouldFeralLunge,       DoFeralLunge,       "Feral Lunge (gap close)"    },
    { ShouldFlameShock,       DoFlameShock,       "Flame Shock"                },
    { ShouldElementalBlastMW, DoElementalBlast,   "Elemental Blast (MW)"       },
    { ShouldChainLightningMW, DoChainLightning,   "Chain Lightning (MW 2+)"    },
    { ShouldLightningBoltMW,  DoLightningBolt,    "Lightning Bolt (MW)"        },
    { ShouldStormstrike,      DoStormstrike,      "Stormstrike"                },
    { ShouldCrashLightning,   DoCrashLightning,   "Crash Lightning (2+ AoE)"   },
    { ShouldLavaLash,         DoLavaLash,         "Lava Lash"                  },
    { ShouldSundering,        DoSundering,        "Sundering (2+ AoE)"         },
    { ShouldFireNova,         DoFireNova,         "Fire Nova (3+ AoE)"         },
    { ShouldIceStrike,        DoIceStrike,        "Ice Strike"                 },
    { ShouldFrostShock,       DoFrostShock,       "Frost Shock (filler)"       },
    { AlwaysInCombat,         DoAutoAttack,       "Engage auto attack"         },
};

} // anonymous

void RegisterApl_Shaman_Enhancement()
{
    constexpr uint32 SPEC_SHAMAN_ENHANCEMENT = 263;
    RegisterRotation(CLASS_SHAMAN, SPEC_SHAMAN_ENHANCEMENT, ApRotation{kRules});
}

} // namespace Playerbot::Combat
