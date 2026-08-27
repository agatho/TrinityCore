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
#include "Log.h"
#include "CharacterTemplateDataStore.h"
#include "ClientConfigPackets.h"
#include "DisableMgr.h"
#include "GameTime.h"
#include "ObjectMgr.h"
#include "RBAC.h"
#include "RealmList.h"
#include "SystemPackets.h"
#include "Timezone.h"
#include "Util.h"
#include "World.h"

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

    features.AvailableGameModeIDs.push_back(8); // GameMode.db2, standard

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
        { "housingEnableCreateGuildNeighborhood"sv, "0"sv },
        { "housingEnableDeleteHouse"sv, "0"sv },
        { "housingServiceEnabled"sv, "0"sv },
        { "housingEnableMoveHouse"sv, "0"sv },
        { "housingEnableCreateCharterNeighborhood"sv, "0"sv },
        { "housingEnableBuyHouse"sv, "0"sv },
        { "housingMarketEnabled"sv, "0"sv },
    };

    WorldPackets::System::MirrorVars variables;
    variables.Variables = vars;
    SendPacket(variables.Write());
}

// CMSG_LATENCY_REPORT (12.1 0x44000F). The client sends these self-timed from C++ in triples ~200 ms apart, with
// Kind 0/1/2 carrying 3/7/11 entries; the coupling is exception-free over 4522 captured packets and the entry list
// is cumulative - the Kind 2 body starts byte-identically with the whole Kind 1 body, which in turn starts with the
// whole Kind 0 body. Taking only Kind 2 therefore keeps every measurement of the triple exactly once; taking all
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
        // Frame == 0 means this entry carries no frame rate. It happens: in the decoded reference triple entry 0
        // has Frame 0 while entries 1 and 2 have 21 and 33. Only the last entry of a packet is written by the
        // sender we decoded (0x20E6F0, which always stores at least 1); who fills the earlier entries of the same
        // report object is not resolved.
        // UNVERIFIED: the producer of the entries with Frame == 0. They are skipped rather than averaged in,
        // because folding a zero into a frame rate average would understate it.
        if (!entry.Frame)
            continue;

        ++_clientPerformanceStats.Samples;
        _clientPerformanceStats.FrameRateSum += entry.Frame;
        _clientPerformanceStats.LastFrameRate = entry.Frame;

        if (!_clientPerformanceStats.MinFrameRate || entry.Frame < _clientPerformanceStats.MinFrameRate)
            _clientPerformanceStats.MinFrameRate = entry.Frame;

        if (entry.Frame > _clientPerformanceStats.MaxFrameRate)
            _clientPerformanceStats.MaxFrameRate = entry.Frame;

        if (entry.TimestampMS > _clientPerformanceStats.LastTimestampMS)
            _clientPerformanceStats.LastTimestampMS = entry.TimestampMS;
    }

    TC_LOG_TRACE("network.telemetry", "WorldSession::HandleLatencyReport: {} reported {} entries (fps last {} min {} max {} avg {}, {} samples over {} reports)",
        GetPlayerInfo(), latencyReport.Entries.size(), _clientPerformanceStats.LastFrameRate, _clientPerformanceStats.MinFrameRate,
        _clientPerformanceStats.MaxFrameRate, _clientPerformanceStats.AverageFrameRate(), _clientPerformanceStats.Samples,
        _clientPerformanceStats.Reports);
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
// output is capped per session and control characters are stripped so a crafted message cannot forge log lines.
// What the cap suppresses is not lost: the reports keep being counted and ~WorldSession logs the total, so the
// counter has a reader for every value it can take.
void WorldSession::HandleLogStreamingError(WorldPackets::Auth::LogStreamingError const& logStreamingError)
{
    if (_streamingErrorsReported >= MaxStreamingErrorsLoggedPerSession)
    {
        ++_streamingErrorsReported;
        return;
    }

    std::string message = logStreamingError.Message;
    for (char& c : message)
        if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) == 0x7F)
            c = '.';

    ++_streamingErrorsReported;

    TC_LOG_INFO("network.telemetry", "WorldSession::HandleLogStreamingError: {} reported a content streaming error ({}/{}): {}",
        GetPlayerInfo(), _streamingErrorsReported, MaxStreamingErrorsLoggedPerSession, message);

    if (_streamingErrorsReported == MaxStreamingErrorsLoggedPerSession)
        TC_LOG_INFO("network.telemetry", "WorldSession::HandleLogStreamingError: {} reached the per session streaming error log cap, further reports are counted but not logged - ~WorldSession prints the total",
            GetPlayerInfo());
}
