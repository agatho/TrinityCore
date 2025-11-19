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

#include "Define.h"
#include "Threading/LockHierarchy.h"
#include "ObjectGuid.h"
#include "Core/DI/Interfaces/IInstanceEventBus.h"
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>

namespace Playerbot
{

class BotAI;

enum class InstanceEventType : uint8
{
    INSTANCE_RESET = 0,
    INSTANCE_RESET_FAILED,
    ENCOUNTER_FRAME_UPDATE,
    RAID_INFO_RECEIVED,
    RAID_GROUP_ONLY_WARNING,
    INSTANCE_SAVE_CREATED,
    INSTANCE_MESSAGE_RECEIVED,
    MAX_INSTANCE_EVENT
};

struct InstanceEvent
{
    InstanceEventType type;
    ObjectGuid playerGuid;
    uint32 mapId;
    uint32 instanceId;
    uint32 encounterId;
    uint32 encounterFrame;
    uint32 errorCode;
    std::string message;
    std::vector<uint32> bossStates;  // For raid info
    std::chrono::steady_clock::time_point timestamp;

    static InstanceEvent InstanceReset(ObjectGuid playerGuid, uint32 mapId);
    static InstanceEvent InstanceResetFailed(ObjectGuid playerGuid, uint32 mapId, uint32 errorCode);
    static InstanceEvent EncounterFrameUpdate(ObjectGuid playerGuid, uint32 encounterId, uint32 frame);
    static InstanceEvent RaidInfoReceived(ObjectGuid playerGuid, uint32 mapId, uint32 instanceId, std::vector<uint32> bossStates);
    static InstanceEvent RaidGroupOnlyWarning(ObjectGuid playerGuid);
    static InstanceEvent InstanceSaveCreated(ObjectGuid playerGuid, uint32 mapId, uint32 instanceId);
    static InstanceEvent InstanceMessageReceived(ObjectGuid playerGuid, uint32 mapId, std::string message);

    bool IsValid() const;
    std::string ToString() const;
};

class TC_GAME_API InstanceEventBus final : public IInstanceEventBus
{
public:
    static InstanceEventBus* instance();
    bool PublishEvent(InstanceEvent const& event) override;

    using EventHandler = std::function<void(InstanceEvent const&)>;

    void Subscribe(BotAI* subscriber, std::vector<InstanceEventType> const& types) override;
    void SubscribeAll(BotAI* subscriber) override;
    void Unsubscribe(BotAI* subscriber) override;

    uint32 SubscribeCallback(EventHandler handler, std::vector<InstanceEventType> const& types) override;
    void UnsubscribeCallback(uint32 subscriptionId) override;

    uint64 GetTotalEventsPublished() const override { return _totalEventsPublished; }
    uint64 GetEventCount(InstanceEventType type) const override;

private:
    InstanceEventBus() = default;
    void DeliverEvent(InstanceEvent const& event);

    std::unordered_map<InstanceEventType, std::vector<BotAI*>> _subscribers;
    std::vector<BotAI*> _globalSubscribers;

    struct CallbackSubscription
    {
        uint32 id;
        EventHandler handler;
        std::vector<InstanceEventType> types;
    };
    std::vector<CallbackSubscription> _callbackSubscriptions;
    uint32 _nextCallbackId = 1;

    std::unordered_map<InstanceEventType, uint64> _eventCounts;
    uint64 _totalEventsPublished = 0;

    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BEHAVIOR_MANAGER> _subscriberMutex;
};

} // namespace Playerbot

#endif // PLAYERBOT_INSTANCE_EVENT_BUS_H
