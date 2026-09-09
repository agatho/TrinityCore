// Restoration Druid - WoW 12.1.0.69587 (Midnight) spec rotation (specId 105).
//
// Stance / form
// -------------
// Resto lives in CASTER FORM. The baseline druid file would otherwise
// park us in Cat Form (any-druid leveling default); for Resto we run the
// spec table first and there are no form-shift rules here. Healers don't
// benefit from any druid stance and Tree of Life (Incarnation: Tree of
// Life) is the one optional form burst — talented + temporary. Outside
// of that we stay in our human/elf form casting heals.
//
// Decision tree (12.1)
// --------------------
//   1) Cast-swap shim       - cancel current heal if a more urgent target appeared
//   2) Emergency layer      - Rebirth, MotW, Symbiotic Relationship (OOC bond
//                             with the tank), Barkskin, Ironbark, Innervate
//   3) Dispel               - Nature's Cure (Magic+Curse+Poison) + Remove
//                             Corruption fallback (Curse+Poison; pre-spec
//                             cleanse) + Soothe (enrage)
//   4) Tranquility          - raid CD when 3+ at <=50% (Flourish is now a
//                             passive rider on it)
//   5) Big CDs              - Heart of the Wild (empowered Wild Growth),
//                             Convoke, Incarnation: Tree of Life
//   6) AoE heal             - Wild Growth threshold, Efflorescence placement
//                             (skipped with Lifetreading: it follows Lifebloom)
//   7) Spike heal           - Swiftmend (needs a HoT), Nature's Swiftness ->
//                             instant Regrowth, Regrowth
//   8) HoT maintenance      - Lifebloom on tank, Rejuvenation on lowest
//   9) Offensive filler     - Sunfire / Moonfire / Wrath when group is full
//
// Validated spell IDs (SpellName.csv, WoW 12.1.0.69587)
// ----------------------------------------------------
//      774  Rejuvenation           (class talent [R])
//     8936  Regrowth
//    33763  Lifebloom              (spec talent [R]; aura id == cast id)
//    18562  Swiftmend              (spec talent [R])
//    48438  Wild Growth            (class talent [R])
//      740  Tranquility            (spec talent [R]; 197721 Flourish is a
//                                   passive that extends HoTs during it)
//   145205  Efflorescence          (spec talent [R]; 1217941 Lifetreading
//                                   passive [R] makes it auto-follow Lifebloom)
//   132158  Nature's Swiftness     (spec talent [R]; next Regrowth instant)
//  1261867  Heart of the Wild      (class talent [R]; caster = empowered Wild Growth)
//   391528  Convoke the Spirits    (spec talent [R])
//    88423  Nature's Cure          (spec spell; Magic + Curse + Poison)
//   440015  Remove Corruption      (spec spell; Curse + Poison fallback)
//     2908  Soothe                 (class talent [R])
//    29166  Innervate              (class talent [R])
//    20484  Rebirth
//    22812  Barkskin
//   102342  Ironbark               (spec talent [R])
//   474750  Symbiotic Relationship (class talent [R]; 474754 is the bond aura
//                                   on the ally)
//     1126  Mark of the Wild
//    93402  Sunfire                (class talent, not in Resto build; DoT 164815)
//     8921  Moonfire               (filler DoT; DoT aura 164812)
//     5176  Wrath                  (baseline Wrath; Balance learns 190984)
//    33891  Incarnation: Tree of Life (spec talent, not in build)
//
// Skipped spells (and why)
// ------------------------
//   * 102351 Cenarion Ward - the talent is gone from the 12.1 class tree
//     (102352 that remains is the ward's HoT sub-spell, not a cast).
//   * 391888 Adaptive Swarm, 108238 Renewal - not learnable in 12.1.
//   * 1226140 Grove Guardians - a passive in 12.1 (Swiftmend / Wild Growth
//     summon the treant automatically); no cast rule.
//   * 197721 Flourish - passive in 12.1 (rides on Tranquility).
//   * 212040 Revitalize - out-of-combat mass resurrection; not a rotation
//     ability.
//   * 22842 Frenzied Regeneration, 106898 Stampeding Roar, 102401 Wild
//     Charge, 102793 Ursol's Vortex, 132469 Typhoon - [R] class talents that
//     need Bear Form or positioning the healer rotation does not do.
//   * 1229376 Single-Button Assistant - client convenience macro.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "ApDispelHelpers.h"
#include "ApHealHelpers.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated against SpellName.csv) ----
constexpr uint32 REJUVENATION      = 774;          // class talent [R]
constexpr uint32 REGROWTH          = 8936;
constexpr uint32 LIFEBLOOM         = 33763;        // spec talent [R]
constexpr uint32 SWIFTMEND         = 18562;        // spec talent [R]
constexpr uint32 WILD_GROWTH       = 48438;        // class talent [R]
constexpr uint32 TRANQUILITY       = 740;          // spec talent [R]
constexpr uint32 EFFLORESCENCE     = 145205;       // spec talent [R]
constexpr uint32 LIFETREADING      = 1217941;      // spec passive [R] - Efflorescence follows Lifebloom target
constexpr uint32 NATURES_SWIFTNESS = 132158;       // spec talent [R] - next Regrowth instant + free
constexpr uint32 HEART_OF_THE_WILD = 1261867;      // class talent [R] - caster: empowered Wild Growth
constexpr uint32 CONVOKE_SPIRITS   = 391528;       // spec talent [R] burst
constexpr uint32 NATURES_CURE      = 88423;        // spec spell: Magic+Curse+Poison (overrides 440015)
constexpr uint32 REMOVE_CORRUPTION = 440015;       // spec spell: Curse+Poison fallback
constexpr uint32 SOOTHE            = 2908;         // class talent [R]
constexpr uint32 INNERVATE         = 29166;        // class talent [R]
constexpr uint32 REBIRTH           = 20484;
constexpr uint32 BARKSKIN          = 22812;
constexpr uint32 IRONBARK          = 102342;       // spec talent [R]
constexpr uint32 SYMBIOTIC_RELATIONSHIP = 474750;  // class talent [R] - OOC bond with the tank
constexpr uint32 SYMBIOTIC_BOND_AURA    = 474754;  // aura on the bonded ally
constexpr uint32 MARK_OF_THE_WILD  = 1126;
constexpr uint32 SUNFIRE           = 93402;        // class talent (not in Resto build) - filler DoT
constexpr uint32 SUNFIRE_DOT       = 164815;       // Sunfire periodic aura
constexpr uint32 MOONFIRE          = 8921;         // filler DoT (cast id)
constexpr uint32 MOONFIRE_DOT      = 164812;       // Moonfire periodic aura
constexpr uint32 WRATH             = 5176;         // Resto's baseline Wrath learn
constexpr uint32 INCARN_TREE       = 33891;        // spec talent (not in build) - Incarnation: Tree of Life

// ---- Helpers ----
struct HealTarget
{
    ObjectGuid guid;
    int32      hp_pct;
};

HealTarget LowestFriendOrSelf(ApPredicateContext const& ctx)
{
    HealTarget t{ ctx.bot.raw().guid, ctx.bot.hp_pct() };
    if (auto const* low = ctx.group.heal_assignment(ctx.bot.raw().guid, ctx.bot.map_id(), ctx.bot.raw().position.x, ctx.bot.raw().position.y, ctx.bot.raw().position.z, 45.0f))
    {
        if (low->online && low->max_hp > 0)
        {
            const int32 pct = (low->hp * 100) / low->max_hp;
            if (pct < t.hp_pct) { t.guid = low->guid; t.hp_pct = pct; }
        }
    }
    return t;
}

int WoundedFriendCount(ApPredicateContext const& ctx, int below_pct)
{
    int n = 0;
    auto const* members = ctx.group.members();
    // SOLO (audit B22): ungrouped, "wounded friend" used to collapse to
    // "my own HP <= below_pct" — the 92% GroupTopped gates then froze ALL
    // damage the moment a questing healer took two melee hits, degenerating
    // solo healer-spec leveling into heal-regen-nuke loops (3-10x kill
    // time). Cap the solo threshold at a 45% survival floor: topped-style
    // gates (92) stay open while merely scratched, true emergency heals
    // (<=45) keep their thresholds.
    if (!members) return ctx.bot.hp_pct() <= std::min(below_pct, 45) ? 1 : 0;
    for (auto const& m : *members)
    {
        if (!m.online || m.max_hp <= 0 || m.hp <= 0) continue;
        if ((m.hp * 100) / m.max_hp <= below_pct) ++n;
    }
    return n;
}

// True when the group is "topped" — every reachable member at >=92% HP. Used
// to gate offensive filler so we don't drift into combat-DPS mode mid-spike.
bool GroupTopped(ApPredicateContext const& ctx)
{
    return WoundedFriendCount(ctx, 92) == 0;
}

GroupMemberSummary const* DispelTarget(ApPredicateContext const& ctx)
{
    return DispelTargetWithPriority(ctx, [](GroupSnapshotView const& g)
        -> GroupMemberSummary const*
    {
        if (auto const* m = g.dispel_candidate(DispelType::Magic))  return m;
        if (auto const* m = g.dispel_candidate(DispelType::Curse))  return m;
        if (auto const* m = g.dispel_candidate(DispelType::Poison)) return m;
        return nullptr;
    });
}

// Remove Corruption fallback: handles Curse+Poison only (no Magic). Used
// when Nature's Cure isn't known yet — typically very low-level Resto
// before the spec passive grants the upgrade. Same priority ordering but
// without the Magic check.
GroupMemberSummary const* DispelTargetCursePoison(ApPredicateContext const& ctx)
{
    return DispelTargetWithPriority(ctx, [](GroupSnapshotView const& g)
        -> GroupMemberSummary const*
    {
        if (auto const* m = g.dispel_candidate(DispelType::Curse))  return m;
        if (auto const* m = g.dispel_candidate(DispelType::Poison)) return m;
        return nullptr;
    });
}

bool SelfNeedsDispel(ApPredicateContext const& ctx)
{
    return ctx.bot.self_dispellable(DispelType::Magic)
        || ctx.bot.self_dispellable(DispelType::Curse)
        || ctx.bot.self_dispellable(DispelType::Poison);
}

bool SelfNeedsDispelCursePoison(ApPredicateContext const& ctx)
{
    return ctx.bot.self_dispellable(DispelType::Curse)
        || ctx.bot.self_dispellable(DispelType::Poison);
}

// ---- Emergency layer ----
bool ShouldBarkskin(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(BARKSKIN)) return false;
    if (!ctx.bot.is_ready(BARKSKIN)) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoBarkskin(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BARKSKIN); }

// Symbiotic Relationship: hour-long bond with the tank - our heals on the
// tank heal us and our self-heals heal the tank. OOC maintenance only
// (1.5s cast); the bond aura 474754 sits on the ally.
bool ShouldSymbioticRelationship(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SYMBIOTIC_RELATIONSHIP)) return false;
    if (!ctx.bot.is_ready(SYMBIOTIC_RELATIONSHIP)) return false;
    if (ctx.bot.in_combat()) return false;
    GroupMemberSummary const* tank = ctx.group.tank();
    if (!tank || !tank->online || tank->hp <= 0) return false;
    if (tank->guid == ctx.bot.raw().guid) return false;
    return !ctx.bot.has_aura(SYMBIOTIC_BOND_AURA, tank->guid);
}
void DoSymbioticRelationship(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* tank = ctx.group.tank())
        e.cast(SYMBIOTIC_RELATIONSHIP, tank->guid);
}

bool ShouldIronbark(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(IRONBARK)) return false;
    if (!ctx.bot.is_ready(IRONBARK)) return false;
    GroupMemberSummary const* tank = ctx.group.tank();
    if (!tank || !tank->online || tank->max_hp <= 0 || tank->hp <= 0) return false;
    return (tank->hp * 100) / tank->max_hp <= 50;
}
void DoIronbark(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* tank = ctx.group.tank())
        e.cast(IRONBARK, tank->guid);
}

bool ShouldInnervate(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(INNERVATE)) return false;
    if (!ctx.bot.is_ready(INNERVATE)) return false;
    if (auto const* m = ctx.group.lowest_mana_caster())
        if (m->max_mana > 0 && (m->mana * 100) / m->max_mana <= 30)
            return true;
    return ctx.bot.max_power(0) > 0 && ctx.bot.power_pct(0) <= 30;
}
void DoInnervate(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* m = ctx.group.lowest_mana_caster())
        if (m->max_mana > 0 && (m->mana * 100) / m->max_mana <= 30)
        { e.cast(INNERVATE, m->guid); return; }
    e.cast(INNERVATE, ctx.bot.raw().guid);
}

// ---- Dispel ----
// Nature's Cure: the full Resto cleanse (Magic + Curse + Poison). Primary
// dispel; sits ahead of Remove Corruption in the rule table.
bool ShouldNaturesCure(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(NATURES_CURE)) return false;
    if (!ctx.bot.is_ready(NATURES_CURE)) return false;
    return DispelTarget(ctx) != nullptr || SelfNeedsDispel(ctx);
}
void DoNaturesCure(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = DispelTarget(ctx))
    {
        e.cast(NATURES_CURE, t->guid);
        return;
    }
    if (SelfNeedsDispel(ctx))
        e.cast(NATURES_CURE, ctx.bot.raw().guid);
}

// Remove Corruption: Curse + Poison only. Fallback for pre-Nature's-Cure
// brackets and a redundant safety net if Nature's Cure isn't known.
bool ShouldRemoveCorruption(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(REMOVE_CORRUPTION)) return false;
    if (!ctx.bot.is_ready(REMOVE_CORRUPTION)) return false;
    // If Nature's Cure is up, prefer it — that path covers Magic too.
    if (ctx.bot.knows_spell(NATURES_CURE) && ctx.bot.is_ready(NATURES_CURE)) return false;
    return DispelTargetCursePoison(ctx) != nullptr || SelfNeedsDispelCursePoison(ctx);
}
void DoRemoveCorruption(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = DispelTargetCursePoison(ctx))
    {
        e.cast(REMOVE_CORRUPTION, t->guid);
        return;
    }
    if (SelfNeedsDispelCursePoison(ctx))
        e.cast(REMOVE_CORRUPTION, ctx.bot.raw().guid);
}

bool ShouldSoothe(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SOOTHE)) return false;
    if (!ctx.bot.is_ready(SOOTHE)) return false;
    if (ctx.bot.victim().IsEmpty()) return false;
    return ctx.bot.target_dispellable(Playerbot::DispelType::Enrage);
}
void DoSoothe(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SOOTHE, ctx.bot.victim());
}

// ---- Battle rez / buff ----
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

// ---- Major CDs ----
bool ShouldTranquility(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(TRANQUILITY)) return false;
    if (!ctx.bot.is_ready(TRANQUILITY)) return false;
    return WoundedFriendCount(ctx, 50) >= 3;
}
void DoTranquility(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TRANQUILITY); }

// Heart of the Wild in caster form = an empowered Wild Growth (more
// targets, bigger heal) on a 2min CD. Fire it on real group-wide damage.
bool ShouldHeartOfTheWild(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HEART_OF_THE_WILD)) return false;
    if (!ctx.bot.is_ready(HEART_OF_THE_WILD)) return false;
    return WoundedFriendCount(ctx, 75) >= 3;
}
void DoHeartOfTheWild(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HEART_OF_THE_WILD, LowestFriendOrSelf(ctx).guid);
}

bool ShouldConvoke(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(CONVOKE_SPIRITS)) return false;
    if (!ctx.bot.is_ready(CONVOKE_SPIRITS)) return false;
    return WoundedFriendCount(ctx, 70) >= 2;
}
void DoConvoke(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CONVOKE_SPIRITS, LowestFriendOrSelf(ctx).guid);
}

// Incarnation: Tree of Life — 30s burst form. Empowers Wild Growth (extra
// target), Regrowth (instant), Rejuvenation (cheaper). Pop on raid-wide
// damage events.
bool ShouldIncarnTree(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(INCARN_TREE)) return false;
    if (!ctx.bot.is_ready(INCARN_TREE)) return false;
    return WoundedFriendCount(ctx, 75) >= 3;
}
void DoIncarnTree(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(INCARN_TREE); }

// ---- Instant-Regrowth setup ----
// Nature's Swiftness makes the next Regrowth instant + free. Arm it when
// the lowest ally is in spike range so the Regrowth rule below lands
// instantly instead of as a 1.5s hard cast.
bool ShouldNaturesSwiftness(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(NATURES_SWIFTNESS)) return false;
    if (!ctx.bot.is_ready(NATURES_SWIFTNESS)) return false;
    if (ctx.bot.has_aura(NATURES_SWIFTNESS)) return false;
    if (!ctx.bot.knows_spell(REGROWTH)) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 40;
}
void DoNaturesSwiftness(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(NATURES_SWIFTNESS); }

// ---- AoE heal ----
bool ShouldWildGrowth(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(WILD_GROWTH)) return false;
    if (!ctx.bot.is_ready(WILD_GROWTH)) return false;
    return WoundedFriendCount(ctx, 80) >= 3;
}
void DoWildGrowth(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(WILD_GROWTH, LowestFriendOrSelf(ctx).guid);
}

// Efflorescence: with the Lifetreading passive [R] the blossom grows under
// the Lifebloom target automatically, so only place it by hand without it.
bool ShouldEfflorescence(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(EFFLORESCENCE)) return false;
    if (ctx.bot.knows_spell(LIFETREADING)) return false;
    if (!ctx.bot.is_ready(EFFLORESCENCE)) return false;
    return !ctx.bot.has_aura(EFFLORESCENCE);
}
void DoEfflorescence(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    float bx, by, bz;
    ctx.bot.position(bx, by, bz);
    e.cast_at(EFFLORESCENCE, bx, by, bz);
}

// ---- Spike heal ----
// Swiftmend consumes (or, with Verdant Infusion, utilizes) one of our
// Rejuvenation / Regrowth / Wild Growth effects - it cannot be cast on a
// target without one, so require a HoT before spending the GCD.
bool ShouldSwiftmend(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SWIFTMEND)) return false;
    if (!ctx.bot.is_ready(SWIFTMEND)) return false;
    HealTarget t = LowestFriendOrSelf(ctx);
    if (t.hp_pct > 50) return false;
    return ctx.bot.has_aura(REJUVENATION, t.guid)
        || ctx.bot.has_aura(REGROWTH, t.guid)
        || ctx.bot.has_aura(WILD_GROWTH, t.guid);
}
void DoSwiftmend(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SWIFTMEND, LowestFriendOrSelf(ctx).guid);
}

bool ShouldRegrowth(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(REGROWTH)) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 65;
}
void DoRegrowth(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(REGROWTH, LowestFriendOrSelf(ctx).guid);
}

// ---- HoT maintenance ----
bool ShouldLifebloom(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(LIFEBLOOM)) return false;
    GroupMemberSummary const* tank = ctx.group.tank();
    ObjectGuid target = tank && tank->online ? tank->guid : ctx.bot.raw().guid;
    AuraEntry const* a = ctx.bot.find_aura(LIFEBLOOM, target);
    return !a || a->remaining.count() <= 4500;
}
void DoLifebloom(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    GroupMemberSummary const* tank = ctx.group.tank();
    ObjectGuid target = tank && tank->online ? tank->guid : ctx.bot.raw().guid;
    e.cast(LIFEBLOOM, target);
}

bool ShouldRejuvenation(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(REJUVENATION)) return false;
    HealTarget t = LowestFriendOrSelf(ctx);
    if (t.hp_pct >= 95) return false;
    AuraEntry const* a = ctx.bot.find_aura(REJUVENATION, t.guid);
    return !a || a->remaining.count() <= 3000;
}
void DoRejuvenation(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(REJUVENATION, LowestFriendOrSelf(ctx).guid);
}

// ---- Offensive filler (group is topped) ----
bool ShouldSunfire(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!GroupTopped(ctx)) return false;
    if (!ctx.bot.knows_spell(SUNFIRE)) return false;
    if (ctx.bot.victim().IsEmpty()) return false;
    AuraEntry const* a = ctx.bot.find_aura(SUNFIRE_DOT, ctx.bot.victim());
    return !a || a->remaining.count() <= 4000;
}
void DoSunfire(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SUNFIRE, ctx.bot.victim());
}

bool ShouldMoonfire(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!GroupTopped(ctx)) return false;
    if (!ctx.bot.knows_spell(MOONFIRE)) return false;
    if (ctx.bot.victim().IsEmpty()) return false;
    AuraEntry const* a = ctx.bot.find_aura(MOONFIRE_DOT, ctx.bot.victim());
    return !a || a->remaining.count() <= 4000;
}
void DoMoonfire(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MOONFIRE, ctx.bot.victim());
}

bool ShouldWrath(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!GroupTopped(ctx)) return false;
    if (!ctx.bot.knows_spell(WRATH)) return false;
    return !ctx.bot.victim().IsEmpty();
}
void DoWrath(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(WRATH, ctx.bot.victim());
}

bool AlwaysAlive(ApPredicateContext const& ctx) { return ctx.bot.is_alive(); }
void DoNothing(ApPredicateContext const&, BotIntentEmitter&) {}

// Cast-swap shim — Resto Druid kit is HoT-heavy (Rejuvenation /
// Lifebloom / Swiftmend = instant). REGROWTH 1.5s is the main slow
// direct heal worth cancelling. See ApHealHelpers.h.
bool ShouldCancelHealForSwap(ApPredicateContext const& ctx)
{
    return ShouldCancelHealForSwapImpl(ctx, { REGROWTH });
}

// ---- Rule table (priority order top-down) ----
// Order rationale:
//   1.  Cancel heal swap      - drop slow cast when a more urgent target appears.
//   2.  Rebirth               - battle rez has highest party impact.
//   3.  Mark of the Wild      - OOC group buff maintenance.
//   4.  Symbiotic Relationship - OOC bond with the tank (hour-long).
//   5.  Barkskin              - off-GCD survival.
//   6.  Ironbark              - tank DR + heal amp at <=50%.
//   7.  Nature's Cure         - full dispel (Magic + Curse + Poison).
//   8.  Remove Corruption     - Curse+Poison fallback when NC unknown.
//   9.  Soothe                - enrage dispel on target.
//   10. Innervate             - own / ally caster mana.
//   11. Tranquility           - raid CD at 3+ heavily wounded.
//   12. Heart of the Wild     - empowered Wild Growth at 3+ wounded.
//   13. Convoke               - burst heal/damage on 2+ wounded.
//   14. Incarnation: Tree of Life - 30s heal-burst window (talent).
//   15. Wild Growth           - group AoE HoT (3+ at <=80%).
//   16. Efflorescence         - ground AoE HoT placement (no Lifetreading).
//   17. Swiftmend             - instant spike heal (<=50%, needs a HoT).
//   18. Nature's Swiftness    - arm instant Regrowth (<=40%).
//   19. Regrowth              - direct heal + HoT (<=65%).
//   20. Lifebloom             - tank HoT maintenance.
//   21. Rejuvenation          - lowest-friend HoT refresh.
//   22. Sunfire / Moonfire / Wrath - offensive filler ONLY when group is topped.
//   23. Idle                  - alive fallthrough.
ApRule const kRules[] = {
    { ShouldCancelHealForSwap, DoCancelHealForSwap, "Cancel heal — swap to lower target" },
    { ShouldRebirth,        DoRebirth,        "Rebirth (battle rez)"         },
    { ShouldMarkOfTheWild,  DoMarkOfTheWild,  "Mark of the Wild"             },
    { ShouldSymbioticRelationship, DoSymbioticRelationship, "Symbiotic Relationship" },
    { ShouldBarkskin,       DoBarkskin,       "Barkskin (<=50% self)"        },
    { ShouldIronbark,       DoIronbark,       "Ironbark (tank <=50%)"        },
    { ShouldNaturesCure,    DoNaturesCure,    "Nature's Cure (dispel)"       },
    { ShouldRemoveCorruption, DoRemoveCorruption, "Remove Corruption (fb)"   },
    { ShouldSoothe,         DoSoothe,         "Soothe (enrage)"              },
    { ShouldInnervate,      DoInnervate,      "Innervate"                    },
    { ShouldTranquility,    DoTranquility,    "Tranquility (3+ at <=50%)"    },
    { ShouldHeartOfTheWild, DoHeartOfTheWild, "Heart of the Wild (WG)"       },
    { ShouldConvoke,        DoConvoke,        "Convoke the Spirits"          },
    { ShouldIncarnTree,     DoIncarnTree,     "Incarnation: Tree of Life"    },
    { ShouldWildGrowth,     DoWildGrowth,     "Wild Growth (3+ at <=80%)"    },
    { ShouldEfflorescence,  DoEfflorescence,  "Efflorescence (maintain)"     },
    { ShouldSwiftmend,      DoSwiftmend,      "Swiftmend (<=50%)"            },
    { ShouldNaturesSwiftness, DoNaturesSwiftness, "Nature's Swiftness (<=40%)" },
    { ShouldRegrowth,       DoRegrowth,       "Regrowth (<=65%)"             },
    { ShouldLifebloom,      DoLifebloom,      "Lifebloom on tank"            },
    { ShouldRejuvenation,   DoRejuvenation,   "Rejuvenation (refresh)"       },
    { ShouldSunfire,        DoSunfire,        "Sunfire (filler, group full)" },
    { ShouldMoonfire,       DoMoonfire,       "Moonfire (filler)"            },
    { ShouldWrath,          DoWrath,          "Wrath (filler)"               },
    { AlwaysAlive,          DoNothing,        "Idle"                         },
};

} // anonymous

void RegisterApl_Druid_Restoration()
{
    constexpr uint32 SPEC_DRUID_RESTORATION = 105;
    RegisterRotation(CLASS_DRUID, SPEC_DRUID_RESTORATION, ApRotation{kRules});
}

} // namespace Playerbot::Combat
