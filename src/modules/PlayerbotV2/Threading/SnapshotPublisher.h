// SnapshotPublisher - Atomic per-bot snapshot slot. World thread publishes;
// AI workers read latest. CONTRACTS.md §5.2.

#pragma once

#include "Bot/BotSnapshot.h"
#include "Group/GroupSnapshot.h"
#include <atomic>
#include <memory>
#include <shared_mutex>
#include <unordered_map>

namespace Playerbot {

class SnapshotPublisher
{
public:
    SnapshotPublisher();
    ~SnapshotPublisher();

    // Called on world thread at start of each world tick.
    void publish(BotId id, std::shared_ptr<BotSnapshot const> snap);
    void publish_group(BotId id, std::shared_ptr<GroupSnapshot const> snap);

    // Called by AI workers (or fleet thread). Returns latest, or null if
    // no snapshot has been published for this bot yet.
    std::shared_ptr<BotSnapshot const>   latest(BotId id) const;
    std::shared_ptr<GroupSnapshot const> latest_group(BotId id) const;

    // Called when a bot is despawning to free the slot.
    void remove(BotId id);

    // Diagnostics
    size_t bot_count() const;

private:
    // Per-slot atomic shared_ptr access is lock-free in C++20. The
    // shared_mutex below guards *map structure* (insert/remove), not
    // per-slot reads/writes — those are atomic.
    //
    // The atomic is held via unique_ptr because std::atomic<shared_ptr<T>>
    // is neither copyable nor movable, which would prevent map rehashing.
    using AtomicSnap      = std::atomic<std::shared_ptr<BotSnapshot const>>;
    using AtomicGroupSnap = std::atomic<std::shared_ptr<GroupSnapshot const>>;
    mutable std::shared_mutex                                  map_mtx_;
    std::unordered_map<BotId, std::unique_ptr<AtomicSnap>>      slots_;
    std::unordered_map<BotId, std::unique_ptr<AtomicGroupSnap>> group_slots_;
};

} // namespace Playerbot
