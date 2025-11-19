/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRINITYCORE_GROUP_COORDINATOR_H
#define TRINITYCORE_GROUP_COORDINATOR_H

#include "Define.h"
#include "ObjectGuid.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>

class Group;
class Unit;
class Player;

namespace Playerbot
{
namespace Coordination
{

class BotAI;

/**
 * @brief Role types for group coordination
 */
enum class GroupRole : uint8
{
    TANK,
    HEALER,
    MELEE_DPS,
    RANGED_DPS,
    SUPPORT,
    UNKNOWN
};

/**
 * @brief Tactical assignment for bot
 */
struct TacticalAssignment
{
    ObjectGuid targetGuid;          // Focus target
    uint32 priority;                // Assignment priority (0-100)
    std::string taskType;           // "interrupt", "dispel", "focus", "cc", etc.
    uint32 timestamp;               // When assigned
    uint32 expirationTime;          // When assignment expires

    bool IsExpired() const;
    bool IsValid() const;
};

/**
 * @brief Group tactical state shared among all members
 */
struct GroupTacticalState
{
    ObjectGuid focusTarget;                 // Current focus target
    std::vector<ObjectGuid> ccTargets;      // Crowd control targets
    std::vector<ObjectGuid> priorityTargets; // Priority kill targets

    // Interrupt rotation
    std::unordered_map<ObjectGuid, uint32> interruptQueue; // Bot GUID → next interrupt time
    uint32 lastInterruptTime = 0;

    // Dispel assignments
    std::unordered_map<ObjectGuid, ObjectGuid> dispelAssignments; // Bot GUID → Target GUID

    // Cooldown coordination
    std::unordered_map<std::string, uint32> groupCooldowns; // Cooldown name → expire time

    bool inCombat = false;
    uint32 combatStartTime = 0;
    uint32 lastUpdateTime = 0;
};

/**
 * @brief Coordinates a single group (5-40 members)
 *
 * Responsibilities:
 * - Role detection and assignment
 * - Focus target coordination
 * - Interrupt rotation management
 * - Dispel assignments
 * - Cooldown coordination
 * - Tactical decision synchronization
 *
 * Performance: <1ms per update for 40 members
 */
class TC_GAME_API GroupCoordinator
{
public:
    GroupCoordinator(Group* group);
    ~GroupCoordinator() = default;

    /**
     * @brief Update group coordination (called every 100-500ms)
     * @param diff Time since last update (ms)
     */
    void Update(uint32 diff);

    /**
     * @brief Get role assignment for bot
     * @param botGuid Bot's GUID
     * @return Assigned role
     */
    GroupRole GetBotRole(ObjectGuid botGuid) const;

    /**
     * @brief Get current focus target
     * @return GUID of focus target or empty if none
     */
    ObjectGuid GetFocusTarget() const { return _tacticalState.focusTarget; }

    /**
     * @brief Set focus target for group
     * @param targetGuid New focus target
     */
    void SetFocusTarget(ObjectGuid targetGuid);

    /**
     * @brief Get tactical assignment for bot
     * @param botGuid Bot's GUID
     * @return Current assignment or nullptr
     */
    TacticalAssignment const* GetAssignment(ObjectGuid botGuid) const;

    /**
     * @brief Assign interrupt to next available bot
     * @param targetGuid Target to interrupt
     * @return GUID of assigned bot or empty if none available
     */
    ObjectGuid AssignInterrupt(ObjectGuid targetGuid);

    /**
     * @brief Assign dispel to appropriate healer
     * @param targetGuid Target needing dispel
     * @return GUID of assigned healer or empty if none available
     */
    ObjectGuid AssignDispel(ObjectGuid targetGuid);

    /**
     * @brief Check if group cooldown is available
     * @param cooldownName Cooldown identifier (e.g., "Bloodlust")
     * @return True if available
     */
    bool IsGroupCooldownAvailable(std::string const& cooldownName) const;

    /**
     * @brief Use group cooldown
     * @param cooldownName Cooldown identifier
     * @param durationMs Cooldown duration in milliseconds
     */
    void UseGroupCooldown(std::string const& cooldownName, uint32 durationMs);

    /**
     * @brief Get group tactical state (read-only)
     */
    GroupTacticalState const& GetTacticalState() const { return _tacticalState; }

    /**
     * @brief Get number of bots by role
     * @param role Role to count
     * @return Number of bots with that role
     */
    uint32 GetRoleCount(GroupRole role) const;

    /**
     * @brief Get all bots with specific role
     * @param role Role to filter
     * @return Vector of bot GUIDs
     */
    std::vector<ObjectGuid> GetBotsByRole(GroupRole role) const;

    /**
     * @brief Check if group is in combat
     */
    bool IsInCombat() const { return _tacticalState.inCombat; }

    /**
     * @brief Get time since combat started (ms)
     */
    uint32 GetCombatDuration() const;

private:
    /**
     * @brief Detect and update bot roles
     */
    void UpdateRoleAssignments();

    /**
     * @brief Update focus target based on threat/priority
     */
    void UpdateFocusTarget();

    /**
     * @brief Update interrupt rotation
     */
    void UpdateInterruptRotation();

    /**
     * @brief Update dispel assignments
     */
    void UpdateDispelAssignments();

    /**
     * @brief Clean up expired assignments and cooldowns
     */
    void CleanupExpiredData();

    /**
     * @brief Detect role from spec and talents
     * @param player Player to analyze
     * @return Detected role
     */
    GroupRole DetectRole(Player* player) const;

    /**
     * @brief Find best target for focus fire
     * @return GUID of best target or empty
     */
    ObjectGuid FindBestFocusTarget() const;

    /**
     * @brief Get next bot for interrupt rotation
     * @return GUID of bot with interrupt ready
     */
    ObjectGuid GetNextInterrupter() const;

private:
    Group* _group;
    GroupTacticalState _tacticalState;

    // Role assignments
    std::unordered_map<ObjectGuid, GroupRole> _roleAssignments;

    // Tactical assignments per bot
    std::unordered_map<ObjectGuid, TacticalAssignment> _assignments;

    // Update timing
    uint32 _lastUpdateTime = 0;
    uint32 _updateInterval = 200; // Update every 200ms

    // Performance tracking
    uint32 _totalUpdates = 0;
    uint64 _totalUpdateTime = 0; // microseconds
};

} // namespace Coordination
} // namespace Playerbot

#endif // TRINITYCORE_GROUP_COORDINATOR_H
