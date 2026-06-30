#include "BotRegistry.h"

namespace Playerbot {

void BotRegistry::register_bot(BotId id, BotPersonality personality, BotRng rng)
{
    // BotRegistryEntry holds a std::mutex, so it's not movable. Construct in
    // place via try_emplace then populate the unique_ptrs directly.
    std::unique_lock lk(mtx_);
    auto [it, inserted] = entries_.try_emplace(id);
    if (!inserted) return;
    auto& entry = it->second;
    entry.ai      = std::make_unique<BotAI>(id, std::move(personality), rng);
    entry.intents = std::make_unique<IntentQueue>();
    entry.next_intent_id = 0;
}

void BotRegistry::unregister_bot(BotId id)
{
    std::unique_lock lk(mtx_);
    entries_.erase(id);
}

bool BotRegistry::has(BotId id) const
{
    std::shared_lock lk(mtx_);
    return entries_.find(id) != entries_.end();
}

BotAI* BotRegistry::ai(BotId id)
{
    std::shared_lock lk(mtx_);
    auto it = entries_.find(id);
    return it == entries_.end() ? nullptr : it->second.ai.get();
}

IntentQueue* BotRegistry::intents(BotId id)
{
    std::shared_lock lk(mtx_);
    auto it = entries_.find(id);
    return it == entries_.end() ? nullptr : it->second.intents.get();
}

IntentId* BotRegistry::next_intent_id(BotId id)
{
    std::shared_lock lk(mtx_);
    auto it = entries_.find(id);
    return it == entries_.end() ? nullptr : &it->second.next_intent_id;
}

void BotRegistry::push_loot(BotId id, ObjectGuid corpse)
{
    std::shared_lock lk(mtx_);
    auto it = entries_.find(id);
    if (it == entries_.end()) return;
    std::lock_guard pl(it->second.pending_loot_mtx);
    if (it->second.pending_loot.size() < kPendingLootMax)
        it->second.pending_loot.push_back(corpse);
}

bool BotRegistry::try_pop_loot(BotId id, ObjectGuid& out)
{
    std::shared_lock lk(mtx_);
    auto it = entries_.find(id);
    if (it == entries_.end()) return false;
    std::lock_guard pl(it->second.pending_loot_mtx);
    if (it->second.pending_loot.empty()) return false;
    out = it->second.pending_loot.front();
    it->second.pending_loot.pop_front();
    return true;
}

size_t BotRegistry::peek_loot_size(BotId id) const
{
    std::shared_lock lk(mtx_);
    auto it = entries_.find(id);
    if (it == entries_.end()) return 0;
    std::lock_guard pl(it->second.pending_loot_mtx);
    return it->second.pending_loot.size();
}

size_t BotRegistry::size() const
{
    std::shared_lock lk(mtx_);
    return entries_.size();
}

void BotRegistry::record_intent_history(BotId id, uint32 ts_ms,
                                        uint32 intent_kind, uint8 result)
{
    std::shared_lock lk(mtx_);
    auto it = entries_.find(id);
    if (it == entries_.end()) return;
    auto& e = it->second;
    std::lock_guard hl(e.intent_history_mtx);
    e.intent_history[e.intent_history_head] =
        IntentHistoryEntry{ts_ms, intent_kind, result};
    e.intent_history_head = (e.intent_history_head + 1) % kIntentHistoryCap;
    if (e.intent_history_size < kIntentHistoryCap) ++e.intent_history_size;
    assert(e.intent_history_size <= kIntentHistoryCap);
}

} // namespace Playerbot
