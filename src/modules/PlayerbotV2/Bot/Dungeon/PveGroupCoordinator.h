// PveGroupCoordinator — group-level dungeon/raid coordination.
//
// Counterpart of Bot/Battleground/BgTeamCoordinator for instanced PvE.
// Before this, group play relied on the tank hierarchy plus per-bot greed:
// every capable kicker burned its interrupt on the same mandatory cast
// (the "stagger" was a SELF-throttle, not a rotation), all healers triaged
// the same lowest-HP target, every bot walked into the same soak circle,
// DPS swapped to adds independently, and two bot tanks could pull
// different packs simultaneously. That roughly works at 5-man scale where
// the tank does the coordinating; in raids (10-30 bots) it reproduces the
// BG thundering herd — and even 5-mans waste kicks and stack soaks.
//
// This service computes ONE plan per GROUP on the WORLD THREAD every
// kPlanIntervalMs for groups with at least one V2 bot inside a dungeon or
// raid map, and publishes composable per-bot duties through the snapshot
// (BotSnapshot::PveOrder): main/off tank designation, an interrupt
// rotation (rank 0 kicks, rank 1 backs up, the rest HOLD), designated
// soakers, healer focus assignments, a synchronized kill target, and
// spread-bearing slots. `active == false` (or per-field sentinels) falls
// back to legacy behavior everywhere — graceful degradation by design.
//
// Threading contract (identical to BgTeamCoordinator): Update() is the only
// WRITER of orders_ and runs on the WORLD THREAD, inline in OnWorldUpdate
// BEFORE the parallel snapshot-build barrier (run_and_wait). OrderFor() is a
// pure const find() READER and is called concurrently from the Phase 4
// snapshot-build WORKER threads. This is safe ONLY because writer and readers
// are temporally disjoint: orders_ is never mutated during the build phase, and
// concurrent reads of a non-mutating unordered_map are well-defined. If Update()
// is ever moved to run concurrently with the build, orders_ needs a shared_mutex.
// Inputs are the published immutable GroupSnapshot / BotSnapshot shared_ptrs and
// DungeonScriptMgr::GetAdvice computed HERE from a member snapshot — never a
// BotAI-owned cache (those are AI-worker property; reading them cross-thread is a
// use-after-free, see the 2026-06-10 BG coordinator review).
//
// Human members are never ordered, but they OCCUPY duties: a human tank
// makes every bot tank an off-tank, a human healer shifts bot healers
// toward raid triage, and human DPS reduce how many bot soakers are
// drafted.

#pragma once

#include "../BotSnapshot.h"
#include <unordered_map>
#include <string>
#include <vector>

namespace Playerbot {

class PveGroupCoordinator
{
public:
    PveGroupCoordinator() = default;

    // World tick driver; internally throttled to the plan cadence.
    void Update(uint32 now_ms);

    // Order lookup for the snapshot builder. Returns nullptr when no
    // current plan covers the bot (consumer publishes a default-inactive
    // order and the AI runs legacy logic).
    PveOrder const* OrderFor(uint64 bot_guid_low) const
    {
        auto it = orders_.find(bot_guid_low);
        return it == orders_.end() ? nullptr : &it->second;
    }

    // Human-readable plan dump for `.playerbot pvecoord`.
    std::string DebugDump() const;

private:
    struct Member
    {
        uint64 guid_low = 0;
        bool   is_bot = false;
        bool   alive = false;
        uint8  cls = 0;
        uint16 spec = 0;
        bool   tank = false;
        bool   healer = false;
        bool   interrupter = false;
    };

    std::unordered_map<uint64, PveOrder> orders_;
    std::unordered_map<uint64, PveOrder> next_orders_;
    // Per-(group, map) plan signature for change-only logging.
    std::unordered_map<uint64, uint64>   plan_sig_;
    // Per-(group, map) kill-focus stickiness — coordinator state rather
    // than a read-back from member orders, which broke when the sampled
    // member left the group or the data ticked cold.
    std::unordered_map<uint64, ObjectGuid> last_kill_focus_;
    uint32 last_plan_ms_ = 0;
    std::string last_dump_;
};

} // namespace Playerbot
