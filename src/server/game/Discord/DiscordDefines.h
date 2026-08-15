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

#ifndef TRINITYCORE_DISCORD_DEFINES_H
#define TRINITYCORE_DISCORD_DEFINES_H

#include "Define.h"

// Discord integration constants recovered from the 12.1.0 client (build 69299).
// See tools/dump121/discord/discord_12_1_spec.md for the full RE. Every value below is
// grounded in the client binary [BIN]; nothing here is invented wire.

// Battle.net account link maturity for the linked Discord account.
// Client enum registrar @ RVA 0xE3F030 (Enum.DiscordAccountType, Meta NumValues=2).
enum class DiscordAccountType : uint8
{
    Normal      = 0,
    Provisional = 1,

    Max
};

// How the player's Discord name is shown in-game.
// Client enum registrar @ RVA 0xE3F030 (Enum.DiscordDisplayNameType, Meta NumValues=3).
enum class DiscordDisplayNameType : uint8
{
    Default    = 0,
    LastOnline = 1,
    GlobalName = 2,

    Max
};

// Per-guild Discord link settings bitmask. Client enum registrar @ RVA 0xE3F030
// (Enum.DiscordGuildSettings, Meta NumValues=1, Min=1, Max=1). Backs C_Discord
// IsDiscordStreamSeparate / IsGuildSettingSet / SetGuildSetting.
enum DiscordGuildSettings : uint32
{
    DISCORD_GUILD_SETTING_NONE            = 0x0,
    DISCORD_GUILD_SETTING_SEPARATE_STREAM = 0x1, // "SeparateStream" - Discord traffic on its own stream

    DISCORD_GUILD_SETTING_MASK            = DISCORD_GUILD_SETTING_SEPARATE_STREAM
};

// OpaqueDiscordUserID in the client is a 64-bit opaque handle (see .rdata type strings
// `enum OpaqueDiscordUserID` paired with unsigned __int64 in the client's hash maps).
using DiscordUserId = uint64;

#endif // TRINITYCORE_DISCORD_DEFINES_H
