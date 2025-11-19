/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "QuestEventBus.h"
#include "BotAI.h"
#include "Player.h"
#include "Log.h"
#include "Timer.h"
#include <sstream>

namespace Playerbot
{

// ============================================================================
// QuestEvent Helper Constructors
// ============================================================================

QuestEvent QuestEvent::QuestAccepted(ObjectGuid player, uint32 questId)
{
    QuestEvent event;
    event.type = QuestEventType::QUEST_CONFIRM_ACCEPT;
    event.priority = QuestEventPriority::HIGH;
    event.playerGuid = player;
    event.questId = questId;
    event.objectiveId = 0;
    event.objectiveCount = 0;
    event.state = QuestState::INCOMPLETE;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(30000);
    return event;
}

QuestEvent QuestEvent::QuestCompleted(ObjectGuid player, uint32 questId)
{
    QuestEvent event;
    event.type = QuestEventType::QUEST_COMPLETED;
    event.priority = QuestEventPriority::HIGH;
    event.playerGuid = player;
    event.questId = questId;
    event.objectiveId = 0;
    event.objectiveCount = 0;
    event.state = QuestState::COMPLETE;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(10000);
    return event;
}

QuestEvent QuestEvent::ObjectiveProgress(ObjectGuid player, uint32 questId, uint32 objId, int32 count)
{
    QuestEvent event;
    event.type = QuestEventType::QUEST_OBJECTIVE_COMPLETE;
    event.priority = QuestEventPriority::MEDIUM;
    event.playerGuid = player;
    event.questId = questId;
    event.objectiveId = objId;
    event.objectiveCount = count;
    event.state = QuestState::INCOMPLETE;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(15000);
    return event;
}

bool QuestEvent::IsValid() const
{
    if (type >= QuestEventType::MAX_QUEST_EVENT)
        return false;
    if (timestamp.time_since_epoch().count() == 0)
        return false;
    if (playerGuid.IsEmpty())
        return false;
    return true;
}

bool QuestEvent::IsExpired() const
{
    return std::chrono::steady_clock::now() >= expiryTime;
}

std::string QuestEvent::ToString() const
{
    std::ostringstream oss;
    oss << "[QuestEvent] Type: " << static_cast<uint32>(type)
        << ", Player: " << playerGuid.ToString()
        << ", Quest: " << questId
        << ", Objective: " << objectiveId
        << " (" << objectiveCount << ")";
    return oss.str();
}

// ============================================================================
// QuestEventBus Implementation
// ============================================================================

QuestEventBus::QuestEventBus()
{
    _stats.startTime = std::chrono::steady_clock::now();
    TC_LOG_INFO("module.playerbot.quest", "QuestEventBus initialized");
}

QuestEventBus::~QuestEventBus()
{
    TC_LOG_INFO("module.playerbot.quest", "QuestEventBus shutting down - Stats: {}", _stats.ToString());
}

QuestEventBus* QuestEventBus::instance()
{
    static QuestEventBus instance;
    return &instance;
}

bool QuestEventBus::PublishEvent(QuestEvent const& event)
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

bool QuestEventBus::Subscribe(BotAI* subscriber, std::vector<QuestEventType> const& types)
{
    if (!subscriber)
        return false;

    std::lock_guard lock(_subscriberMutex);

    for (QuestEventType type : types)
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

bool QuestEventBus::SubscribeAll(BotAI* subscriber)
{
    if (!subscriber)
        return false;

    std::lock_guard lock(_subscriberMutex);

    if (std::find(_globalSubscribers.begin(), _globalSubscribers.end(), subscriber) != _globalSubscribers.end())
        return false;

    _globalSubscribers.push_back(subscriber);
    return true;
}

void QuestEventBus::Unsubscribe(BotAI* subscriber)
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

uint32 QuestEventBus::ProcessEvents(uint32 diff, uint32 maxEvents)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    _cleanupTimer += diff;
    if (_cleanupTimer >= CLEANUP_INTERVAL)
    {
        CleanupExpiredEvents();
        _cleanupTimer = 0;
    }

    uint32 processedCount = 0;
    std::vector<QuestEvent> eventsToProcess;

    {
        std::lock_guard lock(_queueMutex);

        while (!_eventQueue.empty() && (maxEvents == 0 || processedCount < maxEvents))
        {
            QuestEvent event = _eventQueue.top();
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

    for (QuestEvent const& event : eventsToProcess)
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

uint32 QuestEventBus::ProcessUnitEvents(ObjectGuid unitGuid, uint32 diff)
{
    return ProcessEvents(diff, 0);
}

void QuestEventBus::ClearUnitEvents(ObjectGuid unitGuid)
{
    std::lock_guard lock(_queueMutex);

    std::vector<QuestEvent> remainingEvents;

    while (!_eventQueue.empty())
    {
        QuestEvent event = _eventQueue.top();
        _eventQueue.pop();

        if (event.playerGuid != unitGuid)
            remainingEvents.push_back(event);
        else
            _stats.totalEventsDropped++;
    }

    for (QuestEvent const& event : remainingEvents)
        _eventQueue.push(event);
}

uint32 QuestEventBus::GetPendingEventCount() const
{
    std::lock_guard lock(_queueMutex);
    return static_cast<uint32>(_eventQueue.size());
}

uint32 QuestEventBus::GetSubscriberCount() const
{
    std::lock_guard lock(_subscriberMutex);

    uint32 count = static_cast<uint32>(_globalSubscribers.size());

    for (auto const& [type, subscriberList] : _subscribers)
        count += static_cast<uint32>(subscriberList.size());

    return count;
}

bool QuestEventBus::DeliverEvent(BotAI* subscriber, QuestEvent const& event)
{
    if (!subscriber)
        return false;

    try
    {
        // Phase 4: Call virtual event handler on BotAI
        subscriber->OnQuestEvent(event);
        TC_LOG_TRACE("module.playerbot.quest", "QuestEventBus: Delivered event to subscriber");
        return true;
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot.quest", "QuestEventBus: Exception delivering event: {}", e.what());
        return false;
    }
}

bool QuestEventBus::ValidateEvent(QuestEvent const& event) const
{
    return event.IsValid() && !event.IsExpired();
}

uint32 QuestEventBus::CleanupExpiredEvents()
{
    std::lock_guard lock(_queueMutex);

    uint32 cleanedCount = 0;
    std::vector<QuestEvent> validEvents;

    while (!_eventQueue.empty())
    {
        QuestEvent event = _eventQueue.top();
        _eventQueue.pop();

        if (!event.IsExpired())
            validEvents.push_back(event);
        else
            cleanedCount++;
    }

    for (QuestEvent const& event : validEvents)
        _eventQueue.push(event);

    return cleanedCount;
}

void QuestEventBus::UpdateMetrics(std::chrono::microseconds processingTime)
{
    uint64_t currentAvg = _stats.averageProcessingTimeUs.load();
    uint64_t newTime = processingTime.count();
    uint64_t newAvg = (currentAvg * 9 + newTime) / 10;
    _stats.averageProcessingTimeUs.store(newAvg);
}

void QuestEventBus::LogEvent(QuestEvent const& event, std::string const& action) const
{
    TC_LOG_TRACE("module.playerbot.quest", "QuestEventBus: {} event - {}", action, event.ToString());
}

void QuestEventBus::DumpSubscribers() const
{
    std::lock_guard lock(_subscriberMutex);
    TC_LOG_INFO("module.playerbot.quest", "=== QuestEventBus Subscribers: {} global ===", _globalSubscribers.size());
}

void QuestEventBus::DumpEventQueue() const
{
    std::lock_guard lock(_queueMutex);
    TC_LOG_INFO("module.playerbot.quest", "=== QuestEventBus Queue: {} events ===", _eventQueue.size());
}

std::vector<QuestEvent> QuestEventBus::GetQueueSnapshot() const
{
    std::lock_guard lock(_queueMutex);

    std::vector<QuestEvent> snapshot;
    std::priority_queue<QuestEvent> tempQueue = _eventQueue;

    while (!tempQueue.empty())
    {
        snapshot.push_back(tempQueue.top());
        tempQueue.pop();
    }

    return snapshot;
}

void QuestEventBus::Statistics::Reset()
{
    totalEventsPublished.store(0);
    totalEventsProcessed.store(0);
    totalEventsDropped.store(0);
    totalDeliveries.store(0);
    averageProcessingTimeUs.store(0);
    peakQueueSize.store(0);
    startTime = std::chrono::steady_clock::now();
}

std::string QuestEventBus::Statistics::ToString() const
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

