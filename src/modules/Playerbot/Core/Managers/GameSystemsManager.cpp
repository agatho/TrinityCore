/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * GAME SYSTEMS MANAGER Implementation
 *
 * Phase 6: BotAI Decoupling & Final Cleanup
 *
 * This file consolidates all manager initialization, update, and cleanup
 * logic from BotAI.cpp, reducing god class complexity.
 */

#include "GameSystemsManager.h"
#include "AI/BotAI.h"
#include "Player.h"
#include "Log.h"
#include "Timer.h"

// Singleton manager includes for Update() calls
#include "Equipment/EquipmentManager.h"
#include "Professions/ProfessionManager.h"
#include "Banking/BankingManager.h"
#include "Professions/GatheringMaterialsBridge.h"
#include "Professions/ProfessionAuctionBridge.h"
#include "Professions/AuctionMaterialsBridge.h"
#include "Spatial/BlackboardManager.h"

#include <set>

namespace Playerbot
{

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

GameSystemsManager::GameSystemsManager(Player* bot, BotAI* botAI)
    : _bot(bot), _botAI(botAI)
{
    TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Constructing facade for bot '{}'",
        _bot ? _bot->GetName() : "Unknown");

    // Manager instances will be created in Initialize()
}

GameSystemsManager::~GameSystemsManager()
{
    TC_LOG_DEBUG("module.playerbot", "GameSystemsManager::~GameSystemsManager: Begin cleanup for bot '{}'",
        _bot ? _bot->GetName() : "Unknown");

    // ========================================================================
    // CRITICAL: Explicit Manager Destruction Order
    // ========================================================================
    //
    // Problem: C++ destroys members in REVERSE declaration order
    // - _eventDispatcher destroyed BEFORE _combatStateManager
    // - Managers try to UnsubscribeAll() from already-destroyed EventDispatcher
    // - Results in ACCESS_VIOLATION
    //
    // Solution: Manually destroy managers HERE in correct dependency order
    // - Ensures EventDispatcher is still alive during manager cleanup
    // - Managers can safely call UnsubscribeAll() during OnShutdown()
    //
    // Destruction Order (CORRECT):
    // 1. Manual reset() of managers (HERE) ← Managers alive, EventDispatcher alive ✅
    // 2. Automatic _eventDispatcher destruction ← All managers gone, safe ✅
    // ========================================================================

    // 1. Combat state manager - monitors other managers
    if (_combatStateManager)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying CombatStateManager");
        _combatStateManager.reset();
    }

    // 2. Death recovery manager - may interact with combat
    if (_deathRecoveryManager)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying DeathRecoveryManager");
        _deathRecoveryManager.reset();
    }

    // 3. Movement system
    if (_unifiedMovementCoordinator)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying UnifiedMovementCoordinator");
        _unifiedMovementCoordinator.reset();
    }

    // 4. Game system managers (order doesn't matter, no interdependencies)
    if (_questManager)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying QuestManager");
        _questManager.reset();
    }

    if (_tradeManager)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying TradeManager");
        _tradeManager.reset();
    }

    if (_gatheringManager)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying GatheringManager");
        _gatheringManager.reset();
    }

    if (_professionManager)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying ProfessionManager");
        _professionManager.reset();
    }

    if (_gatheringMaterialsBridge)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying GatheringMaterialsBridge");
        _gatheringMaterialsBridge.reset();
    }

    if (_auctionMaterialsBridge)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying AuctionMaterialsBridge");
        _auctionMaterialsBridge.reset();
    }

    if (_professionAuctionBridge)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying ProfessionAuctionBridge");
        _professionAuctionBridge.reset();
    }

    if (_auctionManager)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying AuctionManager");
        _auctionManager.reset();
    }

    if (_bankingManager)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying BankingManager");
        _bankingManager.reset();
    }

    if (_equipmentManager)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying EquipmentManager");
        _equipmentManager.reset();
    }

    if (_mountManager)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying MountManager");
        _mountManager.reset();
    }

    if (_battlePetManager)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying BattlePetManager");
        _battlePetManager.reset();
    }

    if (_arenaAI)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying ArenaAI");
        _arenaAI.reset();
    }

    if (_pvpCombatAI)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying PvPCombatAI");
        _pvpCombatAI.reset();
    }

    if (_auctionHouse)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying AuctionHouse");
        _auctionHouse.reset();
    }

    if (_farmingCoordinator)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying FarmingCoordinator");
        _farmingCoordinator.reset();
    }

    if (_groupCoordinator)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying GroupCoordinator");
        _groupCoordinator.reset();
    }

    // 5. Support systems
    if (_targetScanner)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying TargetScanner");
        _targetScanner.reset();
    }

    if (_groupInvitationHandler)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying GroupInvitationHandler");
        _groupInvitationHandler.reset();
    }

    if (_priorityManager)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying BehaviorPriorityManager");
        _priorityManager.reset();
    }

    // 6. Decision systems
    if (_decisionFusion)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying DecisionFusionSystem");
        _decisionFusion.reset();
    }

    if (_actionPriorityQueue)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying ActionPriorityQueue");
        _actionPriorityQueue.reset();
    }

    if (_behaviorTree)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying BehaviorTree");
        _behaviorTree.reset();
    }

    if (_hybridAI)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying HybridAIController");
        _hybridAI.reset();
    }

    // 7. Finally: Manager registry and event dispatcher
    if (_managerRegistry)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying ManagerRegistry");
        _managerRegistry.reset();
    }

    if (_eventDispatcher)
    {
        TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Destroying EventDispatcher");
        _eventDispatcher.reset();
    }

    TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: ✅ All managers destroyed");
    TC_LOG_INFO("module.playerbot", "GameSystemsManager: Destructor complete for bot '{}'",
        _bot ? _bot->GetName() : "Unknown");
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void GameSystemsManager::Initialize(Player* bot)
{
    if (_initialized)
    {
        TC_LOG_WARN("module.playerbot", "GameSystemsManager: Already initialized for bot '{}'",
            bot ? bot->GetName() : "Unknown");
        return;
    }

    _bot = bot;

    TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Initializing all managers for bot '{}'",
        _bot ? _bot->GetName() : "Unknown");

    // ========================================================================
    // PHASE 1: Create Manager Instances (in dependency order)
    // ========================================================================

    // Priority-based behavior manager
    _priorityManager = std::make_unique<bot::ai::BehaviorPriorityManager>(_botAI);

    // Group management
    _groupInvitationHandler = std::make_unique<GroupInvitationHandler>(_bot);

    // Target scanner for autonomous enemy detection
    _targetScanner = std::make_unique<TargetScanner>(_bot);

    // Game system managers
    _questManager = std::make_unique<QuestManager>(_bot, _botAI);
    _tradeManager = std::make_unique<TradeManager>(_bot, _botAI);
    _gatheringManager = std::make_unique<GatheringManager>(_bot, _botAI);
    _professionManager = std::make_unique<ProfessionManager>(_bot);
    _gatheringMaterialsBridge = std::make_unique<GatheringMaterialsBridge>(_bot);
    _auctionMaterialsBridge = std::make_unique<AuctionMaterialsBridge>(_bot);
    _professionAuctionBridge = std::make_unique<ProfessionAuctionBridge>(_bot);
    _farmingCoordinator = std::make_unique<FarmingCoordinator>(_bot);
    _auctionManager = std::make_unique<AuctionManager>(_bot, _botAI);
    _bankingManager = std::make_unique<BankingManager>(_bot);
    _equipmentManager = std::make_unique<EquipmentManager>(_bot);
    _mountManager = std::make_unique<MountManager>(_bot);
    _battlePetManager = std::make_unique<BattlePetManager>(_bot);
    _arenaAI = std::make_unique<ArenaAI>(_bot);
    _pvpCombatAI = std::make_unique<PvPCombatAI>(_bot);
    _auctionHouse = std::make_unique<AuctionHouse>(_bot);
    _guildBankManager = std::make_unique<GuildBankManager>(_bot);
    _guildEventCoordinator = std::make_unique<GuildEventCoordinator>(_bot);
    _guildIntegration = std::make_unique<GuildIntegration>(_bot);
    _lootDistribution = std::make_unique<LootDistribution>(_bot);
    _tradeSystem = std::make_unique<TradeSystem>(_bot);
    _dynamicQuestSystem = std::make_unique<DynamicQuestSystem>(_bot);
    _objectiveTracker = std::make_unique<ObjectiveTracker>(_bot);
    _questCompletion = std::make_unique<QuestCompletion>(_bot);
    _questPickup = std::make_unique<QuestPickup>(_bot);
    _questTurnIn = std::make_unique<QuestTurnIn>(_bot);
    _questValidation = std::make_unique<QuestValidation>(_bot);
    _roleAssignment = std::make_unique<RoleAssignment>(_bot);
    _lfgBotManager = std::make_unique<LFGBotManager>(_bot);
    _lfgBotSelector = std::make_unique<LFGBotSelector>(_bot);
    _lfgGroupCoordinator = std::make_unique<LFGGroupCoordinator>(_bot);
    _instanceCoordination = std::make_unique<InstanceCoordination>(_bot);
    _botPriorityManager = std::make_unique<BotPriorityManager>(_bot);
    _botWorldSessionMgr = std::make_unique<BotWorldSessionMgr>(_bot);
    _botLifecycleManager = std::make_unique<BotLifecycleManager>(_bot);
    _groupCoordinator = std::make_unique<Advanced::GroupCoordinator>(_bot, _botAI);

    // Death recovery system
    _deathRecoveryManager = std::make_unique<DeathRecoveryManager>(_bot, _botAI);

    // Unified Movement Coordinator (PRIMARY movement system)
    _unifiedMovementCoordinator = std::make_unique<UnifiedMovementCoordinator>(_bot);

    // Combat state manager
    _combatStateManager = std::make_unique<CombatStateManager>(_bot, _botAI);

    TC_LOG_INFO("module.playerbot", "📋 MANAGERS CREATED: {} - Quest, Trade, Gathering, Auction, Group, DeathRecovery, UnifiedMovement, CombatState systems ready",
        _bot->GetName());

    // ========================================================================
    // PHASE 2: Event System
    // ========================================================================

    // Event dispatcher and manager registry
    _eventDispatcher = std::make_unique<Events::EventDispatcher>(512);  // Initial queue size: 512 events
    _managerRegistry = std::make_unique<ManagerRegistry>();

    TC_LOG_INFO("module.playerbot", "🔄 EVENT DISPATCHER & MANAGER REGISTRY: {} - Phase 7.1 integration ready",
        _bot->GetName());

    // ========================================================================
    // PHASE 3: Decision Systems
    // ========================================================================

    // Decision fusion system
    _decisionFusion = std::make_unique<bot::ai::DecisionFusionSystem>();

    TC_LOG_INFO("module.playerbot", "🎯 DECISION FUSION SYSTEM: {} - Phase 5E unified arbitration ready",
        _bot->GetName());

    // Action priority queue
    _actionPriorityQueue = std::make_unique<bot::ai::ActionPriorityQueue>();

    TC_LOG_INFO("module.playerbot", "📋 ACTION PRIORITY QUEUE: {} - Phase 5 spell priority system ready",
        _bot->GetName());

    // Behavior tree
    _behaviorTree = std::make_unique<bot::ai::BehaviorTree>("DefaultTree");

    TC_LOG_INFO("module.playerbot", "🌲 BEHAVIOR TREE: {} - Phase 5 hierarchical decision system ready",
        _bot->GetName());

    // ========================================================================
    // PHASE 4: Hybrid AI System
    // ========================================================================

    InitializeHybridAI();

    // ========================================================================
    // PHASE 5: Manager Initialization via IManagerBase
    // ========================================================================

    if (_managerRegistry && _eventDispatcher)
    {
        // Initialize managers through IManagerBase interface
        if (_questManager)
        {
            _questManager->Initialize();
            TC_LOG_INFO("module.playerbot.managers", "✅ QuestManager initialized via IManagerBase");
        }

        if (_tradeManager)
        {
            _tradeManager->Initialize();
            TC_LOG_INFO("module.playerbot.managers", "✅ TradeManager initialized via IManagerBase");
        }

        if (_gatheringManager)
        {
            _gatheringManager->Initialize();
            TC_LOG_INFO("module.playerbot.managers", "✅ GatheringManager initialized via IManagerBase");
        }

        if (_gatheringMaterialsBridge)
        {
            _gatheringMaterialsBridge->Initialize();
            TC_LOG_INFO("module.playerbot.managers", "✅ GatheringMaterialsBridge initialized - gathering-crafting coordination active");
        }

        if (_auctionMaterialsBridge)
        {
            _auctionMaterialsBridge->Initialize();
            TC_LOG_INFO("module.playerbot.managers", "✅ AuctionMaterialsBridge initialized - material sourcing optimization active");
        }

        if (_professionAuctionBridge)
        {
            _professionAuctionBridge->Initialize();
            TC_LOG_INFO("module.playerbot.managers", "✅ ProfessionAuctionBridge initialized - profession-auction coordination active");
        }

        if (_auctionManager)
        {
            _auctionManager->Initialize();
            TC_LOG_INFO("module.playerbot.managers", "✅ AuctionManager initialized via IManagerBase");
        }

        if (_bankingManager)
        {
            _bankingManager->OnInitialize();
            TC_LOG_INFO("module.playerbot.managers", "✅ BankingManager initialized - personal banking automation active");
        }

        if (_farmingCoordinator)
        {
            _farmingCoordinator->Initialize();
            TC_LOG_INFO("module.playerbot.managers", "✅ FarmingCoordinator initialized - profession farming automation active");
        }

        if (_mountManager)
        {
            _mountManager->Initialize();
            TC_LOG_INFO("module.playerbot.managers", "✅ MountManager initialized - mount automation and collection tracking active");
        }

        if (_battlePetManager)
        {
            _battlePetManager->Initialize();
            TC_LOG_INFO("module.playerbot.managers", "✅ BattlePetManager initialized - battle pet automation and collection active");
        }

        if (_arenaAI)
        {
            _arenaAI->Initialize();
            TC_LOG_INFO("module.playerbot.managers", "✅ ArenaAI initialized - arena PvP automation active");
        }

        if (_pvpCombatAI)
        {
            _pvpCombatAI->Initialize();
            TC_LOG_INFO("module.playerbot.managers", "✅ PvPCombatAI initialized - PvP combat automation active");
        }

        if (_groupCoordinator)
        {
            _groupCoordinator->Initialize();
            TC_LOG_INFO("module.playerbot.managers", "✅ GroupCoordinator initialized - Dungeon/Raid coordination active");
        }

        if (_combatStateManager)
        {
            _combatStateManager->Initialize();
            TC_LOG_INFO("module.playerbot.managers", "✅ CombatStateManager initialized - DAMAGE_TAKEN event subscription active");
        }

        // Subscribe managers to events
        SubscribeManagersToEvents();

        TC_LOG_INFO("module.playerbot.managers",
            "🎯 PHASE 7.1 INTEGRATION COMPLETE: {} - {} managers initialized, events subscribed",
            _bot->GetName(),
            (_questManager ? 1 : 0) + (_tradeManager ? 1 : 0) + (_gatheringManager ? 1 : 0) +
            (_auctionManager ? 1 : 0) + (_combatStateManager ? 1 : 0));
    }

    _initialized = true;

    TC_LOG_INFO("module.playerbot", "✅ GameSystemsManager: Initialization complete for bot '{}'",
        _bot ? _bot->GetName() : "Unknown");
}

void GameSystemsManager::Shutdown()
{
    TC_LOG_DEBUG("module.playerbot", "GameSystemsManager: Shutting down all managers for bot '{}'",
        _bot ? _bot->GetName() : "Unknown");

    // Managers will be destroyed in destructor with proper order
    _initialized = false;
}

// ============================================================================
// HYBRID AI INITIALIZATION
// ============================================================================

void GameSystemsManager::InitializeHybridAI()
{
    // Initialize Hybrid AI Decision System (Utility AI + Behavior Trees)
    // This will be implemented when HybridAIController is available
    // For now, create empty instance
    _hybridAI = std::make_unique<bot::ai::HybridAIController>();

    TC_LOG_INFO("module.playerbot", "🤖 HYBRID AI CONTROLLER: {} - Hybrid decision system ready",
        _bot ? _bot->GetName() : "Unknown");
}

// ============================================================================
// EVENT SUBSCRIPTION
// ============================================================================

void GameSystemsManager::SubscribeManagersToEvents()
{
    // Subscribe QuestManager to quest events
    if (_questManager && _eventDispatcher)
    {
        _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_ACCEPTED, _questManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_COMPLETED, _questManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_TURNED_IN, _questManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_ABANDONED, _questManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_FAILED, _questManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_STATUS_CHANGED, _questManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_OBJECTIVE_COMPLETE, _questManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_OBJECTIVE_PROGRESS, _questManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_ITEM_COLLECTED, _questManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_CREATURE_KILLED, _questManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_EXPLORATION, _questManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_REWARD_RECEIVED, _questManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_REWARD_CHOSEN, _questManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_EXPERIENCE_GAINED, _questManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_REPUTATION_GAINED, _questManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_CHAIN_ADVANCED, _questManager.get());
        TC_LOG_INFO("module.playerbot.managers", "🔗 QuestManager subscribed to 16 quest events");
    }

    // Subscribe TradeManager to trade events
    if (_tradeManager && _eventDispatcher)
    {
        _eventDispatcher->Subscribe(StateMachine::EventType::TRADE_INITIATED, _tradeManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::TRADE_ACCEPTED, _tradeManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::TRADE_CANCELLED, _tradeManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::TRADE_ITEM_ADDED, _tradeManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::TRADE_GOLD_ADDED, _tradeManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::GOLD_RECEIVED, _tradeManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::GOLD_SPENT, _tradeManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::LOW_GOLD_WARNING, _tradeManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::VENDOR_PURCHASE, _tradeManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::VENDOR_SALE, _tradeManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::REPAIR_COST, _tradeManager.get());
        TC_LOG_INFO("module.playerbot.managers", "🔗 TradeManager subscribed to 11 trade/gold events");
    }

    // Subscribe AuctionManager to auction events
    if (_auctionManager && _eventDispatcher)
    {
        _eventDispatcher->Subscribe(StateMachine::EventType::AUCTION_BID_PLACED, _auctionManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::AUCTION_WON, _auctionManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::AUCTION_OUTBID, _auctionManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::AUCTION_EXPIRED, _auctionManager.get());
        _eventDispatcher->Subscribe(StateMachine::EventType::AUCTION_SOLD, _auctionManager.get());
        TC_LOG_INFO("module.playerbot.managers", "🔗 AuctionManager subscribed to 5 auction events");
    }
}

// ============================================================================
// UPDATE
// ============================================================================

void GameSystemsManager::Update(uint32 diff)
{
    if (!_initialized || !_bot || !_bot->IsInWorld())
        return;

    // Update managers
    UpdateManagers(diff);
}

void GameSystemsManager::UpdateManagers(uint32 diff)
{
    // DEBUG LOGGING THROTTLE: Only log every 50 seconds for whitelisted test bots
    _debugLogAccumulator += diff;
    static const std::set<std::string> testBots = {"Anderenz", "Boone", "Nelona", "Sevtap"};
    bool isTestBot = _bot && (testBots.find(_bot->GetName()) != testBots.end());
    bool shouldLog = isTestBot && (_debugLogAccumulator >= 50000);
    if (shouldLog) _debugLogAccumulator = 0;

    if (shouldLog)
    {
        TC_LOG_ERROR("module.playerbot", "🔧 GameSystemsManager::UpdateManagers ENTRY: Bot {}, IsInWorld()={}",
            _bot->GetName(), _bot->IsInWorld());
    }

    if (!_bot || !_bot->IsInWorld())
    {
        if (shouldLog)
            TC_LOG_ERROR("module.playerbot", "❌ GameSystemsManager::UpdateManagers EARLY RETURN: Bot {} not in world",
                _bot->GetName());
        return;
    }

    // ========================================================================
    // PHASE 7.1: EVENT DISPATCHER - Process queued events first
    // ========================================================================
    if (_eventDispatcher)
    {
        // Process up to 100 events per update cycle to maintain performance
        uint32 eventsProcessed = _eventDispatcher->ProcessQueue(100);

        if (eventsProcessed > 0)
        {
            TC_LOG_TRACE("module.playerbot.events",
                "Bot {} processed {} events this cycle", _bot->GetName(), eventsProcessed);
        }

        // Warn if queue is backing up (>500 events indicates processing bottleneck)
        size_t queueSize = _eventDispatcher->GetQueueSize();
        if (queueSize > 500)
        {
            TC_LOG_WARN("module.playerbot.events",
                "Bot {} event queue backlog: {} events pending", _bot->GetName(), queueSize);
        }
    }

    // ========================================================================
    // PHASE 7.1: MANAGER REGISTRY - Update all registered managers
    // ========================================================================
    if (_managerRegistry)
    {
        uint32 managersUpdated = _managerRegistry->UpdateAll(diff);

        if (managersUpdated > 0)
        {
            TC_LOG_TRACE("module.playerbot.managers",
                "Bot {} updated {} managers this cycle", _bot->GetName(), managersUpdated);
        }
    }

    // ========================================================================
    // MANAGER UPDATES - Legacy direct updates during Phase 7 transition
    // ========================================================================

    // Quest manager handles quest acceptance, turn-in, and tracking
    if (_questManager)
        _questManager->Update(diff);

    // Trade manager handles vendor interactions, repairs, and consumables
    if (_tradeManager)
        _tradeManager->Update(diff);

    // Gathering manager handles mining, herbalism, skinning
    if (_gatheringManager)
        _gatheringManager->Update(diff);

    // Gathering materials bridge coordinates gathering with crafting needs
    if (_gatheringMaterialsBridge)
        _gatheringMaterialsBridge->Update(diff);

    // Auction materials bridge optimizes material sourcing (gather vs buy)
    if (_auctionMaterialsBridge)
        _auctionMaterialsBridge->Update(diff);

    // Profession auction bridge handles selling materials/crafts and buying materials for leveling
    if (_professionAuctionBridge)
        _professionAuctionBridge->Update(diff);

    // Auction manager handles auction house buying, selling, and market scanning
    if (_auctionManager)
        _auctionManager->Update(diff);

    // Group coordinator handles group/raid mechanics, role assignment, and coordination
    if (_groupCoordinator)
        _groupCoordinator->Update(diff);

    // Banking manager handles personal banking automation (gold/items)
    if (_bankingManager)
        _bankingManager->OnUpdate(_bot, diff);

    // Farming coordinator handles profession skill leveling automation
    if (_farmingCoordinator)
        _farmingCoordinator->Update(_bot, diff);

    // ========================================================================
    // EQUIPMENT AUTO-EQUIP - Check every 10 seconds
    // ========================================================================
    _equipmentCheckTimer += diff;
    if (_equipmentCheckTimer >= 10000) // 10 seconds
    {
        _equipmentCheckTimer = 0;
        if (_equipmentManager)
            _equipmentManager->AutoEquipBestGear();
    }

    // ========================================================================
    // MOUNT AUTOMATION - Update every frame for responsive mounting
    // ========================================================================
    if (_mountManager)
        _mountManager->Update(diff);

    // ========================================================================
    // BATTLE PET AUTOMATION - Update every frame for battle pet AI
    // ========================================================================
    if (_battlePetManager)
        _battlePetManager->Update(diff);

    // ========================================================================
    // ARENA PVP AI - Update every frame for arena automation
    // ========================================================================
    if (_arenaAI)
        _arenaAI->Update(diff);

    // ========================================================================
    // PVP COMBAT AI - Update for PvP combat automation (100ms throttle)
    // ========================================================================
    if (_pvpCombatAI)
        _pvpCombatAI->Update(diff);

    // ========================================================================
    // PROFESSION AUTOMATION - Check every 15 seconds
    // ========================================================================
    _professionCheckTimer += diff;
    if (_professionCheckTimer >= 15000) // 15 seconds
    {
        _professionCheckTimer = 0;
        if (_professionManager)
            _professionManager->Update(diff);
    }
}

} // namespace Playerbot
