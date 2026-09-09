// Discipline Priest - WoW 12.1.0.69587 enterprise rotation. Atonement-driven
// healer: damage dealt to enemies heals every ally carrying Atonement (the
// buff is applied by PW: Shield / PW: Radiance / Evangelism). Decision tree:
//
//   1) OOC rez:                 Mass Resurrection / Resurrection
//   2) Hard panic peel:         Pain Suppression (ally <=30%), Power Word:
//                               Barrier (3+ at <=50%), Desperate Prayer
//                               (self <=40%)
//   3) Personal survival:       Leap of Faith (tank peel), Fade
//   4) Dispel:                  Purify (Magic; + Disease with Improved
//                               Purify), Mass Dispel
//   5) CC:                      Psychic Scream (panic AoE fear)
//   6) Big CDs:                 Evangelism (instant Radiance + next two
//                               Radiances instant), Ultimate Penitence
//                               (burst), Power Infusion
//   7) Atonement application:   PW: Radiance (3+ wounded), PW: Shield on
//                               tank / lowest
//   8) Spike heal:              Penance (heal branch), Flash Heal, Plea
//   9) Atonement damage rotor:  Holy Nova (3+ enemies), Penance (dmg),
//                               Shadow Word: Death (execute), Mind Blast,
//                               Shadow Word: Pain / Purge the Wicked
//                               refresh, Smite filler
//
// Server-side overrides (Unit::GetCastSpellInfo resolves OVERRIDE_ACTIONBAR
// auras) swap talent versions in when the bot casts the base id: SW: Pain
// -> Purge the Wicked (1250218 [R]), Smite -> Void Blast while an Entropic
// Rift is open (450405 [R][M]), Flash Heal -> Shadow Mend (1252217 [M]),
// Purify Disease -> Purify. No extra rules are needed for those.
//
// ---- Validated spell IDs (WoW 12.1.0.69587 SpellName.csv / kit) ----
//   17     Power Word: Shield       (L4 baseline; 7.5s category CD)
//   527    Purify                   (spec L10; Magic dispel, overrides 440006)
//   585    Smite                    (L1 baseline)
//   586    Fade                     (class talent [R][M])
//   589    Shadow Word: Pain        (L2 baseline)
//   2006   Resurrection             (L10 baseline)
//   2061   Flash Heal               (L3 baseline)
//   8092   Mind Blast               (class talent [R][M])
//   8122   Psychic Scream           (class talent [M])
//   10060  Power Infusion           (class talent [R][M])
//   19236  Desperate Prayer         (class talent [R][M])
//   32375  Mass Dispel              (class talent [R][M])
//   32379  Shadow Word: Death       (class talent [R][M])
//   33206  Pain Suppression         (spec talent [R][M])
//   47540  Penance                  (spec L11 - heal/dmg channel)
//   62618  Power Word: Barrier      (spec talent - choice vs Ult. Penitence)
//   73325  Leap of Faith            (class talent [R][M])
//   81749  Atonement                (AURA - applied by Shield / Radiance)
//   132157 Holy Nova                (class talent [R])
//   194509 Power Word: Radiance     (spec talent [R][M])
//   200829 Plea                     (spec L80 - cheap instant heal)
//   204213 Purge the Wicked         (AURA - DoT applied when 1250218 taken)
//   212036 Mass Resurrection        (spec L37 OOC)
//   390632 Improved Purify          (PASSIVE gate - Purify removes Disease)
//   421453 Ultimate Penitence       (spec talent [R][M])
//   472433 Evangelism               (spec talent [R][M] - instant Radiance)
//
// ---- Skipped spells (and why) ----
//   - Power Word: Life (373481), Schism (424509), Rapture (47536), Spirit
//     Shell (109964), Boon of the Ascended (325013), Divine Star (110744),
//     Mindgames (375901): not learnable by Discipline in 12.1.
//   - Silence (15487), Vampiric Embrace (15286): Shadow spec spells now.
//   - Halo (120517 / 120644): Holy (Archon) / Shadow hero talent; the Disc
//     hero trees (Voidweaver / Oracle) have no active Halo.
//   - Evangelism legacy row 246287: no longer exists in SpellName.
//   - Shadowfiend (34433): PASSIVE in 12.1 ("SW: Death has a chance to
//     summon a Shadowfiend"). Mindbender (1280137 [M]): PASSIVE
//     ("Evangelism summons a Mindbender"). Neither is castable.
//   - Purify Disease (440006): learned together with Purify (527), which
//     overrides it, so the cast always resolves to Purify; disease removal
//     exists only through Improved Purify (390632), which gates it here.
//   - Weakened Soul (6788): nothing applies it in 12.1 - is_ready() on
//     PW: Shield covers the category cooldown instead.
//   - Light's Wrath (207946) / Light of T'uure (208065) / Void Torrent
//     (205065): legacy Artifact rows in the baseline list, no learn level.
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
constexpr uint32 PW_SHIELD              = 17;         // 7.5s category CD (Weakened Soul is gone)
constexpr uint32 PENANCE                = 47540;      // channel heal+damage (Disc spec L11)
constexpr uint32 PAIN_SUPPRESSION       = 33206;
constexpr uint32 PW_RADIANCE            = 194509;
constexpr uint32 PW_BARRIER             = 62618;
constexpr uint32 FLASH_HEAL             = 2061;       // L3 baseline spike heal (Shadow Mend [M] upgrades it via override)
constexpr uint32 PLEA                   = 200829;     // spec L80 - cheap instant heal
constexpr uint32 SHADOW_WORD_PAIN_D     = 589;
constexpr uint32 PURGE_THE_WICKED_DOT   = 204213;     // DoT applied instead of SW:P when Purge the Wicked (1250218 [R]) is taken
constexpr uint32 SHADOW_WORD_DEATH      = 32379;      // execute (class talent)
constexpr uint32 ATONEMENT              = 81749;
constexpr uint32 MIND_BLAST             = 8092;
constexpr uint32 SMITE                  = 585;
constexpr uint32 HOLY_NOVA              = 132157;     // class talent [R] - 12yd AoE dmg + heal
constexpr uint32 PURIFY                 = 527;        // Magic dispel (spec L10; + Disease with Improved Purify)
constexpr uint32 IMPROVED_PURIFY        = 390632;     // PASSIVE gate - Purify additionally removes Disease
constexpr uint32 MASS_DISPEL            = 32375;      // group magic dispel + cleanses CC
constexpr uint32 EVANGELISM             = 472433;     // instant Radiance + next 2 Radiances instant/cheap
constexpr uint32 ULTIMATE_PENITENCE     = 421453;     // talent - channeled burst penance
constexpr uint32 DESPERATE_PRAYER       = 19236;      // self heal CD
constexpr uint32 FADE                   = 586;        // threat drop / temp DR talent
constexpr uint32 POWER_INFUSION         = 10060;      // haste CD
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

// PW: Shield carries a 7.5s category cooldown in 12.1 (Weakened Soul no
// longer exists) - is_ready() is the re-cast gate for every target.
bool ShieldEligible(ApPredicateContext const& ctx)
{
    return ctx.bot.is_ready(PW_SHIELD);
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

// ---- CC ----
// Discipline has no interrupt in 12.1 (Silence is a Shadow spec spell);
// Psychic Scream ([M] class talent) is the only emergency CC.
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

// ---- Big CDs ----
// Evangelism (472433): 12.1 version is an instant Power Word: Radiance on
// the target plus two instant, cheaper Radiances afterwards - the raid
// Atonement opener. Needs a friendly target. The Mindbender [M] passive
// (1280137) summons its pet off this cast.
bool ShouldEvangelism(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(EVANGELISM)) return false;
    if (!ctx.bot.is_ready(EVANGELISM)) return false;
    return WoundedFriendCount(ctx, 80) >= 3;
}
void DoEvangelism(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(EVANGELISM, LowestFriendOrSelf(ctx).guid);
}

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
    if (!ShieldEligible(ctx)) return false;
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
    if (!ShieldEligible(ctx)) return false;
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

// Flash Heal - direct spike heal when Penance is on cooldown or not enough.
// With the Shadow Mend [M] passive the server casts the upgraded version.
bool ShouldFlashHeal(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(FLASH_HEAL)) return false;
    if (!ctx.bot.is_ready(FLASH_HEAL)) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 50;
}
void DoFlashHeal(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FLASH_HEAL, LowestFriendOrSelf(ctx).guid);
}

// Plea (spec L80) - cheap instant top-off; fires in the band above Flash
// Heal so a moving priest still has a direct heal.
bool ShouldPlea(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(PLEA)) return false;
    if (!ctx.bot.is_ready(PLEA)) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 60;
}
void DoPlea(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(PLEA, LowestFriendOrSelf(ctx).guid);
}

// Holy Nova (class talent [R]) - 12yd self-centred AoE damage + heal. With
// 3+ enemies in reach (or the owner /aoe pin) it is the best Atonement
// feed per GCD.
bool ShouldHolyNova(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(HOLY_NOVA)) return false;
    if (!ctx.bot.is_ready(HOLY_NOVA)) return false;
    const size_t near = ctx.bot.enemies_within(12.0f);
    if (ctx.aoe_preference && near >= 1) return true;
    return near >= 3;
}
void DoHolyNova(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HOLY_NOVA); }

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

// SW: Pain refresh. With Purge the Wicked ([R] passive 1250218) the server
// swaps the cast for Purge the Wicked and the victim carries its own DoT
// (204213) instead of 589 - check both so the talented bot does not
// re-cast every tick.
bool ShouldSWP(ApPredicateContext const& ctx)
{
    if (!HasLiveTargetInline(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOW_WORD_PAIN_D)) return false;
    AuraEntry const* a = ctx.bot.find_aura(SHADOW_WORD_PAIN_D, ctx.bot.victim());
    if (!a) a = ctx.bot.find_aura(PURGE_THE_WICKED_DOT, ctx.bot.victim());
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

// Cast-swap shim - Disc's healing kit is mostly absorbs/Atonement (very
// short or instant casts); Penance (heal channel), Flash Heal and PW:
// Radiance (2s) are the casts long enough to be worth cancelling. See
// ApHealHelpers.h.
bool ShouldCancelHealForSwap(ApPredicateContext const& ctx)
{
    return ShouldCancelHealForSwapImpl(ctx, { PENANCE, FLASH_HEAL, PW_RADIANCE });
}

// ---- Rule table (priority order top-down) ----
// Order follows the Discipline decision tree in the header comment block:
// emergencies first (cast-swap + OOC rez), hard panic (Pain Suppression ->
// PW: Barrier -> Desperate Prayer), survival/threat, dispels, CC, big CDs
// (Evangelism -> Ultimate Penitence -> Power Infusion), Atonement
// application (Radiance + Shield), spike heals (Penance heal branch ->
// Flash Heal -> Plea), then the Atonement-feed damage rotor (Holy Nova ->
// SW: Death -> Mind Blast -> SW: Pain -> Smite).
ApRule const kRules[] = {
    { ShouldCancelHealForSwap,   DoCancelHealForSwap,   "Cancel heal — swap to lower target" },
    { ShouldMassResurrection,    DoMassResurrection,    "Mass Resurrection (OOC)"        },
    { ShouldResurrection,        DoResurrection,        "Resurrection (OOC)"             },
    // ---- Hard panic peels (highest priority - ally life-savers) ----
    { ShouldPainSuppression,     DoPainSuppression,     "Pain Suppression (ally <=30%)"  },
    { ShouldPowerWordBarrier,    DoPowerWordBarrier,    "PW: Barrier (3+ at 50%)"        },
    { ShouldDesperatePrayer,     DoDesperatePrayer,     "Desperate Prayer (self <=40%)"  },
    // ---- Threat / personal survival ----
    { ShouldLeapOfFaith,         DoLeapOfFaith,         "Leap of Faith (peel tank)"      },
    { ShouldFade,                DoFade,                "Fade (threat / DR)"             },
    // ---- Dispel ----
    { ShouldPurify,              DoPurify,              "Purify (Magic / Disease)"       },
    { ShouldMassDispel,          DoMassDispel,          "Mass Dispel (raid)"             },
    // ---- CC ----
    { ShouldPsychicScream,       DoPsychicScream,       "Psychic Scream (panic AoE)"     },
    // ---- Big CDs ----
    { ShouldEvangelism,          DoEvangelism,          "Evangelism (instant Radiance)"  },
    { ShouldUltimatePenitence,   DoUltimatePenitence,   "Ultimate Penitence (burst)"     },
    { ShouldPowerInfusion,       DoPowerInfusion,       "Power Infusion (DPS)"           },
    // ---- Atonement application (PW: Shield = Atonement applicator) ----
    { ShouldRadiance,            DoRadiance,            "PW: Radiance (3+ at 90%)"       },
    { ShouldShieldTankAtonement, DoShieldTankAtonement, "PW: Shield (tank atonement)"    },
    { ShouldShield,              DoShield,              "PW: Shield (lowest atonement)"  },
    // ---- Spike heal + Atonement-feed damage rotor ----
    { ShouldPenance,             DoPenance,             "Penance (heal/dmg channel)"     },
    { ShouldFlashHeal,           DoFlashHeal,           "Flash Heal (<=50%)"             },
    { ShouldPlea,                DoPlea,                "Plea (<=60% instant)"           },
    { ShouldHolyNova,            DoHolyNova,            "Holy Nova (3+ atone-feed)"      },
    { ShouldShadowWordDeath,     DoShadowWordDeath,     "SW: Death (<=20% execute)"      },
    { ShouldMindBlast,           DoMindBlast,           "Mind Blast"                     },
    { ShouldSWP,                 DoSWP,                 "SW: Pain / PtW (refresh)"       },
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
