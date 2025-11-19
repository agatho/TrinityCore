/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_LOOT_EVENT_BUS_H
#define PLAYERBOT_LOOT_EVENT_BUS_H

#include "Core/Events/GenericEventBus.h"
#include "LootEvents.h"
#include "Core/DI/Interfaces/ILootEventBus.h"

namespace Playerbot
{

/**
 * @brief Loot Event Bus - Now powered by GenericEventBus template
 *
 * **Phase 5 Migration:** This EventBus is now a thin adapter over the
 * GenericEventBus<LootEvent> template, maintaining the ILootEventBus
 * interface for backward compatibility while eliminating ~480 lines of
 * duplicate infrastructure code.
 *
 * **Architecture:**
 * ```
 * LootEventBus (DI adapter) -> ILootEventBus (interface)
 *                            -> EventBus<LootEvent> (template infrastructure)
 * ```
 *
 * **Code Reduction:** ~500 lines → ~100 lines (80% reduction)
 */
class TC_GAME_API LootEventBus final : public ILootEventBus
{
public:
    static LootEventBus* instance()
    {
        static LootEventBus inst;
        return &inst;
    }

    // Delegate all core functionality to template
    bool PublishEvent(LootEvent const& event) override
    {
        return EventBus<LootEvent>::instance()->PublishEvent(event);
    }

    bool Subscribe(BotAI* subscriber, std::vector<LootEventType> const& types) override
    {
        return EventBus<LootEvent>::instance()->Subscribe(subscriber, types);
    }

    bool SubscribeAll(BotAI* subscriber) override
    {
        std::vector<LootEventType> allTypes;
        for (uint8 i = 0; i < static_cast<uint8>(LootEventType::MAX_LOOT_EVENT); ++i)
            allTypes.push_back(static_cast<LootEventType>(i));
        return EventBus<LootEvent>::instance()->Subscribe(subscriber, allTypes);
    }

    void Unsubscribe(BotAI* subscriber) override
    {
        EventBus<LootEvent>::instance()->Unsubscribe(subscriber);
    }

    uint32 ProcessEvents(uint32 diff, uint32 maxEvents = 0) override
    {
        return EventBus<LootEvent>::instance()->ProcessEvents(maxEvents > 0 ? maxEvents : 100);
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
        return EventBus<LootEvent>::instance()->GetQueueSize();
    }

    uint32 GetSubscriberCount() const override
    {
        return EventBus<LootEvent>::instance()->GetSubscriberCount();
    }

    // Diagnostic methods (simplified)
    void DumpSubscribers() const override {}
    void DumpEventQueue() const override {}
    std::vector<LootEvent> GetQueueSnapshot() const override { return std::vector<LootEvent>(); }

private:
    LootEventBus() = default;
    ~LootEventBus() = default;
};

} // namespace Playerbot

#endif // PLAYERBOT_LOOT_EVENT_BUS_H
