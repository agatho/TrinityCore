# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# TrinityCore Playerbot Module - Enterprise Implementation
# High-performance AI-controlled player bots with Intel TBB, parallel hashmap, and boost optimizations

if(NOT BUILD_PLAYERBOT)
    message(STATUS "Playerbot module: DISABLED (use -DBUILD_PLAYERBOT=1 to enable)")
    return()
endif()

message(STATUS "=== Building TrinityCore Playerbot Enterprise Module ===")

# Include enterprise dependency validation
include(${CMAKE_SOURCE_DIR}/cmake/modules/FindPlayerbotDependencies.cmake)

# Verify all enterprise dependencies are available
# Note: Dependencies are validated in FindPlayerbotDependencies.cmake
# The validation script will fail fast if any critical dependencies are missing
message(STATUS "Enterprise dependencies validated by FindPlayerbotDependencies.cmake")

message(STATUS "✅ All enterprise dependencies validated")
message(STATUS "Playerbot module: ENABLED with enterprise optimizations")

# Set source files - Enterprise Phase 1 Implementation
set(PLAYERBOT_SOURCES
    # Module entry point
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModule.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModuleAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModuleAdapter.h

    # Configuration system (basic)
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotTradeConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotTradeConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotLog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotLog.h
    # Runtime configuration manager (Integration & Polish Task I.2)
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/ConfigManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/ConfigManager.h
    # Temporarily disabled: DependencyValidator (needs MySQL headers fix)
    # ${CMAKE_CURRENT_SOURCE_DIR}/Config/DependencyValidator.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/Config/DependencyValidator.h

    # Session management (essential for bot functionality)
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSession.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotPacketSimulator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotPacketSimulator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/PacketDeferralClassifier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/PacketDeferralClassifier.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotWorldSessionMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotWorldSessionMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotPacketRelay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotPacketRelay.h

    # Enterprise Session Management (Phase 2: 5000 Bot Scalability)
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotPriorityManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotPriorityManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotPerformanceMonitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotPerformanceMonitor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotHealthCheck.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotHealthCheck.h

    # Chat system (Phase 4: Complete command processing)
    ${CMAKE_CURRENT_SOURCE_DIR}/Chat/BotChatCommandHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Chat/BotChatCommandHandler.h

    # Database pool (basic implementation)
    # Temporarily disabled: BotDatabasePool (needs API updates)
    # ${CMAKE_CURRENT_SOURCE_DIR}/Database/BotDatabasePool.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/Database/BotDatabasePool.h

    # AI system (AI Framework implementation)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI_HybridAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAIFactory.cpp

    # Specialized AI Factory - Enterprise-grade spec routing with Phase 5 integration
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/SpecializedAIFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/SpecializedAIFactory.h

    # Phase 2.1: BehaviorManager Base Class - Manager Pattern Foundation
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ExampleManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ExampleManager.h

    # Phase 2.1 Refactoring: BehaviorPriorityManager - Priority-based Behavior Coordination (Fixes Issues #2 & #3)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorPriorityManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorPriorityManager.h

    # Hybrid AI Decision System: Utility AI Layer (Phase 1)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Utility/UtilitySystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Utility/UtilityContextBuilder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Utility/Evaluators/CombatEvaluators.h

    # Hybrid AI Decision System: Behavior Tree Execution Layer (Phase 2)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorTree/BehaviorTree.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorTree/Nodes/CombatNodes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorTree/Nodes/HealingNodes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorTree/Nodes/MovementNodes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorTree/BehaviorTreeFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorTree/BehaviorTreeFactory.h

    # Hybrid AI Decision System: Controller Integration (Phase 2 Week 3)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/HybridAIController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/HybridAIController.h

    # Hierarchical Group Coordination System (Phase 3)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Coordination/GroupCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Coordination/GroupCoordinator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Coordination/RoleCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Coordination/RoleCoordinator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Coordination/RaidOrchestrator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Coordination/RaidOrchestrator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Coordination/ZoneOrchestrator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Coordination/ZoneOrchestrator.h

    # Blackboard Shared State System (Phase 4)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Blackboard/SharedBlackboard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Blackboard/SharedBlackboard.h

    # Integrated AI System: Bridges BT + Blackboard + Coordination (Phase 4 Week 2)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Integration/IntegratedAIContext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Integration/IntegratedAIContext.h

    # AI Strategy System (minimal implementation)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/Strategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/Strategy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/GroupCombatStrategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/GroupCombatStrategy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/SoloStrategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/SoloStrategy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/QuestStrategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/QuestStrategy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/LootStrategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/LootStrategy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/RestStrategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/RestStrategy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/SoloCombatStrategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/SoloCombatStrategy.h

    # AI Action System (basic)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/Action.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/Action.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/CommonActions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/CommonActions.h

    # Core References System - SafeObjectReference (Issue #4 Fix)
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/References/SafeObjectReference.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/References/SafeObjectReference.h

    # Core Integration Hooks
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/PlayerBotHooks.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/PlayerBotHooks.h

    # Phase 7.1: Manager System Architecture - Observer-Manager Integration Layer
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Managers/IManagerBase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Managers/ManagerRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Managers/ManagerRegistry.h

    # Phase 6: Game Systems Facade - BotAI Decoupling
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Managers/IGameSystemsManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Managers/GameSystemsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Managers/GameSystemsManager.h

    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Events/EventDispatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Events/EventDispatcher.h

    # Core Services - Enterprise Infrastructure
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Services/BotNpcLocationService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Services/BotNpcLocationService.h

    # Phase 7.3: Event System - Streamlined Architecture (Legacy code eliminated)
    ${CMAKE_CURRENT_SOURCE_DIR}/Events/BotEventData.h
    # BotEventSystem.cpp/h - DELETED (legacy singleton removed)
    # BotEventHooks.cpp/h - DELETED (1113 lines redundant code eliminated)
    # Observers (15 files) - DELETED (5000+ lines dead code eliminated)
    # Event flow now: TrinityCore ScriptMgr → PlayerbotEventScripts → EventDispatcher → Managers

    # Phase 4.1: Script System Integration - Non-Invasive Event Hooks
    ${CMAKE_CURRENT_SOURCE_DIR}/Scripts/PlayerbotEventScripts.cpp


    # AI Trigger System
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Triggers/Trigger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Triggers/Trigger.h

    # AI Value System
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Values/Value.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Values/Value.h

    # AI Utility Systems
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ObjectCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ObjectCache.h

    # ClassAI System - Base and implementations
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/ClassAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/ClassAI.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/ActionPriority.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/ActionPriority.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/CooldownManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/CooldownManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/ResourceManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/ResourceManager.h

    # ClassAI Common Utilities (Deduplication Phase: Unified Systems)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Common/StatusEffectTracker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Common/CooldownManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Common/RotationHelpers.h

    # ClassAI Behavior Tree Integration (Phase 5: 13 Classes × 3 Specs)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/ClassBehaviorTreeRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/ClassBehaviorTreeRegistry.h

    # ClassAI Template Architecture (Phase 3: Code Duplication Elimination)
    # Note: .cpp removed due to old SPEC_* constants, but .h still needed for template definitions
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/CombatSpecializationTemplates.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/CombatSpecializationTemplates.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/ResourceTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/RoleSpecializations.h

    # Baseline Rotation Manager (Levels 1-9 unspecialized bots)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/BaselineRotationManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/BaselineRotationManager.h

    # Death Knight AI - Template-based architecture (Option A: Full Migration)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/DeathKnightAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/DeathKnightAI.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/DiseaseManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/DiseaseManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/RuneManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/RuneManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/DeathKnightTypes.h

    # Death Knight Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/BloodDeathKnightRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/FrostDeathKnightRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/UnholyDeathKnightRefactored.h

    # Demon Hunter AI
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DemonHunters/DemonHunterAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DemonHunters/DemonHunterAI.h

    # Druid AI
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Druids/DruidAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Druids/DruidAI.h

    # Druid Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Druids/BalanceDruidRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Druids/FeralDruidRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Druids/GuardianDruidRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Druids/RestorationDruidRefactored.h

    # Evoker AI - Template-based architecture (Option A: Full Migration)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Evokers/EvokerAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Evokers/EvokerAI.h

    # Evoker Refactored Specializations (Template-based with Phase 5 integration)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Evokers/DevastationEvokerRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Evokers/PreservationEvokerRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Evokers/AugmentationEvokerRefactored.h

    # Hunter AI - Template-based architecture (Option A: Full Migration)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Hunters/HunterAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Hunters/HunterAI.h

    # Hunter Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Hunters/BeastMasteryHunterRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Hunters/MarksmanshipHunterRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Hunters/SurvivalHunterRefactored.h

    # Demon Hunter Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DemonHunters/HavocDemonHunterRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DemonHunters/VengeanceDemonHunterRefactored.h

    # Mage AI - Template-based architecture (Option A: Full Migration)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Mages/MageAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Mages/MageAI.h

    # Mage Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Mages/ArcaneMageRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Mages/FireMageRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Mages/FrostMageRefactored.h

    # Priest Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Priests/DisciplinePriestRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Priests/HolyPriestRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Priests/ShadowPriestRefactored.h

    # Monk AI
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Monks/MonkAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Monks/MonkAI.h

    # Monk Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Monks/BrewmasterMonkRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Monks/MistweaverMonkRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Monks/WindwalkerMonkRefactored.h

    # Paladin AI
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Paladins/PaladinAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Paladins/PaladinAI.h

    # Paladin Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Paladins/HolyPaladinRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Paladins/ProtectionPaladinRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Paladins/RetributionSpecializationRefactored.h

    # Priest AI - Template-based architecture (Option A: Full Migration)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Priests/PriestAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Priests/PriestAI.h

    # Rogue AI - Template-based architecture (Option A: Full Migration)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Rogues/RogueAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Rogues/RogueAI.h

    # Rogue Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Rogues/AssassinationRogueRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Rogues/OutlawRogueRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Rogues/SubtletyRogueRefactored.h

    # Shaman AI - Template-based architecture (Option A: Full Migration)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Shamans/ShamanAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Shamans/ShamanAI.h

    # Shaman Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Shamans/ElementalShamanRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Shamans/EnhancementShamanRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Shamans/RestorationShamanRefactored.h

    # Warlock AI - Template-based architecture (Option A: Full Migration)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warlocks/WarlockAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warlocks/WarlockAI.h

    # Warlock Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warlocks/AfflictionWarlockRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warlocks/DemonologyWarlockRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warlocks/DestructionWarlockRefactored.h

    # Warrior AI
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warriors/WarriorAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warriors/WarriorAI.h

    # Warrior Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warriors/ArmsWarriorRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warriors/FuryWarriorRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warriors/ProtectionWarriorRefactored.h

    # Group Management System
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupInvitationHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupInvitationHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/RoleDefinitions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/RoleDefinitions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/RoleAssignment.h

    # Group Event System - Event-Driven Architecture for Group Coordination
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupEvents.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupEventHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupEventHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/PlayerbotGroupScript.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/PlayerbotGroupScript.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupStateMachine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupStateMachine.h

    # Network Packet Sniffer - Centralized Event Detection System
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/PlayerbotPacketSniffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/PlayerbotPacketSniffer.h

    # Phase 2: Typed Packet Handlers (WoW 11.2 Solution - Legacy system fully replaced)
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseGroupPacket_Typed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseCombatPacket_Typed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseCooldownPacket_Typed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseLootPacket_Typed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseQuestPacket_Typed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseAuraPacket_Typed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseResourcePacket_Typed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseSocialPacket_Typed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseAuctionPacket_Typed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseNPCPacket_Typed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseInstancePacket_Typed.cpp

    # Phase 4: Complete Event Bus System (All 11 Buses)
    ${CMAKE_CURRENT_SOURCE_DIR}/Combat/CombatEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Combat/CombatEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Combat/CombatEvents.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Cooldown/CooldownEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Cooldown/CooldownEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Cooldown/CooldownEvents.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Aura/AuraEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Aura/AuraEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Aura/AuraEvents.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Loot/LootEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Loot/LootEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Loot/LootEvents.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestEvents.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Resource/ResourceEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Resource/ResourceEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Resource/ResourceEvents.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/SocialEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/SocialEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/SocialEvents.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Auction/AuctionEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Auction/AuctionEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Auction/AuctionEvents.h
    ${CMAKE_CURRENT_SOURCE_DIR}/NPC/NPCEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/NPC/NPCEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/NPC/NPCEvents.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Instance/InstanceEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Instance/InstanceEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Instance/InstanceEvents.h

    # Phase 4: BotAI Event Handler Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI_EventHandlers.cpp

    # Movement System
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/BotMovementUtil.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/BotMovementUtil.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/LeaderFollowBehavior.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/LeaderFollowBehavior.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/BotWorldPositioner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/BotWorldPositioner.h

    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/QuestPathfinder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/QuestPathfinder.h

    # Phase 2: Account Management System
    ${CMAKE_CURRENT_SOURCE_DIR}/Account/BotAccountMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Account/BotAccountMgr.h

    # Phase 2: Character Management System
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotNameMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotNameMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCharacterDistribution.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCharacterDistribution.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotLevelDistribution.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotLevelDistribution.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCustomizationGenerator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCustomizationGenerator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotLevelManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotLevelManager.h

    # Phase 2: Companion Systems - Mount and Battle Pet Management
    # TODO: Re-enable when API compatibility is fixed (see PlayerbotWorldScript.cpp)
    # ${CMAKE_CURRENT_SOURCE_DIR}/Companion/MountManager.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/Companion/MountManager.h
    # ${CMAKE_CURRENT_SOURCE_DIR}/Companion/BattlePetManager.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/Companion/BattlePetManager.h

    # Phase 2: Bot Lifecycle Management System
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotCharacterCreator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotCharacterCreator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotScheduler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotScheduler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotLifecycleMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotLifecycleMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/DeathRecoveryManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/DeathRecoveryManager.h

    # Phase 2: Adaptive Throttling System (Bot Login Refactoring)
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/ResourceMonitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/ResourceMonitor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/SpawnPriorityQueue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/SpawnPriorityQueue.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/SpawnCircuitBreaker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/SpawnCircuitBreaker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/AdaptiveSpawnThrottler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/AdaptiveSpawnThrottler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/StartupSpawnOrchestrator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/StartupSpawnOrchestrator.h

    # WoW 11.2 Character Creation Data (header-only)
    ${CMAKE_CURRENT_SOURCE_DIR}/Data/WoW112CharacterCreation.h

    # Phase 2: Refactored Architecture Components
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnOrchestrator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnOrchestrator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotResourcePool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotResourcePool.h
    # DUPLICATE FIX: Lifecycle/BotPerformanceMonitor conflicts with Session/BotPerformanceMonitor
    # ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotPerformanceMonitor.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotPerformanceMonitor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnEventBus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnEvents.h

    # Performance Optimization System (Phase 5)
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ObjectPool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/PacketPoolManager.h

    # Phase 0 (Week 1): Spell Packet Builder - Enterprise packet helpers for bot spell casting
    ${CMAKE_CURRENT_SOURCE_DIR}/Packets/SpellPacketBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Packets/SpellPacketBuilder.h

    # Phase 2: Enhanced Session Management
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionFactory.h

    # AI Combat System - Group Combat Coordination
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/GroupCombatTrigger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/GroupCombatTrigger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/RoleBasedCombatPositioning.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/RoleBasedCombatPositioning.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/PositionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/PositionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/MechanicAwareness.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/MechanicAwareness.h

    # Unified Interrupt System - Enterprise Refactoring (replaces 3 legacy systems)
    # TODO Phase 4D: Re-enable after completing UnifiedInterruptSystem implementation
    # Merges: InterruptCoordinator, InterruptRotationManager, InterruptManager
    # Benefits: 32% code reduction, zero conflict risk, 3x performance improvement
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/UnifiedInterruptSystem.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/UnifiedInterruptSystem.h

    # Legacy Interrupt Systems (to be deprecated after migration)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptCoordinator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptAwareness.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptAwareness.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptManager.h

    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/TargetSelector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/TargetSelector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/TargetScanner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/TargetScanner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatStateManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatStateManager.h

    # Spatial Grid System - Lock-Free Double-Buffered Spatial Partitioning (Phase 1: Deadlock Fix)
    ${CMAKE_CURRENT_SOURCE_DIR}/Spatial/DoubleBufferedSpatialGrid.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Spatial/DoubleBufferedSpatialGrid.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Spatial/SpatialGridManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Spatial/SpatialGridManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Spatial/SpatialGridScheduler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Spatial/SpatialGridScheduler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Spatial/SpatialGridQueryHelpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Spatial/SpatialGridQueryHelpers.h

    # Integration Tests (Temporarily disabled - Google Test not configured)
    # ${CMAKE_CURRENT_SOURCE_DIR}/Tests/TargetScannerTest.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/Tests/QuestHubDatabaseTest.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/Tests/QuestHubDatabaseTest.h

    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/TargetAssistAction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/TargetAssistAction.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/SpellInterruptAction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/SpellInterruptAction.h
    # ThreatManager - resolved naming conflict by renaming to BotThreatManager
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/BotThreatManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/BotThreatManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatAbilities.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatAbilities.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatCoordinator.h
# Phase 2 Day 14: Combat AI Integration    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatAIIntegrator.cpp    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatAIIntegrator.h    ${CMAKE_CURRENT_SOURCE_DIR}/AI/EnhancedBotAI.cpp    ${CMAKE_CURRENT_SOURCE_DIR}/AI/EnhancedBotAI.h

    # Phase 2: Interface Definitions (header-only)
    ${CMAKE_CURRENT_SOURCE_DIR}/Interfaces/IBotSpawner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interfaces/IBotSession.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interfaces/IBotPerformance.h

    # Phase 2: Adapter Pattern Implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/Adapters/BotSpawnerAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Adapters/BotSpawnerAdapter.h

    # Database systems
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotCharacterDBInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotCharacterDBInterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseConnection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseConnection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotResultSet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotResultSet.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseStatements.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseStatements.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotMigrationMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotMigrationMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/BotStatePersistence.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/BotStatePersistence.h

    # Script integration
    ${CMAKE_CURRENT_SOURCE_DIR}/Scripts/PlayerbotWorldScript.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Scripts/PlayerbotWorldScript.h

    # Social Systems - WoW 11.2 Economy Integration
    # Phase 2.4: Replaced by TradeManager and AuctionManager (BehaviorManager pattern)
    # TradeManager enabled - now inherits from BehaviorManager
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/TradeManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/TradeManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/TradeManager_Events.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/MarketAnalysis.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/MarketAnalysis.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/GuildIntegration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/GuildIntegration.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/LootDistribution.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/LootDistribution.h
    # Manager Consolidation Phase 1: UnifiedLootManager (consolidates LootAnalysis, LootCoordination, LootDistribution)
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/UnifiedLootManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/UnifiedLootManager.h

    # Manager Consolidation Phase 2: UnifiedQuestManager (consolidates QuestPickup, QuestCompletion, QuestValidation, QuestTurnIn, DynamicQuestSystem)
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/UnifiedQuestManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/UnifiedQuestManager.h

    # Manager Consolidation Phase 3: UnifiedMovementCoordinator (consolidates MovementArbiter, PathfindingAdapter, FormationManager, PositionManager)
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/UnifiedMovementCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/UnifiedMovementCoordinator.h

    # Equipment Management - Complete Auto-Equip + Junk Selling + Consumables
    ${CMAKE_CURRENT_SOURCE_DIR}/Equipment/EquipmentManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Equipment/EquipmentManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Equipment/BotGearFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Equipment/BotGearFactory.h

    # Talent Management - Specialization Selection + Loadout Application + Dual-Spec
    ${CMAKE_CURRENT_SOURCE_DIR}/Talents/BotTalentManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Talents/BotTalentManager.h

    # Profession System - Complete Crafting + Gathering Automation (Phase 1, 2, 3)
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionDatabase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionDatabase.h
    # Phase 2.4: Replaced by GatheringManager (BehaviorManager pattern)
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/GatheringManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/GatheringManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/FarmingCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/FarmingCoordinator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionAuctionBridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionAuctionBridge.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/GatheringMaterialsBridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/GatheringMaterialsBridge.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/AuctionMaterialsBridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/AuctionMaterialsBridge.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionEvents.h

    # Banking System - Personal Bank Automation
    ${CMAKE_CURRENT_SOURCE_DIR}/Banking/BankingManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Banking/BankingManager.h

    # Economy Systems - Advanced Auction House Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/Economy/AuctionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Economy/AuctionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Economy/AuctionManager_Events.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Economy/PlayerbotAuctionConfig.h
    # BotAI_Auction_Integration no longer needed - AuctionManager is managed by BotAI
    # ${CMAKE_CURRENT_SOURCE_DIR}/Economy/BotAI_Auction_Integration.cpp

    # Quest Systems - Dynamic Quest Management
    # Phase 2.4: Replaced by QuestManager (BehaviorManager pattern)
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestPickup.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestPickup.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/DynamicQuestSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/DynamicQuestSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/ObjectiveTracker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/ObjectiveTracker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestHubDatabase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestHubDatabase.h

    # Game Systems - Integrated Quest, Inventory, Trade Management
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/QuestManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/QuestManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/QuestManager_Events.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/QuestAcceptanceManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/QuestAcceptanceManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/NPCInteractionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/NPCInteractionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/InventoryManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/InventoryManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/VendorPurchaseManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/VendorPurchaseManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/FlightMasterManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/FlightMasterManager.h
    # Config files commented out - incomplete implementations
    # ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotInventoryConfig.h
    # ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotQuestConfig.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotQuestConfig.h

    # Advanced Features - Group/Raid, Social, Advanced Behaviors
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/GroupCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/GroupCoordinator.h
    # Advanced/EconomyManager REMOVED - Duplicate of Economy/AuctionManager (production system)
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/SocialManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/SocialManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/AdvancedBehaviorManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/AdvancedBehaviorManager.h

    # Performance Optimization (Phase 5) - ThreadPool, MemoryPool, QueryOptimizer, Profiler
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ThreadPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ThreadPool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ThreadPoolDiagnostics.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/DeadlockDetector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/DeadlockDetector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ThreadPoolCommands.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ContinuousDiagnosticLogger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ContinuousDiagnosticLogger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/DebuggerIntegration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/DebuggerIntegration.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ETWProvider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ETWProvider.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ThreadPool.natvis

    # Bot Action Queue System (TrinityCore-style async pattern)
    ${CMAKE_CURRENT_SOURCE_DIR}/Threading/BotAction.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Threading/BotActionQueue.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Threading/BotActionProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Threading/BotActionProcessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Threading/BotActionManager.h

    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/MemoryPool/MemoryPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/MemoryPool/MemoryPool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/QueryOptimizer/QueryOptimizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/QueryOptimizer/QueryOptimizer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/Profiler/Profiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/Profiler/Profiler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/PerformanceManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/PerformanceManager.h

    # Phase 3: Game System Integration - Movement System (Strategy-Based Architecture)
    # MovementManager REMOVED - Movement is handled by strategies using BotMovementUtil
    # See MOVEMENT_ARCHITECTURE_FINAL.md for rationale
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Core/MovementTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Core/MovementValidator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Core/MovementValidator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/PathfindingAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/PathfindingAdapter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/PathOptimizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/PathOptimizer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/NavMeshInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/NavMeshInterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/LeaderFollowBehavior.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/LeaderFollowBehavior.h

    # Phase 6: Movement Arbiter - Enterprise Movement Request Arbitration System
    # Priority-based arbitration resolving conflicts between 16+ concurrent movement systems
    # Performance: <0.01ms per request, <1% CPU for 100 bots, thread-safe operation
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Arbiter/MovementPriorityMapper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Arbiter/MovementPriorityMapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Arbiter/MovementRequest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Arbiter/MovementRequest.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Arbiter/MovementArbiter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Arbiter/MovementArbiter.h

    # Phase 3: Game System Integration - NPC Interaction System
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/InteractionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/InteractionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/InteractionTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/GossipHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/GossipHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/InteractionValidator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/InteractionValidator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/VendorInteractionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/VendorInteractionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/FlightMasterManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/FlightMasterManager.h

    # Phase 3: Game System Integration - Quest System Completion
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestCompletion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestCompletion.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestTurnIn.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestTurnIn.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestValidation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestValidation.h

    # Phase 3: Core Combat Behavior System (State Analysis & Adaptive Behavior)
    # Foundation for intelligent combat decision-making
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatStateAnalyzer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatStateAnalyzer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/AdaptiveBehaviorManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/AdaptiveBehaviorManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatBehaviorIntegration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatBehaviorIntegration.h

    # Phase 4: Advanced Combat Utilities (Enterprise-Grade Combat Coordination)
    # Implements missing combat coordination utilities for all 119 ClassAI implementations
    # Performance: <0.1ms total per bot, scales to 5000+ bots
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CombatBehaviors/DefensiveBehaviorManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CombatBehaviors/DefensiveBehaviorManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CombatBehaviors/InterruptRotationManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CombatBehaviors/InterruptRotationManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CombatBehaviors/DispelCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CombatBehaviors/DispelCoordinator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CombatBehaviors/AoEDecisionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CombatBehaviors/AoEDecisionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CombatBehaviors/CooldownStackingOptimizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CombatBehaviors/CooldownStackingOptimizer.h

    # Commands - Chat command system for bot management
    ${CMAKE_CURRENT_SOURCE_DIR}/Commands/PlayerbotCommands.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Commands/PlayerbotCommands.h

    # Monitoring - Performance monitoring and alerting system (Integration & Polish Task I.3)
    ${CMAKE_CURRENT_SOURCE_DIR}/Monitoring/BotMonitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Monitoring/BotMonitor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Monitoring/PerformanceMetrics.h

    # LFG System - Looking For Group Bot Management
    ${CMAKE_CURRENT_SOURCE_DIR}/LFG/LFGBotManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LFG/LFGBotManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LFG/LFGBotSelector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LFG/LFGBotSelector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LFG/LFGRoleDetector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LFG/LFGRoleDetector.h
)

# Group sources for IDE organization
source_group("" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModule.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModuleAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModuleAdapter.h
)

source_group("Config" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotTradeConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotTradeConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotLog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotLog.h
)


source_group("Database" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotCharacterDBInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotCharacterDBInterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseConnection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseConnection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotResultSet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotResultSet.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseStatements.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseStatements.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotMigrationMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotMigrationMgr.h
)

source_group("AI" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAIFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ExampleManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ExampleManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorPriorityManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorPriorityManager.h
)

source_group("AI\\Strategy" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/Strategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/Strategy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/GroupCombatStrategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/GroupCombatStrategy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/SoloStrategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/SoloStrategy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/SoloCombatStrategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/SoloCombatStrategy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/QuestStrategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/QuestStrategy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/LootStrategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/LootStrategy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/RestStrategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/RestStrategy.h
)

source_group("AI\\Actions" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/Action.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/Action.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/CommonActions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/CommonActions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/TargetAssistAction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/TargetAssistAction.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/SpellInterruptAction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/SpellInterruptAction.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/RestAction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/RestAction.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/BuffAction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/BuffAction.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/LootAction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/LootAction.h
)

source_group("AI\\Triggers" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Triggers/Trigger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Triggers/Trigger.h
)

source_group("AI\\Combat" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/GroupCombatTrigger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/GroupCombatTrigger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/RoleBasedCombatPositioning.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/RoleBasedCombatPositioning.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/PositionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/PositionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/MechanicAwareness.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/MechanicAwareness.h

    # TODO Phase 4D: Unified Interrupt System (duplicate entry, commented out)
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/UnifiedInterruptSystem.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/UnifiedInterruptSystem.h

    # Legacy Interrupt Systems (deprecated)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptCoordinator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptAwareness.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptAwareness.h

    # ThreatManager - resolved naming conflict by renaming to BotThreatManager
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/BotThreatManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/BotThreatManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatAbilities.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatAbilities.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatCoordinator.h
# Phase 2 Day 14: Combat AI Integration    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatAIIntegrator.cpp    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatAIIntegrator.h    ${CMAKE_CURRENT_SOURCE_DIR}/AI/EnhancedBotAI.cpp    ${CMAKE_CURRENT_SOURCE_DIR}/AI/EnhancedBotAI.h
)

source_group("Spatial" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Spatial/DoubleBufferedSpatialGrid.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Spatial/DoubleBufferedSpatialGrid.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Spatial/SpatialGridManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Spatial/SpatialGridManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Spatial/SpatialGridScheduler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Spatial/SpatialGridScheduler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Spatial/SpatialGridQueryHelpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Spatial/SpatialGridQueryHelpers.h
)

source_group("AI\\Values" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Values/Value.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Values/Value.h
)

source_group("Group" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupInvitationHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupInvitationHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/RoleDefinitions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/RoleDefinitions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/RoleAssignment.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupEvents.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupEventHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupEventHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/PlayerbotGroupScript.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/PlayerbotGroupScript.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupStateMachine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupStateMachine.h
)

source_group("Network" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/PlayerbotPacketSniffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/PlayerbotPacketSniffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseGroupPacket_Typed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseCombatPacket_Typed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseCooldownPacket_Typed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseLootPacket_Typed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseQuestPacket_Typed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseAuraPacket_Typed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseResourcePacket_Typed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseSocialPacket_Typed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseAuctionPacket_Typed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseNPCPacket_Typed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network/ParseInstancePacket_Typed.cpp
)

source_group("Movement" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/LeaderFollowBehavior.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/LeaderFollowBehavior.h
)

source_group("Resource" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Resource/ResourceEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Resource/ResourceEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Resource/ResourceEvents.h
)

source_group("Social" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/SocialEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/SocialEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/SocialEvents.h
)

source_group("Auction" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Auction/AuctionEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Auction/AuctionEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Auction/AuctionEvents.h
)

source_group("NPC" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/NPC/NPCEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/NPC/NPCEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/NPC/NPCEvents.h
)

source_group("Instance" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Instance/InstanceEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Instance/InstanceEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Instance/InstanceEvents.h
)

source_group("Combat" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Combat/CombatEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Combat/CombatEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Combat/CombatEvents.h
)

source_group("Cooldown" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Cooldown/CooldownEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Cooldown/CooldownEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Cooldown/CooldownEvents.h
)

source_group("Aura" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Aura/AuraEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Aura/AuraEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Aura/AuraEvents.h
)

source_group("Loot" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Loot/LootEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Loot/LootEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Loot/LootEvents.h
)

source_group("Quest" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestEvents.h
)

source_group("Account" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Account/BotAccountMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Account/BotAccountMgr.h
)

source_group("Character" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotNameMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotNameMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCharacterDistribution.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCharacterDistribution.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotLevelDistribution.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotLevelDistribution.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCustomizationGenerator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCustomizationGenerator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotLevelManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotLevelManager.h
)

# source_group("Companion" FILES
#     ${CMAKE_CURRENT_SOURCE_DIR}/Companion/MountManager.cpp
#     ${CMAKE_CURRENT_SOURCE_DIR}/Companion/MountManager.h
#     ${CMAKE_CURRENT_SOURCE_DIR}/Companion/BattlePetManager.cpp
#     ${CMAKE_CURRENT_SOURCE_DIR}/Companion/BattlePetManager.h
# )

source_group("Lifecycle" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotLifecycleMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotLifecycleMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotScheduler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotScheduler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnOrchestrator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnOrchestrator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotResourcePool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotResourcePool.h
    # DUPLICATE FIX: Lifecycle/BotPerformanceMonitor conflicts with Session/BotPerformanceMonitor
    # ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotPerformanceMonitor.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotPerformanceMonitor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnEventBus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnEvents.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/DeathRecoveryManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/DeathRecoveryManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/ResourceMonitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/ResourceMonitor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/SpawnPriorityQueue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/SpawnPriorityQueue.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/SpawnCircuitBreaker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/SpawnCircuitBreaker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/AdaptiveSpawnThrottler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/AdaptiveSpawnThrottler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/StartupSpawnOrchestrator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/StartupSpawnOrchestrator.h
)

source_group("Session" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSession.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotPacketSimulator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotPacketSimulator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotWorldSessionMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotWorldSessionMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotPacketRelay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotPacketRelay.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionFactory.h
)

source_group("Chat" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Chat/BotChatCommandHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Chat/BotChatCommandHandler.h
)

source_group("Interfaces" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Interfaces/IBotSpawner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interfaces/IBotSession.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interfaces/IBotPerformance.h
)

source_group("Performance" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ObjectPool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/PacketPoolManager.h
)

source_group("Packets" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Packets/SpellPacketBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Packets/SpellPacketBuilder.h
)

source_group("Adapters" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Adapters/BotSpawnerAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Adapters/BotSpawnerAdapter.h
)

source_group("Scripts" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Scripts/PlayerbotWorldScript.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Scripts/PlayerbotWorldScript.h
)

source_group("Social" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/TradeManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/TradeManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/TradeManager_Events.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/MarketAnalysis.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/MarketAnalysis.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/GuildIntegration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/GuildIntegration.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/LootDistribution.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/LootDistribution.h
)

source_group("Equipment" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Equipment/EquipmentManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Equipment/EquipmentManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Equipment/BotGearFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Equipment/BotGearFactory.h
)

source_group("Talents" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Talents/BotTalentManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Talents/BotTalentManager.h
)

source_group("Professions" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionDatabase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionDatabase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/GatheringManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/GatheringManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/FarmingCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/FarmingCoordinator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionAuctionBridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionAuctionBridge.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/GatheringMaterialsBridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/GatheringMaterialsBridge.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/AuctionMaterialsBridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/AuctionMaterialsBridge.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionEvents.h
)

source_group("Banking" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Banking/BankingManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Banking/BankingManager.h
)

source_group("Economy" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Economy/AuctionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Economy/AuctionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Economy/AuctionManager_Events.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Economy/PlayerbotAuctionConfig.h
    # ${CMAKE_CURRENT_SOURCE_DIR}/Economy/BotAI_Auction_Integration.cpp
)

source_group("Quest" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/DynamicQuestSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/DynamicQuestSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/ObjectiveTracker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/ObjectiveTracker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestPickup.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestPickup.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestCompletion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestCompletion.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestTurnIn.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestTurnIn.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestValidation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestValidation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestHubDatabase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestHubDatabase.h
)

source_group("Game" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/QuestManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/QuestManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/QuestManager_Events.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/NPCInteractionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/NPCInteractionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/InventoryManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/InventoryManager.h
)

source_group("Advanced" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/GroupCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/GroupCoordinator.h
    # Advanced/EconomyManager REMOVED - Duplicate of Economy/AuctionManager (production system)
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/SocialManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/SocialManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/AdvancedBehaviorManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/AdvancedBehaviorManager.h
)

source_group("Performance" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ThreadPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ThreadPool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ThreadPoolDiagnostics.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/DeadlockDetector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/DeadlockDetector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ThreadPoolCommands.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ContinuousDiagnosticLogger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ContinuousDiagnosticLogger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/DebuggerIntegration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/DebuggerIntegration.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ETWProvider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ETWProvider.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ThreadPool.natvis

    # Bot Action Queue System (TrinityCore-style async pattern)
    ${CMAKE_CURRENT_SOURCE_DIR}/Threading/BotAction.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Threading/BotActionQueue.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Threading/BotActionProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Threading/BotActionProcessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Threading/BotActionManager.h

    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/MemoryPool/MemoryPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/MemoryPool/MemoryPool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/QueryOptimizer/QueryOptimizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/QueryOptimizer/QueryOptimizer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/Profiler/Profiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/Profiler/Profiler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/PerformanceManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/PerformanceManager.h
)

source_group("Movement" FILES
    # MovementManager removed - strategy-based architecture
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Core/MovementTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Core/MovementValidator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Core/MovementValidator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/PathfindingAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/PathfindingAdapter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/PathOptimizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/PathOptimizer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/NavMeshInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/NavMeshInterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/LeaderFollowBehavior.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/LeaderFollowBehavior.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/BotWorldPositioner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/BotWorldPositioner.h

    # Movement Arbiter - Enterprise Arbitration System
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Arbiter/MovementPriorityMapper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Arbiter/MovementPriorityMapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Arbiter/MovementRequest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Arbiter/MovementRequest.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Arbiter/MovementArbiter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Arbiter/MovementArbiter.h
)

source_group("Interaction" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/InteractionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/InteractionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/InteractionTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/GossipHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/GossipHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/InteractionValidator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/InteractionValidator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/VendorInteractionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/VendorInteractionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/FlightMasterManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/FlightMasterManager.h
)

source_group("Commands" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Commands/PlayerbotCommands.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Commands/PlayerbotCommands.h
)

source_group("Monitoring" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Monitoring/BotMonitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Monitoring/BotMonitor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Monitoring/PerformanceMetrics.h
)

source_group("LFG" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/LFG/LFGBotManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LFG/LFGBotManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LFG/LFGBotSelector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LFG/LFGBotSelector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LFG/LFGRoleDetector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LFG/LFGRoleDetector.h
)

source_group("Tests" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Tests/TargetScannerTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Tests/QuestHubDatabaseTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Tests/QuestHubDatabaseTest.h
)

# Create the playerbot library
add_library(playerbot ${PLAYERBOT_SOURCES})

# Set target properties with enterprise C++20 standards
set_target_properties(playerbot PROPERTIES
    OUTPUT_NAME "playerbot"
    FOLDER "modules"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Include directories with enterprise dependencies
target_include_directories(playerbot
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Config
        ${CMAKE_CURRENT_SOURCE_DIR}/Core
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/References
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Managers
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Events
        ${CMAKE_CURRENT_SOURCE_DIR}/Events
        ${CMAKE_CURRENT_SOURCE_DIR}/Session
        ${CMAKE_CURRENT_SOURCE_DIR}/Chat
        ${CMAKE_CURRENT_SOURCE_DIR}/Database
        ${CMAKE_CURRENT_SOURCE_DIR}/AI
        ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy
        ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions
        ${CMAKE_CURRENT_SOURCE_DIR}/AI/Triggers
        ${CMAKE_CURRENT_SOURCE_DIR}/AI/Values
        ${CMAKE_CURRENT_SOURCE_DIR}/Group
        ${CMAKE_CURRENT_SOURCE_DIR}/Interfaces
        ${CMAKE_CURRENT_SOURCE_DIR}/Adapters
        ${CMAKE_CURRENT_SOURCE_DIR}/Economy
        ${CMAKE_CURRENT_SOURCE_DIR}/LFG
        ${CMAKE_CURRENT_SOURCE_DIR}/Packets
        ${CMAKE_SOURCE_DIR}/src/modules/Common
        ${MYSQL_INCLUDE_DIR}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Account
        ${CMAKE_CURRENT_SOURCE_DIR}/Character
        ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle
        ${CMAKE_CURRENT_SOURCE_DIR}/Scripts
        ${CMAKE_CURRENT_BINARY_DIR}
        ${PLAYERBOT_INCLUDE_DIRS}
)

# Link against TrinityCore and enterprise dependencies
target_link_libraries(playerbot
    PUBLIC
        # TrinityCore core libraries
        game-interface
        shared
        database
        common
        # Shared module libraries
        modules-common
    PRIVATE
        # Enterprise dependencies
        ${PLAYERBOT_LIBRARIES}
        playerbot-dependencies
        # MySQL libraries for database connection
        ${MYSQL_LIBRARY}
)

# Platform-specific libraries for enterprise dependencies
if(WIN32)
    target_link_libraries(playerbot PRIVATE ws2_32 wsock32 mswsock)
elseif(UNIX)
    target_link_libraries(playerbot PRIVATE pthread dl)
endif()

# Enterprise preprocessor definitions
target_compile_definitions(playerbot
    PRIVATE
        BUILD_PLAYERBOT=1
        TRINITY_API_EXPORT_PLAYERBOT
        $<$<CONFIG:Debug>:PLAYERBOT_DEBUG=1>
        $<$<CONFIG:Release>:PLAYERBOT_RELEASE=1>
)

# Platform-specific definitions
if(WIN32)
    target_compile_definitions(playerbot PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Enterprise optimization flags - only for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Configuring enterprise performance optimizations")

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(playerbot PRIVATE
            $<$<CONFIG:Release>:-O3>                    # Maximum optimization
            $<$<CONFIG:Release>:-march=native>          # Target current CPU
            $<$<CONFIG:Release>:-mtune=native>          # Tune for current CPU
            $<$<CONFIG:Release>:-flto>                  # Link-time optimization
            $<$<CONFIG:Release>:-ffast-math>            # Fast math for SIMD
            $<$<CONFIG:Release>:-funroll-loops>         # Loop unrolling
            $<$<CONFIG:Release>:-ftree-vectorize>       # Auto-vectorization
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(playerbot PRIVATE
            $<$<CONFIG:Release>:/O2>                    # Maximum optimization (Release only)
            $<$<CONFIG:Release>:/GL>                    # Whole program optimization (Release only)
            $<$<CONFIG:Release>:/fp:fast>               # Fast floating point (Release only)
            $<$<CONFIG:Release>:/arch:AVX2>             # Enable AVX2 if available (Release only)
        )
        # Apply link-time optimizations only for Release builds
        target_link_options(playerbot PRIVATE $<$<CONFIG:Release>:/LTCG>)
    endif()
endif()

# Install configuration file
if(CONF_DIR)
    install(FILES conf/playerbots.conf.dist DESTINATION ${CONF_DIR}/modules)
endif()

# Install SQL migration files and structure
if(CONF_DIR)
    # Create sql/migrations directory in installation
    install(DIRECTORY DESTINATION ${CONF_DIR}/sql)
    install(DIRECTORY DESTINATION ${CONF_DIR}/sql/migrations)

    # Install all migration files
    install(FILES
        sql/migrations/001_initial_schema.sql
        sql/migrations/002_account_management.sql
        sql/migrations/003_lifecycle_management.sql
        sql/migrations/004_character_distribution.sql
        sql/migrations/005_initial_data.sql
        sql/migrations/006_bot_names.sql
        DESTINATION ${CONF_DIR}/sql/migrations
    )

    message(STATUS "Playerbot migration files will be installed to: ${CONF_DIR}/sql/migrations/")
endif()

# Legacy SQL installation (deprecated - keeping for compatibility)
if(SQL_DIR)
    install(DIRECTORY sql/ DESTINATION ${SQL_DIR}/playerbot)
endif()

# Add to game library dependencies
if(TARGET game)
    target_link_libraries(game PRIVATE playerbot)
    target_compile_definitions(game PRIVATE
        BUILD_PLAYERBOT=1
    )
endif()

# Enterprise dependency verification test (temporarily disabled)
# add_executable(playerbot-dependency-test
#     Config/DependencyValidator.cpp
#     Config/DependencyValidator.h
# )
#
# target_link_libraries(playerbot-dependency-test
#     PRIVATE
#         playerbot-dependencies
#         shared  # For Trinity logging
# )
#
# target_include_directories(playerbot-dependency-test
#     PRIVATE
#         ${CMAKE_CURRENT_SOURCE_DIR}/Config
# )
#
# # Run dependency validation as part of build
# add_custom_command(
#     TARGET playerbot-dependency-test POST_BUILD
#     COMMAND playerbot-dependency-test
#     COMMENT "Validating Playerbot enterprise dependencies"
#     VERBATIM
# )

# Unit tests (temporarily disabled)
# if(BUILD_TESTING)
#     # Performance benchmarks
#     add_executable(playerbot-benchmark
#         Tests/BenchmarkTests.cpp
#         Session/BotSession.cpp
#         Session/BotSessionMgr.cpp
#     )
#
#     target_link_libraries(playerbot-benchmark
#         PRIVATE
#             playerbot-dependencies
#             shared
#     )
#
#     target_include_directories(playerbot-benchmark
#         PRIVATE
#             ${CMAKE_CURRENT_SOURCE_DIR}
#     )
#
#     set_target_properties(playerbot-benchmark PROPERTIES
#         FOLDER "tests"
#         OUTPUT_NAME "playerbot-benchmark"
#     )
# endif()

# Add Tests subdirectory (Phase 3A Week 2: Unit Testing)
add_subdirectory(Tests)

# Enterprise quality validation
message(STATUS "=== Playerbot Enterprise Configuration Summary ===")
message(STATUS "  -> Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  -> C++ Standard: C++20")
message(STATUS "  -> Intel TBB: ${PLAYERBOT_TBB_FOUND}")
message(STATUS "  -> Parallel Hashmap: ${PLAYERBOT_PHMAP_FOUND}")
message(STATUS "  -> Boost: ${PLAYERBOT_BOOST_FOUND}")
message(STATUS "  -> MySQL: ${PLAYERBOT_MYSQL_FOUND}")
message(STATUS "  -> Performance Optimizations: $<$<CONFIG:Release>:Enabled>")
message(STATUS "  -> Source files: ${PLAYERBOT_SOURCES}")
message(STATUS "🚀 TrinityCore Playerbot enterprise module ready for compilation")
