/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "InstanceEventBus.h"
#include "BotAI.h"
#include "Log.h"
#include <sstream>
#include <algorithm>

namespace Playerbot
{

// Factory methods
InstanceEvent InstanceEvent::InstanceReset(ObjectGuid playerGuid, uint32 mapId)
{
    InstanceEvent event;
    event.type = InstanceEventType::INSTANCE_RESET;
    event.playerGuid = playerGuid;
    event.mapId = mapId;
    event.instanceId = 0;
    event.encounterId = 0;
    event.encounterFrame = 0;
    event.errorCode = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

InstanceEvent InstanceEvent::InstanceResetFailed(ObjectGuid playerGuid, uint32 mapId, uint32 errorCode)
{
    InstanceEvent event;
    event.type = InstanceEventType::INSTANCE_RESET_FAILED;
    event.playerGuid = playerGuid;
    event.mapId = mapId;
    event.errorCode = errorCode;
    event.instanceId = 0;
    event.encounterId = 0;
    event.encounterFrame = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

InstanceEvent InstanceEvent::EncounterFrameUpdate(ObjectGuid playerGuid, uint32 encounterId, uint32 frame)
{
    InstanceEvent event;
    event.type = InstanceEventType::ENCOUNTER_FRAME_UPDATE;
    event.playerGuid = playerGuid;
    event.encounterId = encounterId;
    event.encounterFrame = frame;
    event.mapId = 0;
    event.instanceId = 0;
    event.errorCode = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

InstanceEvent InstanceEvent::RaidInfoReceived(ObjectGuid playerGuid, uint32 mapId, uint32 instanceId, std::vector<uint32> bossStates)
{
    InstanceEvent event;
    event.type = InstanceEventType::RAID_INFO_RECEIVED;
    event.playerGuid = playerGuid;
    event.mapId = mapId;
    event.instanceId = instanceId;
    event.bossStates = std::move(bossStates);
    event.encounterId = 0;
    event.encounterFrame = 0;
    event.errorCode = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

InstanceEvent InstanceEvent::RaidGroupOnlyWarning(ObjectGuid playerGuid)
{
    InstanceEvent event;
    event.type = InstanceEventType::RAID_GROUP_ONLY_WARNING;
    event.playerGuid = playerGuid;
    event.mapId = 0;
    event.instanceId = 0;
    event.encounterId = 0;
    event.encounterFrame = 0;
    event.errorCode = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

InstanceEvent InstanceEvent::InstanceSaveCreated(ObjectGuid playerGuid, uint32 mapId, uint32 instanceId)
{
    InstanceEvent event;
    event.type = InstanceEventType::INSTANCE_SAVE_CREATED;
    event.playerGuid = playerGuid;
    event.mapId = mapId;
    event.instanceId = instanceId;
    event.encounterId = 0;
    event.encounterFrame = 0;
    event.errorCode = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

InstanceEvent InstanceEvent::InstanceMessageReceived(ObjectGuid playerGuid, uint32 mapId, std::string message)
{
    InstanceEvent event;
    event.type = InstanceEventType::INSTANCE_MESSAGE_RECEIVED;
    event.playerGuid = playerGuid;
    event.mapId = mapId;
    event.message = std::move(message);
    event.instanceId = 0;
    event.encounterId = 0;
    event.encounterFrame = 0;
    event.errorCode = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

bool InstanceEvent::IsValid() const
{
    if (type >= InstanceEventType::MAX_INSTANCE_EVENT)
        return false;

    switch (type)
    {
        case InstanceEventType::INSTANCE_RESET:
            return !playerGuid.IsEmpty() && mapId > 0;
        case InstanceEventType::INSTANCE_RESET_FAILED:
            return !playerGuid.IsEmpty() && mapId > 0;
        case InstanceEventType::ENCOUNTER_FRAME_UPDATE:
            return !playerGuid.IsEmpty();
        case InstanceEventType::RAID_INFO_RECEIVED:
            return !playerGuid.IsEmpty() && mapId > 0;
        case InstanceEventType::RAID_GROUP_ONLY_WARNING:
            return !playerGuid.IsEmpty();
        case InstanceEventType::INSTANCE_SAVE_CREATED:
            return !playerGuid.IsEmpty() && mapId > 0;
        case InstanceEventType::INSTANCE_MESSAGE_RECEIVED:
            return !playerGuid.IsEmpty();
        default:
            return false;
    }
}

std::string InstanceEvent::ToString() const
{
    std::ostringstream oss;
    oss << "InstanceEvent[";

    switch (type)
    {
        case InstanceEventType::INSTANCE_RESET:
            oss << "INSTANCE_RESET, map=" << mapId;
            break;
        case InstanceEventType::INSTANCE_RESET_FAILED:
            oss << "INSTANCE_RESET_FAILED, map=" << mapId << ", error=" << errorCode;
            break;
        case InstanceEventType::ENCOUNTER_FRAME_UPDATE:
            oss << "ENCOUNTER_FRAME, encounter=" << encounterId << ", frame=" << encounterFrame;
            break;
        case InstanceEventType::RAID_INFO_RECEIVED:
            oss << "RAID_INFO, map=" << mapId << ", instance=" << instanceId
                << ", bosses=" << bossStates.size();
            break;
        case InstanceEventType::RAID_GROUP_ONLY_WARNING:
            oss << "RAID_GROUP_ONLY";
            break;
        case InstanceEventType::INSTANCE_SAVE_CREATED:
            oss << "INSTANCE_SAVE, map=" << mapId << ", instance=" << instanceId;
            break;
        case InstanceEventType::INSTANCE_MESSAGE_RECEIVED:
            oss << "INSTANCE_MESSAGE, map=" << mapId << ", msg=" << message.substr(0, 50);
            break;
        default:
            oss << "UNKNOWN";
    }

    oss << "]";
    return oss.str();
}

// InstanceEventBus implementation
InstanceEventBus* InstanceEventBus::instance()
{
    static InstanceEventBus instance;
    return &instance;
}

bool InstanceEventBus::PublishEvent(InstanceEvent const& event)
{
    if (!event.IsValid())
    {
        TC_LOG_ERROR("playerbot.events", "InstanceEventBus: Invalid event rejected: {}", event.ToString());
        return false;
    }

    // Update statistics
    {
        std::lock_guard lock(_subscriberMutex);
        ++_eventCounts[event.type];
        ++_totalEventsPublished;
    }

    // Deliver to subscribers
    DeliverEvent(event);

    TC_LOG_TRACE("playerbot.events", "InstanceEventBus: Published event: {}", event.ToString());
    return true;
}

void InstanceEventBus::Subscribe(BotAI* subscriber, std::vector<InstanceEventType> const& types)
{
    if (!subscriber)
        return;

    std::lock_guard lock(_subscriberMutex);

    for (auto type : types)
    {
        auto& typeSubscribers = _subscribers[type];
        if (std::find(typeSubscribers.begin(), typeSubscribers.end(), subscriber) == typeSubscribers.end())
        {
            typeSubscribers.push_back(subscriber);
            TC_LOG_DEBUG("playerbot.events", "InstanceEventBus: Subscriber {} registered for type {}",
                static_cast<void*>(subscriber), uint32(type));
        }
    }
}

void InstanceEventBus::SubscribeAll(BotAI* subscriber)
{
    if (!subscriber)
        return;

    std::lock_guard lock(_subscriberMutex);

    if (std::find(_globalSubscribers.begin(), _globalSubscribers.end(), subscriber) == _globalSubscribers.end())
    {
        _globalSubscribers.push_back(subscriber);
        TC_LOG_DEBUG("playerbot.events", "InstanceEventBus: Subscriber {} registered for ALL events",
            static_cast<void*>(subscriber));
    }
}

void InstanceEventBus::Unsubscribe(BotAI* subscriber)
{
    if (!subscriber)
        return;

    std::lock_guard lock(_subscriberMutex);

    // Remove from type-specific subscriptions
    for (auto& [type, subscribers] : _subscribers)
    {
        subscribers.erase(std::remove(subscribers.begin(), subscribers.end(), subscriber), subscribers.end());
    }

    // Remove from global subscriptions
    _globalSubscribers.erase(std::remove(_globalSubscribers.begin(), _globalSubscribers.end(), subscriber),
        _globalSubscribers.end());

    TC_LOG_DEBUG("playerbot.events", "InstanceEventBus: Subscriber {} unregistered", static_cast<void*>(subscriber));
}

uint32 InstanceEventBus::SubscribeCallback(EventHandler handler, std::vector<InstanceEventType> const& types)
{
    if (!handler)
        return 0;

    std::lock_guard lock(_subscriberMutex);

    CallbackSubscription sub;
    sub.id = _nextCallbackId++;
    sub.handler = std::move(handler);
    sub.types = types;

    _callbackSubscriptions.push_back(std::move(sub));

    TC_LOG_DEBUG("playerbot.events", "InstanceEventBus: Callback {} registered for {} types",
        sub.id, types.size());

    return sub.id;
}

void InstanceEventBus::UnsubscribeCallback(uint32 subscriptionId)
{
    std::lock_guard lock(_subscriberMutex);

    _callbackSubscriptions.erase(
        std::remove_if(_callbackSubscriptions.begin(), _callbackSubscriptions.end(),
            [subscriptionId](CallbackSubscription const& sub) { return sub.id == subscriptionId; }),
        _callbackSubscriptions.end());

    TC_LOG_DEBUG("playerbot.events", "InstanceEventBus: Callback {} unregistered", subscriptionId);
}

uint64 InstanceEventBus::GetEventCount(InstanceEventType type) const
{
    std::lock_guard lock(_subscriberMutex);
    auto it = _eventCounts.find(type);
    return it != _eventCounts.end() ? it->second : 0;
}

void InstanceEventBus::DeliverEvent(InstanceEvent const& event)
{
    std::lock_guard lock(_subscriberMutex);

    // Deliver to type-specific subscribers
    auto it = _subscribers.find(event.type);
    if (it != _subscribers.end())
    {
        for (auto subscriber : it->second)
        {
            if (subscriber)
                subscriber->OnInstanceEvent(event);
        }
    }

    // Deliver to global subscribers
    for (auto subscriber : _globalSubscribers)
    {
        if (subscriber)
            subscriber->OnInstanceEvent(event);
    }

    // Deliver to callback subscriptions
    for (auto const& sub : _callbackSubscriptions)
    {
        if (sub.types.empty() || std::find(sub.types.begin(), sub.types.end(), event.type) != sub.types.end())
        {
            sub.handler(event);
        }
    }
}

} // namespace Playerbot
