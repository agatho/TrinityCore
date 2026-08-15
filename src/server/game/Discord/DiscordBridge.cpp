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

#include "DiscordBridge.h"
#include "Config.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Log.h"

DiscordBridge::DiscordBridge() : _provider(std::make_unique<IDiscordBridgeProvider>())
{
}

DiscordBridge* DiscordBridge::instance()
{
    static DiscordBridge instance;
    return &instance;
}

void DiscordBridge::LoadConfig()
{
    _enabled = sConfigMgr->GetBoolDefault("Guild.DiscordBridge.Enabled", false);
    _forwardOutbound = sConfigMgr->GetBoolDefault("Guild.DiscordBridge.ForwardGuildChat", false);

    if (!_provider)
        _provider = std::make_unique<IDiscordBridgeProvider>();

    if (_enabled)
        TC_LOG_INFO("server.loading", "Discord bridge enabled (provider: '{}', connected: {}). "
            "Inbound Discord messages surface as CHAT_MSG_GUILD_DISCORD guild lines.",
            _provider->GetProviderName(), _provider->IsConnected());
    else
        TC_LOG_INFO("server.loading", "Discord bridge disabled (Guild.DiscordBridge.Enabled = 0).");
}

void DiscordBridge::SetProvider(std::unique_ptr<IDiscordBridgeProvider> provider)
{
    _provider = provider ? std::move(provider) : std::make_unique<IDiscordBridgeProvider>();
    TC_LOG_INFO("server.loading", "Discord bridge provider set to '{}'.", _provider->GetProviderName());
}

char const* DiscordBridge::GetProviderName() const
{
    return _provider ? _provider->GetProviderName() : "none";
}

bool DiscordBridge::IsConnected() const
{
    return _enabled && _provider && _provider->IsConnected();
}

void DiscordBridge::OnDiscordMessage(DiscordInboundMessage const& message)
{
    if (!_enabled)
        return;

    Guild* guild = sGuildMgr->GetGuildById(message.GuildId);
    if (!guild)
        return;

    guild->SendGuildDiscordMessage(message.SenderDisplayName, message.Content, message.SenderDiscordId);
}

void DiscordBridge::ForwardGuildChat(ObjectGuid::LowType guildId, std::string_view senderName, std::string_view message)
{
    if (!_enabled || !_forwardOutbound || !_provider)
        return;

    // No-op provider simply drops this; a real bot/webhook forwards to the linked channel.
    _provider->ForwardGuildChatToDiscord(guildId, senderName, message);
}
