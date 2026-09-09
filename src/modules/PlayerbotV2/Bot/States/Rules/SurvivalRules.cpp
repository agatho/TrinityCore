// SurvivalRules - Refactor #3 pass 8. Migrates the three survival
// hazard rules (`idle:flee_hazard`, `idle:flee_damaging_liquid`,
// `idle:surface_to_breathe`) out of the State_Idle linear cascade.
// All three sit at the top of dispatch priority â€” a bot in lava or
// drowning is more urgent than any utility rule.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"

#include <cmath>

namespace Playerbot {

namespace {

// ---------- idle:flee_hazard ----------
bool FleeHazardGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&,uint32)
{
    float bx, by, bz; s.position(bx, by, bz);
    for (auto const& obj : s.raw().world_objects.nearby_objects)
    {
        if (!obj.is_hazard || obj.hazard_radius <= 0.0f) continue;
        const float dx = bx - obj.x;
        const float dy = by - obj.y;
        if (dx*dx + dy*dy < obj.hazard_radius * obj.hazard_radius)
            return true;
    }
    return false;
}

bool FleeHazardFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32)
{
    float bx, by, bz; s.position(bx, by, bz);
    BotSnapshot::NearbyObject const* worst = nullptr;
    float worst_overlap = 0.0f;
    for (auto const& obj : s.raw().world_objects.nearby_objects)
    {
        if (!obj.is_hazard || obj.hazard_radius <= 0.0f) continue;
        const float dx = bx - obj.x;
        const float dy = by - obj.y;
        const float d2 = dx*dx + dy*dy;
        if (d2 >= obj.hazard_radius * obj.hazard_radius) continue;
        const float overlap = obj.hazard_radius - std::sqrt(d2);
        if (overlap > worst_overlap) { worst_overlap = overlap; worst = &obj; }
    }
    if (!worst) return false;
    const float dx = bx - worst->x;
    const float dy = by - worst->y;
    const float d  = std::max(0.5f, std::sqrt(dx*dx + dy*dy));
    const float scale = (worst->hazard_radius + 5.0f) / d;
    emit.move_to(worst->x + dx * scale, worst->y + dy * scale, bz);
    ai.set_last_rule_fired("idle:flee_hazard");
    return true;
}

// ---------- idle:flee_damaging_liquid ----------
bool FleeDamagingLiquidGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&,uint32)
{
    return s.is_in_damaging_liquid();
}

bool FleeDamagingLiquidFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32)
{
    float bx, by, bz; s.position(bx, by, bz);
    const uint32 epoch = s.published_at_ms() / 1000u;
    const uint64 mixed = (uint64(s.bot_id()) * 0x9E3779B97F4A7C15ULL)
                       ^ (uint64(epoch) * 0xBF58476D1CE4E5B9ULL);
    const float angle = (float(mixed & 0xFFFFu) / 65536.0f) * 6.2831853f;
    constexpr float kFleeStep = 25.0f;
    emit.move_to(bx + std::cos(angle) * kFleeStep,
                 by + std::sin(angle) * kFleeStep,
                 bz, /*run*/ true);
    ai.set_last_rule_fired("idle:flee_damaging_liquid");
    return true;
}

// ---------- idle:surface_to_breathe ----------
bool SurfaceToBreatheGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&,uint32)
{
    if (!s.is_underwater() || s.water_surface_z() == 0.0f) return false;
    float bx, by, bz; s.position(bx, by, bz);
    return bz < s.water_surface_z() - 0.5f;
}

bool SurfaceToBreatheFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32)
{
    float bx, by, bz; s.position(bx, by, bz);
    emit.move_to(bx, by, s.water_surface_z() + 0.5f, /*run*/ true);
    ai.set_last_rule_fired("idle:surface_to_breathe");
    return true;
}

// ---------- idle:res_sick_wait ----------
// Resurrection Sickness (spell 15007, applied by a spirit-healer rez) cripples
// the bot (~-75% stats). A real player NEVER quests, grinds, or pulls while sick
// — they wait it out where it's safe (the graveyard) or do a quick local vendor/
// repair run. Without this, a freshly spirit-rezzed bot immediately walks back
// into a fight it now CANNOT win -> dies -> spirit-rezzes -> sick again: the
// resurrection-sickness death spiral (observed: Morthan, L9 warlock, XP frozen).
//
// Priority 709 sits BELOW critical_repair (735) and bags_full_recover (710) — so a
// local repair/vendor run is still allowed while sick — but ABOVE the quest funnel
// (698-705), grind-engage, wander and travel rules, so a sick bot otherwise just
// holds position (usually safe at the graveyard) until the aura expires, then
// resumes normally. Combat is reactive (FSM routes attacks to the combat state),
// so this can't prevent a mob that aggros the waiting bot — but it stops the bot
// from SEEKING fights, which is what sustains the spiral.
bool ResSickWaitGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    return s.is_alive() && s.has_aura(15007 /*Resurrection Sickness*/);
}

bool ResSickWaitFire(BotSnapshotView const&, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter&, uint32)
{
    // Consume the tick and do nothing — wait out the sickness. Higher-priority
    // repair/vendor rules still run; everything below is suppressed.
    ai.set_last_rule_fired("idle:res_sick_wait");
    return true;
}

} // anonymous namespace

void RegisterSurvivalRules(IdleRuleRegistry& r)
{
    // Hazards are the highest-priority idle rules â€” survival precedes
    // every utility behavior. flee_damaging_liquid out-ranks surface_
    // to_breathe (lava > drowning), flee_hazard slightly below those.
    {
        IdleRule rule;
        rule.name     = "idle:flee_damaging_liquid";
        rule.priority = 900;
        rule.gate     = &FleeDamagingLiquidGate;
        rule.fire     = &FleeDamagingLiquidFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:surface_to_breathe";
        rule.priority = 890;
        rule.gate     = &SurfaceToBreatheGate;
        rule.fire     = &SurfaceToBreatheFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:flee_hazard";
        rule.priority = 880;
        rule.gate     = &FleeHazardGate;
        rule.fire     = &FleeHazardFire;
        r.register_rule(std::move(rule));
    }
    {
        // Wait out Resurrection Sickness. 709: below critical_repair(735) /
        // bags_full_recover(710) so a local repair/vendor run is still allowed,
        // above the quest funnel / grind-engage / travel so the bot doesn't seek
        // fights while crippled (the res-sickness death spiral).
        IdleRule rule;
        rule.name     = "idle:res_sick_wait";
        rule.priority = 709;
        rule.gate     = &ResSickWaitGate;
        rule.fire     = &ResSickWaitFire;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot
