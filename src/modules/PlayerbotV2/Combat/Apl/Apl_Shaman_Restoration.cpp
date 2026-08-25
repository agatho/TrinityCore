// Restoration Shaman - WoW 12.0 enterprise rotation. Reactive healer with
// strong group tools (Chain Heal, Healing Tide Totem, Spirit Link Totem,
// Healing Rain), Riptide HoT priming chained heals, and Earth Shield on
// the tank. Major CDs: Healing Tide (3+ wounded), Spirit Link Totem (HP
// redistribute), Ascendance (talent — heals at full effect), Cloudburst
// Totem (stored heal), Wellspring (cone heal), Earthen Wall Totem (DR
// soak), Mana Tide Totem. Self maintenance: Water Shield (mana regen).
//
// Survival: Astral Shift, Stone Bulwark Totem, Earth Shield self.
// CC / utility: Wind Shear, Capacitor Totem, Hex, Tremor Totem (anti-fear),
// Wind Rush Totem (group sprint), Spiritwalker's Grace (cast while moving),
// Earth Elemental (panic taunt), Ancestral Spirit (combat rez via talent).
// Dispel: Purify Spirit (Magic+Curse), Cleanse Spirit (Curse fallback).
//
// Validated IDs (SpellName.csv 2026-05):
//   8004   Healing Surge        77472  Healing Wave      61295  Riptide
//   1064   Chain Heal           73920  Healing Rain      108280 Healing Tide
//   98008  Spirit Link Totem    16191  Mana Tide Totem   974    Earth Shield
//   108271 Astral Shift         108270 Stone Bulwark     5394   Healing Stream
//   157153 Cloudburst Totem     197995 Wellspring        198838 Earthen Wall
//   114052 Ascendance (resto)   77130  Purify Spirit     440012 Cleanse Spirit
//   52127  Water Shield         57994  Wind Shear        79206  Spiritwalker's Grace
//   192058 Capacitor Totem      51514  Hex               370    Purge
//   2008   Ancestral Spirit     198103 Earth Elemental   8143   Tremor Totem
//   192077 Wind Rush Totem      188196 Lightning Bolt    188389 Flame Shock
//   51505  Lava Burst           77762  Lava Surge (proc) 2825   Bloodlust
//   32182  Heroism
//
// Skipped spells (and why):
//   • 212048 Ancestral Vision (passive, no APL — talent that gives Mastery
//     ramp on critical heals)
//   • Reincarnation (21169 passive — out-of-combat death recovery)
//   • Other resto/shaman passives covered by spell auras (Mastery, Tidal
//     Waves proc, etc) — only manifest in heal-pick spreads, not as
//     individual cast rules.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "ApCrowdControl.h"
#include "ApDispelHelpers.h"
#include "ApHealHelpers.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated) ----
constexpr uint32 HEALING_SURGE          = 8004;
constexpr uint32 HEALING_WAVE           = 77472;
constexpr uint32 RIPTIDE                = 61295;
constexpr uint32 CHAIN_HEAL             = 1064;
constexpr uint32 HEALING_RAIN           = 73920;
constexpr uint32 HEALING_TIDE_TOTEM     = 108280;
constexpr uint32 SPIRIT_LINK_TOTEM      = 98008;
constexpr uint32 MANA_TIDE_TOTEM        = 16191;
constexpr uint32 EARTH_SHIELD           = 974;
constexpr uint32 ASTRAL_SHIFT           = 108271;
constexpr uint32 STONE_BULWARK_TOTEM    = 108270;
constexpr uint32 HEALING_STREAM_TOTEM   = 5394;
constexpr uint32 CLOUDBURST_TOTEM       = 157153;       // talent
constexpr uint32 WELLSPRING             = 197995;       // talent — cone heal
constexpr uint32 EARTHEN_WALL_TOTEM     = 198838;       // talent
constexpr uint32 ASCENDANCE_RESTO       = 114052;       // 15s burst
constexpr uint32 PURIFY_SPIRIT          = 77130;       // Magic+Curse (Resto)
constexpr uint32 CLEANSE_SPIRIT         = 440012;      // Curse-only fallback
constexpr uint32 WATER_SHIELD           = 52127;       // self mana-regen buff
constexpr uint32 WIND_SHEAR             = 57994;
constexpr uint32 SPIRITWALKER_GRACE     = 79206;
constexpr uint32 CAPACITOR_TOTEM        = 192058;
constexpr uint32 HEX                    = 51514;
constexpr uint32 PURGE                  = 370;
constexpr uint32 ANCESTRAL_SPIRIT       = 2008;
constexpr uint32 EARTH_ELEMENTAL        = 198103;
constexpr uint32 TREMOR_TOTEM           = 8143;
constexpr uint32 WIND_RUSH_TOTEM        = 192077;
constexpr uint32 LIGHTNING_BOLT         = 188196;
constexpr uint32 FLAME_SHOCK            = 188389;
constexpr uint32 LAVA_BURST             = 51505;
constexpr uint32 LAVA_SURGE             = 77762;
constexpr uint32 BLOODLUST              = 2825;
constexpr uint32 HEROISM                = 32182;
constexpr uint32 SATED_DEBUFF           = 57724;
constexpr uint32 TEMPORAL_DISPL_DEBUFF  = 80354;
constexpr uint32 INSANITY_HUNTER_DEBUFF = 95809;
constexpr uint32 FATIGUED_DEBUFF        = 264689;

// ---- Helpers ----
struct HealTarget
{
    ObjectGuid guid;
    int32      hp_pct;
};

HealTarget LowestFriendOrSelf(ApPredicateContext const& ctx)
{
    HealTarget t{ ctx.bot.raw().guid, ctx.bot.hp_pct() };
    if (auto const* low = ctx.group.heal_assignment(ctx.bot.raw().guid, ctx.bot.map_id(), ctx.bot.raw().position.x, ctx.bot.raw().position.y, ctx.bot.raw().position.z, 45.0f)) {
        if (low->online && low->max_hp > 0) {
            const int32 pct = (low->hp * 100) / low->max_hp;
            if (pct < t.hp_pct) { t.guid = low->guid; t.hp_pct = pct; }
        }
    }
    return t;
}

int WoundedFriendCount(ApPredicateContext const& ctx, int below_pct)
{
    int n = 0;
    auto const* members = ctx.group.members();
    // SOLO (audit B22): ungrouped, "wounded friend" used to collapse to
    // "my own HP <= below_pct" — the 92% GroupTopped gates then froze ALL
    // damage the moment a questing healer took two melee hits, degenerating
    // solo healer-spec leveling into heal-regen-nuke loops (3-10x kill
    // time). Cap the solo threshold at a 45% survival floor: topped-style
    // gates (92) stay open while merely scratched, true emergency heals
    // (<=45) keep their thresholds.
    if (!members) return ctx.bot.hp_pct() <= std::min(below_pct, 45) ? 1 : 0;
    for (auto const& m : *members) {
        if (!m.online || m.max_hp <= 0 || m.hp <= 0) continue;
        if ((m.hp * 100) / m.max_hp <= below_pct) ++n;
    }
    return n;
}

bool GroupTopped(ApPredicateContext const& ctx)
{
    return WoundedFriendCount(ctx, 92) == 0;
}

GroupMemberSummary const* DispelTarget(ApPredicateContext const& ctx)
{
    return DispelTargetWithPriority(ctx, [](GroupSnapshotView const& g)
        -> GroupMemberSummary const*
    {
        if (auto const* m = g.dispel_candidate(DispelType::Magic)) return m;
        if (auto const* m = g.dispel_candidate(DispelType::Curse)) return m;
        return nullptr;
    });
}

bool SelfNeedsDispel(ApPredicateContext const& ctx)
{
    return ctx.bot.self_dispellable(DispelType::Magic) || ctx.bot.self_dispellable(DispelType::Curse);
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

bool ShouldEarthElemental(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(EARTH_ELEMENTAL)) return false;
    if (!ctx.bot.is_ready(EARTH_ELEMENTAL)) return false;
    auto const* tank = ctx.group.tank();
    if (tank && tank->online && tank->hp > 0 && (tank->hp * 100) / tank->max_hp > 30) return false;
    return ctx.bot.attackers_count() >= 1 || (tank && (tank->hp * 100) / tank->max_hp <= 30);
}
void DoEarthElemental(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(EARTH_ELEMENTAL); }

bool ShouldTremorTotem(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(TREMOR_TOTEM)) return false;
    if (!ctx.bot.is_ready(TREMOR_TOTEM)) return false;
    // Tremor Totem pulses anti-fear/sleep/charm. Drop it when any group
    // member (or self) is currently affected by one of those mechanics.
    constexpr uint32 MECH_FEAR  = 5;
    constexpr uint32 MECH_SLEEP = 10;
    constexpr uint32 MECH_CHARM = 1;
    if (ctx.bot.has_mechanic(MECH_FEAR) ||
        ctx.bot.has_mechanic(MECH_SLEEP) ||
        ctx.bot.has_mechanic(MECH_CHARM)) return true;
    const uint32 map = ctx.bot.map_id();
    return ctx.group.group_has_mechanic(MECH_FEAR, map)  ||
           ctx.group.group_has_mechanic(MECH_SLEEP, map) ||
           ctx.group.group_has_mechanic(MECH_CHARM, map);
}
void DoTremorTotem(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TREMOR_TOTEM); }

// ---- Survival ----
bool ShouldAstralShift(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ASTRAL_SHIFT)) return false;
    if (!ctx.bot.is_ready(ASTRAL_SHIFT)) return false;
    return ctx.bot.hp_pct() <= 50;
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

bool ShouldSpiritwalkerGrace(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SPIRITWALKER_GRACE)) return false;
    if (!ctx.bot.is_ready(SPIRITWALKER_GRACE)) return false;
    return ctx.bot.is_moving() && WoundedFriendCount(ctx, 75) >= 1;
}
void DoSpiritwalkerGrace(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SPIRITWALKER_GRACE); }

// ---- Dispel / interrupt ----
bool ShouldPurifySpirit(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(PURIFY_SPIRIT)) return false;
    if (!ctx.bot.is_ready(PURIFY_SPIRIT)) return false;
    return DispelTarget(ctx) != nullptr || SelfNeedsDispel(ctx);
}
void DoPurifySpirit(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = DispelTarget(ctx)) { e.cast(PURIFY_SPIRIT, t->guid); return; }
    if (SelfNeedsDispel(ctx)) e.cast(PURIFY_SPIRIT, ctx.bot.raw().guid);
}

// Cleanse Spirit (440012) — modern Resto fallback when Purify Spirit isn't
// learned yet (pre-talent lock-in). Curse-only; Magic dispels still need
// Purify Spirit. Gate so we don't shadow Purify when both are known.
GroupMemberSummary const* CurseDispelTarget(ApPredicateContext const& ctx)
{
    return DispelTargetWithPriority(ctx, [](GroupSnapshotView const& g)
        -> GroupMemberSummary const* { return g.dispel_candidate(DispelType::Curse); });
}
bool ShouldCleanseSpirit(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(CLEANSE_SPIRIT)) return false;
    if (ctx.bot.knows_spell(PURIFY_SPIRIT)) return false;     // Purify supersedes
    if (!ctx.bot.is_ready(CLEANSE_SPIRIT)) return false;
    return CurseDispelTarget(ctx) != nullptr || ctx.bot.self_dispellable(DispelType::Curse);
}
void DoCleanseSpirit(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = CurseDispelTarget(ctx)) { e.cast(CLEANSE_SPIRIT, t->guid); return; }
    if (ctx.bot.self_dispellable(DispelType::Curse))
        e.cast(CLEANSE_SPIRIT, ctx.bot.raw().guid);
}

// Water Shield — 52127. Self-buff that restores mana on hit, refreshes
// every 60min. Resto's standard mana-regen blanket; should always be up
// out-of-combat-or-in. Cheap to maintain (instant, off-GCD when not
// already up).
bool ShouldWaterShield(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(WATER_SHIELD)) return false;
    return !ctx.bot.has_aura(WATER_SHIELD);
}
void DoWaterShield(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(WATER_SHIELD, ctx.bot.raw().guid);
}

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
    if (!ctx.bot.in_combat()) return false;
    return !PickOffTargetCC(ctx, HEX, ApInPvp(ctx)).IsEmpty();
}
void DoHex(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    ObjectGuid const t = PickOffTargetCC(ctx, HEX, ApInPvp(ctx));
    if (!t.IsEmpty()) e.cast(HEX, t);
}

// ---- Mana / shield maintenance ----
bool ShouldManaTideTotem(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(MANA_TIDE_TOTEM)) return false;
    if (!ctx.bot.is_ready(MANA_TIDE_TOTEM)) return false;
    if (ctx.bot.max_power(0) > 0 && ctx.bot.power_pct(0) <= 35) return true;
    if (auto const* m = ctx.group.lowest_mana_caster())
        if (m->max_mana > 0 && (m->mana * 100) / m->max_mana <= 35)
            return true;
    return false;
}
void DoManaTideTotem(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(MANA_TIDE_TOTEM); }

bool ShouldEarthShield(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(EARTH_SHIELD)) return false;
    GroupMemberSummary const* tank = ctx.group.tank();
    if (!tank || !tank->online) return false;
    AuraEntry const* a = ctx.bot.find_aura(EARTH_SHIELD, tank->guid);
    return !a || a->remaining.count() <= 30000;
}
void DoEarthShield(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* tank = ctx.group.tank())
        e.cast(EARTH_SHIELD, tank->guid);
}

// ---- Major CDs ----
bool ShouldAscendance(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(ASCENDANCE_RESTO)) return false;
    if (!ctx.bot.is_ready(ASCENDANCE_RESTO)) return false;
    return WoundedFriendCount(ctx, 60) >= 3;
}
void DoAscendance(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ASCENDANCE_RESTO); }

bool ShouldHealingTideTotem(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HEALING_TIDE_TOTEM)) return false;
    if (!ctx.bot.is_ready(HEALING_TIDE_TOTEM)) return false;
    return WoundedFriendCount(ctx, 60) >= 3;
}
void DoHealingTideTotem(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HEALING_TIDE_TOTEM); }

bool ShouldSpiritLinkTotem(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SPIRIT_LINK_TOTEM)) return false;
    if (!ctx.bot.is_ready(SPIRIT_LINK_TOTEM)) return false;
    return WoundedFriendCount(ctx, 30) >= 2;
}
void DoSpiritLinkTotem(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SPIRIT_LINK_TOTEM); }

bool ShouldEarthenWallTotem(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(EARTHEN_WALL_TOTEM)) return false;
    if (!ctx.bot.is_ready(EARTHEN_WALL_TOTEM)) return false;
    return WoundedFriendCount(ctx, 80) >= 2;
}
void DoEarthenWallTotem(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(EARTHEN_WALL_TOTEM); }

bool ShouldCloudburstTotem(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(CLOUDBURST_TOTEM)) return false;
    if (!ctx.bot.is_ready(CLOUDBURST_TOTEM)) return false;
    return WoundedFriendCount(ctx, 90) >= 2;
}
void DoCloudburstTotem(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CLOUDBURST_TOTEM); }

bool ShouldHealingStreamTotem(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(HEALING_STREAM_TOTEM)) return false;
    if (!ctx.bot.is_ready(HEALING_STREAM_TOTEM)) return false;
    return !ctx.bot.has_aura(HEALING_STREAM_TOTEM);
}
void DoHealingStreamTotem(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HEALING_STREAM_TOTEM); }

bool ShouldWellspring(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(WELLSPRING)) return false;
    if (!ctx.bot.is_ready(WELLSPRING)) return false;
    return WoundedFriendCount(ctx, 80) >= 3;
}
void DoWellspring(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    float bx, by, bz;
    ctx.bot.position(bx, by, bz);
    e.cast_at(WELLSPRING, bx, by, bz);
}

// ---- AoE / spike heal ----
bool ShouldHealingRain(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HEALING_RAIN)) return false;
    if (!ctx.bot.is_ready(HEALING_RAIN)) return false;
    return WoundedFriendCount(ctx, 85) >= 3;
}
void DoHealingRain(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    float bx, by, bz;
    ctx.bot.position(bx, by, bz);
    e.cast_at(HEALING_RAIN, bx, by, bz);
}

bool ShouldChainHeal(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(CHAIN_HEAL)) return false;
    return WoundedFriendCount(ctx, 75) >= 2;
}
void DoChainHeal(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CHAIN_HEAL, LowestFriendOrSelf(ctx).guid);
}

bool ShouldHealingSurge(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HEALING_SURGE)) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 50;
}
void DoHealingSurge(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HEALING_SURGE, LowestFriendOrSelf(ctx).guid);
}

bool ShouldRiptide(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(RIPTIDE)) return false;
    if (!ctx.bot.is_ready(RIPTIDE)) return false;
    HealTarget t = LowestFriendOrSelf(ctx);
    if (t.hp_pct >= 95) return false;
    AuraEntry const* a = ctx.bot.find_aura(RIPTIDE, t.guid);
    return !a || a->remaining.count() <= 3000;
}
void DoRiptide(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RIPTIDE, LowestFriendOrSelf(ctx).guid);
}

bool ShouldHealingWave(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HEALING_WAVE)) return false;
    if (LowestFriendOrSelf(ctx).hp_pct > 90) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(HEALING_WAVE)) return false;
    return true;
}
void DoHealingWave(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HEALING_WAVE, LowestFriendOrSelf(ctx).guid);
}

// ---- Offensive filler when group is topped (Mastery proc + Ancestral Awakening) ----
bool ShouldFlameShockFiller(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!GroupTopped(ctx)) return false;
    if (!ctx.bot.knows_spell(FLAME_SHOCK)) return false;
    if (ctx.bot.victim().IsEmpty()) return false;
    AuraEntry const* a = ctx.bot.find_aura(FLAME_SHOCK, ctx.bot.victim());
    return !a || a->remaining.count() <= 4000;
}
void DoFlameShock(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FLAME_SHOCK, ctx.bot.victim());
}

bool ShouldLavaBurstFiller(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!GroupTopped(ctx)) return false;
    if (!ctx.bot.knows_spell(LAVA_BURST)) return false;
    if (!ctx.bot.is_ready(LAVA_BURST)) return false;
    if (ctx.bot.victim().IsEmpty()) return false;
    // Lava Surge proc makes the next Lava Burst instant — fire on proc
    // regardless of movement. Otherwise only hard-cast while stationary
    // (2s cast, won't channel through movement).
    if (ctx.bot.has_aura(LAVA_SURGE)) return true;
    return !ctx.bot.is_moving();
}
void DoLavaBurst(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LAVA_BURST, ctx.bot.victim());
}

bool ShouldLightningBoltFiller(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!GroupTopped(ctx)) return false;
    if (!ctx.bot.knows_spell(LIGHTNING_BOLT)) return false;
    return !ctx.bot.victim().IsEmpty();
}
void DoLightningBolt(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LIGHTNING_BOLT, ctx.bot.victim());
}

bool AlwaysAlive(ApPredicateContext const& ctx) { return ctx.bot.is_alive(); }
void DoNothing(ApPredicateContext const&, BotIntentEmitter&) {}

// Cast-swap shim — Resto Shaman slow heals. Healing Wave 2.5s,
// Healing Surge 1.5s, Chain Heal 2.5s. See ApHealHelpers.h.
bool ShouldCancelHealForSwap(ApPredicateContext const& ctx)
{
    return ShouldCancelHealForSwapImpl(ctx,
        { HEALING_WAVE, HEALING_SURGE, CHAIN_HEAL });
}

// Rule ORDER (spec): Spirit Link Totem (raid emergency) → Healing Tide
// Totem (raid panic) → Earthen Wall Totem (panic) → Cleanse Spirit /
// Purify Spirit (dispel) → Water Shield (self-buff maintenance) →
// Riptide (HoT spam priority) → Cloudburst Totem prep → Healing Wave
// (filler) → Healing Rain (group spike) → Chain Heal (multi-spike) →
// AutoAttack / DPS filler when group is topped.
//
// Pre-heal safety: cancel-for-swap, OOC rez, Bloodlust pull window,
// tank Earth Shield, interrupt, AoE CC. Survival CDs (Astral Shift,
// Stone Bulwark) and Ascendance / Mana Tide fire on their own gates.
ApRule const kRules[] = {
    { ShouldCancelHealForSwap,  DoCancelHealForSwap,  "Cancel heal — swap to lower target" },
    { ShouldAncestralSpirit,    DoAncestralSpirit,    "Ancestral Spirit (rez OOC)"   },
    { ShouldBloodlust,          DoBloodlust,          "Bloodlust/Heroism (boss)"     },
    { ShouldEarthShield,        DoEarthShield,        "Earth Shield (tank buff)"     },
    { ShouldAstralShift,        DoAstralShift,        "Astral Shift (<=50%)"         },
    { ShouldStoneBulwark,       DoStoneBulwark,       "Stone Bulwark (<=70%)"        },
    { ShouldSpiritwalkerGrace,  DoSpiritwalkerGrace,  "Spiritwalker's Grace"         },
    { ShouldWindShear,          DoWindShear,          "Wind Shear (interrupt)"       },
    { ShouldCapacitorTotem,     DoCapacitorTotem,     "Capacitor Totem (3+ AoE)"     },
    { ShouldHex,                DoHex,                "Hex (off-target CC)"          },
    { ShouldEarthElemental,     DoEarthElemental,     "Earth Elemental (panic tank)" },
    { ShouldTremorTotem,        DoTremorTotem,        "Tremor Totem (anti-fear)"     },
    { ShouldManaTideTotem,      DoManaTideTotem,      "Mana Tide Totem"              },
    { ShouldAscendance,         DoAscendance,         "Ascendance (3+ wounded)"      },
    { ShouldSpiritLinkTotem,    DoSpiritLinkTotem,    "Spirit Link Totem (emergency)" },
    { ShouldHealingTideTotem,   DoHealingTideTotem,   "Healing Tide Totem (panic)"   },
    { ShouldEarthenWallTotem,   DoEarthenWallTotem,   "Earthen Wall Totem (panic)"   },
    { ShouldPurifySpirit,       DoPurifySpirit,       "Purify Spirit (dispel)"       },
    { ShouldCleanseSpirit,      DoCleanseSpirit,      "Cleanse Spirit (curse, fallback)" },
    { ShouldWaterShield,        DoWaterShield,        "Water Shield (self-buff)"     },
    { ShouldHealingSurge,       DoHealingSurge,       "Healing Surge (<=50%)"        },
    { ShouldRiptide,            DoRiptide,            "Riptide (HoT spam priority)"  },
    { ShouldCloudburstTotem,    DoCloudburstTotem,    "Cloudburst Totem (prep)"      },
    { ShouldHealingStreamTotem, DoHealingStreamTotem, "Healing Stream Totem"         },
    { ShouldWellspring,         DoWellspring,         "Wellspring (3+ at 80%)"       },
    { ShouldHealingWave,        DoHealingWave,        "Healing Wave (filler)"        },
    { ShouldHealingRain,        DoHealingRain,        "Healing Rain (3+ at 85%)"     },
    { ShouldChainHeal,          DoChainHeal,          "Chain Heal (2+ at 75%)"       },
    { ShouldFlameShockFiller,   DoFlameShock,         "Flame Shock (DPS filler)"     },
    { ShouldLavaBurstFiller,    DoLavaBurst,          "Lava Burst (DPS filler)"      },
    { ShouldLightningBoltFiller,DoLightningBolt,      "Lightning Bolt (DPS filler)"  },
    { AlwaysAlive,              DoNothing,            "Idle"                         },
};

} // anonymous

void RegisterApl_Shaman_Restoration()
{
    constexpr uint32 SPEC_SHAMAN_RESTORATION = 264;
    RegisterRotation(CLASS_SHAMAN, SPEC_SHAMAN_RESTORATION, ApRotation{kRules});
}

} // namespace Playerbot::Combat
