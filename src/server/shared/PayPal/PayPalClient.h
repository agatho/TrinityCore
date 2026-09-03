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

#ifndef TRINITYCORE_PAYPAL_CLIENT_H
#define TRINITYCORE_PAYPAL_CLIENT_H

#include "Define.h"
#include "Optional.h"
#include <chrono>
#include <mutex>
#include <string>

// PayPal REST v2 transport client (shared).
//
// Lives in src/server/shared so BOTH worldserver (outbound create/capture) and bnetserver
// (inbound webhook signature verification) can link it. It contains NO game types.
//
// This is a synchronous, short-lived-connection HTTPS client modeled 1:1 on
// DiscordRestProvider::Perform (one io_context + ssl::stream<tcp::socket> per call, SNI via
// SSL_set_tlsext_host_name, ssl::host_name_verification, best-effort shutdown). Every public
// call BLOCKS (connect + TLS + read) and MUST therefore run OFF the world thread - callers
// drive it from a dedicated worker thread / the bnetserver io_context, never inline on a tick.
//
// SECURITY: this class must never log the ClientId, Secret, or the OAuth access token, and never
// log request/response bodies at INFO or above (they can carry tokens / payer data). Only opaque
// status codes and endpoint targets are logged, at DEBUG.
namespace Trinity::PayPal
{
    enum class Mode
    {
        Sandbox,
        Live
    };

    struct Config
    {
        Mode        mode = Mode::Sandbox;
        std::string clientId;
        std::string secret;
        std::string webhookId;
        std::string caBundleFile;           // optional; empty => default verify paths
        bool        verifyCertificate = true;
    };

    // Result of one raw HTTP round-trip.
    struct HttpResult
    {
        unsigned    status = 0;             // HTTP status code (0 => no transport)
        std::string body;
        bool        transportOk = false;    // false => connect/TLS/read failed
    };

    // A created PayPal order and the HATEOAS approve link handed to the client.
    struct Order
    {
        std::string id;
        std::string status;                 // CREATED | PAYER_ACTION_REQUIRED | ...
        std::string approveUrl;             // rel == "payer-action" (fallback "approve")
    };

    class TC_SHARED_API PayPalClient
    {
    public:
        explicit PayPalClient(Config cfg);
        ~PayPalClient();

        PayPalClient(PayPalClient const&) = delete;
        PayPalClient& operator=(PayPalClient const&) = delete;

        // OAuth2 client_credentials. Caches the bearer token until (expiry - 60s). Thread-safe
        // (internal mutex). Returns "" on failure (transport or non-200). BLOCKS.
        std::string GetAccessToken();

        // POST /v2/checkout/orders (intent CAPTURE). custom_id == reference_id == "accountId:productId".
        // Returns Order{id,status,approveUrl}; empty optional on failure. BLOCKS.
        Optional<Order> CreateOrder(uint32 accountId, uint64 productId,
            std::string const& currency, std::string const& value,
            std::string const& description,
            std::string const& returnUrl, std::string const& cancelUrl,
            std::string const& brandName);

        // POST /v2/checkout/orders/{id}/capture. Returns true iff the order status == COMPLETED.
        // Optionally reports the capture id. BLOCKS.
        bool CaptureOrder(std::string const& orderId, std::string* outCaptureId = nullptr);

        // POST /v1/notifications/verify-webhook-signature (postback API - no local crypto).
        // Feed the 5 PAYPAL-* header values verbatim + the RAW, UNMODIFIED request body.
        // Returns true ONLY when the API answers {"verification_status":"SUCCESS"}. BLOCKS.
        bool VerifyWebhookSignature(std::string const& transmissionId, std::string const& transmissionTime,
            std::string const& certUrl, std::string const& authAlgo,
            std::string const& transmissionSig, std::string const& rawBody);

        char const* ApiHost() const;        // "api-m.sandbox.paypal.com" | "api-m.paypal.com"

        // --- exposed for offline unit testing of request-body shape (no network) ---
        // Builds the create-order JSON body exactly as CreateOrder sends it.
        static std::string BuildCreateOrderBody(uint32 accountId, uint64 productId,
            std::string const& currency, std::string const& value,
            std::string const& description,
            std::string const& returnUrl, std::string const& cancelUrl,
            std::string const& brandName);

        // Builds the verify-webhook-signature JSON body exactly as VerifyWebhookSignature sends it.
        static std::string BuildVerifyBody(std::string const& transmissionId, std::string const& transmissionTime,
            std::string const& certUrl, std::string const& authAlgo, std::string const& transmissionSig,
            std::string const& webhookId, std::string const& rawEventBody);

    private:
        // == DiscordRestProvider::Perform, generalized: verb/target/body/content-type/auth header.
        // Empty authHeader => no Authorization header sent. A non-empty idempotencyKey is sent as the
        // PayPal-Request-Id header so PayPal dedups retries of the same logical request.
        HttpResult Perform(int beastVerb, std::string const& target,
            std::string const& body, std::string const& contentType,
            std::string const& authHeader, std::string const& idempotencyKey = std::string());

        Config _cfg;
        std::mutex _tokenMutex;
        std::string _cachedToken;
        std::chrono::steady_clock::time_point _tokenExpiry{};
    };
}

#endif // TRINITYCORE_PAYPAL_CLIENT_H
