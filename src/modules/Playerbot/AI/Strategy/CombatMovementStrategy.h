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

#ifndef PLAYERBOT_COMBATMOVEMENTSTRATEGY_H
#define PLAYERBOT_COMBATMOVEMENTSTRATEGY_H

#include "Strategy.h"
#include "Position.h"
#include <chrono>

class Player;
class Unit;
class WorldObject;
class AreaTrigger;
class DynamicObject;

namespace Playerbot
{
    /**
     * @class CombatMovementStrategy
     * @brief Manages role-based positioning and movement during combat for PlayerBots
     *
     * This strategy handles intelligent combat positioning based on the bot's role (Tank, Healer, DPS),
     * including optimal position calculations, movement execution, and basic mechanic avoidance.
     * It ensures bots maintain proper positioning relative to their target and group members.
     */
    class CombatMovementStrategy : public Strategy
    {
    public:
        /**
         * @enum FormationRole
         * @brief Defines the combat role of a bot for positioning purposes
         */
        enum FormationRole
        {
            ROLE_NONE        = 0,
            ROLE_TANK        = 1,
            ROLE_MELEE_DPS   = 2,
            ROLE_RANGED_DPS  = 3,
            ROLE_HEALER      = 4
        };

        /**
         * @brief Constructor initializes the combat movement strategy
         */
        explicit CombatMovementStrategy();

        /**
         * @brief Destructor
         */
        virtual ~CombatMovementStrategy() = default;

        /**
         * @brief Initialize available actions for combat movement
         */
        void InitializeActions() override;

        /**
         * @brief Initialize triggers that activate combat movement behaviors
         */
        void InitializeTriggers() override;

        /**
         * @brief Initialize values used by the strategy
         */
        void InitializeValues() override;

        /**
         * @brief Called when the strategy becomes active
         * @param ai The bot AI instance
         */
        void OnActivate(BotAI* ai) override;

        /**
         * @brief Called when the strategy becomes inactive
         * @param ai The bot AI instance
         */
        void OnDeactivate(BotAI* ai) override;

        /**
         * @brief Check if combat movement should be active
         * @param ai The bot AI instance
         * @return true if bot is in combat and has a valid target
         */
        bool IsActive(BotAI* ai) const override;

        /**
         * @brief Update combat positioning and movement
         * @param ai The bot AI instance
         * @param diff Time since last update in milliseconds
         */
        void UpdateBehavior(BotAI* ai, uint32 diff) override;

        /**
         * @brief Get the current formation role
         * @return The bot's current combat role
         */
        FormationRole GetCurrentRole() const { return _currentRole; }

    private:
        // Role determination
        /**
         * @brief Determine the bot's role based on class and specialization
         * @param player The bot player
         * @return The determined formation role
         */
        FormationRole DetermineRole(Player* player) const;

        /**
         * @brief Check if the given talent tree indicates a tank spec
         * @param talentTree The primary talent tree ID
         * @return true if this is a tank specialization
         */
        bool IsTankSpec(uint32 talentTree) const;

        /**
         * @brief Check if the given talent tree indicates a healer spec
         * @param talentTree The primary talent tree ID
         * @return true if this is a healer specialization
         */
        bool IsHealerSpec(uint32 talentTree) const;

        /**
         * @brief Check if the given class is melee-based
         * @param classId The class ID
         * @return true if this class primarily fights in melee range
         */
        bool IsMeleeClass(uint8 classId) const;

        // Position calculations
        /**
         * @brief Calculate optimal tank position relative to target
         * @param player The bot player
         * @param target The combat target
         * @return The calculated position
         */
        Position CalculateTankPosition(Player* player, Unit* target) const;

        /**
         * @brief Calculate optimal melee DPS position relative to target
         * @param player The bot player
         * @param target The combat target
         * @return The calculated position
         */
        Position CalculateMeleePosition(Player* player, Unit* target) const;

        /**
         * @brief Calculate optimal ranged DPS position relative to target
         * @param player The bot player
         * @param target The combat target
         * @return The calculated position
         */
        Position CalculateRangedPosition(Player* player, Unit* target) const;

        /**
         * @brief Calculate optimal healer position relative to target and group
         * @param player The bot player
         * @param target The combat target
         * @return The calculated position
         */
        Position CalculateHealerPosition(Player* player, Unit* target) const;

        // Movement execution
        /**
         * @brief Move the bot to the specified position
         * @param player The bot player
         * @param position The target position
         * @return true if movement was initiated successfully
         */
        bool MoveToPosition(Player* player, Position const& position);

        /**
         * @brief Check if the bot is already in the correct position
         * @param player The bot player
         * @param targetPosition The desired position
         * @param tolerance Distance tolerance in yards
         * @return true if bot is within tolerance of target position
         */
        bool IsInCorrectPosition(Player* player, Position const& targetPosition, float tolerance = 2.0f) const;

        /**
         * @brief Check if position is reachable via pathfinding
         * @param player The bot player
         * @param position The position to check
         * @return true if a valid path exists to the position
         */
        bool IsPositionReachable(Player* player, Position const& position) const;

        // Mechanic avoidance
        /**
         * @brief Check if the bot is standing in a dangerous area
         * @param player The bot player
         * @return true if bot is in danger from area effects
         */
        bool IsStandingInDanger(Player* player) const;

        /**
         * @brief Find a safe position away from danger zones
         * @param player The bot player
         * @param preferredPosition The preferred position if safe
         * @param searchRadius Maximum distance to search
         * @return A safe position, or current position if none found
         */
        Position FindSafePosition(Player* player, Position const& preferredPosition, float searchRadius = 10.0f) const;

        /**
         * @brief Check if a position is safe from area effects
         * @param position The position to check
         * @param player The bot player for context
         * @return true if the position is safe
         */
        bool IsPositionSafe(Position const& position, Player* player) const;

        // Utility
        /**
         * @brief Get the optimal distance for the current role
         * @param role The formation role
         * @return The optimal combat distance in yards
         */
        float GetOptimalDistance(FormationRole role) const;

        /**
         * @brief Find the best angle for positioning relative to target
         * @param player The bot player
         * @param target The combat target
         * @param role The formation role
         * @return The optimal angle in radians
         */
        float GetOptimalAngle(Player* player, Unit* target, FormationRole role) const;

        /**
         * @brief Calculate a position at given distance and angle from target
         * @param target The reference unit
         * @param distance Distance from target
         * @param angle Angle in radians
         * @return The calculated position
         */
        Position GetPositionAtDistanceAngle(Unit* target, float distance, float angle) const;

        /**
         * @brief Check if the bot should update its position
         * @param diff Time since last update
         * @return true if position should be recalculated
         */
        bool ShouldUpdatePosition(uint32 diff);

        /**
         * @brief Log position update for debugging
         * @param player The bot player
         * @param targetPos The target position
         * @param reason The reason for movement
         */
        void LogPositionUpdate(Player* player, Position const& targetPos, std::string const& reason) const;

    private:
        // State tracking
        FormationRole _currentRole;                    ///< Current combat role
        Position _lastTargetPosition;                  ///< Last calculated target position
        uint32 _lastPositionUpdate;                    ///< Time since last position update
        uint32 _positionUpdateInterval;                ///< Minimum interval between position updates (ms)
        uint32 _movementTimer;                         ///< Timer for movement timeout
        bool _isMoving;                               ///< Flag indicating if movement is in progress
        ObjectGuid _lastTargetGuid;                    ///< GUID of last combat target

        // Performance optimization
        mutable std::chrono::steady_clock::time_point _lastDangerCheck;  ///< Last danger zone check timestamp
        mutable bool _lastDangerResult;                                  ///< Cached danger check result

        // Configuration
        static constexpr float TANK_DISTANCE = 5.0f;           ///< Tank positioning distance
        static constexpr float MELEE_DISTANCE = 5.0f;          ///< Melee DPS positioning distance
        static constexpr float RANGED_DISTANCE = 25.0f;        ///< Ranged DPS positioning distance
        static constexpr float HEALER_DISTANCE = 18.0f;        ///< Healer positioning distance
        static constexpr float POSITION_TOLERANCE = 2.0f;      ///< Position accuracy tolerance
        static constexpr float DANGER_CHECK_RADIUS = 8.0f;     ///< Radius to check for dangers
        static constexpr uint32 MIN_UPDATE_INTERVAL = 500;     ///< Minimum time between position updates (ms)
        static constexpr uint32 DANGER_CACHE_TIME = 200;       ///< How long to cache danger checks (ms)
        static constexpr uint32 MOVEMENT_TIMEOUT = 5000;       ///< Maximum time to attempt movement (ms)
    };
}

#endif // PLAYERBOT_COMBATMOVEMENTSTRATEGY_H