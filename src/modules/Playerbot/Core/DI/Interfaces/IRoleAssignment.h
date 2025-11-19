/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include "Player.h"
#include <vector>
#include <unordered_map>

class Player;
class Group;

namespace Playerbot
{

// Forward declarations
enum class GroupRole : uint8;
enum class RoleCapability : uint8;
enum class RoleAssignmentStrategy : uint8;
struct RoleScore;
struct PlayerRoleProfile;
struct GroupComposition;
struct RolePerformance;
struct RoleStatistics;

class TC_GAME_API IRoleAssignment
{
public:
    virtual ~IRoleAssignment() = default;

    // Core role assignment
    virtual bool AssignRoles(Group* group, RoleAssignmentStrategy strategy) = 0;
    virtual bool AssignRole(uint32 playerGuid, GroupRole role, Group* group) = 0;
    virtual bool SwapRoles(uint32 player1Guid, uint32 player2Guid, Group* group) = 0;
    virtual void OptimizeRoleDistribution(Group* group) = 0;

    // Role analysis and scoring
    virtual PlayerRoleProfile AnalyzePlayerCapabilities(Player* player) = 0;
    virtual std::vector<RoleScore> CalculateRoleScores(Player* player, Group* group) = 0;
    virtual GroupRole RecommendRole(Player* player, Group* group) = 0;
    virtual float CalculateRoleSynergy(Player* player, GroupRole role, Group* group) = 0;

    // Group composition analysis
    virtual GroupComposition AnalyzeGroupComposition(Group* group) = 0;
    virtual bool IsCompositionViable(const GroupComposition& composition) = 0;
    virtual std::vector<GroupRole> GetMissingRoles(Group* group) = 0;
    virtual std::vector<uint32> FindPlayersForRole(GroupRole role, const std::vector<Player*>& candidates) = 0;

    // Dynamic role adjustment
    virtual void HandleRoleConflict(Group* group, GroupRole conflictedRole) = 0;
    virtual void RebalanceRoles(Group* group) = 0;
    virtual void AdaptToGroupChanges(Group* group, Player* newMember = nullptr, Player* leavingMember = nullptr) = 0;
    virtual bool CanPlayerSwitchRole(Player* player, GroupRole newRole, Group* group) = 0;

    // Content-specific role optimization
    virtual void OptimizeForDungeon(Group* group, uint32 dungeonId) = 0;
    virtual void OptimizeForRaid(Group* group, uint32 raidId) = 0;
    virtual void OptimizeForPvP(Group* group, uint32 battlegroundId) = 0;
    virtual void OptimizeForQuesting(Group* group, uint32 questId) = 0;

    // Role preferences and constraints
    virtual void SetPlayerRolePreference(uint32 playerGuid, GroupRole preferredRole) = 0;
    virtual GroupRole GetPlayerRolePreference(uint32 playerGuid) = 0;
    virtual void SetRoleFlexibility(uint32 playerGuid, bool isFlexible) = 0;
    virtual void AddRoleConstraint(uint32 playerGuid, GroupRole role, RoleCapability capability) = 0;

    // Role performance tracking
    virtual RolePerformance GetPlayerRolePerformance(uint32 playerGuid, GroupRole role) = 0;
    virtual void UpdateRolePerformance(uint32 playerGuid, GroupRole role, bool wasSuccessful, float effectiveness) = 0;

    // Role assignment validation
    virtual bool ValidateRoleAssignment(Group* group) = 0;
    virtual std::vector<std::string> GetRoleAssignmentIssues(Group* group) = 0;
    virtual bool CanGroupFunction(Group* group) = 0;

    // Emergency role filling
    virtual bool FillEmergencyRole(Group* group, GroupRole urgentRole) = 0;
    virtual std::vector<uint32> FindEmergencyReplacements(GroupRole role, uint32 minLevel, uint32 maxLevel) = 0;
    virtual void HandleRoleEmergency(Group* group, uint32 disconnectedPlayerGuid) = 0;

    // Role statistics and monitoring
    virtual RoleStatistics GetGlobalRoleStatistics() = 0;
    virtual void UpdateRoleStatistics() = 0;

    // Configuration and settings
    virtual void SetRoleAssignmentStrategy(Group* group, RoleAssignmentStrategy strategy) = 0;
    virtual void SetContentTypeRequirements(uint32 contentId, const std::unordered_map<GroupRole, uint32>& requirements) = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void RefreshPlayerProfiles() = 0;
    virtual void CleanupInactiveProfiles() = 0;
};

} // namespace Playerbot
