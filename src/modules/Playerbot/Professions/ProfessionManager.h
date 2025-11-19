/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * COMPLETE PROFESSION MANAGEMENT SYSTEM FOR PLAYERBOT
 *
 * This system provides comprehensive profession automation for all 14 WoW professions:
 * - Auto-learn professions based on class
 * - Auto-level professions (1-450 skill)
 * - Crafting automation with optimal recipe selection
 * - Gathering automation (Mining, Herbalism, Skinning)
 * - Recipe learning and discovery
 * - Integration with TradeAutomation and EquipmentManager
 *
 * Architecture:
 * - ProfessionManager: Core singleton managing all profession operations
 * - CraftingAutomation: Automated crafting queues and material management
 * - GatheringAutomation: Resource node detection and harvesting
 */

#pragma once

#include "Define.h"
#include "Threading/LockHierarchy.h"
#include "Player.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "../Core/DI/Interfaces/IProfessionManager.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace Playerbot
{

/**
 * @brief Profession types matching WoW 11.2 skill IDs
 */
enum class ProfessionType : uint16
{
    NONE = 0,

    // Primary Professions (Production)
    ALCHEMY = SKILL_ALCHEMY,                    // 171
    BLACKSMITHING = SKILL_BLACKSMITHING,        // 164
    ENCHANTING = SKILL_ENCHANTING,              // 333
    ENGINEERING = SKILL_ENGINEERING,            // 202
    INSCRIPTION = SKILL_INSCRIPTION,            // 773
    JEWELCRAFTING = SKILL_JEWELCRAFTING,        // 755
    LEATHERWORKING = SKILL_LEATHERWORKING,      // 165
    TAILORING = SKILL_TAILORING,                // 197

    // Primary Professions (Gathering)
    MINING = SKILL_MINING,                      // 186
    HERBALISM = SKILL_HERBALISM,                // 182
    SKINNING = SKILL_SKINNING,                  // 393

    // Secondary Professions
    COOKING = SKILL_COOKING,                    // 185
    FISHING = SKILL_FISHING,                    // 356
    FIRST_AID = 129                             // First Aid (if still in 11.2)
};

/**
 * @brief Profession categories for automation logic
 */
enum class ProfessionCategory : uint8
{
    PRODUCTION,     // Alchemy, Blacksmithing, Enchanting, etc.
    GATHERING,      // Mining, Herbalism, Skinning
    SECONDARY       // Cooking, Fishing, First Aid
};

/**
 * @brief Recipe information for crafting
 */
struct RecipeInfo
{
    uint32 recipeId;
    uint32 spellId;
    ProfessionType profession;
    uint16 requiredSkill;
    uint16 skillUpOrange;    // Always skill up
    uint16 skillUpYellow;    // Often skill up
    uint16 skillUpGreen;     // Rarely skill up
    uint16 skillUpGray;      // Never skill up

    struct Reagent
    {
        uint32 itemId;
        uint32 quantity;
    };
    std::vector<Reagent> reagents;

    uint32 productItemId;
    uint32 productQuantity;
    uint32 createdItemType;  // For disenchant/prospect/mill products

    bool isDiscovery;        // Learned from discovery
    bool isTrainer;          // Learned from trainer
    bool isWorldDrop;        // Learned from world drop

    RecipeInfo() : recipeId(0), spellId(0), profession(ProfessionType::NONE),
        requiredSkill(0), skillUpOrange(0), skillUpYellow(0), skillUpGreen(0), skillUpGray(0),
        productItemId(0), productQuantity(1), createdItemType(0),
        isDiscovery(false), isTrainer(true), isWorldDrop(false) {}
};

/**
 * @brief Profession skill tracking
 */
struct ProfessionSkillInfo
{
    ProfessionType profession;
    uint16 currentSkill;
    uint16 maxSkill;
    uint32 lastUpdate;
    bool isPrimary;          // Primary vs secondary profession

    ProfessionSkillInfo() : profession(ProfessionType::NONE), currentSkill(0),
        maxSkill(0), lastUpdate(0), isPrimary(true) {}
};

/**
 * @brief Profession automation configuration per bot
 */
struct ProfessionAutomationProfile
{
    bool autoLearnProfessions = true;      // Auto-learn professions based on class
    bool autoLevelProfessions = true;      // Auto-level through crafting
    bool preferGathering = false;          // Prioritize gathering over crafting
    bool craftForSelf = true;              // Craft gear/consumables for self
    bool craftForProfit = true;            // Craft items to sell
    float skillUpThreshold = 0.8f;         // Min skill-up chance (0.0-1.0)
    uint32 maxCraftingBudget = 50000;      // Max gold to spend on materials (copper)
    bool learnAllRecipes = false;          // Learn all available recipes vs optimal only

    // Class-specific profession preferences
    std::vector<ProfessionType> preferredProfessions;
    std::unordered_set<uint32> neverCraftItems;

    ProfessionAutomationProfile() = default;
};

/**
 * @brief Complete profession manager for all bot profession operations
 */
class TC_GAME_API ProfessionManager final : public IProfessionManager
{
public:
    static ProfessionManager* instance();

    // ============================================================================
    // CORE PROFESSION MANAGEMENT
    // ============================================================================

    /**
     * Initialize profession system on server startup
     */
    void Initialize() override;

    /**
     * Update profession automation for player (called periodically)
     */
    void Update(::Player* player, uint32 diff) override;

    /**
     * Learn profession for player
     */
    bool LearnProfession(::Player* player, ProfessionType profession) override;

    /**
     * Check if player has profession
     */
    bool HasProfession(::Player* player, ProfessionType profession) const override;

    /**
     * Get profession skill level
     */
    uint16 GetProfessionSkill(::Player* player, ProfessionType profession) const override;

    /**
     * Get max profession skill
     */
    uint16 GetMaxProfessionSkill(::Player* player, ProfessionType profession) const override;

    /**
     * Get all professions for player
     */
    std::vector<ProfessionSkillInfo> GetPlayerProfessions(::Player* player) const override;

    /**
     * Unlearn profession (for respec)
     */
    bool UnlearnProfession(::Player* player, ProfessionType profession) override;

    // ============================================================================
    // AUTO-LEARN SYSTEM
    // ============================================================================

    /**
     * Auto-learn professions based on class
     * Called when bot is created or profession slots are empty
     */
    void AutoLearnProfessionsForClass(::Player* player) override;

    /**
     * Get recommended professions for class
     */
    std::vector<ProfessionType> GetRecommendedProfessions(uint8 classId) const override;

    /**
     * Check if profession is suitable for class
     */
    bool IsProfessionSuitableForClass(uint8 classId, ProfessionType profession) const override;

    /**
     * Get profession category
     */
    ProfessionCategory GetProfessionCategory(ProfessionType profession) const override;

    /**
     * Get beneficial profession pair for a given profession
     * Example: Mining → Blacksmithing/Engineering/Jewelcrafting
     */
    std::vector<ProfessionType> GetBeneficialPairs(ProfessionType profession) const override;

    /**
     * Check if two professions form a beneficial pair
     */
    bool IsBeneficialPair(ProfessionType prof1, ProfessionType prof2) const override;

    /**
     * Get race-specific profession skill bonus
     * Example: Tauren +15 Herbalism, Blood Elf +10 Enchanting
     */
    uint16 GetRaceProfessionBonus(uint8 raceId, ProfessionType profession) const override;

    // ============================================================================
    // RECIPE MANAGEMENT
    // ============================================================================

    /**
     * Learn recipe for player
     */
    bool LearnRecipe(::Player* player, uint32 recipeId) override;

    /**
     * Check if player knows recipe
     */
    bool KnowsRecipe(::Player* player, uint32 recipeId) const override;

    /**
     * Get all recipes for profession
     */
    std::vector<RecipeInfo> GetRecipesForProfession(ProfessionType profession) const override;

    /**
     * Get craftable recipes for player (has skill + reagents)
     */
    std::vector<RecipeInfo> GetCraftableRecipes(::Player* player, ProfessionType profession) const override;

    /**
     * Get optimal recipe for leveling (highest skill-up chance)
     */
    RecipeInfo const* GetOptimalLevelingRecipe(::Player* player, ProfessionType profession) const override;

    /**
     * Check if player can craft recipe
     */
    bool CanCraftRecipe(::Player* player, RecipeInfo const& recipe) const override;

    /**
     * Get skill-up probability for recipe (0.0-1.0)
     */
    float GetSkillUpChance(::Player* player, RecipeInfo const& recipe) const override;

    // ============================================================================
    // CRAFTING AUTOMATION
    // ============================================================================

    /**
     * Auto-level profession by crafting optimal recipes
     */
    bool AutoLevelProfession(::Player* player, ProfessionType profession) override;

    /**
     * Craft item (single craft)
     */
    bool CraftItem(::Player* player, RecipeInfo const& recipe, uint32 quantity = 1) override;

    /**
     * Queue crafting task for automation
     */
    void QueueCraft(::Player* player, uint32 recipeId, uint32 quantity) override;

    /**
     * Process crafting queue (called in Update)
     */
    void ProcessCraftingQueue(::Player* player, uint32 diff) override;

    /**
     * Check if player has materials for recipe
     */
    bool HasMaterialsForRecipe(::Player* player, RecipeInfo const& recipe) const override;

    /**
     * Get missing materials for recipe
     */
    std::vector<std::pair<uint32, uint32>> GetMissingMaterials(::Player* player, RecipeInfo const& recipe) const override;

    // ============================================================================
    // AUTOMATION PROFILES
    // ============================================================================

    void SetAutomationProfile(uint32 playerGuid, ProfessionAutomationProfile const& profile) override;
    ProfessionAutomationProfile GetAutomationProfile(uint32 playerGuid) const override;

    // ============================================================================
    // METRICS
    // ============================================================================

    struct ProfessionMetrics
    {
        std::atomic<uint32> professionsLearned{0};
        std::atomic<uint32> recipesLearned{0};
        std::atomic<uint32> itemsCrafted{0};
        std::atomic<uint32> resourcesGathered{0};
        std::atomic<uint32> skillPointsGained{0};
        std::atomic<uint32> goldSpentOnMaterials{0};
        std::atomic<uint32> goldEarnedFromCrafts{0};

        void Reset()
        {
            professionsLearned = 0;
            recipesLearned = 0;
            itemsCrafted = 0;
            resourcesGathered = 0;
            skillPointsGained = 0;
            goldSpentOnMaterials = 0;
            goldEarnedFromCrafts = 0;
        }
    };

    ProfessionMetrics const& GetPlayerMetrics(uint32 playerGuid) const override;
    ProfessionMetrics const& GetGlobalMetrics() const override;

private:
    ProfessionManager();
    ~ProfessionManager() = default;

    // ============================================================================
    // INITIALIZATION HELPERS
    // ============================================================================

    void LoadRecipeDatabase();
    void LoadProfessionRecommendations();
    void InitializeClassProfessions();
    void InitializeProfessionPairs();
    void InitializeRaceBonuses();

    // Profession recommendation tables (class -> professions)
    void InitializeWarriorProfessions();
    void InitializePaladinProfessions();
    void InitializeHunterProfessions();
    void InitializeRogueProfessions();
    void InitializePriestProfessions();
    void InitializeShamanProfessions();
    void InitializeMageProfessions();
    void InitializeWarlockProfessions();
    void InitializeDruidProfessions();
    void InitializeDeathKnightProfessions();
    void InitializeMonkProfessions();
    void InitializeDemonHunterProfessions();
    void InitializeEvokerProfessions();

    // ============================================================================
    // CRAFTING HELPERS
    // ============================================================================

    bool CastCraftingSpell(::Player* player, RecipeInfo const& recipe);
    bool ConsumeMaterials(::Player* player, RecipeInfo const& recipe);
    void HandleCraftingResult(::Player* player, RecipeInfo const& recipe, bool success);
    ProfessionType GetProfessionTypeFromSkillId(uint16 skillId) const;

    // ============================================================================
    // SKILL CALCULATION HELPERS
    // ============================================================================

    uint16 CalculateSkillUpAmount(RecipeInfo const& recipe, uint16 currentSkill) const;
    bool ShouldCraftForSkillUp(::Player* player, RecipeInfo const& recipe) const;

    // ============================================================================
    // DATA STRUCTURES
    // ============================================================================

    // Recipe database (recipeId -> RecipeInfo)
    std::unordered_map<uint32, RecipeInfo> _recipeDatabase;

    // Profession recipes (profession -> vector of recipeIds)
    std::unordered_map<ProfessionType, std::vector<uint32>> _professionRecipes;

    // Class profession recommendations (classId -> preferred professions)
    std::unordered_map<uint8, std::vector<ProfessionType>> _classRecommendations;

    // Beneficial profession pairs (profession -> synergistic partners)
    std::unordered_map<ProfessionType, std::vector<ProfessionType>> _professionPairs;

    // Race profession bonuses (raceId -> (profession -> bonus))
    std::unordered_map<uint8, std::unordered_map<ProfessionType, uint16>> _raceBonuses;

    // Player automation profiles
    std::unordered_map<uint32, ProfessionAutomationProfile> _playerProfiles;

    // Crafting queues (playerGuid -> queue of recipe IDs)
    struct CraftingTask
    {
        uint32 recipeId;
        uint32 quantity;
        uint32 queueTime;
    };
    std::unordered_map<uint32, std::vector<CraftingTask>> _craftingQueues;

    // Metrics
    std::unordered_map<uint32, ProfessionMetrics> _playerMetrics;
    ProfessionMetrics _globalMetrics;

    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::PROFESSION_MANAGER> _mutex;

    // Update intervals
    static constexpr uint32 PROFESSION_UPDATE_INTERVAL = 5000;  // 5 seconds
    static constexpr uint32 CRAFTING_CAST_TIME = 3000;          // 3 seconds per craft
    std::unordered_map<uint32, uint32> _lastUpdateTimes;
};

} // namespace Playerbot
