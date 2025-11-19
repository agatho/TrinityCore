/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * REFACTORED BotAI - Clean Update Chain Architecture
 *
 * This refactored version provides:
 * 1. Single, clean update path without DoUpdateAI/UpdateEnhanced confusion
 * 2. Clear separation between base behaviors and combat specialization
 * 3. No throttling that breaks movement/following
 * 4. Proper virtual method hierarchy for class-specific overrides
 */

#pragma once

#include "Define.h"
#include "Threading/LockHierarchy.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "Actions/Action.h"
#include "Triggers/Trigger.h"
#include "Strategy/Strategy.h"
#include "Core/DI/Interfaces/IBotAIFactory.h"
#include "ObjectCache.h"
#include "Blackboard/SharedBlackboard.h"
#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <unordered_map>
#include <chrono>
#include <queue>
#include <shared_mutex>

namespace Playerbot
{

// Forward declarations
class BehaviorPriorityManager;
enum class BehaviorPriority : uint8_t;
class Value;
class GroupInvitationHandler;
class TargetScanner;
class QuestManager;
class TradeManager;
class GatheringManager;
class AuctionManager;
class GroupCoordinator; // Advanced/GroupCoordinator
class DeathRecoveryManager;
class MovementArbiter;
class CombatStateManager;
enum class PlayerBotMovementPriority : uint8;

// Phase 3: Tactical Coordination forward declarations
namespace Coordination
{
    class GroupCoordinator;
}

// Phase 4: Event structure forward declarations
struct GroupEvent;
struct CombatEvent;
struct CooldownEvent;
struct AuraEvent;
struct LootEvent;
struct QuestEvent;
struct ResourceEvent;
struct SocialEvent;
struct AuctionEvent;
struct NPCEvent;
struct InstanceEvent;

namespace Events
{
    class CombatEventObserver;
    class AuraEventObserver;
    class ResourceEventObserver;
    class QuestEventObserver;
    class MovementEventObserver;
    class TradeEventObserver;
    class SocialEventObserver;
    class InstanceEventObserver;
    class PvPEventObserver;
    class EventDispatcher;
}

class ManagerRegistry;

// Phase 5E: Decision Fusion System forward declarations
namespace bot { namespace ai {
    class DecisionFusionSystem;
    class ActionPriorityQueue;
    class BehaviorTree;
}}

// TriggerResult comparator for priority queue
struct TriggerResultComparator
{
    bool operator()(TriggerResult const& a, TriggerResult const& b) const;
};

// Enhanced AI state enum
enum class BotAIState
{
    SOLO,           // Solo play mode (questing, gathering, autonomous combat)
    COMBAT,
    DEAD,
    TRAVELLING,
    QUESTING,
    GATHERING,
    TRADING,
    FOLLOWING,
    FLEEING,
    RESTING
};

// AI update result for performance tracking
struct AIUpdateResult
{
    uint32 actionsExecuted = 0;
    uint32 triggersChecked = 0;
    uint32 strategiesEvaluated = 0;
    std::chrono::microseconds updateTime{0};
};

class TC_GAME_API BotAI
{
public:
    explicit BotAI(Player* bot);
    virtual ~BotAI();

    // ========================================================================
    // CLEAN UPDATE INTERFACE - Single entry point, no confusion
    // ========================================================================

    /**
     * Main update method - SINGLE ENTRY POINT for all AI updates
     * This is the ONLY method called by BotSession::Update()
     *
     * Update flow:
     * 1. Update core behaviors (strategies, movement, idle)
     * 2. Check combat state transitions
     * 3. If in combat AND derived class exists, call OnCombatUpdate()
     *
     * CRITICAL: This method is NOT throttled and runs every frame
     * to ensure smooth movement and responsive behavior
     */
    virtual void UpdateAI(uint32 diff);

    /**
     * Virtual method for class-specific COMBAT ONLY updates
     * Called by UpdateAI() when bot is in combat
     *
     * ClassAI implementations should override this for:
     * - Combat rotations
     * - Target selection
     * - Cooldown management
     * - Resource management
     *
     * MUST NOT:
     * - Control movement (handled by strategies)
     * - Throttle updates (causes following issues)
     * - Call base UpdateAI (would cause recursion)
     */
    virtual void OnCombatUpdate(uint32 diff) {}

    // ========================================================================
    // STATE TRANSITIONS - Clean lifecycle management
    // ========================================================================

    virtual void Reset();
    virtual void OnDeath();
    virtual void OnRespawn();
    virtual void OnCombatStart(::Unit* target);
    virtual void OnCombatEnd();

    // ========================================================================
    // STRATEGY MANAGEMENT - Core behavior system
    // ========================================================================

    void AddStrategy(std::unique_ptr<Strategy> strategy);
    void RemoveStrategy(std::string const& name);
    Strategy* GetStrategy(std::string const& name) const;
    std::vector<Strategy*> GetActiveStrategies() const;
    void ActivateStrategy(std::string const& name);
    void DeactivateStrategy(std::string const& name);

    // Priority-based strategy selection
    BehaviorPriorityManager* GetPriorityManager() { return _priorityManager.get(); }
    BehaviorPriorityManager const* GetPriorityManager() const { return _priorityManager.get(); }

    // ========================================================================
    // ACTION EXECUTION - Command pattern implementation
    // ========================================================================

    bool ExecuteAction(std::string const& actionName);
    bool ExecuteAction(std::string const& name, ActionContext const& context);
    bool IsActionPossible(std::string const& actionName) const;
    uint32 GetActionPriority(std::string const& actionName) const;

    void QueueAction(std::shared_ptr<Action> action, ActionContext const& context = {});
    void CancelCurrentAction();
    bool IsActionInProgress() const { return _currentAction != nullptr; }

    // ========================================================================
    // STATE MANAGEMENT - AI state tracking
    // ========================================================================

    BotAIState GetAIState() const { return _aiState; }
    void SetAIState(BotAIState state);
    bool IsInCombat() const { return _aiState == BotAIState::COMBAT; }
    bool IsSolo() const { return _aiState == BotAIState::SOLO; }
    bool IsFollowing() const { return _aiState == BotAIState::FOLLOWING; }

    // ========================================================================
    // BOT ACCESS - Core entity access
    // ========================================================================

    Player* GetBot() const { return _bot; }
    ObjectGuid GetBotGuid() const { return _bot ? _bot->GetGUID() : ObjectGuid::Empty; }

    // ========================================================================
    // GROUP MANAGEMENT - Social behavior
    // ========================================================================

    void OnGroupJoined(Group* group);
    void OnGroupLeft();
    void HandleGroupChange();
    GroupInvitationHandler* GetGroupInvitationHandler() { return _groupInvitationHandler.get(); }
    GroupInvitationHandler const* GetGroupInvitationHandler() const { return _groupInvitationHandler.get(); }

    // ========================================================================
    // TARGET MANAGEMENT - Combat targeting
    // ========================================================================

    void SetTarget(ObjectGuid guid) { _currentTarget = guid; }
    ObjectGuid GetTarget() const { return _currentTarget; }
    ::Unit* GetTargetUnit() const;

    // ========================================================================
    // MOVEMENT CONTROL - Strategy-driven movement
    // ========================================================================

    void MoveTo(float x, float y, float z);
    void Follow(::Unit* target, float distance = 5.0f);
    void StopMovement();
    bool IsMoving() const;

    // ========================================================================
    // GAME SYSTEM MANAGERS - Quest, profession, trade management
    // ========================================================================

    QuestManager* GetQuestManager() { return _questManager.get(); }
    QuestManager const* GetQuestManager() const { return _questManager.get(); }

    TradeManager* GetTradeManager() { return _tradeManager.get(); }
    TradeManager const* GetTradeManager() const { return _tradeManager.get(); }

    GatheringManager* GetGatheringManager() { return _gatheringManager.get(); }
    GatheringManager const* GetGatheringManager() const { return _gatheringManager.get(); }

    AuctionManager* GetAuctionManager() { return _auctionManager.get(); }
    AuctionManager const* GetAuctionManager() const { return _auctionManager.get(); }

    GroupCoordinator* GetGroupCoordinator() { return _groupCoordinator.get(); }
    GroupCoordinator const* GetGroupCoordinator() const { return _groupCoordinator.get(); }

    /**
     * @brief Get Tactical Group Coordinator (Phase 3)
     * Combat-focused coordination separate from Advanced/GroupCoordinator
     * @return Pointer to tactical coordinator, or nullptr if not in group
     */
    Coordination::GroupCoordinator* GetTacticalCoordinator() { return _tacticalCoordinator.get(); }
    Coordination::GroupCoordinator const* GetTacticalCoordinator() const { return _tacticalCoordinator.get(); }

    // ========================================================================
    // UTILITY AI DECISION SYSTEM - Hybrid AI Phase 1
    // ========================================================================

    /**
     * @brief Initialize Hybrid AI Decision System (Phase 2 Week 3)
     * Creates HybridAIController with Utility AI + Behavior Trees
     * Called during BotAI constructor
     */
    void InitializeHybridAI();

    /**
     * @brief Update Hybrid AI (Phase 2 Week 3)
     * Selects best behavior via Utility AI, executes via Behavior Tree
     * @param diff Time since last UpdateAI() call
     */
    void UpdateHybridAI(uint32 diff);

    /**
     * @brief Get Hybrid AI Controller (Phase 2 Week 3)
     * @return Pointer to controller, or nullptr if not initialized
     */
    class HybridAIController* GetHybridAI() const { return _hybridAI.get(); }

    /**
     * @brief Get Shared Blackboard (Phase 4)
     * @return Pointer to blackboard, or nullptr if not initialized
     */
    SharedBlackboard* GetSharedBlackboard() const { return _sharedBlackboard; }

    // ========================================================================
    // DEATH RECOVERY - Resurrection management
    // ========================================================================

    DeathRecoveryManager* GetDeathRecoveryManager() { return _deathRecoveryManager.get(); }
    DeathRecoveryManager const* GetDeathRecoveryManager() const { return _deathRecoveryManager.get(); }

    // ========================================================================
    // MOVEMENT ARBITER - Enterprise movement request arbitration
    // ========================================================================

    MovementArbiter* GetMovementArbiter() { return _movementArbiter.get(); }
    MovementArbiter const* GetMovementArbiter() const { return _movementArbiter.get(); }

    /**
     * Request movement via the Movement Arbiter
     * This is the PREFERRED method for all movement requests
     *
     * @param request MovementRequest to submit for arbitration
     * @return true if request accepted, false if filtered (duplicate/low priority)
     *
     * Example:
     *   auto req = MovementRequest::MakePointMovement(
     *       PlayerBotMovementPriority::COMBAT_AI,
     *       targetPos, true, {}, {}, {},
     *       "Combat positioning", "WarriorAI");
     *   bot->GetBotAI()->RequestMovement(req);
     */
    bool RequestMovement(class MovementRequest const& request);

    /**
     * Convenience method: Request point movement
     *
     * @param priority Movement priority
     * @param position Target position
     * @param reason Debug description
     * @param sourceSystem Source system identifier
     * @return true if accepted
     */
    bool RequestPointMovement(
        PlayerBotMovementPriority priority,
        Position const& position,
        std::string const& reason = "",
        std::string const& sourceSystem = "");

    /**
     * Convenience method: Request chase movement
     *
     * @param priority Movement priority
     * @param target Target to chase
     * @param reason Debug description
     * @param sourceSystem Source system identifier
     * @return true if accepted
     */
    bool RequestChaseMovement(
        PlayerBotMovementPriority priority,
        ObjectGuid targetGuid,
        std::string const& reason = "",
        std::string const& sourceSystem = "");

    /**
     * Convenience method: Request follow movement
     *
     * @param priority Movement priority
     * @param target Target to follow
     * @param distance Follow distance
     * @param reason Debug description
     * @param sourceSystem Source system identifier
     * @return true if accepted
     */
    bool RequestFollowMovement(
        PlayerBotMovementPriority priority,
        ObjectGuid targetGuid,
        float distance = 5.0f,
        std::string const& reason = "",
        std::string const& sourceSystem = "");

    /**
     * Stop all movement immediately
     * Clears pending requests and stops current movement
     */
    void StopAllMovement();

    // ========================================================================
    // PHASE 7.1: EVENT DISPATCHER - Centralized event routing
    // ========================================================================

    Events::EventDispatcher* GetEventDispatcher() { return _eventDispatcher.get(); }
    Events::EventDispatcher const* GetEventDispatcher() const { return _eventDispatcher.get(); }

    // ========================================================================
    // PHASE 7.1: MANAGER REGISTRY - Manager lifecycle management
    // ========================================================================

    ManagerRegistry* GetManagerRegistry() { return _managerRegistry.get(); }
    ManagerRegistry const* GetManagerRegistry() const { return _managerRegistry.get(); }

    // ========================================================================
    // PHASE 5E: DECISION FUSION SYSTEM - Unified decision arbitration
    // ========================================================================

    /**
     * @brief Get Decision Fusion System for unified action selection
     *
     * The DecisionFusionSystem collects votes from all decision-making systems
     * (BehaviorPriorityManager, ActionScoringEngine, etc.) and fuses them
     * into a single, optimal action selection.
     *
     * @return Pointer to DecisionFusionSystem, or nullptr if not initialized
     *
     * Example usage:
     * @code
     * if (auto fusion = GetDecisionFusion())
     * {
     *     auto votes = fusion->CollectVotes(this, CombatContext::DUNGEON_BOSS);
     *     auto result = fusion->FuseDecisions(votes);
     *     if (result.IsValid())
     *         ExecuteAction(result.actionId, result.target);
     * }
     * @endcode
     */
    bot::ai::DecisionFusionSystem* GetDecisionFusion() { return _decisionFusion.get(); }
    bot::ai::DecisionFusionSystem const* GetDecisionFusion() const { return _decisionFusion.get(); }

    /**
     * @brief Get Action Priority Queue for spell priority management
     *
     * The ActionPriorityQueue manages spell priorities dynamically based on
     * cooldowns, resources, combat situations, and custom conditions.
     *
     * @return Pointer to ActionPriorityQueue, or nullptr if not initialized
     *
     * Example usage:
     * @code
     * if (auto queue = GetActionPriorityQueue())
     * {
     *     queue->RegisterSpell(FIREBALL, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
     *     queue->AddCondition(PYROBLAST, [](Player* bot, Unit*) {
     *         return bot->HasAura(HOT_STREAK);
     *     }, "Hot Streak proc");
     *
     *     uint32 bestSpell = queue->GetHighestPrioritySpell(GetBot(), target, context);
     *     if (bestSpell)
     *         CastSpell(target, bestSpell);
     * }
     * @endcode
     */
    bot::ai::ActionPriorityQueue* GetActionPriorityQueue() { return _actionPriorityQueue.get(); }
    bot::ai::ActionPriorityQueue const* GetActionPriorityQueue() const { return _actionPriorityQueue.get(); }

    /**
     * @brief Get Behavior Tree for hierarchical combat flow
     *
     * The BehaviorTree provides structured decision-making through hierarchical
     * node execution (Sequences, Selectors, Conditions, Actions).
     *
     * @return Pointer to BehaviorTree, or nullptr if not initialized
     *
     * Example usage:
     * @code
     * if (auto tree = GetBehaviorTree())
     * {
     *     // Execute one tick of the tree
     *     NodeStatus status = tree->Tick(GetBot(), target);
     *
     *     // Tree can span multiple ticks if RUNNING
     *     if (status == NodeStatus::RUNNING)
     *         return; // Continue next frame
     * }
     * @endcode
     */
    bot::ai::BehaviorTree* GetBehaviorTree() { return _behaviorTree.get(); }
    bot::ai::BehaviorTree const* GetBehaviorTree() const { return _behaviorTree.get(); }

    // ========================================================================
    // PHASE 4: EVENT HANDLERS - Event-driven behavior system
    // ========================================================================

    /**
     * Group event handler - Called when group-related events occur
     * Override in ClassAI for class-specific group coordination
     *
     * @param event Group event containing all event data
     *
     * Default implementation delegates to managers and handles common cases:
     * - Ready checks (auto-respond if configured)
     * - Raid target changes (update targeting)
     * - Leader changes (update following)
     */
    virtual void OnGroupEvent(GroupEvent const& event);

    /**
     * Combat event handler - Called when combat-related events occur
     * Override in ClassAI for combat-specific responses (interrupts, defensive CDs)
     *
     * @param event Combat event (spell casts, damage, interrupts)
     *
     * Default implementation:
     * - Detects interruptible casts
     * - Updates combat state
     * - Delegates to combat managers
     */
    virtual void OnCombatEvent(CombatEvent const& event);

    /**
     * Cooldown event handler - Called when spell/item cooldowns change
     * Override in ClassAI for ability rotation tracking
     *
     * @param event Cooldown event (start, clear, modify)
     */
    virtual void OnCooldownEvent(CooldownEvent const& event);

    /**
     * Aura event handler - Called when buffs/debuffs change
     * Override in ClassAI for buff management and dispel priorities
     *
     * @param event Aura event (applied, removed, updated)
     *
     * Default implementation:
     * - Tracks important debuffs for dispelling
     * - Monitors missing buffs for rebuffing
     */
    virtual void OnAuraEvent(AuraEvent const& event);

    /**
     * Loot event handler - Called when loot becomes available
     * Override in ClassAI for smart loot decisions
     *
     * @param event Loot event (window opened, roll started, item received)
     *
     * Default implementation:
     * - Auto-greed on non-upgrades
     * - Auto-need on class-appropriate gear
     * - Auto-pass on unusable items
     */
    virtual void OnLootEvent(LootEvent const& event);

    /**
     * Quest event handler - Called when quest state changes
     * Override in ClassAI for quest-specific behavior
     *
     * @param event Quest event (offered, completed, objective progress)
     *
     * Default implementation:
     * - Delegates to QuestManager
     * - Auto-accepts appropriate quests
     * - Auto-completes when ready
     */
    virtual void OnQuestEvent(QuestEvent const& event);

    /**
     * Resource event handler - Called when health/power changes
     * Override in ClassAI for healing/resource management
     *
     * @param event Resource event (health update, power update, target break)
     *
     * Default implementation:
     * - Detects low health allies for healing
     * - Monitors own resources for regeneration
     * - Updates healing priorities
     */
    virtual void OnResourceEvent(ResourceEvent const& event);

    /**
     * Social event handler - Called when social events occur
     * Override in ClassAI for chat responses and social behavior
     *
     * @param event Social event (chat, emotes, guild, trade)
     *
     * Default implementation:
     * - Processes whispers for commands
     * - Handles guild invites
     * - Responds to trade requests
     */
    virtual void OnSocialEvent(SocialEvent const& event);

    /**
     * Auction event handler - Called when auction house events occur
     * Override in ClassAI for AH participation strategy
     *
     * @param event Auction event (command result, list, bid, won, outbid)
     *
     * Default implementation:
     * - Delegates to AuctionManager
     * - Handles auction notifications
     */
    virtual void OnAuctionEvent(AuctionEvent const& event);

    /**
     * NPC event handler - Called when interacting with NPCs
     * Override in ClassAI for vendor/trainer automation
     *
     * @param event NPC event (gossip, vendor, trainer, bank)
     *
     * Default implementation:
     * - Auto-selects quest gossip options
     * - Handles vendor repairs
     * - Manages trainer interactions
     */
    virtual void OnNPCEvent(NPCEvent const& event);

    /**
     * Instance event handler - Called when instance/raid events occur
     * Override in ClassAI for dungeon/raid coordination
     *
     * @param event Instance event (reset, lockout, encounter, messages)
     *
     * Default implementation:
     * - Tracks instance lockouts
     * - Monitors boss progress
     * - Handles encounter mechanics
     */
    virtual void OnInstanceEvent(InstanceEvent const& event);

    // ========================================================================
    // PERFORMANCE METRICS - Monitoring and optimization
    // ========================================================================

    struct PerformanceMetrics
    {
        std::atomic<uint32> totalUpdates{0};
        std::atomic<uint32> actionsExecuted{0};
        std::atomic<uint32> triggersProcessed{0};
        std::atomic<uint32> strategiesEvaluated{0};
        std::chrono::microseconds averageUpdateTime{0};
        std::chrono::microseconds maxUpdateTime{0};
        std::chrono::steady_clock::time_point lastUpdate;
    };

    PerformanceMetrics const& GetPerformanceMetrics() const { return _performanceMetrics; }

protected:
    // ========================================================================
    // INTERNAL UPDATE METHODS - Called by UpdateAI()
    // ========================================================================

    /**
     * Update all active strategies
     * CRITICAL: Must run every frame for following behavior
     */
    void UpdateStrategies(uint32 diff);

    /**
     * Update movement based on active strategies
     * CRITICAL: Must run every frame for smooth movement
     */
    void UpdateMovement(uint32 diff);

    /**
     * Update solo behaviors (questing, gathering, trading, autonomous combat)
     * Only runs when bot is in solo play mode (not in group or following)
     */
    void UpdateSoloBehaviors(uint32 diff);

    /**
     * Check and handle combat state transitions
     */
    void UpdateCombatState(uint32 diff);

    /**
     * Process all registered triggers
     */
    void ProcessTriggers();

    /**
     * Execute queued and triggered actions
     */
    void UpdateActions(uint32 diff);

    /**
     * Update internal values and caches
     */
    void UpdateValues(uint32 diff);

    /**
     * Update all BehaviorManager-based managers
     */
    void UpdateManagers(uint32 diff);

    // ========================================================================
    // HELPER METHODS - Utilities for derived classes
    // ========================================================================

    Strategy* SelectBestStrategy();
    std::shared_ptr<Action> SelectNextAction();
    bool CanExecuteAction(Action* action) const;
    ActionResult ExecuteActionInternal(Action* action, ActionContext const& context);

    void EvaluateTrigger(Trigger* trigger);
    void HandleTriggeredAction(TriggerResult const& result);

    void InitializeDefaultStrategies();
    void UpdatePerformanceMetrics(uint32 updateTimeMs);
    void LogAIDecision(std::string const& action, float score) const;

    // ========================================================================
    // EVENT HANDLER HELPERS - Common event processing patterns
    // ========================================================================

    void ProcessGroupReadyCheck(GroupEvent const& event);
    void ProcessCombatInterrupt(CombatEvent const& event);
    void ProcessLowHealthAlert(ResourceEvent const& event);
    void ProcessLootRoll(LootEvent const& event);
    void ProcessQuestProgress(QuestEvent const& event);
    void ProcessAuraDispel(AuraEvent const& event);

    // Combat entry helper for neutral mob detection
    void EnterCombatWithTarget(::Unit* target);

    // Event bus subscription management
    void SubscribeToEventBuses();
    void UnsubscribeFromEventBuses();

protected:
    // Core components
    Player* _bot;
    BotAIState _aiState = BotAIState::SOLO;
    ObjectGuid _currentTarget;

    // Strategy system
    std::unordered_map<std::string, std::unique_ptr<Strategy>> _strategies;
    std::vector<std::string> _activeStrategies;
    std::unique_ptr<BehaviorPriorityManager> _priorityManager;

    // Hybrid AI Decision System: Utility AI + Behavior Trees (Phase 2 Week 3)
    std::unique_ptr<class HybridAIController> _hybridAI;

    // Shared Blackboard: Thread-safe shared state system (Phase 4)
    SharedBlackboard* _sharedBlackboard = nullptr;

    // Action system
    std::queue<std::pair<std::shared_ptr<Action>, ActionContext>> _actionQueue;
    std::shared_ptr<Action> _currentAction;
    ActionContext _currentContext;

    // Trigger system
    std::vector<std::shared_ptr<Trigger>> _triggers;
    std::priority_queue<TriggerResult, std::vector<TriggerResult>, TriggerResultComparator> _triggeredActions;

    // Value cache
    std::unordered_map<std::string, float> _values;

    // Group management
    std::unique_ptr<GroupInvitationHandler> _groupInvitationHandler;
    bool _wasInGroup = false;

    // Solo strategy activation tracking
    bool _soloStrategiesActivated = false;

    // Login spell event cleanup tracking (prevents LOGINEFFECT crash)
    bool _firstUpdateComplete = false;

    // Target scanning for autonomous engagement
    std::unique_ptr<TargetScanner> _targetScanner;

    // Game system managers
    std::unique_ptr<QuestManager> _questManager;
    std::unique_ptr<TradeManager> _tradeManager;
    std::unique_ptr<GatheringManager> _gatheringManager;
    std::unique_ptr<AuctionManager> _auctionManager;
    std::unique_ptr<GroupCoordinator> _groupCoordinator; // Advanced/GroupCoordinator - loot/quest/formation

    // Phase 3: Tactical Group Coordination (separate from Advanced/GroupCoordinator)
    std::unique_ptr<Coordination::GroupCoordinator> _tacticalCoordinator;

    // Death recovery system
    std::unique_ptr<DeathRecoveryManager> _deathRecoveryManager;

    // Movement arbiter - Enterprise movement request arbitration
    std::unique_ptr<MovementArbiter> _movementArbiter;

    // Combat state manager - Automatic combat state synchronization via DAMAGE_TAKEN events
    std::unique_ptr<CombatStateManager> _combatStateManager;

    // Phase 7.1: Event system integration
    std::unique_ptr<Events::EventDispatcher> _eventDispatcher;
    std::unique_ptr<ManagerRegistry> _managerRegistry;

    // Phase 5E: Decision fusion system - Unified arbitration for all decision-making
    std::unique_ptr<bot::ai::DecisionFusionSystem> _decisionFusion;

    // Phase 5 Enhancement: Action priority queue - Spell priority management
    std::unique_ptr<bot::ai::ActionPriorityQueue> _actionPriorityQueue;

    // Phase 5 Enhancement: Behavior tree - Hierarchical combat flow
    std::unique_ptr<bot::ai::BehaviorTree> _behaviorTree;

    // Performance tracking
    mutable PerformanceMetrics _performanceMetrics;

    // Equipment auto-equip timer (check every 10 seconds)
    uint32 _equipmentCheckTimer = 0;

    // Profession automation timer (check every 15 seconds)
    uint32 _professionCheckTimer = 0;

    // Banking automation timer (check every 5 minutes)
    uint32 _bankingCheckTimer = 0;

    // CRITICAL FIX #19: ObjectAccessor Deadlock Resolution
    // Cache for all ObjectAccessor results to eliminate recursive TrinityCore
    // std::shared_mutex acquisitions. Refreshed once per UpdateAI() cycle.
    ObjectCache _objectCache;

    // Thread safety
    // DEADLOCK FIX #14: Changed from std::shared_mutex to std::recursive_mutex
    // After 13 failed fixes, the root cause is that std::shared_mutex does NOT
    // support recursive locking (even shared_lock). When the same thread tries
    // to acquire the lock twice (even read-only), it throws "resource deadlock would occur".
    //
    // Despite exhaustive analysis, we could not eliminate ALL possible recursive
    // lock acquisitions across the complex callback chain (strategies -> triggers ->
    // actions -> group handlers -> back to strategies). Some edge case is still
    // triggering recursive acquisition.
    //
    // Solution: Use std::recursive_mutex which ALLOWS the same thread to acquire
    // the lock multiple times. This trades some performance (recursive_mutex is
    // slower than shared_mutex) for correctness and stability.
    //
    // Performance impact: Negligible - lock contention was already minimal, and
    // recursive_mutex overhead is only a few nanoseconds per acquisition.
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BOT_AI_STATE> _mutex;

    // Phase 7.3: Legacy observers removed (dead code)
    // Events now handled by EventDispatcher → Managers architecture

    // Debug tracking
    uint32 _lastDebugLogTime = 0;
    uint32 _debugLogAccumulator = 0; // Per-bot accumulator for manager update logging throttle
};

// ========================================================================
// DEFAULT IMPLEMENTATION - For bots without specialized AI
// ========================================================================

class TC_GAME_API DefaultBotAI : public BotAI
{
public:
    explicit DefaultBotAI(Player* player) : BotAI(player) {}

    // Uses base UpdateAI() implementation
    // No combat specialization needed for default bots
};

// ========================================================================
// AI FACTORY - Creates appropriate AI for each class
// ========================================================================

class TC_GAME_API BotAIFactory final : public IBotAIFactory
{
    BotAIFactory() = default;
    ~BotAIFactory() = default;
    BotAIFactory(BotAIFactory const&) = delete;
    BotAIFactory& operator=(BotAIFactory const&) = delete;

public:
    static BotAIFactory* instance();

    // AI creation
    std::unique_ptr<BotAI> CreateAI(Player* bot) override;
    std::unique_ptr<BotAI> CreateClassAI(Player* bot, uint8 classId) override;
    std::unique_ptr<BotAI> CreateClassAI(Player* bot, uint8 classId, uint8 spec) override;

    // Specialized AI creation
    std::unique_ptr<BotAI> CreateSpecializedAI(Player* bot, std::string const& type) override;
    std::unique_ptr<BotAI> CreatePvPAI(Player* bot) override;
    std::unique_ptr<BotAI> CreatePvEAI(Player* bot) override;
    std::unique_ptr<BotAI> CreateRaidAI(Player* bot) override;

    // AI registration
    void RegisterAICreator(std::string const& type,
                          std::function<std::unique_ptr<BotAI>(Player*)> creator) override;

    // Initialization
    void InitializeDefaultTriggers(BotAI* ai) override;
    void InitializeDefaultValues(BotAI* ai) override;

private:
    void InitializeDefaultStrategies(BotAI* ai);
    void InitializeClassStrategies(BotAI* ai, uint8 classId, uint8 spec);

    std::unordered_map<std::string, std::function<std::unique_ptr<BotAI>(Player*)>> _creators;
};

#define sBotAIFactory BotAIFactory::instance()

} // namespace Playerbot