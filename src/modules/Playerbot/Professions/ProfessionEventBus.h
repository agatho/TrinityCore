/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * PROFESSION EVENT BUS
 *
 * Event-driven architecture for profession systems:
 * - OnRecipeLearned: Recipe added to profession
 * - OnSkillUp: Profession skill increased
 * - OnCraftingStarted: Bot begins crafting item
 * - OnCraftingCompleted: Bot finishes crafting item
 * - OnCraftingFailed: Crafting attempt failed
 * - OnMaterialsNeeded: Bot needs materials for recipe
 * - OnMaterialGathered: Bot gathered materials
 * - OnMaterialPurchased: Bot bought materials from AH
 * - OnItemBanked: Bot deposited item to bank
 * - OnItemWithdrawn: Bot withdrew item from bank
 * - OnGoldBanked: Bot deposited gold
 * - OnGoldWithdrawn: Bot withdrew gold
 *
 * Subscribers:
 * - ProfessionManager: Recipe learning, skill tracking
 * - GatheringMaterialsBridge: Material gathering coordination
 * - AuctionMaterialsBridge: Material sourcing decisions
 * - ProfessionAuctionBridge: Auction integration
 * - BankingManager: Banking automation
 *
 * Integration:
 * - Similar pattern to AuctionEventBus
 * - BotAI can subscribe to events
 * - Callback-based subscription for non-BotAI systems
 */

#ifndef PLAYERBOT_PROFESSION_EVENT_BUS_H
#define PLAYERBOT_PROFESSION_EVENT_BUS_H

#include "Define.h"
#include "Threading/LockHierarchy.h"
#include "ObjectGuid.h"
#include "ProfessionManager.h"
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>

namespace Playerbot
{

class BotAI;

/**
 * @brief Profession event types
 */
enum class ProfessionEventType : uint8
{
    RECIPE_LEARNED = 0,         // Bot learned new recipe
    SKILL_UP,                   // Profession skill increased
    CRAFTING_STARTED,           // Bot started crafting item
    CRAFTING_COMPLETED,         // Bot completed crafting item
    CRAFTING_FAILED,            // Crafting attempt failed
    MATERIALS_NEEDED,           // Bot needs materials for recipe
    MATERIAL_GATHERED,          // Bot gathered material from node
    MATERIAL_PURCHASED,         // Bot bought material from AH
    ITEM_BANKED,                // Bot deposited item to bank
    ITEM_WITHDRAWN,             // Bot withdrew item from bank
    GOLD_BANKED,                // Bot deposited gold to bank
    GOLD_WITHDRAWN,             // Bot withdrew gold from bank
    MAX_PROFESSION_EVENT
};

/**
 * @brief Profession event data
 */
struct ProfessionEvent
{
    ProfessionEventType type;
    ObjectGuid playerGuid;
    ProfessionType profession;
    uint32 recipeId;
    uint32 itemId;
    uint32 quantity;
    uint32 skillBefore;
    uint32 skillAfter;
    uint32 goldAmount;              // For banking events
    std::string reason;             // Human-readable event reason
    std::chrono::steady_clock::time_point timestamp;

    // Factory methods for each event type
    static ProfessionEvent RecipeLearned(ObjectGuid playerGuid, ProfessionType profession, uint32 recipeId);
    static ProfessionEvent SkillUp(ObjectGuid playerGuid, ProfessionType profession, uint32 skillBefore, uint32 skillAfter);
    static ProfessionEvent CraftingStarted(ObjectGuid playerGuid, ProfessionType profession, uint32 recipeId, uint32 itemId);
    static ProfessionEvent CraftingCompleted(ObjectGuid playerGuid, ProfessionType profession, uint32 recipeId, uint32 itemId, uint32 quantity);
    static ProfessionEvent CraftingFailed(ObjectGuid playerGuid, ProfessionType profession, uint32 recipeId, std::string const& reason);
    static ProfessionEvent MaterialsNeeded(ObjectGuid playerGuid, ProfessionType profession, uint32 recipeId);
    static ProfessionEvent MaterialGathered(ObjectGuid playerGuid, ProfessionType profession, uint32 itemId, uint32 quantity);
    static ProfessionEvent MaterialPurchased(ObjectGuid playerGuid, uint32 itemId, uint32 quantity, uint32 goldSpent);
    static ProfessionEvent ItemBanked(ObjectGuid playerGuid, uint32 itemId, uint32 quantity);
    static ProfessionEvent ItemWithdrawn(ObjectGuid playerGuid, uint32 itemId, uint32 quantity);
    static ProfessionEvent GoldBanked(ObjectGuid playerGuid, uint32 goldAmount);
    static ProfessionEvent GoldWithdrawn(ObjectGuid playerGuid, uint32 goldAmount);

    bool IsValid() const;
    std::string ToString() const;
};

/**
 * @brief Event bus for profession system events
 *
 * DESIGN: Singleton event bus following AuctionEventBus pattern
 * - Thread-safe event publishing and subscription
 * - Supports BotAI subscribers and callback-based subscribers
 * - Event statistics tracking
 * - Type-filtered subscriptions
 */
class TC_GAME_API ProfessionEventBus final
{
public:
    static ProfessionEventBus* instance();

    /**
     * Publish event to all subscribers
     */
    bool PublishEvent(ProfessionEvent const& event);

    /**
     * Event handler callback
     */
    using EventHandler = std::function<void(ProfessionEvent const&)>;

    // ========================================================================
    // BOTAI SUBSCRIPTION
    // ========================================================================

    /**
     * Subscribe BotAI to specific event types
     */
    void Subscribe(BotAI* subscriber, std::vector<ProfessionEventType> const& types);

    /**
     * Subscribe BotAI to all event types
     */
    void SubscribeAll(BotAI* subscriber);

    /**
     * Unsubscribe BotAI from all events
     */
    void Unsubscribe(BotAI* subscriber);

    // ========================================================================
    // CALLBACK SUBSCRIPTION (for non-BotAI systems)
    // ========================================================================

    /**
     * Subscribe callback to specific event types
     * Returns subscription ID for later unsubscribe
     */
    uint32 SubscribeCallback(EventHandler handler, std::vector<ProfessionEventType> const& types);

    /**
     * Unsubscribe callback by subscription ID
     */
    void UnsubscribeCallback(uint32 subscriptionId);

    // ========================================================================
    // STATISTICS
    // ========================================================================

    uint64 GetTotalEventsPublished() const { return _totalEventsPublished; }
    uint64 GetEventCount(ProfessionEventType type) const;

    /**
     * Get subscriber count for event type
     */
    uint32 GetSubscriberCount(ProfessionEventType type) const;

private:
    ProfessionEventBus() = default;
    ~ProfessionEventBus() = default;

    /**
     * Deliver event to all subscribed listeners
     */
    void DeliverEvent(ProfessionEvent const& event);

    // ========================================================================
    // SUBSCRIPTION STORAGE
    // ========================================================================

    // BotAI subscribers (type -> subscribers)
    std::unordered_map<ProfessionEventType, std::vector<BotAI*>> _subscribers;

    // Global subscribers (all events)
    std::vector<BotAI*> _globalSubscribers;

    // Callback subscriptions
    struct CallbackSubscription
    {
        uint32 id;
        EventHandler handler;
        std::vector<ProfessionEventType> types;
    };
    std::vector<CallbackSubscription> _callbackSubscriptions;
    uint32 _nextCallbackId = 1;

    // ========================================================================
    // STATISTICS
    // ========================================================================

    std::unordered_map<ProfessionEventType, uint64> _eventCounts;
    uint64 _totalEventsPublished = 0;

    // Thread safety
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::TRADE_MANAGER> _subscriberMutex;
};

} // namespace Playerbot

#endif // PLAYERBOT_PROFESSION_EVENT_BUS_H
