/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_AUCTION_EVENT_BUS_H
#define PLAYERBOT_AUCTION_EVENT_BUS_H

#include "Define.h"
#include "Threading/LockHierarchy.h"
#include "ObjectGuid.h"
#include "Core/DI/Interfaces/IAuctionEventBus.h"
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>

namespace Playerbot
{

class BotAI;

enum class AuctionEventType : uint8
{
    AUCTION_COMMAND_RESULT = 0,
    AUCTION_LIST_RECEIVED,
    AUCTION_BID_PLACED,
    AUCTION_WON,
    AUCTION_OUTBID,
    AUCTION_EXPIRED,
    MAX_AUCTION_EVENT
};

struct AuctionEvent
{
    AuctionEventType type;
    ObjectGuid playerGuid;
    uint32 auctionId;
    uint32 itemId;
    uint32 itemCount;
    uint64 bidAmount;
    uint64 buyoutAmount;
    uint32 command;      // AuctionAction enum
    uint32 errorCode;    // AuctionError enum
    std::chrono::steady_clock::time_point timestamp;

    static AuctionEvent CommandResult(ObjectGuid playerGuid, uint32 auctionId, uint32 command, uint32 errorCode);
    static AuctionEvent ListReceived(ObjectGuid playerGuid, uint32 itemCount);
    static AuctionEvent BidPlaced(ObjectGuid playerGuid, uint32 auctionId, uint64 bidAmount);
    static AuctionEvent AuctionWon(ObjectGuid playerGuid, uint32 auctionId, uint32 itemId, uint64 bidAmount);
    static AuctionEvent Outbid(ObjectGuid playerGuid, uint32 auctionId, uint64 newBid);
    static AuctionEvent Expired(ObjectGuid playerGuid, uint32 auctionId, uint32 itemId);

    bool IsValid() const;
    std::string ToString() const;
};

class TC_GAME_API AuctionEventBus final : public IAuctionEventBus
{
public:
    static AuctionEventBus* instance();
    bool PublishEvent(AuctionEvent const& event) override;

    using EventHandler = std::function<void(AuctionEvent const&)>;

    void Subscribe(BotAI* subscriber, std::vector<AuctionEventType> const& types) override;
    void SubscribeAll(BotAI* subscriber) override;
    void Unsubscribe(BotAI* subscriber) override;

    uint32 SubscribeCallback(EventHandler handler, std::vector<AuctionEventType> const& types) override;
    void UnsubscribeCallback(uint32 subscriptionId) override;

    uint64 GetTotalEventsPublished() const override { return _totalEventsPublished; }
    uint64 GetEventCount(AuctionEventType type) const override;

private:
    AuctionEventBus() = default;
    void DeliverEvent(AuctionEvent const& event);

    std::unordered_map<AuctionEventType, std::vector<BotAI*>> _subscribers;
    std::vector<BotAI*> _globalSubscribers;

    struct CallbackSubscription
    {
        uint32 id;
        EventHandler handler;
        std::vector<AuctionEventType> types;
    };
    std::vector<CallbackSubscription> _callbackSubscriptions;
    uint32 _nextCallbackId = 1;

    std::unordered_map<AuctionEventType, uint64> _eventCounts;
    uint64 _totalEventsPublished = 0;

    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::TRADE_MANAGER> _subscriberMutex;
};

} // namespace Playerbot

#endif // PLAYERBOT_AUCTION_EVENT_BUS_H
