/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "NPCEventBus.h"
#include "BotAI.h"
#include "Log.h"
#include <sstream>
#include <algorithm>

namespace Playerbot
{

// Factory methods
NPCEvent NPCEvent::GossipMenuReceived(ObjectGuid playerGuid, ObjectGuid npcGuid, uint32 menuId, uint32 textId, std::vector<uint32> options)
{
    NPCEvent event;
    event.type = NPCEventType::GOSSIP_MENU_RECEIVED;
    event.playerGuid = playerGuid;
    event.npcGuid = npcGuid;
    event.menuId = menuId;
    event.textId = textId;
    event.gossipOptions = std::move(options);
    event.vendorEntry = 0;
    event.trainerEntry = 0;
    event.trainerService = 0;
    event.petitionEntry = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

NPCEvent NPCEvent::GossipComplete(ObjectGuid playerGuid, ObjectGuid npcGuid)
{
    NPCEvent event;
    event.type = NPCEventType::GOSSIP_COMPLETE;
    event.playerGuid = playerGuid;
    event.npcGuid = npcGuid;
    event.menuId = 0;
    event.textId = 0;
    event.vendorEntry = 0;
    event.trainerEntry = 0;
    event.trainerService = 0;
    event.petitionEntry = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

NPCEvent NPCEvent::VendorListReceived(ObjectGuid playerGuid, ObjectGuid npcGuid, uint32 vendorEntry, std::vector<uint32> items)
{
    NPCEvent event;
    event.type = NPCEventType::VENDOR_LIST_RECEIVED;
    event.playerGuid = playerGuid;
    event.npcGuid = npcGuid;
    event.vendorEntry = vendorEntry;
    event.availableItems = std::move(items);
    event.menuId = 0;
    event.textId = 0;
    event.trainerEntry = 0;
    event.trainerService = 0;
    event.petitionEntry = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

NPCEvent NPCEvent::TrainerListReceived(ObjectGuid playerGuid, ObjectGuid npcGuid, uint32 trainerEntry, std::vector<uint32> spells)
{
    NPCEvent event;
    event.type = NPCEventType::TRAINER_LIST_RECEIVED;
    event.playerGuid = playerGuid;
    event.npcGuid = npcGuid;
    event.trainerEntry = trainerEntry;
    event.availableItems = std::move(spells);  // Reuse availableItems for spells
    event.menuId = 0;
    event.textId = 0;
    event.vendorEntry = 0;
    event.trainerService = 0;
    event.petitionEntry = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

NPCEvent NPCEvent::TrainerServiceResult(ObjectGuid playerGuid, uint32 trainerService)
{
    NPCEvent event;
    event.type = NPCEventType::TRAINER_SERVICE_RESULT;
    event.playerGuid = playerGuid;
    event.trainerService = trainerService;
    event.menuId = 0;
    event.textId = 0;
    event.vendorEntry = 0;
    event.trainerEntry = 0;
    event.petitionEntry = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

NPCEvent NPCEvent::BankOpened(ObjectGuid playerGuid, ObjectGuid npcGuid)
{
    NPCEvent event;
    event.type = NPCEventType::BANK_OPENED;
    event.playerGuid = playerGuid;
    event.npcGuid = npcGuid;
    event.menuId = 0;
    event.textId = 0;
    event.vendorEntry = 0;
    event.trainerEntry = 0;
    event.trainerService = 0;
    event.petitionEntry = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

NPCEvent NPCEvent::SpiritHealerConfirm(ObjectGuid playerGuid, ObjectGuid npcGuid)
{
    NPCEvent event;
    event.type = NPCEventType::SPIRIT_HEALER_CONFIRM;
    event.playerGuid = playerGuid;
    event.npcGuid = npcGuid;
    event.menuId = 0;
    event.textId = 0;
    event.vendorEntry = 0;
    event.trainerEntry = 0;
    event.trainerService = 0;
    event.petitionEntry = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

NPCEvent NPCEvent::PetitionListReceived(ObjectGuid playerGuid, ObjectGuid npcGuid, uint32 petitionEntry)
{
    NPCEvent event;
    event.type = NPCEventType::PETITION_LIST_RECEIVED;
    event.playerGuid = playerGuid;
    event.npcGuid = npcGuid;
    event.petitionEntry = petitionEntry;
    event.menuId = 0;
    event.textId = 0;
    event.vendorEntry = 0;
    event.trainerEntry = 0;
    event.trainerService = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

bool NPCEvent::IsValid() const
{
    if (type >= NPCEventType::MAX_NPC_EVENT)
        return false;

    switch (type)
    {
        case NPCEventType::GOSSIP_MENU_RECEIVED:
            return !playerGuid.IsEmpty() && !npcGuid.IsEmpty();
        case NPCEventType::GOSSIP_COMPLETE:
            return !playerGuid.IsEmpty();
        case NPCEventType::VENDOR_LIST_RECEIVED:
            return !playerGuid.IsEmpty() && !npcGuid.IsEmpty();
        case NPCEventType::TRAINER_LIST_RECEIVED:
            return !playerGuid.IsEmpty() && !npcGuid.IsEmpty();
        case NPCEventType::TRAINER_SERVICE_RESULT:
            return !playerGuid.IsEmpty();
        case NPCEventType::BANK_OPENED:
            return !playerGuid.IsEmpty() && !npcGuid.IsEmpty();
        case NPCEventType::SPIRIT_HEALER_CONFIRM:
            return !playerGuid.IsEmpty() && !npcGuid.IsEmpty();
        case NPCEventType::PETITION_LIST_RECEIVED:
            return !playerGuid.IsEmpty() && !npcGuid.IsEmpty();
        default:
            return false;
    }
}

std::string NPCEvent::ToString() const
{
    std::ostringstream oss;
    oss << "NPCEvent[";

    switch (type)
    {
        case NPCEventType::GOSSIP_MENU_RECEIVED:
            oss << "GOSSIP_MENU, npc=" << npcGuid.ToString() << ", menu=" << menuId
                << ", options=" << gossipOptions.size();
            break;
        case NPCEventType::GOSSIP_COMPLETE:
            oss << "GOSSIP_COMPLETE, npc=" << npcGuid.ToString();
            break;
        case NPCEventType::VENDOR_LIST_RECEIVED:
            oss << "VENDOR_LIST, npc=" << npcGuid.ToString() << ", vendor=" << vendorEntry
                << ", items=" << availableItems.size();
            break;
        case NPCEventType::TRAINER_LIST_RECEIVED:
            oss << "TRAINER_LIST, npc=" << npcGuid.ToString() << ", trainer=" << trainerEntry
                << ", spells=" << availableItems.size();
            break;
        case NPCEventType::TRAINER_SERVICE_RESULT:
            oss << "TRAINER_SERVICE, service=" << trainerService;
            break;
        case NPCEventType::BANK_OPENED:
            oss << "BANK_OPENED, npc=" << npcGuid.ToString();
            break;
        case NPCEventType::SPIRIT_HEALER_CONFIRM:
            oss << "SPIRIT_HEALER, npc=" << npcGuid.ToString();
            break;
        case NPCEventType::PETITION_LIST_RECEIVED:
            oss << "PETITION_LIST, npc=" << npcGuid.ToString() << ", petition=" << petitionEntry;
            break;
        default:
            oss << "UNKNOWN";
    }

    oss << "]";
    return oss.str();
}

// NPCEventBus implementation
NPCEventBus* NPCEventBus::instance()
{
    static NPCEventBus instance;
    return &instance;
}

bool NPCEventBus::PublishEvent(NPCEvent const& event)
{
    if (!event.IsValid())
    {
        TC_LOG_ERROR("playerbot.events", "NPCEventBus: Invalid event rejected: {}", event.ToString());
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

    TC_LOG_TRACE("playerbot.events", "NPCEventBus: Published event: {}", event.ToString());
    return true;
}

void NPCEventBus::Subscribe(BotAI* subscriber, std::vector<NPCEventType> const& types)
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
            TC_LOG_DEBUG("playerbot.events", "NPCEventBus: Subscriber {} registered for type {}",
                static_cast<void*>(subscriber), uint32(type));
        }
    }
}

void NPCEventBus::SubscribeAll(BotAI* subscriber)
{
    if (!subscriber)
        return;

    std::lock_guard lock(_subscriberMutex);

    if (std::find(_globalSubscribers.begin(), _globalSubscribers.end(), subscriber) == _globalSubscribers.end())
    {
        _globalSubscribers.push_back(subscriber);
        TC_LOG_DEBUG("playerbot.events", "NPCEventBus: Subscriber {} registered for ALL events",
            static_cast<void*>(subscriber));
    }
}

void NPCEventBus::Unsubscribe(BotAI* subscriber)
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

    TC_LOG_DEBUG("playerbot.events", "NPCEventBus: Subscriber {} unregistered", static_cast<void*>(subscriber));
}

uint32 NPCEventBus::SubscribeCallback(EventHandler handler, std::vector<NPCEventType> const& types)
{
    if (!handler)
        return 0;

    std::lock_guard lock(_subscriberMutex);

    CallbackSubscription sub;
    sub.id = _nextCallbackId++;
    sub.handler = std::move(handler);
    sub.types = types;

    _callbackSubscriptions.push_back(std::move(sub));

    TC_LOG_DEBUG("playerbot.events", "NPCEventBus: Callback {} registered for {} types",
        sub.id, types.size());

    return sub.id;
}

void NPCEventBus::UnsubscribeCallback(uint32 subscriptionId)
{
    std::lock_guard lock(_subscriberMutex);

    _callbackSubscriptions.erase(
        std::remove_if(_callbackSubscriptions.begin(), _callbackSubscriptions.end(),
            [subscriptionId](CallbackSubscription const& sub) { return sub.id == subscriptionId; }),
        _callbackSubscriptions.end());

    TC_LOG_DEBUG("playerbot.events", "NPCEventBus: Callback {} unregistered", subscriptionId);
}

uint64 NPCEventBus::GetEventCount(NPCEventType type) const
{
    std::lock_guard lock(_subscriberMutex);
    auto it = _eventCounts.find(type);
    return it != _eventCounts.end() ? it->second : 0;
}

void NPCEventBus::DeliverEvent(NPCEvent const& event)
{
    std::lock_guard lock(_subscriberMutex);

    // Deliver to type-specific subscribers
    auto it = _subscribers.find(event.type);
    if (it != _subscribers.end())
    {
        for (auto subscriber : it->second)
        {
            if (subscriber)
                subscriber->OnNPCEvent(event);
        }
    }

    // Deliver to global subscribers
    for (auto subscriber : _globalSubscribers)
    {
        if (subscriber)
            subscriber->OnNPCEvent(event);
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
