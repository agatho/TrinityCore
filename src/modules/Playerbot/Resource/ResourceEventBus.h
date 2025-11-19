/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_RESOURCE_EVENT_BUS_H
#define PLAYERBOT_RESOURCE_EVENT_BUS_H

#include "Define.h"
#include "Threading/LockHierarchy.h"
#include "ObjectGuid.h"
#include "Core/DI/Interfaces/IResourceEventBus.h"
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <atomic>

namespace Playerbot
{

class BotAI;

enum class ResourceEventType : uint8
{
    HEALTH_UPDATE = 0,
    POWER_UPDATE,
    BREAK_TARGET,
    MAX_RESOURCE_EVENT
};

enum class ResourceEventPriority : uint8
{
    CRITICAL = 0,
    HIGH = 1,
    MEDIUM = 2,
    LOW = 3,
    BATCH = 4
};

enum class Powers : uint8
{
    POWER_MANA = 0,
    POWER_RAGE = 1,
    POWER_FOCUS = 2,
    POWER_ENERGY = 3,
    POWER_RUNIC_POWER = 6
};

struct ResourceEvent
{
    ResourceEventType type;
    ResourceEventPriority priority;
    ObjectGuid playerGuid;
    Powers powerType;
    int32 amount;
    int32 maxAmount;
    bool isRegen;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::steady_clock::time_point expiryTime;

    bool IsValid() const;
    bool IsExpired() const;
    std::string ToString() const;

    // Helper constructors
    static ResourceEvent PowerChanged(ObjectGuid player, Powers type, int32 amt, int32 max);
    static ResourceEvent PowerRegen(ObjectGuid player, Powers type, int32 amt);
    static ResourceEvent PowerDrained(ObjectGuid player, Powers type, int32 amt);

    // Priority comparison for priority queue
    bool operator<(ResourceEvent const& other) const
    {
        return priority > other.priority;
    }
};

class TC_GAME_API ResourceEventBus final : public IResourceEventBus
{
public:
    static ResourceEventBus* instance();

    // Event publishing
    bool PublishEvent(ResourceEvent const& event) override;

    // Subscription management
    bool Subscribe(BotAI* subscriber, std::vector<ResourceEventType> const& types) override;
    bool SubscribeAll(BotAI* subscriber) override;
    void Unsubscribe(BotAI* subscriber) override;

    // Event processing
    uint32 ProcessEvents(uint32 diff, uint32 maxEvents = 0) override;
    uint32 ProcessUnitEvents(ObjectGuid unitGuid, uint32 diff) override;
    void ClearUnitEvents(ObjectGuid unitGuid) override;

    // Status queries
    uint32 GetPendingEventCount() const override;
    uint32 GetSubscriberCount() const override;

    // Diagnostics
    void DumpSubscribers() const override;
    void DumpEventQueue() const override;
    std::vector<ResourceEvent> GetQueueSnapshot() const override;

    // Statistics
    struct Statistics
    {
        std::atomic<uint64_t> totalEventsPublished{0};
        std::atomic<uint64_t> totalEventsProcessed{0};
        std::atomic<uint64_t> totalEventsDropped{0};
        std::atomic<uint64_t> totalDeliveries{0};
        std::atomic<uint64_t> averageProcessingTimeUs{0};
        std::atomic<uint32_t> peakQueueSize{0};
        std::chrono::steady_clock::time_point startTime;

        void Reset();
        std::string ToString() const;
    };

    Statistics const& GetStatistics() const { return _stats; }

private:
    ResourceEventBus();
    ~ResourceEventBus();

    // Event delivery
    bool DeliverEvent(BotAI* subscriber, ResourceEvent const& event);
    bool ValidateEvent(ResourceEvent const& event) const;
    uint32 CleanupExpiredEvents();
    void UpdateMetrics(std::chrono::microseconds processingTime);
    void LogEvent(ResourceEvent const& event, std::string const& action) const;

    // Event queue
    std::priority_queue<ResourceEvent> _eventQueue;
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BEHAVIOR_MANAGER> _queueMutex;

    // Subscriber management
    std::unordered_map<ResourceEventType, std::vector<BotAI*>> _subscribers;
    std::vector<BotAI*> _globalSubscribers;
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BEHAVIOR_MANAGER> _subscriberMutex;

    // Configuration
    static constexpr uint32 MAX_QUEUE_SIZE = 10000;
    static constexpr uint32 CLEANUP_INTERVAL = 30000;
    static constexpr uint32 MAX_SUBSCRIBERS_PER_EVENT = 5000;

    // Timers
    uint32 _cleanupTimer = 0;
    uint32 _metricsUpdateTimer = 0;

    // Statistics
    Statistics _stats;
    uint32 _maxQueueSize = MAX_QUEUE_SIZE;
};

} // namespace Playerbot

#endif // PLAYERBOT_RESOURCE_EVENT_BUS_H

