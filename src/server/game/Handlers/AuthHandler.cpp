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
