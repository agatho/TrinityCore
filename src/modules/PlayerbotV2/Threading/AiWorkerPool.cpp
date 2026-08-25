#include "AiWorkerPool.h"
#include "SnapshotPublisher.h"
#include "IntentQueue.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshot.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotActivityTier.h"  // ClassifyTier / TickPeriodFor for staleness gate
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotRegistry.h"
#include "Group/GroupSnapshot.h"
#include "Services.h"
#include "Diagnostics/PerfCounters.h"
#include "Timer.h"     // getMSTime for per-tick latency recording
#include "GameTime.h"  // GameTime::GetGameTimeMS — same base as snapshot.published_at_ms
#include <algorithm>

namespace Playerbot {

namespace {
size_t ResolveWorkerCount(size_t requested)
{
    if (requested > 0) return requested;
    const unsigned hwc = std::max(1u, std::thread::hardware_concurrency());
    // Auto = up to 24 threads. Modern dedicated TC servers run on 16-
    // to 32-thread CPUs; the previous min(hwc, 8) ceiling left those
    // cores idle. MapUpdate workers + Asio handlers + DB pools still
    // get the remainder. Operator can override explicitly via
    // Playerbot.AiWorkerThreads = <N> for shared-host setups.
    return std::min<size_t>(hwc, 24);
}
} // anonymous

AiWorkerPool::AiWorkerPool(size_t worker_count)
    : desired_count_(ResolveWorkerCount(worker_count))
{}

AiWorkerPool::~AiWorkerPool() { stop(); }

void AiWorkerPool::start()
{
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return;
    stop_requested_.store(false, std::memory_order_release);
    workers_.reserve(desired_count_);
    for (size_t i = 0; i < desired_count_; ++i)
        workers_.emplace_back(&AiWorkerPool::run_worker, this);
}

void AiWorkerPool::stop()
{
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) return;
    {
        std::lock_guard lk(mtx_);
        stop_requested_.store(true, std::memory_order_release);
    }
    cv_.notify_all();
    for (auto& t : workers_)
        if (t.joinable()) t.join();
    workers_.clear();
}

void AiWorkerPool::schedule_tick(BotId bot)
{
    {
        std::lock_guard lk(mtx_);
        // Drop the schedule if a previous tick for this bot is still queued
        // or executing. Without this, two workers could pick up the same
        // bot in parallel and race on its BotAI state.
        if (!in_flight_.insert(bot).second)
            return;
        queue_.push_back(bot);
        stats_.queue_high_watermark = std::max<uint64_t>(stats_.queue_high_watermark, queue_.size());
    }
    cv_.notify_one();
}

void AiWorkerPool::schedule_ticks(std::span<BotId const> bots)
{
    if (bots.empty()) return;
    size_t enqueued = 0;
    {
        std::lock_guard lk(mtx_);
        for (BotId bot : bots)
        {
            if (!in_flight_.insert(bot).second) continue;
            queue_.push_back(bot);
            ++enqueued;
        }
        stats_.queue_high_watermark = std::max<uint64_t>(stats_.queue_high_watermark, queue_.size());
    }
    // notify_all() once at the end. notify_one() in a per-bot loop would
    // wake one worker per bot — N futex wakes/sec. With notify_all() the
    // pool drains via worker re-arm self-wakes (cv_.wait re-checks the
    // queue when re-acquiring mtx_). For a batch of 200+ bots, this is
    // measurably cheaper at scale.
    if (enqueued > 0)
        cv_.notify_all();
}

void AiWorkerPool::run_worker()
{
    for (;;)
    {
        BotId bot = 0;
        {
            std::unique_lock lk(mtx_);
            cv_.wait(lk, [&] { return stop_requested_.load(std::memory_order_acquire) || !queue_.empty(); });
            if (stop_requested_.load(std::memory_order_acquire) && queue_.empty())
                return;
            bot = queue_.front();
            queue_.pop_front();
        }

        try
        {
            run_one(bot);
            ++stats_.ticks_total;
        }
        catch (...)
        {
            ++stats_.exceptions_total;
            // run_one() already wraps tick() in try/catch and reports via
            // PerfCounters. This catches only what escapes that — system-
            // level setup failures. Mirror to PerfCounters too so all bot-
            // facing exceptions land in one number for SystemStatus.
            if (Services::Initialized())
                Services::Perf().record_exception();
        }
        // Allow re-scheduling now that this bot's tick is complete. Done
        // even on exception so a permanently-broken bot stays re-tickable.
        {
            std::lock_guard lk(mtx_);
            in_flight_.erase(bot);
        }
    }
}

void AiWorkerPool::run_one(BotId bot)
{
    if (!Services::Initialized()) return;

    auto& reg     = Services::Registry();
    BotAI* ai     = reg.ai(bot);
    auto*  queue  = reg.intents(bot);
    auto*  next   = reg.next_intent_id(bot);
    if (!ai || !queue || !next) return;

    auto snap = Services::Snapshots().latest(bot);
    if (!snap) return;     // No snapshot published yet — bot hasn't fully spawned.

    // Staleness guard. Under load the snapshot-build pipeline can fall behind
    // the AI worker pool, leaving the most-recently-published snapshot
    // arbitrarily old. Acting on it makes the bot decide against a world
    // state that no longer exists (moved enemies, dead targets, changed HP).
    // Skip the tick when the snapshot's age exceeds a tier-aware threshold —
    // ~3× the bot's AI period so a single dropped/late build is tolerated,
    // but capped at 1s so even Idle/Hibernate bots never run on grossly stale
    // data. published_at_ms is stamped from GameTime::GetGameTimeMS() in the
    // builder, so we compare against the same time base here. The bot simply
    // re-ticks against a fresh snapshot on the next scheduler pass.
    {
        const uint32 now_ms = GameTime::GetGameTimeMS();
        const uint32 age_ms = now_ms - snap->published_at_ms;   // unsigned wrap-safe diff
        const int64  period = TickPeriodFor(ClassifyTier(*snap)).count();
        const uint32 max_age = static_cast<uint32>(std::min<int64>(period * 3, 1000));
        // EXEMPT DEAD BOTS from the staleness skip. The guard exists to stop a bot
        // ACTING on stale WORLD state (moved enemies, dead targets, changed HP). A
        // dead bot's recovery FSM (release -> corpse-run -> accept-rez -> bounded
        // watchdog, State_Dead.cpp) reads only its OWN death timers and the server
        // GameTime clock, never enemy state — so ticking it on a stale snapshot is
        // harmless. This is a CORRECTNESS fix, not an optimization: a dead bot
        // ramps to the parked Idle->Hibernate tier whose 500-2000ms build cadence
        // routinely exceeds this 1000ms cap, so WITH the guard a wiped group's
        // corpses get their recovery ticks skipped for MINUTES and the run
        // deadlocks. Observed 2026-06-26: a dead tank sat in dead:release_pending
        // for 190s (snapshot parked at 2s cadence, every tick stale-skipped, so the
        // 20s release watchdog never ran), and the whole Deadmines group hung on
        // the tank-advance group_not_ready gate until a wake event refreshed it.
        // A dead bot ticking on slightly-stale data lets its GameTime-keyed
        // watchdog fire on schedule and recovery complete in bounded time.
        if (snap->vitals.is_alive && age_ms > max_age)
        {
            ++stats_.stale_skips_total;
            return;
        }
    }

    auto gsnap = Services::Snapshots().latest_group(bot);
    BotSnapshotView   view(*snap);
    GroupSnapshotView group_view = gsnap ? GroupSnapshotView(*gsnap) : GroupSnapshotView{};
    BotIntentEmitter  emit(queue, bot, snap->version, next, ai);

    // Guard the worker thread against rule-level exceptions: a single
    // bad predicate (null deref on stale ObjectGuid lookup, vector OOB) would
    // otherwise unwind through the worker and kill it. Counted in PerfCounters
    // so a rising exception rate surfaces in SystemStatus.
    const uint32 tick_start = getMSTime();
    try
    {
        ai->tick(view, group_view, emit);
    }
    catch (...)
    {
        Services::Perf().record_exception();
    }
    // Record per-bot tick latency. Bucketed histogram in PerfCounters
    // surfaces tick_p50 / tick_p99 via /perf — without this the field
    // stays at zero. Worker pool is the natural recording site since
    // it knows when a single bot's AI tick begins/ends.
    const uint32 tick_end = getMSTime();
    Services::Perf().record_tick_latency(
        std::chrono::milliseconds{int32(tick_end - tick_start)});
}

} // namespace Playerbot
