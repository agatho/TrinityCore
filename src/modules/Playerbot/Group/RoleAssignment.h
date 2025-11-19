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
#include "Threading/LockHierarchy.h"
#include "Player.h"
#include "Group.h"
#include "../Core/DI/Interfaces/IRoleAssignment.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include "GameTime.h"

class Player;
class Group;

namespace Playerbot
{

enum class GroupRole : uint8
{
    TANK        = 0,
    HEALER      = 1,
    MELEE_DPS   = 2,
    RANGED_DPS  = 3,
    SUPPORT     = 4,
    UTILITY     = 5,
    NONE        = 6
};

enum class RoleCapability : uint8
{
    PRIMARY     = 0,  // Main specialization role
    SECONDARY   = 1,  // Off-spec capable
    HYBRID      = 2,  // Dual-role capable (e.g., Paladin tank/heal)
    EMERGENCY   = 3,  // Can fill role in emergency
    INCAPABLE   = 4   // Cannot perform this role
};

enum class RoleAssignmentStrategy : uint8
{
    OPTIMAL         = 0,  // Best possible role distribution
    BALANCED        = 1,  // Even distribution of capabilities
    FLEXIBLE        = 2,  // Adapt to group needs
    STRICT          = 3,  // Only assign primary roles
    HYBRID_FRIENDLY = 4,  // Favor hybrid classes
    DUNGEON_FOCUSED = 5,  // Optimize for dungeon content
    RAID_FOCUSED    = 6,  // Optimize for raid content
    PVP_FOCUSED     = 7   // Optimize for PvP content
};

struct RoleScore
{
    GroupRole role;
    float effectiveness;        // 0.0 - 1.0 how well they can perform role
    float gearScore;           // 0.0 - 1.0 gear appropriateness for role
    float experienceScore;     // 0.0 - 1.0 bot's experience in role
    float availabilityScore;   // 0.0 - 1.0 willingness to perform role
    float synergy;             // 0.0 - 1.0 synergy with group composition
    float totalScore;          // Combined weighted score

    RoleScore(GroupRole r = GroupRole::NONE) : role(r), effectiveness(0.0f)
        , gearScore(0.0f), experienceScore(0.5f), availabilityScore(1.0f)
        , synergy(0.5f), totalScore(0.0f) {}

    void CalculateTotalScore() {
        totalScore = (effectiveness * 0.4f) + (gearScore * 0.25f) +
                    (experienceScore * 0.15f) + (availabilityScore * 0.1f) +
                    (synergy * 0.1f);
    }
};

struct PlayerRoleProfile
{
    uint32 playerGuid;
    uint8 playerClass;
    uint8 playerSpec;
    uint32 playerLevel;
    std::unordered_map<GroupRole, RoleCapability> roleCapabilities;
    std::unordered_map<GroupRole, RoleScore> roleScores;
    GroupRole preferredRole;
    GroupRole assignedRole;
    std::vector<GroupRole> alternativeRoles;
    uint32 lastRoleUpdate;
    bool isFlexible;
    float overallRating;

    PlayerRoleProfile(uint32 guid, uint8 cls, uint8 spec, uint32 level)
        : playerGuid(guid), playerClass(cls), playerSpec(spec), playerLevel(level)
        , preferredRole(GroupRole::NONE), assignedRole(GroupRole::NONE)
        , lastRoleUpdate(GameTime::GetGameTimeMS()), isFlexible(true), overallRating(5.0f) {}
};

struct GroupComposition
{
    std::unordered_map<GroupRole, std::vector<uint32>> roleAssignments; // role -> player guids
    std::unordered_map<GroupRole, uint32> roleRequirements;             // role -> count needed
    std::unordered_map<GroupRole, uint32> roleFulfillment;              // role -> count assigned
    float compositionScore;
    bool isValid;
    bool hasMainTank;
    bool hasMainHealer;
    uint32 dpsCount;
    uint32 totalMembers;

    GroupComposition() : compositionScore(0.0f), isValid(false)
        , hasMainTank(false), hasMainHealer(false), dpsCount(0), totalMembers(0) {
        // Initialize role requirements for standard 5-man group
        roleRequirements[GroupRole::TANK] = 1;
        roleRequirements[GroupRole::HEALER] = 1;
        roleRequirements[GroupRole::MELEE_DPS] = 2;
        roleRequirements[GroupRole::RANGED_DPS] = 1;
        roleRequirements[GroupRole::SUPPORT] = 0;
    }
};

class TC_GAME_API RoleAssignment final : public IRoleAssignment
{
public:
    explicit RoleAssignment(Player* bot);
    ~RoleAssignment();
    RoleAssignment(RoleAssignment const&) = delete;
    RoleAssignment& operator=(RoleAssignment const&) = delete;

    // Core role assignment
    bool AssignRoles(Group* group, RoleAssignmentStrategy strategy = RoleAssignmentStrategy::OPTIMAL) override;
    bool AssignRole(GroupRole role, Group* group) override;
    bool SwapRoles(uint32 player1Guid, uint32 player2Guid, Group* group) override;
    void OptimizeRoleDistribution(Group* group) override;

    // Role analysis and scoring
    PlayerRoleProfile AnalyzePlayerCapabilities() override;
    std::vector<RoleScore> CalculateRoleScores(Group* group) override;
    GroupRole RecommendRole(Group* group) override;
    float CalculateRoleSynergy(GroupRole role, Group* group) override;

    // Group composition analysis
    GroupComposition AnalyzeGroupComposition(Group* group) override;
    bool IsCompositionViable(const GroupComposition& composition) override;
    std::vector<GroupRole> GetMissingRoles(Group* group) override;
    std::vector<uint32> FindPlayersForRole(GroupRole role, const std::vector<Player*>& candidates) override;

    // Dynamic role adjustment
    void HandleRoleConflict(Group* group, GroupRole conflictedRole) override;
    void RebalanceRoles(Group* group) override;
    void AdaptToGroupChanges(Group* group, Player* newMember = nullptr, Player* leavingMember = nullptr) override;
    bool CanPlayerSwitchRole(GroupRole newRole, Group* group) override;

    // Content-specific role optimization
    void OptimizeForDungeon(Group* group, uint32 dungeonId) override;
    void OptimizeForRaid(Group* group, uint32 raidId) override;
    void OptimizeForPvP(Group* group, uint32 battlegroundId) override;
    void OptimizeForQuesting(Group* group, uint32 questId) override;

    // Role preferences and constraints
    void SetPlayerRolePreference(GroupRole preferredRole) override;
    GroupRole GetPlayerRolePreference() override;
    void SetRoleFlexibility(bool isFlexible) override;
    void AddRoleConstraint(GroupRole role, RoleCapability capability) override;

    // Role performance tracking
    struct RolePerformance
    {
        std::atomic<uint32> assignmentsAccepted{0};
        std::atomic<uint32> assignmentsDeclined{0};
        std::atomic<float> performanceRating{5.0f};
        std::atomic<uint32> successfulEncounters{0};
        std::atomic<uint32> failedEncounters{0};
        std::atomic<float> averageEffectiveness{0.5f};
        std::chrono::steady_clock::time_point lastPerformanceUpdate;

        void Reset() {
            assignmentsAccepted = 0; assignmentsDeclined = 0; performanceRating = 5.0f;
            successfulEncounters = 0; failedEncounters = 0; averageEffectiveness = 0.5f;
            lastPerformanceUpdate = std::chrono::steady_clock::now();
        }

        float GetAcceptanceRate() const {
            uint32 total = assignmentsAccepted.load() + assignmentsDeclined.load();
            return total > 0 ? (float)assignmentsAccepted.load() / total : 1.0f;
        }

        float GetSuccessRate() const {
            uint32 total = successfulEncounters.load() + failedEncounters.load();
            return total > 0 ? (float)successfulEncounters.load() / total : 0.5f;
        }
    };

    RolePerformance GetPlayerRolePerformance(GroupRole role) override;
    void UpdateRolePerformance(GroupRole role, bool wasSuccessful, float effectiveness) override;

    // Role assignment validation
    bool ValidateRoleAssignment(Group* group) override;
    std::vector<std::string> GetRoleAssignmentIssues(Group* group) override;
    bool CanGroupFunction(Group* group) override;

    // Emergency role filling
    bool FillEmergencyRole(Group* group, GroupRole urgentRole) override;
    std::vector<uint32> FindEmergencyReplacements(GroupRole role, uint32 minLevel, uint32 maxLevel) override;
    void HandleRoleEmergency(Group* group, uint32 disconnectedPlayerGuid) override;

    // Role statistics and monitoring
    struct RoleStatistics
    {
        std::atomic<uint32> totalAssignments{0};
        std::atomic<uint32> successfulAssignments{0};
        std::atomic<uint32> roleConflicts{0};
        std::atomic<uint32> emergencyFills{0};
        std::atomic<float> averageCompositionScore{5.0f};
        std::atomic<float> roleDistributionEfficiency{0.8f};
        std::chrono::steady_clock::time_point lastStatsUpdate;

        void Reset() {
            totalAssignments = 0; successfulAssignments = 0; roleConflicts = 0;
            emergencyFills = 0; averageCompositionScore = 5.0f; roleDistributionEfficiency = 0.8f;
            lastStatsUpdate = std::chrono::steady_clock::now();
        }

        float GetSuccessRate() const {
            uint32 total = totalAssignments.load();
            uint32 successful = successfulAssignments.load();
            return total > 0 ? (float)successful / total : 0.0f;
        }
    };

    RoleStatistics GetGlobalRoleStatistics() override;
    void UpdateRoleStatistics() override;

    // Configuration and settings
    void SetRoleAssignmentStrategy(Group* group, RoleAssignmentStrategy strategy) override;
    void SetContentTypeRequirements(uint32 contentId, const std::unordered_map<GroupRole, uint32>& requirements) override;
    void EnableAutoRoleAssignment(bool enable) { _autoAssignmentEnabled = enable; }

    // Update and maintenance
    void Update(uint32 diff) override;
    void RefreshPlayerProfiles() override;
    void CleanupInactiveProfiles() override;

private:
    Player* _bot;
    ~RoleAssignment() = default;

    // Core data storage
    std::unordered_map<uint32, PlayerRoleProfile> _playerProfiles; // playerGuid -> profile
    std::unordered_map<uint32, GroupComposition> _groupCompositions; // groupId -> composition
    std::unordered_map<uint32, RoleAssignmentStrategy> _groupStrategies; // groupId -> strategy
    

    // Role performance tracking
    std::unordered_map<uint32, std::unordered_map<GroupRole, RolePerformance>> _rolePerformance; // playerGuid -> role -> performance
    

    // Content-specific requirements
    std::unordered_map<uint32, std::unordered_map<GroupRole, uint32>> _contentRequirements; // contentId -> role requirements
    std::unordered_map<uint32, GroupComposition> _contentOptimalCompositions; // contentId -> optimal composition

    // Global settings
    std::atomic<bool> _autoAssignmentEnabled{true};
    RoleStatistics _globalStatistics;

    // Class and specialization role mappings
    std::unordered_map<uint8, std::unordered_map<uint8, std::vector<std::pair<GroupRole, RoleCapability>>>> _classSpecRoles;

    // Helper functions
    void InitializeClassRoleMappings();
    void BuildPlayerProfile(PlayerRoleProfile& profile);
    void CalculateRoleCapabilities(PlayerRoleProfile& profile);
    void AnalyzePlayerGear(PlayerRoleProfile& profile);
    void UpdateRoleExperience(PlayerRoleProfile& profile);
    GroupRole DetermineOptimalRole(Group* group, RoleAssignmentStrategy strategy);
    float CalculateCompositionScore(const GroupComposition& composition);
    bool HasRoleConflict(Group* group, GroupRole role);
    void ResolveRoleConflict(Group* group, GroupRole role);
    std::vector<uint32> GetAlternativePlayers(GroupRole role, Group* group);
    void NotifyRoleAssignment(GroupRole role, Group* group);
    void HandleRoleAssignmentFailure(Group* group, const std::string& reason);

    // Scoring algorithms
    float CalculateClassRoleEffectiveness(uint8 playerClass, uint8 playerSpec, GroupRole role);
    float CalculateGearScore(GroupRole role);
    float CalculateExperienceScore(GroupRole role);
    float CalculateSynergyScore(GroupRole role, Group* group);
    float CalculateFlexibilityBonus(Group* group);

    // Assignment strategies
    void ExecuteOptimalStrategy(Group* group);
    void ExecuteBalancedStrategy(Group* group);
    void ExecuteFlexibleStrategy(Group* group);
    void ExecuteStrictStrategy(Group* group);
    void ExecuteHybridStrategy(Group* group);
    void ExecuteDungeonStrategy(Group* group);
    void ExecuteRaidStrategy(Group* group);
    void ExecutePvPStrategy(Group* group);

    // Constants
    static constexpr float MIN_ROLE_EFFECTIVENESS = 0.3f;
    static constexpr float ROLE_SWITCH_COOLDOWN = 30000.0f; // 30 seconds
    static constexpr uint32 PROFILE_UPDATE_INTERVAL = 60000; // 1 minute
    static constexpr float COMPOSITION_SCORE_THRESHOLD = 6.0f;
    static constexpr uint32 MAX_ROLE_ASSIGNMENT_ATTEMPTS = 5;
    static constexpr float HYBRID_CLASS_BONUS = 0.1f;
    static constexpr float EXPERIENCE_WEIGHT = 0.15f;
    static constexpr float GEAR_WEIGHT = 0.25f;
    static constexpr float EFFECTIVENESS_WEIGHT = 0.4f;
    static constexpr float SYNERGY_WEIGHT = 0.1f;
    static constexpr float AVAILABILITY_WEIGHT = 0.1f;
};

} // namespace Playerbot