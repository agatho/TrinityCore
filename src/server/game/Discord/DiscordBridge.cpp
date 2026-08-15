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

    // Build the guildId -> Discord channel id map from guild_discord_settings.
    std::unordered_map<uint64, uint64> linkedChannels;
    if (QueryResult result = CharacterDatabase.Query("SELECT guildid, discordChannelId FROM guild_discord_settings WHERE discordChannelId <> 0"))
    {
        do
        {
            Field* fields = result->Fetch();
            linkedChannels[fields[0].GetUInt64()] = fields[1].GetUInt64();
        } while (result->NextRow());
    }

    if (linkedChannels.empty())
    {
        _provider = std::make_unique<IDiscordBridgeProvider>();
        TC_LOG_WARN("server.loading", "Discord bridge enabled with a bot token but no guild has a linked "
            "channel (guild_discord_settings.discordChannelId). Running as no-op until a channel is linked.");
        return;
    }

    DiscordRestProvider::Settings settings;
    settings.BotToken = std::move(botToken);
    settings.ApiHost = sConfigMgr->GetStringDefault("Guild.DiscordBridge.ApiHost", "discord.com");
    settings.CaBundleFile = sConfigMgr->GetStringDefault("Guild.DiscordBridge.CaBundle", "");
    settings.PollIntervalMs = sConfigMgr->GetIntDefault("Guild.DiscordBridge.PollIntervalMs", 3000);
    settings.VerifyCertificate = sConfigMgr->GetBoolDefault("Guild.DiscordBridge.VerifyCertificate", true);

    SetProvider(std::make_unique<DiscordRestProvider>(std::move(settings), std::move(linkedChannels)));

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
