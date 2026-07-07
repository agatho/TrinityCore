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

#ifndef TRINITYCORE_COMMENTATOR_PACKETS_H
#define TRINITYCORE_COMMENTATOR_PACKETS_H

#include "Packet.h"
#include "ObjectGuid.h"
#include <array>
#include <vector>

namespace WorldPackets
{
    namespace Commentator
    {
        class CommentatorEnable final : public ClientPacket
        {
        public:
            explicit CommentatorEnable(WorldPacket&& packet) : ClientPacket(CMSG_COMMENTATOR_ENABLE, std::move(packet)) { }

            void Read() override;

            uint32 Enable = 0;
        };

        class CommentatorGetMapInfo final : public ClientPacket
        {
        public:
            explicit CommentatorGetMapInfo(WorldPacket&& packet) : ClientPacket(CMSG_COMMENTATOR_GET_MAP_INFO, std::move(packet)) { }

            void Read() override;

            std::string TargetPlayer;                       // optional player name to centre the map list on
        };

        class CommentatorEnterInstance final : public ClientPacket
        {
        public:
            explicit CommentatorEnterInstance(WorldPacket&& packet) : ClientPacket(CMSG_COMMENTATOR_ENTER_INSTANCE, std::move(packet)) { }

            void Read() override;

            uint32 MapID = 0;
            uint32 InstanceIDLow = 0;
            uint32 InstanceIDHigh = 0;
            bool Field3 = false;                            // trailing bit (unnamed offline)
        };

        class CommentatorExitInstance final : public ClientPacket
        {
        public:
            explicit CommentatorExitInstance(WorldPacket&& packet) : ClientPacket(CMSG_COMMENTATOR_EXIT_INSTANCE, std::move(packet)) { }

            void Read() override { }                         // empty payload
        };

        class CommentatorSpectate final : public ClientPacket
        {
        public:
            explicit CommentatorSpectate(WorldPacket&& packet) : ClientPacket(CMSG_COMMENTATOR_SPECTATE, std::move(packet)) { }

            void Read() override;

            std::string TargetName;                          // player to follow
        };

        // SMSG_COMMENTATOR_MAP_INFO - the catalogue of arena maps and their currently-active instances.
        // Wire recovered byte-exact from the client deserializer (all fixed-width LE; guids are PackedGuid).
        class CommentatorMapInfo final : public ServerPacket
        {
        public:
            explicit CommentatorMapInfo() : ServerPacket(SMSG_COMMENTATOR_MAP_INFO, 64) { }

            WorldPacket const* Write() override;

            struct PlayerInfo
            {
                ObjectGuid PlayerGUID;
                uint32 Field1 = 0;                          // per-player triple (hypothesis: specID)
                uint32 Field2 = 0;
                uint8 Field3 = 0;                           // hypothesis: faction
            };

            struct TeamInfo
            {
                ObjectGuid TeamGUID;
                std::vector<PlayerInfo> Players;
            };

            struct InstanceInfo
            {
                uint32 MapID = 0;
                uint32 Field1 = 0;                          // per-instance triple (unnamed offline)
                uint32 Field2 = 0;
                uint8 Field3 = 0;
                uint64 InstanceID = 0;                      // InstanceIDLow | (InstanceIDHigh << 32)
                uint32 Status = 0;
                std::array<TeamInfo, 2> Teams;              // arena = 2 factions
            };

            struct MapInfo
            {
                uint32 TeamSize = 0;
                uint32 MinLevel = 0;
                uint32 MaxLevel = 0;
                uint16 Field3 = 0;                          // unnamed (bracket/season/flags?)
                std::vector<InstanceInfo> Instances;
            };

            uint64 DirectoryId = 0;                         // opaque blob id (unnamed offline)
            std::vector<MapInfo> Maps;
        };

        class CommentatorStateChanged final : public ServerPacket
        {
        public:
            explicit CommentatorStateChanged() : ServerPacket(SMSG_COMMENTATOR_STATE_CHANGED, 16 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid MatchGUID;
            bool Enabled = false;
        };
    }
}

#endif // TRINITYCORE_COMMENTATOR_PACKETS_H
