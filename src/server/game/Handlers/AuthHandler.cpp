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
#include "ClientConfigPackets.h"
#include "DB2Stores.h"
#include "DisableMgr.h"
#include "GameTime.h"
#include "MapUtils.h"
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

        response.SuccessInfo->AvailableClasses = &sObjectMgr->GetClassExpansionRequirements();

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

// SMSG_CACHE_INFO, one packet per cache domain. Structure and key space are taken from live
// 12.1 recordings (26 of 26 packets parse to the exact packet length): every entry is
// "<DB2TableName>RecordCount" / "<DB2TableName>HotfixCount" for a table that feeds the domain.
// The client stores each value in the CVar "CACHE-<Prefix>-<Key>"; on the first difference it
// discards the whole cache behind Prefix and re-queries it (handler RVA 0x341AD0).
// Observed domains: WGOB (GameObjects) and WQST (QuestV2, QuestObjective, QuestObjectiveXEffect).
// QuestObjective and QuestObjectiveXEffect are not DB2 stores in TrinityCore - quest objectives
// live in the world database - so only the tables this core actually owns are reported.
// The client also knows WNPC, WPTX and WPTN (MatchesPrefix bodies at RVA 0x331100..0x331580);
// no recording shows which keys the retail server sends for them, so they stay unpopulated.
void WorldSession::SendCacheInfo()
{
    struct CacheDomain
    {
        std::string_view Prefix;
        std::span<DB2StorageBase const* const> Stores;
    };

    static DB2StorageBase const* const gameObjectStores[] = { &sGameObjectsStore };
    static DB2StorageBase const* const questStores[] = { &sQuestV2Store };
    static CacheDomain const domains[] =        // recordings send WQST before WGOB
    {
        { "WQST", std::span(questStores)      },
        { "WGOB", std::span(gameObjectStores) }
    };

    uint32 localeMask = 1 << GetSessionDbcLocale();

    // one pass over the hotfix data, counting per table hash - it is walked once for all domains
    std::unordered_map<uint32, uint32> hotfixCountByTableHash;
    for (auto const& [pushId, push] : sDB2Manager.GetHotfixData())
        for (DB2Manager::HotfixRecord const& record : push.Records)
            if (record.AvailableLocalesMask & localeMask)
                ++hotfixCountByTableHash[record.TableHash];

    for (CacheDomain const& domain : domains)
    {
        WorldPackets::ClientConfig::CacheInfo cacheInfo;
        cacheInfo.Prefix = domain.Prefix;
        cacheInfo.Entries.reserve(domain.Stores.size() * 2);

        for (DB2StorageBase const* store : domain.Stores)
        {
            std::string_view tableName = store->GetFileName();
            tableName.remove_suffix(std::string_view(".db2").length());

            uint32 const* hotfixCount = Trinity::Containers::MapGetValuePtr(hotfixCountByTableHash, store->GetTableHash());

            cacheInfo.Entries.push_back({ Trinity::StringFormat("{}RecordCount", tableName), std::to_string(store->GetNumRows()) });
            cacheInfo.Entries.push_back({ Trinity::StringFormat("{}HotfixCount", tableName), std::to_string(hotfixCount ? *hotfixCount : 0) });
        }

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
