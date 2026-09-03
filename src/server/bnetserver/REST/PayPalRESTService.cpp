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

#include "PayPalRESTService.h"
#include "Configuration/Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "PayPalClient.h"
#include <rapidjson/document.h>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/verb.hpp>
#include <charconv>
#include <string>

using namespace std::string_view_literals;

namespace
{
    namespace http = boost::beast::http;

    // Parse "accountId:productId" (both decimal, non-negative). Returns false unless BOTH parse
    // cleanly and consume their whole field - this is a security gate (custom_id routing key).
    bool ParseCustomId(std::string const& customId, uint32& accountId, uint64& productId)
    {
        std::size_t colon = customId.find(':');
        if (colon == std::string::npos || colon == 0 || colon + 1 >= customId.size())
            return false;

        std::string_view accPart(customId.data(), colon);
        std::string_view prodPart(customId.data() + colon + 1, customId.size() - colon - 1);

        uint32 acc = 0;
        uint64 prod = 0;
        auto a = std::from_chars(accPart.data(), accPart.data() + accPart.size(), acc);
        if (a.ec != std::errc() || a.ptr != accPart.data() + accPart.size())
            return false;
        auto p = std::from_chars(prodPart.data(), prodPart.data() + prodPart.size(), prod);
        if (p.ec != std::errc() || p.ptr != prodPart.data() + prodPart.size())
            return false;
        if (!acc || !prod)
            return false;

        accountId = acc;
        productId = prod;
        return true;
    }

    std::string GetString(rapidjson::Value const& obj, char const* key)
    {
        auto itr = obj.FindMember(key);
        if (itr != obj.MemberEnd() && itr->value.IsString())
            return itr->value.GetString();
        return std::string();
    }
}

PayPalRESTService::PayPalRESTService() : HttpService("paypal")
{
}

PayPalRESTService::~PayPalRESTService() = default;

PayPalRESTService& PayPalRESTService::Instance()
{
    static PayPalRESTService instance;
    return instance;
}

std::shared_ptr<Trinity::Net::Http::SessionState> PayPalRESTService::CreateNewSessionState(boost::asio::ip::address const& address)
{
    std::shared_ptr<Trinity::Net::Http::SessionState> state = std::make_shared<Battlenet::LoginSessionState>();
    InitAndStoreSessionState(state, address);
    return state;
}

bool PayPalRESTService::StartNetwork(Trinity::Asio::IoContext& ioContext, std::string const& bindIp, uint16 port, int32 threadCount)
{
    Trinity::PayPal::Config cfg;
    std::string modeStr = sConfigMgr->GetStringDefault("PayPal.Mode", "sandbox");
    cfg.mode = (modeStr == "live") ? Trinity::PayPal::Mode::Live : Trinity::PayPal::Mode::Sandbox;
    cfg.clientId = sConfigMgr->GetStringDefault("PayPal.ClientId", "");
    cfg.secret = sConfigMgr->GetStringDefault("PayPal.Secret", "");
    cfg.webhookId = sConfigMgr->GetStringDefault("PayPal.WebhookId", "");
    cfg.caBundleFile = sConfigMgr->GetStringDefault("PayPal.CaBundle", "");
    cfg.verifyCertificate = sConfigMgr->GetBoolDefault("PayPal.VerifyCertificate", true);

    if (cfg.webhookId.empty() || cfg.clientId.empty() || cfg.secret.empty())
    {
        TC_LOG_ERROR("server.http.paypal", "PayPal webhook listener not started: PayPal.WebhookId / ClientId / Secret must be set "
            "(they are required to verify webhook signatures). Listener remains OFF.");
        return false;
    }

    _client = std::make_unique<Trinity::PayPal::PayPalClient>(std::move(cfg));

    if (!HttpService::StartNetwork(ioContext, bindIp, port, threadCount))
        return false;

    using Trinity::Net::Http::RequestHandlerFlag;

    RegisterHandler(http::verb::post, "/paypal/webhook"sv,
        [this](std::shared_ptr<Battlenet::LoginHttpSession> session, HttpRequestContext& context)
        {
            return HandleWebhook(std::move(session), context);
        }, RequestHandlerFlag::DoNotLogRequestContent | RequestHandlerFlag::DoNotLogResponseContent);

    TC_LOG_INFO("server.http.paypal", "PayPal webhook listener started on {}:{} (POST /paypal/webhook).", bindIp, port);
    return true;
}

PayPalRESTService::RequestHandlerResult PayPalRESTService::HandleWebhook(std::shared_ptr<Battlenet::LoginHttpSession> /*session*/, HttpRequestContext& context)
{
    auto const& req = context.request;

    // RAW body bytes - fed verbatim to the verify call; re-serializing would break the signature.
    std::string rawBody = req.body();

    auto hdr = [&](char const* key) -> std::string
    {
        auto it = req.find(key);
        return it != req.end() ? std::string(it->value().data(), it->value().size()) : std::string();
    };

    // SECURITY GATE #1: verify BEFORE any parsing that drives state, and before any DB write.
    bool verified = _client && _client->VerifyWebhookSignature(
        hdr("paypal-transmission-id"), hdr("paypal-transmission-time"),
        hdr("paypal-cert-url"), hdr("paypal-auth-algo"), hdr("paypal-transmission-sig"), rawBody);

    if (!verified)
    {
        TC_LOG_WARN("server.http.paypal", "PayPal webhook REJECTED (signature not verified). No state change performed.");
        context.response.result(http::status::bad_request);
        context.response.set(http::field::content_type, "application/json");
        context.response.body() = R"({"ok":false,"error":"verification_failed"})";
        context.response.prepare_payload();
        return RequestHandlerResult::Handled;
    }

    rapidjson::Document doc;
    doc.Parse(rawBody.c_str(), rawBody.size());
    if (doc.HasParseError() || !doc.IsObject())
    {
        // Verified but unparseable: acknowledge so PayPal does not retry forever; nothing to do.
        TC_LOG_ERROR("server.http.paypal", "PayPal webhook verified but body was not valid JSON.");
        context.response.result(http::status::ok);
        context.response.set(http::field::content_type, "application/json");
        context.response.body() = R"({"ok":true})";
        context.response.prepare_payload();
        return RequestHandlerResult::Handled;
    }

    std::string eventType = GetString(doc, "event_type");
    auto resItr = doc.FindMember("resource");
    rapidjson::Value const* resource = (resItr != doc.MemberEnd() && resItr->value.IsObject()) ? &resItr->value : nullptr;

    if (eventType == "CHECKOUT.ORDER.APPROVED" && resource)
    {
        // Payer approved; money not yet moved. Trigger the capture so PAYMENT.CAPTURE.COMPLETED fires
        // (that event, which carries custom_id, is the authoritative grant trigger). No DB write here.
        std::string orderId = GetString(*resource, "id");
        if (!orderId.empty() && _client)
        {
            if (_client->CaptureOrder(orderId))
                TC_LOG_DEBUG("server.http.paypal", "PayPal: auto-captured approved order {}.", orderId);
            else
                TC_LOG_WARN("server.http.paypal", "PayPal: auto-capture of approved order {} did not complete.", orderId);
        }
    }
    else if ((eventType == "PAYMENT.CAPTURE.COMPLETED" || eventType == "PAYMENT.CAPTURE.DENIED"
              || eventType == "PAYMENT.CAPTURE.REFUNDED") && resource)
    {
        std::string customId = GetString(*resource, "custom_id");
        std::string captureId = GetString(*resource, "id");

        uint32 accountId = 0;
        uint64 productId = 0;
        if (!ParseCustomId(customId, accountId, productId))
        {
            TC_LOG_ERROR("server.http.paypal", "PayPal webhook {}: custom_id missing or malformed - cannot route grant.", eventType);
        }
        else
        {
            std::string newStatus = "SETTLED";
            if (eventType == "PAYMENT.CAPTURE.DENIED")
                newStatus = "DENIED";
            else if (eventType == "PAYMENT.CAPTURE.REFUNDED")
                newStatus = "REFUNDED";

            // status-source guard (CREATED/APPROVED only) lives in the prepared statement: a duplicate
            // COMPLETED cannot re-arm an already-DELIVERED row, so webhook retries cannot double-grant.
            LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_PAYPAL_SETTLEMENT_STATUS);
            stmt->setString(0, newStatus);
            stmt->setString(1, captureId);
            stmt->setUInt32(2, accountId);
            stmt->setUInt64(3, productId);
            LoginDatabase.Execute(stmt);

            TC_LOG_INFO("server.http.paypal", "PayPal webhook {}: account {} product {} -> {}.", eventType, accountId, productId, newStatus);
        }
    }
    else
    {
        TC_LOG_DEBUG("server.http.paypal", "PayPal webhook: ignoring event_type '{}'.", eventType);
    }

    // Verified + accepted: ALWAYS 200 (PayPal retries any non-2xx).
    context.response.result(http::status::ok);
    context.response.set(http::field::content_type, "application/json");
    context.response.body() = R"({"ok":true})";
    context.response.prepare_payload();
    return RequestHandlerResult::Handled;
}
