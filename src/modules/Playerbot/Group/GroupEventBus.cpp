/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "GroupEventBus.h"
#include "BotAI.h"
#include "Player.h"
#include "Group.h"
#include "Log.h"
#include "Timer.h"
#include <sstream>
#include <iomanip>

namespace Playerbot
{

// Static helper functions for event type names
namespace
{
    const char* GetEventTypeName(GroupEventType type)
    {
        switch (type)
        {
            case GroupEventType::MEMBER_JOINED: return "MEMBER_JOINED";
            case GroupEventType::MEMBER_LEFT: return "MEMBER_LEFT";
            case GroupEventType::LEADER_CHANGED: return "LEADER_CHANGED";
            case GroupEventType::GROUP_DISBANDED: return "GROUP_DISBANDED";
            case GroupEventType::RAID_CONVERTED: return "RAID_CONVERTED";
            case GroupEventType::READY_CHECK_STARTED: return "READY_CHECK_STARTED";
            case GroupEventType::READY_CHECK_RESPONSE: return "READY_CHECK_RESPONSE";
            case GroupEventType::READY_CHECK_COMPLETED: return "READY_CHECK_COMPLETED";
            case GroupEventType::TARGET_ICON_CHANGED: return "TARGET_ICON_CHANGED";
            case GroupEventType::RAID_MARKER_CHANGED: return "RAID_MARKER_CHANGED";
            case GroupEventType::ASSIST_TARGET_CHANGED: return "ASSIST_TARGET_CHANGED";
            case GroupEventType::LOOT_METHOD_CHANGED: return "LOOT_METHOD_CHANGED";
            case GroupEventType::LOOT_THRESHOLD_CHANGED: return "LOOT_THRESHOLD_CHANGED";
            case GroupEventType::MASTER_LOOTER_CHANGED: return "MASTER_LOOTER_CHANGED";
            case GroupEventType::SUBGROUP_CHANGED: return "SUBGROUP_CHANGED";
            case GroupEventType::ASSISTANT_CHANGED: return "ASSISTANT_CHANGED";
            case GroupEventType::MAIN_TANK_CHANGED: return "MAIN_TANK_CHANGED";
            case GroupEventType::MAIN_ASSIST_CHANGED: return "MAIN_ASSIST_CHANGED";
            case GroupEventType::DIFFICULTY_CHANGED: return "DIFFICULTY_CHANGED";
            case GroupEventType::INSTANCE_LOCK_MESSAGE: return "INSTANCE_LOCK_MESSAGE";
            case GroupEventType::PING_RECEIVED: return "PING_RECEIVED";
            case GroupEventType::COMMAND_RESULT: return "COMMAND_RESULT";
            case GroupEventType::STATE_UPDATE_REQUIRED: return "STATE_UPDATE_REQUIRED";
            case GroupEventType::ERROR_OCCURRED: return "ERROR_OCCURRED";
            default: return "UNKNOWN";
        }
    }

    const char* GetPriorityName(EventPriority priority)
    {
        switch (priority)
        {
            case EventPriority::CRITICAL: return "CRITICAL";
            case EventPriority::HIGH: return "HIGH";
            case EventPriority::MEDIUM: return "MEDIUM";
            case EventPriority::LOW: return "LOW";
            case EventPriority::BATCH: return "BATCH";
            default: return "UNKNOWN";
        }
    }

    EventPriority GetDefaultPriority(GroupEventType type)
    {
        switch (type)
        {
            case GroupEventType::GROUP_DISBANDED:
            case GroupEventType::ERROR_OCCURRED:
                return EventPriority::CRITICAL;

            case GroupEventType::MEMBER_JOINED:
            case GroupEventType::MEMBER_LEFT:
            case GroupEventType::LEADER_CHANGED:
            case GroupEventType::READY_CHECK_STARTED:
            case GroupEventType::TARGET_ICON_CHANGED:
            case GroupEventType::DIFFICULTY_CHANGED:
                return EventPriority::HIGH;

            case GroupEventType::RAID_CONVERTED:
            case GroupEventType::LOOT_METHOD_CHANGED:
            case GroupEventType::SUBGROUP_CHANGED:
            case GroupEventType::ASSISTANT_CHANGED:
            case GroupEventType::PING_RECEIVED:
                return EventPriority::MEDIUM;

            case GroupEventType::READY_CHECK_RESPONSE:
            case GroupEventType::RAID_MARKER_CHANGED:
            case GroupEventType::LOOT_THRESHOLD_CHANGED:
            case GroupEventType::MASTER_LOOTER_CHANGED:
            case GroupEventType::MAIN_TANK_CHANGED:
            case GroupEventType::MAIN_ASSIST_CHANGED:
            case GroupEventType::ASSIST_TARGET_CHANGED:
            case GroupEventType::INSTANCE_LOCK_MESSAGE:
            case GroupEventType::COMMAND_RESULT:
                return EventPriority::LOW;

            case GroupEventType::READY_CHECK_COMPLETED:
            case GroupEventType::STATE_UPDATE_REQUIRED:
                return EventPriority::BATCH;

            default:
                return EventPriority::MEDIUM;
        }
    }
}

// ============================================================================
// GroupEvent Helper Constructors
// ============================================================================

GroupEvent GroupEvent::MemberJoined(ObjectGuid groupGuid, ObjectGuid memberGuid)
{
    GroupEvent event;
    event.type = GroupEventType::MEMBER_JOINED;
    event.priority = EventPriority::HIGH;
    event.groupGuid = groupGuid;
    event.sourceGuid = memberGuid;
    event.targetGuid = memberGuid;
    event.data1 = 0;
    event.data2 = 0;
    event.data3 = 0;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(30000); // 30 second TTL
    return event;
}

GroupEvent GroupEvent::MemberLeft(ObjectGuid groupGuid, ObjectGuid memberGuid, uint32 removeMethod)
{
    GroupEvent event;
    event.type = GroupEventType::MEMBER_LEFT;
    event.priority = EventPriority::HIGH;
    event.groupGuid = groupGuid;
    event.sourceGuid = memberGuid;
    event.targetGuid = memberGuid;
    event.data1 = removeMethod; // RemoveMethod enum
    event.data2 = 0;
    event.data3 = 0;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(30000);
    return event;
}

GroupEvent GroupEvent::LeaderChanged(ObjectGuid groupGuid, ObjectGuid newLeaderGuid)
{
    GroupEvent event;
    event.type = GroupEventType::LEADER_CHANGED;
    event.priority = EventPriority::HIGH;
    event.groupGuid = groupGuid;
    event.sourceGuid = newLeaderGuid;
    event.targetGuid = newLeaderGuid;
    event.data1 = 0;
    event.data2 = 0;
    event.data3 = 0;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(30000);
    return event;
}

GroupEvent GroupEvent::GroupDisbanded(ObjectGuid groupGuid)
{
    GroupEvent event;
    event.type = GroupEventType::GROUP_DISBANDED;
    event.priority = EventPriority::CRITICAL;
    event.groupGuid = groupGuid;
    event.sourceGuid = ObjectGuid::Empty;
    event.targetGuid = ObjectGuid::Empty;
    event.data1 = 0;
    event.data2 = 0;
    event.data3 = 0;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(10000); // 10 second TTL (critical)
    return event;
}

GroupEvent GroupEvent::ReadyCheckStarted(ObjectGuid groupGuid, ObjectGuid initiatorGuid, uint32 durationMs)
{
    GroupEvent event;
    event.type = GroupEventType::READY_CHECK_STARTED;
    event.priority = EventPriority::HIGH;
    event.groupGuid = groupGuid;
    event.sourceGuid = initiatorGuid;
    event.targetGuid = ObjectGuid::Empty;
    event.data1 = durationMs;
    event.data2 = 0;
    event.data3 = 0;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(durationMs + 5000); // Duration + 5s grace
    return event;
}

GroupEvent GroupEvent::TargetIconChanged(ObjectGuid groupGuid, uint8 icon, ObjectGuid targetGuid)
{
    GroupEvent event;
    event.type = GroupEventType::TARGET_ICON_CHANGED;
    event.priority = EventPriority::HIGH;
    event.groupGuid = groupGuid;
    event.sourceGuid = ObjectGuid::Empty;
    event.targetGuid = targetGuid;
    event.data1 = icon;
    event.data2 = 0;
    event.data3 = 0;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(20000);
    return event;
}

GroupEvent GroupEvent::RaidMarkerChanged(ObjectGuid groupGuid, uint32 markerId, Position const& position)
{
    GroupEvent event;
    event.type = GroupEventType::RAID_MARKER_CHANGED;
    event.priority = EventPriority::LOW;
    event.groupGuid = groupGuid;
    event.sourceGuid = ObjectGuid::Empty;
    event.targetGuid = ObjectGuid::Empty;
    event.data1 = markerId;
    // Pack position into data2 and data3 (not perfect but works for our needs)
    event.data2 = *reinterpret_cast<uint32 const*>(&position.m_positionX);
    event.data3 = (static_cast<uint64_t>(*reinterpret_cast<uint32 const*>(&position.m_positionY)) << 32) |
                  *reinterpret_cast<uint32 const*>(&position.m_positionZ);
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(60000); // 60 second TTL
    return event;
}

GroupEvent GroupEvent::LootMethodChanged(ObjectGuid groupGuid, uint8 lootMethod)
{
    GroupEvent event;
    event.type = GroupEventType::LOOT_METHOD_CHANGED;
    event.priority = EventPriority::MEDIUM;
    event.groupGuid = groupGuid;
    event.sourceGuid = ObjectGuid::Empty;
    event.targetGuid = ObjectGuid::Empty;
    event.data1 = lootMethod;
    event.data2 = 0;
    event.data3 = 0;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(30000);
    return event;
}

GroupEvent GroupEvent::DifficultyChanged(ObjectGuid groupGuid, uint8 difficulty)
{
    GroupEvent event;
    event.type = GroupEventType::DIFFICULTY_CHANGED;
    event.priority = EventPriority::HIGH;
    event.groupGuid = groupGuid;
    event.sourceGuid = ObjectGuid::Empty;
    event.targetGuid = ObjectGuid::Empty;
    event.data1 = difficulty;
    event.data2 = 0;
    event.data3 = 0;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::milliseconds(20000);
    return event;
}

bool GroupEvent::IsValid() const
{
    // Group GUID must be valid (unless it's an error event)
    if (type != GroupEventType::ERROR_OCCURRED && groupGuid.IsEmpty())
        return false;

    // Event type must be within valid range
    if (type >= GroupEventType::MAX_EVENT_TYPE)
        return false;

    // Timestamp must be valid
    if (timestamp.time_since_epoch().count() == 0)
        return false;

    return true;
}

bool GroupEvent::IsExpired() const
{
    return std::chrono::steady_clock::now() >= expiryTime;
}

std::string GroupEvent::ToString() const
{
    std::ostringstream oss;
    oss << "[GroupEvent] "
        << "Type: " << GetEventTypeName(type)
        << ", Priority: " << GetPriorityName(priority)
        << ", Group: " << groupGuid.ToString()
        << ", Source: " << sourceGuid.ToString()
        << ", Target: " << targetGuid.ToString()
        << ", Data: " << data1 << "/" << data2 << "/" << data3;
    return oss.str();
}

// ============================================================================
// GroupEventBus Implementation
// ============================================================================

GroupEventBus::GroupEventBus()
{
    _stats.startTime = std::chrono::steady_clock::now();
    TC_LOG_INFO("module.playerbot.group", "GroupEventBus initialized");
}

GroupEventBus::~GroupEventBus()
{
    TC_LOG_INFO("module.playerbot.group", "GroupEventBus shutting down - Stats: {}", _stats.ToString());
}

GroupEventBus* GroupEventBus::instance()
{
    static GroupEventBus instance;
    return &instance;
}

bool GroupEventBus::PublishEvent(GroupEvent const& event)
{
    // Validate event
    if (!ValidateEvent(event))
    {
        TC_LOG_ERROR("module.playerbot.group", "GroupEventBus: Invalid event rejected: {}", event.ToString());
        _stats.totalEventsDropped++;
        return false;
    }

    // Check queue size limit
    {
        std::lock_guard lock(_queueMutex);
        if (_eventQueue.size() >= _maxQueueSize)
        {
            TC_LOG_WARN("module.playerbot.group", "GroupEventBus: Event queue full ({} events), dropping event: {}",
                _eventQueue.size(), event.ToString());
            _stats.totalEventsDropped++;
            return false;
        }

        // Add to queue
        _eventQueue.push(event);

        // Update peak queue size
        uint32 currentSize = static_cast<uint32>(_eventQueue.size());
        uint32 expectedPeak = _stats.peakQueueSize.load();
        while (currentSize > expectedPeak &&
               !_stats.peakQueueSize.compare_exchange_weak(expectedPeak, currentSize))
        {
            // CAS loop
        }
    }

    _stats.totalEventsPublished++;

    LogEvent(event, "Published");

    return true;
}

bool GroupEventBus::Subscribe(BotAI* subscriber, std::vector<GroupEventType> const& types)
{

    std::lock_guard lock(_subscriberMutex);

    for (GroupEventType type : types)
    {
        auto& subscriberList = _subscribers[type];

        // Check if already subscribed
        if (std::find(subscriberList.begin(), subscriberList.end(), subscriber) != subscriberList.end())
        {
            TC_LOG_WARN("module.playerbot.group", "GroupEventBus: Subscriber already registered for event type {}",
                GetEventTypeName(type));
            continue;
        }

        // Sanity check
        if (subscriberList.size() >= MAX_SUBSCRIBERS_PER_EVENT)
        {
            TC_LOG_ERROR("module.playerbot.group", "GroupEventBus: Too many subscribers for event type {} (max {})",
                GetEventTypeName(type), MAX_SUBSCRIBERS_PER_EVENT);
            return false;
        }

        subscriberList.push_back(subscriber);
    }

    TC_LOG_DEBUG("module.playerbot.group", "GroupEventBus: Subscriber {} registered for {} event types",
        static_cast<void*>(subscriber), types.size());

    return true;
}

bool GroupEventBus::SubscribeAll(BotAI* subscriber)
{

    std::lock_guard lock(_subscriberMutex);

    // Check if already subscribed
    if (std::find(_globalSubscribers.begin(), _globalSubscribers.end(), subscriber) != _globalSubscribers.end())
    {
        TC_LOG_WARN("module.playerbot.group", "GroupEventBus: Subscriber already registered for all events");
        return false;
    }

    _globalSubscribers.push_back(subscriber);

    TC_LOG_DEBUG("module.playerbot.group", "GroupEventBus: Subscriber {} registered for all events",
        static_cast<void*>(subscriber));

    return true;
}

void GroupEventBus::Unsubscribe(BotAI* subscriber)
{
    if (!subscriber)
        return;

    std::lock_guard lock(_subscriberMutex);

    // Remove from all specific event subscriptions
    for (auto& [type, subscriberList] : _subscribers)
    {
        subscriberList.erase(
            std::remove(subscriberList.begin(), subscriberList.end(), subscriber),
            subscriberList.end()
        );
    }

    // Remove from global subscribers
    _globalSubscribers.erase(
        std::remove(_globalSubscribers.begin(), _globalSubscribers.end(), subscriber),
        _globalSubscribers.end()
    );

    TC_LOG_DEBUG("module.playerbot.group", "GroupEventBus: Subscriber {} unsubscribed from all events",
        static_cast<void*>(subscriber));
}

uint32 GroupEventBus::ProcessEvents(uint32 diff, uint32 maxEvents)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    // Update timers
    _cleanupTimer += diff;
    _metricsUpdateTimer += diff;

    // Periodic cleanup
    if (_cleanupTimer >= CLEANUP_INTERVAL)
    {
        CleanupExpiredEvents();
        _cleanupTimer = 0;
    }

    uint32 processedCount = 0;
    std::vector<GroupEvent> eventsToProcess;

    // Extract events from queue (hold lock briefly)
    {
        std::lock_guard lock(_queueMutex);

        while (!_eventQueue.empty() && (maxEvents == 0 || processedCount < maxEvents))
        {
            GroupEvent event = _eventQueue.top();
            _eventQueue.pop();

            // Check if expired
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

    // Process events without holding queue lock
    for (GroupEvent const& event : eventsToProcess)
    {
        // Get subscribers for this event type
        std::vector<BotAI*> subscribers;
        std::vector<BotAI*> globalSubs;

        {
            std::lock_guard lock(_subscriberMutex);

            // Add specific subscribers
            auto it = _subscribers.find(event.type);
            if (it != _subscribers.end())
                subscribers = it->second;

            // Add global subscribers
            globalSubs = _globalSubscribers;
        }

        // Deliver to specific subscribers
        for (BotAI* subscriber : subscribers)
        {
            if (DeliverEvent(subscriber, event))
                _stats.totalDeliveries++;
        }

        // Deliver to global subscribers
        for (BotAI* subscriber : globalSubs)
        {
            if (DeliverEvent(subscriber, event))
                _stats.totalDeliveries++;
        }

        _stats.totalEventsProcessed++;
        LogEvent(event, "Processed");
    }

    // Update metrics
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    if (processedCount > 0)
        UpdateMetrics(duration);

    return processedCount;
}

uint32 GroupEventBus::ProcessGroupEvents(ObjectGuid groupGuid, uint32 diff)
{
    // Process only events for a specific group
    // This is an optimization for group-specific updates

    uint32 processedCount = 0;
    std::vector<GroupEvent> eventsToProcess;
    std::vector<GroupEvent> otherEvents;

    // Extract group events from queue
    {
        std::lock_guard lock(_queueMutex);

        while (!_eventQueue.empty())
        {
            GroupEvent event = _eventQueue.top();
            _eventQueue.pop();

            if (event.groupGuid == groupGuid)
            {
                if (!event.IsExpired())
                    eventsToProcess.push_back(event);
                else
                    _stats.totalEventsDropped++;
            }
            else
            {
                otherEvents.push_back(event);
            }
        }

        // Re-add other events to queue
        for (GroupEvent const& event : otherEvents)
            _eventQueue.push(event);
    }

    // Process group events (same logic as ProcessEvents)
    for (GroupEvent const& event : eventsToProcess)
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
        processedCount++;
    }

    return processedCount;
}

void GroupEventBus::ClearGroupEvents(ObjectGuid groupGuid)
{
    std::lock_guard lock(_queueMutex);

    std::vector<GroupEvent> remainingEvents;

    while (!_eventQueue.empty())
    {
        GroupEvent event = _eventQueue.top();
        _eventQueue.pop();

        if (event.groupGuid != groupGuid)
            remainingEvents.push_back(event);
        else
            _stats.totalEventsDropped++;
    }

    // Re-add remaining events
    for (GroupEvent const& event : remainingEvents)
        _eventQueue.push(event);

    TC_LOG_DEBUG("module.playerbot.group", "GroupEventBus: Cleared all events for group {}",
        groupGuid.ToString());
}

uint32 GroupEventBus::GetPendingEventCount() const
{
    std::lock_guard lock(_queueMutex);
    return static_cast<uint32>(_eventQueue.size());
}

uint32 GroupEventBus::GetSubscriberCount() const
{
    std::lock_guard lock(_subscriberMutex);

    uint32 count = static_cast<uint32>(_globalSubscribers.size());

    for (auto const& [type, subscriberList] : _subscribers)
        count += static_cast<uint32>(subscriberList.size());

    return count;
}

bool GroupEventBus::DeliverEvent(BotAI* subscriber, GroupEvent const& event)
{
    if (!subscriber)
        return false;

    try
    {
        // Phase 4: Call virtual event handler on BotAI
        subscriber->OnGroupEvent(event);

        TC_LOG_TRACE("module.playerbot.group", "GroupEventBus: Delivered event {} to subscriber {}",
            event.ToString(), static_cast<void*>(subscriber));
        return true;
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot.group", "GroupEventBus: Exception delivering event: {}", e.what());
        return false;
    }
}

bool GroupEventBus::ValidateEvent(GroupEvent const& event) const
{
    if (!event.IsValid())
        return false;

    if (event.IsExpired())
        return false;

    return true;
}

uint32 GroupEventBus::CleanupExpiredEvents()
{
    std::lock_guard lock(_queueMutex);

    uint32 cleanedCount = 0;
    std::vector<GroupEvent> validEvents;

    while (!_eventQueue.empty())
    {
        GroupEvent event = _eventQueue.top();
        _eventQueue.pop();

        if (!event.IsExpired())
            validEvents.push_back(event);
        else
        {
            cleanedCount++;
            _stats.totalEventsDropped++;
        }
    }

    // Re-add valid events
    for (GroupEvent const& event : validEvents)
        _eventQueue.push(event);

    if (cleanedCount > 0)
    {
        TC_LOG_DEBUG("module.playerbot.group", "GroupEventBus: Cleaned up {} expired events",
            cleanedCount);
    }

    return cleanedCount;
}

void GroupEventBus::UpdateMetrics(std::chrono::microseconds processingTime)
{
    // Update moving average of processing time
    uint64_t currentAvg = _stats.averageProcessingTimeUs.load();
    uint64_t newTime = processingTime.count();

    // Simple moving average with weight 0.9 for old, 0.1 for new
    uint64_t newAvg = (currentAvg * 9 + newTime) / 10;
    _stats.averageProcessingTimeUs.store(newAvg);
}

void GroupEventBus::LogEvent(GroupEvent const& event, std::string const& action) const
{
    TC_LOG_TRACE("module.playerbot.group", "GroupEventBus: {} event - {}", action, event.ToString());
}

void GroupEventBus::DumpSubscribers() const
{
    std::lock_guard lock(_subscriberMutex);

    TC_LOG_INFO("module.playerbot.group", "=== GroupEventBus Subscribers Dump ===");
    TC_LOG_INFO("module.playerbot.group", "Global subscribers: {}", _globalSubscribers.size());

    for (auto const& [type, subscriberList] : _subscribers)
    {
        TC_LOG_INFO("module.playerbot.group", "Event {}: {} subscribers",
            GetEventTypeName(type), subscriberList.size());
    }
}

void GroupEventBus::DumpEventQueue() const
{
    std::lock_guard lock(_queueMutex);

    TC_LOG_INFO("module.playerbot.group", "=== GroupEventBus Queue Dump ===");
    TC_LOG_INFO("module.playerbot.group", "Queue size: {}", _eventQueue.size());

    // Note: Can't iterate priority_queue without modifying it
    // Would need to copy to temp queue for full dump
}

std::vector<GroupEvent> GroupEventBus::GetQueueSnapshot() const
{
    std::lock_guard lock(_queueMutex);

    std::vector<GroupEvent> snapshot;
    std::priority_queue<GroupEvent> tempQueue = _eventQueue;

    while (!tempQueue.empty())
    {
        snapshot.push_back(tempQueue.top());
        tempQueue.pop();
    }

    return snapshot;
}

void GroupEventBus::Statistics::Reset()
{
    totalEventsPublished.store(0);
    totalEventsProcessed.store(0);
    totalEventsDropped.store(0);
    totalDeliveries.store(0);
    averageProcessingTimeUs.store(0);
    peakQueueSize.store(0);
    startTime = std::chrono::steady_clock::now();
}

std::string GroupEventBus::Statistics::ToString() const
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
