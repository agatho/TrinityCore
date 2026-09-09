#include "SnapshotPublisher.h"

namespace Playerbot {

SnapshotPublisher::SnapshotPublisher() = default;
SnapshotPublisher::~SnapshotPublisher() = default;

void SnapshotPublisher::publish(BotId id, std::shared_ptr<BotSnapshot const> snap)
{
    {
        std::shared_lock lk(map_mtx_);
        auto it = slots_.find(id);
        if (it != slots_.end())
        {
            it->second->store(std::move(snap), std::memory_order_release);
            return;
        }
    }
    // Slow path: insert under exclusive lock.
    std::unique_lock lk(map_mtx_);
    auto [it, _ins] = slots_.try_emplace(id, std::make_unique<AtomicSnap>());
    it->second->store(std::move(snap), std::memory_order_release);
}

std::shared_ptr<BotSnapshot const> SnapshotPublisher::latest(BotId id) const
{
    std::shared_lock lk(map_mtx_);
    auto it = slots_.find(id);
    if (it == slots_.end()) return {};
    return it->second->load(std::memory_order_acquire);
}

void SnapshotPublisher::publish_group(BotId id, std::shared_ptr<GroupSnapshot const> snap)
{
    {
        std::shared_lock lk(map_mtx_);
        auto it = group_slots_.find(id);
        if (it != group_slots_.end())
        {
            it->second->store(std::move(snap), std::memory_order_release);
            return;
        }
    }
    std::unique_lock lk(map_mtx_);
    auto [it, _ins] = group_slots_.try_emplace(id, std::make_unique<AtomicGroupSnap>());
    it->second->store(std::move(snap), std::memory_order_release);
}

std::shared_ptr<GroupSnapshot const> SnapshotPublisher::latest_group(BotId id) const
{
    std::shared_lock lk(map_mtx_);
    auto it = group_slots_.find(id);
    if (it == group_slots_.end()) return {};
    return it->second->load(std::memory_order_acquire);
}

void SnapshotPublisher::remove(BotId id)
{
    std::unique_lock lk(map_mtx_);
    slots_.erase(id);
    group_slots_.erase(id);
}

size_t SnapshotPublisher::bot_count() const
{
    std::shared_lock lk(map_mtx_);
    return slots_.size();
}

} // namespace Playerbot
