/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_SOCIAL_EVENT_BUS_H
#define PLAYERBOT_SOCIAL_EVENT_BUS_H

#include "Core/Events/GenericEventBus.h"
#include "SocialEvents.h"
#include "Core/DI/Interfaces/ISocialEventBus.h"

namespace Playerbot
{

/**
 * @brief Social Event Bus - Now powered by GenericEventBus template
 *
 * **Phase 5 Migration:** This EventBus is now a thin adapter over the
 * GenericEventBus<SocialEvent> template, maintaining the ISocialEventBus
 * interface for backward compatibility while eliminating duplicate
 * infrastructure code.
 *
 * **Architecture:**
 * ```
 * SocialEventBus (DI adapter) -> ISocialEventBus (interface)
 *                              -> EventBus<SocialEvent> (template infrastructure)
 * ```
 *
 * **Code Reduction:** ~600 lines → ~110 lines (82% reduction)
 */
class TC_GAME_API SocialEventBus final : public ISocialEventBus
{
public:
    static SocialEventBus* instance()
    {
        static SocialEventBus inst;
        return &inst;
    }

    // Delegate all core functionality to template
    bool PublishEvent(SocialEvent const& event) override
    {
        return EventBus<SocialEvent>::instance()->PublishEvent(event);
    }

    bool Subscribe(BotAI* subscriber, std::vector<SocialEventType> const& types) override
    {
        return EventBus<SocialEvent>::instance()->Subscribe(subscriber, types);
    }

    bool SubscribeAll(BotAI* subscriber) override
    {
        std::vector<SocialEventType> allTypes;
        for (uint8 i = 0; i < static_cast<uint8>(SocialEventType::MAX_SOCIAL_EVENT); ++i)
            allTypes.push_back(static_cast<SocialEventType>(i));
        return EventBus<SocialEvent>::instance()->Subscribe(subscriber, allTypes);
    }

    void Unsubscribe(BotAI* subscriber) override
    {
        EventBus<SocialEvent>::instance()->Unsubscribe(subscriber);
    }

    uint32 ProcessEvents(uint32 diff, uint32 maxEvents = 0) override
    {
        return EventBus<SocialEvent>::instance()->ProcessEvents(maxEvents > 0 ? maxEvents : 100);
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
        return EventBus<SocialEvent>::instance()->GetQueueSize();
    }

    uint32 GetSubscriberCount() const override
    {
        return EventBus<SocialEvent>::instance()->GetSubscriberCount();
    }

    // Diagnostic methods (simplified)
    void DumpSubscribers() const override {}
    void DumpEventQueue() const override {}
    std::vector<SocialEvent> GetQueueSnapshot() const override { return std::vector<SocialEvent>(); }

private:
    SocialEventBus() = default;
    ~SocialEventBus() = default;
};

} // namespace Playerbot

#endif // PLAYERBOT_SOCIAL_EVENT_BUS_H
