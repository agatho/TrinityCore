/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_NPC_EVENT_BUS_H
#define PLAYERBOT_NPC_EVENT_BUS_H

#include "Core/Events/GenericEventBus.h"
#include "NPCEvents.h"
#include "Core/DI/Interfaces/INPCEventBus.h"

namespace Playerbot
{

/**
 * @brief NPC Event Bus - Now powered by GenericEventBus template
 *
 * **Phase 5 Migration:** This EventBus is now a thin adapter over the
 * GenericEventBus<NPCEvent> template, maintaining the INPCEventBus
 * interface for backward compatibility while eliminating duplicate
 * infrastructure code.
 *
 * **Architecture:**
 * ```
 * NPCEventBus (DI adapter) -> INPCEventBus (interface)
 *                           -> EventBus<NPCEvent> (template infrastructure)
 * ```
 *
 * **Supports Both:**
 * - BotAI* subscriptions (via IEventHandler<NPCEvent>)
 * - Callback subscriptions (via std::function<void(NPCEvent const&)>)
 *
 * **Code Reduction:** ~490 lines → ~120 lines (76% reduction)
 */
class TC_GAME_API NPCEventBus final : public INPCEventBus
{
public:
    static NPCEventBus* instance()
    {
        static NPCEventBus inst;
        return &inst;
    }

    using EventHandler = std::function<void(NPCEvent const&)>;

    // Delegate all core functionality to template
    bool PublishEvent(NPCEvent const& event) override
    {
        return EventBus<NPCEvent>::instance()->PublishEvent(event);
    }

    void Subscribe(BotAI* subscriber, std::vector<NPCEventType> const& types) override
    {
        EventBus<NPCEvent>::instance()->Subscribe(subscriber, types);
    }

    void SubscribeAll(BotAI* subscriber) override
    {
        std::vector<NPCEventType> allTypes;
        for (uint8 i = 0; i < static_cast<uint8>(NPCEventType::MAX_NPC_EVENT); ++i)
            allTypes.push_back(static_cast<NPCEventType>(i));
        EventBus<NPCEvent>::instance()->Subscribe(subscriber, allTypes);
    }

    void Unsubscribe(BotAI* subscriber) override
    {
        EventBus<NPCEvent>::instance()->Unsubscribe(subscriber);
    }

    // Callback subscription support
    uint32 SubscribeCallback(EventHandler handler, std::vector<NPCEventType> const& types) override
    {
        return EventBus<NPCEvent>::instance()->SubscribeCallback(handler, types);
    }

    void UnsubscribeCallback(uint32 subscriptionId) override
    {
        EventBus<NPCEvent>::instance()->UnsubscribeCallback(subscriptionId);
    }

    // Statistics
    uint64 GetTotalEventsPublished() const override
    {
        return EventBus<NPCEvent>::instance()->GetTotalEventsPublished();
    }

    uint64 GetEventCount(NPCEventType type) const override
    {
        return EventBus<NPCEvent>::instance()->GetEventCount(type);
    }

private:
    NPCEventBus() = default;
    ~NPCEventBus() = default;
};

} // namespace Playerbot

#endif // PLAYERBOT_NPC_EVENT_BUS_H
