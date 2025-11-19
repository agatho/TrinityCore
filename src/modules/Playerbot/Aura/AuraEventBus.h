/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_AURA_EVENT_BUS_H
#define PLAYERBOT_AURA_EVENT_BUS_H

#include "Define.h"
#include "Threading/LockHierarchy.h"
#include "ObjectGuid.h"
#include "Core/DI/Interfaces/IAuraEventBus.h"
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

enum class AuraEventType : uint8
{
    AURA_APPLIED = 0,
    AURA_REMOVED,
    AURA_UPDATED,
    DISPEL_FAILED,
    SPELL_MODIFIER_CHANGED,
    MAX_AURA_EVENT
};

enum class AuraEventPriority : uint8
{
    CRITICAL = 0,
    HIGH = 1,
    MEDIUM = 2,
    LOW = 3,
    BATCH = 4
};

struct AuraEvent
{
    AuraEventType type;
    AuraEventPriority priority;
    ObjectGuid targetGuid;
    ObjectGuid casterGuid;
    uint32 spellId;
    uint32 auraSlot;
    uint8 stackCount;
    uint32 duration;
    bool isBuff;
    bool isHarmful;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::steady_clock::time_point expiryTime;

    bool IsValid() const;
    bool IsExpired() const;
    std::string ToString() const;

    // Helper constructors
    static AuraEvent AuraApplied(ObjectGuid target, ObjectGuid caster, uint32 spellId, uint8 stacks, bool harmful);
    static AuraEvent AuraRemoved(ObjectGuid target, uint32 spellId);
    static AuraEvent AuraUpdated(ObjectGuid target, uint32 spellId, uint8 stacks);

    // Priority comparison for priority queue
    bool operator<(AuraEvent const& other) const
    {
        return priority > other.priority;
    }
};

class TC_GAME_API AuraEventBus final : public IAuraEventBus
{
public:
    static AuraEventBus* instance();

    // Event publishing
    bool PublishEvent(AuraEvent const& event) override;

    // Subscription management
    bool Subscribe(BotAI* subscriber, std::vector<AuraEventType> const& types) override;
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
    std::vector<AuraEvent> GetQueueSnapshot() const override;

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
    AuraEventBus();
    ~AuraEventBus();

    // Event delivery
    bool DeliverEvent(BotAI* subscriber, AuraEvent const& event);
    bool ValidateEvent(AuraEvent const& event) const;
    uint32 CleanupExpiredEvents();
    void UpdateMetrics(std::chrono::microseconds processingTime);
    void LogEvent(AuraEvent const& event, std::string const& action) const;

    // Event queue
    std::priority_queue<AuraEvent> _eventQueue;
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BEHAVIOR_MANAGER> _queueMutex;

    // Subscriber management
    std::unordered_map<AuraEventType, std::vector<BotAI*>> _subscribers;
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

#endif // PLAYERBOT_AURA_EVENT_BUS_H
