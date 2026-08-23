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

#ifndef TRINITYCORE_ELAPSED_TIMER_MGR_H
#define TRINITYCORE_ELAPSED_TIMER_MGR_H

#include "Define.h"
#include "Duration.h"
#include "ObjectGuid.h"
#include "Optional.h"
#include <unordered_map>
#include <vector>

class Map;
class Player;

// ---------------------------------------------------------------------------------------------
// Elapsed ("world") timers - the client's count-UP timer primitive, distinct from the count-DOWN
// SMSG_START_TIMER / SMSG_STOP_TIMER pair.
//
// Wire (all four opcodes flipped to STATUS_NEVER alongside this manager):
//   SMSG_START_ELAPSED_TIMER  0x4200AA  { int64 CurrentDuration; uint32 TimerID; }
//   SMSG_START_ELAPSED_TIMERS 0x4200AC  { uint32 Count; Count x <as above> }   - zone-in resync
//   SMSG_STOP_ELAPSED_TIMER   0x4200AB  { uint32 TimerID; bit KeepTimer; }
//   SMSG_STOP_TIMER           0x42003E  { uint32 CountdownTimerType }
//
// IMPORTANT - what the 68275 client will actually render:
//
// TimerID is a WorldElapsedTimer.db2 row id, and the *type* the client switches on comes from that
// DB2 row, not from the wire. WorldElapsedTimer.db2 has exactly five rows in 12.0.7:
//
//     ID  Name                                   Type
//      1  "Challenge Mode Time"                  1 (ChallengeMode)
//      2  "Proving Grounds Timer"                2 (ProvingGround)
//      3  "Event - AO - Trial Timer"             0 (None)
//      4  "Noodle Stand Shift"                   0 (None)
//     12  "Tanaan 6.2 - Scenario - Event Timer"  0 (None)
//
// The only consumer in the entire 68275 client UI is ScenarioTimerMixin:CheckTimers
// (Blizzard_ScenarioObjectiveTracker.lua), and it renders a timer only when its type is
// ChallengeMode or ProvingGround - every other type falls through to StopTimer() and draws nothing.
//
// So this is NOT a generic on-screen countdown primitive that arbitrary systems (warfronts, delves,
// scenario steps) can drive: there is no DB2 row and no client renderer for them. Inventing a
// TimerID for those systems would put a value on the wire that resolves to nothing client-side.
// Only CHALLENGE_MODE is wired today; PROVING_GROUND is listed for completeness but Proving Grounds
// is not implemented server-side.
// ---------------------------------------------------------------------------------------------
enum WorldElapsedTimerId : uint32
{
    WORLD_ELAPSED_TIMER_CHALLENGE_MODE  = 1,
    WORLD_ELAPSED_TIMER_PROVING_GROUND  = 2
};

// Tracks which elapsed timers each player currently has running, so that they can be replayed on
// zone-in / relog. Deliberately NOT persisted - see the note on SendActiveTimers().
class TC_GAME_API ElapsedTimerMgr
{
public:
    ElapsedTimerMgr() = default;
    ~ElapsedTimerMgr() = default;

    ElapsedTimerMgr(ElapsedTimerMgr const&) = delete;
    ElapsedTimerMgr& operator=(ElapsedTimerMgr const&) = delete;

    static ElapsedTimerMgr* instance();

    // Start (or re-base) a timer for one player. elapsed is how much time has already run; the
    // client free-runs its own clock from that baseline, so passing the true elapsed makes a
    // mid-run join land on the correct value.
    void StartTimer(Player* player, uint32 timerId, Seconds elapsed);
    // Stop a timer for one player. keepTimer leaves the final value on screen instead of hiding it.
    void StopTimer(Player* player, uint32 timerId, bool keepTimer);

    // Same, for every player currently on a map (the normal instance-wide case).
    void StartTimerForMap(Map* map, uint32 timerId, Seconds elapsed);
    void StopTimerForMap(Map* map, uint32 timerId, bool keepTimer);

    // Resynchronise on zone-in / relog. Re-derives from the owning system first (see the .cpp),
    // then pushes the whole set as SMSG_START_ELAPSED_TIMERS, which is what the client's
    // PLAYER_ENTERING_WORLD -> GetWorldElapsedTimers() path expects.
    void SendActiveTimers(Player* player);

    // Drop all bookkeeping for a player (logout / session teardown).
    void RemoveAllTimers(ObjectGuid const& playerGuid);

    // Time this timer has been running for the given player, if it is running at all.
    Optional<Seconds> GetElapsed(ObjectGuid const& playerGuid, uint32 timerId) const;

private:
    struct ActiveTimer
    {
        uint32 TimerID = 0;
        // Monotonic instant the timer is considered to have started at; elapsed is always derived
        // from this so a resync can never drift from the first push.
        TimePoint StartTime;
    };

    // Records/updates the entry and returns the value that should go on the wire.
    Seconds Register(ObjectGuid const& playerGuid, uint32 timerId, Seconds elapsed);
    void Unregister(ObjectGuid const& playerGuid, uint32 timerId);

    std::unordered_map<ObjectGuid, std::vector<ActiveTimer>> _timers;
};

#define sElapsedTimerMgr ElapsedTimerMgr::instance()

#endif // TRINITYCORE_ELAPSED_TIMER_MGR_H
