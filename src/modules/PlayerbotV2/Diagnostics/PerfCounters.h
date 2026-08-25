// PerfCounters - Counters and latency histograms for V2 internals.
// CONTRACTS.md §8.1.

#pragma once

#include "Bot/BotTypes.h"
#include <atomic>
#include <array>
#include <shared_mutex>
#include <unordered_map>

namespace Playerbot {

class PerfCounters
{
public:
    void record_tick_latency(Ms latency);
    void record_intent_emitted();
    void record_intent_executed();
    void record_intent_failed();
    void record_event_pushed();
    void record_event_dropped();    // ring overflow
    // Intent producer couldn't push because the per-bot IntentQueue is full.
    // Sustained increments mean an AI worker is producing faster than the
    // world thread drains, or the per-tick budget is too tight.
    void record_intent_dropped();
    // Bucketed by Result enum value (cast to size_t at the call site to avoid
    // dragging PlayerbotAPI.h into this header). Out-of-range values fold to
    // the last bucket (Other).
    void record_intent_result(size_t result_index);
    void record_exception();
    void record_snapshot_publish();
    void record_world_update_latency(Ms latency);
    // Phase D queue auto-fill: counts player queues we observed and JIT-spawn
    // events. Latency is wall-clock from "Filler::Fill called" to "JIT bot
    // setup-pipeline complete + queue intent pushed". Useful for verifying
    // the < 10s target and spotting DB write storms during peak queue load.
    void record_queue_fill_request();
    void record_queue_fill_jit_spawned();
    void record_queue_fill_completion(Ms latency);
    // Path-validation metrics. Recorded by PlayerbotAPI::move_to so we can
    // verify the path-validation gate (Movement Enhancement #1) is doing
    // what it should — most increments should be _ok; sustained _nopath /
    // _farfrompoly_* signal mmap coverage gaps for those zones / coords.
    enum class PathOutcome : uint8 {
        Ok = 0,                  // PATHFIND_NORMAL or shortcut (intentional)
        NoPath,                  // PATHFIND_NOPATH or CalculatePath returned false
        FarFromPolyStart,        // bot off-mesh; recovered via NearTeleport
        FarFromPolyEnd,          // dest off-mesh; rule must pick another target
        Incomplete,              // partial path (caller walks what we have)
        Short,                   // truncated by length limit
    };
    void record_path_outcome(PathOutcome o);

    // BG outcome tally for peer-learning data foundation. Recorded by the
    // post-match leave rule when the bot finishes a BG. Keyed by
    // (class * 1000 + spec_bracket_lo10) so a single 32-bit key fits
    // every (class, level-bracket) combo. Won/lost counted separately.
    // Reads via snapshot() returns the top-N win-rate entries.
    void record_bg_outcome(uint8 cls, uint16 spec, uint8 level_bracket_lo10,
                           bool won);

    // BG advice cache instrumentation. Per-bot cache in BotAI elides
    // most GetAdvice() reconstructions at scale — count hits and misses
    // so the /tickperf snapshot can show hit rate. Sustained low hit
    // rate (<90%) means a script's snapshot inputs aren't in the cache
    // key and advice is rebuilding too often.
    void record_bg_advice_hit();
    void record_bg_advice_miss();

    // Wedge-category tally (#1B WedgeWatchdog). Recorded once per wedge
    // EPISODE the watchdog classifies + reports (not per tick), so this is
    // a lifetime count of distinct stuck episodes broken down by root-cause
    // category. The argument is the WedgeCategory enum value cast to size_t
    // at the call site (same decoupling idiom as record_intent_result) so
    // this header stays free of the WedgeWatchdog/BotAI include chain.
    // Out-of-range folds to the last bucket. kWedgeCategoryCount MUST match
    // the WedgeCategory enum cardinality in WedgeWatchdog.h (static_assert
    // tripwire lives there).
    static constexpr size_t kWedgeCategoryCount = 8;
    void record_wedge(size_t category_index);

    // ---- Fleet-vitals rolling window (#1C) -----------------------------
    // One bucket per 60s sample. The world thread pushes a bucket once per
    // minute (push_vitals_bucket, co-located with the FleetStatus log emit);
    // GM-thread digest reads the last bucket + 1h aggregates. Bucketed (not a
    // cumulative counter) so the digest can show TREND deltas (in_world now vs
    // 1h ago, wedged slope, tick-p99 drift) that a monotonic counter can't.
    //
    // 60 buckets = 1h at the 60s cadence. The ring is small + read rarely, so
    // it reuses the same shared_mutex idiom as bg_buckets_ rather than a
    // seqlock: the writer holds a unique_lock for the O(1) bucket store, the
    // reader a shared_lock for an O(60) copy. Contention is negligible (one
    // write/min, reads only on `.playerbot health` / the alert sampler).
    static constexpr size_t kVitalsWindowBuckets = 60;

    struct VitalsBucket
    {
        // Wall-clock (GameTimeMS) at which this bucket was pushed. 0 = empty
        // slot (ring not yet filled). Lets the reader skip never-written slots
        // and compute real time spans for slope math.
        uint32_t sample_at_ms = 0;
        // Instantaneous fleet census at push time.
        uint32_t in_world  = 0;
        uint32_t alive     = 0;
        uint32_t in_combat = 0;
        uint32_t wedged    = 0;   // sum across all wedge categories (active list size)
        // Per-category active wedge counts (index = WedgeCategory enum order).
        std::array<uint32_t, kWedgeCategoryCount> wedged_by_category{};
        // Rates derived over the elapsed window since the previous bucket
        // (computed by the caller from cumulative-counter deltas).
        uint32_t path_fail_per_min   = 0;
        uint32_t intents_per_sec     = 0;
        uint32_t intents_dropped     = 0;   // delta over the window
        uint32_t intents_executed    = 0;   // delta over the window
        // Tick latency percentiles at push time (current histogram snapshot).
        uint32_t tick_p50_us = 0;
        uint32_t tick_p99_us = 0;
        // Fleet-average character level at push time (x100 fixed-point so the
        // bucket stays integer-only; reader divides by 100 for a float).
        uint32_t avg_level_x100 = 0;
    };

    // World thread: store one sampled bucket into the ring (overwrites the
    // oldest slot once full).
    void push_vitals_bucket(VitalsBucket const& b);

    // GM-thread read: the most recently pushed bucket (sample_at_ms == 0 when
    // nothing has been pushed yet).
    VitalsBucket latest_vitals_bucket() const;

    // 1h aggregate over all populated buckets: count, the oldest & newest
    // populated bucket (for trend deltas), and min/max/avg of the headline
    // gauges. Returned by value so the reader holds the lock only briefly.
    struct VitalsWindow
    {
        uint32_t      populated = 0;       // number of non-empty buckets
        VitalsBucket  oldest{};            // earliest populated (1h-ago anchor)
        VitalsBucket  newest{};            // latest populated (== latest_vitals_bucket)
        uint32_t      in_world_min = 0, in_world_max = 0, in_world_avg = 0;
        uint32_t      wedged_min = 0, wedged_max = 0, wedged_avg = 0;
        uint32_t      tick_p99_us_min = 0, tick_p99_us_max = 0, tick_p99_us_avg = 0;
    };
    VitalsWindow vitals_window_snapshot() const;

    // Microsecond-precision tick-latency percentile straight off the live
    // histogram. The Snapshot exposes p50/p99 only in whole ms (count() *
    // 1000 loses sub-ms detail), but the vitals bucket + alerting want
    // microseconds, so the sampler reads these directly. p in [0,1].
    uint32_t tick_latency_percentile_us(double p) const;

    struct Snapshot
    {
        uint64_t ticks_total;
        uint64_t intents_emitted_total;
        uint64_t intents_executed_total;
        uint64_t intents_failed_total;
        uint64_t events_pushed_total;
        uint64_t events_dropped_total;
        uint64_t intents_dropped_total;
        // Index = Result enum order (Ok, NotReady, OutOfRange, InvalidTarget,
        // NotEnoughResource, NotKnown, ServerRefused, InventoryFull, Locked,
        // Other). Each slot accumulates regardless of Ok/non-Ok; Ok mirrors
        // intents_executed_total.
        std::array<uint64_t, 10> intents_by_result;
        uint64_t exceptions_total;
        uint64_t snapshots_published_total;
        uint64_t world_updates_total;
        uint64_t queue_fill_requests_total;
        uint64_t queue_fill_jit_spawned_total;
        uint64_t queue_fill_completions_total;
        // Path-outcome counters. Order matches PathOutcome enum.
        std::array<uint64_t, 6> path_outcomes;
        // BG outcome global totals. Per-(class, spec, bracket) detail not
        // exposed here; query the data via FleetHealth or the bg_outcomes
        // diagnostic command. These two top-line counters give a quick
        // health check ("are bots winning roughly half their BGs?").
        uint64_t bg_wins_total = 0;
        uint64_t bg_losses_total = 0;
        uint64_t bg_advice_cache_hits_total = 0;
        uint64_t bg_advice_cache_misses_total = 0;
        // Per-category wedge episode totals. Index = WedgeCategory enum order
        // (None, Navmesh, OffMesh, Travel, CombatLoop, PickerNone,
        // GoalUnreachable). None is normally 0 (the watchdog only records a
        // classified wedge), kept for index alignment with the enum.
        std::array<uint64_t, kWedgeCategoryCount> wedge_by_category;

        // Approximate p50/p99 from a fixed-bucket histogram of tick latency.
        Ms tick_p50;
        Ms tick_p99;
        Ms world_p50;
        Ms world_p99;
        Ms queue_fill_p50;
        Ms queue_fill_p99;
    };

    Snapshot snapshot() const;

    void reset();

private:
    // Power-of-two latency buckets, microseconds: <2, <4, <8, <16, ..., <524288 (~500ms)
    static constexpr size_t kBuckets = 19;

    std::atomic<uint64_t> ticks_{0};
    std::atomic<uint64_t> intents_emitted_{0};
    std::atomic<uint64_t> intents_executed_{0};
    std::atomic<uint64_t> intents_failed_{0};
    std::array<std::atomic<uint64_t>, 10> intents_by_result_{};
    std::atomic<uint64_t> events_pushed_{0};
    std::atomic<uint64_t> events_dropped_{0};
    std::atomic<uint64_t> intents_dropped_{0};
    std::atomic<uint64_t> exceptions_{0};
    std::atomic<uint64_t> snapshots_{0};
    std::atomic<uint64_t> world_updates_{0};
    std::atomic<uint64_t> queue_fill_requests_{0};
    std::atomic<uint64_t> queue_fill_jit_spawns_{0};
    std::atomic<uint64_t> queue_fill_completions_{0};
    std::array<std::atomic<uint64_t>, 6> path_outcomes_{};
    std::atomic<uint64_t> bg_wins_{0};
    std::atomic<uint64_t> bg_losses_{0};
    std::atomic<uint64_t> bg_advice_cache_hits_{0};
    std::atomic<uint64_t> bg_advice_cache_misses_{0};
    std::array<std::atomic<uint64_t>, kWedgeCategoryCount> wedge_by_category_{};

    // Per-(class, spec, bracket) BG outcome bucket store. Key encoding:
    // (cls << 24) | (spec << 8) | bracket_lo10. shared_mutex bridges
    // multiple AI threads firing record_bg_outcome at near-simultaneous
    // BG-end ticks; reads are infrequent (only on diag command).
public:
    struct BgBucket { uint32_t wins = 0; uint32_t losses = 0; };
    using BgBucketMap = std::unordered_map<uint32_t, BgBucket>;
    BgBucketMap bg_buckets_snapshot() const;
private:
    mutable std::shared_mutex bg_buckets_mtx_;
    BgBucketMap               bg_buckets_;

    // Fleet-vitals ring (#1C). Fixed-size circular buffer; vitals_head_ is the
    // index of the NEXT slot to write. Guarded by its own shared_mutex
    // (writer: world thread once/min; readers: GM digest + alert sampler).
    mutable std::shared_mutex                          vitals_mtx_;
    std::array<VitalsBucket, kVitalsWindowBuckets>     vitals_ring_{};
    size_t                                             vitals_head_  = 0;
    size_t                                             vitals_count_ = 0;   // populated slots, caps at kVitalsWindowBuckets

    std::array<std::atomic<uint64_t>, kBuckets> tick_hist_{};
    std::array<std::atomic<uint64_t>, kBuckets> world_hist_{};
    std::array<std::atomic<uint64_t>, kBuckets> queue_fill_hist_{};

    static size_t bucket_for_us(uint64_t us);
    static Ms     percentile(std::array<std::atomic<uint64_t>, kBuckets> const& hist, double p);
};

} // namespace Playerbot
