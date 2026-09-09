// Devourer Demon Hunter - WoW 12.1.0.69587 (Midnight) rotation. The third
// DH spec (ChrSpecialization 1480, DPS): a Void-themed soul harvester.
// Consume (2s cast, usable while moving) builds Fury and Soul Fragments,
// Reap gathers nearby souls into a heavy 25y bolt, Void Ray dumps 100 Fury
// into a channel, Voidblade / Hungering Slash are the melee builders, and
// once enough souls are banked (stacks of aura 1225789) Void Metamorphosis
// opens a burst window in which Consume becomes Devour, Void Ray drains
// Fury instead of costing it, and Collapsing Star becomes available.
//
// Survival: Blur (DR), Soul Immolation (self-heal + Fury + fragments),
// Darkness (group dodge). CC: Disrupt interrupt, Void Nova (AoE stun),
// Sigil of Misery (fear, M+ build).
//
// ---- Validated IDs (WoW 12.1.0.69587 kit: SpecializationSpells 1480 +
//      simc trait data, Void-Scarred raid build
//      MID2_Demon_Hunter_Devourer; M+ = method.gg Void-Scarred melee) ----
//
//   Spec spells:
//     473662  Consume                 - builder: 2s cast, 25y, moving OK,
//                                       generates Fury (+ fragments with
//                                       Predator's Thirst [R])
//     1217610 Devour                  - Void Meta override of Consume
//                                       (not in the spellbook: gate on
//                                       aura 1217607 + knows Consume)
//     1226019 Reap                    - 25y bolt that gathers souls; extra
//                                       charge from Second Helping [R]
//     1225826 Eradicate               - empowered Reap after a full Void
//                                       Ray channel (aura 1239524); an
//                                       override, gated like Devour
//     1217605 Void Metamorphosis      - burst form cast; requires the
//                                       1225789 soul stacks (max 50, Soul
//                                       Glutton [R] lowers it)
//     1217607 Void Metamorphosis      - in-form aura we test for
//     1225789 Void Metamorphosis      - soul-stack aura (read-only)
//     198589  Blur                    - damage reduction
//
//   Talents (raid build [R] unless noted):
//     473728  Void Ray                - 100 Fury channel (drain-only while
//                                       in Void Meta)
//     1241937 Soul Immolation         - heal + Fury + fragments over 5s;
//                                       the self-aura marks it active
//     1246167 The Hunt (Devourer)     - 50y charge + DoT
//     1245412 Voidblade               - 15y charge builder (Duty Eternal
//                                       [R] makes it generate Fury)
//     1239123 Hungering Slash         - replaces Voidblade after The Hunt
//                                       / Voidblade damage (aura 1239525)
//     1221150 Collapsing Star         - granted inside Void Meta every N
//                                       souls (access aura 1221171)
//     1234195 Void Nova               - target + nearby AoE stun, 45s
//     198793  Vengeful Retreat        - cast for the Voidstep (1223157)
//                                       explosion Hungering Slash grants
//     196718  Darkness                - group dodge
//     207684  Sigil of Misery (M+)    - AoE fear
//     183752  Disrupt                 - interrupt (class baseline)
//     217832  Imprison                - single-target incap (no rule yet)
//
// ---- Skipped spells (and why) ----
//
//   1245453 Cull / 1245470 Reaper's Toll / 1245483 Pierce the Veil /
//   1259431 Predator's Wake         - Void-Scarred "Voidsurge" empowered
//                                     variants of Reap / Hungering Slash /
//                                     Voidblade / The Hunt. Their trigger
//                                     auras are not resolvable from the
//                                     snapshot; the base casts above cover
//                                     the buttons.
//   1234796 Shift                   - Blink-style teleport to a location;
//                                     positioning the bot cannot reason
//                                     about.
//   278326 Consume Magic            - purge; no purgeable-buff predicate.
//   185123 Throw Glaive / 258920 Immolation Aura / 204596 Sigil of Flame
//                                   - class baseline, absent from the
//                                     Devourer simc priority (Consume is
//                                     the ranged filler already).
//   1251417 Spectral Sight          - utility, no combat use.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated) ----
constexpr uint32 CONSUME              = 473662;       // spec builder (2s cast)
constexpr uint32 DEVOUR               = 1217610;      // Void Meta override of Consume
constexpr uint32 REAP                 = 1226019;      // spec spender / soul gather
constexpr uint32 ERADICATE            = 1225826;      // empowered Reap (override)
constexpr uint32 ERADICATE_READY      = 1239524;      // "next Reap is empowered"
constexpr uint32 VOID_RAY             = 473728;       // talent [R] - 100 Fury channel
constexpr uint32 VOID_METAMORPHOSIS   = 1217605;      // spec - burst form cast
constexpr uint32 VOID_META_BUFF       = 1217607;      // in-form aura
constexpr uint32 VOID_META_STACKS     = 1225789;      // banked souls (max 50)
constexpr uint32 SOUL_IMMOLATION      = 1241937;      // talent [R] - heal + Fury
constexpr uint32 THE_HUNT             = 1246167;      // talent [R] - Devourer id
constexpr uint32 VOIDBLADE            = 1245412;      // talent [R] - charge builder
constexpr uint32 HUNGERING_SLASH      = 1239123;      // Voidblade replacement
constexpr uint32 HUNGERING_SLASH_READY= 1239525;      // "Voidblade is replaced"
constexpr uint32 COLLAPSING_STAR      = 1221150;      // Meta burst
constexpr uint32 COLLAPSING_STAR_READY= 1221171;      // "access to Collapsing Star"
constexpr uint32 VOID_NOVA            = 1234195;      // talent [R] - AoE stun
constexpr uint32 VENGEFUL_RETREAT     = 198793;       // talent [R]
constexpr uint32 VOIDSTEP             = 1223157;      // "next Vengeful Retreat explodes"
constexpr uint32 DISRUPT              = 183752;
constexpr uint32 IMPRISON             = 217832;       // talent [R] (no rule yet)
constexpr uint32 SIGIL_OF_MISERY      = 207684;       // talent [M]
constexpr uint32 BLUR                 = 198589;
constexpr uint32 DARKNESS             = 196718;       // talent [R]

constexpr uint8 POWER_FURY_IDX = 17;       // POWER_FURY in WoW 12.x enum

// Soul stacks needed before we try to enter Void Metamorphosis. The aura
// caps at 50; Soul Glutton [R] lowers the requirement, so 40 is a safe
// "almost full" threshold - the server refuses the cast if it is early.
constexpr int32 kVoidMetaSoulStacks = 40;

bool HasLiveTarget(ApPredicateContext const& ctx)
{
    return !ctx.bot.victim().IsEmpty();
}

int32 Fury(ApPredicateContext const& ctx) { return ctx.bot.power(POWER_FURY_IDX); }
bool InMeta(ApPredicateContext const& ctx) { return ctx.bot.has_aura(VOID_META_BUFF); }

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

// Soul Immolation - 5s self-buff: heals, generates Fury and shatters
// fragments. simc keeps it rolling whenever it is not active and Fury is
// low (or, in Meta, below the drain rate); we also use it as the spec's
// self-heal when HP dips.
bool ShouldSoulImmolation(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SOUL_IMMOLATION)) return false;
    if (!ctx.bot.is_ready(SOUL_IMMOLATION)) return false;
    if (ctx.bot.has_aura(SOUL_IMMOLATION)) return false;
    return ctx.bot.hp_pct() <= 70 || Fury(ctx) < 40;
}
void DoSoulImmolation(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SOUL_IMMOLATION); }

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

// Void Nova - stuns the target and everything around it (30y). Packs only.
bool ShouldVoidNova(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(VOID_NOVA)) return false;
    if (!ctx.bot.is_ready(VOID_NOVA)) return false;
    return ctx.bot.enemies_within(8.0f) >= 3;
}
void DoVoidNova(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(VOID_NOVA, ctx.bot.victim());
}

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

// ---- Major cooldowns ----
// Void Metamorphosis - no cooldown of its own; the gate is the banked soul
// stacks. simc enters it as soon as the stack aura is at max.
bool ShouldVoidMetamorphosis(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (InMeta(ctx)) return false;
    if (!ctx.bot.knows_spell(VOID_METAMORPHOSIS)) return false;
    if (!ctx.bot.is_ready(VOID_METAMORPHOSIS)) return false;
    return ctx.bot.aura_stacks(VOID_META_STACKS) >= kVoidMetaSoulStacks;
}
void DoVoidMetamorphosis(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(VOID_METAMORPHOSIS); }

// The Hunt - 50y charge + DoT, 90s. Devourer's Bite [R] amplifies damage
// on the target afterwards, so it is simply used on cooldown.
bool ShouldTheHunt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(THE_HUNT)) return false;
    return ctx.bot.is_ready(THE_HUNT);
}
void DoTheHunt(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(THE_HUNT, ctx.bot.victim());
}

// Collapsing Star - granted inside Void Meta once enough souls were
// harvested (access aura 1221171). Override-style: never in the
// spellbook, so gate on the aura and cast the id directly.
bool ShouldCollapsingStar(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!InMeta(ctx)) return false;
    if (!ctx.bot.knows_spell(VOID_METAMORPHOSIS)) return false;
    return ctx.bot.has_aura(COLLAPSING_STAR_READY);
}
void DoCollapsingStar(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(COLLAPSING_STAR, ctx.bot.victim());
}

// ---- Spenders ----
// Eradicate - the empowered Reap a full Void Ray channel unlocks (aura
// 1239524 "Your next Reap is substantially empowered"). Override of Reap:
// gate on the aura + Reap readiness, cast the override id.
bool ShouldEradicate(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(REAP)) return false;
    if (!ctx.bot.is_ready(REAP)) return false;
    return ctx.bot.has_aura(ERADICATE_READY);
}
void DoEradicate(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ERADICATE, ctx.bot.victim());
}

// Hungering Slash - after The Hunt / Voidblade land, Voidblade is replaced
// by this self-centred scythe sweep for a few seconds (aura 1239525). It
// generates Fury and shatters fragments; needs something in melee range.
bool ShouldHungeringSlash(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(VOIDBLADE)) return false;
    if (!ctx.bot.has_aura(HUNGERING_SLASH_READY)) return false;
    return ctx.bot.enemies_within(8.0f) >= 1;
}
void DoHungeringSlash(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HUNGERING_SLASH); }

// Vengeful Retreat - only for the Voidstep explosion Hungering Slash
// grants (simc: vengeful_retreat,if=buff.voidstep.up). The vault itself
// is harmless for a 25y spec; the Consume filler keeps casting.
bool ShouldVengefulRetreat(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(VENGEFUL_RETREAT)) return false;
    if (!ctx.bot.is_ready(VENGEFUL_RETREAT)) return false;
    if (!ctx.bot.has_aura(VOIDSTEP)) return false;
    return ctx.bot.enemies_within(8.0f) >= 1;
}
void DoVengefulRetreat(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(VENGEFUL_RETREAT); }

// Void Ray - 3s channel. Costs 100 Fury outside Void Meta; inside Meta it
// only slows the Fury drain, so channel it whenever ready.
bool ShouldVoidRay(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(VOID_RAY)) return false;
    if (!ctx.bot.is_ready(VOID_RAY)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(VOID_RAY)) return false;
    return InMeta(ctx) || Fury(ctx) >= 100;
}
void DoVoidRay(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(VOID_RAY, ctx.bot.victim());
}

// Voidblade - 15y charge builder (Fury with Duty Eternal [R]); also the
// gap close. Yields to Hungering Slash while the replacement aura is up.
bool ShouldVoidblade(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(VOIDBLADE)) return false;
    if (!ctx.bot.is_ready(VOIDBLADE)) return false;
    return !ctx.bot.has_aura(HUNGERING_SLASH_READY);
}
void DoVoidblade(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(VOIDBLADE, ctx.bot.victim());
}

// Reap - 25y bolt that gathers the souls around the target on the way.
// simc waits for 4+ gatherable souls; the snapshot has no soul-object
// count, so it fires whenever a charge is up (Reap generates Fury with
// Scythe's Embrace and never costs any).
bool ShouldReap(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(REAP)) return false;
    return ctx.bot.is_ready(REAP);
}
void DoReap(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(REAP, ctx.bot.victim());
}

// ---- Builders / filler ----
// Devour - Void Meta override of Consume (aura 1217607). Gate on the aura
// + knows_spell(Consume) and cast the override id.
bool ShouldDevour(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!InMeta(ctx)) return false;
    if (!ctx.bot.knows_spell(CONSUME)) return false;
    return ctx.bot.is_ready(CONSUME);
}
void DoDevour(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DEVOUR, ctx.bot.victim());
}

// Consume - the 2s builder, castable while moving. Unconditional filler
// so the ladder always ends in a cast (also the fallback if the Devour
// override emit is dropped).
bool ShouldConsume(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CONSUME)) return false;
    return ctx.bot.is_ready(CONSUME);
}
void DoConsume(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CONSUME, ctx.bot.victim());
}

bool AlwaysInCombat(ApPredicateContext const& ctx) { return ctx.bot.in_combat(); }
void DoAutoAttack(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    ObjectGuid t = ctx.bot.victim();
    if (t.IsEmpty()) t = ctx.bot.current_target();
    // Retaliate fallback: in combat with no victim/target, engage whatever
    // is meleeing us (attackers are adjacent and reachable), else the
    // nearest visible enemy, so combat actually resolves. Strictly
    // additive: only runs when no target was already selected.
    if (t.IsEmpty())
        for (auto const& u : ctx.bot.attackers())
            if (u.hp > 0) { t = u.guid; break; }
    if (t.IsEmpty())
        for (auto const& u : ctx.bot.nearby_enemies())
            if (u.hp > 0 && u.in_los) { t = u.guid; break; }
    if (!t.IsEmpty()) e.start_attack(t);
}

// Rule order (12.1 Void-Scarred simc priority, 2026-09-09):
//   1. Panic survival   - Blur (DR)
//   2. Group defensive  - Darkness on multi-wounded
//   3. Interrupts / CC  - Disrupt > Void Nova (3+ stun) > Sigil of
//                         Misery (panic fear)
//   4. Sustain          - Soul Immolation (HP <= 70 or Fury < 40)
//   5. Major CDs        - Void Metamorphosis (souls banked), The Hunt,
//                         Collapsing Star (Meta)
//   6. Empowered casts  - Eradicate (post-Void Ray Reap), Hungering
//                         Slash (post-Voidblade), Vengeful Retreat
//                         (Voidstep)
//   7. Spenders         - Void Ray (100 Fury / Meta) > Voidblade > Reap
//   8. Builders         - Devour (Meta) > Consume
//   9. Engage           - start_attack to keep swings going
ApRule const kRules[] = {
    { ShouldBlur,              DoBlur,              "Blur (<=50%)"                },
    { ShouldDarkness,          DoDarkness,          "Darkness (3+ wounded)"       },
    { ShouldDisrupt,           DoDisrupt,           "Disrupt (interrupt)"         },
    { ShouldVoidNova,          DoVoidNova,          "Void Nova (3+ AoE stun)"     },
    { ShouldSigilOfMisery,     DoSigilOfMisery,     "Sigil of Misery (panic)"     },
    { ShouldSoulImmolation,    DoSoulImmolation,    "Soul Immolation (heal/Fury)" },
    { ShouldVoidMetamorphosis, DoVoidMetamorphosis, "Void Metamorphosis"          },
    { ShouldTheHunt,           DoTheHunt,           "The Hunt"                    },
    { ShouldCollapsingStar,    DoCollapsingStar,    "Collapsing Star (Meta)"      },
    { ShouldEradicate,         DoEradicate,         "Eradicate (empowered Reap)"  },
    { ShouldHungeringSlash,    DoHungeringSlash,    "Hungering Slash"             },
    { ShouldVengefulRetreat,   DoVengefulRetreat,   "Vengeful Retreat (Voidstep)" },
    { ShouldVoidRay,           DoVoidRay,           "Void Ray (Fury>=100/Meta)"   },
    { ShouldVoidblade,         DoVoidblade,         "Voidblade"                   },
    { ShouldReap,              DoReap,              "Reap"                        },
    { ShouldDevour,            DoDevour,            "Devour (Meta)"               },
    { ShouldConsume,           DoConsume,           "Consume (builder)"           },
    { AlwaysInCombat,          DoAutoAttack,        "Engage auto attack"          },
};

} // anonymous

void RegisterApl_DemonHunter_Devourer()
{
    constexpr uint32 SPEC_DEMONHUNTER_DEVOURER = 1480;
    RegisterRotation(CLASS_DEMON_HUNTER, SPEC_DEMONHUNTER_DEVOURER, ApRotation{kRules});
}

} // namespace Playerbot::Combat
