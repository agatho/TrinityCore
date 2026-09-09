// Services - Static accessors to V2's long-lived singletons. Initialized in
// PlayerbotV2::Module::Init, torn down in Shutdown. CONTRACTS.md §10.
//
// Not a DI framework. Tests can swap implementations by re-init.

#pragma once

#include "Bot/BotTypes.h"

namespace Playerbot {

class SnapshotPublisher;
class IntentQueue;
class AiWorkerPool;
class FleetThread;
class TickScheduler;
class BotRegistry;
class BotIdentityRegistry;
class OwnerRegistry;
class DungeonScriptMgr;
class BattlegroundScriptMgr;
class BgTeamCoordinator;
class PveGroupCoordinator;
class ConfigReader;
class PerfCounters;
class IdleRuleRegistry;

namespace V2 { class BotSession; class BotSessionMgr; class BotAccountMgr; class BotGuildMgr; class BotGuildNamePool; class BotCoordinationBus; class BotGroupBuilder; class CraftOrderBoard; }
namespace V2::Travel { class QuestHubDatabase; class PortalIndex; class UnifiedTravelGraph; class RepairVendorIndex; }
namespace V2::Fleet { class BotPopulationManager; }

namespace Services {

// Lifetime
void Init();
void Shutdown();
bool Initialized();

// Accessors — null until Init().
SnapshotPublisher& Snapshots();
AiWorkerPool&      AiPool();
FleetThread&       Fleet();
TickScheduler&     Scheduler();
BotRegistry&       Registry();
BotIdentityRegistry& Lifecycle();
OwnerRegistry&       Owners();
DungeonScriptMgr&    Dungeons();
BattlegroundScriptMgr& Battlegrounds();
// Team-level BG strategy service (BG audit N60). World-thread only.
BgTeamCoordinator&     BgCoordinator();
// Group-level dungeon/raid coordination service. World-thread only.
PveGroupCoordinator&   PveCoordinator();
V2::BotSessionMgr& SessionMgr();
V2::BotAccountMgr& Accounts();
V2::Travel::QuestHubDatabase& Hubs();
// Nearest faction-appropriate repair-vendor spawn index. Built once at boot
// right after Hubs(); lock-free reads after. Used by the broken-gear repair /
// death-spiral-escape routing. World-thread init, AI-worker read.
V2::Travel::RepairVendorIndex& RepairVendors();
V2::Travel::PortalIndex&      Portals();
V2::Travel::UnifiedTravelGraph& TravelGraph();
V2::Fleet::BotPopulationManager& Population();
V2::BotGuildMgr&   Guilds();
V2::BotCoordinationBus& Coordination();
V2::BotGroupBuilder&    GroupBuilder();
// Bot-to-bot craft-order board + escrow ledger (#4B-2). World-thread only.
V2::CraftOrderBoard&    CraftOrders();
ConfigReader&      Config();
PerfCounters&      Perf();
// REFACTOR_3: State_Idle rule registry. Initialized at module init via
// RegisterAllIdleRules. Read by State_Idle::OnTick to dispatch ticks
// and by /whyidle to dump the rule table.
IdleRuleRegistry&  IdleRules();

// Per-bot intent queue — created on demand when a bot is registered.
IntentQueue& Intents(BotId id);
bool         HasIntents(BotId id);

} // namespace Services
} // namespace Playerbot
