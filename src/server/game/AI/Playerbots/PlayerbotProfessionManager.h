/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRINITY_PLAYERBOTPROFESSIONMANAGER_H
#define TRINITY_PLAYERBOTPROFESSIONMANAGER_H

#ifdef WITH_PLAYERBOTS

#include "Define.h"
#include "ObjectGuid.h"
#include "ItemTemplate.h"
#include "SpellDefines.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <queue>

class Player;
class PlayerbotPlayerAI;
class Item;
class ItemTemplate;
class SpellInfo;

enum class PlayerbotProfessionPriority : uint8
{
    VERY_LOW        = 1,
    LOW             = 2,
    NORMAL          = 3,
    HIGH            = 4,
    VERY_HIGH       = 5,
    CRITICAL        = 6
};

enum class PlayerbotProfessionStrategy : uint8
{
    SELF_SUFFICIENT = 0,    // Focus on items for personal use
    PROFIT_FOCUSED  = 1,    // Maximize gold generation
    LEVELING        = 2,    // Fast skill progression
    MATERIAL_PREP   = 3,    // Prepare materials for other professions
    GUILD_SUPPORT   = 4,    // Focus on guild needs
    BALANCED        = 5,    // Balance all considerations
    SPECIALIZED     = 6     // Focus on specific recipe categories
};

enum class PlayerbotCraftingReason : uint8
{
    PERSONAL_USE    = 0,
    SKILL_GAIN      = 1,
    PROFIT          = 2,
    GUILD_REQUEST   = 3,
    MATERIAL_PREP   = 4,
    QUEST_REQUIREMENT = 5,
    SET_COMPLETION  = 6,
    EXPERIMENTAL    = 7
};

enum class PlayerbotProfessionType : uint8
{
    PRIMARY_PRODUCTION  = 0,    // Alchemy, Blacksmithing, etc.
    PRIMARY_GATHERING   = 1,    // Mining, Herbalism, Skinning
    SECONDARY           = 2,    // Cooking, First Aid, Fishing
    CLASS_SPECIFIC      = 3     // Rogue lockpicking, etc.
};

struct PlayerbotCraftingTask
{
    uint32 recipeId;
    SpellInfo const* recipe;
    uint32 itemId;
    std::string itemName;
    uint32 quantity;
    PlayerbotProfessionPriority priority;
    PlayerbotCraftingReason reason;
    uint32 professionId;
    uint32 requiredSkill;
    uint32 estimatedCost;
    uint32 estimatedProfit;
    float successChance;
    std::vector<uint32> requiredMaterials;  // itemId -> quantity pairs
    std::map<uint32, uint32> materialQuantities;
    bool hasMaterials;
    bool canCraftNow;
    uint32 createdTime;
    uint32 deadline;
    std::string requestedBy;
    
    PlayerbotCraftingTask() : recipeId(0), recipe(nullptr), itemId(0), quantity(1),
                             priority(PlayerbotProfessionPriority::NORMAL),
                             reason(PlayerbotCraftingReason::PERSONAL_USE),
                             professionId(0), requiredSkill(0), estimatedCost(0),
                             estimatedProfit(0), successChance(0.0f), hasMaterials(false),
                             canCraftNow(false), createdTime(0), deadline(0) {}
};

struct PlayerbotProfessionInfo
{
    uint32 professionId;
    std::string name;
    PlayerbotProfessionType type;
    uint32 currentSkill;
    uint32 maxSkill;
    uint32 skillCap;
    std::vector<uint32> knownRecipes;
    std::vector<uint32> availableRecipes;  // Can learn but don't know yet
    std::map<uint32, uint32> materialStock;  // itemId -> quantity
    uint32 totalCrafted;
    uint32 goldSpent;
    uint32 goldEarned;
    float profitability;
    uint32 lastSkillGain;
    
    PlayerbotProfessionInfo() : professionId(0), type(PlayerbotProfessionType::PRIMARY_PRODUCTION),
                               currentSkill(0), maxSkill(0), skillCap(0), totalCrafted(0),
                               goldSpent(0), goldEarned(0), profitability(0.0f), lastSkillGain(0) {}
};

struct PlayerbotRecipeAnalysis
{
    uint32 recipeId;
    uint32 itemId;
    uint32 requiredSkill;
    PlayerbotProfessionPriority learnPriority;
    std::vector<uint32> ingredients;
    uint32 materialCost;
    uint32 marketValue;
    float profitMargin;
    uint32 demandScore;
    bool isUsefulForBot;
    bool isUsefulForGuild;
    bool hasSkillGainPotential;
    std::string category;
    
    PlayerbotRecipeAnalysis() : recipeId(0), itemId(0), requiredSkill(0),
                               learnPriority(PlayerbotProfessionPriority::NORMAL),
                               materialCost(0), marketValue(0), profitMargin(0.0f),
                               demandScore(0), isUsefulForBot(false), isUsefulForGuild(false),
                               hasSkillGainPotential(false) {}
};

struct PlayerbotMaterialRequest
{
    uint32 itemId;
    std::string itemName;
    uint32 quantityNeeded;
    uint32 quantityHave;
    PlayerbotProfessionPriority priority;
    std::vector<std::string> sources;  // Where to get this material
    uint32 estimatedCost;
    bool canGatherSelf;
    bool canBuySelf;
    bool needsGuildHelp;
    uint32 requestTime;
    
    PlayerbotMaterialRequest() : itemId(0), quantityNeeded(0), quantityHave(0),
                                priority(PlayerbotProfessionPriority::NORMAL),
                                estimatedCost(0), canGatherSelf(false), canBuySelf(false),
                                needsGuildHelp(false), requestTime(0) {}
};

/**
 * @brief Manages automated profession activities and crafting optimization for playerbots
 * 
 * This class handles:
 * - Profession skill progression and optimization
 * - Recipe learning and prioritization
 * - Material management and procurement
 * - Automated crafting workflows
 * - Profit optimization and market analysis
 * - Guild profession coordination
 * - Quest-driven crafting support
 */
class TC_GAME_API PlayerbotProfessionManager
{
public:
    PlayerbotProfessionManager(PlayerbotPlayerAI* ai);
    ~PlayerbotProfessionManager();

    // Profession discovery and analysis
    void DiscoverProfessions();
    void AnalyzeProfessionPotential(uint32 professionId);
    void EvaluateRecipes();
    PlayerbotProfessionInfo GetProfessionInfo(uint32 professionId) const;
    std::vector<PlayerbotProfessionInfo> GetAllProfessions() const;
    
    // Strategy and configuration
    void SetProfessionStrategy(PlayerbotProfessionStrategy strategy) { _professionStrategy = strategy; }
    PlayerbotProfessionStrategy GetProfessionStrategy() const { return _professionStrategy; }
    void SetProfessionPriority(uint32 professionId, PlayerbotProfessionPriority priority);
    PlayerbotProfessionPriority GetProfessionPriority(uint32 professionId) const;
    
    // Crafting management
    void AddCraftingTask(uint32 recipeId, uint32 quantity, PlayerbotCraftingReason reason = PlayerbotCraftingReason::PERSONAL_USE);
    void ProcessCraftingQueue();
    bool CanCraftItem(uint32 recipeId) const;
    bool HasMaterialsForRecipe(uint32 recipeId, uint32 quantity = 1) const;
    void CraftItem(uint32 recipeId, uint32 quantity = 1);
    void CancelCraftingTask(uint32 recipeId);
    
    // Recipe management
    void LearnAvailableRecipes();
    void PrioritizeRecipeLearning();
    PlayerbotRecipeAnalysis AnalyzeRecipe(uint32 recipeId) const;
    bool ShouldLearnRecipe(uint32 recipeId) const;
    std::vector<uint32> GetRecommendedRecipes(uint32 professionId) const;
    uint32 GetBestSkillUpRecipe(uint32 professionId) const;
    
    // Material management
    void UpdateMaterialInventory();
    void OptimizeMaterialStorage();
    std::vector<PlayerbotMaterialRequest> GetMaterialNeeds() const;
    void RequestMaterials(std::vector<uint32> const& itemIds);
    bool HasSufficientMaterials(std::map<uint32, uint32> const& requirements) const;
    void PurchaseMaterials(std::vector<uint32> const& itemIds);
    
    // Skill progression
    void OptimizeSkillProgression();
    void CreateSkillPlan(uint32 professionId, uint32 targetSkill);
    uint32 GetMostEfficientSkillUpItem(uint32 professionId) const;
    void ExecuteSkillUpPlan();
    float CalculateSkillGainEfficiency(uint32 recipeId) const;
    
    // Economic analysis
    float CalculateProfitability(uint32 recipeId, uint32 quantity = 1) const;
    uint32 EstimateMaterialCost(uint32 recipeId, uint32 quantity = 1) const;
    uint32 GetMarketValue(uint32 itemId) const;
    void UpdateMarketData();
    std::vector<uint32> GetMostProfitableItems(uint32 professionId) const;
    
    // Automation and scheduling
    void ScheduleRegularCrafting();
    void ProcessScheduledTasks();
    void AutoCraftConsumables();
    void AutoCraftGearUpgrades();
    void AutoCraftQuestItems();
    void SetAutoCraftingEnabled(bool enabled) { _autoCraftingEnabled = enabled; }
    bool IsAutoCraftingEnabled() const { return _autoCraftingEnabled; }
    
    // Guild coordination
    void ProcessGuildRequests();
    void OfferProfessionServices();
    void ShareProfessionMaterials();
    void CoordinateGuildCrafting();
    std::vector<uint32> GetGuildNeededItems() const;
    void NotifyGuildOfAvailability();
    
    // Quest support
    void HandleCraftingQuests();
    void CraftQuestItems(uint32 questId);
    std::vector<uint32> GetQuestCraftingRequirements(uint32 questId) const;
    bool CanCompleteQuestCrafting(uint32 questId) const;
    
    // Specialization management
    void ChooseSpecialization(uint32 professionId);
    void EvaluateSpecializationOptions(uint32 professionId);
    uint32 GetRecommendedSpecialization(uint32 professionId) const;
    bool HasSpecialization(uint32 professionId) const;
    
    // Inventory optimization
    void OptimizeProfessionInventory();
    void ManageBagSpace();
    std::vector<uint32> IdentifyUnneededMaterials() const;
    void SellExcessMaterials();
    void BankImportantMaterials();
    
    // Statistics and reporting
    uint32 GetTotalItemsCrafted() const { return _totalItemsCrafted; }
    uint32 GetProfessionProfit() const { return _totalProfessionProfit; }
    float GetAverageProfitMargin() const;
    std::map<uint32, uint32> GetCraftingStatistics() const; // itemId -> quantity crafted
    void GenerateProfessionReport() const;
    
    // Performance optimization
    void Update(uint32 diff);
    void UpdateProfessionData();
    void OptimizeCraftingOrder();
    void CacheRecipeData();
    void PreloadMaterialData();

private:
    PlayerbotPlayerAI* _ai;
    Player* _bot;
    
    // Configuration
    PlayerbotProfessionStrategy _professionStrategy;
    std::map<uint32, PlayerbotProfessionPriority> _professionPriorities;
    bool _autoCraftingEnabled;
    bool _guildSharingEnabled;
    uint32 _maxCraftingQueueSize;
    
    // Profession data
    std::map<uint32, PlayerbotProfessionInfo> _professions;
    std::queue<PlayerbotCraftingTask> _craftingQueue;
    std::map<uint32, PlayerbotRecipeAnalysis> _recipeAnalysisCache;
    std::vector<PlayerbotMaterialRequest> _materialRequests;
    
    // Statistics
    uint32 _totalItemsCrafted;
    uint32 _totalMaterialsUsed;
    uint32 _totalGoldSpent;
    uint32 _totalProfessionProfit;
    std::map<uint32, uint32> _itemsCraftedByType;
    
    // Update timers
    uint32 _professionUpdateTimer;
    uint32 _materialCheckTimer;
    uint32 _marketDataTimer;
    uint32 _guildCoordinationTimer;
    uint32 _questCheckTimer;
    
    // Cache data
    std::map<uint32, uint32> _marketPriceCache;  // itemId -> price
    std::map<uint32, std::vector<uint32>> _recipeIngredientCache;  // recipeId -> ingredients
    std::map<uint32, uint32> _skillUpRecipeCache;  // professionId -> best recipe
    std::set<uint32> _availableRecipesCache;
    
    // Guild coordination
    std::vector<uint32> _guildRequestedItems;
    std::map<uint32, std::string> _guildRequesters;  // itemId -> requester name
    uint32 _lastGuildOfferTime;
    
    // Helper methods
    void InitializeProfessions();
    void LoadProfessionData();
    void SaveProfessionData();
    void CacheMarketPrices();
    
    // Profession discovery
    bool IsProfessionKnown(uint32 professionId) const;
    uint32 GetProfessionSkill(uint32 professionId) const;
    std::vector<uint32> GetKnownRecipes(uint32 professionId) const;
    std::vector<uint32> GetAvailableRecipes(uint32 professionId) const;
    
    // Recipe analysis
    std::vector<uint32> GetRecipeIngredients(uint32 recipeId) const;
    uint32 GetRecipeRequiredSkill(uint32 recipeId) const;
    uint32 CalculateRecipeMaterialCost(uint32 recipeId) const;
    float CalculateRecipeProfitMargin(uint32 recipeId) const;
    uint32 GetRecipeCategory(uint32 recipeId) const;
    
    // Material management
    uint32 GetItemQuantityInInventory(uint32 itemId) const;
    uint32 GetItemQuantityInBank(uint32 itemId) const;
    bool CanObtainMaterial(uint32 itemId, uint32 quantity) const;
    std::vector<std::string> GetMaterialSources(uint32 itemId) const;
    
    // Crafting algorithms
    bool ExecuteCraftingTask(PlayerbotCraftingTask const& task);
    void ConsumeCraftingMaterials(uint32 recipeId, uint32 quantity);
    void UpdateCraftingStatistics(uint32 itemId, uint32 quantity, uint32 cost);
    bool ValidateCraftingRequirements(PlayerbotCraftingTask const& task) const;
    
    // Strategy implementations
    void ExecuteSelfSufficientStrategy();
    void ExecuteProfitStrategy();
    void ExecuteLevelingStrategy();
    void ExecuteMaterialPrepStrategy();
    void ExecuteGuildSupportStrategy();
    void ExecuteBalancedStrategy();
    void ExecuteSpecializedStrategy();
    
    // Economic calculations
    uint32 GetAuctionHousePrice(uint32 itemId) const;
    uint32 GetVendorPrice(uint32 itemId) const;
    float CalculateROI(uint32 recipeId) const;  // Return on Investment
    uint32 EstimateDemand(uint32 itemId) const;
    
    // Skill optimization
    std::vector<uint32> GenerateSkillUpPath(uint32 professionId, uint32 targetSkill) const;
    uint32 GetNextSkillUpRecipe(uint32 professionId) const;
    float CalculateMaterialEfficiency(uint32 recipeId) const;
    
    // Quest integration
    std::vector<uint32> GetActiveQuestCraftingNeeds() const;
    bool IsQuestCraftingItem(uint32 itemId) const;
    uint32 GetQuestItemQuantityNeeded(uint32 itemId) const;
    
    // Guild features
    void UpdateGuildRequests();
    void ProcessGuildMaterialSharing();
    bool ShouldOfferToGuild(uint32 itemId) const;
    void NotifyGuildCraftingComplete(uint32 itemId, uint32 quantity);
    
    // Specialization logic
    std::vector<uint32> GetAvailableSpecializations(uint32 professionId) const;
    float EvaluateSpecialization(uint32 professionId, uint32 specializationId) const;
    bool MeetsSpecializationRequirements(uint32 specializationId) const;
    
    // Inventory management
    void CleanupCraftingMaterials();
    void OrganizeProfessionItems();
    bool ShouldKeepMaterial(uint32 itemId, uint32 quantity) const;
    void TransferMaterialsFromBank();
    
    // Performance optimizations
    void OptimizeCraftingBatch();
    void PrefetchCraftingData();
    void UpdateCraftingCache();
    void CleanupExpiredCache();
    
    // Utility methods
    std::string GetProfessionName(uint32 professionId) const;
    std::string GetRecipeName(uint32 recipeId) const;
    std::string GetItemName(uint32 itemId) const;
    bool IsGatheringProfession(uint32 professionId) const;
    bool IsProductionProfession(uint32 professionId) const;
    
    // Logging and debugging
    void LogCraftingActivity(std::string const& action, std::string const& details = "");
    void DebugPrintProfessionInfo() const;
    void DebugPrintCraftingQueue() const;
};

#endif // WITH_PLAYERBOTS
#endif // TRINITY_PLAYERBOTPROFESSIONMANAGER_H