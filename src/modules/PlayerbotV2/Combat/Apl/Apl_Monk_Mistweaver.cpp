// Mistweaver Monk - WoW 12.0 enterprise rotation. Hybrid mist healer that
// stays in caster style (no fistweaving here — the action queue would need a
// melee/caster mode pivot we don't yet have). Decision tree:
//
//   1) Battle rez / OOC rez:  Resuscitate
//   2) Emergency layer:       Life Cocoon, Fortifying Brew, Dampen Harm,
//                             Diffuse Magic, Zen Meditation, Tiger's Lust
//   3) Dispel:                Detox (Magic+Disease+Poison)
//   4) Interrupt / CC:        Spear Hand Strike, Paralysis off-target,
//                             Leg Sweep, Ring of Peace
//   5) Battle CDs:            Mana Tea, Thunder Focus Tea, Invoke Yu'lon
//                             / Invoke Chi-Ji, Restoral, Revival
//   6) AoE blanket:           Essence Font, Sheilun's Gift, Refreshing
//                             Jade Wind, Faeline Stomp
//   7) HoT maintenance:       Renewing Mist (auto-jumps via talent)
//   8) Spike heal:            Enveloping Mist (<=60%), Vivify (<=85%)
//   9) Filler / mana floor:   Soothing Mist channel
//  10) Offensive (group full): Tiger Palm, Blackout Kick, Rising Sun Kick
//                              (light DPS that returns mana via Spinning
//                              Crane Kick replacement on some talents)

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
// Validated IDs:
//   115175 Soothing Mist            124682 Enveloping Mist        115151 Renewing Mist
//   116670 Vivify                   116849 Life Cocoon            115310 Revival
//   388615 Restoral (talent)        191837 Essence Font           399491 Sheilun's Gift
//   196725 Refreshing Jade Wind     388193 Jadefire Stomp         322118 Invoke Yu'lon
//   325197 Invoke Chi-Ji            123904 Invoke Xuen            197908 Mana Tea
//   116680 Thunder Focus Tea        123986 Chi Burst              115098 Chi Wave
//   115203 Fortifying Brew          122278 Dampen Harm            122783 Diffuse Magic
//   115176 Zen Meditation           115450 Detox (MW)             116705 Spear Hand Strike
//   115078 Paralysis                119381 Leg Sweep              116844 Ring of Peace
//   116841 Tiger's Lust             115178 Resuscitate (OOC rez)  212051 Reawaken (combat rez)
//   100780 Tiger Palm               100784 Blackout Kick (generic)107428 Rising Sun Kick
//   119611 Renewing Mist aura
//
// Skipped (with reason):
//   116645 Teachings of the Monastery passive — buffs Tiger Palm / Blackout
//                                      Kick / Rising Sun Kick / SCK with
//                                      healing splash; not castable.
//   388023 Ancient Teachings          passive talent — Tiger Palm/RSK heal
//                                      based on damage dealt; not castable.
//   202577 Dome of Mist               passive — Renewing Mist refresh on
//                                      cast; not castable.
//   274909 Rising Mist                talent passive that refreshes ReM/EnvM
//                                      when RSK is cast — not castable.
//   231602 Improved Vivify            passive cast-while-moving talent.
constexpr uint32 SOOTHING_MIST          = 115175;
constexpr uint32 ENVELOPING_MIST        = 124682;
constexpr uint32 RENEWING_MIST          = 115151;
constexpr uint32 VIVIFY                 = 116670;
constexpr uint32 LIFE_COCOON            = 116849;
constexpr uint32 REVIVAL                = 115310;
constexpr uint32 RESTORAL               = 388615;       // talent — replaces Revival on some trees
constexpr uint32 ESSENCE_FONT           = 191837;
constexpr uint32 SHEILUNS_GIFT          = 399491;
constexpr uint32 REFRESHING_JADE_WIND   = 196725;       // talent — channeled AoE HoT
constexpr uint32 FAELINE_STOMP          = 388193;       // talent
constexpr uint32 JADEFIRE_STOMP         = 388193;       // current label / same id
constexpr uint32 INVOKE_YULON           = 322118;       // jade serpent statue burst
constexpr uint32 INVOKE_CHI_JI          = 325197;       // crane spirit (talent variant)
constexpr uint32 INVOKE_XUEN            = 123904;       // optional offensive talent
constexpr uint32 MANA_TEA               = 197908;
constexpr uint32 MANA_TEA_AURA          = 197908;       // self-aura: -50% mana cost
constexpr uint32 THUNDER_FOCUS_TEA      = 116680;
constexpr uint32 CHI_BURST              = 123986;       // talent — AoE heal+dmg
constexpr uint32 CHI_WAVE               = 115098;       // talent — bounces 7 times

// Survival
constexpr uint32 FORTIFYING_BREW        = 115203;
constexpr uint32 DAMPEN_HARM            = 122278;
constexpr uint32 DIFFUSE_MAGIC          = 122783;
constexpr uint32 ZEN_MEDITATION         = 115176;

// Utility / CC
constexpr uint32 DETOX                  = 115450;
constexpr uint32 SPEAR_HAND_STRIKE      = 116705;
constexpr uint32 PARALYSIS              = 115078;
constexpr uint32 LEG_SWEEP              = 119381;
constexpr uint32 RING_OF_PEACE          = 116844;
constexpr uint32 TIGERS_LUST            = 116841;
constexpr uint32 RESUSCITATE            = 115178;       // OOC rez (out of combat only)
constexpr uint32 REAWAKEN               = 212051;       // Mistweaver in-combat battle rez

// Offensive filler
constexpr uint32 TIGER_PALM             = 100780;
constexpr uint32 BLACKOUT_KICK          = 100784;       // Mistweaver / generic id
constexpr uint32 RISING_SUN_KICK        = 107428;

// Aura trackers
constexpr uint32 RENEWING_MIST_AURA     = 119611;
constexpr uint32 ENVELOPING_MIST_AURA   = 124682;
constexpr uint32 LIFE_COCOON_AURA       = 116849;

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

GroupMemberSummary const* DispelTarget(ApPredicateContext const& ctx)
{
    return DispelTargetWithPriority(ctx, [](GroupSnapshotView const& g)
        -> GroupMemberSummary const*
    {
        if (auto const* m = g.dispel_candidate(DispelType::Magic))   return m;
        if (auto const* m = g.dispel_candidate(DispelType::Disease)) return m;
        if (auto const* m = g.dispel_candidate(DispelType::Poison))  return m;
        return nullptr;
    });
}

bool SelfNeedsDispel(ApPredicateContext const& ctx)
{
    return ctx.bot.self_dispellable(DispelType::Magic)
        || ctx.bot.self_dispellable(DispelType::Disease)
        || ctx.bot.self_dispellable(DispelType::Poison);
}

// ---- Battle / OOC rez ----
// Reawaken (212051) is Mistweaver's in-combat battle rez (60s cast on a
// fallen ally returns them to life mid-fight). Resuscitate (115178) is the
// OOC rez. We try Reawaken first whenever combat is active and an ally is
// down on our map; otherwise fall back to Resuscitate when out of combat.
bool ShouldReawaken(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(REAWAKEN)) return false;
    if (!ctx.bot.is_ready(REAWAKEN)) return false;
    return ctx.group.dead_member(ctx.bot.map_id()) != nullptr;
}
void DoReawaken(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* m = ctx.group.dead_member(ctx.bot.map_id()))
        e.cast(REAWAKEN, m->guid);
}

bool ShouldResuscitate(ApPredicateContext const& ctx)
{
    if (ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(RESUSCITATE)) return false;
    if (!ctx.bot.is_ready(RESUSCITATE)) return false;
    return ctx.group.dead_member(ctx.bot.map_id()) != nullptr;
}
void DoResuscitate(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* m = ctx.group.dead_member(ctx.bot.map_id()))
        e.cast(RESUSCITATE, m->guid);
}

// ---- Interrupt / CC ----
bool ShouldSpearHandStrike(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SPEAR_HAND_STRIKE)) return false;
    if (!ctx.bot.is_ready(SPEAR_HAND_STRIKE)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (pvp) return ctx.bot.kick_target(true, 5.0f) != nullptr;
    auto const* c = ctx.bot.interruptible_caster();
    return c && c->guid == ctx.bot.victim();
}
void DoSpearHandStrike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 5.0f))
        e.cast(SPEAR_HAND_STRIKE, c->guid);
}

bool ShouldParalysisOffTarget(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(PARALYSIS)) return false;
    if (!ctx.bot.is_ready(PARALYSIS)) return false;
    auto const* c = ctx.bot.interruptible_caster();
    if (!c || c->guid == ctx.bot.victim()) return false;
    return !ctx.bot.is_ready(SPEAR_HAND_STRIKE);
}
void DoParalysisOffTarget(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* c = ctx.bot.interruptible_caster())
        e.cast(PARALYSIS, c->guid);
}

bool ShouldLegSweep(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(LEG_SWEEP)) return false;
    if (!ctx.bot.is_ready(LEG_SWEEP)) return false;
    return ctx.bot.attackers_count() >= 3;
}
void DoLegSweep(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(LEG_SWEEP); }

bool ShouldRingOfPeace(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(RING_OF_PEACE)) return false;
    if (!ctx.bot.is_ready(RING_OF_PEACE)) return false;
    return ctx.bot.attackers_count() >= 4 && ctx.bot.hp_pct() <= 40;
}
void DoRingOfPeace(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    float bx, by, bz;
    ctx.bot.position(bx, by, bz);
    e.cast_at(RING_OF_PEACE, bx, by, bz);
}

// ---- Survival ladder ----
bool ShouldFortifyingBrew(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(FORTIFYING_BREW)) return false;
    if (!ctx.bot.is_ready(FORTIFYING_BREW)) return false;
    return ctx.bot.hp_pct() <= 35;
}
void DoFortifyingBrew(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FORTIFYING_BREW); }

bool ShouldDampenHarm(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DAMPEN_HARM)) return false;
    if (!ctx.bot.is_ready(DAMPEN_HARM)) return false;
    return ctx.bot.hp_pct() <= 60;
}
void DoDampenHarm(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DAMPEN_HARM); }

bool ShouldDiffuseMagic(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DIFFUSE_MAGIC)) return false;
    if (!ctx.bot.is_ready(DIFFUSE_MAGIC)) return false;
    if (ctx.bot.hp_pct() > 60) return false;
    return ctx.bot.interruptible_caster() != nullptr;
}
void DoDiffuseMagic(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DIFFUSE_MAGIC); }

bool ShouldZenMeditation(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ZEN_MEDITATION)) return false;
    if (!ctx.bot.is_ready(ZEN_MEDITATION)) return false;
    if (ctx.bot.hp_pct() > 50) return false;
    return ctx.bot.interruptible_caster() != nullptr;
}
void DoZenMeditation(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ZEN_MEDITATION); }

// ---- Dispel ----
bool ShouldDetox(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(DETOX)) return false;
    if (!ctx.bot.is_ready(DETOX)) return false;
    return DispelTarget(ctx) != nullptr || SelfNeedsDispel(ctx);
}
void DoDetox(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = DispelTarget(ctx)) { e.cast(DETOX, t->guid); return; }
    if (SelfNeedsDispel(ctx))                e.cast(DETOX, ctx.bot.raw().guid);
}

// ---- Battle CDs (mana economy + raid burst) ----
bool ShouldManaTea(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(MANA_TEA)) return false;
    if (!ctx.bot.is_ready(MANA_TEA)) return false;
    if (ctx.bot.has_aura(MANA_TEA_AURA)) return false;
    // Pop when our mana is hurting OR a wave of raid damage is incoming.
    if (ctx.bot.max_power(0) > 0 && ctx.bot.power_pct(0) <= 70) return true;
    return WoundedFriendCount(ctx, 70) >= 3;
}
void DoManaTea(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(MANA_TEA); }

bool ShouldThunderFocusTea(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(THUNDER_FOCUS_TEA)) return false;
    if (!ctx.bot.is_ready(THUNDER_FOCUS_TEA)) return false;
    // Empower the next big spell — pair with Renewing Mist (free spread) or
    // Vivify (instant). Gate on a real heal target.
    return LowestFriendOrSelf(ctx).hp_pct <= 80;
}
void DoThunderFocusTea(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(THUNDER_FOCUS_TEA); }

bool ShouldInvokeYulon(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(INVOKE_YULON)) return false;
    if (!ctx.bot.is_ready(INVOKE_YULON)) return false;
    return WoundedFriendCount(ctx, 70) >= 3;
}
void DoInvokeYulon(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(INVOKE_YULON); }

bool ShouldInvokeChiJi(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(INVOKE_CHI_JI)) return false;
    if (!ctx.bot.is_ready(INVOKE_CHI_JI)) return false;
    return WoundedFriendCount(ctx, 75) >= 3;
}
void DoInvokeChiJi(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(INVOKE_CHI_JI); }

bool ShouldInvokeXuen(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(INVOKE_XUEN)) return false;
    if (!ctx.bot.is_ready(INVOKE_XUEN)) return false;
    if (ctx.bot.victim().IsEmpty()) return false;
    // Offensive talent — only when group is mostly healthy.
    return GroupTopped(ctx);
}
void DoInvokeXuen(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(INVOKE_XUEN); }

// ---- Emergency layer ----
bool ShouldLifeCocoon(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(LIFE_COCOON)) return false;
    if (!ctx.bot.is_ready(LIFE_COCOON)) return false;
    HealTarget t = LowestFriendOrSelf(ctx);
    if (t.hp_pct > 30) return false;
    return !ctx.bot.has_aura(LIFE_COCOON_AURA, t.guid);
}
void DoLifeCocoon(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LIFE_COCOON, LowestFriendOrSelf(ctx).guid);
}

bool ShouldRevival(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(REVIVAL)) return false;
    if (!ctx.bot.is_ready(REVIVAL)) return false;
    return WoundedFriendCount(ctx, 50) >= 3;
}
void DoRevival(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(REVIVAL); }

bool ShouldRestoral(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(RESTORAL)) return false;
    if (!ctx.bot.is_ready(RESTORAL)) return false;
    return WoundedFriendCount(ctx, 55) >= 3;
}
void DoRestoral(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(RESTORAL); }

// ---- AoE blanket ----
bool ShouldEssenceFont(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(ESSENCE_FONT)) return false;
    if (!ctx.bot.is_ready(ESSENCE_FONT)) return false;
    return WoundedFriendCount(ctx, 80) >= 3;
}
void DoEssenceFont(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ESSENCE_FONT, LowestFriendOrSelf(ctx).guid);
}

bool ShouldSheilunsGift(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SHEILUNS_GIFT)) return false;
    if (!ctx.bot.is_ready(SHEILUNS_GIFT)) return false;
    return WoundedFriendCount(ctx, 75) >= 3;
}
void DoSheilunsGift(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHEILUNS_GIFT, LowestFriendOrSelf(ctx).guid);
}

bool ShouldRefreshingJadeWind(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(REFRESHING_JADE_WIND)) return false;
    if (!ctx.bot.is_ready(REFRESHING_JADE_WIND)) return false;
    return WoundedFriendCount(ctx, 85) >= 3;
}
void DoRefreshingJadeWind(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(REFRESHING_JADE_WIND); }

bool ShouldFaelineStomp(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(FAELINE_STOMP)) return false;
    if (!ctx.bot.is_ready(FAELINE_STOMP)) return false;
    return WoundedFriendCount(ctx, 85) >= 2;
}
void DoFaelineStomp(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FAELINE_STOMP); }

bool ShouldChiBurst(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(CHI_BURST)) return false;
    if (!ctx.bot.is_ready(CHI_BURST)) return false;
    return WoundedFriendCount(ctx, 80) >= 2;
}
void DoChiBurst(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CHI_BURST); }

bool ShouldChiWave(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(CHI_WAVE)) return false;
    if (!ctx.bot.is_ready(CHI_WAVE)) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 85;
}
void DoChiWave(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CHI_WAVE, LowestFriendOrSelf(ctx).guid);
}

// ---- HoT maintenance ----
bool ShouldRenewingMist(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(RENEWING_MIST)) return false;
    if (!ctx.bot.is_ready(RENEWING_MIST)) return false;
    HealTarget t = LowestFriendOrSelf(ctx);
    if (t.hp_pct >= 95) return false;
    AuraEntry const* a = ctx.bot.find_aura(RENEWING_MIST_AURA, t.guid);
    return !a || a->remaining.count() <= 4000;
}
void DoRenewingMist(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RENEWING_MIST, LowestFriendOrSelf(ctx).guid);
}

// ---- Spike heal ----
bool ShouldEnvelopingMist(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(ENVELOPING_MIST)) return false;
    HealTarget t = LowestFriendOrSelf(ctx);
    if (t.hp_pct > 60) return false;
    AuraEntry const* a = ctx.bot.find_aura(ENVELOPING_MIST_AURA, t.guid);
    return !a || a->remaining.count() <= 2000;
}
void DoEnvelopingMist(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ENVELOPING_MIST, LowestFriendOrSelf(ctx).guid);
}

bool ShouldVivify(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(VIVIFY)) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 80;
}
void DoVivify(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(VIVIFY, LowestFriendOrSelf(ctx).guid);
}

// ---- Filler ----
bool ShouldSoothingMist(ApPredicateContext const& ctx)
{
    if (!ctx.bot.is_ready(SOOTHING_MIST)) return false;
    // IN COMBAT the 8s self-channel is a pacifist trap for a SOLO MW (audit
    // B22): below 95% it re-channeled on itself indefinitely while the mob
    // kept hitting (is_casting skips the whole APL), so a scratched solo
    // Mistweaver could literally never finish a kill. In combat, channel
    // only on a genuinely hurt target (<=60%); out of combat the relaxed
    // 95% top-up stays.
    const int gate = ctx.bot.in_combat() ? 60 : 95;
    return LowestFriendOrSelf(ctx).hp_pct <= gate;
}
void DoSoothingMist(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SOOTHING_MIST, LowestFriendOrSelf(ctx).guid);
}

// ---- Offensive filler (group topped) ----
bool ShouldRisingSunKickFiller(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!GroupTopped(ctx)) return false;
    if (!ctx.bot.knows_spell(RISING_SUN_KICK)) return false;
    if (!ctx.bot.is_ready(RISING_SUN_KICK)) return false;
    return !ctx.bot.victim().IsEmpty();
}
void DoRisingSunKickFiller(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RISING_SUN_KICK, ctx.bot.victim());
}

bool ShouldBlackoutKickFiller(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!GroupTopped(ctx)) return false;
    if (!ctx.bot.knows_spell(BLACKOUT_KICK)) return false;
    if (!ctx.bot.is_ready(BLACKOUT_KICK)) return false;
    return !ctx.bot.victim().IsEmpty();
}
void DoBlackoutKickFiller(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BLACKOUT_KICK, ctx.bot.victim());
}

bool ShouldTigerPalmFiller(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!GroupTopped(ctx)) return false;
    if (!ctx.bot.knows_spell(TIGER_PALM)) return false;
    return !ctx.bot.victim().IsEmpty();
}
void DoTigerPalmFiller(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(TIGER_PALM, ctx.bot.victim());
}

bool AlwaysAlive(ApPredicateContext const& ctx) { return ctx.bot.is_alive(); }
void DoNothing(ApPredicateContext const&, BotIntentEmitter&) {}

// Cast-swap shim — Mistweaver slow heals. Soothing Mist channeled,
// Enveloping Mist 2s cast, Vivify 1.5s. See ApHealHelpers.h.
bool ShouldCancelHealForSwap(ApPredicateContext const& ctx)
{
    return ShouldCancelHealForSwapImpl(ctx,
        { SOOTHING_MIST, ENVELOPING_MIST, VIVIFY });
}

// ---- Rule table (canonical Mistweaver priority order) ----
//   Cancel-heal swap (drop current cast if a better heal target appeared) ->
//   Battle rez (Reawaken in combat / Resuscitate OOC) ->
//   Raid emergency (Revival / Restoral) ->
//   Ally panic (Life Cocoon) ->
//   Self survival (Fortifying Brew / Diffuse Magic / Zen Meditation / Dampen Harm) ->
//   Interrupts + CC (Spear Hand / Paralysis / Ring of Peace / Leg Sweep) ->
//   Dispel (Detox) ->
//   Mana / setup (Mana Tea / Thunder Focus Tea) ->
//   Major CDs (Invoke Yu'lon / Chi-Ji) ->
//   Big spike (Enveloping Mist) -> HoT spread (Renewing Mist) ->
//   Spam single-target (Vivify) ->
//   Raid heal (Essence Font / Sheilun's Gift / RJW / Jadefire Stomp / Chi Burst / Chi Wave) ->
//   Offensive filler when group is topped (Xuen / RSK / BoK / TP) ->
//   Soothing Mist channel filler -> idle.
ApRule const kRules[] = {
    { ShouldCancelHealForSwap,   DoCancelHealForSwap,   "Cancel heal — swap to lower target" },
    { ShouldReawaken,            DoReawaken,            "Reawaken (in-combat battle rez)" },
    { ShouldResuscitate,         DoResuscitate,         "Resuscitate (OOC rez)"          },
    { ShouldRevival,             DoRevival,             "Revival (3+ at <=50%)"          },
    { ShouldRestoral,            DoRestoral,            "Restoral (3+ at <=55%)"         },
    { ShouldLifeCocoon,          DoLifeCocoon,          "Life Cocoon (<=30%)"            },
    { ShouldFortifyingBrew,      DoFortifyingBrew,      "Fortifying Brew (<=35%)"        },
    { ShouldDiffuseMagic,        DoDiffuseMagic,        "Diffuse Magic (<=60% caster)"   },
    { ShouldZenMeditation,       DoZenMeditation,       "Zen Meditation (<=50% caster)"  },
    { ShouldDampenHarm,          DoDampenHarm,          "Dampen Harm (<=60%)"            },
    { ShouldSpearHandStrike,     DoSpearHandStrike,     "Spear Hand Strike (interrupt)"  },
    { ShouldParalysisOffTarget,  DoParalysisOffTarget,  "Paralysis (off-target caster)"  },
    { ShouldRingOfPeace,         DoRingOfPeace,         "Ring of Peace (panic peel)"     },
    { ShouldLegSweep,            DoLegSweep,            "Leg Sweep (3+ AoE stun)"        },
    { ShouldDetox,               DoDetox,               "Detox (dispel)"                 },
    { ShouldManaTea,             DoManaTea,             "Mana Tea (mana economy)"        },
    { ShouldThunderFocusTea,     DoThunderFocusTea,     "Thunder Focus Tea (empower)"    },
    { ShouldInvokeYulon,         DoInvokeYulon,         "Invoke Yu'lon (3+ at 70%)"      },
    { ShouldInvokeChiJi,         DoInvokeChiJi,         "Invoke Chi-Ji (3+ at 75%)"      },
    { ShouldEnvelopingMist,      DoEnvelopingMist,      "Enveloping Mist (<=60%)"        },
    { ShouldRenewingMist,        DoRenewingMist,        "Renewing Mist (HoT refresh)"    },
    { ShouldVivify,              DoVivify,              "Vivify (<=80%)"                 },
    { ShouldEssenceFont,         DoEssenceFont,         "Essence Font (3+ at 80%)"       },
    { ShouldSheilunsGift,        DoSheilunsGift,        "Sheilun's Gift (3+ at 75%)"     },
    { ShouldRefreshingJadeWind,  DoRefreshingJadeWind,  "Refreshing Jade Wind (3+ 85%)"  },
    { ShouldFaelineStomp,        DoFaelineStomp,        "Jadefire Stomp (2+ at 85%)"     },
    { ShouldChiBurst,            DoChiBurst,            "Chi Burst (2+ at 80%)"          },
    { ShouldChiWave,             DoChiWave,             "Chi Wave (<=85%)"               },
    { ShouldInvokeXuen,          DoInvokeXuen,          "Invoke Xuen (DPS, group full)"  },
    { ShouldRisingSunKickFiller, DoRisingSunKickFiller, "Rising Sun Kick (filler)"       },
    { ShouldBlackoutKickFiller,  DoBlackoutKickFiller,  "Blackout Kick (filler)"         },
    { ShouldTigerPalmFiller,     DoTigerPalmFiller,     "Tiger Palm (filler)"            },
    { ShouldSoothingMist,        DoSoothingMist,        "Soothing Mist (channel filler)" },
    { AlwaysAlive,               DoNothing,             "Idle"                           },
};

} // anonymous

void RegisterApl_Monk_Mistweaver()
{
    constexpr uint32 SPEC_MONK_MISTWEAVER = 270;
    RegisterRotation(CLASS_MONK, SPEC_MONK_MISTWEAVER, ApRotation{kRules});
}

} // namespace Playerbot::Combat
