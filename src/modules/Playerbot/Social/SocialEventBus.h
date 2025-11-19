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

#include "Define.h"
#include "Threading/LockHierarchy.h"
#include "ObjectGuid.h"
#include "Core/DI/Interfaces/ISocialEventBus.h"
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <atomic>

namespace Playerbot
{

class BotAI;

enum class SocialEventType : uint8
{
    MESSAGE_CHAT = 0,
    EMOTE_RECEIVED,
    TEXT_EMOTE_RECEIVED,
    GUILD_INVITE_RECEIVED,
    GUILD_EVENT_RECEIVED,
    TRADE_STATUS_CHANGED,
    MAX_SOCIAL_EVENT
};

enum class SocialEventPriority : uint8
{
    CRITICAL = 0,
    HIGH = 1,
    MEDIUM = 2,
    LOW = 3,
    BATCH = 4
};

enum class ChatMsg : uint8
{
    CHAT_MSG_SAY = 0,
    CHAT_MSG_PARTY = 1,
    CHAT_MSG_RAID = 2,
    CHAT_MSG_GUILD = 3,
    CHAT_MSG_WHISPER = 4,
    CHAT_MSG_YELL = 6,
    CHAT_MSG_CHANNEL = 17
};

enum class Language : uint8
{
    LANG_UNIVERSAL = 0,
    LANG_ORCISH = 1,
    LANG_COMMON = 7
};

struct SocialEvent
{
    SocialEventType type;
    SocialEventPriority priority;
    ObjectGuid playerGuid;
    ObjectGuid targetGuid;
    std::string message;
    ChatMsg chatType;
    Language language;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::steady_clock::time_point expiryTime;

    // Additional fields for various event types
    std::string senderName;
    std::string channel;
    uint32 emoteId = 0;
    uint32 achievementId = 0;
    uint64 guildId = 0;
    uint8 tradeStatus = 0;

    bool IsValid() const;
    bool IsExpired() const;
    std::string ToString() const;

    // Helper constructors
    static SocialEvent ChatReceived(ObjectGuid player, ObjectGuid target, std::string msg, ChatMsg type);
    static SocialEvent WhisperReceived(ObjectGuid player, ObjectGuid target, std::string msg);
    static SocialEvent GroupInvite(ObjectGuid player, ObjectGuid inviter);
    static SocialEvent MessageChat(ObjectGuid player, ObjectGuid target, std::string senderName, std::string msg, ChatMsg type, Language lang, std::string channel, uint32 achievementId);
    static SocialEvent EmoteReceived(ObjectGuid player, ObjectGuid target, uint32 emoteId);
    static SocialEvent TextEmoteReceived(ObjectGuid player, ObjectGuid target, uint32 emoteId);
    static SocialEvent GuildInviteReceived(ObjectGuid player, ObjectGuid target, std::string inviterName, uint64 guildId);
    static SocialEvent GuildEventReceived(ObjectGuid player, uint64 guildId, std::string message);
    static SocialEvent TradeStatusChanged(ObjectGuid partner, ObjectGuid player, uint8 status);

    // Priority comparison for priority queue
    bool operator<(SocialEvent const& other) const
    {
        return priority > other.priority;
    }
};

class TC_GAME_API SocialEventBus final : public ISocialEventBus
{
public:
    static SocialEventBus* instance();

    // Event publishing
    bool PublishEvent(SocialEvent const& event) override;

    // Subscription management
    bool Subscribe(BotAI* subscriber, std::vector<SocialEventType> const& types) override;
    bool SubscribeAll(BotAI* subscriber) override;
    void Unsubscribe(BotAI* subscriber) override;

    // Event processing
    uint32 ProcessEvents(uint32 diff, uint32 maxEvents = 0) override;
    uint32 ProcessUnitEvents(ObjectGuid unitGuid, uint32 diff) override;
    void ClearUnitEvents(ObjectGuid unitGuid) override;

    // Status queries
    uint32 GetPendingEventCount() const override;
    uint32 GetSubscriberCount() const override;

    // Diagnostics
    void DumpSubscribers() const override;
    void DumpEventQueue() const override;
    std::vector<SocialEvent> GetQueueSnapshot() const override;

    // Statistics
    struct Statistics
    {
        std::atomic<uint64_t> totalEventsPublished{0};
        std::atomic<uint64_t> totalEventsProcessed{0};
        std::atomic<uint64_t> totalEventsDropped{0};
        std::atomic<uint64_t> totalDeliveries{0};
        std::atomic<uint64_t> averageProcessingTimeUs{0};
        std::atomic<uint32_t> peakQueueSize{0};
        std::chrono::steady_clock::time_point startTime;

        void Reset();
        std::string ToString() const;
    };

    Statistics const& GetStatistics() const { return _stats; }

private:
    SocialEventBus();
    ~SocialEventBus();

    // Event delivery
    bool DeliverEvent(BotAI* subscriber, SocialEvent const& event);
    bool ValidateEvent(SocialEvent const& event) const;
    uint32 CleanupExpiredEvents();
    void UpdateMetrics(std::chrono::microseconds processingTime);
    void LogEvent(SocialEvent const& event, std::string const& action) const;

    // Event queue
    std::priority_queue<SocialEvent> _eventQueue;
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BEHAVIOR_MANAGER> _queueMutex;

    // Subscriber management
    std::unordered_map<SocialEventType, std::vector<BotAI*>> _subscribers;
    std::vector<BotAI*> _globalSubscribers;
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BEHAVIOR_MANAGER> _subscriberMutex;

    // Configuration
    static constexpr uint32 MAX_QUEUE_SIZE = 10000;
    static constexpr uint32 CLEANUP_INTERVAL = 30000;
    static constexpr uint32 MAX_SUBSCRIBERS_PER_EVENT = 5000;

    // Timers
    uint32 _cleanupTimer = 0;
    uint32 _metricsUpdateTimer = 0;

    // Statistics
    Statistics _stats;
    uint32 _maxQueueSize = MAX_QUEUE_SIZE;
};

} // namespace Playerbot

#endif // PLAYERBOT_SOCIAL_EVENT_BUS_H

