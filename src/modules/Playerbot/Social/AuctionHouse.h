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
#include "Threading/LockHierarchy.h"
#include "Player.h"
#include "AuctionHouseMgr.h"
#include "Item.h"
#include "../Core/DI/Interfaces/IAuctionHouse.h"
#include <unordered_map>
#include <vector>
#include <queue>
#include <atomic>
#include <mutex>
#include <chrono>
#include "GameTime.h"

namespace Playerbot
{

enum class AuctionStrategy : uint8
{
    CONSERVATIVE    = 0,  // Buy below market value, list at market value
    AGGRESSIVE      = 1,  // Willing to pay market price, undercut significantly
    OPPORTUNISTIC   = 2,  // Scan for bargains and flip opportunities
    MARKET_MAKER    = 3,  // Provide liquidity by buying and selling regularly
    COLLECTOR       = 4,  // Focus on rare items and collectibles
    PROFIT_FOCUSED  = 5   // Maximize gold generation through trading
};

enum class AuctionActionType : uint8
{
    BUY_ITEM        = 0,
    SELL_ITEM       = 1,
    CANCEL_AUCTION  = 2,
    UPDATE_BID      = 3,
    SEARCH_MARKET   = 4,
    ANALYZE_PRICES  = 5
};

struct AuctionItem
{
    uint32 auctionId;
    uint32 itemId;
    uint32 itemGuid;
    uint32 stackCount;
    uint32 currentBid;
    uint32 buyoutPrice;
    uint32 timeLeft;
    uint32 sellerGuid;
    std::string sellerName;
    uint32 quality;
    uint32 itemLevel;
    bool hasEnchants;
    bool hasSockets;
    float marketValue;
    float pricePerItem;
    bool isBargain;
    uint32 lastSeen;

    AuctionItem() : auctionId(0), itemId(0), itemGuid(0), stackCount(1)
        , currentBid(0), buyoutPrice(0), timeLeft(0), sellerGuid(0)
        , quality(0), itemLevel(0), hasEnchants(false), hasSockets(false)
        , marketValue(0.0f), pricePerItem(0.0f), isBargain(false)
        , lastSeen(GameTime::GetGameTimeMS()) {}
};

struct AuctionSearchQuery
{
    std::string itemName;
    uint32 itemId;
    uint32 minLevel;
    uint32 maxLevel;
    uint32 minQuality;
    uint32 maxQuality;
    uint32 maxPrice;
    uint32 minItemLevel;
    std::vector<uint32> itemClasses;
    std::vector<uint32> itemSubClasses;
    bool exactMatch;
    bool usableOnly;

    AuctionSearchQuery() : itemId(0), minLevel(0), maxLevel(0), minQuality(0)
        , maxQuality(6), maxPrice(0), minItemLevel(0), exactMatch(false)
        , usableOnly(false) {}
};

/**
 * @brief Advanced auction house system for automated buying and selling
 *
 * This system provides intelligent auction house interactions, market analysis,
 * and automated trading strategies for playerbots using TrinityCore's auction system.
 */
class TC_GAME_API AuctionHouse final : public IAuctionHouse
{
public:
    static AuctionHouse* instance();

    // Core auction house operations using TrinityCore's AuctionHouseMgr
    void SearchAuctionHouse(Player* player, const AuctionSearchQuery& query) override;
    bool PlaceAuctionBid(Player* player, uint32 auctionId, uint32 bidAmount) override;
    bool BuyoutAuction(Player* player, uint32 auctionId) override;
    bool CreateAuction(Player* player, uint32 itemGuid, uint32 stackCount, uint32 bid, uint32 buyout, uint32 duration) override;
    bool CancelAuction(Player* player, uint32 auctionId) override;

    // Intelligent auction strategies
    void ExecuteAuctionStrategy(Player* player, AuctionStrategy strategy) override;
    void ScanForBargains(Player* player) override;
    void AutoSellItems(Player* player, const std::vector<uint32>& itemGuids) override;
    void AutoBuyNeededItems(Player* player) override;
    void ManageActiveAuctions(Player* player) override;

    // Market analysis and price discovery
    float GetMarketPrice(uint32 itemId, uint32 stackSize = 1) override;
    float GetPriceHistory(uint32 itemId, uint32 days = 7) override;
    std::vector<AuctionItem> GetSimilarAuctions(uint32 itemId, uint32 maxResults = 10) override;
    bool IsPriceBelowMarket(uint32 itemId, uint32 price, float threshold = 0.8f) override;
    void UpdateMarketData() override;

    // Advanced auction features
    struct AuctionProfile
    {
        AuctionStrategy primaryStrategy;
        AuctionStrategy secondaryStrategy;
        uint32 maxBiddingBudget;
        uint32 maxListingBudget;
        float bargainThreshold; // Buy if price is below this % of market value
        float profitMargin; // Minimum profit margin for flipping
        std::vector<uint32> preferredItemTypes;
        std::vector<uint32> avoidedItemTypes;
        std::unordered_set<uint32> watchList; // Items to monitor
        std::unordered_set<uint32> blackList; // Never buy these items
        bool autoRelist; // Automatically relist unsold items
        bool autoBuyConsumables;
        bool autoSellJunk;
        uint32 maxAuctionsActive;

        AuctionProfile() : primaryStrategy(AuctionStrategy::CONSERVATIVE)
            , secondaryStrategy(AuctionStrategy::OPPORTUNISTIC), maxBiddingBudget(10000)
            , maxListingBudget(5000), bargainThreshold(0.7f), profitMargin(0.2f)
            , autoRelist(true), autoBuyConsumables(false), autoSellJunk(true)
            , maxAuctionsActive(10) {}
    };

    void SetAuctionProfile(uint32 playerGuid, const AuctionProfile& profile) override;
    AuctionProfile GetAuctionProfile(uint32 playerGuid) override;

    // Auction monitoring and automation
    struct AuctionSession
    {
        uint32 sessionId;
        uint32 playerGuid;
        AuctionActionType actionType;
        std::vector<AuctionItem> searchResults;
        std::vector<uint32> targetAuctions;
        std::queue<std::pair<AuctionActionType, uint32>> actionQueue;
        uint32 sessionStartTime;
        uint32 budgetUsed;
        uint32 itemsSold;
        uint32 itemsBought;
        bool isActive;

        AuctionSession(uint32 id, uint32 pGuid) : sessionId(id), playerGuid(pGuid)
            , actionType(AuctionActionType::SEARCH_MARKET), sessionStartTime(GameTime::GetGameTimeMS())
            , budgetUsed(0), itemsSold(0), itemsBought(0), isActive(true) {}
    };

    uint32 StartAuctionSession(Player* player, AuctionActionType primaryAction) override;
    void UpdateAuctionSession(uint32 sessionId) override;
    void CompleteAuctionSession(uint32 sessionId) override;
    AuctionSession GetAuctionSession(uint32 sessionId) override;

    // Price optimization and profit calculation
    uint32 CalculateOptimalListingPrice(Player* player, uint32 itemId, uint32 stackSize) override;
    uint32 CalculateMaxBidAmount(Player* player, uint32 itemId, uint32 stackSize) override;
    float CalculatePotentialProfit(const AuctionItem& auction, uint32 resellPrice) override;
    bool IsWorthBuying(Player* player, const AuctionItem& auction) override;
    bool ShouldUndercut(Player* player, uint32 itemId, uint32 currentLowest) override;

    // Market intelligence and learning
    void TrackPriceMovement(uint32 itemId, uint32 price, uint32 timestamp) override;
    void AnalyzeMarketTrends(uint32 itemId) override;
    void LearnFromAuctionOutcome(Player* player, uint32 auctionId, bool wasSuccessful) override;
    void AdaptAuctionBehavior(Player* player) override;

    // Specialized auction services
    void HandleConsumableAutomation(Player* player) override;
    void HandleEquipmentUpgrades(Player* player) override;
    void HandleCraftingMaterials(Player* player) override;
    void HandleCollectibleTrading(Player* player) override;
    void HandleBulkItemTrading(Player* player) override;

    // Competition analysis
    void AnalyzeCompetition(uint32 itemId) override;
    std::vector<uint32> GetFrequentSellers(uint32 itemId) override;
    float GetCompetitorUndercutRate(uint32 sellerGuid) override;
    void TrackCompetitorBehavior(uint32 sellerGuid, const AuctionItem& auction) override;

    // Performance monitoring
    struct AuctionMetrics
    {
        std::atomic<uint32> auctionsCreated{0};
        std::atomic<uint32> auctionsWon{0};
        std::atomic<uint32> auctionsLost{0};
        std::atomic<uint32> itemsSold{0};
        std::atomic<uint32> itemsBought{0};
        std::atomic<uint32> totalGoldSpent{0};
        std::atomic<uint32> totalGoldEarned{0};
        std::atomic<float> averageProfitMargin{0.2f};
        std::atomic<float> successRate{0.8f};
        std::atomic<uint32> marketScans{0};
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            auctionsCreated = 0; auctionsWon = 0; auctionsLost = 0;
            itemsSold = 0; itemsBought = 0; totalGoldSpent = 0;
            totalGoldEarned = 0; averageProfitMargin = 0.2f; successRate = 0.8f;
            marketScans = 0; lastUpdate = std::chrono::steady_clock::now();
        }

        uint32 GetNetProfit() const {
            uint32 earned = totalGoldEarned.load();
            uint32 spent = totalGoldSpent.load();
            return earned > spent ? earned - spent : 0;
        }

        float GetWinRate() const {
            uint32 won = auctionsWon.load();
            uint32 total = won + auctionsLost.load();
            return total > 0 ? (float)won / total : 0.0f;
        }
    };

    AuctionMetrics GetPlayerAuctionMetrics(uint32 playerGuid) override;
    AuctionMetrics GetGlobalAuctionMetrics() override;

    // Auction house integration with TrinityCore
    void LoadAuctionData() override;
    void SynchronizeWithAuctionHouseMgr() override;
    AuctionHouseObject* GetAuctionHouseForPlayer(Player* player) override;
    bool ValidateAuctionAccess(Player* player, uint32 auctionHouseId) override;

    // Configuration and customization
    void SetAuctionHouseEnabled(uint32 playerGuid, bool enabled) override;
    void SetMaxConcurrentAuctions(uint32 playerGuid, uint32 maxAuctions) override;
    void SetAuctionBudget(uint32 playerGuid, uint32 budget) override;
    void AddToWatchList(uint32 playerGuid, uint32 itemId) override;
    void RemoveFromWatchList(uint32 playerGuid, uint32 itemId) override;

    // Error handling and recovery
    void HandleAuctionError(uint32 sessionId, const std::string& error) override;
    void RecoverFromAuctionFailure(uint32 sessionId) override;
    void HandleInsufficientFunds(Player* player, uint32 requiredAmount) override;
    void HandleAuctionTimeout(uint32 auctionId) override;

    // Update and maintenance
    void Update(uint32 diff) override;
    void UpdateAuctionSessions() override;
    void UpdateMarketAnalysis() override;
    void CleanupExpiredData() override;

private:
    AuctionHouse();
    ~AuctionHouse() = default;

    // Core auction data
    std::unordered_map<uint32, AuctionProfile> _playerProfiles; // playerGuid -> profile
    std::unordered_map<uint32, AuctionSession> _activeSessions; // sessionId -> session
    std::unordered_map<uint32, AuctionMetrics> _playerMetrics; // playerGuid -> metrics
    std::atomic<uint32> _nextSessionId{1};
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::TRADE_MANAGER> _auctionMutex;

    // Market data and analysis
    struct MarketData
    {
        uint32 itemId;
        std::vector<std::pair<uint32, uint32>> priceHistory; // price, timestamp
        float averagePrice;
        float medianPrice;
        uint32 totalVolume;
        uint32 activeListings;
        float volatility;
        uint32 lastAnalysisTime;

        MarketData(uint32 id) : itemId(id), averagePrice(0.0f), medianPrice(0.0f)
            , totalVolume(0), activeListings(0), volatility(0.0f)
            , lastAnalysisTime(GameTime::GetGameTimeMS()) {}
    };

    std::unordered_map<uint32, MarketData> _marketData; // itemId -> market data
    std::unordered_map<uint32, std::vector<AuctionItem>> _auctionCache; // itemId -> cached auctions
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::TRADE_MANAGER> _marketMutex;

    // Competition tracking
    struct CompetitorProfile
    {
        uint32 sellerGuid;
        std::string sellerName;
        std::vector<uint32> frequentItems;
        std::vector<std::pair<uint32, uint32>> pricingHistory; // itemId, price
        float averageUndercutRate;
        float aggressiveness;
        uint32 totalAuctions;
        uint32 successfulSales;
        uint32 lastActivity;

        CompetitorProfile(uint32 guid) : sellerGuid(guid), averageUndercutRate(0.05f)
            , aggressiveness(0.5f), totalAuctions(0), successfulSales(0)
            , lastActivity(GameTime::GetGameTimeMS()) {}
    };

    std::unordered_map<uint32, CompetitorProfile> _competitors; // sellerGuid -> profile

    // Performance tracking
    AuctionMetrics _globalMetrics;

    // Strategy implementations
    void ExecuteConservativeStrategy(Player* player, AuctionSession& session);
    void ExecuteAggressiveStrategy(Player* player, AuctionSession& session);
    void ExecuteOpportunisticStrategy(Player* player, AuctionSession& session);
    void ExecuteMarketMakerStrategy(Player* player, AuctionSession& session);
    void ExecuteCollectorStrategy(Player* player, AuctionSession& session);
    void ExecuteProfitFocusedStrategy(Player* player, AuctionSession& session);

    // Market analysis helpers
    void UpdateItemMarketData(uint32 itemId);
    float CalculateMarketVolatility(const std::vector<std::pair<uint32, uint32>>& priceHistory);
    float PredictPriceMovement(uint32 itemId);
    bool IsMarketTrendy(uint32 itemId, bool& isRising);

    // Search and filtering
    std::vector<AuctionItem> FilterAuctionResults(const std::vector<AuctionItem>& auctions,
                                                 const AuctionSearchQuery& query);
    std::vector<AuctionItem> FindBargainAuctions(Player* player, float maxPriceRatio = 0.8f);
    std::vector<AuctionItem> FindFlipOpportunities(Player* player);
    bool MatchesSearchCriteria(const AuctionItem& auction, const AuctionSearchQuery& query);

    // Price calculation algorithms
    uint32 CalculateReasonableBid(const AuctionItem& auction, float aggressiveness = 0.5f);
    uint32 CalculateCompetitivePrice(uint32 itemId, uint32 stackSize);
    uint32 CalculateUndercutPrice(uint32 itemId, uint32 currentLowest, float undercutRate = 0.05f);
    float CalculateExpectedReturn(const AuctionItem& auction, uint32 resellPrice);

    // Auction execution helpers
    bool ExecuteBuyAction(Player* player, const AuctionItem& auction);
    bool ExecuteSellAction(Player* player, uint32 itemGuid, uint32 stackCount, uint32 price);
    bool ExecuteCancelAction(Player* player, uint32 auctionId);
    void ProcessActionQueue(Player* player, AuctionSession& session);

    // Learning and adaptation
    void UpdateSuccessRates(Player* player, bool wasSuccessful, AuctionActionType actionType);
    void AdaptPricingStrategy(Player* player);
    void LearnMarketPatterns(uint32 itemId);
    void UpdatePlayerPreferences(Player* player, uint32 itemId, bool wasPurchased);

    // Performance optimization
    void CacheFrequentAuctions();
    void PreloadMarketData(const std::vector<uint32>& itemIds);
    void OptimizeAuctionQueries();
    void UpdateAuctionMetrics(uint32 playerGuid, AuctionActionType actionType,
                             uint32 goldAmount, bool wasSuccessful);

    // Constants
    static constexpr uint32 AUCTION_UPDATE_INTERVAL = 30000; // 30 seconds
    static constexpr uint32 MARKET_ANALYSIS_INTERVAL = 300000; // 5 minutes
    static constexpr uint32 SESSION_TIMEOUT = 1800000; // 30 minutes
    static constexpr uint32 MAX_SEARCH_RESULTS = 100;
    static constexpr float DEFAULT_UNDERCUT_RATE = 0.05f; // 5%
    static constexpr float MIN_PROFIT_MARGIN = 0.1f; // 10%
    static constexpr uint32 PRICE_HISTORY_DAYS = 30;
    static constexpr uint32 MAX_CONCURRENT_SESSIONS = 50;
    static constexpr float BARGAIN_THRESHOLD = 0.8f; // 80% of market price
    static constexpr uint32 MARKET_DATA_CACHE_DURATION = 600000; // 10 minutes
};

} // namespace Playerbot