// GroupWedgeWatchdog implementation. See header for the design contract.
//
// READ-ONLY: classify + log only. No bot state is mutated. Mirrors
// PveGroupCoordinator's registry bucketing + immutable-snapshot inputs, and runs
// in the same world-thread OnWorldUpdate slot as Diagnostics::WedgeWatchdog.

#include "GroupWedgeWatchdog.h"
#include "DungeonScript.h"
#include "../BotRegistry.h"
#include "../BotSnapshotView.h"
#include "../../Group/GroupSnapshot.h"
#include "../../Services.h"
#include "../../Threading/SnapshotPublisher.h"

#include "PlayerbotAPI.h"        // Playerbot::PathBudget
#include "PlayerbotMovement.h"   // BotMovement::SehSafeCalculatePath (tile-race guard)
#include "PathGenerator.h"       // PathGenerator + PathType reachability verdict

#include "Config.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Playerbot {

namespace {

// Diagnose cadence. Much slower than the 500ms coordinator — a wedge is a
// multi-second condition and a path probe is comparatively costly.
constexpr uint32 kIntervalMs = 3000;

inline float sq(float v) { return v * v; }

} // namespace

void GroupWedgeWatchdog::Update(uint32 now_ms)
{
    if (now_ms - last_tick_ms_ < kIntervalMs)
        return;
    last_tick_ms_ = now_ms;

    if (!sConfigMgr->GetBoolDefault("PlayerbotV2.GroupWedge.Enabled", true))
    {
        if (!groups_.empty()) groups_.clear();
        return;
    }

    // Tunables (read live so they are hot-reloadable; quiet defaults so the keys
    // are optional). A wedge is "no forward progress (group centroid moved
    // < MoveYards) AND not in real combat for WindowMs".
    const uint32 windowMs = uint32(sConfigMgr->GetIntDefault("PlayerbotV2.GroupWedge.WindowMs", 20000));
    const float  cohereY  = float(sConfigMgr->GetFloatDefault("PlayerbotV2.GroupWedge.CohereYards", 30.0));
    const float  splitY   = float(sConfigMgr->GetFloatDefault("PlayerbotV2.GroupWedge.SplitYards", 45.0));
    const float  farY     = float(sConfigMgr->GetFloatDefault("PlayerbotV2.GroupWedge.FarYards", 45.0));
    const float  moveY    = float(sConfigMgr->GetFloatDefault("PlayerbotV2.GroupWedge.MoveYards", 12.0));

    // -- Bucket grouped, instanced bots by (group, map) (mirror PveGroupCoordinator) --
    struct Bucket
    {
        uint64 group_low = 0;
        uint32 map_id    = 0;
        std::vector<std::pair<uint64, Player*>> bots;
    };
    std::unordered_map<uint64, Bucket> buckets;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const& e)
    {
        if (!e.ai) return;
        Player* p = ObjectAccessor::FindConnectedPlayer(
                        ObjectGuid::Create<HighGuid::Player>(id));
        if (!p) return;
        Group const* grp = p->GetGroup();
        if (!grp) return;
        Map* m = p->GetMap();
        if (!m || !m->IsDungeon()) return;
        const uint64 glow = grp->GetGUID().GetCounter();
        const uint64 key  = (glow << 16) ^ uint64(m->GetId());
        Bucket& b = buckets[key];
        b.group_low = glow;
        b.map_id    = m->GetId();
        b.bots.emplace_back(uint64(id), p);
    });

    // Forget groups that are no longer bucketed (left instance / disbanded).
    for (auto it = groups_.begin(); it != groups_.end(); )
        it = (buckets.find(it->first) == buckets.end()) ? groups_.erase(it) : std::next(it);

    for (auto& [key, b] : buckets)
    {
        std::shared_ptr<GroupSnapshot const> gs =
            Services::Snapshots().latest_group(b.bots.front().first);
        if (!gs || gs->members.empty())
            continue;

        // Alive, same-map member geometry. (Corpse-runners / cross-map laggards
        // excluded — a wedge is about the live body's progress.)
        struct M
        {
            uint64    low;
            float     x, y, z;
            bool      tank, in_combat, has_victim;
            Player*   p;
        };
        std::vector<M> mem;
        mem.reserve(gs->members.size());
        for (auto const& gm : gs->members)
        {
            if (!gm.online || !gm.is_alive || gm.map_id != b.map_id)
                continue;
            M e;
            e.low        = gm.guid.GetCounter();
            e.x = gm.x; e.y = gm.y; e.z = gm.z;
            e.tank       = (gm.role == Role::Tank);
            e.in_combat  = gm.in_combat;
            e.has_victim = !gm.victim.IsEmpty();
            e.p          = nullptr;
            for (auto const& [bl, pp] : b.bots) if (bl == e.low) { e.p = pp; break; }
            mem.push_back(e);
        }
        if (mem.size() < 2)
        {
            groups_.erase(key);
            continue;
        }

        // Group centroid + spread (max member distance from centroid).
        float cx = 0, cy = 0, cz = 0;
        for (auto const& e : mem) { cx += e.x; cy += e.y; cz += e.z; }
        cx /= mem.size(); cy /= mem.size(); cz /= mem.size();
        float spread = 0.f;
        const M* farthest = nullptr;
        for (auto const& e : mem)
        {
            const float d = std::sqrt(sq(e.x - cx) + sq(e.y - cy) + sq(e.z - cz));
            if (d > spread) { spread = d; farthest = &e; }
        }

        // Tank + non-tank "body" centroid + tank->body distance.
        const M* tank = nullptr;
        for (auto const& e : mem) if (e.tank) { tank = &e; }
        float bx = 0, by = 0, bz = 0; int nb = 0;
        for (auto const& e : mem) if (!e.tank) { bx += e.x; by += e.y; bz += e.z; ++nb; }
        float tankToBody = 0.f;
        if (nb > 0 && tank)
        {
            bx /= nb; by /= nb; bz /= nb;
            tankToBody = std::sqrt(sq(tank->x - bx) + sq(tank->y - by) + sq(tank->z - bz));
        }

        // "Real combat" = the tank is fighting a fightable (targetable, non-
        // stalker) enemy. This distinguishes a genuine boss/trash fight (which
        // IS progress) from the SFK false-combat wedge (in combat, fightable=0).
        int tankFightable = -1;   // -1 = unknown (human tank / cold snapshot)
        if (tank)
        {
            if (auto ts = Services::Snapshots().latest(tank->low))
                tankFightable = int(BotSnapshotView(*ts).fightable_attackers_count());
        }
        const bool real_combat = (tankFightable > 0);

        GroupState& st = groups_[key];
        if (!st.anchored || st.map_id != b.map_id)
        {
            st.anchored = true; st.map_id = b.map_id;
            st.anchor_x = cx; st.anchor_y = cy; st.anchor_z = cz;
            st.progress_ms = now_ms; st.last_log_ms = 0;
            continue;
        }

        const float moved = std::sqrt(sq(cx - st.anchor_x) + sq(cy - st.anchor_y) + sq(cz - st.anchor_z));
        if (moved > moveY)
        {
            st.anchor_x = cx; st.anchor_y = cy; st.anchor_z = cz;
            st.progress_ms = now_ms;
            continue;
        }
        if (real_combat)
        {
            // Engaged with real enemies = progressing; hold the anchor (the body
            // legitimately stands still during a fight) but reset the clock.
            st.progress_ms = now_ms;
            continue;
        }

        const uint32 wedged_ms = now_ms - st.progress_ms;
        if (wedged_ms < windowMs)
            continue;   // not (yet) wedged

        // Throttle: one [group_wedge] line per WindowMs per group.
        if (st.last_log_ms != 0 && now_ms - st.last_log_ms < windowMs)
            continue;
        st.last_log_ms = now_ms;

        // -- Classify the wedge ------------------------------------------------
        char const* cls = "other";
        char const* detail = "";

        // 1. tank false-combat: in combat but no fightable enemies (SFK).
        if (tank && tank->in_combat && tankFightable == 0)
        {
            cls = "false_combat";
        }
        // 2. stranded: a member far from the body whose path to it is NoPath.
        else if (farthest && std::sqrt(sq(farthest->x - cx) + sq(farthest->y - cy) + sq(farthest->z - cz)) > farY)
        {
            bool nopath = false;
            if (farthest->p && Playerbot::PathBudget::HasBudget(now_ms))
            {
                PathGenerator pg(farthest->p);
                BotMovement::SehSafeCalculatePath(pg, cx, cy, cz);
                PathType const pt = pg.GetPathType();
                nopath = (pt & (PATHFIND_NOPATH | PATHFIND_FARFROMPOLY | PATHFIND_INCOMPLETE)) != 0;
            }
            if (nopath) { cls = "stranded"; detail = "nopath"; }
            else        { cls = "split_straggler"; detail = "haspath"; }
        }
        // 3. tank forward, body left behind (RFC).
        else if (tank && nb >= 2 && tankToBody > splitY)
        {
            cls = "split_tank_fwd";
        }
        // 4. cohered but idle: tight cluster, tank OOC, no advance (WC run 2).
        else if (spread < cohereY && (!tank || !tank->in_combat))
        {
            cls = "cohered_idle";
        }

        TC_LOG_INFO("playerbot.v2",
            "[group_wedge] grp={} map={} cls={} {} n={} spread={:.0f} tank_d={:.0f} "
            "far={:.0f} moved={:.0f} wedged_s={} tank_combat={} tank_victim={} tank_fightable={}",
            b.group_low, b.map_id, cls, detail, unsigned(mem.size()), spread, tankToBody,
            farthest ? std::sqrt(sq(farthest->x - cx) + sq(farthest->y - cy) + sq(farthest->z - cz)) : 0.f,
            moved, wedged_ms / 1000u,
            tank ? (tank->in_combat ? 1 : 0) : -1,
            tank ? (tank->has_victim ? 1 : 0) : -1, tankFightable);
    }
}

} // namespace Playerbot
