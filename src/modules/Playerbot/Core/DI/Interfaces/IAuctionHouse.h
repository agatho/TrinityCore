/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include "Player.h"
#include <vector>
#include <string>

class Player;
class AuctionHouseObject;

namespace Playerbot
{

// Forward declarations
enum class AuctionStrategy : uint8;
enum class AuctionActionType : uint8;
struct AuctionItem;
struct AuctionSearchQuery;
struct AuctionProfile;
struct AuctionSession;
struct AuctionMetrics;

class TC_GAME_API IAuctionHouse
{
public:
    virtual ~IAuctionHouse() = default;

    // Core auction house operations
    virtual void SearchAuctionHouse(const AuctionSearchQuery& query) = 0;
    virtual bool PlaceAuctionBid(uint32 auctionId, uint32 bidAmount) = 0;
    virtual bool BuyoutAuction(uint32 auctionId) = 0;
    virtual bool CreateAuction(uint32 itemGuid, uint32 stackCount, uint32 bid, uint32 buyout, uint32 duration) = 0;
    virtual bool CancelAuction(uint32 auctionId) = 0;

    // Intelligent auction strategies
    virtual void ExecuteAuctionStrategy(AuctionStrategy strategy) = 0;
    virtual void ScanForBargains() = 0;
    virtual void AutoSellItems(const std::vector<uint32>& itemGuids) = 0;
    virtual void AutoBuyNeededItems() = 0;
    virtual void ManageActiveAuctions() = 0;

    // Market analysis and price discovery
    virtual float GetMarketPrice(uint32 itemId, uint32 stackSize) = 0;
    virtual float GetPriceHistory(uint32 itemId, uint32 days) = 0;
    virtual std::vector<AuctionItem> GetSimilarAuctions(uint32 itemId, uint32 maxResults) = 0;
    virtual bool IsPriceBelowMarket(uint32 itemId, uint32 price, float threshold) = 0;
    virtual void UpdateMarketData() = 0;

    // Advanced auction features
    virtual void SetAuctionProfile(const AuctionProfile& profile) = 0;
    virtual AuctionProfile GetAuctionProfile() = 0;

    // Auction monitoring and automation
    virtual uint32 StartAuctionSession(AuctionActionType primaryAction) = 0;
    virtual void UpdateAuctionSession(uint32 sessionId) = 0;
    virtual void CompleteAuctionSession(uint32 sessionId) = 0;
    virtual AuctionSession GetAuctionSession(uint32 sessionId) = 0;

    // Price optimization and profit calculation
    virtual uint32 CalculateOptimalListingPrice(uint32 itemId, uint32 stackSize) = 0;
    virtual uint32 CalculateMaxBidAmount(uint32 itemId, uint32 stackSize) = 0;
    virtual float CalculatePotentialProfit(const AuctionItem& auction, uint32 resellPrice) = 0;
    virtual bool IsWorthBuying(const AuctionItem& auction) = 0;
    virtual bool ShouldUndercut(uint32 itemId, uint32 currentLowest) = 0;

    // Market intelligence and learning
    virtual void TrackPriceMovement(uint32 itemId, uint32 price, uint32 timestamp) = 0;
    virtual void AnalyzeMarketTrends(uint32 itemId) = 0;
    virtual void LearnFromAuctionOutcome(uint32 auctionId, bool wasSuccessful) = 0;
    virtual void AdaptAuctionBehavior() = 0;

    // Specialized auction services
    virtual void HandleConsumableAutomation() = 0;
    virtual void HandleEquipmentUpgrades() = 0;
    virtual void HandleCraftingMaterials() = 0;
    virtual void HandleCollectibleTrading() = 0;
    virtual void HandleBulkItemTrading() = 0;

    // Competition analysis
    virtual void AnalyzeCompetition(uint32 itemId) = 0;
    virtual std::vector<uint32> GetFrequentSellers(uint32 itemId) = 0;
    virtual float GetCompetitorUndercutRate(uint32 sellerGuid) = 0;
    virtual void TrackCompetitorBehavior(uint32 sellerGuid, const AuctionItem& auction) = 0;

    // Performance monitoring
    virtual AuctionMetrics GetAuctionMetrics() = 0;
    virtual AuctionMetrics GetGlobalAuctionMetrics() = 0;

    // Auction house integration
    virtual void LoadAuctionData() = 0;
    virtual void SynchronizeWithAuctionHouseMgr() = 0;
    virtual AuctionHouseObject* GetAuctionHouseForPlayer() = 0;
    virtual bool ValidateAuctionAccess(uint32 auctionHouseId) = 0;

    // Configuration and customization
    virtual void SetAuctionHouseEnabled(bool enabled) = 0;
    virtual void SetMaxConcurrentAuctions(uint32 maxAuctions) = 0;
    virtual void SetAuctionBudget(uint32 budget) = 0;
    virtual void AddToWatchList(uint32 itemId) = 0;
    virtual void RemoveFromWatchList(uint32 itemId) = 0;

    // Error handling and recovery
    virtual void HandleAuctionError(uint32 sessionId, const std::string& error) = 0;
    virtual void RecoverFromAuctionFailure(uint32 sessionId) = 0;
    virtual void HandleInsufficientFunds(uint32 requiredAmount) = 0;
    virtual void HandleAuctionTimeout(uint32 auctionId) = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void UpdateAuctionSessions() = 0;
    virtual void UpdateMarketAnalysis() = 0;
    virtual void CleanupExpiredData() = 0;
};

} // namespace Playerbot
