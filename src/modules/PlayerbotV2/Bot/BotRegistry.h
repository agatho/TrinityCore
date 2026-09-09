// BotRegistry - Owns BotAI instances + per-bot IntentQueue and EventInbox.
// Pure storage; lifecycle decisions (spawn/despawn) live in Fleet.

#pragma once

#include "BotAI.h"
#include "../Threading/IntentQueue.h"
#include "ObjectGuid.h"
#include <array>
#include <atomic>
#include <cassert>
#include <deque>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace Playerbot {

// Per-bot intent execution history — populated by BotIntentExecutor after
// each visit() call so /diag can show the most recent N (kind, result)
// pairs in chronological order. 32 entries is one or two seconds of dense
// combat at typical intent emission rates; long enough to spot loops, short
// enough to fit in an in-memory ring keyed by every registered bot.
struct IntentHistoryEntry
{
    uint32 ts_ms      = 0;   // GameTime::GetGameTimeMS at execution
    uint32 intent_kind = 0;  // IntentBody variant index (see Intent.h)
    uint8  result     = 0;   // PlayerbotAPI::Result (Ok=0, NotReady=1, …)
    // Destination for MoveToIntent (0,0,0 otherwise) — /diag prints it so
    // alternating-target loops are visible verbatim (an Ok-per-150ms stream
    // is ambiguous without the coords: the API's goal-key dedup also
    // returns Ok without re-issuing).
    float  x = 0.0f, y = 0.0f, z = 0.0f;
};

inline constexpr size_t kIntentHistoryCap = 32;

struct BotRegistryEntry
{
    std::unique_ptr<BotAI>          ai;
    std::unique_ptr<IntentQueue>    intents;
    IntentId                        next_intent_id = 0;
    // Intent execution ring. Pushed by BotIntentExecutor::DrainIntents on
    // the world thread; read by /diag (also world thread). Mutex bridges
    // the case where a future diagnostic panel reads from another thread.
    std::array<IntentHistoryEntry, kIntentHistoryCap> intent_history{};
    size_t                          intent_history_head = 0;   // next write slot
    size_t                          intent_history_size = 0;   // [0, kIntentHistoryCap]
    mutable std::mutex              intent_history_mtx;
    // Corpses queued for looting (FIFO). Drained by State_Idle on the AI
    // worker thread; written by V2::Module::OnDeath on the world thread.
    // The mutex bridges those threads — without it the deque races (both
    // sides do push_back/pop_front). Bounded by kPendingLootMax so a
    // runaway pull can't unbound this list.
    std::deque<ObjectGuid>          pending_loot;
    mutable std::mutex              pending_loot_mtx;
    // GameTimeMS at which intent execution may resume. 0 (or past) means
    // run normally. Set by /wait whisper. Read-only on the world thread
    // by the intent executor (it skips the bot for the tick when paused).
    // Atomic because writers are the world thread (parser) and readers are
    // also the world thread (executor) — same thread, but avoids a race
    // when other threads peek for diagnostics.
    std::atomic<uint32_t>           paused_until_ms{0};
};

inline constexpr size_t kPendingLootMax = 64;

class BotRegistry
{
public:
    void register_bot(BotId id, BotPersonality personality, BotRng rng);
    void unregister_bot(BotId id);
    bool has(BotId id) const;

    // Borrows — caller holds the snapshot lock implicitly via the registry.
    BotAI*         ai(BotId id);
    IntentQueue*   intents(BotId id);
    IntentId*      next_intent_id(BotId id);
    // Thread-safe loot queue access. Push appends if under the cap (silently
    // drops otherwise). try_pop_front removes and returns the oldest entry
    // (returns false if empty). peek_loot_size is a snapshot read.
    void   push_loot(BotId id, ObjectGuid corpse);
    bool   try_pop_loot(BotId id, ObjectGuid& out);
    size_t peek_loot_size(BotId id) const;

    // Append one intent-execution record for `id`. Silent no-op if the bot
    // isn't registered. Called from the world-thread intent executor right
    // after std::visit produces a Result.
    void record_intent_history(BotId id, uint32 ts_ms,
                               uint32 intent_kind, uint8 result,
                               float x = 0.0f, float y = 0.0f, float z = 0.0f);
    // Walk the per-bot intent history oldest-to-newest. fn takes
    // (size_t i, IntentHistoryEntry const&). Empty / unregistered → no-op.
    template <class F>
    void for_each_intent_history(BotId id, F&& fn) const
    {
        std::shared_lock lk(mtx_);
        auto it = entries_.find(id);
        if (it == entries_.end()) return;
        std::lock_guard hl(it->second.intent_history_mtx);
        const size_t n = it->second.intent_history_size;
        assert(n <= kIntentHistoryCap);
        const size_t start = (n < kIntentHistoryCap)
                                 ? 0
                                 : it->second.intent_history_head;
        for (size_t i = 0; i < n; ++i)
        {
            const size_t idx = (start + i) % kIntentHistoryCap;
            fn(i, it->second.intent_history[idx]);
        }
    }
    // Apply fn(deque&) under the per-bot loot mutex. Used by drainers that
    // need to inspect+pop atomically (e.g. distance check + pop).
    template <class F>
    void with_loot(BotId id, F&& fn)
    {
        std::shared_lock lk(mtx_);
        auto it = entries_.find(id);
        if (it == entries_.end()) return;
        std::lock_guard pl(it->second.pending_loot_mtx);
        fn(it->second.pending_loot);
    }

    size_t size() const;

    // Iteration helper — invokes fn under shared lock with id. Don't hold long.
    template <class F>
    void for_each(F&& fn) const
    {
        std::shared_lock lk(mtx_);
        for (auto const& [id, entry] : entries_)
            fn(id, entry);
    }

private:
    mutable std::shared_mutex mtx_;
    std::unordered_map<BotId, BotRegistryEntry> entries_;
};

} // namespace Playerbot
