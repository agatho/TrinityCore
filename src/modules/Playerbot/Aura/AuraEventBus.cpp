/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "AuraEventBus.h"
#include "BotAI.h"
#include "Player.h"
#include "Log.h"
#include "Timer.h"
#include <sstream>

namespace Playerbot
{

// ============================================================================
// AuraEvent Helper Constructors
// ============================================================================

AuraEvent AuraEvent::AuraApplied(ObjectGuid target, ObjectGuid caster, uint32 spellId, uint8 stacks, bool harmful)
{
    AuraEvent event;
    event.type = AuraEventType::AURA_APPLIED;
    event.priority = harmful ? AuraEventPriority::HIGH : AuraEventPriority::MEDIUM;
    event.targetGuid = target;
    event.casterGuid = caster;
    event.spellId = spellId;
    event.auraSlot = 0;
    event.stackCount = stacks;
    event.duration = 0;
    event.isBuff = !harmful;
    event.isHarmful = harmful;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(30000);
    return event;
}

AuraEvent AuraEvent::AuraRemoved(ObjectGuid target, uint32 spellId)
{
    AuraEvent event;
    event.type = AuraEventType::AURA_REMOVED;
    event.priority = AuraEventPriority::MEDIUM;
    event.targetGuid = target;
    event.casterGuid = ObjectGuid::Empty;
    event.spellId = spellId;
    event.auraSlot = 0;
    event.stackCount = 0;
    event.duration = 0;
    event.isBuff = false;
    event.isHarmful = false;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(10000);
    return event;
}

AuraEvent AuraEvent::AuraUpdated(ObjectGuid target, uint32 spellId, uint8 stacks)
{
    AuraEvent event;
    event.type = AuraEventType::AURA_UPDATED;
    event.priority = AuraEventPriority::LOW;
    event.targetGuid = target;
    event.casterGuid = ObjectGuid::Empty;
    event.spellId = spellId;
    event.auraSlot = 0;
    event.stackCount = stacks;
    event.duration = 0;
    event.isBuff = false;
    event.isHarmful = false;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(10000);
    return event;
}

bool AuraEvent::IsValid() const
{
    if (type >= AuraEventType::MAX_AURA_EVENT)
        return false;
    if (timestamp.time_since_epoch().count() == 0)
        return false;
    if (targetGuid.IsEmpty())
        return false;
    return true;
}

bool AuraEvent::IsExpired() const
{
    return std::chrono::steady_clock::now() >= expiryTime;
}

std::string AuraEvent::ToString() const
{
    std::ostringstream oss;
    oss << "[AuraEvent] Type: " << static_cast<uint32>(type)
        << ", Target: " << targetGuid.ToString()
        << ", Spell: " << spellId
        << ", Stacks: " << static_cast<uint32>(stackCount)
        << ", Harmful: " << isHarmful;
    return oss.str();
}

// ============================================================================
// AuraEventBus Implementation
// ============================================================================

AuraEventBus::AuraEventBus()
{
    _stats.startTime = std::chrono::steady_clock::now();
    TC_LOG_INFO("module.playerbot.aura", "AuraEventBus initialized");
}

AuraEventBus::~AuraEventBus()
{
    TC_LOG_INFO("module.playerbot.aura", "AuraEventBus shutting down - Stats: {}", _stats.ToString());
}

AuraEventBus* AuraEventBus::instance()
{
    static AuraEventBus instance;
    return &instance;
}

bool AuraEventBus::PublishEvent(AuraEvent const& event)
{
    if (!ValidateEvent(event))
    {
        _stats.totalEventsDropped++;
        return false;
    }

    {
        std::lock_guard lock(_queueMutex);
        if (_eventQueue.size() >= _maxQueueSize)
        {
            _stats.totalEventsDropped++;
            return false;
        }

        _eventQueue.push(event);

        uint32 currentSize = static_cast<uint32>(_eventQueue.size());
        uint32 expectedPeak = _stats.peakQueueSize.load();
        while (currentSize > expectedPeak &&
               !_stats.peakQueueSize.compare_exchange_weak(expectedPeak, currentSize))
        {
        }
    }

    _stats.totalEventsPublished++;
    LogEvent(event, "Published");
    return true;
}

bool AuraEventBus::Subscribe(BotAI* subscriber, std::vector<AuraEventType> const& types)
{
    if (!subscriber)
        return false;

    std::lock_guard lock(_subscriberMutex);

    for (AuraEventType type : types)
    {
        auto& subscriberList = _subscribers[type];
        if (std::find(subscriberList.begin(), subscriberList.end(), subscriber) != subscriberList.end())
            continue;
        if (subscriberList.size() >= MAX_SUBSCRIBERS_PER_EVENT)
            return false;
        subscriberList.push_back(subscriber);
    }

    return true;
}

bool AuraEventBus::SubscribeAll(BotAI* subscriber)
{
    if (!subscriber)
        return false;

    std::lock_guard lock(_subscriberMutex);

    if (std::find(_globalSubscribers.begin(), _globalSubscribers.end(), subscriber) != _globalSubscribers.end())
        return false;

    _globalSubscribers.push_back(subscriber);
    return true;
}

void AuraEventBus::Unsubscribe(BotAI* subscriber)
{
    if (!subscriber)
        return;

    std::lock_guard lock(_subscriberMutex);

    for (auto& [type, subscriberList] : _subscribers)
    {
        subscriberList.erase(
            std::remove(subscriberList.begin(), subscriberList.end(), subscriber),
            subscriberList.end()
        );
    }

    _globalSubscribers.erase(
        std::remove(_globalSubscribers.begin(), _globalSubscribers.end(), subscriber),
        _globalSubscribers.end()
    );
}

uint32 AuraEventBus::ProcessEvents(uint32 diff, uint32 maxEvents)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    _cleanupTimer += diff;
    if (_cleanupTimer >= CLEANUP_INTERVAL)
    {
        CleanupExpiredEvents();
        _cleanupTimer = 0;
    }

    uint32 processedCount = 0;
    std::vector<AuraEvent> eventsToProcess;

    {
        std::lock_guard lock(_queueMutex);

        while (!_eventQueue.empty() && (maxEvents == 0 || processedCount < maxEvents))
        {
            AuraEvent event = _eventQueue.top();
            _eventQueue.pop();

            if (event.IsExpired())
            {
                _stats.totalEventsDropped++;
                continue;
            }

            eventsToProcess.push_back(event);
            processedCount++;
        }
    }

    for (AuraEvent const& event : eventsToProcess)
    {
        std::vector<BotAI*> subscribers;
        std::vector<BotAI*> globalSubs;

        {
            std::lock_guard lock(_subscriberMutex);
            auto it = _subscribers.find(event.type);
            if (it != _subscribers.end())
                subscribers = it->second;
            globalSubs = _globalSubscribers;
        }

        for (BotAI* subscriber : subscribers)
        {
            if (DeliverEvent(subscriber, event))
                _stats.totalDeliveries++;
        }

        for (BotAI* subscriber : globalSubs)
        {
            if (DeliverEvent(subscriber, event))
                _stats.totalDeliveries++;
        }

        _stats.totalEventsProcessed++;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    if (processedCount > 0)
        UpdateMetrics(duration);

    return processedCount;
}

uint32 AuraEventBus::ProcessUnitEvents(ObjectGuid unitGuid, uint32 diff)
{
    return ProcessEvents(diff, 0);
}

void AuraEventBus::ClearUnitEvents(ObjectGuid unitGuid)
{
    std::lock_guard lock(_queueMutex);

    std::vector<AuraEvent> remainingEvents;

    while (!_eventQueue.empty())
    {
        AuraEvent event = _eventQueue.top();
        _eventQueue.pop();

        if (event.targetGuid != unitGuid)
            remainingEvents.push_back(event);
        else
            _stats.totalEventsDropped++;
    }

    for (AuraEvent const& event : remainingEvents)
        _eventQueue.push(event);
}

uint32 AuraEventBus::GetPendingEventCount() const
{
    std::lock_guard lock(_queueMutex);
    return static_cast<uint32>(_eventQueue.size());
}

uint32 AuraEventBus::GetSubscriberCount() const
{
    std::lock_guard lock(_subscriberMutex);

    uint32 count = static_cast<uint32>(_globalSubscribers.size());

    for (auto const& [type, subscriberList] : _subscribers)
        count += static_cast<uint32>(subscriberList.size());

    return count;
}

bool AuraEventBus::DeliverEvent(BotAI* subscriber, AuraEvent const& event)
{
    if (!subscriber)
        return false;

    try
    {
        // Phase 4: Call virtual event handler on BotAI
        subscriber->OnAuraEvent(event);
        TC_LOG_TRACE("module.playerbot.aura", "AuraEventBus: Delivered event to subscriber");
        return true;
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot.aura", "AuraEventBus: Exception delivering event: {}", e.what());
        return false;
    }
}

bool AuraEventBus::ValidateEvent(AuraEvent const& event) const
{
    return event.IsValid() && !event.IsExpired();
}

uint32 AuraEventBus::CleanupExpiredEvents()
{
    std::lock_guard lock(_queueMutex);

    uint32 cleanedCount = 0;
    std::vector<AuraEvent> validEvents;

    while (!_eventQueue.empty())
    {
        AuraEvent event = _eventQueue.top();
        _eventQueue.pop();

        if (!event.IsExpired())
            validEvents.push_back(event);
        else
            cleanedCount++;
    }

    for (AuraEvent const& event : validEvents)
        _eventQueue.push(event);

    return cleanedCount;
}

void AuraEventBus::UpdateMetrics(std::chrono::microseconds processingTime)
{
    uint64_t currentAvg = _stats.averageProcessingTimeUs.load();
    uint64_t newTime = processingTime.count();
    uint64_t newAvg = (currentAvg * 9 + newTime) / 10;
    _stats.averageProcessingTimeUs.store(newAvg);
}

void AuraEventBus::LogEvent(AuraEvent const& event, std::string const& action) const
{
    TC_LOG_TRACE("module.playerbot.aura", "AuraEventBus: {} event - {}", action, event.ToString());
}

void AuraEventBus::DumpSubscribers() const
{
    std::lock_guard lock(_subscriberMutex);
    TC_LOG_INFO("module.playerbot.aura", "=== AuraEventBus Subscribers: {} global ===", _globalSubscribers.size());
}

void AuraEventBus::DumpEventQueue() const
{
    std::lock_guard lock(_queueMutex);
    TC_LOG_INFO("module.playerbot.aura", "=== AuraEventBus Queue: {} events ===", _eventQueue.size());
}

std::vector<AuraEvent> AuraEventBus::GetQueueSnapshot() const
{
    std::lock_guard lock(_queueMutex);

    std::vector<AuraEvent> snapshot;
    std::priority_queue<AuraEvent> tempQueue = _eventQueue;

    while (!tempQueue.empty())
    {
        snapshot.push_back(tempQueue.top());
        tempQueue.pop();
    }

    return snapshot;
}

void AuraEventBus::Statistics::Reset()
{
    totalEventsPublished.store(0);
    totalEventsProcessed.store(0);
    totalEventsDropped.store(0);
    totalDeliveries.store(0);
    averageProcessingTimeUs.store(0);
    peakQueueSize.store(0);
    startTime = std::chrono::steady_clock::now();
}

std::string AuraEventBus::Statistics::ToString() const
{
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);

    std::ostringstream oss;
    oss << "Published: " << totalEventsPublished.load()
        << ", Processed: " << totalEventsProcessed.load()
        << ", Dropped: " << totalEventsDropped.load()
        << ", Deliveries: " << totalDeliveries.load()
        << ", Avg Processing: " << averageProcessingTimeUs.load() << "μs"
        << ", Peak Queue: " << peakQueueSize.load()
        << ", Uptime: " << uptime.count() << "s";
    return oss.str();
}

} // namespace Playerbot
