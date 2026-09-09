#include "IntentQueue.h"
#include <bit>
#include <cassert>

namespace Playerbot {

namespace {

size_t RoundUpToPow2(size_t v)
{
    if (v < 2) return 2;
    return std::bit_ceil(v);
}

} // anonymous

IntentQueue::IntentQueue(size_t capacity)
    : slots_(RoundUpToPow2(capacity))
{
    mask_ = slots_.size() - 1;
    for (size_t i = 0; i < slots_.size(); ++i)
        slots_[i].seq.store(i, std::memory_order_relaxed);
}

bool IntentQueue::push(Intent intent)
{
    // Vyukov bounded MPMC pattern, restricted to MPSC semantics here.
    uint64_t pos = head_.load(std::memory_order_relaxed);
    for (;;)
    {
        Slot& s = slots_[pos & mask_];
        const uint64_t seq = s.seq.load(std::memory_order_acquire);
        const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
        if (diff == 0)
        {
            if (head_.compare_exchange_weak(pos, pos + 1,
                                            std::memory_order_relaxed,
                                            std::memory_order_relaxed))
            {
                s.value = std::move(intent);
                s.seq.store(pos + 1, std::memory_order_release);
                return true;
            }
            // Lost the race; retry with updated pos.
        }
        else if (diff < 0)
        {
            return false;   // Full
        }
        else
        {
            pos = head_.load(std::memory_order_relaxed);
        }
    }
}

bool IntentQueue::pop(Intent& out)
{
    const uint64_t pos = tail_.load(std::memory_order_relaxed);
    Slot& s = slots_[pos & mask_];
    const uint64_t seq = s.seq.load(std::memory_order_acquire);
    const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
    if (diff != 0)
        return false;       // Empty (or producer mid-write)
    out = std::move(s.value);
    s.seq.store(pos + slots_.size(), std::memory_order_release);
    tail_.store(pos + 1, std::memory_order_relaxed);
    return true;
}

size_t IntentQueue::approximate_size() const
{
    const uint64_t h = head_.load(std::memory_order_relaxed);
    const uint64_t t = tail_.load(std::memory_order_relaxed);
    return h > t ? static_cast<size_t>(h - t) : 0;
}

} // namespace Playerbot
