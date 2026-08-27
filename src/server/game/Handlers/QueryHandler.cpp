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
#include "Common.h"
#include "Corpse.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "GameTime.h"
#include "Item.h"
#include "Log.h"
#include "Map.h"
#include "NPCHandler.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "CharacterCache.h"
#include "ClubUtils.h"
#include "Player.h"
#include "QueryPackets.h"
#include "RealmList.h"
#include "TerrainMgr.h"
#include "Transport.h"
#include "World.h"

void WorldSession::BuildNameQueryData(ObjectGuid guid, WorldPackets::Query::NameCacheLookupResult& lookupData)
{
    Player* player = ObjectAccessor::FindConnectedPlayer(guid);

    lookupData.Player = guid;

    lookupData.Data.emplace();
    if (lookupData.Data->Initialize(guid, player))
        lookupData.Result = RESPONSE_SUCCESS; // name known
    else
        lookupData.Result = RESPONSE_FAILURE; // name unknown
}

void WorldSession::HandleQueryPlayerNames(WorldPackets::Query::QueryPlayerNames& queryPlayerNames)
{
    WorldPackets::Query::QueryPlayerNamesResponse response;
    for (ObjectGuid guid : queryPlayerNames.Players)
        BuildNameQueryData(guid, response.Players.emplace_back());

    SendPacket(response.Write());
}

// The Communities and Channel member lists identify a club member only by the pair
// { BNet account guid, club member id } - never by character guid - so the ordinary CMSG_QUERY_PLAYER_NAMES path
// cannot resolve them. Without an answer C_Club.AreMembersReady stays false forever: the member list is hidden and
// the spinner runs unbounded, because CommunitiesMemberList.lua has no timer, no retry and no error dialog.
//
// The trigger is a cache miss in the client's CommunityNameCache (DBCacheCommunityName.cpp), reached from
// C_Club.FocusMembers - CommunitiesMemberList.lua:498-511 and ChannelFrame.lua:103.
// D5 note: that Lua-to-opcode mapping is derived from the cache architecture plus the only Lua entry point that
// requests member data, not measured through a call chain - the client's CMSG senders are reached through vtables.
// The wire format itself is measured (client writers 0x5D5C40 / 0x5D5EC0, response parser case 0x67C6ED).
void WorldSession::SendPlayerNameByCommunityId(WorldPackets::Query::BNetAccountAndCommunityID const& member)
{
    WorldPackets::Query::QueryPlayerNameByCommunityIdResponse response;
    // Echoing the key back verbatim is what the client matches the answer on. It also satisfies a hard client side
    // precondition for free: consumer 0x3498F0 throws the guid away and substitutes an empty one unless its
    // HighGuid is BNetAccount (30) or Null, so the guid in the answer must never be synthesized from something
    // else - keep the echo.
    response.Member = member;

    ObjectGuid guid = Battlenet::Services::Clubs::GetGuidFromClubMemberId(member.CommunityID);
    if (guid.IsEmpty())
    {
        // The member id was minted on another realm. Nothing here will ever resolve it, so the permanent answer is
        // the correct one - the client may burn the entry.
        response.Result = WorldPackets::Query::QueryPlayerNameByCommunityIdResponse::PermanentFailure;
    }
    else if (!sCharacterCache->GetCharacterCacheByGuid(guid))
    {
        // Right realm, but no such character. Also permanent.
        response.Result = WorldPackets::Query::QueryPlayerNameByCommunityIdResponse::PermanentFailure;
    }
    else
    {
        // Pass the online player through exactly like BuildNameQueryData does. Without it the answer would be
        // built from cache values only, which for a logged in character means no TimerunningSeasonID, no
        // DeclinedNames and a level that can be stale - a real divergence from the sibling path.
        Player const* player = ObjectAccessor::FindConnectedPlayer(guid);
        if (response.Data.emplace().Initialize(guid, player))
            response.Result = WorldPackets::Query::QueryPlayerNameByCommunityIdResponse::Success;
        else
        {
            // The cache entry existed a moment ago and Initialize still failed, so this is a transient state and
            // NOT a permanent negative. Result 1 here would brand the member negative in the client's cache and
            // leave that player nameless for the rest of the session; Result 2 only clears the pending bit and
            // lets the client ask again (consumer 0x3498F0).
            response.Data.reset();
            response.Result = WorldPackets::Query::QueryPlayerNameByCommunityIdResponse::TemporaryFailure;
        }
    }

    SendPacket(response.Write());
}

void WorldSession::HandleQueryPlayerNameByCommunityId(WorldPackets::Query::QueryPlayerNameByCommunityId& queryPlayerNameByCommunityId)
{
    SendPlayerNameByCommunityId(queryPlayerNameByCommunityId.Member);
}

void WorldSession::HandleQueryPlayerNamesForCommunity(WorldPackets::Query::QueryPlayerNamesForCommunity& queryPlayerNamesForCommunity)
{
    // There is no batched response opcode. Checked against the 12.1 opcode table rather than inherited as a claim:
    // Opcodes.h contains SMSG_QUERY_PLAYER_NAME_BY_COMMUNITY_ID_RESPONSE (0x64000B) and no collective counterpart.
    // The client sorts the answers out through the key each response echoes, so one response per requested member
    // is the only form the 12.1 opcode set allows. The client's bulk observable is CLUB_MEMBERS_UPDATED, which the
    // club subsystem raises once its cache has filled.
    // ClubID is not needed to answer - every member id already carries its own realm and character counter - but it
    // is read because it is on the wire.
    //
    // EVERY requested member gets an answer. That is not a nicety: a member the server answers with nothing at all
    // stays pending in the client's community name cache, C_Club.AreMembersReady never turns true, and
    // CommunitiesMemberList.lua has no timer, no retry and no error dialog to get out of it - one silent omission
    // keeps the whole member list spinning.
    //
    // Bounded is only the expensive half. MaxResponsesPerRequest members are resolved and answered in full; beyond
    // that the answer is a bare negative - 11..27 bytes, Result plus the echoed key, no PlayerGuidLookupData - and
    // that is enough, because clearing the pending bit is what consumer 0x3498F0 does for any non-zero Result.
    // PermanentFailure and deliberately not TemporaryFailure: Result 2 invites the client to ask again, so the
    // surplus of an unchanged roster would be refused again on every retry - that retry loop is why the previous
    // TemporaryFailure overflow path was removed. Result 1 terminates: those members render nameless, and the list
    // becomes ready. A nameless entry is a visible, bounded degradation; a pending entry blocks the entire list.
    //
    // The surplus is reachable on THIS server, which is why it is answered instead of dropped: the budget is the
    // client's club capacity (1000), but TrinityCore enforces no guild member limit whatsoever - no
    // MAX_GUILD_MEMBERS in Guild.h/.cpp, no switch in worldserver.conf.dist - and a cold name cache makes the
    // client ask for the entire roster in one message. A 1500 member guild therefore produces a perfectly
    // legitimate 1500 member request.
    std::size_t const budget = WorldPackets::Query::QueryPlayerNamesForCommunity::MaxResponsesPerRequest;
    std::size_t resolved = 0;
    for (WorldPackets::Query::BNetAccountAndCommunityID const& member : queryPlayerNamesForCommunity.Members)
    {
        if (resolved < budget)
        {
            ++resolved;
            SendPlayerNameByCommunityId(member);
        }
        else
        {
            WorldPackets::Query::QueryPlayerNameByCommunityIdResponse response;
            response.Member = member;
            response.Result = WorldPackets::Query::QueryPlayerNameByCommunityIdResponse::PermanentFailure;
            SendPacket(response.Write());
        }
    }

    if (queryPlayerNamesForCommunity.Members.size() > resolved)
        TC_LOG_WARN("network", "WorldSession::HandleQueryPlayerNamesForCommunity: {} asked for {} members of club {}, "
            "more than the client's own club capacity - resolved {}, answered the remaining {} as permanently unresolvable",
            GetPlayerInfo(), queryPlayerNamesForCommunity.Members.size(), queryPlayerNamesForCommunity.ClubID, resolved,
            queryPlayerNamesForCommunity.Members.size() - resolved);
}

void WorldSession::HandleQueryTimeOpcode(WorldPackets::Query::QueryTime& /*queryTime*/)
{
    SendQueryTimeResponse();
}

void WorldSession::SendQueryTimeResponse()
{
    WorldPackets::Query::QueryTimeResponse queryTimeResponse;
    queryTimeResponse.CurrentTime = GameTime::GetSystemTime();
    SendPacket(queryTimeResponse.Write());
}

/// Only _static_ data is sent in this packet !!!
void WorldSession::HandleCreatureQuery(WorldPackets::Query::QueryCreature& packet)
{
    if (CreatureTemplate const* ci = sObjectMgr->GetCreatureTemplate(packet.CreatureID))
    {
        TC_LOG_DEBUG("network", "WORLD: CMSG_QUERY_CREATURE '{}' - Entry: {}.", ci->Name, packet.CreatureID);

        Difficulty difficulty = _player->GetMap()->GetDifficultyID();

        // Cache only exists for difficulty base
        if (ci->QueryData && difficulty == DIFFICULTY_NONE)
            SendPacket(&ci->QueryData[static_cast<uint32>(GetSessionDbLocaleIndex())]);
        else
        {
            WorldPacket response = ci->BuildQueryData(GetSessionDbLocaleIndex(), difficulty);
            SendPacket(&response);
        }
        TC_LOG_DEBUG("network", "WORLD: Sent SMSG_QUERY_CREATURE_RESPONSE");
    }
    else
    {
        TC_LOG_DEBUG("network", "WORLD: CMSG_QUERY_CREATURE - NO CREATURE INFO! (ENTRY: {})", packet.CreatureID);

        WorldPackets::Query::QueryCreatureResponse response;
        response.CreatureID = packet.CreatureID;
        SendPacket(response.Write());
        TC_LOG_DEBUG("network", "WORLD: Sent SMSG_QUERY_CREATURE_RESPONSE");
    }
}

/// Only _static_ data is sent in this packet !!!
void WorldSession::HandleGameObjectQueryOpcode(WorldPackets::Query::QueryGameObject& packet)
{
    if (GameObjectTemplate const* info = sObjectMgr->GetGameObjectTemplate(packet.GameObjectID))
    {
        if (info->QueryData)
            SendPacket(&info->QueryData[static_cast<uint32>(GetSessionDbLocaleIndex())]);
        else
        {
            WorldPacket response = info->BuildQueryData(GetSessionDbLocaleIndex());
            SendPacket(&response);
        }
        TC_LOG_DEBUG("network", "WORLD: Sent SMSG_GAMEOBJECT_QUERY_RESPONSE");
    }
    else
    {
        TC_LOG_DEBUG("network", "WORLD: CMSG_GAMEOBJECT_QUERY - Missing gameobject info for (ENTRY: {})", packet.GameObjectID);

        WorldPackets::Query::QueryGameObjectResponse response;
        response.GameObjectID = packet.GameObjectID;
        response.Guid = packet.Guid;
        SendPacket(response.Write());
        TC_LOG_DEBUG("network", "WORLD: Sent SMSG_GAMEOBJECT_QUERY_RESPONSE");
    }
}

void WorldSession::HandleQueryCorpseLocation(WorldPackets::Query::QueryCorpseLocationFromClient& queryCorpseLocation)
{
    Player* player = ObjectAccessor::FindConnectedPlayer(queryCorpseLocation.Player);
    if (!player || !player->HasCorpse() || !_player->IsInSameRaidWith(player))
    {
        WorldPackets::Query::CorpseLocation packet;
        packet.Valid = false;                               // corpse not found
        packet.Player = queryCorpseLocation.Player;
        SendPacket(packet.Write());
        return;
    }

    WorldLocation corpseLocation = player->GetCorpseLocation();
    uint32 corpseMapID = corpseLocation.GetMapId();
    uint32 mapID = corpseLocation.GetMapId();
    float x = corpseLocation.GetPositionX();
    float y = corpseLocation.GetPositionY();
    float z = corpseLocation.GetPositionZ();

    // if corpse at different map
    if (mapID != player->GetMapId())
    {
        // search entrance map for proper show entrance
        if (MapEntry const* corpseMapEntry = sMapStore.LookupEntry(mapID))
        {
            if (corpseMapEntry->IsDungeon() && corpseMapEntry->CorpseMapID >= 0)
            {
                // if corpse map have entrance
                if (std::shared_ptr<TerrainInfo> entranceTerrain = sTerrainMgr.LoadTerrain(corpseMapEntry->CorpseMapID))
                {
                    mapID = corpseMapEntry->CorpseMapID;
                    x = corpseMapEntry->Corpse.X;
                    y = corpseMapEntry->Corpse.Y;
                    z = entranceTerrain->GetStaticHeight(player->GetPhaseShift(), mapID, x, y, MAX_HEIGHT);
                }
            }
        }
    }

    WorldPackets::Query::CorpseLocation packet;
    packet.Valid = true;
    packet.Player = queryCorpseLocation.Player;
    packet.MapID = corpseMapID;
    packet.ActualMapID = mapID;
    packet.Position = Position(x, y, z);
    packet.Transport = ObjectGuid::Empty;   // TODO: If corpse is on transport, send transport offsets and transport guid
    SendPacket(packet.Write());
}

void WorldSession::HandleNpcTextQueryOpcode(WorldPackets::Query::QueryNPCText& packet)
{
    TC_LOG_DEBUG("network", "WORLD: CMSG_NPC_TEXT_QUERY TextId: {}", packet.TextID);

    NpcText const* npcText = sObjectMgr->GetNpcText(packet.TextID);

    WorldPackets::Query::QueryNPCTextResponse response;
    response.TextID = packet.TextID;

    if (npcText)
    {
        for (uint8 i = 0; i < MAX_NPC_TEXT_OPTIONS; ++i)
        {
            response.Probabilities[i] = npcText->Data[i].Probability;
            response.BroadcastTextID[i] = npcText->Data[i].BroadcastTextID;
            if (!response.Allow && npcText->Data[i].BroadcastTextID)
                response.Allow = true;
        }
    }

    if (!response.Allow)
        TC_LOG_ERROR("sql.sql", "HandleNpcTextQueryOpcode: no BroadcastTextID found for text {} in `npc_text table`", packet.TextID);

    SendPacket(response.Write());
}

/// Only _static_ data is sent in this packet !!!
void WorldSession::HandleQueryPageText(WorldPackets::Query::QueryPageText& packet)
{
    WorldPackets::Query::QueryPageTextResponse response;
    response.PageTextID = packet.PageTextID;

    uint32 pageID = packet.PageTextID;
    while (pageID)
    {
        PageText const* pageText = sObjectMgr->GetPageText(pageID);
        if (!pageText)
            break;

        WorldPackets::Query::QueryPageTextResponse::PageTextInfo page;
        page.ID = pageID;
        page.NextPageID = pageText->NextPageID;
        page.Text = pageText->Text;
        page.PlayerConditionID = pageText->PlayerConditionID;
        page.Flags = pageText->Flags;

        LocaleConstant locale = GetSessionDbLocaleIndex();
        if (locale != LOCALE_enUS)
            if (PageTextLocale const* pageTextLocale = sObjectMgr->GetPageTextLocale(pageID))
                ObjectMgr::GetLocaleString(pageTextLocale->Text, locale, page.Text);

        response.Pages.push_back(page);
        pageID = pageText->NextPageID;
    }

    response.Allow = !response.Pages.empty();

    SendPacket(response.Write());
}

void WorldSession::HandleQueryCorpseTransport(WorldPackets::Query::QueryCorpseTransport& queryCorpseTransport)
{
    WorldPackets::Query::CorpseTransportQuery response;
    response.Player = queryCorpseTransport.Player;
    if (Player* player = ObjectAccessor::FindConnectedPlayer(queryCorpseTransport.Player); player && _player->IsInSameRaidWith(player))
    {
        if (Corpse const* corpse = _player->GetCorpse())
        {
            if (Transport const* transport = dynamic_cast<Transport const*>(corpse->GetTransport()))
            {
                if (transport->GetGUID() == queryCorpseTransport.Transport)
                {
                    response.Position = transport->GetPosition();
                    response.Facing = transport->GetOrientation();
                }
            }
        }
    }

    SendPacket(response.Write());
}

void WorldSession::HandleQueryQuestCompletionNPCs(WorldPackets::Query::QueryQuestCompletionNPCs& queryQuestCompletionNPCs)
{
    WorldPackets::Query::QuestCompletionNPCResponse response;

    for (int32& questID : queryQuestCompletionNPCs.QuestCompletionNPCs)
    {
        WorldPackets::Query::QuestCompletionNPC questCompletionNPC;

        if (!sObjectMgr->GetQuestTemplate(questID))
        {
            TC_LOG_DEBUG("network", "WORLD: Unknown quest {} in CMSG_QUERY_QUEST_COMPLETION_NPCS by {}", questID, _player->GetGUID().ToString());
            continue;
        }

        questCompletionNPC.QuestID = questID;

        for (auto const& creatures : sObjectMgr->GetCreatureQuestInvolvedRelationReverseBounds(questID))
            questCompletionNPC.NPCs.push_back(creatures.second);

        for (auto const& gos : sObjectMgr->GetGOQuestInvolvedRelationReverseBounds(questID))
            questCompletionNPC.NPCs.push_back(gos.second | 0x80000000); // GO mask

        response.QuestCompletionNPCs.push_back(std::move(questCompletionNPC));
    }

    SendPacket(response.Write());
}

void WorldSession::HandleQuestPOIQuery(WorldPackets::Query::QuestPOIQuery& questPoiQuery)
{
    if (questPoiQuery.MissingQuestCount > MAX_QUEST_LOG_SIZE)
        return;

    // Read quest ids and add the in a unordered_set so we don't send POIs for the same quest multiple times
    std::unordered_set<int32> questIds;
    for (int32 i = 0; i < questPoiQuery.MissingQuestCount; ++i)
        questIds.insert(questPoiQuery.MissingQuestPOIs[i]); // QuestID

    WorldPackets::Query::QuestPOIQueryResponse response;

    for (uint32 questId : questIds)
        if (_player->FindQuestSlot(questId) != MAX_QUEST_LOG_SIZE)
            if (QuestPOIData const* poiData = sObjectMgr->GetQuestPOIData(questId))
                response.QuestPOIDataStats.push_back(poiData);

    SendPacket(response.Write());
}

/**
* Handles the packet sent by the client when requesting information about item text.
*
* This function is called when player clicks on item which has some flag set
*/
void WorldSession::HandleItemTextQuery(WorldPackets::Query::ItemTextQuery& itemTextQuery)
{
    WorldPackets::Query::QueryItemTextResponse queryItemTextResponse;
    queryItemTextResponse.Id = itemTextQuery.Id;

    if (Item* item = _player->GetItemByGuid(itemTextQuery.Id))
    {
        queryItemTextResponse.Valid = true;
        queryItemTextResponse.Item.Text = item->GetText();
    }

    SendPacket(queryItemTextResponse.Write());
}

void WorldSession::HandleQueryRealmName(WorldPackets::Query::QueryRealmName& queryRealmName)
{
    WorldPackets::Query::RealmQueryResponse realmQueryResponse;
    realmQueryResponse.VirtualRealmAddress = queryRealmName.VirtualRealmAddress;

    if (std::shared_ptr<Realm const> realm = sRealmList->GetRealm(queryRealmName.VirtualRealmAddress))
    {
        realmQueryResponse.LookupState = RESPONSE_SUCCESS;
        realmQueryResponse.NameInfo.IsInternalRealm = false;
        realmQueryResponse.NameInfo.IsLocal = queryRealmName.VirtualRealmAddress == GetVirtualRealmAddress();
        realmQueryResponse.NameInfo.RealmNameActual = realm->Name;
        realmQueryResponse.NameInfo.RealmNameNormalized = realm->NormalizedName;
    }
    else
        realmQueryResponse.LookupState = RESPONSE_FAILURE;

    SendPacket(realmQueryResponse.Write());
}

void WorldSession::HandleQueryTreasurePicker(WorldPackets::Query::QueryTreasurePicker const& queryTreasurePicker)
{
    Quest const* questInfo = sObjectMgr->GetQuestTemplate(queryTreasurePicker.QuestID);
    if (!questInfo)
        return;

    WorldPackets::Query::TreasurePickerResponse treasurePickerResponse;
    treasurePickerResponse.QuestID = queryTreasurePicker.QuestID;
    treasurePickerResponse.TreasurePickerID = queryTreasurePicker.TreasurePickerID;

    // TODO: Missing treasure picker implementation

    SendPacket(treasurePickerResponse.Write());
}
