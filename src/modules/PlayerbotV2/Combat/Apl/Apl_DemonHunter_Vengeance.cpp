// Vengeance Demon Hunter - WoW 12.1.0.69587 (Midnight) tank rotation. Fury
// resource, Soul Fragment management (TC tracks fragments as stacks of the
// 203981 counter aura), Demon Spikes / Metamorphosis active mitigation,
// Fiery Brand damage-reduction debuff, Soul Cleave Fury spender that
// consumes fragments for self-heal. Spirit Bomb detonates fragments for
// AoE + Frailty. Group utility: Sigil of Silence (M+ build), Sigil of
// Misery (fear), Darkness (group dodge).
//
// ---- Validated IDs (WoW 12.1.0.69587 kit: SkillLineAbility +
//      SpecializationSpells 581 + simc trait data, Annihilator raid build) ----
//
//   Core builders / spenders:
//     263642 Fracture                  - Spec generator (overrides Demon's
//                                        Bite). Shear 203783 is a PASSIVE
//                                        in 12.1 - never cast it.
//     228477 Soul Cleave               - 35 Fury spender + self-heal +
//                                        consumes up to 2 fragments.
//     247454 Spirit Bomb (talent [R])  - Fragment detonator AoE + Frailty.
//     232893 Felblade (talent [R])     - 15y charge + Fury.
//     203981 Soul Fragments            - counter aura (stacks = fragments);
//                                        read-only, never cast.
//
//   Active mitigation / defensive:
//     203720 Demon Spikes              - cast id; 203819 is the armour/parry
//                                        buff we test for. 321028 is the
//                                        "Rank 2" parry passive.
//     204021 Fiery Brand (talent [R])  - DR debuff; 320962 is the rank-2
//                                        DoT passive.
//     187827 Metamorphosis             - Vengeance spec spell (overrides
//                                        191427); 321067/321068 are rank
//                                        passives.
//     212084 Fel Devastation (talent)  - 2s channel: AoE damage + heal
//                                        (320639 = rank-2 heal passive).
//     196718 Darkness (talent [R])     - Group dodge AoE.
//
//   AoE / threat:
//     204596 Sigil of Flame            - Ground AoE tick + Fury (320794 =
//                                        rank-2 DoT passive).
//     390163 Sigil of Spite (talent)   - Chaos burst + shatters fragments.
//     258920 Immolation Aura           - Self-buff AoE + Fury.
//     189110 Infernal Strike           - Spec leap + AoE damage.
//     207407 Soul Carver (talent)      - 1min CD - damage + fragments.
//
//   Range / pull:
//     204157 Throw Glaive (Vengeance)  - Spec override of 185123 (high
//                                        threat). Both probed.
//     185123 Throw Glaive (baseline)   - Generic id kept as fallback.
//
//   CC / utility:
//     183752 Disrupt                   - Interrupt.
//     202137 Sigil of Silence (M+)     - AoE caster silence (interrupt fb).
//     207684 Sigil of Misery (talent)  - AoE fear.
//     202138 Sigil of Chains           - AoE pull/clump.
//     185245 Torment                   - Taunt.
//     217832 Imprison (M+)             - Single-target incap (no rule yet).
//
// ---- Skipped spells (and why) ----
//
//   203783 Shear              - Passive in 12.1 (fragment-on-hit helper);
//                               Fracture is the spec generator.
//   263648/1265924 Soul Barrier - 1265924 is a PASSIVE (Spirit Bomb
//                               shields you) and not in the raid build.
//   320341 Bulk Extraction    - Not learnable by Vengeance in 12.1.
//   306830 Elysian Decree     - Not learnable in 12.1 (Sigil of Spite is
//                               the successor).
//   370965 The Hunt           - Havoc-only talent in 12.1.
//   198793 Vengeful Retreat   - Selected class talent, but the tank
//                               vaulting away from its pack is a threat
//                               loss; the simc Annihilator list never
//                               casts it. No rule.
//   179057 Chaos Nova         - M+ build only; left out to keep the
//                               ladder readable.
//   278326 Consume Magic      - Purge; no purgeable-buff predicate.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated) ----
constexpr uint32 FRACTURE             = 263642;       // spec generator
constexpr uint32 SOUL_CLEAVE          = 228477;       // spec spender (35 Fury)
constexpr uint32 IMMOLATION_AURA      = 258920;
constexpr uint32 SIGIL_OF_FLAME       = 204596;       // (320794 = rank-2 passive)
constexpr uint32 SIGIL_OF_SILENCE     = 202137;       // talent [M]
constexpr uint32 SIGIL_OF_MISERY      = 207684;       // talent [R]
constexpr uint32 SIGIL_OF_CHAINS      = 202138;       // spec spell
constexpr uint32 SIGIL_OF_SPITE       = 390163;       // talent [R]
constexpr uint32 INFERNAL_STRIKE      = 189110;       // spec spell
constexpr uint32 FELBLADE             = 232893;       // talent [R]
constexpr uint32 DEMON_SPIKES         = 203720;       // cast id (321028 = rank-2 passive)
constexpr uint32 DEMON_SPIKES_BUFF    = 203819;       // armour/parry buff
constexpr uint32 METAMORPHOSIS_TANK   = 187827;       // spec spell (321067/8 = passives)
constexpr uint32 FIERY_BRAND          = 204021;       // talent [R] (320962 = rank-2 passive)
constexpr uint32 SPIRIT_BOMB          = 247454;       // talent [R]
constexpr uint32 SOUL_FRAGMENTS       = 203981;       // counter aura (stacks)
constexpr uint32 DISRUPT              = 183752;
constexpr uint32 TORMENT              = 185245;
constexpr uint32 DARKNESS             = 196718;       // talent [R]
constexpr uint32 IMPRISON             = 217832;       // talent [M] (no rule yet)
constexpr uint32 FEL_DEVASTATION      = 212084;       // talent [R] (320639 = rank-2 passive)
constexpr uint32 SOUL_CARVER          = 207407;       // talent - 1min CD
constexpr uint32 THROW_GLAIVE         = 185123;       // baseline
constexpr uint32 THROW_GLAIVE_V       = 204157;       // Vengeance override

// 12.x Vengeance runs on FURY (index 17), not the Legion-era Pain (18):
// no TC code grants POWER_PAIN to players anymore, so power[18] was
// permanently 0 and every Pain-gated rule below returned false forever —
// Vengeance tanks never cast Demon Spikes / Soul Cleave / Spirit Bomb
// (audit B12). Thresholds below are display-unit Fury costs.
constexpr uint8 POWER_PAIN_IDX = 17;     // POWER_FURY (see above)

// Returns the spell ID the bot actually knows, preferring the spec
// override over the generic baseline ID. Returns 0 if neither.
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
// Soul Fragments currently orbiting the bot. TC keeps the count as stacks
// of the 203981 counter aura (SPELL_DH_SOUL_FRAGMENT_COUNTER); 0 when the
// aura is absent, which makes every fragment-gated rule fall back to the
// no-fragment branch.
int32 Fragments(ApPredicateContext const& ctx) { return ctx.bot.aura_stacks(SOUL_FRAGMENTS); }

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
// Demon Spikes - core physical mitigation. Armour/parry buff (203819), 2
// charges, no resource cost. Refresh window: cast when not active OR
// remaining <= 1s so mitigation never gaps at boss damage rates.
bool ShouldDemonSpikes(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DEMON_SPIKES)) return false;
    if (!ctx.bot.is_ready(DEMON_SPIKES)) return false;
    if (AuraEntry const* a = ctx.bot.find_aura(DEMON_SPIKES_BUFF))
        return a->remaining.count() <= 1000;
    return true;
}
void DoDemonSpikes(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DEMON_SPIKES); }

// Fiery Brand - boss/elite damage-reduction debuff. Prefer big targets;
// also fire defensively when HP dips.
bool ShouldFieryBrand(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FIERY_BRAND)) return false;
    if (!ctx.bot.is_ready(FIERY_BRAND)) return false;
    return ctx.bot.hp_pct() <= 70 || BossLikeTargetEngaged(ctx);
}
void DoFieryBrand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FIERY_BRAND, ctx.bot.victim());
}

// Fel Devastation - 2s channel: AoE damage + self heal (50 Fury). Active
// mitigation layer when HP dips, cleave on packs, or a Fury dump when
// capped (simc: fel_devastation,if=fury>85).
bool ShouldFelDevastation(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FEL_DEVASTATION)) return false;
    if (!ctx.bot.is_ready(FEL_DEVASTATION)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(FEL_DEVASTATION)) return false;
    if (Pain(ctx) < 50) return false;
    if (ctx.bot.hp_pct() <= 60) return true;
    return ctx.bot.attackers_count() >= 3 || Pain(ctx) >= 85;
}
void DoFelDevastation(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FEL_DEVASTATION); }

// Metamorphosis (tank, 187827) - big defensive CD; HP + huge armour.
bool ShouldMetamorphosis(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(METAMORPHOSIS_TANK)) return false;
    if (!ctx.bot.is_ready(METAMORPHOSIS_TANK)) return false;
    return ctx.bot.hp_pct() <= 35;
}
void DoMetamorphosis(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(METAMORPHOSIS_TANK); }

// Soul Carver - damage + fragments over 3s. Long CD; bosses and packs,
// only when the fragment bank has room (simc: soul_fragments<=3).
bool ShouldSoulCarver(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SOUL_CARVER)) return false;
    if (!ctx.bot.is_ready(SOUL_CARVER)) return false;
    if (Fragments(ctx) > 3) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(8.0f) >= 3;
}
void DoSoulCarver(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SOUL_CARVER, ctx.bot.victim());
}

bool ShouldDarkness(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(DARKNESS)) return false;
    if (!ctx.bot.is_ready(DARKNESS)) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoDarkness(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DARKNESS); }

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
// Sigil of Spite - ground sigil: Chaos burst + shatters fragments from
// everything inside. simc fires it whenever the fragment bank has room.
bool ShouldSigilOfSpite(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SIGIL_OF_SPITE)) return false;
    if (!ctx.bot.is_ready(SIGIL_OF_SPITE)) return false;
    return Fragments(ctx) <= 3;
}
void DoSigilOfSpite(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(SIGIL_OF_SPITE, v->x, v->y, v->z);
    else
        e.cast(SIGIL_OF_SPITE);
}

// ---- Damage / threat ----
bool ShouldImmolationAura(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(IMMOLATION_AURA)) return false;
    return ctx.bot.is_ready(IMMOLATION_AURA);
}
void DoImmolationAura(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(IMMOLATION_AURA); }

// Sigil of Flame - AoE ground tick + Fury generation. Core Vengeance
// threat tool. Fire whenever ready (Vengeance never has filler issues
// because of fragment economy).
bool ShouldSigilOfFlame(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SIGIL_OF_FLAME)) return false;
    return ctx.bot.is_ready(SIGIL_OF_FLAME);
}
void DoSigilOfFlame(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(SIGIL_OF_FLAME, v->x, v->y, v->z);
    else
        e.cast(SIGIL_OF_FLAME);
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

// Spirit Bomb - detonate fragments for AoE + Frailty DR debuff. 40 Fury.
// simc: spirit_bomb,if=soul_fragments>=fragment_target (5 single target,
// 4 on packs / in Meta). Uses the 203981 fragment counter; when the aura
// is absent (0 stacks) the rule stays off and Soul Cleave takes over.
bool ShouldSpiritBomb(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SPIRIT_BOMB)) return false;
    if (!ctx.bot.is_ready(SPIRIT_BOMB)) return false;
    if (Pain(ctx) < 40) return false;
    const bool pack = ctx.aoe_preference || ctx.bot.enemies_within(8.0f) >= 2;
    return Fragments(ctx) >= (pack ? 4 : 5);
}
void DoSpiritBomb(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SPIRIT_BOMB); }

// Soul Cleave - 35 Fury spender + heal, eats up to 2 fragments. When
// Spirit Bomb is talented let the fragments pool for it: only cleave on
// an empty bank or when Fury is about to cap (simc: soul_fragments<=1 |
// fury.deficit<=15).
bool ShouldSoulCleave(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SOUL_CLEAVE)) return false;
    if (!ctx.bot.is_ready(SOUL_CLEAVE)) return false;
    if (Pain(ctx) < 35) return false;
    if (!ctx.bot.knows_spell(SPIRIT_BOMB)) return true;
    return Fragments(ctx) <= 1 || Pain(ctx) >= 85;
}
void DoSoulCleave(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SOUL_CLEAVE, ctx.bot.victim());
}

// Fracture - the Vengeance generator (spec spell, 2 charges, shatters
// fragments). Fire whenever a charge is up.
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

// Felblade (talent [R]) - 15y charge + Fury. simc filler after Fracture;
// also the gap close when nothing is in melee.
bool ShouldFelblade(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FELBLADE)) return false;
    if (!ctx.bot.is_ready(FELBLADE)) return false;
    return Pain(ctx) <= 60 || ctx.bot.enemies_within(8.0f) == 0;
}
void DoFelblade(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FELBLADE, ctx.bot.victim());
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

// Rule order (12.1 Annihilator simc priority, 2026-09-09 - tank priority):
//   1. Threat tools      - Torment taunt, Sigil of Chains pack pull
//   2. Active mitigation - Demon Spikes (physical) > Fiery Brand
//                          (boss DR) > Fel Devastation (heal+AoE)
//   3. Big defensives    - Metamorphosis (35%) > Darkness
//   4. Interrupts / CC   - Disrupt > Sigil of Silence > Sigil of
//                          Misery (panic fear)
//   5. Major CDs         - Sigil of Spite, Soul Carver (fragment bank)
//   6. Damage / threat   - Sigil of Flame > Throw Glaive (ranged
//                          tag) > Spirit Bomb (fragments) > Soul
//                          Cleave (Fury spender + heal) >
//                          Immolation Aura > Infernal Strike
//   7. Generator         - Fracture > Felblade
//   8. Engage            - start_attack to keep swings going
ApRule const kRules[] = {
    { ShouldTorment,         DoTorment,         "Torment (taunt)"             },
    { ShouldSigilOfChains,   DoSigilOfChains,   "Sigil of Chains (4+)"        },
    { ShouldDemonSpikes,     DoDemonSpikes,     "Demon Spikes (mitigate)"     },
    { ShouldFieryBrand,      DoFieryBrand,      "Fiery Brand (boss/elite DR)" },
    { ShouldFelDevastation,  DoFelDevastation,  "Fel Devastation (heal/AoE)"  },
    { ShouldMetamorphosis,   DoMetamorphosis,   "Metamorphosis (<=35%)"       },
    { ShouldDarkness,        DoDarkness,        "Darkness (<=50%)"            },
    { ShouldDisrupt,         DoDisrupt,         "Disrupt (interrupt)"         },
    { ShouldSigilOfSilence,  DoSigilOfSilence,  "Sigil of Silence (interrupt fb)" },
    { ShouldSigilOfMisery,   DoSigilOfMisery,   "Sigil of Misery (panic)"     },
    { ShouldSigilOfSpite,    DoSigilOfSpite,    "Sigil of Spite"              },
    { ShouldSoulCarver,      DoSoulCarver,      "Soul Carver (boss/pack)"     },
    { ShouldSigilOfFlame,    DoSigilOfFlame,    "Sigil of Flame"              },
    { ShouldThrowGlaive,     DoThrowGlaive,     "Throw Glaive (ranged tag)"   },
    { ShouldSpiritBomb,      DoSpiritBomb,      "Spirit Bomb (fragments)"     },
    { ShouldSoulCleave,      DoSoulCleave,      "Soul Cleave (Fury>=35)"      },
    { ShouldImmolationAura,  DoImmolationAura,  "Immolation Aura"             },
    { ShouldInfernalStrike,  DoInfernalStrike,  "Infernal Strike"             },
    { ShouldFracture,        DoFracture,        "Fracture (generator)"        },
    { ShouldFelblade,        DoFelblade,        "Felblade (builder)"          },
    { AlwaysInCombat,        DoAutoAttack,      "Engage auto attack"          },
};

} // anonymous

void RegisterApl_DemonHunter_Vengeance()
{
    constexpr uint32 SPEC_DEMONHUNTER_VENGEANCE = 581;
    RegisterRotation(CLASS_DEMON_HUNTER, SPEC_DEMONHUNTER_VENGEANCE, ApRotation{kRules});
}

} // namespace Playerbot::Combat
