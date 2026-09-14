#include "IntentQueue.h"
#include "Log.h"
#include <bit>
#include <cassert>
#include <mutex>

namespace Playerbot {

namespace {

size_t RoundUpToPow2(size_t v)
{
    if (v < 2) return 2;
    return std::bit_ceil(v);
}

} // anonymous

IntentQueue::IntentQueue(size_t capacity)
    : capacity_(RoundUpToPow2(capacity))
{
    // No slot array yet - see ensure_allocated(). Only the depth is fixed here,
    // so a registered-but-silent bot costs a few dozen bytes instead of
    // capacity_ * sizeof(Slot).
    mask_ = capacity_ - 1;
}

void IntentQueue::ensure_allocated()
{
    std::lock_guard lk(init_mtx_);
    if (ready_.load(std::memory_order_relaxed))
        return;                     // Lost the race; another producer built it.

    // Move-assign a freshly sized vector rather than resize(): Slot holds a
    // std::atomic, so it is not MoveInsertable and resize() would not compile.
    // The count constructor only requires DefaultInsertable, and vector's move
    // assignment steals the buffer without touching elements.
    slots_ = std::vector<Slot>(capacity_);
    for (size_t i = 0; i < slots_.size(); ++i)
        slots_[i].seq.store(i, std::memory_order_relaxed);

    // Report the real per-bot cost once per run. The slot array is by far the
    // largest per-bot allocation in the module and its size depends on
    // sizeof(Intent), which moves whenever an IntentBody variant is added - so
    // measure it rather than estimating from the struct.
    static std::once_flag s_reported;
    std::call_once(s_reported, [this]
    {
        TC_LOG_INFO("playerbot.v2",
            "[IntentQueue] ring materialised: {} slots x {} bytes = {} KiB per active bot "
            "(sizeof(Intent)={})",
            capacity_, sizeof(Slot), (capacity_ * sizeof(Slot)) / 1024, sizeof(Intent));
    });

    ready_.store(true, std::memory_order_release);
}

bool IntentQueue::push(Intent intent)
{
    if (!ready_.load(std::memory_order_acquire))
        ensure_allocated();

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
    // Nothing was ever pushed, so the ring does not exist yet and the queue is
    // empty by definition. Checked before slots_ is touched.
    if (!ready_.load(std::memory_order_acquire))
        return false;

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

size_t IntentQueue::allocated_bytes() const
{
    return ready_.load(std::memory_order_acquire) ? capacity_ * sizeof(Slot) : 0;
}

size_t IntentQueue::approximate_size() const
{
    const uint64_t h = head_.load(std::memory_order_relaxed);
    const uint64_t t = tail_.load(std::memory_order_relaxed);
    return h > t ? static_cast<size_t>(h - t) : 0;
}

} // namespace Playerbot
