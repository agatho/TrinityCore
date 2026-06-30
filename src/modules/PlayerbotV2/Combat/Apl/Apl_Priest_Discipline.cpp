// Discipline Priest - WoW 12.0 enterprise rotation. Atonement-driven healer:
// damage dealt to enemies heals all targets carrying Atonement (the buff is
// applied by PW:Shield / PW:Radiance / Power Word: Life). Decision tree:
//
//   1) Battle rez / OOC rez:    Resurrection
//   2) Hard panic peel:         Pain Suppression (ally <=30%), Power Word:
//                               Barrier (3+ at <=50%), Desperate Prayer
//                               (self <=40%), Power Word: Life (<=35%)
//   3) Personal survival:       Fade, Vampiric Embrace
//   4) Dispel:                  Purify (Magic), Purify Disease, Mass Dispel
//   5) Interrupt / CC:          Silence, Psychic Scream, Leap of Faith (peel),
//                               Power Infusion
//   6) Big damage CDs:          Evangelism (extend all Atonement), Spirit
//                               Shell (banked absorbs), Boon of the Ascended,
//                               Ultimate Penitence (burst), Mindbender /
//                               Shadowfiend, Rapture (free shields)
//   7) Atonement application:   PW: Shield on tank/lowest (Weakened Soul
//                               guarded), PW: Radiance pre-dmg (3+ wounded)
//   8) Atonement extension:     Penance (heal+dmg cleave), Halo / Divine
//                               Star, Mindgames
//   9) Atonement damage rotor:  Schism, Shadow Word: Death (execute), Mind
//                               Blast, Shadow Word: Pain refresh, Smite
//                               filler
//
// ---- Validated spell IDs (WoW 12.0 SpellName.csv / SpellLevels.csv) ----
//   17     Power Word: Shield       (L4)
//   139    Renew                    (passive learn)
//   527    Purify                   (L10; Magic dispel)
//   440006 Purify Disease           (L10; Disease dispel — Disc/Holy)
//   585    Smite
//   589    Shadow Word: Pain
//   2006   Resurrection
//   6788   Weakened Soul            (PW:Shield re-cast debuff)
//   8092   Mind Blast
//   8122   Psychic Scream
//   10060  Power Infusion           (L58)
//   15286  Vampiric Embrace         (L25)
//   15487  Silence                  (L26 — Shadow but trained pre-spec)
//   19236  Desperate Prayer
//   32375  Mass Dispel
//   32379  Shadow Word: Death       (L14)
//   34433  Shadowfiend
//   47536  Rapture                  (L41)
//   47540  Penance                  (L11 — channel heal/damage)
//   62618  Power Word: Barrier
//   73325  Leap of Faith            (L49)
//   81749  Atonement                (buff applied by PW:Shield/Radiance/Life)
//   109964 Spirit Shell             (talent — heals become absorbs)
//   110744 Divine Star              (talent)
//   120517 Halo                     (talent)
//   194509 Power Word: Radiance     (raid Atonement applicator)
//   200174 Mindbender               (talent replaces Shadowfiend)
//   212036 Mass Resurrection        (L37 OOC)
//   325013 Boon of the Ascended     (L48 talent — covenant-era burst CD)
//   33206  Pain Suppression
//   373481 Power Word: Life         (instant heal <=35% HP gate)
//   375901 Mindgames                (talent — heal/dmg flip)
//   421453 Ultimate Penitence       (talent — channeled burst Penance)
//   424509 Schism                   (talent — damage amp + Atonement transfer)
//
// ---- Skipped spells (and why) ----
//   - Power Word: Solace (talent removed in 12.0; CSV has no player-cast row).
//   - Mind Bomb (205369): removed from Discipline kit in modern WoW; Psychic
//     Scream covers the AoE CC slot.
//   - Premonition (Disc): the modern Premonition family is on Holy spec
//     ("Premonition of Piety" 438733); Disc has no equivalent talent in
//     12.0, so the old PREMONITION_SUNDERING placeholder (428933 = wrong —
//     that ID is "Premonition of Insight") is removed entirely.
//   - Evangelism (was 246287 → now 472433): kept as a knows_spell-gated CD;
//     extends all Atonement durations by 6s.

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
constexpr uint32 PW_SHIELD              = 17;
constexpr uint32 WEAKENED_SOUL          = 6788;       // PW:Shield cooldown debuff (~7.5s)
constexpr uint32 PENANCE                = 47540;      // channel heal+damage (Disc base, same spell line as 197419)
constexpr uint32 PAIN_SUPPRESSION       = 33206;
constexpr uint32 PW_RADIANCE            = 194509;
constexpr uint32 PW_BARRIER             = 62618;
constexpr uint32 PW_LIFE                = 373481;     // <=35% emergency heal
constexpr uint32 SCHISM                 = 424509;     // talent — damage amp (modern 12.0 ID — was 214621 which is now "Mind Blast")
constexpr uint32 SHADOW_WORD_PAIN_D     = 589;
constexpr uint32 SHADOW_WORD_DEATH      = 32379;      // execute (L14 baseline)
constexpr uint32 ATONEMENT              = 81749;
constexpr uint32 MIND_BLAST             = 8092;
constexpr uint32 SMITE                  = 585;
constexpr uint32 RAPTURE                = 47536;
constexpr uint32 SILENCE                = 15487;
constexpr uint32 PURIFY                 = 527;        // Magic dispel (Priest baseline)
constexpr uint32 PURIFY_DISEASE         = 440006;     // Disease dispel — Disc/Holy
constexpr uint32 MASS_DISPEL            = 32375;      // group magic dispel + cleanses CC
constexpr uint32 HALO                   = 120517;     // talent — outgoing ring of dmg+heal
constexpr uint32 DIVINE_STAR            = 110744;     // talent
constexpr uint32 MINDGAMES              = 375901;     // talent — heal/dmg flip
// Evangelism: 472433 is the modern Disc talent ID, 246287 was the legacy
// row. Both probed via knows_spell so older data builds still work.
constexpr uint32 EVANGELISM             = 472433;
constexpr uint32 EVANGELISM_LEGACY      = 246287;
constexpr uint32 SPIRIT_SHELL           = 109964;     // talent — heals become absorbs
constexpr uint32 BOON_OF_THE_ASCENDED   = 325013;     // covenant-talent burst CD (L48)
constexpr uint32 ULTIMATE_PENITENCE     = 421453;     // talent — channeled burst penance
constexpr uint32 MINDBENDER             = 200174;     // talent — pet
constexpr uint32 SHADOWFIEND            = 34433;      // baseline — pet, mana return
constexpr uint32 DESPERATE_PRAYER       = 19236;      // self heal CD
constexpr uint32 FADE                   = 586;        // threat drop / temp DR talent
constexpr uint32 VAMPIRIC_EMBRACE       = 15286;      // group lifelink CD
constexpr uint32 POWER_INFUSION         = 10060;      // 25% haste, 20s
constexpr uint32 LEAP_OF_FAITH          = 73325;      // friendly pull / peel
constexpr uint32 PSYCHIC_SCREAM         = 8122;       // AoE fear
constexpr uint32 RESURRECTION           = 2006;
constexpr uint32 MASS_RESURRECTION      = 212036;     // OOC raid rez

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

// Skip Shield if target has Weakened Soul (PW:Shield will not stick).
bool ShieldEligible(ApPredicateContext const& ctx, ObjectGuid g)
{
    return !ctx.bot.has_aura(WEAKENED_SOUL, g);
}

GroupMemberSummary const* OffensivePIBeneficiary(ApPredicateContext const& ctx)
{
    auto const* members = ctx.group.members();
    if (!members) return nullptr;
    GroupMemberSummary const* best = nullptr;
    for (auto const& m : *members)
    {
        if (!m.online || m.hp <= 0) continue;
        if (m.guid == ctx.bot.raw().guid) continue;
        if (m.role != Role::Dps) continue;
        if (!best) best = &m;
    }
    return best;
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
    // Worth the long cast only if multiple are dead.
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
    // With Phantasm/Fade talent provides DR; baseline is threat drop. Pop when
    // we're being hit and our HP is dipping while the tank is alive.
    if (ctx.bot.hp_pct() > 55) return false;
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
// Purify casts the right spell for the right debuff type: Purify (527) for
// Magic, Purify Disease (440006) for Disease. We pick the spell at predicate
// time so the target lookup matches the dispel call exactly.
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

// CB-P1d (Discipline sibling of the Holy fix): Mass Dispel is a GROUND-targeted
// AoE that strips Magic from allies in the area. Fire when MULTIPLE grouped
// allies share a dispellable Magic debuff and aim at their centroid — NOT when
// the priest itself happens to be debuffed, aimed at self (the old gate, which
// meant Mass Dispel essentially never fired for its real raid use).
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
    return MagicAfflictedAllyCount(ctx) >= 3;
}
void DoMassDispel(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    float cx = 0.f, cy = 0.f, cz = 0.f;
    if (MagicAfflictedAllyCount(ctx, &cx, &cy, &cz) > 0)
        e.cast_at(MASS_DISPEL, cx, cy, cz);
}

// ---- Interrupt / CC ----
bool ShouldSilence(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SILENCE)) return false;
    if (!ctx.bot.is_ready(SILENCE)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (pvp) return ctx.bot.kick_target(true, 30.0f) != nullptr;
    auto const* c = ctx.bot.interruptible_caster();
    return c && c->guid == ctx.bot.victim();
}
void DoSilence(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 30.0f))
        e.cast(SILENCE, c->guid);
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

bool ShouldPowerInfusion(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(POWER_INFUSION)) return false;
    if (!ctx.bot.is_ready(POWER_INFUSION)) return false;
    // Self-PI when group is healthy and we're in burn mode, otherwise hand
    // it to a DPS (best DPS heuristic — first non-self Dps in the snapshot).
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

// ---- Hard panic ----
bool ShouldPowerWordBarrier(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(PW_BARRIER)) return false;
    if (!ctx.bot.is_ready(PW_BARRIER)) return false;
    return WoundedFriendCount(ctx, 50) >= 3;
}
void DoPowerWordBarrier(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* tank = ctx.group.tank())
    {
        e.cast(PW_BARRIER, tank->guid);
        return;
    }
    e.cast(PW_BARRIER, ctx.bot.raw().guid);
}

bool ShouldPainSuppression(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(PAIN_SUPPRESSION)) return false;
    if (!ctx.bot.is_ready(PAIN_SUPPRESSION)) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 30;
}
void DoPainSuppression(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(PAIN_SUPPRESSION, LowestFriendOrSelf(ctx).guid);
}

bool ShouldPowerWordLife(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(PW_LIFE)) return false;
    if (!ctx.bot.is_ready(PW_LIFE)) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 35;
}
void DoPowerWordLife(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(PW_LIFE, LowestFriendOrSelf(ctx).guid);
}

// ---- Big damage CDs ----
// Evangelism: modern Disc talent ID is 472433; older data may carry 246287.
// Probe both via knows_spell so the rotation works regardless of which row
// the data build picked up.
bool ShouldEvangelism(ApPredicateContext const& ctx)
{
    uint32 sid = 0;
    if (ctx.bot.knows_spell(EVANGELISM)        && ctx.bot.is_ready(EVANGELISM))        sid = EVANGELISM;
    else if (ctx.bot.knows_spell(EVANGELISM_LEGACY) && ctx.bot.is_ready(EVANGELISM_LEGACY)) sid = EVANGELISM_LEGACY;
    if (sid == 0) return false;
    return WoundedFriendCount(ctx, 80) >= 3;
}
void DoEvangelism(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (ctx.bot.knows_spell(EVANGELISM) && ctx.bot.is_ready(EVANGELISM)) { e.cast(EVANGELISM); return; }
    e.cast(EVANGELISM_LEGACY);
}

bool ShouldSpiritShell(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SPIRIT_SHELL)) return false;
    if (!ctx.bot.is_ready(SPIRIT_SHELL)) return false;
    return WoundedFriendCount(ctx, 85) >= 2;
}
void DoSpiritShell(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SPIRIT_SHELL); }

bool ShouldBoonOfTheAscended(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(BOON_OF_THE_ASCENDED)) return false;
    if (!ctx.bot.is_ready(BOON_OF_THE_ASCENDED)) return false;
    return BossLikeTargetEngaged(ctx) || WoundedFriendCount(ctx, 75) >= 2;
}
void DoBoonOfTheAscended(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BOON_OF_THE_ASCENDED); }

bool ShouldUltimatePenitence(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ULTIMATE_PENITENCE)) return false;
    if (!ctx.bot.is_ready(ULTIMATE_PENITENCE)) return false;
    return WoundedFriendCount(ctx, 60) >= 2;
}
void DoUltimatePenitence(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ULTIMATE_PENITENCE, LowestFriendOrSelf(ctx).guid);
}

bool ShouldMindbender(ApPredicateContext const& ctx)
{
    if (!HasLiveTargetInline(ctx)) return false;
    if (!ctx.bot.knows_spell(MINDBENDER)) return false;
    if (!ctx.bot.is_ready(MINDBENDER)) return false;
    return ctx.bot.in_combat();
}
void DoMindbender(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MINDBENDER, ctx.bot.victim());
}

bool ShouldShadowfiend(ApPredicateContext const& ctx)
{
    if (!HasLiveTargetInline(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOWFIEND)) return false;
    if (!ctx.bot.is_ready(SHADOWFIEND)) return false;
    if (ctx.bot.knows_spell(MINDBENDER)) return false;
    return ctx.bot.in_combat();
}
void DoShadowfiend(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHADOWFIEND, ctx.bot.victim());
}

bool ShouldRapture(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(RAPTURE)) return false;
    if (!ctx.bot.is_ready(RAPTURE)) return false;
    return WoundedFriendCount(ctx, 70) >= 3;
}
void DoRapture(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RAPTURE, LowestFriendOrSelf(ctx).guid);
}

// ---- Atonement application ----
bool ShouldRadiance(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(PW_RADIANCE)) return false;
    if (!ctx.bot.is_ready(PW_RADIANCE)) return false;
    return WoundedFriendCount(ctx, 90) >= 3;
}
void DoRadiance(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(PW_RADIANCE, LowestFriendOrSelf(ctx).guid);
}

bool ShouldShieldTankAtonement(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(PW_SHIELD)) return false;
    GroupMemberSummary const* tank = ctx.group.tank();
    if (!tank || !tank->online || tank->hp <= 0) return false;
    if (!ShieldEligible(ctx, tank->guid)) return false;
    AuraEntry const* shield = ctx.bot.find_aura(PW_SHIELD, tank->guid);
    AuraEntry const* atone  = ctx.bot.find_aura(ATONEMENT, tank->guid);
    return !shield && (!atone || atone->remaining.count() <= 4000);
}
void DoShieldTankAtonement(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* tank = ctx.group.tank())
        e.cast(PW_SHIELD, tank->guid);
}

bool ShouldShield(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(PW_SHIELD)) return false;
    HealTarget t = LowestFriendOrSelf(ctx);
    if (t.hp_pct >= 95) return false;
    if (!ShieldEligible(ctx, t.guid)) return false;
    AuraEntry const* shield = ctx.bot.find_aura(PW_SHIELD, t.guid);
    AuraEntry const* atone  = ctx.bot.find_aura(ATONEMENT, t.guid);
    return !shield && (!atone || atone->remaining.count() <= 4000);
}
void DoShield(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(PW_SHIELD, LowestFriendOrSelf(ctx).guid);
}

// ---- Atonement-fed damage (heals via Atonement transfer) ----
bool ShouldPenance(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(PENANCE)) return false;
    if (!ctx.bot.is_ready(PENANCE)) return false;
    // Penance is a heal+dmg cleave — cast on lowest friendly when wounded,
    // otherwise on the victim to drive Atonement healing.
    if (LowestFriendOrSelf(ctx).hp_pct <= 80) return true;
    return HasLiveTargetInline(ctx);
}
void DoPenance(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    HealTarget t = LowestFriendOrSelf(ctx);
    if (t.hp_pct <= 80) { e.cast(PENANCE, t.guid); return; }
    e.cast(PENANCE, ctx.bot.victim());
}

bool ShouldHalo(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(HALO)) return false;
    if (!ctx.bot.is_ready(HALO)) return false;
    return WoundedFriendCount(ctx, 85) >= 2 || ctx.bot.attackers_count() >= 2;
}
void DoHalo(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HALO); }

bool ShouldDivineStar(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DIVINE_STAR)) return false;
    if (!ctx.bot.is_ready(DIVINE_STAR)) return false;
    return WoundedFriendCount(ctx, 90) >= 2 || ctx.bot.attackers_count() >= 2;
}
void DoDivineStar(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DIVINE_STAR); }

bool ShouldMindgames(ApPredicateContext const& ctx)
{
    if (!HasLiveTargetInline(ctx)) return false;
    if (!ctx.bot.knows_spell(MINDGAMES)) return false;
    return ctx.bot.is_ready(MINDGAMES);
}
void DoMindgames(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MINDGAMES, ctx.bot.victim());
}

bool ShouldSchism(ApPredicateContext const& ctx)
{
    if (!HasLiveTargetInline(ctx)) return false;
    if (!ctx.bot.knows_spell(SCHISM)) return false;
    return ctx.bot.is_ready(SCHISM);
}
void DoSchism(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SCHISM, ctx.bot.victim());
}

bool ShouldShadowWordDeath(ApPredicateContext const& ctx)
{
    if (!HasLiveTargetInline(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOW_WORD_DEATH)) return false;
    if (!ctx.bot.is_ready(SHADOW_WORD_DEATH)) return false;
    NearbyUnit const* t = ctx.bot.victim_info();
    if (!t || t->max_hp <= 0) return false;
    int32 hp_pct = (t->hp * 100) / t->max_hp;
    return hp_pct <= 20;
}
void DoShadowWordDeath(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHADOW_WORD_DEATH, ctx.bot.victim());
}

bool ShouldMindBlast(ApPredicateContext const& ctx)
{
    if (!HasLiveTargetInline(ctx)) return false;
    if (!ctx.bot.knows_spell(MIND_BLAST)) return false;
    return ctx.bot.is_ready(MIND_BLAST);
}
void DoMindBlast(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MIND_BLAST, ctx.bot.victim());
}

bool ShouldSWP(ApPredicateContext const& ctx)
{
    if (!HasLiveTargetInline(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOW_WORD_PAIN_D)) return false;
    AuraEntry const* a = ctx.bot.find_aura(SHADOW_WORD_PAIN_D, ctx.bot.victim());
    return !a || a->remaining.count() <= 3000;
}
void DoSWP(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHADOW_WORD_PAIN_D, ctx.bot.victim());
}

bool ShouldSmite(ApPredicateContext const& ctx)
{
    if (!HasLiveTargetInline(ctx)) return false;
    if (!ctx.bot.knows_spell(SMITE)) return false;
    if (ctx.bot.is_moving() && !ctx.bot.can_cast_while_moving(SMITE)) return false;
    return true;
}
void DoSmite(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SMITE, ctx.bot.victim());
}

bool AlwaysAlive(ApPredicateContext const& ctx) { return ctx.bot.is_alive(); }
void DoNothing(ApPredicateContext const&, BotIntentEmitter&) {}

// Cast-swap shim — Disc's healing kit is mostly absorbs/Atonement
// (very short or instant casts), so PENANCE (heal channel) is the
// main spell long enough to be worth cancelling. See ApHealHelpers.h.
bool ShouldCancelHealForSwap(ApPredicateContext const& ctx)
{
    return ShouldCancelHealForSwapImpl(ctx, { PENANCE });
}

// ---- Rule table (priority order top-down) ----
// Order follows the Discipline decision tree in the header comment block:
// emergencies first (cast-swap + OOC rez), hard panic (Pain Suppression →
// PW: Barrier → Desperate Prayer → PW: Life), survival/threat, dispels,
// interrupts/CC, group CDs, Atonement application (Shield + Radiance), then
// the Atonement-feed damage rotor (Penance heal/dmg → SW:P → Smite).
ApRule const kRules[] = {
    { ShouldCancelHealForSwap,   DoCancelHealForSwap,   "Cancel heal — swap to lower target" },
    { ShouldMassResurrection,    DoMassResurrection,    "Mass Resurrection (OOC)"        },
    { ShouldResurrection,        DoResurrection,        "Resurrection (OOC)"             },
    // ---- Hard panic peels (highest priority — ally life-savers) ----
    { ShouldPainSuppression,     DoPainSuppression,     "Pain Suppression (ally <=30%)"  },
    { ShouldPowerWordBarrier,    DoPowerWordBarrier,    "PW: Barrier (3+ at 50%)"        },
    { ShouldDesperatePrayer,     DoDesperatePrayer,     "Desperate Prayer (self <=40%)"  },
    { ShouldPowerWordLife,       DoPowerWordLife,       "PW: Life (<=35%)"               },
    // ---- Threat / personal survival ----
    { ShouldLeapOfFaith,         DoLeapOfFaith,         "Leap of Faith (peel tank)"      },
    { ShouldFade,                DoFade,                "Fade (threat / DR)"             },
    { ShouldVampiricEmbrace,     DoVampiricEmbrace,     "Vampiric Embrace"               },
    // ---- Dispel ----
    { ShouldPurify,              DoPurify,              "Purify / Purify Disease"        },
    { ShouldMassDispel,          DoMassDispel,          "Mass Dispel (raid)"             },
    // ---- Interrupt / CC ----
    { ShouldSilence,             DoSilence,             "Silence (interrupt)"            },
    { ShouldPsychicScream,       DoPsychicScream,       "Psychic Scream (panic AoE)"     },
    // ---- Big CDs ----
    { ShouldRapture,             DoRapture,             "Rapture (free shields)"         },
    { ShouldEvangelism,          DoEvangelism,          "Evangelism (extend Atone)"      },
    { ShouldSpiritShell,         DoSpiritShell,         "Spirit Shell (banked absorbs)"  },
    { ShouldUltimatePenitence,   DoUltimatePenitence,   "Ultimate Penitence (burst)"     },
    { ShouldBoonOfTheAscended,   DoBoonOfTheAscended,   "Boon of the Ascended"           },
    { ShouldPowerInfusion,       DoPowerInfusion,       "Power Infusion (DPS)"           },
    { ShouldMindbender,          DoMindbender,          "Mindbender"                     },
    { ShouldShadowfiend,         DoShadowfiend,         "Shadowfiend"                    },
    // ---- Atonement application (PW: Shield = Atonement applicator) ----
    { ShouldRadiance,            DoRadiance,            "PW: Radiance (3+ at 90%)"       },
    { ShouldShieldTankAtonement, DoShieldTankAtonement, "PW: Shield (tank atonement)"    },
    { ShouldShield,              DoShield,              "PW: Shield (lowest atonement)"  },
    // ---- Spike heal + Atonement-feed damage rotor ----
    { ShouldPenance,             DoPenance,             "Penance (heal/dmg channel)"     },
    { ShouldHalo,                DoHalo,                "Halo (AoE atone-feed)"          },
    { ShouldDivineStar,          DoDivineStar,          "Divine Star (cleave)"           },
    { ShouldMindgames,           DoMindgames,           "Mindgames"                      },
    { ShouldSchism,              DoSchism,              "Schism (424509)"                },
    { ShouldShadowWordDeath,     DoShadowWordDeath,     "SW: Death (<=20% execute)"      },
    { ShouldMindBlast,           DoMindBlast,           "Mind Blast"                     },
    { ShouldSWP,                 DoSWP,                 "SW: Pain (refresh, DoT)"        },
    { ShouldSmite,               DoSmite,               "Smite (atonement filler)"       },
    { AlwaysAlive,               DoNothing,             "Idle"                           },
};

} // anonymous

void RegisterApl_Priest_Discipline()
{
    constexpr uint32 SPEC_PRIEST_DISCIPLINE = 256;
    RegisterRotation(CLASS_PRIEST, SPEC_PRIEST_DISCIPLINE, ApRotation{kRules});
}

} // namespace Playerbot::Combat
