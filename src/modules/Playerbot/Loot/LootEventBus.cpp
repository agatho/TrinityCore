/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "LootEventBus.h"
#include "BotAI.h"
#include "Player.h"
#include "Log.h"
#include "Timer.h"
#include <sstream>

namespace Playerbot
{

// ============================================================================
// LootEvent Helper Constructors
// ============================================================================

LootEvent LootEvent::ItemLooted(ObjectGuid looter, ObjectGuid item, uint32 entry, uint32 count, LootType type)
{
    LootEvent event;
    event.type = LootEventType::LOOT_ITEM_RECEIVED;
    event.priority = LootEventPriority::MEDIUM;
    event.looterGuid = looter;
    event.itemGuid = item;
    event.itemEntry = entry;
    event.itemCount = count;
    event.lootType = type;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(30000);
    return event;
}

LootEvent LootEvent::LootRollStarted(ObjectGuid item, uint32 entry)
{
    LootEvent event;
    event.type = LootEventType::LOOT_ROLL_STARTED;
    event.priority = LootEventPriority::HIGH;
    event.looterGuid = ObjectGuid::Empty;
    event.itemGuid = item;
    event.itemEntry = entry;
    event.itemCount = 1;
    event.lootType = LootType::CORPSE;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(60000);
    return event;
}

LootEvent LootEvent::LootRollWon(ObjectGuid winner, ObjectGuid item, uint32 entry)
{
    LootEvent event;
    event.type = LootEventType::LOOT_ROLL_WON;
    event.priority = LootEventPriority::HIGH;
    event.looterGuid = winner;
    event.itemGuid = item;
    event.itemEntry = entry;
    event.itemCount = 1;
    event.lootType = LootType::CORPSE;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(10000);
    return event;
}

bool LootEvent::IsValid() const
{
    if (type >= LootEventType::MAX_LOOT_EVENT)
        return false;
    if (timestamp.time_since_epoch().count() == 0)
        return false;
    if (looterGuid.IsEmpty() && type != LootEventType::LOOT_ROLL_STARTED)
        return false;
    return true;
}

bool LootEvent::IsExpired() const
{
    return std::chrono::steady_clock::now() >= expiryTime;
}

std::string LootEvent::ToString() const
{
    std::ostringstream oss;
    oss << "[LootEvent] Type: " << static_cast<uint32>(type)
        << ", Looter: " << looterGuid.ToString()
        << ", Item: " << itemEntry
        << " x" << itemCount;
    return oss.str();
}

// ============================================================================
// LootEventBus Implementation
// ============================================================================

LootEventBus::LootEventBus()
{
    _stats.startTime = std::chrono::steady_clock::now();
    TC_LOG_INFO("module.playerbot.loot", "LootEventBus initialized");
}

LootEventBus::~LootEventBus()
{
    TC_LOG_INFO("module.playerbot.loot", "LootEventBus shutting down - Stats: {}", _stats.ToString());
}

LootEventBus* LootEventBus::instance()
{
    static LootEventBus instance;
    return &instance;
}

bool LootEventBus::PublishEvent(LootEvent const& event)
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

bool LootEventBus::Subscribe(BotAI* subscriber, std::vector<LootEventType> const& types)
{
    if (!subscriber)
        return false;

    std::lock_guard lock(_subscriberMutex);

    for (LootEventType type : types)
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

bool LootEventBus::SubscribeAll(BotAI* subscriber)
{
    if (!subscriber)
        return false;

    std::lock_guard lock(_subscriberMutex);

    if (std::find(_globalSubscribers.begin(), _globalSubscribers.end(), subscriber) != _globalSubscribers.end())
        return false;

    _globalSubscribers.push_back(subscriber);
    return true;
}

void LootEventBus::Unsubscribe(BotAI* subscriber)
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

uint32 LootEventBus::ProcessEvents(uint32 diff, uint32 maxEvents)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    _cleanupTimer += diff;
    if (_cleanupTimer >= CLEANUP_INTERVAL)
    {
        CleanupExpiredEvents();
        _cleanupTimer = 0;
    }

    uint32 processedCount = 0;
    std::vector<LootEvent> eventsToProcess;

    {
        std::lock_guard lock(_queueMutex);

        while (!_eventQueue.empty() && (maxEvents == 0 || processedCount < maxEvents))
        {
            LootEvent event = _eventQueue.top();
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

    for (LootEvent const& event : eventsToProcess)
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

uint32 LootEventBus::ProcessUnitEvents(ObjectGuid unitGuid, uint32 diff)
{
    return ProcessEvents(diff, 0);
}

void LootEventBus::ClearUnitEvents(ObjectGuid unitGuid)
{
    std::lock_guard lock(_queueMutex);

    std::vector<LootEvent> remainingEvents;

    while (!_eventQueue.empty())
    {
        LootEvent event = _eventQueue.top();
        _eventQueue.pop();

        if (event.looterGuid != unitGuid)
            remainingEvents.push_back(event);
        else
            _stats.totalEventsDropped++;
    }

    for (LootEvent const& event : remainingEvents)
        _eventQueue.push(event);
}

uint32 LootEventBus::GetPendingEventCount() const
{
    std::lock_guard lock(_queueMutex);
    return static_cast<uint32>(_eventQueue.size());
}

uint32 LootEventBus::GetSubscriberCount() const
{
    std::lock_guard lock(_subscriberMutex);

    uint32 count = static_cast<uint32>(_globalSubscribers.size());

    for (auto const& [type, subscriberList] : _subscribers)
        count += static_cast<uint32>(subscriberList.size());

    return count;
}

bool LootEventBus::DeliverEvent(BotAI* subscriber, LootEvent const& event)
{
    if (!subscriber)
        return false;

    try
    {
        // Phase 4: Call virtual event handler on BotAI
        subscriber->OnLootEvent(event);
        TC_LOG_TRACE("module.playerbot.loot", "LootEventBus: Delivered event to subscriber");
        return true;
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot.loot", "LootEventBus: Exception delivering event: {}", e.what());
        return false;
    }
}

bool LootEventBus::ValidateEvent(LootEvent const& event) const
{
    return event.IsValid() && !event.IsExpired();
}

uint32 LootEventBus::CleanupExpiredEvents()
{
    std::lock_guard lock(_queueMutex);

    uint32 cleanedCount = 0;
    std::vector<LootEvent> validEvents;

    while (!_eventQueue.empty())
    {
        LootEvent event = _eventQueue.top();
        _eventQueue.pop();

        if (!event.IsExpired())
            validEvents.push_back(event);
        else
            cleanedCount++;
    }

    for (LootEvent const& event : validEvents)
        _eventQueue.push(event);

    return cleanedCount;
}

void LootEventBus::UpdateMetrics(std::chrono::microseconds processingTime)
{
    uint64_t currentAvg = _stats.averageProcessingTimeUs.load();
    uint64_t newTime = processingTime.count();
    uint64_t newAvg = (currentAvg * 9 + newTime) / 10;
    _stats.averageProcessingTimeUs.store(newAvg);
}

void LootEventBus::LogEvent(LootEvent const& event, std::string const& action) const
{
    TC_LOG_TRACE("module.playerbot.loot", "LootEventBus: {} event - {}", action, event.ToString());
}

void LootEventBus::DumpSubscribers() const
{
    std::lock_guard lock(_subscriberMutex);
    TC_LOG_INFO("module.playerbot.loot", "=== LootEventBus Subscribers: {} global ===", _globalSubscribers.size());
}

void LootEventBus::DumpEventQueue() const
{
    std::lock_guard lock(_queueMutex);
    TC_LOG_INFO("module.playerbot.loot", "=== LootEventBus Queue: {} events ===", _eventQueue.size());
}

std::vector<LootEvent> LootEventBus::GetQueueSnapshot() const
{
    std::lock_guard lock(_queueMutex);

    std::vector<LootEvent> snapshot;
    std::priority_queue<LootEvent> tempQueue = _eventQueue;

    while (!tempQueue.empty())
    {
        snapshot.push_back(tempQueue.top());
        tempQueue.pop();
    }

    return snapshot;
}

void LootEventBus::Statistics::Reset()
{
    totalEventsPublished.store(0);
    totalEventsProcessed.store(0);
    totalEventsDropped.store(0);
    totalDeliveries.store(0);
    averageProcessingTimeUs.store(0);
    peakQueueSize.store(0);
    startTime = std::chrono::steady_clock::now();
}

std::string LootEventBus::Statistics::ToString() const
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

