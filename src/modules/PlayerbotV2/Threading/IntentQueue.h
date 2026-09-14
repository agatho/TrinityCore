// IntentQueue - Lock-free MPSC ring buffer.
// Many AI workers + fleet thread produce; world thread consumes.
// CONTRACTS.md §5.1.

#pragma once

#include "Bot/BotIntent.h"
#include <atomic>
#include <mutex>
#include <vector>

namespace Playerbot {

// Default ring depth. One of these exists per REGISTERED bot, and the slot
// array is sized capacity * sizeof(Slot) where Slot is an atomic<uint64_t>
// alongside a whole Intent - so this number is the single biggest per-bot
// allocation in the module. It is deliberately left at the historical value:
// push() drops the intent and bumps intents_dropped_total when the ring is
// full, so shrinking it trades memory for lost bot actions under burst. Size it
// from the measured peak (".playerbot inspect" reports "Dropped total") rather
// than from the typical depth, which is under five.
inline constexpr size_t kDefaultIntentQueueCapacity = 4096;

class IntentQueue
{
public:
    explicit IntentQueue(size_t capacity = kDefaultIntentQueueCapacity);

    // Producer side. Returns false if the queue is full (caller should drop
    // or retry next tick; never blocks). Lock-free once the ring exists.
    bool push(Intent intent);

    // Consumer side — single thread (world thread) only. Returns false if
    // empty.
    bool pop(Intent& out);

    // Approximate count; not authoritative under contention.
    size_t approximate_size() const;

    // Configured depth. Reported even before the ring is materialised, so this
    // is the queue's capacity rather than its current allocation.
    size_t capacity() const { return capacity_; }

    // Bytes the slot array occupies once materialised. Zero until then.
    size_t allocated_bytes() const;

private:
    struct Slot
    {
        std::atomic<uint64_t> seq;
        Intent                value;
    };

    // The ring is allocated on first push rather than at registration.
    // Registration happens for every bot the fleet brings up, in bursts during
    // a mass spawn, while a bot that is registered and released without ever
    // emitting an intent never needs the storage at all. Allocating here also
    // keeps the spike off the registration path, which runs under
    // BotRegistry's lock.
    //
    // Publication is a release store to ready_; every reader acquire-loads it
    // before touching slots_ or mask_, so the fully initialised array is
    // visible to whoever observes the flag. The array is never resized
    // afterwards, so references handed out by push()/pop() stay valid.
    void ensure_allocated();

    std::vector<Slot> slots_;
    std::atomic<uint64_t> head_{0};   // Producers CAS-increment
    std::atomic<uint64_t> tail_{0};   // Consumer increments (single-threaded)
    size_t mask_ = 0;
    size_t capacity_ = 0;             // Rounded-up depth, known before allocation
    std::atomic<bool> ready_{false};  // Release-published once slots_ is usable
    std::mutex init_mtx_;             // Guards the one-time allocation only
};

} // namespace Playerbot
