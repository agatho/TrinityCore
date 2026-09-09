// Havoc Demon Hunter - WoW 12.1.0.69587 (Midnight) rotation. Fury melee
// with Eye Beam burst (Demonic -> free Metamorphosis), Blade Dance (First
// Blood makes it a single-target spender too), Chaos Strike main spender,
// Immolation Aura ground tick, Fel Rush / Felblade mobility, Metamorphosis
// burst form (Chaos Strike / Blade Dance become Annihilation / Death Sweep
// while the 162264 demon-form aura is up). Talent layer: Essence Break
// (debuff window), Sigil of Flame (baseline AoE bleed + Fury), The Hunt
// (gap close + DoT), Felblade (Fury builder + charge), Vengeful Retreat.
//
// Survival: Blur (DR + dodge), Darkness (group dodge). CC: Disrupt
// interrupt, Chaos Nova (PBAoE stun), Sigil of Misery (fear).
//
// ---- Validated IDs (WoW 12.1.0.69587 kit: SkillLineAbility +
//      SpecializationSpells + simc trait data, Fel-Scarred raid build) ----
//
//   Core builders / spenders:
//     162243 Demon's Bite              - Fury generator (spec spell)
//     162794 Chaos Strike              - Fury spender (spec spell). 197125
//                                        is the Chaos Strike PASSIVE (refund
//                                        chance) - never cast it.
//     201427 Annihilation              - Meta-form Chaos Strike. Override
//                                        spell granted by aura 162264, not
//                                        in the spellbook: gate on the aura
//                                        + knows_spell(Chaos Strike).
//     188499 Blade Dance               - Spender, L14. 320402 is the
//                                        "Rank 2" cooldown passive.
//     210152 Death Sweep               - Meta-form Blade Dance (override,
//                                        same gating as Annihilation)
//     232893 Felblade (talent [R])     - 15y charge + Fury
//
//   Movement / range:
//     185123 Throw Glaive              - Ranged opener / kiting
//     195072 Fel Rush                  - Forward dash + damage
//     198793 Vengeful Retreat (talent) - Backward dash + AoE damage
//
//   Cooldowns:
//     258920 Immolation Aura           - Self-buff AoE + Fury (Burning Hatred)
//     198013 Eye Beam (talent [R])     - Channelled AoE / Demonic trigger
//     191427 Metamorphosis             - Burst form (cast id); 162264 is the
//                                        demon-form aura we test for
//     258860 Essence Break (talent)    - vulnerability debuff window
//     204596 Sigil of Flame            - Ground AoE bleed + Fury
//     370965 The Hunt (talent [R])     - leap + DoT (Havoc-only in 12.1)
//
//   CC / utility:
//     183752 Disrupt                   - Interrupt
//     179057 Chaos Nova (talent)       - PBAoE stun
//     217832 Imprison (talent)         - Single-target incap (no rule yet)
//     207684 Sigil of Misery (talent)  - AoE fear
//
//   Defensive:
//     198589 Blur                      - damage reduction + dodge
//     196718 Darkness (talent)         - Group dodge
//
// ---- Skipped spells (and why) ----
//
//   342817/1244557 Glaive Tempest - In 12.1 Glaive Tempest is a PASSIVE
//                               (1244557: the final Blade Dance slash
//                               launches the glaives on 2+ targets). No
//                               active cast; not in the raid build anyway.
//   258925 Fel Barrage        - Not learnable by Havoc in 12.1 (removed).
//   211881 Fel Eruption       - Not learnable by Havoc in 12.1 (removed).
//   196555 Netherwalk         - Not learnable by Havoc in 12.1 (removed).
//   278326 Consume Magic      - Purge; the snapshot has no purgeable-buff
//                               predicate, so no rule.
//   203555 Demon Blades       - Passive replacing Demon's Bite; the
//                               generator rule is knows_spell-gated so it
//                               transparently drops out.
//   213410 Demonic / 206416 First Blood / 388112 Chaotic Transformation
//                             - Passives that shape Eye Beam / Blade Dance
//                               priority; reflected in the ladder order.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated) ----
constexpr uint32 DEMONS_BITE          = 162243;       // spec spell
constexpr uint32 CHAOS_STRIKE         = 162794;       // spec spell (197125 = passive)
constexpr uint32 ANNIHILATION         = 201427;       // Meta override of Chaos Strike
constexpr uint32 BLADE_DANCE          = 188499;       // L14 (320402 = rank-2 passive)
constexpr uint32 DEATH_SWEEP          = 210152;       // Meta override of Blade Dance
constexpr uint32 IMMOLATION_AURA      = 258920;
constexpr uint32 EYE_BEAM             = 198013;       // talent [R]
constexpr uint32 FEL_RUSH             = 195072;
constexpr uint32 FELBLADE             = 232893;       // talent [R] - charge + Fury
constexpr uint32 VENGEFUL_RETREAT     = 198793;       // talent [R]
constexpr uint32 METAMORPHOSIS        = 191427;       // cast id
constexpr uint32 METAMORPHOSIS_BUFF   = 162264;       // Havoc demon-form aura
constexpr uint32 ESSENCE_BREAK        = 258860;       // talent [R]
constexpr uint32 SIGIL_OF_FLAME       = 204596;
constexpr uint32 THE_HUNT             = 370965;       // talent [R]
constexpr uint32 THROW_GLAIVE         = 185123;
constexpr uint32 DISRUPT              = 183752;
constexpr uint32 CHAOS_NOVA           = 179057;       // talent [R]
constexpr uint32 IMPRISON             = 217832;       // talent [R] (no rule yet)
constexpr uint32 SIGIL_OF_MISERY      = 207684;       // talent [R]
constexpr uint32 BLUR                 = 198589;
constexpr uint32 DARKNESS             = 196718;       // talent [R]

constexpr uint8 POWER_FURY_IDX = 17;       // POWER_FURY in WoW 12.x enum

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

int32 Fury(ApPredicateContext const& ctx) { return ctx.bot.power(POWER_FURY_IDX); }
// Demon form: the cast (191427) applies the 162264 transform aura, which is
// what grants the Annihilation / Death Sweep overrides.
bool InMeta(ApPredicateContext const& ctx) { return ctx.bot.has_aura(METAMORPHOSIS_BUFF); }

// ---- Survival ----
bool ShouldBlur(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(BLUR)) return false;
    if (!ctx.bot.is_ready(BLUR)) return false;
    // PvP: bump the threshold so the DR catches the burst.
    const int32 threshold = ctx.pvp.under_player_attack ? 60 : 50;
    return ctx.bot.hp_pct() <= threshold;
}
void DoBlur(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLUR); }

bool ShouldDarkness(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(DARKNESS)) return false;
    if (!ctx.bot.is_ready(DARKNESS)) return false;
    int wounded = 0;
    if (auto const* members = ctx.group.members())
        for (auto const& m : *members)
            if (m.online && m.max_hp > 0 && (m.hp * 100) / m.max_hp <= 60)
                if (++wounded >= 3) return true;
    return false;
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

bool ShouldChaosNova(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(CHAOS_NOVA)) return false;
    if (!ctx.bot.is_ready(CHAOS_NOVA)) return false;
    return ctx.bot.enemies_within(8.0f) >= 3;
}
void DoChaosNova(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CHAOS_NOVA); }

bool ShouldSigilOfMisery(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SIGIL_OF_MISERY)) return false;
    if (!ctx.bot.is_ready(SIGIL_OF_MISERY)) return false;
    return ctx.bot.enemies_within(8.0f) >= 3 && ctx.bot.hp_pct() <= 50;
}
void DoSigilOfMisery(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    float bx, by, bz;
    ctx.bot.position(bx, by, bz);
    e.cast_at(SIGIL_OF_MISERY, bx, by, bz);
}

// ---- Major offensive cooldowns ----
bool ShouldMetamorphosis(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(METAMORPHOSIS)) return false;
    if (!ctx.bot.is_ready(METAMORPHOSIS)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(10.0f) >= 3;
}
void DoMetamorphosis(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(METAMORPHOSIS); }

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

bool ShouldEssenceBreak(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ESSENCE_BREAK)) return false;
    if (!ctx.bot.is_ready(ESSENCE_BREAK)) return false;
    // Needs a spender to follow inside the window (Chaos Strike 40 Fury).
    return Fury(ctx) >= 40;
}
void DoEssenceBreak(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ESSENCE_BREAK, ctx.bot.victim());
}

// Eye Beam - 12.1 raid build takes Demonic (213410: Eye Beam grants a short
// Metamorphosis) and Cycle of Hatred, so the simc priority casts it on
// cooldown in every target count. Only the 30-Fury cost gates it.
bool ShouldEyeBeam(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(EYE_BEAM)) return false;
    if (!ctx.bot.is_ready(EYE_BEAM)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(EYE_BEAM)) return false;
    return Fury(ctx) >= 30;
}
void DoEyeBeam(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(EYE_BEAM, ctx.bot.victim());
}

bool ShouldSigilOfFlame(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SIGIL_OF_FLAME)) return false;
    if (!ctx.bot.is_ready(SIGIL_OF_FLAME)) return false;
    return ctx.bot.enemies_within(10.0f) >= 2;
}
void DoSigilOfFlame(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(SIGIL_OF_FLAME, v->x, v->y, v->z);
    else
        e.cast(SIGIL_OF_FLAME);
}

bool ShouldImmolationAura(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(IMMOLATION_AURA)) return false;
    return ctx.bot.is_ready(IMMOLATION_AURA);
}
void DoImmolationAura(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(IMMOLATION_AURA); }

// ---- Mobility ----
bool ShouldFelRush(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FEL_RUSH)) return false;
    if (!ctx.bot.is_ready(FEL_RUSH)) return false;
    return ctx.bot.enemies_within(8.0f) == 0;
}
void DoFelRush(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FEL_RUSH, ctx.bot.victim());
}

bool ShouldVengefulRetreat(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(VENGEFUL_RETREAT)) return false;
    if (!ctx.bot.is_ready(VENGEFUL_RETREAT)) return false;
    return ctx.bot.hp_pct() <= 40 && ctx.bot.enemies_within(8.0f) >= 2;
}
void DoVengefulRetreat(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(VENGEFUL_RETREAT); }

bool ShouldThrowGlaive(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(THROW_GLAIVE)) return false;
    if (!ctx.bot.is_ready(THROW_GLAIVE)) return false;
    return ctx.bot.enemies_within(8.0f) == 0;
}
void DoThrowGlaive(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(THROW_GLAIVE, ctx.bot.victim());
}

// ---- Spenders ----
// Death Sweep / Annihilation are Metamorphosis OVERRIDE spells: they are
// never in the spellbook (knows_spell / is_ready on their ids is always
// false), the 162264 aura grants them. Gate on the aura + the base spell
// (shared cooldown category) and cast the override id.
bool ShouldDeathSweep(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!InMeta(ctx)) return false;
    if (!ctx.bot.knows_spell(BLADE_DANCE)) return false;
    if (!ctx.bot.is_ready(BLADE_DANCE)) return false;
    return Fury(ctx) >= 35;
}
void DoDeathSweep(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DEATH_SWEEP); }

// Blade Dance - with First Blood [R] it is the best Fury spender in every
// target count (simc casts it on cooldown), so no AoE gate. Outside Meta
// only (Death Sweep covers demon form).
bool ShouldBladeDance(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (InMeta(ctx)) return false;
    if (!ctx.bot.knows_spell(BLADE_DANCE)) return false;
    if (!ctx.bot.is_ready(BLADE_DANCE)) return false;
    return Fury(ctx) >= 35;
}
void DoBladeDance(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLADE_DANCE); }

bool ShouldAnnihilation(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!InMeta(ctx)) return false;
    if (!ctx.bot.knows_spell(CHAOS_STRIKE)) return false;
    if (!ctx.bot.is_ready(CHAOS_STRIKE)) return false;
    return Fury(ctx) >= 40;
}
void DoAnnihilation(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ANNIHILATION, ctx.bot.victim());
}

bool ShouldChaosStrike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (InMeta(ctx)) return false;
    if (!ctx.bot.knows_spell(CHAOS_STRIKE)) return false;
    if (!ctx.bot.is_ready(CHAOS_STRIKE)) return false;
    return Fury(ctx) >= 40;
}
void DoChaosStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CHAOS_STRIKE, ctx.bot.victim());
}

// Felblade (talent [R]) - 15y charge that generates Fury. simc: after the
// spenders, before the generator; also doubles as a gap close.
bool ShouldFelblade(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FELBLADE)) return false;
    if (!ctx.bot.is_ready(FELBLADE)) return false;
    return Fury(ctx) <= 70 || ctx.bot.enemies_within(8.0f) == 0;
}
void DoFelblade(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FELBLADE, ctx.bot.victim());
}

bool ShouldDemonsBite(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    return ctx.bot.knows_spell(DEMONS_BITE);
}
void DoDemonsBite(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DEMONS_BITE, ctx.bot.victim());
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

// Rule order (12.1 Fel-Scarred simc priority, 2026-09-09):
//   1. Panic survival   - Blur (DR)
//   2. Group defensive  - Darkness on multi-wounded
//   3. Interrupts / CC  - Disrupt > Chaos Nova (3+ stun) > Sigil of
//                         Misery (panic fear)
//   4. Mobility peel    - Vengeful Retreat (low HP + cluster)
//   5. Major CDs        - Metamorphosis, The Hunt, Essence Break
//   6. Eye Beam         - on CD (Demonic trigger)
//   7. AoE bleed        - Sigil of Flame (2+)
//   8. Immolation Aura  - On CD; Fury proc
//   9. Spenders         - Death Sweep (Meta) > Blade Dance > Annihilation
//                         (Meta) > Chaos Strike (40+ Fury)
//  10. Builder          - Felblade (Fury <= 70 or gap close)
//  11. Mobility filler  - Fel Rush / Throw Glaive when nothing in melee
//  12. Generator        - Demon's Bite
//  13. Engage           - start_attack to keep swings going
ApRule const kRules[] = {
    { ShouldBlur,            DoBlur,            "Blur (<=50%)"               },
    { ShouldDarkness,        DoDarkness,        "Darkness (3+ wounded)"      },
    { ShouldDisrupt,         DoDisrupt,         "Disrupt (interrupt)"        },
    { ShouldChaosNova,       DoChaosNova,       "Chaos Nova (3+ AoE stun)"   },
    { ShouldSigilOfMisery,   DoSigilOfMisery,   "Sigil of Misery (panic)"    },
    { ShouldVengefulRetreat, DoVengefulRetreat, "Vengeful Retreat (peel)"    },
    { ShouldMetamorphosis,   DoMetamorphosis,   "Metamorphosis"              },
    { ShouldTheHunt,         DoTheHunt,         "The Hunt"                   },
    { ShouldEssenceBreak,    DoEssenceBreak,    "Essence Break"              },
    { ShouldEyeBeam,         DoEyeBeam,         "Eye Beam (on CD)"           },
    { ShouldSigilOfFlame,    DoSigilOfFlame,    "Sigil of Flame (2+ AoE)"    },
    { ShouldImmolationAura,  DoImmolationAura,  "Immolation Aura"            },
    { ShouldDeathSweep,      DoDeathSweep,      "Death Sweep (Meta)"         },
    { ShouldBladeDance,      DoBladeDance,      "Blade Dance"                },
    { ShouldAnnihilation,    DoAnnihilation,    "Annihilation (Meta)"        },
    { ShouldChaosStrike,     DoChaosStrike,     "Chaos Strike (Fury>=40)"    },
    { ShouldFelblade,        DoFelblade,        "Felblade (builder)"         },
    { ShouldFelRush,         DoFelRush,         "Fel Rush (gap close)"       },
    { ShouldThrowGlaive,     DoThrowGlaive,     "Throw Glaive (range)"       },
    { ShouldDemonsBite,      DoDemonsBite,      "Demon's Bite (generator)"   },
    { AlwaysInCombat,        DoAutoAttack,      "Engage auto attack"         },
};

} // anonymous

void RegisterApl_DemonHunter_Havoc()
{
    constexpr uint32 SPEC_DEMONHUNTER_HAVOC = 577;
    RegisterRotation(CLASS_DEMON_HUNTER, SPEC_DEMONHUNTER_HAVOC, ApRotation{kRules});
}

} // namespace Playerbot::Combat
