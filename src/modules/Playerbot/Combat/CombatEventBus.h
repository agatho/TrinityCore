/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_COMBAT_EVENT_BUS_H
#define PLAYERBOT_COMBAT_EVENT_BUS_H

#include "Define.h"
#include "Threading/LockHierarchy.h"
#include "ObjectGuid.h"
#include "Core/DI/Interfaces/ICombatEventBus.h"
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
class Unit;
class Spell;

namespace Playerbot
{

class BotAI;

/**
 * @enum CombatEventType
 * @brief Categorizes all combat-related events that bots must handle
 */
enum class CombatEventType : uint8
{
    // Spell casting
    SPELL_CAST_START = 0,       // Spell cast begins (for interrupts)
    SPELL_CAST_GO,              // Spell cast completes (for positioning)
    SPELL_CAST_FAILED,          // Spell cast failed
    SPELL_CAST_DELAYED,         // Spell cast delayed (pushback)

    // Spell damage and healing
    SPELL_DAMAGE_DEALT,         // Spell damage dealt
    SPELL_DAMAGE_TAKEN,         // Spell damage received
    SPELL_HEAL_DEALT,           // Healing done
    SPELL_HEAL_TAKEN,           // Healing received
    SPELL_ENERGIZE,             // Resource gain (mana, rage, etc.)

    // Periodic effects
    PERIODIC_DAMAGE,            // DoT tick
    PERIODIC_HEAL,              // HoT tick
    PERIODIC_ENERGIZE,          // Periodic resource gain

    // Spell interruption
    SPELL_INTERRUPTED,          // Spell was interrupted
    SPELL_INTERRUPT_FAILED,     // Interrupt attempt failed

    // Dispel and cleanse
    SPELL_DISPELLED,            // Aura was dispelled
    SPELL_DISPEL_FAILED,        // Dispel attempt failed
    SPELL_STOLEN,               // Aura was stolen (Spellsteal)

    // Melee combat
    ATTACK_START,               // Melee combat initiated
    ATTACK_STOP,                // Melee combat ended
    ATTACK_SWING,               // Melee swing (damage)

    // Attack errors
    ATTACK_ERROR_NOT_IN_RANGE,  // Target out of range
    ATTACK_ERROR_BAD_FACING,    // Wrong facing
    ATTACK_ERROR_DEAD_TARGET,   // Target is dead
    ATTACK_ERROR_CANT_ATTACK,   // Can't attack target

    // Threat and aggro
    AI_REACTION,                // NPC aggro change
    THREAT_UPDATE,              // Threat value changed
    THREAT_TRANSFER,            // Threat redirected

    // Crowd control
    CC_APPLIED,                 // CC effect applied
    CC_BROKEN,                  // CC effect broken
    CC_IMMUNE,                  // Target immune to CC

    // Absorb and shields
    SPELL_ABSORB,               // Damage absorbed
    SHIELD_BROKEN,              // Shield depleted

    // Combat state
    COMBAT_ENTERED,             // Entered combat
    COMBAT_LEFT,                // Left combat
    EVADE_START,                // NPC evading

    // Spell school lockout
    SPELL_LOCKOUT,              // School locked from interrupts

    MAX_COMBAT_EVENT
};

/**
 * @enum EventPriority
 * @brief Defines processing priority for combat events
 */
enum class CombatEventPriority : uint8
{
    CRITICAL = 0,   // Interrupts, CC breaks - process immediately
    HIGH = 1,       // Damage, healing, threat - process within 50ms
    MEDIUM = 2,     // DoT/HoT ticks - process within 200ms
    LOW = 3,        // Combat state changes - process within 500ms
    BATCH = 4       // Periodic updates - batch process
};

/**
 * @struct CombatEvent
 * @brief Encapsulates all data for a combat-related event
 */
struct CombatEvent
{
    CombatEventType type;
    CombatEventPriority priority;

    ObjectGuid casterGuid;      // Who cast the spell / initiated action
    ObjectGuid targetGuid;      // Who was targeted
    ObjectGuid victimGuid;      // Who was affected (for AoE, may differ from target)

    uint32 spellId;             // Spell ID (0 for melee)
    int32 amount;               // Damage/healing/resource amount
    uint32 schoolMask;          // Spell school mask
    uint32 flags;               // Combat flags (crit, resist, block, etc.)

    std::chrono::steady_clock::time_point timestamp;
    std::chrono::steady_clock::time_point expiryTime;

    // Helper constructors
    static CombatEvent SpellCastStart(ObjectGuid caster, ObjectGuid target, uint32 spellId, uint32 castTime);
    static CombatEvent SpellCastGo(ObjectGuid caster, ObjectGuid target, uint32 spellId);
    static CombatEvent SpellDamage(ObjectGuid caster, ObjectGuid victim, uint32 spellId, int32 damage, uint32 school);
    static CombatEvent SpellHeal(ObjectGuid caster, ObjectGuid target, uint32 spellId, int32 heal);
    static CombatEvent SpellInterrupt(ObjectGuid interrupter, ObjectGuid victim, uint32 interruptedSpell, uint32 interruptSpell);
    static CombatEvent AttackStart(ObjectGuid attacker, ObjectGuid victim);
    static CombatEvent AttackStop(ObjectGuid attacker, ObjectGuid victim, bool nowDead);
    static CombatEvent ThreatUpdate(ObjectGuid unit, ObjectGuid victim, int32 threatChange);

    // Comparison operator for priority queue
    bool operator<(CombatEvent const& other) const
    {
        if (priority != other.priority)
            return priority > other.priority;
        return timestamp > other.timestamp;
    }

    bool IsValid() const;
    bool IsExpired() const;
    std::string ToString() const;
};

/**
 * @class CombatEventBus
 * @brief Central event distribution system for all combat-related events
 *
 * Performance Targets:
 * - Event publishing: <5 microseconds
 * - Event processing: <500 microseconds per event
 * - Batch processing: 100 events in <5ms
 */
class TC_GAME_API CombatEventBus final : public ICombatEventBus
{
public:
    static CombatEventBus* instance();

    // Event publishing
    bool PublishEvent(CombatEvent const& event) override;

    // Subscription
    bool Subscribe(BotAI* subscriber, std::vector<CombatEventType> const& types) override;
    bool SubscribeAll(BotAI* subscriber) override;
    void Unsubscribe(BotAI* subscriber) override;

    // Event processing
    uint32 ProcessEvents(uint32 diff, uint32 maxEvents = 100) override;
    uint32 ProcessUnitEvents(ObjectGuid unitGuid, uint32 diff) override;
    void ClearUnitEvents(ObjectGuid unitGuid) override;

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
    void ResetStatistics() { _stats.Reset(); }

    // Configuration
    void SetMaxQueueSize(uint32 size) override { _maxQueueSize = size; }
    void SetEventTTL(uint32 ttlMs) override { _eventTTLMs = ttlMs; }
    void SetBatchSize(uint32 size) override { _batchSize = size; }

    uint32 GetMaxQueueSize() const override { return _maxQueueSize; }
    uint32 GetEventTTL() const override { return _eventTTLMs; }
    uint32 GetBatchSize() const override { return _batchSize; }

    // Debugging
    void DumpSubscribers() const override;
    void DumpEventQueue() const override;
    std::vector<CombatEvent> GetQueueSnapshot() const override;

private:
    CombatEventBus();
    ~CombatEventBus();

    CombatEventBus(CombatEventBus const&) = delete;
    CombatEventBus& operator=(CombatEventBus const&) = delete;

    bool DeliverEvent(BotAI* subscriber, CombatEvent const& event);
    bool ValidateEvent(CombatEvent const& event) const;
    uint32 CleanupExpiredEvents();
    void UpdateMetrics(std::chrono::microseconds processingTime);
    void LogEvent(CombatEvent const& event, std::string const& action) const;

private:
    std::priority_queue<CombatEvent> _eventQueue;
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::TARGET_SELECTOR> _queueMutex;

    std::unordered_map<CombatEventType, std::vector<BotAI*>> _subscribers;
    std::vector<BotAI*> _globalSubscribers;
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::TARGET_SELECTOR> _subscriberMutex;

    std::atomic<uint32_t> _maxQueueSize{10000};
    std::atomic<uint32_t> _eventTTLMs{5000};  // Combat events expire faster (5s vs 30s)
    std::atomic<uint32_t> _batchSize{100};

    Statistics _stats;

    uint32 _cleanupTimer{0};
    uint32 _metricsUpdateTimer{0};

    static constexpr uint32 CLEANUP_INTERVAL = 2000;        // 2 seconds (faster than group)
    static constexpr uint32 METRICS_UPDATE_INTERVAL = 1000; // 1 second
    static constexpr uint32 MAX_SUBSCRIBERS_PER_EVENT = 100;
};

} // namespace Playerbot

#endif // PLAYERBOT_COMBAT_EVENT_BUS_H
