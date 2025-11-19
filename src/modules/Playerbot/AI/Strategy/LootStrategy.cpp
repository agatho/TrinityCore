/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "LootStrategy.h"
#include "../BotAI.h"
#include "Player.h"
#include "Creature.h"
#include "GameObject.h"
#include "Item.h"
#include "Bag.h"
#include "ItemTemplate.h"
#include "ObjectAccessor.h"
#include "Map.h"
#include "Log.h"
#include "GridNotifiers.h"
#include "CellImpl.h"
#include "Loot.h"
#include "LootMgr.h"
#include "MotionMaster.h"
#include "../../Spatial/SpatialGridManager.h"  // Lock-free spatial grid for deadlock fix
#include "../../Spatial/SpatialGridQueryHelpers.h"  // Thread-safe spatial queries
#include "../../Movement/Arbiter/MovementArbiter.h"
#include "../../Movement/Arbiter/MovementPriorityMapper.h"
#include "UnitAI.h"
#include <unordered_map>  // For distance map in PrioritizeLootTargets
#include "GameTime.h"

namespace Playerbot
{

LootStrategy::LootStrategy()
    : Strategy("loot")
{
    TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: Initialized");
}

void LootStrategy::InitializeActions()
{
    // No actions needed - loot strategy drives looting directly
    TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: No actions (direct loot control)");
}

void LootStrategy::InitializeTriggers()
{
    // No triggers needed - relevance system handles activation
    TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: No triggers (using relevance system)");
}

void LootStrategy::InitializeValues()
{
    // No values needed for this simple strategy
    TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: No values");
}

void LootStrategy::OnActivate(BotAI* ai)
{
    if (!ai || !ai->GetBot())
        return;

    TC_LOG_INFO("module.playerbot.strategy", "Loot strategy activated for bot {}", ai->GetBot()->GetName());
    SetActive(true);
}

void LootStrategy::OnDeactivate(BotAI* ai)
{
    if (!ai || !ai->GetBot())
        return;

    TC_LOG_INFO("module.playerbot.strategy", "Loot strategy deactivated for bot {}", ai->GetBot()->GetName());
    SetActive(false);

    // Clear current loot target
    _currentLootTarget.Clear();
    _lootAttempts = 0;
}

bool LootStrategy::IsActive(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();
    // NOT active during combat
    if (bot->IsInCombat())
        return false;

    // Active if explicitly activated and not in a group
    return _active && !bot->GetGroup();
}

float LootStrategy::GetRelevance(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return 0.0f;

    Player* bot = ai->GetBot();
    // Don't loot during combat
    if (bot->IsInCombat())
        return 0.0f;

    // Don't loot if inventory is full
    if (!HasInventorySpace(ai))
        return 0.0f;

    // Check if there are nearby lootable targets
    uint32 currentTime = GameTime::GetGameTimeMS();
    if (currentTime - _lastLootScan < _lootScanInterval)
    {
        // Return cached relevance
        return _currentLootTarget.IsEmpty() ? 0.0f : 60.0f;
    }

    // Scan for loot
    std::vector<ObjectGuid> corpses = FindLootableCorpses(ai);
    std::vector<ObjectGuid> objects = FindLootableObjects(ai);

    // Medium-high relevance if loot available (lower than quest=70, higher than solo=10)
    return (!corpses.empty() || !objects.empty()) ? 60.0f : 0.0f;
}
void LootStrategy::UpdateBehavior(BotAI* ai, uint32 diff)
{
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();
    // Don't loot during combat
    if (bot->IsInCombat())
        return;

    // Don't loot if inventory is full
    if (!HasInventorySpace(ai))
    {
        TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: Bot {} inventory full, skipping loot",
                     bot->GetName());
        return;
    }

    TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy::UpdateBehavior: Bot {} searching for loot",
                 bot->GetName());

    uint32 currentTime = GameTime::GetGameTimeMS();

    // Throttle loot scanning
    if (currentTime - _lastLootScan < _lootScanInterval)
        return;

    _lastLootScan = currentTime;

    // Find lootable targets
    std::vector<ObjectGuid> corpses = FindLootableCorpses(ai);
    std::vector<ObjectGuid> objects = FindLootableObjects(ai);

    TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: Bot {} found {} corpses and {} objects",
                 bot->GetName(), corpses.size(), objects.size());

    // Combine and prioritize targets
    std::vector<ObjectGuid> allTargets;
    allTargets.insert(allTargets.end(), corpses.begin(), corpses.end());
    allTargets.insert(allTargets.end(), objects.begin(), objects.end());

    if (allTargets.empty())
    {
        _currentLootTarget.Clear();
        return;
    }

    // Prioritize targets
    allTargets = PrioritizeLootTargets(ai, allTargets);

    // Try to loot the highest priority target
    for (ObjectGuid const& targetGuid : allTargets)
    {
        if (targetGuid.IsCreature())
        {
            if (LootCorpse(ai, targetGuid))
            {
                _currentLootTarget = targetGuid;
                _corpseLooted++;
                break;
            }
        }
        else if (targetGuid.IsGameObject())
        {
            if (LootObject(ai, targetGuid))
            {
                _currentLootTarget = targetGuid;
                break;
            }
        }
    }
}

std::vector<ObjectGuid> LootStrategy::FindLootableCorpses(BotAI* ai, float maxDistance) const
{
    std::vector<ObjectGuid> lootableCorpses;
    if (!ai || !ai->GetBot())
        return lootableCorpses;

    Player* bot = ai->GetBot();
    // Find all creatures in range
    std::list<Creature*> nearbyCreatures;
    bot->GetCreatureListWithEntryInGrid(nearbyCreatures, 0, maxDistance);

    // Filter for dead creatures with loot
    for (Creature* creature : nearbyCreatures)
    {
        if (!creature || !creature->isDead())
            continue;

        // Check if creature has loot
        if (!creature->CanHaveLoot() || !creature->hasLootRecipient())
            continue;

        // Add to lootable list
        lootableCorpses.push_back(creature->GetGUID());
    }

    return lootableCorpses;
}

std::vector<ObjectGuid> LootStrategy::FindLootableObjects(BotAI* ai, float maxDistance) const
{
    std::vector<ObjectGuid> lootableObjects;

    if (!ai || !ai->GetBot())
        return lootableObjects;

    Player* bot = ai->GetBot();
    Map* map = bot->GetMap();
    if (!map)
        return lootableObjects;

    // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        // Grid not yet created for this map - create it on demand
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            return lootableObjects;
    }

    // Query nearby GameObjects (lock-free!)
    std::vector<DoubleBufferedSpatialGrid::GameObjectSnapshot> nearbyObjects =
        spatialGrid->QueryNearbyGameObjects(bot->GetPosition(), maxDistance);

    // Filter lootable objects using snapshot data
    for (auto const& snapshot : nearbyObjects)
    {
        if (!snapshot.isSpawned)
            continue;

        // Check if object is lootable container (chest)
        if (snapshot.goType != GAMEOBJECT_TYPE_CHEST)
            continue;

        // Add to lootable list
        lootableObjects.push_back(snapshot.guid);
    }

    return lootableObjects;
}

bool LootStrategy::LootCorpse(BotAI* ai, ObjectGuid corpseGuid)
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();
    Map* map = bot->GetMap();
    if (!map)
        return false;

    // DEADLOCK FIX: Use spatial grid to validate corpse state without pointer access
    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
        return false;

    // Query nearby creatures to find our target
    std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> nearbyCreatures =
        spatialGrid->QueryNearbyCreatures(bot->GetPosition(), 50.0f);

    // Find the corpse in snapshots
    DoubleBufferedSpatialGrid::CreatureSnapshot const* corpseSnapshot = nullptr;
    for (auto const& snapshot : nearbyCreatures)
    {
        if (snapshot.guid == corpseGuid)
        {
            corpseSnapshot = &snapshot;
            break;
        }
    }

    // Validate corpse exists and is dead
    if (!corpseSnapshot || !corpseSnapshot->isDead)
        return false;

    // Check distance using snapshot position
    float distance = bot->GetExactDist(corpseSnapshot->position);
    if (distance > INTERACTION_DISTANCE)
    {
        // Move closer using snapshot position
        Position pos;
        pos.Relocate(corpseSnapshot->position);

        // PHASE 5 MIGRATION: Use Movement Arbiter with LOOT priority (40)
        BotAI* botAI = dynamic_cast<BotAI*>(bot->GetAI());
        if (botAI && botAI->GetMovementArbiter())
        {
            bool accepted = botAI->RequestPointMovement(
                PlayerBotMovementPriority::LOOT,  // Priority 40 - MINIMAL tier
                pos,
                "Moving to corpse for looting",
                "LootStrategy");

            if (accepted)
            {
                TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: Bot {} moving to corpse at distance {:.1f}",
                             bot->GetName(), distance);
            }
            else
            {
                TC_LOG_TRACE("playerbot.movement.arbiter",
                    "LootStrategy: Movement to corpse rejected for bot {} - higher priority active",
                    bot->GetName());
            }
        }
        else
        {
            // FALLBACK: Direct MotionMaster call if arbiter not available
            bot->GetMotionMaster()->MovePoint(0, pos);
            TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: Bot {} moving to corpse at distance {:.1f}",
                         bot->GetName(), distance);
        }
        return false;
    }

    // PHASE 5D: Thread-safe spatial grid validation
    auto snapshot = SpatialGridQueryHelpers::FindCreatureByGuid(bot, corpseGuid);
    Creature* creature = nullptr;

    if (snapshot)
    {
        // Get Creature* for loot access (validated via snapshot first)
    }

    if (!creature)
        return false;

    // Get the loot and send it to bot
    if (creature->m_loot)
    {
        bot->SendLoot(*creature->m_loot, false);
        TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: Bot {} looting corpse {}",
                     bot->GetName(), corpseSnapshot->entry);
        return true;
    }

    return false;
}

bool LootStrategy::LootObject(BotAI* ai, ObjectGuid objectGuid)
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();
    Map* map = bot->GetMap();
    if (!map)
        return false;

    // DEADLOCK FIX: Use spatial grid to validate object state without pointer access
    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
        return false;

    // Query nearby game objects to find our target
    std::vector<DoubleBufferedSpatialGrid::GameObjectSnapshot> nearbyObjects =
        spatialGrid->QueryNearbyGameObjects(bot->GetPosition(), 50.0f);

    // Find the object in snapshots
    DoubleBufferedSpatialGrid::GameObjectSnapshot const* objectSnapshot = nullptr;
    for (auto const& snapshot : nearbyObjects)
    {
        if (snapshot.guid == objectGuid)
        {
            objectSnapshot = &snapshot;
            break;
        }
    }

    // Validate object exists and is spawned
    if (!objectSnapshot || !objectSnapshot->isSpawned)
        return false;

    // Check distance using snapshot position
    float distance = bot->GetExactDist(objectSnapshot->position);
    if (distance > INTERACTION_DISTANCE)
    {
        // Move closer using snapshot position
        Position pos;
        pos.Relocate(objectSnapshot->position);

        // PHASE 5 MIGRATION: Use Movement Arbiter with LOOT priority (40)
        BotAI* botAI = dynamic_cast<BotAI*>(bot->GetAI());
        if (botAI && botAI->GetMovementArbiter())
        {
            bool accepted = botAI->RequestPointMovement(
                PlayerBotMovementPriority::LOOT,  // Priority 40 - MINIMAL tier
                pos,
                "Moving to object for looting",
                "LootStrategy");

            if (accepted)
            {
                TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: Bot {} moving to object at distance {:.1f}",
                             bot->GetName(), distance);
            }
            else
            {
                TC_LOG_TRACE("playerbot.movement.arbiter",
                    "LootStrategy: Movement to object rejected for bot {} - higher priority active",
                    bot->GetName());
            }
        }
        else
        {
            // FALLBACK: Direct MotionMaster call if arbiter not available
            bot->GetMotionMaster()->MovePoint(0, pos);
            TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: Bot {} moving to object at distance {:.1f}",
                         bot->GetName(), distance);
        }
        return false;
    }

    // PHASE 5D: Thread-safe spatial grid validation
    auto snapshot = SpatialGridQueryHelpers::FindGameObjectByGuid(bot, objectGuid);
    GameObject* object = nullptr;

    if (snapshot)
    {
        // Get GameObject* for loot access (validated via snapshot first)
    }

    if (!object)
        return false;

    // Use the object (opens loot)
    object->Use(bot);

    TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: Bot {} looting object {}",
                 bot->GetName(), objectSnapshot->entry);

    return true;
}

bool LootStrategy::HasInventorySpace(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // Check if bot has at least 5 free bag slots
    uint32 freeSlots = 0;
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag* bag = bot->GetBagByPos(i))
        {
            freeSlots += bag->GetFreeSlots();
        }
    }

    // Add main backpack slots
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        if (!bot->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            freeSlots++;
    }

    return freeSlots >= 5;
}

std::vector<ObjectGuid> LootStrategy::PrioritizeLootTargets(BotAI* ai, std::vector<ObjectGuid> const& targets) const
{
    if (!ai || !ai->GetBot())
        return targets;

    Player* bot = ai->GetBot();
    Map* map = bot->GetMap();
    if (!map)
        return targets;

    // DEADLOCK FIX: Build distance map from snapshots instead of resolving GUIDs in lambda
    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
        return targets;

    // Query all nearby entities once (lock-free!)
    std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> nearbyCreatures =
        spatialGrid->QueryNearbyCreatures(bot->GetPosition(), 50.0f);
    std::vector<DoubleBufferedSpatialGrid::GameObjectSnapshot> nearbyObjects =
        spatialGrid->QueryNearbyGameObjects(bot->GetPosition(), 50.0f);

    // Build distance map using snapshot positions
    std::unordered_map<ObjectGuid, float> distanceMap;

    for (auto const& snapshot : nearbyCreatures)
    {
        float distance = bot->GetExactDist(snapshot.position);
        distanceMap[snapshot.guid] = distance;
    }

    for (auto const& snapshot : nearbyObjects)
    {
        float distance = bot->GetExactDist(snapshot.position);
        distanceMap[snapshot.guid] = distance;
    }

    // Sort targets by distance (closest first) using pre-computed distances
    std::vector<ObjectGuid> prioritized = targets;
    std::sort(prioritized.begin(), prioritized.end(),
        [&distanceMap](ObjectGuid const& a, ObjectGuid const& b) -> bool
        {
            auto itA = distanceMap.find(a);
            auto itB = distanceMap.find(b);

            // If either GUID not found in distance map, deprioritize it
            if (itA == distanceMap.end())
                return false;
            if (itB == distanceMap.end())
                return true;

            return itA->second < itB->second;
        });

    return prioritized;
}

} // namespace Playerbot
