/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "AuctionEventBus.h"
#include "BotAI.h"
#include "Log.h"
#include <sstream>
#include <algorithm>

namespace Playerbot
{

// Factory methods
AuctionEvent AuctionEvent::CommandResult(ObjectGuid playerGuid, uint32 auctionId, uint32 command, uint32 errorCode)
{
    AuctionEvent event;
    event.type = AuctionEventType::AUCTION_COMMAND_RESULT;
    event.playerGuid = playerGuid;
    event.auctionId = auctionId;
    event.command = command;
    event.errorCode = errorCode;
    event.itemId = 0;
    event.itemCount = 0;
    event.bidAmount = 0;
    event.buyoutAmount = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

AuctionEvent AuctionEvent::ListReceived(ObjectGuid playerGuid, uint32 itemCount)
{
    AuctionEvent event;
    event.type = AuctionEventType::AUCTION_LIST_RECEIVED;
    event.playerGuid = playerGuid;
    event.itemCount = itemCount;
    event.auctionId = 0;
    event.itemId = 0;
    event.bidAmount = 0;
    event.buyoutAmount = 0;
    event.command = 0;
    event.errorCode = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

AuctionEvent AuctionEvent::BidPlaced(ObjectGuid playerGuid, uint32 auctionId, uint64 bidAmount)
{
    AuctionEvent event;
    event.type = AuctionEventType::AUCTION_BID_PLACED;
    event.playerGuid = playerGuid;
    event.auctionId = auctionId;
    event.bidAmount = bidAmount;
    event.itemId = 0;
    event.itemCount = 0;
    event.buyoutAmount = 0;
    event.command = 0;
    event.errorCode = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

AuctionEvent AuctionEvent::AuctionWon(ObjectGuid playerGuid, uint32 auctionId, uint32 itemId, uint64 bidAmount)
{
    AuctionEvent event;
    event.type = AuctionEventType::AUCTION_WON;
    event.playerGuid = playerGuid;
    event.auctionId = auctionId;
    event.itemId = itemId;
    event.bidAmount = bidAmount;
    event.itemCount = 0;
    event.buyoutAmount = 0;
    event.command = 0;
    event.errorCode = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

AuctionEvent AuctionEvent::Outbid(ObjectGuid playerGuid, uint32 auctionId, uint64 newBid)
{
    AuctionEvent event;
    event.type = AuctionEventType::AUCTION_OUTBID;
    event.playerGuid = playerGuid;
    event.auctionId = auctionId;
    event.bidAmount = newBid;
    event.itemId = 0;
    event.itemCount = 0;
    event.buyoutAmount = 0;
    event.command = 0;
    event.errorCode = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

AuctionEvent AuctionEvent::Expired(ObjectGuid playerGuid, uint32 auctionId, uint32 itemId)
{
    AuctionEvent event;
    event.type = AuctionEventType::AUCTION_EXPIRED;
    event.playerGuid = playerGuid;
    event.auctionId = auctionId;
    event.itemId = itemId;
    event.itemCount = 0;
    event.bidAmount = 0;
    event.buyoutAmount = 0;
    event.command = 0;
    event.errorCode = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

bool AuctionEvent::IsValid() const
{
    if (type >= AuctionEventType::MAX_AUCTION_EVENT)
        return false;

    switch (type)
    {
        case AuctionEventType::AUCTION_COMMAND_RESULT:
            return !playerGuid.IsEmpty();
        case AuctionEventType::AUCTION_LIST_RECEIVED:
            return !playerGuid.IsEmpty();
        case AuctionEventType::AUCTION_BID_PLACED:
            return !playerGuid.IsEmpty() && auctionId > 0;
        case AuctionEventType::AUCTION_WON:
            return !playerGuid.IsEmpty() && auctionId > 0;
        case AuctionEventType::AUCTION_OUTBID:
            return !playerGuid.IsEmpty() && auctionId > 0;
        case AuctionEventType::AUCTION_EXPIRED:
            return !playerGuid.IsEmpty() && auctionId > 0;
        default:
            return false;
    }
}

std::string AuctionEvent::ToString() const
{
    std::ostringstream oss;
    oss << "AuctionEvent[";

    switch (type)
    {
        case AuctionEventType::AUCTION_COMMAND_RESULT:
            oss << "COMMAND_RESULT, auction=" << auctionId << ", command=" << command << ", error=" << errorCode;
            break;
        case AuctionEventType::AUCTION_LIST_RECEIVED:
            oss << "LIST_RECEIVED, items=" << itemCount;
            break;
        case AuctionEventType::AUCTION_BID_PLACED:
            oss << "BID_PLACED, auction=" << auctionId << ", bid=" << bidAmount;
            break;
        case AuctionEventType::AUCTION_WON:
            oss << "AUCTION_WON, auction=" << auctionId << ", item=" << itemId << ", bid=" << bidAmount;
            break;
        case AuctionEventType::AUCTION_OUTBID:
            oss << "OUTBID, auction=" << auctionId << ", newBid=" << bidAmount;
            break;
        case AuctionEventType::AUCTION_EXPIRED:
            oss << "EXPIRED, auction=" << auctionId << ", item=" << itemId;
            break;
        default:
            oss << "UNKNOWN";
    }

    oss << "]";
    return oss.str();
}

// AuctionEventBus implementation
AuctionEventBus* AuctionEventBus::instance()
{
    static AuctionEventBus instance;
    return &instance;
}

bool AuctionEventBus::PublishEvent(AuctionEvent const& event)
{
    if (!event.IsValid())
    {
        TC_LOG_ERROR("playerbot.events", "AuctionEventBus: Invalid event rejected: {}", event.ToString());
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

    TC_LOG_TRACE("playerbot.events", "AuctionEventBus: Published event: {}", event.ToString());
    return true;
}

void AuctionEventBus::Subscribe(BotAI* subscriber, std::vector<AuctionEventType> const& types)
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
            TC_LOG_DEBUG("playerbot.events", "AuctionEventBus: Subscriber {} registered for type {}",
                static_cast<void*>(subscriber), uint32(type));
        }
    }
}

void AuctionEventBus::SubscribeAll(BotAI* subscriber)
{
    if (!subscriber)
        return;

    std::lock_guard lock(_subscriberMutex);

    if (std::find(_globalSubscribers.begin(), _globalSubscribers.end(), subscriber) == _globalSubscribers.end())
    {
        _globalSubscribers.push_back(subscriber);
        TC_LOG_DEBUG("playerbot.events", "AuctionEventBus: Subscriber {} registered for ALL events",
            static_cast<void*>(subscriber));
    }
}

void AuctionEventBus::Unsubscribe(BotAI* subscriber)
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

    TC_LOG_DEBUG("playerbot.events", "AuctionEventBus: Subscriber {} unregistered", static_cast<void*>(subscriber));
}

uint32 AuctionEventBus::SubscribeCallback(EventHandler handler, std::vector<AuctionEventType> const& types)
{
    if (!handler)
        return 0;

    std::lock_guard lock(_subscriberMutex);

    CallbackSubscription sub;
    sub.id = _nextCallbackId++;
    sub.handler = std::move(handler);
    sub.types = types;

    _callbackSubscriptions.push_back(std::move(sub));

    TC_LOG_DEBUG("playerbot.events", "AuctionEventBus: Callback {} registered for {} types",
        sub.id, types.size());

    return sub.id;
}

void AuctionEventBus::UnsubscribeCallback(uint32 subscriptionId)
{
    std::lock_guard lock(_subscriberMutex);

    _callbackSubscriptions.erase(
        std::remove_if(_callbackSubscriptions.begin(), _callbackSubscriptions.end(),
            [subscriptionId](CallbackSubscription const& sub) { return sub.id == subscriptionId; }),
        _callbackSubscriptions.end());

    TC_LOG_DEBUG("playerbot.events", "AuctionEventBus: Callback {} unregistered", subscriptionId);
}

uint64 AuctionEventBus::GetEventCount(AuctionEventType type) const
{
    std::lock_guard lock(_subscriberMutex);
    auto it = _eventCounts.find(type);
    return it != _eventCounts.end() ? it->second : 0;
}

void AuctionEventBus::DeliverEvent(AuctionEvent const& event)
{
    std::lock_guard lock(_subscriberMutex);

    // Deliver to type-specific subscribers
    auto it = _subscribers.find(event.type);
    if (it != _subscribers.end())
    {
        for (auto subscriber : it->second)
        {
            if (subscriber)
                subscriber->OnAuctionEvent(event);
        }
    }

    // Deliver to global subscribers
    for (auto subscriber : _globalSubscribers)
    {
        if (subscriber)
            subscriber->OnAuctionEvent(event);
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
