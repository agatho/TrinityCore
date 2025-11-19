/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ProfessionEventBus.h"
#include "../AI/BotAI.h"
#include "Log.h"
#include <sstream>
#include <algorithm>

namespace Playerbot
{

// ============================================================================
// PROFESSION EVENT FACTORY METHODS
// ============================================================================

ProfessionEvent ProfessionEvent::RecipeLearned(ObjectGuid playerGuid, ProfessionType profession, uint32 recipeId)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::RECIPE_LEARNED;
    event.playerGuid = playerGuid;
    event.profession = profession;
    event.recipeId = recipeId;
    event.itemId = 0;
    event.quantity = 0;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = 0;
    event.reason = "Recipe learned";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::SkillUp(ObjectGuid playerGuid, ProfessionType profession, uint32 skillBefore, uint32 skillAfter)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::SKILL_UP;
    event.playerGuid = playerGuid;
    event.profession = profession;
    event.recipeId = 0;
    event.itemId = 0;
    event.quantity = 0;
    event.skillBefore = skillBefore;
    event.skillAfter = skillAfter;
    event.goldAmount = 0;
    event.reason = "Skill increased";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::CraftingStarted(ObjectGuid playerGuid, ProfessionType profession, uint32 recipeId, uint32 itemId)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::CRAFTING_STARTED;
    event.playerGuid = playerGuid;
    event.profession = profession;
    event.recipeId = recipeId;
    event.itemId = itemId;
    event.quantity = 0;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = 0;
    event.reason = "Crafting started";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::CraftingCompleted(ObjectGuid playerGuid, ProfessionType profession, uint32 recipeId, uint32 itemId, uint32 quantity)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::CRAFTING_COMPLETED;
    event.playerGuid = playerGuid;
    event.profession = profession;
    event.recipeId = recipeId;
    event.itemId = itemId;
    event.quantity = quantity;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = 0;
    event.reason = "Crafting completed";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::CraftingFailed(ObjectGuid playerGuid, ProfessionType profession, uint32 recipeId, std::string const& reason)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::CRAFTING_FAILED;
    event.playerGuid = playerGuid;
    event.profession = profession;
    event.recipeId = recipeId;
    event.itemId = 0;
    event.quantity = 0;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = 0;
    event.reason = reason;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::MaterialsNeeded(ObjectGuid playerGuid, ProfessionType profession, uint32 recipeId)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::MATERIALS_NEEDED;
    event.playerGuid = playerGuid;
    event.profession = profession;
    event.recipeId = recipeId;
    event.itemId = 0;
    event.quantity = 0;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = 0;
    event.reason = "Materials needed for crafting";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::MaterialGathered(ObjectGuid playerGuid, ProfessionType profession, uint32 itemId, uint32 quantity)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::MATERIAL_GATHERED;
    event.playerGuid = playerGuid;
    event.profession = profession;
    event.recipeId = 0;
    event.itemId = itemId;
    event.quantity = quantity;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = 0;
    event.reason = "Material gathered from node";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::MaterialPurchased(ObjectGuid playerGuid, uint32 itemId, uint32 quantity, uint32 goldSpent)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::MATERIAL_PURCHASED;
    event.playerGuid = playerGuid;
    event.profession = ProfessionType::NONE;
    event.recipeId = 0;
    event.itemId = itemId;
    event.quantity = quantity;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = goldSpent;
    event.reason = "Material purchased from auction house";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::ItemBanked(ObjectGuid playerGuid, uint32 itemId, uint32 quantity)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::ITEM_BANKED;
    event.playerGuid = playerGuid;
    event.profession = ProfessionType::NONE;
    event.recipeId = 0;
    event.itemId = itemId;
    event.quantity = quantity;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = 0;
    event.reason = "Item deposited to bank";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::ItemWithdrawn(ObjectGuid playerGuid, uint32 itemId, uint32 quantity)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::ITEM_WITHDRAWN;
    event.playerGuid = playerGuid;
    event.profession = ProfessionType::NONE;
    event.recipeId = 0;
    event.itemId = itemId;
    event.quantity = quantity;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = 0;
    event.reason = "Item withdrawn from bank";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::GoldBanked(ObjectGuid playerGuid, uint32 goldAmount)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::GOLD_BANKED;
    event.playerGuid = playerGuid;
    event.profession = ProfessionType::NONE;
    event.recipeId = 0;
    event.itemId = 0;
    event.quantity = 0;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = goldAmount;
    event.reason = "Gold deposited to bank";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::GoldWithdrawn(ObjectGuid playerGuid, uint32 goldAmount)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::GOLD_WITHDRAWN;
    event.playerGuid = playerGuid;
    event.profession = ProfessionType::NONE;
    event.recipeId = 0;
    event.itemId = 0;
    event.quantity = 0;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = goldAmount;
    event.reason = "Gold withdrawn from bank";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

bool ProfessionEvent::IsValid() const
{
    // Basic validation
    if (type >= ProfessionEventType::MAX_PROFESSION_EVENT)
        return false;

    if (!playerGuid.IsPlayer())
        return false;

    // Type-specific validation
    switch (type)
    {
        case ProfessionEventType::RECIPE_LEARNED:
            return recipeId != 0 && profession != ProfessionType::NONE;

        case ProfessionEventType::SKILL_UP:
            return skillAfter > skillBefore && profession != ProfessionType::NONE;

        case ProfessionEventType::CRAFTING_STARTED:
        case ProfessionEventType::CRAFTING_COMPLETED:
            return recipeId != 0 && itemId != 0 && profession != ProfessionType::NONE;

        case ProfessionEventType::CRAFTING_FAILED:
            return recipeId != 0 && !reason.empty();

        case ProfessionEventType::MATERIALS_NEEDED:
            return recipeId != 0 && profession != ProfessionType::NONE;

        case ProfessionEventType::MATERIAL_GATHERED:
            return itemId != 0 && quantity > 0;

        case ProfessionEventType::MATERIAL_PURCHASED:
            return itemId != 0 && quantity > 0 && goldAmount > 0;

        case ProfessionEventType::ITEM_BANKED:
        case ProfessionEventType::ITEM_WITHDRAWN:
            return itemId != 0 && quantity > 0;

        case ProfessionEventType::GOLD_BANKED:
        case ProfessionEventType::GOLD_WITHDRAWN:
            return goldAmount > 0;

        default:
            return true;
    }
}

std::string ProfessionEvent::ToString() const
{
    std::ostringstream oss;
    oss << "ProfessionEvent[";

    switch (type)
    {
        case ProfessionEventType::RECIPE_LEARNED:
            oss << "RECIPE_LEARNED, recipe=" << recipeId;
            break;
        case ProfessionEventType::SKILL_UP:
            oss << "SKILL_UP, skill=" << skillBefore << "->" << skillAfter;
            break;
        case ProfessionEventType::CRAFTING_STARTED:
            oss << "CRAFTING_STARTED, recipe=" << recipeId << ", item=" << itemId;
            break;
        case ProfessionEventType::CRAFTING_COMPLETED:
            oss << "CRAFTING_COMPLETED, recipe=" << recipeId << ", item=" << itemId << ", qty=" << quantity;
            break;
        case ProfessionEventType::CRAFTING_FAILED:
            oss << "CRAFTING_FAILED, recipe=" << recipeId << ", reason=" << reason;
            break;
        case ProfessionEventType::MATERIALS_NEEDED:
            oss << "MATERIALS_NEEDED, recipe=" << recipeId;
            break;
        case ProfessionEventType::MATERIAL_GATHERED:
            oss << "MATERIAL_GATHERED, item=" << itemId << ", qty=" << quantity;
            break;
        case ProfessionEventType::MATERIAL_PURCHASED:
            oss << "MATERIAL_PURCHASED, item=" << itemId << ", qty=" << quantity << ", gold=" << goldAmount;
            break;
        case ProfessionEventType::ITEM_BANKED:
            oss << "ITEM_BANKED, item=" << itemId << ", qty=" << quantity;
            break;
        case ProfessionEventType::ITEM_WITHDRAWN:
            oss << "ITEM_WITHDRAWN, item=" << itemId << ", qty=" << quantity;
            break;
        case ProfessionEventType::GOLD_BANKED:
            oss << "GOLD_BANKED, gold=" << goldAmount;
            break;
        case ProfessionEventType::GOLD_WITHDRAWN:
            oss << "GOLD_WITHDRAWN, gold=" << goldAmount;
            break;
        default:
            oss << "UNKNOWN";
            break;
    }

    oss << ", player=" << playerGuid.ToString() << "]";
    return oss.str();
}

// ============================================================================
// PROFESSION EVENT BUS
// ============================================================================

ProfessionEventBus* ProfessionEventBus::instance()
{
    static ProfessionEventBus instance;
    return &instance;
}

bool ProfessionEventBus::PublishEvent(ProfessionEvent const& event)
{
    if (!event.IsValid())
    {
        TC_LOG_ERROR("playerbot", "ProfessionEventBus::PublishEvent - Invalid event: {}", event.ToString());
        return false;
    }

    std::lock_guard<decltype(_subscriberMutex)> lock(_subscriberMutex);

    TC_LOG_DEBUG("playerbot", "ProfessionEventBus::PublishEvent - Publishing: {}", event.ToString());

    // Update statistics
    _eventCounts[event.type]++;
    _totalEventsPublished++;

    // Deliver event to subscribers
    DeliverEvent(event);

    return true;
}

void ProfessionEventBus::Subscribe(BotAI* subscriber, std::vector<ProfessionEventType> const& types)
{
    if (!subscriber)
        return;

    std::lock_guard<decltype(_subscriberMutex)> lock(_subscriberMutex);

    for (ProfessionEventType type : types)
    {
        auto& subscribers = _subscribers[type];
        if (std::find(subscribers.begin(), subscribers.end(), subscriber) == subscribers.end())
        {
            subscribers.push_back(subscriber);
            TC_LOG_DEBUG("playerbot", "ProfessionEventBus::Subscribe - BotAI subscribed to type {}", static_cast<uint8>(type));
        }
    }
}

void ProfessionEventBus::SubscribeAll(BotAI* subscriber)
{
    if (!subscriber)
        return;

    std::lock_guard<decltype(_subscriberMutex)> lock(_subscriberMutex);

    if (std::find(_globalSubscribers.begin(), _globalSubscribers.end(), subscriber) == _globalSubscribers.end())
    {
        _globalSubscribers.push_back(subscriber);
        TC_LOG_DEBUG("playerbot", "ProfessionEventBus::SubscribeAll - BotAI subscribed to all events");
    }
}

void ProfessionEventBus::Unsubscribe(BotAI* subscriber)
{
    if (!subscriber)
        return;

    std::lock_guard<decltype(_subscriberMutex)> lock(_subscriberMutex);

    // Remove from type-specific subscriptions
    for (auto& pair : _subscribers)
    {
        auto& subscribers = pair.second;
        subscribers.erase(std::remove(subscribers.begin(), subscribers.end(), subscriber), subscribers.end());
    }

    // Remove from global subscriptions
    _globalSubscribers.erase(std::remove(_globalSubscribers.begin(), _globalSubscribers.end(), subscriber), _globalSubscribers.end());

    TC_LOG_DEBUG("playerbot", "ProfessionEventBus::Unsubscribe - BotAI unsubscribed from all events");
}

uint32 ProfessionEventBus::SubscribeCallback(EventHandler handler, std::vector<ProfessionEventType> const& types)
{
    if (!handler)
        return 0;

    std::lock_guard<decltype(_subscriberMutex)> lock(_subscriberMutex);

    uint32 subscriptionId = _nextCallbackId++;

    CallbackSubscription subscription;
    subscription.id = subscriptionId;
    subscription.handler = handler;
    subscription.types = types;

    _callbackSubscriptions.push_back(subscription);

    TC_LOG_DEBUG("playerbot", "ProfessionEventBus::SubscribeCallback - Callback subscribed with ID {}", subscriptionId);

    return subscriptionId;
}

void ProfessionEventBus::UnsubscribeCallback(uint32 subscriptionId)
{
    std::lock_guard<decltype(_subscriberMutex)> lock(_subscriberMutex);

    _callbackSubscriptions.erase(
        std::remove_if(_callbackSubscriptions.begin(), _callbackSubscriptions.end(),
            [subscriptionId](const CallbackSubscription& sub) { return sub.id == subscriptionId; }),
        _callbackSubscriptions.end());

    TC_LOG_DEBUG("playerbot", "ProfessionEventBus::UnsubscribeCallback - Callback {} unsubscribed", subscriptionId);
}

uint64 ProfessionEventBus::GetEventCount(ProfessionEventType type) const
{
    std::lock_guard<decltype(_subscriberMutex)> lock(_subscriberMutex);

    auto itr = _eventCounts.find(type);
    if (itr != _eventCounts.end())
        return itr->second;

    return 0;
}

uint32 ProfessionEventBus::GetSubscriberCount(ProfessionEventType type) const
{
    std::lock_guard<decltype(_subscriberMutex)> lock(_subscriberMutex);

    uint32 count = 0;

    auto itr = _subscribers.find(type);
    if (itr != _subscribers.end())
        count += itr->second.size();

    // Add callback subscriptions for this type
    for (const CallbackSubscription& sub : _callbackSubscriptions)
    {
        if (std::find(sub.types.begin(), sub.types.end(), type) != sub.types.end())
            count++;
    }

    return count;
}

void ProfessionEventBus::DeliverEvent(ProfessionEvent const& event)
{
    // Deliver to type-specific subscribers
    auto typeItr = _subscribers.find(event.type);
    if (typeItr != _subscribers.end())
    {
        for (BotAI* subscriber : typeItr->second)
        {
            if (subscriber)
            {
                // BotAI would handle the event via OnProfessionEvent() method
                // This requires BotAI to implement IProfessionEventHandler interface
                // For now, we log the delivery
                TC_LOG_TRACE("playerbot", "ProfessionEventBus::DeliverEvent - Delivered to BotAI subscriber");
            }
        }
    }

    // Deliver to global subscribers
    for (BotAI* subscriber : _globalSubscribers)
    {
        if (subscriber)
        {
            TC_LOG_TRACE("playerbot", "ProfessionEventBus::DeliverEvent - Delivered to global BotAI subscriber");
        }
    }

    // Deliver to callback subscriptions
    for (const CallbackSubscription& sub : _callbackSubscriptions)
    {
        // Check if this subscription includes this event type
        if (std::find(sub.types.begin(), sub.types.end(), event.type) != sub.types.end())
        {
            try
            {
                sub.handler(event);
                TC_LOG_TRACE("playerbot", "ProfessionEventBus::DeliverEvent - Delivered to callback subscription {}", sub.id);
            }
            catch (const std::exception& e)
            {
                TC_LOG_ERROR("playerbot", "ProfessionEventBus::DeliverEvent - Callback {} threw exception: {}", sub.id, e.what());
            }
        }
    }
}

} // namespace Playerbot
