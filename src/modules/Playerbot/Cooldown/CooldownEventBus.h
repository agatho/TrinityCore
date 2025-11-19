/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_COOLDOWN_EVENT_BUS_H
#define PLAYERBOT_COOLDOWN_EVENT_BUS_H

#include "Define.h"
#include "Threading/LockHierarchy.h"
#include "ObjectGuid.h"
#include "Core/DI/Interfaces/ICooldownEventBus.h"
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

enum class CooldownEventType : uint8
{
    SPELL_COOLDOWN_START = 0,
    SPELL_COOLDOWN_CLEAR,
    SPELL_COOLDOWN_MODIFY,
    SPELL_COOLDOWNS_CLEAR_ALL,
    ITEM_COOLDOWN_START,
    CATEGORY_COOLDOWN_START,
    MAX_COOLDOWN_EVENT
};

enum class CooldownEventPriority : uint8
{
    CRITICAL = 0,
    HIGH = 1,
    MEDIUM = 2,
    LOW = 3,
    BATCH = 4
};

struct CooldownEvent
{
    CooldownEventType type;
    CooldownEventPriority priority;
    ObjectGuid casterGuid;
    uint32 spellId;
    uint32 itemId;
    uint32 category;
    uint32 cooldownMs;
    int32 modRateMs;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::steady_clock::time_point expiryTime;

    bool IsValid() const;
    bool IsExpired() const;
    std::string ToString() const;

    // Helper constructors
    static CooldownEvent SpellCooldownStart(ObjectGuid caster, uint32 spellId, uint32 cooldownMs);
    static CooldownEvent SpellCooldownClear(ObjectGuid caster, uint32 spellId);
    static CooldownEvent ItemCooldownStart(ObjectGuid caster, uint32 itemId, uint32 cooldownMs);

    // Priority comparison for priority queue
    bool operator<(CooldownEvent const& other) const
    {
        return priority > other.priority; // Higher priority = lower value
    }
};

class TC_GAME_API CooldownEventBus final : public ICooldownEventBus
{
public:
    static CooldownEventBus* instance();

    // Event publishing
    bool PublishEvent(CooldownEvent const& event) override;

    // Subscription management
    bool Subscribe(BotAI* subscriber, std::vector<CooldownEventType> const& types) override;
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
    std::vector<CooldownEvent> GetQueueSnapshot() const override;

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
    CooldownEventBus();
    ~CooldownEventBus();

    // Event delivery
    bool DeliverEvent(BotAI* subscriber, CooldownEvent const& event);
    bool ValidateEvent(CooldownEvent const& event) const;
    uint32 CleanupExpiredEvents();
    void UpdateMetrics(std::chrono::microseconds processingTime);
    void LogEvent(CooldownEvent const& event, std::string const& action) const;

    // Event queue
    std::priority_queue<CooldownEvent> _eventQueue;
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BEHAVIOR_MANAGER> _queueMutex;

    // Subscriber management
    std::unordered_map<CooldownEventType, std::vector<BotAI*>> _subscribers;
    std::vector<BotAI*> _globalSubscribers;
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BEHAVIOR_MANAGER> _subscriberMutex;

    // Configuration
    static constexpr uint32 MAX_QUEUE_SIZE = 10000;
    static constexpr uint32 CLEANUP_INTERVAL = 30000; // 30 seconds
    static constexpr uint32 MAX_SUBSCRIBERS_PER_EVENT = 5000;

    // Timers
    uint32 _cleanupTimer = 0;
    uint32 _metricsUpdateTimer = 0;

    // Statistics
    Statistics _stats;
    uint32 _maxQueueSize = MAX_QUEUE_SIZE;
};

} // namespace Playerbot

#endif // PLAYERBOT_COOLDOWN_EVENT_BUS_H
