// FlatIndex - cache-friendly replacement for the small per-build
// std::unordered_maps that accelerate snapshot lookups (aura index,
// cooldown index, quest index, bag-count, outbound-aura index).
//
// Rationale (SNAPSHOT_PERF_BACKLOG.md Tier 3.3): each of those maps was
// rebuilt every Build() — ~5 maps × ~30-100 entries × ~10-20K Builds/s =
// ~100 node allocations per build churned through the heap. A sorted
// std::vector<{key,value}> with binary-search lookup keeps the same O(log n)
// (vs O(1) hash, but n is tiny: 25-100 entries) while:
//   * allocating ONE contiguous buffer instead of N nodes,
//   * retaining capacity across the snapshot recycle pool (Tier 3.1), so a
//     reused snapshot does zero allocations for these indices,
//   * being far more cache-friendly for the build-time fill loop and the
//     read-time lookups in BotSnapshotView.
//
// Semantics are IDENTICAL to the unordered_map they replace:
//   * find(key) returns the value for the FIRST inserted entry with that key
//     (mirrors unordered_map::emplace, which keeps the first insertion for a
//     duplicate key — the builder relies on "first index wins" for
//     own_auras_index / my_auras_on_others_index).
//   * add(key, delta) accumulates (mirrors `map[key] += delta` for
//     bag_count_by_entry).
// The fill protocol is: clear() → reserve() → push(...) per entry →
// finalize(). finalize() stably sorts by key and (for the additive variant)
// merges duplicate keys, so lookups are valid only after finalize().
//
// World-thread-only writes (the builder); read-only on AI workers after the
// snapshot is published — the same access discipline the unordered_maps had.

#pragma once

#include "Define.h"
#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace Playerbot {

// First-insertion-wins flat map. Replaces an unordered_map<Key,uint32> whose
// values are indices into a parallel vector and where duplicate keys keep the
// first insertion (emplace semantics).
template <typename Key>
class FlatIndexMap
{
public:
    using value_type = std::pair<Key, uint32>;

    void clear() { entries_.clear(); }
    void reserve(size_t n) { entries_.reserve(n); }
    bool empty() const { return entries_.empty(); }
    size_t size() const { return entries_.size(); }

    // Push one (key, index) pair. Order of pushes is preserved among equal
    // keys by the stable sort in finalize(), so the FIRST push for a key wins
    // its lookup — identical to unordered_map::emplace.
    void push(Key key, uint32 index) { entries_.emplace_back(key, index); }

    // Sort by key (stable: equal keys keep push order). Call once after all
    // pushes. Idempotent on an already-sorted buffer.
    void finalize()
    {
        std::stable_sort(entries_.begin(), entries_.end(),
                         [](value_type const& a, value_type const& b) { return a.first < b.first; });
    }

    // Returns a pointer to the value (index) for the FIRST entry with `key`,
    // or nullptr when absent. Valid only after finalize().
    uint32 const* find(Key key) const
    {
        auto it = std::lower_bound(entries_.begin(), entries_.end(), key,
                                   [](value_type const& e, Key k) { return e.first < k; });
        if (it != entries_.end() && it->first == key)
            return &it->second;
        return nullptr;
    }

private:
    std::vector<value_type> entries_;
};

// Accumulating flat map. Replaces an unordered_map<Key,uint32> used as a
// counter (map[key] += delta). Pushes raw (key, delta) pairs; finalize()
// sorts and merge-sums duplicate keys into one entry each.
template <typename Key>
class FlatCountMap
{
public:
    using value_type = std::pair<Key, uint32>;

    void clear() { entries_.clear(); }
    void reserve(size_t n) { entries_.reserve(n); }
    bool empty() const { return entries_.empty(); }
    size_t size() const { return entries_.size(); }

    // Equivalent to `map[key] += delta` — accumulation is deferred to
    // finalize() so the fill loop stays push-only (no per-item search).
    void add(Key key, uint32 delta) { entries_.emplace_back(key, delta); }

    // Sort by key then merge-sum runs of equal keys in place. After this the
    // buffer holds one entry per distinct key with the summed total.
    void finalize()
    {
        std::sort(entries_.begin(), entries_.end(),
                  [](value_type const& a, value_type const& b) { return a.first < b.first; });
        size_t out = 0;
        for (size_t i = 0; i < entries_.size(); )
        {
            Key const k = entries_[i].first;
            uint32 sum = 0;
            size_t j = i;
            for (; j < entries_.size() && entries_[j].first == k; ++j)
                sum += entries_[j].second;
            entries_[out++] = value_type(k, sum);
            i = j;
        }
        entries_.resize(out);
    }

    // Summed value for `key`, or 0 when absent. Valid only after finalize().
    uint32 get(Key key) const
    {
        auto it = std::lower_bound(entries_.begin(), entries_.end(), key,
                                   [](value_type const& e, Key k) { return e.first < k; });
        if (it != entries_.end() && it->first == key)
            return it->second;
        return 0u;
    }

private:
    std::vector<value_type> entries_;
};

} // namespace Playerbot
