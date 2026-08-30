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
#include "CharacterCache.h"
#include "ClubUtils.h"
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
        // Either the member id belongs to another realm, or it is a value no realm can have minted (bits
        // CreateClubMemberId leaves at zero are set) - GetGuidFromClubMemberId separates the two cases and rejects
        // both. Neither will ever resolve here, so the permanent answer is the correct one and the client may burn
        // the entry.
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
            // Unreachable as this function stands, and said so rather than dressed up as a transient state:
            // PlayerGuidLookupData::Initialize has exactly one `return false` (QueryPackets.cpp:144-148) and its
            // condition is the same sCharacterCache->GetCharacterCacheByGuid(guid) that the else-if above just
            // found non-null - same synchronous function, nothing in between that could evict the entry. Result 2
            // therefore never goes out today: of the three result values this opcode defines, the server reaches
            // two. The third is written correctly and read correctly by the client, but there is no server state in
            // which it is the right answer, and inventing one would be worse than recording the gap.
            // The branch stays as a guard because if Initialize ever grows a second failure condition,
            // TemporaryFailure remains the correct answer here: Result 1 would brand the member negative in the
            // client's cache and leave that player nameless for the rest of the session, while Result 2 only clears
            // the pending bit and lets the client ask again (consumer 0x3498F0).
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
    //
    // MAY THIS SESSION ASK ABOUT THIS CLUB AT ALL? ClubID is read and then not used, and that is a decision with
    // sources behind it, not an oversight. Three of them, in arbiter order:
    //
    // 1. THE WIRE SAYS THE GATE IS NOT HERE (rank 1, and on its own decisive). The single member sibling
    //    CMSG_QUERY_PLAYER_NAME_BY_COMMUNITY_ID carries NO ClubID - the whole request is one
    //    { bnetAccount, communityID } pair (client writer 0x5D5C40). A protocol that scoped name resolution to
    //    club membership could not express that request at all: there would be no club to check it against. The
    //    key is authorization free by construction, exactly as an ObjectGuid is for CMSG_QUERY_PLAYER_NAMES.
    // 2. AND THE CLIENT HAS NO PLACE TO PUT SUCH A REFUSAL. The response Result is a three way discriminator and
    //    all three values are about whether the MEMBER can be resolved - success, gone for good, ask again
    //    (consumer 0x3498F0; see QueryPlayerNameByCommunityIdResponse::ResultCode). None of them means "you may
    //    not ask". The same client does carry that code - ClubErrorType.ErrorClubNotMember = 18,
    //    ClubDocumentation.lua - but on the club RPC surface, not here. Where Blizzard put the error tells us
    //    where Blizzard put the check.
    // 3. IN THIS TREE THE GATE IS ALREADY ON THAT RPC SURFACE, one layer up. Battlenet::Services::ClubService is
    //    what hands out community ids in the first place, and it is membership scoped twice over:
    //    HandleSubscribe refuses a non member with ERROR_CLUB_NOT_MEMBER (ClubService.cpp:64), and HandleGetMembers
    //    ignores the requested club id outright and answers from player->GetGuild() alone (ClubService.cpp:132-143),
    //    so it can only ever return the caller's own roster. Duplicating that check down here would gate the
    //    lookup, not the disclosure.
    //
    // What this deliberately does NOT claim is that the answer is confidential. It is not, and not because of this
    // handler: PlayerGuidLookupData carries AccountID, BnetAccountID, IsDeleted and GuildClubMemberID for every
    // player it describes, and CMSG_QUERY_PLAYER_NAMES already serves exactly that struct for 50 ARBITRARY guids
    // to any logged in session (HandleQueryPlayerNames above, Array<ObjectGuid, 50>). Player guids are as
    // enumerable as community ids are. The same mapping is published a second time through the player update field
    // PlayerData::GuildClubMemberID (Player.cpp, LoadFromDB). Community id <-> guid <-> name is therefore public
    // in both directions in this tree, tree wide, independently of these two opcodes - a club membership check
    // here would withhold nothing that is not already available for the asking one opcode over.
    //
    // What IS this unit's doing, and is bounded rather than argued away, is the AMPLIFICATION: 6551 answers per
    // request against the sibling's 50, or ~0.61 MB against ~5 kB. A membership check would not have bounded
    // it, and the reason is worth recording because it is the reason no gate of that shape can: such a check
    // decides WHO may ask, not HOW MANY answers one request costs, and the largest request that exists - a
    // whole cold guild roster - is precisely the one a member is entitled to make. What does bound it is
    // written out at the end of this comment.
    //
    // EVERY requested member gets an answer, and every answer is a resolved one. That is not a nicety: a member the
    // server answers with nothing at all stays pending in the client's community name cache,
    // C_Club.AreMembersReady never turns true, and CommunitiesMemberList.lua has no timer, no retry and no error
    // dialog to get out of it - one silent omission keeps the whole member list spinning.
    //
    // There is deliberately NO per request resolve budget. An earlier version resolved the first 1000 members and
    // refused the rest with PermanentFailure; it was removed because it saved only a cache lookup and the
    // PlayerGuidLookupData block per surplus member - 51..63 bytes, median 57, measured on the corpus; the packet,
    // the allocation and the EncryptSend are paid either way - while PermanentFailure makes consumer 0x3498F0 brand
    // that member negative for the rest of the session. With no guild member limit in this tree, a 1500 member
    // guild with a cold name cache is a legitimate 1500 member request, so the budget bought ~28.5 kB and its only
    // other measurable effect was 500 permanently nameless players. The full accounting, including why the figure
    // 1000 did not carry its own weight, is on QueryPlayerNamesForCommunity in QueryPackets.h.
    //
    // WHAT BOUNDS THIS REQUEST INSTEAD - two bounds of different kind, and they must not be described as one:
    //   * the wire, through QueryPlayerNamesForCommunity::MaxMembers. This one bounds a request at a point where it
    //     is still answered in full, which is what a resolve budget cannot do.
    //   * the AntiDOS entry in WorldSession::DosProtection::GetMaxPacketCounterAllowed. This one does NOT truncate
    //     anything: on overflow EvaluateOpcode returns false (WorldSession.cpp, DosProtection::EvaluateOpcode),
    //     WorldSession::Update discards the whole packet unread, and under the default PacketSpoof.Policy =
    //     POLICY_KICK (World.cpp) KickPlayer closes every socket of the session. The overflowing request is not
    //     answered at all, and the session is gone with it.
    // That failure mode is why the rate is set where a deliberate flood reaches it and a real client cannot, rather
    // than at the tightest value clicking around survives - see the entry itself for the figure and its reasoning.
    // But note what the second bound does NOT do, because an earlier revision of this file got it backwards: it is
    // a SUSTAINED per-second rate, not a per-session budget. The counter resets every calendar second, so a caller
    // that stays at the limit is never kicked and keeps drawing limit x 6551 responses per second. The rate caps
    // the amplification, it does not end it; the residual is carried as an open point at the entry itself.
    for (WorldPackets::Query::BNetAccountAndCommunityID const& member : queryPlayerNamesForCommunity.Members)
        SendPlayerNameByCommunityId(member);
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

    // Only answer for pickers the quest actually advertises - refuse unrelated picker ids.
    bool questUsesPicker = false;
    for (int32 treasurePickerId : questInfo->GetTreasurePickerId())
    {
        if (uint32(treasurePickerId) == queryTreasurePicker.TreasurePickerID)
        {
            questUsesPicker = true;
            break;
        }
    }

    if (!questUsesPicker)
        return;

    WorldPackets::Query::TreasurePickerResponse treasurePickerResponse;
    treasurePickerResponse.QuestID = queryTreasurePicker.QuestID;
    treasurePickerResponse.TreasurePickerID = queryTreasurePicker.TreasurePickerID;

    // A quest template can advertise a TreasurePickerID that world has no matching `treasure_picker`
    // row for (e.g. the DH intro quest 40077 -> picker 3688). We must still answer: the client's quest
    // frame blocks on the CMSG_QUERY_TREASURE_PICKER reply and never sends ACCEPT if we stay silent.
    // So fall through to an empty SMSG_TREASURE_PICKER_RESPONSE rather than returning - no invented
    // loot; fill `treasure_picker` when the data is actually known.
    if (TreasurePickerTemplate const* treasurePicker = sObjectMgr->GetTreasurePicker(queryTreasurePicker.TreasurePickerID))
    {
        treasurePickerResponse.Treasure.Flags = treasurePicker->Flags;
        treasurePickerResponse.Treasure.IsChoice = treasurePicker->IsChoice;
        treasurePickerResponse.Treasure.Gold = treasurePicker->Gold;

        Player* player = GetPlayer();
        for (TreasurePickerItem const& pickerItem : treasurePicker->Items)
        {
            if (!sObjectMgr->IsTreasurePickerItemEligibleForPlayer(player, pickerItem.ItemID))
                continue;

            WorldPackets::Query::TreasurePickItem& itemPick = treasurePickerResponse.Treasure.ItemPicks.emplace_back();
            itemPick.Item.ItemID = pickerItem.ItemID;
            itemPick.Quantity = pickerItem.Quantity;
            if (pickerItem.BonusListID)
            {
                itemPick.Item.ItemBonus.emplace();
                itemPick.Item.ItemBonus->Context = ItemContext(pickerItem.Context);
                itemPick.Item.ItemBonus->BonusListIDs.push_back(pickerItem.BonusListID);
            }
        }
    }
    else
        TC_LOG_DEBUG("network", "WORLD: CMSG_QUERY_TREASURE_PICKER quest {} picker {} - no `treasure_picker` row, sending empty response",
            queryTreasurePicker.QuestID, queryTreasurePicker.TreasurePickerID);

    SendPacket(treasurePickerResponse.Write());
}

// Tells the client that one page text record is stale. The client drops exactly that record from
// its page text cache (client handler RVA 0x351F00 -> DBCache::InvalidateRecord) and asks for it
// again with CMSG_QUERY_PAGE_TEXT the next time it needs it. Nothing else is thrown away, and no
// Lua event fires - the follow up query is the observable effect.
// No core path calls this yet: TrinityCore only knows the bulk ".reload page_text", not a change to
// a single record, and this message carries exactly one record. The bulk case has a lever of its
// own, but it is opt-in: retail never sends the WPTX domain of SMSG_CACHE_INFO - not in any of the
// 11 recorded logins - so SendCacheInfo does not send it either, and the core therefore has no
// automatic page text stamp. To invalidate the whole page text cache, put a WPTX row into the world
// table `cache_info`, bump its Value and run `.reload cache_info` alongside `.reload page_text`;
// the new stamp is what the next login sees. What is missing here is therefore the single record
// trigger, and the bulk path costs one deliberate table row.
void WorldSession::SendInvalidatePageText(uint32 pageTextId)
{
    WorldPackets::Query::InvalidatePageText invalidatePageText;
    invalidatePageText.PageTextID = pageTextId;

    SendPacket(invalidatePageText.Write());
}
