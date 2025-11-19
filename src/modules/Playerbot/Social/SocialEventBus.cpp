/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "SocialEventBus.h"
#include "BotAI.h"
#include "Log.h"
#include <sstream>
#include <algorithm>

namespace Playerbot
{

// Factory methods
SocialEvent SocialEvent::ChatReceived(ObjectGuid player, ObjectGuid target, std::string msg, ChatMsg type)
{
    SocialEvent event;
    event.type = SocialEventType::MESSAGE_CHAT;
    event.priority = SocialEventPriority::MEDIUM;
    event.playerGuid = player;
    event.targetGuid = target;
    event.message = std::move(msg);
    event.chatType = type;
    event.language = Language::LANG_UNIVERSAL;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::seconds(30);
    return event;
}

SocialEvent SocialEvent::WhisperReceived(ObjectGuid player, ObjectGuid target, std::string msg)
{
    SocialEvent event;
    event.type = SocialEventType::MESSAGE_CHAT;
    event.priority = SocialEventPriority::HIGH;
    event.playerGuid = player;
    event.targetGuid = target;
    event.message = std::move(msg);
    event.chatType = ChatMsg::CHAT_MSG_WHISPER;
    event.language = Language::LANG_UNIVERSAL;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::seconds(30);
    return event;
}

SocialEvent SocialEvent::GroupInvite(ObjectGuid player, ObjectGuid inviter)
{
    SocialEvent event;
    event.type = SocialEventType::GUILD_INVITE_RECEIVED;  // Generic invite event
    event.priority = SocialEventPriority::HIGH;
    event.playerGuid = player;
    event.targetGuid = inviter;
    event.message = "";
    event.chatType = ChatMsg::CHAT_MSG_PARTY;
    event.language = Language::LANG_UNIVERSAL;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::seconds(60);
    return event;
}

SocialEvent SocialEvent::MessageChat(ObjectGuid player, ObjectGuid target, std::string senderName, std::string msg, ChatMsg type, Language lang, std::string channel, uint32 achievementId)
{
    SocialEvent event;
    event.type = SocialEventType::MESSAGE_CHAT;
    event.priority = (type == ChatMsg::CHAT_MSG_WHISPER) ? SocialEventPriority::HIGH : SocialEventPriority::MEDIUM;
    event.playerGuid = player;
    event.targetGuid = target;
    event.senderName = std::move(senderName);
    event.message = std::move(msg);
    event.chatType = type;
    event.language = lang;
    event.channel = std::move(channel);
    event.achievementId = achievementId;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::seconds(30);
    return event;
}

SocialEvent SocialEvent::EmoteReceived(ObjectGuid player, ObjectGuid target, uint32 emoteId)
{
    SocialEvent event;
    event.type = SocialEventType::EMOTE_RECEIVED;
    event.priority = SocialEventPriority::LOW;
    event.playerGuid = player;
    event.targetGuid = target;
    event.emoteId = emoteId;
    event.chatType = ChatMsg::CHAT_MSG_SAY;
    event.language = Language::LANG_UNIVERSAL;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::seconds(10);
    return event;
}

SocialEvent SocialEvent::TextEmoteReceived(ObjectGuid player, ObjectGuid target, uint32 emoteId)
{
    SocialEvent event;
    event.type = SocialEventType::TEXT_EMOTE_RECEIVED;
    event.priority = SocialEventPriority::LOW;
    event.playerGuid = player;
    event.targetGuid = target;
    event.emoteId = emoteId;
    event.chatType = ChatMsg::CHAT_MSG_SAY;
    event.language = Language::LANG_UNIVERSAL;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::seconds(10);
    return event;
}

SocialEvent SocialEvent::GuildInviteReceived(ObjectGuid player, ObjectGuid target, std::string inviterName, uint64 guildId)
{
    SocialEvent event;
    event.type = SocialEventType::GUILD_INVITE_RECEIVED;
    event.priority = SocialEventPriority::HIGH;
    event.playerGuid = player;
    event.targetGuid = target;
    event.senderName = std::move(inviterName);
    event.guildId = guildId;
    event.chatType = ChatMsg::CHAT_MSG_GUILD;
    event.language = Language::LANG_UNIVERSAL;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::seconds(60);
    return event;
}

SocialEvent SocialEvent::GuildEventReceived(ObjectGuid player, uint64 guildId, std::string message)
{
    SocialEvent event;
    event.type = SocialEventType::GUILD_EVENT_RECEIVED;
    event.priority = SocialEventPriority::MEDIUM;
    event.playerGuid = player;
    event.targetGuid = ObjectGuid::Empty;
    event.guildId = guildId;
    event.message = std::move(message);
    event.chatType = ChatMsg::CHAT_MSG_GUILD;
    event.language = Language::LANG_UNIVERSAL;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::seconds(30);
    return event;
}

SocialEvent SocialEvent::TradeStatusChanged(ObjectGuid partner, ObjectGuid player, uint8 status)
{
    SocialEvent event;
    event.type = SocialEventType::TRADE_STATUS_CHANGED;
    event.priority = SocialEventPriority::HIGH;
    event.playerGuid = player;
    event.targetGuid = partner;
    event.tradeStatus = status;
    event.chatType = ChatMsg::CHAT_MSG_SAY;
    event.language = Language::LANG_UNIVERSAL;
    event.timestamp = std::chrono::steady_clock::now();
    event.expiryTime = event.timestamp + std::chrono::seconds(60);
    return event;
}

bool SocialEvent::IsValid() const
{
    if (type >= SocialEventType::MAX_SOCIAL_EVENT)
        return false;

    switch (type)
    {
        case SocialEventType::MESSAGE_CHAT:
            return !playerGuid.IsEmpty() && !message.empty();
        case SocialEventType::EMOTE_RECEIVED:
        case SocialEventType::TEXT_EMOTE_RECEIVED:
            return !playerGuid.IsEmpty();
        case SocialEventType::GUILD_INVITE_RECEIVED:
            return !playerGuid.IsEmpty() && !targetGuid.IsEmpty();
        case SocialEventType::GUILD_EVENT_RECEIVED:
            return !playerGuid.IsEmpty();
        case SocialEventType::TRADE_STATUS_CHANGED:
            return !playerGuid.IsEmpty();
        default:
            return false;
    }
}

bool SocialEvent::IsExpired() const
{
    return std::chrono::steady_clock::now() > expiryTime;
}

std::string SocialEvent::ToString() const
{
    std::ostringstream oss;
    oss << "SocialEvent[";

    switch (type)
    {
        case SocialEventType::MESSAGE_CHAT:
            oss << "MESSAGE_CHAT, player=" << playerGuid.ToString()
                << ", target=" << targetGuid.ToString()
                << ", type=" << static_cast<uint32>(chatType)
                << ", msg=" << (message.length() > 50 ? message.substr(0, 50) + "..." : message);
            break;
        case SocialEventType::EMOTE_RECEIVED:
            oss << "EMOTE_RECEIVED, player=" << playerGuid.ToString()
                << ", target=" << targetGuid.ToString();
            break;
        case SocialEventType::TEXT_EMOTE_RECEIVED:
            oss << "TEXT_EMOTE_RECEIVED, player=" << playerGuid.ToString()
                << ", target=" << targetGuid.ToString();
            break;
        case SocialEventType::GUILD_INVITE_RECEIVED:
            oss << "GUILD_INVITE_RECEIVED, player=" << playerGuid.ToString()
                << ", inviter=" << targetGuid.ToString();
            break;
        case SocialEventType::GUILD_EVENT_RECEIVED:
            oss << "GUILD_EVENT_RECEIVED, player=" << playerGuid.ToString()
                << ", msg=" << message;
            break;
        case SocialEventType::TRADE_STATUS_CHANGED:
            oss << "TRADE_STATUS_CHANGED, player=" << playerGuid.ToString()
                << ", trader=" << targetGuid.ToString();
            break;
        default:
            oss << "UNKNOWN";
    }

    oss << "]";
    return oss.str();
}

// SocialEventBus implementation
SocialEventBus* SocialEventBus::instance()
{
    static SocialEventBus instance;
    return &instance;
}

SocialEventBus::SocialEventBus()
{
    TC_LOG_INFO("playerbot", "SocialEventBus: Initialized");
}

SocialEventBus::~SocialEventBus()
{
    TC_LOG_INFO("playerbot", "SocialEventBus: Shutdown");
}

bool SocialEventBus::PublishEvent(SocialEvent const& event)
{
    if (!ValidateEvent(event))
    {
        TC_LOG_ERROR("playerbot.events", "SocialEventBus: Invalid event rejected: {}", event.ToString());
        _stats.totalEventsDropped.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    std::lock_guard lock(_queueMutex);

    if (_eventQueue.size() >= MAX_QUEUE_SIZE)
    {
        TC_LOG_WARN("playerbot.events", "SocialEventBus: Queue full, dropping event: {}", event.ToString());
        _stats.totalEventsDropped.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    _eventQueue.push(event);
    _stats.totalEventsPublished.fetch_add(1, std::memory_order_relaxed);

    uint32 queueSize = static_cast<uint32>(_eventQueue.size());
    uint32 currentPeak = _stats.peakQueueSize.load(std::memory_order_relaxed);
    while (queueSize > currentPeak &&
           !_stats.peakQueueSize.compare_exchange_weak(currentPeak, queueSize, std::memory_order_relaxed));

    LogEvent(event, "Published");
    return true;
}

bool SocialEventBus::Subscribe(BotAI* subscriber, std::vector<SocialEventType> const& types)
{
    if (!subscriber)
        return false;

    std::lock_guard lock(_subscriberMutex);

    for (auto type : types)
    {
        auto& typeSubscribers = _subscribers[type];
        if (std::find(typeSubscribers.begin(), typeSubscribers.end(), subscriber) == typeSubscribers.end())
        {
            typeSubscribers.push_back(subscriber);
            TC_LOG_DEBUG("playerbot.events", "SocialEventBus: Subscriber registered for type {}",
                static_cast<uint32>(type));
        }
    }

    return true;
}

bool SocialEventBus::SubscribeAll(BotAI* subscriber)
{
    if (!subscriber)
        return false;

    std::lock_guard lock(_subscriberMutex);

    if (std::find(_globalSubscribers.begin(), _globalSubscribers.end(), subscriber) == _globalSubscribers.end())
    {
        _globalSubscribers.push_back(subscriber);
        TC_LOG_DEBUG("playerbot.events", "SocialEventBus: Subscriber registered for ALL events");
    }

    return true;
}

void SocialEventBus::Unsubscribe(BotAI* subscriber)
{
    if (!subscriber)
        return;

    std::lock_guard lock(_subscriberMutex);

    // Remove from type-specific subscriptions
    for (auto& [type, subscribers] : _subscribers)
    {
        subscribers.erase(std::remove(subscribers.begin(), subscribers.end(), subscriber), subscribers.end());
    }

    // Remove from global subscriptions
    _globalSubscribers.erase(std::remove(_globalSubscribers.begin(), _globalSubscribers.end(), subscriber),
        _globalSubscribers.end());

    TC_LOG_DEBUG("playerbot.events", "SocialEventBus: Subscriber unregistered");
}

uint32 SocialEventBus::ProcessEvents(uint32 diff, uint32 maxEvents)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    uint32 processed = 0;
    std::vector<SocialEvent> eventsToDeliver;

    {
        std::lock_guard lock(_queueMutex);

        while (!_eventQueue.empty() && (maxEvents == 0 || processed < maxEvents))
        {
            SocialEvent event = _eventQueue.top();
            _eventQueue.pop();

            if (event.IsExpired())
            {
                _stats.totalEventsDropped.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            eventsToDeliver.push_back(event);
            processed++;
        }
    }

    // Deliver events outside the lock
    for (auto const& event : eventsToDeliver)
    {
        if (DeliverEvent(nullptr, event))
            _stats.totalEventsProcessed.fetch_add(1, std::memory_order_relaxed);
    }

    // Update cleanup timer
    _cleanupTimer += diff;
    if (_cleanupTimer >= CLEANUP_INTERVAL)
    {
        CleanupExpiredEvents();
        _cleanupTimer = 0;
    }

    // Update metrics
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    UpdateMetrics(duration);

    return processed;
}

uint32 SocialEventBus::ProcessUnitEvents(ObjectGuid unitGuid, uint32 diff)
{
    // Process events for specific unit
    return ProcessEvents(diff, 0);
}

void SocialEventBus::ClearUnitEvents(ObjectGuid unitGuid)
{
    std::lock_guard lock(_queueMutex);

    // Remove all events for this unit
    std::priority_queue<SocialEvent> newQueue;
    while (!_eventQueue.empty())
    {
        SocialEvent event = _eventQueue.top();
        _eventQueue.pop();

        if (event.playerGuid != unitGuid && event.targetGuid != unitGuid)
            newQueue.push(event);
    }

    _eventQueue = std::move(newQueue);
}

uint32 SocialEventBus::GetPendingEventCount() const
{
    std::lock_guard lock(_queueMutex);
    return static_cast<uint32>(_eventQueue.size());
}

uint32 SocialEventBus::GetSubscriberCount() const
{
    std::lock_guard lock(_subscriberMutex);
    uint32 count = static_cast<uint32>(_globalSubscribers.size());
    for (auto const& [type, subscribers] : _subscribers)
        count += static_cast<uint32>(subscribers.size());
    return count;
}

void SocialEventBus::DumpSubscribers() const
{
    std::lock_guard lock(_subscriberMutex);
    TC_LOG_INFO("playerbot.events", "SocialEventBus: {} global subscribers", _globalSubscribers.size());
    for (auto const& [type, subscribers] : _subscribers)
        TC_LOG_INFO("playerbot.events", "  Type {}: {} subscribers", static_cast<uint32>(type), subscribers.size());
}

void SocialEventBus::DumpEventQueue() const
{
    std::lock_guard lock(_queueMutex);
    TC_LOG_INFO("playerbot.events", "SocialEventBus: {} events in queue", _eventQueue.size());
}

std::vector<SocialEvent> SocialEventBus::GetQueueSnapshot() const
{
    std::vector<SocialEvent> snapshot;
    std::lock_guard lock(_queueMutex);

    std::priority_queue<SocialEvent> queueCopy = _eventQueue;
    while (!queueCopy.empty())
    {
        snapshot.push_back(queueCopy.top());
        queueCopy.pop();
    }

    return snapshot;
}

bool SocialEventBus::DeliverEvent(BotAI* subscriber, SocialEvent const& event)
{
    std::lock_guard lock(_subscriberMutex);

    // Deliver to type-specific subscribers
    auto it = _subscribers.find(event.type);
    if (it != _subscribers.end())
    {
        for (auto sub : it->second)
        {
            if (sub)
            {
                sub->OnSocialEvent(event);
                _stats.totalDeliveries.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    // Deliver to global subscribers
    for (auto sub : _globalSubscribers)
    {
        if (sub)
        {
            sub->OnSocialEvent(event);
            _stats.totalDeliveries.fetch_add(1, std::memory_order_relaxed);
        }
    }

    return true;
}

bool SocialEventBus::ValidateEvent(SocialEvent const& event) const
{
    return event.IsValid();
}

uint32 SocialEventBus::CleanupExpiredEvents()
{
    std::lock_guard lock(_queueMutex);

    std::priority_queue<SocialEvent> newQueue;
    uint32 removed = 0;

    while (!_eventQueue.empty())
    {
        SocialEvent event = _eventQueue.top();
        _eventQueue.pop();

        if (!event.IsExpired())
            newQueue.push(event);
        else
            removed++;
    }

    _eventQueue = std::move(newQueue);

    if (removed > 0)
        TC_LOG_DEBUG("playerbot.events", "SocialEventBus: Cleaned up {} expired events", removed);

    return removed;
}

void SocialEventBus::UpdateMetrics(std::chrono::microseconds processingTime)
{
    uint64_t timeUs = processingTime.count();
    _stats.averageProcessingTimeUs.store(timeUs, std::memory_order_relaxed);
}

void SocialEventBus::LogEvent(SocialEvent const& event, std::string const& action) const
{
    TC_LOG_TRACE("playerbot.events", "SocialEventBus: {} event: {}", action, event.ToString());
}

void SocialEventBus::Statistics::Reset()
{
    totalEventsPublished.store(0);
    totalEventsProcessed.store(0);
    totalEventsDropped.store(0);
    totalDeliveries.store(0);
    averageProcessingTimeUs.store(0);
    peakQueueSize.store(0);
    startTime = std::chrono::steady_clock::now();
}

std::string SocialEventBus::Statistics::ToString() const
{
    std::ostringstream oss;
    oss << "SocialEventBus Statistics:\n";
    oss << "  Published: " << totalEventsPublished.load() << "\n";
    oss << "  Processed: " << totalEventsProcessed.load() << "\n";
    oss << "  Dropped: " << totalEventsDropped.load() << "\n";
    oss << "  Deliveries: " << totalDeliveries.load() << "\n";
    oss << "  Avg Process Time: " << averageProcessingTimeUs.load() << " μs\n";
    oss << "  Peak Queue Size: " << peakQueueSize.load();
    return oss.str();
}

} // namespace Playerbot
