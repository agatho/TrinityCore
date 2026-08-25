// Feral Druid — WoW 12.0 spec rotation (specId 103).
//
// Stance / form
// -------------
// Feral lives in CAT FORM. The baseline druid file already runs a cat-
// form-centric kit (Rake / Rip / Shred / FB at 4+ CP); Feral SPEC tightens
// that to:
//
//   * 5-CP-only finishers (Rip / Ferocious Bite / Primal Wrath) — better
//     damage per CP than baseline's 4+ CP threshold.
//   * Predatory Swiftness proc tracking — instant-Regrowth window after a
//     5-CP finisher, used for self-top-up without losing GCDs to a hard
//     cast.
//   * Feral Frenzy (talent) — 5-CP instant builder + bleed on the talent
//     tree.
//   * Tiger's Fury / Berserk / Incarnation: King of the Jungle CD pair.
//   * Convoke the Spirits when burst windows are aligned.
//   * Bear Form bail at <=25% as a last-ditch when both Survival
//     Instincts and Barkskin are on CD — armor + reduced damage gives
//     the healer a window to top us up before we re-shift to cat.
//
// We do NOT cast Moonkin Form here. The baseline rotation handles cat-
// form re-entry too; this spec rotation just runs FIRST, so our stricter
// form-management gate fires before the baseline can take over.
//
// Validated spell IDs (SpellName.csv, WoW 12.0)
// ---------------------------------------------
//      768  Cat Form
//     5487  Bear Form              (emergency bail)
//     5221  Shred
//     1822  Rake
//   155722  Rake (debuff)
//     1079  Rip
//   106830  Thrash (Cat)           (cast = debuff)
//    22568  Ferocious Bite
//     5217  Tiger's Fury
//   106951  Berserk
//   102543  Incarnation: Avatar of Ashamane (Feral "Incarnation: King of
//                                            the Jungle" override id —
//                                            spell name in DB2 reads
//                                            "Avatar of Ashamane")
//   391528  Convoke the Spirits    (talent burst)
//   274837  Feral Frenzy           (talent 5-CP builder)
//   202028  Brutal Slash           (talent replaces Swipe)
//   155625  Moonfire (Feral cat variant — Lunar Inspiration learn)
//   106839  Skull Bash
//   106785  Swipe (Cat)
//   285381  Primal Wrath
//    22570  Maim                   (CP-spend stun)
//     5211  Mighty Bash
//     2908  Soothe
//    20484  Rebirth
//    29166  Innervate
//   108238  Renewal
//     8936  Regrowth
//    61336  Survival Instincts
//    22812  Barkskin
//     5215  Prowl
//     1126  Mark of the Wild
//    16974  Predatory Swiftness    (talent — also the buff aura id; 69369
//                                   is the proc aura the user may see in
//                                   the combat log, but find_aura(16974)
//                                   resolves to the buff in this build)
//
// Skipped spells (and why)
// ---------------------------
//   * 300349  Feline Adept   — passive talent (mana cost reductions /
//     form bonuses), not a cast. Implicit benefit.
//   * 405834  Improved Prowl — passive talent. Modifies Rake's opener
//     damage when used out of stealth; no predicate needed.
//   * 197490  Feral Affinity — passive talent that grants Rake/Shred/Rip
//     to non-Feral specs. Already in our toolkit; no extra rule.
//   * 163505  Rake opener bonus — cosmetic/extra-damage modifier on the
//     stealth opener, not a cast.
//   * Mark of the Wild during combat — would drop cat form. Gated on
//     `!in_combat` in the ShouldMarkOfTheWild predicate.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated against SpellName.csv) ----
constexpr uint32 CAT_FORM            = 768;
constexpr uint32 BEAR_FORM           = 5487;       // emergency bail
constexpr uint32 SHRED               = 5221;
constexpr uint32 RAKE                = 1822;
constexpr uint32 RAKE_DEBUFF         = 155722;
constexpr uint32 RIP                 = 1079;
constexpr uint32 THRASH_CAT          = 106830;
constexpr uint32 THRASH_DEBUFF       = 106830;     // matches cast id
constexpr uint32 FEROCIOUS_BITE      = 22568;
constexpr uint32 TIGERS_FURY         = 5217;
constexpr uint32 BERSERK             = 106951;
constexpr uint32 INCARNATION_KING_OF_THE_JUNGLE = 102543;
constexpr uint32 CONVOKE_SPIRITS     = 391528;     // talent burst
constexpr uint32 FERAL_FRENZY        = 274837;     // talent — 5cp instant builder
constexpr uint32 BRUTAL_SLASH        = 202028;     // talent — replaces Swipe
constexpr uint32 MOONFIRE_FERAL      = 155625;
constexpr uint32 SKULL_BASH          = 106839;
constexpr uint32 SWIPE_CAT           = 106785;
constexpr uint32 PRIMAL_WRATH        = 285381;
constexpr uint32 MAIM                = 22570;       // 5sec stun (CP)
constexpr uint32 MIGHTY_BASH         = 5211;
constexpr uint32 SOOTHE              = 2908;
constexpr uint32 REBIRTH             = 20484;
constexpr uint32 INNERVATE           = 29166;
constexpr uint32 RENEWAL             = 108238;
constexpr uint32 REGROWTH            = 8936;
constexpr uint32 SURVIVAL_INSTINCTS  = 61336;
constexpr uint32 BARKSKIN_FERAL      = 22812;
constexpr uint32 PROWL               = 5215;        // OOC stealth
constexpr uint32 MARK_OF_THE_WILD    = 1126;
constexpr uint32 PREDATORY_SWIFTNESS = 16974;       // proc aura — instant Regrowth window

// Combo points live in POWER_COMBO_POINTS (4) in WoW 12.0 power array.
constexpr uint8 POWER_COMBO_POINTS_IDX = 4;
// Energy in POWER_ENERGY (3).
constexpr uint8 POWER_ENERGY_IDX = 3;

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

uint8 ComboPoints(ApPredicateContext const& ctx)
{
    return static_cast<uint8>(ctx.bot.power(POWER_COMBO_POINTS_IDX));
}

bool CanShapeshiftNow(ApPredicateContext const& ctx)
{
    auto const& mv = ctx.bot.raw().movement;
    return !mv.is_mounted && !mv.is_flying;
}

// ---- Stealth opener (mirrors Assassination Rogue's Stealth → Garrote) ----
// Prowl out of combat so we open from stealth: the stealth Rake hits for
// bonus damage (Improved Prowl) and is the spec's strongest opener. Requires
// an enemy to be present/nearby — no point stealthing in an empty field.
bool ShouldProwl(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(PROWL)) return false;
    if (ctx.bot.in_combat()) return false;
    if (ctx.bot.has_aura(PROWL)) return false;
    if (!CanShapeshiftNow(ctx)) return false;
    // Only stealth when there's something to open on — a live victim or a
    // hostile within engage range.
    return HasLiveTarget(ctx) || ctx.bot.enemies_within(30.0f) >= 1;
}
void DoProwl(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(PROWL); }

// Stealth-opener Rake: from Prowl, Rake applies its bonus opener damage +
// stun. Fire BEFORE the standard builders so the stealth window isn't
// wasted on a plain Shred. Requires Cat Form (Prowl coexists with Cat Form
// for Feral).
bool ShouldRakeOpener(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RAKE)) return false;
    if (!ctx.bot.has_aura(PROWL)) return false;
    return true;
}
void DoRakeOpener(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RAKE, ctx.bot.victim());
}

// ---- Stance / buffs ----
// Cat Form is the spec's home stance. We enter it whenever the aura is
// missing AND we're not currently bailing in Bear Form for survival —
// the bear-form bail rule below has higher priority, so this rule only
// fires when the bail isn't active (or HP recovered above the bail
// threshold).
bool ShouldCatForm(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(CAT_FORM)) return false;
    if (ctx.bot.has_aura(CAT_FORM)) return false;
    if (!CanShapeshiftNow(ctx)) return false;
    // Don't pop Cat Form while we're bailing in Bear — the bail rule
    // wants us to stay in bear armor until HP recovers above 40%.
    if (ctx.bot.has_aura(BEAR_FORM) && ctx.bot.hp_pct() <= 40) return false;
    return true;
}
void DoCatForm(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CAT_FORM); }

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

// ---- Survival ----
// Last-ditch bail to Bear Form: both major DRs on CD, sub-25% HP. Bear
// armor + reduced damage buys the healer one or two GCDs.
bool ShouldBearFormBail(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BEAR_FORM)) return false;
    if (ctx.bot.has_aura(BEAR_FORM)) return false;
    if (!CanShapeshiftNow(ctx)) return false;
    if (ctx.bot.hp_pct() > 25) return false;
    if (ctx.bot.knows_spell(SURVIVAL_INSTINCTS) && ctx.bot.is_ready(SURVIVAL_INSTINCTS)) return false;
    if (ctx.bot.knows_spell(BARKSKIN_FERAL) && ctx.bot.is_ready(BARKSKIN_FERAL)) return false;
    return true;
}
void DoBearForm(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BEAR_FORM); }

bool ShouldSurvivalInstincts(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SURVIVAL_INSTINCTS)) return false;
    if (!ctx.bot.is_ready(SURVIVAL_INSTINCTS)) return false;
    return ctx.bot.hp_pct() <= 30;
}
void DoSurvivalInstincts(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SURVIVAL_INSTINCTS); }

bool ShouldBarkskin(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(BARKSKIN_FERAL)) return false;
    if (!ctx.bot.is_ready(BARKSKIN_FERAL)) return false;
    return ctx.bot.hp_pct() <= 60;
}
void DoBarkskin(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BARKSKIN_FERAL); }

bool ShouldRenewal(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(RENEWAL)) return false;
    if (!ctx.bot.is_ready(RENEWAL)) return false;
    return ctx.bot.hp_pct() <= 40;
}
void DoRenewal(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(RENEWAL); }

// Predatory Swiftness — instant Regrowth window. The talent buffs the
// next Regrowth to be instant + no mana cost when consumed from a 5-CP
// finisher. We exploit it whenever the buff is up and we're below 80%
// HP, because the instant cast does NOT drop Cat Form (the buff
// suppresses the form-shift). This is the spec-specific advantage over
// baseline Regrowth panic, which DOES drop us.
bool ShouldPredatorySwiftnessRegrowth(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(REGROWTH)) return false;
    if (!ctx.bot.has_aura(PREDATORY_SWIFTNESS)) return false;
    return ctx.bot.hp_pct() <= 80;
}
void DoPredatorySwiftnessRegrowth(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(REGROWTH, ctx.bot.raw().guid);
}

// Hard-cast Regrowth fallback: OOC only (in cat, it would drop us; we
// only run it between pulls when the cost is a few seconds of caster
// form before re-shifting). In combat use Predatory Swiftness above.
bool ShouldRegrowth(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(REGROWTH)) return false;
    return ctx.bot.hp_pct() <= 35 && !ctx.bot.in_combat();
}
void DoRegrowth(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(REGROWTH, ctx.bot.raw().guid);
}

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

// ---- Interrupt / CC ----
// Skull Bash: primary 13y kick. kick_target() already verifies the
// victim is mid-cast on an interruptible spell — no false fires on melee
// trash that just happens to be close.
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

// Maim is a CP-spending stun — fallback interrupt when Skull Bash is on
// CD. Needs at least 1 CP and a kick target in melee range.
bool ShouldMaim(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MAIM)) return false;
    if (!ctx.bot.is_ready(MAIM)) return false;
    if (ComboPoints(ctx) < 1) return false;
    if (ctx.bot.is_ready(SKULL_BASH) && ctx.bot.knows_spell(SKULL_BASH)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    return ctx.bot.kick_target(pvp, 5.0f) != nullptr;
}
void DoMaim(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 5.0f))
        e.cast(MAIM, c->guid);
}

bool ShouldMightyBash(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MIGHTY_BASH)) return false;
    if (!ctx.bot.is_ready(MIGHTY_BASH)) return false;
    if (ctx.bot.is_ready(SKULL_BASH) && ctx.bot.knows_spell(SKULL_BASH)) return false;
    return ctx.bot.interruptible_caster() != nullptr
        || ctx.bot.enemies_within(8.0f) >= 1;
}
void DoMightyBash(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* c = ctx.bot.interruptible_caster())
    { e.cast(MIGHTY_BASH, c->guid); return; }
    e.cast(MIGHTY_BASH, ctx.bot.victim());
}

// ---- Major offensive cooldowns ----
bool ShouldTigersFury(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(TIGERS_FURY)) return false;
    if (!ctx.bot.is_ready(TIGERS_FURY)) return false;
    if (!ctx.bot.has_aura(CAT_FORM)) return false;       // useless outside cat
    // Energy floor — only worthwhile when we have room for the +60 refund.
    return ctx.bot.power(POWER_ENERGY_IDX) <= 50;
}
void DoTigersFury(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TIGERS_FURY); }

bool ShouldBerserk(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BERSERK)) return false;
    if (!ctx.bot.is_ready(BERSERK)) return false;
    if (!ctx.bot.has_aura(CAT_FORM)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(10.0f) >= 3;
}
void DoBerserk(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BERSERK); }

bool ShouldIncarnation(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(INCARNATION_KING_OF_THE_JUNGLE)) return false;
    if (!ctx.bot.is_ready(INCARNATION_KING_OF_THE_JUNGLE)) return false;
    if (!ctx.bot.has_aura(CAT_FORM)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(10.0f) >= 3;
}
void DoIncarnation(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(INCARNATION_KING_OF_THE_JUNGLE); }

bool ShouldConvoke(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CONVOKE_SPIRITS)) return false;
    if (!ctx.bot.is_ready(CONVOKE_SPIRITS)) return false;
    // Best paired with Berserk/Incarnation; require one of them up.
    return ctx.bot.has_aura(BERSERK) || ctx.bot.has_aura(INCARNATION_KING_OF_THE_JUNGLE);
}
void DoConvoke(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CONVOKE_SPIRITS, ctx.bot.victim());
}

// Feral Frenzy: talent 5-CP instant builder with its own bleed. Best
// when our combo bar is low so we don't overcap.
bool ShouldFeralFrenzy(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FERAL_FRENZY)) return false;
    if (!ctx.bot.is_ready(FERAL_FRENZY)) return false;
    if (!ctx.bot.has_aura(CAT_FORM)) return false;
    return ComboPoints(ctx) <= 1;
}
void DoFeralFrenzy(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FERAL_FRENZY, ctx.bot.victim());
}

// ---- AoE ----
bool ShouldThrash(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(THRASH_CAT)) return false;
    if (!ctx.bot.has_aura(CAT_FORM)) return false;
    if (!ctx.aoe_preference && ctx.bot.enemies_within(8.0f) < 2) return false;
    AuraEntry const* a = ctx.bot.find_aura(THRASH_DEBUFF, ctx.bot.victim());
    return !a || a->remaining.count() <= 4500;
}
void DoThrash(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(THRASH_CAT); }

bool ShouldBrutalSlash(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BRUTAL_SLASH)) return false;
    if (!ctx.bot.is_ready(BRUTAL_SLASH)) return false;
    if (!ctx.bot.has_aura(CAT_FORM)) return false;
    return ctx.bot.enemies_within(8.0f) >= 2;
}
void DoBrutalSlash(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BRUTAL_SLASH); }

bool ShouldSwipe(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SWIPE_CAT)) return false;
    if (!ctx.bot.has_aura(CAT_FORM)) return false;
    return ctx.bot.enemies_within(8.0f) >= 3;
}
void DoSwipe(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SWIPE_CAT); }

// ---- Bleeds (primary + multi-target expand) ----
bool ShouldRakePrimary(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RAKE)) return false;
    if (!ctx.bot.has_aura(CAT_FORM)) return false;
    AuraEntry const* a = ctx.bot.find_aura(RAKE_DEBUFF, ctx.bot.victim());
    return !a || a->remaining.count() <= 4500;
}
void DoRakePrimary(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RAKE, ctx.bot.victim());
}

bool ShouldRakeExpand(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(RAKE)) return false;
    if (!ctx.bot.has_aura(CAT_FORM)) return false;
    return ctx.bot.enemy_without_my_aura(RAKE_DEBUFF, 8.0f) != nullptr;
}
void DoRakeExpand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* off = ctx.bot.enemy_without_my_aura(RAKE_DEBUFF, 8.0f))
        e.cast(RAKE, off->guid);
}

bool ShouldMoonfirePrimary(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MOONFIRE_FERAL)) return false;
    if (!ctx.bot.has_aura(CAT_FORM)) return false;
    AuraEntry const* a = ctx.bot.find_aura(MOONFIRE_FERAL, ctx.bot.victim());
    return !a || a->remaining.count() <= 4500;
}
void DoMoonfirePrimary(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MOONFIRE_FERAL, ctx.bot.victim());
}

bool ShouldMoonfireExpand(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(MOONFIRE_FERAL)) return false;
    if (!ctx.bot.has_aura(CAT_FORM)) return false;
    return ctx.bot.enemy_without_my_aura(MOONFIRE_FERAL, 30.0f) != nullptr;
}
void DoMoonfireExpand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* off = ctx.bot.enemy_without_my_aura(MOONFIRE_FERAL, 30.0f))
        e.cast(MOONFIRE_FERAL, off->guid);
}

// Strict 5-CP Rip — Feral wants the maximum-duration bleed, not the 4-CP
// shortcut the baseline accepts.
bool ShouldRip(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(RIP)) return false;
    if (!ctx.bot.has_aura(CAT_FORM)) return false;
    if (ComboPoints(ctx) < 5) return false;
    AuraEntry const* a = ctx.bot.find_aura(RIP, ctx.bot.victim());
    return !a || a->remaining.count() <= 7500;
}
void DoRip(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RIP, ctx.bot.victim());
}

// ---- Spenders ----
bool ShouldPrimalWrath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(PRIMAL_WRATH)) return false;
    if (!ctx.bot.is_ready(PRIMAL_WRATH)) return false;
    if (!ctx.bot.has_aura(CAT_FORM)) return false;
    if (ComboPoints(ctx) < 5) return false;
    return ctx.aoe_preference ||
           ctx.bot.attackers_count() >= 2 || ctx.bot.enemies_within(8.0f) >= 2;
}
void DoPrimalWrath(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(PRIMAL_WRATH, ctx.bot.victim());
}

bool ShouldFerociousBite(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FEROCIOUS_BITE)) return false;
    if (!ctx.bot.is_ready(FEROCIOUS_BITE)) return false;
    if (!ctx.bot.has_aura(CAT_FORM)) return false;
    if (ComboPoints(ctx) < 5) return false;
    // Don't bite over Rip if Rip is missing/short — we want Rip up first.
    AuraEntry const* rip = ctx.bot.find_aura(RIP, ctx.bot.victim());
    if (!rip || rip->remaining.count() <= 4500) return false;
    return true;
}
void DoFerociousBite(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FEROCIOUS_BITE, ctx.bot.victim());
}

// ---- Filler ----
bool ShouldShred(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHRED)) return false;
    if (!ctx.bot.has_aura(CAT_FORM)) return false;
    return ctx.bot.power(POWER_ENERGY_IDX) >= 40;
}
void DoShred(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHRED, ctx.bot.victim());
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

// ---- Rule table (priority order top-down) ----
// Order rationale:
//   1.  Rebirth                — battle-rez in group.
//   2.  Survival Instincts     — 50% DR on <=30%, fires before bail.
//   3.  Barkskin               — 20% DR on <=60%.
//   4.  Renewal                — instant 30% self-heal on <=40%.
//   5.  Bear Form bail         — last-ditch <=25% with both DRs on CD.
//   6.  PS Regrowth            — instant Regrowth via Predatory Swiftness
//                                proc; does NOT drop cat (talent rider).
//   7.  Regrowth OOC           — hard-cast top-up between pulls only.
//   8.  Cat Form               — spec stance entry (re-enter post-bail).
//   9.  Mark of the Wild       — OOC group buff.
//   9b. Prowl                  — OOC stealth before the pull (enemy present).
//   9c. Rake opener            — bonus-damage stealth opener while in Prowl.
//   10. Skull Bash             — primary 13y interrupt.
//   11. Maim                   — CP-spend interrupt fallback.
//   12. Mighty Bash            — interrupt last-resort (talent).
//   13. Soothe                 — enrage dispel.
//   14. Innervate              — ally caster mana.
//   15. Tiger's Fury           — energy CD when low.
//   16. Berserk                — burst CD on boss/AoE.
//   17. Incarnation: KotJ      — burst CD (talent override).
//   18. Convoke the Spirits    — paired with Berserk/Incarnation.
//   19. Feral Frenzy           — talent 5-CP builder (low CP).
//   20. Brutal Slash           — talent AoE (2+).
//   21. Thrash cat             — AoE bleed.
//   22. Swipe cat              — AoE damage (3+).
//   23. Primal Wrath           — 5-CP AoE spender.
//   24. Rip                    — 5-CP single-target bleed.
//   25. Rake primary           — single-target bleed refresh.
//   26. Rake expand            — multi-dot Rake on off-targets.
//   27. Moonfire primary       — Feral Moonfire (Lunar Inspiration).
//   28. Moonfire expand        — multi-dot Moonfire.
//   29. Ferocious Bite         — 5-CP single-target spender (after Rip).
//   30. Shred                  — CP filler.
//   31. Auto attack            — engage fallthrough.
ApRule const kRules[] = {
    { ShouldRebirth,                  DoRebirth,                  "Rebirth (battle rez)"           },
    { ShouldSurvivalInstincts,        DoSurvivalInstincts,        "Survival Instincts (<=30%)"     },
    { ShouldBarkskin,                 DoBarkskin,                 "Barkskin (<=60%)"               },
    { ShouldRenewal,                  DoRenewal,                  "Renewal (<=40%)"                },
    { ShouldBearFormBail,             DoBearForm,                 "Bear Form (panic bail)"         },
    { ShouldPredatorySwiftnessRegrowth, DoPredatorySwiftnessRegrowth, "PS Regrowth (instant)"      },
    { ShouldRegrowth,                 DoRegrowth,                 "Regrowth (OOC <=35%)"           },
    { ShouldCatForm,                  DoCatForm,                  "Cat Form"                       },
    { ShouldMarkOfTheWild,            DoMarkOfTheWild,            "Mark of the Wild"               },
    { ShouldProwl,                    DoProwl,                    "Prowl (OOC stealth opener)"     },
    { ShouldRakeOpener,               DoRakeOpener,               "Rake (stealth opener)"          },
    { ShouldSkullBash,                DoSkullBash,                "Skull Bash (interrupt)"         },
    { ShouldMaim,                     DoMaim,                     "Maim (interrupt fallback)"      },
    { ShouldMightyBash,               DoMightyBash,               "Mighty Bash"                    },
    { ShouldSoothe,                   DoSoothe,                   "Soothe (enrage)"                },
    { ShouldInnervate,                DoInnervate,                "Innervate (healer mana)"        },
    { ShouldTigersFury,               DoTigersFury,               "Tiger's Fury"                   },
    { ShouldBerserk,                  DoBerserk,                  "Berserk"                        },
    { ShouldIncarnation,              DoIncarnation,              "Incarnation: King of the Jungle"},
    { ShouldConvoke,                  DoConvoke,                  "Convoke the Spirits"            },
    { ShouldFeralFrenzy,              DoFeralFrenzy,              "Feral Frenzy (5 CP gen)"        },
    { ShouldBrutalSlash,              DoBrutalSlash,              "Brutal Slash (2+ AoE)"          },
    { ShouldThrash,                   DoThrash,                   "Thrash (2+ AoE bleed)"          },
    { ShouldSwipe,                    DoSwipe,                    "Swipe (3+ targets)"             },
    { ShouldPrimalWrath,              DoPrimalWrath,              "Primal Wrath (2+ AoE spend)"    },
    { ShouldRip,                      DoRip,                      "Rip (5 CP refresh)"             },
    { ShouldRakePrimary,              DoRakePrimary,              "Rake (primary)"                 },
    { ShouldRakeExpand,               DoRakeExpand,               "Rake (expand off-target)"       },
    { ShouldMoonfirePrimary,          DoMoonfirePrimary,          "Moonfire (primary)"             },
    { ShouldMoonfireExpand,           DoMoonfireExpand,           "Moonfire (expand off-target)"   },
    { ShouldFerociousBite,            DoFerociousBite,            "Ferocious Bite (5 CP spend)"    },
    { ShouldShred,                    DoShred,                    "Shred (filler)"                 },
    { AlwaysInCombat,                 DoAutoAttack,               "Engage auto attack"             },
};

} // anonymous

void RegisterApl_Druid_Feral()
{
    constexpr uint32 SPEC_DRUID_FERAL = 103;
    RegisterRotation(CLASS_DRUID, SPEC_DRUID_FERAL, ApRotation{kRules});
}

} // namespace Playerbot::Combat
