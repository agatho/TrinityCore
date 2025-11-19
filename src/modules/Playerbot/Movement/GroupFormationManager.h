/*
 * Copyright (C) 2024+ TrinityCore <http://www.trinitycore.org/>
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

#ifndef _PLAYERBOT_GROUP_FORMATION_MANAGER_H
#define _PLAYERBOT_GROUP_FORMATION_MANAGER_H

#include "Common.h"
#include "Position.h"
#include <vector>

class Player;
class Unit;

namespace Playerbot
{
    /**
     * @enum FormationType
     * @brief Tactical formation patterns for bot groups
     */
    enum class FormationType
    {
        WEDGE = 0,                      ///< V-shaped penetration formation (tank at point)
        DIAMOND,                        ///< Diamond formation (tank front, healer rear, DPS sides)
        DEFENSIVE_SQUARE,               ///< Square formation (healers center, tanks corners)
        ARROW,                          ///< Arrow formation (concentrated assault)
        LINE,                           ///< Line formation (maximum frontal coverage)
        COLUMN,                         ///< Column formation (single-file march)
        SCATTER,                        ///< Scattered positions (PvP, anti-AoE)
        CIRCLE                          ///< Circle formation (360° coverage)
    };

    /**
     * @enum BotRole
     * @brief Combat role classification for formation positioning
     */
    enum class BotRole
    {
        TANK = 0,                       ///< Main tank (front positions)
        HEALER,                         ///< Healer (protected positions)
        MELEE_DPS,                      ///< Melee damage dealer (flanking positions)
        RANGED_DPS,                     ///< Ranged damage dealer (rear positions)
        UTILITY                         ///< Utility/support (flexible positions)
    };

    /**
     * @struct FormationPosition
     * @brief A single position within a formation
     */
    struct FormationPosition
    {
        Position position;              ///< World coordinates
        float offsetX;                  ///< X offset from leader
        float offsetY;                  ///< Y offset from leader
        BotRole preferredRole;          ///< Role best suited for this position
        uint32 priority;                ///< Assignment priority (0 = highest)

        FormationPosition()
            : offsetX(0.0f), offsetY(0.0f),
              preferredRole(BotRole::UTILITY), priority(0) {}
    };

    /**
     * @struct FormationLayout
     * @brief Complete formation layout with all positions
     */
    struct FormationLayout
    {
        FormationType type;                         ///< Formation type
        std::vector<FormationPosition> positions;   ///< All positions in formation
        float spacing;                              ///< Distance between positions (yards)
        float width;                                ///< Formation width (yards)
        float depth;                                ///< Formation depth (yards)
        std::string description;                    ///< Formation description

        FormationLayout()
            : type(FormationType::WEDGE), spacing(3.0f),
              width(0.0f), depth(0.0f) {}
    };

    /**
     * @struct BotFormationAssignment
     * @brief Assigns a bot to a formation position
     */
    struct BotFormationAssignment
    {
        Player* bot;                    ///< Bot player
        FormationPosition position;     ///< Assigned position
        BotRole role;                   ///< Bot's combat role
        float distanceToPosition;       ///< Current distance to assigned position

        BotFormationAssignment()
            : bot(nullptr), role(BotRole::UTILITY), distanceToPosition(0.0f) {}
    };

    /**
     * @class GroupFormationManager
     * @brief Tactical formation system for bot group coordination
     *
     * Purpose:
     * - Organize bot groups into tactical formations
     * - Optimize positioning based on combat roles
     * - Provide strategic advantages (protection, coordination, efficiency)
     * - Support dynamic formation changes during combat
     *
     * Features:
     * - 8 tactical formations (wedge, diamond, square, arrow, line, column, scatter, circle)
     * - Role-based positioning (tanks front, healers protected, DPS optimized)
     * - Scalable formations (5 to 40+ bots)
     * - Dynamic spacing adjustment
     * - Formation rotation around leader
     *
     * Performance Targets:
     * - Formation calculation: < 1ms for 40 bots
     * - Position assignment: < 0.5ms for 40 bots
     * - Memory: < 2KB per formation
     *
     * Quality Standards:
     * - NO shortcuts - Full formation pattern implementation
     * - Complete role-based positioning
     * - Production-ready code (no TODOs)
     *
     * @code
     * // Example usage:
     * GroupFormationManager mgr;
     *
     * // Create wedge formation for 10 bots
     * FormationLayout wedge = mgr.CreateFormation(FormationType::WEDGE, 10);
     *
     * // Assign bots to positions
     * std::vector<Player*> bots = GetGroupBots();
     * auto assignments = mgr.AssignBotsToFormation(leader, bots, wedge);
     *
     * // Move bots to assigned positions
     * for (auto const& assignment : assignments)
     * {
     *     assignment.bot->GetMotionMaster()->MovePoint(0, assignment.position.position);
     * }
     * @endcode
     */
    class GroupFormationManager
    {
    public:
        GroupFormationManager() = default;
        ~GroupFormationManager() = default;

        /// Delete copy/move (stateless utility class)
        GroupFormationManager(GroupFormationManager const&) = delete;
        GroupFormationManager(GroupFormationManager&&) = delete;
        GroupFormationManager& operator=(GroupFormationManager const&) = delete;
        GroupFormationManager& operator=(GroupFormationManager&&) = delete;

        /**
         * @brief Creates formation layout for specified number of bots
         *
         * Generates tactical formation pattern with role-optimized positions.
         *
         * @param type Formation type to create
         * @param botCount Number of bots in formation
         * @param spacing Distance between positions (default: 3 yards)
         * @return Complete formation layout
         *
         * Performance: < 1ms for 40 bots
         * Thread-safety: Thread-safe (pure calculation)
         */
        [[nodiscard]] static FormationLayout CreateFormation(
            FormationType type,
            uint32 botCount,
            float spacing = 3.0f);

        /**
         * @brief Assigns bots to formation positions based on roles
         *
         * Optimally assigns bots to positions matching their combat roles.
         * Tanks assigned to front positions, healers to protected positions, etc.
         *
         * @param leader Group leader (formation anchor point)
         * @param bots Bots to assign
         * @param formation Formation layout
         * @return Vector of bot assignments (bot → position mapping)
         *
         * Performance: < 0.5ms for 40 bots
         * Thread-safety: Thread-safe (read-only bot access)
         */
        [[nodiscard]] static std::vector<BotFormationAssignment> AssignBotsToFormation(
            Player const* leader,
            std::vector<Player*> const& bots,
            FormationLayout const& formation);

        /**
         * @brief Updates formation positions based on leader movement/rotation
         *
         * Recalculates all formation positions when leader moves or turns.
         * Formation rotates with leader's facing direction.
         *
         * @param leader Group leader
         * @param formation Formation layout (updated in-place)
         *
         * Performance: < 0.5ms for 40 bots
         * Thread-safety: Not thread-safe (modifies formation)
         */
        static void UpdateFormationPositions(
            Player const* leader,
            FormationLayout& formation);

        /**
         * @brief Determines bot's combat role from class and spec
         *
         * @param bot Bot to classify
         * @return Combat role (TANK, HEALER, MELEE_DPS, RANGED_DPS, UTILITY)
         *
         * Performance: < 0.01ms
         * Thread-safety: Thread-safe (read-only)
         */
        [[nodiscard]] static BotRole DetermineBotRole(Player const* bot);

        /**
         * @brief Gets human-readable formation type name
         *
         * @param type Formation type
         * @return String name
         */
        [[nodiscard]] static char const* GetFormationName(FormationType type);

        /**
         * @brief Gets recommended formation for situation
         *
         * Suggests optimal formation based on:
         * - Group size
         * - Group composition (tank/healer/DPS ratio)
         * - Combat situation (PvE vs PvP, dungeon vs raid)
         *
         * @param botCount Number of bots
         * @param tankCount Number of tanks
         * @param healerCount Number of healers
         * @param isPvP true if PvP situation
         * @return Recommended formation type
         *
         * Performance: < 0.01ms
         */
        [[nodiscard]] static FormationType RecommendFormation(
            uint32 botCount,
            uint32 tankCount,
            uint32 healerCount,
            bool isPvP = false);

    private:
        /**
         * @brief Creates wedge formation (V-shaped penetration)
         *
         * Tactical Use: Breakthrough, boss encounters, dungeon pulls
         * Structure: Tank at point, melee DPS on flanks, ranged DPS rear, healers protected
         *
         * @param botCount Number of bots
         * @param spacing Distance between positions
         * @return Wedge formation layout
         */
        [[nodiscard]] static FormationLayout CreateWedgeFormation(
            uint32 botCount,
            float spacing);

        /**
         * @brief Creates diamond formation
         *
         * Tactical Use: Balanced offense/defense, general purpose
         * Structure: Tank front, DPS sides, healer rear center
         *
         * @param botCount Number of bots
         * @param spacing Distance between positions
         * @return Diamond formation layout
         */
        [[nodiscard]] static FormationLayout CreateDiamondFormation(
            uint32 botCount,
            float spacing);

        /**
         * @brief Creates defensive square formation
         *
         * Tactical Use: Maximum protection, surrounded situations
         * Structure: Tanks on corners, healers in center, DPS on edges
         *
         * @param botCount Number of bots
         * @param spacing Distance between positions
         * @return Defensive square formation layout
         */
        [[nodiscard]] static FormationLayout CreateDefensiveSquareFormation(
            uint32 botCount,
            float spacing);

        /**
         * @brief Creates arrow formation (concentrated assault)
         *
         * Tactical Use: Focused damage, raid boss positioning
         * Structure: Tight arrowhead, all DPS concentrated on point
         *
         * @param botCount Number of bots
         * @param spacing Distance between positions
         * @return Arrow formation layout
         */
        [[nodiscard]] static FormationLayout CreateArrowFormation(
            uint32 botCount,
            float spacing);

        /**
         * @brief Creates line formation (frontal coverage)
         *
         * Tactical Use: Wide fronts, battleground defense
         * Structure: Single horizontal line, maximum width
         *
         * @param botCount Number of bots
         * @param spacing Distance between positions
         * @return Line formation layout
         */
        [[nodiscard]] static FormationLayout CreateLineFormation(
            uint32 botCount,
            float spacing);

        /**
         * @brief Creates column formation (march/travel)
         *
         * Tactical Use: Narrow passages, travel, stealth
         * Structure: Single-file column
         *
         * @param botCount Number of bots
         * @param spacing Distance between positions
         * @return Column formation layout
         */
        [[nodiscard]] static FormationLayout CreateColumnFormation(
            uint32 botCount,
            float spacing);

        /**
         * @brief Creates scatter formation (anti-AoE)
         *
         * Tactical Use: PvP, AoE-heavy encounters
         * Structure: Random dispersed positions
         *
         * @param botCount Number of bots
         * @param spacing Distance between positions
         * @return Scatter formation layout
         */
        [[nodiscard]] static FormationLayout CreateScatterFormation(
            uint32 botCount,
            float spacing);

        /**
         * @brief Creates circle formation (360° coverage)
         *
         * Tactical Use: Defense, surrounded situations
         * Structure: Circular perimeter
         *
         * @param botCount Number of bots
         * @param spacing Distance between positions (radius)
         * @return Circle formation layout
         */
        [[nodiscard]] static FormationLayout CreateCircleFormation(
            uint32 botCount,
            float spacing);

        /**
         * @brief Rotates formation position around leader by angle
         *
         * @param offsetX X offset from leader
         * @param offsetY Y offset from leader
         * @param angle Rotation angle (radians)
         * @param[out] rotatedX Rotated X offset
         * @param[out] rotatedY Rotated Y offset
         */
        static void RotatePosition(
            float offsetX,
            float offsetY,
            float angle,
            float& rotatedX,
            float& rotatedY);

        /**
         * @brief Calculates formation dimensions
         *
         * @param formation Formation to measure (updated in-place)
         */
        static void CalculateFormationDimensions(FormationLayout& formation);
    };

} // namespace Playerbot

#endif // _PLAYERBOT_GROUP_FORMATION_MANAGER_H
