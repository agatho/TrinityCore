/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "CooldownEventBus.h"
#include "BotAI.h"
#include "Player.h"
#include "Log.h"
#include "Timer.h"
#include <sstream>

namespace Playerbot
{

// ============================================================================
// CooldownEvent Helper Constructors
// ============================================================================

CooldownEvent CooldownEvent::SpellCooldownStart(ObjectGuid caster, uint32 spellId, uint32 cooldownMs)
{
    CooldownEvent event;
    event.type = CooldownEventType::SPELL_COOLDOWN_START;
    event.priority = CooldownEventPriority::MEDIUM;
    event.casterGuid = caster;
    event.spellId = spellId;
    event.itemId = 0;
    event.category = 0;
    event.cooldownMs = cooldownMs;
    event.modRateMs = 0;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(cooldownMs + 5000);
    return event;
}

CooldownEvent CooldownEvent::SpellCooldownClear(ObjectGuid caster, uint32 spellId)
{
    CooldownEvent event;
    event.type = CooldownEventType::SPELL_COOLDOWN_CLEAR;
    event.priority = CooldownEventPriority::HIGH;
    event.casterGuid = caster;
    event.spellId = spellId;
    event.itemId = 0;
    event.category = 0;
    event.cooldownMs = 0;
    event.modRateMs = 0;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(5000);
    return event;
}

CooldownEvent CooldownEvent::ItemCooldownStart(ObjectGuid caster, uint32 itemId, uint32 cooldownMs)
{
    CooldownEvent event;
    event.type = CooldownEventType::ITEM_COOLDOWN_START;
    event.priority = CooldownEventPriority::MEDIUM;
    event.casterGuid = caster;
    event.spellId = 0;
    event.itemId = itemId;
    event.category = 0;
    event.cooldownMs = cooldownMs;
    event.modRateMs = 0;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(cooldownMs + 5000);
    return event;
}

bool CooldownEvent::IsValid() const
{
    if (type >= CooldownEventType::MAX_COOLDOWN_EVENT)
        return false;
    if (timestamp.time_since_epoch().count() == 0)
        return false;
    if (casterGuid.IsEmpty())
        return false;
    return true;
}

bool CooldownEvent::IsExpired() const
{
    return std::chrono::steady_clock::now() >= expiryTime;
}

std::string CooldownEvent::ToString() const
{
    std::ostringstream oss;
    oss << "[CooldownEvent] Type: " << static_cast<uint32>(type)
        << ", Caster: " << casterGuid.ToString()
        << ", Spell: " << spellId
        << ", Item: " << itemId
        << ", Duration: " << cooldownMs << "ms";
    return oss.str();
}

// ============================================================================
// CooldownEventBus Implementation
// ============================================================================

CooldownEventBus::CooldownEventBus()
{
    _stats.startTime = std::chrono::steady_clock::now();
    TC_LOG_INFO("module.playerbot.cooldown", "CooldownEventBus initialized");
}

CooldownEventBus::~CooldownEventBus()
{
    TC_LOG_INFO("module.playerbot.cooldown", "CooldownEventBus shutting down - Stats: {}", _stats.ToString());
}

CooldownEventBus* CooldownEventBus::instance()
{
    static CooldownEventBus instance;
    return &instance;
}

bool CooldownEventBus::PublishEvent(CooldownEvent const& event)
{
    if (!ValidateEvent(event))
    {
        TC_LOG_ERROR("module.playerbot.cooldown", "CooldownEventBus: Invalid event rejected: {}", event.ToString());
        _stats.totalEventsDropped++;
        return false;
    }

    {
        std::lock_guard lock(_queueMutex);
        if (_eventQueue.size() >= _maxQueueSize)
        {
            TC_LOG_WARN("module.playerbot.cooldown", "CooldownEventBus: Event queue full ({} events), dropping event: {}",
                _eventQueue.size(), event.ToString());
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

bool CooldownEventBus::Subscribe(BotAI* subscriber, std::vector<CooldownEventType> const& types)
{

    std::lock_guard lock(_subscriberMutex);

    for (CooldownEventType type : types)
    {
        auto& subscriberList = _subscribers[type];

        if (std::find(subscriberList.begin(), subscriberList.end(), subscriber) != subscriberList.end())
        {
            TC_LOG_WARN("module.playerbot.cooldown", "CooldownEventBus: Subscriber already registered for event type {}",
                static_cast<uint32>(type));
            continue;
        }

        if (subscriberList.size() >= MAX_SUBSCRIBERS_PER_EVENT)
        {
            TC_LOG_ERROR("module.playerbot.cooldown", "CooldownEventBus: Too many subscribers for event type {} (max {})",
                static_cast<uint32>(type), MAX_SUBSCRIBERS_PER_EVENT);
            return false;
        }

        subscriberList.push_back(subscriber);
    }

    TC_LOG_DEBUG("module.playerbot.cooldown", "CooldownEventBus: Subscriber {} registered for {} event types",
        static_cast<void*>(subscriber), types.size());

    return true;
}

bool CooldownEventBus::SubscribeAll(BotAI* subscriber)
{

    std::lock_guard lock(_subscriberMutex);

    if (std::find(_globalSubscribers.begin(), _globalSubscribers.end(), subscriber) != _globalSubscribers.end())
    {
        TC_LOG_WARN("module.playerbot.cooldown", "CooldownEventBus: Subscriber already registered for all events");
        return false;
    }

    _globalSubscribers.push_back(subscriber);

    TC_LOG_DEBUG("module.playerbot.cooldown", "CooldownEventBus: Subscriber {} registered for all events",
        static_cast<void*>(subscriber));

    return true;
}

void CooldownEventBus::Unsubscribe(BotAI* subscriber)
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

    TC_LOG_DEBUG("module.playerbot.cooldown", "CooldownEventBus: Subscriber {} unsubscribed from all events",
        static_cast<void*>(subscriber));
}

uint32 CooldownEventBus::ProcessEvents(uint32 diff, uint32 maxEvents)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    _cleanupTimer += diff;
    if (_cleanupTimer >= CLEANUP_INTERVAL)
    {
        CleanupExpiredEvents();
        _cleanupTimer = 0;
    }

    uint32 processedCount = 0;
    std::vector<CooldownEvent> eventsToProcess;

    {
        std::lock_guard lock(_queueMutex);

        while (!_eventQueue.empty() && (maxEvents == 0 || processedCount < maxEvents))
        {
            CooldownEvent event = _eventQueue.top();
            _eventQueue.pop();

            if (event.IsExpired())
            {
                LogEvent(event, "Expired");
                _stats.totalEventsDropped++;
                continue;
            }

            eventsToProcess.push_back(event);
            processedCount++;
        }
    }

    for (CooldownEvent const& event : eventsToProcess)
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
        LogEvent(event, "Processed");
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    if (processedCount > 0)
        UpdateMetrics(duration);

    return processedCount;
}

uint32 CooldownEventBus::ProcessUnitEvents(ObjectGuid unitGuid, uint32 diff)
{
    return ProcessEvents(diff, 0);
}

void CooldownEventBus::ClearUnitEvents(ObjectGuid unitGuid)
{
    std::lock_guard lock(_queueMutex);

    std::vector<CooldownEvent> remainingEvents;

    while (!_eventQueue.empty())
    {
        CooldownEvent event = _eventQueue.top();
        _eventQueue.pop();

        if (event.casterGuid != unitGuid)
            remainingEvents.push_back(event);
        else
            _stats.totalEventsDropped++;
    }

    for (CooldownEvent const& event : remainingEvents)
        _eventQueue.push(event);

    TC_LOG_DEBUG("module.playerbot.cooldown", "CooldownEventBus: Cleared all events for unit {}",
        unitGuid.ToString());
}

uint32 CooldownEventBus::GetPendingEventCount() const
{
    std::lock_guard lock(_queueMutex);
    return static_cast<uint32>(_eventQueue.size());
}

uint32 CooldownEventBus::GetSubscriberCount() const
{
    std::lock_guard lock(_subscriberMutex);

    uint32 count = static_cast<uint32>(_globalSubscribers.size());

    for (auto const& [type, subscriberList] : _subscribers)
        count += static_cast<uint32>(subscriberList.size());

    return count;
}

bool CooldownEventBus::DeliverEvent(BotAI* subscriber, CooldownEvent const& event)
{
    if (!subscriber)
        return false;

    try
    {
        // Phase 4: Call virtual event handler on BotAI
        subscriber->OnCooldownEvent(event);

        TC_LOG_TRACE("module.playerbot.cooldown", "CooldownEventBus: Delivered event {} to subscriber {}",
            event.ToString(), static_cast<void*>(subscriber));
        return true;
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot.cooldown", "CooldownEventBus: Exception delivering event: {}", e.what());
        return false;
    }
}

bool CooldownEventBus::ValidateEvent(CooldownEvent const& event) const
{
    return event.IsValid() && !event.IsExpired();
}

uint32 CooldownEventBus::CleanupExpiredEvents()
{
    std::lock_guard lock(_queueMutex);

    uint32 cleanedCount = 0;
    std::vector<CooldownEvent> validEvents;

    while (!_eventQueue.empty())
    {
        CooldownEvent event = _eventQueue.top();
        _eventQueue.pop();

        if (!event.IsExpired())
            validEvents.push_back(event);
        else
        {
            cleanedCount++;
            _stats.totalEventsDropped++;
        }
    }

    for (CooldownEvent const& event : validEvents)
        _eventQueue.push(event);

    if (cleanedCount > 0)
    {
        TC_LOG_DEBUG("module.playerbot.cooldown", "CooldownEventBus: Cleaned up {} expired events",
            cleanedCount);
    }

    return cleanedCount;
}

void CooldownEventBus::UpdateMetrics(std::chrono::microseconds processingTime)
{
    uint64_t currentAvg = _stats.averageProcessingTimeUs.load();
    uint64_t newTime = processingTime.count();
    uint64_t newAvg = (currentAvg * 9 + newTime) / 10;
    _stats.averageProcessingTimeUs.store(newAvg);
}

void CooldownEventBus::LogEvent(CooldownEvent const& event, std::string const& action) const
{
    TC_LOG_TRACE("module.playerbot.cooldown", "CooldownEventBus: {} event - {}", action, event.ToString());
}

void CooldownEventBus::DumpSubscribers() const
{
    std::lock_guard lock(_subscriberMutex);

    TC_LOG_INFO("module.playerbot.cooldown", "=== CooldownEventBus Subscribers Dump ===");
    TC_LOG_INFO("module.playerbot.cooldown", "Global subscribers: {}", _globalSubscribers.size());

    for (auto const& [type, subscriberList] : _subscribers)
    {
        TC_LOG_INFO("module.playerbot.cooldown", "Event {}: {} subscribers",
            static_cast<uint32>(type), subscriberList.size());
    }
}

void CooldownEventBus::DumpEventQueue() const
{
    std::lock_guard lock(_queueMutex);

    TC_LOG_INFO("module.playerbot.cooldown", "=== CooldownEventBus Queue Dump ===");
    TC_LOG_INFO("module.playerbot.cooldown", "Queue size: {}", _eventQueue.size());
}

std::vector<CooldownEvent> CooldownEventBus::GetQueueSnapshot() const
{
    std::lock_guard lock(_queueMutex);

    std::vector<CooldownEvent> snapshot;
    std::priority_queue<CooldownEvent> tempQueue = _eventQueue;

    while (!tempQueue.empty())
    {
        snapshot.push_back(tempQueue.top());
        tempQueue.pop();
    }

    return snapshot;
}

void CooldownEventBus::Statistics::Reset()
{
    totalEventsPublished.store(0);
    totalEventsProcessed.store(0);
    totalEventsDropped.store(0);
    totalDeliveries.store(0);
    averageProcessingTimeUs.store(0);
    peakQueueSize.store(0);
    startTime = std::chrono::steady_clock::now();
}

std::string CooldownEventBus::Statistics::ToString() const
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
