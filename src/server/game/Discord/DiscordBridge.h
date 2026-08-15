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

#ifndef TRINITYCORE_DISCORD_BRIDGE_H
#define TRINITYCORE_DISCORD_BRIDGE_H

#include "Define.h"
#include "DiscordDefines.h"
#include "ObjectGuid.h"
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

// EXTERNAL BOUNDARY (spec section C).
//
// A real connection to a Discord server (posting/reading messages) requires an external
// Discord bot or webhook - an external service, exactly like the shop2 external checkout.
// The core intentionally ships a *no-op* bridge and a clean registration seam so that:
//   * the server protocol surface (CHAT_MSG_GUILD_DISCORD, per-guild settings) exists and
//     is exercised, and
//   * a real provider can be dropped in behind IDiscordBridge without touching the core.
//
// The default build never claims to be "connected to Discord". It is honestly a no-op until
// an external provider is registered via DiscordBridge::SetProvider().

// One inbound message delivered from Discord that should surface as a guild chat line.
struct DiscordInboundMessage
{
    ObjectGuid::LowType GuildId = 0;
    DiscordUserId SenderDiscordId = 0;
    std::string SenderDisplayName;      // shown as the CHAT_MSG_GUILD_DISCORD sender name
    std::string Content;
};

// The seam a real bot/webhook implementation fulfils. All methods are no-ops in the default.
class TC_GAME_API IDiscordBridgeProvider
{
public:
    virtual ~IDiscordBridgeProvider() = default;

    // Forward an in-game guild chat line out to the linked Discord channel.
    // Called by the core whenever a guild message should be mirrored to Discord.
    virtual void ForwardGuildChatToDiscord(ObjectGuid::LowType /*guildId*/,
        std::string_view /*senderName*/, std::string_view /*message*/) { }

    // True only when a real provider has an established Discord connection.
    virtual bool IsConnected() const { return false; }

    virtual char const* GetProviderName() const { return "noop"; }
};

class TC_GAME_API DiscordBridge
{
public:
    static DiscordBridge* instance();

    // Reads Guild.DiscordBridge.* config. Installs the no-op provider unless a real one is set.
    void LoadConfig();

    bool IsEnabled() const { return _enabled; }
    bool IsConnected() const;

    // Registration seam for a real external bridge (bot/webhook). Passing nullptr reinstalls
    // the no-op provider. Ownership is transferred to the bridge.
    void SetProvider(std::unique_ptr<IDiscordBridgeProvider> provider);
    char const* GetProviderName() const;

    // Inbound (WORLD THREAD ONLY): routes a message into guild chat as CHAT_MSG_GUILD_DISCORD
    // (no-op if disabled or the guild is gone). Called by Update() while draining the queue, or
    // directly by world-thread code.
    void OnDiscordMessage(DiscordInboundMessage const& message);

    // Inbound (THREAD SAFE): a real provider running on its own I/O thread calls this to hand a
    // Discord message to the core. It is queued and delivered on the world thread by Update().
    void QueueInbound(DiscordInboundMessage message);

    // World-thread pump: drains the inbound queue and routes each message. Called from World::Update.
    void Update();

    // Outbound: the core calls this when a guild chat line should be mirrored to Discord.
    // No-op unless enabled and a connected provider is installed.
    void ForwardGuildChat(ObjectGuid::LowType guildId, std::string_view senderName, std::string_view message);

private:
    DiscordBridge();

    bool _enabled = false;                                  // Guild.DiscordBridge.Enabled
    bool _forwardOutbound = false;                          // Guild.DiscordBridge.ForwardGuildChat
    std::unique_ptr<IDiscordBridgeProvider> _provider;      // no-op by default

    // Inbound messages handed over from the provider's I/O thread, drained on the world thread.
    std::mutex _inboundMutex;
    std::vector<DiscordInboundMessage> _inboundQueue;
};

#define sDiscordBridge DiscordBridge::instance()

#endif // TRINITYCORE_DISCORD_BRIDGE_H
