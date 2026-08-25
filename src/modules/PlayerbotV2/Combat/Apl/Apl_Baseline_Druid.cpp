// Apl_Baseline_Druid.cpp — baseline rotation for class CLASS_DRUID (spec=0).
// Extracted from the monolithic Apl_Baseline.cpp on the split refactor;
// future edits go here exclusively. See Apl_Baseline_Common.h for the
// shared helpers + rule macros.
//
// Form-management strategy (THE point of this file)
// ------------------------------------------------
// Unlike the other 12 baseline rotations, the druid baseline is a
// FORM-AWARE state machine. Cat Form is the optimal leveling form: it
// stacks armor, gives bleed CP builders, has a stealth opener, and lets
// every druid (Balance / Feral / Guardian / Resto, any level) contribute
// meaningful melee damage even before they pick a spec. Sitting in
// caster form auto-attacking with a staff is what the previous baseline
// did and it was wrong. The user explicitly called that out.
//
// Rules that work IN CAT FORM (form-independent or cat-only):
//   * Barkskin              — INSTANT, off-GCD, no shapeshift gate.
//   * Cenarion Ward         — INSTANT, off-GCD.
//   * Cat Form entry        — the shapeshift cast itself; only fires
//                             when not yet in cat, not mounted, not
//                             flying, and we have a live target (or
//                             we're OOC and want to Prowl).
//   * Prowl                 — cat-form stealth opener, OOC only.
//   * Tiger's Fury          — cat-form energy CD.
//   * Rake / Rip            — cat-form bleeds (CP builder / 5-CP finisher).
//   * Shred                 — cat-form CP filler.
//   * Ferocious Bite        — cat-form 4-5 CP finisher.
//   * Thrash (cat)          — cat-form AoE bleed (3+ enemies).
//   * Swipe (cat)           — cat-form AoE damage (3+ enemies).
//   * Auto attack           — works in any form.
//
// Rules that DROP cat form when they fire (caster-form spells):
//   * Regrowth (panic)      — emergency hard-cast at <=50%; bot will be
//                             back in caster form afterwards, and the
//                             next tick's Cat Form rule will re-shift.
//   * Rejuvenation          — only fires OOC, so the form drop is cheap
//                             (we'll re-enter cat on pull).
//   * Mark of the Wild      — only fires OOC, same rationale.
//   * Wrath / Moonfire      — caster-form fillers ONLY used if the bot
//                             somehow has no Cat Form known (very low
//                             level pre-L1 racial?, polymorphed,
//                             aura-stripped). Gated on "not in cat" so
//                             they don't pop us out of cat mid-pull.
//   * Entangling Roots      — caster-form CC, same "not in cat" gate.
//
// Avoidance design choices
// ------------------------
// - **Do not shapeshift while mounted or flying.** Casting Cat Form on a
//   mount dismounts and then shifts — fine in PvP burst but disastrous
//   when crossing zones. We gate on `!is_mounted && !is_flying`.
// - **Do not refresh Mark of the Wild mid-combat.** It requires caster
//   form, which would drop the cat-form bleeds + reset Prowl. Gated on
//   `!in_combat`.
// - **Rejuvenation is OOC-only** for the same reason: a 1-rule HoT
//   refresh would otherwise blast us out of cat every 12s on the dot.
//   Emergency healing is Regrowth's job at <=50% HP.
// - **Regrowth panic IS allowed to drop cat.** Dying in cat with full CP
//   is worse than spending a GCD healing and re-shifting on the next
//   tick. The Cat Form re-entry rule sits high in priority so the
//   transition costs at most ~1.5s of caster-form exposure.
// - **Caster-form damage rules (Wrath / Moonfire / Roots) are gated on
//   `!has_aura(CAT_FORM)`.** They only run when we genuinely can't get
//   into cat (no spell known, sub-L1 race-gate, debuff stripping form).
//
// What we DON'T cover (intentionally):
//   * Bear Form / Mangle / Growl / Maul — tank specialty. Guardian spec
//     owns the bear stance + tank kit; the baseline is for any druid.
//   * Predatory Swiftness instant Regrowth proc — requires aura tracking
//     past baseline scope; Feral spec handles it.
//   * Combo-point-aware finishers below 4 CP — the baseline treats Rip /
//     Ferocious Bite as "4-5 CP go", not "5 CP only". Feral is stricter.
//   * Tank-form swap on heavy hits — Feral does that; baseline can't
//     assume the bot has bear-form rotations to run after the swap.
//
// To audit coverage:
//   python src/modules/PlayerbotV2/tools/baseline_coverage_audit.py

#include "Apl_Baseline_Common.h"

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::HasLiveTarget;
using ::Playerbot::Combat::baseline_common::AlwaysInCombat;
using ::Playerbot::Combat::baseline_common::DoAutoAttack;

// ---- Spell IDs (WoW 12.0, mined from Apl_Druid_Feral.cpp) ----
constexpr uint32 CAT_FORM          = 768;        // shapeshift; aura id == spell id
constexpr uint32 PROWL             = 5215;       // cat-form OOC stealth
constexpr uint32 DASH              = 1850;       // cat-form sprint
constexpr uint32 SHRED             = 5221;       // cat-form CP builder
constexpr uint32 RAKE              = 1822;       // cat-form bleed CP builder
constexpr uint32 RAKE_DEBUFF       = 155722;     // Rake bleed aura id
constexpr uint32 RIP               = 1079;       // cat-form 5-CP bleed finisher
constexpr uint32 FEROCIOUS_BITE    = 22568;      // cat-form 4-5 CP finisher
constexpr uint32 TIGERS_FURY       = 5217;       // cat-form energy CD
constexpr uint32 THRASH_CAT        = 106830;     // cat-form AoE bleed (cast = debuff)
constexpr uint32 SWIPE_CAT         = 106785;     // cat-form AoE damage

constexpr uint32 WRATH             = 5176;       // caster-form filler nuke
constexpr uint32 MOONFIRE          = 8921;       // caster-form DoT
constexpr uint32 REJUVENATION      = 774;        // caster-form HoT
constexpr uint32 ENTANGLING_ROOTS  = 339;        // caster-form root CC
constexpr uint32 REGROWTH          = 8936;       // caster-form panic heal
constexpr uint32 MARK_OF_THE_WILD  = 1126;       // caster-form buff
constexpr uint32 BARKSKIN          = 22812;      // form-independent, off-GCD, 20% DR
constexpr uint32 CENARION_WARD     = 102351;     // form-independent talent shield

// Combo points / energy live in the snapshot power array. Indices match
// the WoW 12.0 power enum used by Apl_Druid_Feral.cpp.
constexpr uint8 POWER_COMBO_POINTS_IDX = 4;
constexpr uint8 POWER_ENERGY_IDX       = 3;

inline bool InCatForm(ApPredicateContext const& ctx)
{
    return ctx.bot.has_aura(CAT_FORM);
}

inline bool CanShapeshiftNow(ApPredicateContext const& ctx)
{
    // No shifting while mounted or flying — would either dismount + shift
    // (long zone crossings) or worse, kill flight and drop us.
    auto const& mv = ctx.bot.raw().movement;
    if (mv.is_mounted) return false;
    if (mv.is_flying)  return false;
    return true;
}

inline uint8 ComboPoints(ApPredicateContext const& ctx)
{
    return static_cast<uint8>(ctx.bot.power(POWER_COMBO_POINTS_IDX));
}

// ---- Defensive (form-independent, off-GCD) ----
// Barkskin: 20% damage reduction, off-GCD, fires from any form. Threshold
// 60% — slightly more eager than the generic 50% since the druid baseline
// has no other big DR cooldown.
BASELINE_DEFENSIVE_RULE(Barkskin, BARKSKIN, 60)

// Cenarion Ward: pre-shield HoT trigger. Refires after fall-off.
BASELINE_SELFBUFF_RULE(CenarionWard, CENARION_WARD)

// ---- Emergency hard-cast heal (Regrowth) ----
// Regrowth is the only direct heal a druid has at low level. It REQUIRES
// caster form — casting it shifts us out of cat. That's an accepted cost
// at <=50% HP: dying with full CP is worse than spending a GCD healing
// and re-shifting next tick. The Cat Form rule sits high in priority so
// re-entry costs at most ~1.5s of caster-form exposure.
bool ShouldRegrowthSelfPanic(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(REGROWTH)) return false;
    if (ctx.bot.hp_pct() > 50) return false;
    return true;
}
void DoRegrowthSelfPanic(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(REGROWTH, ctx.bot.raw().guid);
}

// ---- HoT self-maintain (Rejuvenation) ----
// Rejuv requires caster form, so we gate on `!in_combat` — refreshing a
// HoT mid-pull would otherwise blast us out of cat every 12s. OOC top-up
// only; emergency healing during combat is Regrowth's job.
bool ShouldRejuvenationSelf(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(REJUVENATION)) return false;
    if (!ctx.bot.is_ready(REJUVENATION)) return false;
    if (ctx.bot.in_combat()) return false;             // OOC-only — see header
    if (ctx.bot.hp_pct() > 80) return false;
    return ctx.bot.find_aura(REJUVENATION, ObjectGuid::Empty) == nullptr;
}
void DoRejuvenationSelf(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(REJUVENATION, ObjectGuid::Empty);
}

// ---- Buff (Mark of the Wild) ----
// Mark requires caster form. Gate on `!in_combat` so we don't shift out
// of cat mid-pull just to refresh a buff. The aura is hours long; the
// "refresh OOC between pulls" cadence is fine.
bool ShouldMarkOfTheWild(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(MARK_OF_THE_WILD)) return false;
    if (!ctx.bot.is_ready(MARK_OF_THE_WILD)) return false;
    if (ctx.bot.in_combat()) return false;
    return ctx.bot.find_aura(MARK_OF_THE_WILD, ObjectGuid::Empty) == nullptr;
}
void DoMarkOfTheWild(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MARK_OF_THE_WILD, ctx.bot.raw().guid);
}

// ---- Form entry: Cat Form ----
// Fires when:
//   * we know the spell,
//   * we aren't already in cat,
//   * we have a live target (combat ramp) OR we're OOC and could Prowl,
//   * we can safely shapeshift (not mounted, not flying).
// We deliberately allow shifting OOC too — it lets Prowl come up before
// the bot engages, and keeps the bot in cat form between pulls so we
// don't pay the 1.5s shift-back tax on every engage.
bool ShouldCatForm(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(CAT_FORM)) return false;
    if (InCatForm(ctx)) return false;
    if (!CanShapeshiftNow(ctx)) return false;
    return true;
}
void DoCatForm(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(CAT_FORM);
}

// ---- Prowl (OOC stealth opener) ----
// Only useful when:
//   * in cat form,
//   * out of combat (Prowl breaks on damage / combat),
//   * not already prowling.
// Skipped while mounted (would have dropped cat already) — covered by
// the in-cat gate.
bool ShouldProwl(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(PROWL)) return false;
    if (!ctx.bot.is_ready(PROWL)) return false;
    if (!InCatForm(ctx)) return false;
    if (ctx.bot.in_combat()) return false;
    return !ctx.bot.has_aura(PROWL);
}
void DoProwl(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(PROWL);
}

// ---- Tiger's Fury (cat-form energy CD) ----
// Fills energy + boosts damage. Only worthwhile when we have room for
// the refund (energy <= 50). Requires cat form to be useful — gate on it
// so we don't pop the CD outside cat (it works there too but wastes the
// energy refund).
bool ShouldTigersFury(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(TIGERS_FURY)) return false;
    if (!ctx.bot.is_ready(TIGERS_FURY)) return false;
    if (!InCatForm(ctx)) return false;
    return ctx.bot.power(POWER_ENERGY_IDX) <= 50;
}
void DoTigersFury(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(TIGERS_FURY);
}

// ---- Rake (cat-form bleed CP builder) ----
// Apply / refresh on victim. Costs energy; requires cat form. We refresh
// when the bleed is missing or has <=4.5s remaining (pandemic-window
// approximation — Feral uses the same threshold).
bool ShouldRake(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RAKE)) return false;
    if (!InCatForm(ctx)) return false;
    AuraEntry const* a = ctx.bot.find_aura(RAKE_DEBUFF, ctx.bot.victim());
    if (a && a->remaining.count() > 4500) return false;
    return true;
}
void DoRake(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RAKE, ctx.bot.victim());
}

// ---- Rip (cat-form 5-CP bleed finisher) ----
// Highest priority finisher when Rip is missing/short and we have CP.
// Baseline relaxes Feral's "5 CP only" to "4+ CP" — a 4-CP Rip is still
// better than no Rip at all on this bracket.
bool ShouldRip(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RIP)) return false;
    if (!InCatForm(ctx)) return false;
    if (ComboPoints(ctx) < 4) return false;
    AuraEntry const* a = ctx.bot.find_aura(RIP, ctx.bot.victim());
    if (a && a->remaining.count() > 4500) return false;
    return true;
}
void DoRip(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RIP, ctx.bot.victim());
}

// ---- Ferocious Bite (cat-form 4-5 CP finisher) ----
// Fires only AFTER Rip is up (Rip rule sits higher in the table so this
// only triggers when Rip is present + long-duration). Baseline: 4+ CP.
bool ShouldFerociousBite(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FEROCIOUS_BITE)) return false;
    if (!ctx.bot.is_ready(FEROCIOUS_BITE)) return false;
    if (!InCatForm(ctx)) return false;
    if (ComboPoints(ctx) < 4) return false;
    return true;
}
void DoFerociousBite(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FEROCIOUS_BITE, ctx.bot.victim());
}

// ---- Thrash (cat-form AoE bleed, 3+ enemies) ----
// Applies a bleed to all nearby enemies; refresh when missing or short.
bool ShouldThrashCat(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(THRASH_CAT)) return false;
    if (!InCatForm(ctx)) return false;
    if (ctx.bot.enemies_within(8.0f) < 3) return false;
    AuraEntry const* a = ctx.bot.find_aura(THRASH_CAT, ctx.bot.victim());
    if (a && a->remaining.count() > 4500) return false;
    return true;
}
void DoThrashCat(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(THRASH_CAT);
}

// ---- Swipe (cat-form AoE damage, 3+ enemies) ----
bool ShouldSwipeCat(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SWIPE_CAT)) return false;
    if (!InCatForm(ctx)) return false;
    return ctx.bot.enemies_within(8.0f) >= 3;
}
void DoSwipeCat(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(SWIPE_CAT);
}

// ---- Shred (cat-form CP filler) ----
// Primary cat-form filler. Energy floor of 40 mirrors Feral; below that
// the auto-attack rule will tick energy back up.
bool ShouldShred(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHRED)) return false;
    if (!InCatForm(ctx)) return false;
    return ctx.bot.power(POWER_ENERGY_IDX) >= 40;
}
void DoShred(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHRED, ctx.bot.victim());
}

// ---- Caster-form fallbacks (only when NOT in cat) ----
// These exist for the edge cases where the bot can't be in cat form:
// pre-Cat-Form level (very early class quests for non-druid starting
// races), polymorphed, aura-stripped, or just shapeshift-dispelled. They
// are gated on `!InCatForm` so they don't fire while we're in cat (which
// would either fail to cast or — worse — shift us out).

bool ShouldWrathCaster(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(WRATH)) return false;
    if (InCatForm(ctx)) return false;
    return true;
}
void DoWrathCaster(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(WRATH, ctx.bot.victim());
}

bool ShouldMoonfireCaster(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MOONFIRE)) return false;
    if (!ctx.bot.is_ready(MOONFIRE)) return false;
    if (InCatForm(ctx)) return false;
    return ctx.bot.find_aura(MOONFIRE, ctx.bot.victim()) == nullptr;
}
void DoMoonfireCaster(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MOONFIRE, ctx.bot.victim());
}

bool ShouldEntanglingRoots(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(ENTANGLING_ROOTS)) return false;
    if (!ctx.bot.is_ready(ENTANGLING_ROOTS)) return false;
    if (InCatForm(ctx)) return false;                       // caster-form only
    if (ctx.bot.enemies_within(8.0f) == 0) return false;
    return ctx.bot.find_aura(ENTANGLING_ROOTS, ctx.bot.victim()) == nullptr;
}
void DoEntanglingRoots(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ENTANGLING_ROOTS, ctx.bot.victim());
}

// ---- Rule table ----
// Order rationale (top = highest priority):
//   1.  Barkskin             — off-GCD panic DR, fires from any form.
//   2.  Cenarion Ward        — pre-shield, form-independent.
//   3.  RegrowthSelfPanic    — emergency hard-cast heal at <=50%.
//                              DROPS cat form; accepted cost vs. dying.
//   4.  MarkOfTheWild        — OOC buff maintenance (caster form).
//   5.  RejuvenationSelf     — OOC HoT top-up when wounded.
//   6.  CatForm              — re-enter cat ASAP after any caster-form
//                              spell or fresh login. High priority so the
//                              caster-form-exposure window is short.
//   7.  Prowl                — OOC stealth opener once in cat.
//   8.  TigersFury           — energy CD when running low.
//   9.  Rake                 — bleed CP builder (refresh on victim).
//   10. ThrashCat            — AoE bleed at 3+ targets.
//   11. SwipeCat             — AoE damage at 3+ targets.
//   12. Rip                  — 4-5 CP bleed finisher (priority over FB).
//   13. FerociousBite        — 4-5 CP direct finisher (only after Rip).
//   14. Shred                — CP filler.
//   15. EntanglingRoots      — caster-form CC fallback (only if not cat).
//   16. MoonfireCaster       — caster-form DoT fallback.
//   17. WrathCaster          — caster-form nuke fallback.
//   18. AutoAttack           — no-mana floor (works in cat).
ApRule const baseline_druid_kRules[] = {
    { ShouldBarkskin,          DoBarkskin,          "Barkskin (<60% panic DR)"          },
    { ShouldCenarionWard,      DoCenarionWard,      "Cenarion Ward (pre-shield)"        },
    { ShouldRegrowthSelfPanic, DoRegrowthSelfPanic, "Regrowth (<=50% self panic)"       },
    { ShouldMarkOfTheWild,     DoMarkOfTheWild,     "Mark of the Wild (OOC buff)"       },
    { ShouldRejuvenationSelf,  DoRejuvenationSelf,  "Rejuvenation (<=80% OOC HoT)"      },
    { ShouldCatForm,           DoCatForm,           "Cat Form (form entry)"             },
    { ShouldProwl,             DoProwl,             "Prowl (OOC stealth opener)"        },
    { ShouldTigersFury,        DoTigersFury,        "Tiger's Fury (energy CD)"          },
    { ShouldRake,              DoRake,              "Rake (cat bleed builder)"          },
    { ShouldThrashCat,         DoThrashCat,         "Thrash cat (3+ AoE bleed)"         },
    { ShouldSwipeCat,          DoSwipeCat,          "Swipe cat (3+ AoE damage)"         },
    { ShouldRip,               DoRip,               "Rip (4+ CP bleed finisher)"        },
    { ShouldFerociousBite,     DoFerociousBite,     "Ferocious Bite (4+ CP finisher)"   },
    { ShouldShred,             DoShred,             "Shred (CP filler)"                 },
    { ShouldEntanglingRoots,   DoEntanglingRoots,   "Entangling Roots (caster CC)"      },
    { ShouldMoonfireCaster,    DoMoonfireCaster,    "Moonfire (caster DoT fallback)"    },
    { ShouldWrathCaster,       DoWrathCaster,       "Wrath (caster nuke fallback)"      },
    { AlwaysInCombat,          DoAutoAttack,        "Auto attack"                       },
};

} // anonymous

void RegisterApl_Baseline_Druid()
{
    RegisterRotation(CLASS_DRUID, 0, ApRotation{baseline_druid_kRules});
}

} // namespace Playerbot::Combat
