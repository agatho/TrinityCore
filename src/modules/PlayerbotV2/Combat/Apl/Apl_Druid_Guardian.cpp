// Guardian Druid - WoW 12.1.0.69587 (Midnight) spec rotation (specId 104).
//
// Stance / form
// -------------
// Guardian lives in BEAR FORM. The baseline druid file would otherwise
// park us in Cat Form (any-druid leveling default); this spec table runs
// first, so the Bear Form rule near the top of the cascade re-shifts us
// out of cat as soon as the spec is active.
//
// We do NOT cast Cat Form here. The bear-form rotation owns the bot:
// Mangle (rage + bleed-stack helper), Thrash (rage + AoE bleed), Moonfire
// (off-GCD-feeling refresh), Maul (rage spender), plus the mitigation
// triad Ironfur / Frenzied Regeneration / Survival Instincts.
//
// Tank duties
// -----------
// * Growl — single-target taunt on the closest enemy not already attacking
//   the tank. Critical for picking up adds when the tank already has aggro.
// * Skull Bash + Mighty Bash — interrupt ladder. Skull Bash leads; Mighty
//   Bash is a fallback that targets an interruptible caster.
// * Stampeding Roar — group sprint, fired as a panic reposition tool.
//
// Mitigation ladder (12.1)
// ------------------------
// * Ironfur - physical DR active mitigation. Burn rage continuously; the
//   buff stacks so we re-cast every time the cooldown allows and rage
//   permits, re-stacking up to 4s of remaining duration (pandemic).
// * Frenzied Regeneration - self-heal HoT. Fires <=70%.
// * Survival Instincts - 50% DR panic; <=35%.
// * Heart of the Wild [R] - Bear Form: +max health for a while; <=50%.
// * Lunar Beam [R] - ground beam that also heals us and raises mastery;
//   used on cooldown in real fights.
// Pulverize, Rage of the Sleeper and Renewal no longer exist in 12.1.
//
// Bear Form variant (270100)
// --------------------------
// Modern Guardian receives an extra Bear Form definition (270100) that
// piggy-backs additional passive auras (armor / Stamina / etc.). The
// LEARNED spell that actually triggers shapeshift on the player is still
// 5487 in this build — 270100 is an aura-only variant applied implicitly
// by the spec passive. We cast 5487 and let the spec passive layer on
// 270100. Don't add a separate cast rule for 270100.
//
// Validated spell IDs (SpellName.csv, WoW 12.1.0.69587)
// ----------------------------------------------------
//     5487  Bear Form              (cast id - produces both 5487 + 270100
//                                   aura on Guardian via spec passive)
//    33917  Mangle
//    77758  Thrash (Bear)          (cast id; bleed aura 192090)
//   213764  Swipe                  (class talent [R]; single id for cat + bear)
//     8921  Moonfire               (cast id; DoT aura 164812)
//  1252871  Red Moon               (spec talent [R]; replaces Moonfire; 8s
//                                   DoT on the target, Mangle extends it)
//   213708  Galactic Guardian      (proc buff: next Moonfire free + Rage)
//     6807  Maul                   (spec talent [R])
//   400254  Raze                   (spec talent, not in build; replaces Maul)
//   192081  Ironfur                (class talent [R]; cast & aura same id)
//    22842  Frenzied Regeneration  (class talent [R])
//   102558  Incarnation: Guardian of Ursoc (spec talent [R])
//    50334  Berserk (Guardian)     (spec talent [R]; replaced by Incarnation)
//   204066  Lunar Beam             (spec talent [R])
//  1261867  Heart of the Wild      (class talent [R]; Bear = +max health)
//    22812  Barkskin
//    61336  Survival Instincts     (spec talent [R])
//   106898  Stampeding Roar        (class talent [R]; Bear-form cast)
//   106839  Skull Bash             (class talent [R])
//     5211  Mighty Bash            (class talent, not in build)
//     6795  Growl
//    20484  Rebirth
//     2908  Soothe                 (class talent [R])
//     1126  Mark of the Wild
//    29166  Innervate              (class talent [R])
//
// Skipped spells (and why)
// ------------------------
//   * 80313 Pulverize, 200851 Rage of the Sleeper, 108238 Renewal - not
//     learnable in 12.1; rules and the 192090 Thrash-stack check deleted.
//   * 1253799 Sundering Roar, 155835 Bristling Fur, 391528 Convoke the
//     Spirits - spec talents the curated Guardian builds do not take.
//   * 2782 Remove Corruption - [R] class talent but not castable in Bear
//     Form; leaving the stance mid-pull is worse than the dispel.
//   * 99 Incapacitating Roar, 132469 Typhoon - [M]-only class talents.
//   * 102401 Wild Charge, 102793 Ursol's Vortex - [R] but need ally /
//     ground positioning the tank rotation does not do.
//   * 8936 Regrowth - hard cast that leaves Bear Form; the healer covers it.
//   * 270100  Bear Form variant   - passive aura, not a separate cast.
//   * 1229376 Single-Button Assistant - client convenience macro.
//   * Cat Form cast               - would break the bear-form rotation.
//     Spec rotation overrides the baseline cat-form re-entry by running
//     first and casting Bear Form ASAP.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated against SpellName.csv) ----
constexpr uint32 BEAR_FORM             = 5487;
constexpr uint32 CAT_FORM              = 768;        // for "drop cat into bear" guard
constexpr uint32 MANGLE                = 33917;
constexpr uint32 THRASH_BEAR           = 77758;
constexpr uint32 SWIPE                 = 213764;     // class talent [R] - single Swipe id in 12.1
constexpr uint32 MOONFIRE              = 8921;       // cast id
constexpr uint32 MOONFIRE_DOT          = 164812;     // Moonfire periodic aura
constexpr uint32 RED_MOON              = 1252871;    // spec talent [R] - replaces Moonfire; aura id == cast id
constexpr uint32 GALACTIC_GUARDIAN_BUFF = 213708;    // proc: next Moonfire free + extra Rage
constexpr uint32 MAUL                  = 6807;       // spec talent [R]
constexpr uint32 RAZE                  = 400254;     // spec talent (not in build) - replaces Maul, cleaves
constexpr uint32 IRONFUR               = 192081;     // class talent [R]
constexpr uint32 FRENZIED_REGEN        = 22842;      // class talent [R]
constexpr uint32 INCARNATION_GUARDIAN  = 102558;     // spec talent [R] - replaces Berserk
constexpr uint32 BERSERK_GUARDIAN      = 50334;      // spec talent [R]
constexpr uint32 LUNAR_BEAM            = 204066;     // spec talent [R] - AoE beam + self heal + mastery
constexpr uint32 HEART_OF_THE_WILD     = 1261867;    // class talent [R] - Bear: +max health
constexpr uint32 BARKSKIN              = 22812;
constexpr uint32 SURVIVAL_INSTINCTS    = 61336;      // spec talent [R]
constexpr uint32 STAMPEDING_ROAR_BEAR  = 106898;     // class talent [R]
constexpr uint32 SKULL_BASH            = 106839;     // class talent [R]
constexpr uint32 MIGHTY_BASH           = 5211;       // class talent (not in build)
constexpr uint32 GROWL                 = 6795;
constexpr uint32 REBIRTH               = 20484;
constexpr uint32 SOOTHE                = 2908;       // class talent [R]
constexpr uint32 MARK_OF_THE_WILD      = 1126;
constexpr uint32 INNERVATE             = 29166;      // class talent [R]

constexpr uint8 POWER_RAGE_IDX = 1;

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

bool CanShapeshiftNow(ApPredicateContext const& ctx)
{
    auto const& mv = ctx.bot.raw().movement;
    return !mv.is_mounted && !mv.is_flying;
}

// ---- Stance / buffs ----
// Bear Form: the spec stance. We cast it whenever the aura is missing AND
// we're not mounted/flying. The "drop cat into bear" case is handled by
// the same rule (Cat Form aura present => Bear Form aura missing => we
// re-shift to bear).
bool ShouldBearForm(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BEAR_FORM)) return false;
    if (ctx.bot.has_aura(BEAR_FORM)) return false;
    if (!CanShapeshiftNow(ctx)) return false;
    return true;
}
void DoBearForm(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BEAR_FORM); }

bool ShouldMarkOfTheWild(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(MARK_OF_THE_WILD)) return false;
    if (ctx.bot.in_combat()) return false;
    return !ctx.bot.has_aura(MARK_OF_THE_WILD);
}
void DoMarkOfTheWild(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MARK_OF_THE_WILD, ctx.bot.raw().guid);
}

// ---- Tank utility ----
bool ShouldGrowl(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(GROWL)) return false;
    if (!ctx.bot.is_ready(GROWL)) return false;
    return ctx.bot.untaunted_enemy() != nullptr;
}
void DoGrowl(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.untaunted_enemy())
        e.cast(GROWL, t->guid);
}

bool ShouldSkullBash(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SKULL_BASH)) return false;
    if (!ctx.bot.is_ready(SKULL_BASH)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    return ctx.bot.kick_target(pvp, 13.0f) != nullptr;
}
void DoSkullBash(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 13.0f))
        e.cast(SKULL_BASH, c->guid);
}

bool ShouldMightyBash(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MIGHTY_BASH)) return false;
    if (!ctx.bot.is_ready(MIGHTY_BASH)) return false;
    // Only fire when Skull Bash isn't a better option.
    if (ctx.bot.is_ready(SKULL_BASH) && ctx.bot.knows_spell(SKULL_BASH)) return false;
    return ctx.bot.interruptible_caster() != nullptr;
}
void DoMightyBash(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* c = ctx.bot.interruptible_caster())
        e.cast(MIGHTY_BASH, c->guid);
}

bool ShouldStampedingRoar(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(STAMPEDING_ROAR_BEAR)) return false;
    if (!ctx.bot.is_ready(STAMPEDING_ROAR_BEAR)) return false;
    // Group sprint — panic reposition for the bot/party.
    return ctx.bot.hp_pct() <= 35;
}
void DoStampedingRoar(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(STAMPEDING_ROAR_BEAR); }

// ---- Survival ladder ----
bool ShouldSurvivalInstincts(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SURVIVAL_INSTINCTS)) return false;
    if (!ctx.bot.is_ready(SURVIVAL_INSTINCTS)) return false;
    return ctx.bot.hp_pct() <= 35;
}
void DoSurvivalInstincts(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SURVIVAL_INSTINCTS); }

bool ShouldBarkskin(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(BARKSKIN)) return false;
    if (!ctx.bot.is_ready(BARKSKIN)) return false;
    return ctx.bot.hp_pct() <= 60;
}
void DoBarkskin(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BARKSKIN); }

// Heart of the Wild in Bear Form raises maximum health for its duration
// (2min CD) - a defensive in the Guardian kit, so it lives on the HP ladder.
bool ShouldHeartOfTheWild(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(HEART_OF_THE_WILD)) return false;
    if (!ctx.bot.is_ready(HEART_OF_THE_WILD)) return false;
    if (!ctx.bot.has_aura(BEAR_FORM)) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoHeartOfTheWild(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HEART_OF_THE_WILD, ctx.bot.raw().guid);
}

// Lunar Beam: ground beam at the target's feet that damages nearby
// enemies, heals us and raises mastery for its duration (1min CD). Used on
// cooldown whenever the fight is real (pressure, boss or a pack).
bool ShouldLunarBeam(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(LUNAR_BEAM)) return false;
    if (!ctx.bot.is_ready(LUNAR_BEAM)) return false;
    return ctx.bot.hp_pct() <= 80 || BossLikeTargetEngaged(ctx)
        || ctx.bot.enemies_within(8.0f) >= 2;
}
void DoLunarBeam(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(LUNAR_BEAM, v->x, v->y, v->z);
    else
        e.cast(LUNAR_BEAM, ctx.bot.victim());
}

bool ShouldFrenziedRegen(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(FRENZIED_REGEN)) return false;
    if (!ctx.bot.is_ready(FRENZIED_REGEN)) return false;
    if (ctx.bot.power(POWER_RAGE_IDX) < 10) return false;
    return ctx.bot.hp_pct() <= 70;
}
void DoFrenziedRegen(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FRENZIED_REGEN); }

// Ironfur: physical DR active mitigation. The buff STACKS in WoW 12.0;
// previous predicate suppressed re-casts whenever any Ironfur aura was
// up, which is wrong. We allow re-stack up to ~4s remaining (pandemic
// window) so the stacks build during heavy melee pressure.
bool ShouldIronfur(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(IRONFUR)) return false;
    if (!ctx.bot.is_ready(IRONFUR)) return false;
    if (ctx.bot.power(POWER_RAGE_IDX) < 40) return false;
    if (ctx.bot.attackers_count() < 1) return false;
    AuraEntry const* a = ctx.bot.find_aura(IRONFUR);
    return !a || a->remaining.count() <= 4000;
}
void DoIronfur(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(IRONFUR); }

// ---- Group utility ----
bool ShouldRebirth(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(REBIRTH)) return false;
    if (!ctx.bot.is_ready(REBIRTH)) return false;
    return ctx.group.dead_member_priority(ctx.bot.map_id()) != nullptr;
}
void DoRebirth(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* m = ctx.group.dead_member_priority(ctx.bot.map_id()))
        e.cast(REBIRTH, m->guid);
}

bool ShouldInnervate(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(INNERVATE)) return false;
    if (!ctx.bot.is_ready(INNERVATE)) return false;
    auto const* m = ctx.group.lowest_mana_caster();
    if (!m || !m->online || m->hp <= 0) return false;
    if (m->guid == ctx.bot.raw().guid) return false;
    return m->max_mana > 0 && (m->mana * 100) / m->max_mana <= 30;
}
void DoInnervate(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* m = ctx.group.lowest_mana_caster())
        e.cast(INNERVATE, m->guid);
}

bool ShouldSoothe(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SOOTHE)) return false;
    if (!ctx.bot.is_ready(SOOTHE)) return false;
    return ctx.bot.target_dispellable(Playerbot::DispelType::Enrage);
}
void DoSoothe(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SOOTHE, ctx.bot.victim());
}

// ---- Major offensive cooldowns ----
bool ShouldIncarnation(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(INCARNATION_GUARDIAN)) return false;
    if (!ctx.bot.is_ready(INCARNATION_GUARDIAN)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoIncarnation(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(INCARNATION_GUARDIAN); }

bool ShouldBerserkGuardian(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    // Incarnation replaces Berserk when talented (choice node).
    if (ctx.bot.knows_spell(INCARNATION_GUARDIAN)) return false;
    if (!ctx.bot.knows_spell(BERSERK_GUARDIAN)) return false;
    if (!ctx.bot.is_ready(BERSERK_GUARDIAN)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoBerserkGuardian(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BERSERK_GUARDIAN); }

// ---- Damage / threat ----
// Red Moon replaces Moonfire when talented [R]: an 8s DoT on the target
// that Mangle extends and that generates Rage. Keep it on the victim.
bool ShouldRedMoon(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RED_MOON)) return false;
    if (!ctx.bot.is_ready(RED_MOON)) return false;
    AuraEntry const* a = ctx.bot.find_aura(RED_MOON, ctx.bot.victim());
    return !a || a->remaining.count() <= 1500;
}
void DoRedMoon(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RED_MOON, ctx.bot.victim());
}

// Moonfire: refresh the 164812 DoT on the victim, or spend a Galactic
// Guardian proc (free instant + Rage) immediately. Skipped when Red Moon
// has replaced Moonfire.
bool ShouldMoonfire(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MOONFIRE)) return false;
    if (ctx.bot.knows_spell(RED_MOON)) return false;
    if (ctx.bot.has_aura(GALACTIC_GUARDIAN_BUFF)) return true;
    AuraEntry const* a = ctx.bot.find_aura(MOONFIRE_DOT, ctx.bot.victim());
    return !a || a->remaining.count() <= 4000;
}
void DoMoonfire(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MOONFIRE, ctx.bot.victim());
}

bool ShouldThrash(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(THRASH_BEAR)) return false;
    return ctx.bot.is_ready(THRASH_BEAR);
}
void DoThrash(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(THRASH_BEAR); }

bool ShouldMangle(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MANGLE)) return false;
    return ctx.bot.is_ready(MANGLE);
}
void DoMangle(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MANGLE, ctx.bot.victim());
}

// Maul / Raze: rage spenders (Raze is the cleaving talent override of
// Maul). Only spend when Ironfur and Frenzied Regen are satisfied - never
// starve those of resources.
bool RageSpendAllowed(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.power(POWER_RAGE_IDX) < 60) return false;
    if (ctx.bot.hp_pct() <= 60) return false;
    return true;
}

bool ShouldRaze(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(RAZE)) return false;
    if (!ctx.bot.is_ready(RAZE)) return false;
    return RageSpendAllowed(ctx);
}
void DoRaze(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RAZE, ctx.bot.victim());
}

bool ShouldMaul(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(RAZE)) return false;          // Raze replaces Maul
    if (!ctx.bot.knows_spell(MAUL)) return false;
    if (!ctx.bot.is_ready(MAUL)) return false;
    return RageSpendAllowed(ctx);
}
void DoMaul(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MAUL, ctx.bot.victim());
}

// Swipe is a single spell id in 12.1 (213764, damage varies by form).
bool ShouldSwipe(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SWIPE)) return false;
    return true;
}
void DoSwipe(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SWIPE); }

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

// ---- Rule table (priority order top-down) ----
// Order rationale (tank lens):
//   1.  Rebirth                - battle-rez in group.
//   2.  Bear Form              - spec stance, MUST be active before
//                                anything else fires. Sits above survival
//                                CDs because most of them are bear-only.
//   3.  Mark of the Wild       - OOC group buff (rare maintenance).
//   4.  Growl                  - taunt off-target enemies.
//   5.  Skull Bash             - primary interrupt.
//   6.  Mighty Bash            - interrupt fallback.
//   7.  Soothe                 - enrage dispel.
//   8.  Stampeding Roar        - group sprint panic.
//   9.  Survival Instincts     - 50% DR panic.
//   10. Heart of the Wild      - +max health (<=50%).
//   11. Barkskin               - 20% DR.
//   12. Frenzied Regen         - self-heal HoT (<=70%).
//   13. Ironfur                - physical DR mit stack maintenance.
//   14. Innervate              - ally caster mana.
//   15. Incarnation: Guardian  - burst CD on boss.
//   16. Berserk (Guardian)     - burst CD on boss (when not talented away).
//   17. Lunar Beam             - AoE beam + self heal + mastery.
//   18. Red Moon               - Moonfire replacement DoT (talent).
//   19. Moonfire               - DoT refresh / Galactic Guardian proc.
//   20. Thrash                 - rage gen + AoE bleed stacks.
//   21. Mangle                 - rage generator + filler.
//   22. Raze / Maul            - rage spender at >60% (Raze overrides Maul).
//   23. Swipe                  - AoE filler.
//   24. Auto attack            - engage fallthrough.
ApRule const kRules[] = {
    { ShouldRebirth,           DoRebirth,           "Rebirth (battle rez)"           },
    { ShouldBearForm,          DoBearForm,          "Bear Form"                      },
    { ShouldMarkOfTheWild,     DoMarkOfTheWild,     "Mark of the Wild"               },
    { ShouldGrowl,             DoGrowl,             "Growl (taunt)"                  },
    { ShouldSkullBash,         DoSkullBash,         "Skull Bash (interrupt)"         },
    { ShouldMightyBash,        DoMightyBash,        "Mighty Bash (interrupt fb)"     },
    { ShouldSoothe,            DoSoothe,            "Soothe (enrage)"                },
    { ShouldStampedingRoar,    DoStampedingRoar,    "Stampeding Roar (panic)"        },
    { ShouldSurvivalInstincts, DoSurvivalInstincts, "Survival Instincts (<=35%)"     },
    { ShouldHeartOfTheWild,    DoHeartOfTheWild,    "Heart of the Wild (<=50%)"      },
    { ShouldBarkskin,          DoBarkskin,          "Barkskin (<=60%)"               },
    { ShouldFrenziedRegen,     DoFrenziedRegen,     "Frenzied Regen (<=70%)"         },
    { ShouldIronfur,           DoIronfur,           "Ironfur (active mit)"           },
    { ShouldInnervate,         DoInnervate,         "Innervate (healer mana)"        },
    { ShouldIncarnation,       DoIncarnation,       "Incarnation: Guardian (boss)"   },
    { ShouldBerserkGuardian,   DoBerserkGuardian,   "Berserk (boss)"                 },
    { ShouldLunarBeam,         DoLunarBeam,         "Lunar Beam"                     },
    { ShouldRedMoon,           DoRedMoon,           "Red Moon (refresh)"             },
    { ShouldMoonfire,          DoMoonfire,          "Moonfire (refresh)"             },
    { ShouldThrash,            DoThrash,            "Thrash (rage gen + AoE bleed)"  },
    { ShouldMangle,            DoMangle,            "Mangle"                         },
    { ShouldRaze,              DoRaze,              "Raze (rage spend, cleave)"      },
    { ShouldMaul,              DoMaul,              "Maul (rage spend)"              },
    { ShouldSwipe,             DoSwipe,             "Swipe (filler)"                 },
    { AlwaysInCombat,          DoAutoAttack,        "Engage auto attack"             },
};

} // anonymous

void RegisterApl_Druid_Guardian()
{
    constexpr uint32 SPEC_DRUID_GUARDIAN = 104;
    RegisterRotation(CLASS_DRUID, SPEC_DRUID_GUARDIAN, ApRotation{kRules});
}

} // namespace Playerbot::Combat
