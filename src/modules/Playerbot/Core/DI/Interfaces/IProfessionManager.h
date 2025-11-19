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

namespace Playerbot
{

// Forward declarations
enum class ProfessionType : uint16;
enum class ProfessionCategory : uint8;
struct RecipeInfo;
struct ProfessionSkillInfo;
struct ProfessionAutomationProfile;
struct ProfessionMetrics;

class TC_GAME_API IProfessionManager
{
public:
    virtual ~IProfessionManager() = default;

    // Core profession management
    virtual void Initialize() = 0;
    virtual void Update(::Player* player, uint32 diff) = 0;
    virtual bool LearnProfession(::Player* player, ProfessionType profession) = 0;
    virtual bool HasProfession(::Player* player, ProfessionType profession) const = 0;
    virtual uint16 GetProfessionSkill(::Player* player, ProfessionType profession) const = 0;
    virtual uint16 GetMaxProfessionSkill(::Player* player, ProfessionType profession) const = 0;
    virtual std::vector<ProfessionSkillInfo> GetPlayerProfessions(::Player* player) const = 0;
    virtual bool UnlearnProfession(::Player* player, ProfessionType profession) = 0;

    // Auto-learn system
    virtual void AutoLearnProfessionsForClass(::Player* player) = 0;
    virtual std::vector<ProfessionType> GetRecommendedProfessions(uint8 classId) const = 0;
    virtual bool IsProfessionSuitableForClass(uint8 classId, ProfessionType profession) const = 0;
    virtual ProfessionCategory GetProfessionCategory(ProfessionType profession) const = 0;
    virtual std::vector<ProfessionType> GetBeneficialPairs(ProfessionType profession) const = 0;
    virtual bool IsBeneficialPair(ProfessionType prof1, ProfessionType prof2) const = 0;
    virtual uint16 GetRaceProfessionBonus(uint8 raceId, ProfessionType profession) const = 0;

    // Recipe management
    virtual bool LearnRecipe(::Player* player, uint32 recipeId) = 0;
    virtual bool KnowsRecipe(::Player* player, uint32 recipeId) const = 0;
    virtual std::vector<RecipeInfo> GetRecipesForProfession(ProfessionType profession) const = 0;
    virtual std::vector<RecipeInfo> GetCraftableRecipes(::Player* player, ProfessionType profession) const = 0;
    virtual RecipeInfo const* GetOptimalLevelingRecipe(::Player* player, ProfessionType profession) const = 0;
    virtual bool CanCraftRecipe(::Player* player, RecipeInfo const& recipe) const = 0;
    virtual float GetSkillUpChance(::Player* player, RecipeInfo const& recipe) const = 0;

    // Crafting automation
    virtual bool AutoLevelProfession(::Player* player, ProfessionType profession) = 0;
    virtual bool CraftItem(::Player* player, RecipeInfo const& recipe, uint32 quantity = 1) = 0;
    virtual void QueueCraft(::Player* player, uint32 recipeId, uint32 quantity) = 0;
    virtual void ProcessCraftingQueue(::Player* player, uint32 diff) = 0;
    virtual bool HasMaterialsForRecipe(::Player* player, RecipeInfo const& recipe) const = 0;
    virtual std::vector<std::pair<uint32, uint32>> GetMissingMaterials(::Player* player, RecipeInfo const& recipe) const = 0;

    // Automation profiles
    virtual void SetAutomationProfile(uint32 playerGuid, ProfessionAutomationProfile const& profile) = 0;
    virtual ProfessionAutomationProfile GetAutomationProfile(uint32 playerGuid) const = 0;

    // Metrics
    virtual ProfessionMetrics const& GetPlayerMetrics(uint32 playerGuid) const = 0;
    virtual ProfessionMetrics const& GetGlobalMetrics() const = 0;
};

} // namespace Playerbot
