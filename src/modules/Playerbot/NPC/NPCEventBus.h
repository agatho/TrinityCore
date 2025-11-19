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

#include "Define.h"
#include "Threading/LockHierarchy.h"
#include "ObjectGuid.h"
#include "Core/DI/Interfaces/INPCEventBus.h"
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>

namespace Playerbot
{

class BotAI;

enum class NPCEventType : uint8
{
    GOSSIP_MENU_RECEIVED = 0,
    GOSSIP_COMPLETE,
    VENDOR_LIST_RECEIVED,
    TRAINER_LIST_RECEIVED,
    TRAINER_SERVICE_RESULT,
    BANK_OPENED,
    SPIRIT_HEALER_CONFIRM,
    PETITION_LIST_RECEIVED,
    MAX_NPC_EVENT
};

struct NPCEvent
{
    NPCEventType type;
    ObjectGuid playerGuid;
    ObjectGuid npcGuid;
    uint32 menuId;
    uint32 textId;
    uint32 vendorEntry;
    uint32 trainerEntry;
    uint32 trainerService;
    uint32 petitionEntry;
    std::vector<uint32> gossipOptions;
    std::vector<uint32> availableItems;  // For vendor/trainer
    std::chrono::steady_clock::time_point timestamp;

    static NPCEvent GossipMenuReceived(ObjectGuid playerGuid, ObjectGuid npcGuid, uint32 menuId, uint32 textId, std::vector<uint32> options);
    static NPCEvent GossipComplete(ObjectGuid playerGuid, ObjectGuid npcGuid);
    static NPCEvent VendorListReceived(ObjectGuid playerGuid, ObjectGuid npcGuid, uint32 vendorEntry, std::vector<uint32> items);
    static NPCEvent TrainerListReceived(ObjectGuid playerGuid, ObjectGuid npcGuid, uint32 trainerEntry, std::vector<uint32> spells);
    static NPCEvent TrainerServiceResult(ObjectGuid playerGuid, uint32 trainerService);
    static NPCEvent BankOpened(ObjectGuid playerGuid, ObjectGuid npcGuid);
    static NPCEvent SpiritHealerConfirm(ObjectGuid playerGuid, ObjectGuid npcGuid);
    static NPCEvent PetitionListReceived(ObjectGuid playerGuid, ObjectGuid npcGuid, uint32 petitionEntry);

    bool IsValid() const;
    std::string ToString() const;
};

class TC_GAME_API NPCEventBus final : public INPCEventBus
{
public:
    static NPCEventBus* instance();
    bool PublishEvent(NPCEvent const& event) override;

    using EventHandler = std::function<void(NPCEvent const&)>;

    void Subscribe(BotAI* subscriber, std::vector<NPCEventType> const& types) override;
    void SubscribeAll(BotAI* subscriber) override;
    void Unsubscribe(BotAI* subscriber) override;

    uint32 SubscribeCallback(EventHandler handler, std::vector<NPCEventType> const& types) override;
    void UnsubscribeCallback(uint32 subscriptionId) override;

    uint64 GetTotalEventsPublished() const override { return _totalEventsPublished; }
    uint64 GetEventCount(NPCEventType type) const override;

private:
    NPCEventBus() = default;
    void DeliverEvent(NPCEvent const& event);

    std::unordered_map<NPCEventType, std::vector<BotAI*>> _subscribers;
    std::vector<BotAI*> _globalSubscribers;

    struct CallbackSubscription
    {
        uint32 id;
        EventHandler handler;
        std::vector<NPCEventType> types;
    };
    std::vector<CallbackSubscription> _callbackSubscriptions;
    uint32 _nextCallbackId = 1;

    std::unordered_map<NPCEventType, uint64> _eventCounts;
    uint64 _totalEventsPublished = 0;

    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BEHAVIOR_MANAGER> _subscriberMutex;
};

} // namespace Playerbot

#endif // PLAYERBOT_NPC_EVENT_BUS_H
