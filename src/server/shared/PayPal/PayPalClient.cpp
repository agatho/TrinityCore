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

#include "PayPalClient.h"
#include "Base64.h"
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
#include <vector>

namespace
{
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace asio = boost::asio;
    namespace ssl = boost::asio::ssl;
    using tcp = boost::asio::ip::tcp;

    constexpr char const* PAYPAL_HOST_SANDBOX = "api-m.sandbox.paypal.com";
    constexpr char const* PAYPAL_HOST_LIVE    = "api-m.paypal.com";
    constexpr char const* PAYPAL_USER_AGENT   = "TrinityCore/12.1 (+https://www.trinitycore.org)";

    // Safety margin before expiry at which we proactively refresh the OAuth token.
    constexpr int64 TOKEN_REFRESH_MARGIN_SEC = 60;

    // JSON-encode a single string value (quotes + escapes). Used to build request bodies safely.
    std::string JsonQuote(std::string_view s)
    {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        writer.String(s.data(), rapidjson::SizeType(s.length()));
        return std::string(buffer.GetString(), buffer.GetSize());
    }
}

namespace Trinity::PayPal
{
PayPalClient::PayPalClient(Config cfg) : _cfg(std::move(cfg))
{
}

PayPalClient::~PayPalClient() = default;

char const* PayPalClient::ApiHost() const
{
    return _cfg.mode == Mode::Live ? PAYPAL_HOST_LIVE : PAYPAL_HOST_SANDBOX;
}

std::string PayPalClient::GetAccessToken()
{
    std::lock_guard<std::mutex> lock(_tokenMutex);

    // Serve a cached token until it is within the refresh margin of expiry.
    if (!_cachedToken.empty() && std::chrono::steady_clock::now() < _tokenExpiry)
        return _cachedToken;

    if (_cfg.clientId.empty() || _cfg.secret.empty())
    {
        TC_LOG_ERROR("server.paypal", "PayPal: cannot request an access token, ClientId/Secret not configured.");
        return std::string();
    }

    // Authorization: Basic base64(CLIENT_ID:CLIENT_SECRET)
    std::string creds = _cfg.clientId + ":" + _cfg.secret;
    std::string basic = "Basic " + Trinity::Encoding::Base64::Encode(std::vector<uint8>(creds.begin(), creds.end()));

    HttpResult res = Perform(int(http::verb::post), "/v1/oauth2/token",
        "grant_type=client_credentials", "application/x-www-form-urlencoded", basic);

    if (!res.transportOk)
    {
        TC_LOG_ERROR("server.paypal", "PayPal: OAuth token request could not reach {} (TLS/connect failed).", ApiHost());
        return std::string();
    }
    if (res.status != 200)
    {
        // NEVER log the body here - it may echo credential-related detail.
        TC_LOG_ERROR("server.paypal", "PayPal: OAuth token request returned HTTP {} (check ClientId/Secret/Mode).", res.status);
        return std::string();
    }

    rapidjson::Document doc;
    doc.Parse(res.body.c_str(), res.body.length());
    if (doc.HasParseError() || !doc.IsObject())
    {
        TC_LOG_ERROR("server.paypal", "PayPal: OAuth token response was not valid JSON.");
        return std::string();
    }

    auto tokenItr = doc.FindMember("access_token");
    if (tokenItr == doc.MemberEnd() || !tokenItr->value.IsString())
    {
        TC_LOG_ERROR("server.paypal", "PayPal: OAuth token response lacked access_token.");
        return std::string();
    }

    int64 expiresIn = 3600;
    if (auto exp = doc.FindMember("expires_in"); exp != doc.MemberEnd() && exp->value.IsInt64())
        expiresIn = exp->value.GetInt64();

    _cachedToken = tokenItr->value.GetString();
    int64 lifetime = expiresIn - TOKEN_REFRESH_MARGIN_SEC;
    if (lifetime < 0)
        lifetime = 0;
    _tokenExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(lifetime);

    TC_LOG_DEBUG("server.paypal", "PayPal: obtained a new access token (valid ~{}s).", expiresIn);
    return _cachedToken;
}

std::string PayPalClient::BuildCreateOrderBody(uint32 accountId, uint64 productId,
    std::string const& currency, std::string const& value,
    std::string const& description,
    std::string const& returnUrl, std::string const& cancelUrl,
    std::string const& brandName)
{
    // custom_id == reference_id == "accountId:productId" (decimal) -> the webhook routing key.
    std::string correlation = Trinity::StringFormat("{}:{}", accountId, productId);

    return Trinity::StringFormat(
        R"({{"intent":"CAPTURE",)"
        R"("purchase_units":[{{)"
            R"("reference_id":{},)"
            R"("custom_id":{},)"
            R"("description":{},)"
            R"("amount":{{"currency_code":{},"value":{}}})"
        R"(}}],)"
        R"("payment_source":{{"paypal":{{"experience_context":{{)"
            R"("brand_name":{},)"
            R"("user_action":"PAY_NOW",)"
            R"("shipping_preference":"NO_SHIPPING",)"
            R"("return_url":{},)"
            R"("cancel_url":{})"
        R"(}}}}}})"
        R"(}})",
        JsonQuote(correlation), JsonQuote(correlation), JsonQuote(description),
        JsonQuote(currency), JsonQuote(value),
        JsonQuote(brandName), JsonQuote(returnUrl), JsonQuote(cancelUrl));
}

Optional<Order> PayPalClient::CreateOrder(uint32 accountId, uint64 productId,
    std::string const& currency, std::string const& value,
    std::string const& description,
    std::string const& returnUrl, std::string const& cancelUrl,
    std::string const& brandName)
{
    std::string token = GetAccessToken();
    if (token.empty())
        return std::nullopt;

    std::string body = BuildCreateOrderBody(accountId, productId, currency, value, description,
        returnUrl, cancelUrl, brandName);

    // Deterministic idempotency key: retries of the same purchase dedup server-side at PayPal.
    std::string idempotency = Trinity::StringFormat("tc-order-{}-{}", accountId, productId);

    HttpResult res = Perform(int(http::verb::post), "/v2/checkout/orders", body,
        "application/json", "Bearer " + token, idempotency);

    if (!res.transportOk)
    {
        TC_LOG_ERROR("server.paypal", "PayPal: CreateOrder could not reach {} (TLS/connect failed).", ApiHost());
        return std::nullopt;
    }
    if (res.status != 200 && res.status != 201)
    {
        TC_LOG_ERROR("server.paypal", "PayPal: CreateOrder returned HTTP {}.", res.status);
        return std::nullopt;
    }

    rapidjson::Document doc;
    doc.Parse(res.body.c_str(), res.body.length());
    if (doc.HasParseError() || !doc.IsObject())
        return std::nullopt;

    Order order;
    if (auto id = doc.FindMember("id"); id != doc.MemberEnd() && id->value.IsString())
        order.id = id->value.GetString();
    if (auto st = doc.FindMember("status"); st != doc.MemberEnd() && st->value.IsString())
        order.status = st->value.GetString();

    if (order.id.empty())
        return std::nullopt;

    // Pick the HATEOAS approve link: prefer rel == "payer-action", fall back to "approve".
    std::string approve;
    if (auto links = doc.FindMember("links"); links != doc.MemberEnd() && links->value.IsArray())
    {
        for (rapidjson::Value const& link : links->value.GetArray())
        {
            if (!link.IsObject())
                continue;
            auto rel = link.FindMember("rel");
            auto href = link.FindMember("href");
            if (rel == link.MemberEnd() || !rel->value.IsString() || href == link.MemberEnd() || !href->value.IsString())
                continue;

            std::string relStr = rel->value.GetString();
            if (relStr == "payer-action")
            {
                approve = href->value.GetString();
                break;
            }
            if (relStr == "approve" && approve.empty())
                approve = href->value.GetString();
        }
    }
    order.approveUrl = std::move(approve);

    TC_LOG_DEBUG("server.paypal", "PayPal: created order {} (status {}).", order.id, order.status);
    return order;
}

bool PayPalClient::CaptureOrder(std::string const& orderId, std::string* outCaptureId)
{
    if (orderId.empty())
        return false;

    std::string token = GetAccessToken();
    if (token.empty())
        return false;

    std::string target = Trinity::StringFormat("/v2/checkout/orders/{}/capture", orderId);
    std::string idempotency = Trinity::StringFormat("tc-capture-{}", orderId);

    HttpResult res = Perform(int(http::verb::post), target, "{}", "application/json",
        "Bearer " + token, idempotency);

    if (!res.transportOk || (res.status != 200 && res.status != 201))
    {
        TC_LOG_ERROR("server.paypal", "PayPal: CaptureOrder {} failed (transport {}, HTTP {}).",
            orderId, res.transportOk, res.status);
        return false;
    }

    rapidjson::Document doc;
    doc.Parse(res.body.c_str(), res.body.length());
    if (doc.HasParseError() || !doc.IsObject())
        return false;

    auto st = doc.FindMember("status");
    bool completed = st != doc.MemberEnd() && st->value.IsString() && std::string(st->value.GetString()) == "COMPLETED";

    if (completed && outCaptureId)
    {
        // purchase_units[0].payments.captures[0].id
        if (auto pu = doc.FindMember("purchase_units"); pu != doc.MemberEnd() && pu->value.IsArray() && !pu->value.Empty())
        {
            rapidjson::Value const& unit = pu->value[0];
            if (auto pay = unit.FindMember("payments"); pay != unit.MemberEnd() && pay->value.IsObject())
                if (auto caps = pay->value.FindMember("captures"); caps != pay->value.MemberEnd() && caps->value.IsArray() && !caps->value.Empty())
                    if (auto cid = caps->value[0].FindMember("id"); cid != caps->value[0].MemberEnd() && cid->value.IsString())
                        *outCaptureId = cid->value.GetString();
        }
    }

    return completed;
}

std::string PayPalClient::BuildVerifyBody(std::string const& transmissionId, std::string const& transmissionTime,
    std::string const& certUrl, std::string const& authAlgo, std::string const& transmissionSig,
    std::string const& webhookId, std::string const& rawEventBody)
{
    // webhook_event is the RAW event object inserted verbatim (NOT re-serialized / NOT quoted).
    return Trinity::StringFormat(
        R"({{"transmission_id":{},"transmission_time":{},"cert_url":{},"auth_algo":{},)"
        R"("transmission_sig":{},"webhook_id":{},"webhook_event":{}}})",
        JsonQuote(transmissionId), JsonQuote(transmissionTime), JsonQuote(certUrl),
        JsonQuote(authAlgo), JsonQuote(transmissionSig), JsonQuote(webhookId), rawEventBody);
}

bool PayPalClient::VerifyWebhookSignature(std::string const& transmissionId, std::string const& transmissionTime,
    std::string const& certUrl, std::string const& authAlgo,
    std::string const& transmissionSig, std::string const& rawBody)
{
    if (_cfg.webhookId.empty())
    {
        TC_LOG_ERROR("server.paypal", "PayPal: webhook verification refused - PayPal.WebhookId is not configured.");
        return false;
    }
    // Reject obviously incomplete deliveries before spending an API round-trip.
    if (transmissionId.empty() || transmissionSig.empty() || certUrl.empty() || rawBody.empty())
    {
        TC_LOG_WARN("server.paypal", "PayPal: webhook verification refused - missing transmission headers or body.");
        return false;
    }

    std::string token = GetAccessToken();
    if (token.empty())
        return false;

    std::string body = BuildVerifyBody(transmissionId, transmissionTime, certUrl, authAlgo,
        transmissionSig, _cfg.webhookId, rawBody);

    HttpResult res = Perform(int(http::verb::post), "/v1/notifications/verify-webhook-signature",
        body, "application/json", "Bearer " + token);

    if (!res.transportOk || res.status != 200)
    {
        TC_LOG_WARN("server.paypal", "PayPal: verify-webhook-signature call failed (transport {}, HTTP {}) - treating as NOT verified.",
            res.transportOk, res.status);
        return false;
    }

    rapidjson::Document doc;
    doc.Parse(res.body.c_str(), res.body.length());
    if (doc.HasParseError() || !doc.IsObject())
        return false;

    auto vs = doc.FindMember("verification_status");
    bool success = vs != doc.MemberEnd() && vs->value.IsString() && std::string(vs->value.GetString()) == "SUCCESS";
    if (!success)
        TC_LOG_WARN("server.paypal", "PayPal: webhook signature verification returned a non-SUCCESS status - rejecting.");
    return success;
}

HttpResult PayPalClient::Perform(int beastVerb, std::string const& target,
    std::string const& body, std::string const& contentType, std::string const& authHeader,
    std::string const& idempotencyKey)
{
    HttpResult result;
    char const* host = ApiHost();
    try
    {
        asio::io_context ioc;

        ssl::context ctx(ssl::context::tls_client);
        if (_cfg.verifyCertificate)
        {
            ctx.set_verify_mode(ssl::verify_peer);
            if (!_cfg.caBundleFile.empty())
                ctx.load_verify_file(_cfg.caBundleFile);
            else
                ctx.set_default_verify_paths();
        }
        else
            ctx.set_verify_mode(ssl::verify_none);

        tcp::resolver resolver(ioc);
        auto const endpoints = resolver.resolve(host, "443");

        ssl::stream<tcp::socket> stream(ioc, ctx);

        // SNI - required to route TLS correctly at PayPal's edge.
        if (!SSL_set_tlsext_host_name(stream.native_handle(), host))
            throw beast::system_error(beast::error_code(int(::ERR_get_error()), asio::error::get_ssl_category()));

        if (_cfg.verifyCertificate)
            stream.set_verify_callback(ssl::host_name_verification(host));

        asio::connect(stream.next_layer(), endpoints);
        stream.handshake(ssl::stream_base::client);

        http::request<http::string_body> req(static_cast<http::verb>(beastVerb), target, 11);
        req.set(http::field::host, host);
        req.set(http::field::user_agent, PAYPAL_USER_AGENT);
        req.set(http::field::accept, "application/json");
        if (!authHeader.empty())
            req.set(http::field::authorization, authHeader);
        if (!idempotencyKey.empty())
            req.set("PayPal-Request-Id", idempotencyKey);
        if (!body.empty())
        {
            req.set(http::field::content_type, contentType);
            req.body() = body;
            req.prepare_payload();
        }

        http::write(stream, req);

        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);

        result.status = res.result_int();
        result.body = res.body();
        result.transportOk = true;

        beast::error_code ec;
        stream.shutdown(ec);        // best-effort; server may close first, ignore truncation
    }
    catch (std::exception const& ex)
    {
        result.transportOk = false;
        // Log only the target + exception text (never body/auth), at DEBUG.
        TC_LOG_DEBUG("server.paypal", "PayPal: HTTPS request to '{}{}' threw: {}", host, target, ex.what());
    }
    return result;
}
}
