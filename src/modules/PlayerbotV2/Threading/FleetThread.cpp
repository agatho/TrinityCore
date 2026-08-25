#include "FleetThread.h"
#include "../Services.h"
#include "../Diagnostics/PerfCounters.h"

namespace Playerbot {

FleetThread::FleetThread() = default;
FleetThread::~FleetThread() { stop(); }

void FleetThread::start()
{
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return;
    stop_requested_.store(false, std::memory_order_release);
    thr_ = std::thread(&FleetThread::run, this);
}

void FleetThread::stop()
{
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) return;
    {
        std::lock_guard lk(mtx_);
        stop_requested_.store(true, std::memory_order_release);
    }
    cv_.notify_all();
    if (thr_.joinable()) thr_.join();
}

void FleetThread::post(std::function<void()> fn)
{
    {
        std::lock_guard lk(mtx_);
        tasks_.emplace_back(std::move(fn));
    }
    cv_.notify_one();
}

size_t FleetThread::pending() const
{
    std::lock_guard lk(mtx_);
    return tasks_.size();
}

void FleetThread::run()
{
    for (;;)
    {
        std::function<void()> task;
        {
            std::unique_lock lk(mtx_);
            cv_.wait(lk, [&] { return stop_requested_.load(std::memory_order_acquire) || !tasks_.empty(); });
            if (stop_requested_.load(std::memory_order_acquire) && tasks_.empty())
                return;
            task = std::move(tasks_.front());
            tasks_.pop_front();
        }
        // Bot-layer rule: per-bot exceptions can't cascade. The fleet layer
        // runs admin-ish operations but the same isolation principle helps.
        try { task(); }
        catch (...)
        {
            if (Services::Initialized())
                Services::Perf().record_exception();
        }
    }
}

} // namespace Playerbot
