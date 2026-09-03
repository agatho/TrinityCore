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

#ifndef TRINITYCORE_DISCORD_PACKETS_H
#define TRINITYCORE_DISCORD_PACKETS_H

#include "Packet.h"

// Client-driven Discord opcodes (12.1.0). The wire below was recovered by RE'ing the client
// packet writers in the 12.1 client binary (ImageBase 0x140000000); every field is grounded
// in the disassembled writer, nothing is invented:
//   CMSG_DISCORD_REFRESH_AUTH          (0x3D02EC) writer @0x1406d1b50 : header only (empty body)
//   CMSG_DISCORD_SET_DISPLAY_NAME_TYPE (0x3D02ED) writer @0x1406d1b80 : u8  DisplayNameType
//   CMSG_DISCORD_GUILD_LINK            (0x3D02EE) writer @0x1406d1c10 : u64 ServerId, u64 ChannelId
//   CMSG_DISCORD_GUILD_UNLINK          (0x3D02EF) writer @0x1406d1c70 : header only (empty body)
//   CMSG_DISCORD_SET_GUILD_SETTING     (0x3D02F0) writer @0x1406d1ca0 : u8  Settings
// The C_Discord API surface (DiscordDocumentation.lua) supplies the semantics:
//   GuildLink(serverIndex, channelIndex) -> resolves to the Discord server/channel snowflakes.
//   GuildUnlink(), RefreshAuth()         -> no arguments (empty packets).
//   SetGuildSetting(setting, set)        -> DiscordGuildSettings has exactly one bit
//                                           (SeparateStream=1), so the single wire byte is the new
//                                           settings bitmask (== the toggled bit value).
//   SetDisplayNameType(type)             -> DiscordDisplayNameType enum (0..2).

namespace WorldPackets
{
    namespace Discord
    {
        // CMSG_DISCORD_REFRESH_AUTH (0x3D02EC) - C_Discord.RefreshAuth(). Empty body.
        class DiscordRefreshAuth final : public ClientPacket
        {
        public:
            explicit DiscordRefreshAuth(WorldPacket&& packet) : ClientPacket(CMSG_DISCORD_REFRESH_AUTH, std::move(packet)) { }

            void Read() override { }
        };

        // CMSG_DISCORD_SET_DISPLAY_NAME_TYPE (0x3D02ED) - single u8 DiscordDisplayNameType.
        class DiscordSetDisplayNameType final : public ClientPacket
        {
        public:
            explicit DiscordSetDisplayNameType(WorldPacket&& packet) : ClientPacket(CMSG_DISCORD_SET_DISPLAY_NAME_TYPE, std::move(packet)) { }

            void Read() override;

            uint8 DisplayNameType = 0;
        };

        // CMSG_DISCORD_GUILD_LINK (0x3D02EE) - u64 Discord server (guild) id + u64 Discord channel id.
        class DiscordGuildLink final : public ClientPacket
        {
        public:
            explicit DiscordGuildLink(WorldPacket&& packet) : ClientPacket(CMSG_DISCORD_GUILD_LINK, std::move(packet)) { }

            void Read() override;

            uint64 DiscordServerId = 0;      // linked Discord server (guild) snowflake
            uint64 DiscordChannelId = 0;     // linked Discord text channel snowflake
        };

        // CMSG_DISCORD_GUILD_UNLINK (0x3D02EF) - C_Discord.GuildUnlink(). Empty body.
        class DiscordGuildUnlink final : public ClientPacket
        {
        public:
            explicit DiscordGuildUnlink(WorldPacket&& packet) : ClientPacket(CMSG_DISCORD_GUILD_UNLINK, std::move(packet)) { }

            void Read() override { }
        };

        // CMSG_DISCORD_SET_GUILD_SETTING (0x3D02F0) - single u8 = new DiscordGuildSettings bitmask.
        class DiscordSetGuildSetting final : public ClientPacket
        {
        public:
            explicit DiscordSetGuildSetting(WorldPacket&& packet) : ClientPacket(CMSG_DISCORD_SET_GUILD_SETTING, std::move(packet)) { }

            void Read() override;

            uint8 Settings = 0;
        };
    }
}

#endif // TRINITYCORE_DISCORD_PACKETS_H
