// Elemental Shaman - WoW 12.1.0.69587 (Midnight) enterprise rotation.
// Maelstrom-builder caster: Lava Burst (instant via Lava Surge proc),
// Elemental Blast (90 Maelstrom, curated builds) / Earth Shock (60, choice
// node) single-target spender, Earthquake AoE spender (61882 ground-target
// in the raid build, 462620 target-cast in the M+ build), Chain Lightning
// AoE filler, Tempest (Stormbringer proc that replaces Lightning Bolt).
// Major CDs: Stormkeeper, Ascendance (Flame Ascendant burst), Ancestral
// Swiftness (Farseer instant), Nature's Swiftness (instant Nature spell
// while moving). Group utility: Bloodlust/Heroism, Skyfury (group Mastery),
// Cleanse Spirit (Curse), Chain Heal (emergency group heal), Healing Stream
// Totem, Earth Elemental (panic taunt / Primordial Bond HP), Spiritwalker's
// Grace (cast while moving). CC: Wind Shear interrupt, Hex, Capacitor Totem
// (stun), Earthgrab / Earthbind Totem (root / slow), Thunderstorm (knockback
// panic peel). Survival: Astral Shift, Earth Shield, Lightning Shield
// (Elemental Orbit lets both shields sit on the bot), Healing Surge.
//
// Validated spell IDs (WoW 12.1.0.69587):
//   188196 Lightning Bolt      | 51505  Lava Burst         | 470411 Flame Shock
//   8042   Earth Shock         | 117014 Elemental Blast    | 191634 Stormkeeper
//   188443 Chain Lightning     | 61882  Earthquake (ground)| 462620 Earthquake (target)
//   196840 Frost Shock         | 51490  Thunderstorm       | 114050 Ascendance
//   470057 Voltaic Blaze       | 452201 Tempest            | 443454 Ancestral Swiftness
//   378081 Nature's Swiftness  | 79206  Spiritwalker's Gr. | 198103 Earth Elemental
//   2484   Earthbind Totem     | 51485  Earthgrab Totem    | 192058 Capacitor Totem
//   51514  Hex                 | 370    Purge              | 57994  Wind Shear
//   2825   Bloodlust           | 32182  Heroism            | 462854 Skyfury
//   108271 Astral Shift        | 974    Earth Shield       | 192106 Lightning Shield
//   5394   Healing Stream Totem| 8004   Healing Surge      | 1064   Chain Heal
//   51886  Cleanse Spirit      | 2008   Ancestral Spirit   | 318038 Flametongue Weapon
//   Aura-only: 77762 Lava Surge proc | 188389 Flame Shock DoT (legacy debuff row)
//              454015 Tempest ready  | 319778 Flametongue imbue | 57724/80354/95809/264689 sated
//   Passive gates: 383010 Elemental Orbit | 392915 Healing Stream Totem talent (teaches 5394)
//
// Skipped spells (and why):
//   210714 Icefury               - removed from the Elemental tree in 12.1 (Frost Shock is now
//                                  a plain instant / moving filler)
//   192222 Liquid Magma Totem    - removed from the Elemental tree in 12.1
//   198067 Fire Elemental / 192249 Storm Elemental - no longer castable talents in 12.1
//                                  (elemental passives Call of Fire / Fury of the Storms remain)
//   108270 Stone Bulwark Totem   - removed from the class tree in 12.1
//   98008  Spirit Link Totem     - Restoration-only talent in 12.1
//   187828 / 343725 Maelstrom    - passive resource, consumed via power(11)
//   77756  Lava Surge            - spec passive that grants the 77762 proc aura
//   20608  Reincarnation         - passive out-of-combat death recovery, not APL
//   192063 Gust of Wind / 192077 Wind Rush Totem - movement utility the bot cannot aim
//   205495 Stormkeeper (artifact row) - unlearnable Legion artifact spell; 191634 is the talent
//   1229376 Single-Button Assistant - client convenience macro, not a rotation ability

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

// ---- Spell IDs (WoW 12.1.0.69587, validated against SpellName.csv) ----
constexpr uint32 LIGHTNING_BOLT          = 188196;
constexpr uint32 LAVA_BURST              = 51505;
// Lava Surge proc - the aura applied to the player that makes the next
// Lava Burst instant + ignores CD. Two distinct DB2 rows share the name
// "Lava Surge": 77756 is the spec passive, 77762 is the proc aura applied
// at runtime when the passive triggers. APL checks `has_aura(LAVA_SURGE)`
// against the on-bot aura, so 77762 is correct.
constexpr uint32 LAVA_SURGE              = 77762;
// Flame Shock was renumbered in Midnight: 470411 is the castable (its
// description aliases 188389). The legacy row still exists and may be the
// id the periodic debuff lands under, so the refresh check probes both.
constexpr uint32 FLAME_SHOCK             = 470411;
constexpr uint32 FLAME_SHOCK_DOT_LEGACY  = 188389;      // aura-only
constexpr uint32 EARTH_SHOCK             = 8042;        // choice node vs Elemental Blast
constexpr uint32 ELEMENTAL_BLAST         = 117014;      // [R][M] - 90 Maelstrom
constexpr uint32 STORMKEEPER             = 191634;
constexpr uint32 CHAIN_LIGHTNING         = 188443;
constexpr uint32 EARTHQUAKE              = 61882;       // [R] ground-target variant
constexpr uint32 EARTHQUAKE_TARGETED     = 462620;      // [M] cast-at-target variant
constexpr uint32 FROST_SHOCK             = 196840;      // instant moving filler
constexpr uint32 THUNDERSTORM            = 51490;       // knockback AoE panic
constexpr uint32 VOLTAIC_BLAZE           = 470057;      // [M] AoE Flame Shock applicator
constexpr uint32 TEMPEST                 = 452201;      // Stormbringer proc cast
constexpr uint32 TEMPEST_READY_AURA      = 454015;      // "Lightning Bolt replaced by Tempest"
constexpr uint32 ANCESTRAL_SWIFTNESS     = 443454;      // Farseer - taught by 448861
constexpr uint32 NATURES_SWIFTNESS       = 378081;
constexpr uint32 ASCENDANCE_ELE          = 114050;
constexpr uint32 EARTH_ELEMENTAL         = 198103;
constexpr uint32 SPIRITWALKER_GRACE      = 79206;
constexpr uint32 EARTHBIND_TOTEM         = 2484;
constexpr uint32 EARTHGRAB_TOTEM         = 51485;       // [R] talent overriding Earthbind
constexpr uint32 CAPACITOR_TOTEM         = 192058;
constexpr uint32 HEX                     = 51514;
constexpr uint32 PURGE                   = 370;
constexpr uint32 BLOODLUST               = 2825;
constexpr uint32 HEROISM                 = 32182;
constexpr uint32 SKYFURY                 = 462854;      // group Mastery buff (L16)
constexpr uint32 SATED_DEBUFF            = 57724;
constexpr uint32 TEMPORAL_DISPL_DEBUFF   = 80354;
constexpr uint32 INSANITY_HUNTER_DEBUFF  = 95809;
constexpr uint32 FATIGUED_DEBUFF         = 264689;
constexpr uint32 WIND_SHEAR              = 57994;
constexpr uint32 ASTRAL_SHIFT            = 108271;
constexpr uint32 EARTH_SHIELD            = 974;
constexpr uint32 LIGHTNING_SHIELD        = 192106;
constexpr uint32 ELEMENTAL_ORBIT         = 383010;      // passive: +1 shield on self
constexpr uint32 FLAMETONGUE_WEAPON      = 318038;      // [R][M] imbue (+Fire damage)
constexpr uint32 FLAMETONGUE_AURA        = 319778;      // aura-only
// Healing Stream Totem: the class-tree talent row is 392915 and it teaches
// the castable 5394. Cast 5394; accept either id in the spellbook.
constexpr uint32 HEALING_STREAM_TOTEM    = 5394;
constexpr uint32 HEALING_STREAM_TALENT   = 392915;
constexpr uint32 HEALING_SURGE           = 8004;
constexpr uint32 CHAIN_HEAL              = 1064;        // [R][M] class talent
constexpr uint32 CLEANSE_SPIRIT          = 51886;       // Curse only in 12.1
constexpr uint32 ANCESTRAL_SPIRIT        = 2008;

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

// Cleanse Spirit (51886) is Curse-only in 12.1 ("Removes all Curse
// effects"); Magic dispel belongs to Restoration's Purify Spirit.
bool ShouldCleanseSpirit(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(CLEANSE_SPIRIT)) return false;
    if (!ctx.bot.is_ready(CLEANSE_SPIRIT)) return false;
    if (ctx.group.dispel_candidate(DispelType::Curse)) return true;
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

bool KnowsHealingStreamTotem(ApPredicateContext const& ctx)
{
    return ctx.bot.knows_spell(HEALING_STREAM_TOTEM) || ctx.bot.knows_spell(HEALING_STREAM_TALENT);
}
bool ShouldHealingStreamTotem(ApPredicateContext const& ctx)
{
    if (!KnowsHealingStreamTotem(ctx)) return false;
    if (!ctx.bot.is_ready(HEALING_STREAM_TOTEM)) return false;
    if (ctx.bot.has_aura(HEALING_STREAM_TOTEM)) return false;
    return ctx.bot.in_combat();
}
void DoHealingStreamTotem(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HEALING_STREAM_TOTEM); }

// Chain Heal (1064) - class talent in the curated Elemental builds. A DPS
// only reaches for it when the group is genuinely collapsing (2+ members
// at or below 45%) and the 2s cast is not going to be interrupted by
// movement; below that it is a Healing Surge self-heal question.
int WoundedFriendCount(ApPredicateContext const& ctx, int below_pct)
{
    auto const* members = ctx.group.members();
    if (!members) return 0;
    int n = 0;
    for (auto const& m : *members)
    {
        if (!m.online || m.max_hp <= 0 || m.hp <= 0) continue;
        if ((m.hp * 100) / m.max_hp <= below_pct) ++n;
    }
    return n;
}
bool ShouldChainHeal(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(CHAIN_HEAL)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(CHAIN_HEAL)) return false;
    return WoundedFriendCount(ctx, 45) >= 2;
}
void DoChainHeal(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    auto const* low = ctx.group.lowest_hp_on_map(ctx.bot.map_id(), Role::Unknown, ctx.bot.raw().position.x, ctx.bot.raw().position.y, ctx.bot.raw().position.z, 40.0f);
    e.cast(CHAIN_HEAL, low ? low->guid : ctx.bot.raw().guid);
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

// Flametongue Weapon (318038) - Elemental imbue that buffs Fire spell
// damage. 1h self-buff; the imbue shows as aura 319778 on the bot.
bool ShouldFlametongueWeapon(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(FLAMETONGUE_WEAPON)) return false;
    return !ctx.bot.has_aura(FLAMETONGUE_AURA);
}
void DoFlametongueWeapon(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FLAMETONGUE_WEAPON); }

// ---- Survival ----
bool ShouldAstralShift(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ASTRAL_SHIFT)) return false;
    if (!ctx.bot.is_ready(ASTRAL_SHIFT)) return false;
    return ctx.bot.hp_pct() <= 40;
}
void DoAstralShift(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ASTRAL_SHIFT); }

// Earth Elemental (198103) - panic button. Without Primordial Bond it is a
// taunting Greater Earth Elemental that pulls melee off the caster; with
// the [M] Primordial Bond talent it is a max-HP buff instead. Both make
// sense at the same trigger: real pressure on the bot itself.
bool ShouldEarthElemental(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(EARTH_ELEMENTAL)) return false;
    if (!ctx.bot.is_ready(EARTH_ELEMENTAL)) return false;
    return ctx.bot.hp_pct() <= 50 && ctx.bot.fightable_attackers_count() >= 1;
}
void DoEarthElemental(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(EARTH_ELEMENTAL); }

bool ShouldEarthShield(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(EARTH_SHIELD)) return false;
    return !ctx.bot.has_aura(EARTH_SHIELD);
}
void DoEarthShield(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(EARTH_SHIELD, ctx.bot.raw().guid);
}

// Lightning Shield (192106) - baseline L9 elemental shield. Only one
// elemental shield may sit on the bot unless Elemental Orbit (383010,
// [R][M]) is known, so without Orbit a bot that already runs Earth Shield
// on itself must NOT alternate the two every tick.
bool ShouldLightningShield(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(LIGHTNING_SHIELD)) return false;
    if (ctx.bot.has_aura(LIGHTNING_SHIELD)) return false;
    if (ctx.bot.knows_spell(EARTH_SHIELD) && !ctx.bot.knows_spell(ELEMENTAL_ORBIT)) return false;
    return true;
}
void DoLightningShield(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(LIGHTNING_SHIELD); }

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

// Earthgrab Totem (51485, [R] talent) overrides Earthbind Totem (2484,
// baseline L5). Two-branch so both talent and non-talent bots peel melee:
// drop it when 2+ mobs are on the caster in the open world. Skipped in
// instances - a root/slow totem on the tank's pack is griefing.
uint32 PickSnareTotem(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(EARTHGRAB_TOTEM)) return EARTHGRAB_TOTEM;
    if (ctx.bot.knows_spell(EARTHBIND_TOTEM)) return EARTHBIND_TOTEM;
    return 0;
}
bool ShouldSnareTotem(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (ctx.bot.is_in_instance()) return false;
    const uint32 sid = PickSnareTotem(ctx);
    if (!sid || !ctx.bot.is_ready(sid)) return false;
    return ctx.bot.melee_attackers_within(8.0f) >= 2;
}
void DoSnareTotem(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (const uint32 sid = PickSnareTotem(ctx)) e.cast(sid);
}

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

// Nature's Swiftness (378081, [R][M]) - next Nature spell instant and free.
// Fired when the bot is moving without Spiritwalker's Grace so the next
// Lightning Bolt / Chain Lightning goes off mid-run instead of being held.
bool ShouldNaturesSwiftness(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(NATURES_SWIFTNESS)) return false;
    if (!ctx.bot.is_ready(NATURES_SWIFTNESS)) return false;
    if (ctx.bot.has_aura(NATURES_SWIFTNESS)) return false;
    if (ctx.bot.has_aura(SPIRITWALKER_GRACE)) return false;
    return ctx.bot.is_moving();
}
void DoNaturesSwiftness(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(NATURES_SWIFTNESS); }

// ---- Major offensive cooldowns ----
// Ancestral Swiftness (443454) - Farseer hero active taught by the 448861
// [R][M] passive: next damaging spell instant, free and amplified. Used on
// cooldown against a live target; the following Lava Burst / Lightning
// Bolt in the ladder consumes it.
bool ShouldAncestralSwiftness(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ANCESTRAL_SWIFTNESS)) return false;
    if (!ctx.bot.is_ready(ANCESTRAL_SWIFTNESS)) return false;
    return !ctx.bot.has_aura(ANCESTRAL_SWIFTNESS);
}
void DoAncestralSwiftness(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ANCESTRAL_SWIFTNESS); }

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

// Frost Shock (196840, [R] class talent) - with Icefury gone in 12.1 this is
// a plain instant: the Maelstrom-generating filler while the bot is moving
// and cannot hard-cast Lightning Bolt (no Spiritwalker's Grace up).
bool ShouldFrostShockMoving(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FROST_SHOCK)) return false;
    if (!ctx.bot.is_ready(FROST_SHOCK)) return false;
    return ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(LIGHTNING_BOLT);
}
void DoFrostShock(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FROST_SHOCK, ctx.bot.victim());
}

// ---- Damage rotation ----
// Flame Shock DoT presence - the 12.1 castable is 470411 but the periodic
// debuff may still be tracked under the legacy 188389 row; accept either.
AuraEntry const* FlameShockOnVictim(ApPredicateContext const& ctx)
{
    if (AuraEntry const* a = ctx.bot.find_aura(FLAME_SHOCK, ctx.bot.victim())) return a;
    return ctx.bot.find_aura(FLAME_SHOCK_DOT_LEGACY, ctx.bot.victim());
}

// Voltaic Blaze (470057, [M]) - instant Flame Shock on the target plus
// nearby enemies, always crits, generates Maelstrom. When known it is the
// preferred Flame Shock applicator: fire on cooldown whenever the DoT is
// missing/expiring or there is a second target to spread onto.
bool ShouldVoltaicBlaze(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(VOLTAIC_BLAZE)) return false;
    if (!ctx.bot.is_ready(VOLTAIC_BLAZE)) return false;
    AuraEntry const* a = FlameShockOnVictim(ctx);
    return !a || a->remaining.count() <= 6000 || ctx.bot.enemies_within(10.0f) >= 2;
}
void DoVoltaicBlaze(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(VOLTAIC_BLAZE, ctx.bot.victim());
}

bool ShouldFlameShock(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FLAME_SHOCK)) return false;
    AuraEntry const* a = FlameShockOnVictim(ctx);
    return !a || a->remaining.count() <= 4000;
}
void DoFlameShock(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FLAME_SHOCK, ctx.bot.victim());
}

// Lava Burst - split into two rules so the rotation can prioritize the
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
    // Hard-cast Lava Burst is a 2s cast - never start mid-move.
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(LAVA_BURST)) return false;
    return true;
}
void DoLavaBurst(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LAVA_BURST, ctx.bot.victim());
}

// Thunderstorm - 51490, 30y AoE knockback + small damage on a 45s CD.
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
    // Solo / open-world only - in dungeons / raids the tank's positioning
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

// Earthquake is a choice node in 12.1: 61882 (raid build) is placed at a
// location, 462620 (M+ build) is cast at the target. Pick whichever the
// bot knows; both cost 60 Maelstrom.
uint32 PickEarthquake(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(EARTHQUAKE_TARGETED)) return EARTHQUAKE_TARGETED;
    if (ctx.bot.knows_spell(EARTHQUAKE)) return EARTHQUAKE;
    return 0;
}
bool ShouldEarthquake(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    const uint32 sid = PickEarthquake(ctx);
    if (!sid || !ctx.bot.is_ready(sid)) return false;
    if (Maelstrom(ctx) < 60) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(20.0f) >= 3;
}
void DoEarthquake(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const uint32 sid = PickEarthquake(ctx);
    if (!sid) return;
    if (sid == EARTHQUAKE_TARGETED) { e.cast(sid, ctx.bot.victim()); return; }
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(sid, v->x, v->y, v->z);
    else
        e.cast(sid, ctx.bot.victim());
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
    // 3+ targets matches modern Elemental AoE breakpoint - at 2 targets
    // Lightning Bolt + spender is comparable damage and generates more
    // Maelstrom per GCD. owner-flagged aoe_preference still overrides.
    return ctx.aoe_preference || ctx.bot.enemies_within(20.0f) >= 3;
}
void DoChainLightning(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CHAIN_LIGHTNING, ctx.bot.victim());
}

// Tempest (452201) - Stormbringer hero cast. Spending Maelstrom can upgrade
// the next Lightning Bolt into Tempest; the 454015 aura ("Lightning Bolt
// replaced by Tempest") marks the window. Cast the Tempest id explicitly:
// emitting the base Lightning Bolt would fire the un-upgraded spell.
bool ShouldTempest(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(TEMPEST)) return false;
    if (!ctx.bot.has_aura(TEMPEST_READY_AURA)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(TEMPEST)) return false;
    return true;
}
void DoTempest(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(TEMPEST, ctx.bot.victim());
}

bool ShouldLightningBolt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(LIGHTNING_BOLT)) return false;
    // Hard-cast - bots in motion need an instant alternative (Frost Shock).
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(LIGHTNING_BOLT)) return false;
    return true;
}
void DoLightningBolt(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LIGHTNING_BOLT, ctx.bot.victim());
}

bool AlwaysAlive(ApPredicateContext const& ctx) { return ctx.bot.is_alive(); }
void DoNothing(ApPredicateContext const&, BotIntentEmitter&) {}

// Rule ORDER (12.1 Elemental):
// OOC rez -> self buffs (Skyfury, Earth Shield, Lightning Shield,
// Flametongue) -> panic (Astral Shift <=40%, Earth Elemental <=50%,
// Healing Surge <=40%, Thunderstorm peel, snare totem) -> interrupt / CC
// (Wind Shear, Capacitor, Hex, Purge) -> group (Cleanse Spirit, Chain Heal
// collapse, Healing Stream, Spiritwalker's Grace, Nature's Swiftness,
// Bloodlust) -> major CDs (Ascendance, Ancestral Swiftness) -> Lava Surge
// instant Lava Burst -> Voltaic Blaze / Flame Shock upkeep -> Stormkeeper
// -> Tempest proc -> Elemental Blast (90) -> Chain Lightning (3+) ->
// Earthquake (3+, 60) -> Earth Shock (60) -> Lava Burst hardcast -> Frost
// Shock (moving) -> Lightning Bolt filler -> Idle.
ApRule const kRules[] = {
    { ShouldAncestralSpirit,    DoAncestralSpirit,    "Ancestral Spirit (rez OOC)" },
    { ShouldSkyfury,            DoSkyfury,            "Skyfury (group buff)"       },
    { ShouldEarthShield,        DoEarthShield,        "Earth Shield (self)"        },
    { ShouldLightningShield,    DoLightningShield,    "Lightning Shield (self)"    },
    { ShouldFlametongueWeapon,  DoFlametongueWeapon,  "Flametongue Weapon (imbue)" },
    { ShouldAstralShift,        DoAstralShift,        "Astral Shift (<=40%)"       },
    { ShouldEarthElemental,     DoEarthElemental,     "Earth Elemental (<=50%)"    },
    { ShouldHealingSurgeSelf,   DoHealingSurgeSelf,   "Healing Surge (<=40% self)" },
    { ShouldThunderstorm,       DoThunderstorm,       "Thunderstorm (panic peel)"  },
    { ShouldSnareTotem,         DoSnareTotem,         "Earthgrab/Earthbind (peel)" },
    { ShouldWindShear,          DoWindShear,          "Wind Shear (interrupt)"     },
    { ShouldCapacitorTotem,     DoCapacitorTotem,     "Capacitor Totem (3+ AoE)"   },
    { ShouldHex,                DoHex,                "Hex (off-target CC)"        },
    { ShouldPurge,              DoPurge,              "Purge (Magic dispel)"       },
    { ShouldCleanseSpirit,      DoCleanseSpirit,      "Cleanse Spirit (Curse)"     },
    { ShouldChainHeal,          DoChainHeal,          "Chain Heal (2+ <=45%)"      },
    { ShouldHealingStreamTotem, DoHealingStreamTotem, "Healing Stream Totem"       },
    { ShouldSpiritwalkerGrace,  DoSpiritwalkerGrace,  "Spiritwalker's Grace"       },
    { ShouldNaturesSwiftness,   DoNaturesSwiftness,   "Nature's Swiftness (moving)"},
    { ShouldBloodlust,          DoBloodlust,          "Bloodlust/Heroism (boss)"   },
    { ShouldAscendance,         DoAscendance,         "Ascendance"                 },
    { ShouldAncestralSwiftness, DoAncestralSwiftness, "Ancestral Swiftness"        },
    { ShouldLavaBurstSurgeProc, DoLavaBurst,          "Lava Burst (Lava Surge proc)"},
    { ShouldVoltaicBlaze,       DoVoltaicBlaze,       "Voltaic Blaze (Flame Shock)"},
    { ShouldFlameShock,         DoFlameShock,         "Flame Shock (refresh)"      },
    { ShouldStormkeeper,        DoStormkeeper,        "Stormkeeper"                },
    { ShouldTempest,            DoTempest,            "Tempest (proc)"             },
    { ShouldElementalBlast,     DoElementalBlast,     "Elemental Blast (90 maelstr)"},
    { ShouldChainLightning,     DoChainLightning,     "Chain Lightning (3+ AoE)"   },
    { ShouldEarthquake,         DoEarthquake,         "Earthquake (3+ AoE)"        },
    { ShouldEarthShock,         DoEarthShock,         "Earth Shock (60 maelstr)"   },
    { ShouldLavaBurstHardcast,  DoLavaBurst,          "Lava Burst (hardcast)"      },
    { ShouldFrostShockMoving,   DoFrostShock,         "Frost Shock (moving)"       },
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
