// BotGuildEventScheduler - see header.

#include "BotGuildEventScheduler.h"

#include "BotCoordinationBus.h"
#include "BotGuildEvent.h"
#include "BotGuildMgr.h"
#include "Log.h"

#include "../Services.h"

#include <ctime>

namespace Playerbot::V2 {

namespace {

struct LocalNow
{
    uint8_t wday;     // 0=Sun..6=Sat
    uint8_t hour;     // 0..23
    uint8_t minute;   // 0..59
};

LocalNow ServerLocalNow()
{
    LocalNow ln{};
    std::time_t t = std::time(nullptr);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    ln.wday   = static_cast<uint8_t>(tm_buf.tm_wday);
    ln.hour   = static_cast<uint8_t>(tm_buf.tm_hour);
    ln.minute = static_cast<uint8_t>(tm_buf.tm_min);
    return ln;
}

bool EventActiveNow(GuildEventDescriptor const& d, LocalNow const& now,
                    uint16_t& out_minutes_into)
{
    if (((d.weekday_mask >> now.wday) & 1u) == 0) return false;
    const uint32_t now_mins   = uint32_t(now.hour) * 60u + now.minute;
    const uint32_t start_mins = uint32_t(d.start_hour) * 60u;
    if (now_mins < start_mins) return false;
    const uint32_t delta = now_mins - start_mins;
    if (delta >= d.duration_min) return false;
    out_minutes_into = static_cast<uint16_t>(delta);
    return true;
}

bool EventInPreAnnounceNow(GuildEventDescriptor const& d, LocalNow const& now,
                           uint16_t& out_minutes_until)
{
    if (d.pre_announce_min == 0) return false;
    if (((d.weekday_mask >> now.wday) & 1u) == 0) return false;
    const uint32_t now_mins   = uint32_t(now.hour) * 60u + now.minute;
    const uint32_t start_mins = uint32_t(d.start_hour) * 60u;
    if (now_mins >= start_mins) return false;
    const uint32_t until = start_mins - now_mins;
    if (until > d.pre_announce_min) return false;
    out_minutes_until = static_cast<uint16_t>(until);
    return true;
}

} // anonymous

GuildEventKind BotGuildEventScheduler::CurrentEventNow()
{
    const LocalNow now = ServerLocalNow();
    for (auto const& d : kBotGuildEvents)
    {
        uint16_t mins_into = 0;
        if (EventActiveNow(d, now, mins_into))
            return d.kind;
    }
    return GuildEventKind::None;
}

bool BotGuildEventScheduler::IsPreAnnounceWindow(GuildEventKind& out_kind,
                                                 uint16_t& out_minutes_until)
{
    const LocalNow now = ServerLocalNow();
    for (auto const& d : kBotGuildEvents)
    {
        uint16_t until = 0;
        if (EventInPreAnnounceNow(d, now, until))
        {
            out_kind = d.kind;
            out_minutes_until = until;
            return true;
        }
    }
    return false;
}

void BotGuildEventScheduler::Tick(BotGuildMgr& mgr, uint32 now_ms)
{
    // 60s rate limit.
    if (last_tick_ms_ != 0 && (now_ms - last_tick_ms_) < 60u * 1000u)
        return;
    last_tick_ms_ = now_ms;

    // Publish current active event to the manager (which the snapshot
    // builder reads per-bot).
    const GuildEventKind prev_active = static_cast<GuildEventKind>(mgr.ActiveEventKind());
    const GuildEventKind active = CurrentEventNow();
    mgr.SetActiveEventKind(active);

    // Bus publish on edge transition None→Active for events that need
    // group formation (Raid/Dungeon/BG nights). TavernParty doesn't
    // form a group (everyone walks to the tavern individually). The
    // per-guild GroupBuilder picks bots; one signal per faction per
    // guild keeps the picker scoped.
    if (active != prev_active && active != GuildEventKind::None &&
        active != GuildEventKind::TavernParty &&
        Services::Initialized())
    {
        // Per-faction level brackets: raid 60+, dungeon 15+, bg 10+.
        // Values are placeholders; D.4 may add per-content-id tuning.
        uint8 lvl_min = 10;
        switch (active)
        {
            case GuildEventKind::RaidNight:    lvl_min = 60; break;
            case GuildEventKind::DungeonNight: lvl_min = 15; break;
            case GuildEventKind::BgNight:      lvl_min = 10; break;
            default: break;
        }
        for (uint8 fi = 0; fi < 2; ++fi)
        {
            for (uint64 gid : mgr.ActiveGuildIds(static_cast<BotGuildMgr::Faction>(fi)))
            {
                CoordEvent ev{};
                ev.kind         = CoordSignal::GuildEventForming;
                ev.origin_low   = gid;
                ev.content_id   = static_cast<uint32>(active);
                ev.level_min    = lvl_min;
                ev.level_max    = 0;
                ev.faction_mask = 1u << fi;
                ev.content_name = (active == GuildEventKind::RaidNight)    ? "raid night"
                                 :(active == GuildEventKind::DungeonNight) ? "dungeon night"
                                                                            : "BG night";
                Services::Coordination().Publish(ev);
            }
        }
    }

    // Pre-announce: if we're in a pre-announce window AND we haven't
    // already announced this kind in the current window, ask the
    // manager to queue a callout from an online officer of each
    // bot-managed guild.
    GuildEventKind pre_kind = GuildEventKind::None;
    uint16_t until_min = 0;
    if (IsPreAnnounceWindow(pre_kind, until_min))
    {
        const uint8_t kind_byte = static_cast<uint8_t>(pre_kind);
        // Dedup: same kind, within the last 60 min (covers the full
        // pre-announce window without re-firing each tick).
        const bool same_window =
            last_pre_announce_kind_ == kind_byte &&
            (now_ms - last_pre_announce_ms_) < 60u * 60u * 1000u;
        if (!same_window)
        {
            mgr.QueueEventPreAnnounce(pre_kind, until_min);
            last_pre_announce_kind_ = kind_byte;
            last_pre_announce_ms_   = now_ms;
            TC_LOG_INFO("playerbot.v2",
                "[BotGuildEventScheduler] Pre-announced event kind={} in {} min",
                kind_byte, until_min);
        }
    }
    else
    {
        // Reset the pre-announce dedup once outside the window so the
        // next week's event re-arms.
        last_pre_announce_kind_ = 0;
    }
}

} // namespace Playerbot::V2
