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

#ifndef TRINITYCORE_PAYPAL_REST_SERVICE_H
#define TRINITYCORE_PAYPAL_REST_SERVICE_H

#include "HttpService.h"
#include "LoginHttpSession.h"
#include <memory>

namespace Trinity::PayPal
{
    class PayPalClient;
}

// Inbound PayPal webhook receiver, hosted on bnetserver (the only process with a TLS HTTP listener).
//
// Mirrors Battlenet::LoginRESTService: a Trinity::Net::Http::HttpService with a single registered
// POST route. It reuses LoginHttpSession as the socket type (a webhook has no login session state,
// so no dedicated session class is needed). It runs inert unless PayPal.Enabled = 1 and is started
// from bnetserver Main.cpp.
//
// SECURITY: the handler verifies the PayPal signature (postback API) BEFORE any DB write. On
// anything other than verification SUCCESS it returns HTTP 400 and performs no state change.
class PayPalRESTService final : public Trinity::Net::Http::HttpService<Battlenet::LoginHttpSession>
{
public:
    using RequestHandlerResult = Trinity::Net::Http::RequestHandlerResult;
    using HttpRequestContext = Trinity::Net::Http::RequestContext;

    PayPalRESTService();
    ~PayPalRESTService();

    static PayPalRESTService& Instance();

    bool StartNetwork(Trinity::Asio::IoContext& ioContext, std::string const& bindIp, uint16 port, int32 threadCount = 1) override;

    // A webhook carries no login session state, but LoginHttpSession casts GetSessionState() to
    // LoginSessionState, so hand it the matching concrete state type.
    std::shared_ptr<Trinity::Net::Http::SessionState> CreateNewSessionState(boost::asio::ip::address const& address) override;

private:
    RequestHandlerResult HandleWebhook(std::shared_ptr<Battlenet::LoginHttpSession> session, HttpRequestContext& context);

    std::unique_ptr<Trinity::PayPal::PayPalClient> _client;
};

#define sPayPalRESTService PayPalRESTService::Instance()

#endif // TRINITYCORE_PAYPAL_REST_SERVICE_H
