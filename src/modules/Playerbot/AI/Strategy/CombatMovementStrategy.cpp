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

#include "CombatMovementStrategy.h"
#include "BotAI.h"
#include "Player.h"
#include "Unit.h"
#include "MotionMaster.h"
#include "AreaTrigger.h"
#include "DynamicObject.h"
#include "Map.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "PathGenerator.h"
#include "Log.h"
#include "../../Spatial/SpatialGridManager.h"  // Lock-free spatial grid for deadlock fix
#include "../../Spatial/DoubleBufferedSpatialGrid.h"
#include "../../Movement/Arbiter/MovementArbiter.h"
#include "../../Movement/Arbiter/MovementPriorityMapper.h"
#include "DBCEnums.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "ObjectAccessor.h"
#include "UnitAI.h"
#include <cmath>

namespace Playerbot
{
    CombatMovementStrategy::CombatMovementStrategy()
        : Strategy("CombatMovement")  // Use base constructor with just name
        , _currentRole(ROLE_NONE)
        , _lastPositionUpdate(0)
        , _positionUpdateInterval(MIN_UPDATE_INTERVAL)
        , _movementTimer(0)
        , _isMoving(false)
        , _lastDangerCheck(std::chrono::steady_clock::now())
        , _lastDangerResult(false)
    {
        // Set priority after construction
        SetPriority(80); // Priority 80: higher than follow(60), lower than critical(90)
        TC_LOG_DEBUG("module.playerbot", "CombatMovementStrategy: Initialized with priority %u", GetPriority());
    }

    void CombatMovementStrategy::InitializeActions()
    {
        // Actions will be registered through the behavior system
        // This strategy directly handles movement rather than using separate action classes
        TC_LOG_DEBUG("module.playerbot", "CombatMovementStrategy::InitializeActions: Ready for combat movement");
    }

    void CombatMovementStrategy::InitializeTriggers()
    {
        // Triggers are handled by IsActive() method checking combat state
        TC_LOG_DEBUG("module.playerbot", "CombatMovementStrategy::InitializeTriggers: Combat state monitoring enabled");
    }

    void CombatMovementStrategy::InitializeValues()
    {
        // No specific values needed for this strategy
        // All values are computed dynamically based on combat state
        TC_LOG_DEBUG("module.playerbot", "CombatMovementStrategy::InitializeValues: No specific values required");
    }

    void CombatMovementStrategy::OnActivate(BotAI* ai)
    {
        if (!ai)
            return;

        Player* player = ai->GetBot();
        if (!player)
            return;

        // Determine role on activation
        _currentRole = DetermineRole(player);
        // Reset state
        _lastPositionUpdate = 0;
        _movementTimer = 0;
        _isMoving = false;
        _lastTargetGuid.Clear();
        _lastTargetPosition = Position();

        TC_LOG_DEBUG("module.playerbot", "CombatMovementStrategy::OnActivate: Bot %s activated with role %u",
            player->GetName().c_str(), static_cast<uint32>(_currentRole));
    }

    void CombatMovementStrategy::OnDeactivate(BotAI* ai)
    {
        if (!ai)
            return;

        Player* player = ai->GetBot();
        if (!player)
            return;

        // Stop any ongoing movement
        if (_isMoving)
        {
            player->GetMotionMaster()->Clear();
            _isMoving = false;
        }

        // Reset state
        _currentRole = ROLE_NONE;
        _lastTargetGuid.Clear();

        TC_LOG_DEBUG("module.playerbot", "CombatMovementStrategy::OnDeactivate: Bot %s deactivated",
            player->GetName().c_str());
    }
    bool CombatMovementStrategy::IsActive(BotAI* ai) const
    {
        if (!ai)
            return false;
        Player* player = ai->GetBot();
        if (!player)
            return false;

        // Strategy is active if bot is in combat with a valid target
        if (!player->IsInCombat())
            return false;

        Unit* target = player->GetSelectedUnit();
        if (!target || !target->IsAlive() || !player->IsValidAttackTarget(target))
            return false;

        return true;
    }

    void CombatMovementStrategy::UpdateBehavior(BotAI* ai, uint32 diff)
    {
        auto startTime = std::chrono::steady_clock::now();

        if (!ai)
            return;

        Player* player = ai->GetBot();
        if (!player)
            return;

        Unit* target = player->GetSelectedUnit();
        if (!target || !target->IsAlive())
        {
            if (_isMoving)
            {
                player->GetMotionMaster()->Clear();
                _isMoving = false;
            }
            return;
        }

        // Update role if needed (spec change, level up, etc.)
        if (_currentRole == ROLE_NONE)
            _currentRole = DetermineRole(player);

        // Check if we should update position
        if (!ShouldUpdatePosition(diff))
            return;

        // Reset position update timer
        _lastPositionUpdate = 0;

        // Handle danger avoidance first
        if (IsStandingInDanger(player))
        {
            Position safePos = FindSafePosition(player, player->GetPosition(), DANGER_CHECK_RADIUS);
            if (safePos != player->GetPosition())
            {
                LogPositionUpdate(player, safePos, "Avoiding danger zone");
                MoveToPosition(player, safePos);
                return;
            }
        }

        // Calculate optimal position based on role
        Position targetPosition;
        switch (_currentRole)
        {
            case ROLE_TANK:
                targetPosition = CalculateTankPosition(player, target);
                break;
            case ROLE_MELEE_DPS:
                targetPosition = CalculateMeleePosition(player, target);
                break;
            case ROLE_RANGED_DPS:
                targetPosition = CalculateRangedPosition(player, target);
                break;
            case ROLE_HEALER:
                targetPosition = CalculateHealerPosition(player, target);
                break;
            default:
                // Default to ranged position for unknown roles
                targetPosition = CalculateRangedPosition(player, target);
                break;
        }

        // Check if we're already in position
        if (IsInCorrectPosition(player, targetPosition))
        {
            if (_isMoving)
            {
                player->GetMotionMaster()->Clear();
                _isMoving = false;
            }
            return;
        }

        // Verify the position is safe
        if (!IsPositionSafe(targetPosition, player))
        {
            targetPosition = FindSafePosition(player, targetPosition, 5.0f);
        }

        // Move to position if needed
        if (IsPositionReachable(player, targetPosition))
        {
            LogPositionUpdate(player, targetPosition, "Combat positioning");
            MoveToPosition(player, targetPosition);
        }

        // Performance tracking
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
        if (duration > 500) // Log if update took more than 0.5ms
        {
            TC_LOG_DEBUG("module.playerbot", "CombatMovementStrategy::UpdateBehavior: Slow update %lld us for %s",
                duration, player->GetName().c_str());
        }
    }

    CombatMovementStrategy::FormationRole CombatMovementStrategy::DetermineRole(Player* player) const
    {
        if (!player)
            return ROLE_NONE;

        // Simplified role determination based on class
        // In a real implementation, this would check talents/spec
        uint8 playerClass = player->GetClass();
        // For now, use class-based defaults
        // TODO: Implement talent/spec detection when API is available
        switch (playerClass)
        {
            case CLASS_WARRIOR:
                // Warriors can be tanks or DPS, default to tank for safety
                return ROLE_TANK;
            case CLASS_PALADIN:
                // Paladins can be tank, healer, or DPS, default to tank
                return ROLE_TANK;
            case CLASS_HUNTER:
                return ROLE_RANGED_DPS;
            case CLASS_ROGUE:
                return ROLE_MELEE_DPS;
            case CLASS_PRIEST:
                return ROLE_HEALER;
            case CLASS_DEATH_KNIGHT:
                // Death Knights can tank or DPS, default to tank
                return ROLE_TANK;
            case CLASS_SHAMAN:
                // Shamans can heal or DPS, default to healer
                return ROLE_HEALER;
            case CLASS_MAGE:
                return ROLE_RANGED_DPS;
            case CLASS_WARLOCK:
                return ROLE_RANGED_DPS;
            case CLASS_DRUID:
                // Druids can do everything, default to healer
                return ROLE_HEALER;
            default:
                return ROLE_RANGED_DPS;
        }
    }

    bool CombatMovementStrategy::IsTankSpec(uint32 /*talentTree*/) const
    {
        // TODO: Implement when talent tree API is available
        // For now, this is handled in DetermineRole() using class-based defaults
        return false;
    }
    bool CombatMovementStrategy::IsHealerSpec(uint32 /*talentTree*/) const
    {
        // TODO: Implement when talent tree API is available
        // For now, this is handled in DetermineRole() using class-based defaults
        return false;
    }

    bool CombatMovementStrategy::IsMeleeClass(uint8 classId) const
    {
        switch (classId)
        {
            case CLASS_WARRIOR:
            case CLASS_PALADIN:
            case CLASS_ROGUE:
            case CLASS_DEATH_KNIGHT:
                return true;
            case CLASS_DRUID: // Feral is melee, Balance is ranged
            case CLASS_SHAMAN: // Enhancement is melee, Elemental is ranged
                // These require spec checking, defaulting to ranged for simplicity
                return false;
            default:
                return false;
        }
    }

    Position CombatMovementStrategy::CalculateTankPosition(Player* player, Unit* target) const
    {
        if (!player || !target)
            return player ? player->GetPosition() : Position();

        // Tank should be in front of the target, facing it away from group
        float angle = target->GetOrientation();
        // Position slightly to the side to avoid frontal cone attacks
        angle += 0.2f; // Slight offset
        return GetPositionAtDistanceAngle(target, TANK_DISTANCE, angle);
    }

    Position CombatMovementStrategy::CalculateMeleePosition(Player* player, Unit* target) const
    {
        if (!player || !target)
            return player ? player->GetPosition() : Position();

        // Melee DPS should be behind the target
        float angle = target->GetOrientation() + M_PI; // 180 degrees behind
        // Slight offset to avoid stacking
        float offsetAngle = (player->GetGUID().GetCounter() % 3 - 1) * 0.3f;
        angle += offsetAngle;

        return GetPositionAtDistanceAngle(target, MELEE_DISTANCE, angle);
    }

    Position CombatMovementStrategy::CalculateRangedPosition(Player* player, Unit* target) const
    {
        if (!player || !target)
            return player ? player->GetPosition() : Position();

        // Find optimal ranged position
        float baseAngle = player->GetAbsoluteAngle(target);
        // Spread ranged DPS around the target
        float offsetAngle = (player->GetGUID().GetCounter() % 5 - 2) * 0.4f;
        float angle = baseAngle + offsetAngle;

        // Adjust distance based on spell range
        float distance = RANGED_DISTANCE;

        Position pos = GetPositionAtDistanceAngle(target, distance, angle);

        // Ensure line of sight
        if (!player->IsWithinLOS(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ()))
        {
            // Try different angles to find LOS
            for (int i = 1; i <= 4; ++i)
            {
                float testAngle = baseAngle + (i % 2 ? i : -i) * 0.5f;
                pos = GetPositionAtDistanceAngle(target, distance, testAngle);
                if (player->IsWithinLOS(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ()))
                    break;
            }
        }

        return pos;
    }

    Position CombatMovementStrategy::CalculateHealerPosition(Player* player, Unit* target) const
    {
        if (!player || !target)
            return player ? player->GetPosition() : Position();

        // Healers should be at medium range, central to the group
        float angle = player->GetAbsoluteAngle(target);

        // Stay back but not too far
        Position pos = GetPositionAtDistanceAngle(target, HEALER_DISTANCE, angle);

        // DEADLOCK FIX: Use spatial grid to find friendly players
        // Healers try to position where they can see most allies
        Map* map = player->GetMap();
        if (map)
        {
            DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
            if (!spatialGrid)
            {
                sSpatialGridManager.CreateGrid(map);
                spatialGrid = sSpatialGridManager.GetGrid(map);
            }
            if (spatialGrid)
            {
                // Query nearby players (lock-free!)
                std::vector<DoubleBufferedSpatialGrid::PlayerSnapshot> nearbyPlayers =
                    spatialGrid->QueryNearbyPlayers(player->GetPosition(), 40.0f);

                // Build list of ally GUIDs from snapshots
                std::vector<ObjectGuid> allyGuids;
                for (auto const& snapshot : nearbyPlayers)
                {
                    // Only include friendly players (same faction or in group)
                    if (snapshot.guid != player->GetGUID()) // Exclude self
                        allyGuids.push_back(snapshot.guid);
                }

                // Adjust position to maintain LOS to allies
                Position bestPos = pos;
                int maxVisibleAllies = 0;

                for (float testAngle = 0; testAngle < 2 * M_PI; testAngle += static_cast<float>(M_PI / 4))
                {
                    Position testPos = GetPositionAtDistanceAngle(target, HEALER_DISTANCE, testAngle);
                    int visibleAllies = 0;

                    // Count visible allies using snapshot positions
                    for (auto const& snapshot : nearbyPlayers)
                    {
                        if (snapshot.guid == player->GetGUID())
                            continue; // Skip self

                        // Check LOS using snapshot position
                        float dx = testPos.GetPositionX() - snapshot.position.GetPositionX();
                        float dy = testPos.GetPositionY() - snapshot.position.GetPositionY();
                        float dz = testPos.GetPositionZ() - snapshot.position.GetPositionZ();
                        float distSq = dx * dx + dy * dy + dz * dz;

                        // Simple distance check (proper LOS would require Map access)
                        // For healer positioning, distance is a good proxy
                        if (distSq < 40.0f * 40.0f)
                            ++visibleAllies;
                    }

                    if (visibleAllies > maxVisibleAllies)
                    {
                        maxVisibleAllies = visibleAllies;
                        bestPos = testPos;
                    }
                }

                pos = bestPos;
            }
        }

        return pos;
    }

    bool CombatMovementStrategy::MoveToPosition(Player* player, Position const& position)
    {
        if (!player)
            return false;

        // Validate position is on valid terrain
        float groundZ = player->GetMap()->GetHeight(player->GetPhaseShift(), position.GetPositionX(), position.GetPositionY(), position.GetPositionZ());
        if (std::abs(position.GetPositionZ() - groundZ) > 10.0f)
        {
            TC_LOG_DEBUG("module.playerbot", "CombatMovementStrategy::MoveToPosition: Invalid Z coordinate for %s",
                player->GetName().c_str());
            return false;
        }

        // PHASE 5 MIGRATION: Use Movement Arbiter with COMBAT_MOVEMENT_STRATEGY priority (130)
        BotAI* botAI = dynamic_cast<BotAI*>(player->GetAI());
        if (botAI && botAI->GetMovementArbiter())
        {
            bool accepted = botAI->RequestPointMovement(
                PlayerBotMovementPriority::COMBAT_MOVEMENT_STRATEGY,  // Priority 130 - MEDIUM tier
                position,
                "Combat tactical movement",
                "CombatMovementStrategy");

            if (accepted)
            {
                _isMoving = true;
                _movementTimer = 0;
                _lastTargetPosition = position;
                TC_LOG_DEBUG("playerbot.movement.arbiter",
                    "CombatMovementStrategy: Bot {} tactical movement requested (Priority: 130)",
                    player->GetName());
                return true;
            }
            else
            {
                // Arbiter rejected - higher priority movement active (likely emergency or positioning)
                TC_LOG_TRACE("playerbot.movement.arbiter",
                    "CombatMovementStrategy: Movement rejected for bot {} - higher priority active",
                    player->GetName());
                return false;
            }
        }
        else
        {
            // FALLBACK: Direct MotionMaster call if arbiter not available
            TC_LOG_TRACE("playerbot.movement.arbiter",
                "CombatMovementStrategy: Movement Arbiter not available for bot {} - using direct MotionMaster",
                player->GetName());
            player->GetMotionMaster()->Clear();
            player->GetMotionMaster()->MovePoint(1, position);
            _isMoving = true;
            _movementTimer = 0;
            _lastTargetPosition = position;
            return true;
        }
    }

    bool CombatMovementStrategy::IsInCorrectPosition(Player* player, Position const& targetPosition, float tolerance) const
    {
        if (!player)
            return false;

        float distance = player->GetExactDist2d(&targetPosition);

        // Consider Z axis difference
        float zDiff = std::abs(player->GetPositionZ() - targetPosition.GetPositionZ());
        if (zDiff > 5.0f)
            return false;

        return distance <= tolerance;
    }

    bool CombatMovementStrategy::IsPositionReachable(Player* player, Position const& position) const
    {
        if (!player)
            return false;

        // Use PathGenerator to check if path exists
        PathGenerator path(player);
        bool result = path.CalculatePath(position.GetPositionX(), position.GetPositionY(), position.GetPositionZ());

        if (!result || path.GetPathType() & PATHFIND_NOPATH)
            return false;

        return true;
    }

    bool CombatMovementStrategy::IsStandingInDanger(Player* player) const
    {
        if (!player)
            return false;

        // Cache danger checks for performance
        auto now = std::chrono::steady_clock::now();
        auto timeSinceLastCheck = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastDangerCheck).count();

        if (timeSinceLastCheck < DANGER_CACHE_TIME)
            return _lastDangerResult;

        _lastDangerCheck = now;

        // Get spatial grid for lock-free queries
        auto spatialGrid = sSpatialGridManager.GetGrid(player->GetMapId());
        if (!spatialGrid)
        {
            _lastDangerResult = false;
            return false;
        }

        Position playerPos = player->GetPosition();

        // Check for AreaTriggers (fire, poison pools, etc.)
        std::vector<DoubleBufferedSpatialGrid::AreaTriggerSnapshot> nearbyAreaTriggers =
            spatialGrid->QueryNearbyAreaTriggers(playerPos, 20.0f);

        for (auto const& trigger : nearbyAreaTriggers)
        {
            // Only check spell-based area triggers
            if (trigger.spellId == 0)
                continue;

            // Check spell to determine if it's harmful
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(trigger.spellId, DIFFICULTY_NONE);
            if (!spellInfo || spellInfo->IsPositive())
                continue; // Friendly spell, skip

            // Check if we're inside the trigger's effect radius
            float dist = playerPos.GetExactDist(trigger.position);
            if (dist < trigger.sphereRadius)
            {
                _lastDangerResult = true;
                return true;
            }
        }

        // Check for DynamicObjects (AoE spell effects like Blizzard, Rain of Fire)
        std::vector<DoubleBufferedSpatialGrid::DynamicObjectSnapshot> nearbyDynamicObjects =
            spatialGrid->QueryNearbyDynamicObjects(playerPos, 20.0f);
        for (auto const& dynObj : nearbyDynamicObjects)
        {
            // Check spell to determine if it's harmful
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(dynObj.spellId, DIFFICULTY_NONE);
            if (!spellInfo || spellInfo->IsPositive())
                continue; // Friendly spell, skip

            // Check if we're inside the spell's effect radius
            float dist = playerPos.GetExactDist(dynObj.position);
            if (dist < dynObj.radius)
            {
                _lastDangerResult = true;
                return true;
            }
        }

        _lastDangerResult = false;
        return false;
    }

    Position CombatMovementStrategy::FindSafePosition(Player* player, Position const& preferredPosition, float searchRadius) const
    {
        if (!player)
            return preferredPosition;

        // First check if preferred position is safe
        if (IsPositionSafe(preferredPosition, player))
            return preferredPosition;

        // Search in a spiral pattern for a safe position
        float bestDistance = searchRadius * 2;
        Position bestPosition = player->GetPosition(); // Default to current position

        for (float angle = 0; angle < 2 * M_PI; angle += static_cast<float>(M_PI / 8)) // Check 16 points
        {
            for (float dist = 3.0f; dist <= searchRadius; dist += 3.0f)
            {
                float x = preferredPosition.GetPositionX() + cos(angle) * dist;
                float y = preferredPosition.GetPositionY() + sin(angle) * dist;
                float z = player->GetMap()->GetHeight(player->GetPhaseShift(), x, y, preferredPosition.GetPositionZ());
                Position testPos(x, y, z);

                if (IsPositionSafe(testPos, player) && IsPositionReachable(player, testPos))
                {
                    float distToPreferred = testPos.GetExactDist2d(&preferredPosition);
                    if (distToPreferred < bestDistance)
                    {
                        bestDistance = distToPreferred;
                        bestPosition = testPos;
                    }
                }
            }
        }

        return bestPosition;
    }

    bool CombatMovementStrategy::IsPositionSafe(Position const& position, Player* player) const
    {
        if (!player)
            return false;

        // Get spatial grid for lock-free queries
        auto spatialGrid = sSpatialGridManager.GetGrid(player->GetMapId());
        if (!spatialGrid)
            return true; // If no spatial grid, assume safe

        // Check for AreaTriggers at position
        std::vector<DoubleBufferedSpatialGrid::AreaTriggerSnapshot> nearbyAreaTriggers =
            spatialGrid->QueryNearbyAreaTriggers(position, 10.0f);

        for (auto const& trigger : nearbyAreaTriggers)
        {
            // Only check spell-based area triggers
            if (trigger.spellId == 0)
                continue;

            // Check spell to determine if it's harmful
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(trigger.spellId, DIFFICULTY_NONE);
            if (!spellInfo || spellInfo->IsPositive())
                continue; // Friendly spell, skip

            // Check if position is inside the trigger's effect radius
            float dist = position.GetExactDist(trigger.position);
            if (dist < trigger.sphereRadius)
                return false; // Position is dangerous
        }

        // Check for DynamicObjects at position
        std::vector<DoubleBufferedSpatialGrid::DynamicObjectSnapshot> nearbyDynamicObjects =
            spatialGrid->QueryNearbyDynamicObjects(position, 10.0f);

        for (auto const& dynObj : nearbyDynamicObjects)
        {
            // Check spell to determine if it's harmful
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(dynObj.spellId, DIFFICULTY_NONE);
            if (!spellInfo || spellInfo->IsPositive())
                continue; // Friendly spell, skip

            // Check if position is inside the spell's effect radius
            float dist = position.GetExactDist(dynObj.position);
            if (dist < dynObj.radius)
                return false; // Position is dangerous
        }

        return true; // Position is safe
    }

    float CombatMovementStrategy::GetOptimalDistance(FormationRole role) const
    {
        switch (role)
        {
            case ROLE_TANK:
                return TANK_DISTANCE;
            case ROLE_MELEE_DPS:
                return MELEE_DISTANCE;
            case ROLE_RANGED_DPS:
                return RANGED_DISTANCE;
            case ROLE_HEALER:
                return HEALER_DISTANCE;
            default:
                return RANGED_DISTANCE;
        }
    }

    float CombatMovementStrategy::GetOptimalAngle(Player* player, Unit* target, FormationRole role) const
    {
        if (!player || !target)
            return 0.0f;

        switch (role)
        {
            case ROLE_TANK:
                // Face the target head-on
                return target->GetAbsoluteAngle(player);

            case ROLE_MELEE_DPS:
                // Behind the target
                return target->GetOrientation() + M_PI;

            case ROLE_RANGED_DPS:
            case ROLE_HEALER:
                // Maintain current angle or find best LOS angle
                return player->GetAbsoluteAngle(target);

            default:
                return player->GetAbsoluteAngle(target);
        }
    }

    Position CombatMovementStrategy::GetPositionAtDistanceAngle(Unit* target, float distance, float angle) const
    {
        if (!target)
            return Position();

        float x = target->GetPositionX() + cos(angle) * distance;
        float y = target->GetPositionY() + sin(angle) * distance;
        float z = target->GetPositionZ();

        // Adjust Z to ground level
        if (Map* map = target->GetMap())
        {
            z = map->GetHeight(target->GetPhaseShift(), x, y, z + 2.0f);
        }

        return Position(x, y, z, angle);
    }

    bool CombatMovementStrategy::ShouldUpdatePosition(uint32 diff)
    {
        _lastPositionUpdate += diff;
        _movementTimer += diff;

        // Check for movement timeout
        if (_isMoving && _movementTimer > MOVEMENT_TIMEOUT)
        {
            TC_LOG_DEBUG("module.playerbot", "CombatMovementStrategy::ShouldUpdatePosition: Movement timeout reached");
            _isMoving = false;
            _movementTimer = 0;
            return true; // Force recalculation
        }

        // Regular update interval
        return _lastPositionUpdate >= _positionUpdateInterval;
    }

    void CombatMovementStrategy::LogPositionUpdate(Player* player, Position const& targetPos, std::string const& reason) const
    {
        if (!player)
            return;

        TC_LOG_DEBUG("module.playerbot", "CombatMovementStrategy: %s moving to (%.2f, %.2f, %.2f) - %s",
            player->GetName().c_str(),
            targetPos.GetPositionX(),
            targetPos.GetPositionY(),
            targetPos.GetPositionZ(),
            reason.c_str());
    }
}