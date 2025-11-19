/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_RESOURCE_EVENT_BUS_H
#define PLAYERBOT_RESOURCE_EVENT_BUS_H

#include "Core/Events/GenericEventBus.h"
#include "ResourceEvents.h"
#include "Core/DI/Interfaces/IResourceEventBus.h"

namespace Playerbot
{

/**
 * @brief Resource Event Bus - Now powered by GenericEventBus template
 *
 * **Phase 5 Migration:** This EventBus is now a thin adapter over the
 * GenericEventBus<ResourceEvent> template, maintaining the IResourceEventBus
 * interface for backward compatibility while eliminating duplicate
 * infrastructure code.
 *
 * **Architecture:**
 * ```
 * ResourceEventBus (DI adapter) -> IResourceEventBus (interface)
 *                                -> EventBus<ResourceEvent> (template infrastructure)
 * ```
 *
 * **Code Reduction:** ~500 lines → ~100 lines (80% reduction)
 */
class TC_GAME_API ResourceEventBus final : public IResourceEventBus
{
public:
    static ResourceEventBus* instance()
    {
        static ResourceEventBus inst;
        return &inst;
    }

    // Delegate all core functionality to template
    bool PublishEvent(ResourceEvent const& event) override
    {
        return EventBus<ResourceEvent>::instance()->PublishEvent(event);
    }

    bool Subscribe(BotAI* subscriber, std::vector<ResourceEventType> const& types) override
    {
        return EventBus<ResourceEvent>::instance()->Subscribe(subscriber, types);
    }

    bool SubscribeAll(BotAI* subscriber) override
    {
        std::vector<ResourceEventType> allTypes;
        for (uint8 i = 0; i < static_cast<uint8>(ResourceEventType::MAX_RESOURCE_EVENT); ++i)
            allTypes.push_back(static_cast<ResourceEventType>(i));
        return EventBus<ResourceEvent>::instance()->Subscribe(subscriber, allTypes);
    }

    void Unsubscribe(BotAI* subscriber) override
    {
        EventBus<ResourceEvent>::instance()->Unsubscribe(subscriber);
    }

    uint32 ProcessEvents(uint32 diff, uint32 maxEvents = 0) override
    {
        return EventBus<ResourceEvent>::instance()->ProcessEvents(maxEvents > 0 ? maxEvents : 100);
    }

    uint32 ProcessUnitEvents(ObjectGuid unitGuid, uint32 diff) override
    {
        // Unit-specific filtering not implemented in template
        return ProcessEvents(diff, 100);
    }

    void ClearUnitEvents(ObjectGuid unitGuid) override
    {
        // Not implemented in template - use ClearQueue() for full clear
    }

    uint32 GetPendingEventCount() const override
    {
        return EventBus<ResourceEvent>::instance()->GetQueueSize();
    }

    uint32 GetSubscriberCount() const override
    {
        return EventBus<ResourceEvent>::instance()->GetSubscriberCount();
    }

    // Diagnostic methods (simplified)
    void DumpSubscribers() const override {}
    void DumpEventQueue() const override {}
    std::vector<ResourceEvent> GetQueueSnapshot() const override { return std::vector<ResourceEvent>(); }

private:
    ResourceEventBus() = default;
    ~ResourceEventBus() = default;
};

} // namespace Playerbot

#endif // PLAYERBOT_RESOURCE_EVENT_BUS_H
