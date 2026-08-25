// Vengeance Demon Hunter - WoW 12.0 enterprise tank rotation. Pain
// resource, Soul Fragment management, Demon Spikes / Metamorphosis active
// mitigation, Fiery Brand damage-reduction debuff, Soul Cleave Pain
// spender that consumes fragments for self-heal. Spirit Bomb (talent)
// detonates fragments for AoE + Frailty (DR debuff). Group utility:
// Sigil of Silence (caster lockdown), Sigil of Misery (fear), Darkness
// (group dodge).
//
// ---- Validated IDs (cross-checked against wago.tools SpellName.csv +
//      SpellLevels.csv on 2026-05-27) ----
//
//   Core builders / spenders:
//     203783 Shear                     - Pain generator (SpellLevel=1).
//                                        REPLACES Demon's Bite for the
//                                        Vengeance spec from L1.
//     263642 Fracture (talent)         - Replaces Shear when talented;
//                                        cheaper, charge-based, spawns
//                                        extra fragments.
//     228477 Soul Cleave               - Pain spender + self-heal +
//                                        consumes up to 2 fragments.
//     247454 Spirit Bomb (talent)      - Fragment detonator AoE +
//                                        Frailty DR debuff.
//
//   Active mitigation / defensive:
//     321028 Demon Spikes              - Modern Vengeance variant
//                                        (SpellLevel=33). Older 203720
//                                        ID still works for legacy
//                                        characters; we probe both.
//     203720 Demon Spikes (legacy)     - SpellLevel=1 variant. Either
//                                        ID may resolve depending on
//                                        client state; rotation picks
//                                        whichever knows_spell hits.
//     320962 Fiery Brand               - Modern Vengeance variant
//                                        (SpellLevel=38). Single-target
//                                        damage-reduction debuff.
//     204021 Fiery Brand (legacy)      - BaseLevel=0; pre-spec/legacy.
//     321067 Metamorphosis (Vengeance) - Tank defensive CD at L20.
//                                        Different from Havoc's 191427.
//     321068 Metamorphosis (Vengeance) - Upgrade at L48 (same name,
//                                        different SpellLevels entry).
//                                        Probed alongside 321067.
//     187827 Metamorphosis (legacy)    - Older Vengeance variant.
//     320639 Fel Devastation           - L23 channel: AoE damage + self
//                                        heal. Active mitigation layer.
//     212084 Fel Devastation (legacy)  - L11 variant; same effect.
//     263648 Soul Barrier (talent)     - Absorb shield based on fragments.
//     320341 Bulk Extraction (talent)  - Shatters nearby souls into
//                                        fragments + heal.
//     196718 Darkness                  - Group 20% dodge AoE.
//
//   AoE / threat:
//     320794 Sigil of Flame            - L10 Vengeance variant. Ground
//                                        AoE tick + Pain generation.
//     204596 Sigil of Flame (legacy)   - Same name, same level (L10);
//                                        probed for older clients.
//     258920 Immolation Aura           - Self-buff AoE + Pain proc.
//     247454 Spirit Bomb               - (see above)
//     189110 Infernal Strike           - L? gap close + AoE damage.
//     306830 Elysian Decree (talent)   - AoE sigil burst.
//     370965 The Hunt                  - Hero talent.
//     207407 Soul Carver (talent)      - 1min CD - armour strip + 3
//                                        soul fragments.
//
//   Range / pull:
//     204157 Throw Glaive (Vengeance)  - Vengeance variant of Throw
//                                        Glaive (185123). Ranged pull /
//                                        threat tag. Probed alongside
//                                        185123 for compatibility.
//     185123 Throw Glaive (baseline)   - Generic ID kept as fallback.
//
//   CC / utility:
//     183752 Disrupt                   - Interrupt.
//     202137 Sigil of Silence          - AoE caster silence (interrupt fb).
//     207684 Sigil of Misery           - AoE fear.
//     202138 Sigil of Chains           - AoE pull/clump.
//     185245 Torment                   - Taunt (Vengeance core).
//     217832 Imprison                  - Single-target incap (out-of-combat).
//
// ---- Skipped spells (and why) ----
//
//   178940 Shattered Souls    - Passive that drops a soul fragment on
//                               enemy death. No active cast; mechanics
//                               are read by Soul Cleave / Spirit Bomb.
//   203513 Demonic Wards      - Passive armour buff. No active.
//   206478 Demonic Appetite   - Havoc-side passive; not on Vengeance.
//   221351 Critical Strikes   - Passive. No active.
//   162243 Demon's Bite       - Havoc Fury generator. Vengeance uses
//                               Shear (203783) / Fracture (263642)
//                               instead. Demon's Bite would not even be
//                               on the spellbook for Vengeance bots.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated) ----
constexpr uint32 SHEAR                = 203783;       // L1 Vengeance generator
constexpr uint32 FRACTURE             = 263642;       // talent generator
constexpr uint32 SOUL_CLEAVE          = 228477;
constexpr uint32 IMMOLATION_AURA      = 258920;
constexpr uint32 SIGIL_OF_FLAME       = 204596;       // legacy
constexpr uint32 SIGIL_OF_FLAME_V     = 320794;       // L10 Vengeance variant
constexpr uint32 SIGIL_OF_SILENCE     = 202137;
constexpr uint32 SIGIL_OF_MISERY      = 207684;
constexpr uint32 SIGIL_OF_CHAINS      = 202138;
constexpr uint32 INFERNAL_STRIKE      = 189110;
constexpr uint32 DEMON_SPIKES         = 203720;       // legacy
constexpr uint32 DEMON_SPIKES_V       = 321028;       // L33 Vengeance variant
constexpr uint32 METAMORPHOSIS_TANK   = 187827;       // legacy Vengeance Meta
constexpr uint32 METAMORPHOSIS_V1     = 321067;       // L20 Vengeance variant
constexpr uint32 METAMORPHOSIS_V2     = 321068;       // L48 Vengeance variant
constexpr uint32 FIERY_BRAND          = 204021;       // legacy
constexpr uint32 FIERY_BRAND_V        = 320962;       // L38 Vengeance variant
constexpr uint32 SPIRIT_BOMB          = 247454;
constexpr uint32 SOUL_BARRIER         = 263648;       // talent - absorb shield
constexpr uint32 BULK_EXTRACTION      = 320341;       // talent - gather souls
constexpr uint32 ELYSIAN_DECREE       = 306830;       // talent - sigil burst
constexpr uint32 THE_HUNT             = 370965;
constexpr uint32 DISRUPT              = 183752;
constexpr uint32 TORMENT              = 185245;
constexpr uint32 DARKNESS             = 196718;
constexpr uint32 IMPRISON             = 217832;
constexpr uint32 FEL_DEVASTATION      = 212084;       // legacy
constexpr uint32 FEL_DEVASTATION_V    = 320639;       // L23 Vengeance variant
constexpr uint32 SOUL_CARVER          = 207407;       // talent - 1min CD
constexpr uint32 THROW_GLAIVE         = 185123;       // baseline
constexpr uint32 THROW_GLAIVE_V       = 204157;       // Vengeance variant

// 12.0 Vengeance runs on FURY (index 17), not the Legion-era Pain (18):
// no TC code grants POWER_PAIN to players anymore, so power[18] was
// permanently 0 and every Pain-gated rule below returned false forever —
// Vengeance tanks never cast Demon Spikes / Soul Cleave / Spirit Bomb
// (audit B12). Thresholds below are display-unit Fury costs.
constexpr uint8 POWER_PAIN_IDX = 17;     // POWER_FURY (see above)

// Returns the spell ID the bot actually knows, preferring the modern
// Vengeance variant over the legacy generic ID. Returns 0 if neither.
uint32 PickKnown(ApPredicateContext const& ctx, uint32 modern, uint32 legacy)
{
    if (ctx.bot.knows_spell(modern)) return modern;
    if (ctx.bot.knows_spell(legacy)) return legacy;
    return 0;
}

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

int32 Pain(ApPredicateContext const& ctx) { return ctx.bot.power(POWER_PAIN_IDX); }

// ---- Tank utility ----
bool ShouldTorment(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(TORMENT)) return false;
    if (!ctx.bot.is_ready(TORMENT)) return false;
    return ctx.bot.untaunted_enemy() != nullptr;
}
void DoTorment(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.untaunted_enemy())
        e.cast(TORMENT, t->guid);
}

bool ShouldSigilOfChains(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SIGIL_OF_CHAINS)) return false;
    if (!ctx.bot.is_ready(SIGIL_OF_CHAINS)) return false;
    return ctx.bot.enemies_within(20.0f) >= 4;
}
void DoSigilOfChains(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(SIGIL_OF_CHAINS, v->x, v->y, v->z);
    else
        e.cast(SIGIL_OF_CHAINS);
}

// ---- Active mitigation ----
// Demon Spikes - core physical mitigation. 6s armour/parry buff, 2
// charges. Refresh window: cast when not active OR remaining <= 1s.
// Previous logic required the aura to fully expire before re-casting,
// dropping ~1s of mitigation every refresh - significant uptime loss at
// boss damage rates. Probes both modern (321028) and legacy (203720)
// variants because the character may have learned either.
bool ShouldDemonSpikes(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    // No resource gate: Demon Spikes is cost-free and charge-based in 12.0
    // (the old Pain>=20 gate could never pass and double-blocked it).
    uint32 spell = PickKnown(ctx, DEMON_SPIKES_V, DEMON_SPIKES);
    if (!spell) return false;
    if (!ctx.bot.is_ready(spell)) return false;
    if (AuraEntry const* a = ctx.bot.find_aura(spell))
        return a->remaining.count() <= 1000;
    // Aura might be tracked under the variant we did not probe; check both.
    if (spell == DEMON_SPIKES_V) {
        if (AuraEntry const* a2 = ctx.bot.find_aura(DEMON_SPIKES))
            return a2->remaining.count() <= 1000;
    } else {
        if (AuraEntry const* a2 = ctx.bot.find_aura(DEMON_SPIKES_V))
            return a2->remaining.count() <= 1000;
    }
    return true;
}
void DoDemonSpikes(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 spell = PickKnown(ctx, DEMON_SPIKES_V, DEMON_SPIKES))
        e.cast(spell);
}

// Fiery Brand - boss/elite damage-reduction debuff. Prefer big targets;
// also fire defensively when HP dips. Probes both variants.
bool ShouldFieryBrand(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    uint32 spell = PickKnown(ctx, FIERY_BRAND_V, FIERY_BRAND);
    if (!spell) return false;
    if (!ctx.bot.is_ready(spell)) return false;
    return ctx.bot.hp_pct() <= 70 || BossLikeTargetEngaged(ctx);
}
void DoFieryBrand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 spell = PickKnown(ctx, FIERY_BRAND_V, FIERY_BRAND))
        e.cast(spell, ctx.bot.victim());
}

// Fel Devastation - 2s channel: AoE damage + self heal. Active
// mitigation layer when HP dips, or cleave on packs. Probes both
// variants.
bool ShouldFelDevastation(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    uint32 spell = PickKnown(ctx, FEL_DEVASTATION_V, FEL_DEVASTATION);
    if (!spell) return false;
    if (!ctx.bot.is_ready(spell)) return false;
    if (Pain(ctx) < 50) return false;
    if (ctx.bot.hp_pct() <= 60) return true;
    return ctx.bot.attackers_count() >= 3;
}
void DoFelDevastation(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 spell = PickKnown(ctx, FEL_DEVASTATION_V, FEL_DEVASTATION))
        e.cast(spell);
}

// Metamorphosis (tank) - big defensive CD; HP + huge armour. Probes
// all three Vengeance variants (L20, L48, legacy).
bool ShouldMetamorphosis(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    uint32 spell = 0;
    if (ctx.bot.knows_spell(METAMORPHOSIS_V2)) spell = METAMORPHOSIS_V2;
    else if (ctx.bot.knows_spell(METAMORPHOSIS_V1)) spell = METAMORPHOSIS_V1;
    else if (ctx.bot.knows_spell(METAMORPHOSIS_TANK)) spell = METAMORPHOSIS_TANK;
    if (!spell) return false;
    if (!ctx.bot.is_ready(spell)) return false;
    return ctx.bot.hp_pct() <= 35;
}
void DoMetamorphosis(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (ctx.bot.knows_spell(METAMORPHOSIS_V2)) e.cast(METAMORPHOSIS_V2);
    else if (ctx.bot.knows_spell(METAMORPHOSIS_V1)) e.cast(METAMORPHOSIS_V1);
    else if (ctx.bot.knows_spell(METAMORPHOSIS_TANK)) e.cast(METAMORPHOSIS_TANK);
}

// Soul Carver - strips armour + 3 soul fragments. Long CD; boss fights.
bool ShouldSoulCarver(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SOUL_CARVER)) return false;
    if (!ctx.bot.is_ready(SOUL_CARVER)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoSoulCarver(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SOUL_CARVER, ctx.bot.victim());
}

bool ShouldSoulBarrier(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SOUL_BARRIER)) return false;
    if (!ctx.bot.is_ready(SOUL_BARRIER)) return false;
    return ctx.bot.hp_pct() <= 60;
}
void DoSoulBarrier(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SOUL_BARRIER); }

bool ShouldDarkness(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(DARKNESS)) return false;
    if (!ctx.bot.is_ready(DARKNESS)) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoDarkness(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DARKNESS); }

bool ShouldBulkExtraction(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(BULK_EXTRACTION)) return false;
    if (!ctx.bot.is_ready(BULK_EXTRACTION)) return false;
    return ctx.bot.hp_pct() <= 60;
}
void DoBulkExtraction(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BULK_EXTRACTION); }

// ---- Interrupt / CC ----
bool ShouldDisrupt(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(DISRUPT)) return false;
    if (!ctx.bot.is_ready(DISRUPT)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    return ctx.bot.kick_target(pvp, 10.0f) != nullptr;
}
void DoDisrupt(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 10.0f))
        e.cast(DISRUPT, c->guid);
}

bool ShouldSigilOfSilence(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SIGIL_OF_SILENCE)) return false;
    if (!ctx.bot.is_ready(SIGIL_OF_SILENCE)) return false;
    if (ctx.bot.is_ready(DISRUPT)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    return ctx.bot.kick_target(pvp, 30.0f) != nullptr;
}
void DoSigilOfSilence(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 30.0f))
        e.cast_at(SIGIL_OF_SILENCE, c->x, c->y, c->z);
}

bool ShouldSigilOfMisery(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SIGIL_OF_MISERY)) return false;
    if (!ctx.bot.is_ready(SIGIL_OF_MISERY)) return false;
    return ctx.bot.enemies_within(8.0f) >= 4 && ctx.bot.hp_pct() <= 50;
}
void DoSigilOfMisery(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    float bx, by, bz;
    ctx.bot.position(bx, by, bz);
    e.cast_at(SIGIL_OF_MISERY, bx, by, bz);
}

// ---- Major offensive cooldowns ----
bool ShouldElysianDecree(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ELYSIAN_DECREE)) return false;
    if (!ctx.bot.is_ready(ELYSIAN_DECREE)) return false;
    return ctx.bot.enemies_within(10.0f) >= 2;
}
void DoElysianDecree(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(ELYSIAN_DECREE, v->x, v->y, v->z);
    else
        e.cast(ELYSIAN_DECREE);
}

bool ShouldTheHunt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(THE_HUNT)) return false;
    if (!ctx.bot.is_ready(THE_HUNT)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoTheHunt(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(THE_HUNT, ctx.bot.victim());
}

// ---- Damage / threat ----
bool ShouldImmolationAura(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(IMMOLATION_AURA)) return false;
    return ctx.bot.is_ready(IMMOLATION_AURA);
}
void DoImmolationAura(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(IMMOLATION_AURA); }

// Sigil of Flame - AoE ground tick + Pain generation. Core Vengeance
// threat tool. Fire whenever ready (Vengeance never has filler issues
// because of fragment economy). Probes the L10 Vengeance variant first.
bool ShouldSigilOfFlame(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    uint32 spell = PickKnown(ctx, SIGIL_OF_FLAME_V, SIGIL_OF_FLAME);
    if (!spell) return false;
    return ctx.bot.is_ready(spell);
}
void DoSigilOfFlame(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    uint32 spell = PickKnown(ctx, SIGIL_OF_FLAME_V, SIGIL_OF_FLAME);
    if (!spell) return;
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(spell, v->x, v->y, v->z);
    else
        e.cast(spell);
}

// Throw Glaive - ranged tag for pulls + threat ping on enemies the tank
// can't reach. Audit places this BEFORE Soul Cleave so the tank pulls
// stragglers before spending Pain on melee damage. Probes the Vengeance
// variant (204157) first, then the baseline (185123). Skipped when
// already in melee - melee uptime wins over ranged tag for current
// target.
bool ShouldThrowGlaive(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    uint32 spell = PickKnown(ctx, THROW_GLAIVE_V, THROW_GLAIVE);
    if (!spell) return false;
    if (!ctx.bot.is_ready(spell)) return false;
    // Fire when nothing is in melee range - mirrors the Havoc / baseline
    // idiom for ranged tag / gap-close. We don't want to consume the CD
    // while standing on the target.
    return ctx.bot.enemies_within(8.0f) == 0;
}
void DoThrowGlaive(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 spell = PickKnown(ctx, THROW_GLAIVE_V, THROW_GLAIVE))
        e.cast(spell, ctx.bot.victim());
}

// Spirit Bomb - detonate fragments for AoE + Frailty DR debuff.
// Vengeance only - drops Pain spender priority below Spirit Bomb when
// fragments are stockpiled. We don't have a fragment counter in the
// snapshot view yet, so we proxy via Pain >= 40 + 2+ enemies; the spell
// will fizzle if no fragments are held but won't cost Pain.
bool ShouldSpiritBomb(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SPIRIT_BOMB)) return false;
    if (!ctx.bot.is_ready(SPIRIT_BOMB)) return false;
    if (Pain(ctx) < 40) return false;
    return ctx.aoe_preference || ctx.bot.enemies_within(8.0f) >= 2;
}
void DoSpiritBomb(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SPIRIT_BOMB); }

bool ShouldSoulCleave(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SOUL_CLEAVE)) return false;
    if (!ctx.bot.is_ready(SOUL_CLEAVE)) return false;
    if (Pain(ctx) < 30) return false;
    return true;
}
void DoSoulCleave(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SOUL_CLEAVE, ctx.bot.victim());
}

bool ShouldFracture(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FRACTURE)) return false;
    return ctx.bot.is_ready(FRACTURE);
}
void DoFracture(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FRACTURE, ctx.bot.victim());
}

// Shear - the L1 Vengeance Pain generator. Replaces Demon's Bite from
// L1 onwards for Vengeance bots. Yields to Fracture (talent) when both
// are known.
bool ShouldShear(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(FRACTURE)) return false;
    return ctx.bot.knows_spell(SHEAR);
}
void DoShear(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHEAR, ctx.bot.victim());
}

bool ShouldInfernalStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(INFERNAL_STRIKE)) return false;
    if (!ctx.bot.is_ready(INFERNAL_STRIKE)) return false;
    return ctx.bot.enemies_within(8.0f) == 0 || ctx.bot.enemies_within(8.0f) >= 3;
}
void DoInfernalStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(INFERNAL_STRIKE, v->x, v->y, v->z);
    else
        e.cast(INFERNAL_STRIKE);
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

// Rule order (audit, 2026-05-27 - tank priority):
//   1. Threat tools      - Torment taunt, Sigil of Chains pack pull
//   2. Active mitigation - Demon Spikes (physical) > Fiery Brand
//                          (boss DR) > Fel Devastation (heal+AoE)
//   3. Big defensives    - Metamorphosis (35%) > Darkness > Soul
//                          Barrier > Bulk Extraction
//   4. Interrupts / CC   - Disrupt > Sigil of Silence > Sigil of
//                          Misery (panic fear)
//   5. Major CDs         - Elysian Decree, The Hunt, Soul Carver
//   6. Damage / threat   - Sigil of Flame > Throw Glaive (ranged
//                          tag) > Spirit Bomb (AoE+frailty) > Soul
//                          Cleave (Pain spender + heal) >
//                          Immolation Aura > Infernal Strike
//   7. Generator         - Fracture (talented) > Shear (baseline)
//   8. Engage            - start_attack to keep swings going
ApRule const kRules[] = {
    { ShouldTorment,         DoTorment,         "Torment (taunt)"             },
    { ShouldSigilOfChains,   DoSigilOfChains,   "Sigil of Chains (4+)"        },
    { ShouldDemonSpikes,     DoDemonSpikes,     "Demon Spikes (mitigate)"     },
    { ShouldFieryBrand,      DoFieryBrand,      "Fiery Brand (boss/elite DR)" },
    { ShouldFelDevastation,  DoFelDevastation,  "Fel Devastation (heal/AoE)"  },
    { ShouldMetamorphosis,   DoMetamorphosis,   "Metamorphosis (<=35%)"       },
    { ShouldDarkness,        DoDarkness,        "Darkness (<=50%)"            },
    { ShouldSoulBarrier,     DoSoulBarrier,     "Soul Barrier (<=60%)"        },
    { ShouldBulkExtraction,  DoBulkExtraction,  "Bulk Extraction (<=60%)"     },
    { ShouldDisrupt,         DoDisrupt,         "Disrupt (interrupt)"         },
    { ShouldSigilOfSilence,  DoSigilOfSilence,  "Sigil of Silence (interrupt fb)" },
    { ShouldSigilOfMisery,   DoSigilOfMisery,   "Sigil of Misery (panic)"     },
    { ShouldElysianDecree,   DoElysianDecree,   "Elysian Decree (2+ AoE)"     },
    { ShouldTheHunt,         DoTheHunt,         "The Hunt"                    },
    { ShouldSoulCarver,      DoSoulCarver,      "Soul Carver (boss)"          },
    { ShouldSigilOfFlame,    DoSigilOfFlame,    "Sigil of Flame"              },
    { ShouldThrowGlaive,     DoThrowGlaive,     "Throw Glaive (ranged tag)"   },
    { ShouldSpiritBomb,      DoSpiritBomb,      "Spirit Bomb (2+ AoE)"        },
    { ShouldSoulCleave,      DoSoulCleave,      "Soul Cleave (Pain>=30)"      },
    { ShouldImmolationAura,  DoImmolationAura,  "Immolation Aura"             },
    { ShouldInfernalStrike,  DoInfernalStrike,  "Infernal Strike"             },
    { ShouldFracture,        DoFracture,        "Fracture (talent generator)" },
    { ShouldShear,           DoShear,           "Shear (generator)"           },
    { AlwaysInCombat,        DoAutoAttack,      "Engage auto attack"          },
};

} // anonymous

void RegisterApl_DemonHunter_Vengeance()
{
    constexpr uint32 SPEC_DEMONHUNTER_VENGEANCE = 581;
    RegisterRotation(CLASS_DEMON_HUNTER, SPEC_DEMONHUNTER_VENGEANCE, ApRotation{kRules});
}

} // namespace Playerbot::Combat
