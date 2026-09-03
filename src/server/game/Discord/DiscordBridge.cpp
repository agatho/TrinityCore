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
#include "DatabaseEnv.h"
#include "DiscordRestProvider.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Log.h"
#include <unordered_map>

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

    if (!_enabled)
    {
        // Reloading with the bridge turned off: drop any live provider (joins its I/O thread).
        _provider = std::make_unique<IDiscordBridgeProvider>();
        TC_LOG_INFO("server.loading", "Discord bridge disabled (Guild.DiscordBridge.Enabled = 0).");
        return;
    }

    // Enabled: install the real REST provider if a bot token is configured, otherwise fall back to
    // the honest no-op (so the chat type / per-guild settings still exist, but nothing is claimed
    // to be connected).
    std::string botToken = sConfigMgr->GetStringDefault("Guild.DiscordBridge.BotToken", "");
    if (botToken.empty())
    {
        _provider = std::make_unique<IDiscordBridgeProvider>();
        TC_LOG_WARN("server.loading", "Discord bridge enabled but Guild.DiscordBridge.BotToken is empty - "
            "running as no-op. Set a bot token to connect to Discord.");
        return;
    }

    // Cache the connection config so a runtime link (CMSG_DISCORD_GUILD_LINK) can lazily build a
    // REST provider even if no guild was linked at startup.
    _botToken = std::move(botToken);
    _apiHost = sConfigMgr->GetStringDefault("Guild.DiscordBridge.ApiHost", "discord.com");
    _caBundleFile = sConfigMgr->GetStringDefault("Guild.DiscordBridge.CaBundle", "");
    _pollIntervalMs = sConfigMgr->GetIntDefault("Guild.DiscordBridge.PollIntervalMs", 3000);
    _verifyCertificate = sConfigMgr->GetBoolDefault("Guild.DiscordBridge.VerifyCertificate", true);

    // Build the guildId -> Discord channel id map from guild_discord_settings.
    _linkedChannels.clear();
    if (QueryResult result = CharacterDatabase.Query("SELECT guildid, discordChannelId FROM guild_discord_settings WHERE discordChannelId <> 0"))
    {
        do
        {
            Field* fields = result->Fetch();
            _linkedChannels[fields[0].GetUInt64()] = fields[1].GetUInt64();
        } while (result->NextRow());
    }

    if (_linkedChannels.empty())
    {
        _provider = std::make_unique<IDiscordBridgeProvider>();
        TC_LOG_WARN("server.loading", "Discord bridge enabled with a bot token but no guild has a linked "
            "channel (guild_discord_settings.discordChannelId). Running as no-op until a channel is linked.");
        return;
    }

    RebuildRestProvider();

    TC_LOG_INFO("server.loading", "Discord bridge enabled (provider: '{}'). Inbound Discord messages "
        "surface as CHAT_MSG_GUILD_DISCORD guild lines; outbound guild chat mirroring is {}.",
        _provider->GetProviderName(), _forwardOutbound ? "ON" : "OFF");
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

bool DiscordBridge::RebuildRestProvider()
{
    if (!_enabled || _botToken.empty())
        return false;

    DiscordRestProvider::Settings settings;
    settings.BotToken = _botToken;
    settings.ApiHost = _apiHost;
    settings.CaBundleFile = _caBundleFile;
    settings.PollIntervalMs = _pollIntervalMs;
    settings.VerifyCertificate = _verifyCertificate;

    SetProvider(std::make_unique<DiscordRestProvider>(std::move(settings), _linkedChannels));
    return true;
}

void DiscordBridge::SetGuildLink(ObjectGuid::LowType guildId, uint64 discordChannelId)
{
    if (!_enabled)
        return;

    if (discordChannelId != 0)
        _linkedChannels[guildId] = discordChannelId;
    else
        _linkedChannels.erase(guildId);

    // If a real REST provider is already running, update it in place (keeps poll state).
    if (_provider && std::string_view(_provider->GetProviderName()) == "discord-rest")
    {
        _provider->SetGuildChannel(guildId, discordChannelId);
        return;
    }

    // Otherwise we are a no-op provider. If a bot token is configured and at least one link now
    // exists, upgrade to the real REST bridge; nothing to do if the last link was just removed.
    if (!_linkedChannels.empty() && !_botToken.empty())
        RebuildRestProvider();
}

void DiscordBridge::QueueInbound(DiscordInboundMessage message)
{
    // Called from the provider's I/O thread - only touch the guarded queue here, never Guild state.
    std::lock_guard<std::mutex> lock(_inboundMutex);
    _inboundQueue.push_back(std::move(message));
}

void DiscordBridge::Update()
{
    if (!_enabled)
        return;

    std::vector<DiscordInboundMessage> pending;
    {
        std::lock_guard<std::mutex> lock(_inboundMutex);
        if (_inboundQueue.empty())
            return;
        pending.swap(_inboundQueue);
    }

    // Now on the world thread: safe to touch GuildMgr / Guild.
    for (DiscordInboundMessage const& message : pending)
        OnDiscordMessage(message);
}
