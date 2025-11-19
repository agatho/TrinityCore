/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "AuctionHouse.h"

namespace Playerbot
{

// Static member definitions
std::atomic<uint32> AuctionHouse::_nextSessionId{1};
std::unordered_map<uint32, AuctionHouse::MarketData> AuctionHouse::_marketData;
std::unordered_map<uint32, std::vector<AuctionItem>> AuctionHouse::_auctionCache;
std::unordered_map<uint32, AuctionHouse::CompetitorProfile> AuctionHouse::_competitors;
AuctionHouse::AuctionMetrics AuctionHouse::_globalMetrics;
Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::TRADE_MANAGER> AuctionHouse::_marketMutex;

// Constructor
AuctionHouse::AuctionHouse(Player* bot)
    : _bot(bot)
{
    if (!_bot)
    {
        TC_LOG_ERROR("playerbot.auction", "AuctionHouse: Attempted to create with null bot!");
        return;
    }

    TC_LOG_DEBUG("playerbot.auction", "AuctionHouse: Created for bot {} ({})",
                 _bot->GetName(), _bot->GetGUID().ToString());
}

// Destructor
AuctionHouse::~AuctionHouse()
{
    if (_bot)
        TC_LOG_DEBUG("playerbot.auction", "AuctionHouse: Destroyed for bot {} ({})",
                     _bot->GetName(), _bot->GetGUID().ToString());
}

} // namespace Playerbot
