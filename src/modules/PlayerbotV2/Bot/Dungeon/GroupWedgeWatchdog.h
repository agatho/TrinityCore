// GroupWedgeWatchdog — read-only group-level no-progress DIAGNOSE engine.
//
// The per-bot Diagnostics::WedgeWatchdog deliberately EXEMPTS in-instance/grouped
// bots from its no-progress detector, so a dungeon group that stops making forward
// progress has NO safety net today. The 2026-06-30 multi-dungeon validation showed
// the group fails DIFFERENTLY each run — SFK tank-can't-reach false-combat / RFC
// tank-forward split / WC frozen Z-straggler + cohered-but-idle stall / Deadmines
// off-mesh — a CLUSTER of nav/advance/cohesion fragilities, not one bug. The
// robustness program is: watchdog -> diagnose {stranded|split|cohered_idle|
// false_combat} -> recover.
//
// THIS class is the DIAGNOSE stage and is intentionally READ-ONLY: it classifies a
// wedge and emits one structured [group_wedge] log line per episode. It changes NO
// behavior, so it cannot regress the working dungeons (Deadmines 5/6) and can ship
// before live verification. Remediation is a SEPARATE, config-gated follow-up
// (PlayerbotV2.GroupWedge.RemediationEnabled) enabled per-classification only once
// the live classification is confirmed — so the delicate advance/cohesion core is
// never touched blind (it has regressed twice already).
//
// Runs on the WORLD THREAD in OnWorldUpdate, same lifecycle slot as
// Diagnostics::WedgeWatchdog, internally throttled. Mirrors PveGroupCoordinator's
// per-(group, map) bucketing and immutable-snapshot inputs.

#pragma once

#include "Define.h"
#include <unordered_map>

namespace Playerbot {

class GroupWedgeWatchdog
{
public:
    GroupWedgeWatchdog() = default;

    // World-thread driver; internally throttled to its own cadence. Reads only
    // immutable published snapshots + (for one stranded probe) a world-thread
    // path calculation. Never mutates bot state.
    void Update(uint32 now_ms);

private:
    struct GroupState
    {
        uint32 map_id      = 0;
        float  anchor_x    = 0.f;
        float  anchor_y    = 0.f;
        float  anchor_z    = 0.f;
        uint32 progress_ms = 0;   // last tick the group made forward progress
        uint32 last_log_ms = 0;   // [group_wedge] emit throttle
        bool   anchored    = false;
    };

    // Keyed by (group_low << 16) ^ map_id, same as PveGroupCoordinator.
    std::unordered_map<uint64, GroupState> groups_;
    uint32 last_tick_ms_ = 0;
};

} // namespace Playerbot
