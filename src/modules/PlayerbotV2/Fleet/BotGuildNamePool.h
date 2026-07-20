// BotGuildNamePool - Curated guild name candidates per faction.
//
// The Phase A.2 charter FSM picks a name from this pool the moment
// the founder is elected (so the name is reserved server-side via
// `bot_guild_name_reserved` and can't be claimed by a parallel
// founder), then commits it to `bot_guild_meta` on charter submit.
//
// Pool size target: ~30 candidate names per faction. With a 6-guild
// per-faction cap that leaves plenty of rotation as old bot guilds
// get disbanded and re-founded over the server's lifetime, without
// duplicating names. Real player guilds may have claimed some pool
// names already; PickAvailableName() filters those out via
// `GuildMgr::GetGuildByName` and the reservation table at lookup
// time, so collisions never trigger a second pick at charter submit.
//
// World-thread only.

#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace Playerbot::V2 {

class BotGuildNamePool
{
public:
    enum Faction : uint8
    {
        FACTION_ALLIANCE = 0,
        FACTION_HORDE    = 1,
    };

    BotGuildNamePool() = default;

    // Loads candidate names + currently-reserved set from
    // bot_guild_name_reserved. Call once at Services::Init.
    void LoadFromDb();

    // Pick the first candidate name not already taken by an existing
    // guild AND not currently reserved. Atomically inserts a
    // reservation row for `founder_low`. Returns empty string on
    // exhaustion (extremely unlikely; ~30 names × 2 factions).
    std::string PickAndReserve(Faction f, uint64 founder_low);

    // Release a previously-reserved name. Called on charter abort
    // (FSM timeout, founder logged out, etc.) and at successful
    // submit (after `bot_guild_meta` INSERT). Idempotent.
    void Release(std::string const& name);

    // Hygiene sweep — drops reservations older than `max_age_sec`
    // seconds. Catches orphans from crashed founders. Called from
    // BotGuildMgr's periodic tick.
    void SweepStale(uint32 max_age_sec);

private:
    // Read-only after LoadFromDb. Per-faction static pool.
    std::vector<std::string> const& candidate_pool(Faction f) const;

    mutable std::mutex                       mtx_;
    std::vector<std::string>                 reserved_;     // simple list; ~12 reservations at peak
};

} // namespace Playerbot::V2
