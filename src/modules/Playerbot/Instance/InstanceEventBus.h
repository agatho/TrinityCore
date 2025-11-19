/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_INSTANCE_EVENT_BUS_H
#define PLAYERBOT_INSTANCE_EVENT_BUS_H

#include "Core/Events/GenericEventBus.h"
#include "InstanceEvents.h"
#include "Core/DI/Interfaces/IInstanceEventBus.h"

namespace Playerbot
{

/**
 * @brief Instance Event Bus - Now powered by GenericEventBus template
 *
 * **Phase 5 Migration:** This EventBus is now a thin adapter over the
 * GenericEventBus<InstanceEvent> template, maintaining the IInstanceEventBus
 * interface for backward compatibility while eliminating duplicate
 * infrastructure code.
 *
 * **Architecture:**
 * ```
 * InstanceEventBus (DI adapter) -> IInstanceEventBus (interface)
 *                                -> EventBus<InstanceEvent> (template infrastructure)
 * ```
 *
 * **Supports Both:**
 * - BotAI* subscriptions (via IEventHandler<InstanceEvent>)
 * - Callback subscriptions (via std::function<void(InstanceEvent const&)>)
 *
 * **Code Reduction:** ~450 lines → ~120 lines (73% reduction)
 */
class TC_GAME_API InstanceEventBus final : public IInstanceEventBus
{
public:
    static InstanceEventBus* instance()
    {
        static InstanceEventBus inst;
        return &inst;
    }

    using EventHandler = std::function<void(InstanceEvent const&)>;

    // Delegate all core functionality to template
    bool PublishEvent(InstanceEvent const& event) override
    {
        return EventBus<InstanceEvent>::instance()->PublishEvent(event);
    }

    void Subscribe(BotAI* subscriber, std::vector<InstanceEventType> const& types) override
    {
        EventBus<InstanceEvent>::instance()->Subscribe(subscriber, types);
    }

    void SubscribeAll(BotAI* subscriber) override
    {
        std::vector<InstanceEventType> allTypes;
        for (uint8 i = 0; i < static_cast<uint8>(InstanceEventType::MAX_INSTANCE_EVENT); ++i)
            allTypes.push_back(static_cast<InstanceEventType>(i));
        EventBus<InstanceEvent>::instance()->Subscribe(subscriber, allTypes);
    }

    void Unsubscribe(BotAI* subscriber) override
    {
        EventBus<InstanceEvent>::instance()->Unsubscribe(subscriber);
    }

    // Callback subscription support
    uint32 SubscribeCallback(EventHandler handler, std::vector<InstanceEventType> const& types) override
    {
        return EventBus<InstanceEvent>::instance()->SubscribeCallback(handler, types);
    }

    void UnsubscribeCallback(uint32 subscriptionId) override
    {
        EventBus<InstanceEvent>::instance()->UnsubscribeCallback(subscriptionId);
    }

    // Statistics
    uint64 GetTotalEventsPublished() const override
    {
        return EventBus<InstanceEvent>::instance()->GetTotalEventsPublished();
    }

    uint64 GetEventCount(InstanceEventType type) const override
    {
        return EventBus<InstanceEvent>::instance()->GetEventCount(type);
    }

private:
    InstanceEventBus() = default;
    ~InstanceEventBus() = default;
};

} // namespace Playerbot

#endif // PLAYERBOT_INSTANCE_EVENT_BUS_H
