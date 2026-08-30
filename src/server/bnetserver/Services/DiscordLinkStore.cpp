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
#include "Log.h"

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

        std::lock_guard<std::mutex> guard(_mutex);
        auto itr = _links.find(bnetAccountId);
        if (itr == _links.end())
            return {};

        return itr->second;
    }

    void DiscordLinkStore::SetLink(uint32 bnetAccountId, DiscordLink link)
    {
        std::lock_guard<std::mutex> guard(_mutex);
        _links[bnetAccountId] = std::move(link);
    }

    void DiscordLinkStore::RemoveLink(uint32 bnetAccountId)
    {
        std::lock_guard<std::mutex> guard(_mutex);
        _links.erase(bnetAccountId);
    }
}
