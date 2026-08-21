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
#include "ObjectMgr.h"
#include "RBAC.h"
#include "RealmList.h"
#include "SystemPackets.h"
#include "Timezone.h"
#include "Util.h"
#include "World.h"
#include <algorithm>
#include <array>
#include <span>

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

// SMSG_CACHE_INFO, one packet per cache domain. The client turns every entry into the CVar
// "CACHE-<Prefix>-<Key>" (format string RVA 0x3B6AA20) and compares the value case insensitively
// against what it stored; on the first difference it discards the whole cache behind Prefix and
// re-queries it (handler RVA 0x341AD0). A packet with no entries makes it do nothing at all.
//
// The Key space belongs to the server alone. There is exactly one "CACHE-" string in the whole
// 69382 binary - that format string - and no CVar list carries a CACHE- entry, so the client has no
// static idea which keys exist. What is closed is the PREFIX list, not the key list: only WGOB,
// WNPC, WQST, WPTX and WPTN are matched (MatchesPrefix bodies at RVA 0x331100..0x331580), any other
// prefix writes a CVar and discards nothing. All five are served below.
//
// The values are counts of the data behind each domain, which is what makes them useful: they move
// exactly when the cached data moves. Where this core owns the data in its world database, the
// count comes from there; where the data is a DB2, it comes from the store plus its hotfix count;
// and the world table `cache_info` adds realm defined stamps on top, which is the only way to reach
// a domain the core has no static count for - petitions, which live per character in the characters
// database. A single hand written row there also lets an administrator force an invalidation after
// an out of band change such as `.reload page_text`.
void WorldSession::SendCacheInfo()
{
    struct CacheDomain
    {
        std::string_view Prefix;
        std::span<DB2StorageBase const* const> Stores;
        std::string_view WorldTableKey;                 ///< empty when this core owns no such table
        std::size_t WorldTableRows;
    };

    static DB2StorageBase const* const gameObjectStores[] = { &sGameObjectsStore };
    static DB2StorageBase const* const questStores[] = { &sQuestV2Store };

    CacheDomain const domains[] =               // recordings send WQST before WGOB
    {
        { "WQST", std::span(questStores),      "QuestTemplateCount",      sObjectMgr->GetQuestTemplates().size()      },
        { "WGOB", std::span(gameObjectStores), "GameObjectTemplateCount", sObjectMgr->GetGameObjectTemplates().size() },
        { "WNPC", {},                          "CreatureTemplateCount",   sObjectMgr->GetCreatureTemplates().size()   },
        { "WPTX", {},                          "PageTextCount",           sObjectMgr->GetPageTexts().size()           },
        { "WPTN", {},                          {},                        0                                          }
    };

    // Only two table hashes are ever asked for, so the filter goes in front of the counting instead
    // of behind it - HandleHotfixRequest does the same with the push ids it was given. Without it
    // every login walks every hotfix record of the realm on the map thread just to throw almost all
    // of them away.
    std::array<uint32, 2> const countedTableHashes = { sQuestV2Store.GetTableHash(), sGameObjectsStore.GetTableHash() };
    std::array<uint32, 2> hotfixCounts = { };

    uint32 localeMask = 1 << GetSessionDbcLocale();
    for (auto const& [pushId, push] : sDB2Manager.GetHotfixData())
    {
        for (DB2Manager::HotfixRecord const& record : push.Records)
        {
            if (!(record.AvailableLocalesMask & localeMask))
                continue;

            auto itr = std::ranges::find(countedTableHashes, record.TableHash);
            if (itr != countedTableHashes.end())
                ++hotfixCounts[std::distance(countedTableHashes.begin(), itr)];
        }
    }

    for (CacheDomain const& domain : domains)
    {
        WorldPackets::ClientConfig::CacheInfo cacheInfo;
        cacheInfo.Prefix = domain.Prefix;

        for (DB2StorageBase const* store : domain.Stores)
        {
            std::string_view tableName = store->GetFileName();
            tableName.remove_suffix(std::string_view(".db2").length());

            auto itr = std::ranges::find(countedTableHashes, store->GetTableHash());
            uint32 hotfixCount = itr != countedTableHashes.end()
                ? hotfixCounts[std::distance(countedTableHashes.begin(), itr)] : 0;

            cacheInfo.Entries.push_back({ Trinity::StringFormat("{}RecordCount", tableName), std::to_string(store->GetNumRows()) });
            cacheInfo.Entries.push_back({ Trinity::StringFormat("{}HotfixCount", tableName), std::to_string(hotfixCount) });
        }

        if (!domain.WorldTableKey.empty())
            cacheInfo.Entries.push_back({ std::string(domain.WorldTableKey), std::to_string(domain.WorldTableRows) });

        for (CacheInfoStamp const& stamp : sObjectMgr->GetCacheInfoStamps())
            if (stamp.Prefix == domain.Prefix)
                cacheInfo.Entries.push_back({ stamp.Key, stamp.Value });

        // An empty packet is a no-op for the client, so it is not worth sending. WPTN reaches this
        // point empty unless the realm put a row into `cache_info` for it.
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
