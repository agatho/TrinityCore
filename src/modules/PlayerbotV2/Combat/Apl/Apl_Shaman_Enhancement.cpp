// Enhancement Shaman - WoW 12.1.0.69587 (Midnight) enterprise rotation.
// Dual-wield melee with Maelstrom Weapon (5-stack proc) gating instant
// Lightning Bolt / Tempest / Chain Lightning / Chain Heal / Healing Surge.
// Stormstrike (Windstrike while Ascendant) is the signature strike, Lava
// Lash spreads Flame Shock (Molten Assault), Voltaic Blaze applies Flame
// Shock to a pack and feeds Maelstrom Weapon. Major CDs: Ascendance (which
// also unleashes Doom Winds in 12.1) / Doom Winds (non-talent fallback),
// Sundering (line damage; summons Fire Feral Spirits via the passive).
// Gap closer: Feral Lunge. Imbues: Windfury Weapon (MH), Flametongue
// Weapon (OH).
//
// Group utility: Bloodlust/Heroism, Skyfury (group Mastery), Healing
// Stream Totem, Cleanse Spirit (Curse), Chain Heal (MW-instant group
// heal), Earth Shield. CC: Wind Shear, Capacitor Totem, Hex, Purge.
// Survival: Astral Shift, Healing Surge (MW-instant self heal), Earth
// Shield, Lightning Shield (Elemental Orbit lets both sit on the bot).
//
// Validated spell IDs (WoW 12.1.0.69587):
//   17364  Stormstrike        | 115356 Windstrike         | 60103  Lava Lash
//   187874 Crash Lightning    | 196840 Frost Shock        | 470411 Flame Shock
//   470057 Voltaic Blaze      | 188196 Lightning Bolt     | 452201 Tempest
//   188443 Chain Lightning    | 1064   Chain Heal         | 384352 Doom Winds
//   114051 Ascendance (enh)   | 197214 Sundering          | 196884 Feral Lunge
//   33757  Windfury Weapon    | 318038 Flametongue Weapon | 57994  Wind Shear
//   108271 Astral Shift       | 974    Earth Shield       | 192106 Lightning Shield
//   8004   Healing Surge      | 5394   Healing Stream     | 192058 Capacitor Totem
//   51514  Hex                | 370    Purge
//   51886  Cleanse Spirit     | 2008   Ancestral Spirit   | 462854 Skyfury
//   2825   Bloodlust          | 32182  Heroism
//   Aura-only: 344179 Maelstrom Weapon stacks | 454015 Tempest ready
//              188389 Flame Shock DoT (legacy debuff row) | 319773 Windfury imbue
//              319778 Flametongue imbue | 57724/80354/95809/264689 sated
//   Passive gates: 383010 Elemental Orbit | 392915 Healing Stream Totem talent (teaches 5394)
//
// Skipped spells (and why):
//   117014 Elemental Blast      - Elemental-only talent in 12.1
//   51533  Feral Spirit         - no longer castable: 469314 Feral Spirit is a passive
//                                 (Sundering / Doom Winds summon the wolves)
//   333974 Fire Nova            - no longer castable: 1260666 Fire Nova is a passive
//                                 (Voltaic Blaze has a chance to trigger it); 466620 is
//                                 the internal detonation row, not learnable
//   375982 Primordial Wave      - removed from the Enhancement tree; 327163 is the
//                                 Shadowlands Necrolord covenant row, not learnable
//   342240 Ice Strike           - removed from the Enhancement tree in 12.1
//   108270 Stone Bulwark Totem  - removed from the class tree in 12.1
//   98008  Spirit Link Totem    - Restoration-only talent in 12.1
//   1218047 Primordial Storm    - passive that morphs Sundering; not in the curated builds
//   187880 Maelstrom Weapon     - passive; the 344179 stack aura is what the APL reads
//   1252197 Ascendance          - empty companion row taught by the 114051 talent; 114051
//                                 is the castable (cd/duration)
//   58875 Spirit Walk / 192063 Gust of Wind / 192077 Wind Rush Totem - movement utility
//                                 the bot cannot aim
//   79206  Spiritwalker's Grace - not in the Enhancement builds; MW procs cover casts
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
constexpr uint32 STORMSTRIKE          = 17364;
constexpr uint32 WINDSTRIKE           = 115356;      // Stormstrike while Ascendant
constexpr uint32 LAVA_LASH            = 60103;
constexpr uint32 CRASH_LIGHTNING      = 187874;
constexpr uint32 FROST_SHOCK_ENH      = 196840;
// Flame Shock was renumbered in Midnight: 470411 is the castable (its
// description aliases 188389). The legacy row still exists and may be the
// id the periodic debuff lands under, so the refresh check probes both.
constexpr uint32 FLAME_SHOCK          = 470411;
constexpr uint32 FLAME_SHOCK_DOT_LEGACY = 188389;    // aura-only
constexpr uint32 VOLTAIC_BLAZE        = 470057;      // [R][M] AoE Flame Shock + MW
constexpr uint32 LIGHTNING_BOLT_ENH   = 188196;
constexpr uint32 TEMPEST              = 452201;      // Stormbringer proc cast
constexpr uint32 TEMPEST_READY_AURA   = 454015;      // "Lightning Bolt replaced by Tempest"
constexpr uint32 CHAIN_LIGHTNING_ENH  = 188443;
constexpr uint32 CHAIN_HEAL           = 1064;        // [R][M] class talent
constexpr uint32 DOOM_WINDS           = 384352;      // overridden by Ascendance when talented
constexpr uint32 ASCENDANCE_ENH       = 114051;
constexpr uint32 SUNDERING            = 197214;      // talent (not in curated builds)
constexpr uint32 FERAL_LUNGE          = 196884;      // spec spell - gap closer
constexpr uint32 WINDFURY_WEAPON      = 33757;       // [R][M] main-hand imbue
constexpr uint32 WINDFURY_AURA        = 319773;      // aura-only
constexpr uint32 FLAMETONGUE_WEAPON   = 318038;      // [R][M] off-hand imbue
constexpr uint32 FLAMETONGUE_AURA     = 319778;      // aura-only
constexpr uint32 MAELSTROM_WEAPON     = 344179;      // stack aura (passive is 187880)
constexpr uint32 WIND_SHEAR           = 57994;
constexpr uint32 ASTRAL_SHIFT         = 108271;
constexpr uint32 EARTH_SHIELD         = 974;
constexpr uint32 LIGHTNING_SHIELD     = 192106;
constexpr uint32 ELEMENTAL_ORBIT      = 383010;      // passive: +1 shield on self
constexpr uint32 HEALING_SURGE        = 8004;
// Healing Stream Totem: the class-tree talent row is 392915 and it teaches
// the castable 5394. Cast 5394; accept either id in the spellbook.
constexpr uint32 HEALING_STREAM_TOTEM = 5394;
constexpr uint32 HEALING_STREAM_TALENT= 392915;
constexpr uint32 CAPACITOR_TOTEM      = 192058;
constexpr uint32 HEX                  = 51514;
constexpr uint32 PURGE                = 370;
constexpr uint32 CLEANSE_SPIRIT       = 51886;
constexpr uint32 ANCESTRAL_SPIRIT     = 2008;
constexpr uint32 BLOODLUST            = 2825;
constexpr uint32 HEROISM              = 32182;
constexpr uint32 SKYFURY              = 462854;      // group Mastery buff (L16)
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

bool KnowsHealingStreamTotem(ApPredicateContext const& ctx)
{
    return ctx.bot.knows_spell(HEALING_STREAM_TOTEM) || ctx.bot.knows_spell(HEALING_STREAM_TALENT);
}
bool ShouldHealingStreamTotem(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!KnowsHealingStreamTotem(ctx)) return false;
    if (!ctx.bot.is_ready(HEALING_STREAM_TOTEM)) return false;
    return !ctx.bot.has_aura(HEALING_STREAM_TOTEM);
}
void DoHealingStreamTotem(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HEALING_STREAM_TOTEM); }

// Chain Heal (1064, [R][M] class talent) - at 5+ Maelstrom Weapon stacks
// it is instant, so a melee DPS can plug a group collapse (2+ members at
// or below 45%) without stopping to hard-cast.
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
bool ShouldChainHealMW(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(CHAIN_HEAL)) return false;
    if (MaelstromStacks(ctx) < 5) return false;
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

// Weapon imbues - Windfury on the main hand (33757 -> aura 319773),
// Flametongue on the off hand (318038 -> aura 319778). 1h self-buffs.
bool ShouldWindfuryWeapon(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(WINDFURY_WEAPON)) return false;
    return !ctx.bot.has_aura(WINDFURY_AURA);
}
void DoWindfuryWeapon(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(WINDFURY_WEAPON); }

bool ShouldFlametongueWeapon(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(FLAMETONGUE_WEAPON)) return false;
    return !ctx.bot.has_aura(FLAMETONGUE_AURA);
}
void DoFlametongueWeapon(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FLAMETONGUE_WEAPON); }

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
// See ApCrowdControl.h - replaced the old nearby_enemies.size()>=2 + has_aura
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

// Doom Winds (384352) is overridden by Ascendance (114051) in 12.1: the
// talented Ascendance "unleashes Doom Winds" itself. Only bots WITHOUT the
// Ascendance talent keep a separate Doom Winds button.
bool ShouldDoomWinds(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DOOM_WINDS)) return false;
    if (ctx.bot.knows_spell(ASCENDANCE_ENH)) return false;
    if (!ctx.bot.is_ready(DOOM_WINDS)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoDoomWinds(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DOOM_WINDS); }

// Sundering (197214) - frontal line damage on a 30s CD. Not in the curated
// builds but gated anyway; with the 469314 Feral Spirit passive it also
// summons Fire Feral Spirits, so a lone boss target is worth it too.
bool ShouldSundering(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SUNDERING)) return false;
    if (!ctx.bot.is_ready(SUNDERING)) return false;
    return ctx.bot.enemies_within(10.0f) >= 2 || BossLikeTargetEngaged(ctx);
}
void DoSundering(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SUNDERING); }

// ---- Damage rotation ----
// Flame Shock DoT presence - the 12.1 castable is 470411 but the periodic
// debuff may still be tracked under the legacy 188389 row; accept either.
AuraEntry const* FlameShockOnVictim(ApPredicateContext const& ctx)
{
    if (AuraEntry const* a = ctx.bot.find_aura(FLAME_SHOCK, ctx.bot.victim())) return a;
    return ctx.bot.find_aura(FLAME_SHOCK_DOT_LEGACY, ctx.bot.victim());
}

// Voltaic Blaze (470057, [R][M]) - instant Flame Shock on the target plus
// nearby enemies, always crits, generates Maelstrom Weapon (and can
// trigger Fire Nova via the 1260666 passive). Fire on cooldown whenever
// the DoT is missing/expiring or there is a pack to spread onto.
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

// Tempest (452201) - Stormbringer hero cast. Spending Maelstrom Weapon can
// upgrade the next Lightning Bolt into Tempest; the 454015 aura marks the
// window. Cast the Tempest id explicitly at 5+ stacks: emitting the base
// Lightning Bolt would fire the un-upgraded spell.
bool ShouldTempestMW(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(TEMPEST)) return false;
    if (!ctx.bot.has_aura(TEMPEST_READY_AURA)) return false;
    return MaelstromStacks(ctx) >= 5;
}
void DoTempest(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(TEMPEST, ctx.bot.victim());
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

// Feral Lunge - 196884, 25y leap to a target. Enhancement gap-closer.
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
    // cap. Squared so we avoid sqrt: 8y^2 = 64, 25y^2 = 625.
    return d2 > 64.0f && d2 <= 625.0f;
}
void DoFeralLunge(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FERAL_LUNGE, ctx.bot.victim());
}

// Stormstrike (17364) becomes Windstrike (115356, baseline L15) while the
// bot is an Air Ascendant: cheaper, armor-bypassing, 30y range. Cast the
// Windstrike id explicitly during the Ascendance aura so the override
// actually applies instead of the base strike.
uint32 PickStormstrike(ApPredicateContext const& ctx)
{
    if (ctx.bot.has_aura(ASCENDANCE_ENH) && ctx.bot.knows_spell(WINDSTRIKE)) return WINDSTRIKE;
    if (ctx.bot.knows_spell(STORMSTRIKE)) return STORMSTRIKE;
    return 0;
}
bool ShouldStormstrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    const uint32 sid = PickStormstrike(ctx);
    return sid != 0 && ctx.bot.is_ready(sid);
}
void DoStormstrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (const uint32 sid = PickStormstrike(ctx)) e.cast(sid, ctx.bot.victim());
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

// Rule ORDER (12.1 Enhancement): OOC rez -> self buffs (Skyfury, Earth
// Shield, Lightning Shield, Windfury / Flametongue imbues) -> Astral Shift
// -> Healing Surge self (MW gate) -> Wind Shear -> Capacitor / Hex / Purge
// -> Cleanse Spirit -> Chain Heal (MW, group collapse) -> Healing Stream
// -> Bloodlust -> Ascendance (or Doom Winds when untalented) -> Feral
// Lunge gap-close -> Voltaic Blaze / Flame Shock DoT -> Maelstrom Weapon
// 5-stack spenders (Tempest proc / Chain Lightning AoE / Lightning Bolt)
// -> Stormstrike / Windstrike (signature) -> Crash Lightning (AoE 2+) ->
// Lava Lash -> Sundering -> Frost Shock filler / snare -> AutoAttack.
// The MW spenders sit ABOVE the melee builders because the evaluator fires
// only the first matching rule per tick: the builders gate only on
// knows_spell+is_ready (essentially always true off-GCD) and would otherwise
// consume the tick at 5+ MW stacks, wasting Enhancement's instant procs.
// The spenders gate on MaelstromStacks>=5, so below 5 stacks they fall
// through to the builders unchanged.
ApRule const kRules[] = {
    { ShouldAncestralSpirit,  DoAncestralSpirit,  "Ancestral Spirit (rez OOC)" },
    { ShouldSkyfury,          DoSkyfury,          "Skyfury (group buff)"       },
    { ShouldEarthShield,      DoEarthShield,      "Earth Shield (self)"        },
    { ShouldLightningShield,  DoLightningShield,  "Lightning Shield (self)"    },
    { ShouldWindfuryWeapon,   DoWindfuryWeapon,   "Windfury Weapon (MH imbue)" },
    { ShouldFlametongueWeapon,DoFlametongueWeapon,"Flametongue Weapon (OH)"    },
    { ShouldAstralShift,      DoAstralShift,      "Astral Shift (<=40%)"       },
    { ShouldHealingSurgeProc, DoHealingSurgeSelf, "Healing Surge (MW self)"    },
    { ShouldWindShear,        DoWindShear,        "Wind Shear (interrupt)"     },
    { ShouldCapacitorTotem,   DoCapacitorTotem,   "Capacitor Totem (3+ AoE)"   },
    { ShouldHex,              DoHex,              "Hex (off-target CC)"        },
    { ShouldPurge,            DoPurge,            "Purge (Magic dispel)"       },
    { ShouldCleanseSpirit,    DoCleanseSpirit,    "Cleanse Spirit (Curse)"     },
    { ShouldChainHealMW,      DoChainHeal,        "Chain Heal (MW, 2+ <=45%)"  },
    { ShouldHealingStreamTotem,DoHealingStreamTotem,"Healing Stream Totem"     },
    { ShouldBloodlust,        DoBloodlust,        "Bloodlust/Heroism (boss)"   },
    { ShouldAscendance,       DoAscendance,       "Ascendance (+Doom Winds)"   },
    { ShouldDoomWinds,        DoDoomWinds,        "Doom Winds (untalented)"    },
    { ShouldFeralLunge,       DoFeralLunge,       "Feral Lunge (gap close)"    },
    { ShouldVoltaicBlaze,     DoVoltaicBlaze,     "Voltaic Blaze (Flame Shock)"},
    { ShouldFlameShock,       DoFlameShock,       "Flame Shock"                },
    { ShouldTempestMW,        DoTempest,          "Tempest (MW proc)"          },
    { ShouldChainLightningMW, DoChainLightning,   "Chain Lightning (MW 2+)"    },
    { ShouldLightningBoltMW,  DoLightningBolt,    "Lightning Bolt (MW)"        },
    { ShouldStormstrike,      DoStormstrike,      "Stormstrike / Windstrike"   },
    { ShouldCrashLightning,   DoCrashLightning,   "Crash Lightning (2+ AoE)"   },
    { ShouldLavaLash,         DoLavaLash,         "Lava Lash"                  },
    { ShouldSundering,        DoSundering,        "Sundering (2+ AoE / boss)"  },
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
