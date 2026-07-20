// BotCoordinationBus - Typed pub-sub for cross-bot coordination.
//
// **Why this exists** (vs. the historical mod-playerbots pattern of
// every bot joining a custom chat channel and serializing structured
// data as strings):
//
//   - V2 already has a BotRegistry holding Player* + BotAI* for every
//     bot, plus a lock-free Services::Intents(other_bot_id).push(...)
//     queue. Coordination doesn't need to ride the chat broadcast
//     pipeline (packet/serialize/distance-filter/aura) — direct queue
//     push is one cache line.
//   - Typed events via the variant means no string parsing, no
//     encoding bugs, no surprise when a player joins the "p2p" channel
//     and sees garbage.
//   - World-thread ordering is deterministic; chat ordering races
//     with snapshot ticks.
//
// **What this layer adds** on top of direct intent pushes:
//
//   - Pub-sub semantics — publishers don't know subscribers; new
//     consumers attach without modifying publishers.
//   - Discovery — a subscriber declares "I handle LfgTankNeeded" and
//     any publisher can fire that signal.
//   - Hybrid dispatch — fast handlers run inline on the publisher's
//     thread (cheap, deterministic); heavy handlers run async on the
//     subscriber's tick (decoupled timing).
//
// **What this layer is NOT:**
//   - Not a general message bus. CoordSignal is a closed enum; new
//     signals require a code change (intentional — keeps the registry
//     scrutable).
//   - Not for per-event response state (subscribers track their own
//     state; the bus is fire-and-forget).
//   - Not durable. Bus events don't survive a worldserver restart;
//     they're tactical coordination, not persistent state.
//
// **Threading:**
//   - Subscribe() is called once at Services::Init from the world
//     thread; the subscriber list is then read-only.
//   - Publish() must be called from the world thread.
//     Sync handlers run inline; async handlers receive a copy of the
//     event via their bot's intent queue (push is lock-free; consume
//     happens on the world-thread intent drain).

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Playerbot::V2 {

// Closed enum: every category of cross-bot signal V2 currently
// supports. Add new signals here as new use cases land; consumers
// that don't handle a signal silently ignore it.
enum class CoordSignal : uint8_t
{
    None                = 0,

    // ---- LFG queue auto-fill ----
    // Published when a player joins the LFG queue and tank/healer/dps
    // slots are needed. BotQueueFiller (the consumer) JIT-spawns or
    // promotes online bots into the queue.
    LfgTankNeeded       = 1,
    LfgHealerNeeded     = 2,
    LfgDpsNeeded        = 3,

    // ---- BG queue team formation ----
    // Published when a BG queue needs filling. content_id = bg_type_id.
    BgTeamForming       = 10,

    // ---- Arena skirmish team formation ----
    // Published (SeedArenaMatches) when an arena skirmish should be seeded.
    // content_id = arena BattlemasterList id (4=Nagrand / 6=AllArenas /
    // 8=Ruins); arena_type = team size (2/3/5). BotGroupBuilder forms one
    // arena_type-sized group per published faction and the leader queues
    // the group for the arena skirmish. Distinct from BgTeamForming so the
    // builder can size the group to the arena bracket (not the BG default)
    // and route the queue through the Arena queue-id.
    ArenaTeamForming    = 11,

    // ---- Guild events ----
    // Published when a scheduled guild event starts (D.3). content_id
    // encodes the event kind (uint8 cast of GuildEventKind).
    // origin_low = the guild_id that's running the event.
    GuildEventForming   = 20,

    // ---- Owner squad ----
    // Published when an owner issues `;all run` or equivalent assemble
    // command. Subscribers (squad-control rules) form the group.
    // AGENT-AUDITED 2026-05-21: publisher not yet wired. Existing
    // owner-squad commands (`;all <cmd>` family in BotCommandParser)
    // route directly to bots without going through the bus — when a
    // dedicated "owner wants 5-man party formed around me" command
    // lands (e.g. `.playerbot party`), it should `bus.Publish(...)` a
    // CoordEvent with `origin_low = owner_guid_low` and `faction_mask`
    // set; the BotGroupBuilder subscriber will then pick 4 owned bots
    // and form the group around the owner.
    OwnerSquadAssemble  = 30,

    // ---- World boss / random encounters (publisher deferred) ----
    // Subscribers ready; publisher needs world-boss detection
    // infrastructure that doesn't exist yet. Stubbed here so the
    // wiring is complete when detection lands. AGENT-AUDITED 2026-05-21:
    // intentional pre-wired stub, not dead code.
    WorldBossSpotted    = 40,

    // ---- M+ keystone group ----
    // Subscribers ready; publisher needs M+ scheduling that doesn't
    // exist yet. Same stub status as WorldBossSpotted. AGENT-AUDITED
    // 2026-05-21: intentional pre-wired stub.
    MPlusKeyForming     = 50,
};

struct CoordEvent
{
    CoordSignal kind         = CoordSignal::None;
    uint64      origin_low   = 0;     // initiating bot/player guid_low (0 = system)
    uint32      content_id   = 0;     // signal-specific: bg_type_id / event_kind / dungeon_id / etc.
    uint8       level_min    = 0;     // minimum eligible bot level (0 = any)
    uint8       level_max    = 0;     // maximum eligible bot level (0 = any)
    uint32      faction_mask = 0;     // bit0=Alliance, bit1=Horde (0 = any)
    uint8       arena_type   = 0;     // ArenaTeamForming only: team size (2/3/5); 0 = not an arena
    std::string content_name;         // human-readable label for chat callouts ("Stormwind Adventurers raid")
};

enum class CoordDispatch : uint8_t
{
    // Subscriber handler runs INLINE on the publisher's thread (world
    // thread). Use for fast, allocation-light handlers — registry
    // lookups, BotAI state stamps, intent pushes. Avoid DB calls,
    // file I/O, anything > 100µs.
    Sync   = 0,
    // Subscriber handler runs on the next world-thread intent drain.
    // (Current implementation: queued in a manager-local std::vector
    // and drained via BotGuildMgr::Tick. Future: per-bot intent queue
    // when we add CoordDeliverIntent.) Use for heavy work — group
    // formation, mass invites, AH walks.
    Async  = 1,
};

class BotCoordinationBus
{
public:
    using Handler = std::function<void(CoordEvent const&)>;

    BotCoordinationBus() = default;

    // Register a handler. Called from Services::Init only. Each
    // signal can have N subscribers; Publish fans out to all of them.
    void Subscribe(CoordSignal signal, Handler handler, CoordDispatch mode);

    // Fire an event. World-thread only. Sync handlers execute inline
    // in registration order; async handlers are queued and drained on
    // the next DrainAsync() call (currently invoked from
    // BotGuildMgr::Tick at 60s cadence — heavy handlers don't need
    // finer granularity).
    void Publish(CoordEvent const& ev);

    // Run all queued async handlers. World-thread only. Called from
    // the manager Tick path. Idempotent — empty queue is a no-op.
    void DrainAsync();

private:
    struct Subscription
    {
        Handler        fn;
        CoordDispatch  mode;
    };
    // Per-signal subscriber list. Closed enum size = 60 max; vector
    // of vectors is plenty for the foreseeable subscriber count.
    std::vector<std::vector<Subscription>>  subs_by_signal_;
    // Async queue. Drained on each DrainAsync() call.
    std::vector<CoordEvent>                  pending_async_;
};

} // namespace Playerbot::V2
