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

#include "DiscordLinkStore.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "Log.h"
#include "LoginDatabase.h"
#include "PreparedStatement.h"
#include "QueryResult.h"

namespace Battlenet
{
    DiscordLinkStore* DiscordLinkStore::instance()
    {
        static DiscordLinkStore instance;
        return &instance;
    }

    void DiscordLinkStore::LoadConfig()
    {
        _enabled = sConfigMgr->GetBoolDefault("BattlenetDiscord.Enabled", false);

        if (_enabled)
            TC_LOG_INFO("server.bnetserver", "Discord account-link store enabled. Awaiting an external "
                "linker to populate presence external identities. NOTE: the client server/channel "
                "attribute wire is past the offline RE ceiling and is not published on the wire yet.");
        else
            TC_LOG_INFO("server.bnetserver", "Discord account-link store disabled (BattlenetDiscord.Enabled = 0).");
    }

    Optional<DiscordLink> DiscordLinkStore::GetLinkForAccount(uint32 bnetAccountId) const
    {
        if (!_enabled)
            return {};

        {
            std::lock_guard<std::mutex> guard(_mutex);
            auto itr = _links.find(bnetAccountId);
            if (itr != _links.end())
                return itr->second;
        }

        // Cache miss: read the persisted link from `account_discord` (shared with the worldserver).
        LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ACCOUNT_DISCORD);
        stmt->setUInt32(0, bnetAccountId);
        PreparedQueryResult result = LoginDatabase.Query(stmt);
        if (!result)
            return {};

        Field* fields = result->Fetch();
        DiscordLink link;
        link.DiscordUserId = fields[0].GetUInt64();
        link.DiscordUserName = fields[1].GetString();
        link.AccountType = DiscordAccountType(fields[2].GetUInt8());

        std::lock_guard<std::mutex> guard(_mutex);
        _links[bnetAccountId] = link;
        return link;
    }

    void DiscordLinkStore::SetLink(uint32 bnetAccountId, DiscordLink link)
    {
        {
            std::lock_guard<std::mutex> guard(_mutex);
            _links[bnetAccountId] = link;
        }

        // Write-through so the worldserver can answer CMSG_DISCORD_REFRESH_AUTH from `account_discord`.
        LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_REP_ACCOUNT_DISCORD);
        stmt->setUInt32(0, bnetAccountId);
        stmt->setUInt64(1, link.DiscordUserId);
        stmt->setString(2, link.DiscordUserName);
        stmt->setUInt8(3, uint8(link.AccountType));
        stmt->setString(4, "");             // access token is populated by the OAuth linker when available
        LoginDatabase.Execute(stmt);
    }

    void DiscordLinkStore::RemoveLink(uint32 bnetAccountId)
    {
        {
            std::lock_guard<std::mutex> guard(_mutex);
            _links.erase(bnetAccountId);
        }

        LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_DEL_ACCOUNT_DISCORD);
        stmt->setUInt32(0, bnetAccountId);
        LoginDatabase.Execute(stmt);
    }
}
