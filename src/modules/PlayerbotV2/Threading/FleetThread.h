// FleetThread - Single-threaded executor for fleet-layer operations.
// CONTRACTS.md §5.4. Posts run FIFO; thread pulls them off a mutex/condvar
// queue. Not lock-free because contention is bounded (one main producer per
// hook, one consumer = the fleet thread itself), and clarity beats throughput
// for this particular thread.

#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

namespace Playerbot {

class FleetThread
{
public:
    FleetThread();
    ~FleetThread();

    void start();
    void stop();
    bool running() const { return running_.load(std::memory_order_acquire); }

    // Post a task to be executed on the fleet thread.
    void post(std::function<void()> fn);

    // Diagnostics
    size_t pending() const;

private:
    void run();

    std::thread             thr_;
    mutable std::mutex      mtx_;
    std::condition_variable cv_;
    std::deque<std::function<void()>> tasks_;
    std::atomic<bool>       running_{false};
    std::atomic<bool>       stop_requested_{false};
};

} // namespace Playerbot
