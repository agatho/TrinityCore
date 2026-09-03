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

#include "PaymentMgr.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "PayPalClient.h"
#include "StringFormat.h"
#include <algorithm>

PaymentMgr::PaymentMgr() = default;

PaymentMgr::~PaymentMgr()
{
    StopWorker();
}

PaymentMgr* PaymentMgr::instance()
{
    static PaymentMgr instance;
    return &instance;
}

void PaymentMgr::LoadConfig()
{
    // Tear down any live worker/client first so a reload starts from a clean state (also drains the
    // worker thread). This is safe: LoadConfig runs on the world thread.
    StopWorker();
    _client.reset();

    _enabled = sConfigMgr->GetBoolDefault("PayPal.Enabled", false);
    if (!_enabled)
    {
        TC_LOG_INFO("server.loading", "PayPal payment provider disabled (PayPal.Enabled = 0).");
        return;
    }

    std::string modeStr = sConfigMgr->GetStringDefault("PayPal.Mode", "sandbox");
    std::transform(modeStr.begin(), modeStr.end(), modeStr.begin(), [](unsigned char c) { return char(std::tolower(c)); });

    Trinity::PayPal::Config cfg;
    cfg.mode = (modeStr == "live") ? Trinity::PayPal::Mode::Live : Trinity::PayPal::Mode::Sandbox;
    cfg.clientId = sConfigMgr->GetStringDefault("PayPal.ClientId", "");
    cfg.secret = sConfigMgr->GetStringDefault("PayPal.Secret", "");
    cfg.webhookId = sConfigMgr->GetStringDefault("PayPal.WebhookId", "");
    cfg.caBundleFile = sConfigMgr->GetStringDefault("PayPal.CaBundle", "");
    cfg.verifyCertificate = sConfigMgr->GetBoolDefault("PayPal.VerifyCertificate", true);

    _currency = sConfigMgr->GetStringDefault("PayPal.Currency", "USD");
    _brandName = sConfigMgr->GetStringDefault("PayPal.BrandName", "My WoW Realm Store");
    _returnUrl = sConfigMgr->GetStringDefault("PayPal.ReturnUrl", "");
    _cancelUrl = sConfigMgr->GetStringDefault("PayPal.CancelUrl", "");
    _testPrice = sConfigMgr->GetStringDefault("PayPal.TestProductPrice", "");
    _settlementPollMs = sConfigMgr->GetIntDefault("PayPal.SettlementPollMs", 2000);
    if (_settlementPollMs < 250)
        _settlementPollMs = 250;

    _client = std::make_unique<Trinity::PayPal::PayPalClient>(std::move(cfg));

    // NEVER log the ClientId / Secret. Report only whether creds are present.
    bool haveCreds = !sConfigMgr->GetStringDefault("PayPal.ClientId", "").empty();
    TC_LOG_INFO("server.loading", "PayPal payment provider enabled (mode: {}, host: {}, credentials: {}). "
        "Outbound order-create runs on a dedicated worker thread; settlement poll every {} ms.",
        modeStr, _client->ApiHost(), haveCreds ? "present" : "MISSING (outbound calls will fail)", _settlementPollMs);

    StartWorker();
}

void PaymentMgr::StartWorker()
{
    _stop.store(false, std::memory_order_relaxed);
    _worker = std::thread(&PaymentMgr::WorkerLoop, this);
}

void PaymentMgr::StopWorker()
{
    _stop.store(true, std::memory_order_relaxed);
    _wake.notify_all();
    if (_worker.joinable())
        _worker.join();

    // Drop any pending jobs/results so a fresh config does not replay stale ones.
    {
        std::lock_guard<std::mutex> lock(_createMutex);
        _createQueue.clear();
    }
    {
        std::lock_guard<std::mutex> lock(_approveMutex);
        _approveQueue.clear();
    }
}

void PaymentMgr::Update(uint32 diff)
{
    if (!_enabled)
        return;

    PumpApproveUrls();

    _settlementPollTimer += diff;
    if (_settlementPollTimer >= _settlementPollMs)
    {
        _settlementPollTimer = 0;
        PumpSettlements();
    }
}

void PaymentMgr::BeginRealMoneyCheckout(uint32 accountId, ObjectGuid playerGuid, uint64 productId, uint32 clientToken)
{
    if (!_enabled || !_client)
    {
        TC_LOG_DEBUG("server.paypal", "PayPal: BeginRealMoneyCheckout ignored - provider disabled.");
        return;
    }

    // Validate the routing key components before they ever reach the wire / DB.
    if (!accountId || !productId)
    {
        TC_LOG_ERROR("server.paypal", "PayPal: BeginRealMoneyCheckout rejected - invalid account ({}) or product ({}).", accountId, productId);
        return;
    }

    std::string value, description;
    if (!ResolveProduct(productId, value, description))
    {
        TC_LOG_WARN("server.paypal", "PayPal: cannot begin checkout for product {} - price unresolved "
            "(set PayPal.TestProductPrice for QA, or wait for the P2 commerce catalog).", productId);
        return;
    }

    CreateJob job;
    job.account = accountId;
    job.player = playerGuid;
    job.productId = productId;
    job.clientToken = clientToken;
    job.value = std::move(value);
    job.description = std::move(description);

    {
        std::lock_guard<std::mutex> lock(_createMutex);
        _createQueue.push_back(std::move(job));
    }
    _wake.notify_all();
}

bool PaymentMgr::ResolveProduct(uint64 /*productId*/, std::string& outValue, std::string& outDescription) const
{
    // TODO(P2): resolve real price + localized description via sBattlePayMgr->GetProduct(productId)
    // once the commerce checkout rail is forward-ported onto integration. Until then, a configured
    // test price lets QA drive a real sandbox order without the catalog.
    if (_testPrice.empty())
        return false;

    outValue = _testPrice;
    outDescription = "Realm Store purchase";
    return true;
}

void PaymentMgr::WorkerLoop()
{
    while (!_stop.load(std::memory_order_relaxed))
    {
        CreateJob job;
        {
            std::unique_lock<std::mutex> lock(_createMutex);
            _wake.wait(lock, [this] { return _stop.load(std::memory_order_relaxed) || !_createQueue.empty(); });
            if (_stop.load(std::memory_order_relaxed))
                return;
            job = std::move(_createQueue.front());
            _createQueue.pop_front();
        }

        // BLOCKING PayPal call - this is why it runs here and never on the world thread.
        Optional<Trinity::PayPal::Order> order = _client->CreateOrder(job.account, job.productId,
            _currency, job.value, job.description, _returnUrl, _cancelUrl, _brandName);

        if (!order || order->id.empty() || order->approveUrl.empty())
        {
            TC_LOG_ERROR("server.paypal", "PayPal: order creation failed for account {} product {}.", job.account, job.productId);
            continue;
        }

        // Persist the CREATED row so the webhook (bnetserver) can later flip it to SETTLED, and the
        // settlement poll can deliver it. amount/currency recorded for audit + future revoke.
        LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_INS_PAYPAL_SETTLEMENT);
        stmt->setString(0, order->id);
        stmt->setUInt32(1, job.account);
        stmt->setUInt64(2, job.productId);
        stmt->setString(3, job.value);
        stmt->setString(4, _currency);
        LoginDatabase.Execute(stmt);

        ApproveResult res;
        res.player = job.player;
        res.productId = job.productId;
        res.clientToken = job.clientToken;
        res.approveUrl = order->approveUrl;
        res.orderId = order->id;
        {
            std::lock_guard<std::mutex> lock(_approveMutex);
            _approveQueue.push_back(std::move(res));
        }

        TC_LOG_DEBUG("server.paypal", "PayPal: order {} created for account {} product {}.", order->id, job.account, job.productId);
    }
}

void PaymentMgr::PumpApproveUrls()
{
    std::deque<ApproveResult> pending;
    {
        std::lock_guard<std::mutex> lock(_approveMutex);
        if (_approveQueue.empty())
            return;
        pending.swap(_approveQueue);
    }

    for (ApproveResult const& res : pending)
    {
        // TODO(P2): deliver the approve URL to the player's session via the Rail-A web-URL SMSG
        // (SMSG_MIRROR_VARS host / the StoreUI SMSG). Until the commerce SMSG lands, log it so a
        // headless/QA test can open it manually. The URL is not a secret (PayPal shows it to the
        // buyer anyway), so logging it is acceptable; the token, creds and payer data are NOT logged.
        TC_LOG_INFO("server.paypal", "PayPal: approve URL ready for player {} (product {}, order {}): {}",
            res.player.ToString(), res.productId, res.orderId, res.approveUrl);
    }
}

void PaymentMgr::PumpSettlements()
{
    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_PAYPAL_SETTLED);
    stmt->setUInt32(0, 50);     // batch cap per poll
    PreparedQueryResult result = LoginDatabase.Query(stmt);
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        std::string orderId = fields[0].GetString();
        uint32 accountId = fields[2].GetUInt32();
        uint64 productId = fields[3].GetUInt64();

        // World-thread-only guards: never re-process a row we already handled this session.
        if (_deliveredThisSession.contains(orderId) || _deferredThisSession.contains(orderId))
            continue;

        if (DeliverSettledProduct(accountId, productId, orderId))
        {
            _deliveredThisSession.insert(orderId);

            // Idempotent claim: only a row still SETTLED is flipped to DELIVERED, so a duplicate
            // webhook / a racing poll can never grant twice.
            LoginDatabasePreparedStatement* upd = LoginDatabase.GetPreparedStatement(LOGIN_UPD_PAYPAL_DELIVERED);
            upd->setString(0, orderId);
            LoginDatabase.Execute(upd);

            TC_LOG_INFO("server.paypal", "PayPal: delivered product {} to account {} (order {}).", productId, accountId, orderId);
        }
        else
        {
            _deferredThisSession.insert(orderId);
            TC_LOG_WARN("server.paypal", "PayPal: order {} settled for account {} product {} but the delivery "
                "pipeline (P2) is not present - leaving SETTLED for a later grant.", orderId, accountId, productId);
        }
    } while (result->NextRow());
}

bool PaymentMgr::DeliverSettledProduct(uint32 /*accountId*/, uint64 /*productId*/, std::string const& /*orderId*/)
{
    // TODO(P2): call the real grant path used by the Shop.RealMoney.Mode = web branch of
    // HandleBattlePayOpenCheckout -> DeliveryPipeline -> SMSG_BATTLE_PAY_DELIVERY_ENDED +
    // DISTRIBUTION_UPDATE, ideally claiming (UPD ... WHERE status='SETTLED') and granting inside a
    // single DB transaction. Returning false here (delivery not yet possible on this branch) keeps
    // the settled row intact so no money-backed purchase is silently dropped.
    return false;
}
