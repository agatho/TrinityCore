#include "Services.h"
#include "Bot/BotRegistry.h"
#include "Bot/IdleRule.h"
#include "Fleet/BotAccountMgr.h"
#include "Fleet/BotIdentityRegistry.h"
#include "Fleet/OwnerRegistry.h"
#include "Bot/Dungeon/DungeonScript.h"
#include "Bot/Battleground/BattlegroundScript.h"
#include "Bot/Battleground/BgTeamCoordinator.h"
#include "Bot/Dungeon/PveGroupCoordinator.h"

namespace Playerbot {
// Forward declarations for per-dungeon factory functions. Each
// Bot/Dungeon/Scripts/*.cpp file defines a Make<Name>Script() that
// returns a heap-allocated DungeonScript subclass; we register them
// at module init below. Adding a new dungeon = adding one factory
// declaration here + one Register call in Services::Init.
std::unique_ptr<DungeonScript> MakeMPlusAffixScript();
std::unique_ptr<DungeonScript> MakeDeadminesScript();
std::unique_ptr<DungeonScript> MakeWailingCavernsScript();
std::unique_ptr<DungeonScript> MakeRagefireChasmScript();
std::unique_ptr<DungeonScript> MakeShadowfangKeepScript();
std::unique_ptr<DungeonScript> MakeBlackfathomDeepsScript();
std::unique_ptr<DungeonScript> MakeStockadeScript();
std::unique_ptr<DungeonScript> MakeGnomereganScript();
std::unique_ptr<DungeonScript> MakeRazorfenKraulScript();
std::unique_ptr<DungeonScript> MakeScarletMonasteryScript();
std::unique_ptr<DungeonScript> MakeRazorfenDownsScript();
std::unique_ptr<DungeonScript> MakeUldamanScript();
std::unique_ptr<DungeonScript> MakeZulFarrakScript();
std::unique_ptr<DungeonScript> MakeMaraudonScript();
std::unique_ptr<DungeonScript> MakeSunkenTempleScript();
std::unique_ptr<DungeonScript> MakeStratholmeScript();
std::unique_ptr<DungeonScript> MakeScholomanceScript();
std::unique_ptr<DungeonScript> MakeDireMaulScript();
std::unique_ptr<DungeonScript> MakeBlackrockDepthsScript();
std::unique_ptr<DungeonScript> MakeLowerBlackrockSpireScript();
std::unique_ptr<DungeonScript> MakeHellfireRampartsScript();
std::unique_ptr<DungeonScript> MakeBloodFurnaceScript();
std::unique_ptr<DungeonScript> MakeBlackMorassScript();
std::unique_ptr<DungeonScript> MakeSlavePensScript();
std::unique_ptr<DungeonScript> MakeUnderbogScript();
std::unique_ptr<DungeonScript> MakeSteamvaultScript();
std::unique_ptr<DungeonScript> MakeManaTombsScript();
std::unique_ptr<DungeonScript> MakeAuchenaiCryptsScript();
std::unique_ptr<DungeonScript> MakeSethekkHallsScript();
std::unique_ptr<DungeonScript> MakeShadowLabyrinthScript();
std::unique_ptr<DungeonScript> MakeMechanarScript();
std::unique_ptr<DungeonScript> MakeBotanicaScript();
std::unique_ptr<DungeonScript> MakeArcatrazScript();
std::unique_ptr<DungeonScript> MakeShatteredHallsScript();
std::unique_ptr<DungeonScript> MakeMagistersTerraceScript();
std::unique_ptr<DungeonScript> MakeOldHillsbradScript();
std::unique_ptr<DungeonScript> MakeUtgardeKeepScript();
std::unique_ptr<DungeonScript> MakeUtgardePinnacleScript();
std::unique_ptr<DungeonScript> MakeNexusScript();
std::unique_ptr<DungeonScript> MakeAzjolNerubScript();
std::unique_ptr<DungeonScript> MakeVioletHoldScript();
std::unique_ptr<DungeonScript> MakeCullingOfStratholmeScript();
std::unique_ptr<DungeonScript> MakeAhnkahetScript();
std::unique_ptr<DungeonScript> MakeDrakTharonScript();
std::unique_ptr<DungeonScript> MakeGundrakScript();
std::unique_ptr<DungeonScript> MakeHallsOfStoneScript();
std::unique_ptr<DungeonScript> MakeHallsOfLightningScript();
std::unique_ptr<DungeonScript> MakeOculusScript();
std::unique_ptr<DungeonScript> MakePitOfSaronScript();
std::unique_ptr<DungeonScript> MakeHallsOfReflectionScript();
std::unique_ptr<DungeonScript> MakeTrialOfChampionScript();
std::unique_ptr<DungeonScript> MakeForgeOfSoulsScript();
std::unique_ptr<DungeonScript> MakeThroneOfTidesScript();
std::unique_ptr<DungeonScript> MakeBlackrockCavernsScript();
std::unique_ptr<DungeonScript> MakeVortexPinnacleScript();
std::unique_ptr<DungeonScript> MakeHallsOfOriginationScript();
std::unique_ptr<DungeonScript> MakeGrimBatolScript();
std::unique_ptr<DungeonScript> MakeLostCityOfTolvirScript();
std::unique_ptr<DungeonScript> MakeStonecoreScript();
std::unique_ptr<DungeonScript> MakeZulAmanScript();
std::unique_ptr<DungeonScript> MakeZulGurubScript();
std::unique_ptr<DungeonScript> MakeEndTimeScript();
std::unique_ptr<DungeonScript> MakeHourOfTwilightScript();
std::unique_ptr<DungeonScript> MakeWellOfEternityScript();
std::unique_ptr<DungeonScript> MakeStormstoutBreweryScript();
std::unique_ptr<DungeonScript> MakeTempleOfJadeSerpentScript();
std::unique_ptr<DungeonScript> MakeShadoPanMonasteryScript();
std::unique_ptr<DungeonScript> MakeMogushanPalaceScript();
std::unique_ptr<DungeonScript> MakeGateOfSettingSunScript();
std::unique_ptr<DungeonScript> MakeAuchindounScript();
std::unique_ptr<DungeonScript> MakeHallsOfValorScript();
std::unique_ptr<DungeonScript> MakeEyeOfAzsharaScript();
std::unique_ptr<DungeonScript> MakeDarkheartThicketScript();
std::unique_ptr<DungeonScript> MakeMawOfSoulsScript();
std::unique_ptr<DungeonScript> MakeNeltharionsLairScript();
std::unique_ptr<DungeonScript> MakeBlackRookHoldScript();
std::unique_ptr<DungeonScript> MakeAtalDazarScript();
std::unique_ptr<DungeonScript> MakeKingsRestScript();
std::unique_ptr<DungeonScript> MakeWaycrestManorScript();
std::unique_ptr<DungeonScript> MakeFreeholdScript();
std::unique_ptr<DungeonScript> MakeNecroticWakeScript();
std::unique_ptr<DungeonScript> MakeTheaterOfPainScript();
std::unique_ptr<DungeonScript> MakeHallsOfAtonementScript();
std::unique_ptr<DungeonScript> MakeSpiresOfAscensionScript();
std::unique_ptr<DungeonScript> MakeSanguineDepthsScript();
std::unique_ptr<DungeonScript> MakePlaguefallScript();
std::unique_ptr<DungeonScript> MakeMistsOfTirnaScitheScript();
std::unique_ptr<DungeonScript> MakeDeOtherSideScript();
std::unique_ptr<DungeonScript> MakeTazaveshScript();
std::unique_ptr<DungeonScript> MakeRubyLifePoolsScript();
std::unique_ptr<DungeonScript> MakeAlgetharAcademyScript();
std::unique_ptr<DungeonScript> MakeNokhudOffensiveScript();
std::unique_ptr<DungeonScript> MakeAzureVaultScript();
std::unique_ptr<DungeonScript> MakeHallsOfInfusionScript();
std::unique_ptr<DungeonScript> MakeBrackenhideHollowScript();
std::unique_ptr<DungeonScript> MakeNeltharusScript();
std::unique_ptr<DungeonScript> MakeUldamanLegacyOfTyrScript();
std::unique_ptr<DungeonScript> MakeUnderrotScript();
std::unique_ptr<DungeonScript> MakeTolDagorScript();
std::unique_ptr<DungeonScript> MakeShrineOfTheStormScript();
std::unique_ptr<DungeonScript> MakeSiegeOfBoralusScript();
std::unique_ptr<DungeonScript> MakeTempleOfSethralissScript();
std::unique_ptr<DungeonScript> MakeMotherlodeScript();
std::unique_ptr<DungeonScript> MakeVaultOfTheWardensScript();
std::unique_ptr<DungeonScript> MakeCourtOfStarsScript();
std::unique_ptr<DungeonScript> MakeCathedralOfEternalNightScript();
std::unique_ptr<DungeonScript> MakeArcwayScript();
std::unique_ptr<DungeonScript> MakeSeatOfTheTriumvirateScript();
std::unique_ptr<DungeonScript> MakeReturnToKarazhanScript();
std::unique_ptr<DungeonScript> MakeBloodmaulSlagMinesScript();
std::unique_ptr<DungeonScript> MakeIronDocksScript();
std::unique_ptr<DungeonScript> MakeSkyreachScript();
std::unique_ptr<DungeonScript> MakeShadowmoonBurialGroundsScript();
std::unique_ptr<DungeonScript> MakeEverbloomScript();
std::unique_ptr<DungeonScript> MakeGrimrailDepotScript();
std::unique_ptr<DungeonScript> MakeUpperBlackrockSpireScript();
std::unique_ptr<DungeonScript> MakeSiegeOfNiuzaoTempleScript();
std::unique_ptr<DungeonScript> MakeAraKaraScript();
std::unique_ptr<DungeonScript> MakeCityOfThreadsScript();
std::unique_ptr<DungeonScript> MakeDawnbreakerScript();
std::unique_ptr<DungeonScript> MakeStonevaultScript();
std::unique_ptr<DungeonScript> MakeCinderbrewMeaderyScript();
std::unique_ptr<DungeonScript> MakeDarkflameCleftScript();
std::unique_ptr<DungeonScript> MakeRookeryScript();
std::unique_ptr<DungeonScript> MakePrioryOfTheSacredFlameScript();
std::unique_ptr<DungeonScript> MakeOperationFloodgateScript();
std::unique_ptr<DungeonScript> MakeOperationMechagonWorkshopScript();
std::unique_ptr<DungeonScript> MakeScarletHallsScript();
std::unique_ptr<DungeonScript> MakeDawnOfTheInfiniteFallScript();
std::unique_ptr<DungeonScript> MakeDawnOfTheInfiniteRiseScript();
std::unique_ptr<DungeonScript> MakeScarletMonasteryMoPScript();
// Raid scripts — registered through the same DungeonScript pipeline.
// Raids also use map_id and DungeonRunMode; the gate in State_Idle.cpp
// includes is_in_raid alongside is_in_dungeon.
std::unique_ptr<DungeonScript> MakeIcecrownCitadelScript();
std::unique_ptr<DungeonScript> MakeNaxxramasScript();
std::unique_ptr<DungeonScript> MakeObsidianSanctumScript();
std::unique_ptr<DungeonScript> MakeEyeOfEternityScript();
std::unique_ptr<DungeonScript> MakeVaultOfArchavonScript();
std::unique_ptr<DungeonScript> MakeUlduarScript();
std::unique_ptr<DungeonScript> MakeTrialOfTheCrusaderScript();
std::unique_ptr<DungeonScript> MakeOnyxiasLairScript();
std::unique_ptr<DungeonScript> MakeRubySanctumScript();
std::unique_ptr<DungeonScript> MakeKarazhanScript();
std::unique_ptr<DungeonScript> MakeGruulsLairScript();
std::unique_ptr<DungeonScript> MakeMagtheridonsLairScript();
std::unique_ptr<DungeonScript> MakeSerpentshrineCavernScript();
std::unique_ptr<DungeonScript> MakeTempestKeepRaidScript();
std::unique_ptr<DungeonScript> MakeMountHyjalScript();
std::unique_ptr<DungeonScript> MakeBlackTempleScript();
std::unique_ptr<DungeonScript> MakeSunwellPlateauScript();
std::unique_ptr<DungeonScript> MakeMoltenCoreScript();
std::unique_ptr<DungeonScript> MakeBlackwingLairScript();
std::unique_ptr<DungeonScript> MakeTempleOfAhnQirajScript();
std::unique_ptr<DungeonScript> MakeRuinsOfAhnQirajScript();
std::unique_ptr<DungeonScript> MakeBastionOfTwilightScript();
std::unique_ptr<DungeonScript> MakeBlackwingDescentScript();
std::unique_ptr<DungeonScript> MakeFirelandsScript();
std::unique_ptr<DungeonScript> MakeThroneOfFourWindsScript();
std::unique_ptr<DungeonScript> MakeDragonSoulScript();
std::unique_ptr<BattlegroundScript> MakeSeethingShoreScript();
std::unique_ptr<BattlegroundScript> MakeWarsongGulchScript();
std::unique_ptr<BattlegroundScript> MakeArathiBasinScript();
std::unique_ptr<BattlegroundScript> MakeAlteracValleyScript();
std::unique_ptr<BattlegroundScript> MakeEyeOfTheStormScript();
std::unique_ptr<BattlegroundScript> MakeStrandOfAncientsScript();
std::unique_ptr<BattlegroundScript> MakeIsleOfConquestScript();
std::unique_ptr<BattlegroundScript> MakeTwinPeaksScript();
std::unique_ptr<BattlegroundScript> MakeBattleForGilneasScript();
std::unique_ptr<BattlegroundScript> MakeTempleOfKotmoguScript();
std::unique_ptr<BattlegroundScript> MakeSilvershardMinesScript();
std::unique_ptr<BattlegroundScript> MakeDeephaulRavineScript();
// Arena scripts — one per BattlemasterList id; logic is shared.
std::unique_ptr<BattlegroundScript> MakeNagrandArenaScript();
std::unique_ptr<BattlegroundScript> MakeBladesEdgeArenaScript();
std::unique_ptr<BattlegroundScript> MakeAllArenasArenaScript();
std::unique_ptr<BattlegroundScript> MakeRuinsOfLordaeronArenaScript();
std::unique_ptr<BattlegroundScript> MakeDalaranSewersArenaScript();
std::unique_ptr<BattlegroundScript> MakeRingOfValorArenaScript();
std::unique_ptr<BattlegroundScript> MakeTolVironArenaScript();
std::unique_ptr<BattlegroundScript> MakeTigersPeakArenaScript();
std::unique_ptr<BattlegroundScript> MakeRuinsOfLordaeron2ArenaScript();
std::unique_ptr<BattlegroundScript> MakeDalaranSewers2ArenaScript();
std::unique_ptr<BattlegroundScript> MakeTolViron2ArenaScript();
std::unique_ptr<BattlegroundScript> MakeTigersPeak2ArenaScript();
std::unique_ptr<BattlegroundScript> MakeNagrand2ArenaScript();
std::unique_ptr<BattlegroundScript> MakeBladesEdge2ArenaScript();
// Legion arena maps + later "v2/v3" Brawl/Solo Shuffle rebrands and
// BfA/SL/DF arenas. All share the generic ArenaScript logic; each id
// just needs an instance so the dispatcher can route advice.
std::unique_ptr<BattlegroundScript> MakeBlackRookHoldArenaScript();
std::unique_ptr<BattlegroundScript> MakeAshamanesFallArenaScript();
std::unique_ptr<BattlegroundScript> MakeBlackRookHold2ArenaScript();
std::unique_ptr<BattlegroundScript> MakeAshamanesFall2ArenaScript();
std::unique_ptr<BattlegroundScript> MakeHookPointArenaScript();
std::unique_ptr<BattlegroundScript> MakeTigersPeak3ArenaScript();
std::unique_ptr<BattlegroundScript> MakeMugambalaArenaScript();
std::unique_ptr<BattlegroundScript> MakeAshamanesFall3ArenaScript();
std::unique_ptr<BattlegroundScript> MakeBladesEdge3ArenaScript();
std::unique_ptr<BattlegroundScript> MakeBladesEdgeV2MeshArenaScript();
std::unique_ptr<BattlegroundScript> MakeDalaranSewers3ArenaScript();
std::unique_ptr<BattlegroundScript> MakeNagrand3ArenaScript();
std::unique_ptr<BattlegroundScript> MakeRuinsOfLordaeron3ArenaScript();
std::unique_ptr<BattlegroundScript> MakeTolViron3ArenaScript();
std::unique_ptr<BattlegroundScript> MakeBlackRookHold3ArenaScript();
std::unique_ptr<BattlegroundScript> MakeRobodromeArenaScript();
std::unique_ptr<BattlegroundScript> MakeEmpyreanDomainArenaScript();
}
#include "Session/BotSessionMgr.h"
#include "Travel/QuestHubDatabase.h"
#include "Travel/RepairVendorIndex.h"
#include "Travel/PortalIndex.h"
#include "Travel/PortalPocketIndex.h"
#include "Travel/ElevatorIndex.h"
#include "Travel/UnifiedTravelGraph.h"
#include "Fleet/BotPopulationManager.h"
#include "Fleet/BotGuildMgr.h"
#include "Fleet/BotGuildNamePool.h"
#include "Fleet/BotCoordinationBus.h"
#include "Fleet/BotGroupBuilder.h"
#include "Fleet/CraftOrderBoard.h"
#include "Bot/Gear/BotGearGenerator.h"
#include "Threading/SnapshotPublisher.h"
#include "Threading/IntentQueue.h"
#include "Threading/AiWorkerPool.h"
#include "Threading/FleetThread.h"
#include "Threading/TickScheduler.h"
#include "Util/ConfigReader.h"
#include "Diagnostics/PerfCounters.h"
#include "Config.h"   // sConfigMgr->GetFilename for resolving playerbot.conf next to worldserver.conf
#include <atomic>
#include <filesystem>
#include <memory>

namespace Playerbot::Services {

namespace {

struct Holder
{
    std::unique_ptr<ConfigReader>      config;
    std::unique_ptr<SnapshotPublisher> snapshots;
    std::unique_ptr<AiWorkerPool>      ai_pool;
    std::unique_ptr<FleetThread>       fleet;
    std::unique_ptr<TickScheduler>     scheduler;
    std::unique_ptr<BotRegistry>           registry;
    std::unique_ptr<BotIdentityRegistry>   lifecycle;
    std::unique_ptr<OwnerRegistry>         owners;
    std::unique_ptr<DungeonScriptMgr>      dungeons;
    std::unique_ptr<BattlegroundScriptMgr> battlegrounds;
    std::unique_ptr<BgTeamCoordinator>     bg_coordinator;
    std::unique_ptr<PveGroupCoordinator>   pve_coordinator;
    std::unique_ptr<V2::BotSessionMgr>     session_mgr;
    std::unique_ptr<V2::BotAccountMgr>     accounts;
    std::unique_ptr<V2::Travel::QuestHubDatabase> hubs;
    std::unique_ptr<V2::Travel::RepairVendorIndex> repair_vendors;
    std::unique_ptr<V2::Travel::PortalIndex>      portals;
    std::unique_ptr<V2::Travel::UnifiedTravelGraph> travel_graph;
    std::unique_ptr<V2::Fleet::BotPopulationManager> population;
    std::unique_ptr<V2::BotGuildMgr>             guilds;
    std::unique_ptr<V2::BotGuildNamePool>        guild_names;
    std::unique_ptr<V2::BotCoordinationBus>      coord_bus;
    std::unique_ptr<V2::BotGroupBuilder>         group_builder;
    std::unique_ptr<V2::CraftOrderBoard>         craft_orders;
    std::unique_ptr<PerfCounters>          perf;
    std::unique_ptr<IdleRuleRegistry>      idle_rules;
};

Holder*                  g_holder    = nullptr;
std::atomic<bool>        g_init_flag = {false};

} // anonymous

void Init()
{
    bool expected = false;
    if (!g_init_flag.compare_exchange_strong(expected, true)) return;

    auto h = new Holder();
    // Stamp the global pointer BEFORE any subsystem Initialize(). Several
    // initializers (notably UnifiedTravelGraph::LoadPortalsAndDocks → uses
    // Services::Portals(); BotGuildMgr::ApplyConfig → uses Services::Config())
    // dereference g_holder during their setup. Without this early stamp the
    // first such call yields ACCESS_VIOLATION at Services accessor +7. The
    // accessor functions safely return reference-to-member via the holder,
    // and every accessor that's read during init must have its member already
    // populated above the call site — which is the case below because we
    // populate `h->X` and `g_holder = h` (same pointer), so reading
    // `g_holder->X` works as long as `h->X` is set before the call. The
    // late-Init re-stamp at the end is harmless idempotent.
    g_holder = h;
    h->config    = std::make_unique<ConfigReader>();
    // Resolve playerbot.conf next to the worldserver.conf the server actually
    // loaded — bare "playerbot.conf" would resolve against process CWD, which
    // is the deploy dir (e.g. M:\PlayerbotServer) and may differ from the
    // config dir on systems that pass `-c` with an absolute path.
    {
        std::filesystem::path const wsConf{sConfigMgr->GetFilename()};
        std::filesystem::path const playerbotConf = wsConf.parent_path() / "playerbot.conf";
        h->config->load(playerbotConf.generic_string());
    }
    h->perf      = std::make_unique<PerfCounters>();
    h->snapshots = std::make_unique<SnapshotPublisher>();
    h->ai_pool   = std::make_unique<AiWorkerPool>(h->config->ai_worker_threads());
    h->fleet     = std::make_unique<FleetThread>();
    h->scheduler = std::make_unique<TickScheduler>(*h->ai_pool);
    h->registry  = std::make_unique<BotRegistry>();
    h->lifecycle = std::make_unique<BotIdentityRegistry>();
    h->lifecycle->LoadFromDb();
    // Owner bindings — depends on the character table which lifecycle
    // also reads, so safe to load right after.
    h->owners = std::make_unique<OwnerRegistry>();
    h->owners->LoadFromDb();
    // Dungeon script registry — pure in-memory, populated by
    // factories defined in Bot/Dungeon/Scripts/*.cpp. Empty registry
    // is fine — script-less dungeons fall back to generic logic.
    h->dungeons = std::make_unique<DungeonScriptMgr>();
    // Global affix bundle — merged into every per-dungeon GetAdvice call.
    // Idle/combat rules consume only fields they understand; affix entries/
    // spell ids that aren't present in the current pull are inert.
    h->dungeons->RegisterGlobal(MakeMPlusAffixScript());
    h->dungeons->Register(MakeDeadminesScript());
    h->dungeons->Register(MakeWailingCavernsScript());
    h->dungeons->Register(MakeRagefireChasmScript());
    h->dungeons->Register(MakeShadowfangKeepScript());
    h->dungeons->Register(MakeBlackfathomDeepsScript());
    h->dungeons->Register(MakeStockadeScript());
    h->dungeons->Register(MakeGnomereganScript());
    h->dungeons->Register(MakeRazorfenKraulScript());
    h->dungeons->Register(MakeScarletMonasteryScript());
    h->dungeons->Register(MakeRazorfenDownsScript());
    h->dungeons->Register(MakeUldamanScript());
    h->dungeons->Register(MakeZulFarrakScript());
    h->dungeons->Register(MakeMaraudonScript());
    h->dungeons->Register(MakeSunkenTempleScript());
    h->dungeons->Register(MakeStratholmeScript());
    h->dungeons->Register(MakeScholomanceScript());
    h->dungeons->Register(MakeDireMaulScript());
    h->dungeons->Register(MakeBlackrockDepthsScript());
    h->dungeons->Register(MakeLowerBlackrockSpireScript());
    h->dungeons->Register(MakeHellfireRampartsScript());
    h->dungeons->Register(MakeBloodFurnaceScript());
    h->dungeons->Register(MakeBlackMorassScript());
    h->dungeons->Register(MakeSlavePensScript());
    h->dungeons->Register(MakeUnderbogScript());
    h->dungeons->Register(MakeSteamvaultScript());
    h->dungeons->Register(MakeManaTombsScript());
    h->dungeons->Register(MakeAuchenaiCryptsScript());
    h->dungeons->Register(MakeSethekkHallsScript());
    h->dungeons->Register(MakeShadowLabyrinthScript());
    h->dungeons->Register(MakeMechanarScript());
    h->dungeons->Register(MakeBotanicaScript());
    h->dungeons->Register(MakeArcatrazScript());
    h->dungeons->Register(MakeShatteredHallsScript());
    h->dungeons->Register(MakeMagistersTerraceScript());
    h->dungeons->Register(MakeOldHillsbradScript());
    h->dungeons->Register(MakeUtgardeKeepScript());
    h->dungeons->Register(MakeUtgardePinnacleScript());
    h->dungeons->Register(MakeNexusScript());
    h->dungeons->Register(MakeAzjolNerubScript());
    h->dungeons->Register(MakeVioletHoldScript());
    h->dungeons->Register(MakeCullingOfStratholmeScript());
    h->dungeons->Register(MakeAhnkahetScript());
    h->dungeons->Register(MakeDrakTharonScript());
    h->dungeons->Register(MakeGundrakScript());
    h->dungeons->Register(MakeHallsOfStoneScript());
    h->dungeons->Register(MakeHallsOfLightningScript());
    h->dungeons->Register(MakeOculusScript());
    h->dungeons->Register(MakePitOfSaronScript());
    h->dungeons->Register(MakeHallsOfReflectionScript());
    h->dungeons->Register(MakeTrialOfChampionScript());
    h->dungeons->Register(MakeForgeOfSoulsScript());
    h->dungeons->Register(MakeThroneOfTidesScript());
    h->dungeons->Register(MakeBlackrockCavernsScript());
    h->dungeons->Register(MakeVortexPinnacleScript());
    h->dungeons->Register(MakeHallsOfOriginationScript());
    h->dungeons->Register(MakeGrimBatolScript());
    h->dungeons->Register(MakeLostCityOfTolvirScript());
    h->dungeons->Register(MakeStonecoreScript());
    h->dungeons->Register(MakeZulAmanScript());
    h->dungeons->Register(MakeZulGurubScript());
    h->dungeons->Register(MakeEndTimeScript());
    h->dungeons->Register(MakeHourOfTwilightScript());
    h->dungeons->Register(MakeWellOfEternityScript());
    h->dungeons->Register(MakeStormstoutBreweryScript());
    h->dungeons->Register(MakeTempleOfJadeSerpentScript());
    h->dungeons->Register(MakeShadoPanMonasteryScript());
    h->dungeons->Register(MakeMogushanPalaceScript());
    h->dungeons->Register(MakeGateOfSettingSunScript());
    h->dungeons->Register(MakeAuchindounScript());
    h->dungeons->Register(MakeHallsOfValorScript());
    h->dungeons->Register(MakeEyeOfAzsharaScript());
    h->dungeons->Register(MakeDarkheartThicketScript());
    h->dungeons->Register(MakeMawOfSoulsScript());
    h->dungeons->Register(MakeNeltharionsLairScript());
    h->dungeons->Register(MakeBlackRookHoldScript());
    h->dungeons->Register(MakeAtalDazarScript());
    h->dungeons->Register(MakeKingsRestScript());
    h->dungeons->Register(MakeWaycrestManorScript());
    h->dungeons->Register(MakeFreeholdScript());
    h->dungeons->Register(MakeNecroticWakeScript());
    h->dungeons->Register(MakeTheaterOfPainScript());
    h->dungeons->Register(MakeHallsOfAtonementScript());
    h->dungeons->Register(MakeSpiresOfAscensionScript());
    h->dungeons->Register(MakeSanguineDepthsScript());
    h->dungeons->Register(MakePlaguefallScript());
    h->dungeons->Register(MakeMistsOfTirnaScitheScript());
    h->dungeons->Register(MakeDeOtherSideScript());
    h->dungeons->Register(MakeTazaveshScript());
    h->dungeons->Register(MakeRubyLifePoolsScript());
    h->dungeons->Register(MakeAlgetharAcademyScript());
    h->dungeons->Register(MakeNokhudOffensiveScript());
    h->dungeons->Register(MakeAzureVaultScript());
    h->dungeons->Register(MakeHallsOfInfusionScript());
    h->dungeons->Register(MakeBrackenhideHollowScript());
    h->dungeons->Register(MakeNeltharusScript());
    h->dungeons->Register(MakeUldamanLegacyOfTyrScript());
    h->dungeons->Register(MakeUnderrotScript());
    h->dungeons->Register(MakeTolDagorScript());
    h->dungeons->Register(MakeShrineOfTheStormScript());
    h->dungeons->Register(MakeSiegeOfBoralusScript());
    h->dungeons->Register(MakeTempleOfSethralissScript());
    h->dungeons->Register(MakeMotherlodeScript());
    h->dungeons->Register(MakeVaultOfTheWardensScript());
    h->dungeons->Register(MakeCourtOfStarsScript());
    h->dungeons->Register(MakeCathedralOfEternalNightScript());
    h->dungeons->Register(MakeArcwayScript());
    h->dungeons->Register(MakeSeatOfTheTriumvirateScript());
    h->dungeons->Register(MakeReturnToKarazhanScript());
    h->dungeons->Register(MakeBloodmaulSlagMinesScript());
    h->dungeons->Register(MakeIronDocksScript());
    h->dungeons->Register(MakeSkyreachScript());
    h->dungeons->Register(MakeShadowmoonBurialGroundsScript());
    h->dungeons->Register(MakeEverbloomScript());
    h->dungeons->Register(MakeGrimrailDepotScript());
    h->dungeons->Register(MakeUpperBlackrockSpireScript());
    h->dungeons->Register(MakeSiegeOfNiuzaoTempleScript());
    h->dungeons->Register(MakeAraKaraScript());
    h->dungeons->Register(MakeCityOfThreadsScript());
    h->dungeons->Register(MakeDawnbreakerScript());
    h->dungeons->Register(MakeStonevaultScript());
    h->dungeons->Register(MakeCinderbrewMeaderyScript());
    h->dungeons->Register(MakeDarkflameCleftScript());
    h->dungeons->Register(MakeRookeryScript());
    h->dungeons->Register(MakePrioryOfTheSacredFlameScript());
    h->dungeons->Register(MakeOperationFloodgateScript());
    h->dungeons->Register(MakeOperationMechagonWorkshopScript());
    h->dungeons->Register(MakeScarletHallsScript());
    h->dungeons->Register(MakeDawnOfTheInfiniteFallScript());
    h->dungeons->Register(MakeDawnOfTheInfiniteRiseScript());
    h->dungeons->Register(MakeScarletMonasteryMoPScript());
    // Raid scripts.
    h->dungeons->Register(MakeIcecrownCitadelScript());
    h->dungeons->Register(MakeNaxxramasScript());
    h->dungeons->Register(MakeObsidianSanctumScript());
    h->dungeons->Register(MakeEyeOfEternityScript());
    h->dungeons->Register(MakeVaultOfArchavonScript());
    h->dungeons->Register(MakeUlduarScript());
    h->dungeons->Register(MakeTrialOfTheCrusaderScript());
    h->dungeons->Register(MakeOnyxiasLairScript());
    h->dungeons->Register(MakeRubySanctumScript());
    h->dungeons->Register(MakeKarazhanScript());
    h->dungeons->Register(MakeGruulsLairScript());
    h->dungeons->Register(MakeMagtheridonsLairScript());
    h->dungeons->Register(MakeSerpentshrineCavernScript());
    h->dungeons->Register(MakeTempestKeepRaidScript());
    h->dungeons->Register(MakeMountHyjalScript());
    h->dungeons->Register(MakeBlackTempleScript());
    h->dungeons->Register(MakeSunwellPlateauScript());
    h->dungeons->Register(MakeMoltenCoreScript());
    h->dungeons->Register(MakeBlackwingLairScript());
    h->dungeons->Register(MakeTempleOfAhnQirajScript());
    h->dungeons->Register(MakeRuinsOfAhnQirajScript());
    h->dungeons->Register(MakeBastionOfTwilightScript());
    h->dungeons->Register(MakeBlackwingDescentScript());
    h->dungeons->Register(MakeFirelandsScript());
    h->dungeons->Register(MakeThroneOfFourWindsScript());
    h->dungeons->Register(MakeDragonSoulScript());
    // Inject DB route_waypoints (playerbot_dungeon_routes) into every dungeon
    // whose script left route_waypoints empty, and load the DB traversal links
    // (playerbot_nav_links). Must run AFTER all scripts are registered; both
    // are hot-reloadable via `.playerbot reloadroutes`.
    h->dungeons->LoadGeneratedRoutes();
    h->dungeons->LoadNavLinks();
    // BG script registry — mirrors dungeon registry; empty registry
    // is fine (script-less BGs fall back to generic combat).
    h->battlegrounds = std::make_unique<BattlegroundScriptMgr>();
    h->battlegrounds->Register(MakeWarsongGulchScript());
    h->battlegrounds->Register(MakeArathiBasinScript());
    h->battlegrounds->Register(MakeAlteracValleyScript());
    h->battlegrounds->Register(MakeEyeOfTheStormScript());
    h->battlegrounds->Register(MakeStrandOfAncientsScript());
    h->battlegrounds->Register(MakeIsleOfConquestScript());
    h->battlegrounds->Register(MakeTwinPeaksScript());
    h->battlegrounds->Register(MakeBattleForGilneasScript());
    h->battlegrounds->Register(MakeTempleOfKotmoguScript());
    h->battlegrounds->Register(MakeSilvershardMinesScript());
    h->battlegrounds->Register(MakeDeephaulRavineScript());
    h->battlegrounds->Register(MakeSeethingShoreScript());
    // Deepwind Gorge (754), Ashran (1020/1021) and Wintergrasp (1017/1030)
    // registrations REMOVED (BG audit dead-code cleanup): none has a
    // battleground_template row on this core and none is in the bot queue
    // seed rotation, so Player::InBattleground() is never true for them and
    // their scripts could never be dispatched. Ashran/WG are Battlefields,
    // not Battlegrounds; DG has no server-side BG script at all. Re-add only
    // if/when those become real, queueable Battlegrounds.
    // Arena scripts — one register per arena map id.
    h->battlegrounds->Register(MakeNagrandArenaScript());
    h->battlegrounds->Register(MakeBladesEdgeArenaScript());
    h->battlegrounds->Register(MakeAllArenasArenaScript());
    h->battlegrounds->Register(MakeRuinsOfLordaeronArenaScript());
    h->battlegrounds->Register(MakeDalaranSewersArenaScript());
    h->battlegrounds->Register(MakeRingOfValorArenaScript());
    h->battlegrounds->Register(MakeTolVironArenaScript());
    h->battlegrounds->Register(MakeTigersPeakArenaScript());
    h->battlegrounds->Register(MakeRuinsOfLordaeron2ArenaScript());
    h->battlegrounds->Register(MakeDalaranSewers2ArenaScript());
    h->battlegrounds->Register(MakeTolViron2ArenaScript());
    h->battlegrounds->Register(MakeTigersPeak2ArenaScript());
    h->battlegrounds->Register(MakeNagrand2ArenaScript());
    h->battlegrounds->Register(MakeBladesEdge2ArenaScript());
    // Legion arenas (ids 808/816) + their v2/v3 Brawl rebrands.
    h->battlegrounds->Register(MakeBlackRookHoldArenaScript());
    h->battlegrounds->Register(MakeAshamanesFallArenaScript());
    h->battlegrounds->Register(MakeBlackRookHold2ArenaScript());
    h->battlegrounds->Register(MakeAshamanesFall2ArenaScript());
    // BfA / Shadowlands / Dragonflight arenas + their v3 rebrands.
    h->battlegrounds->Register(MakeHookPointArenaScript());
    h->battlegrounds->Register(MakeTigersPeak3ArenaScript());
    h->battlegrounds->Register(MakeMugambalaArenaScript());
    h->battlegrounds->Register(MakeAshamanesFall3ArenaScript());
    h->battlegrounds->Register(MakeBladesEdge3ArenaScript());
    h->battlegrounds->Register(MakeBladesEdgeV2MeshArenaScript());
    h->battlegrounds->Register(MakeDalaranSewers3ArenaScript());
    h->battlegrounds->Register(MakeNagrand3ArenaScript());
    h->battlegrounds->Register(MakeRuinsOfLordaeron3ArenaScript());
    h->battlegrounds->Register(MakeTolViron3ArenaScript());
    h->battlegrounds->Register(MakeBlackRookHold3ArenaScript());
    h->battlegrounds->Register(MakeRobodromeArenaScript());
    h->battlegrounds->Register(MakeEmpyreanDomainArenaScript());
    // Team-level BG coordinator (BG audit N60). Driven from
    // Module::OnWorldUpdate ahead of the snapshot pass.
    h->bg_coordinator = std::make_unique<BgTeamCoordinator>();
    // Group-level dungeon/raid coordinator — same drive point.
    h->pve_coordinator = std::make_unique<PveGroupCoordinator>();
    h->session_mgr = std::make_unique<V2::BotSessionMgr>();
    h->accounts    = std::make_unique<V2::BotAccountMgr>();
    h->accounts->LoadFromDb();
    // Build the quest-hub spatial map. Synchronous DB-bound work — runs on
    // the world thread once at boot (~200ms on retail data per V1 numbers),
    // afterwards reads are lock-free-ish via shared_mutex on the AI workers.
    h->hubs = std::make_unique<V2::Travel::QuestHubDatabase>();
    h->hubs->Initialize();
    // Build the repair-vendor spawn index — same lifecycle as Hubs: one
    // synchronous DB-bound boot query, lock-free reads afterwards. Gives every
    // bot a queryable nearest faction-appropriate repair vendor for the
    // broken-gear repair + death-spiral-escape rules (the snapshot 80y scan and
    // the operator-curated Vendor metadata don't cover questless wilderness
    // bots). Built right after Hubs so the repair routing can fall back to
    // GetNearestQuestHub cross-map.
    h->repair_vendors = std::make_unique<V2::Travel::RepairVendorIndex>();
    h->repair_vendors->Initialize();
    // Build the portal/transport spawn index. Same lifecycle as Hubs:
    // synchronous DB-bound walk at boot, read-only afterwards. Drives
    // the cross-map cascade rules in Idle (walk to portal/dock).
    h->portals = std::make_unique<V2::Travel::PortalIndex>();
    h->portals->Initialize();

    // PortalPocketIndex: detect capital "portal rooms" that are navmesh-
    // disconnected and entered via a server-side teleporter areatrigger (e.g.
    // the Stormwind Mage Tower portal room). Built from the custom cast-on-enter
    // areatriggers + the portal cluster just loaded into PortalIndex, so it MUST
    // run after h->portals->Initialize(). Drives the walk_to_portal gateway
    // redirect in State_Idle (bot walks onto the entrance trigger -> server
    // teleports it into the room -> normal portal use takes over).
    V2::Travel::PortalPocketIndex::Instance().Initialize(*h->portals);

    // ElevatorIndex: auto-detect every GAMEOBJECT_TYPE_TRANSPORT spawn
    // and compute world-space stop positions from gameobject_template
    // .transport.Timeto*floor + TransportAnimation.db2 frame data. No
    // operator annotations required — the idle:elevator_step_on /
    // step_off rules consult this index first, falling back to
    // WorldMetadataKind::Elevator entries when nothing was auto-detected.
    V2::Travel::ElevatorIndex::Instance().LoadFromGameObjects();

    // UnifiedTravelGraph: builds the A* graph on top of Hubs+Portals+
    // TaxiNodes+Capitals+DungeonEntries. Replaces the per-system
    // greedy planners. MUST come after Hubs+Portals because it reads
    // their loaded data.
    h->travel_graph = std::make_unique<V2::Travel::UnifiedTravelGraph>();
    h->travel_graph->Initialize();

    // Build the gear-generation pool (per-class candidate items) — synchronous
    // walk of sObjectMgr->GetItemTemplateStore(), happens once at boot.
    V2::Gear::Initialize();

    // Distribution shaper (Phase A of WORLD_POPULATION_PLAN). Creating it
    // is cheap; first reconcile happens on the first OnWorldUpdate tick that
    // crosses the kTickIntervalMs threshold.
    h->population = std::make_unique<V2::Fleet::BotPopulationManager>();

    // Bot guild ecosystem (GUILD_PLAN.md). Phase A is the manager
    // interface + per-faction guild target table; Phase A.2 wires the
    // charter founding FSM. Init order: name pool → manager (the
    // manager links to the pool via BotGuildMgr_SetNamePool). Both
    // are cheap construction; the only DB work at boot is loading the
    // small bot_guild_meta + bot_guild_name_reserved tables.
    h->guild_names = std::make_unique<V2::BotGuildNamePool>();
    h->guild_names->LoadFromDb();
    h->guilds      = std::make_unique<V2::BotGuildMgr>();
    V2::BotGuildMgr_SetNamePool(h->guild_names.get());
    h->guilds->LoadFromDb();
    // Cross-bot coordination bus + shared group builder. Order: bus
    // first, then builder (builder registers subscriptions on bus).
    h->coord_bus     = std::make_unique<V2::BotCoordinationBus>();
    h->group_builder = std::make_unique<V2::BotGroupBuilder>();
    h->group_builder->RegisterSubscriptions(*h->coord_bus);
    // Bot-to-bot craft-order board + escrow ledger (#4B-2). Constructed here;
    // LoadFromDb runs after g_holder is stamped (it uses Services::Registry()
    // for the human-firewall reload check), so the call is deferred until just
    // before the worker pool starts (below).
    h->craft_orders = std::make_unique<V2::CraftOrderBoard>();
    // Phase E: pull config knobs (master enable, target count, events
    // toggle, recruitment-channel toggle) before the first Tick().
    g_holder = h;
    h->guilds->ApplyConfig();
    // (g_holder must be set *before* ApplyConfig — ApplyConfig reads
    // Services::Config() which dereferences g_holder. We re-stamp at
    // the end of Init too as the normal flow, harmless idempotent.)

    // REFACTOR_3: Wire the State_Idle rule registry. RegisterAllIdleRules
    // calls each subsystem registrar (RegisterVendorRules, etc.) so the
    // first State_Idle::OnTick after init walks a fully-populated table.
    h->idle_rules = std::make_unique<IdleRuleRegistry>();
    RegisterAllIdleRules(*h->idle_rules);

    // Reconcile the craft-order board from the DB now that g_holder is stamped
    // (LoadFromDb is read-only at this point; the human-firewall is enforced at
    // live transitions, not at load — see CraftOrderBoard::LoadFromDb).
    h->craft_orders->LoadFromDb();

    h->ai_pool->start();
    h->fleet->start();

    g_holder = h;
}

void Shutdown()
{
    bool expected = true;
    if (!g_init_flag.compare_exchange_strong(expected, false)) return;
    if (!g_holder) return;

    g_holder->fleet->stop();
    g_holder->ai_pool->stop();

    delete g_holder;
    g_holder = nullptr;
}

bool Initialized() { return g_init_flag.load(std::memory_order_acquire); }

SnapshotPublisher& Snapshots() { return *g_holder->snapshots; }
AiWorkerPool&      AiPool()    { return *g_holder->ai_pool; }
FleetThread&       Fleet()     { return *g_holder->fleet; }
TickScheduler&     Scheduler() { return *g_holder->scheduler; }
BotRegistry&         Registry()  { return *g_holder->registry; }
BotIdentityRegistry& Lifecycle() { return *g_holder->lifecycle; }
OwnerRegistry&       Owners()    { return *g_holder->owners; }
DungeonScriptMgr&    Dungeons()  { return *g_holder->dungeons; }
BattlegroundScriptMgr& Battlegrounds() { return *g_holder->battlegrounds; }
BgTeamCoordinator&     BgCoordinator() { return *g_holder->bg_coordinator; }
PveGroupCoordinator&   PveCoordinator() { return *g_holder->pve_coordinator; }
V2::BotSessionMgr&   SessionMgr(){ return *g_holder->session_mgr; }
V2::BotAccountMgr&   Accounts()  { return *g_holder->accounts; }
V2::Travel::QuestHubDatabase& Hubs() { return *g_holder->hubs; }
V2::Travel::RepairVendorIndex& RepairVendors() { return *g_holder->repair_vendors; }
V2::Travel::PortalIndex&      Portals() { return *g_holder->portals; }
V2::Travel::UnifiedTravelGraph& TravelGraph() { return *g_holder->travel_graph; }
V2::Fleet::BotPopulationManager& Population() { return *g_holder->population; }
V2::BotGuildMgr&     Guilds()    { return *g_holder->guilds; }
V2::BotCoordinationBus& Coordination() { return *g_holder->coord_bus; }
V2::BotGroupBuilder&    GroupBuilder() { return *g_holder->group_builder; }
V2::CraftOrderBoard&    CraftOrders()  { return *g_holder->craft_orders; }
ConfigReader&        Config()    { return *g_holder->config; }
PerfCounters&        Perf()      { return *g_holder->perf; }
IdleRuleRegistry&    IdleRules() { return *g_holder->idle_rules; }

IntentQueue& Intents(BotId id)
{
    return *g_holder->registry->intents(id);
}

bool HasIntents(BotId id)
{
    return g_holder && g_holder->registry->intents(id) != nullptr;
}

} // namespace Playerbot::Services
