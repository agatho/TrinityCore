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

#include "MapUpdater.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Map.h"
#include "Metric.h"
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <eh.h>
#include <windows.h>

// SEH-wrapping helper. MSVC requires functions that use __try/__except to
// have no C++ objects with destructors in scope, so this lives outside
// MapUpdateRequest::call (which has a TC_METRIC_TIMER with a destructor).
// Returns the SEH exception code, or 0 on success. Marked noexcept so the
// compiler doesn't try to compose a C++ unwind path around it.
extern "C" unsigned int SehSafeMapUpdate(Map* m, uint32 diff) noexcept;
unsigned int SehSafeMapUpdate(Map* m, uint32 diff) noexcept
{
    __try
    {
        m->Update(diff);
        return 0;
    }
    // EXCEPTION_ASSERTION_FAILURE (TC's ASSERT/ABORT, 0xC0000420) must
    // CONTINUE_SEARCH to the top-level crash handler: an assert means the
    // game state is already corrupt, and swallowing it here trades a clean
    // dump at the fault site for a delayed, misleading crash later.
    // (Observed 2026-06-11: a swallowed AuraApplication::_HandleEffect
    // assert left a Spell with m_executedCurrently set; the bot's eventual
    // logout hit the ~SpellEvent ABORT on the world thread with a stack
    // that pointed at session teardown instead of the aura bug.)
    __except (GetExceptionCode() == EXCEPTION_ASSERTION_FAILURE
              ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER)
    {
        return GetExceptionCode();
    }
}
#endif

class MapUpdateRequest
{
    private:

        Map& m_map;
        MapUpdater& m_updater;
        uint32 m_diff;

    public:

        MapUpdateRequest(Map& m, MapUpdater& u, uint32 d)
            : m_map(m), m_updater(u), m_diff(d)
        {
        }

        void call()
        {
            TC_METRIC_TIMER("map_update_time_diff", TC_METRIC_TAG("map_id", std::to_string(m_map.GetId())));
            // Wrap the map update in a try/catch so an uncaught exception
            // in any handler (spell scripts, BIH/DynamicTree builds,
            // ConditionMgr, etc.) doesn't terminate the world. Without
            // this guard the exception unwinds through MapUpdater
            // ::WorkerThread and the std::thread machinery and crashes
            // the process. Observed failure modes recently:
            //   * BIH::subdivide → std::logic_error("invalid node overlap")
            //     on dynamic GameObjectModel rebuilds when geometry data
            //     is malformed (TC core data bug, not playerbot)
            //   * std::bad_alloc inside DB worker callbacks under load
            //   * NaN / inf in path math throwing range_error
            // Each of these used to drop the entire world thread; logging
            // and continuing keeps the server alive while we hunt the
            // root cause per-map. update_finished() runs in the finally
            // path so the MapUpdater wait barrier isn't deadlocked.
#ifdef _WIN32
            // SEH wrapping via helper. Catches ACCESS_VIOLATION,
            // STACK_OVERFLOW, INT_DIVIDE_BY_ZERO, etc. that pass right
            // through C++ catch(...) under MSVC's default /EHsc. The
            // earlier _set_se_translator approach didn't work for the
            // same reason — the translator only runs when the SEH
            // unwinder reaches a /EHa-compiled frame. Direct __try /
            // __except is /EHsc-compatible.
            unsigned int seh_code = SehSafeMapUpdate(&m_map, m_diff);
            if (seh_code != 0)
            {
                TC_LOG_ERROR("maps",
                    "Map::Update SEH exception 0x{:08X} (map {} instance {}) — map skipped this tick",
                    seh_code, m_map.GetId(), m_map.GetInstanceId());
            }
#else
            // Linux / non-Windows: C++ try/catch only. POSIX signals
            // (SIGSEGV) bypass this and crash the process — fix at the
            // source as the bugs surface.
            try
            {
                m_map.Update(m_diff);
            }
            catch (std::exception const& e)
            {
                TC_LOG_ERROR("maps",
                    "Map::Update threw std::exception (map {} instance {}): {} — map skipped this tick",
                    m_map.GetId(), m_map.GetInstanceId(), e.what());
            }
            catch (...)
            {
                TC_LOG_ERROR("maps",
                    "Map::Update threw non-std exception (map {} instance {}) — map skipped this tick",
                    m_map.GetId(), m_map.GetInstanceId());
            }
#endif
            m_updater.update_finished();
        }
};

void MapUpdater::activate(size_t num_threads)
{
    for (size_t i = 0; i < num_threads; ++i)
        _workerThreads.emplace_back(&MapUpdater::WorkerThread, this);
}

void MapUpdater::deactivate()
{
    _cancelationToken = true;

    wait();

    _queue.Cancel();

    for (auto& thread : _workerThreads)
        thread.join();
}

void MapUpdater::wait()
{
    // Block until every scheduled map update for this tick has actually
    // finished. This barrier is load-bearing: TC's whole map model assumes
    // one thread touches a given Map at a time, and MapManager::Update relies
    // on this wait to guarantee no worker is still inside Map::Update before
    // the world thread proceeds to DelayedUpdate / the next tick.
    //
    // A previous version used a 30s bounded wait_for() that, on timeout,
    // force-reset pending_requests and returned WHILE a worker was still
    // running. AddressSanitizer proved that this is what caused the random
    // server crashes: the still-running worker and the next tick's main
    // thread (e.g. a PlayerbotV2 login doing AddPlayerToMap -> terrain query)
    // both entered DynamicMapTree::balance() on the same map, double-freeing
    // the BIH's internal vector and corrupting the heap. The corruption then
    // surfaced later as unrelated-looking crashes (std::any / std::string
    // move ctors in boss scripts and SmartAI). The "stuck counter" that
    // motivated the timeout was itself a symptom: a worker whose Map::Update
    // hit the corruption crashed without calling update_finished().
    //
    // So we wait unconditionally. DynamicTree access is additionally guarded
    // by a mutex (see DynamicTree.cpp) as defense in depth.
    std::unique_lock lock(_lock);
    _condition.wait(lock, [&] { return pending_requests == 0; });
}

void MapUpdater::schedule_update(Map& map, uint32 diff)
{
    std::scoped_lock lock(_lock);

    ++pending_requests;

    _queue.Push(new MapUpdateRequest(map, *this, diff));
}

bool MapUpdater::activated() const
{
    return !_workerThreads.empty();
}

void MapUpdater::update_finished()
{
    std::scoped_lock lock(_lock);

    --pending_requests;

    _condition.notify_all();
}

void MapUpdater::WorkerThread()
{
    LoginDatabase.WarnAboutSyncQueries(true);
    CharacterDatabase.WarnAboutSyncQueries(true);
    WorldDatabase.WarnAboutSyncQueries(true);
    HotfixDatabase.WarnAboutSyncQueries(true);

    // (SEH translation is handled per-request via SehSafeMapUpdate()
    // because _set_se_translator only fires on /EHa-compiled frames;
    // TC builds with /EHsc so the translator never gets invoked.)

    while (true)
    {
        MapUpdateRequest* request = nullptr;

        _queue.WaitAndPop(request);

        if (_cancelationToken)
            return;

        request->call();

        delete request;
    }
}
