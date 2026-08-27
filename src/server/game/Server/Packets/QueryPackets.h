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

        // UNVERIFIED: none of the three community messages below is proven at the wire. All three - the two 0x44
        // requests and their 0x64 response - occur in NONE of the 25 captures: 0 raw occurrences of 0x44000D,
        // 0x44000E and 0x64000B, and 0 of their 12.0.7 numbers 0x41000D, 0x41000E and 0x5F000B. No reference bytes
        // exist, so no round trip was possible for any of them. The field order is the client writers and the
        // response reader read backwards (arbiter rank 1); the only check it got is the length rule noted at each
        // class.
        //
        // The client's community/club member windows identify a member by the pair
        // JamBNetAccountAndCommunityID { ObjectGuid bnetAccount; uint64 communityID; }. The type name is proven,
        // not derived: the destructor of the batch message class (0x5D4830) frees its element buffer tagged
        // WowGetRawTypeName<struct JamBNetAccountAndCommunityID>, and the reflection descriptor (tag 0x388E928)
        // declares bnetAccount@0x00 and communityID@0x10.
        // communityID is this server's Clubs::CreateClubMemberId value - the same number PlayerGuidLookupData
        // carries as GuildClubMemberID.
        // Field order taken from the 12.1.0.69382 client writers 0x5D5C40 (single) and 0x5D5EC0 (batch).
        struct BNetAccountAndCommunityID
        {
            ObjectGuid BnetAccountGUID;
            uint64 CommunityID = 0;
        };

        // CMSG_QUERY_PLAYER_NAME_BY_COMMUNITY_ID (12.1 0x44000D) - { PackedGuid, uint64 }, client object 56 bytes.
        // No reference bytes (see the note above the member struct); checked against the length rule alone:
        // 10..26 byte body, packed guid 2..18 plus the uint64.
        class QueryPlayerNameByCommunityId final : public ClientPacket
        {
        public:
            explicit QueryPlayerNameByCommunityId(WorldPacket&& packet) : ClientPacket(CMSG_QUERY_PLAYER_NAME_BY_COMMUNITY_ID, std::move(packet)) { }

            void Read() override;

            BNetAccountAndCommunityID Member;
        };

        // CMSG_QUERY_PLAYER_NAMES_FOR_COMMUNITY (12.1 0x44000E) - { uint64 ClubID, uint32 Count, Count x member },
        // client object 64 bytes, in-memory element stride 24.
        // No reference bytes (see the note above the member struct); checked against the length rule alone:
        // 12 + Count * (10..26) byte body.
        class QueryPlayerNamesForCommunity final : public ClientPacket
        {
        public:
            // Reading capacity, and deliberately not a policy number. The socket rejects every packet whose size
            // is not below 0x10000 (WorldSocket.h, PacketHeader::IsValidSize) and one element cannot be shorter
            // than 10 bytes on the wire (2 byte packed guid mask for an empty guid, plus the uint64 community id),
            // so (0xFFFF - 4 opcode - 8 ClubID - 4 Count) / 10 is the largest member count a packet this server
            // accepts can carry at all.
            // Bounding the read by the wire instead of by a chosen number is what keeps a large request from being
            // DROPPED: at the previous value of 200, Array::resize called OnInvalidArraySize and threw
            // PacketArrayMaxCapacityException, which WorldSession::Update swallows as "Skipped packet" - the
            // handler never ran, not one response went out, and C_Club.AreMembersReady stayed false forever. That
            // is precisely the unbounded spinner this opcode was implemented to prevent. It is also the ONLY bound
            // on the size of one request - see the cost accounting below for why a second, smaller budget inside
            // the handler was tried and removed again.
            static constexpr std::size_t MaxMembers = (0xFFFF - 4 - 8 - 4) / 10;

            // WHAT ONE REQUEST COSTS. Written out because no constant bounds it and this is the open point of the
            // opcode - whoever wants to change it needs the numbers, not a reassurance.
            // There is no batched response opcode, so every answered member costs one SMSG of its own: one
            // allocation, one EncryptSend, a ~35 byte PlayerGuidLookupData body, hence ~55 bytes on the wire with
            // the 16 byte PacketHeader and the 4 byte opcode. And EVERY requested member is answered, because a
            // member left unanswered stays pending in the client's community name cache and keeps the whole member
            // list spinning - the failure this opcode exists to avoid; see HandleQueryPlayerNamesForCommunity.
            // The response count is therefore bounded by the wire alone: one 65 519 byte request yields
            // MaxMembers = 6551 responses and ~0.36 MB. The AntiDOS entry of 20 requests/s (WorldSession.cpp,
            // GetMaxPacketCounterAllowed) does NOT extend that into a sustained rate cap, and must not be read as
            // one: exceeding it drops the packet unread and, at the default PacketSpoof.Policy, closes the session.
            // So the worst case an account can extract is a ONE-SHOT ~131 000 responses and ~7.2 MB before it is
            // disconnected, per session and not per second. The entry is written up in full at the case label in
            // WorldSession.cpp, including why a smaller figure would cost honest sessions without buying much.
            // The opcode is reachable from STATUS_AUTHED.
            //
            // A SECOND BUDGET INSIDE THE HANDLER WAS TRIED AND REMOVED - it resolved the first 1000 members and
            // refused the rest with ResultCode::PermanentFailure. It bought almost nothing and paid for it with a
            // permanent defect:
            //   * saved per surplus member: one sCharacterCache lookup, one ObjectAccessor::FindConnectedPlayer,
            //     and 14 bytes (a bare negative is ~41 bytes on the wire against ~55). The packet, the allocation
            //     and the EncryptSend - the whole of the amplification above - are paid either way, because the
            //     surplus still has to be answered at all.
            //   * cost per surplus member: consumer 0x3498F0 answers any non-zero Result by clearing the pending
            //     bit, and Result 1 additionally marks the entry negative and drops its callbacks, so that player
            //     stays nameless in the UI for the rest of the session.
            //   * and the situation is reachable on THIS server: TrinityCore enforces no guild member limit at all
            //     (no MAX_GUILD_MEMBERS in Guild.h/.cpp, no switch in worldserver.conf.dist) and a cold name cache
            //     makes the client ask for the entire roster at once, so a 1500 member guild produces a perfectly
            //     legitimate 1500 member request. The budget would have left 500 of them nameless until relog, in
            //     exchange for ~7 kB and 500 hash lookups.
            //   * the budget's figure was not even established for this case. 1000 is the client's own club
            //     capacity - C_Club.GetClubCapacity is a constant-returning Lua binding, 0x7FF781C46B33
            //     `mov [rbp+arg_10], 3E8h` written once into the slot that 0x7FF781C46F2A pushes back to Lua - and
            //     C_Club.FocusMembers focuses a WHOLE roster (ClubDocumentation.lua: its only argument is clubId),
            //     so 1000 is the size of request the client is built to make. Whether that capacity also governs
            //     guild backed clubs is not measurable here. An unproven limit against a demonstrated defect.
            // Doing better than the amplification above needs a way to answer many members in one frame, which the
            // 12.1 opcode set does not offer. Whoever revisits it should weigh it here, against these numbers.

            explicit QueryPlayerNamesForCommunity(WorldPacket&& packet) : ClientPacket(CMSG_QUERY_PLAYER_NAMES_FOR_COMMUNITY, std::move(packet)) { }

            void Read() override;

            uint64 ClubID = 0;
            Array<BNetAccountAndCommunityID, MaxMembers> Members;
        };

        // SMSG_QUERY_PLAYER_NAME_BY_COMMUNITY_ID_RESPONSE (12.1 0x64000B). No reference bytes either (see the note
        // above the member struct); checked against the length rule alone: 11..27 bytes for a negative answer
        // (uint8 Result, packed guid 2..18, uint64), plus the PlayerGuidLookupData block on success.
        // Note the family: the response to two 0x44 opcodes lives in 0x64. Read out of the 12.1 dispatcher case at
        // 0x67C6ED, which reads
        //   Read<uint8> Result, ReadPackedGuid, Read<uint64>
        // and only then, and only when Result == 0, calls the PlayerGuidLookupData sub reader 0x6E17E0 - the very
        // same function that parses the per player payload of SMSG_QUERY_PLAYER_NAMES_RESPONSE, so the existing
        // writer is reused verbatim.
        // Framing detail worth stating because it looks like a contradiction: the sibling NameCacheLookupResult
        // gates its payload with OptionalInit bits, this one gates with a full uint8. Both are correct - the client
        // really does read a whole byte here, and the sub reader's own bit section therefore starts byte aligned.
        class QueryPlayerNameByCommunityIdResponse final : public ServerPacket
        {
        public:
            // Result is a THREE WAY discriminator, not a bool. Measured in consumer 0x3498F0
            // (DBCacheCommunityName.cpp):
            //   0 = success, the PlayerGuidLookupData payload is read and cached
            //   2 = temporarily unavailable: only the pending bit is cleared, the entry and its callbacks survive
            //       and the client may ask again
            //   anything else = permanently unresolvable: the entry is marked negative, all waiting callbacks fire
            //       with the error flag and are dropped. That player then stays nameless in the UI for good.
            enum ResultCode : uint8
            {
                Success             = 0,
                PermanentFailure    = 1,
                TemporaryFailure    = 2
            };

            explicit QueryPlayerNameByCommunityIdResponse() : ServerPacket(SMSG_QUERY_PLAYER_NAME_BY_COMMUNITY_ID_RESPONSE, 60) { }

            WorldPacket const* Write() override;

            uint8 Result = Success;   ///< one of ResultCode
            BNetAccountAndCommunityID Member;   ///< echoed back, this is the key the client matches on
            Optional<PlayerGuidLookupData> Data;
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

        ByteBuffer& operator<<(ByteBuffer& data, PlayerGuidLookupData const& lookupData);
        ByteBuffer& operator>>(ByteBuffer& data, BNetAccountAndCommunityID& member);
        ByteBuffer& operator<<(ByteBuffer& data, BNetAccountAndCommunityID const& member);
    }
}

ByteBuffer& operator<<(ByteBuffer& data, QuestPOIData const& questPOIData);

#endif // TRINITYCORE_QUERY_PACKETS_H
