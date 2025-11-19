/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * Phase 1B Refactoring (2025-11-18):
 * - Updated interface for per-bot ProfessionManager instances
 * - Removed Player* parameters (now implicit via constructor)
 * - Removed playerGuid parameters (now per-bot instance)
 */

#pragma once

#include "Define.h"
#include "Player.h"
#include <vector>

namespace Playerbot
{

// Forward declarations
enum class ProfessionType : uint16;
enum class ProfessionCategory : uint8;
struct RecipeInfo;
struct ProfessionSkillInfo;
struct ProfessionAutomationProfile;
struct ProfessionMetrics;

/**
 * @brief Interface for per-bot profession management
 *
 * Phase 1B: Converted to per-bot instance pattern
 * - Each bot has its own ProfessionManager instance
 * - Player* passed to constructor, not to methods
 */
class TC_GAME_API IProfessionManager
{
public:
    virtual ~IProfessionManager() = default;

    // Core profession management (per-bot operations)
    virtual void Initialize() = 0;
    virtual void Update(uint32 diff) = 0;
    virtual bool LearnProfession(ProfessionType profession) = 0;
    virtual bool HasProfession(ProfessionType profession) const = 0;
    virtual uint16 GetProfessionSkill(ProfessionType profession) const = 0;
    virtual uint16 GetMaxProfessionSkill(ProfessionType profession) const = 0;
    virtual std::vector<ProfessionSkillInfo> GetPlayerProfessions() const = 0;
    virtual bool UnlearnProfession(ProfessionType profession) = 0;

    // Auto-learn system
    virtual void AutoLearnProfessionsForClass() = 0;

    // Shared data queries (delegate to ProfessionDatabase)
    virtual std::vector<ProfessionType> GetRecommendedProfessions(uint8 classId) const = 0;
    virtual bool IsProfessionSuitableForClass(uint8 classId, ProfessionType profession) const = 0;
    virtual ProfessionCategory GetProfessionCategory(ProfessionType profession) const = 0;
    virtual std::vector<ProfessionType> GetBeneficialPairs(ProfessionType profession) const = 0;
    virtual bool IsBeneficialPair(ProfessionType prof1, ProfessionType prof2) const = 0;
    virtual uint16 GetRaceProfessionBonus(uint8 raceId, ProfessionType profession) const = 0;

    // Recipe management (per-bot operations)
    virtual bool LearnRecipe(uint32 recipeId) = 0;
    virtual bool KnowsRecipe(uint32 recipeId) const = 0;

    // Recipe queries (delegate to ProfessionDatabase)
    virtual std::vector<RecipeInfo> GetRecipesForProfession(ProfessionType profession) const = 0;

    // Crafting queries (per-bot operations)
    virtual std::vector<RecipeInfo> GetCraftableRecipes(ProfessionType profession) const = 0;
    virtual RecipeInfo const* GetOptimalLevelingRecipe(ProfessionType profession) const = 0;
    virtual bool CanCraftRecipe(RecipeInfo const& recipe) const = 0;
    virtual float GetSkillUpChance(RecipeInfo const& recipe) const = 0;

    // Crafting automation (per-bot operations)
    virtual bool AutoLevelProfession(ProfessionType profession) = 0;
    virtual bool CraftItem(RecipeInfo const& recipe, uint32 quantity = 1) = 0;
    virtual void QueueCraft(uint32 recipeId, uint32 quantity) = 0;
    virtual void ProcessCraftingQueue(uint32 diff) = 0;
    virtual bool HasMaterialsForRecipe(RecipeInfo const& recipe) const = 0;
    virtual std::vector<std::pair<uint32, uint32>> GetMissingMaterials(RecipeInfo const& recipe) const = 0;

    // Automation profiles (per-bot)
    virtual void SetAutomationProfile(ProfessionAutomationProfile const& profile) = 0;
    virtual ProfessionAutomationProfile GetAutomationProfile() const = 0;

    // Metrics
    virtual ProfessionMetrics const& GetMetrics() const = 0;
    virtual ProfessionMetrics const& GetGlobalMetrics() const = 0;
};

} // namespace Playerbot
