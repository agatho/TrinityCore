/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_GROUP_EVENT_BUS_H
#define PLAYERBOT_GROUP_EVENT_BUS_H

#include "Define.h"
#include "Threading/LockHierarchy.h"
#include "ObjectGuid.h"
#include "Core/DI/Interfaces/IGroupEventBus.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <chrono>
#include <functional>
#include <atomic>

// Forward declarations
class Player;
class Group;
class Unit;
struct Position;

namespace Playerbot
{

class BotAI;

/**
 * @enum GroupEventType
 * @brief Categorizes all group-related events that bots must handle
 *
 * Each event type corresponds to a specific group state change or action
 * that requires bot AI to respond appropriately.
 */
enum class GroupEventType : uint8
{
    // Core group lifecycle events
    MEMBER_JOINED = 0,          // New member added to group
    MEMBER_LEFT,                // Member removed from group (left/kicked)
    LEADER_CHANGED,             // Group leadership transferred
    GROUP_DISBANDED,            // Group completely disbanded
    RAID_CONVERTED,             // Party converted to raid (or vice versa)

    // Ready check system
    READY_CHECK_STARTED,        // Ready check initiated by leader
    READY_CHECK_RESPONSE,       // Member responded to ready check
    READY_CHECK_COMPLETED,      // Ready check finished (all responded or timeout)

    // Combat coordination
    TARGET_ICON_CHANGED,        // Raid target icon assigned/cleared
    RAID_MARKER_CHANGED,        // World raid marker placed/removed (legacy name)
    WORLD_MARKER_CHANGED,       // World raid marker placed/removed (new name for clarity)
    ASSIST_TARGET_CHANGED,      // Main assist target changed

    // Loot and distribution
    LOOT_METHOD_CHANGED,        // Group loot method modified
    LOOT_THRESHOLD_CHANGED,     // Item quality threshold changed
    MASTER_LOOTER_CHANGED,      // Master looter assigned

    // Raid organization
    SUBGROUP_CHANGED,           // Member moved to different subgroup
    ASSISTANT_CHANGED,          // Member promoted/demoted as assistant
    MAIN_TANK_CHANGED,          // Main tank assigned/cleared
    MAIN_ASSIST_CHANGED,        // Main assist assigned/cleared

    // Instance and difficulty
    DIFFICULTY_CHANGED,         // Instance difficulty modified
    INSTANCE_LOCK_MESSAGE,      // Instance lock/reset notification

    // Communication
    PING_RECEIVED,              // Ping notification (unit or location)
    COMMAND_RESULT,             // Group command execution result

    // Status updates
    GROUP_LIST_UPDATE,          // Group member list updated
    MEMBER_STATS_CHANGED,       // Member health/mana/stats changed
    INVITE_DECLINED,            // Group invite was declined

    // Internal events
    STATE_UPDATE_REQUIRED,      // Full state synchronization needed
    ERROR_OCCURRED,             // Error in group operation

    MAX_EVENT_TYPE
};

/**
 * @enum EventPriority
 * @brief Defines processing priority for group events
 *
 * Critical events (disbanding, errors) are processed immediately,
 * while informational events can be batched for efficiency.
 */
enum class EventPriority : uint8
{
    CRITICAL = 0,   // Process immediately (disbanding, errors)
    HIGH = 1,       // Process within 100ms (combat events, ready checks)
    MEDIUM = 2,     // Process within 500ms (loot changes, role changes)
    LOW = 3,        // Process within 1000ms (cosmetic updates)
    BATCH = 4       // Batch process with others
};

/**
 * @struct GroupEvent
 * @brief Encapsulates all data for a group-related event
 *
 * This structure is designed to be lightweight and copyable for
 * efficient queue operations while containing all necessary context.
 */
struct GroupEvent
{
    GroupEventType type;
    EventPriority priority;
    ObjectGuid groupGuid;           // Group involved in event
    ObjectGuid sourceGuid;          // Event originator (player/leader)
    ObjectGuid targetGuid;          // Event target (affected player/unit)

    uint32 data1;                   // Event-specific data 1
    uint32 data2;                   // Event-specific data 2
    uint64 data3;                   // Event-specific data 3 (64-bit for positions)

    std::chrono::steady_clock::time_point timestamp;
    std::chrono::steady_clock::time_point expiryTime;

    // Helper constructors for common event types
    static GroupEvent MemberJoined(ObjectGuid groupGuid, ObjectGuid memberGuid);
    static GroupEvent MemberLeft(ObjectGuid groupGuid, ObjectGuid memberGuid, uint32 removeMethod);
    static GroupEvent LeaderChanged(ObjectGuid groupGuid, ObjectGuid newLeaderGuid);
    static GroupEvent GroupDisbanded(ObjectGuid groupGuid);
    static GroupEvent ReadyCheckStarted(ObjectGuid groupGuid, ObjectGuid initiatorGuid, uint32 durationMs);
    static GroupEvent TargetIconChanged(ObjectGuid groupGuid, uint8 icon, ObjectGuid targetGuid);
    static GroupEvent RaidMarkerChanged(ObjectGuid groupGuid, uint32 markerId, Position const& position);
    static GroupEvent LootMethodChanged(ObjectGuid groupGuid, uint8 lootMethod);
    static GroupEvent DifficultyChanged(ObjectGuid groupGuid, uint8 difficulty);

    // Comparison operator for priority queue (higher priority = lower value)
    bool operator<(GroupEvent const& other) const
    {
        if (priority != other.priority)
            return priority > other.priority; // Lower enum value = higher priority

        return timestamp > other.timestamp; // Earlier events first within same priority
    }

    // Validation
    bool IsValid() const;
    bool IsExpired() const;
    std::string ToString() const;
};

/**
 * @class GroupEventBus
 * @brief Central event distribution system for all group-related events
 *
 * This singleton class implements a thread-safe, priority-based event bus
 * that decouples TrinityCore's group system from playerbot AI logic.
 *
 * Architecture:
 * - TrinityCore hooks publish events to the bus
 * - BotAI instances subscribe to specific event types
 * - Events are queued by priority and processed in batches
 * - Lock-free where possible for maximum performance
 *
 * Performance Targets:
 * - Event publishing: <10 microseconds
 * - Event processing: <1ms per event
 * - Batch processing: 50 events in <5ms
 * - Memory overhead: <1KB per active event
 */
class TC_GAME_API GroupEventBus final : public IGroupEventBus
{
public:
    /**
     * Get the singleton instance
     * @return Global GroupEventBus instance
     */
    static GroupEventBus* instance();

    /**
     * Publish an event to all subscribers
     * @param event The event to publish
     * @return true if event was queued successfully
     *
     * Thread-safe: Can be called from any thread (TrinityCore hooks)
     */
    bool PublishEvent(GroupEvent const& event) override;

    /**
     * Subscribe to specific event types
     * @param subscriber The BotAI that wants to receive events
     * @param types Vector of event types to subscribe to
     * @return true if subscription was successful
     *
     * Note: Subscriber must call Unsubscribe before destruction
     */
    bool Subscribe(BotAI* subscriber, std::vector<GroupEventType> const& types) override;

    /**
     * Subscribe to all event types
     * @param subscriber The BotAI that wants to receive all events
     * @return true if subscription was successful
     */
    bool SubscribeAll(BotAI* subscriber) override;

    /**
     * Unsubscribe from all events
     * @param subscriber The BotAI to remove
     *
     * Must be called in BotAI destructor to prevent dangling pointers
     */
    void Unsubscribe(BotAI* subscriber) override;

    /**
     * Process pending events and deliver to subscribers
     * @param diff Time elapsed since last update (milliseconds)
     * @param maxEvents Maximum events to process (0 = process all)
     * @return Number of events processed
     *
     * Should be called from World update loop at regular intervals
     */
    uint32 ProcessEvents(uint32 diff, uint32 maxEvents = 100) override;

    /**
     * Process events for a specific group only
     * @param groupGuid The group to process events for
     * @param diff Time elapsed since last update
     * @return Number of events processed
     */
    uint32 ProcessGroupEvents(ObjectGuid groupGuid, uint32 diff) override;

    /**
     * Clear all events for a specific group (on disbanding)
     * @param groupGuid The group to clear events for
     */
    void ClearGroupEvents(ObjectGuid groupGuid) override;

    /**
     * Get pending event count
     * @return Number of events in queue
     */
    uint32 GetPendingEventCount() const override;

    /**
     * Get subscriber count
     * @return Number of active subscribers
     */
    uint32 GetSubscriberCount() const override;

    // Statistics and monitoring
    struct Statistics
    {
        std::atomic<uint64_t> totalEventsPublished{0};
        std::atomic<uint64_t> totalEventsProcessed{0};
        std::atomic<uint64_t> totalEventsDropped{0};      // Expired or invalid
        std::atomic<uint64_t> totalDeliveries{0};         // Event→Subscriber deliveries
        std::atomic<uint64_t> averageProcessingTimeUs{0}; // Microseconds
        std::atomic<uint32_t> peakQueueSize{0};
        std::chrono::steady_clock::time_point startTime;

        void Reset();
        std::string ToString() const;
    };

    Statistics const& GetStatistics() const { return _stats; }
    void ResetStatistics() { _stats.Reset(); }

    /**
     * Configuration
     */
    void SetMaxQueueSize(uint32 size) override { _maxQueueSize = size; }
    void SetEventTTL(uint32 ttlMs) override { _eventTTLMs = ttlMs; }
    void SetBatchSize(uint32 size) override { _batchSize = size; }

    uint32 GetMaxQueueSize() const override { return _maxQueueSize; }
    uint32 GetEventTTL() const override { return _eventTTLMs; }
    uint32 GetBatchSize() const override { return _batchSize; }

    /**
     * Debugging and diagnostics
     */
    void DumpSubscribers() const override;
    void DumpEventQueue() const override;
    std::vector<GroupEvent> GetQueueSnapshot() const override;

private:
    GroupEventBus();
    ~GroupEventBus();

    // Singleton - prevent copying
    GroupEventBus(GroupEventBus const&) = delete;
    GroupEventBus& operator=(GroupEventBus const&) = delete;

    /**
     * Deliver event to a specific subscriber
     * @param subscriber The BotAI to deliver to
     * @param event The event to deliver
     * @return true if delivery was successful
     */
    bool DeliverEvent(BotAI* subscriber, GroupEvent const& event);

    /**
     * Validate event before processing
     * @param event The event to validate
     * @return true if event is valid and not expired
     */
    bool ValidateEvent(GroupEvent const& event) const;

    /**
     * Clean up expired events from queue
     * @return Number of events removed
     */
    uint32 CleanupExpiredEvents();

    /**
     * Update performance metrics
     * @param processingTime Time taken to process event
     */
    void UpdateMetrics(std::chrono::microseconds processingTime);

    /**
     * Log event for debugging
     * @param event The event to log
     * @param action Action being performed (Published/Processed/Dropped)
     */
    void LogEvent(GroupEvent const& event, std::string const& action) const;

private:
    // Event queue (priority queue for automatic priority sorting)
    std::priority_queue<GroupEvent> _eventQueue;
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::GROUP_MANAGER> _queueMutex;

    // Subscriber management
    // Map: EventType → Vector of subscribers for that type
    std::unordered_map<GroupEventType, std::vector<BotAI*>> _subscribers;
    std::vector<BotAI*> _globalSubscribers; // Subscribed to all events
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::GROUP_MANAGER> _subscriberMutex;

    // Configuration
    std::atomic<uint32_t> _maxQueueSize{10000};     // Maximum events in queue
    std::atomic<uint32_t> _eventTTLMs{30000};       // Event time-to-live (30 seconds)
    std::atomic<uint32_t> _batchSize{50};           // Events per batch

    // Statistics
    Statistics _stats;

    // Timers
    uint32 _cleanupTimer{0};
    uint32 _metricsUpdateTimer{0};

    static constexpr uint32 CLEANUP_INTERVAL = 5000;        // 5 seconds
    static constexpr uint32 METRICS_UPDATE_INTERVAL = 1000; // 1 second
    static constexpr uint32 MAX_SUBSCRIBERS_PER_EVENT = 100; // Sanity limit
};

} // namespace Playerbot

#endif // PLAYERBOT_GROUP_EVENT_BUS_H
