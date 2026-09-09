// Restoration Shaman - WoW 12.1.0.69587 (Midnight) enterprise rotation.
// Reactive healer with strong group tools (Chain Heal, Healing Tide Totem,
// Spirit Link Totem, Healing Rain / Surging Totem in the Totemic builds),
// Riptide HoT priming chained heals, Unleash Life amplifying the next heal,
// and Earth Shield on the tank. Healing Wave replaces Healing Surge for
// Restoration in 12.1 (spec spell override); Nature's Swiftness makes the
// emergency cast instant. Major CDs: Ascendance (raid build) / Healing
// Tide Totem (M+ build), Spirit Link Totem (HP redistribute). Self
// maintenance: Water Shield (mana regen), Earthliving Weapon and
// Tidecaller's Guard imbues, Skyfury group buff.
//
// Survival: Astral Shift, Earth Elemental (panic taunt / Primordial Bond).
// CC / utility: Wind Shear, Capacitor Totem, Hex, Tremor Totem (anti-fear),
// Poison Cleansing Totem, Spiritwalker's Grace (cast while moving),
// Ancestral Spirit / Ancestral Vision (OOC rez). Dispel: Purify Spirit
// (Magic; +Curse with Improved Purify Spirit), Cleanse Spirit (Curse
// fallback before Purify is learned).
//
// Validated spell IDs (WoW 12.1.0.69587):
//   8004   Healing Surge       | 77472  Healing Wave        | 61295  Riptide
//   1064   Chain Heal          | 73920  Healing Rain        | 444995 Surging Totem
//   73685  Unleash Life        | 378081 Nature's Swiftness  | 108280 Healing Tide Totem
//   98008  Spirit Link Totem   | 114052 Ascendance (resto)  | 974    Earth Shield
//   108271 Astral Shift        | 5394   Healing Stream Totem| 383013 Poison Cleansing Totem
//   77130  Purify Spirit       | 440012 Cleanse Spirit      | 52127  Water Shield
//   382021 Earthliving Weapon  | 457481 Tidecaller's Guard  | 462854 Skyfury
//   57994  Wind Shear          | 79206  Spiritwalker's Grace| 192058 Capacitor Totem
//   51514  Hex                 | 370    Purge               | 8143   Tremor Totem
//   198103 Earth Elemental     | 2008   Ancestral Spirit    | 212048 Ancestral Vision
//   188196 Lightning Bolt      | 188443 Chain Lightning     | 470411 Flame Shock
//   51505  Lava Burst          | 2825   Bloodlust           | 32182  Heroism
//   Aura-only: 188389 Flame Shock DoT (legacy debuff row) | 382022 Earthliving imbue
//              457496 Tidecaller's Guard imbue | 57724/80354/95809/264689 sated
//   Passive gates: 383016 Improved Purify Spirit | 392915/392916 Healing Stream Totem
//                  talent rows (teach 5394)
//
// Skipped spells (and why):
//   16191  Mana Tide Totem      - removed from the Restoration tree in 12.1
//   108270 Stone Bulwark Totem  - removed from the class tree in 12.1
//   157153 Cloudburst Totem     - id no longer exists in 12.1 SpellName
//   197995 Wellspring           - removed from the Restoration tree in 12.1
//   198838 Earthen Wall Totem   - removed from the Restoration tree in 12.1
//   77762  Lava Surge           - Elemental-only proc; Restoration Lava Burst is hardcast
//   207778 Downpour             - granted by the 462486 talent, not in the curated builds
//   108287 Totemic Projection / 192063 Gust of Wind / 192077 Wind Rush Totem -
//                                 positioning utility the bot cannot aim
//   20608  Reincarnation        - passive out-of-combat death recovery, not APL
//   1229376 Single-Button Assistant - client convenience macro, not a rotation ability
//   MECH_FEAR / MECH_SLEEP / MECH_CHARM below are Mechanics ids, not spells

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

// ---- Spell IDs (WoW 12.1.0.69587, validated against SpellName.csv) ----
constexpr uint32 HEALING_SURGE          = 8004;        // overridden by Healing Wave at L10
constexpr uint32 HEALING_WAVE           = 77472;       // spec spell, overrides 8004
constexpr uint32 RIPTIDE                = 61295;
constexpr uint32 CHAIN_HEAL             = 1064;
constexpr uint32 HEALING_RAIN           = 73920;
constexpr uint32 SURGING_TOTEM          = 444995;      // Totemic hero: replaces Healing Rain
constexpr uint32 UNLEASH_LIFE           = 73685;       // [R][M] instant heal + next-heal amp
constexpr uint32 NATURES_SWIFTNESS      = 378081;      // [R][M] instant next Nature heal
constexpr uint32 HEALING_TIDE_TOTEM     = 108280;      // [M] choice node vs Ascendance
constexpr uint32 SPIRIT_LINK_TOTEM      = 98008;
constexpr uint32 EARTH_SHIELD           = 974;
constexpr uint32 ASTRAL_SHIFT           = 108271;
// Healing Stream Totem: the talent rows 392915 (class) / 392916 (spec)
// teach the castable 5394. Cast 5394; accept any of the ids in the book.
constexpr uint32 HEALING_STREAM_TOTEM   = 5394;
constexpr uint32 HEALING_STREAM_TALENT  = 392915;
constexpr uint32 HEALING_STREAM_TALENT2 = 392916;
constexpr uint32 POISON_CLEANSING_TOTEM = 383013;      // [R][M]
constexpr uint32 ASCENDANCE_RESTO       = 114052;      // [R] 15s burst
constexpr uint32 PURIFY_SPIRIT          = 77130;       // Magic (+Curse with 383016)
constexpr uint32 IMPROVED_PURIFY_SPIRIT = 383016;      // [M] passive: Purify removes Curses
constexpr uint32 CLEANSE_SPIRIT         = 440012;      // Curse-only fallback
constexpr uint32 WATER_SHIELD           = 52127;       // self mana-regen buff
constexpr uint32 EARTHLIVING_WEAPON     = 382021;      // [R][M] imbue
constexpr uint32 EARTHLIVING_AURA       = 382022;      // aura-only
constexpr uint32 TIDECALLERS_GUARD      = 457481;      // Totemic shield imbue (taught by 445033)
constexpr uint32 TIDECALLERS_GUARD_AURA = 457496;      // aura-only
constexpr uint32 SKYFURY                = 462854;      // group Mastery buff (L16)
constexpr uint32 WIND_SHEAR             = 57994;
constexpr uint32 SPIRITWALKER_GRACE     = 79206;
constexpr uint32 CAPACITOR_TOTEM        = 192058;
constexpr uint32 HEX                    = 51514;
constexpr uint32 PURGE                  = 370;
constexpr uint32 ANCESTRAL_SPIRIT       = 2008;
constexpr uint32 ANCESTRAL_VISION       = 212048;      // spec spell: mass OOC rez
constexpr uint32 EARTH_ELEMENTAL        = 198103;
constexpr uint32 TREMOR_TOTEM           = 8143;
constexpr uint32 LIGHTNING_BOLT         = 188196;
constexpr uint32 CHAIN_LIGHTNING        = 188443;      // [R][M] AoE DPS filler
// Flame Shock was renumbered in Midnight: 470411 is the castable (its
// description aliases 188389). The legacy row may still carry the DoT.
constexpr uint32 FLAME_SHOCK            = 470411;
constexpr uint32 FLAME_SHOCK_DOT_LEGACY = 188389;      // aura-only
constexpr uint32 LAVA_BURST             = 51505;
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
    // "my own HP <= below_pct" - the 92% GroupTopped gates then froze ALL
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

// Purify Spirit removes Magic; Curses only with Improved Purify Spirit
// (383016, [M]). Without it a Curse is not dispellable by Restoration at
// all (Cleanse Spirit is overridden once Purify is learned).
bool PurifyHandlesCurse(ApPredicateContext const& ctx)
{
    return ctx.bot.knows_spell(IMPROVED_PURIFY_SPIRIT);
}

GroupMemberSummary const* DispelTarget(ApPredicateContext const& ctx)
{
    const bool curse = PurifyHandlesCurse(ctx);
    if (curse)
        return DispelTargetWithPriority(ctx, [](GroupSnapshotView const& g)
            -> GroupMemberSummary const*
        {
            if (auto const* m = g.dispel_candidate(DispelType::Magic)) return m;
            if (auto const* m = g.dispel_candidate(DispelType::Curse)) return m;
            return nullptr;
        });
    return DispelTargetWithPriority(ctx, [](GroupSnapshotView const& g)
        -> GroupMemberSummary const* { return g.dispel_candidate(DispelType::Magic); });
}

bool SelfNeedsDispel(ApPredicateContext const& ctx)
{
    if (ctx.bot.self_dispellable(DispelType::Magic)) return true;
    return PurifyHandlesCurse(ctx) && ctx.bot.self_dispellable(DispelType::Curse);
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

// Ancestral Vision (212048, spec spell) - 10s mass rez. Preferred over the
// single-target Ancestral Spirit when 2+ members are dead on this map.
int DeadMemberCount(ApPredicateContext const& ctx)
{
    auto const* members = ctx.group.members();
    if (!members) return 0;
    int n = 0;
    for (auto const& m : *members)
        if (m.online && m.hp <= 0) ++n;
    return n;
}
bool ShouldAncestralVision(ApPredicateContext const& ctx)
{
    if (ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ANCESTRAL_VISION)) return false;
    if (ctx.group.dead_member(ctx.bot.map_id()) == nullptr) return false;
    return DeadMemberCount(ctx) >= 2;
}
void DoAncestralVision(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ANCESTRAL_VISION); }

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

// Earth Elemental (198103, [R]) - with Primordial Bond ([R]) it is a max-HP
// buff for the healer instead of a taunt pet, so it fires on the bot's own
// pressure as well as on a collapsing tank.
bool ShouldEarthElemental(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(EARTH_ELEMENTAL)) return false;
    if (!ctx.bot.is_ready(EARTH_ELEMENTAL)) return false;
    if (ctx.bot.hp_pct() <= 45 && ctx.bot.fightable_attackers_count() >= 1) return true;
    auto const* tank = ctx.group.tank();
    if (tank && tank->online && tank->hp > 0 && (tank->hp * 100) / tank->max_hp > 30) return false;
    return ctx.bot.attackers_count() >= 1 || (tank && (tank->hp * 100) / tank->max_hp <= 30);
}
void DoEarthElemental(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(EARTH_ELEMENTAL); }

// Poison Cleansing Totem (383013, [R][M]) - pulses Poison removal on the
// group for 6s. Drop it when anyone (or the bot) carries a Poison.
bool ShouldPoisonCleansingTotem(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(POISON_CLEANSING_TOTEM)) return false;
    if (!ctx.bot.is_ready(POISON_CLEANSING_TOTEM)) return false;
    if (ctx.bot.self_dispellable(DispelType::Poison)) return true;
    return ctx.group.dispel_candidate(DispelType::Poison) != nullptr;
}
void DoPoisonCleansingTotem(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(POISON_CLEANSING_TOTEM); }

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

// Cleanse Spirit (440012) - modern Resto fallback when Purify Spirit isn't
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

// Water Shield - 52127. Self-buff that restores mana on hit, refreshes
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

// Skyfury (462854) - group Mastery + extra-attack buff, 1h duration. Keep
// it up like a Battle Shout; the party/raid inherits it from the self cast.
bool ShouldSkyfury(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SKYFURY)) return false;
    return !ctx.bot.has_aura(SKYFURY);
}
void DoSkyfury(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SKYFURY, ctx.bot.raw().guid);
}

// Imbues - Earthliving Weapon (382021 -> aura 382022, [R][M]) on the
// weapon, Tidecaller's Guard (457481 -> aura 457496, Totemic hero) on the
// shield. Both are 1h self-buffs.
bool ShouldEarthlivingWeapon(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(EARTHLIVING_WEAPON)) return false;
    return !ctx.bot.has_aura(EARTHLIVING_AURA);
}
void DoEarthlivingWeapon(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(EARTHLIVING_WEAPON); }

bool ShouldTidecallersGuard(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(TIDECALLERS_GUARD)) return false;
    return !ctx.bot.has_aura(TIDECALLERS_GUARD_AURA);
}
void DoTidecallersGuard(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TIDECALLERS_GUARD); }

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
// See ApCrowdControl.h - replaced the old nearby_enemies.size()>=2 + has_aura
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

// Purge (370, [M] class talent) - strip a Magic buff off the current
// enemy when the group is not in need of healing.
bool ShouldPurge(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(PURGE)) return false;
    if (ctx.bot.victim().IsEmpty()) return false;
    if (!GroupTopped(ctx)) return false;
    return ctx.bot.target_dispellable(Playerbot::DispelType::Magic);
}
void DoPurge(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(PURGE, ctx.bot.victim());
}

// ---- Shield maintenance ----
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

bool KnowsHealingStreamTotem(ApPredicateContext const& ctx)
{
    return ctx.bot.knows_spell(HEALING_STREAM_TOTEM)
        || ctx.bot.knows_spell(HEALING_STREAM_TALENT)
        || ctx.bot.knows_spell(HEALING_STREAM_TALENT2);
}
bool ShouldHealingStreamTotem(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!KnowsHealingStreamTotem(ctx)) return false;
    if (!ctx.bot.is_ready(HEALING_STREAM_TOTEM)) return false;
    return !ctx.bot.has_aura(HEALING_STREAM_TOTEM);
}
void DoHealingStreamTotem(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HEALING_STREAM_TOTEM); }

// ---- AoE / spike heal ----
// Healing Rain (73920) is replaced by Surging Totem (444995) in the Totemic
// hero builds (both curated Restoration builds). Two-branch so bots with
// and without the hero tree both drop their ground heal.
uint32 PickGroundHeal(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(SURGING_TOTEM)) return SURGING_TOTEM;
    if (ctx.bot.knows_spell(HEALING_RAIN)) return HEALING_RAIN;
    return 0;
}
bool ShouldHealingRain(ApPredicateContext const& ctx)
{
    const uint32 sid = PickGroundHeal(ctx);
    if (!sid || !ctx.bot.is_ready(sid)) return false;
    return WoundedFriendCount(ctx, 85) >= 3;
}
void DoHealingRain(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const uint32 sid = PickGroundHeal(ctx);
    if (!sid) return;
    float bx, by, bz;
    ctx.bot.position(bx, by, bz);
    e.cast_at(sid, bx, by, bz);
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

// Nature's Swiftness (378081, [R][M]) - next Nature heal instant and free.
// Pop it right before the emergency direct heal so the 2s Healing Wave
// lands immediately on a target at or below 35%.
bool ShouldNaturesSwiftness(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(NATURES_SWIFTNESS)) return false;
    if (!ctx.bot.is_ready(NATURES_SWIFTNESS)) return false;
    if (ctx.bot.has_aura(NATURES_SWIFTNESS)) return false;
    if (!ctx.bot.knows_spell(HEALING_WAVE) && !ctx.bot.knows_spell(HEALING_SURGE)) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 35;
}
void DoNaturesSwiftness(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(NATURES_SWIFTNESS); }

// Emergency direct heal. Healing Wave (77472) overrides Healing Surge
// (8004) for Restoration in 12.1, so the spec normally has only the wave;
// Healing Surge remains the pre-L10 / fallback id.
uint32 PickDirectHeal(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(HEALING_WAVE)) return HEALING_WAVE;
    if (ctx.bot.knows_spell(HEALING_SURGE)) return HEALING_SURGE;
    return 0;
}
bool ShouldHealingSurge(ApPredicateContext const& ctx)
{
    if (!PickDirectHeal(ctx)) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 50;
}
void DoHealingSurge(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (const uint32 sid = PickDirectHeal(ctx)) e.cast(sid, LowestFriendOrSelf(ctx).guid);
}

// Unleash Life (73685, [R][M]) - instant heal that also amplifies the next
// Riptide / Chain Heal / Healing Wave. Fire it on cooldown ahead of the
// Riptide rung whenever someone actually needs the follow-up.
bool ShouldUnleashLife(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(UNLEASH_LIFE)) return false;
    if (!ctx.bot.is_ready(UNLEASH_LIFE)) return false;
    if (ctx.bot.has_aura(UNLEASH_LIFE)) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 80;
}
void DoUnleashLife(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(UNLEASH_LIFE, LowestFriendOrSelf(ctx).guid);
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
    // The 12.1 castable is 470411; the DoT may still be tracked under the
    // legacy 188389 row, so accept either before re-applying.
    AuraEntry const* a = ctx.bot.find_aura(FLAME_SHOCK, ctx.bot.victim());
    if (!a) a = ctx.bot.find_aura(FLAME_SHOCK_DOT_LEGACY, ctx.bot.victim());
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
    // Restoration has no Lava Surge proc in 12.1: Lava Burst is a 2s hard
    // cast, so only start it while stationary.
    return !ctx.bot.is_moving() || ctx.bot.can_cast_while_moving(LAVA_BURST);
}
void DoLavaBurst(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LAVA_BURST, ctx.bot.victim());
}

// Chain Lightning (188443, [R][M] class talent) - AoE DPS filler once the
// group is topped and 3+ enemies are stacked.
bool ShouldChainLightningFiller(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!GroupTopped(ctx)) return false;
    if (!ctx.bot.knows_spell(CHAIN_LIGHTNING)) return false;
    if (ctx.bot.victim().IsEmpty()) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(CHAIN_LIGHTNING)) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(20.0f) >= 3;
}
void DoChainLightning(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CHAIN_LIGHTNING, ctx.bot.victim());
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

// Cast-swap shim - Resto Shaman slow heals. Healing Wave 2.5s,
// Healing Surge 1.5s, Chain Heal 2.5s. See ApHealHelpers.h.
bool ShouldCancelHealForSwap(ApPredicateContext const& ctx)
{
    return ShouldCancelHealForSwapImpl(ctx,
        { HEALING_WAVE, HEALING_SURGE, CHAIN_HEAL });
}

// Rule ORDER (12.1 Restoration): cancel-for-swap -> OOC rez (Ancestral
// Vision for 2+ dead, else Ancestral Spirit) -> Bloodlust -> tank Earth
// Shield -> self buffs (Skyfury, Water Shield, Earthliving / Tidecaller's
// imbues) -> Astral Shift -> Spiritwalker's Grace -> interrupt / CC ->
// Earth Elemental / Tremor / Poison Cleansing -> raid CDs (Ascendance,
// Spirit Link, Healing Tide) -> dispel (Purify / Cleanse) -> Nature's
// Swiftness + emergency direct heal (<=50%) -> Unleash Life -> Riptide ->
// Healing Stream -> Healing Wave (filler) -> Healing Rain / Surging Totem
// (group spike) -> Chain Heal (multi-spike) -> DPS filler when topped
// (Flame Shock, Lava Burst, Chain Lightning, Lightning Bolt) -> Idle.
ApRule const kRules[] = {
    { ShouldCancelHealForSwap,  DoCancelHealForSwap,  "Cancel heal - swap to lower target" },
    { ShouldAncestralVision,    DoAncestralVision,    "Ancestral Vision (2+ dead OOC)" },
    { ShouldAncestralSpirit,    DoAncestralSpirit,    "Ancestral Spirit (rez OOC)"   },
    { ShouldBloodlust,          DoBloodlust,          "Bloodlust/Heroism (boss)"     },
    { ShouldEarthShield,        DoEarthShield,        "Earth Shield (tank buff)"     },
    { ShouldSkyfury,            DoSkyfury,            "Skyfury (group buff)"         },
    { ShouldWaterShield,        DoWaterShield,        "Water Shield (self-buff)"     },
    { ShouldEarthlivingWeapon,  DoEarthlivingWeapon,  "Earthliving Weapon (imbue)"   },
    { ShouldTidecallersGuard,   DoTidecallersGuard,   "Tidecaller's Guard (imbue)"   },
    { ShouldAstralShift,        DoAstralShift,        "Astral Shift (<=50%)"         },
    { ShouldSpiritwalkerGrace,  DoSpiritwalkerGrace,  "Spiritwalker's Grace"         },
    { ShouldWindShear,          DoWindShear,          "Wind Shear (interrupt)"       },
    { ShouldCapacitorTotem,     DoCapacitorTotem,     "Capacitor Totem (3+ AoE)"     },
    { ShouldHex,                DoHex,                "Hex (off-target CC)"          },
    { ShouldPurge,              DoPurge,              "Purge (Magic, group topped)"  },
    { ShouldEarthElemental,     DoEarthElemental,     "Earth Elemental (panic)"      },
    { ShouldTremorTotem,        DoTremorTotem,        "Tremor Totem (anti-fear)"     },
    { ShouldPoisonCleansingTotem,DoPoisonCleansingTotem,"Poison Cleansing Totem"     },
    { ShouldAscendance,         DoAscendance,         "Ascendance (3+ wounded)"      },
    { ShouldSpiritLinkTotem,    DoSpiritLinkTotem,    "Spirit Link Totem (emergency)" },
    { ShouldHealingTideTotem,   DoHealingTideTotem,   "Healing Tide Totem (panic)"   },
    { ShouldPurifySpirit,       DoPurifySpirit,       "Purify Spirit (dispel)"       },
    { ShouldCleanseSpirit,      DoCleanseSpirit,      "Cleanse Spirit (curse, fallback)" },
    { ShouldNaturesSwiftness,   DoNaturesSwiftness,   "Nature's Swiftness (<=35%)"   },
    { ShouldHealingSurge,       DoHealingSurge,       "Healing Wave/Surge (<=50%)"   },
    { ShouldUnleashLife,        DoUnleashLife,        "Unleash Life (<=80%)"         },
    { ShouldRiptide,            DoRiptide,            "Riptide (HoT spam priority)"  },
    { ShouldHealingStreamTotem, DoHealingStreamTotem, "Healing Stream Totem"         },
    { ShouldHealingWave,        DoHealingWave,        "Healing Wave (filler)"        },
    { ShouldHealingRain,        DoHealingRain,        "Healing Rain/Surging (3+ 85%)"},
    { ShouldChainHeal,          DoChainHeal,          "Chain Heal (2+ at 75%)"       },
    { ShouldFlameShockFiller,   DoFlameShock,         "Flame Shock (DPS filler)"     },
    { ShouldLavaBurstFiller,    DoLavaBurst,          "Lava Burst (DPS filler)"      },
    { ShouldChainLightningFiller,DoChainLightning,    "Chain Lightning (3+ filler)"  },
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
