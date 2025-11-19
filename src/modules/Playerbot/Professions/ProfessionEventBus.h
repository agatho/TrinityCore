/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * PROFESSION EVENT BUS - Phase 5 Template Migration
 *
 * **Phase 5 Migration:** This EventBus is now a thin adapter over the
 * GenericEventBus<ProfessionEvent> template, eliminating ~220 lines of
 * duplicate infrastructure code while maintaining 100% backward compatibility.
 *
 * **Architecture:**
 * ```
 * ProfessionEventBus (adapter) -> EventBus<ProfessionEvent> (template infrastructure)
 * ```
 *
 * **Code Reduction:** ~440 lines (header + impl) → ~120 lines header only (73% reduction)
 *
 * **Event Types:**
 * - Recipe learning and profession skill progression
 * - Crafting lifecycle (started, completed, failed)
 * - Material management (gathering, purchasing)
 * - Banking operations (item and gold transactions)
 *
 * **Subscribers:**
 * - BotAI: Core bot event handling
 * - ProfessionManager: Recipe learning, skill tracking
 * - GatheringMaterialsBridge: Material gathering coordination
 * - AuctionMaterialsBridge: Material sourcing decisions
 * - BankingManager: Banking automation
 */

#ifndef PLAYERBOT_PROFESSION_EVENT_BUS_H
#define PLAYERBOT_PROFESSION_EVENT_BUS_H

#include "Core/Events/GenericEventBus.h"
#include "ProfessionEvents.h"
#include <functional>

namespace Playerbot
{

class BotAI;

/**
 * @brief Profession Event Bus - Template-powered event dispatch
 *
 * **Phase 5 Template Integration:**
 * Now delegates all core functionality to GenericEventBus<ProfessionEvent>,
 * maintaining the existing public API for backward compatibility.
 *
 * **Features:**
 * - Priority queue processing (HIGH priority for crafting/materials, LOW for skill-ups)
 * - Event expiry (events expire after 30 seconds to 30 minutes based on type)
 * - Type-safe event handling via IEventHandler<ProfessionEvent> interface
 * - Thread-safe subscription management
 * - Callback-based subscriptions for non-BotAI systems
 *
 * **Migration Note:**
 * The .cpp implementation file has been removed - all infrastructure is provided by
 * the GenericEventBus template.
 */
class TC_GAME_API ProfessionEventBus final
{
public:
    static ProfessionEventBus* instance()
    {
        static ProfessionEventBus inst;
        return &inst;
    }

    /**
     * @brief Event handler callback type for non-BotAI subscribers
     */
    using EventHandler = std::function<void(ProfessionEvent const&)>;

    // ========================================================================
    // EVENT PUBLISHING
    // ========================================================================

    /**
     * @brief Publish profession event to all subscribers
     * @param event The profession event to publish
     * @return true if event was valid and published, false otherwise
     *
     * Events are validated via ProfessionEvent::IsValid() before publishing.
     * Events are ordered by priority (HIGH > MEDIUM > LOW > BATCH) and timestamp.
     */
    bool PublishEvent(ProfessionEvent const& event)
    {
        return EventBus<ProfessionEvent>::instance()->PublishEvent(event);
    }

    // ========================================================================
    // BOTAI SUBSCRIPTION
    // ========================================================================

    /**
     * @brief Subscribe BotAI to specific profession event types
     * @param subscriber BotAI instance (must inherit from IEventHandler<ProfessionEvent>)
     * @param types Vector of event types to subscribe to
     *
     * BotAI will receive events via HandleEvent(ProfessionEvent const&) interface.
     */
    void Subscribe(BotAI* subscriber, std::vector<ProfessionEventType> const& types)
    {
        EventBus<ProfessionEvent>::instance()->Subscribe(subscriber, types);
    }

    /**
     * @brief Subscribe BotAI to all profession event types
     * @param subscriber BotAI instance (must inherit from IEventHandler<ProfessionEvent>)
     */
    void SubscribeAll(BotAI* subscriber)
    {
        std::vector<ProfessionEventType> allTypes;
        for (uint8 i = 0; i < static_cast<uint8>(ProfessionEventType::MAX_PROFESSION_EVENT); ++i)
            allTypes.push_back(static_cast<ProfessionEventType>(i));
        EventBus<ProfessionEvent>::instance()->Subscribe(subscriber, allTypes);
    }

    /**
     * @brief Unsubscribe BotAI from all profession events
     * @param subscriber BotAI instance to unsubscribe
     */
    void Unsubscribe(BotAI* subscriber)
    {
        EventBus<ProfessionEvent>::instance()->Unsubscribe(subscriber);
    }

    // ========================================================================
    // CALLBACK SUBSCRIPTION (for non-BotAI systems)
    // ========================================================================

    /**
     * @brief Subscribe callback to specific profession event types
     * @param handler Callback function to invoke for matching events
     * @param types Vector of event types to subscribe to
     * @return Subscription ID for later unsubscribe
     *
     * Use this for non-BotAI systems (managers, bridges) that need event notifications.
     */
    uint32 SubscribeCallback(EventHandler handler, std::vector<ProfessionEventType> const& types)
    {
        return EventBus<ProfessionEvent>::instance()->SubscribeCallback(
            [handler](ProfessionEvent const& event) { handler(event); },
            types
        );
    }

    /**
     * @brief Unsubscribe callback by subscription ID
     * @param subscriptionId ID returned from SubscribeCallback
     */
    void UnsubscribeCallback(uint32 subscriptionId)
    {
        EventBus<ProfessionEvent>::instance()->UnsubscribeCallback(subscriptionId);
    }

    // ========================================================================
    // STATISTICS & DIAGNOSTICS
    // ========================================================================

    /**
     * @brief Get total number of profession events published (all time)
     */
    uint64 GetTotalEventsPublished() const
    {
        return EventBus<ProfessionEvent>::instance()->GetTotalEventsProcessed();
    }

    /**
     * @brief Get count of specific profession event type published
     * @param type Event type to query
     */
    uint64 GetEventCount(ProfessionEventType type) const
    {
        return EventBus<ProfessionEvent>::instance()->GetEventTypeCount(type);
    }

    /**
     * @brief Get number of subscribers for specific event type
     * @param type Event type to query
     */
    uint32 GetSubscriberCount(ProfessionEventType type) const
    {
        return EventBus<ProfessionEvent>::instance()->GetSubscriberCountForType(type);
    }

    /**
     * @brief Get total number of active subscriptions (all types)
     */
    uint32 GetTotalSubscriberCount() const
    {
        return EventBus<ProfessionEvent>::instance()->GetSubscriberCount();
    }

    /**
     * @brief Get number of pending events in queue
     */
    uint32 GetPendingEventCount() const
    {
        return EventBus<ProfessionEvent>::instance()->GetQueueSize();
    }

private:
    ProfessionEventBus() = default;
    ~ProfessionEventBus() = default;

    // Non-copyable
    ProfessionEventBus(ProfessionEventBus const&) = delete;
    ProfessionEventBus& operator=(ProfessionEventBus const&) = delete;
};

} // namespace Playerbot

#endif // PLAYERBOT_PROFESSION_EVENT_BUS_H
