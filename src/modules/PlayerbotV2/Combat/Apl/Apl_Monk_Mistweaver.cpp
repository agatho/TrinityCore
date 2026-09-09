// Mistweaver Monk - WoW 12.1.0.69587 (Midnight) enterprise rotation. Hybrid
// mist healer that stays in caster style (no fistweaving here - the action
// queue would need a melee/caster mode pivot we don't yet have). Decision tree:
//
//   1) Battle rez / OOC rez:  Reawaken (combat) / Resuscitate (OOC)
//   2) Emergency layer:       Life Cocoon, Fortifying Brew, Tiger's Lust
//                             (Dampen Harm / Diffuse Magic / Zen Meditation
//                             are gone from the 12.1 kit)
//   3) Dispel:                Detox (Magic + Poison/Disease with Improved Detox)
//   4) CC:                    Paralysis off-target caster, Leg Sweep, Ring
//                             of Peace (Spear Hand Strike is WW/BrM-only now)
//   5) Battle CDs:            Mana Tea, Thunder Focus Tea, Invoke Yu'lon
//                             / Invoke Chi-Ji, Celestial Conduit, Restoral,
//                             Revival
//   6) Spike heal:            Enveloping Mist (<=60%), Renewing Mist spread,
//                             Vivify or Sheilun's Gift (override pair, <=80%)
//   7) Filler / mana floor:   Soothing Mist channel
//   8) Offensive (group full): Rushing Wind Kick or Rising Sun Kick (override
//                              pair), Blackout Kick, Tiger Palm

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
// Validated spell IDs (WoW 12.1.0.69587):
//   115175 Soothing Mist            124682 Enveloping Mist        115151 Renewing Mist
//   116670 Vivify                   399491 Sheilun's Gift         116849 Life Cocoon
//   115310 Revival                  388615 Restoral               322118 Invoke Yu'lon
//   325197 Invoke Chi-Ji            443028 Celestial Conduit      115294 Mana Tea (cast)
//   115869 Mana Tea (talent)        116680 Thunder Focus Tea      115203 Fortifying Brew (cast)
//   388917 Fortifying Brew (talent) 115450 Detox (MW)             115078 Paralysis
//   119381 Leg Sweep                116844 Ring of Peace          116841 Tiger's Lust
//   115178 Resuscitate (OOC rez)    212051 Reawaken (combat rez)  100780 Tiger Palm
//   100784 Blackout Kick            107428 Rising Sun Kick        467307 Rushing Wind Kick
//   Aura-only: 119611 Renewing Mist HoT | 115867 Mana Tea stacks
//
// Skipped (with reason):
//   388917 Fortifying Brew / 115869 Mana Tea (talents)
//                                      passive trait spells; TC learns them but
//                                      never their taught casts (115203 / 115294),
//                                      so the rules gate on EITHER id and cast
//                                      the active one.
//   1243287 Diffuse Magic             12.1 passive rider on Fortifying Brew.
//   122278 Dampen Harm / 115176 Zen Meditation / 196725 Refreshing Jade Wind /
//   388193 Jadefire Stomp / 191837 Essence Font
//                                      removed from the 12.1 Monk kit (Essence
//                                      Font no longer exists in SpellName).
//   116705 Spear Hand Strike          Windwalker/Brewmaster-only in 12.1.
//   123986 Chi Burst / 123904 Invoke Xuen
//                                      Brewmaster-only / Windwalker-only in 12.1.
//   450391 Chi Wave / 446326 Zen Pulse passives in 12.1 (ride on RSK / Vivify).
//   116645 Teachings of the Monastery / 274909 Rising Mist / 388812 Vivacious
//   Vivification                      passives; not castable.
//   115313 Summon Jade Serpent Statue not in the curated builds; placement.
//   1229376 Single-Button Assistant   client convenience macro.
constexpr uint32 SOOTHING_MIST          = 115175;
constexpr uint32 ENVELOPING_MIST        = 124682;
constexpr uint32 RENEWING_MIST          = 115151;
constexpr uint32 VIVIFY                 = 116670;
constexpr uint32 SHEILUNS_GIFT          = 399491;       // [M] talent - OVERRIDES Vivify (no CD in 12.1)
constexpr uint32 LIFE_COCOON            = 116849;
constexpr uint32 REVIVAL                = 115310;       // [M] pick of the Revival/Restoral choice node
constexpr uint32 RESTORAL               = 388615;       // [R] pick - Revival without the Magic cleanse
constexpr uint32 INVOKE_YULON           = 322118;       // [R] jade serpent burst
constexpr uint32 INVOKE_CHI_JI          = 325197;       // [M] crane spirit
constexpr uint32 CELESTIAL_CONDUIT      = 443028;       // [R][M] hero active - 4s AoE heal/dmg channel
constexpr uint32 MANA_TEA               = 115294;       // cast id (channel that drinks stacks)
constexpr uint32 MANA_TEA_TALENT        = 115869;       // learned trait spell - knows_spell gate
constexpr uint32 MANA_TEA_STACKS        = 115867;       // stack aura built by spending mana
constexpr uint32 THUNDER_FOCUS_TEA      = 116680;

// Survival
constexpr uint32 FORTIFYING_BREW        = 115203;       // cast id (VisibleSpellID of the talent)
constexpr uint32 FORTIFYING_BREW_TALENT = 388917;       // learned trait spell - knows_spell gate

// Utility / CC
constexpr uint32 DETOX                  = 115450;
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
constexpr uint32 RUSHING_WIND_KICK      = 467307;       // [R] talent - OVERRIDES Rising Sun Kick, heals HoT targets

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
    // "my own HP <= below_pct" -the 92% GroupTopped gates then froze ALL
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

// ---- CC ----
// Mistweaver has no interrupt in 12.1 (Spear Hand Strike is WW/BrM-only);
// Paralysis on an off-target caster is the closest substitute.
bool ShouldParalysisOffTarget(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(PARALYSIS)) return false;
    if (!ctx.bot.is_ready(PARALYSIS)) return false;
    auto const* c = ctx.bot.interruptible_caster();
    return c && c->guid != ctx.bot.victim();
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
// Fortifying Brew: TC learns the trait spell 388917, never its VisibleSpellID
// 115203 (the actual cast). Accept either id as proof the talent is known.
bool ShouldFortifyingBrew(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(FORTIFYING_BREW_TALENT) && !ctx.bot.knows_spell(FORTIFYING_BREW)) return false;
    if (!ctx.bot.is_ready(FORTIFYING_BREW)) return false;
    // Only big self CD left in 12.1 - fire a little earlier than before.
    return ctx.bot.hp_pct() <= 40;
}
void DoFortifyingBrew(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FORTIFYING_BREW); }

bool ShouldTigersLust(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(TIGERS_LUST)) return false;
    if (!ctx.bot.is_ready(TIGERS_LUST)) return false;
    // Self root/snare break so the healer can reposition.
    return ctx.bot.has_mechanic(MECHANIC_ROOT) || ctx.bot.has_mechanic(MECHANIC_SNARE);
}
void DoTigersLust(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(TIGERS_LUST, ctx.bot.raw().guid);
}

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
// Mana Tea (12.1): the talent 115869 is a passive that banks a stack per
// mana spent; the cast 115294 channels those stacks back into mana. TC
// learns only the talent, so gate on either id and cast the channel.
bool ShouldManaTea(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(MANA_TEA_TALENT) && !ctx.bot.knows_spell(MANA_TEA)) return false;
    if (!ctx.bot.is_ready(MANA_TEA)) return false;
    // Channeling with no stacks restores nothing - need a real bank first.
    if (ctx.bot.aura_stacks(MANA_TEA_STACKS) < 5) return false;
    // Drink when mana is hurting and nobody is about to die (it is a channel).
    if (ctx.bot.max_power(0) <= 0 || ctx.bot.power_pct(0) > 60) return false;
    return WoundedFriendCount(ctx, 50) == 0;
}
void DoManaTea(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(MANA_TEA); }

bool ShouldThunderFocusTea(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(THUNDER_FOCUS_TEA)) return false;
    if (!ctx.bot.is_ready(THUNDER_FOCUS_TEA)) return false;
    // Empower the next big spell -pair with Renewing Mist (free spread) or
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

bool ShouldCelestialConduit(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(CELESTIAL_CONDUIT)) return false;
    if (!ctx.bot.is_ready(CELESTIAL_CONDUIT)) return false;
    // 4s channel radiating heals onto injured allies - a raid-damage answer.
    return WoundedFriendCount(ctx, 70) >= 3;
}
void DoCelestialConduit(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CELESTIAL_CONDUIT); }

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

// Sheilun's Gift (talent) OVERRIDES Vivify in 12.1: same slot, no cooldown,
// heals the target plus nearby allies scaled by banked mist clouds. Cast it
// wherever Vivify would have gone; Vivify stays for non-talented bots.
bool ShouldSheilunsGift(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SHEILUNS_GIFT)) return false;
    if (!ctx.bot.is_ready(SHEILUNS_GIFT)) return false;
    return LowestFriendOrSelf(ctx).hp_pct <= 80;
}
void DoSheilunsGift(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHEILUNS_GIFT, LowestFriendOrSelf(ctx).guid);
}

bool ShouldVivify(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(SHEILUNS_GIFT)) return false;
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
// Rushing Wind Kick (talent) OVERRIDES Rising Sun Kick: frontal cone that
// also heals allies carrying our HoTs. Prefer it when known, else RSK.
bool ShouldRushingWindKickFiller(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!GroupTopped(ctx)) return false;
    if (!ctx.bot.knows_spell(RUSHING_WIND_KICK)) return false;
    if (!ctx.bot.is_ready(RUSHING_WIND_KICK)) return false;
    return !ctx.bot.victim().IsEmpty();
}
void DoRushingWindKickFiller(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RUSHING_WIND_KICK, ctx.bot.victim());
}

bool ShouldRisingSunKickFiller(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!GroupTopped(ctx)) return false;
    if (ctx.bot.knows_spell(RUSHING_WIND_KICK)) return false;
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

// Cast-swap shim - Mistweaver slow heals. Soothing Mist channeled,
// Enveloping Mist 2s cast, Vivify 1.5s / Sheilun's Gift 2s. See ApHealHelpers.h.
bool ShouldCancelHealForSwap(ApPredicateContext const& ctx)
{
    return ShouldCancelHealForSwapImpl(ctx,
        { SOOTHING_MIST, ENVELOPING_MIST, VIVIFY, SHEILUNS_GIFT });
}

// ---- Rule table (canonical Mistweaver priority order, 12.1) ----
//   Cancel-heal swap (drop current cast if a better heal target appeared) ->
//   Battle rez (Reawaken in combat / Resuscitate OOC) ->
//   Raid emergency (Revival / Restoral) ->
//   Ally panic (Life Cocoon) ->
//   Self survival (Fortifying Brew / Tiger's Lust root break) ->
//   CC (Paralysis / Ring of Peace / Leg Sweep) ->
//   Dispel (Detox) ->
//   Mana / setup (Mana Tea / Thunder Focus Tea) ->
//   Major CDs (Invoke Yu'lon / Chi-Ji / Celestial Conduit) ->
//   Big spike (Enveloping Mist) -> HoT spread (Renewing Mist) ->
//   Spam single-target (Sheilun's Gift or Vivify - override pair) ->
//   Offensive filler when group is topped (RWK or RSK / BoK / TP) ->
//   Soothing Mist channel filler -> idle.
ApRule const kRules[] = {
    { ShouldCancelHealForSwap,   DoCancelHealForSwap,   "Cancel heal - swap to lower target" },
    { ShouldReawaken,            DoReawaken,            "Reawaken (in-combat battle rez)" },
    { ShouldResuscitate,         DoResuscitate,         "Resuscitate (OOC rez)"          },
    { ShouldRevival,             DoRevival,             "Revival (3+ at <=50%)"          },
    { ShouldRestoral,            DoRestoral,            "Restoral (3+ at <=55%)"         },
    { ShouldLifeCocoon,          DoLifeCocoon,          "Life Cocoon (<=30%)"            },
    { ShouldFortifyingBrew,      DoFortifyingBrew,      "Fortifying Brew (<=40%)"        },
    { ShouldTigersLust,          DoTigersLust,          "Tiger's Lust (root break)"      },
    { ShouldParalysisOffTarget,  DoParalysisOffTarget,  "Paralysis (off-target caster)"  },
    { ShouldRingOfPeace,         DoRingOfPeace,         "Ring of Peace (panic peel)"     },
    { ShouldLegSweep,            DoLegSweep,            "Leg Sweep (3+ AoE stun)"        },
    { ShouldDetox,               DoDetox,               "Detox (dispel)"                 },
    { ShouldManaTea,             DoManaTea,             "Mana Tea (drink stacks)"        },
    { ShouldThunderFocusTea,     DoThunderFocusTea,     "Thunder Focus Tea (empower)"    },
    { ShouldInvokeYulon,         DoInvokeYulon,         "Invoke Yu'lon (3+ at 70%)"      },
    { ShouldInvokeChiJi,         DoInvokeChiJi,         "Invoke Chi-Ji (3+ at 75%)"      },
    { ShouldCelestialConduit,    DoCelestialConduit,    "Celestial Conduit (3+ at 70%)"  },
    { ShouldEnvelopingMist,      DoEnvelopingMist,      "Enveloping Mist (<=60%)"        },
    { ShouldRenewingMist,        DoRenewingMist,        "Renewing Mist (HoT refresh)"    },
    { ShouldSheilunsGift,        DoSheilunsGift,        "Sheilun's Gift (<=80%)"         },
    { ShouldVivify,              DoVivify,              "Vivify (<=80%)"                 },
    { ShouldRushingWindKickFiller,DoRushingWindKickFiller,"Rushing Wind Kick (filler)"   },
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
