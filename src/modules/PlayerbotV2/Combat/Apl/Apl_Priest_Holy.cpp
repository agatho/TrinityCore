// Holy Priest - WoW 12.1.0.69587 enterprise rotation. Direct-cast healer
// built around Flash Heal / Prayer of Healing feeding the Holy Word
// cooldowns (Serenity / Sanctify / Chastise), Prayer of Mending on the
// tank, and Halo (Archon hero tree) for raid healing. Mastery (Echo of
// Light 77489) proc-HoTs on direct heals - no rule needed. Heal (2060) and
// Renew (139) are gone from the 12.1 kit: Flash Heal is both the spike heal
// and the efficient filler (Improved Flash Heal / Surge of Light [R][M]).
// Decision tree:
//
//   1) OOC rez:                Mass Resurrection / Resurrection
//   2) Interrupt / CC / threat:Holy Word: Chastise ([M]), Psychic Scream
//                              ([M]), Leap of Faith (peel), Fade
//   3) Personal survival:      Desperate Prayer, PW: Shield self (only
//                              until Prayer of Mending overrides it at L11)
//   4) Dispel:                 Purify (Magic; + Disease with Improved
//                              Purify), Mass Dispel
//   5) Hard panic:             Guardian Spirit (ally <=25%), Divine Hymn
//                              (3+ <=50%), Holy Word: Serenity (<=60%),
//                              Apotheosis (Holy Word reset)
//   6) Burst CD:               Power Infusion
//   7) HoT maintenance:        Prayer of Mending on tank
//   8) Spike heal:             Flash Heal (<=50%)
//   9) AoE heal:               Holy Word: Sanctify (or Serenity with the
//                              Ultimate Serenity passive), Prayer of
//                              Healing, Holy Nova, Halo
//  10) Filler:                 Flash Heal top-off (<=85%, mana-gated)
//  11) Offensive filler:       Holy Fire / SW:P / Smite when group topped
//                              (also feeds Holy Word CDR)
//
// Server-side overrides (Unit::GetCastSpellInfo resolves OVERRIDE_ACTIONBAR
// auras): Prayer of Mending (33076) overrides PW: Shield (17), Holy Fire
// (14914 [R][M]) overrides SW: Pain (589), Ultimate Serenity (1246517
// [R][M] passive) removes HW: Sanctify (34861), Purify overrides Purify
// Disease. Each base rule is knows_spell-gated on the override so both the
// talented and the untalented bot cast the right thing.
//
// ---- Validated spell IDs (WoW 12.1.0.69587 SpellName.csv / kit) ----
//   17      Power Word: Shield       (L4 baseline - self absorb until PoM)
//   527     Purify                   (spec L10 - Magic dispel)
//   585     Smite                    (L1 baseline - offensive filler / CDR)
//   586     Fade                     (class talent [R][M])
//   589     Shadow Word: Pain        (L2 baseline - DoT filler until Holy Fire)
//   596     Prayer of Healing        (spec talent [R][M])
//   2006    Resurrection             (L10 baseline)
//   2050    Holy Word: Serenity      (spec talent [R][M])
//   2061    Flash Heal               (L3 baseline - spike heal + filler)
//   8122    Psychic Scream           (class talent [M])
//   10060   Power Infusion           (class talent [R][M])
//   14914   Holy Fire                (class talent [R][M] - overrides SW:P)
//   19236   Desperate Prayer         (class talent [R][M])
//   32375   Mass Dispel              (class talent [R][M])
//   33076   Prayer of Mending        (spec L11 - overrides PW: Shield)
//   34861   Holy Word: Sanctify      (spec talent [R][M]; gone with Ult. Serenity)
//   47788   Guardian Spirit          (spec talent [R][M])
//   64843   Divine Hymn              (spec talent [R][M])
//   73325   Leap of Faith            (class talent [R][M])
//   88625   Holy Word: Chastise      (spec talent [M])
//   120517  Halo                     (Archon hero talent [R][M] - Holy ring)
//   132157  Holy Nova                (class talent [R][M])
//   200183  Apotheosis               (spec talent [R][M])
//   212036  Mass Resurrection        (spec L37 OOC)
//   390632  Improved Purify          (PASSIVE gate - Purify removes Disease)
//   1246517 Ultimate Serenity        (PASSIVE gate - Serenity AoE, no Sanctify)
//
// ---- Skipped spells (and why) ----
//   - Heal (2060), Renew (139), Circle of Healing (204883), Symbol of Hope
//     (64901), Holy Word: Salvation (265202), Binding Heal (32546), Power
//     Word: Life (373481), Premonition of Piety (438733), Divine Star
//     (110744): not learnable by Holy in 12.1.
//   - Vampiric Embrace (15286): Shadow spec spell now.
//   - Empyreal Blaze (372616 [M]): a PASSIVE in 12.1 ("Holy Word: Chastise
//     makes your next Holy Fires instant") - not castable.
//   - Purify Disease (440006): learned with Purify (527), which overrides
//     it; disease removal exists only via Improved Purify (390632).
//   - Weakened Soul (6788): nothing applies it in 12.1 - is_ready() gates
//     the PW: Shield re-cast (7.5s category cooldown).
//   - Echo of Light (77489), Spirit of Redemption (215769 PvP talent),
//     Light of T'uure (208065), Light's Wrath (207946), Void Torrent
//     (205065): passives / legacy Artifact rows with no learn level.
//   - Angelic Feather (121536), Dominate Mind (205364), Mind Control (605),
//     Shackle Horror (9484), Dispel Magic (528 [M]): positioning / niche.
//   - Single-Button Assistant (1229376): the APL is the assistant.

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

// ---- Spell IDs (WoW 12.1.0.69587, validated) ----
constexpr uint32 FLASH_HEAL              = 2061;         // spike heal AND efficient filler (Heal / Renew are gone)
constexpr uint32 PRAYER_OF_HEALING       = 596;
constexpr uint32 PRAYER_OF_MENDING       = 33076;        // spec L11 - overrides PW: Shield
constexpr uint32 HOLY_WORD_SERENITY      = 2050;
constexpr uint32 HOLY_WORD_SANCTIFY      = 34861;        // removed by the Ultimate Serenity passive
constexpr uint32 ULTIMATE_SERENITY       = 1246517;      // PASSIVE gate [R][M] - Serenity heals nearby allies, no Sanctify
constexpr uint32 HOLY_WORD_CHASTISE      = 88625;
constexpr uint32 DIVINE_HYMN             = 64843;
constexpr uint32 GUARDIAN_SPIRIT         = 47788;
constexpr uint32 PURIFY                  = 527;          // Magic dispel (+ Disease with Improved Purify)
constexpr uint32 IMPROVED_PURIFY         = 390632;       // PASSIVE gate - Purify additionally removes Disease
constexpr uint32 MASS_DISPEL             = 32375;
constexpr uint32 HOLY_NOVA               = 132157;       // class talent [R][M] - AoE heal + dmg
constexpr uint32 APOTHEOSIS              = 200183;       // talent - Holy Word reset + buff
constexpr uint32 POWER_INFUSION          = 10060;
constexpr uint32 DESPERATE_PRAYER        = 19236;
constexpr uint32 POWER_WORD_SHIELD       = 17;           // self-absorb until Prayer of Mending overrides it
constexpr uint32 FADE                    = 586;
constexpr uint32 LEAP_OF_FAITH           = 73325;
constexpr uint32 PSYCHIC_SCREAM          = 8122;
constexpr uint32 RESURRECTION            = 2006;
constexpr uint32 MASS_RESURRECTION       = 212036;

// Offensive filler
constexpr uint32 SMITE                   = 585;
constexpr uint32 HOLY_FIRE               = 14914;        // class talent [R][M] - overrides SW: Pain
constexpr uint32 SHADOW_WORD_PAIN        = 589;
constexpr uint32 HALO                    = 120517;       // Archon hero talent [R][M] - Holy variant

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

// Mana-floor gate. <=15% mana = only emergencies (LowestFriendOrSelf
// <=35% HP) - let Prayer of Mending bounces / Echo of Light carry the
// rest until the bot regenerates. Without this, Holy Priest spammed Flash
// Heal into OOM and had zero mana when the tank actually spiked (every
// spec other than Holy already had this gate). Apply at the top of normal-
// heal predicates ONLY - Guardian Spirit, Divine Hymn, HW: Serenity are
// the emergency floor and stay unmodified.
bool InManaFloor(ApPredicateContext const& ctx)
{
    return ctx.bot.max_power(0) > 0 && ctx.bot.power_pct(0) <= 15;
}

bool GroupTopped(ApPredicateContext const& ctx)
{
    return WoundedFriendCount(ctx, 92) == 0;
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

bool HasLiveTargetInline(ApPredicateContext const& ctx)
{
    return !ctx.bot.victim().IsEmpty();
}

GroupMemberSummary const* OffensivePIBeneficiary(ApPredicateContext const& ctx)
{
    auto const* members = ctx.group.members();
    if (!members) return nullptr;
    for (auto const& m : *members)
    {
        if (!m.online || m.hp <= 0) continue;
        if (m.guid == ctx.bot.raw().guid) continue;
        if (m.role != Role::Dps) continue;
        return &m;
    }
    return nullptr;
}

// Cast-swap shim — Holy Priest's slow single-target heals worth
// cancelling when a different member spikes critical. See
// ApHealHelpers.h for the predicate body.
bool ShouldCancelHealForSwap(ApPredicateContext const& ctx)
{
    return ShouldCancelHealForSwapImpl(ctx,
        { FLASH_HEAL, PRAYER_OF_HEALING });
}

// ---- OOC rez ----
bool ShouldResurrection(ApPredicateContext const& ctx)
{
    if (ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(RESURRECTION)) return false;
    if (!ctx.bot.is_ready(RESURRECTION)) return false;
    return ctx.group.dead_member(ctx.bot.map_id()) != nullptr;
}
void DoResurrection(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* m = ctx.group.dead_member(ctx.bot.map_id()))
        e.cast(RESURRECTION, m->guid);
}

bool ShouldMassResurrection(ApPredicateContext const& ctx)
{
    if (ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(MASS_RESURRECTION)) return false;
    if (!ctx.bot.is_ready(MASS_RESURRECTION)) return false;
    int dead = 0;
    auto const* members = ctx.group.members();
    if (!members) return false;
    for (auto const& m : *members)
        if (m.online && m.hp <= 0) ++dead;
    return dead >= 2;
}
void DoMassResurrection(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(MASS_RESURRECTION); }

// ---- Personal survival ----
bool ShouldDesperatePrayer(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(DESPERATE_PRAYER)) return false;
    if (!ctx.bot.is_ready(DESPERATE_PRAYER)) return false;
    return ctx.bot.hp_pct() <= 40;
}
void DoDesperatePrayer(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DESPERATE_PRAYER); }

bool ShouldFade(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(FADE)) return false;
    if (!ctx.bot.is_ready(FADE)) return false;
    if (ctx.bot.hp_pct() > 60) return false;
    return ctx.bot.attackers_count() >= 1;
}
void DoFade(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FADE); }

// ---- Dispel ----
// Purify (527) removes Magic; with the Improved Purify passive (390632) it
// also removes Disease. Purify Disease (440006) is learned alongside but
// always overridden by 527, so every friendly dispel goes through PURIFY.
bool ShouldPurify(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(PURIFY) || !ctx.bot.is_ready(PURIFY)) return false;
    const bool can_disease = ctx.bot.knows_spell(IMPROVED_PURIFY);
    if (ctx.group.dispel_candidate(DispelType::Magic)) return true;
    if (can_disease && ctx.group.dispel_candidate(DispelType::Disease)) return true;
    if (ctx.bot.self_dispellable(DispelType::Magic)) return true;
    if (can_disease && ctx.bot.self_dispellable(DispelType::Disease)) return true;
    return false;
}
void DoPurify(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool can_disease = ctx.bot.knows_spell(IMPROVED_PURIFY);
    if (auto const* mg = ctx.group.dispel_candidate(DispelType::Magic)) { e.cast(PURIFY, mg->guid); return; }
    if (can_disease)
        if (auto const* ds = ctx.group.dispel_candidate(DispelType::Disease)) { e.cast(PURIFY, ds->guid); return; }
    if (ctx.bot.self_dispellable(DispelType::Magic)) { e.cast(PURIFY, ctx.bot.raw().guid); return; }
    if (can_disease && ctx.bot.self_dispellable(DispelType::Disease)) { e.cast(PURIFY, ctx.bot.raw().guid); return; }
}

// CB-P1d: Mass Dispel is a GROUND-targeted AoE that strips Magic from allies
// in the area. The trigger must reflect that — fire when MULTIPLE grouped
// allies share a dispellable Magic debuff, and aim at the centroid of those
// allies (not at the bot's own guid). Counts allies on the bot's own map that
// are online + alive and carry at least one harmful Magic aura, accumulating
// their positions so DoMassDispel can target the affected cluster.
int MagicAfflictedAllyCount(ApPredicateContext const& ctx,
                            float* out_cx = nullptr, float* out_cy = nullptr, float* out_cz = nullptr)
{
    auto const* members = ctx.group.members();
    if (!members) return 0;
    uint32 const my_map = ctx.bot.map_id();
    int n = 0;
    double sx = 0.0, sy = 0.0, sz = 0.0;
    for (auto const& m : *members)
    {
        if (!m.online || m.hp <= 0 || m.map_id != my_map) continue;
        bool magic = false;
        for (auto const& d : m.debuffs)
            if (d.is_harmful && d.dispel_type == DispelType::Magic) { magic = true; break; }
        if (!magic) continue;
        ++n;
        sx += m.x; sy += m.y; sz += m.z;
    }
    if (n > 0 && out_cx) { *out_cx = static_cast<float>(sx / n); *out_cy = static_cast<float>(sy / n); *out_cz = static_cast<float>(sz / n); }
    return n;
}

bool ShouldMassDispel(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(MASS_DISPEL)) return false;
    if (!ctx.bot.is_ready(MASS_DISPEL)) return false;
    // Fire when at least 3 allies share a dispellable Magic effect — the
    // scenario Mass Dispel actually exists for (raid-wide magic debuff, mass
    // bubble dispel). Decoupled from SelfNeedsDispel (CB-P1d).
    return MagicAfflictedAllyCount(ctx) >= 3;
}
void DoMassDispel(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    float cx = 0.f, cy = 0.f, cz = 0.f;
    if (MagicAfflictedAllyCount(ctx, &cx, &cy, &cz) > 0)
        e.cast_at(MASS_DISPEL, cx, cy, cz);
}

// ---- Interrupt / CC ----
bool ShouldHolyWordChastise(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HOLY_WORD_CHASTISE)) return false;
    if (!ctx.bot.is_ready(HOLY_WORD_CHASTISE)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    return ctx.bot.kick_target(pvp, 30.0f) != nullptr;
}
void DoHolyWordChastise(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 30.0f))
        e.cast(HOLY_WORD_CHASTISE, c->guid);
}

bool ShouldPsychicScream(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(PSYCHIC_SCREAM)) return false;
    if (!ctx.bot.is_ready(PSYCHIC_SCREAM)) return false;
    return ctx.bot.attackers_count() >= 3 && ctx.bot.hp_pct() <= 50;
}
void DoPsychicScream(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(PSYCHIC_SCREAM); }

bool ShouldLeapOfFaith(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(LEAP_OF_FAITH)) return false;
    if (!ctx.bot.is_ready(LEAP_OF_FAITH)) return false;
    GroupMemberSummary const* tank = ctx.group.tank();
    if (!tank || !tank->online || tank->hp <= 0) return false;
    if (tank->guid == ctx.bot.raw().guid) return false;
    if (tank->max_hp <= 0) return false;
    return (tank->hp * 100) / tank->max_hp <= 30;
}
void DoLeapOfFaith(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* tank = ctx.group.tank())
        e.cast(LEAP_OF_FAITH, tank->guid);
}

// ---- Hard panic ----
bool ShouldGuardianSpirit(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(GUARDIAN_SPIRIT)) return false;
    if (!ctx.bot.is_ready(GUARDIAN_SPIRIT)) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 25;
}
void DoGuardianSpirit(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(GUARDIAN_SPIRIT, LowestFriendOrSelf(ctx).guid);
}

bool ShouldDivineHymn(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(DIVINE_HYMN)) return false;
    if (!ctx.bot.is_ready(DIVINE_HYMN)) return false;
    return WoundedFriendCount(ctx, 50) >= 3;
}
void DoDivineHymn(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DIVINE_HYMN); }

// ---- Burst CDs ----
bool ShouldApotheosis(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(APOTHEOSIS)) return false;
    if (!ctx.bot.is_ready(APOTHEOSIS)) return false;
    return WoundedFriendCount(ctx, 75) >= 3;
}
void DoApotheosis(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(APOTHEOSIS); }

bool ShouldPowerInfusion(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(POWER_INFUSION)) return false;
    if (!ctx.bot.is_ready(POWER_INFUSION)) return false;
    if (BossLikeTargetEngaged(ctx)) return true;
    return GroupTopped(ctx);
}
void DoPowerInfusion(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* dps = OffensivePIBeneficiary(ctx))
        e.cast(POWER_INFUSION, dps->guid);
    else
        e.cast(POWER_INFUSION, ctx.bot.raw().guid);
}

// ---- AoE heal ----
// Holy Word: Sanctify is removed by the Ultimate Serenity passive ([R][M]);
// the talented bot uses the Serenity AoE branch below instead.
bool ShouldHolyWordSanctify(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HOLY_WORD_SANCTIFY)) return false;
    if (ctx.bot.knows_spell(ULTIMATE_SERENITY)) return false;
    if (!ctx.bot.is_ready(HOLY_WORD_SANCTIFY)) return false;
    return WoundedFriendCount(ctx, 70) >= 3;
}
void DoHolyWordSanctify(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HOLY_WORD_SANCTIFY, LowestFriendOrSelf(ctx).guid);
}

// Ultimate Serenity branch: Serenity also heals nearby injured allies, so it
// takes over the 3+ wounded raid-spike slot.
bool ShouldHolyWordSerenityAoe(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(ULTIMATE_SERENITY)) return false;
    if (!ctx.bot.knows_spell(HOLY_WORD_SERENITY)) return false;
    if (!ctx.bot.is_ready(HOLY_WORD_SERENITY)) return false;
    return WoundedFriendCount(ctx, 70) >= 3;
}
void DoHolyWordSerenityAoe(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HOLY_WORD_SERENITY, LowestFriendOrSelf(ctx).guid);
}

bool ShouldPrayerOfHealing(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(PRAYER_OF_HEALING)) return false;
    if (InManaFloor(ctx) && LowestFriendOrSelf(ctx).hp_pct > 35) return false;
    return WoundedFriendCount(ctx, 80) >= 3;
}
void DoPrayerOfHealing(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(PRAYER_OF_HEALING, LowestFriendOrSelf(ctx).guid);
}

bool ShouldHolyNova(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HOLY_NOVA)) return false;
    if (ctx.bot.attackers_count() < 2) return false;
    return WoundedFriendCount(ctx, 92) >= 2;
}
void DoHolyNova(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HOLY_NOVA); }

bool ShouldHaloHeal(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(HALO)) return false;
    if (!ctx.bot.is_ready(HALO)) return false;
    return WoundedFriendCount(ctx, 85) >= 2;
}
void DoHaloHeal(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HALO); }

// ---- Spike heal ----
bool ShouldHolyWordSerenity(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HOLY_WORD_SERENITY)) return false;
    if (!ctx.bot.is_ready(HOLY_WORD_SERENITY)) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 60;
}
void DoHolyWordSerenity(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HOLY_WORD_SERENITY, LowestFriendOrSelf(ctx).guid);
}

bool ShouldFlashHeal(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(FLASH_HEAL)) return false;
    if (InManaFloor(ctx) && LowestFriendOrSelf(ctx).hp_pct > 35) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 50;
}
void DoFlashHeal(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FLASH_HEAL, LowestFriendOrSelf(ctx).guid);
}

// ---- HoT maintenance ----
bool ShouldPrayerOfMending(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(PRAYER_OF_MENDING)) return false;
    if (!ctx.bot.is_ready(PRAYER_OF_MENDING)) return false;
    GroupMemberSummary const* tank = ctx.group.tank();
    ObjectGuid target = tank && tank->online ? tank->guid : LowestFriendOrSelf(ctx).guid;
    AuraEntry const* a = ctx.bot.find_aura(PRAYER_OF_MENDING, target);
    return !a || a->remaining.count() <= 5000;
}
void DoPrayerOfMending(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    GroupMemberSummary const* tank = ctx.group.tank();
    ObjectGuid target = tank && tank->online ? tank->guid : LowestFriendOrSelf(ctx).guid;
    e.cast(PRAYER_OF_MENDING, target);
}

// ---- Filler ----
// Flash Heal top-off - Heal (2060) and Renew (139) are gone in 12.1, so
// Flash Heal doubles as the efficient filler (Improved Flash Heal [R][M])
// and drives the Serenity cooldown reduction. Mana-floor gated.
bool ShouldFlashHealFiller(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(FLASH_HEAL)) return false;
    if (LowestFriendOrSelf(ctx).hp_pct > 85) return false;
    if (InManaFloor(ctx) && LowestFriendOrSelf(ctx).hp_pct > 35) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(FLASH_HEAL)) return false;
    return true;
}
void DoFlashHealFiller(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FLASH_HEAL, LowestFriendOrSelf(ctx).guid);
}

// ---- Offensive filler (group topped — drives Holy Word CDR) ----
bool ShouldHolyFireFiller(ApPredicateContext const& ctx)
{
    if (!HasLiveTargetInline(ctx)) return false;
    if (!GroupTopped(ctx)) return false;
    if (!ctx.bot.knows_spell(HOLY_FIRE)) return false;
    return ctx.bot.is_ready(HOLY_FIRE);
}
void DoHolyFireFiller(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HOLY_FIRE, ctx.bot.victim());
}

// SW: Pain filler - only for the untalented bot: the Holy Fire talent
// ([R][M]) overrides SW: Pain, so casting 589 would resolve to Holy Fire
// and fail against its 10s cooldown.
bool ShouldShadowWordPainFiller(ApPredicateContext const& ctx)
{
    if (!HasLiveTargetInline(ctx)) return false;
    if (!GroupTopped(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOW_WORD_PAIN)) return false;
    if (ctx.bot.knows_spell(HOLY_FIRE)) return false;
    AuraEntry const* a = ctx.bot.find_aura(SHADOW_WORD_PAIN, ctx.bot.victim());
    return !a || a->remaining.count() <= 3000;
}
void DoShadowWordPainFiller(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHADOW_WORD_PAIN, ctx.bot.victim());
}

bool ShouldSmiteFiller(ApPredicateContext const& ctx)
{
    if (!HasLiveTargetInline(ctx)) return false;
    if (!GroupTopped(ctx)) return false;
    if (!ctx.bot.knows_spell(SMITE)) return false;
    return !(ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(SMITE));
}
void DoSmiteFiller(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SMITE, ctx.bot.victim());
}

bool AlwaysAlive(ApPredicateContext const& ctx) { return ctx.bot.is_alive(); }
void DoNothing(ApPredicateContext const&, BotIntentEmitter&) {}

// Self Power Word: Shield - Holy's only non-talent damage-reduction CD on
// self, and only until L11: Prayer of Mending (spec spell) OVERRIDES PW:
// Shield, so once PoM is known casting 17 would resolve to PoM. PW: Shield
// carries a 7.5s category cooldown in 12.1 (Weakened Soul is gone), which
// is_ready() covers. Fires at <=55% with active fight + a nearby enemy.
bool ShouldPowerWordShieldSelf(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(POWER_WORD_SHIELD)) return false;
    if (ctx.bot.knows_spell(PRAYER_OF_MENDING)) return false;
    if (!ctx.bot.is_ready(POWER_WORD_SHIELD)) return false;
    if (ctx.bot.hp_pct() > 55) return false;
    if (ctx.bot.enemies_within(40.0f) == 0) return false;
    if (ctx.bot.find_aura(POWER_WORD_SHIELD, ctx.bot.raw().guid)) return false;
    return true;
}
void DoPowerWordShieldSelf(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(POWER_WORD_SHIELD, ctx.bot.raw().guid);
}

// ---- Rule table (priority order) ----
// Order follows the Holy decision tree: cast-swap first (so we can change
// targets mid-cast), OOC rez, interrupt / CC / threat, personal survival,
// dispels, hard panic CDs (Guardian Spirit ally -> Divine Hymn raid -> HW:
// Serenity instant -> Apotheosis reset), Power Infusion, Prayer of Mending
// bouncing HoT, Flash Heal spike, AoE heals (HW: Sanctify or the Ultimate
// Serenity branch -> Prayer of Healing -> Holy Nova -> Halo), the Flash
// Heal top-off filler, and finally the offensive filler that drives Holy
// Word CDR (Holy Fire / SW: Pain / Smite).
ApRule const kRules[] = {
    // Cast-swap MUST be first in the priority list - if we don't
    // cancel the in-flight cast immediately, lower rules can't change
    // target because is_casting blocks them.
    { ShouldCancelHealForSwap,   DoCancelHealForSwap,   "Cancel heal — swap to lower target" },
    { ShouldMassResurrection,    DoMassResurrection,    "Mass Resurrection (OOC)"        },
    { ShouldResurrection,        DoResurrection,        "Resurrection (OOC)"             },
    // ---- Interrupt / CC / threat ----
    { ShouldHolyWordChastise,    DoHolyWordChastise,    "HW: Chastise (interrupt)"       },
    { ShouldPsychicScream,       DoPsychicScream,       "Psychic Scream (panic AoE)"     },
    { ShouldLeapOfFaith,         DoLeapOfFaith,         "Leap of Faith (peel tank)"      },
    { ShouldFade,                DoFade,                "Fade (threat)"                  },
    // ---- Personal survival ----
    { ShouldDesperatePrayer,     DoDesperatePrayer,     "Desperate Prayer (<=40%)"       },
    { ShouldPowerWordShieldSelf, DoPowerWordShieldSelf, "PW: Shield self (<=55%)"        },
    // ---- Dispel ----
    { ShouldPurify,              DoPurify,              "Purify (Magic / Disease)"       },
    { ShouldMassDispel,          DoMassDispel,          "Mass Dispel (raid)"             },
    // ---- Hard panic heals (ally life-savers, ordered by raw save power) ----
    { ShouldGuardianSpirit,      DoGuardianSpirit,      "Guardian Spirit (ally panic)"   },
    { ShouldDivineHymn,          DoDivineHymn,          "Divine Hymn (raid panic)"       },
    { ShouldHolyWordSerenity,    DoHolyWordSerenity,    "HW: Serenity (instant <=60%)"   },
    { ShouldApotheosis,          DoApotheosis,          "Apotheosis (Holy Word reset)"   },
    // ---- Burst CD ----
    { ShouldPowerInfusion,       DoPowerInfusion,       "Power Infusion"                 },
    // ---- HoT maintenance ----
    { ShouldPrayerOfMending,     DoPrayerOfMending,     "Prayer of Mending (tank)"       },
    // ---- Spike heal ----
    { ShouldFlashHeal,           DoFlashHeal,           "Flash Heal (<=50%)"             },
    // ---- AoE heals ----
    { ShouldHolyWordSanctify,    DoHolyWordSanctify,    "HW: Sanctify (raid spike)"      },
    { ShouldHolyWordSerenityAoe, DoHolyWordSerenityAoe, "HW: Serenity (Ult. AoE)"        },
    { ShouldPrayerOfHealing,     DoPrayerOfHealing,     "Prayer of Healing (3+ at 80%)"  },
    { ShouldHolyNova,            DoHolyNova,            "Holy Nova (cleave heal+dmg)"    },
    { ShouldHaloHeal,            DoHaloHeal,            "Halo (raid heal)"               },
    // ---- Efficient filler ----
    { ShouldFlashHealFiller,     DoFlashHealFiller,     "Flash Heal (top-off <=85%)"     },
    // ---- Offensive filler (group topped - drives Holy Word CDR) ----
    { ShouldHolyFireFiller,      DoHolyFireFiller,      "Holy Fire (filler / CDR)"       },
    { ShouldShadowWordPainFiller,DoShadowWordPainFiller,"SW: Pain (filler, no HF)"       },
    { ShouldSmiteFiller,         DoSmiteFiller,         "Smite (filler / CDR)"           },
    { AlwaysAlive,               DoNothing,             "Idle"                           },
};

} // anonymous

void RegisterApl_Priest_Holy()
{
    constexpr uint32 SPEC_PRIEST_HOLY = 257;
    RegisterRotation(CLASS_PRIEST, SPEC_PRIEST_HOLY, ApRotation{kRules});
}

} // namespace Playerbot::Combat
