// BotGroupBuilder - Shared "pick N eligible bots, invite, form group"
// consumer for the BotCoordinationBus.
//
// **What problem it solves:** every place V2 needs to assemble a
// group from idle bots (LFG queue auto-fill, BG team formation,
// guild RaidNight/DungeonNight, owner squad ;all run, world-boss
// response, M+ keystone) shares the same core mechanic: pick bots
// by role + level + faction + locality, invite them, optionally
// start a queue or walk them to a location.
//
// Before this class existed, every consumer reimplemented the picker
// + invite loop separately. BotQueueFiller had one for LFG; squad
// control had its own; guild events would need a third. Each
// duplication was a source of subtle bugs (bot already in group,
// bot is in BG, eligibility race with snapshot).
//
// **Bus integration:** BotGroupBuilder subscribes (Async) to
// signals that need group formation:
//   - GuildEventForming    → form group for raid/dungeon/bg night
//   - LfgTankNeeded / LfgHealerNeeded / LfgDpsNeeded → role-targeted fill
//   - BgTeamForming        → BG-bracket fill
//   - OwnerSquadAssemble   → squad pickup
//   - WorldBossSpotted     → grab nearby high-level bots
//   - MPlusKeyForming      → M+ specific (5-man + roles)
//
// Each handler resolves the event into a `GroupRequest` and runs the
// shared `BuildGroup()` path. Async dispatch keeps the heavy work
// off the publisher's tick.
//
// **Threading:** world-thread only. The bus's async drain runs on
// the world thread (from BotGuildMgr::Tick); BuildGroup() emits
// invites via per-bot intent queues which are lock-free.

#pragma once

#include "BotCoordinationBus.h"
#include <cstdint>
#include <vector>

class Player;

namespace Playerbot::V2 {

// Per-call request shape — what kind of group to build.
struct GroupRequest
{
    // Source signal — drives picker priorities + post-form behavior
    // (e.g. GuildEventForming triggers a walk-to-staging; LfgTank
    // triggers a queue start).
    CoordSignal source       = CoordSignal::None;
    // Required group size (5 dungeon, 10/25 raid, 10-40 BG).
    uint8       required     = 5;
    // Role mix. 0 = any; otherwise required count per slot.
    uint8       want_tank    = 0;
    uint8       want_healer  = 0;
    uint8       want_dps     = 0;
    // Faction mask (bit0=Alliance, bit1=Horde; 0 = any).
    uint32      faction_mask = 0;
    // Level bracket. 0 = ignore.
    uint8       level_min    = 0;
    uint8       level_max    = 0;
    // For guild-driven requests: only pick bots from this guild_id.
    // 0 = open to any bot.
    uint64      guild_id     = 0;
    // Leader candidate (the bot that initiated; if 0 the builder
    // picks the longest-active member as leader).
    uint64      leader_low   = 0;
    // Content id passed through to post-form behavior (bg_type_id /
    // dungeon_id / raid_map_id).
    uint32      content_id   = 0;
    // Arena skirmish team size (2/3/5) for ArenaTeamForming requests;
    // 0 = not an arena. Threaded through to the deferred BgQueueIntent so
    // the leader queues the formed group via the Arena queue id.
    uint8       arena_type   = 0;
};

class BotGroupBuilder
{
public:
    BotGroupBuilder() = default;

    // Wire bus subscriptions. Called once at Services::Init after the
    // bus is constructed.
    void RegisterSubscriptions(BotCoordinationBus& bus);

    // Form a group matching `req`. Returns number of bots actually
    // invited (≤ req.required). The leader sends invites via
    // InviteToGroupIntent; invitees auto-accept via the existing
    // group-invite handler. Subsequent post-form behavior (queue
    // start, walk-to-staging) is enqueued by the specific
    // signal handler that called BuildGroup.
    uint32 BuildGroup(GroupRequest const& req);

    // D.4: pending-finish drain. After BuildGroup emits invites, the
    // builder stamps a `PendingFinish` record; ~10s later (after the
    // invitees have had time to accept), DrainPending fires the
    // content-specific follow-up:
    //   - DungeonNight (content_id=3): leader emits LfgQueueIntent
    //     with a level-bracket-appropriate dungeon_id.
    //   - BgNight (content_id=4): leader emits BgQueueIntent with
    //     empty battlemaster (queue-from-anywhere path).
    //   - RaidNight (content_id=2): leader walks to a raid entrance
    //     (hardcoded raid_map per faction; D.5 may add per-content
    //     selection).
    //   - LfgRoleNeeded / BgTeamForming: existing queue path
    //     handles the queue start; D.4 just confirms group formed.
    // Called from the bus DrainAsync path via BotGuildMgr::Tick.
    // Records expire after kPendingFinishMaxAgeMs.
    void DrainPending(uint32 now_ms);

private:
    // Internal handlers — one per signal that maps to group formation.
    void OnGuildEventForming(CoordEvent const& ev);
    void OnLfgRoleNeeded(CoordEvent const& ev);
    void OnBgTeamForming(CoordEvent const& ev);
    void OnArenaTeamForming(CoordEvent const& ev);
    void OnOwnerSquadAssemble(CoordEvent const& ev);
    void OnWorldBossSpotted(CoordEvent const& ev);
    void OnMPlusKeyForming(CoordEvent const& ev);

    // Find candidate bots matching `req`. Returns ordered list:
    // leader candidate first (longest-active), then role-matching
    // members. Caller invites in order until required count is met.
    std::vector<uint64> PickCandidates(GroupRequest const& req);

    // D.4 pending-finish state. Bounded by the small number of guild
    // events firing per real-day (≤24 per faction-pair); resizes are
    // rare. Drained from BotGuildMgr::Tick via DrainPending() on the
    // world thread (single-writer).
    static constexpr uint32 kPendingFinishGraceMs    = 10u * 1000u;
    // MUST exceed the DrainPending cadence (BotGuildMgr::Tick, ~60s) by a wide
    // margin: at 60s it equalled the tick, so the FIRST drain pass saw age≈60s >
    // max and aged EVERY record out before the queue/walk could fire (live: all
    // arena + guild premades aged out at age_ms=60005, 0 queued). 4 min gives
    // several drain passes inside the window.
    static constexpr uint32 kPendingFinishMaxAgeMs   = 240u * 1000u;
    struct PendingFinish
    {
        uint64       leader_low;
        CoordSignal  source;
        uint32       content_id;   // event_kind / bg_type_id / arena bml / etc.
        uint8        level_min;
        uint8        level_max;
        uint8        arena_type;   // ArenaTeamForming: team size (2/3/5); 0 otherwise
        uint32       stamped_ms;
    };
    std::vector<PendingFinish> pending_finish_;
};

} // namespace Playerbot::V2
