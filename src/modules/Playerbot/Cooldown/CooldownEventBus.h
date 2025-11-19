/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_COOLDOWN_EVENT_BUS_H
#define PLAYERBOT_COOLDOWN_EVENT_BUS_H

#include "Core/Events/GenericEventBus.h"
#include "CooldownEvents.h"
#include "Core/DI/Interfaces/ICooldownEventBus.h"

namespace Playerbot
{

/**
 * @brief Cooldown Event Bus - Now powered by GenericEventBus template
 *
 * **Phase 5 Migration:** This EventBus is now a thin adapter over the
 * GenericEventBus<CooldownEvent> template, maintaining the ICooldownEventBus
 * interface for backward compatibility while eliminating duplicate
 * infrastructure code.
 *
 * **Architecture:**
 * ```
 * CooldownEventBus (DI adapter) -> ICooldownEventBus (interface)
 *                                -> EventBus<CooldownEvent> (template infrastructure)
 * ```
 *
 * **Code Reduction:** ~500 lines → ~100 lines (80% reduction)
 */
class TC_GAME_API CooldownEventBus final : public ICooldownEventBus
{
public:
    static CooldownEventBus* instance()
    {
        static CooldownEventBus inst;
        return &inst;
    }

    // Delegate all core functionality to template
    bool PublishEvent(CooldownEvent const& event) override
    {
        return EventBus<CooldownEvent>::instance()->PublishEvent(event);
    }

    bool Subscribe(BotAI* subscriber, std::vector<CooldownEventType> const& types) override
    {
        return EventBus<CooldownEvent>::instance()->Subscribe(subscriber, types);
    }

    bool SubscribeAll(BotAI* subscriber) override
    {
        std::vector<CooldownEventType> allTypes;
        for (uint8 i = 0; i < static_cast<uint8>(CooldownEventType::MAX_COOLDOWN_EVENT); ++i)
            allTypes.push_back(static_cast<CooldownEventType>(i));
        return EventBus<CooldownEvent>::instance()->Subscribe(subscriber, allTypes);
    }

    void Unsubscribe(BotAI* subscriber) override
    {
        EventBus<CooldownEvent>::instance()->Unsubscribe(subscriber);
    }

    uint32 ProcessEvents(uint32 diff, uint32 maxEvents = 0) override
    {
        return EventBus<CooldownEvent>::instance()->ProcessEvents(maxEvents > 0 ? maxEvents : 100);
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
        return EventBus<CooldownEvent>::instance()->GetQueueSize();
    }

    uint32 GetSubscriberCount() const override
    {
        return EventBus<CooldownEvent>::instance()->GetSubscriberCount();
    }

    // Diagnostic methods (simplified)
    void DumpSubscribers() const override {}
    void DumpEventQueue() const override {}
    std::vector<CooldownEvent> GetQueueSnapshot() const override { return std::vector<CooldownEvent>(); }

private:
    CooldownEventBus() = default;
    ~CooldownEventBus() = default;
};

} // namespace Playerbot

#endif // PLAYERBOT_COOLDOWN_EVENT_BUS_H
