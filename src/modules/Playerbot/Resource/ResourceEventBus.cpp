/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ResourceEventBus.h"
#include "BotAI.h"
#include "Player.h"
#include "Log.h"
#include "Timer.h"
#include <sstream>

namespace Playerbot
{

// ============================================================================
// ResourceEvent Helper Constructors
// ============================================================================

ResourceEvent ResourceEvent::PowerChanged(ObjectGuid player, Powers type, int32 amt, int32 max)
{
    ResourceEvent event;
    event.type = ResourceEventType::POWER_UPDATE;
    event.priority = ResourceEventPriority::MEDIUM;
    event.playerGuid = player;
    event.powerType = type;
    event.amount = amt;
    event.maxAmount = max;
    event.isRegen = false;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(5000);
    return event;
}

ResourceEvent ResourceEvent::PowerRegen(ObjectGuid player, Powers type, int32 amt)
{
    ResourceEvent event;
    event.type = ResourceEventType::POWER_UPDATE;
    event.priority = ResourceEventPriority::LOW;
    event.playerGuid = player;
    event.powerType = type;
    event.amount = amt;
    event.maxAmount = 0;
    event.isRegen = true;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(2000);
    return event;
}

ResourceEvent ResourceEvent::PowerDrained(ObjectGuid player, Powers type, int32 amt)
{
    ResourceEvent event;
    event.type = ResourceEventType::POWER_UPDATE;
    event.priority = ResourceEventPriority::HIGH;
    event.playerGuid = player;
    event.powerType = type;
    event.amount = -amt;
    event.maxAmount = 0;
    event.isRegen = false;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(10000);
    return event;
}

bool ResourceEvent::IsValid() const
{
    if (type >= ResourceEventType::MAX_RESOURCE_EVENT)
        return false;
    if (timestamp.time_since_epoch().count() == 0)
        return false;
    if (playerGuid.IsEmpty())
        return false;
    return true;
}

bool ResourceEvent::IsExpired() const
{
    return std::chrono::steady_clock::now() >= expiryTime;
}

std::string ResourceEvent::ToString() const
{
    std::ostringstream oss;
    oss << "[ResourceEvent] Type: " << static_cast<uint32>(type)
        << ", Player: " << playerGuid.ToString()
        << ", Power: " << static_cast<uint32>(powerType)
        << ", Amount: " << amount;
    return oss.str();
}

// ============================================================================
// ResourceEventBus Implementation
// ============================================================================

ResourceEventBus::ResourceEventBus()
{
    _stats.startTime = std::chrono::steady_clock::now();
    TC_LOG_INFO("module.playerbot.resource", "ResourceEventBus initialized");
}

ResourceEventBus::~ResourceEventBus()
{
    TC_LOG_INFO("module.playerbot.resource", "ResourceEventBus shutting down - Stats: {}", _stats.ToString());
}

ResourceEventBus* ResourceEventBus::instance()
{
    static ResourceEventBus instance;
    return &instance;
}

bool ResourceEventBus::PublishEvent(ResourceEvent const& event)
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

bool ResourceEventBus::Subscribe(BotAI* subscriber, std::vector<ResourceEventType> const& types)
{
    if (!subscriber)
        return false;

    std::lock_guard lock(_subscriberMutex);

    for (ResourceEventType type : types)
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

bool ResourceEventBus::SubscribeAll(BotAI* subscriber)
{
    if (!subscriber)
        return false;

    std::lock_guard lock(_subscriberMutex);

    if (std::find(_globalSubscribers.begin(), _globalSubscribers.end(), subscriber) != _globalSubscribers.end())
        return false;

    _globalSubscribers.push_back(subscriber);
    return true;
}

void ResourceEventBus::Unsubscribe(BotAI* subscriber)
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

uint32 ResourceEventBus::ProcessEvents(uint32 diff, uint32 maxEvents)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    _cleanupTimer += diff;
    if (_cleanupTimer >= CLEANUP_INTERVAL)
    {
        CleanupExpiredEvents();
        _cleanupTimer = 0;
    }

    uint32 processedCount = 0;
    std::vector<ResourceEvent> eventsToProcess;

    {
        std::lock_guard lock(_queueMutex);

        while (!_eventQueue.empty() && (maxEvents == 0 || processedCount < maxEvents))
        {
            ResourceEvent event = _eventQueue.top();
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

    for (ResourceEvent const& event : eventsToProcess)
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

uint32 ResourceEventBus::ProcessUnitEvents(ObjectGuid unitGuid, uint32 diff)
{
    return ProcessEvents(diff, 0);
}

void ResourceEventBus::ClearUnitEvents(ObjectGuid unitGuid)
{
    std::lock_guard lock(_queueMutex);

    std::vector<ResourceEvent> remainingEvents;

    while (!_eventQueue.empty())
    {
        ResourceEvent event = _eventQueue.top();
        _eventQueue.pop();

        if (event.playerGuid != unitGuid)
            remainingEvents.push_back(event);
        else
            _stats.totalEventsDropped++;
    }

    for (ResourceEvent const& event : remainingEvents)
        _eventQueue.push(event);
}

uint32 ResourceEventBus::GetPendingEventCount() const
{
    std::lock_guard lock(_queueMutex);
    return static_cast<uint32>(_eventQueue.size());
}

uint32 ResourceEventBus::GetSubscriberCount() const
{
    std::lock_guard lock(_subscriberMutex);

    uint32 count = static_cast<uint32>(_globalSubscribers.size());

    for (auto const& [type, subscriberList] : _subscribers)
        count += static_cast<uint32>(subscriberList.size());

    return count;
}

bool ResourceEventBus::DeliverEvent(BotAI* subscriber, ResourceEvent const& event)
{
    if (!subscriber)
        return false;

    try
    {
        // Phase 4: Call virtual event handler on BotAI
        subscriber->OnResourceEvent(event);
        TC_LOG_TRACE("module.playerbot.resource", "ResourceEventBus: Delivered event to subscriber");
        return true;
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot.resource", "ResourceEventBus: Exception delivering event: {}", e.what());
        return false;
    }
}

bool ResourceEventBus::ValidateEvent(ResourceEvent const& event) const
{
    return event.IsValid() && !event.IsExpired();
}

uint32 ResourceEventBus::CleanupExpiredEvents()
{
    std::lock_guard lock(_queueMutex);

    uint32 cleanedCount = 0;
    std::vector<ResourceEvent> validEvents;

    while (!_eventQueue.empty())
    {
        ResourceEvent event = _eventQueue.top();
        _eventQueue.pop();

        if (!event.IsExpired())
            validEvents.push_back(event);
        else
            cleanedCount++;
    }

    for (ResourceEvent const& event : validEvents)
        _eventQueue.push(event);

    return cleanedCount;
}

void ResourceEventBus::UpdateMetrics(std::chrono::microseconds processingTime)
{
    uint64_t currentAvg = _stats.averageProcessingTimeUs.load();
    uint64_t newTime = processingTime.count();
    uint64_t newAvg = (currentAvg * 9 + newTime) / 10;
    _stats.averageProcessingTimeUs.store(newAvg);
}

void ResourceEventBus::LogEvent(ResourceEvent const& event, std::string const& action) const
{
    TC_LOG_TRACE("module.playerbot.resource", "ResourceEventBus: {} event - {}", action, event.ToString());
}

void ResourceEventBus::DumpSubscribers() const
{
    std::lock_guard lock(_subscriberMutex);
    TC_LOG_INFO("module.playerbot.resource", "=== ResourceEventBus Subscribers: {} global ===", _globalSubscribers.size());
}

void ResourceEventBus::DumpEventQueue() const
{
    std::lock_guard lock(_queueMutex);
    TC_LOG_INFO("module.playerbot.resource", "=== ResourceEventBus Queue: {} events ===", _eventQueue.size());
}

std::vector<ResourceEvent> ResourceEventBus::GetQueueSnapshot() const
{
    std::lock_guard lock(_queueMutex);

    std::vector<ResourceEvent> snapshot;
    std::priority_queue<ResourceEvent> tempQueue = _eventQueue;

    while (!tempQueue.empty())
    {
        snapshot.push_back(tempQueue.top());
        tempQueue.pop();
    }

    return snapshot;
}

void ResourceEventBus::Statistics::Reset()
{
    totalEventsPublished.store(0);
    totalEventsProcessed.store(0);
    totalEventsDropped.store(0);
    totalDeliveries.store(0);
    averageProcessingTimeUs.store(0);
    peakQueueSize.store(0);
    startTime = std::chrono::steady_clock::now();
}

std::string ResourceEventBus::Statistics::ToString() const
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
