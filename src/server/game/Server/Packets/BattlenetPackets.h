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

#ifndef TRINITYCORE_BATTLENET_PACKETS_H
#define TRINITYCORE_BATTLENET_PACKETS_H

#include "Packet.h"
#include "BattlenetRpcErrorCodes.h"
#include "MessageBuffer.h"
#include "ObjectGuid.h"
#include "PacketUtilities.h"
#include <array>
#include <string>
#include <vector>

namespace WorldPackets
{
    namespace Battlenet
    {
        struct MethodCall
        {
            uint64 Type = 0;
            uint64 ObjectId = 0;
            uint32 Token = 0;

            uint32 GetServiceHash() const { return uint32(Type >> 32); }
            uint32 GetMethodId() const { return uint32(Type & 0xFFFFFFFF); }
        };

        class Notification final : public ServerPacket
        {
        public:
            explicit Notification() : ServerPacket(SMSG_BATTLENET_NOTIFICATION, 8 + 8 + 4 + 4) { }

            WorldPacket const* Write() override;

            MethodCall Method;
            ByteBuffer Data;
        };

        class Response final : public ServerPacket
        {
        public:
            explicit Response() : ServerPacket(SMSG_BATTLENET_RESPONSE, 4 + 8 + 8 + 4 + 4) { }

            WorldPacket const* Write() override;

            BattlenetRpcErrorCode BnetStatus = ERROR_OK;
            MethodCall Method;
            ByteBuffer Data;
        };

        class ConnectionStatus final : public ServerPacket
        {
        public:
            explicit ConnectionStatus() : ServerPacket(SMSG_BATTLE_NET_CONNECTION_STATUS, 1) { }

            WorldPacket const* Write() override;

            uint8 State = 0;
            bool SuppressNotification = true;
        };

        class ChangeRealmTicketResponse final : public ServerPacket
        {
        public:
            explicit ChangeRealmTicketResponse() : ServerPacket(SMSG_CHANGE_REALM_TICKET_RESPONSE) { }

            WorldPacket const* Write() override;

            uint32 Token = 0;
            bool Allow = false;
            ByteBuffer Ticket;
        };

        class Request final : public ClientPacket
        {
        public:
            explicit Request(WorldPacket&& packet) : ClientPacket(CMSG_BATTLENET_REQUEST, std::move(packet)) { }

            void Read() override;

            MethodCall Method;
            MessageBuffer Data;
        };

        class ChangeRealmTicket final : public ClientPacket
        {
        public:
            explicit ChangeRealmTicket(WorldPacket&& packet) : ClientPacket(CMSG_CHANGE_REALM_TICKET, std::move(packet)) { }

            void Read() override;

            uint32 Token = 0;
            std::array<uint8, 32> Secret = { };
        };

        // ------------------------------------------------------------------------------------
        // Einheit w4_cmsg_43_3D - Block B4 (Bnet-Praesenz / Community), Phase A.
        // Feldfolgen aus dem Client-Serializer 12.1.0.69382, Writer-RVA je Klasse.
        // ------------------------------------------------------------------------------------

        // Writer 0x6A8AD0 -> Rumpf 0x6A8950 - 20..134 Byte.
        // Der Subplan uebersieht das fuehrende Einzelbit vor den beiden Laengen; wer es
        // ueberliest, verschiebt beide Zeichenketten um ein Bit.
        class AddBattlenetFriend final : public ClientPacket
        {
        public:
            explicit AddBattlenetFriend(WorldPacket&& packet) : ClientPacket(CMSG_ADD_BATTLENET_FRIEND, std::move(packet)) { }

            void Read() override;

            uint64 Field32 = 0;                 // UNVERIFIED: Semantik offen (Objekt +0x20)
            uint32 Field44 = 0;                 // UNVERIFIED: Semantik offen (Objekt +0x2C)
            ObjectGuid PlayerGUID;              // Objekt +0x30
            uint32 Field64 = 0;                 // UNVERIFIED: Semantik offen (Objekt +0x40)
            bool Flag = false;                  // Objekt +0x28, EIN Bit
            std::string Field68;                // Puffer 49, ohne NUL
            std::string Field117;               // Puffer 49, ohne NUL
        };

        // Writer 0x6ACD70 - getaggte Union, 5 oder 39 Byte.
        // bits<6> Laenge UND die Zeichenkette haengen beide an Tag == 5; wer sie unbedingt
        // liest, verschiebt bei jedem anderen Tag die Bytegrenze.
        class BattlenetChallengeResponse final : public ClientPacket
        {
        public:
            explicit BattlenetChallengeResponse(WorldPacket&& packet) : ClientPacket(CMSG_BATTLENET_CHALLENGE_RESPONSE, std::move(packet)) { }

            void Read() override;

            static constexpr uint32 TagWithResponse = 5;

            uint32 ChallengeID = 0;             // Objekt +0x20
            uint32 Tag = 0;                     // Objekt +0x24, bits<3>
            std::string Response;               // Puffer 33, ohne NUL, nur bei Tag == 5
        };

        // Writer 0x6AF0E0 - 5 + 8*n Byte.
        // Lua C_Club.SetClubPresenceSubscription(clubId) bzw. ClearClubPresenceSubscription();
        // Blizzards Doku: "You can only be subscribed to 0 or 1 clubs for presence."
        // Der Serializer deckelt Count NICHT und liest ihn signed - die Schranke ist serverseitig.
        class ClubPresenceSubscribe final : public ClientPacket
        {
        public:
            explicit ClubPresenceSubscribe(WorldPacket&& packet) : ClientPacket(CMSG_CLUB_PRESENCE_SUBSCRIBE, std::move(packet)) { }

            void Read() override;

            bool Subscribe = false;             // Objekt +0x20, EIN Bit
            Array<uint64, 4> ClubIDs;
        };

        // Writer 0x6AF4A0 -> Rumpf 0x6AF2D0 - 42..621+n Byte.
        // ZWEI getrennte Bit-Sektionen, je ein bits<9>-Laengenfeld: der Compiler zerlegt sie in
        // Write<uint8>(len >> 1) plus ein eingebettetes Bit. Beide "u8" des Subplans sind
        // deshalb keine Felder. Puffer 306 und 257 - nicht 511, wie die Bitbreite nahelegt.
        class SendCharacterClubInvitation final : public ClientPacket
        {
        public:
            explicit SendCharacterClubInvitation(WorldPacket&& packet) : ClientPacket(CMSG_SEND_CHARACTER_CLUB_INVITATION, std::move(packet)) { }

            void Read() override;

            uint64 Field32 = 0;                 // UNVERIFIED: Semantik offen
            uint64 Field40 = 0;                 // UNVERIFIED: Semantik offen
            uint32 Field48 = 0;                 // UNVERIFIED: Semantik offen
            uint32 Token = 0;                   // Objekt +0x38, aus dem Bnet-RPC-Tokengenerator 0x34B67F0
            ObjectGuid CharacterGUID;           // Objekt +0x40
            uint64 Field80 = 0;                 // UNVERIFIED: Semantik offen
            std::string Field88;                // Puffer 306, ohne NUL
            std::string Field394;               // Puffer 257, ohne NUL
            std::vector<uint8> Blob;
        };

        // Writer 0x6AA740 - guid + uint32.
        // Lua C_StoreSecure.RequestCharacterGuildFollowInfo(guid, realmAddress).
        class RequestCharacterGuildFollowInfo final : public ClientPacket
        {
        public:
            explicit RequestCharacterGuildFollowInfo(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_CHARACTER_GUILD_FOLLOW_INFO, std::move(packet)) { }

            void Read() override;

            ObjectGuid CharacterGUID;
            uint32 VirtualRealmAddress = 0;
        };

        // Writer 0x6B22C0 - der uint32 des Subplans IST der Listenzaehler, kein Skalar.
        // Lua C_AccountServices.SaveAccountData(); der Client sendet in diesem Build immer
        // Count == 0 (die Auswahl trifft der Server), das Format traegt aber eine Liste.
        class SaveAccountDataExport final : public ClientPacket
        {
        public:
            explicit SaveAccountDataExport(WorldPacket&& packet) : ClientPacket(CMSG_SAVE_ACCOUNT_DATA_EXPORT, std::move(packet)) { }

            void Read() override;

            Array<ObjectGuid, 200> Characters;
        };

        // Writer 0x6B2750 - uint32 RealmAddress, dann der Listenzaehler.
        // Lua C_StoreSecure.RequestRealmGuildMasterInfo(realmAddress) hat nur EIN Argument;
        // die GUID-Liste stellt der Client selbst aus seiner Charakterliste zusammen.
        class RequestRealmGuildMasterInfo final : public ClientPacket
        {
        public:
            explicit RequestRealmGuildMasterInfo(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_REALM_GUILD_MASTER_INFO, std::move(packet)) { }

            void Read() override;

            uint32 VirtualRealmAddress = 0;
            Array<ObjectGuid, 200> Characters;
        };
    }
}

#endif // TRINITYCORE_BATTLENET_PACKETS_H
