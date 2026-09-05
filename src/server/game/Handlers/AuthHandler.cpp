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

#include "WorldSession.h"
#include "AuthenticationPackets.h"
#include "BattlenetRpcErrorCodes.h"
#include "CharacterTemplateDataStore.h"
#include "Config.h"
#include "ClientConfigPackets.h"
#include "DB2Stores.h"
#include "DisableMgr.h"
#include "GameTime.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "RBAC.h"
#include "RealmList.h"
#include "StringFormat.h"
#include "SystemPackets.h"
#include "Timezone.h"
#include "Util.h"
#include "World.h"
#include <span>

#include <utf8.h>

void WorldSession::SendAuthResponse(uint32 code, bool queued, uint32 queuePos)
{
    WorldPackets::Auth::AuthResponse response;
    response.Result = code;

    if (code == ERROR_OK)
    {
        response.SuccessInfo.emplace();

        response.SuccessInfo->ActiveExpansionLevel = GetExpansion();
        response.SuccessInfo->AccountExpansionLevel = GetAccountExpansion();
        response.SuccessInfo->Time = int32(GameTime::GetGameTime());

        // Send current home realm. Also there is no need to send it later in realm queries.
        if (std::shared_ptr<Realm const> currentRealm = sRealmList->GetCurrentRealm())
        {
            response.SuccessInfo->VirtualRealmAddress = currentRealm->Id.GetAddress();
            response.SuccessInfo->VirtualRealms.emplace_back(currentRealm->Id.GetAddress(), true, false, currentRealm->Name, currentRealm->NormalizedName);
        }

        if (HasPermission(rbac::RBAC_PERM_USE_CHARACTER_TEMPLATES))
            for (auto&& templ : sCharacterTemplateDataStore->GetCharacterTemplates())
                response.SuccessInfo->Templates.push_back(&templ.second);

        response.SuccessInfo->AvailableClasses = &sObjectMgr->GetRaceClassRequirements();

        // TEMPORARY - prevent creating characters in uncompletable zone
        // This has the side effect of disabling Exile's Reach choice clientside without actually forcing character templates
        response.SuccessInfo->ForceCharacterTemplate = DisableMgr::IsDisabledFor(DISABLE_TYPE_MAP, 2175 /*Exile's Reach*/, nullptr);
    }

    if (queued)
    {
        response.WaitInfo.emplace();
        response.WaitInfo->WaitCount = queuePos;
    }

    SendPacket(response.Write());
}

void WorldSession::SendAuthWaitQueue(uint32 position)
{
    if (position)
    {
        WorldPackets::Auth::WaitQueueUpdate waitQueueUpdate;
        waitQueueUpdate.WaitInfo.WaitCount = position;
        waitQueueUpdate.WaitInfo.WaitTime = 0;
        waitQueueUpdate.WaitInfo.HasFCM = false;
        SendPacket(waitQueueUpdate.Write());
    }
    else
        SendPacket(WorldPackets::Auth::WaitQueueFinish().Write());
}

void WorldSession::SendClientCacheVersion(uint32 version)
{
    WorldPackets::ClientConfig::ClientCacheVersion cache;
    cache.CacheVersion = version;

    SendPacket(cache.Write());
}

// SMSG_CACHE_INFO, one packet per cache domain. The client turns every entry into the CVar
// "CACHE-<Prefix>-<Key>" (format string RVA 0x3B6AA20) and compares the value case insensitively
// against what it stored; on the first difference it discards the whole cache behind Prefix and
// re-queries it (handler RVA 0x341AD0). A packet with no entries makes it do nothing at all.
//
// The Key space belongs to the server alone. There is exactly one "CACHE-" string in the whole
// 69382 binary - that format string - and no CVar list carries a CACHE- entry, so the client has no
// static idea which keys exist. What is closed is the PREFIX list, not the key list: only WGOB,
// WNPC, WQST, WPTX and WPTN are matched (MatchesPrefix bodies at RVA 0x331100..0x331580), any other
// prefix writes a CVar and discards nothing. All five are known below; which of them actually go
// out is the next paragraph.
//
// Which domains and which keys is not a design decision, it is measured. Retail sends exactly two
// packets per login, and it sends them in every one of the 11 twelve-point-one recordings that
// cover a login - 22 packets, counted and round-tripped byte for byte by
// C:\dumps\tools\w0_query_hotfix\round_trip.py:
//
//   WQST  QuestObjectiveXEffectRecordCount, QuestObjectiveXEffectHotfixCount,
//         QuestV2RecordCount, QuestV2HotfixCount,
//         QuestObjectiveRecordCount, QuestObjectiveHotfixCount
//   WGOB  GameObjectsRecordCount, GameObjectsHotfixCount
//
// WNPC, WPTX and WPTN are matched by the client but never sent, in any recording. That is not the
// "absent from the sniff, so probably rare" fallacy: the situation that produces this message is
// every single login, and every recorded login carries these two packets and no others. So the list
// above is what "not more, not less" means here, and the table below sends nothing beyond it.
// Whatever a realm wants on top goes through the `cache_info` world table, opt-in per row - the
// three unused domains stay in the table for exactly that reason and send no packet while empty.
//
// Of the eight retail keys this core can fill five. There is no QuestObjective and no
// QuestObjectiveXEffect DB2 in this core, nor in WoWDBDefs - quest objectives live in the world
// database - so their HotfixCount companions have no table hash to count, and QuestObjectiveXEffect
// has no counterpart at all. QuestObjectiveRecordCount does have one, and it is a true count of the
// loaded `quest_objectives` rows; retail's value is that same table's DB2 row count, so the key
// means the same thing on both sides and only the source differs, exactly as it already does for
// QuestV2RecordCount.
//
// The values are counts, not checksums over the domain data, and it is worth being precise about
// what that buys. A count moves when rows are ADDED or REMOVED; editing an existing row leaves it
// where it is. `<Table>RecordCount` is weaker still: DB2Store::GetNumRows returns _indexTableSize
// (DB2Store.h), the length of the table indexed by record id - so it only moves when a record above
// the current highest id appears, and EraseRecord nulls a slot without shrinking it. Its companion
// `<Table>HotfixCount` is a true count and covers the usual case, because every DB2 change on this
// core arrives as a hotfix record. QuestObjectiveRecordCount is a true count as well and misses
// only the edit-in-place case; since almost every world side quest change adds or removes an
// objective row, it is the key that actually moves when this core's quest data moves.
//
// What the counts cannot see, the world table `cache_info` covers: it adds realm defined stamps on
// top. That is the only way to reach a domain this core has no retail key for - petitions, which
// live per character in the characters database, and creature, page text or gameobject template
// edits, for which retail has no key because retail has no such tables - and the only way to force
// an invalidation after an edit that moved no count. Bump the Value there and run
// `.reload cache_info`; every client that logs in afterwards discards the domain. A stamp is a hand
// written string, not a live count. That is the price of staying identical to retail by default.
void WorldSession::SendCacheInfo()
{
    struct CacheDomain
    {
        std::string_view Prefix;
        std::span<DB2StorageBase const* const> Stores;
        std::string_view CoreKey;                       ///< retail key this core fills from its own data; empty when there is none
        std::size_t CoreCount;
    };

    static DB2StorageBase const* const gameObjectStores[] = { &sGameObjectsStore };
    static DB2StorageBase const* const questStores[] = { &sQuestV2Store };

    CacheDomain const domains[] =               // recordings send WQST before WGOB
    {
        { "WQST", std::span(questStores),      "QuestObjectiveRecordCount", sObjectMgr->GetQuestObjectiveCount() },
        { "WGOB", std::span(gameObjectStores), {},                          0                                    },
        { "WNPC", {},                          {},                          0                                    },
        { "WPTX", {},                          {},                          0                                    },
        { "WPTN", {},                          {},                          0                                    }
    };

    for (CacheDomain const& domain : domains)
    {
        WorldPackets::ClientConfig::CacheInfo cacheInfo;
        cacheInfo.Prefix = domain.Prefix;

        for (DB2StorageBase const* store : domain.Stores)
        {
            std::string_view tableName = store->GetFileName();
            tableName.remove_suffix(std::string_view(".db2").length());

            // Looked up, not counted. DB2Manager keeps this number per table hash and locale while
            // it fills _hotfixData, precisely so that a login does not have to walk every hotfix
            // record of the realm on the map thread for the two hashes it actually asks about.
            uint32 hotfixCount = sDB2Manager.GetHotfixRecordCount(store->GetTableHash(), GetSessionDbcLocale());

            cacheInfo.Entries.push_back({ Trinity::StringFormat("{}RecordCount", tableName), std::to_string(store->GetNumRows()) });
            cacheInfo.Entries.push_back({ Trinity::StringFormat("{}HotfixCount", tableName), std::to_string(hotfixCount) });
        }

        if (!domain.CoreKey.empty())
            cacheInfo.Entries.push_back({ std::string(domain.CoreKey), std::to_string(domain.CoreCount) });

        for (CacheInfoStamp const& stamp : sObjectMgr->GetCacheInfoStamps())
            if (stamp.Prefix == domain.Prefix)
                cacheInfo.Entries.push_back({ stamp.Key, stamp.Value });

        // An empty packet is a no-op for the client, so it is not worth sending. WNPC, WPTX and WPTN
        // reach this point empty unless the realm put a row into `cache_info` for them, which is
        // what keeps the default output identical to the two packets retail sends.
        if (cacheInfo.Entries.empty())
            continue;

        SendPacket(cacheInfo.Write());
    }
}

void WorldSession::SendSetTimeZoneInformation()
{
    Minutes timezoneOffset = Trinity::Timezone::GetSystemZoneOffset(false);
    std::string realTimezone = Trinity::Timezone::GetSystemZoneName();
    std::string_view clientSupportedTZ = Trinity::Timezone::FindClosestClientSupportedTimezone(realTimezone, timezoneOffset);

    WorldPackets::System::SetTimeZoneInformation packet;
    packet.ServerTimeTZ = clientSupportedTZ;
    packet.GameTimeTZ = clientSupportedTZ;
    packet.ServerRegionalTimeTZ = clientSupportedTZ;
    SendPacket(packet.Write());
}

void WorldSession::SendFeatureSystemStatusGlueScreen()
{
    WorldPackets::System::FeatureSystemStatusGlueScreen features;
    features.BpayStoreAvailable = false;
    features.BpayStoreDisabledByParentalControls = false;
    features.CharUndeleteEnabled = sWorld->getBoolConfig(CONFIG_FEATURE_SYSTEM_CHARACTER_UNDELETE_ENABLED);
    features.MaxCharactersOnThisRealm = sWorld->getIntConfig(CONFIG_CHARACTERS_PER_REALM);
    features.MinimumExpansionLevel = EXPANSION_CLASSIC;
    features.MaximumExpansionLevel = sWorld->getIntConfig(CONFIG_EXPANSION);

    features.EuropaTicketSystemStatus.emplace();
    features.EuropaTicketSystemStatus->ThrottleState.MaxTries = 10;
    features.EuropaTicketSystemStatus->ThrottleState.PerMilliseconds = 60000;
    features.EuropaTicketSystemStatus->ThrottleState.TryCount = 1;
    features.EuropaTicketSystemStatus->ThrottleState.LastResetTimeBeforeNow = 111111;
    features.EuropaTicketSystemStatus->TicketsEnabled = sWorld->getBoolConfig(CONFIG_SUPPORT_TICKETS_ENABLED);
    features.EuropaTicketSystemStatus->BugsEnabled = sWorld->getBoolConfig(CONFIG_SUPPORT_BUGS_ENABLED);
    features.EuropaTicketSystemStatus->ComplaintsEnabled = sWorld->getBoolConfig(CONFIG_SUPPORT_COMPLAINTS_ENABLED);
    features.EuropaTicketSystemStatus->SuggestionsEnabled = sWorld->getBoolConfig(CONFIG_SUPPORT_SUGGESTIONS_ENABLED);

    for (World::GameRule const& gameRule : sWorld->GetGameRules())
    {
        WorldPackets::System::GameRuleValuePair& rule = features.GameRules.emplace_back();
        rule.Rule = AsUnderlyingType(gameRule.Rule);
        std::visit([&]<typename T>(T value)
        {
            if constexpr (std::is_same_v<T, float>)
                rule.ValueF = value;
            else
                rule.Value = value;
        }, gameRule.Value);
    }

    // Plunderstorm / WoW Labs (ER-3): on the WoW Labs event realm this session gets the Plunderstorm game rules
    // here at the glue screen too (so C_GameRules.GetActiveGameMode() reads Plunderstorm and the glue shows the
    // Plunderstorm lobby), while the main realm's rules stay standard.
    if (IsOnWowLabsRealm())
    {
        for (::GameRule rule : { ::GameRule::CharacterlessLogin, ::GameRule::MapPlunderstormCircle, ::GameRule::PlunderstormAreaSelection })
        {
            WorldPackets::System::GameRuleValuePair& pair = features.GameRules.emplace_back();
            pair.Rule = AsUnderlyingType(rule);
            pair.Value = 1;
        }
    }

    features.AvailableGameModeIDs.push_back(8); // GameMode.db2, standard
    // Offer Plunderstorm (GameMode.db2 id 9) on the character-select game-mode selector when a WoW Labs event
    // realm is configured; selecting it makes the client C_RealmList.ConnectToEventRealm(plunderStormRealm).
    if (sConfigMgr->GetIntDefault("WowLabs.EventRealmId", 0))
    {
        features.AvailableGameModeIDs.push_back(9); // GameMode.db2, plunderstorm

        // The character-select NavBar (Blizzard_CharacterSelectNavBar.lua) force-shows the game-mode selector when
        // C_GameRules.GetCurrentEventRealmQueues() ~= Enum.EventRealmQueues.None. That value is fed by this glue
        // packet's EventRealmQueues field (a bitmask: Solo=1, Duo=2, Trio=4, Training=8). Advertise which
        // Plunderstorm queues this event realm accepts so the selector actually appears; default Solo|Duo|Trio.
        features.EventRealmQueues = uint32(sConfigMgr->GetIntDefault("WowLabs.EventRealmQueues", 0x7));
    }

    SendPacket(features.Write());

    WorldPackets::System::MirrorVarSingle vars[] =
    {
        { "raidLockoutExtendEnabled"sv, "1"sv },
        { "sellAllJunkEnabled"sv, "1"sv },
        { "bypassItemLevelScalingCode"sv, "0"sv },
        { "shop2Enabled"sv, "0"sv },
        { "bpayStoreEnable"sv, "0"sv },
        { "recentAlliesEnabledClient"sv, "0"sv },
        { "browserEnabled"sv, "0"sv },
        // Housing game rules — ALL values verified against 12.0.1.65940 sniff packet data (Feb 2026)
        // Service & feature flags (read from config, default true)
        { "performHousingExpansionCheckClient"sv, "1"sv },
        { "housingServiceEnabled"sv, "1"sv },
        { "housingEnableBuyHouse"sv, sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_BUY_HOUSE) ? "1"sv : "0"sv },
        { "housingEnableDeleteHouse"sv, sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_DELETE_HOUSE) ? "1"sv : "0"sv },
        { "housingEnableMoveHouse"sv, sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_MOVE_HOUSE) ? "1"sv : "0"sv },
        { "housingEnableCreateCharterNeighborhood"sv, sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_CREATE_CHARTER_NEIGHBORHOOD) ? "1"sv : "0"sv },
        { "housingEnableCreateGuildNeighborhood"sv, sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_CREATE_GUILD_NEIGHBORHOOD) ? "1"sv : "0"sv },
        // Market
        { "housingMarketEnabled"sv, "1"sv },
        { "housingMarketShopEnabled"sv, "1"sv },
        { "housingMarketCartFullRemoveEnabled"sv, "1"sv },
        // Neighborhood & exterior
        { "housingExteriorTypeByNeighborhoodFactionRestriction"sv, "1"sv },
        { "minNeighborhoodGroupMembers"sv, "3"sv },
        // Decoration limits
        { "housingBasicDecor_MaxPreviewLimit"sv, "100"sv },
        { "housingCatalog_CartSizeLimit"sv, "20"sv },
        // Decor scale limits
        { "housingExpertDecor_Scale_Indoor_Min"sv, "0.200000"sv },
        { "housingExpertDecor_Scale_Indoor_Max"sv, "2.000000"sv },
        { "housingExpertDecor_Scale_Outdoor_Min"sv, "0.200000"sv },
        { "housingExpertDecor_Scale_Outdoor_Max"sv, "2.000000"sv },
        // Screenshot report thresholds
        { "housingDecorReportScreenshotFacingDotThreshold"sv, "0.500000"sv },
        { "housingDecorReportScreenshotDistanceThreshold"sv, "150.000000"sv },
        // Market telemetry throttles — sniff-verified against build 12.0.1.66838,
        // SMSG_MIRROR_VARS at packet idx 9976 (dump_12.0.1.66838_2026-04-15_09-35-59).
        // The client reads these before it will send any SMSG_HOUSING_MARKET_*
        // telemetry CMSGs; without them the market UI may throttle-fail silently.
        { "housingMarketViewInStoreTelemThrottle"sv, "5"sv },
        { "housingMarketViewBundleTelemThrottle"sv, "10"sv },
        { "housingMarketAddToCartTelemThrottle"sv, "15"sv },
        { "housingMarketClearCartTelemThrottle"sv, "5"sv },
        { "housingMarketRemoveFromCartTelemThrottle"sv, "20"sv },
        { "housingMarketThrottleTimePeriodMs"sv, "10000"sv },
        // Situation flags — driver context for the client's "situation" state
        // machine (automatic/manual triggered events). Retail sends all three
        // set on login; we were sending none. Same sniff reference.
        { "enableAutomaticSituations"sv, "1"sv },
        { "enableManualSituations"sv, "1"sv },
        { "enableTransmogUpdateSituation"sv, "1"sv },
        // Transmog system flags — the client gates parts of the transmog UI
        // on these being present. Retail sends them at this stage of login.
        { "transmogEnableSystem"sv, "1"sv },
        { "transmogAllowArtifactOverride"sv, "1"sv },
        { "transmogAllowCanUseEverChanges"sv, "0"sv },
        { "transmogEnableOutfitPurchases"sv, "1"sv },
        { "transmogEnableOutfitSlotChanges"sv, "1"sv },
        // --- Additional vars from retail MIRROR_VARS audit (2026-04-21) ---
        // Retail sends 116 MIRROR_VARS entries, we were sending 41. These 40
        // below are the subset that drive client behaviour without requiring
        // Blizzard-specific service URLs (shop2 / Pinterest skipped).
        // Damage meter — gates the in-game damage meter addon feature
        { "damageMeterCacheEnabled"sv, "1"sv },
        { "damageMeterProcessingEnabled"sv, "1"sv },
        // Addon chat restrictions — affects WHISPER/GROUP addon message routing
        { "addonChatRestrictionsEnabled"sv, "1"sv },
        { "addonChatRestrictionsEnabledForOutgoingAddonMessages"sv, "1"sv },
        // Lua resource caps — the client throttles AddOn resources when these
        // are present. Retail's caps, keep identical to not break addons.
        { "limitedLuaResourcesEnabled"sv, "0"sv },
        { "limitedLuaResourcesAddonCapacityAnim"sv, "5000"sv },
        { "limitedLuaResourcesAddonCapacityAnimGroup"sv, "2000"sv },
        { "limitedLuaResourcesAddonCapacityFont"sv, "300"sv },
        { "limitedLuaResourcesAddonCapacityFontString"sv, "5000"sv },
        { "limitedLuaResourcesAddonCapacityFrame"sv, "10000"sv },
        { "limitedLuaResourcesAddonCapacityTexture"sv, "40000"sv },
        { "limitedLuaResourcesAddonCapacityTimer"sv, "500"sv },
        { "limitedLuaResourcesGlobalCapacityAnim"sv, "50000"sv },
        { "limitedLuaResourcesGlobalCapacityAnimGroup"sv, "20000"sv },
        { "limitedLuaResourcesGlobalCapacityFont"sv, "3000"sv },
        { "limitedLuaResourcesGlobalCapacityFontString"sv, "50000"sv },
        { "limitedLuaResourcesGlobalCapacityFrame"sv, "100000"sv },
        { "limitedLuaResourcesGlobalCapacityTexture"sv, "400000"sv },
        { "limitedLuaResourcesGlobalCapacityTimer"sv, "500"sv },
        // Lua script throttling — bucket limits per second / burst
        { "luaScriptBucketThrottleEnabled"sv, "1"sv },
        { "luaScriptBucketThrottleMaxMsBurstNormal"sv, "20000"sv },
        { "luaScriptBucketThrottleMaxMsBurstRestricted"sv, "1000"sv },
        { "luaScriptBucketThrottleMaxMsPerSecondNormal"sv, "2000"sv },
        { "luaScriptBucketThrottleMaxMsPerSecondRestricted"sv, "500"sv },
        // Hardcore mode throttling — not used but sent for parity
        { "hardcoreScriptThrottlingEnabled"sv, "0"sv },
        // PvP training grounds — the PvP duel area feature
        { "pvpTrainingGroundsEnabledClient"sv, "0"sv },
        // Recent allies request throttle
        { "recentAlliesRequestDataThrottle"sv, "5000"sv },
        // LFG text filters
        { "enableEndgameEditRestrictionsForLFGText"sv, "1"sv },
        // Disabled game modes (retail passes an empty string; keep empty)
        { "disabledGamemodes"sv, ""sv },
    };

    WorldPackets::System::MirrorVars variables;
    variables.Variables = vars;
    SendPacket(variables.Write());

    // Plunderstorm / WoW Labs (ER-3): point the character-select selector at the event realm. The client CVar is
    // named "plunderstormRealm" and holds a REALM_ADDRESS ("REALM_ADDRESS to connect to when pressing the
    // Plunderstorm button" - confirmed from the client's own CVar registration), which the selector feeds to
    // C_RealmList.ConnectToEventRealm([eventRealmAddress]). So push the event realm's virtual realm address
    // (Region<<24 | Site<<16 | realmId), computed from this realm's handle + the event realm id (same bnet
    // region/site as this realm, which ER-2's cloned realmlist row uses).
    if (int32 const eventRealmId = sConfigMgr->GetIntDefault("WowLabs.EventRealmId", 0))
    {
        Battlenet::RealmHandle const mainId = sRealmList->GetCurrentRealmId();
        uint32 const eventAddress = Battlenet::RealmHandle(mainId.Region, mainId.Site, uint32(eventRealmId)).GetAddress();
        std::string const eventAddressStr = std::to_string(eventAddress);
        WorldPackets::System::MirrorVarSingle eventVars[] =
        {
            { "plunderstormRealm"sv, eventAddressStr },
        };
        WorldPackets::System::MirrorVars eventVariables;
        eventVariables.Variables = eventVars;
        SendPacket(eventVariables.Write());
    }

    TC_LOG_INFO("housing", "<<< SMSG_MIRROR_VARS sent: housingServiceEnabled=1, MaxExpansionLevel={}, AccountExpansion={}",
        sWorld->getIntConfig(CONFIG_EXPANSION), GetAccountExpansion());
}

// CMSG_LATENCY_REPORT (12.1 0x44000F). The client sends these self-timed from C++ in triples ~200 ms apart, with
// Kind 0/1/2 carrying 3/7/11 entries; the coupling is exception-free over the corpus's 4522 packets, and the entry
// list is cumulative - the Kind 2 body starts byte-identically with the whole Kind 1 body, which in turn starts with
// the whole Kind 0 body. (The corpus is defined once above WorldPackets::Auth::SuspendComms in
// AuthenticationPackets.h, and every figure in this function is measured on it.)
// Taking only Kind 2 therefore keeps every measurement of the triple exactly once; taking all
// three would count every measurement three times (finding K3 of AGENT_BRIEF_CONN_44_4C).
//
// D2/D4 - what the server does with it: nothing observable by the client. There is no reply opcode, no Lua event
// (the report has no Lua trigger and no Lua reader; only the display getters GetNetStats/GetFramerate exist) and no
// CVar that gates it, so anything the server sends back would be invented. Decision O3 of unit conn_44_4C: the
// values are aggregated into per-session statistics and deliberately NOT persisted. A table would take roughly
// 20000 rows per play session with no retention strategy.
//
// The aggregate has exactly one consumer, and it is server side: WorldSession::~WorldSession writes one summary
// line per session that reported anything, through GetClientPerformanceStats(). That line is visible in the
// shipped configuration - logger network.telemetry is enabled at Info in worldserver.conf.dist - because a
// handler whose only effect needs the operator to lower a log level first has no effect. The per report line
// below stays at Trace: it is detail for someone chasing one session, not something a realm wants every few
// seconds per player.
void WorldSession::HandleLatencyReport(WorldPackets::Auth::LatencyReport const& latencyReport)
{
    // Deduplication. 2 is the last and largest stage of the triple.
    if (latencyReport.Kind != 2)
        return;

    ++_clientPerformanceStats.Reports;

    for (WorldPackets::Auth::LatencyReportEntry const& entry : latencyReport.Entries)
    {
        // The client's clock, taken from EVERY entry - deliberately BEFORE the frame rate filter below.
        // LastTimestampMS is declared as the newest timestamp seen and it is the one member of this aggregate that
        // says something about the client's clock instead of its frame rate, so it must not inherit the frame
        // rate's selection: an entry with Frame == 0 still carries a perfectly good timestamp. Over the corpus -
        // 4522 packets, 31 650 entries, 9052 of them with Frame == 0 - not one Frame == 0 entry has
        // TimestampMS == 0, so there is nothing here to guard against.
        // A maximum rather than "the last one written", because the entries of one packet are NOT in timestamp
        // order: the entry the sender appends itself (Unknown8 == 33) is ALWAYS older than the entry before it -
        // 9043 of the 27 128 consecutive within-packet pairs go backwards, that is exactly the number of
        // Unknown8 == 33 entries, and in all 9043 the later entry of the pair IS the ==33 one. Across packets
        // the per-packet maximum IS monotonic (4498 comparisons, 0 violations), which is what makes this
        // accumulator meaningful at all.
        if (entry.TimestampMS > _clientPerformanceStats.LastTimestampMS)
            _clientPerformanceStats.LastTimestampMS = entry.TimestampMS;

        // Frame == 0 means this entry carries no frame rate. It happens: in the decoded reference triple entry 0
        // has Frame 0 while entries 1 and 2 have 21 and 33.
        // The report is built out of one BLOCK per stage, appended to the cumulative list, and over the corpus the
        // block has a fixed shape at both ends - measured, without exception: it BEGINS with a Frame == 0 entry and
        // ENDS with the entry the sender we decoded appends itself (Unknown8 == 33, Server == 0, Unknown4 == 0).
        // Kind 0 contributes indices 0..2, Kind 1 appends 3..6, Kind 2 appends 7..10, so the corpus holds
        // 1*1508 + 2*1507 + 3*1507 = 9043 blocks; Unknown8 == 33 occurs 9043 times and only ever at a block's last
        // index (1508 at index 2 for Kind 0; 1507 each at 2 and 6 for Kind 1; 1507 each at 2, 6 and 10 for Kind 2),
        // and Frame == 0 occurs at a block's FIRST index 9043 times - once per block.
        // So an earlier revision's "only the last entry of a packet is written by the sender we decoded" does not
        // account for the ==33 entries of a multi-stage packet: it covers 4522 of 9043 of them. The rest are the
        // block-closing entries of the EARLIER stages, carried along because the list is cumulative. Consistently
        // with 0x20E6F0 always storing at least 1, not one of those 9043 entries has Frame == 0.
        // UNVERIFIED: which function opens a block, i.e. the producer of the Frame == 0 entry. Its POSITION is
        // settled by the measurement above, its identity is not. Their frame rate is skipped rather than averaged
        // in, because folding a zero into a frame rate average would understate it. Their timestamp is NOT skipped
        // - see above. The 9 remaining Frame == 0 entries (9052 in total against 9043 blocks) sit in a block's
        // interior, on the Unknown8 == 0 slot: 3 Kind 1 packets carry one at index 4, and 3 Kind 2 packets carry
        // one at index 4 and one at index 8. The skip covers them the same way.
        if (!entry.Frame)
            continue;

        ++_clientPerformanceStats.Samples;
        _clientPerformanceStats.FrameRateSum += entry.Frame;

        // "Last" is meant as the NEWEST measurement, so it is selected by timestamp - the same selection as
        // LastTimestampMS above, for the same reason, and it has to be made separately because the filter this
        // branch sits behind makes the two selections range over different entries.
        // Assigning in iteration order instead, as an earlier revision of this handler did, hands the value to
        // the last entry that passes the filter, and that entry is not merely sometimes stale - it is the
        // block-closing Unknown8 == 33 at index 10 in 1507 of the corpus's 1507 accepted packets, it never has
        // Frame == 0 so it always passes, and by the measurement above it is older than the entry before it in
        // all 1507. So the iteration-order value is the newest measurement in 0 of 1507 packets. The two rules
        // disagree in 1440 of the 1507 (95.6%), by a median of 5 and up to 192 frames per second: not a rounding
        // difference in a diagnostic, a different measurement.
        // >= and not >, i.e. a tie goes to the LATER entry: indices 8 and 9 carry the same millisecond in 743 of
        // the 1507 packets, and in 733 of those their frame rates differ - median 31 apart - so the tie is real
        // and has to be broken on purpose rather than by accident. With this rule the entry chosen is index 9 in
        // 1507 of 1507.
        // UNVERIFIED: that the later of two equally stamped entries is the better of the two. Nothing available
        // orders them - both sit in the interior of the last stage's block, the timestamps are equal at the
        // millisecond the field resolves, and the only thing telling them apart is Unknown8 (0 at index 8, 12 at
        // index 9), whose meaning is itself unverified. What IS measured is that either of them beats index 10,
        // which is the whole point of this selection.
        if (entry.TimestampMS >= _clientPerformanceStats.LastFrameRateTimestampMS)
        {
            _clientPerformanceStats.LastFrameRateTimestampMS = entry.TimestampMS;
            _clientPerformanceStats.LastFrameRate = entry.Frame;
        }

        if (!_clientPerformanceStats.MinFrameRate || entry.Frame < _clientPerformanceStats.MinFrameRate)
            _clientPerformanceStats.MinFrameRate = entry.Frame;

        if (entry.Frame > _clientPerformanceStats.MaxFrameRate)
            _clientPerformanceStats.MaxFrameRate = entry.Frame;
    }

    TC_LOG_TRACE("network.telemetry", "WorldSession::HandleLatencyReport: {} reported {} entries (fps last {} min {} max {} avg {}, {} samples over {} reports)",
        GetPlayerInfo(), latencyReport.Entries.size(), _clientPerformanceStats.LastFrameRate, _clientPerformanceStats.MinFrameRate,
        _clientPerformanceStats.MaxFrameRate, _clientPerformanceStats.AverageFrameRate(), _clientPerformanceStats.Samples,
        _clientPerformanceStats.Reports);
}

// Makes an arbitrary byte string off the wire safe to hand to TC_LOG_*, for two separate reasons.
//
// Control characters collapse to '.' so that a crafted message cannot forge log lines - the message is
// attacker-controlled and would otherwise reach the log verbatim.
//
// Every byte that is not part of a valid UTF-8 sequence is escaped as \xNN, because the log pipeline DOES
// interpret the text even though nothing in the handler does. On Windows - the platform of this tree - the
// console appender hands prefix + text + "\n" to WriteWinConsole (AppenderConsole.cpp), which calls Utf8toWStr
// first and returns without writing a single character as soon as utf8::utf8to16 throws (Util.cpp: the exception
// is caught, wstr is cleared, false is returned). WriteConsoleW is never reached, so the entire line is lost
// silently - no prefix, no truncation marker, nothing in the log saying a report was dropped - while the file
// appender writes the same line raw (AppenderFile.cpp, fwrite). The shipped configuration has both appenders on
// this logger (worldserver.conf.dist: Logger.network.telemetry=3,Console Server), so half of it would go blind,
// and since logging IS the whole effect of this opcode (decision O4) that is the effect going missing.
// It matters here specifically: CMSG_LOG_STREAMING_ERROR is the one client string in this tree that is read with
// Strings::DontValidateUtf8 (see LogStreamingError::Read) - every other logged client string comes in under the
// Strings::ValidUtf8 default and cannot carry an invalid sequence at all. Reading it unvalidated stays the right
// call, a malformed diagnostic must not cost the client its session, but it only holds if the string is made
// loggable before it is logged, and that is what this function is for.
//
// \xNN rather than U+FFFD, and rather than the '.' the control characters get: the payload is a vsnprintf result
// out of the client's CASC layer that can legitimately carry binary key material, and for a diagnostic the byte
// value is the interesting part. Collapsing distinct bytes into one glyph would throw away the only content such
// a report has. utf8::find_invalid returns the first byte of the offending sequence, so a multi-byte sequence
// truncated at the 511 byte message boundary comes out as one escape per byte instead of swallowing the rest.
static std::string MakeStreamingErrorMessageLoggable(std::string const& raw)
{
    std::string message;
    message.reserve(raw.length());

    std::string::const_iterator itr = raw.begin();
    while (itr != raw.end())
    {
        std::string::const_iterator invalid = utf8::find_invalid(itr, raw.end());
        for (; itr != invalid; ++itr)
        {
            unsigned char const c = static_cast<unsigned char>(*itr);
            message += (c < 0x20 || c == 0x7F) ? '.' : *itr;
        }

        if (itr == raw.end())
            break;

        message += Trinity::StringFormat(R"(\x{:02X})", static_cast<unsigned char>(*itr));
        ++itr;
    }

    return message;
}

// CMSG_LOG_STREAMING_ERROR (12.1 0x44000B). Free-form English CASC/TACT error text out of the client's streaming
// logger; only severity >= 4 messages reach the ring this is drained from. There is nothing structured in it - no
// file name field, no FileDataID field, no error code - and the Streaming Lua API has an empty Events table, so
// there is no client state to change and no reply to send. Logging it is the whole of the effect (decision O4),
// which is why the line goes to network.telemetry: worldserver.conf.dist enables that logger at Info, so it is
// actually written in the shipped configuration. On the "network" logger it would fall through to Logger.root=5
// (Error) and be discarded - the handler would then have no effect at all, which is exactly the empty handler
// this opcode was implemented to avoid.
//
// The message is attacker-controlled and the client keeps a 64 slot error ring it can drain in a burst, so the
// output is capped per session and the text goes through MakeStreamingErrorMessageLoggable above - which both
// keeps a crafted message from forging log lines and keeps the line from vanishing in the console appender.
// What the cap suppresses is not lost: the reports keep being counted and ~WorldSession logs the total, so the
// counter has a reader for every value it can take.
void WorldSession::HandleLogStreamingError(WorldPackets::Auth::LogStreamingError const& logStreamingError)
{
    if (_streamingErrorsReported >= MaxStreamingErrorsLoggedPerSession)
    {
        ++_streamingErrorsReported;
        return;
    }

    std::string const message = MakeStreamingErrorMessageLoggable(logStreamingError.Message);

    ++_streamingErrorsReported;

    TC_LOG_INFO("network.telemetry", "WorldSession::HandleLogStreamingError: {} reported a content streaming error ({}/{}): {}",
        GetPlayerInfo(), _streamingErrorsReported, MaxStreamingErrorsLoggedPerSession, message);

    if (_streamingErrorsReported == MaxStreamingErrorsLoggedPerSession)
        TC_LOG_INFO("network.telemetry", "WorldSession::HandleLogStreamingError: {} reached the per session streaming error log cap, further reports are counted but not logged - ~WorldSession prints the total",
            GetPlayerInfo());
}
