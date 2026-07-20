// Guardian Druid — WoW 12.0 spec rotation (specId 104).
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
// Mitigation triad
// -----------------
// * Ironfur — physical DR active mitigation. Burn rage continuously; the
//   buff stacks so we re-cast every time the cooldown allows and rage
//   permits. The original predicate suppressed Ironfur once any Ironfur
//   aura was up — that's wrong for the stack model in WoW 12.0. Updated
//   to allow re-stacking up to 4s of remaining duration (pandemic).
// * Frenzied Regeneration — 2-charge self-heal HoT. Fires <=70%.
// * Survival Instincts — 50% DR panic; <=35%.
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
// Validated spell IDs (SpellName.csv, WoW 12.0)
// ---------------------------------------------
//     5487  Bear Form              (cast id — produces both 5487 + 270100
//                                   aura on Guardian via spec passive)
//    33917  Mangle
//    77758  Thrash (Bear)
//   213771  Swipe (Bear)
//     8921  Moonfire
//     6807  Maul
//   192081  Ironfur                (cast & aura same id)
//    22842  Frenzied Regeneration
//    80313  Pulverize              (talent — 3-stack DR consume)
//   102558  Incarnation: Guardian of Ursoc
//    50334  Berserk (Guardian)     (talent)
//   200851  Rage of the Sleeper    (talent — 25% DR + leech)
//    22812  Barkskin
//    61336  Survival Instincts
//   108238  Renewal
//     8936  Regrowth
//   106898  Stampeding Roar        (Bear-form cast; cat-form variant is
//                                   77761 but bear casts 106898)
//   106839  Skull Bash
//     5211  Mighty Bash
//     6795  Growl
//    20484  Rebirth
//     2908  Soothe
//     1126  Mark of the Wild
//   192090  Thrash (Bear) — bleed debuff for Pulverize stack check
//    29166  Innervate
//
// Skipped spells (and why)
// ---------------------------
//   * 270100  Bear Form variant   — passive aura, not a separate cast.
//     The 5487 cast covers shapeshift; the variant aura is layered on by
//     the spec passive automatically.
//   * 300346  Ursine Adept        — passive talent (form bonuses); no
//     predicate needed.
//   * 405834  Improved Prowl      — Feral-tree talent; Guardian doesn't
//     run cat-form openers, no value here.
//   * Cat Form cast               — would break the bear-form rotation.
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

// ---- Spell IDs (WoW 12.0, validated against SpellName.csv) ----
constexpr uint32 BEAR_FORM             = 5487;
constexpr uint32 CAT_FORM              = 768;        // for "drop cat into bear" guard
constexpr uint32 MANGLE                = 33917;
constexpr uint32 THRASH_BEAR           = 77758;
constexpr uint32 SWIPE_BEAR            = 213771;
constexpr uint32 MOONFIRE              = 8921;
constexpr uint32 MAUL                  = 6807;
constexpr uint32 IRONFUR               = 192081;
constexpr uint32 FRENZIED_REGEN        = 22842;
constexpr uint32 PULVERIZE             = 80313;       // talent — DR consumes Thrash stacks
constexpr uint32 INCARNATION_GUARDIAN  = 102558;
constexpr uint32 BERSERK_GUARDIAN      = 50334;       // historically Berserk; talent
constexpr uint32 RAGE_OF_THE_SLEEPER   = 200851;      // talent — 25% DR + leech
constexpr uint32 BARKSKIN              = 22812;
constexpr uint32 SURVIVAL_INSTINCTS    = 61336;
constexpr uint32 RENEWAL               = 108238;
constexpr uint32 REGROWTH              = 8936;
constexpr uint32 STAMPEDING_ROAR_BEAR  = 106898;
constexpr uint32 SKULL_BASH            = 106839;
constexpr uint32 MIGHTY_BASH           = 5211;
constexpr uint32 GROWL                 = 6795;
constexpr uint32 REBIRTH               = 20484;
constexpr uint32 SOOTHE                = 2908;
constexpr uint32 MARK_OF_THE_WILD      = 1126;
constexpr uint32 THRASH_DEBUFF         = 192090;     // matches the bleed for Pulverize check
constexpr uint32 INNERVATE             = 29166;

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

bool ShouldRageOfTheSleeper(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(RAGE_OF_THE_SLEEPER)) return false;
    if (!ctx.bot.is_ready(RAGE_OF_THE_SLEEPER)) return false;
    return ctx.bot.hp_pct() <= 70 || BossLikeTargetEngaged(ctx);
}
void DoRageOfTheSleeper(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(RAGE_OF_THE_SLEEPER); }

bool ShouldRenewal(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(RENEWAL)) return false;
    if (!ctx.bot.is_ready(RENEWAL)) return false;
    return ctx.bot.hp_pct() <= 40;
}
void DoRenewal(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(RENEWAL); }

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
    if (!ctx.bot.knows_spell(BERSERK_GUARDIAN)) return false;
    if (!ctx.bot.is_ready(BERSERK_GUARDIAN)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoBerserkGuardian(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BERSERK_GUARDIAN); }

// ---- Damage / threat ----
// Pulverize: consumes 3 Thrash bleed stacks for a +9% damage reduction
// buff. The DR uptime is a major Guardian mitigation pillar.
bool ShouldPulverize(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(PULVERIZE)) return false;
    if (!ctx.bot.is_ready(PULVERIZE)) return false;
    return ctx.bot.aura_stacks(THRASH_DEBUFF, ctx.bot.victim()) >= 3;
}
void DoPulverize(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(PULVERIZE, ctx.bot.victim());
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

bool ShouldMoonfire(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MOONFIRE)) return false;
    AuraEntry const* a = ctx.bot.find_aura(MOONFIRE, ctx.bot.victim());
    return !a || a->remaining.count() <= 4000;
}
void DoMoonfire(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MOONFIRE, ctx.bot.victim());
}

// Maul: rage spender. Only spend when Ironfur and Frenzied Regen are
// satisfied — never starve those of resources.
bool ShouldMaul(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MAUL)) return false;
    if (!ctx.bot.is_ready(MAUL)) return false;
    if (ctx.bot.power(POWER_RAGE_IDX) < 60) return false;
    if (ctx.bot.hp_pct() <= 60) return false;
    return true;
}
void DoMaul(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MAUL, ctx.bot.victim());
}

bool ShouldSwipe(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SWIPE_BEAR)) return false;
    return true;
}
void DoSwipe(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SWIPE_BEAR); }

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
//   1.  Rebirth                — battle-rez in group.
//   2.  Bear Form              — spec stance, MUST be active before
//                                anything else fires. Sits above survival
//                                CDs because most of them are bear-only.
//   3.  Mark of the Wild       — OOC group buff (rare maintenance).
//   4.  Growl                  — taunt off-target enemies.
//   5.  Skull Bash             — primary interrupt.
//   6.  Mighty Bash            — interrupt fallback.
//   7.  Soothe                 — enrage dispel.
//   8.  Stampeding Roar        — group sprint panic.
//   9.  Survival Instincts     — 50% DR panic.
//   10. Renewal                — instant 30% self-heal.
//   11. Barkskin               — 20% DR.
//   12. Rage of the Sleeper    — 25% DR + leech on boss.
//   13. Frenzied Regen         — self-heal HoT (<=70%).
//   14. Ironfur                — physical DR mit stack maintenance.
//   15. Innervate              — ally caster mana.
//   16. Incarnation: Guardian  — burst CD on boss.
//   17. Berserk (Guardian)     — burst CD on boss.
//   18. Moonfire               — DoT refresh.
//   19. Thrash                 — rage gen + AoE bleed stacks.
//   20. Pulverize              — consume 3 Thrash stacks for DR.
//   21. Mangle                 — rage generator + filler.
//   22. Maul                   — rage spender at >60%.
//   23. Swipe                  — AoE filler.
//   24. Auto attack            — engage fallthrough.
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
    { ShouldRenewal,           DoRenewal,           "Renewal (<=40%)"                },
    { ShouldBarkskin,          DoBarkskin,          "Barkskin (<=60%)"               },
    { ShouldRageOfTheSleeper,  DoRageOfTheSleeper,  "Rage of the Sleeper"            },
    { ShouldFrenziedRegen,     DoFrenziedRegen,     "Frenzied Regen (<=70%)"         },
    { ShouldIronfur,           DoIronfur,           "Ironfur (active mit)"           },
    { ShouldInnervate,         DoInnervate,         "Innervate (healer mana)"        },
    { ShouldIncarnation,       DoIncarnation,       "Incarnation: Guardian (boss)"   },
    { ShouldBerserkGuardian,   DoBerserkGuardian,   "Berserk (boss)"                 },
    { ShouldMoonfire,          DoMoonfire,          "Moonfire (refresh)"             },
    { ShouldThrash,            DoThrash,            "Thrash (rage gen + AoE bleed)"  },
    { ShouldPulverize,         DoPulverize,         "Pulverize (3-stack DR)"         },
    { ShouldMangle,            DoMangle,            "Mangle"                         },
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
