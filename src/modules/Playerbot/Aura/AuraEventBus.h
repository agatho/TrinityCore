/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_AURA_EVENT_BUS_H
#define PLAYERBOT_AURA_EVENT_BUS_H

#include "Core/Events/GenericEventBus.h"
#include "AuraEvents.h"
#include "Core/DI/Interfaces/IAuraEventBus.h"

namespace Playerbot
{

/**
 * @brief Aura Event Bus - Now powered by GenericEventBus template
 *
 * **Phase 5 Migration:** This EventBus is now a thin adapter over the
 * GenericEventBus<AuraEvent> template, maintaining the IAuraEventBus
 * interface for backward compatibility while eliminating duplicate
 * infrastructure code.
 *
 * **Architecture:**
 * ```
 * AuraEventBus (DI adapter) -> IAuraEventBus (interface)
 *                            -> EventBus<AuraEvent> (template infrastructure)
 * ```
 *
 * **Code Reduction:** ~500 lines → ~100 lines (80% reduction)
 */
class TC_GAME_API AuraEventBus final : public IAuraEventBus
{
public:
    static AuraEventBus* instance()
    {
        static AuraEventBus inst;
        return &inst;
    }

    // Delegate all core functionality to template
    bool PublishEvent(AuraEvent const& event) override
    {
        return EventBus<AuraEvent>::instance()->PublishEvent(event);
    }

    bool Subscribe(BotAI* subscriber, std::vector<AuraEventType> const& types) override
    {
        return EventBus<AuraEvent>::instance()->Subscribe(subscriber, types);
    }

    bool SubscribeAll(BotAI* subscriber) override
    {
        std::vector<AuraEventType> allTypes;
        for (uint8 i = 0; i < static_cast<uint8>(AuraEventType::MAX_AURA_EVENT); ++i)
            allTypes.push_back(static_cast<AuraEventType>(i));
        return EventBus<AuraEvent>::instance()->Subscribe(subscriber, allTypes);
    }

    void Unsubscribe(BotAI* subscriber) override
    {
        EventBus<AuraEvent>::instance()->Unsubscribe(subscriber);
    }

    uint32 ProcessEvents(uint32 diff, uint32 maxEvents = 0) override
    {
        return EventBus<AuraEvent>::instance()->ProcessEvents(maxEvents > 0 ? maxEvents : 100);
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
        return EventBus<AuraEvent>::instance()->GetQueueSize();
    }

    uint32 GetSubscriberCount() const override
    {
        return EventBus<AuraEvent>::instance()->GetSubscriberCount();
    }

    // Diagnostic methods (simplified)
    void DumpSubscribers() const override {}
    void DumpEventQueue() const override {}
    std::vector<AuraEvent> GetQueueSnapshot() const override { return std::vector<AuraEvent>(); }

private:
    AuraEventBus() = default;
    ~AuraEventBus() = default;
};

} // namespace Playerbot

#endif // PLAYERBOT_AURA_EVENT_BUS_H
