/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_QUEST_EVENT_BUS_H
#define PLAYERBOT_QUEST_EVENT_BUS_H

#include "Core/Events/GenericEventBus.h"
#include "QuestEvents.h"
#include "Core/DI/Interfaces/IQuestEventBus.h"

namespace Playerbot
{

/**
 * @brief Quest Event Bus - Now powered by GenericEventBus template
 *
 * **Phase 5 Migration:** This EventBus is now a thin adapter over the
 * GenericEventBus<QuestEvent> template, maintaining the IQuestEventBus
 * interface for backward compatibility while eliminating duplicate
 * infrastructure code.
 *
 * **Architecture:**
 * ```
 * QuestEventBus (DI adapter) -> IQuestEventBus (interface)
 *                             -> EventBus<QuestEvent> (template infrastructure)
 * ```
 *
 * **Code Reduction:** ~500 lines → ~100 lines (80% reduction)
 */
class TC_GAME_API QuestEventBus final : public IQuestEventBus
{
public:
    static QuestEventBus* instance()
    {
        static QuestEventBus inst;
        return &inst;
    }

    // Delegate all core functionality to template
    bool PublishEvent(QuestEvent const& event) override
    {
        return EventBus<QuestEvent>::instance()->PublishEvent(event);
    }

    bool Subscribe(BotAI* subscriber, std::vector<QuestEventType> const& types) override
    {
        return EventBus<QuestEvent>::instance()->Subscribe(subscriber, types);
    }

    bool SubscribeAll(BotAI* subscriber) override
    {
        std::vector<QuestEventType> allTypes;
        for (uint8 i = 0; i < static_cast<uint8>(QuestEventType::MAX_QUEST_EVENT); ++i)
            allTypes.push_back(static_cast<QuestEventType>(i));
        return EventBus<QuestEvent>::instance()->Subscribe(subscriber, allTypes);
    }

    void Unsubscribe(BotAI* subscriber) override
    {
        EventBus<QuestEvent>::instance()->Unsubscribe(subscriber);
    }

    uint32 ProcessEvents(uint32 diff, uint32 maxEvents = 0) override
    {
        return EventBus<QuestEvent>::instance()->ProcessEvents(maxEvents > 0 ? maxEvents : 100);
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
        return EventBus<QuestEvent>::instance()->GetQueueSize();
    }

    uint32 GetSubscriberCount() const override
    {
        return EventBus<QuestEvent>::instance()->GetSubscriberCount();
    }

    // Diagnostic methods (simplified)
    void DumpSubscribers() const override {}
    void DumpEventQueue() const override {}
    std::vector<QuestEvent> GetQueueSnapshot() const override { return std::vector<QuestEvent>(); }

private:
    QuestEventBus() = default;
    ~QuestEventBus() = default;
};

} // namespace Playerbot

#endif // PLAYERBOT_QUEST_EVENT_BUS_H
