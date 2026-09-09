#include "PerfCounters.h"
#include <bit>

namespace Playerbot {

size_t PerfCounters::bucket_for_us(uint64_t us)
{
    if (us < 2) return 0;
    // bucket = floor(log2(us))
    const size_t b = static_cast<size_t>(std::bit_width(us)) - 1;
    return b < kBuckets ? b : kBuckets - 1;
}

void PerfCounters::record_tick_latency(Ms latency)
{
    ticks_.fetch_add(1, std::memory_order_relaxed);
    const uint64_t us = static_cast<uint64_t>(latency.count() * 1000);
    tick_hist_[bucket_for_us(us)].fetch_add(1, std::memory_order_relaxed);
}

void PerfCounters::record_intent_emitted()
{
    intents_emitted_.fetch_add(1, std::memory_order_relaxed);
}

void PerfCounters::record_intent_executed()
{
    intents_executed_.fetch_add(1, std::memory_order_relaxed);
}

void PerfCounters::record_intent_failed()
{
    intents_failed_.fetch_add(1, std::memory_order_relaxed);
}

void PerfCounters::record_intent_result(size_t result_index)
{
    if (result_index >= intents_by_result_.size())
        result_index = intents_by_result_.size() - 1;
    intents_by_result_[result_index].fetch_add(1, std::memory_order_relaxed);
}

void PerfCounters::record_event_pushed()
{
    events_pushed_.fetch_add(1, std::memory_order_relaxed);
}

void PerfCounters::record_event_dropped()
{
    events_dropped_.fetch_add(1, std::memory_order_relaxed);
}

void PerfCounters::record_intent_dropped()
{
    intents_dropped_.fetch_add(1, std::memory_order_relaxed);
}

void PerfCounters::record_exception()
{
    exceptions_.fetch_add(1, std::memory_order_relaxed);
}

void PerfCounters::record_snapshot_publish()
{
    snapshots_.fetch_add(1, std::memory_order_relaxed);
}

void PerfCounters::record_world_update_latency(Ms latency)
{
    world_updates_.fetch_add(1, std::memory_order_relaxed);
    const uint64_t us = static_cast<uint64_t>(latency.count() * 1000);
    world_hist_[bucket_for_us(us)].fetch_add(1, std::memory_order_relaxed);
}

void PerfCounters::record_queue_fill_request()
{
    queue_fill_requests_.fetch_add(1, std::memory_order_relaxed);
}

void PerfCounters::record_queue_fill_jit_spawned()
{
    queue_fill_jit_spawns_.fetch_add(1, std::memory_order_relaxed);
}

void PerfCounters::record_queue_fill_completion(Ms latency)
{
    queue_fill_completions_.fetch_add(1, std::memory_order_relaxed);
    const uint64_t us = static_cast<uint64_t>(latency.count() * 1000);
    queue_fill_hist_[bucket_for_us(us)].fetch_add(1, std::memory_order_relaxed);
}

void PerfCounters::record_path_outcome(PathOutcome o)
{
    const size_t idx = static_cast<size_t>(o);
    if (idx < path_outcomes_.size())
        path_outcomes_[idx].fetch_add(1, std::memory_order_relaxed);
}

void PerfCounters::record_bg_outcome(uint8 cls, uint16 spec,
                                     uint8 level_bracket_lo10, bool won)
{
    if (won) bg_wins_.fetch_add(1, std::memory_order_relaxed);
    else     bg_losses_.fetch_add(1, std::memory_order_relaxed);

    const uint32_t key = (uint32_t(cls) << 24)
                       | (uint32_t(spec) << 8)
                       | uint32_t(level_bracket_lo10);
    std::unique_lock lk(bg_buckets_mtx_);
    auto& b = bg_buckets_[key];
    if (won) ++b.wins; else ++b.losses;
}

PerfCounters::BgBucketMap PerfCounters::bg_buckets_snapshot() const
{
    std::shared_lock lk(bg_buckets_mtx_);
    return bg_buckets_;
}

void PerfCounters::record_bg_advice_hit()
{
    bg_advice_cache_hits_.fetch_add(1, std::memory_order_relaxed);
}

void PerfCounters::record_bg_advice_miss()
{
    bg_advice_cache_misses_.fetch_add(1, std::memory_order_relaxed);
}

void PerfCounters::record_wedge(size_t category_index)
{
    if (category_index >= wedge_by_category_.size())
        category_index = wedge_by_category_.size() - 1;
    wedge_by_category_[category_index].fetch_add(1, std::memory_order_relaxed);
}

void PerfCounters::push_vitals_bucket(VitalsBucket const& b)
{
    std::unique_lock lk(vitals_mtx_);
    vitals_ring_[vitals_head_] = b;
    vitals_head_ = (vitals_head_ + 1) % kVitalsWindowBuckets;
    if (vitals_count_ < kVitalsWindowBuckets)
        ++vitals_count_;
}

PerfCounters::VitalsBucket PerfCounters::latest_vitals_bucket() const
{
    std::shared_lock lk(vitals_mtx_);
    if (vitals_count_ == 0)
        return VitalsBucket{};
    // The newest bucket sits at the slot just before the write head.
    const size_t idx = (vitals_head_ + kVitalsWindowBuckets - 1) % kVitalsWindowBuckets;
    return vitals_ring_[idx];
}

PerfCounters::VitalsWindow PerfCounters::vitals_window_snapshot() const
{
    VitalsWindow w;
    std::shared_lock lk(vitals_mtx_);
    if (vitals_count_ == 0)
        return w;

    w.populated = static_cast<uint32_t>(vitals_count_);

    // Walk the populated slots oldest -> newest. When the ring is full the
    // oldest slot is the write head (next to be overwritten); when it is still
    // filling the oldest is slot 0.
    const size_t start = (vitals_count_ == kVitalsWindowBuckets)
        ? vitals_head_
        : 0;

    uint64_t in_world_sum = 0, wedged_sum = 0, p99_sum = 0;
    bool first = true;
    for (size_t n = 0; n < vitals_count_; ++n)
    {
        const size_t idx = (start + n) % kVitalsWindowBuckets;
        VitalsBucket const& b = vitals_ring_[idx];
        if (b.sample_at_ms == 0)
            continue;   // defensive: skip an unexpectedly-empty slot

        if (first)
        {
            w.oldest = b;
            w.in_world_min = w.in_world_max = b.in_world;
            w.wedged_min   = w.wedged_max   = b.wedged;
            w.tick_p99_us_min = w.tick_p99_us_max = b.tick_p99_us;
            first = false;
        }
        else
        {
            if (b.in_world < w.in_world_min) w.in_world_min = b.in_world;
            if (b.in_world > w.in_world_max) w.in_world_max = b.in_world;
            if (b.wedged   < w.wedged_min)   w.wedged_min   = b.wedged;
            if (b.wedged   > w.wedged_max)   w.wedged_max   = b.wedged;
            if (b.tick_p99_us < w.tick_p99_us_min) w.tick_p99_us_min = b.tick_p99_us;
            if (b.tick_p99_us > w.tick_p99_us_max) w.tick_p99_us_max = b.tick_p99_us;
        }
        w.newest = b;
        in_world_sum += b.in_world;
        wedged_sum   += b.wedged;
        p99_sum      += b.tick_p99_us;
    }

    const uint32_t denom = w.populated > 0 ? w.populated : 1;
    w.in_world_avg    = static_cast<uint32_t>(in_world_sum / denom);
    w.wedged_avg      = static_cast<uint32_t>(wedged_sum   / denom);
    w.tick_p99_us_avg = static_cast<uint32_t>(p99_sum      / denom);
    return w;
}

Ms PerfCounters::percentile(std::array<std::atomic<uint64_t>, kBuckets> const& hist, double p)
{
    uint64_t total = 0;
    for (auto const& b : hist) total += b.load(std::memory_order_relaxed);
    if (total == 0) return Ms{0};

    const uint64_t target = static_cast<uint64_t>(static_cast<double>(total) * p);
    uint64_t cumulative = 0;
    for (size_t i = 0; i < kBuckets; ++i)
    {
        cumulative += hist[i].load(std::memory_order_relaxed);
        if (cumulative >= target)
        {
            // Bucket upper-bound microseconds = 2^(i+1).
            const uint64_t us = uint64_t(1) << (i + 1);
            return Ms{static_cast<int64_t>(us / 1000)};
        }
    }
    return Ms{0};
}

uint32_t PerfCounters::tick_latency_percentile_us(double p) const
{
    uint64_t total = 0;
    for (auto const& b : tick_hist_) total += b.load(std::memory_order_relaxed);
    if (total == 0) return 0;

    const uint64_t target = static_cast<uint64_t>(static_cast<double>(total) * p);
    uint64_t cumulative = 0;
    for (size_t i = 0; i < kBuckets; ++i)
    {
        cumulative += tick_hist_[i].load(std::memory_order_relaxed);
        if (cumulative >= target)
            return static_cast<uint32_t>(uint64_t(1) << (i + 1));   // bucket upper-bound µs
    }
    return 0;
}

PerfCounters::Snapshot PerfCounters::snapshot() const
{
    Snapshot s;
    s.ticks_total                 = ticks_.load(std::memory_order_relaxed);
    s.intents_emitted_total       = intents_emitted_.load(std::memory_order_relaxed);
    s.intents_executed_total      = intents_executed_.load(std::memory_order_relaxed);
    s.intents_failed_total        = intents_failed_.load(std::memory_order_relaxed);
    s.events_pushed_total         = events_pushed_.load(std::memory_order_relaxed);
    s.events_dropped_total        = events_dropped_.load(std::memory_order_relaxed);
    s.intents_dropped_total       = intents_dropped_.load(std::memory_order_relaxed);
    for (size_t i = 0; i < s.intents_by_result.size(); ++i)
        s.intents_by_result[i] = intents_by_result_[i].load(std::memory_order_relaxed);
    s.exceptions_total            = exceptions_.load(std::memory_order_relaxed);
    s.snapshots_published_total   = snapshots_.load(std::memory_order_relaxed);
    s.world_updates_total         = world_updates_.load(std::memory_order_relaxed);
    s.queue_fill_requests_total   = queue_fill_requests_.load(std::memory_order_relaxed);
    s.queue_fill_jit_spawned_total= queue_fill_jit_spawns_.load(std::memory_order_relaxed);
    s.queue_fill_completions_total= queue_fill_completions_.load(std::memory_order_relaxed);
    for (size_t i = 0; i < s.path_outcomes.size(); ++i)
        s.path_outcomes[i] = path_outcomes_[i].load(std::memory_order_relaxed);
    s.bg_wins_total   = bg_wins_.load(std::memory_order_relaxed);
    s.bg_losses_total = bg_losses_.load(std::memory_order_relaxed);
    s.bg_advice_cache_hits_total   = bg_advice_cache_hits_.load(std::memory_order_relaxed);
    s.bg_advice_cache_misses_total = bg_advice_cache_misses_.load(std::memory_order_relaxed);
    for (size_t i = 0; i < s.wedge_by_category.size(); ++i)
        s.wedge_by_category[i] = wedge_by_category_[i].load(std::memory_order_relaxed);
    s.tick_p50                    = percentile(tick_hist_, 0.50);
    s.tick_p99                    = percentile(tick_hist_, 0.99);
    s.world_p50                   = percentile(world_hist_, 0.50);
    s.world_p99                   = percentile(world_hist_, 0.99);
    s.queue_fill_p50              = percentile(queue_fill_hist_, 0.50);
    s.queue_fill_p99              = percentile(queue_fill_hist_, 0.99);
    return s;
}

void PerfCounters::reset()
{
    ticks_.store(0); intents_emitted_.store(0); intents_executed_.store(0);
    intents_failed_.store(0);
    events_pushed_.store(0); events_dropped_.store(0);
    intents_dropped_.store(0);
    for (auto& c : intents_by_result_) c.store(0);
    exceptions_.store(0); snapshots_.store(0); world_updates_.store(0);
    for (auto& c : path_outcomes_) c.store(0);
    for (auto& c : wedge_by_category_) c.store(0);
    queue_fill_requests_.store(0);
    queue_fill_jit_spawns_.store(0);
    queue_fill_completions_.store(0);
    for (auto& b : tick_hist_)  b.store(0);
    for (auto& b : world_hist_) b.store(0);
    for (auto& b : queue_fill_hist_) b.store(0);
    {
        std::unique_lock lk(vitals_mtx_);
        vitals_ring_.fill(VitalsBucket{});
        vitals_head_  = 0;
        vitals_count_ = 0;
    }
}

} // namespace Playerbot
