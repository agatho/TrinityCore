// SnapshotBuildPool - A fixed, persistent worker pool used to parallelize the
// per-tick snapshot Build phase by Map* (PlayerbotV2 #5 Phase 4).
//
// Unlike AiWorkerPool (a long-lived producer/consumer queue that runs the AI
// tick), this pool is a *fork-join barrier* primitive: the world thread submits
// a batch of independent tasks (one per Map* partition) via run_and_wait(),
// the persistent workers drain them in parallel, and run_and_wait() returns
// only after every task finished.
//
// LANE PINNING — each task carries a `lane` index; tasks are dispatched to
// worker `lane % worker_count`. The world thread services lane 0's queue plus
// any lane while waiting (work-stealing as a fallback so no core idles), but in
// the common case task L runs on the SAME worker thread every tick. That
// stability is what keeps BotSnapshotBuilder's thread_local recycle / LoS pools
// warm for a given Map* across ticks (the ping-pong use_count reuse invariant).
// An occasional steal just costs one fresh make_shared that tick — never a
// crash, never wrong data.
//
// Persistence matters: workers are created ONCE in start() and reused for every
// batch, so the thread_local scratch buffers and thread_local recycle/LoS
// caches in BotSnapshotBuilder amortize across ticks (a spawn-per-task model
// would defeat that and is explicitly avoided — see the audit).
//
// THREADING CONTRACT
//   * run_and_wait() is called ONLY from the world thread, strictly inside the
//     single-threaded snapshot window (no concurrent World::Add/RemoveSession,
//     no Map::Update). Tasks read live core objects read-only; that is only
//     safe in that window (HARD RULE 6).
//   * Tasks must be independent: each task owns one Map's bots; a single bot is
//     therefore only ever touched by one worker per batch (partition rule).
//   * The pool does NOT re-enter: run_and_wait() must not be called from within
//     a task. (It is not.)

#pragma once

#include "Define.h"   // uint32 and friends
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace Playerbot {

class SnapshotBuildPool
{
public:
    using Task = std::function<void()>;

    struct LaneTask
    {
        uint32 lane = 0;   // preferred worker lane (assignment site keeps it stable per Map*)
        Task   fn;
    };

    // worker_count == 0 => auto (hardware_concurrency-2, clamped to [1, 24]).
    // The "-2" leaves the world thread + one core for TC MapUpdate/Asio/DB.
    // The world thread also participates in run_and_wait(), so effective
    // parallelism is worker_count + 1.
    explicit SnapshotBuildPool(std::size_t worker_count = 0);
    ~SnapshotBuildPool();

    SnapshotBuildPool(SnapshotBuildPool const&) = delete;
    SnapshotBuildPool& operator=(SnapshotBuildPool const&) = delete;

    void start();
    void stop();
    bool running() const { return running_.load(std::memory_order_acquire); }

    std::size_t worker_count() const { return lanes_.size(); }

    // Submit a batch of lane-tagged tasks and block until ALL have completed.
    // Each task is enqueued on worker (lane % worker_count); the world thread
    // runs tasks alongside the workers (preferring its own lane, then stealing)
    // until the batch is drained, then returns. Exceptions thrown by a task are
    // swallowed per-task (a build for one map must not abort the whole batch).
    void run_and_wait(std::vector<LaneTask>& tasks);

private:
    struct Lane
    {
        std::mutex          mtx;
        std::vector<Task>   queue;   // LIFO drain (order within a lane irrelevant)
    };

    void worker_loop(std::size_t lane_index);
    // Pops one task from `lane` and runs it; returns false if that lane was
    // empty. On completion decrements the shared outstanding counter.
    bool try_run_from(std::size_t lane_index);
    void run_task(Task& task);

    std::size_t                          desired_count_ = 0;
    std::vector<std::thread>             workers_;
    std::vector<std::unique_ptr<Lane>>   lanes_;       // one per worker
    std::mutex                           done_mtx_;
    std::condition_variable              cv_work_;      // workers wait for tasks
    std::condition_variable              cv_done_;      // submitter waits for drain
    // outstanding_ = queued + in-flight (drives the batch barrier).
    // queued_      = tasks sitting in lanes not yet picked up (drives the
    //                worker sleep predicate so idle workers don't busy-spin
    //                while other workers finish the tail of a batch).
    std::atomic<std::size_t>             outstanding_{0};
    std::atomic<std::size_t>             queued_{0};
    std::atomic<bool>                    running_{false};
    std::atomic<bool>                    stop_requested_{false};
};

} // namespace Playerbot
