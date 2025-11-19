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
#include "QuestDef.h"
#include "Creature.h"
#include "GameObject.h"
#include "Position.h"
#include "../Core/DI/Interfaces/IQuestCompletion.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <atomic>
#include <mutex>
#include <chrono>
#include "GameTime.h"

class Player;
class Quest;
class Creature;
class GameObject;
class Group;
class Unit;

namespace Playerbot
{

enum class QuestObjectiveType : uint8
{
    KILL_CREATURE       = 0,
    COLLECT_ITEM        = 1,
    TALK_TO_NPC         = 2,
    REACH_LOCATION      = 3,
    USE_GAMEOBJECT      = 4,
    CAST_SPELL          = 5,
    EMOTE_AT_TARGET     = 6,
    ESCORT_NPC          = 7,
    DEFEND_AREA         = 8,
    SURVIVE_TIME        = 9,
    WIN_BATTLEGROUND    = 10,
    COMPLETE_DUNGEON    = 11,
    GAIN_EXPERIENCE     = 12,
    LEARN_SPELL         = 13,
    CUSTOM_OBJECTIVE    = 14
};

enum class QuestCompletionStrategy : uint8
{
    EFFICIENT_COMPLETION    = 0,  // Complete objectives most efficiently
    SAFE_COMPLETION        = 1,  // Prioritize safety over speed
    GROUP_COORDINATION     = 2,  // Coordinate with group members
    SOLO_FOCUS            = 3,  // Complete independently
    EXPERIENCE_MAXIMIZING  = 4,  // Maximize experience gain
    SPEED_COMPLETION      = 5,  // Complete as fast as possible
    THOROUGH_EXPLORATION  = 6   // Explore thoroughly while completing
};

enum class ObjectiveStatus : uint8
{
    NOT_STARTED    = 0,
    IN_PROGRESS    = 1,
    COMPLETED      = 2,
    FAILED         = 3,
    BLOCKED        = 4,
    SKIPPED        = 5
};

struct QuestObjectiveData
{
    uint32 questId;
    uint32 objectiveIndex;
    QuestObjectiveType type;
    uint32 targetId;           // Creature ID, Item ID, etc.
    uint32 requiredCount;
    uint32 currentCount;
    Position targetLocation;
    float searchRadius;
    ObjectiveStatus status;
    uint32 lastUpdateTime;
    uint32 timeSpent;
    uint32 retryCount;
    bool isOptional;
    bool requiresGroup;
    std::string description;
    std::vector<uint32> alternativeTargets;

    QuestObjectiveData(uint32 qId, uint32 index, QuestObjectiveType t, uint32 tId, uint32 required)
        : questId(qId), objectiveIndex(index), type(t), targetId(tId), requiredCount(required)
        , currentCount(0), searchRadius(50.0f), status(ObjectiveStatus::NOT_STARTED)
        , lastUpdateTime(GameTime::GetGameTimeMS()), timeSpent(0), retryCount(0)
        , isOptional(false), requiresGroup(false) {}
};

struct QuestProgressData
{
    uint32 questId;
    uint32 botGuid;
    std::vector<QuestObjectiveData> objectives;
    QuestCompletionStrategy strategy;
    uint32 startTime;
    uint32 lastUpdateTime;
    uint32 estimatedCompletionTime;
    float completionPercentage;
    bool isStuck;
    uint32 stuckTime;
    uint32 consecutiveFailures;
    Position lastKnownLocation;
    std::vector<std::string> completionLog;
    bool requiresTurnIn;
    uint32 questGiverGuid;
    Position questGiverLocation;

    QuestProgressData(uint32 qId, uint32 bGuid) : questId(qId), botGuid(bGuid)
        , strategy(QuestCompletionStrategy::EFFICIENT_COMPLETION), startTime(GameTime::GetGameTimeMS())
        , lastUpdateTime(GameTime::GetGameTimeMS()), estimatedCompletionTime(1200000) // 20 minutes
        , completionPercentage(0.0f), isStuck(false), stuckTime(0)
        , consecutiveFailures(0), requiresTurnIn(true), questGiverGuid(0) {}
};

class TC_GAME_API QuestCompletion final : public IQuestCompletion
{
public:
    explicit QuestCompletion(Player* bot);
    ~QuestCompletion();
    QuestCompletion(QuestCompletion const&) = delete;
    QuestCompletion& operator=(QuestCompletion const&) = delete;

    // Core quest completion management
    bool StartQuestCompletion(uint32 questId, Player* bot) override;
    void UpdateQuestProgress(Player* bot) override;
    void CompleteQuest(uint32 questId, Player* bot) override;
    bool TurnInQuest(uint32 questId, Player* bot) override;

    // Objective tracking and execution
    void TrackQuestObjectives(Player* bot) override;
    void ExecuteObjective(Player* bot, QuestObjectiveData& objective) override;
    void UpdateObjectiveProgress(Player* bot, uint32 questId, uint32 objectiveIndex) override;
    bool IsObjectiveComplete(const QuestObjectiveData& objective) override;

    // Objective-specific handlers
    void HandleKillObjective(Player* bot, QuestObjectiveData& objective) override;
    void HandleCollectObjective(Player* bot, QuestObjectiveData& objective) override;
    void HandleTalkToNpcObjective(Player* bot, QuestObjectiveData& objective) override;
    void HandleLocationObjective(Player* bot, QuestObjectiveData& objective) override;
    void HandleGameObjectObjective(Player* bot, QuestObjectiveData& objective) override;
    void HandleSpellCastObjective(Player* bot, QuestObjectiveData& objective) override;
    void HandleEmoteObjective(Player* bot, QuestObjectiveData& objective) override;
    void HandleEscortObjective(Player* bot, QuestObjectiveData& objective) override;

    // Navigation and pathfinding
    void NavigateToObjective(Player* bot, const QuestObjectiveData& objective) override;
    bool FindObjectiveTarget(Player* bot, QuestObjectiveData& objective) override;
    std::vector<Position> GetObjectiveLocations(const QuestObjectiveData& objective) override;
    Position GetOptimalObjectivePosition(Player* bot, const QuestObjectiveData& objective) override;

    // Group coordination for quest completion
    void CoordinateGroupQuestCompletion(Group* group, uint32 questId) override;
    void ShareObjectiveProgress(Group* group, uint32 questId) override;
    void SynchronizeGroupObjectives(Group* group, uint32 questId) override;
    void HandleGroupObjectiveConflict(Group* group, uint32 questId, uint32 objectiveIndex) override;

    // Quest completion optimization
    void OptimizeQuestCompletionOrder(Player* bot) override;
    void OptimizeObjectiveSequence(Player* bot, uint32 questId) override;
    void FindEfficientCompletionPath(Player* bot, const std::vector<uint32>& questIds) override;
    void MinimizeTravelTime(Player* bot, const std::vector<QuestObjectiveData>& objectives) override;

    // Stuck detection and recovery
    void DetectStuckState(Player* bot, uint32 questId) override;
    void HandleStuckObjective(Player* bot, QuestObjectiveData& objective) override;
    void RecoverFromStuckState(Player* bot, uint32 questId) override;
    void SkipProblematicObjective(Player* bot, QuestObjectiveData& objective) override;

    // Quest turn-in management
    void ProcessQuestTurnIn(Player* bot, uint32 questId) override;
    bool FindQuestTurnInNpc(Player* bot, uint32 questId) override;
    void HandleQuestRewardSelection(Player* bot, uint32 questId) override;
    void CompleteQuestDialog(Player* bot, uint32 questId) override;

    // Performance monitoring
    struct QuestCompletionMetrics
    {
        std::atomic<uint32> questsStarted{0};
        std::atomic<uint32> questsCompleted{0};
        std::atomic<uint32> questsFailed{0};
        std::atomic<uint32> objectivesCompleted{0};
        std::atomic<uint32> stuckInstances{0};
        std::atomic<float> averageCompletionTime{1200000.0f}; // 20 minutes
        std::atomic<float> completionSuccessRate{0.85f};
        std::atomic<float> objectiveEfficiency{0.9f};
        std::atomic<uint32> totalDistanceTraveled{0};
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            questsStarted = 0; questsCompleted = 0; questsFailed = 0;
            objectivesCompleted = 0; stuckInstances = 0; averageCompletionTime = 1200000.0f;
            completionSuccessRate = 0.85f; objectiveEfficiency = 0.9f;
            totalDistanceTraveled = 0;
            lastUpdate = std::chrono::steady_clock::now();
        }

        float GetCompletionRate() const {
            uint32 started = questsStarted.load();
            uint32 completed = questsCompleted.load();
            return started > 0 ? (float)completed / started : 0.0f;
        }

        // Delete copy operations (atomics are not copyable)
        QuestCompletionMetrics() = default;
        QuestCompletionMetrics(QuestCompletionMetrics const&) = delete;
        QuestCompletionMetrics& operator=(QuestCompletionMetrics const&) = delete;

        // Snapshot structure for returning metrics
        struct Snapshot {
            uint32 questsStarted;
            uint32 questsCompleted;
            uint32 questsFailed;
            uint32 objectivesCompleted;
            uint32 stuckInstances;
            float averageCompletionTime;
            float completionSuccessRate;
            float objectiveEfficiency;
            uint32 totalDistanceTraveled;
            std::chrono::steady_clock::time_point lastUpdate;

            float GetCompletionRate() const {
                return questsStarted > 0 ? (float)questsCompleted / questsStarted : 0.0f;
            }
        };

        // Create a snapshot of current metrics
        Snapshot CreateSnapshot() const {
            Snapshot snapshot;
            snapshot.questsStarted = questsStarted.load();
            snapshot.questsCompleted = questsCompleted.load();
            snapshot.questsFailed = questsFailed.load();
            snapshot.objectivesCompleted = objectivesCompleted.load();
            snapshot.stuckInstances = stuckInstances.load();
            snapshot.averageCompletionTime = averageCompletionTime.load();
            snapshot.completionSuccessRate = completionSuccessRate.load();
            snapshot.objectiveEfficiency = objectiveEfficiency.load();
            snapshot.totalDistanceTraveled = totalDistanceTraveled.load();
            snapshot.lastUpdate = lastUpdate;
            return snapshot;
        }
    };

    QuestCompletionMetrics::Snapshot GetBotCompletionMetrics(uint32 botGuid) override;
    QuestCompletionMetrics::Snapshot GetGlobalCompletionMetrics() override;

    // Quest data analysis
    std::vector<uint32> GetActiveQuests(Player* bot) override;
    std::vector<uint32> GetCompletableQuests(Player* bot) override;
    uint32 GetHighestPriorityQuest(Player* bot) override;
    float CalculateQuestProgress(uint32 questId, Player* bot) override;

    // Configuration and settings
    void SetQuestCompletionStrategy(uint32 botGuid, QuestCompletionStrategy strategy) override;
    QuestCompletionStrategy GetQuestCompletionStrategy(uint32 botGuid) override;
    void SetMaxConcurrentQuests(uint32 botGuid, uint32 maxQuests) override;
    void EnableGroupCoordination(uint32 botGuid, bool enable) override;

    // Advanced quest completion features
    void HandleDungeonQuests(Player* bot, uint32 dungeonId) override;
    void HandlePvPQuests(Player* bot, uint32 battlegroundId) override;
    void HandleSeasonalQuests(Player* bot) override;
    void HandleDailyQuests(Player* bot) override;

    // Error handling and recovery
    void HandleQuestCompletionError(Player* bot, uint32 questId, const std::string& error) override;
    void RecoverFromCompletionFailure(Player* bot, uint32 questId) override;
    void AbandonUncompletableQuest(Player* bot, uint32 questId) override;
    void DiagnoseCompletionIssues(Player* bot, uint32 questId) override;

    // Update and maintenance
    void Update(uint32 diff) override;
    void UpdateBotQuestCompletion(Player* bot, uint32 diff) override;
    void CleanupCompletedQuests() override;
    void ValidateQuestStates() override;

private:
    Player* _bot;
    ~QuestCompletion() = default;

    // Core data structures
    std::unordered_map<uint32, std::vector<QuestProgressData>> _botQuestProgress; // botGuid -> quests
    std::unordered_map<uint32, QuestCompletionStrategy> _botStrategies;
    std::unordered_map<uint32, QuestCompletionMetrics> _botMetrics;
    

    // Objective execution state
    std::unordered_map<uint32, uint32> _botCurrentObjective; // botGuid -> objectiveIndex
    std::unordered_map<uint32, uint32> _botLastObjectiveUpdate; // botGuid -> timestamp
    std::unordered_set<uint32> _botsInQuestMode; // bots actively completing quests

    // Group coordination data
    std::unordered_map<uint32, std::vector<uint32>> _groupQuestSharing; // groupId -> questIds
    std::unordered_map<uint32, std::unordered_map<uint32, uint32>> _groupObjectiveSync; // groupId -> questId -> syncTime
    

    // Performance tracking
    QuestCompletionMetrics _globalMetrics;

    // Helper functions
    void InitializeQuestProgress(Player* bot, uint32 questId);
    void ParseQuestObjectives(QuestProgressData& progress, const Quest* quest);
    void UpdateQuestObjectiveFromProgress(QuestObjectiveData& objective, Player* bot);
    bool CanExecuteObjective(Player* bot, const QuestObjectiveData& objective);
    void MoveToObjectiveTarget(Player* bot, const QuestObjectiveData& objective);
    bool IsInObjectiveRange(Player* bot, const QuestObjectiveData& objective);

    // Objective-specific implementations
    bool FindKillTarget(Player* bot, QuestObjectiveData& objective);
    bool FindCollectibleItem(Player* bot, QuestObjectiveData& objective);
    bool FindNpcTarget(Player* bot, QuestObjectiveData& objective);
    bool FindGameObjectTarget(Player* bot, QuestObjectiveData& objective);
    void InteractWithTarget(Player* bot, const QuestObjectiveData& objective, uint32 targetGuid);

    // Combat integration
    void HandleQuestCombat(Player* bot, const QuestObjectiveData& objective, Unit* target);
    void PrioritizeQuestTargets(Player* bot, const std::vector<Unit*>& enemies);
    bool ShouldEngageQuestTarget(Player* bot, Unit* target, const QuestObjectiveData& objective);

    // Pathfinding and navigation
    std::vector<Position> GenerateObjectivePath(Player* bot, const QuestObjectiveData& objective);
    Position FindNearestObjectiveLocation(Player* bot, const QuestObjectiveData& objective);
    bool ValidateObjectivePosition(const Position& position, const QuestObjectiveData& objective);

    // Group coordination helpers
    void BroadcastObjectiveProgress(Group* group, uint32 questId, uint32 objectiveIndex, Player* bot);
    void SynchronizeObjectiveStates(Group* group, uint32 questId);
    void CoordinateObjectiveExecution(Group* group, uint32 questId, uint32 objectiveIndex);

    // Strategy implementations
    void ExecuteEfficientStrategy(Player* bot, QuestProgressData& progress);
    void ExecuteSafeStrategy(Player* bot, QuestProgressData& progress);
    void ExecuteGroupStrategy(Player* bot, QuestProgressData& progress);
    void ExecuteSoloStrategy(Player* bot, QuestProgressData& progress);
    void ExecuteExperienceStrategy(Player* bot, QuestProgressData& progress);
    void ExecuteSpeedStrategy(Player* bot, QuestProgressData& progress);
    void ExecuteExplorationStrategy(Player* bot, QuestProgressData& progress);

    // Performance optimization
    void OptimizeQuestCompletionPerformance(Player* bot);
    void CacheObjectiveTargets(Player* bot);
    void PreloadQuestData(Player* bot);
    void BatchObjectiveUpdates();

    // Error handling
    void LogCompletionError(Player* bot, uint32 questId, const std::string& error);
    void HandleObjectiveTimeout(Player* bot, QuestObjectiveData& objective);
    void RecoverFromObjectiveFailure(Player* bot, QuestObjectiveData& objective);

    // Constants
    static constexpr uint32 OBJECTIVE_UPDATE_INTERVAL = 2000; // 2 seconds
    static constexpr uint32 STUCK_DETECTION_TIME = 60000; // 1 minute
    static constexpr uint32 MAX_OBJECTIVE_RETRIES = 5;
    static constexpr float OBJECTIVE_COMPLETION_RADIUS = 10.0f;
    static constexpr uint32 QUEST_COMPLETION_TIMEOUT = 3600000; // 1 hour
    static constexpr uint32 GROUP_SYNC_INTERVAL = 5000; // 5 seconds
    static constexpr float MIN_PROGRESS_THRESHOLD = 0.01f; // 1% progress
    static constexpr uint32 MAX_CONCURRENT_OBJECTIVES = 3;
    static constexpr uint32 OBJECTIVE_SEARCH_RADIUS = 100; // 100 yards
    static constexpr uint32 TARGET_INTERACTION_TIMEOUT = 10000; // 10 seconds
};

} // namespace Playerbot