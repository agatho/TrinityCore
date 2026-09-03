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
#include "DatabaseEnv.h"
#include "DiscordDefines.h"
#include "DiscordPackets.h"
#include "Guild.h"
#include "Log.h"
#include "Player.h"

// Client-driven Discord opcodes (12.1.0). Wire recovered from the 12.1 client packet writers; see
// DiscordPackets.h. There is no dedicated SMSG_DISCORD_* opcode - the client reads its
// server-authoritative Discord state from ActivePlayerData::DiscordInfo, so these handlers refresh
// that update field (and per-guild state) rather than sending a bespoke response packet.

void WorldSession::HandleDiscordRefreshAuth(WorldPackets::Discord::DiscordRefreshAuth& /*packet*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // C_Discord.RefreshAuth(): re-read the account's Discord OAuth link (written by the bnetserver
    // Discord link store / external OAuth linker) and push it into DiscordInfo. Changing the field
    // makes the client fire DISCORD_STATUS_UPDATE / DISCORD_LINK_UPDATE and re-read IsUserOAuthed().
    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ACCOUNT_DISCORD);
    stmt->setUInt32(0, GetAccountId());
    if (PreparedQueryResult result = LoginDatabase.Query(stmt))
    {
        Field* fields = result->Fetch();
        player->SetDiscordAuthInfo(fields[0].GetUInt64(), fields[2].GetUInt8(), fields[3].GetString());
    }
    else
        player->SetDiscordAuthInfo(0, 0, "");   // account is not linked to a Discord identity

    TC_LOG_DEBUG("network", "CMSG_DISCORD_REFRESH_AUTH [{}]", GetPlayerInfo());
}

void WorldSession::HandleDiscordSetDisplayNameType(WorldPackets::Discord::DiscordSetDisplayNameType& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Reject out-of-range values (client enum DiscordDisplayNameType: 0 Default .. 2 GlobalName).
    if (packet.DisplayNameType >= uint8(DiscordDisplayNameType::Max))
    {
        TC_LOG_DEBUG("network", "CMSG_DISCORD_SET_DISPLAY_NAME_TYPE [{}]: invalid type {}",
            GetPlayerInfo(), packet.DisplayNameType);
        return;
    }

    player->SetDiscordDisplayNameType(packet.DisplayNameType);
    player->SaveDiscordDisplayNameType();
}

void WorldSession::HandleDiscordGuildLink(WorldPackets::Discord::DiscordGuildLink& packet)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleDiscordGuildLink(this, packet.DiscordServerId, packet.DiscordChannelId);
}

void WorldSession::HandleDiscordGuildUnlink(WorldPackets::Discord::DiscordGuildUnlink& /*packet*/)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleDiscordGuildUnlink(this);
}

void WorldSession::HandleDiscordSetGuildSetting(WorldPackets::Discord::DiscordSetGuildSetting& packet)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleDiscordSetGuildSetting(this, packet.Settings);
}
