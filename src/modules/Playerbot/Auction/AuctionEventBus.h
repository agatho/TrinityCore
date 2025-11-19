/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_AUCTION_EVENT_BUS_H
#define PLAYERBOT_AUCTION_EVENT_BUS_H

#include "Core/Events/GenericEventBus.h"
#include "AuctionEvents.h"
#include "Core/DI/Interfaces/IAuctionEventBus.h"

namespace Playerbot
{

/**
 * @brief Auction Event Bus - Now powered by GenericEventBus template
 *
 * **Phase 5 Migration:** This EventBus is now a thin adapter over the
 * GenericEventBus<AuctionEvent> template, maintaining the IAuctionEventBus
 * interface for backward compatibility while eliminating duplicate
 * infrastructure code.
 *
 * **Architecture:**
 * ```
 * AuctionEventBus (DI adapter) -> IAuctionEventBus (interface)
 *                               -> EventBus<AuctionEvent> (template infrastructure)
 * ```
 *
 * **Supports Both:**
 * - BotAI* subscriptions (via IEventHandler<AuctionEvent>)
 * - Callback subscriptions (via std::function<void(AuctionEvent const&)>)
 *
 * **Code Reduction:** ~450 lines → ~120 lines (73% reduction)
 */
class TC_GAME_API AuctionEventBus final : public IAuctionEventBus
{
public:
    static AuctionEventBus* instance()
    {
        static AuctionEventBus inst;
        return &inst;
    }

    using EventHandler = std::function<void(AuctionEvent const&)>;

    // Delegate all core functionality to template
    bool PublishEvent(AuctionEvent const& event) override
    {
        return EventBus<AuctionEvent>::instance()->PublishEvent(event);
    }

    void Subscribe(BotAI* subscriber, std::vector<AuctionEventType> const& types) override
    {
        EventBus<AuctionEvent>::instance()->Subscribe(subscriber, types);
    }

    void SubscribeAll(BotAI* subscriber) override
    {
        std::vector<AuctionEventType> allTypes;
        for (uint8 i = 0; i < static_cast<uint8>(AuctionEventType::MAX_AUCTION_EVENT); ++i)
            allTypes.push_back(static_cast<AuctionEventType>(i));
        EventBus<AuctionEvent>::instance()->Subscribe(subscriber, allTypes);
    }

    void Unsubscribe(BotAI* subscriber) override
    {
        EventBus<AuctionEvent>::instance()->Unsubscribe(subscriber);
    }

    // Callback subscription support
    uint32 SubscribeCallback(EventHandler handler, std::vector<AuctionEventType> const& types) override
    {
        return EventBus<AuctionEvent>::instance()->SubscribeCallback(handler, types);
    }

    void UnsubscribeCallback(uint32 subscriptionId) override
    {
        EventBus<AuctionEvent>::instance()->UnsubscribeCallback(subscriptionId);
    }

    // Statistics
    uint64 GetTotalEventsPublished() const override
    {
        return EventBus<AuctionEvent>::instance()->GetTotalEventsPublished();
    }

    uint64 GetEventCount(AuctionEventType type) const override
    {
        return EventBus<AuctionEvent>::instance()->GetEventCount(type);
    }

private:
    AuctionEventBus() = default;
    ~AuctionEventBus() = default;
};

} // namespace Playerbot

#endif // PLAYERBOT_AUCTION_EVENT_BUS_H
