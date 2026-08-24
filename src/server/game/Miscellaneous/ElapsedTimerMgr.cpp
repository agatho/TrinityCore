/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ElapsedTimerMgr.h"
#include "ChallengeMode.h"
#include "GameTime.h"
#include "Map.h"
#include "MiscPackets.h"
#include "Player.h"
#include <algorithm>

// ---------------------------------------------------------------------------------------------
// PERSISTENCE: deliberately none. Elapsed timers are ephemeral by design, for three reasons.
//
//  1. They are derived state, never authoritative. The only thing that decides whether a run timer
//     should be showing is the owning system - today ChallengeMode, which lives on the InstanceMap
//     and is itself not persisted. Persisting the timer would let it outlive its own source of
//     truth and resurrect a countdown for a run that no longer exists.
//  2. The client does not expect the server to remember them. On PLAYER_ENTERING_WORLD it calls
//     GetWorldElapsedTimers() and repopulates from whatever the server pushes right then - i.e.
//     retail's own model is "re-derive and re-push on every zone-in", which is exactly
//     SendActiveTimers().
//  3. Re-deriving is strictly more accurate than restoring: it reads the live elapsed value out of
//     the owning system rather than a stored snapshot that could have drifted.
// ---------------------------------------------------------------------------------------------

ElapsedTimerMgr* ElapsedTimerMgr::instance()
{
    static ElapsedTimerMgr instance;
    return &instance;
}

Seconds ElapsedTimerMgr::Register(ObjectGuid const& playerGuid, uint32 timerId, Seconds elapsed)
{
    // Back-date the start instant so that every later read derives the same timeline.
    TimePoint const startTime = GameTime::Now() - elapsed;

    std::vector<ActiveTimer>& timers = _timers[playerGuid];
    auto itr = std::ranges::find(timers, timerId, &ActiveTimer::TimerID);
    if (itr != timers.end())
        itr->StartTime = startTime;
    else
        timers.emplace_back(ActiveTimer{ .TimerID = timerId, .StartTime = startTime });

    return elapsed;
}

void ElapsedTimerMgr::Unregister(ObjectGuid const& playerGuid, uint32 timerId)
{
    auto mapItr = _timers.find(playerGuid);
    if (mapItr == _timers.end())
        return;

    std::erase_if(mapItr->second, [timerId](ActiveTimer const& timer) { return timer.TimerID == timerId; });
    if (mapItr->second.empty())
        _timers.erase(mapItr);
}

void ElapsedTimerMgr::StartTimer(Player* player, uint32 timerId, Seconds elapsed)
{
    if (!player)
        return;

    WorldPackets::Misc::StartElapsedTimer packet;
    packet.Timer.TimerID = timerId;
    packet.Timer.CurrentDuration = Register(player->GetGUID(), timerId, elapsed);
    player->SendDirectMessage(packet.Write());
}

void ElapsedTimerMgr::StopTimer(Player* player, uint32 timerId, bool keepTimer)
{
    if (!player)
        return;

    Unregister(player->GetGUID(), timerId);

    WorldPackets::Misc::StopElapsedTimer packet;
    packet.TimerID = timerId;
    packet.KeepTimer = keepTimer;
    player->SendDirectMessage(packet.Write());
}

void ElapsedTimerMgr::StartTimerForMap(Map* map, uint32 timerId, Seconds elapsed)
{
    if (!map)
        return;

    map->DoOnPlayers([this, timerId, elapsed](Player* player) { StartTimer(player, timerId, elapsed); });
}

void ElapsedTimerMgr::StopTimerForMap(Map* map, uint32 timerId, bool keepTimer)
{
    if (!map)
        return;

    map->DoOnPlayers([this, timerId, keepTimer](Player* player) { StopTimer(player, timerId, keepTimer); });
}

void ElapsedTimerMgr::SendActiveTimers(Player* player)
{
    if (!player)
        return;

    ObjectGuid const guid = player->GetGUID();

    // Rebuild from the owning system rather than replaying bookkeeping: a player who has just left
    // a running instance must not carry a stale timer into the new map, and a player who has just
    // zoned INTO a running instance was never registered by ChallengeMode::Start and would
    // otherwise see no timer at all (this is the mid-run join case that was previously broken).
    _timers.erase(guid);

    if (Map* map = player->GetMap())
        if (InstanceMap* instanceMap = map->ToInstanceMap())
            if (ChallengeMode const* challenge = instanceMap->GetChallengeMode())
                if (challenge->IsActive())
                    Register(guid, WORLD_ELAPSED_TIMER_CHALLENGE_MODE, Seconds(challenge->GetElapsedMs() / IN_MILLISECONDS));

    WorldPackets::Misc::StartElapsedTimers packet;

    if (auto itr = _timers.find(guid); itr != _timers.end())
    {
        TimePoint const now = GameTime::Now();
        packet.Timers.reserve(itr->second.size());
        for (ActiveTimer const& timer : itr->second)
        {
            WorldPackets::Misc::ElapsedTimer& entry = packet.Timers.emplace_back();
            entry.TimerID = timer.TimerID;
            entry.CurrentDuration = std::chrono::duration_cast<Seconds>(now - timer.StartTime);
        }
    }

    // An empty list is meaningful and is sent on purpose: the client's CheckTimers() with no timers
    // hides any widget left over from a previous map.
    player->SendDirectMessage(packet.Write());
}

void ElapsedTimerMgr::RemoveAllTimers(ObjectGuid const& playerGuid)
{
    _timers.erase(playerGuid);
}

Optional<Seconds> ElapsedTimerMgr::GetElapsed(ObjectGuid const& playerGuid, uint32 timerId) const
{
    auto mapItr = _timers.find(playerGuid);
    if (mapItr == _timers.end())
        return {};

    auto itr = std::ranges::find(mapItr->second, timerId, &ActiveTimer::TimerID);
    if (itr == mapItr->second.end())
        return {};

    return std::chrono::duration_cast<Seconds>(GameTime::Now() - itr->StartTime);
}
