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

#include "GroupFormationManager.h"
#include "Log.h"
#include "Player.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace Playerbot
{
    // ============================================================================
    // Public API Implementation
    // ============================================================================

    FormationLayout GroupFormationManager::CreateFormation(
        FormationType type,
        uint32 botCount,
        float spacing)
    {
        if (botCount == 0)
        {
            TC_LOG_ERROR("playerbot.formation", "GroupFormationManager: Cannot create formation for 0 bots");
            return FormationLayout();
        }

        if (spacing <= 0.0f)
        {
            TC_LOG_WARN("playerbot.formation", "GroupFormationManager: Invalid spacing {:.2f}, using default 3.0", spacing);
            spacing = 3.0f;
        }

        // Dispatch to specific formation creator
        switch (type)
        {
            case FormationType::WEDGE:
                return CreateWedgeFormation(botCount, spacing);
            case FormationType::DIAMOND:
                return CreateDiamondFormation(botCount, spacing);
            case FormationType::DEFENSIVE_SQUARE:
                return CreateDefensiveSquareFormation(botCount, spacing);
            case FormationType::ARROW:
                return CreateArrowFormation(botCount, spacing);
            case FormationType::LINE:
                return CreateLineFormation(botCount, spacing);
            case FormationType::COLUMN:
                return CreateColumnFormation(botCount, spacing);
            case FormationType::SCATTER:
                return CreateScatterFormation(botCount, spacing);
            case FormationType::CIRCLE:
                return CreateCircleFormation(botCount, spacing);
            default:
                TC_LOG_ERROR("playerbot.formation", "GroupFormationManager: Unknown formation type {}", static_cast<int>(type));
                return CreateWedgeFormation(botCount, spacing); // Fallback to wedge
        }
    }

    std::vector<BotFormationAssignment> GroupFormationManager::AssignBotsToFormation(
        Player const* leader,
        std::vector<Player*> const& bots,
        FormationLayout const& formation)
    {
        std::vector<BotFormationAssignment> assignments;
        assignments.reserve(bots.size());

        if (!leader || bots.empty() || formation.positions.empty())
            return assignments;

        // Step 1: Classify all bots by role
        std::vector<std::pair<Player*, BotRole>> botsWithRoles;
        botsWithRoles.reserve(bots.size());

        for (Player* bot : bots)
        {
            if (!bot)
                continue;

            BotRole role = DetermineBotRole(bot);
            botsWithRoles.emplace_back(bot, role);
        }

        // Step 2: Create position priority lists by role
        std::vector<std::pair<FormationPosition const*, uint32>> tankPositions;
        std::vector<std::pair<FormationPosition const*, uint32>> healerPositions;
        std::vector<std::pair<FormationPosition const*, uint32>> meleeDpsPositions;
        std::vector<std::pair<FormationPosition const*, uint32>> rangedDpsPositions;
        std::vector<std::pair<FormationPosition const*, uint32>> utilityPositions;

        for (size_t i = 0; i < formation.positions.size(); ++i)
        {
            FormationPosition const& pos = formation.positions[i];
            auto posPair = std::make_pair(&pos, static_cast<uint32>(i));

            switch (pos.preferredRole)
            {
                case BotRole::TANK:
                    tankPositions.push_back(posPair);
                    break;
                case BotRole::HEALER:
                    healerPositions.push_back(posPair);
                    break;
                case BotRole::MELEE_DPS:
                    meleeDpsPositions.push_back(posPair);
                    break;
                case BotRole::RANGED_DPS:
                    rangedDpsPositions.push_back(posPair);
                    break;
                case BotRole::UTILITY:
                    utilityPositions.push_back(posPair);
                    break;
            }
        }

        // Sort by priority (lower priority value = higher importance)
        auto sortByPriority = [](auto const& a, auto const& b) {
            return a.first->priority < b.first->priority;
        };

        std::sort(tankPositions.begin(), tankPositions.end(), sortByPriority);
        std::sort(healerPositions.begin(), healerPositions.end(), sortByPriority);
        std::sort(meleeDpsPositions.begin(), meleeDpsPositions.end(), sortByPriority);
        std::sort(rangedDpsPositions.begin(), rangedDpsPositions.end(), sortByPriority);
        std::sort(utilityPositions.begin(), utilityPositions.end(), sortByPriority);

        // Step 3: Assign bots to positions by role matching
        std::vector<bool> positionTaken(formation.positions.size(), false);

        auto assignBotToPosition = [&](Player* bot, BotRole role, std::vector<std::pair<FormationPosition const*, uint32>> const& positions) -> bool
        {
            for (auto const& [pos, index] : positions)
            {
                if (positionTaken[index])
                    continue;

                BotFormationAssignment assignment;
                assignment.bot = bot;
                assignment.position = *pos;
                assignment.role = role;
                assignment.distanceToPosition = 0.0f; // Will be updated when formation moves

                assignments.push_back(assignment);
                positionTaken[index] = true;
                return true;
            }
            return false;
        };

        // Assign tanks first (highest priority)
        for (auto const& [bot, role] : botsWithRoles)
        {
            if (role == BotRole::TANK)
            {
                if (!assignBotToPosition(bot, role, tankPositions))
                {
                    // No tank positions left, try utility positions
                    assignBotToPosition(bot, role, utilityPositions);
                }
            }
        }

        // Assign healers second
        for (auto const& [bot, role] : botsWithRoles)
        {
            if (role == BotRole::HEALER)
            {
                if (!assignBotToPosition(bot, role, healerPositions))
                {
                    assignBotToPosition(bot, role, utilityPositions);
                }
            }
        }

        // Assign melee DPS third
        for (auto const& [bot, role] : botsWithRoles)
        {
            if (role == BotRole::MELEE_DPS)
            {
                if (!assignBotToPosition(bot, role, meleeDpsPositions))
                {
                    assignBotToPosition(bot, role, utilityPositions);
                }
            }
        }

        // Assign ranged DPS fourth
        for (auto const& [bot, role] : botsWithRoles)
        {
            if (role == BotRole::RANGED_DPS)
            {
                if (!assignBotToPosition(bot, role, rangedDpsPositions))
                {
                    assignBotToPosition(bot, role, utilityPositions);
                }
            }
        }

        // Assign utility last
        for (auto const& [bot, role] : botsWithRoles)
        {
            if (role == BotRole::UTILITY)
            {
                assignBotToPosition(bot, role, utilityPositions);
            }
        }

        TC_LOG_DEBUG("playerbot.formation",
            "GroupFormationManager: Assigned {} bots to {} formation positions",
            assignments.size(), formation.positions.size());

        return assignments;
    }

    void GroupFormationManager::UpdateFormationPositions(
        Player const* leader,
        FormationLayout& formation)
    {
        if (!leader || formation.positions.empty())
            return;

        float leaderX = leader->GetPositionX();
        float leaderY = leader->GetPositionY();
        float leaderZ = leader->GetPositionZ();
        float leaderOrientation = leader->GetOrientation();

        // Rotate all formation positions based on leader's facing direction
        for (FormationPosition& pos : formation.positions)
        {
            float rotatedX, rotatedY;
            RotatePosition(pos.offsetX, pos.offsetY, leaderOrientation, rotatedX, rotatedY);

            pos.position.Relocate(
                leaderX + rotatedX,
                leaderY + rotatedY,
                leaderZ // Use leader's Z for now (pathfinding will adjust)
            );
        }
    }

    BotRole GroupFormationManager::DetermineBotRole(Player const* bot)
    {
        if (!bot)
            return BotRole::UTILITY;

        uint8 classId = bot->GetClass();
        uint32 specId = static_cast<uint32>(bot->GetPrimarySpecialization());

        // Determine role based on class and specialization
        switch (classId)
        {
            case CLASS_WARRIOR:
                if (specId == 73) return BotRole::TANK;          // Protection
                return BotRole::MELEE_DPS;                       // Arms, Fury

            case CLASS_PALADIN:
                if (specId == 66) return BotRole::TANK;          // Protection
                if (specId == 65) return BotRole::HEALER;        // Holy
                return BotRole::MELEE_DPS;                       // Retribution

            case CLASS_HUNTER:
                return BotRole::RANGED_DPS;                      // All specs

            case CLASS_ROGUE:
                return BotRole::MELEE_DPS;                       // All specs

            case CLASS_PRIEST:
                if (specId == 256) return BotRole::HEALER;       // Discipline
                if (specId == 257) return BotRole::HEALER;       // Holy
                return BotRole::RANGED_DPS;                      // Shadow

            case CLASS_DEATH_KNIGHT:
                if (specId == 250) return BotRole::TANK;         // Blood
                return BotRole::MELEE_DPS;                       // Frost, Unholy

            case CLASS_SHAMAN:
                if (specId == 264) return BotRole::HEALER;       // Restoration
                if (specId == 262) return BotRole::RANGED_DPS;   // Elemental
                return BotRole::MELEE_DPS;                       // Enhancement

            case CLASS_MAGE:
                return BotRole::RANGED_DPS;                      // All specs

            case CLASS_WARLOCK:
                return BotRole::RANGED_DPS;                      // All specs

            case CLASS_MONK:
                if (specId == 268) return BotRole::TANK;         // Brewmaster
                if (specId == 270) return BotRole::HEALER;       // Mistweaver
                return BotRole::MELEE_DPS;                       // Windwalker

            case CLASS_DRUID:
                if (specId == 104) return BotRole::TANK;         // Guardian
                if (specId == 105) return BotRole::HEALER;       // Restoration
                if (specId == 102) return BotRole::RANGED_DPS;   // Balance
                return BotRole::MELEE_DPS;                       // Feral

            case CLASS_DEMON_HUNTER:
                if (specId == 581) return BotRole::TANK;         // Vengeance
                return BotRole::MELEE_DPS;                       // Havoc

            case CLASS_EVOKER:
                if (specId == 1468) return BotRole::HEALER;      // Preservation
                return BotRole::RANGED_DPS;                      // Devastation, Augmentation

            default:
                return BotRole::UTILITY;
        }
    }

    char const* GroupFormationManager::GetFormationName(FormationType type)
    {
        switch (type)
        {
            case FormationType::WEDGE:
                return "Wedge";
            case FormationType::DIAMOND:
                return "Diamond";
            case FormationType::DEFENSIVE_SQUARE:
                return "Defensive Square";
            case FormationType::ARROW:
                return "Arrow";
            case FormationType::LINE:
                return "Line";
            case FormationType::COLUMN:
                return "Column";
            case FormationType::SCATTER:
                return "Scatter";
            case FormationType::CIRCLE:
                return "Circle";
            default:
                return "Unknown";
        }
    }

    FormationType GroupFormationManager::RecommendFormation(
        uint32 botCount,
        uint32 tankCount,
        uint32 healerCount,
        bool isPvP)
    {
        // PvP formations prioritize scatter and mobility
        if (isPvP)
        {
            if (botCount >= 10)
                return FormationType::SCATTER;  // Anti-AoE for large groups
            else
                return FormationType::DIAMOND;  // Balanced for small groups
        }

        // PvE formations prioritize optimization and protection
        if (botCount <= 5)
        {
            // Small group (dungeon size)
            if (tankCount >= 1 && healerCount >= 1)
                return FormationType::WEDGE;    // Standard dungeon formation
            else
                return FormationType::LINE;     // No dedicated roles
        }
        else if (botCount <= 10)
        {
            // Medium group
            if (tankCount >= 2 && healerCount >= 2)
                return FormationType::DIAMOND;  // Balanced offense/defense
            else
                return FormationType::ARROW;    // Offensive formation
        }
        else if (botCount <= 25)
        {
            // Large group (raid size)
            if (tankCount >= 2 && healerCount >= 5)
                return FormationType::DEFENSIVE_SQUARE; // Maximum healer protection
            else
                return FormationType::WEDGE;    // Penetration formation
        }
        else
        {
            // Very large group (40-man raid)
            return FormationType::CIRCLE;       // 360° coverage
        }
    }

    // ============================================================================
    // Private Formation Creators
    // ============================================================================

    FormationLayout GroupFormationManager::CreateWedgeFormation(
        uint32 botCount,
        float spacing)
    {
        FormationLayout layout;
        layout.type = FormationType::WEDGE;
        layout.spacing = spacing;
        layout.description = "V-shaped penetration formation (tank at point, DPS flanks, healers rear)";

        // Wedge geometry:
        // - Tank at point (0, 0)
        // - Bots arranged in V-shape spreading backwards
        // - Angle: 60 degrees (30 degrees each side)

        constexpr float WEDGE_ANGLE = 30.0f * (M_PI / 180.0f); // 30 degrees in radians

        uint32 priority = 0;

        // Position 0: Tank at point
        FormationPosition tankPos;
        tankPos.offsetX = 0.0f;
        tankPos.offsetY = spacing; // Slightly ahead of leader
        tankPos.preferredRole = BotRole::TANK;
        tankPos.priority = priority++;
        layout.positions.push_back(tankPos);

        // Remaining bots arranged in V-shape
        uint32 remainingBots = botCount - 1;
        uint32 leftSide = remainingBots / 2;
        uint32 rightSide = remainingBots - leftSide;

        // Left flank (melee DPS)
        for (uint32 i = 0; i < leftSide; ++i)
        {
            float distance = spacing * (i + 1);
            FormationPosition pos;
            pos.offsetX = -distance * std::sin(WEDGE_ANGLE);
            pos.offsetY = -distance * std::cos(WEDGE_ANGLE);
            pos.preferredRole = (i < leftSide / 2) ? BotRole::MELEE_DPS : BotRole::RANGED_DPS;
            pos.priority = priority++;
            layout.positions.push_back(pos);
        }

        // Right flank (melee DPS)
        for (uint32 i = 0; i < rightSide; ++i)
        {
            float distance = spacing * (i + 1);
            FormationPosition pos;
            pos.offsetX = distance * std::sin(WEDGE_ANGLE);
            pos.offsetY = -distance * std::cos(WEDGE_ANGLE);
            pos.preferredRole = (i < rightSide / 2) ? BotRole::MELEE_DPS : BotRole::RANGED_DPS;
            pos.priority = priority++;
            layout.positions.push_back(pos);
        }

        // Healers at rear center (protected position)
        uint32 healerCount = std::max(1u, botCount / 5); // ~20% healers
        for (uint32 i = 0; i < healerCount && layout.positions.size() < botCount; ++i)
        {
            FormationPosition pos;
            pos.offsetX = 0.0f;
            pos.offsetY = -(spacing * (leftSide + 1));
            pos.preferredRole = BotRole::HEALER;
            pos.priority = 1; // High priority (protected position)
            layout.positions.push_back(pos);
        }

        CalculateFormationDimensions(layout);
        return layout;
    }

    FormationLayout GroupFormationManager::CreateDiamondFormation(
        uint32 botCount,
        float spacing)
    {
        FormationLayout layout;
        layout.type = FormationType::DIAMOND;
        layout.spacing = spacing;
        layout.description = "Diamond formation (tank front, DPS sides, healer rear center)";

        uint32 priority = 0;

        // Diamond has 4 cardinal points + fill interior
        // Point 0: Tank front (N)
        // Point 1: DPS left (W)
        // Point 2: DPS right (E)
        // Point 3: Healer rear (S)

        // North (tank)
        FormationPosition tankPos;
        tankPos.offsetX = 0.0f;
        tankPos.offsetY = spacing * 2.0f;
        tankPos.preferredRole = BotRole::TANK;
        tankPos.priority = priority++;
        layout.positions.push_back(tankPos);

        // South (healer)
        FormationPosition healerPos;
        healerPos.offsetX = 0.0f;
        healerPos.offsetY = -spacing * 2.0f;
        healerPos.preferredRole = BotRole::HEALER;
        healerPos.priority = priority++;
        layout.positions.push_back(healerPos);

        // West (DPS)
        FormationPosition westPos;
        westPos.offsetX = -spacing * 2.0f;
        westPos.offsetY = 0.0f;
        westPos.preferredRole = BotRole::MELEE_DPS;
        westPos.priority = priority++;
        layout.positions.push_back(westPos);

        // East (DPS)
        FormationPosition eastPos;
        eastPos.offsetX = spacing * 2.0f;
        eastPos.offsetY = 0.0f;
        eastPos.preferredRole = BotRole::MELEE_DPS;
        eastPos.priority = priority++;
        layout.positions.push_back(eastPos);

        // Fill interior diamond with remaining bots
        uint32 remainingBots = botCount - 4;
        for (uint32 i = 0; i < remainingBots; ++i)
        {
            FormationPosition pos;

            // Distribute evenly across diamond quadrants
            float angle = (i / static_cast<float>(remainingBots)) * 2.0f * M_PI;
            float radius = spacing * 1.5f;

            pos.offsetX = radius * std::cos(angle);
            pos.offsetY = radius * std::sin(angle);
            pos.preferredRole = (i % 2 == 0) ? BotRole::RANGED_DPS : BotRole::UTILITY;
            pos.priority = priority++;
            layout.positions.push_back(pos);
        }

        CalculateFormationDimensions(layout);
        return layout;
    }

    FormationLayout GroupFormationManager::CreateDefensiveSquareFormation(
        uint32 botCount,
        float spacing)
    {
        FormationLayout layout;
        layout.type = FormationType::DEFENSIVE_SQUARE;
        layout.spacing = spacing;
        layout.description = "Square formation (healers center, tanks corners, DPS edges)";

        uint32 priority = 0;

        // Square has 4 corners (tanks) + 4 edges (DPS) + center (healers)
        float halfSize = spacing * 2.0f;

        // Corner 1: NW (tank)
        FormationPosition nw;
        nw.offsetX = -halfSize;
        nw.offsetY = halfSize;
        nw.preferredRole = BotRole::TANK;
        nw.priority = priority++;
        layout.positions.push_back(nw);

        // Corner 2: NE (tank)
        FormationPosition ne;
        ne.offsetX = halfSize;
        ne.offsetY = halfSize;
        ne.preferredRole = BotRole::TANK;
        ne.priority = priority++;
        layout.positions.push_back(ne);

        // Corner 3: SW (tank)
        FormationPosition sw;
        sw.offsetX = -halfSize;
        sw.offsetY = -halfSize;
        sw.preferredRole = BotRole::TANK;
        sw.priority = priority++;
        layout.positions.push_back(sw);

        // Corner 4: SE (tank)
        FormationPosition se;
        se.offsetX = halfSize;
        se.offsetY = -halfSize;
        se.preferredRole = BotRole::TANK;
        se.priority = priority++;
        layout.positions.push_back(se);

        // Center: Healers (protected)
        uint32 healerCount = std::max(1u, botCount / 5);
        for (uint32 i = 0; i < healerCount && layout.positions.size() < botCount; ++i)
        {
            FormationPosition healer;
            healer.offsetX = (i % 2 == 0) ? -spacing * 0.5f : spacing * 0.5f;
            healer.offsetY = (i / 2 % 2 == 0) ? -spacing * 0.5f : spacing * 0.5f;
            healer.preferredRole = BotRole::HEALER;
            healer.priority = 1; // High priority
            layout.positions.push_back(healer);
        }

        // Edges: DPS
        uint32 remainingBots = botCount - layout.positions.size();
        uint32 botsPerEdge = remainingBots / 4;

        // North edge
        for (uint32 i = 0; i < botsPerEdge; ++i)
        {
            FormationPosition pos;
            float t = (i + 1) / static_cast<float>(botsPerEdge + 1);
            pos.offsetX = -halfSize + (2.0f * halfSize * t);
            pos.offsetY = halfSize;
            pos.preferredRole = BotRole::RANGED_DPS;
            pos.priority = priority++;
            layout.positions.push_back(pos);
        }

        // South edge
        for (uint32 i = 0; i < botsPerEdge; ++i)
        {
            FormationPosition pos;
            float t = (i + 1) / static_cast<float>(botsPerEdge + 1);
            pos.offsetX = -halfSize + (2.0f * halfSize * t);
            pos.offsetY = -halfSize;
            pos.preferredRole = BotRole::RANGED_DPS;
            pos.priority = priority++;
            layout.positions.push_back(pos);
        }

        // West edge
        for (uint32 i = 0; i < botsPerEdge; ++i)
        {
            FormationPosition pos;
            float t = (i + 1) / static_cast<float>(botsPerEdge + 1);
            pos.offsetX = -halfSize;
            pos.offsetY = -halfSize + (2.0f * halfSize * t);
            pos.preferredRole = BotRole::MELEE_DPS;
            pos.priority = priority++;
            layout.positions.push_back(pos);
        }

        // East edge
        for (uint32 i = 0; i < botsPerEdge; ++i)
        {
            FormationPosition pos;
            float t = (i + 1) / static_cast<float>(botsPerEdge + 1);
            pos.offsetX = halfSize;
            pos.offsetY = -halfSize + (2.0f * halfSize * t);
            pos.preferredRole = BotRole::MELEE_DPS;
            pos.priority = priority++;
            layout.positions.push_back(pos);
        }

        CalculateFormationDimensions(layout);
        return layout;
    }

    FormationLayout GroupFormationManager::CreateArrowFormation(
        uint32 botCount,
        float spacing)
    {
        FormationLayout layout;
        layout.type = FormationType::ARROW;
        layout.spacing = spacing;
        layout.description = "Arrow formation (concentrated assault, tight arrowhead)";

        // Arrow is similar to wedge but tighter angle (sharper point)
        constexpr float ARROW_ANGLE = 20.0f * (M_PI / 180.0f); // 20 degrees (sharper than wedge)

        uint32 priority = 0;

        // Tip: Tank
        FormationPosition tip;
        tip.offsetX = 0.0f;
        tip.offsetY = spacing * 1.5f;
        tip.preferredRole = BotRole::TANK;
        tip.priority = priority++;
        layout.positions.push_back(tip);

        // Arrow shaft (remaining bots in tight V)
        uint32 remainingBots = botCount - 1;
        uint32 leftSide = remainingBots / 2;
        uint32 rightSide = remainingBots - leftSide;

        for (uint32 i = 0; i < leftSide; ++i)
        {
            float distance = spacing * (i + 1);
            FormationPosition pos;
            pos.offsetX = -distance * std::sin(ARROW_ANGLE);
            pos.offsetY = spacing - distance * std::cos(ARROW_ANGLE);
            pos.preferredRole = (i < 2) ? BotRole::MELEE_DPS : BotRole::RANGED_DPS;
            pos.priority = priority++;
            layout.positions.push_back(pos);
        }

        for (uint32 i = 0; i < rightSide; ++i)
        {
            float distance = spacing * (i + 1);
            FormationPosition pos;
            pos.offsetX = distance * std::sin(ARROW_ANGLE);
            pos.offsetY = spacing - distance * std::cos(ARROW_ANGLE);
            pos.preferredRole = (i < 2) ? BotRole::MELEE_DPS : BotRole::RANGED_DPS;
            pos.priority = priority++;
            layout.positions.push_back(pos);
        }

        CalculateFormationDimensions(layout);
        return layout;
    }

    FormationLayout GroupFormationManager::CreateLineFormation(
        uint32 botCount,
        float spacing)
    {
        FormationLayout layout;
        layout.type = FormationType::LINE;
        layout.spacing = spacing;
        layout.description = "Line formation (horizontal line, maximum frontal coverage)";

        float totalWidth = spacing * (botCount - 1);
        float startX = -totalWidth / 2.0f;

        uint32 priority = 0;

        for (uint32 i = 0; i < botCount; ++i)
        {
            FormationPosition pos;
            pos.offsetX = startX + (spacing * i);
            pos.offsetY = 0.0f;

            // Tanks on ends, DPS in middle, healers scattered
            if (i == 0 || i == botCount - 1)
                pos.preferredRole = BotRole::TANK;
            else if (i % 3 == 0)
                pos.preferredRole = BotRole::HEALER;
            else
                pos.preferredRole = (i % 2 == 0) ? BotRole::MELEE_DPS : BotRole::RANGED_DPS;

            pos.priority = priority++;
            layout.positions.push_back(pos);
        }

        CalculateFormationDimensions(layout);
        return layout;
    }

    FormationLayout GroupFormationManager::CreateColumnFormation(
        uint32 botCount,
        float spacing)
    {
        FormationLayout layout;
        layout.type = FormationType::COLUMN;
        layout.spacing = spacing;
        layout.description = "Column formation (single-file march, narrow passages)";

        uint32 priority = 0;

        for (uint32 i = 0; i < botCount; ++i)
        {
            FormationPosition pos;
            pos.offsetX = 0.0f;
            pos.offsetY = spacing * i - (spacing * botCount / 2.0f);

            // Tank front, healer rear, DPS middle
            if (i == 0)
                pos.preferredRole = BotRole::TANK;
            else if (i == botCount - 1)
                pos.preferredRole = BotRole::HEALER;
            else
                pos.preferredRole = (i % 2 == 0) ? BotRole::MELEE_DPS : BotRole::RANGED_DPS;

            pos.priority = priority++;
            layout.positions.push_back(pos);
        }

        CalculateFormationDimensions(layout);
        return layout;
    }

    FormationLayout GroupFormationManager::CreateScatterFormation(
        uint32 botCount,
        float spacing)
    {
        FormationLayout layout;
        layout.type = FormationType::SCATTER;
        layout.spacing = spacing;
        layout.description = "Scatter formation (random dispersed positions, anti-AoE)";

        // Use deterministic random for reproducibility
        std::mt19937 rng(42); // Fixed seed for consistency
        std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * M_PI);
        std::uniform_real_distribution<float> radiusDist(spacing * 2.0f, spacing * 5.0f);

        uint32 priority = 0;

        for (uint32 i = 0; i < botCount; ++i)
        {
            FormationPosition pos;

            float angle = angleDist(rng);
            float radius = radiusDist(rng);

            pos.offsetX = radius * std::cos(angle);
            pos.offsetY = radius * std::sin(angle);

            // Random role assignment (favors DPS)
            uint32 roleRoll = i % 5;
            if (roleRoll == 0)
                pos.preferredRole = BotRole::TANK;
            else if (roleRoll == 1)
                pos.preferredRole = BotRole::HEALER;
            else
                pos.preferredRole = (roleRoll % 2 == 0) ? BotRole::MELEE_DPS : BotRole::RANGED_DPS;

            pos.priority = priority++;
            layout.positions.push_back(pos);
        }

        CalculateFormationDimensions(layout);
        return layout;
    }

    FormationLayout GroupFormationManager::CreateCircleFormation(
        uint32 botCount,
        float spacing)
    {
        FormationLayout layout;
        layout.type = FormationType::CIRCLE;
        layout.spacing = spacing;
        layout.description = "Circle formation (360-degree coverage, defensive perimeter)";

        float radius = spacing * botCount / (2.0f * M_PI); // Circumference = 2πr
        float angleIncrement = (2.0f * M_PI) / botCount;

        uint32 priority = 0;

        for (uint32 i = 0; i < botCount; ++i)
        {
            FormationPosition pos;

            float angle = angleIncrement * i;
            pos.offsetX = radius * std::cos(angle);
            pos.offsetY = radius * std::sin(angle);

            // Tanks evenly distributed, healers between tanks, DPS fill gaps
            if (i % (botCount / 4) == 0)
                pos.preferredRole = BotRole::TANK;
            else if (i % (botCount / 8) == 0)
                pos.preferredRole = BotRole::HEALER;
            else
                pos.preferredRole = (i % 2 == 0) ? BotRole::RANGED_DPS : BotRole::MELEE_DPS;

            pos.priority = priority++;
            layout.positions.push_back(pos);
        }

        CalculateFormationDimensions(layout);
        return layout;
    }

    // ============================================================================
    // Private Helper Methods
    // ============================================================================

    void GroupFormationManager::RotatePosition(
        float offsetX,
        float offsetY,
        float angle,
        float& rotatedX,
        float& rotatedY)
    {
        // 2D rotation matrix:
        // [cos θ  -sin θ] [x]
        // [sin θ   cos θ] [y]

        float cosAngle = std::cos(angle);
        float sinAngle = std::sin(angle);

        rotatedX = offsetX * cosAngle - offsetY * sinAngle;
        rotatedY = offsetX * sinAngle + offsetY * cosAngle;
    }

    void GroupFormationManager::CalculateFormationDimensions(FormationLayout& formation)
    {
        if (formation.positions.empty())
        {
            formation.width = 0.0f;
            formation.depth = 0.0f;
            return;
        }

        float minX = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float minY = std::numeric_limits<float>::max();
        float maxY = std::numeric_limits<float>::lowest();

        for (FormationPosition const& pos : formation.positions)
        {
            minX = std::min(minX, pos.offsetX);
            maxX = std::max(maxX, pos.offsetX);
            minY = std::min(minY, pos.offsetY);
            maxY = std::max(maxY, pos.offsetY);
        }

        formation.width = maxX - minX;
        formation.depth = maxY - minY;

        TC_LOG_DEBUG("playerbot.formation",
            "GroupFormationManager: Formation {} - width: {:.1f}, depth: {:.1f}, positions: {}",
            GetFormationName(formation.type), formation.width, formation.depth, formation.positions.size());
    }

} // namespace Playerbot
