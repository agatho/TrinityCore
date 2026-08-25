// BotGuildEventScheduler - Phase D event ticker.
//
// Runs from the per-frame manager Tick. Reads server local-time
// weekday + hour, consults `kBotGuildEvents`, and computes the
// currently-active event kind (if any). Publishes that into the
// BotGuildMgr per-guild active-event slot for the snapshot builder.
//
// "Pre-announce" callouts are handled here too — N minutes before
// event start, the scheduler asks the manager to pick one online
// officer per guild to emit a guild-chat heads-up. The actual chat
// emit happens via the bot's intent queue (so it goes through normal
// throttling).
//
// World-thread only.

#pragma once

#include <cstdint>

namespace Playerbot::V2 {

enum class GuildEventKind : uint8_t;
class BotGuildMgr;

class BotGuildEventScheduler
{
public:
    BotGuildEventScheduler() = default;

    // Called once per `BotGuildMgr::Tick`. Internally rate-limits to
    // 60s — finer resolution is wasted since event-start cron uses
    // whole-minute precision. Pass the manager so we can call
    // back into per-guild publishers without circular includes.
    void Tick(BotGuildMgr& mgr, uint32 now_ms);

    // Resolve the currently-active event (or None) for a generic
    // bot-managed guild. Doesn't take guild_id because Phase D events
    // are server-wide schedule, not per-guild — the per-guild filter
    // happens at consumption time (e.g. "only guilds with ≥3 online
    // bots respond to TavernParty"). Future per-guild events will
    // add a guild_id overload.
    static GuildEventKind CurrentEventNow();

    // True when right now is within [start - pre_announce_min, start)
    // for any event. Used by the scheduler to emit pre-event callouts
    // exactly once per event.
    static bool IsPreAnnounceWindow(GuildEventKind& out_kind,
                                    uint16_t& out_minutes_until);

private:
    // Last tick's emitted-pre-announce kind so we don't spam multiple
    // callouts within the same 15-min window. 0 = none yet.
    uint8_t  last_pre_announce_kind_ = 0;
    uint32_t last_pre_announce_ms_   = 0;
    // 60s rate-limit gate.
    uint32_t last_tick_ms_           = 0;
};

} // namespace Playerbot::V2
