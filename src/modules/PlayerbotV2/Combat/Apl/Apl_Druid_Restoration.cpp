// Restoration Druid — WoW 12.0 spec rotation (specId 105).
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
// Decision tree
// -------------
//   1) Cast-swap shim       — cancel current heal if a more urgent target appeared
//   2) Emergency layer      — Rebirth, MotW, Barkskin, Renewal, Ironbark, Innervate
//   3) Dispel               — Nature's Cure (Magic+Curse+Poison) + Remove
//                             Corruption fallback (Curse+Poison; pre-spec
//                             cleanse) + Soothe (enrage)
//   4) Tranquility          — raid CD when 3+ at <=50%
//   5) Big CDs              — Flourish (extend all HoTs), Convoke,
//                             Incarnation: Tree of Life, Grove Guardians
//   6) Pre-shield           — Cenarion Ward on tank, Adaptive Swarm
//   7) AoE heal             — Wild Growth threshold, Efflorescence placement
//   8) Spike heal           — Swiftmend, Regrowth
//   9) HoT maintenance      — Lifebloom on tank, Rejuvenation on lowest
//   10) Offensive filler    — Sunfire / Moonfire / Wrath when group is full
//
// Validated spell IDs (SpellName.csv, WoW 12.0)
// ---------------------------------------------
//      774  Rejuvenation
//     8936  Regrowth
//    33763  Lifebloom
//    18562  Swiftmend
//    48438  Wild Growth
//      740  Tranquility
//   145205  Efflorescence
//   102351  Cenarion Ward          (talent — tank pre-shield)
//   197721  Flourish               (talent — extends all HoTs)
//   391528  Convoke the Spirits    (burst CD)
//   391888  Adaptive Swarm         (talent — buff/debuff hybrid)
//    88423  Nature's Cure          (Magic + Curse + Poison dispel — Resto)
//   440015  Remove Corruption      (Curse + Poison fallback — pre-Resto / shared)
//     2908  Soothe                 (enrage dispel)
//    29166  Innervate              (mana CD)
//    20484  Rebirth                (battle rez)
//    22812  Barkskin               (off-GCD 20% DR)
//   102342  Ironbark               (ally 20% DR + heal amp)
//   108238  Renewal                (talent instant 30% self-heal)
//     1126  Mark of the Wild       (group buff)
//    93402  Sunfire                (filler DoT)
//     8921  Moonfire               (filler DoT)
//     5176  Wrath                  (filler nuke — Resto's baseline Wrath
//                                   id; Balance learns 190984 instead)
//   102693  Grove Guardians        (talent — 3-charge healing treants)
//    33891  Incarnation: Tree of Life (talent — 30s burst form)
//
// Skipped spells (and why)
// ---------------------------
//   * 212040  Revitalize            — passive mana regen talent, not a
//     cast. Implicit benefit on every spell.
//   * 468146  Reactive Resin        — buff aura (proc / item / encounter
//     interaction in modern content). Not a player ability we cast on a
//     priority — applied externally or via talent procs. No predicate.
//   * 197490  Feral Affinity        — passive talent that grants Rake/
//     Shred/Rip to non-Feral specs. Resto has no use for the cat-form
//     spells from a healing rotation; baseline druid handles them if the
//     bot ever drops into cat. No spec-rotation predicate.
//   * 270100  Bear Form variant     — Guardian-specific aura, not a
//     Resto ability.

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

// ---- Spell IDs (WoW 12.0, validated against SpellName.csv) ----
constexpr uint32 REJUVENATION      = 774;
constexpr uint32 REGROWTH          = 8936;
constexpr uint32 LIFEBLOOM         = 33763;
constexpr uint32 SWIFTMEND         = 18562;
constexpr uint32 WILD_GROWTH       = 48438;
constexpr uint32 TRANQUILITY       = 740;
constexpr uint32 EFFLORESCENCE     = 145205;
constexpr uint32 CENARION_WARD     = 102351;       // talent — tank pre-shield
constexpr uint32 FLOURISH          = 197721;       // talent — extends all HoTs
constexpr uint32 CONVOKE_SPIRITS   = 391528;       // burst CD
constexpr uint32 ADAPTIVE_SWARM    = 391888;       // talent — buff/debuff hybrid
constexpr uint32 NATURES_CURE      = 88423;        // Resto: Magic+Curse+Poison
constexpr uint32 REMOVE_CORRUPTION = 440015;       // Curse+Poison fallback
constexpr uint32 SOOTHE            = 2908;
constexpr uint32 INNERVATE         = 29166;
constexpr uint32 REBIRTH           = 20484;
constexpr uint32 BARKSKIN          = 22812;
constexpr uint32 IRONBARK          = 102342;
constexpr uint32 RENEWAL           = 108238;      // talent — instant 30% max-HP self-heal
constexpr uint32 MARK_OF_THE_WILD  = 1126;
constexpr uint32 SUNFIRE           = 93402;
constexpr uint32 MOONFIRE          = 8921;
constexpr uint32 WRATH             = 5176;          // Resto's baseline Wrath learn
constexpr uint32 GROVE_GUARDIANS   = 102693;        // talent — 3-charge healing treants
constexpr uint32 INCARN_TREE       = 33891;         // talent — Incarnation: Tree of Life

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

// Renewal: instant 30% max-HP self-heal, 90s CD. Resto's true panic
// instant — Regrowth is a hard-cast that breaks on damage. Fires at
// <=35%. Alternation: skip if Barkskin is ready (Barkskin first = 12s of
// 20% DR stacks with Renewal). Skip if Renewal is already up.
bool ShouldRenewal(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(RENEWAL)) return false;
    if (!ctx.bot.is_ready(RENEWAL)) return false;
    if (ctx.bot.hp_pct() > 35) return false;
    if (ctx.bot.knows_spell(BARKSKIN) && ctx.bot.is_ready(BARKSKIN)
        && ctx.bot.hp_pct() > 20) return false;
    return true;
}
void DoRenewal(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(RENEWAL); }

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

bool ShouldFlourish(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(FLOURISH)) return false;
    if (!ctx.bot.is_ready(FLOURISH)) return false;
    // Best when a lot of HoTs are out — gate on 3+ wounded so we know our
    // blanket is broad. Also benefits from raid burst windows.
    return WoundedFriendCount(ctx, 70) >= 3;
}
void DoFlourish(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FLOURISH); }

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

// Grove Guardians — 3-charge healing-treant summon. Off-GCD; can fire
// during channels. Spend on heavy raid damage windows or when the lowest
// friendly drops below 75%.
bool ShouldGroveGuardians(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(GROVE_GUARDIANS)) return false;
    if (!ctx.bot.is_ready(GROVE_GUARDIANS)) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 75 || WoundedFriendCount(ctx, 80) >= 2;
}
void DoGroveGuardians(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(GROVE_GUARDIANS, LowestFriendOrSelf(ctx).guid);
}

// ---- Pre-shield / pre-emptive ----
bool ShouldCenarionWard(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(CENARION_WARD)) return false;
    if (!ctx.bot.is_ready(CENARION_WARD)) return false;
    GroupMemberSummary const* tank = ctx.group.tank();
    if (!tank || !tank->online || tank->hp <= 0) return false;
    if (ctx.bot.has_aura(CENARION_WARD, tank->guid)) return false;
    return true;
}
void DoCenarionWard(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* tank = ctx.group.tank())
        e.cast(CENARION_WARD, tank->guid);
}

bool ShouldAdaptiveSwarm(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(ADAPTIVE_SWARM)) return false;
    if (!ctx.bot.is_ready(ADAPTIVE_SWARM)) return false;
    HealTarget t = LowestFriendOrSelf(ctx);
    return t.hp_pct <= 80;
}
void DoAdaptiveSwarm(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ADAPTIVE_SWARM, LowestFriendOrSelf(ctx).guid);
}

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

bool ShouldEfflorescence(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(EFFLORESCENCE)) return false;
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
bool ShouldSwiftmend(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SWIFTMEND)) return false;
    if (!ctx.bot.is_ready(SWIFTMEND)) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 50;
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
    AuraEntry const* a = ctx.bot.find_aura(SUNFIRE, ctx.bot.victim());
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
    AuraEntry const* a = ctx.bot.find_aura(MOONFIRE, ctx.bot.victim());
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
//   1.  Cancel heal swap      — drop slow cast when a more urgent target appears.
//   2.  Rebirth               — battle rez has highest party impact.
//   3.  Mark of the Wild      — OOC group buff maintenance.
//   4.  Barkskin              — off-GCD survival.
//   5.  Renewal               — instant 30% self-heal panic.
//   6.  Ironbark              — tank DR + heal amp at <=50%.
//   7.  Nature's Cure         — full dispel (Magic + Curse + Poison).
//   8.  Remove Corruption     — Curse+Poison fallback when NC unknown.
//   9.  Soothe                — enrage dispel on target.
//   10. Innervate             — own / ally caster mana.
//   11. Tranquility           — raid CD at 3+ heavily wounded.
//   12. Flourish              — extend all HoTs at 3+ wounded.
//   13. Convoke               — burst heal/damage on 2+ wounded.
//   14. Incarnation: Tree of Life — 30s heal-burst window.
//   15. Grove Guardians       — off-GCD treant summons.
//   16. Cenarion Ward         — tank pre-shield refresh.
//   17. Wild Growth           — group AoE HoT (3+ at <=80%).
//   18. Efflorescence         — ground AoE HoT placement.
//   19. Swiftmend             — instant spike heal (<=50%).
//   20. Regrowth              — direct heal + HoT (<=65%).
//   21. Adaptive Swarm        — talent buff/debuff (<=80%).
//   22. Lifebloom             — tank HoT maintenance.
//   23. Rejuvenation          — lowest-friend HoT refresh.
//   24. Sunfire / Moonfire / Wrath — offensive filler ONLY when group is topped.
//   25. Idle                  — alive fallthrough.
ApRule const kRules[] = {
    { ShouldCancelHealForSwap, DoCancelHealForSwap, "Cancel heal — swap to lower target" },
    { ShouldRebirth,        DoRebirth,        "Rebirth (battle rez)"         },
    { ShouldMarkOfTheWild,  DoMarkOfTheWild,  "Mark of the Wild"             },
    { ShouldBarkskin,       DoBarkskin,       "Barkskin (<=50% self)"        },
    { ShouldRenewal,        DoRenewal,        "Renewal (<=35% emergency)"    },
    { ShouldIronbark,       DoIronbark,       "Ironbark (tank <=50%)"        },
    { ShouldNaturesCure,    DoNaturesCure,    "Nature's Cure (dispel)"       },
    { ShouldRemoveCorruption, DoRemoveCorruption, "Remove Corruption (fb)"   },
    { ShouldSoothe,         DoSoothe,         "Soothe (enrage)"              },
    { ShouldInnervate,      DoInnervate,      "Innervate"                    },
    { ShouldTranquility,    DoTranquility,    "Tranquility (3+ at <=50%)"    },
    { ShouldFlourish,       DoFlourish,       "Flourish (extend HoTs)"       },
    { ShouldConvoke,        DoConvoke,        "Convoke the Spirits"          },
    { ShouldIncarnTree,     DoIncarnTree,     "Incarnation: Tree of Life"    },
    { ShouldGroveGuardians, DoGroveGuardians, "Grove Guardians (off-GCD)"    },
    { ShouldCenarionWard,   DoCenarionWard,   "Cenarion Ward (tank)"         },
    { ShouldWildGrowth,     DoWildGrowth,     "Wild Growth (3+ at <=80%)"    },
    { ShouldEfflorescence,  DoEfflorescence,  "Efflorescence (maintain)"     },
    { ShouldSwiftmend,      DoSwiftmend,      "Swiftmend (<=50%)"            },
    { ShouldRegrowth,       DoRegrowth,       "Regrowth (<=65%)"             },
    { ShouldAdaptiveSwarm,  DoAdaptiveSwarm,  "Adaptive Swarm (<=80%)"       },
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
