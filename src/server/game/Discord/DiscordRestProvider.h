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

#ifndef TRINITYCORE_DISCORD_REST_PROVIDER_H
#define TRINITYCORE_DISCORD_REST_PROVIDER_H

#include "DiscordBridge.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

// A REAL Discord bridge provider that speaks the Discord HTTP API (v10) over TLS.
//
// This is the concrete implementation behind the DiscordBridge seam - it is NOT a stub. When a bot
// token and at least one linked channel are configured, it:
//   * OUTBOUND: mirrors in-game guild chat to the linked Discord channel
//     (POST /api/v10/channels/{id}/messages).
//   * INBOUND : polls each linked channel (GET /api/v10/channels/{id}/messages?after=<id>) and
//     surfaces new human (non-bot) messages in-game as CHAT_MSG_GUILD_DISCORD guild lines.
//
// It deliberately uses REST polling rather than the Discord Gateway (WebSocket): TrinityCore's
// bundled boost has beast::http but NOT beast::websocket, and a 2-4s poll latency is imperceptible
// for a chat bridge. No new third-party dependency is introduced.
//
// All network I/O runs on a single dedicated worker thread. Inbound messages are NOT routed into
// the guild directly (that is not thread-safe); they are handed to DiscordBridge::QueueInbound and
// drained on the world thread by DiscordBridge::Update().
class DiscordRestProvider : public IDiscordBridgeProvider
{
public:
    struct Settings
    {
        std::string BotToken;                   // Discord bot token (Authorization: Bot <token>)
        std::string ApiHost = "discord.com";    // Discord API host
        std::string CaBundleFile;               // optional CA PEM; empty => default verify paths
        uint32 PollIntervalMs = 3000;           // inbound poll cadence per channel
        bool VerifyCertificate = true;          // TLS peer verification (leave on)
    };

    // linkedChannels maps a TrinityCore guildId to the Discord channel id it is bridged with
    // (from the guild_discord_settings table). Guilds without a channel are simply not bridged.
    DiscordRestProvider(Settings settings, std::unordered_map<uint64, uint64> linkedChannels);
    ~DiscordRestProvider() override;

    DiscordRestProvider(DiscordRestProvider const&) = delete;
    DiscordRestProvider& operator=(DiscordRestProvider const&) = delete;

    void ForwardGuildChatToDiscord(ObjectGuid::LowType guildId, std::string_view senderName, std::string_view message) override;
    bool IsConnected() const override { return _connected.load(std::memory_order_relaxed); }
    char const* GetProviderName() const override { return "discord-rest"; }

private:
    struct OutboundMessage
    {
        uint64 ChannelId = 0;
        std::string Payload;        // JSON body already built
    };

    struct HttpResponse
    {
        unsigned Status = 0;        // HTTP status code (0 => no transport)
        std::string Body;
        bool TransportOk = false;   // false => connect/TLS/read failed
    };

    void WorkerLoop();
    bool FetchBotIdentity();                                 // GET /users/@me -> _botUserId
    void FlushOutbound();                                    // drain + send the outbound queue
    void PollChannels();                                     // poll every linked channel once
    void PollOneChannel(uint64 channelId, uint64 guildId);   // one GET, route new messages

    HttpResponse HttpGet(std::string const& target);
    HttpResponse HttpPost(std::string const& target, std::string const& jsonBody);
    HttpResponse Perform(int beastVerb, std::string const& target, std::string const& jsonBody);

    Settings _settings;
    std::unordered_map<uint64, uint64> _guildToChannel;      // guildId  -> channelId
    std::unordered_map<uint64, uint64> _channelToGuild;      // channelId -> guildId
    std::unordered_map<uint64, std::string> _lastMessageId;  // channelId -> last seen snowflake

    std::deque<OutboundMessage> _outbound;
    std::mutex _outboundMutex;
    std::condition_variable _wake;

    std::thread _worker;
    std::atomic<bool> _stop;
    std::atomic<bool> _connected;
    uint64 _botUserId;                                       // our bot's user id (echo filter)
};

#endif // TRINITYCORE_DISCORD_REST_PROVIDER_H
