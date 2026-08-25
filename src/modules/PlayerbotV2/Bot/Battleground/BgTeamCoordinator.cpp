// BgTeamCoordinator implementation. See header for the design contract.
//
// Planning model
// --------------
// Every kPlanIntervalMs the coordinator rebuilds the full order map from
// scratch (registry walk -> bucket by (BG instance, team) -> per-family
// planner). Hysteresis is applied INSIDE the assignment step: a bot whose
// previous order matches a candidate (same kind, same target within
// kStickyRadius) gets a cost discount, so stable inputs reproduce the
// previous plan and bots don't thrash between equidistant objectives.
//
// The coordinator only orders V2 bots. Human teammates are observed
// indirectly (through the node pressure counts the builder harvests) but
// never directed. Bots the plan does not cover keep order.kind == None and
// run the legacy greedy role logic — the coordinator concentrates force
// where coordination beats greed and deliberately leaves the rest alone.

#include "BgTeamCoordinator.h"
#include "BattlegroundScript.h"
#include "../BotRegistry.h"
#include "../BotSnapshotView.h"
#include "../ClassTables.h"
#include "../../Services.h"
#include "../../Threading/SnapshotPublisher.h"

#include "Battleground.h"
#include "Config.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_set>

namespace Playerbot {

namespace {

// Full re-plan cadence. 750ms is fast enough to chase carrier movement and
// node flips (BG state changes on second granularity) while keeping the
// cost negligible: one registry walk + a few hundred float ops per team.
constexpr uint32 kPlanIntervalMs = 750;

// A previous order "matches" a new candidate when same-kind and the target
// moved less than this — used for the hysteresis cost discount.
constexpr float kStickyRadius = 10.0f;

// Cost multiplier for a sticky match. 0.7 means a bot keeps its current
// assignment unless a competing one is >30% closer.
constexpr float kStickyDiscount = 0.7f;

float Dist2(float ax, float ay, float bx, float by)
{
    const float dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy;
}

bool PosSet(float x, float y) { return x != 0.f || y != 0.f; }

} // namespace

// ---------------------------------------------------------------------------
// Assignment bookkeeping
// ---------------------------------------------------------------------------

void BgTeamCoordinator::AssignOrder(uint64 guid, uint8 kind, float x, float y,
                                    float z, ObjectGuid focus, uint8 squad,
                                    uint32 target_entry)
{
    BgOrder o;
    o.kind         = kind;
    o.x            = x;
    o.y            = y;
    o.z            = z;
    o.focus        = focus;
    o.squad        = squad;
    o.target_entry = target_entry;
    next_orders_[guid] = o;
}

// Pick the cheapest unassigned, alive member for a target, with the
// hysteresis discount against the PREVIOUS order map. `healer_bias` > 0
// pushes healers toward the end of the pick order (defense posts), < 0
// pulls them forward (escort slots), 0 is neutral. For focus-orders
// (escort/hunt) pass the focus guid: stickiness keys on it instead of
// position — the carrier MOVES between plans, so a positional match
// would never hold and escorts would churn every 750ms.
int BgTeamCoordinator::PickNearest(std::vector<Member>& members, uint8 kind,
                                   float tx, float ty, int healer_bias,
                                   bool allow_carrier, ObjectGuid focus) const
{
    int   best     = -1;
    float best_cost = 1e30f;
    for (int i = 0; i < int(members.size()); ++i)
    {
        Member const& m = members[i];
        if (m.assigned || !m.alive) continue;
        if (m.is_carrier && !allow_carrier) continue;
        // Order matters: the sticky discount scales DISTANCE only. Adding
        // the signed healer bias first would let the multiplier magnify
        // (or, on negative costs, invert) the role preference instead of
        // expressing "30% closer before reassignment".
        float cost = std::sqrt(Dist2(m.x, m.y, tx, ty));
        auto prev = orders_.find(m.guid_low);
        if (prev != orders_.end() && prev->second.kind == kind &&
            (!focus.IsEmpty()
                 ? prev->second.focus == focus
                 : Dist2(prev->second.x, prev->second.y, tx, ty) <
                       kStickyRadius * kStickyRadius))
            cost *= kStickyDiscount;
        if (m.healer && healer_bias > 0) cost += 200.f;   // keep healers off lone posts
        if (m.healer && healer_bias < 0) cost -= 150.f;   // prefer healers for escort
        if (cost < best_cost) { best_cost = cost; best = i; }
    }
    return best;
}

// ---------------------------------------------------------------------------
// CTF / carrier family (WSG, TP, Kotmogu, and the flag layer of EotS /
// Deephaul). Assigns: carriers -> CarryHome, a pickup runner when no
// friendly carrier exists, escorts on each carrier, and an EFC hunt squad.
// ---------------------------------------------------------------------------

void BgTeamCoordinator::PlanCtf(TeamPlanContext& ctx)
{
    const bool kotmogu   = ctx.type_id == 699;
    const bool node_play = !ctx.nodes.empty();   // hybrid (EotS/Deephaul)

    // -- Carriers: run it home / hold center --------------------------------
    for (auto& m : ctx.members)
    {
        if (!m.is_carrier || !m.alive) continue;
        float hx = ctx.home_x, hy = ctx.home_y, hz = ctx.home_z;
        if (!kotmogu && node_play)
        {
            // EotS-style: cap at the nearest node we own; if we own none,
            // run at the nearest node at all (capping it wins twice).
            float best = 1e30f;
            bool  found_owned = false;
            for (auto const& n : ctx.nodes)
            {
                if (n.is_destroyed) continue;
                const bool owned = n.owner_team == ctx.team && !n.is_contested;
                if (found_owned && !owned) continue;
                const float d = Dist2(m.x, m.y, n.x, n.y);
                if ((owned && !found_owned) || d < best)
                {
                    best = d; hx = n.x; hy = n.y; hz = n.z;
                    if (owned) found_owned = true;
                }
            }
        }
        else if (!kotmogu && PosSet(ctx.own_flag_x, ctx.own_flag_y))
        {
            // WSG/TP: cap point is the own-flag spawn.
            hx = ctx.own_flag_x; hy = ctx.own_flag_y; hz = ctx.own_flag_z;
        }
        // Kotmogu: home_base IS the center scoring zone — already set.
        if (PosSet(hx, hy))
        {
            AssignOrder(m.guid_low, BgOrder::CarryHome, hx, hy, hz);
            m.assigned = true;
        }
    }

    // -- Pickup runner(s) when we have no carrier ---------------------------
    // Kotmogu fields 4 orbs; send up to 2 runners for free ones. Everyone
    // else fields exactly one flag: 1 runner.
    int have_carriers = 0;
    for (auto const& m : ctx.members)
        if (m.is_carrier && m.alive) ++have_carriers;
    // External (human) carriers count toward "we already have the flag"
    // on single-flag maps; on Kotmogu more orbs is always better, so the
    // bot-runner count ignores them there.
    const int want_runners =
        kotmogu ? std::max(0, 2 - have_carriers)
                : ((have_carriers + ctx.external_carriers) > 0 ? 0 : 1);
    std::vector<int> runner_idx;
    if (kotmogu && want_runners > 0 && !ctx.advice_nodes.empty())
    {
        // Kotmogu: nodes[] holds the FOUR orb spawns, but the scalar
        // enemy_flag is the ref bot's guid-hashed corner — sending both
        // runners there means one grabs and one stares at an empty spawn.
        // Assign each runner a DISTINCT orb: greedy cheapest
        // (member, unclaimed-orb) pair per round.
        std::vector<bool> orb_claimed(ctx.advice_nodes.size(), false);
        for (int r = 0; r < want_runners; ++r)
        {
            int   best_m = -1, best_o = -1;
            float best_d = 1e30f;
            for (int i = 0; i < int(ctx.members.size()); ++i)
            {
                Member const& m = ctx.members[i];
                if (m.assigned || !m.alive || m.healer || m.is_carrier)
                    continue;
                for (int o = 0; o < int(ctx.advice_nodes.size()); ++o)
                {
                    if (orb_claimed[o]) continue;
                    const float d = Dist2(m.x, m.y, ctx.advice_nodes[o].x,
                                          ctx.advice_nodes[o].y);
                    if (d < best_d) { best_d = d; best_m = i; best_o = o; }
                }
            }
            if (best_m < 0) break;
            AssignOrder(ctx.members[best_m].guid_low, BgOrder::PickupFlag,
                        ctx.advice_nodes[best_o].x, ctx.advice_nodes[best_o].y,
                        ctx.advice_nodes[best_o].z);
            ctx.members[best_m].assigned = true;
            orb_claimed[best_o] = true;
            runner_idx.push_back(best_m);
        }
    }
    else if (want_runners > 0 && PosSet(ctx.enemy_flag_x, ctx.enemy_flag_y))
    {
        for (int r = 0; r < want_runners; ++r)
        {
            // Prefer the script's FC classes (stealth in WSG/TP): try a
            // preferred-class pick first, fall back to anyone.
            int pick = -1;
            if (!ctx.fc_class_preference.empty())
            {
                float best_cost = 1e30f;
                for (int i = 0; i < int(ctx.members.size()); ++i)
                {
                    Member const& m = ctx.members[i];
                    if (m.assigned || !m.alive || m.healer || m.is_carrier)
                        continue;
                    bool preferred = false;
                    for (uint8 c : ctx.fc_class_preference)
                        if (c == m.cls) { preferred = true; break; }
                    if (!preferred) continue;
                    const float cost =
                        std::sqrt(Dist2(m.x, m.y, ctx.enemy_flag_x, ctx.enemy_flag_y));
                    if (cost < best_cost) { best_cost = cost; pick = i; }
                }
            }
            if (pick < 0)
                pick = PickNearest(ctx.members, BgOrder::PickupFlag,
                                   ctx.enemy_flag_x, ctx.enemy_flag_y,
                                   /*healer_bias=*/+1, /*allow_carrier=*/false);
            if (pick < 0) break;
            AssignOrder(ctx.members[pick].guid_low, BgOrder::PickupFlag,
                        ctx.enemy_flag_x, ctx.enemy_flag_y, ctx.enemy_flag_z);
            ctx.members[pick].assigned = true;
            runner_idx.push_back(pick);
        }
    }

    // -- Escorts -------------------------------------------------------------
    // Each live carrier gets an escort detail; the pickup runner gets one
    // too on pure-CTF maps (the run back is where flags die). First escort
    // slot prefers a healer.
    const int escorts_per_carrier = node_play ? 1 : (kotmogu ? 1 : 3);
    auto escort_to = [&](float ex, float ey, float ez, ObjectGuid focus,
                         int count, uint8 squad)
    {
        for (int e = 0; e < count; ++e)
        {
            const int pick =
                PickNearest(ctx.members, BgOrder::EscortFC, ex, ey,
                            /*healer_bias=*/e == 0 ? -1 : +1,
                            /*allow_carrier=*/false, focus);
            if (pick < 0) return;
            AssignOrder(ctx.members[pick].guid_low, BgOrder::EscortFC,
                        ex, ey, ez, focus, squad);
            ctx.members[pick].assigned = true;
        }
    };
    uint8 squad_no = 1;
    for (auto const& m : ctx.members)
        if (m.is_carrier && m.alive)
            escort_to(m.x, m.y, m.z,
                      ObjectGuid::Create<HighGuid::Player>(m.guid_low),
                      escorts_per_carrier, squad_no++);
    // A human teammate carrying the flag gets the same escort detail —
    // the executor follows the live carrier guid, so the plan-time
    // position only seeds the distance ranking.
    if (ctx.scalar_carrier_is_external && !kotmogu &&
        PosSet(ctx.friendly_carrier_x, ctx.friendly_carrier_y))
        escort_to(ctx.friendly_carrier_x, ctx.friendly_carrier_y,
                  ctx.friendly_carrier_z, ctx.friendly_carrier,
                  escorts_per_carrier, squad_no++);
    if (!node_play && !kotmogu)
        for (int ri : runner_idx)
            escort_to(ctx.members[ri].x, ctx.members[ri].y, ctx.members[ri].z,
                      ObjectGuid::Create<HighGuid::Player>(ctx.members[ri].guid_low),
                      /*count=*/1, squad_no++);

    // -- EFC hunt squad (pure CTF only) --------------------------------------
    // Concentrated 3-bot intercept on the enemy carrier. On hybrid maps the
    // node planner owns the remainder; on Kotmogu killing carriers is the
    // whole midfield game and the legacy in-combat target switch (EFC-first)
    // already handles it once bodies are in the center.
    if (!node_play && !kotmogu && !ctx.enemy_carrier.IsEmpty() &&
        PosSet(ctx.enemy_carrier_x, ctx.enemy_carrier_y))
    {
        const int hunters = int(ctx.members.size()) >= 8 ? 3 : 2;
        for (int h = 0; h < hunters; ++h)
        {
            const int pick =
                PickNearest(ctx.members, BgOrder::HuntEFC,
                            ctx.enemy_carrier_x, ctx.enemy_carrier_y,
                            /*healer_bias=*/+1, /*allow_carrier=*/false,
                            ctx.enemy_carrier);
            if (pick < 0) break;
            AssignOrder(ctx.members[pick].guid_low, BgOrder::HuntEFC,
                        ctx.enemy_carrier_x, ctx.enemy_carrier_y,
                        ctx.enemy_carrier_z, ctx.enemy_carrier);
            ctx.members[pick].assigned = true;
        }
    }
    // Remainder stays unordered on pure CTF: the legacy mid-pressure /
    // score-bias roles are already good there, and leaving them free keeps
    // graceful degradation honest.
}

// ---------------------------------------------------------------------------
// Node-race family (AB, BfG, AV, IoC, Deephaul, EotS towers, DG).
// Quota-based defense scaled by live enemy pressure, then ONE concentrated
// attack squad on the weakest takeable node — the single biggest win over
// per-bot greed, which smears attackers across every enemy node at once.
// ---------------------------------------------------------------------------

void BgTeamCoordinator::PlanNodeRace(TeamPlanContext& ctx)
{
    if (ctx.nodes.empty()) return;
    const uint8 enemy_team = ctx.team == 1 ? 2 : 1;

    const bool turtle = ctx.score_delta >= ctx.score_bias_threshold ||
                        (ctx.in_progress_ms > 18u * 60u * 1000u && ctx.score_delta > 0);
    const bool all_in = ctx.score_delta <= -ctx.score_bias_threshold ||
                        (ctx.in_progress_ms > 18u * 60u * 1000u && ctx.score_delta < 0);

    auto enemy_near = [&](BgNodeState const& n) -> int
    { return ctx.team == 1 ? n.horde_players_near : n.alliance_players_near; };

    // -- Defense demands ------------------------------------------------------
    // CONTESTED SEMANTICS (builder contract, BotSnapshotBuilder ~6206):
    // a contested node's owner_team names the team DOING THE FLIP, not the
    // team that held it. So:
    //   owner==enemy && contested  -> the enemy is capping something
    //                                 (our node or a neutral): STOP THE CAP.
    //   owner==us    && contested  -> WE are mid-flip: that's an attack-
    //                                 reinforce candidate, NOT a defense.
    //   owner==us    && !contested -> held node: standing garrison.
    // Emergencies first, then garrisons sized to live enemy pressure.
    // All-in strips garrisons to a single sentry; turtle adds one.
    struct Demand { float x, y, z; int quota; bool emergency; };
    std::vector<Demand> defense;
    for (auto const& n : ctx.nodes)
    {
        if (n.is_destroyed) continue;   // razed AV tower — nothing to hold
        if (n.owner_team == enemy_team && n.is_contested)
            defense.push_back({n.x, n.y, n.z,
                               std::max(2, std::min(enemy_near(n) + 1, 4)),
                               true});
        else if (n.owner_team == ctx.team && !n.is_contested)
        {
            int quota = all_in ? 1 : std::clamp(enemy_near(n), 1, 3);
            if (turtle) ++quota;
            defense.push_back({n.x, n.y, n.z, quota, false});
        }
    }
    std::stable_sort(defense.begin(), defense.end(),
                     [](Demand const& a, Demand const& b)
                     { return a.emergency > b.emergency; });

    // Defense BUDGET: demands must never consume the whole roster — on
    // epic maps (AV: 15 nodes, ~7 held per side at start) unbudgeted
    // garrisons would drain all 40 bots and the attack section would
    // never run, turning the team permanently passive. Garrisons may
    // take ~60% of the live roster; emergencies may borrow up to ~75%;
    // the rest is the guaranteed attacker core.
    int alive_free = 0;
    for (auto const& m : ctx.members)
        if (m.alive && !m.assigned && !m.is_carrier) ++alive_free;
    const int garrison_budget  = std::max(1, (alive_free * 3) / 5);
    const int emergency_budget = std::max(1, (alive_free * 3) / 4);
    int spent = 0;
    for (auto const& d : defense)
    {
        const int budget = d.emergency ? emergency_budget : garrison_budget;
        for (int q = 0; q < d.quota && spent < budget; ++q)
        {
            // Emergencies take healers if that's what's left; garrisons don't.
            const int pick = PickNearest(ctx.members, BgOrder::DefendNode,
                                         d.x, d.y,
                                         /*healer_bias=*/d.emergency ? 0 : +1,
                                         /*allow_carrier=*/false);
            if (pick < 0) return;   // out of bodies entirely
            AssignOrder(ctx.members[pick].guid_low, BgOrder::DefendNode,
                        d.x, d.y, d.z);
            ctx.members[pick].assigned = true;
            ++spent;
        }
    }

    // -- Attack: one concentrated squad --------------------------------------
    // Target = cheapest takeable node: neutral beats enemy-held, light
    // defense beats heavy, script priority (AV towers) breaks ties, distance
    // from the unassigned force breaks the rest. Endgame coords override the
    // target when the all-in switch is thrown and the script has one.
    float cx = 0.f, cy = 0.f;
    int   free_count = 0;
    for (auto const& m : ctx.members)
        if (!m.assigned && m.alive) { cx += m.x; cy += m.y; ++free_count; }
    if (free_count == 0) return;
    cx /= float(free_count); cy /= float(free_count);

    float tx = 0.f, ty = 0.f, tz = 0.f;
    uint8  order_kind   = BgOrder::AttackNode;
    uint32 push_entry   = 0;
    // Push the enemy boss when LOSING (all_in) OR when the script flags the
    // push unconditional (AV: enemy reinforcements are low — the boss kill
    // ends the match regardless of node count, so push even while winning).
    // Stage the enemy CAPTAIN (priority-3 node) before the general: the
    // general's room is only deliberately cleared once the captain is dead.
    // Also commit a clearly-LEADING team to the captain->general push once the
    // early game is past: killing the enemy general is an instant win, so the
    // winning team should drive for it rather than wait for the slow AV
    // reinforcement race to drain below endgame_unconditional (which stalls in
    // bot-only matches around a 1-tower / +75 lead). The losing team keeps
    // contesting nodes to defend.
    const bool lead_push = ctx.endgame_creature_entry != 0 &&
                           ctx.score_delta >= 50 &&
                           ctx.in_progress_ms > 4u * 60u * 1000u;
    // STICKY endgame commit. lead_push flips off the instant the reinforcement
    // race nudges the lead below +50, which in bot-only AV happens constantly
    // around the +-75 one-tower stall — so the captain push kept starting and
    // stopping and never sustained long enough to burn the captain down. Once a
    // team commits (lead_push fires while an endgame creature exists), latch the
    // commitment and keep pushing until the endgame creature is dead (the AV
    // script stops advertising it -> endgame_creature_entry==0) or the team is
    // genuinely crushed (score_delta < -150, i.e. about to lose — fall back to
    // defending). This is also how humans break the stall: the leading team
    // drives the captain->general kill instead of trading nodes forever.
    const uint64 commit_key = (uint64(ctx.instance_id) << 2) | uint64(ctx.team);
    if (lead_push)
        endgame_commit_[commit_key] = ctx.in_progress_ms;
    const bool committed = endgame_commit_.find(commit_key) != endgame_commit_.end();
    if (ctx.endgame_creature_entry == 0 || ctx.score_delta < -150)
        endgame_commit_.erase(commit_key);  // endgame done / team collapsing
    const bool sticky_push = committed && ctx.endgame_creature_entry != 0 &&
                             ctx.score_delta >= -150;
    const bool push_boss = all_in || ctx.endgame_unconditional || lead_push ||
                           sticky_push;
    if (push_boss && ctx.captain_alive && PosSet(ctx.captain_x, ctx.captain_y))
    {
        tx = ctx.captain_x; ty = ctx.captain_y; tz = ctx.captain_z;
        order_kind = BgOrder::PushEndgame;
        push_entry = ctx.captain_creature_entry;
    }
    else if (push_boss && PosSet(ctx.endgame_x, ctx.endgame_y))
    {
        tx = ctx.endgame_x; ty = ctx.endgame_y; tz = ctx.endgame_z;
        order_kind = BgOrder::PushEndgame;
        push_entry = ctx.endgame_creature_entry;
    }
    else
    {
        float best_cost = 1e30f;
        for (auto const& n : ctx.nodes)
        {
            // Skip razed structures, held garrison nodes and enemy-flip
            // emergencies (the defense demands above own those).
            // Candidates: neutral, enemy-held uncontested, and OUR
            // flip-in-progress (owner==us && contested — the cheapest
            // finish of all, reinforce it).
            if (n.is_destroyed) continue;
            if (n.owner_team == ctx.team && !n.is_contested) continue;
            if (n.owner_team == enemy_team && n.is_contested) continue;
            const bool neutral = n.owner_team == 0;
            const bool enemy   = n.owner_team == enemy_team;
            float cost = std::sqrt(Dist2(cx, cy, n.x, n.y)) * 0.05f;
            if (enemy)   cost += 8.f;
            if (neutral) cost += 2.f;
            cost += float(enemy_near(n)) * 3.f;
            // Find the matching advice-node priority (AV towers > GYs).
            cost -= float(NodePriorityFor(ctx, n)) * 2.f;
            if (cost < best_cost)
            { best_cost = cost; tx = n.x; ty = n.y; tz = n.z; }
        }
        if (!PosSet(tx, ty))
        {
            // We own everything (or no takeable node) — turtle in place.
            // Leave the remainder unordered; legacy roamers patrol well.
            return;
        }
    }

    for (auto& m : ctx.members)
    {
        if (m.assigned || !m.alive || m.is_carrier) continue;
        AssignOrder(m.guid_low, order_kind, tx, ty, tz, ObjectGuid::Empty,
                    /*squad=*/1, /*target_entry=*/push_entry);
        m.assigned = true;
    }
}

// Match a live node against the script's static node list to read its
// priority weight (position match within 25y — banner GO vs advice coords
// are a few yards apart on most maps).
uint8 BgTeamCoordinator::NodePriorityFor(TeamPlanContext const& ctx,
                                         BgNodeState const& n)
{
    for (auto const& an : ctx.advice_nodes)
        if (Dist2(an.x, an.y, n.x, n.y) < 25.f * 25.f)
            return an.priority;
    return 0;
}

// ---------------------------------------------------------------------------
// Per-team plan entry: family selection + diagnostics.
// ---------------------------------------------------------------------------

void BgTeamCoordinator::PlanTeam(TeamPlanContext& ctx, uint32 /*now_ms*/)
{
    const bool flag_play = PosSet(ctx.enemy_flag_x, ctx.enemy_flag_y) ||
                           !ctx.enemy_carrier.IsEmpty() ||
                           std::any_of(ctx.members.begin(), ctx.members.end(),
                                       [](Member const& m) { return m.is_carrier; });
    const bool node_play = !ctx.nodes.empty();

    if (flag_play)
        PlanCtf(ctx);          // also covers the flag layer of hybrids
    if (node_play)
        PlanNodeRace(ctx);     // remaining members

    // Plan-change diagnostic: log when the team's attack focus or carrier
    // detail changed since the last plan (not every 750ms tick).
    uint64 sig = 1469598103934665603ull;   // FNV-1a over order kinds+coords
    auto mix = [&sig](uint64 v)
    { sig ^= v; sig *= 1099511628211ull; };
    int ordered = 0;
    for (auto const& m : ctx.members)
    {
        auto it = next_orders_.find(m.guid_low);
        if (it == next_orders_.end()) continue;
        ++ordered;
        mix(uint64(it->second.kind));
        // Carrier-tracking orders (escort / hunt / carry) target a MOVING
        // player — their coords shift every plan while a flag runs, which
        // re-logged "plan changed" up to once per 750ms cycle for a whole
        // carry. Only positional objective coords participate in the
        // change signature; for tracking kinds the kind + focus identity
        // is the stable plan content.
        const bool tracks_carrier =
            it->second.kind == BgOrder::EscortFC ||
            it->second.kind == BgOrder::HuntEFC  ||
            it->second.kind == BgOrder::CarryHome;
        if (tracks_carrier)
        {
            mix(it->second.focus.GetCounter());
        }
        else
        {
            mix(uint64(int64(it->second.x / 15.f)));   // 15y grid: ignore jitter
            mix(uint64(int64(it->second.y / 15.f)));
        }
    }
    const uint64 key = (uint64(ctx.instance_id) << 2) | ctx.team;
    auto sig_it = plan_sig_.find(key);
    if (sig_it == plan_sig_.end() || sig_it->second != sig)
    {
        plan_sig_[key] = sig;
        TC_LOG_INFO("playerbot.v2",
            "[bgcoord] bg_type={} instance={} team={} plan changed: "
            "{} bots, {} ordered, nodes={} flag_play={} score_delta={}",
            ctx.type_id, ctx.instance_id, uint32(ctx.team),
            uint32(ctx.members.size()), ordered, uint32(ctx.nodes.size()),
            flag_play ? 1 : 0, ctx.score_delta);
    }
}

// ---------------------------------------------------------------------------
// World-tick driver.
// ---------------------------------------------------------------------------

void BgTeamCoordinator::Update(uint32 now_ms)
{
    if (now_ms - last_plan_ms_ < kPlanIntervalMs)
        return;
    last_plan_ms_ = now_ms;

    if (!sConfigMgr->GetBoolDefault("Playerbot.Bg.Coordinator.Enable", true))
    {
        if (!orders_.empty()) orders_.clear();
        last_dump_ = "coordinator disabled (Playerbot.Bg.Coordinator.Enable=0)";
        return;
    }

    // -- Bucket every in-BG bot by (instance, team) ---------------------------
    struct Bucket
    {
        uint32 instance_id = 0;
        uint16 type_id = 0;
        uint8  team = 0;
        std::vector<std::pair<uint64, Player*>> bots;
    };
    std::unordered_map<uint64, Bucket> buckets;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const& e)
    {
        if (!e.ai) return;
        Player* p = ObjectAccessor::FindConnectedPlayer(
                        ObjectGuid::Create<HighGuid::Player>(id));
        if (!p) return;
        Battleground* bg = p->GetBattleground();
        if (!bg || bg->isArena()) return;
        if (bg->GetStatus() != STATUS_IN_PROGRESS) return;
        const uint8 team_u8 = p->GetEffectiveTeam() == ALLIANCE ? 1 : 2;
        const uint64 key = (uint64(bg->GetInstanceID()) << 2) | team_u8;
        Bucket& b = buckets[key];
        b.instance_id = bg->GetInstanceID();
        b.type_id     = uint16(bg->GetTypeID());
        b.team        = team_u8;
        b.bots.emplace_back(uint64(id), p);
    });

    next_orders_.clear();
    std::ostringstream dump;

    for (auto& [key, b] : buckets)
    {
        // Reference snapshot: the freshest member view of map-global BG
        // state (node ownership, carriers, scores). The builder harvests
        // these every snapshot build; one member's view serves the team.
        std::shared_ptr<BotSnapshot const> ref;
        for (auto const& [glow, p] : b.bots)
        {
            auto s = Services::Snapshots().latest(glow);
            if (s && s->bg.current_type_id != 0 &&
                (!ref || s->version > ref->version))
                ref = s;
        }
        if (!ref)
            continue;   // data cold (first seconds of a match) — legacy runs
        // The snapshot's RESOLVED bg type id (the builder falls back to
        // the queue's BattlemasterList id when Battleground::GetTypeID()
        // is 0) — keep family checks consistent with the advice source.
        const uint16 resolved_type_id = ref->bg.current_type_id;
        // Advice is computed HERE, on the world thread, from the immutable
        // reference snapshot. Do NOT borrow a member BotAI's BgAdviceCache:
        // that cache is OWNED BY THE AI WORKER THREAD — BgDispatch
        // reassigns its heap vectors every ≤2s, so reading it here could
        // iterate freed buffers (use-after-free; adversarial review
        // 2026-06-10). Scripts are stateless and registered once at boot;
        // GetAdvice on a snapshot view is thread-safe and costs one call
        // per (instance, team) per 750ms plan — negligible next to the
        // per-bot per-tick churn the AI-side cache exists to avoid.
        BattlegroundAdvice const adv =
            Services::Battlegrounds().GetAdvice(BotSnapshotView(*ref));

        TeamPlanContext ctx;
        ctx.team        = b.team;
        ctx.type_id     = resolved_type_id;
        ctx.instance_id = b.instance_id;
        ctx.nodes       = ref->bg.node_states;
        ctx.in_progress_ms = ref->bg.in_progress_ms;
        ctx.score_delta = b.team == 1
            ? int32(ref->bg.score_alliance) - int32(ref->bg.score_horde)
            : int32(ref->bg.score_horde)    - int32(ref->bg.score_alliance);
        ctx.score_bias_threshold =
            adv.score_bias_threshold > 0 ? adv.score_bias_threshold : 200;
        ctx.enemy_flag_x = adv.enemy_flag_x; ctx.enemy_flag_y = adv.enemy_flag_y;
        ctx.enemy_flag_z = adv.enemy_flag_z;
        ctx.own_flag_x = adv.own_flag_x; ctx.own_flag_y = adv.own_flag_y;
        ctx.own_flag_z = adv.own_flag_z;
        ctx.home_x = adv.home_base_x; ctx.home_y = adv.home_base_y;
        ctx.home_z = adv.home_base_z;
        ctx.endgame_x = adv.endgame_target_x; ctx.endgame_y = adv.endgame_target_y;
        ctx.endgame_z = adv.endgame_target_z;
        ctx.endgame_unconditional  = adv.endgame_unconditional;
        ctx.endgame_creature_entry = adv.endgame_creature_entry;
        // Enemy captain = the ENEMY-side priority-3 node the AV script still
        // advertises (it drops each captain from the list once that captain
        // dies). team 1 = Alliance hunts Galvangar 11947 (Frostwolf, the
        // -545,-165 spawn); team 2 = Horde hunts Balinda 11949 (Stormpike,
        // the -57,-286 spawn). The script lists BOTH captains with priority
        // 3, so match by spawn position — never take "the last one" or the
        // team would push toward its own captain.
        ctx.captain_creature_entry = (b.team == 1) ? 11947u : 11949u;
        const float cap_tx = (b.team == 1) ? -545.2f : -57.8f;
        const float cap_ty = (b.team == 1) ? -165.4f : -286.6f;
        for (auto const& an : adv.nodes)
            if (an.priority == 3 &&
                Dist2(an.x, an.y, cap_tx, cap_ty) < 25.f * 25.f)
            {
                ctx.captain_x = an.x; ctx.captain_y = an.y; ctx.captain_z = an.z;
                ctx.captain_alive = true;
            }
        ctx.fc_class_preference = adv.fc_class_preference;
        for (auto const& an : adv.nodes)
            ctx.advice_nodes.push_back({an.x, an.y, an.z, an.priority});
        ctx.friendly_carrier   = ref->bg.friendly_flag_carrier;
        ctx.enemy_carrier      = ref->bg.enemy_flag_carrier;
        ctx.enemy_carrier_x    = ref->bg.enemy_carrier_x;
        ctx.enemy_carrier_y    = ref->bg.enemy_carrier_y;
        ctx.enemy_carrier_z    = ref->bg.enemy_carrier_z;

        ctx.members.reserve(b.bots.size());
        for (auto const& [glow, p] : b.bots)
        {
            Member m;
            m.guid_low = glow;
            m.x = p->GetPositionX(); m.y = p->GetPositionY();
            m.z = p->GetPositionZ();
            m.cls   = uint8(p->GetClass());
            m.alive = p->IsAlive();
            uint16 spec = 0;
            if (auto s = Services::Snapshots().latest(glow))
                spec = uint16(s->identity.spec);
            m.healer = IsHealerSpec(m.cls, spec);
            m.tank   = IsTankSpec(m.cls, spec);
            ObjectGuid const pg = p->GetGUID();
            m.is_carrier = pg == ref->bg.friendly_flag_carrier;
            if (!m.is_carrier)
                for (ObjectGuid const& cg : ref->bg.all_friendly_carriers)
                    if (cg == pg) { m.is_carrier = true; break; }
            ctx.members.push_back(m);
        }
        // Deterministic order: assignment must not depend on registry
        // iteration order or the sticky discount loses its anchor.
        std::sort(ctx.members.begin(), ctx.members.end(),
                  [](Member const& a, Member const& b2)
                  { return a.guid_low < b2.guid_low; });

        // Detect friendly carriers who are NOT bucketed bots — a human
        // teammate carrying the flag. Without this, PlanCtf would think
        // "no carrier" and send a pickup runner to an empty flag stand
        // while leaving the human FC unescorted.
        {
            std::vector<ObjectGuid> carriers = ref->bg.all_friendly_carriers;
            if (carriers.empty() && !ref->bg.friendly_flag_carrier.IsEmpty())
                carriers.push_back(ref->bg.friendly_flag_carrier);
            for (ObjectGuid const& cg : carriers)
            {
                bool is_member = false;
                for (auto const& m : ctx.members)
                    if (cg.GetCounter() == m.guid_low)
                    { is_member = true; break; }
                if (!is_member)
                {
                    ++ctx.external_carriers;
                    if (cg == ref->bg.friendly_flag_carrier)
                        ctx.scalar_carrier_is_external = true;
                }
            }
            ctx.friendly_carrier_x = ref->bg.friendly_carrier_x;
            ctx.friendly_carrier_y = ref->bg.friendly_carrier_y;
            ctx.friendly_carrier_z = ref->bg.friendly_carrier_z;
        }

        PlanTeam(ctx, now_ms);

        // Diagnostic dump line per team.
        int kinds[9] = {};
        for (auto const& m : ctx.members)
        {
            auto it = next_orders_.find(m.guid_low);
            if (it != next_orders_.end() && it->second.kind < 9)
                ++kinds[it->second.kind];
        }
        dump << "bg_type=" << b.type_id << " inst=" << b.instance_id
             << " team=" << uint32(b.team) << " bots=" << b.bots.size()
             << " atk=" << kinds[BgOrder::AttackNode]
             << " def=" << kinds[BgOrder::DefendNode]
             << " esc=" << kinds[BgOrder::EscortFC]
             << " hunt=" << kinds[BgOrder::HuntEFC]
             << " pickup=" << kinds[BgOrder::PickupFlag]
             << " carry=" << kinds[BgOrder::CarryHome]
             << " push=" << kinds[BgOrder::PushEndgame]
             << " delta=" << ctx.score_delta << "\n";
    }

    orders_ = std::move(next_orders_);
    next_orders_.clear();
    // Drop change-log signatures for teams that no longer exist (match
    // ended) so the map doesn't grow for the server's whole uptime.
    for (auto it = plan_sig_.begin(); it != plan_sig_.end();)
        it = buckets.count(it->first) ? std::next(it) : plan_sig_.erase(it);
    for (auto it = endgame_commit_.begin(); it != endgame_commit_.end();)
        it = buckets.count(it->first) ? std::next(it) : endgame_commit_.erase(it);
    last_dump_ = dump.str();
    if (last_dump_.empty())
        last_dump_ = "no active bot battleground teams";
}

std::string BgTeamCoordinator::DebugDump() const
{
    return last_dump_.empty() ? std::string("coordinator has not planned yet")
                              : last_dump_;
}

} // namespace Playerbot
