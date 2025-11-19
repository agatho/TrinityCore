/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_AUCTION_EVENTS_H
#define PLAYERBOT_AUCTION_EVENTS_H

#include "Define.h"
#include "ObjectGuid.h"
#include <chrono>
#include <string>

namespace Playerbot
{

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
    using EventType = AuctionEventType;

    AuctionEventType type;
    ObjectGuid playerGuid;
    uint32 auctionId;
    uint32 itemId;
    uint32 itemCount;
    uint64 bidAmount;
    uint64 buyoutAmount;
    uint32 command;
    uint32 errorCode;
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

} // namespace Playerbot

#endif // PLAYERBOT_AUCTION_EVENTS_H
