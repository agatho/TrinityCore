// Elemental Shaman - WoW 12.0 enterprise rotation. Maelstrom-builder caster
// with Lava Burst (instant via Lava Surge proc), Earth Shock single-target
// spender, Earthquake AoE spender, Chain Lightning AoE filler. Major CDs:
// Stormkeeper (2 instant Lightning Bolts), Fire Elemental / Storm Elemental
// (talent), Ascendance (talent burst), Icefury (talent — buff Frost Shocks),
// Liquid Magma Totem (talent ground). Group utility: Bloodlust/Heroism,
// Cleanse Spirit (Magic+Curse), Healing Stream Totem, Spirit Link Totem
// (talent — HP redistribution), Spiritwalker's Grace (cast while moving).
// CC: Wind Shear interrupt, Hex (sheep), Capacitor Totem (stun), Earthbind
// Totem (slow), Thunderstorm (knockback panic peel).
// Survival: Astral Shift, Stone Bulwark Totem, Earth Shield.
//
// Validated IDs (SpellName.csv 2026-05):
//   188196 Lightning Bolt        51505 Lava Burst         77762 Lava Surge (proc)
//   188389 Flame Shock           8042  Earth Shock        117014 Elemental Blast
//   191634 Stormkeeper           188443 Chain Lightning   61882 Earthquake
//   210714 Icefury               196840 Frost Shock       51490 Thunderstorm
//   192222 Liquid Magma Totem    114050 Ascendance (ele)  192249 Storm Elemental
//   198067 Fire Elemental        79206  Spiritwalker's    2484  Earthbind Totem
//   192058 Capacitor Totem       51514  Hex               370   Purge
//   2825   Bloodlust             32182  Heroism           57994 Wind Shear
//   108271 Astral Shift          108270 Stone Bulwark     974   Earth Shield
//   5394   Healing Stream Totem  8004   Healing Surge     51886 Cleanse Spirit
//   98008  Spirit Link Totem     2008   Ancestral Spirit
//
// Skipped spells (and why):
//   • 187828 Maelstrom (passive resource, consumed via power(11))
//   • 343725 Maelstrom (duplicate Mastery proc passive — already covered by
//     the active Maelstrom power index)
//   • 378776 Inundate (talent passive Maelstrom-gen — no cast, no APL entry)
//   • 77756 Lava Surge (talent passive that grants the 77762 proc aura;
//     the aura is what `has_aura(LAVA_SURGE)` checks)
//   • 21169 Reincarnation (out-of-combat death recovery, not APL)

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
constexpr uint32 LIGHTNING_BOLT          = 188196;
constexpr uint32 LAVA_BURST              = 51505;
// Lava Surge proc — the aura applied to the player that makes the next
// Lava Burst instant + ignores CD. Two distinct DB2 rows share the name
// "Lava Surge": 77756 is the talent passive (granted at L12), 77762 is
// the proc aura applied at runtime when the passive triggers. APL checks
// `has_aura(LAVA_SURGE)` against the on-bot aura, so 77762 is correct.
constexpr uint32 LAVA_SURGE              = 77762;
constexpr uint32 FLAME_SHOCK             = 188389;
constexpr uint32 EARTH_SHOCK             = 8042;
constexpr uint32 ELEMENTAL_BLAST         = 117014;
constexpr uint32 STORMKEEPER             = 191634;
constexpr uint32 CHAIN_LIGHTNING         = 188443;
constexpr uint32 EARTHQUAKE              = 61882;
constexpr uint32 ICEFURY                 = 210714;
constexpr uint32 FROST_SHOCK             = 196840;
constexpr uint32 THUNDERSTORM            = 51490;       // knockback AoE panic
constexpr uint32 LIQUID_MAGMA_TOTEM      = 192222;
constexpr uint32 ASCENDANCE_ELE          = 114050;
constexpr uint32 STORM_ELEMENTAL         = 192249;
constexpr uint32 FIRE_ELEMENTAL          = 198067;
constexpr uint32 SPIRITWALKER_GRACE      = 79206;
constexpr uint32 EARTHBIND_TOTEM         = 2484;
constexpr uint32 CAPACITOR_TOTEM         = 192058;
constexpr uint32 HEX                     = 51514;
constexpr uint32 PURGE                   = 370;
constexpr uint32 BLOODLUST               = 2825;
constexpr uint32 HEROISM                 = 32182;
constexpr uint32 SATED_DEBUFF            = 57724;
constexpr uint32 TEMPORAL_DISPL_DEBUFF   = 80354;
constexpr uint32 INSANITY_HUNTER_DEBUFF  = 95809;
constexpr uint32 FATIGUED_DEBUFF         = 264689;
constexpr uint32 WIND_SHEAR              = 57994;
constexpr uint32 ASTRAL_SHIFT            = 108271;
constexpr uint32 STONE_BULWARK_TOTEM     = 108270;
constexpr uint32 EARTH_SHIELD            = 974;
constexpr uint32 HEALING_STREAM_TOTEM    = 5394;
constexpr uint32 HEALING_SURGE           = 8004;
constexpr uint32 CLEANSE_SPIRIT          = 51886;
constexpr uint32 SPIRIT_LINK_TOTEM       = 98008;
constexpr uint32 ANCESTRAL_SPIRIT        = 2008;
constexpr uint32 REINCARNATION           = 21169;        // passive

constexpr uint8 POWER_MAELSTROM_IDX = 11;

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

int32 Maelstrom(ApPredicateContext const& ctx) { return ctx.bot.power(POWER_MAELSTROM_IDX); }

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
    // Multi-shaman dedup: skip if any group member is already casting
    // Bloodlust/Heroism/Time Warp/Primal Rage/Drums of Fury this tick.
    // Without this, two Shamans in the same group both fire on boss
    // pull, wasting the second cast (server applies Sated immediately
    // after the first). Audit 2026-05-22.
    if (auto const* members = ctx.group.members())
    {
        constexpr uint32 TIME_WARP    = 80353;
        constexpr uint32 PRIMAL_RAGE  = 264667;     // Hunter pet
        constexpr uint32 DRUMS_FURY   = 178207;     // Engineering drums
        for (auto const& m : *members)
        {
            if (m.guid == ctx.bot.raw().guid) continue;
            if (!m.is_casting) continue;
            if (m.casting_spell == BLOODLUST ||
                m.casting_spell == HEROISM   ||
                m.casting_spell == TIME_WARP ||
                m.casting_spell == PRIMAL_RAGE ||
                m.casting_spell == DRUMS_FURY)
                return false;
        }
    }
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

bool ShouldCleanseSpirit(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(CLEANSE_SPIRIT)) return false;
    if (!ctx.bot.is_ready(CLEANSE_SPIRIT)) return false;
    if (auto const* m = ctx.group.dispel_candidate(DispelType::Magic)) return true;
    if (auto const* m = ctx.group.dispel_candidate(DispelType::Curse)) return true;
    return ctx.bot.self_dispellable(DispelType::Magic) || ctx.bot.self_dispellable(DispelType::Curse);
}
void DoCleanseSpirit(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    GroupMemberSummary const* tgt = DispelTargetWithPriority(ctx,
        [](GroupSnapshotView const& g) -> GroupMemberSummary const*
        {
            if (auto const* m = g.dispel_candidate(DispelType::Magic)) return m;
            if (auto const* m = g.dispel_candidate(DispelType::Curse)) return m;
            return nullptr;
        });
    if (tgt) { e.cast(CLEANSE_SPIRIT, tgt->guid); return; }
    e.cast(CLEANSE_SPIRIT, ctx.bot.raw().guid);
}

bool ShouldHealingStreamTotem(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HEALING_STREAM_TOTEM)) return false;
    if (!ctx.bot.is_ready(HEALING_STREAM_TOTEM)) return false;
    if (ctx.bot.has_aura(HEALING_STREAM_TOTEM)) return false;
    return ctx.bot.in_combat();
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

bool ShouldHealingSurgeSelf(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HEALING_SURGE)) return false;
    return ctx.bot.hp_pct() <= 40;
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

// Off-target Hex via the shared PickOffTargetCC gate. Hex outranks Polymorph
// in PvP because it can't be broken by damage from the Hex'd target's allies
// until 8s; the picker escalates to enemy Healer > caster there. In PvE it
// only fires on a genuine 2+ ATTACKER pull (never a 40y scan bystander while
// solo-questing) and skips already-CC'd mobs via NearbyUnit::is_cc_locked.
// See ApCrowdControl.h for why the old has_aura gate CC-spammed every GCD.
bool ShouldHex(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HEX)) return false;
    if (!ctx.bot.is_ready(HEX)) return false;
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
    return ctx.bot.target_dispellable(Playerbot::DispelType::Magic);
}
void DoPurge(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(PURGE, ctx.bot.victim());
}

bool ShouldSpiritwalkerGrace(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SPIRITWALKER_GRACE)) return false;
    if (!ctx.bot.is_ready(SPIRITWALKER_GRACE)) return false;
    return ctx.bot.is_moving();
}
void DoSpiritwalkerGrace(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SPIRITWALKER_GRACE); }

// ---- Major offensive cooldowns ----
bool ShouldFireElemental(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FIRE_ELEMENTAL)) return false;
    if (ctx.bot.knows_spell(STORM_ELEMENTAL)) return false;
    if (!ctx.bot.is_ready(FIRE_ELEMENTAL)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoFireElemental(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FIRE_ELEMENTAL); }

bool ShouldStormElemental(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(STORM_ELEMENTAL)) return false;
    if (!ctx.bot.is_ready(STORM_ELEMENTAL)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoStormElemental(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(STORM_ELEMENTAL); }

bool ShouldAscendance(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ASCENDANCE_ELE)) return false;
    if (!ctx.bot.is_ready(ASCENDANCE_ELE)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoAscendance(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ASCENDANCE_ELE); }

bool ShouldStormkeeper(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(STORMKEEPER)) return false;
    if (!ctx.bot.is_ready(STORMKEEPER)) return false;
    return true;
}
void DoStormkeeper(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(STORMKEEPER); }

bool ShouldLiquidMagmaTotem(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(LIQUID_MAGMA_TOTEM)) return false;
    if (!ctx.bot.is_ready(LIQUID_MAGMA_TOTEM)) return false;
    return ctx.bot.enemies_within(20.0f) >= 2;
}
void DoLiquidMagmaTotem(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(LIQUID_MAGMA_TOTEM); }

bool ShouldIcefury(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ICEFURY)) return false;
    if (!ctx.bot.is_ready(ICEFURY)) return false;
    return true;
}
void DoIcefury(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ICEFURY, ctx.bot.victim());
}

bool ShouldFrostShock(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FROST_SHOCK)) return false;
    if (!ctx.bot.is_ready(FROST_SHOCK)) return false;
    return ctx.bot.has_aura(ICEFURY);
}
void DoFrostShock(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FROST_SHOCK, ctx.bot.victim());
}

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

// Lava Burst — split into two rules so the rotation can prioritize the
// Lava Surge proc cast (instant, no CD) at the top of the damage chain,
// then fall through to a hardcast version after spenders are exhausted.
bool ShouldLavaBurstSurgeProc(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(LAVA_BURST)) return false;
    if (!ctx.bot.is_ready(LAVA_BURST)) return false;
    return ctx.bot.has_aura(LAVA_SURGE);
}
bool ShouldLavaBurstHardcast(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(LAVA_BURST)) return false;
    if (!ctx.bot.is_ready(LAVA_BURST)) return false;
    // Hard-cast Lava Burst is a 2s cast — never start mid-move.
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(LAVA_BURST)) return false;
    return true;
}
void DoLavaBurst(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LAVA_BURST, ctx.bot.victim());
}

// Thunderstorm — 51490, 30y AoE knockback + small damage on a 45s CD.
// Used as a panic / peel button when melee converge on the caster:
// fires when at least 2 hostiles are within 10y AND bot HP is under
// pressure. Not used as a general AoE damage tool; the knockback is
// the value, and using it on a pack the tank just established would
// be griefing in a group context.
bool ShouldThunderstorm(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(THUNDERSTORM)) return false;
    if (!ctx.bot.is_ready(THUNDERSTORM)) return false;
    // Solo / open-world only — in dungeons / raids the tank's positioning
    // shouldn't be disrupted by a stray knockback. is_in_instance covers
    // both dungeon and raid maps.
    if (ctx.bot.is_in_instance()) return false;
    return ctx.bot.enemies_within(10.0f) >= 2 && ctx.bot.hp_pct() <= 55;
}
void DoThunderstorm(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(THUNDERSTORM); }

bool ShouldElementalBlast(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ELEMENTAL_BLAST)) return false;
    if (!ctx.bot.is_ready(ELEMENTAL_BLAST)) return false;
    return Maelstrom(ctx) >= 90;
}
void DoElementalBlast(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ELEMENTAL_BLAST, ctx.bot.victim());
}

bool ShouldEarthquake(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(EARTHQUAKE)) return false;
    if (!ctx.bot.is_ready(EARTHQUAKE)) return false;
    if (Maelstrom(ctx) < 60) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(20.0f) >= 3;
}
void DoEarthquake(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(EARTHQUAKE, v->x, v->y, v->z);
    else
        e.cast(EARTHQUAKE, ctx.bot.victim());
}

bool ShouldEarthShock(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(EARTH_SHOCK)) return false;
    if (!ctx.bot.is_ready(EARTH_SHOCK)) return false;
    return Maelstrom(ctx) >= 60;
}
void DoEarthShock(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(EARTH_SHOCK, ctx.bot.victim());
}

bool ShouldChainLightning(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CHAIN_LIGHTNING)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(CHAIN_LIGHTNING)) return false;
    // 3+ targets matches modern Elemental AoE breakpoint — at 2 targets
    // Lightning Bolt + spender is comparable damage and generates more
    // Maelstrom per GCD. owner-flagged aoe_preference still overrides.
    return ctx.aoe_preference || ctx.bot.enemies_within(20.0f) >= 3;
}
void DoChainLightning(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CHAIN_LIGHTNING, ctx.bot.victim());
}

bool ShouldLightningBolt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(LIGHTNING_BOLT)) return false;
    // Hard-cast — bots in motion need an instant alternative (Frost Shock).
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(LIGHTNING_BOLT)) return false;
    return true;
}
void DoLightningBolt(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LIGHTNING_BOLT, ctx.bot.victim());
}

bool AlwaysAlive(ApPredicateContext const& ctx) { return ctx.bot.is_alive(); }
void DoNothing(ApPredicateContext const&, BotIntentEmitter&) {}

// Rule ORDER follows the spec:
// Astral Shift (panic ≤40%) → Healing Surge self (≤50%) → Wind Shear
// → Lava Surge instant Lava Burst → Flame Shock refresh → Stormkeeper
// → Chain Lightning (AoE 3+) → Earthquake (AoE, Maelstrom 60+) →
// Earth Shock (ST, Maelstrom 60+) → Lava Burst hardcast → Lightning
// Bolt filler → Idle. Survival, utility, dispel, OOC-rez, and major
// cooldowns slot between the safety floor and the damage chain.
ApRule const kRules[] = {
    { ShouldAncestralSpirit,    DoAncestralSpirit,    "Ancestral Spirit (rez OOC)" },
    { ShouldEarthShield,        DoEarthShield,        "Earth Shield (self)"        },
    { ShouldAstralShift,        DoAstralShift,        "Astral Shift (<=40%)"       },
    { ShouldStoneBulwark,       DoStoneBulwark,       "Stone Bulwark Totem"        },
    { ShouldHealingSurgeSelf,   DoHealingSurgeSelf,   "Healing Surge (<=40% self)" },
    { ShouldThunderstorm,       DoThunderstorm,       "Thunderstorm (panic peel)"  },
    { ShouldWindShear,          DoWindShear,          "Wind Shear (interrupt)"     },
    { ShouldCapacitorTotem,     DoCapacitorTotem,     "Capacitor Totem (3+ AoE)"   },
    { ShouldHex,                DoHex,                "Hex (off-target CC)"        },
    { ShouldPurge,              DoPurge,              "Purge (Magic dispel)"       },
    { ShouldCleanseSpirit,      DoCleanseSpirit,      "Cleanse Spirit"             },
    { ShouldSpiritLinkTotem,    DoSpiritLinkTotem,    "Spirit Link Totem"          },
    { ShouldHealingStreamTotem, DoHealingStreamTotem, "Healing Stream Totem"       },
    { ShouldSpiritwalkerGrace,  DoSpiritwalkerGrace,  "Spiritwalker's Grace"       },
    { ShouldBloodlust,          DoBloodlust,          "Bloodlust/Heroism (boss)"   },
    { ShouldFireElemental,      DoFireElemental,      "Fire Elemental"             },
    { ShouldStormElemental,     DoStormElemental,     "Storm Elemental"            },
    { ShouldAscendance,         DoAscendance,         "Ascendance"                 },
    { ShouldLiquidMagmaTotem,   DoLiquidMagmaTotem,   "Liquid Magma Totem"         },
    { ShouldLavaBurstSurgeProc, DoLavaBurst,          "Lava Burst (Lava Surge proc)"},
    { ShouldFlameShock,         DoFlameShock,         "Flame Shock (refresh)"      },
    { ShouldStormkeeper,        DoStormkeeper,        "Stormkeeper"                },
    { ShouldFrostShock,         DoFrostShock,         "Frost Shock (Icefury)"      },
    { ShouldIcefury,            DoIcefury,            "Icefury"                    },
    { ShouldElementalBlast,     DoElementalBlast,     "Elemental Blast (90 maelstr)"},
    { ShouldChainLightning,     DoChainLightning,     "Chain Lightning (3+ AoE)"   },
    { ShouldEarthquake,         DoEarthquake,         "Earthquake (3+ AoE)"        },
    { ShouldEarthShock,         DoEarthShock,         "Earth Shock (60 maelstr)"   },
    { ShouldLavaBurstHardcast,  DoLavaBurst,          "Lava Burst (hardcast)"      },
    { ShouldLightningBolt,      DoLightningBolt,      "Lightning Bolt (filler)"    },
    { AlwaysAlive,              DoNothing,            "Idle"                       },
};

} // anonymous

void RegisterApl_Shaman_Elemental()
{
    constexpr uint32 SPEC_SHAMAN_ELEMENTAL = 262;
    RegisterRotation(CLASS_SHAMAN, SPEC_SHAMAN_ELEMENTAL, ApRotation{kRules});
}

} // namespace Playerbot::Combat
