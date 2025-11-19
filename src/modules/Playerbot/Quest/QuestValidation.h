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
#include "QuestDef.h"
#include "QuestPickup.h"
#include "../Core/DI/Interfaces/IQuestValidation.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <atomic>
#include <mutex>

class Player;
class Quest;

namespace Playerbot
{

// Quest validation data structures
struct ValidationContext
{
    Player* bot;
    uint32 questId;
    const Quest* quest;
    uint32 validationTime;
    bool strictValidation;
    bool checkOptionalRequirements;
    bool validateFutureRequirements;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;

    ValidationContext(Player* p, uint32 qId) : bot(p), questId(qId), quest(nullptr)
        , validationTime(0), strictValidation(true)
        , checkOptionalRequirements(true), validateFutureRequirements(false) {}
};

struct ValidationResult
{
    bool isValid;
    QuestEligibility eligibility;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    uint32 validationTime;
    uint32 cacheExpiry;

    ValidationResult() : isValid(false), eligibility(QuestEligibility::NOT_AVAILABLE)
        , validationTime(0), cacheExpiry(60000) {} // 1 minute cache
};

/**
 * @brief Comprehensive quest validation system for playerbot quest acceptance
 *
 * This system validates all quest requirements, prerequisites, and constraints
 * before allowing bots to accept quests, ensuring proper quest progression
 * and preventing invalid quest states.
 */
class TC_GAME_API QuestValidation final : public IQuestValidation
{
public:
    explicit QuestValidation(Player* bot);
    ~QuestValidation();
    QuestValidation(QuestValidation const&) = delete;
    QuestValidation& operator=(QuestValidation const&) = delete;

    // Core validation methods
    bool ValidateQuestAcceptance(uint32 questId, Player* bot) override;
    QuestEligibility GetDetailedEligibility(uint32 questId, Player* bot) override;
    std::vector<std::string> GetValidationErrors(uint32 questId, Player* bot) override;
    bool CanQuestBeStarted(uint32 questId, Player* bot) override;

    // Requirement validation
    bool ValidateLevelRequirements(uint32 questId, Player* bot) override;
    bool ValidateClassRequirements(uint32 questId, Player* bot) override;
    bool ValidateRaceRequirements(uint32 questId, Player* bot) override;
    bool ValidateFactionRequirements(uint32 questId, Player* bot) override;
    bool ValidateSkillRequirements(uint32 questId, Player* bot) override;

    // Prerequisite validation
    bool ValidateQuestPrerequisites(uint32 questId, Player* bot) override;
    bool ValidateQuestChainPosition(uint32 questId, Player* bot) override;
    std::vector<uint32> GetMissingPrerequisites(uint32 questId, Player* bot) override;
    bool HasCompletedPrerequisiteQuests(uint32 questId, Player* bot) override;

    // Item and inventory validation
    bool ValidateRequiredItems(uint32 questId, Player* bot) override;
    bool ValidateInventorySpace(uint32 questId, Player* bot) override;
    bool ValidateQuestItemRequirements(uint32 questId, Player* bot) override;
    std::vector<std::pair<uint32, uint32>> GetMissingItems(uint32 questId, Player* bot) override; // itemId, count

    // Status and state validation
    bool ValidateQuestStatus(uint32 questId, Player* bot) override;
    bool IsQuestAlreadyCompleted(uint32 questId, Player* bot) override;
    bool IsQuestInProgress(uint32 questId, Player* bot) override;
    bool IsQuestLogFull(Player* bot) override;
    bool IsQuestRepeatable(uint32 questId, Player* bot) override;

    // Reputation and standing validation
    bool ValidateReputationRequirements(uint32 questId, Player* bot) override;
    bool ValidateMinimumReputation(uint32 questId, Player* bot) override;
    bool ValidateMaximumReputation(uint32 questId, Player* bot) override;
    std::vector<std::pair<uint32, int32>> GetReputationRequirements(uint32 questId) override; // factionId, standing

    // Time and availability validation
    bool ValidateQuestAvailability(uint32 questId, Player* bot) override;
    bool ValidateSeasonalAvailability(uint32 questId) override;
    bool ValidateDailyQuestLimits(uint32 questId, Player* bot) override;
    bool ValidateQuestTimer(uint32 questId, Player* bot) override;

    // Zone and location validation
    bool ValidateZoneRequirements(uint32 questId, Player* bot) override;
    bool ValidateAreaRequirements(uint32 questId, Player* bot) override;
    bool IsInCorrectZone(uint32 questId, Player* bot) override;
    bool CanQuestBeStartedAtLocation(uint32 questId, const Position& location) override;

    // Group and party validation
    bool ValidateGroupRequirements(uint32 questId, Player* bot) override;
    bool ValidatePartyQuestRequirements(uint32 questId, Player* bot) override;
    bool ValidateRaidQuestRequirements(uint32 questId, Player* bot) override;
    bool CanGroupMemberShareQuest(uint32 questId, Player* sharer, Player* receiver) override;

    // Advanced validation
    bool ValidateWithContext(ValidationContext& context) override;
    bool ValidateQuestObjectives(uint32 questId, Player* bot) override;
    bool ValidateQuestRewards(uint32 questId, Player* bot) override;
    bool ValidateQuestDifficulty(uint32 questId, Player* bot) override;

    // Validation caching and optimization
    ValidationResult GetCachedValidation(uint32 questId, uint32 botGuid) override;
    void CacheValidationResult(uint32 questId, uint32 botGuid, const ValidationResult& result) override;
    void InvalidateValidationCache(uint32 botGuid) override;
    void CleanupExpiredCache() override;

    // Batch validation for efficiency
    std::unordered_map<uint32, ValidationResult> ValidateMultipleQuests(
        const std::vector<uint32>& questIds, Player* bot) override;
    std::vector<uint32> FilterValidQuests(const std::vector<uint32>& questIds, Player* bot) override;
    std::vector<uint32> GetEligibleQuests(Player* bot, const std::vector<uint32>& candidates) override;

    // Error reporting and diagnostics
    std::string GetDetailedValidationReport(uint32 questId, Player* bot) override;
    void LogValidationFailure(uint32 questId, Player* bot, const std::string& reason) override;
    std::vector<std::string> GetRecommendationsForFailedQuest(uint32 questId, Player* bot) override;

    // Configuration and settings
    void SetStrictValidation(bool strict) { _strictValidation = strict; }
    void SetValidationCaching(bool enabled) { _enableCaching = enabled; }
    void SetCacheTimeout(uint32 timeoutMs) { _cacheTimeoutMs = timeoutMs; }

    // Performance monitoring
    struct ValidationMetrics
    {
        std::atomic<uint32> totalValidations{0};
        std::atomic<uint32> passedValidations{0};
        std::atomic<uint32> failedValidations{0};
        std::atomic<uint32> cacheHits{0};
        std::atomic<uint32> cacheMisses{0};
        std::atomic<float> averageValidationTime{5.0f};
        std::atomic<float> validationSuccessRate{0.85f};

        // Default constructor
        ValidationMetrics() = default;

        // Copy constructor for atomic members
        ValidationMetrics(const ValidationMetrics& other) :
            totalValidations(other.totalValidations.load()),
            passedValidations(other.passedValidations.load()),
            failedValidations(other.failedValidations.load()),
            cacheHits(other.cacheHits.load()),
            cacheMisses(other.cacheMisses.load()),
            averageValidationTime(other.averageValidationTime.load()),
            validationSuccessRate(other.validationSuccessRate.load()) {}

        // Assignment operator for atomic members
        ValidationMetrics& operator=(const ValidationMetrics& other) {
            if (this != &other) {
                totalValidations.store(other.totalValidations.load());
                passedValidations.store(other.passedValidations.load());
                failedValidations.store(other.failedValidations.load());
                cacheHits.store(other.cacheHits.load());
                cacheMisses.store(other.cacheMisses.load());
                averageValidationTime.store(other.averageValidationTime.load());
                validationSuccessRate.store(other.validationSuccessRate.load());
            }
            return *this;
        }

        void Reset() {
            totalValidations = 0; passedValidations = 0; failedValidations = 0;
            cacheHits = 0; cacheMisses = 0; averageValidationTime = 5.0f;
            validationSuccessRate = 0.85f;
        }

        float GetCacheHitRate() const {
            uint32 total = cacheHits.load() + cacheMisses.load();
            return total > 0 ? (float)cacheHits.load() / total : 0.0f;
        }
    };

    ValidationMetrics GetValidationMetrics() const override { return _metrics; }

    // Update and maintenance
    void Update(uint32 diff) override;

private:
    Player* _bot;
    ~QuestValidation() = default;

    // Validation cache
    std::unordered_map<uint64, ValidationResult> _validationCache; // (questId << 32 | botGuid) -> result
    

    // Configuration
    std::atomic<bool> _strictValidation{true};
    std::atomic<bool> _enableCaching{true};
    std::atomic<uint32> _cacheTimeoutMs{60000}; // 1 minute

    // Performance tracking
    ValidationMetrics _metrics;

    // Helper methods
    bool ValidateBasicRequirements(const Quest* quest, Player* bot);
    bool ValidateAdvancedRequirements(const Quest* quest, Player* bot);
    bool ValidateQuestFlags(const Quest* quest, Player* bot);
    bool ValidateSpecialRequirements(const Quest* quest, Player* bot);
    uint64 GenerateCacheKey(uint32 questId, uint32 botGuid);
    void UpdateValidationMetrics(bool wasSuccessful, uint32 validationTime);

    // Specific requirement checkers
    bool CheckMinLevel(const Quest* quest, Player* bot);
    bool CheckMaxLevel(const Quest* quest, Player* bot);
    bool CheckRequiredClass(const Quest* quest, Player* bot);
    bool CheckRequiredRace(const Quest* quest, Player* bot);
    bool CheckRequiredSkill(const Quest* quest, Player* bot);
    bool CheckRequiredFaction(const Quest* quest, Player* bot);
    bool CheckPreviousQuest(const Quest* quest, Player* bot);
    bool CheckExclusiveGroup(const Quest* quest, Player* bot);
    bool CheckBreadcrumbQuest(const Quest* quest, Player* bot);

    // Constants
    static constexpr uint32 DEFAULT_CACHE_TIMEOUT = 60000; // 1 minute
    static constexpr uint32 MAX_CACHE_SIZE = 10000;
    static constexpr uint32 CACHE_CLEANUP_INTERVAL = 300000; // 5 minutes
    static constexpr float VALIDATION_TIME_WARNING_THRESHOLD = 50.0f; // 50ms
};

/**
 * @brief Quest requirement analyzer for detailed validation feedback
 *
 * Provides comprehensive analysis of why a quest cannot be accepted
 * and what the bot needs to do to become eligible.
 */
class TC_GAME_API QuestRequirementAnalyzer
{
public:
    struct RequirementIssue
    {
        enum Type
        {
            LEVEL_TOO_LOW,
            LEVEL_TOO_HIGH,
            MISSING_CLASS,
            MISSING_RACE,
            MISSING_FACTION_REP,
            MISSING_SKILL,
            MISSING_ITEM,
            MISSING_PREREQUISITE_QUEST,
            ALREADY_COMPLETED,
            QUEST_LOG_FULL,
            WRONG_ZONE,
            SEASONAL_UNAVAILABLE,
            DAILY_LIMIT_REACHED,
            GROUP_REQUIRED,
            SPECIAL_REQUIREMENT
        };

        Type type;
        std::string description;
        std::string solution;
        uint32 requiredValue;
        uint32 currentValue;
        std::vector<uint32> relatedIds; // quest IDs, item IDs, faction IDs, etc.
        bool isBlocker; // true if this prevents quest acceptance
        uint32 estimatedTimeToResolve; // seconds

        RequirementIssue(Type t, const std::string& desc, bool blocker = true)
            : type(t), description(desc), isBlocker(blocker), estimatedTimeToResolve(0) {}
    };

    static std::vector<RequirementIssue> AnalyzeQuestRequirements(uint32 questId, Player* bot);
    static std::string GenerateRequirementReport(uint32 questId, Player* bot);
    static std::vector<std::string> GetActionableRecommendations(uint32 questId, Player* bot);
    static uint32 EstimateTimeToEligibility(uint32 questId, Player* bot);

private:
    static void AnalyzeLevelRequirements(const Quest* quest, Player* bot, std::vector<RequirementIssue>& issues);
    static void AnalyzeClassRaceRequirements(const Quest* quest, Player* bot, std::vector<RequirementIssue>& issues);
    static void AnalyzeSkillRequirements(const Quest* quest, Player* bot, std::vector<RequirementIssue>& issues);
    static void AnalyzeReputationRequirements(const Quest* quest, Player* bot, std::vector<RequirementIssue>& issues);
    static void AnalyzeItemRequirements(const Quest* quest, Player* bot, std::vector<RequirementIssue>& issues);
    static void AnalyzePrerequisiteRequirements(const Quest* quest, Player* bot, std::vector<RequirementIssue>& issues);
    static void AnalyzeAvailabilityRequirements(const Quest* quest, Player* bot, std::vector<RequirementIssue>& issues);
};

} // namespace Playerbot