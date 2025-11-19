/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifdef BUILD_PLAYERBOT

#include "PlayerbotModule.h"
#include "Config/PlayerbotConfig.h"
#include "Account/BotAccountMgr.h"
#include "Character/BotNameMgr.h"
#include "Character/BotCharacterDistribution.h"
#include "Database/PlayerbotDatabase.h"
#include "Database/PlayerbotCharacterDBInterface.h"
#include "Database/PlayerbotMigrationMgr.h"
#include "Lifecycle/BotSpawner.h"
// #include "Lifecycle/BotLifecycleMgr.h"
#include "Session/BotSessionMgr.h"
#include "Session/BotWorldSessionMgr.h"
#include "Session/BotPacketRelay.h"
#include "Chat/BotChatCommandHandler.h"
#include "Professions/ProfessionManager.h"
#include "Professions/ProfessionAuctionBridge.h"
#include "Professions/GatheringMaterialsBridge.h"
#include "Professions/AuctionMaterialsBridge.h"
#include "Banking/BankingManager.h"
#include "Quest/QuestHubDatabase.h"
#include "Equipment/BotGearFactory.h"
#include "AI/ClassAI/ClassBehaviorTreeRegistry.h"
#include "PlayerbotModuleAdapter.h"
#include "Update/ModuleUpdateManager.h"
#include "Group/GroupEventBus.h"
#include "Network/PlayerbotPacketSniffer.h"
#include "Threading/BotActionManager.h"
#include "Log.h"
#include "GitRevision.h"
#include <chrono>

// Module state - using inline static in header for DLL compatibility

bool PlayerbotModule::Initialize()
{
    TC_LOG_INFO("server.loading", "Initializing Playerbot Module...");

    // Load configuration first
    if (!sPlayerbotConfig->Initialize())
    {
        _lastError = "Failed to load playerbot configuration";
        TC_LOG_ERROR("server.loading", "Playerbot Module: {}", _lastError);
        return false;
    }

    // Check if playerbot is enabled
    if (!sPlayerbotConfig->GetBool("Playerbot.Enable", false))
    {
        TC_LOG_INFO("server.loading", "Playerbot Module: Disabled in configuration");
        _initialized = true;
        _enabled = false;
        return true;
    }

    // Validate configuration
    if (!ValidateConfig())
    {
        _lastError = "Configuration validation failed";
        TC_LOG_ERROR("server.loading", "Playerbot Module: {}", _lastError);
        return false;
    }

    // Initialize logging
    InitializeLogging();

    // Initialize Playerbot Database
    if (!InitializeDatabase())
    {
        _lastError = "Failed to initialize Playerbot Database";
        return false;
    }

    // Initialize Character Database Interface
    if (!sPlayerbotCharDB->Initialize())
    {
        _lastError = "Failed to initialize Character Database Interface";
        TC_LOG_ERROR("server.loading", "Playerbot Module: {}", _lastError);
        return false;
    }

    // Initialize Migration Manager FIRST (before any database access)
    if (!PlayerbotMigrationMgr::instance()->Initialize())
    {
        _lastError = "Failed to initialize Migration Manager";
        return false;
    }

    // Apply pending migrations
    if (!PlayerbotMigrationMgr::instance()->ApplyMigrations())
    {
        _lastError = "Failed to apply database migrations";
        return false;
    }

    // Initialize Bot Account Manager
    TC_LOG_INFO("server.loading", "PlayerbotModule: About to call sBotAccountMgr->Initialize()...");
    if (!sBotAccountMgr->Initialize())
    {
        _lastError = "Failed to initialize Bot Account Manager";
        TC_LOG_ERROR("server.loading", "PlayerbotModule: BotAccountMgr->Initialize() returned FALSE");
        // Check if strict mode is enabled
        bool strictMode = sPlayerbotConfig->GetBool("Playerbot.StrictCharacterLimit", true);
        TC_LOG_ERROR("server.loading", "PlayerbotModule: StrictCharacterLimit = {}", strictMode);
        if (strictMode)
        {
            TC_LOG_ERROR("server.loading", "PlayerbotModule: ABORTING initialization due to strict mode");
            return false;
        }
        else
        {
            TC_LOG_WARN("server.loading", "PlayerbotModule: Continuing despite BotAccountMgr failure (strict mode disabled)");
        }
    }
    else
    {
        TC_LOG_INFO("server.loading", "PlayerbotModule: sBotAccountMgr->Initialize() returned TRUE");
    }

    // Initialize Bot Name Manager
    if (!sBotNameMgr->Initialize())
    {
        _lastError = "Failed to initialize Bot Name Manager";
        return false;
    }

    // Initialize Bot Character Distribution
    if (!sBotCharacterDistribution->LoadFromDatabase())
    {
        _lastError = "Failed to initialize Bot Character Distribution";
        return false;
    }

    // Initialize Bot Session Manager
    if (!sBotSessionMgr->Initialize())
    {
        _lastError = "Failed to initialize Bot Session Manager";
        return false;
    }

    // Initialize Bot World Session Manager (native TrinityCore login)
    if (!Playerbot::sBotWorldSessionMgr->Initialize())
    {
        _lastError = "Failed to initialize Bot World Session Manager";
        return false;
    }

    // Initialize Bot Packet Relay (Phase 1-3: Group packet relay for combat logs and chat)
    TC_LOG_INFO("server.loading", "Initializing Bot Packet Relay...");
    Playerbot::BotPacketRelay::Initialize();
    TC_LOG_INFO("server.loading", "Bot Packet Relay initialized successfully");

    // Initialize Bot Chat Command Handler (Phase 4: Command processing system)
    TC_LOG_INFO("server.loading", "Initializing Bot Chat Command Handler...");
    Playerbot::BotChatCommandHandler::Initialize();
    TC_LOG_INFO("server.loading", "Bot Chat Command Handler initialized successfully");

    // NOTE: Bot Spawner initialization is handled by PlayerbotModuleAdapter::OnModuleStartup()
    // This ensures proper timing - BotSpawner needs the world to be fully loaded, which happens
    // AFTER this Initialize() function completes. The ModuleManager calls OnModuleStartup()
    // at the correct time, and it will initialize both BotAccountMgr and BotSpawner.

    // Initialize Bot Lifecycle Manager
    //     TC_LOG_INFO("server.loading", "Initializing Bot Lifecycle Manager...");
    //     if (!BotLifecycleMgr::instance()->Initialize())
    //     {
    //         _lastError = "Failed to initialize Bot Lifecycle Manager";
    //         TC_LOG_ERROR("server.loading", "Playerbot Module: {}", _lastError);
    //         return false;
    //     }
    //
    // Initialize Profession Manager (must happen before bots are created)
    TC_LOG_INFO("server.loading", "Initializing Profession Manager...");
    Playerbot::ProfessionManager::instance()->Initialize();
    TC_LOG_INFO("server.loading", "Profession Manager initialized successfully");

    // Initialize Profession Bridges (Phase 3 Option C: Economic integration)
    TC_LOG_INFO("server.loading", "Initializing Profession-Auction Bridge...");
    Playerbot::ProfessionAuctionBridge::instance()->Initialize();
    TC_LOG_INFO("server.loading", "Profession-Auction Bridge initialized successfully");

    TC_LOG_INFO("server.loading", "Initializing Gathering-Materials Bridge...");
    Playerbot::GatheringMaterialsBridge::instance()->Initialize();
    TC_LOG_INFO("server.loading", "Gathering-Materials Bridge initialized successfully");

    TC_LOG_INFO("server.loading", "Initializing Auction-Materials Bridge...");
    Playerbot::AuctionMaterialsBridge::instance()->Initialize();
    TC_LOG_INFO("server.loading", "Auction-Materials Bridge initialized successfully");

    TC_LOG_INFO("server.loading", "Initializing Banking Manager...");
    Playerbot::BankingManager::instance()->OnInitialize();
    TC_LOG_INFO("server.loading", "Banking Manager initialized successfully");

    TC_LOG_INFO("server.loading", "Initializing Profession Event Bus...");
    // ProfessionEventBus is event-driven, no initialization required (lazy init)
    TC_LOG_INFO("server.loading", "Profession Event Bus ready");

    // Initialize Class Behavior Tree Registry (Phase 5: Class-specific AI trees for all 13 classes)
    TC_LOG_INFO("server.loading", "Initializing Class Behavior Tree Registry...");
    Playerbot::ClassBehaviorTreeRegistry::Initialize();
    TC_LOG_INFO("server.loading", "Class Behavior Tree Registry initialized successfully - {} class/spec trees registered",
        39); // 13 classes × 3 specs = 39 combinations

    // Initialize Quest Hub Database (spatial clustering of quest givers for efficient pathfinding)
    TC_LOG_INFO("server.loading", "Initializing Quest Hub Database...");
    if (!Playerbot::QuestHubDatabase::Instance().Initialize())
    {
        _lastError = "Failed to initialize Quest Hub Database";
        TC_LOG_ERROR("server.loading", "Playerbot Module: {}", _lastError);
        return false;
    }
    TC_LOG_INFO("server.loading", "Quest Hub Database initialized successfully - {} quest hubs loaded",
        Playerbot::QuestHubDatabase::Instance().GetQuestHubCount());

    // Initialize Bot Gear Factory (automated gear generation for instant level-up)
    TC_LOG_INFO("server.loading", "Initializing Bot Gear Factory...");
    Playerbot::sBotGearFactory->Initialize();
    TC_LOG_INFO("server.loading", "Bot Gear Factory initialized successfully");

    // Initialize Packet Sniffer (centralized event detection system)
    TC_LOG_INFO("server.loading", "Initializing Packet Sniffer...");
    Playerbot::PlayerbotPacketSniffer::Initialize();
    TC_LOG_INFO("server.loading", "Packet Sniffer initialized successfully");

    // Register hooks with TrinityCore
    RegisterHooks();

    // Initialize Bot Action Manager for worker thread → main thread action queue
    TC_LOG_INFO("server.loading", "Initializing Bot Action Manager...");
    sBotActionMgr->Initialize();
    TC_LOG_INFO("server.loading", "Bot Action Manager initialized successfully");

    // Register with the shared ModuleUpdateManager for world updates
    if (!sModuleUpdateManager->RegisterModule("playerbot", [](uint32 diff) { OnWorldUpdate(diff); }))
    {
        _lastError = "Failed to register with ModuleUpdateManager";
        TC_LOG_ERROR("server.loading", "Playerbot Module: ModuleUpdateManager registration failed");
        return false;
    }
    TC_LOG_INFO("server.loading", "Playerbot Module: Successfully registered with ModuleUpdateManager");

    _initialized = true;
    _enabled = true;


    return true;
}

void PlayerbotModule::Shutdown()
{
    if (!_initialized)
        return;

    TC_LOG_INFO("server.loading", "Shutting down Playerbot Module...");

    // Shutdown Bot Action Manager
    sBotActionMgr->Shutdown();
    TC_LOG_INFO("server.loading", "Bot Action Manager shutdown complete");

    // Unregister hooks
    UnregisterHooks();

    // Unregister from ModuleUpdateManager
    sModuleUpdateManager->UnregisterModule("playerbot");
    TC_LOG_DEBUG("server.loading", "Unregistered playerbot from ModuleUpdateManager");

    if (_enabled)
    {
        // Shutdown Bot Lifecycle Manager
        //         TC_LOG_INFO("server.loading", "Shutting down Bot Lifecycle Manager...");
        //         BotLifecycleMgr::instance()->Shutdown();
        //
        // Shutdown Bot Chat Command Handler
        TC_LOG_INFO("server.loading", "Shutting down Bot Chat Command Handler...");
        Playerbot::BotChatCommandHandler::Shutdown();

        // Shutdown Bot Packet Relay
        TC_LOG_INFO("server.loading", "Shutting down Bot Packet Relay...");
        Playerbot::BotPacketRelay::Shutdown();

        // Shutdown Packet Sniffer
        TC_LOG_INFO("server.loading", "Shutting down Packet Sniffer...");
        Playerbot::PlayerbotPacketSniffer::Shutdown();

        // Shutdown Bot World Session Manager
        TC_LOG_INFO("server.loading", "Shutting down Bot World Session Manager...");
        Playerbot::sBotWorldSessionMgr->Shutdown();

        // Shutdown Bot Name Manager
        TC_LOG_INFO("server.loading", "Shutting down Bot Name Manager...");
        sBotNameMgr->Shutdown();

        // Shutdown Bot Account Manager
        TC_LOG_INFO("server.loading", "Shutting down Bot Account Manager...");
        sBotAccountMgr->Shutdown();

        // Shutdown Playerbot Database first to stop all operations
        TC_LOG_INFO("server.loading", "Shutting down Playerbot Database...");
        ShutdownDatabase();

        // Then shutdown Character Database Interface after operations complete
        TC_LOG_INFO("server.loading", "Shutting down Character Database Interface...");
        sPlayerbotCharDB->Shutdown();
    }

    // Mark as shutdown
    _initialized = false;
    _enabled = false;

    TC_LOG_INFO("server.loading", "Playerbot Module: Shutdown complete");
}

bool PlayerbotModule::IsEnabled()
{
    return _initialized && _enabled;
}

std::string PlayerbotModule::GetVersion()
{
    return Trinity::StringFormat("{}.{}.{}", MODULE_VERSION_MAJOR, MODULE_VERSION_MINOR, MODULE_VERSION_PATCH);
}

std::string PlayerbotModule::GetBuildInfo()
{
    return Trinity::StringFormat("Playerbot Module {} (Built: {} {})",
        GetVersion(), __DATE__, __TIME__);
}

void PlayerbotModule::RegisterHooks()
{
    // Register with ModuleManager for reliable lifecycle management
    Playerbot::PlayerbotModuleAdapter::RegisterWithModuleManager();
}

void PlayerbotModule::UnregisterHooks()
{
    // TODO: Unregister event hooks
}

void PlayerbotModule::OnWorldUpdate(uint32 diff)
{
    if (!_enabled || !_initialized)
        return;

    // CRITICAL SAFETY: Wrap entire update in try-catch to prevent crashes
    try
    {

    // One-time trigger to complete login for existing sessions
    static bool loginTriggered = false;
    static uint32 totalTime = 0;
    totalTime += diff;

    if (!loginTriggered && totalTime > 5000) // Wait 5 seconds after startup
    {
        TC_LOG_INFO("module.playerbot", "🔄 OnWorldUpdate: Auto-triggering character logins for existing sessions");
        TriggerBotCharacterLogins();
        loginTriggered = true;
    }

    // PERFORMANCE PROFILING: Track time for each manager
    auto timeStart = std::chrono::high_resolution_clock::now();
    auto lastTime = timeStart;

    // Update BotAccountMgr for thread-safe callback processing
    sBotAccountMgr->Update(diff);
    auto t1 = std::chrono::high_resolution_clock::now();
    auto accountTime = std::chrono::duration_cast<std::chrono::microseconds>(t1 - lastTime).count();
    lastTime = t1;

    // Update BotSpawner for automatic character creation and management
    Playerbot::sBotSpawner->Update(diff);
    auto t2 = std::chrono::high_resolution_clock::now();
    auto spawnerTime = std::chrono::duration_cast<std::chrono::microseconds>(t2 - lastTime).count();
    lastTime = t2;

    // Update BotSessionMgr for active bot session processing (legacy)
    sBotSessionMgr->UpdateAllSessions(diff);
    auto t3 = std::chrono::high_resolution_clock::now();
    auto sessionMgrTime = std::chrono::duration_cast<std::chrono::microseconds>(t3 - lastTime).count();
    lastTime = t3;

    // Update BotWorldSessionMgr for native TrinityCore login sessions
    Playerbot::sBotWorldSessionMgr->UpdateSessions(diff);
    auto t4 = std::chrono::high_resolution_clock::now();
    auto worldSessionTime = std::chrono::duration_cast<std::chrono::microseconds>(t4 - lastTime).count();
    lastTime = t4;

    // Update PlayerbotCharacterDBInterface to process sync queue
    sPlayerbotCharDB->Update(diff);
    auto t5 = std::chrono::high_resolution_clock::now();
    auto charDBTime = std::chrono::duration_cast<std::chrono::microseconds>(t5 - lastTime).count();
    lastTime = t5;

    // Update GroupEventBus to process pending group events
    Playerbot::GroupEventBus::instance()->ProcessEvents(diff);
    auto t6 = std::chrono::high_resolution_clock::now();
    auto groupEventTime = std::chrono::duration_cast<std::chrono::microseconds>(t6 - lastTime).count();

    // Calculate total time
    auto totalUpdateTime = std::chrono::duration_cast<std::chrono::microseconds>(t6 - timeStart).count();

    // Log if total time exceeds 100ms
    if (totalUpdateTime > 100000) // 100ms in microseconds
    {
        TC_LOG_WARN("module.playerbot.performance", "PERFORMANCE: OnWorldUpdate took {:.2f}ms - Account:{:.2f}ms, Spawner:{:.2f}ms, SessionMgr:{:.2f}ms, WorldSession:{:.2f}ms, CharDB:{:.2f}ms, GroupEvent:{:.2f}ms",
            totalUpdateTime / 1000.0f,
            accountTime / 1000.0f,
            spawnerTime / 1000.0f,
            sessionMgrTime / 1000.0f,
            worldSessionTime / 1000.0f,
            charDBTime / 1000.0f,
            groupEventTime / 1000.0f);
    }

    }
    catch (std::exception const& ex)
    {
        TC_LOG_ERROR("module.playerbot", "CRITICAL EXCEPTION in PlayerbotModule::OnWorldUpdate: {}", ex.what());
        TC_LOG_ERROR("module.playerbot", "Disabling playerbot to prevent further crashes");
        _enabled = false;
    }
    catch (...)
    {
        TC_LOG_ERROR("module.playerbot", "CRITICAL UNKNOWN EXCEPTION in PlayerbotModule::OnWorldUpdate");
        TC_LOG_ERROR("module.playerbot", "Disabling playerbot to prevent further crashes");
        _enabled = false;
    }
}

bool PlayerbotModule::ValidateConfig()
{
    // Basic configuration validation
    if (!sPlayerbotConfig)
    {
        _lastError = "Configuration system not initialized";
        return false;
    }

    // Validate bot count limits
    uint32 maxBots = sPlayerbotConfig->GetInt("Playerbot.MaxBotsPerAccount", 10);
    if (maxBots < 1 || maxBots > 50)
    {
        _lastError = Trinity::StringFormat("Playerbot.MaxBotsPerAccount invalid ({}), must be between 1-50", maxBots);
        TC_LOG_WARN("server.loading", "Playerbot Module: {}", _lastError);
        return false;
    }

    // Validate character limit per account - WICHTIG: MAX 10!
    uint32 maxChars = sPlayerbotConfig->GetInt("Playerbot.MaxCharactersPerAccount", 10);
    if (maxChars < 1 || maxChars > 10)
    {
        _lastError = Trinity::StringFormat("Playerbot.MaxCharactersPerAccount invalid ({}), must be between 1-10", maxChars);
        TC_LOG_WARN("server.loading", "Playerbot Module: {}", _lastError);
        return false;
    }

    // Validate update intervals
    uint32 updateMs = sPlayerbotConfig->GetInt("Playerbot.UpdateInterval", 1000);
    if (updateMs < 100)
    {
        _lastError = "Playerbot.UpdateInterval too low (minimum 100ms)";
        TC_LOG_WARN("server.loading", "Playerbot Module: {}", _lastError);
        return false;
    }

    return true;
}

void PlayerbotModule::InitializeLogging()
{
    // Check if sPlayerbotConfig is valid
    if (!sPlayerbotConfig)
    {
        return;
    }

    sPlayerbotConfig->InitializeLogging();
}

bool PlayerbotModule::InitializeDatabase()
{

    // Get individual database settings from configuration
    std::string host = sPlayerbotConfig->GetString("Playerbot.Database.Host", "127.0.0.1");
    uint32 port = sPlayerbotConfig->GetInt("Playerbot.Database.Port", 3306);
    std::string user = sPlayerbotConfig->GetString("Playerbot.Database.User", "trinity");
    std::string password = sPlayerbotConfig->GetString("Playerbot.Database.Password", "trinity");
    std::string database = sPlayerbotConfig->GetString("Playerbot.Database.Name", "characters");


    // Construct connection string in format: hostname;port;username;password;database
    std::string dbString = Trinity::StringFormat("{};{};{};{};{}", host, port, user, password, database);


    TC_LOG_INFO("server.loading", "Playerbot Database: Connecting to {}:{}/{}", host, port, database);


    // Initialize database connection using our custom manager
    if (!sPlayerbotDatabase->Initialize(dbString))
    {
        TC_LOG_ERROR("server.loading", "Playerbot Database: Failed to initialize connection");
        return false;
    }


    TC_LOG_INFO("server.loading", "Playerbot Database: Successfully connected");

    // Validate database schema
    TC_LOG_INFO("server.loading", "Validating Playerbot Database Schema...");
    if (!sPlayerbotDatabase->ValidateSchema())
    {
        TC_LOG_WARN("server.loading", "Playerbot Database: Schema validation failed - some features may not work correctly");
        TC_LOG_WARN("server.loading", "Consider running database migrations or checking table structures");
    }

    return true;
}

void PlayerbotModule::TriggerBotCharacterLogins()
{
    if (!_enabled || !_initialized)
    {
        TC_LOG_WARN("module.playerbot", "TriggerBotCharacterLogins: Module not enabled or initialized");
        return;
    }

    TC_LOG_INFO("module.playerbot", "🚀 TriggerBotCharacterLogins: Manually triggering character logins for existing sessions");

    // Call the BotSessionMgr method to trigger logins (legacy approach)
    sBotSessionMgr->TriggerCharacterLoginForAllSessions();

    // Call the BotWorldSessionMgr method to trigger native logins
    Playerbot::sBotWorldSessionMgr->TriggerCharacterLoginForAllSessions();

    TC_LOG_INFO("module.playerbot", "🚀 TriggerBotCharacterLogins: Complete");
}

void PlayerbotModule::ShutdownDatabase()
{
    TC_LOG_DEBUG("module.playerbot", "Closing Playerbot Database connection");
    sPlayerbotDatabase->Close();
    TC_LOG_DEBUG("module.playerbot", "Playerbot Database connection closed");
}

#endif // BUILD_PLAYERBOT
