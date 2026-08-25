// IntentQueue - Lock-free MPSC ring buffer.
// Many AI workers + fleet thread produce; world thread consumes.
// CONTRACTS.md §5.1.

#pragma once

#include "Bot/BotIntent.h"
#include <atomic>
#include <vector>

namespace Playerbot {

class IntentQueue
{
public:
    explicit IntentQueue(size_t capacity = 4096);

    // Producer side. Returns false if the queue is full (caller should drop
    // or retry next tick; never blocks). Lock-free.
    bool push(Intent intent);

    // Consumer side — single thread (world thread) only. Returns false if
    // empty.
    bool pop(Intent& out);

    // Approximate count; not authoritative under contention.
    size_t approximate_size() const;

    size_t capacity() const { return slots_.size(); }

private:
    struct Slot
    {
        std::atomic<uint64_t> seq;
        Intent                value;
    };

    std::vector<Slot> slots_;
    std::atomic<uint64_t> head_{0};   // Producers CAS-increment
    std::atomic<uint64_t> tail_{0};   // Consumer increments (single-threaded)
    size_t mask_ = 0;
};

} // namespace Playerbot
