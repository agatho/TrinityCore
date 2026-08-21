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

#ifndef TRINITYCORE_QUERY_PACKETS_H
#define TRINITYCORE_QUERY_PACKETS_H

#include "Packet.h"
#include "AuthenticationPackets.h"
#include "ItemPacketsCommon.h"
#include "NPCHandler.h"
#include "ObjectGuid.h"
#include "PacketUtilities.h"
#include "Position.h"
#include "RaceMask.h"
#include "SharedDefines.h"
#include "UnitDefines.h"
#include <array>

class Player;
struct QuestPOIData;
enum class QuestRewardContextFlags : int32;

namespace WorldPackets
{
    namespace Query
    {
        class QueryCreature final : public ClientPacket
        {
        public:
            explicit QueryCreature(WorldPacket&& packet) : ClientPacket(CMSG_QUERY_CREATURE, std::move(packet)) { }

            void Read() override;

            uint32 CreatureID = 0;
        };

        struct CreatureXDisplay
        {
            uint32 CreatureDisplayID = 0;
            float Scale = 1.0f;
            float Probability = 1.0f;
        };

        struct CreatureDisplayStats
        {
            float TotalProbability = 0.0f;
            std::vector<CreatureXDisplay> CreatureDisplay;
        };

        struct CreatureStats
        {
            std::string Title;
            std::string TitleAlt;
            std::string CursorName;
            uint8 CreatureType = 0;
            int32 CreatureFamily = 0;
            int8 Classification = 0;
            CreatureDisplayStats Display;
            float HpMulti = 0.0f;
            float EnergyMulti = 0.0f;
            bool Leader = false;
            std::vector<int32> QuestItems;
            std::vector<int32> QuestCurrencies;
            uint32 CreatureMovementInfoID = 0;
            int32 HealthScalingExpansion = 0;
            uint32 RequiredExpansion = 0;
            uint32 VignetteID = 0;
            int32 Class = 0;
            int32 CreatureDifficultyID = 0;
            int32 WidgetSetID = 0;
            int32 WidgetSetUnitConditionID = 0;
            std::array<uint32, 3> Flags = { };
            std::array<uint32, 2> ProxyCreatureID = { };
            std::array<std::string, 4> Name = { };
            std::array<std::string, 4> NameAlt = { };
        };

        class QueryCreatureResponse final : public ServerPacket
        {
        public:
            explicit QueryCreatureResponse() : ServerPacket(SMSG_QUERY_CREATURE_RESPONSE, 76) { }

            WorldPacket const* Write() override;

            bool Allow = false;
            CreatureStats Stats;
            uint32 CreatureID = 0;
        };

        class QueryPlayerNames final : public ClientPacket
        {
        public:
            explicit QueryPlayerNames(WorldPacket&& packet) : ClientPacket(CMSG_QUERY_PLAYER_NAMES, std::move(packet)) { }

            void Read() override;

            Array<ObjectGuid, 50> Players;
        };

        struct PlayerGuidLookupData
        {
            bool Initialize(ObjectGuid const& guid, Player const* player = nullptr);

            bool IsDeleted = false;
            ObjectGuid AccountID;
            ObjectGuid BnetAccountID;
            ObjectGuid GuidActual;
            std::string Name;
            uint64 GuildClubMemberID = 0;   // same as bgs.protocol.club.v1.MemberId.unique_id
            uint32 VirtualRealmAddress = 0;
            uint8 Race = RACE_NONE;
            uint8 Sex = GENDER_NONE;
            uint8 ClassID = CLASS_NONE;
            uint8 Level = 0;
            uint8 PvpFaction = 0;
            int32 TimerunningSeasonID = 0;
            DeclinedName DeclinedNames;
        };

        struct GuildGuidLookupData
        {
            uint32 VirtualRealmAddress = 0;
            ObjectGuid Guid;
            std::string_view Name;
        };

        struct HouseLookupData
        {
            ObjectGuid Guid;
            std::string_view Name;
        };

        struct NameCacheLookupResult
        {
            ObjectGuid Player;
            uint8 Result = 0; // 0 - full packet, != 0 - only guid
            Optional<PlayerGuidLookupData> Data;
            Optional<GuildGuidLookupData> GuildData;
            Optional<HouseLookupData> HouseData;
        };

        class QueryPlayerNamesResponse final : public ServerPacket
        {
        public:
            explicit QueryPlayerNamesResponse() : ServerPacket(SMSG_QUERY_PLAYER_NAMES_RESPONSE, 60) { }

            WorldPacket const* Write() override;

            std::vector<NameCacheLookupResult> Players;
        };

        class QueryPageText final : public ClientPacket
        {
        public:
            explicit QueryPageText(WorldPacket&& packet) : ClientPacket(CMSG_QUERY_PAGE_TEXT, std::move(packet)) { }

            void Read() override;

            ObjectGuid ItemGUID;
            uint32 PageTextID = 0;
        };

        class QueryPageTextResponse final : public ServerPacket
        {
        public:
            explicit QueryPageTextResponse() : ServerPacket(SMSG_QUERY_PAGE_TEXT_RESPONSE, 15) { }

            WorldPacket const* Write() override;

            struct PageTextInfo
            {
                uint32 ID = 0;
                uint32 NextPageID = 0;
                int32 PlayerConditionID = 0;
                uint8 Flags = 0;
                std::string Text;
            };

            uint32 PageTextID = 0;
            bool Allow = false;
            std::vector<PageTextInfo> Pages;
        };

        // Drops exactly one page text record from the client's page text cache; the client marks the
        // record dirty and re-queries it with CMSG_QUERY_PAGE_TEXT the next time it is needed.
        // Client handler RVA 0x351F00 reads the leading uint32 and forwards it to
        // DBCache::InvalidateRecord of the WPTX (page text) cache - the rest of the packet is ignored.
        // No Lua event fires; the observable effect is the follow up query.
        class InvalidatePageText final : public ServerPacket
        {
        public:
            explicit InvalidatePageText() : ServerPacket(SMSG_INVALIDATE_PAGE_TEXT, 4) { }

            WorldPacket const* Write() override;

            uint32 PageTextID = 0;
        };

        class QueryNPCText final : public ClientPacket
        {
        public:
            explicit QueryNPCText(WorldPacket&& packet) : ClientPacket(CMSG_QUERY_NPC_TEXT, std::move(packet)) { }

            void Read() override;

            ObjectGuid Guid;
            uint32 TextID = 0;
        };

        class QueryNPCTextResponse final : public ServerPacket
        {
        public:
            explicit QueryNPCTextResponse() : ServerPacket(SMSG_QUERY_NPC_TEXT_RESPONSE, 73) { }

            WorldPacket const* Write() override;

            uint32 TextID = 0;
            bool Allow = false;
            std::array<float, MAX_NPC_TEXT_OPTIONS> Probabilities = { };
            std::array<uint32, MAX_NPC_TEXT_OPTIONS> BroadcastTextID = { };
        };

        class QueryGameObject final : public ClientPacket
        {
        public:
            explicit QueryGameObject(WorldPacket&& packet) : ClientPacket(CMSG_QUERY_GAME_OBJECT, std::move(packet)) { }

            void Read() override;

            ObjectGuid Guid;
            uint32 GameObjectID = 0;
        };

        struct GameObjectStats
        {
            std::string Name[4];
            std::string IconName;
            std::string CastBarCaption;
            std::string UnkString;
            uint32 Type = 0;
            uint32 DisplayID = 0;
            std::array<uint32, MAX_GAMEOBJECT_DATA> Data = { };
            float Size = 0.0f;
            std::vector<int32> QuestItems;
            int32 ContentTuningId = 0;
            int32 RequiredLevel = 0;
        };

        class QueryGameObjectResponse final : public ServerPacket
        {
        public:
            explicit QueryGameObjectResponse() : ServerPacket(SMSG_QUERY_GAME_OBJECT_RESPONSE, 165) { }

            WorldPacket const* Write() override;

            uint32 GameObjectID = 0;
            ObjectGuid Guid;
            bool Allow = false;
            GameObjectStats Stats;
        };

        class QueryCorpseLocationFromClient final : public ClientPacket
        {
        public:
            explicit QueryCorpseLocationFromClient(WorldPacket&& packet) : ClientPacket(CMSG_QUERY_CORPSE_LOCATION_FROM_CLIENT, std::move(packet)) { }

            void Read() override;

            ObjectGuid Player;
        };

        class CorpseLocation final : public ServerPacket
        {
        public:
            explicit CorpseLocation() : ServerPacket(SMSG_CORPSE_LOCATION, 1 + (5 * 4) + 16) { }

            WorldPacket const* Write() override;

            ObjectGuid Player;
            ObjectGuid Transport;
            TaggedPosition<::Position::XYZ> Position;
            int32 ActualMapID = 0;
            int32 MapID = 0;
            bool Valid = false;
        };

        class QueryCorpseTransport final : public ClientPacket
        {
        public:
            explicit QueryCorpseTransport(WorldPacket&& packet) : ClientPacket(CMSG_QUERY_CORPSE_TRANSPORT , std::move(packet)) { }

            void Read() override;

            ObjectGuid Player;
            ObjectGuid Transport;
        };

        class CorpseTransportQuery final : public ServerPacket
        {
        public:
            explicit CorpseTransportQuery() : ServerPacket(SMSG_CORPSE_TRANSPORT_QUERY, 16) { }

            WorldPacket const* Write() override;

            ObjectGuid Player;
            TaggedPosition<::Position::XYZ> Position;
            float Facing = 0.0f;
        };

        class QueryTime final : public ClientPacket
        {
        public:
            explicit QueryTime(WorldPacket&& packet) : ClientPacket(CMSG_QUERY_TIME, std::move(packet)) { }

            void Read() override { }
        };

        class QueryTimeResponse final : public ServerPacket
        {
        public:
            explicit QueryTimeResponse() : ServerPacket(SMSG_QUERY_TIME_RESPONSE, 4 + 4) { }

            WorldPacket const* Write() override;

            Timestamp<> CurrentTime;
        };

        class QuestPOIQuery final : public ClientPacket
        {
        public:
            explicit QuestPOIQuery(WorldPacket&& packet) : ClientPacket(CMSG_QUEST_POI_QUERY, std::move(packet)) { }

            void Read() override;

            int32 MissingQuestCount = 0;
            std::array<int32, 175> MissingQuestPOIs = { };
        };

        class QuestPOIQueryResponse final : public ServerPacket
        {
        public:
            explicit QuestPOIQueryResponse() : ServerPacket(SMSG_QUEST_POI_QUERY_RESPONSE, 4 + 4) { }

            WorldPacket const* Write() override;

            std::vector<QuestPOIData const*> QuestPOIDataStats;
        };

        class QueryQuestCompletionNPCs final : public ClientPacket
        {
        public:
            explicit QueryQuestCompletionNPCs(WorldPacket&& packet) : ClientPacket(CMSG_QUERY_QUEST_COMPLETION_NPCS, std::move(packet)) { }

            void Read() override;

            Array<int32, 100> QuestCompletionNPCs;
        };

        struct QuestCompletionNPC
        {
            int32 QuestID = 0;
            std::vector<int32> NPCs;
        };

        class QuestCompletionNPCResponse final : public ServerPacket
        {
        public:
            explicit QuestCompletionNPCResponse() : ServerPacket(SMSG_QUEST_COMPLETION_NPC_RESPONSE, 4) { }

            WorldPacket const* Write() override;

            std::vector<QuestCompletionNPC> QuestCompletionNPCs;
        };

        class QueryPetName final : public ClientPacket
        {
        public:
            explicit QueryPetName(WorldPacket&& packet) : ClientPacket(CMSG_QUERY_PET_NAME, std::move(packet)) { }

            void Read() override;

            ObjectGuid UnitGUID;
        };

        class QueryPetNameResponse final : public ServerPacket
        {
        public:
            explicit QueryPetNameResponse() : ServerPacket(SMSG_QUERY_PET_NAME_RESPONSE, 16 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid UnitGUID;
            bool Allow = false;

            bool HasDeclined = false;
            DeclinedName DeclinedNames;
            WorldPackets::Timestamp<> Timestamp;
            std::string Name;
        };

        class ItemTextQuery final : public ClientPacket
        {
        public:
            explicit ItemTextQuery(WorldPacket&& packet) : ClientPacket(CMSG_ITEM_TEXT_QUERY, std::move(packet)) { }

            void Read() override;

            ObjectGuid Id;
        };

        struct ItemTextCache
        {
            std::string Text;
        };

        class QueryItemTextResponse final : public ServerPacket
        {
        public:
            explicit QueryItemTextResponse() : ServerPacket(SMSG_QUERY_ITEM_TEXT_RESPONSE) { }

            WorldPacket const* Write() override;

            ObjectGuid Id;
            bool Valid = false;
            ItemTextCache Item;
        };

        class QueryRealmName final : public ClientPacket
        {
        public:
            explicit QueryRealmName(WorldPacket&& packet) : ClientPacket(CMSG_QUERY_REALM_NAME, std::move(packet)) { }

            void Read() override;

            uint32 VirtualRealmAddress = 0;
        };

        class RealmQueryResponse final : public ServerPacket
        {
        public:
            explicit RealmQueryResponse() : ServerPacket(SMSG_REALM_QUERY_RESPONSE) { }

            WorldPacket const* Write() override;

            uint32 VirtualRealmAddress = 0;
            uint8 LookupState = 0;
            WorldPackets::Auth::VirtualRealmNameInfo NameInfo;
        };

        class QueryTreasurePicker final : public ClientPacket
        {
        public:
            explicit QueryTreasurePicker(WorldPacket&& packet) : ClientPacket(CMSG_QUERY_TREASURE_PICKER, std::move(packet)) { }

            void Read() override;

            uint32 QuestID = 0;
            uint32 TreasurePickerID = 0;
        };

        struct TreasurePickItem
        {
            Item::ItemInstance Item;
            uint32 Quantity = 0;
            Optional<QuestRewardContextFlags> ContextFlags;
        };

        struct TreasurePickCurrency
        {
            uint32 CurrencyID = 0;
            uint32 Quantity = 0;
            Optional<QuestRewardContextFlags> ContextFlags;
        };

        enum class TreasurePickerBonusContext : uint8
        {
            None    = 0,
            WarMode = 1
        };

        struct TreasurePickerBonus
        {
            std::vector<TreasurePickItem> ItemPicks;
            std::vector<TreasurePickCurrency> CurrencyPicks;
            uint64 Gold = 0;
            TreasurePickerBonusContext Context = TreasurePickerBonusContext::None;
        };

        struct TreasurePickerPick
        {
            std::vector<TreasurePickItem> ItemPicks;
            std::vector<TreasurePickCurrency> CurrencyPicks;
            std::vector<TreasurePickerBonus> Bonuses;
            uint64 Gold = 0;
            int32 Flags = 0;
            bool IsChoice = false;
        };

        class TreasurePickerResponse final : public ServerPacket
        {
        public:
            explicit TreasurePickerResponse() : ServerPacket(SMSG_TREASURE_PICKER_RESPONSE) { }

            WorldPacket const* Write() override;

            uint32 QuestID = 0;
            uint32 TreasurePickerID = 0;
            TreasurePickerPick Treasure;
        };

        // Sent by the client's neighbourhood name cache whenever it is asked for a name it does not
        // have (DataCache<JamCliNeighborhoodName>::Get miss, query callback RVA 0x34F540). The
        // request is guarded by an "in flight" bit, so a server that never answers leaves the entry
        // stuck forever and the client never asks again.
        // Reference bytes: 12.1.0.69273_preyandwqpart1.pkt carries one, opcode 0x4300B7, 7 bytes -
        // the packed guid and nothing else. Round-tripped byte for byte by
        // C:\dumps\tools\w0_query_hotfix\round_trip.py.
        class QueryNeighborhoodInfo final : public ClientPacket
        {
        public:
            explicit QueryNeighborhoodInfo(WorldPacket&& packet) : ClientPacket(CMSG_QUERY_NEIGHBORHOOD_INFO, std::move(packet)) { }

            void Read() override;

            ObjectGuid NeighborhoodGUID;
        };

        // Answer carrying the authoritative name of a housing neighbourhood. The client fires the Lua
        // event NEIGHBORHOOD_NAME_UPDATED(neighborhoodGuid, neighborhoodName)
        // (HousingNeighborhoodUIDocumentation.lua:324-334). Reader RVA 0x72E972: HasName and the
        // 8 bit name length sit in two separate bytes, not in one bit group. The consumer
        // (RVA 0x34F580) copies at most 128 bytes of the name.
        // The layout is not only decompiled but measured. 12.1.0.69273_preyandwqpart1.pkt carries a
        // retail answer, opcode 0x490012, 17 bytes:
        //
        //   03 d0 e1 67 01 80 dc | 80 | 08 | "62-74-76"
        //   \_____ packed guid _/   ^     ^    \_ 8 name bytes
        //                       HasName  full 8 bit length
        //
        // Write() below reproduces those 17 bytes exactly (round_trip.py, 1 of 1). That also settles
        // the one question the decompilate left open - whether the length really is 8 bits and not a
        // 6 bit SizedString like the rest of this file: read as bits<6> the length byte 0x08 would
        // announce 2 characters and leave 6 bytes unconsumed, so the packet only closes with 8.
        class QueryNeighborhoodNameResponse final : public ServerPacket
        {
        public:
            static constexpr std::size_t MaxNameLength = 128;

            explicit QueryNeighborhoodNameResponse() : ServerPacket(SMSG_QUERY_NEIGHBORHOOD_NAME_RESPONSE) { }

            WorldPacket const* Write() override;

            ObjectGuid NeighborhoodGUID;
            Optional<std::string> Name;
        };

        // Drops the cached name of one neighbourhood. Consumer RVA 0x34F7D0 takes the guid and
        // nothing else; the client re-asks for the name afterwards.
        class InvalidateNeighborhoodName final : public ServerPacket
        {
        public:
            explicit InvalidateNeighborhoodName() : ServerPacket(SMSG_INVALIDATE_NEIGHBORHOOD_NAME, 16) { }

            WorldPacket const* Write() override;

            ObjectGuid NeighborhoodGUID;
        };

        ByteBuffer& operator<<(ByteBuffer& data, PlayerGuidLookupData const& lookupData);
    }
}

ByteBuffer& operator<<(ByteBuffer& data, QuestPOIData const& questPOIData);

#endif // TRINITYCORE_QUERY_PACKETS_H
