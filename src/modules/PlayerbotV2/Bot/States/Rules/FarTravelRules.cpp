// FarTravelRules - far same-map travel. Adds idle:far_same_map_travel @697,
// the dedicated rule for driving a bot toward a SAME-MAP objective POI that is
// too far for the pursue_quest_goal routable band (>2500y). pursue_quest_goal
// (@698) caps at 2500y and DEFERS larger goals "to the flight/taxi cascade",
// but that cascade lived inline in AutoactDispatch at the starved autoact@50
// level — so a bot with a 2500y+ same-map goal never proactively walked to a
// flight master nor chunk-walked toward the goal, and wedged (the long-distance
// pathfind area noted in the fleet-behavior memos: Durnan far-turn-in, etc.).
//
// This rule sits at 697 (just below pursue_quest_goal@698, above the OOC
// maintenance band 660-696) so questing toward a near goal still wins, but a
// far same-map goal now drives travel from the top-of-tick registry pass. It
// reuses the SAME extracted travel executors as AutoactDispatch
// (::Playerbot::States::DriveRecommendedTaxi / DriveTravelPlanTo) so it composes
// the identical proactive-flight / chunk-walk behavior — no duplicated logic.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/States/MaintainHelpers.h"
#include <cmath>

namespace Playerbot {

namespace {

using namespace ::Playerbot::States;

// ---------- idle:far_same_map_travel ----------
// Threshold mirrors pursue_quest_goal's upper band edge (2500y). Below this the
// pursue rule owns the goal with full-path routing; at/above it the goal is
// continent-scale and this rule drives the flight/taxi + chunk-walk approach.
constexpr float kFarTravelThreshold = 2500.0f;

bool FarSameMapTravelGate(BotSnapshotView const& s, BotAI& ai,
                          GroupSnapshotView const&, uint32 /*now_ms*/)
{
    // Mirror the pursue_quest_goal / walk_to_known_hub exclusions: no scripted
    // content, no profession detours, heal before a long trek.
    if (s.in_combat() || s.is_casting()) return false;
    if (s.is_in_dungeon() || s.in_battleground()) return false;
    if (ai.in_profession_mode(s.published_at_ms())) return false;
    // HP<50%: heal first (same gate as pursue_quest_goal — a hurt bot should not
    // set off across open terrain to a far goal unhealed).
    {
        auto const& v = s.raw().vitals;
        if (v.max_hp > 0 && uint64(v.hp) * 100u < uint64(v.max_hp) * 50u)
            return false;
    }
    // Yield to a nearby acceptable offer (giver in scan) or any pending turn-in —
    // accept/turn-in own those; trekking to a far goal must not preempt a quest
    // hand-in the bot could complete right here (mirrors pursue_quest_goal).
    // EXCEPTION (Durnan wedge, 2026-06-21): if walk_to_known_hub@700 has declared
    // its offer close-approach FUTILE (hub_offer_backoff active — the bot is
    // wedged and physically cannot reach the accept margin), those in-scan offers
    // are confirmed un-grabable for now. Yielding to them here too would deadlock
    // far-travel on the SAME phantom offers that wedged the hub rule, so the bot
    // never travels (Durnan L15: 4 in-scan offers it can't accept, every travel
    // rule yielding to them, position+XP frozen). During the back-off, ignore the
    // offers and proceed with travel; the bot retries the offers after the cooldown.
    if (s.published_at_ms() >= ai.hub_offer_backoff_until())
        for (auto const& off : s.raw().quest_discovery.quest_offers)
        {
            bool giver_inscan = false;
            for (auto const& u : s.raw().combat.nearby_friends)
                if (u.guid == off.giver) { giver_inscan = true; break; }
            if (!giver_inscan)
                for (auto const& o : s.raw().world_objects.nearby_objects)
                    if (o.guid == off.giver) { giver_inscan = true; break; }
            if (giver_inscan) return false;
        }
    if (!s.raw().quest_discovery.quest_turnins.empty()) return false;
    // Same-map POI only — cross-map goals stay with the AutoactDispatch
    // portal/ship relocation cascade (driveTravelPlanTo handles to_map != map_id).
    auto const& poi = s.current_objective_poi();
    if (!poi.valid || poi.map_id != s.map_id()) return false;
    float bx, by, bz; s.position(bx, by, bz);
    const float dx = poi.x - bx, dy = poi.y - by;
    const float d2 = dx * dx + dy * dy;
    return d2 > (kFarTravelThreshold * kFarTravelThreshold);
}

bool FarSameMapTravelFire(BotSnapshotView const& s, BotAI& ai,
                          GroupSnapshotView const&,
                          BotIntentEmitter& emit, uint32 now_ms)
{
    float bx, by, bz; s.position(bx, by, bz);

    // (A) Proactive flight: if the builder published a recommended taxi route for
    // a far same-map goal, fly it (walk to the FM, dismount, fly_to_node). Reuses
    // the exact extracted cascade so behavior matches AutoactDispatch. When it
    // falls through (no taxi route / no proactive start), drop to the chunk walk.
    if (s.has_recommended_taxi_route())
    {
        bool ft = false;
        if (::Playerbot::States::DriveRecommendedTaxi(s, ai, emit, bx, by, bz, &ft))
            return true;
        if (!ft)
            return false;   // wedged/yield — consume tick (the taxi cascade decided)
        // ft == true: no proactive start position — fall through to the walk.
    }

    // (B) No taxi route (or it yielded a fall-through): chunk-walk toward the POI.
    // Shared ChunkedWalkToward steps in bounded chunks (a single far move_to
    // NoPaths on unloaded tiles), deflects on blocks, ESCALATES the deflection on
    // a position stall to escape ledges/corners, and yields when genuinely stuck.
    auto const& poi = s.current_objective_poi();
    return ::Playerbot::States::ChunkedWalkToward(s, ai, emit, poi.x, poi.y, bz,
                                                  "idle:far_same_map_travel", now_ms);
}

} // anonymous namespace

void RegisterFarTravelRules(IdleRuleRegistry& r)
{
    {
        IdleRule rule;
        rule.name     = "idle:far_same_map_travel";
        // 697: just BELOW idle:pursue_quest_goal (698, the routable 80-2500y
        // band) and ABOVE the OOC maintenance band (660-696). A near goal stays
        // with pursue; a far same-map goal (>2500y) drives flight/taxi + chunk
        // walk from the top-of-tick pass instead of the starved autoact@50 path.
        rule.priority = 697;
        rule.gate     = &FarSameMapTravelGate;
        rule.fire     = &FarSameMapTravelFire;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot
