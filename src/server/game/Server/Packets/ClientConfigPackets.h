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

#ifndef TRINITYCORE_CLIENT_CONFIG_PACKETS_H
#define TRINITYCORE_CLIENT_CONFIG_PACKETS_H

#include "Packet.h"
#include "PacketUtilities.h"
#include "WorldSession.h"

namespace WorldPackets
{
    namespace ClientConfig
    {
        class AccountDataTimes final : public ServerPacket
        {
        public:
            explicit AccountDataTimes() : ServerPacket(SMSG_ACCOUNT_DATA_TIMES, 16 + 8 + 8 * NUM_ACCOUNT_DATA_TYPES) { }

            WorldPacket const* Write() override;

            ObjectGuid PlayerGuid;
            Timestamp<> ServerTime;
            std::array<Timestamp<>, NUM_ACCOUNT_DATA_TYPES> AccountTimes = { };
        };

        class ClientCacheVersion final : public ServerPacket
        {
        public:
            explicit ClientCacheVersion() : ServerPacket(SMSG_CACHE_VERSION, 4) { }

            WorldPacket const* Write() override;

            uint32 CacheVersion = 0;
        };

        // Cache invalidation signal. For every entry the client forms the CVar name
        // "CACHE-<Prefix>-<Key>" and compares its stored value against Value case insensitively
        // (client handler RVA 0x341AD0, format string RVA 0x3B6AA20, compare is _stricmp).
        // On the first difference it notifies the first registered cache whose MatchesPrefix
        // accepts Prefix, that cache throws its contents away, and all CVars are rewritten.
        // An empty Entries list makes the client do nothing at all.
        struct CacheInfoEntry
        {
            std::string Key;                                ///< max 63 characters (6 bit length)
            std::string Value;                              ///< max 63 characters (6 bit length)
        };

        class CacheInfo final : public ServerPacket
        {
        public:
            explicit CacheInfo() : ServerPacket(SMSG_CACHE_INFO) { }

            WorldPacket const* Write() override;

            std::vector<CacheInfoEntry> Entries;
            std::string Prefix;                             ///< cache domain, one of WGOB, WNPC, WQST, WPTX, WPTN
        };

        class RequestAccountData final : public ClientPacket
        {
        public:
            explicit RequestAccountData(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_ACCOUNT_DATA, std::move(packet)) { }

            void Read() override;

            ObjectGuid PlayerGuid;
            int32 DataType = 0; ///< @see enum AccountDataType
        };

        class UpdateAccountData final : public ServerPacket
        {
        public:
            explicit UpdateAccountData() : ServerPacket(SMSG_UPDATE_ACCOUNT_DATA) { }

            WorldPacket const* Write() override;

            ObjectGuid Player;
            Timestamp<> Time;
            uint32 Size    = 0; ///< decompressed size
            int32 DataType = 0; ///< @see enum AccountDataType
            std::vector<uint8> CompressedData;
        };

        class UserClientUpdateAccountData final : public ClientPacket
        {
        public:
            explicit UserClientUpdateAccountData(WorldPacket&& packet) : ClientPacket(CMSG_UPDATE_ACCOUNT_DATA, std::move(packet)) { }

            void Read() override;

            ObjectGuid PlayerGuid;
            Timestamp<> Time;
            uint32 Size    = 0; ///< decompressed size
            int32 DataType = 0; ///< @see enum AccountDataType
            std::span<uint8> CompressedData;
        };

        class UpdateAccountDataComplete final : public ServerPacket
        {
        public:
            explicit UpdateAccountDataComplete() : ServerPacket(SMSG_UPDATE_ACCOUNT_DATA_COMPLETE, 16 + 4 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid Player;
            int32 DataType = 0; ///< @see enum AccountDataType
            int32 Result = 0;
        };

        class SetAdvancedCombatLogging final : public ClientPacket
        {
        public:
            explicit SetAdvancedCombatLogging(WorldPacket&& packet) : ClientPacket(CMSG_SET_ADVANCED_COMBAT_LOGGING, std::move(packet)) { }

            void Read() override;

            bool Enable = false;
        };
    }
}

#endif // TRINITYCORE_CLIENT_CONFIG_PACKETS_H
