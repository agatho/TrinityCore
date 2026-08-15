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

#include "DiscordRestProvider.h"
#include "Log.h"
#include "StringFormat.h"
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <openssl/ssl.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>

namespace
{
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace asio = boost::asio;
    namespace ssl = boost::asio::ssl;
    using tcp = boost::asio::ip::tcp;

    // Discord rejects requests without a descriptive User-Agent (API ToS). Keep the format Discord
    // documents: DiscordBot(<url>, <version>).
    constexpr char const* DISCORD_USER_AGENT = "DiscordBot (https://www.trinitycore.org, 12.1)";
    constexpr char const* DISCORD_API_VERSION = "/api/v10";
    constexpr std::size_t DISCORD_MAX_CONTENT = 2000;   // Discord hard limit on message content

    // JSON-encode a single string value (used to build the message payload safely).
    std::string JsonQuote(std::string_view s)
    {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        writer.String(s.data(), rapidjson::SizeType(s.length()));
        return std::string(buffer.GetString(), buffer.GetSize());
    }
}

DiscordRestProvider::DiscordRestProvider(Settings settings, std::unordered_map<uint64, uint64> linkedChannels)
    : _settings(std::move(settings)), _guildToChannel(std::move(linkedChannels)), _stop(false), _connected(false), _botUserId(0)
{
    for (auto const& [guildId, channelId] : _guildToChannel)
        if (channelId)
            _channelToGuild[channelId] = guildId;

    TC_LOG_INFO("server.loading", "Discord REST provider starting: host '{}', {} linked channel(s), poll {} ms.",
        _settings.ApiHost, uint32(_channelToGuild.size()), _settings.PollIntervalMs);

    _worker = std::thread(&DiscordRestProvider::WorkerLoop, this);
}

DiscordRestProvider::~DiscordRestProvider()
{
    _stop.store(true, std::memory_order_relaxed);
    _wake.notify_all();
    if (_worker.joinable())
        _worker.join();
}

void DiscordRestProvider::ForwardGuildChatToDiscord(ObjectGuid::LowType guildId, std::string_view senderName, std::string_view message)
{
    auto itr = _guildToChannel.find(guildId);
    if (itr == _guildToChannel.end() || !itr->second)
        return;     // this guild is not bridged

    // "**Name**: message", clamped to Discord's content limit.
    std::string line = Trinity::StringFormat("**{}**: {}", senderName, message);
    if (line.size() > DISCORD_MAX_CONTENT)
        line.resize(DISCORD_MAX_CONTENT);

    // Body: {"content":"...","allowed_mentions":{"parse":[]}} - the empty parse list stops a guild
    // member from @everyone-ing a Discord server through the bridge.
    std::string payload = Trinity::StringFormat(R"({{"content":{},"allowed_mentions":{{"parse":[]}}}})", JsonQuote(line));

    {
        std::lock_guard<std::mutex> lock(_outboundMutex);
        _outbound.push_back({ itr->second, std::move(payload) });
    }
    _wake.notify_all();
}

void DiscordRestProvider::WorkerLoop()
{
    // Establish identity first; retry with backoff so a transient network/Discord outage does not
    // permanently disable the bridge.
    while (!_stop.load(std::memory_order_relaxed) && !FetchBotIdentity())
    {
        std::unique_lock<std::mutex> lock(_outboundMutex);
        _wake.wait_for(lock, std::chrono::seconds(30), [this] { return _stop.load(std::memory_order_relaxed); });
    }
    if (_stop.load(std::memory_order_relaxed))
        return;

    auto nextPoll = std::chrono::steady_clock::now();
    while (!_stop.load(std::memory_order_relaxed))
    {
        FlushOutbound();

        auto now = std::chrono::steady_clock::now();
        if (now >= nextPoll)
        {
            PollChannels();
            nextPoll = std::chrono::steady_clock::now() + std::chrono::milliseconds(_settings.PollIntervalMs);
        }

        std::unique_lock<std::mutex> lock(_outboundMutex);
        _wake.wait_until(lock, nextPoll, [this] { return _stop.load(std::memory_order_relaxed) || !_outbound.empty(); });
    }
}

bool DiscordRestProvider::FetchBotIdentity()
{
    HttpResponse res = HttpGet(std::string(DISCORD_API_VERSION) + "/users/@me");
    if (!res.TransportOk)
    {
        TC_LOG_ERROR("server.loading", "Discord bridge: could not reach {} (TLS/connect failed). Will retry.", _settings.ApiHost);
        return false;
    }
    if (res.Status == 401)
    {
        TC_LOG_ERROR("server.loading", "Discord bridge: bot token rejected (HTTP 401). Check Guild.DiscordBridge.BotToken.");
        return false;
    }
    if (res.Status != 200)
    {
        TC_LOG_ERROR("server.loading", "Discord bridge: GET /users/@me returned HTTP {}. Will retry.", res.Status);
        return false;
    }

    rapidjson::Document doc;
    doc.Parse(res.Body.c_str(), res.Body.length());
    if (doc.HasParseError() || !doc.IsObject())
        return false;

    auto id = doc.FindMember("id");
    if (id == doc.MemberEnd() || !id->value.IsString())
        return false;

    _botUserId = std::strtoull(id->value.GetString(), nullptr, 10);

    std::string username;
    if (auto name = doc.FindMember("username"); name != doc.MemberEnd() && name->value.IsString())
        username = name->value.GetString();

    _connected.store(true, std::memory_order_relaxed);
    TC_LOG_INFO("server.loading", "Discord bridge connected as '{}' (id {}). Bridging {} channel(s).",
        username, _botUserId, uint32(_channelToGuild.size()));
    return true;
}

void DiscordRestProvider::FlushOutbound()
{
    for (;;)
    {
        OutboundMessage msg;
        {
            std::lock_guard<std::mutex> lock(_outboundMutex);
            if (_outbound.empty())
                return;
            msg = std::move(_outbound.front());
            _outbound.pop_front();
        }

        std::string target = Trinity::StringFormat("{}/channels/{}/messages", DISCORD_API_VERSION, msg.ChannelId);
        HttpResponse res = HttpPost(target, msg.Payload);

        if (res.Status == 429)
        {
            // Rate limited: honour retry_after (seconds) and requeue the message at the front.
            double retryAfter = 1.0;
            rapidjson::Document doc;
            doc.Parse(res.Body.c_str(), res.Body.length());
            if (!doc.HasParseError() && doc.IsObject())
                if (auto ra = doc.FindMember("retry_after"); ra != doc.MemberEnd() && ra->value.IsNumber())
                    retryAfter = ra->value.GetDouble();

            {
                std::lock_guard<std::mutex> lock(_outboundMutex);
                _outbound.push_front(std::move(msg));
            }
            TC_LOG_DEBUG("misc", "Discord bridge: outbound rate limited, backing off {:.2f}s.", retryAfter);
            std::this_thread::sleep_for(std::chrono::milliseconds(int64(retryAfter * 1000.0) + 50));
            return; // let the loop re-enter after the backoff
        }

        if (res.TransportOk && (res.Status == 200 || res.Status == 201))
            continue;

        TC_LOG_DEBUG("misc", "Discord bridge: outbound to channel {} failed (transport {}, HTTP {}).",
            msg.ChannelId, res.TransportOk, res.Status);
        // Drop on hard failure rather than loop forever on a bad channel/permission.
    }
}

void DiscordRestProvider::PollChannels()
{
    for (auto const& [channelId, guildId] : _channelToGuild)
    {
        if (_stop.load(std::memory_order_relaxed))
            return;
        PollOneChannel(channelId, guildId);
    }
}

void DiscordRestProvider::PollOneChannel(uint64 channelId, uint64 guildId)
{
    std::string const& last = _lastMessageId[channelId];

    // First poll: seed the cursor with the newest message id WITHOUT replaying history.
    if (last.empty())
    {
        HttpResponse seed = HttpGet(Trinity::StringFormat("{}/channels/{}/messages?limit=1", DISCORD_API_VERSION, channelId));
        if (seed.TransportOk && seed.Status == 200)
        {
            rapidjson::Document doc;
            doc.Parse(seed.Body.c_str(), seed.Body.length());
            if (!doc.HasParseError() && doc.IsArray() && !doc.Empty())
                if (auto id = doc[0].FindMember("id"); id != doc[0].MemberEnd() && id->value.IsString())
                    _lastMessageId[channelId] = id->value.GetString();
        }
        else if (seed.Status == 403)
            TC_LOG_ERROR("misc", "Discord bridge: no permission to read channel {} (HTTP 403). Grant the bot 'View Channel' + 'Read Message History'.", channelId);
        return;
    }

    HttpResponse res = HttpGet(Trinity::StringFormat("{}/channels/{}/messages?after={}&limit=50", DISCORD_API_VERSION, channelId, last));
    if (!res.TransportOk || res.Status != 200)
        return;

    rapidjson::Document doc;
    doc.Parse(res.Body.c_str(), res.Body.length());
    if (doc.HasParseError() || !doc.IsArray() || doc.Empty())
        return;

    // Discord returns newest-first; deliver oldest-first and advance the cursor to the newest id.
    std::string newest;
    for (rapidjson::SizeType i = doc.Size(); i-- > 0; )
    {
        rapidjson::Value const& m = doc[i];
        if (!m.IsObject())
            continue;

        auto idItr = m.FindMember("id");
        if (idItr == m.MemberEnd() || !idItr->value.IsString())
            continue;
        newest = idItr->value.GetString();      // ascending order => last wins

        auto authorItr = m.FindMember("author");
        if (authorItr == m.MemberEnd() || !authorItr->value.IsObject())
            continue;
        rapidjson::Value const& author = authorItr->value;

        // Skip bots (including our own mirrored posts and webhooks) to prevent echo loops.
        if (auto botFlag = author.FindMember("bot"); botFlag != author.MemberEnd() && botFlag->value.IsBool() && botFlag->value.GetBool())
            continue;

        auto contentItr = m.FindMember("content");
        if (contentItr == m.MemberEnd() || !contentItr->value.IsString() || contentItr->value.GetStringLength() == 0)
            continue;

        DiscordInboundMessage inbound;
        inbound.GuildId = guildId;
        inbound.Content = contentItr->value.GetString();

        if (auto aid = author.FindMember("id"); aid != author.MemberEnd() && aid->value.IsString())
            inbound.SenderDiscordId = std::strtoull(aid->value.GetString(), nullptr, 10);

        // Prefer the server display name, then global name, then username.
        if (auto gn = author.FindMember("global_name"); gn != author.MemberEnd() && gn->value.IsString() && gn->value.GetStringLength())
            inbound.SenderDisplayName = gn->value.GetString();
        else if (auto un = author.FindMember("username"); un != author.MemberEnd() && un->value.IsString())
            inbound.SenderDisplayName = un->value.GetString();
        else
            inbound.SenderDisplayName = "discord";

        sDiscordBridge->QueueInbound(inbound);
    }

    if (!newest.empty())
        _lastMessageId[channelId] = newest;
}

DiscordRestProvider::HttpResponse DiscordRestProvider::HttpGet(std::string const& target)
{
    return Perform(int(http::verb::get), target, std::string());
}

DiscordRestProvider::HttpResponse DiscordRestProvider::HttpPost(std::string const& target, std::string const& jsonBody)
{
    return Perform(int(http::verb::post), target, jsonBody);
}

DiscordRestProvider::HttpResponse DiscordRestProvider::Perform(int beastVerb, std::string const& target, std::string const& jsonBody)
{
    HttpResponse result;
    try
    {
        asio::io_context ioc;

        ssl::context ctx(ssl::context::tls_client);
        if (_settings.VerifyCertificate)
        {
            ctx.set_verify_mode(ssl::verify_peer);
            if (!_settings.CaBundleFile.empty())
                ctx.load_verify_file(_settings.CaBundleFile);
            else
                ctx.set_default_verify_paths();
        }
        else
            ctx.set_verify_mode(ssl::verify_none);

        tcp::resolver resolver(ioc);
        auto const endpoints = resolver.resolve(_settings.ApiHost, "443");

        ssl::stream<tcp::socket> stream(ioc, ctx);

        // SNI - Discord (Cloudflare) requires the server name to route TLS correctly.
        if (!SSL_set_tlsext_host_name(stream.native_handle(), _settings.ApiHost.c_str()))
            throw beast::system_error(beast::error_code(int(::ERR_get_error()), asio::error::get_ssl_category()));

        if (_settings.VerifyCertificate)
            stream.set_verify_callback(ssl::host_name_verification(_settings.ApiHost));

        asio::connect(stream.next_layer(), endpoints);
        stream.handshake(ssl::stream_base::client);

        http::request<http::string_body> req(static_cast<http::verb>(beastVerb), target, 11);
        req.set(http::field::host, _settings.ApiHost);
        req.set(http::field::user_agent, DISCORD_USER_AGENT);
        req.set(http::field::authorization, "Bot " + _settings.BotToken);
        req.set(http::field::accept, "application/json");
        if (!jsonBody.empty())
        {
            req.set(http::field::content_type, "application/json");
            req.body() = jsonBody;
            req.prepare_payload();
        }

        http::write(stream, req);

        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);

        result.Status = res.result_int();
        result.Body = res.body();
        result.TransportOk = true;

        beast::error_code ec;
        stream.shutdown(ec);    // best-effort; Discord closes first, ignore the resulting truncation
    }
    catch (std::exception const& ex)
    {
        result.TransportOk = false;
        TC_LOG_DEBUG("misc", "Discord bridge: HTTP request to '{}' threw: {}", target, ex.what());
    }
    return result;
}
