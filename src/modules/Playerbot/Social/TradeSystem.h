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
#include "Item.h"
#include "Creature.h"
#include "NPCHandler.h"
#include "Position.h"
#include "../Core/DI/Interfaces/ITradeSystem.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <atomic>
#include <mutex>
#include <chrono>

class Player;
class Item;
class Creature;

namespace Playerbot
{

enum class TradeType : uint8
{
    PLAYER_TO_PLAYER    = 0,
    VENDOR_BUY          = 1,
    VENDOR_SELL         = 2,
    REPAIR_EQUIPMENT    = 3,
    INNKEEPER_SERVICE   = 4,
    GUILD_BANK          = 5,
    MAIL_TRADE          = 6
};

enum class TradeDecision : uint8
{
    ACCEPT      = 0,
    DECLINE     = 1,
    NEGOTIATE   = 2,
    COUNTER     = 3,
    PENDING     = 4
};

enum class VendorType : uint8
{
    GENERAL_GOODS   = 0,
    ARMOR_VENDOR    = 1,
    WEAPON_VENDOR   = 2,
    FOOD_VENDOR     = 3,
    REAGENT_VENDOR  = 4,
    REPAIR_VENDOR   = 5,
    INNKEEPER       = 6,
    FLIGHT_MASTER   = 7,
    QUEST_GIVER     = 8,
    TRAINER         = 9
};

struct VendorInfo
{
    uint32 creatureId;
    uint32 creatureGuid;
    std::string vendorName;
    VendorType vendorType;
    Position location;
    uint32 zoneId;
    uint32 areaId;
    std::vector<uint32> availableItems;
    std::vector<uint32> buyableItems;
    bool isRepairVendor;
    bool isInnkeeper;
    bool isFlightMaster;
    uint32 factionId;
    float discountRate;
    uint32 lastInteractionTime;

    VendorInfo(uint32 id, uint32 guid, const std::string& name, VendorType type)
        : creatureId(id), creatureGuid(guid), vendorName(name), vendorType(type)
        , zoneId(0), areaId(0), isRepairVendor(false), isInnkeeper(false)
        , isFlightMaster(false), factionId(0), discountRate(0.0f)
        , lastInteractionTime(0) {}
};

struct TradeSession
{
    uint32 sessionId;
    uint32 initiatorGuid;
    uint32 targetGuid;
    TradeType tradeType;
    std::vector<std::pair<uint32, uint32>> initiatorItems; // itemGuid, count
    std::vector<std::pair<uint32, uint32>> targetItems;
    uint32 initiatorGold;
    uint32 targetGold;
    bool initiatorAccepted;
    bool targetAccepted;
    uint32 sessionStartTime;
    uint32 sessionTimeout;
    bool isActive;
    std::string tradeReason;

    TradeSession(uint32 id, uint32 init, uint32 target, TradeType type)
        : sessionId(id), initiatorGuid(init), targetGuid(target), tradeType(type)
        , initiatorGold(0), targetGold(0), initiatorAccepted(false), targetAccepted(false)
        , sessionStartTime(GameTime::GetGameTimeMS()), sessionTimeout(GameTime::GetGameTimeMS() + 120000) // 2 minutes
        , isActive(true) {}
};

struct TradeConfiguration
{
    bool autoAcceptTrades;
    bool autoDeclineTrades;
    bool acceptTradesFromGuildMembers;
    bool acceptTradesFromFriends;
    uint32 maxTradeValue;
    uint32 minItemLevel;
    std::unordered_set<uint32> acceptableItemTypes;
    std::unordered_set<uint32> blacklistedItems;
    std::unordered_set<uint32> trustedPlayers;
    float acceptanceThreshold; // 0.0 = never accept, 1.0 = always accept
    bool requireItemAnalysis;
    bool enableTradeHistory;

    TradeConfiguration() : autoAcceptTrades(false), autoDeclineTrades(false)
        , acceptTradesFromGuildMembers(true), acceptTradesFromFriends(true)
        , maxTradeValue(10000), minItemLevel(0), acceptanceThreshold(0.5f)
        , requireItemAnalysis(true), enableTradeHistory(true) {}
};

class TC_GAME_API TradeSystem final : public ITradeSystem
{
public:
    explicit TradeSystem(Player* bot);
    ~TradeSystem();
    TradeSystem(TradeSystem const&) = delete;
    TradeSystem& operator=(TradeSystem const&) = delete;

    // Core trade functionality
    bool InitiateTrade(Player* initiator, Player* target) override;
    void ProcessTradeRequest(uint32 sessionId, TradeDecision decision) override;
    void UpdateTradeSession(uint32 sessionId) override;
    void CompleteTradeSession(uint32 sessionId) override;
    void CancelTradeSession(uint32 sessionId) override;

    // Player-to-player trading
    bool CanInitiateTrade(Player* initiator, Player* target) override;
    TradeDecision EvaluateTradeRequest(uint32 sessionId) override;
    void AddItemToTrade(uint32 sessionId, uint32 itemGuid, uint32 count);
    void SetTradeGold(uint32 sessionId, uint32 goldAmount);

    // Vendor interactions using TrinityCore data
    void LoadVendorDatabase() override;
    std::vector<VendorInfo> FindNearbyVendors(float radius = 100.0f) override;
    VendorInfo GetVendorInfo(uint32 creatureGuid);
    bool InteractWithVendor(uint32 vendorGuid) override;

    // Vendor purchasing and selling
    void ProcessVendorBuy(uint32 vendorGuid, uint32 itemId, uint32 count) override;
    void ProcessVendorSell(uint32 vendorGuid, uint32 itemGuid, uint32 count) override;
    bool CanBuyFromVendor(uint32 vendorGuid, uint32 itemId) override;
    bool ShouldSellToVendor(uint32 itemGuid);

    // Equipment repair using TrinityCore repair vendors
    void AutoRepairEquipment() override;
    std::vector<uint32> FindRepairVendors(float radius = 200.0f) override;
    uint32 CalculateRepairCost();
    void ProcessEquipmentRepair(uint32 vendorGuid) override;

    // Innkeeper services using TrinityCore innkeeper data
    void InteractWithInnkeeper(uint32 innkeeperGuid) override;
    void SetHearthstone(uint32 innkeeperGuid);
    std::vector<uint32> FindNearbyInnkeepers(float radius = 150.0f) override;
    bool CanUseInnkeeperServices(uint32 innkeeperGuid);

    // Intelligent trade decision making
    float AnalyzeTradeValue(const TradeSession& session) override;
    bool IsTradeWorthwhile(const TradeSession& session) override;
    void GenerateTradeRecommendation(uint32 sessionId);
    TradeDecision MakeAutomatedTradeDecision(uint32 sessionId);

    // Trade safety and validation
    bool ValidateTradeSession(const TradeSession& session) override;
    bool DetectSuspiciousTradeActivity(const TradeSession& session) override;
    void LogTradeTransaction(const TradeSession& session);
    void HandleTradeScamAttempt(Player* victim, Player* scammer);

    // Performance monitoring
    struct TradeMetrics
    {
        std::atomic<uint32> tradesInitiated{0};
        std::atomic<uint32> tradesCompleted{0};
        std::atomic<uint32> tradesCancelled{0};
        std::atomic<uint32> vendorTransactions{0};
        std::atomic<uint32> repairTransactions{0};
        std::atomic<float> averageTradeValue{1000.0f};
        std::atomic<float> tradeSuccessRate{0.8f};
        std::atomic<uint32> totalGoldTraded{0};
        std::atomic<uint32> totalItemsTraded{0};
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            tradesInitiated = 0; tradesCompleted = 0; tradesCancelled = 0;
            vendorTransactions = 0; repairTransactions = 0; averageTradeValue = 1000.0f;
            tradeSuccessRate = 0.8f; totalGoldTraded = 0; totalItemsTraded = 0;
            lastUpdate = std::chrono::steady_clock::now();
        }

        float GetCompletionRate() const {
            uint32 initiated = tradesInitiated.load();
            uint32 completed = tradesCompleted.load();
            return initiated > 0 ? (float)completed / initiated : 0.0f;
        }
    };

    TradeMetrics GetPlayerTradeMetrics() override;
    TradeMetrics GetGlobalTradeMetrics() override;

    // Automated vendor management
    void AutoSellJunkItems() override;
    void AutoBuyConsumables() override;
    void AutoRepairWhenNeeded();
    void ManageInventorySpace();

    // Trade history and learning
    void RecordTradeHistory(const TradeSession& session);
    void AnalyzeTradePatterns();
    void LearnFromTradeOutcomes(uint32 sessionId, bool wasSuccessful);
    void AdaptTradingBehavior();

    // Guild and social integration
    void HandleGuildBankInteraction(uint32 guildBankGuid);
    void ProcessGuildBankDeposit(uint32 itemGuid, uint32 count);
    void ProcessGuildBankWithdrawal(uint32 itemId, uint32 count);
    bool CanAccessGuildBank();

    // Configuration and settings
    void SetTradeConfiguration(const TradeConfiguration& config) override;
    TradeConfiguration GetTradeConfiguration() override;
    void UpdatePlayerTrustLevel(uint32 targetGuid, float trustDelta);
    float GetPlayerTrustLevel(uint32 targetGuid);

    // Error handling and recovery
    void HandleTradeError(uint32 sessionId, const std::string& error);
    void RecoverFromTradeFailure(uint32 sessionId);
    void HandleTradeTimeout(uint32 sessionId);
    void ValidateTradeStates();

    // Update and maintenance
    void Update(uint32 diff) override;
    void ProcessActiveTrades() override;
    void CleanupExpiredTradeSessions() override;
    void RefreshVendorData();

private:
    Player* _bot;
    ~TradeSystem() = default;

    // Core data structures
    std::unordered_map<uint32, TradeSession> _activeTrades; // sessionId -> session
    std::unordered_map<uint32, TradeConfiguration> _playerConfigs; // playerGuid -> config
    std::unordered_map<uint32, TradeMetrics> _playerMetrics; // playerGuid -> metrics
    std::atomic<uint32> _nextSessionId{1};
    

    // Vendor database loaded from TrinityCore
    std::unordered_map<uint32, VendorInfo> _vendorDatabase; // creatureGuid -> vendor info
    std::unordered_map<uint32, std::vector<uint32>> _zoneVendors; // zoneId -> vendorGuids
    std::unordered_map<VendorType, std::vector<uint32>> _vendorsByType; // type -> vendorGuids
    

    // Trade history and learning
    struct PlayerTradeHistory
    {
        std::vector<uint32> completedTrades;
        std::unordered_map<uint32, uint32> tradePartnerCounts; // playerGuid -> trade count
        std::unordered_map<uint32, float> partnerTrustLevels; // playerGuid -> trust level
        std::vector<std::pair<uint32, bool>> recentTradeOutcomes; // sessionId, wasSuccessful
        uint32 totalTradeValue;
        uint32 lastTradeTime;

        PlayerTradeHistory() : totalTradeValue(0), lastTradeTime(0) {}
    };

    std::unordered_map<uint32, PlayerTradeHistory> _playerTradeHistory; // playerGuid -> history

    // Performance tracking
    TradeMetrics _globalMetrics;

    // Helper functions using TrinityCore systems
    void LoadVendorsFromCreatureDatabase();
    void LoadInnkeepersFromDatabase();
    void LoadRepairVendorsFromDatabase();
    void UpdateVendorAvailability();
    VendorType DetermineVendorType(const Creature* creature);

    // Trade session management
    void InitializeTradeSession(TradeSession& session);
    void ValidateTradeItems(TradeSession& session);
    void ExecuteTradeExchange(TradeSession& session);
    void NotifyTradeParticipants(const TradeSession& session, const std::string& message);

    // Vendor interaction implementations
    bool NavigateToVendor(uint32 vendorGuid);
    void ProcessVendorDialog(const VendorInfo& vendor);
    void AnalyzeVendorInventory(const VendorInfo& vendor);
    void OptimizeVendorTransactions();

    // Trade decision algorithms
    float CalculateItemTradeValue(uint32 itemGuid);
    float AssessTradeRisk(const TradeSession& session);
    bool MeetsTradeRequirements(const TradeSession& session);
    void GenerateCounterOffer(TradeSession& session);

    // Safety and anti-scam measures
    bool DetectItemDuplication(const TradeSession& session);
    bool DetectUnfairTrade(const TradeSession& session);
    bool ValidatePlayerReputations(const TradeSession& session);
    void UpdateScamDetectionDatabase(uint32 suspectedScammer);

    // Performance optimization
    void OptimizeVendorQueries();
    void CacheFrequentVendorData();
    void PreloadNearbyVendors();
    void UpdateTradeMetrics(const TradeSession& session, bool wasSuccessful);

    // Constants leveraging TrinityCore systems
    static constexpr uint32 TRADE_UPDATE_INTERVAL = 1000; // 1 second
    static constexpr uint32 TRADE_SESSION_TIMEOUT = 120000; // 2 minutes
    static constexpr uint32 MAX_ACTIVE_TRADES = 1000;
    static constexpr float VENDOR_SEARCH_RADIUS = 200.0f; // 200 yards
    static constexpr uint32 VENDOR_DATA_REFRESH_INTERVAL = 300000; // 5 minutes
    static constexpr float MIN_TRADE_VALUE = 1.0f; // 1 copper minimum
    static constexpr float MAX_TRADE_VALUE_RATIO = 10.0f; // Max 10:1 value ratio
    static constexpr uint32 TRADE_HISTORY_SIZE = 100;
    static constexpr float SCAM_DETECTION_THRESHOLD = 0.8f;
    static constexpr uint32 REPAIR_COST_THRESHOLD = 1000; // 10 silver
    static constexpr float TRUST_DECAY_RATE = 0.01f; // 1% per day
};

/**
 * Integration with TrinityCore systems:
 *
 * VENDOR DATA SOURCE:
 * - creature_template table for vendor flags and types
 * - npc_vendor table for vendor inventories
 * - creature table for vendor locations and spawns
 * - gossip_menu for vendor interaction options
 *
 * INNKEEPER DATA SOURCE:
 * - creature_template.npcflag & UNIT_NPC_FLAG_INNKEEPER
 * - Use existing innkeeper gossip and bind location systems
 *
 * REPAIR VENDOR DATA:
 * - creature_template.npcflag & UNIT_NPC_FLAG_REPAIR
 * - Leverage existing repair cost calculations
 *
 * TRADE VALIDATION:
 * - Use existing TrinityCore trade window system
 * - Integrate with item validation and anti-cheat systems
 * - Leverage existing player reputation systems
 */

} // namespace Playerbot