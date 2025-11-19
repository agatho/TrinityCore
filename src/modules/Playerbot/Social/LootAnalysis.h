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
#include "LootDistribution.h"
#include "Player.h"
#include "Item.h"
#include "Core/DI/Interfaces/ILootAnalysis.h"
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>
#include "GameTime.h"

namespace Playerbot
{

/**
 * @brief Advanced loot analysis system for intelligent item evaluation
 *
 * This system provides comprehensive analysis of loot items to determine
 * their value, upgrade potential, and appropriateness for each player.
 */
class TC_GAME_API LootAnalysis final : public ILootAnalysis
{
public:
    static LootAnalysis* instance();

    // Core item analysis
    float CalculateItemValue(Player* player, const LootItem& item) override;
    float CalculateUpgradeValue(Player* player, const LootItem& item) override;
    bool IsSignificantUpgrade(Player* player, const LootItem& item) override;
    float CalculateStatWeight(Player* player, uint32 statType) override;

    // Item comparison and evaluation
    float CompareItems(Player* player, const LootItem& newItem, const Item* currentItem) override;
    float CalculateItemScore(Player* player, const LootItem& item) override;
    std::vector<std::pair<uint32, float>> GetStatPriorities(Player* player) override;
    float GetItemLevelWeight(Player* player, uint32 itemLevel) override;

    // Class and spec specific analysis
    float CalculateArmorValue(Player* player, const LootItem& item);
    float CalculateWeaponValue(Player* player, const LootItem& item);
    float CalculateAccessoryValue(Player* player, const LootItem& item);
    bool IsItemForMainSpec(Player* player, const LootItem& item);
    bool IsItemForOffSpec(Player* player, const LootItem& item);

    // Advanced analysis features
    struct ItemAnalysisResult
    {
        float overallScore;
        float upgradeValue;
        float statScore;
        float itemLevelScore;
        float classAppropriatenessScore;
        LootPriority recommendedPriority;
        LootRollType recommendedAction;
        std::vector<std::string> analysisNotes;
        bool isSignificantUpgrade;
        bool isMainSpecItem;
        bool isOffSpecItem;

        ItemAnalysisResult() : overallScore(0.0f), upgradeValue(0.0f), statScore(0.0f)
            , itemLevelScore(0.0f), classAppropriatenessScore(0.0f)
            , recommendedPriority(LootPriority::NOT_USEFUL)
            , recommendedAction(LootRollType::PASS), isSignificantUpgrade(false)
            , isMainSpecItem(false), isOffSpecItem(false) {}
    };

    ItemAnalysisResult AnalyzeItemForPlayer(Player* player, const LootItem& item);
    void CacheAnalysisResult(Player* player, const LootItem& item, const ItemAnalysisResult& result);
    ItemAnalysisResult GetCachedAnalysis(Player* player, uint32 itemId);

    // Stat weight calculation
    struct StatWeights
    {
        float strength;
        float agility;
        float stamina;
        float intellect;
        float spirit;
        float attackPower;
        float spellPower;
        float criticalStrike;
        float haste;
        float mastery;
        float versatility;
        float dodge;
        float parry;
        float block;
        float armor;
        float expertise;
        float hit;

        StatWeights() : strength(0.0f), agility(0.0f), stamina(0.0f), intellect(0.0f)
            , spirit(0.0f), attackPower(0.0f), spellPower(0.0f), criticalStrike(0.0f)
            , haste(0.0f), mastery(0.0f), versatility(0.0f), dodge(0.0f), parry(0.0f)
            , block(0.0f), armor(0.0f), expertise(0.0f), hit(0.0f) {}
    };

    StatWeights GetClassSpecStatWeights(uint8 playerClass, uint8 playerSpec);
    void UpdateStatWeights(uint8 playerClass, uint8 playerSpec, const StatWeights& weights);
    float CalculateWeightedStatValue(const StatWeights& weights, const LootItem& item);

    // Equipment slot analysis
    bool CanEquipItem(Player* player, const LootItem& item) override;
    uint32 GetEquipmentSlot(const LootItem& item) override;
    Item* GetCurrentEquippedItem(Player* player, uint32 slot) override;
    std::vector<uint32> GetAffectedSlots(const LootItem& item);

    // Market and vendor value analysis
    float CalculateVendorValue(const LootItem& item) override;
    float CalculateAuctionHouseValue(const LootItem& item) override;
    float CalculateDisenchantValue(const LootItem& item) override;
    bool IsValuableForVendoring(const LootItem& item) override;

    // Group context analysis
    void AnalyzeGroupLootNeeds(Group* group, const LootItem& item) override;
    std::vector<std::pair<uint32, float>> RankPlayersForItem(Group* group, const LootItem& item) override;
    bool IsItemContestedInGroup(Group* group, const LootItem& item) override;
    Player* GetBestCandidateForItem(Group* group, const LootItem& item) override;

    // Learning and adaptation
    void UpdateAnalysisAccuracy(Player* player, const LootItem& item, LootRollType actualDecision);
    void LearnFromPlayerChoices(Player* player, const std::vector<std::pair<LootItem, LootRollType>>& choices);
    void AdaptAnalysisForPlayer(Player* player);

    // Performance monitoring
    struct AnalysisMetrics
    {
        std::atomic<uint32> itemsAnalyzed{0};
        std::atomic<uint32> analysisRequests{0};
        std::atomic<uint32> cacheHits{0};
        std::atomic<uint32> cacheMisses{0};
        std::atomic<float> averageAnalysisTime{5.0f}; // milliseconds
        std::atomic<float> analysisAccuracy{0.85f};
        std::atomic<float> predictionAccuracy{0.8f};

        void Reset() {
            itemsAnalyzed = 0; analysisRequests = 0; cacheHits = 0; cacheMisses = 0;
            averageAnalysisTime = 5.0f; analysisAccuracy = 0.85f; predictionAccuracy = 0.8f;
        }

        float GetCacheHitRate() const {
            uint32 total = cacheHits.load() + cacheMisses.load();
            return total > 0 ? (float)cacheHits.load() / total : 0.0f;
        }
    };

    AnalysisMetrics GetAnalysisMetrics() const { return _metrics; }

    // Configuration and settings
    void SetAnalysisPrecision(float precision); // 0.0 = fast, 1.0 = thorough
    void EnableLearning(bool enable) { _learningEnabled = enable; }
    void SetCacheSize(uint32 size) { _maxCacheSize = size; }

    // Update and maintenance
    void Update(uint32 diff);
    void CleanupCache();

private:
    LootAnalysis();
    ~LootAnalysis() = default;

    // Analysis cache
    std::unordered_map<uint64, ItemAnalysisResult> _analysisCache; // (playerGuid << 32 | itemId) -> result
    std::unordered_map<uint32, std::unordered_map<uint32, float>> _statWeightCache; // playerGuid -> statType -> weight
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::LOOT_MANAGER> _cacheMutex;

    // Stat weights database
    std::unordered_map<uint8, std::unordered_map<uint8, StatWeights>> _classSpecStatWeights; // class -> spec -> weights
    std::unordered_map<uint32, StatWeights> _playerCustomWeights; // playerGuid -> custom weights

    // Learning system
    struct PlayerLearningData
    {
        std::unordered_map<uint32, std::vector<std::pair<LootRollType, float>>> itemDecisionHistory; // itemId -> decisions
        std::unordered_map<uint32, float> statPreferenceLearning; // statType -> preference weight
        uint32 totalDecisions;
        uint32 correctPredictions;
        uint32 lastLearningUpdate;

        PlayerLearningData() : totalDecisions(0), correctPredictions(0), lastLearningUpdate(GameTime::GetGameTimeMS()) {}
    };

    std::unordered_map<uint32, PlayerLearningData> _playerLearningData; // playerGuid -> learning data
    std::atomic<bool> _learningEnabled{true};

    // Configuration
    std::atomic<float> _analysisPrecision{0.8f};
    std::atomic<uint32> _maxCacheSize{10000};
    std::atomic<uint32> _cacheTimeoutMs{300000}; // 5 minutes

    // Performance tracking
    AnalysisMetrics _metrics;

    // Helper functions
    void InitializeStatWeights();
    void LoadClassSpecificWeights();
    uint64 GenerateCacheKey(uint32 playerGuid, uint32 itemId);
    void UpdateAnalysisMetrics(bool wasCacheHit, uint32 analysisTime);

    // Item analysis implementations
    float AnalyzeArmorItem(Player* player, const LootItem& item);
    float AnalyzeWeaponItem(Player* player, const LootItem& item);
    float AnalyzeTrinketItem(Player* player, const LootItem& item);
    float AnalyzeConsumableItem(Player* player, const LootItem& item);

    // Stat analysis helpers
    float GetStatValueOnItem(const LootItem& item, uint32 statType);
    float CalculateStatBudgetUsed(const LootItem& item);
    float GetItemLevelBudget(uint32 itemLevel, uint32 slot);
    float CalculateStatEfficiency(const StatWeights& weights, const LootItem& item);

    // Comparison algorithms
    float CompareArmorItems(Player* player, const LootItem& newItem, const Item* currentItem);
    float CompareWeaponItems(Player* player, const LootItem& newItem, const Item* currentItem);
    float CompareAccessoryItems(Player* player, const LootItem& newItem, const Item* currentItem);

    // Learning algorithm implementations
    void UpdateStatPreferences(Player* player, const LootItem& item, LootRollType decision);
    void AdaptWeightsBasedOnChoices(Player* player);
    void PredictPlayerPreference(Player* player, const LootItem& item, float& needProbability, float& greedProbability);

    // Constants
    static constexpr float MIN_UPGRADE_THRESHOLD = 0.05f; // 5% improvement
    static constexpr float SIGNIFICANT_UPGRADE_THRESHOLD = 0.15f; // 15% improvement
    static constexpr uint32 ANALYSIS_CACHE_CLEANUP_INTERVAL = 300000; // 5 minutes
    static constexpr uint32 LEARNING_UPDATE_INTERVAL = 600000; // 10 minutes
    static constexpr float DEFAULT_ANALYSIS_PRECISION = 0.8f;
    static constexpr uint32 MIN_DECISIONS_FOR_LEARNING = 10;
    static constexpr float LEARNING_RATE = 0.1f;
    static constexpr uint32 MAX_DECISION_HISTORY = 100;
};

} // namespace Playerbot