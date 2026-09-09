// AiWorkerPool - N worker threads consuming a tick-task queue. Each task is
// "run AI tick for bot X using snapshot Y, push intents to queue Z." The pool
// is otherwise dumb — TickScheduler decides who runs and when.
//
// CONTRACTS.md §5.3.

#pragma once

#include "Bot/BotTypes.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <span>
#include <thread>
#include <unordered_set>
#include <vector>

namespace Playerbot {

struct PerfStats
{
    uint64_t ticks_total           = 0;
    uint64_t exceptions_total      = 0;
    uint64_t queue_high_watermark  = 0;
    // AI ticks skipped because the published snapshot was grossly stale
    // (age > ~3× the bot's tier AI period, capped at 1s). Acting on an
    // arbitrarily-old snapshot under load produces decisions against a
    // world state that no longer exists; skipping lets the bot re-tick
    // against a fresh snapshot on the next pass.
    uint64_t stale_skips_total     = 0;
};

class AiWorkerPool
{
public:
    explicit AiWorkerPool(size_t worker_count = 0);    // 0 = auto
    ~AiWorkerPool();

    void start();
    void stop();
    bool running() const { return running_.load(std::memory_order_acquire); }

    // Called by TickScheduler when a bot should tick. Lock-protected enqueue.
    void schedule_tick(BotId bot);

    // Batched enqueue — takes the mutex ONCE for the entire batch instead
    // of N times. At 1000+ bots × ~50 Hz × ~30% due-ratio = ~15K
    // mutex acq/sec per world thread; the batched path collapses that
    // to one acq per tick. Also uses notify_all() once at the end so the
    // worker pool drains without per-bot wakeups under load. Per-bot
    // dedup against in_flight_ is preserved.
    void schedule_ticks(std::span<BotId const> bots);

    PerfStats stats() const { return stats_; }
    size_t    worker_count() const { return workers_.size(); }

private:
    void run_worker();
    void run_one(BotId bot);

    size_t                  desired_count_ = 0;
    std::vector<std::thread> workers_;
    mutable std::mutex      mtx_;
    std::condition_variable cv_;
    std::deque<BotId>       queue_;
    // Set of bots currently in queue OR being processed by a worker. Used to
    // dedup re-schedules: if a bot is still in flight from a previous tick,
    // we drop the new schedule rather than enqueue a duplicate (which would
    // let two workers process the same bot in parallel and race on BotAI).
    std::unordered_set<BotId> in_flight_;
    std::atomic<bool>       running_{false};
    std::atomic<bool>       stop_requested_{false};
    PerfStats               stats_{};
};

} // namespace Playerbot
