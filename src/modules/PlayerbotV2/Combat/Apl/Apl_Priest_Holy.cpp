// Holy Priest - WoW 12.0 enterprise rotation. Direct-cast healer with Holy
// Word triplet (Serenity/Sanctify/Chastise) sped up by Heal/PoH/Smite casts.
// Mastery (Echo of Light) proc-HoTs on direct heals. Spirit of Redemption
// (20711) is a passive ghost-form on death — no rule needed.
// Decision tree:
//
//   1) OOC rez:                Mass Resurrection / Resurrection
//   2) Personal survival:      Desperate Prayer, Fade, PW: Shield self
//   3) Dispel:                 Purify (Magic), Purify Disease (440006),
//                              Mass Dispel
//   4) Interrupt / CC:         Holy Word: Chastise, Psychic Scream,
//                              Leap of Faith (peel)
//   5) Hard panic:             Guardian Spirit (ally <=25%), Holy Word:
//                              Salvation (4+ wiping), Divine Hymn (3+
//                              <=50%), Power Word: Life (<=35%),
//                              Vampiric Embrace
//   6) Mana / burst CDs:       Symbol of Hope, Apotheosis, Power Infusion,
//                              Premonition of Piety (talent), Empyreal
//                              Blaze (talent)
//   7) AoE heal:               Holy Word: Sanctify, Circle of Healing,
//                              Prayer of Healing, Holy Nova, Halo, Divine Star
//   8) Spike heal:             Holy Word: Serenity, Flash Heal, Binding Heal
//   9) HoT maintenance:        Renew, Prayer of Mending on tank
//  10) Filler:                 Heal (efficient mana)
//  11) Offensive filler:       Smite / Holy Fire / SW:P when group topped
//                              (also speeds up Holy Word CDs)
//
// ---- Validated spell IDs (WoW 12.0 SpellName.csv / SpellLevels.csv) ----
//   17     Power Word: Shield        (L4 — self-absorb)
//   139    Renew                     (HoT)
//   527    Purify                    (L10 — Magic dispel)
//   440006 Purify Disease            (L10 — Disease dispel)
//   585    Smite                     (offensive filler / Holy Word CDR)
//   589    Shadow Word: Pain         (DoT filler)
//   596    Prayer of Healing
//   2006   Resurrection
//   2050   Holy Word: Serenity       (instant heal CD)
//   2060   Heal                      (mana-efficient filler)
//   2061   Flash Heal                (L3)
//   6788   Weakened Soul             (PW:Shield re-cast debuff)
//   8122   Psychic Scream
//   10060  Power Infusion            (L58)
//   13864  Power Word: Fortitude
//   14914  Holy Fire                 (offensive filler / Holy Word CDR)
//   15286  Vampiric Embrace          (L25 group lifelink)
//   19236  Desperate Prayer
//   20711  Spirit of Redemption      (PASSIVE — die in soul form; no rule)
//   32375  Mass Dispel
//   32546  Binding Heal              (heals self + target)
//   33076  Prayer of Mending         (L11 bouncing HoT)
//   34861  Holy Word: Sanctify       (raid heal CD)
//   47788  Guardian Spirit           (ally panic CD)
//   63733  Serendipity               (PASSIVE — Heal/Flash Heal speed up HW)
//   64843  Divine Hymn               (raid CD)
//   64901  Symbol of Hope            (mana regen)
//   73325  Leap of Faith             (L49 friendly pull)
//   88625  Holy Word: Chastise       (CC + damage)
//   110744 Divine Star               (talent)
//   120517 Halo                      (talent)
//   132157 Holy Nova                 (modern AoE heal+dmg ID)
//   200183 Apotheosis                (Holy Word CDR talent)
//   204883 Circle of Healing         (L39 raid AoE)
//   212036 Mass Resurrection         (L37 OOC)
//   265202 Holy Word: Salvation      (L27 raid panic)
//   372616 Empyreal Blaze            (talent — Holy Fire burst)
//   373481 Power Word: Life          (instant heal <=35%)
//   438733 Premonition of Piety      (talent — pre-emptive heal burst)
//
// ---- Skipped spells (and why) ----
//   - Serendipity (63733): passive proc speeding up Holy Word casts after
//     Heal/Flash Heal — no active cast, no rule needed (Heal/Flash Heal
//     are already in the priority list).
//   - Spirit of Redemption (20711): passive death effect. The bot dies
//     normally and the engine handles the soul-form aura — no APL rule.
//   - Holy Nova legacy ID 20694 was older; modern player cast is 132157.

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

// ---- Spell IDs (WoW 12.0, validated) ----
constexpr uint32 FLASH_HEAL              = 2061;
constexpr uint32 HEAL                    = 2060;
constexpr uint32 RENEW                   = 139;
constexpr uint32 PRAYER_OF_HEALING       = 596;
constexpr uint32 PRAYER_OF_MENDING       = 33076;
constexpr uint32 CIRCLE_OF_HEALING       = 204883;
constexpr uint32 HOLY_WORD_SERENITY      = 2050;
constexpr uint32 HOLY_WORD_SANCTIFY      = 34861;
constexpr uint32 HOLY_WORD_CHASTISE      = 88625;
constexpr uint32 DIVINE_HYMN             = 64843;
constexpr uint32 GUARDIAN_SPIRIT         = 47788;
constexpr uint32 SYMBOL_OF_HOPE          = 64901;
constexpr uint32 HOLY_WORD_SALVATION     = 265202;
constexpr uint32 PURIFY                  = 527;          // Magic dispel
constexpr uint32 PURIFY_DISEASE          = 440006;       // Disease dispel — Holy/Disc
constexpr uint32 MASS_DISPEL             = 32375;
constexpr uint32 BINDING_HEAL            = 32546;
constexpr uint32 HOLY_NOVA               = 132157;       // modern player cast (legacy 20694 deprecated)
constexpr uint32 POWER_WORD_LIFE         = 373481;
constexpr uint32 APOTHEOSIS              = 200183;       // talent — Holy Word CDR + buff
constexpr uint32 PREMONITION_OF_PIETY    = 438733;       // talent — pre-emptive heal burst (was wrongly 428930)
constexpr uint32 EMPYREAL_BLAZE          = 372616;       // talent — Holy Fire burst (Holy)
constexpr uint32 POWER_INFUSION          = 10060;
constexpr uint32 VAMPIRIC_EMBRACE        = 15286;
constexpr uint32 DESPERATE_PRAYER        = 19236;
constexpr uint32 POWER_WORD_SHIELD       = 17;           // self-absorb baseline
constexpr uint32 WEAKENED_SOUL           = 6788;         // PW:Shield debuff — gates re-cast
constexpr uint32 FADE                    = 586;
constexpr uint32 LEAP_OF_FAITH           = 73325;
constexpr uint32 PSYCHIC_SCREAM          = 8122;
constexpr uint32 RESURRECTION            = 2006;
constexpr uint32 MASS_RESURRECTION       = 212036;

// Offensive filler
constexpr uint32 SMITE                   = 585;
constexpr uint32 HOLY_FIRE               = 14914;
constexpr uint32 SHADOW_WORD_PAIN        = 589;
constexpr uint32 HALO                    = 120517;       // talent
constexpr uint32 DIVINE_STAR             = 110744;       // talent

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
// <=35% HP) — let active HoTs / Renew ticks carry the rest until the
// bot regenerates. Without this, Holy Priest spammed Flash Heal into
// OOM and had zero mana when the tank actually spiked (every spec
// other than Holy already had this gate). Apply at the top of normal-
// heal predicates ONLY — Guardian Spirit, HW: Salvation, Divine Hymn,
// PW: Life are the emergency floor and stay unmodified.
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
        { HEAL, FLASH_HEAL, BINDING_HEAL, PRAYER_OF_HEALING });
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

bool ShouldVampiricEmbrace(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(VAMPIRIC_EMBRACE)) return false;
    if (!ctx.bot.is_ready(VAMPIRIC_EMBRACE)) return false;
    return WoundedFriendCount(ctx, 75) >= 2;
}
void DoVampiricEmbrace(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(VAMPIRIC_EMBRACE); }

// ---- Dispel ----
// Purify dispatches to Purify (Magic) or Purify Disease based on what the
// debuff actually is on the chosen target. We pick the spell at predicate
// time so the target lookup matches the cast.
bool ShouldPurify(ApPredicateContext const& ctx)
{
    const bool can_magic   = ctx.bot.knows_spell(PURIFY)         && ctx.bot.is_ready(PURIFY);
    const bool can_disease = ctx.bot.knows_spell(PURIFY_DISEASE) && ctx.bot.is_ready(PURIFY_DISEASE);
    if (!can_magic && !can_disease) return false;
    if (auto const* mg = ctx.group.dispel_candidate(DispelType::Magic);   mg && can_magic)   return true;
    if (auto const* ds = ctx.group.dispel_candidate(DispelType::Disease); ds && can_disease) return true;
    if (can_magic   && ctx.bot.self_dispellable(DispelType::Magic))   return true;
    if (can_disease && ctx.bot.self_dispellable(DispelType::Disease)) return true;
    return false;
}
void DoPurify(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool can_magic   = ctx.bot.knows_spell(PURIFY)         && ctx.bot.is_ready(PURIFY);
    const bool can_disease = ctx.bot.knows_spell(PURIFY_DISEASE) && ctx.bot.is_ready(PURIFY_DISEASE);
    if (can_magic)
        if (auto const* mg = ctx.group.dispel_candidate(DispelType::Magic)) { e.cast(PURIFY, mg->guid); return; }
    if (can_disease)
        if (auto const* ds = ctx.group.dispel_candidate(DispelType::Disease)) { e.cast(PURIFY_DISEASE, ds->guid); return; }
    if (can_magic   && ctx.bot.self_dispellable(DispelType::Magic))   { e.cast(PURIFY,         ctx.bot.raw().guid); return; }
    if (can_disease && ctx.bot.self_dispellable(DispelType::Disease)) { e.cast(PURIFY_DISEASE, ctx.bot.raw().guid); return; }
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
bool ShouldHolyWordSalvation(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HOLY_WORD_SALVATION)) return false;
    if (!ctx.bot.is_ready(HOLY_WORD_SALVATION)) return false;
    return WoundedFriendCount(ctx, 50) >= 4;
}
void DoHolyWordSalvation(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HOLY_WORD_SALVATION); }

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

bool ShouldPowerWordLife(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(POWER_WORD_LIFE)) return false;
    if (!ctx.bot.is_ready(POWER_WORD_LIFE)) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 35;
}
void DoPowerWordLife(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(POWER_WORD_LIFE, LowestFriendOrSelf(ctx).guid);
}

// ---- Mana / burst CDs ----
bool ShouldSymbolOfHope(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SYMBOL_OF_HOPE)) return false;
    if (!ctx.bot.is_ready(SYMBOL_OF_HOPE)) return false;
    if (auto const* m = ctx.group.lowest_mana_caster())
        if (m->max_mana > 0 && (m->mana * 100) / m->max_mana <= 35)
            return true;
    return ctx.bot.max_power(0) > 0 && ctx.bot.power_pct(0) <= 35;
}
void DoSymbolOfHope(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SYMBOL_OF_HOPE); }

bool ShouldApotheosis(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(APOTHEOSIS)) return false;
    if (!ctx.bot.is_ready(APOTHEOSIS)) return false;
    return WoundedFriendCount(ctx, 75) >= 3;
}
void DoApotheosis(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(APOTHEOSIS); }

bool ShouldPremonitionOfPiety(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(PREMONITION_OF_PIETY)) return false;
    if (!ctx.bot.is_ready(PREMONITION_OF_PIETY)) return false;
    return WoundedFriendCount(ctx, 80) >= 2;
}
void DoPremonitionOfPiety(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(PREMONITION_OF_PIETY); }

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

bool ShouldEmpyrealBlaze(ApPredicateContext const& ctx)
{
    if (!HasLiveTargetInline(ctx)) return false;
    if (!ctx.bot.knows_spell(EMPYREAL_BLAZE)) return false;
    return ctx.bot.is_ready(EMPYREAL_BLAZE);
}
void DoEmpyrealBlaze(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(EMPYREAL_BLAZE, ctx.bot.victim());
}

// ---- AoE heal ----
bool ShouldHolyWordSanctify(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HOLY_WORD_SANCTIFY)) return false;
    if (!ctx.bot.is_ready(HOLY_WORD_SANCTIFY)) return false;
    return WoundedFriendCount(ctx, 70) >= 3;
}
void DoHolyWordSanctify(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HOLY_WORD_SANCTIFY, LowestFriendOrSelf(ctx).guid);
}

bool ShouldCircleOfHealing(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(CIRCLE_OF_HEALING)) return false;
    if (!ctx.bot.is_ready(CIRCLE_OF_HEALING)) return false;
    if (InManaFloor(ctx) && LowestFriendOrSelf(ctx).hp_pct > 35) return false;
    return WoundedFriendCount(ctx, 80) >= 3;
}
void DoCircleOfHealing(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CIRCLE_OF_HEALING, LowestFriendOrSelf(ctx).guid);
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

bool ShouldDivineStar(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DIVINE_STAR)) return false;
    if (!ctx.bot.is_ready(DIVINE_STAR)) return false;
    return WoundedFriendCount(ctx, 90) >= 2;
}
void DoDivineStar(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DIVINE_STAR); }

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

bool ShouldBindingHeal(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BINDING_HEAL)) return false;
    HealTarget t = LowestFriendOrSelf(ctx);
    if (InManaFloor(ctx) && t.hp_pct > 35) return false;
    // Binding Heal heals self + target — best when both bot AND another are
    // wounded.
    return t.hp_pct <= 65 && ctx.bot.hp_pct() <= 80 && t.guid != ctx.bot.raw().guid;
}
void DoBindingHeal(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BINDING_HEAL, LowestFriendOrSelf(ctx).guid);
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

bool ShouldRenew(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(RENEW)) return false;
    HealTarget t = LowestFriendOrSelf(ctx);
    if (t.hp_pct >= 95) return false;
    AuraEntry const* a = ctx.bot.find_aura(RENEW, t.guid);
    return !a || a->remaining.count() <= 3000;
}
void DoRenew(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RENEW, LowestFriendOrSelf(ctx).guid);
}

// ---- Filler ----
bool ShouldHeal(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HEAL)) return false;
    if (LowestFriendOrSelf(ctx).hp_pct > 90) return false;
    if (InManaFloor(ctx) && LowestFriendOrSelf(ctx).hp_pct > 35) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(HEAL)) return false;
    return true;
}
void DoHeal(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HEAL, LowestFriendOrSelf(ctx).guid);
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

bool ShouldShadowWordPainFiller(ApPredicateContext const& ctx)
{
    if (!HasLiveTargetInline(ctx)) return false;
    if (!GroupTopped(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOW_WORD_PAIN)) return false;
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

// Self Power Word: Shield — Holy's only non-talent damage-reduction CD on
// self. Discipline has it on kRules; Holy previously omitted it, leaving
// self-defense limited to Fade (threat dump) + Desperate Prayer (heal at
// <=40%). PW:Shield absorbs ~25-30% max HP for 15s. Weakened Soul (debuff
// 6788) prevents re-cast on the same target for 6s — gate on its absence.
// Fires at <=55% with active fight + a nearby enemy.
bool ShouldPowerWordShieldSelf(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(POWER_WORD_SHIELD)) return false;
    if (!ctx.bot.is_ready(POWER_WORD_SHIELD)) return false;
    if (ctx.bot.hp_pct() > 55) return false;
    if (ctx.bot.enemies_within(40.0f) == 0) return false;
    // Weakened Soul gate — can't re-shield self while debuff is up.
    if (ctx.bot.find_aura(WEAKENED_SOUL, ctx.bot.raw().guid)) return false;
    return true;
}
void DoPowerWordShieldSelf(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(POWER_WORD_SHIELD, ctx.bot.raw().guid);
}

// ---- Rule table (priority order) ----
// Order follows the Holy decision tree: cast-swap first (so we can change
// targets mid-cast), OOC rez, hard panic CDs (Guardian Spirit ally → Divine
// Hymn raid → HW: Salvation → HW: Serenity instant → Apotheosis CD), then
// PW: Shield self, Renew, Heal/Flash Heal, HW: Sanctify raid spike,
// Prayer of Mending bouncing HoT, Circle of Healing AoE, and finally the
// offensive filler that drives Holy Word CDR (Smite/Holy Fire/SW: Pain).
ApRule const kRules[] = {
    // Cast-swap MUST be first in the priority list — if we don't
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
    { ShouldPurify,              DoPurify,              "Purify / Purify Disease"        },
    { ShouldMassDispel,          DoMassDispel,          "Mass Dispel (raid)"             },
    // ---- Hard panic heals (ally life-savers, ordered by raw save power) ----
    { ShouldGuardianSpirit,      DoGuardianSpirit,      "Guardian Spirit (ally panic)"   },
    { ShouldDivineHymn,          DoDivineHymn,          "Divine Hymn (raid panic)"       },
    { ShouldHolyWordSalvation,   DoHolyWordSalvation,   "HW: Salvation (4+ at <=50%)"    },
    { ShouldHolyWordSerenity,    DoHolyWordSerenity,    "HW: Serenity (instant <=60%)"   },
    { ShouldApotheosis,          DoApotheosis,          "Apotheosis (Holy Word CDR)"     },
    { ShouldPowerWordLife,       DoPowerWordLife,       "PW: Life (<=35%)"               },
    { ShouldVampiricEmbrace,     DoVampiricEmbrace,     "Vampiric Embrace"               },
    // ---- Mana / burst CDs ----
    { ShouldSymbolOfHope,        DoSymbolOfHope,        "Symbol of Hope (mana)"          },
    { ShouldPremonitionOfPiety,  DoPremonitionOfPiety,  "Premonition of Piety"           },
    { ShouldPowerInfusion,       DoPowerInfusion,       "Power Infusion"                 },
    // ---- HoT maintenance ----
    { ShouldRenew,               DoRenew,               "Renew (HoT refresh)"            },
    { ShouldPrayerOfMending,     DoPrayerOfMending,     "Prayer of Mending (tank)"       },
    // ---- Spike heals ----
    { ShouldFlashHeal,           DoFlashHeal,           "Flash Heal (<=50%)"             },
    { ShouldBindingHeal,         DoBindingHeal,         "Binding Heal (<=65% other)"     },
    // ---- AoE heals ----
    { ShouldHolyWordSanctify,    DoHolyWordSanctify,    "HW: Sanctify (raid spike)"      },
    { ShouldCircleOfHealing,     DoCircleOfHealing,     "Circle of Healing (3+ at 80%)"  },
    { ShouldPrayerOfHealing,     DoPrayerOfHealing,     "Prayer of Healing (3+ at 80%)"  },
    { ShouldHolyNova,            DoHolyNova,            "Holy Nova (cleave heal+dmg)"    },
    { ShouldHaloHeal,            DoHaloHeal,            "Halo (raid heal)"               },
    { ShouldDivineStar,          DoDivineStar,          "Divine Star (cleave heal)"      },
    // ---- Mana-efficient filler ----
    { ShouldHeal,                DoHeal,                "Heal (efficient filler)"        },
    // ---- Offensive filler (group topped — drives Holy Word CDR) ----
    { ShouldEmpyrealBlaze,       DoEmpyrealBlaze,       "Empyreal Blaze (HF burst)"      },
    { ShouldHolyFireFiller,      DoHolyFireFiller,      "Holy Fire (filler / CDR)"       },
    { ShouldShadowWordPainFiller,DoShadowWordPainFiller,"SW: Pain (filler / CDR)"        },
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
