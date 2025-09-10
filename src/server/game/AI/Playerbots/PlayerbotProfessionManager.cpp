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

#ifdef WITH_PLAYERBOTS

#include "PlayerbotProfessionManager.h"
#include "PlayerbotPlayerAI.h"
#include "Player.h"
#include "DatabaseEnv.h"
#include "WorldSession.h"
#include "ObjectMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "AuctionHouseMgr.h"
#include "Log.h"
#include <algorithm>
#include <numeric>

PlayerbotProfessionManager::PlayerbotProfessionManager(PlayerbotPlayerAI* ai) : 
    _ai(ai), _bot(ai->GetBot()),
    _professionStrategy(PlayerbotProfessionStrategy::BALANCED),
    _autoCraftingEnabled(true),
    _guildSharingEnabled(true),
    _maxCraftingQueueSize(50),
    _totalItemsCrafted(0),
    _totalMaterialsUsed(0),
    _totalGoldSpent(0),
    _totalProfessionProfit(0),
    _professionUpdateTimer(0),
    _materialCheckTimer(0),
    _marketDataTimer(0),
    _guildCoordinationTimer(0),
    _questCheckTimer(0),
    _lastGuildOfferTime(0)
{
    InitializeProfessions();
    LoadProfessionData();
    CacheMarketPrices();
}

PlayerbotProfessionManager::~PlayerbotProfessionManager()
{
    SaveProfessionData();
}

void PlayerbotProfessionManager::InitializeProfessions()
{
    // Initialize profession priority defaults
    _professionPriorities[SKILL_ALCHEMY] = PlayerbotProfessionPriority::HIGH;
    _professionPriorities[SKILL_BLACKSMITHING] = PlayerbotProfessionPriority::NORMAL;
    _professionPriorities[SKILL_LEATHERWORKING] = PlayerbotProfessionPriority::NORMAL;
    _professionPriorities[SKILL_TAILORING] = PlayerbotProfessionPriority::NORMAL;
    _professionPriorities[SKILL_ENGINEERING] = PlayerbotProfessionPriority::NORMAL;
    _professionPriorities[SKILL_ENCHANTING] = PlayerbotProfessionPriority::HIGH;
    _professionPriorities[SKILL_JEWELCRAFTING] = PlayerbotProfessionPriority::HIGH;
    _professionPriorities[SKILL_INSCRIPTION] = PlayerbotProfessionPriority::NORMAL;
    
    // Gathering professions
    _professionPriorities[SKILL_HERBALISM] = PlayerbotProfessionPriority::HIGH;
    _professionPriorities[SKILL_MINING] = PlayerbotProfessionPriority::HIGH;
    _professionPriorities[SKILL_SKINNING] = PlayerbotProfessionPriority::NORMAL;
    
    // Secondary professions
    _professionPriorities[SKILL_COOKING] = PlayerbotProfessionPriority::HIGH;
    _professionPriorities[SKILL_FIRST_AID] = PlayerbotProfessionPriority::NORMAL;
    _professionPriorities[SKILL_FISHING] = PlayerbotProfessionPriority::LOW;
    
    LogProfessionActivity("Profession Manager Initialized", "Default priorities set");
}

void PlayerbotProfessionManager::LoadProfessionData()
{
    if (!_bot)
        return;
    
    // Load profession data from database
    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SKILLS);
    stmt->setUInt32(0, _bot->GetGUID().GetCounter());
    PreparedQueryResult result = CharacterDatabase.Query(stmt);
    
    if (!result)
        return;
    
    do
    {
        Field* fields = result->Fetch();
        uint32 skill = fields[0].GetUInt16();
        uint32 value = fields[1].GetUInt16();
        uint32 max = fields[2].GetUInt16();
        
        // Check if this is a profession skill
        if (IsProfessionSkill(skill))
        {
            PlayerbotProfessionInfo profInfo;
            profInfo.professionId = skill;
            profInfo.name = GetProfessionName(skill);
            profInfo.type = DetermineProfessionType(skill);
            profInfo.currentSkill = value;
            profInfo.maxSkill = max;
            profInfo.skillCap = GetMaxSkillValueForLevel(_bot->getLevel());
            
            // Load known recipes
            profInfo.knownRecipes = GetKnownRecipes(skill);
            profInfo.availableRecipes = GetAvailableRecipes(skill);
            
            _professions[skill] = profInfo;
            
            LogProfessionActivity("Loaded Profession", 
                std::string(profInfo.name + " [" + std::to_string(value) + "/" + std::to_string(max) + "]"));
        }
    } while (result->NextRow());
}

void PlayerbotProfessionManager::SaveProfessionData()
{
    // Save profession statistics and configuration to database
    for (auto const& [professionId, profInfo] : _professions)
    {
        PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_PLAYERBOT_PROFESSION_DATA);
        stmt->setUInt32(0, _bot->GetGUID().GetCounter());
        stmt->setUInt32(1, professionId);
        stmt->setUInt32(2, profInfo.totalCrafted);
        stmt->setUInt32(3, profInfo.goldSpent);
        stmt->setUInt32(4, profInfo.goldEarned);
        stmt->setFloat(5, profInfo.profitability);
        CharacterDatabase.Execute(stmt);
    }
}

void PlayerbotProfessionManager::DiscoverProfessions()
{
    if (!_bot)
        return;
    
    // Scan known skills for professions
    for (auto const& [skillId, skillInfo] : _bot->GetSkillMap())
    {
        if (IsProfessionSkill(skillId) && _professions.find(skillId) == _professions.end())
        {
            PlayerbotProfessionInfo profInfo;
            profInfo.professionId = skillId;
            profInfo.name = GetProfessionName(skillId);
            profInfo.type = DetermineProfessionType(skillId);
            profInfo.currentSkill = skillInfo.CurrentValue;
            profInfo.maxSkill = skillInfo.MaxValue;
            profInfo.skillCap = GetMaxSkillValueForLevel(_bot->getLevel());
            profInfo.knownRecipes = GetKnownRecipes(skillId);
            profInfo.availableRecipes = GetAvailableRecipes(skillId);
            
            _professions[skillId] = profInfo;
            
            LogProfessionActivity("Discovered Profession", profInfo.name);
        }
    }
    
    EvaluateRecipes();
}

void PlayerbotProfessionManager::EvaluateRecipes()
{
    for (auto& [professionId, profInfo] : _professions)
    {
        for (uint32 recipeId : profInfo.knownRecipes)
        {
            if (_recipeAnalysisCache.find(recipeId) == _recipeAnalysisCache.end())
            {
                _recipeAnalysisCache[recipeId] = AnalyzeRecipe(recipeId);
            }
        }
        
        for (uint32 recipeId : profInfo.availableRecipes)
        {
            if (_recipeAnalysisCache.find(recipeId) == _recipeAnalysisCache.end())
            {
                _recipeAnalysisCache[recipeId] = AnalyzeRecipe(recipeId);
            }
        }
    }
}

PlayerbotRecipeAnalysis PlayerbotProfessionManager::AnalyzeRecipe(uint32 recipeId) const
{
    PlayerbotRecipeAnalysis analysis;
    analysis.recipeId = recipeId;
    
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(recipeId);
    if (!spellInfo)
        return analysis;
    
    // Get crafted item ID from spell effects
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (spellInfo->Effects[i].Effect == SPELL_EFFECT_CREATE_ITEM)
        {
            analysis.itemId = spellInfo->Effects[i].ItemType;
            break;
        }
    }
    
    if (!analysis.itemId)
        return analysis;
    
    // Get item template for analysis
    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(analysis.itemId);
    if (!itemTemplate)
        return analysis;
    
    // Calculate required skill level
    analysis.requiredSkill = spellInfo->BaseLevel;
    
    // Analyze material costs
    analysis.ingredients = GetRecipeIngredients(recipeId);
    analysis.materialCost = CalculateRecipeMaterialCost(recipeId);
    
    // Get market value
    analysis.marketValue = GetMarketValue(analysis.itemId);
    
    // Calculate profit margin
    if (analysis.materialCost > 0)
        analysis.profitMargin = float(analysis.marketValue - analysis.materialCost) / float(analysis.materialCost);
    
    // Evaluate usefulness for bot
    analysis.isUsefulForBot = EvaluateItemUsefulness(itemTemplate, _bot);
    
    // Evaluate usefulness for guild
    if (_bot->GetGuild())
        analysis.isUsefulForGuild = EvaluateGuildItemUsefulness(analysis.itemId);
    
    // Check skill gain potential
    analysis.hasSkillGainPotential = CanGainSkillFromRecipe(recipeId);
    
    // Set learning priority
    analysis.learnPriority = CalculateRecipeLearningPriority(analysis);
    
    return analysis;
}

void PlayerbotProfessionManager::AddCraftingTask(uint32 recipeId, uint32 quantity, PlayerbotCraftingReason reason)
{
    if (_craftingQueue.size() >= _maxCraftingQueueSize)
    {
        LogProfessionActivity("Crafting Queue Full", "Cannot add task for recipe " + std::to_string(recipeId));
        return;
    }
    
    PlayerbotCraftingTask task;
    task.recipeId = recipeId;
    task.recipe = sSpellMgr->GetSpellInfo(recipeId);
    task.quantity = quantity;
    task.reason = reason;
    task.createdTime = time(nullptr);
    task.priority = CalculateTaskPriority(recipeId, reason);
    
    if (!task.recipe)
    {
        LogProfessionActivity("Invalid Recipe", "Recipe ID " + std::to_string(recipeId) + " not found");
        return;
    }
    
    // Get crafted item ID
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (task.recipe->Effects[i].Effect == SPELL_EFFECT_CREATE_ITEM)
        {
            task.itemId = task.recipe->Effects[i].ItemType;
            break;
        }
    }
    
    if (task.itemId)
    {
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(task.itemId);
        if (itemTemplate)
            task.itemName = itemTemplate->Name1;
    }
    
    // Calculate requirements
    task.requiredSkill = task.recipe->BaseLevel;
    task.requiredMaterials = GetRecipeIngredients(recipeId);
    task.materialQuantities = GetRecipeMaterialQuantities(recipeId);
    task.estimatedCost = CalculateRecipeMaterialCost(recipeId) * quantity;
    task.estimatedProfit = (GetMarketValue(task.itemId) - CalculateRecipeMaterialCost(recipeId)) * quantity;
    task.hasMaterials = HasMaterialsForRecipe(recipeId, quantity);
    task.canCraftNow = CanCraftItem(recipeId);
    task.successChance = CalculateRecipeSuccessChance(recipeId);
    
    _craftingQueue.push(task);
    
    LogProfessionActivity("Added Crafting Task", 
        task.itemName + " x" + std::to_string(quantity) + " (Priority: " + std::to_string(uint32(task.priority)) + ")");
}

void PlayerbotProfessionManager::ProcessCraftingQueue()
{
    if (_craftingQueue.empty() || !_autoCraftingEnabled)
        return;
    
    // Sort queue by priority (would need priority queue or manual sorting)
    std::priority_queue<PlayerbotCraftingTask, std::vector<PlayerbotCraftingTask>, CraftingTaskComparator> sortedQueue;
    
    while (!_craftingQueue.empty())
    {
        sortedQueue.push(_craftingQueue.front());
        _craftingQueue.pop();
    }
    
    // Process highest priority tasks
    while (!sortedQueue.empty())
    {
        PlayerbotCraftingTask task = sortedQueue.top();
        sortedQueue.pop();
        
        if (ExecuteCraftingTask(task))
        {
            LogProfessionActivity("Crafted Item", 
                task.itemName + " x" + std::to_string(task.quantity));
            
            UpdateCraftingStatistics(task.itemId, task.quantity, task.estimatedCost);
        }
        else
        {
            // Put back in queue if couldn't complete (missing materials, etc.)
            if (task.hasMaterials || task.canCraftNow)
                _craftingQueue.push(task);
        }
        
        // Process only one task per update to avoid lag
        break;
    }
    
    // Put remaining tasks back in queue
    while (!sortedQueue.empty())
    {
        _craftingQueue.push(sortedQueue.top());
        sortedQueue.pop();
    }
}

bool PlayerbotProfessionManager::ExecuteCraftingTask(PlayerbotCraftingTask const& task)
{
    if (!ValidateCraftingRequirements(task))
        return false;
    
    // Check if we have the materials
    if (!HasMaterialsForRecipe(task.recipeId, task.quantity))
    {
        RequestMaterials(task.requiredMaterials);
        return false;
    }
    
    // Check skill requirement
    uint32 currentSkill = GetProfessionSkill(task.professionId);
    if (currentSkill < task.requiredSkill)
        return false;
    
    // Execute the crafting
    for (uint32 i = 0; i < task.quantity; ++i)
    {
        if (CraftSingleItem(task.recipeId))
        {
            ConsumeCraftingMaterials(task.recipeId, 1);
            _totalItemsCrafted++;
            
            // Update profession info
            if (_professions.find(task.professionId) != _professions.end())
            {
                _professions[task.professionId].totalCrafted++;
                _professions[task.professionId].goldSpent += task.estimatedCost / task.quantity;
                _professions[task.professionId].goldEarned += GetMarketValue(task.itemId);
            }
        }
        else
        {
            return false; // Failed to craft
        }
    }
    
    return true;
}

bool PlayerbotProfessionManager::CraftSingleItem(uint32 recipeId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(recipeId);
    if (!spellInfo)
        return false;
    
    // Create spell and cast it
    Spell* spell = new Spell(_bot, spellInfo, TRIGGERED_NONE);
    SpellCastTargets targets;
    targets.SetUnitTarget(_bot);
    
    SpellCastResult result = spell->CheckCast(false);
    if (result != SPELL_CAST_OK)
    {
        delete spell;
        return false;
    }
    
    spell->cast();
    return true;
}

void PlayerbotProfessionManager::ConsumeCraftingMaterials(uint32 recipeId, uint32 quantity)
{
    std::map<uint32, uint32> materialQuantities = GetRecipeMaterialQuantities(recipeId);
    
    for (auto const& [itemId, requiredQuantity] : materialQuantities)
    {
        uint32 totalRequired = requiredQuantity * quantity;
        _bot->DestroyItemCount(itemId, totalRequired, true);
        _totalMaterialsUsed += totalRequired;
    }
}

void PlayerbotProfessionManager::UpdateCraftingStatistics(uint32 itemId, uint32 quantity, uint32 cost)
{
    _itemsCraftedByType[itemId] += quantity;
    _totalGoldSpent += cost;
    
    uint32 revenue = GetMarketValue(itemId) * quantity;
    _totalProfessionProfit += (revenue > cost) ? (revenue - cost) : 0;
}

bool PlayerbotProfessionManager::ValidateCraftingRequirements(PlayerbotCraftingTask const& task) const
{
    // Check if bot knows the recipe
    if (std::find(_professions.at(task.professionId).knownRecipes.begin(),
                  _professions.at(task.professionId).knownRecipes.end(),
                  task.recipeId) == _professions.at(task.professionId).knownRecipes.end())
    {
        return false;
    }
    
    // Check skill level
    uint32 currentSkill = GetProfessionSkill(task.professionId);
    if (currentSkill < task.requiredSkill)
        return false;
    
    // Check materials
    if (!HasMaterialsForRecipe(task.recipeId, task.quantity))
        return false;
    
    // Check bag space for output
    if (_bot->GetBagsFreeSlots() < int32(task.quantity))
        return false;
    
    return true;
}

bool PlayerbotProfessionManager::CanCraftItem(uint32 recipeId) const
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(recipeId);
    if (!spellInfo)
        return false;
    
    // Find profession for this recipe
    uint32 professionId = GetRecipeProfession(recipeId);
    if (professionId == 0)
        return false;
    
    // Check if we know the profession and recipe
    auto profItr = _professions.find(professionId);
    if (profItr == _professions.end())
        return false;
    
    // Check if we know the recipe
    if (std::find(profItr->second.knownRecipes.begin(), profItr->second.knownRecipes.end(), recipeId) == profItr->second.knownRecipes.end())
        return false;
    
    // Check skill requirement
    if (profItr->second.currentSkill < spellInfo->BaseLevel)
        return false;
    
    return true;
}

bool PlayerbotProfessionManager::HasMaterialsForRecipe(uint32 recipeId, uint32 quantity) const
{
    std::map<uint32, uint32> materialQuantities = GetRecipeMaterialQuantities(recipeId);
    
    for (auto const& [itemId, requiredQuantity] : materialQuantities)
    {
        uint32 totalRequired = requiredQuantity * quantity;
        uint32 available = GetItemQuantityInInventory(itemId) + GetItemQuantityInBank(itemId);
        
        if (available < totalRequired)
            return false;
    }
    
    return true;
}

void PlayerbotProfessionManager::Update(uint32 diff)
{
    if (!_bot || !_ai)
        return;
    
    // Update profession data periodically
    if (_professionUpdateTimer <= diff)
    {
        UpdateProfessionData();
        _professionUpdateTimer = 30000; // 30 seconds
    }
    else
        _professionUpdateTimer -= diff;
    
    // Check material needs
    if (_materialCheckTimer <= diff)
    {
        UpdateMaterialInventory();
        ProcessMaterialRequests();
        _materialCheckTimer = 60000; // 1 minute
    }
    else
        _materialCheckTimer -= diff;
    
    // Update market data
    if (_marketDataTimer <= diff)
    {
        UpdateMarketData();
        _marketDataTimer = 300000; // 5 minutes
    }
    else
        _marketDataTimer -= diff;
    
    // Guild coordination
    if (_guildCoordinationTimer <= diff && _guildSharingEnabled)
    {
        ProcessGuildRequests();
        CoordinateGuildCrafting();
        _guildCoordinationTimer = 120000; // 2 minutes
    }
    else
        _guildCoordinationTimer -= diff;
    
    // Quest support
    if (_questCheckTimer <= diff)
    {
        HandleCraftingQuests();
        _questCheckTimer = 45000; // 45 seconds
    }
    else
        _questCheckTimer -= diff;
    
    // Process crafting queue
    ProcessCraftingQueue();
    
    // Execute strategy-specific behaviors
    switch (_professionStrategy)
    {
        case PlayerbotProfessionStrategy::SELF_SUFFICIENT:
            ExecuteSelfSufficientStrategy();
            break;
        case PlayerbotProfessionStrategy::PROFIT_FOCUSED:
            ExecuteProfitStrategy();
            break;
        case PlayerbotProfessionStrategy::LEVELING:
            ExecuteLevelingStrategy();
            break;
        case PlayerbotProfessionStrategy::GUILD_SUPPORT:
            ExecuteGuildSupportStrategy();
            break;
        case PlayerbotProfessionStrategy::BALANCED:
            ExecuteBalancedStrategy();
            break;
        default:
            break;
    }
}

void PlayerbotProfessionManager::UpdateProfessionData()
{
    DiscoverProfessions();
    
    // Update current skill levels
    for (auto& [professionId, profInfo] : _professions)
    {
        if (_bot->HasSkill(professionId))
        {
            profInfo.currentSkill = _bot->GetSkillValue(professionId);
            profInfo.maxSkill = _bot->GetMaxSkillValue(professionId);
            profInfo.skillCap = GetMaxSkillValueForLevel(_bot->getLevel());
            
            // Update recipes if skill increased
            if (profInfo.currentSkill > profInfo.lastSkillGain)
            {
                profInfo.knownRecipes = GetKnownRecipes(professionId);
                profInfo.availableRecipes = GetAvailableRecipes(professionId);
                profInfo.lastSkillGain = profInfo.currentSkill;
                
                EvaluateRecipes();
            }
        }
    }
}

void PlayerbotProfessionManager::ExecuteBalancedStrategy()
{
    // Balance between personal needs, profit, and skill progression
    
    // 1. Craft items needed for personal use
    AutoCraftConsumables();
    AutoCraftGearUpgrades();
    
    // 2. Work on skill progression if below cap
    for (auto const& [professionId, profInfo] : _professions)
    {
        if (profInfo.currentSkill < profInfo.skillCap - 50) // 50 skill points buffer
        {
            uint32 skillUpRecipe = GetBestSkillUpRecipe(professionId);
            if (skillUpRecipe && CanCraftItem(skillUpRecipe))
            {
                AddCraftingTask(skillUpRecipe, 1, PlayerbotCraftingReason::SKILL_GAIN);
            }
        }
    }
    
    // 3. Craft profitable items if materials are available
    for (auto const& [professionId, profInfo] : _professions)
    {
        auto profitableItems = GetMostProfitableItems(professionId);
        for (uint32 itemId : profitableItems)
        {
            uint32 recipeId = FindRecipeForItem(itemId);
            if (recipeId && CanCraftItem(recipeId) && HasMaterialsForRecipe(recipeId))
            {
                AddCraftingTask(recipeId, 1, PlayerbotCraftingReason::PROFIT);
                break; // Only add one profitable item per profession per update
            }
        }
    }
    
    // 4. Support guild requests if enabled
    if (_guildSharingEnabled)
    {
        ProcessGuildRequests();
    }
}

void PlayerbotProfessionManager::ExecuteProfitStrategy()
{
    // Focus on maximizing gold generation through crafting
    
    for (auto const& [professionId, profInfo] : _professions)
    {
        auto profitableItems = GetMostProfitableItems(professionId);
        
        for (uint32 itemId : profitableItems)
        {
            uint32 recipeId = FindRecipeForItem(itemId);
            if (!recipeId || !CanCraftItem(recipeId))
                continue;
            
            float profitMargin = CalculateProfitability(recipeId);
            if (profitMargin < 0.15f) // Require at least 15% profit margin
                continue;
            
            // Calculate optimal quantity based on market demand and materials
            uint32 demandScore = EstimateDemand(itemId);
            uint32 maxQuantity = std::min(demandScore, GetMaxCraftableQuantity(recipeId));
            
            if (maxQuantity > 0)
            {
                AddCraftingTask(recipeId, maxQuantity, PlayerbotCraftingReason::PROFIT);
            }
        }
    }
}

void PlayerbotProfessionManager::ExecuteLevelingStrategy()
{
    // Focus on fast skill progression
    
    for (auto const& [professionId, profInfo] : _professions)
    {
        if (profInfo.currentSkill >= profInfo.skillCap)
            continue; // Already at cap
        
        uint32 skillUpRecipe = GetBestSkillUpRecipe(professionId);
        if (!skillUpRecipe)
            continue;
        
        float efficiency = CalculateSkillGainEfficiency(skillUpRecipe);
        if (efficiency < 0.5f) // Skip inefficient recipes
            continue;
        
        // Calculate how many to craft for maximum skill gain
        uint32 quantity = CalculateOptimalSkillUpQuantity(skillUpRecipe, profInfo.currentSkill, profInfo.skillCap);
        
        if (quantity > 0 && HasMaterialsForRecipe(skillUpRecipe, quantity))
        {
            AddCraftingTask(skillUpRecipe, quantity, PlayerbotCraftingReason::SKILL_GAIN);
        }
        else if (quantity > 0)
        {
            // Request materials if we don't have them
            auto materials = GetRecipeMaterialQuantities(skillUpRecipe);
            std::vector<uint32> materialIds;
            for (auto const& [itemId, q] : materials)
                materialIds.push_back(itemId);
            
            RequestMaterials(materialIds);
        }
    }
}

void PlayerbotProfessionManager::ExecuteSelfSufficientStrategy()
{
    // Focus on crafting items the bot can use
    
    // 1. Craft consumables if running low
    AutoCraftConsumables();
    
    // 2. Craft gear upgrades if available
    AutoCraftGearUpgrades();
    
    // 3. Craft quest items if needed
    AutoCraftQuestItems();
    
    // 4. Craft materials for other professions
    for (auto const& [professionId, profInfo] : _professions)
    {
        if (IsGatheringProfession(professionId))
            continue;
        
        // Find recipes that create materials needed by other professions
        for (uint32 recipeId : profInfo.knownRecipes)
        {
            uint32 itemId = GetRecipeOutput(recipeId);
            if (IsUsefulMaterial(itemId) && CanCraftItem(recipeId))
            {
                uint32 needed = EstimateMaterialNeed(itemId);
                uint32 have = GetItemQuantityInInventory(itemId) + GetItemQuantityInBank(itemId);
                
                if (have < needed && HasMaterialsForRecipe(recipeId, needed - have))
                {
                    AddCraftingTask(recipeId, needed - have, PlayerbotCraftingReason::MATERIAL_PREP);
                }
            }
        }
    }
}

void PlayerbotProfessionManager::AutoCraftConsumables()
{
    // List of consumable categories to maintain
    std::vector<uint32> consumableCategories = {
        ITEM_SUBCLASS_CONSUMABLE_POTION,
        ITEM_SUBCLASS_CONSUMABLE_ELIXIR,
        ITEM_SUBCLASS_CONSUMABLE_FLASK,
        ITEM_SUBCLASS_CONSUMABLE_SCROLL,
        ITEM_SUBCLASS_CONSUMABLE_FOOD_DRINK
    };
    
    for (auto const& [professionId, profInfo] : _professions)
    {
        for (uint32 recipeId : profInfo.knownRecipes)
        {
            uint32 itemId = GetRecipeOutput(recipeId);
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
            
            if (!itemTemplate || itemTemplate->Class != ITEM_CLASS_CONSUMABLE)
                continue;
            
            // Check if this is a useful consumable for the bot
            if (!IsUsefulConsumable(itemId))
                continue;
            
            // Check current stock
            uint32 currentStock = GetItemQuantityInInventory(itemId);
            uint32 desiredStock = CalculateDesiredConsumableStock(itemId);
            
            if (currentStock < desiredStock)
            {
                uint32 toCraft = desiredStock - currentStock;
                if (HasMaterialsForRecipe(recipeId, toCraft))
                {
                    AddCraftingTask(recipeId, toCraft, PlayerbotCraftingReason::PERSONAL_USE);
                }
            }
        }
    }
}

void PlayerbotProfessionManager::LogProfessionActivity(std::string const& action, std::string const& details)
{
    if (!details.empty())
        TC_LOG_DEBUG("playerbots", "PlayerbotProfessionManager ({}): {} - {}", 
                    _bot ? _bot->GetName() : "Unknown", action, details);
    else
        TC_LOG_DEBUG("playerbots", "PlayerbotProfessionManager ({}): {}", 
                    _bot ? _bot->GetName() : "Unknown", action);
}

// Utility helper methods

bool PlayerbotProfessionManager::IsProfessionSkill(uint32 skillId) const
{
    return skillId == SKILL_ALCHEMY || skillId == SKILL_BLACKSMITHING ||
           skillId == SKILL_LEATHERWORKING || skillId == SKILL_TAILORING ||
           skillId == SKILL_ENGINEERING || skillId == SKILL_ENCHANTING ||
           skillId == SKILL_JEWELCRAFTING || skillId == SKILL_INSCRIPTION ||
           skillId == SKILL_HERBALISM || skillId == SKILL_MINING ||
           skillId == SKILL_SKINNING || skillId == SKILL_COOKING ||
           skillId == SKILL_FIRST_AID || skillId == SKILL_FISHING;
}

PlayerbotProfessionType PlayerbotProfessionManager::DetermineProfessionType(uint32 professionId) const
{
    switch (professionId)
    {
        case SKILL_ALCHEMY:
        case SKILL_BLACKSMITHING:
        case SKILL_LEATHERWORKING:
        case SKILL_TAILORING:
        case SKILL_ENGINEERING:
        case SKILL_ENCHANTING:
        case SKILL_JEWELCRAFTING:
        case SKILL_INSCRIPTION:
            return PlayerbotProfessionType::PRIMARY_PRODUCTION;
            
        case SKILL_HERBALISM:
        case SKILL_MINING:
        case SKILL_SKINNING:
            return PlayerbotProfessionType::PRIMARY_GATHERING;
            
        case SKILL_COOKING:
        case SKILL_FIRST_AID:
        case SKILL_FISHING:
            return PlayerbotProfessionType::SECONDARY;
            
        default:
            return PlayerbotProfessionType::PRIMARY_PRODUCTION;
    }
}

std::string PlayerbotProfessionManager::GetProfessionName(uint32 professionId) const
{
    switch (professionId)
    {
        case SKILL_ALCHEMY: return "Alchemy";
        case SKILL_BLACKSMITHING: return "Blacksmithing";
        case SKILL_LEATHERWORKING: return "Leatherworking";
        case SKILL_TAILORING: return "Tailoring";
        case SKILL_ENGINEERING: return "Engineering";
        case SKILL_ENCHANTING: return "Enchanting";
        case SKILL_JEWELCRAFTING: return "Jewelcrafting";
        case SKILL_INSCRIPTION: return "Inscription";
        case SKILL_HERBALISM: return "Herbalism";
        case SKILL_MINING: return "Mining";
        case SKILL_SKINNING: return "Skinning";
        case SKILL_COOKING: return "Cooking";
        case SKILL_FIRST_AID: return "First Aid";
        case SKILL_FISHING: return "Fishing";
        default: return "Unknown";
    }
}

uint32 PlayerbotProfessionManager::GetMaxSkillValueForLevel(uint32 level) const
{
    if (level >= 60)
        return 300;
    else if (level >= 50)
        return 250;
    else if (level >= 40)
        return 200;
    else if (level >= 25)
        return 150;
    else if (level >= 10)
        return 75;
    else
        return 1;
}

std::vector<uint32> PlayerbotProfessionManager::GetKnownRecipes(uint32 professionId) const
{
    std::vector<uint32> recipes;
    
    if (!_bot)
        return recipes;
    
    // Iterate through all known spells and find crafting recipes
    PlayerSpellMap const& spells = _bot->GetSpellMap();
    for (auto const& spellPair : spells)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellPair.first);
        if (!spellInfo)
            continue;
        
        // Check if this spell is a crafting recipe for the given profession
        if (IsRecipeForProfession(spellPair.first, professionId))
        {
            recipes.push_back(spellPair.first);
        }
    }
    
    return recipes;
}

std::vector<uint32> PlayerbotProfessionManager::GetAvailableRecipes(uint32 professionId) const
{
    std::vector<uint32> recipes;
    // This would need to query available trainers, vendors, and drops
    // Implementation depends on recipe availability data
    return recipes;
}

bool PlayerbotProfessionManager::IsRecipeForProfession(uint32 recipeId, uint32 professionId) const
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(recipeId);
    if (!spellInfo)
        return false;
    
    // Check spell attributes and skill requirements
    return spellInfo->EquippedItemClass == ITEM_CLASS_RECIPE ||
           spellInfo->RequiresSpellFocus == professionId;
}

std::map<uint32, uint32> PlayerbotProfessionManager::GetRecipeMaterialQuantities(uint32 recipeId) const
{
    std::map<uint32, uint32> materials;
    
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(recipeId);
    if (!spellInfo)
        return materials;
    
    // Parse reagents from spell
    for (uint8 i = 0; i < MAX_SPELL_REAGENTS; ++i)
    {
        if (spellInfo->Reagent[i] > 0 && spellInfo->ReagentCount[i] > 0)
        {
            materials[spellInfo->Reagent[i]] = spellInfo->ReagentCount[i];
        }
    }
    
    return materials;
}

uint32 PlayerbotProfessionManager::GetItemQuantityInInventory(uint32 itemId) const
{
    if (!_bot)
        return 0;
    
    return _bot->GetItemCount(itemId, false);
}

uint32 PlayerbotProfessionManager::GetItemQuantityInBank(uint32 itemId) const
{
    if (!_bot)
        return 0;
    
    return _bot->GetItemCount(itemId, true) - _bot->GetItemCount(itemId, false);
}

#endif // WITH_PLAYERBOTS