// Havoc Demon Hunter - WoW 12.0 enterprise rotation. Fury melee with Eye
// Beam burst, Blade Dance dodge window, Chaos Strike main spender,
// Immolation Aura ground tick, Fel Rush mobility + damage, Metamorphosis
// burst form. Talent layer: Glaive Tempest (AoE), Fel Barrage (channel
// AoE), Essence Break (debuff window), Sigil of Flame (AoE bleed), The
// Hunt (talent - gap close + DoT), Vengeful Retreat (mobility + Momentum).
//
// Survival: Netherwalk (immune), Blur (DR + dodge), Darkness (group dodge).
// CC: Disrupt interrupt, Chaos Nova (PBAoE stun), Imprison (incap), Sigil
// of Misery (fear), Fel Eruption (talent - single-target stun).
//
// ---- Validated IDs (cross-checked against wago.tools SpellName.csv +
//      SpellLevels.csv on 2026-05-27) ----
//
//   Core builders / spenders:
//     162243 Demon's Bite              - Fury generator (no SpellLevels row;
//                                        granted via class kit, not Levels DBC)
//     197125 Chaos Strike              - Fury spender, SpellLevel=1 in
//                                        SpellLevels DBC. This is the modern
//                                        Havoc variant; the older 162794
//                                        ID has no SpellLevels row and is
//                                        retained ONLY in Apl_Baseline_DH
//                                        for pre-spec L8-9 bots.
//     201427 Annihilation              - Metamorphosis-form Chaos Strike
//     188499 Blade Dance               - AoE spender, unlocks at L14
//     320402 Blade Dance (Havoc)       - Modern Havoc spec variant at L22.
//                                        Same name; either may be live on a
//                                        given character. We probe BOTH via
//                                        knows_spell so the rotation works
//                                        from unlock through max level.
//     210152 Death Sweep               - Meta-form Blade Dance
//
//   Movement / range:
//     185123 Throw Glaive              - Ranged opener / kiting
//     195072 Fel Rush                  - Forward dash + damage
//     198793 Vengeful Retreat          - Backward dash + AoE damage
//
//   Cooldowns:
//     258920 Immolation Aura           - Self-buff AoE + Fury proc
//     198013 Eye Beam                  - Channelled AoE / Demonic burst
//     191427 Metamorphosis             - Burst form (DPS variant)
//     258860 Essence Break (talent)    - 4s vulnerability debuff window
//     342817 Glaive Tempest (talent)   - AoE storm
//     258925 Fel Barrage (talent)      - Channelled AoE
//     204596 Sigil of Flame            - Ground AoE bleed
//     370965 The Hunt                  - Hero talent - leap + DoT
//
//   CC / utility:
//     183752 Disrupt                   - Interrupt
//     179057 Chaos Nova                - PBAoE stun
//     217832 Imprison                  - Single-target incap (out-of-combat)
//     207684 Sigil of Misery           - AoE fear
//     211881 Fel Eruption (talent)     - Single-target stun (interrupt fb)
//
//   Defensive:
//     198589 Blur                      - 50% damage reduction + dodge
//     196555 Netherwalk                - Immunity (talent)
//     196718 Darkness                  - Group 20% dodge
//
// ---- Skipped spells (and why) ----
//
//   178940 Shattered Souls    - Passive that drops a soul fragment on enemy
//                               death. No active cast; consumed by other
//                               abilities (Demonic Appetite). Tracking would
//                               belong on the snapshot, not the rotation.
//   203555 Demon Blades       - Passive talent that replaces Demon's Bite
//                               with an auto-attack Fury proc. No active
//                               button to fire; the live spec already
//                               cycles Demon's Bite as filler and the
//                               passive transparently changes its mechanic.
//   221351 Critical Strikes   - Passive crit bonus. No active.
//   278386 Demonic Wards      - Passive armour. No active.
//   206478 Demonic Appetite   - Passive talent that makes Chaos Strike
//                               spawn soul fragments. No active cast.
//   162794 Chaos Strike (old) - Legacy generic ID, no SpellLevels row.
//                               Replaced by 197125 for the Havoc rotation
//                               and only retained inside Apl_Baseline_DH
//                               for the L8-9 pre-spec window.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated) ----
constexpr uint32 DEMONS_BITE          = 162243;
constexpr uint32 CHAOS_STRIKE         = 197125;       // Havoc spec, SpellLevel=1
constexpr uint32 ANNIHILATION         = 201427;       // Meta-form Chaos Strike
constexpr uint32 BLADE_DANCE          = 188499;       // L14 unlock
constexpr uint32 BLADE_DANCE_HAVOC    = 320402;       // L22 Havoc variant
constexpr uint32 DEATH_SWEEP          = 210152;       // Meta-form Blade Dance
constexpr uint32 IMMOLATION_AURA      = 258920;
constexpr uint32 EYE_BEAM             = 198013;
constexpr uint32 FEL_RUSH             = 195072;
constexpr uint32 VENGEFUL_RETREAT     = 198793;
constexpr uint32 METAMORPHOSIS        = 191427;
constexpr uint32 ESSENCE_BREAK        = 258860;       // talent
constexpr uint32 GLAIVE_TEMPEST       = 342817;       // talent - AoE
constexpr uint32 FEL_BARRAGE          = 258925;       // talent - AoE channel
constexpr uint32 SIGIL_OF_FLAME       = 204596;
constexpr uint32 THE_HUNT             = 370965;       // talent
constexpr uint32 FEL_ERUPTION         = 211881;       // talent stun
constexpr uint32 THROW_GLAIVE         = 185123;
constexpr uint32 DISRUPT              = 183752;
constexpr uint32 CHAOS_NOVA           = 179057;
constexpr uint32 IMPRISON             = 217832;
constexpr uint32 SIGIL_OF_MISERY      = 207684;
constexpr uint32 BLUR                 = 198589;
constexpr uint32 NETHERWALK           = 196555;
constexpr uint32 DARKNESS             = 196718;

constexpr uint8 POWER_FURY_IDX = 17;       // POWER_FURY in WoW 12.0 enum

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
bool InMeta(ApPredicateContext const& ctx) { return ctx.bot.has_aura(METAMORPHOSIS); }

// ---- Survival ----
bool ShouldNetherwalk(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(NETHERWALK)) return false;
    if (!ctx.bot.is_ready(NETHERWALK)) return false;
    // PvP: bump the panic threshold so the immunity catches the burst.
    const int32 threshold = ctx.pvp.under_player_attack ? 40 : 20;
    return ctx.bot.hp_pct() <= threshold;
}
void DoNetherwalk(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(NETHERWALK); }

bool ShouldBlur(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(BLUR)) return false;
    if (!ctx.bot.is_ready(BLUR)) return false;
    return ctx.bot.hp_pct() <= 50;
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

bool ShouldFelEruption(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FEL_ERUPTION)) return false;
    if (!ctx.bot.is_ready(FEL_ERUPTION)) return false;
    if (ctx.bot.is_ready(DISRUPT)) return false;
    return ctx.bot.interruptible_caster() != nullptr;
}
void DoFelEruption(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* c = ctx.bot.interruptible_caster())
        e.cast(FEL_ERUPTION, c->guid);
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
    return Fury(ctx) >= 80;
}
void DoEssenceBreak(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ESSENCE_BREAK, ctx.bot.victim());
}

// Eye Beam - audit order requires this AoE-3+ gated to keep it from being
// spent on solo trash. Single-target damage is acceptable in burst windows
// but the audit explicitly anchors Eye Beam as the AoE primary. We honour
// that gate here (>=3) and let Glaive Tempest / Fel Barrage cover 2-target
// cleave below.
bool ShouldEyeBeam(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(EYE_BEAM)) return false;
    if (!ctx.bot.is_ready(EYE_BEAM)) return false;
    if (Fury(ctx) < 30) return false;
    if (ctx.bot.enemies_within(10.0f) >= 3) return true;
    // Single-target boss usage still allowed - Eye Beam is also the Demonic
    // (talent) trigger for free Metamorphosis. Use on bosses regardless of
    // enemy count.
    return BossLikeTargetEngaged(ctx);
}
void DoEyeBeam(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(EYE_BEAM, ctx.bot.victim());
}

bool ShouldFelBarrage(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FEL_BARRAGE)) return false;
    if (!ctx.bot.is_ready(FEL_BARRAGE)) return false;
    return ctx.bot.enemies_within(15.0f) >= 2;
}
void DoFelBarrage(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FEL_BARRAGE); }

bool ShouldGlaiveTempest(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(GLAIVE_TEMPEST)) return false;
    if (!ctx.bot.is_ready(GLAIVE_TEMPEST)) return false;
    if (Fury(ctx) < 30) return false;
    return ctx.bot.enemies_within(8.0f) >= 2;
}
void DoGlaiveTempest(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(GLAIVE_TEMPEST); }

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
bool ShouldDeathSweep(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!InMeta(ctx)) return false;
    if (!ctx.bot.knows_spell(DEATH_SWEEP)) return false;
    if (!ctx.bot.is_ready(DEATH_SWEEP)) return false;
    return Fury(ctx) >= 35;
}
void DoDeathSweep(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DEATH_SWEEP); }

// Blade Dance - audit calls for AoE 2+ gating. We probe BOTH 320402 (L22
// Havoc variant) and 188499 (L14 baseline unlock); whichever the
// character has trained is fired. Outside Meta only.
bool ShouldBladeDance(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (InMeta(ctx)) return false;
    if (Fury(ctx) < 35) return false;
    if (ctx.bot.enemies_within(8.0f) < 2) return false;
    if (ctx.bot.knows_spell(BLADE_DANCE_HAVOC) && ctx.bot.is_ready(BLADE_DANCE_HAVOC))
        return true;
    return ctx.bot.knows_spell(BLADE_DANCE) && ctx.bot.is_ready(BLADE_DANCE);
}
void DoBladeDance(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (ctx.bot.knows_spell(BLADE_DANCE_HAVOC) && ctx.bot.is_ready(BLADE_DANCE_HAVOC))
        e.cast(BLADE_DANCE_HAVOC);
    else
        e.cast(BLADE_DANCE);
}

bool ShouldAnnihilation(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!InMeta(ctx)) return false;
    if (!ctx.bot.knows_spell(ANNIHILATION)) return false;
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

// Rule order (audit, 2026-05-27):
//   1. Panic survival   - Netherwalk (immune) then Blur (DR)
//   2. Group defensive  - Darkness on multi-wounded
//   3. Interrupts / CC  - Disrupt > Fel Eruption fallback > Chaos Nova
//                         (3+ stun) > Sigil of Misery (panic fear)
//   4. Mobility peel    - Vengeful Retreat (low HP + cluster)
//   5. Major CDs        - Metamorphosis, The Hunt, Essence Break
//   6. AoE channels     - Eye Beam (3+) before Fel Barrage / Glaive
//                         Tempest before Sigil of Flame (2+)
//   7. Immolation Aura  - On CD; Painbringer / Fury proc
//   8. AoE spenders     - Death Sweep (Meta) > Blade Dance (2+)
//   9. ST spenders      - Annihilation (Meta) > Chaos Strike (40+ Fury)
//  10. Mobility filler  - Fel Rush / Throw Glaive when nothing in melee
//  11. Generator        - Demon's Bite
//  12. Engage           - start_attack to keep swings going
ApRule const kRules[] = {
    { ShouldNetherwalk,      DoNetherwalk,      "Netherwalk (<=20%)"         },
    { ShouldBlur,            DoBlur,            "Blur (<=50%)"               },
    { ShouldDarkness,        DoDarkness,        "Darkness (3+ wounded)"      },
    { ShouldDisrupt,         DoDisrupt,         "Disrupt (interrupt)"        },
    { ShouldFelEruption,     DoFelEruption,     "Fel Eruption (interrupt fb)"},
    { ShouldChaosNova,       DoChaosNova,       "Chaos Nova (3+ AoE stun)"   },
    { ShouldSigilOfMisery,   DoSigilOfMisery,   "Sigil of Misery (panic)"    },
    { ShouldVengefulRetreat, DoVengefulRetreat, "Vengeful Retreat (peel)"    },
    { ShouldMetamorphosis,   DoMetamorphosis,   "Metamorphosis"              },
    { ShouldTheHunt,         DoTheHunt,         "The Hunt"                   },
    { ShouldEssenceBreak,    DoEssenceBreak,    "Essence Break"              },
    { ShouldEyeBeam,         DoEyeBeam,         "Eye Beam (3+ AoE / boss)"   },
    { ShouldFelBarrage,      DoFelBarrage,      "Fel Barrage"                },
    { ShouldGlaiveTempest,   DoGlaiveTempest,   "Glaive Tempest (2+ AoE)"    },
    { ShouldSigilOfFlame,    DoSigilOfFlame,    "Sigil of Flame (2+ AoE)"    },
    { ShouldImmolationAura,  DoImmolationAura,  "Immolation Aura"            },
    { ShouldDeathSweep,      DoDeathSweep,      "Death Sweep (Meta)"         },
    { ShouldBladeDance,      DoBladeDance,      "Blade Dance (2+ AoE)"       },
    { ShouldAnnihilation,    DoAnnihilation,    "Annihilation (Meta)"        },
    { ShouldChaosStrike,     DoChaosStrike,     "Chaos Strike (Fury>=40)"    },
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
