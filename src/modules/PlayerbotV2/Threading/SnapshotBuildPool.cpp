#include "SnapshotBuildPool.h"

#include "Services.h"
#include "Diagnostics/PerfCounters.h"
#include <algorithm>

namespace Playerbot {

namespace {
std::size_t ResolveBuildWorkers(std::size_t requested)
{
    if (requested > 0)
        return requested;
    const unsigned hwc = std::max(1u, std::thread::hardware_concurrency());
    // Leave the world thread + one core for TC MapUpdate workers / Asio
    // handlers / DB pools. The world thread also runs tasks in run_and_wait,
    // so effective parallelism is (this + 1). Clamp to 24 to mirror the
    // AiWorkerPool ceiling on big CPUs.
    const unsigned avail = hwc > 2 ? hwc - 2 : 1;
    return std::min<std::size_t>(avail, 24);
}
} // anonymous

SnapshotBuildPool::SnapshotBuildPool(std::size_t worker_count)
    : desired_count_(ResolveBuildWorkers(worker_count))
{}

SnapshotBuildPool::~SnapshotBuildPool() { stop(); }

void SnapshotBuildPool::start()
{
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true))
        return;
    stop_requested_.store(false, std::memory_order_release);
    lanes_.reserve(desired_count_);
    for (std::size_t i = 0; i < desired_count_; ++i)
        lanes_.emplace_back(std::make_unique<Lane>());
    workers_.reserve(desired_count_);
    for (std::size_t i = 0; i < desired_count_; ++i)
        workers_.emplace_back(&SnapshotBuildPool::worker_loop, this, i);
}

void SnapshotBuildPool::stop()
{
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false))
        return;
    {
        std::lock_guard lk(done_mtx_);
        stop_requested_.store(true, std::memory_order_release);
    }
    cv_work_.notify_all();
    for (auto& t : workers_)
        if (t.joinable())
            t.join();
    workers_.clear();
    lanes_.clear();
}

void SnapshotBuildPool::run_task(Task& task)
{
    // A build for one map must not abort the batch, so swallow any escaping
    // exception (task bodies harden internally and route their own
    // diagnostics; this mirrors the AiWorkerPool worker discipline).
    try
    {
        task();
    }
    catch (...)
    {
        if (Services::Initialized())
            Services::Perf().record_exception();
    }
    // Account completion + wake the submitter if the batch just drained.
    if (outstanding_.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        std::lock_guard lk(done_mtx_);
        cv_done_.notify_all();
    }
}

bool SnapshotBuildPool::try_run_from(std::size_t lane_index)
{
    if (lane_index >= lanes_.size())
        return false;
    Lane& lane = *lanes_[lane_index];
    Task task;
    {
        std::lock_guard lk(lane.mtx);
        if (lane.queue.empty())
            return false;
        task = std::move(lane.queue.back());
        lane.queue.pop_back();
    }
    queued_.fetch_sub(1, std::memory_order_acq_rel);
    run_task(task);   // OUTSIDE the lane lock
    return true;
}

void SnapshotBuildPool::worker_loop(std::size_t lane_index)
{
    for (;;)
    {
        // Drain own lane first, then steal from any other lane.
        bool did_work = false;
        if (try_run_from(lane_index))
            did_work = true;
        else
        {
            for (std::size_t i = 0; i < lanes_.size(); ++i)
            {
                if (i == lane_index) continue;
                if (try_run_from(i)) { did_work = true; break; }
            }
        }
        if (did_work)
            continue;

        // Nothing to steal right now. Sleep until there is freshly QUEUED work
        // (queued_ != 0) or stop is requested. Waiting on queued_ rather than
        // outstanding_ prevents a busy-spin while other workers finish the
        // tail of a batch (outstanding_ > 0 but nothing left to pick up).
        std::unique_lock lk(done_mtx_);
        cv_work_.wait(lk, [&] {
            return stop_requested_.load(std::memory_order_acquire) ||
                   queued_.load(std::memory_order_acquire) != 0;
        });
        if (stop_requested_.load(std::memory_order_acquire) &&
            queued_.load(std::memory_order_acquire) == 0)
            return;
    }
}

void SnapshotBuildPool::run_and_wait(std::vector<LaneTask>& tasks)
{
    if (tasks.empty())
        return;

    // No workers (auto resolved to 0 on a tiny box, or pool not started):
    // run everything inline on the calling thread. Still correct — same
    // thread_local pools as the serial path.
    if (!running_.load(std::memory_order_acquire) || lanes_.empty())
    {
        for (auto& lt : tasks)
        {
            if (!lt.fn) continue;
            try { lt.fn(); }
            catch (...) { if (Services::Initialized()) Services::Perf().record_exception(); }
        }
        tasks.clear();
        return;
    }

    const std::size_t nlanes = lanes_.size();
    // Enqueue per lane (lane % worker_count). Bump outstanding BEFORE notifying
    // so a worker waking up sees a non-zero count.
    std::size_t valid = 0;
    for (auto& lt : tasks)
        if (lt.fn) ++valid;
    if (valid == 0) { tasks.clear(); return; }
    outstanding_.fetch_add(valid, std::memory_order_acq_rel);

    for (auto& lt : tasks)
    {
        if (!lt.fn) continue;
        Lane& lane = *lanes_[lt.lane % nlanes];
        std::lock_guard lk(lane.mtx);
        lane.queue.push_back(std::move(lt.fn));
    }
    // Publish queued count + wake workers. The increment + notify happen under
    // done_mtx_ (the same mutex the workers' cv_work_.wait re-checks the
    // predicate under) so there is no lost-wakeup window between a worker
    // finding its lanes empty and re-arming its wait.
    {
        std::lock_guard lk(done_mtx_);
        queued_.fetch_add(valid, std::memory_order_acq_rel);
    }
    cv_work_.notify_all();

    // The world thread participates: drain tasks itself (prefer lane 0, then
    // steal) so a single dominant map doesn't leave it idle and so progress
    // is still made if every worker is momentarily busy.
    for (;;)
    {
        bool did_work = false;
        for (std::size_t i = 0; i < nlanes; ++i)
            if (try_run_from(i)) { did_work = true; break; }
        if (!did_work)
            break;
    }

    // Anything left is in-flight on the workers; wait for the batch barrier.
    {
        std::unique_lock lk(done_mtx_);
        cv_done_.wait(lk, [&] {
            return outstanding_.load(std::memory_order_acquire) == 0;
        });
    }

    tasks.clear();
}

} // namespace Playerbot
