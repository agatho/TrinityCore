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

#ifndef TRINITYCORE_PAYMENT_MGR_H
#define TRINITYCORE_PAYMENT_MGR_H

#include "Define.h"
#include "ObjectGuid.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>

namespace Trinity::PayPal
{
    class PayPalClient;
}

// Worldserver-side brain for Commerce Rail A (real money -> web / PayPal).
//
// Lifecycle mirrors DiscordBridge: World::LoadConfigSettings calls LoadConfig(), World::Update calls
// Update(diff). When PayPal.Enabled = 0 this is a complete no-op - no worker thread is spawned, no
// PayPalClient is constructed, nothing calls out.
//
// It never blocks the world thread: outbound create-order calls run on a dedicated worker thread
// (mirroring DiscordRestProvider::WorkerLoop). The world thread only enqueues jobs and drains
// results (approve URLs) + polls the settlement table.
class TC_GAME_API PaymentMgr
{
public:
    static PaymentMgr* instance();

    PaymentMgr(PaymentMgr const&) = delete;
    PaymentMgr& operator=(PaymentMgr const&) = delete;

    void LoadConfig();          // World::LoadConfigSettings  (like sDiscordBridge->LoadConfig())
    void Update(uint32 diff);   // World::Update              (like sDiscordBridge->Update())

    bool IsEnabled() const { return _enabled; }

    // Called from the Rail-A (CurrencyTypesID == 0) Web branch of HandleBattlePayOpenCheckout.
    // Non-blocking: enqueues an order-create job for the worker thread. The approve URL is delivered
    // to the player's session later, from the world thread, by PumpApproveUrls().
    void BeginRealMoneyCheckout(uint32 accountId, ObjectGuid playerGuid,
        uint64 productId, uint32 clientToken);

private:
    PaymentMgr();
    ~PaymentMgr();

    void StartWorker();
    void StopWorker();

    void WorkerLoop();          // dedicated thread: drains _createQueue -> PayPalClient::CreateOrder
    void PumpApproveUrls();     // world thread: hand approve URLs to sessions
    void PumpSettlements();     // world thread: poll paypal_settlement (SETTLED) -> grant

    // Resolve the price + description for a product from the real BattlePay catalog: a routed real-money
    // product is charged its shop DisplayPrice (/100000); an unrouted/QA product falls back to the
    // PayPal.TestProductPrice config so a sandbox order can still be driven, and a product with neither is
    // refused rather than charged a guessed amount.
    bool ResolveProduct(uint64 productId, std::string& outValue, std::string& outDescription) const;

    // Grant a settled product to an account by creating a real account-level BattlePay entitlement
    // (sBattlePayMgr->CreateEntitlement) that the buyer claims on next login - offline-safe, since a
    // webhook can settle while the buyer is offline. Returns true only if the entitlement was actually
    // created; a false return leaves the row SETTLED so it is retried later, never flipping it to
    // DELIVERED without an actual grant.
    bool DeliverSettledProduct(uint32 accountId, uint64 productId, std::string const& orderId);

    bool _enabled = false;
    std::string _returnUrl, _cancelUrl, _brandName, _currency, _testPrice;
    uint32 _settlementPollMs = 2000;
    std::unique_ptr<Trinity::PayPal::PayPalClient> _client;

    // world <-> worker handoff (mutex + deque, exactly like DiscordRestProvider's _outbound).
    struct CreateJob
    {
        uint32 account = 0;
        ObjectGuid player;
        uint64 productId = 0;
        uint32 clientToken = 0;
        std::string value;
        std::string description;
    };
    struct ApproveResult
    {
        ObjectGuid player;
        uint64 productId = 0;
        uint32 clientToken = 0;
        std::string approveUrl;
        std::string orderId;
    };

    std::deque<CreateJob>     _createQueue;
    std::mutex                _createMutex;
    std::condition_variable   _wake;

    std::deque<ApproveResult> _approveQueue;
    std::mutex                _approveMutex;

    std::thread               _worker;
    std::atomic<bool>         _stop{ false };

    uint32 _settlementPollTimer = 0;

    // World-thread-only: orderIds already granted / already deferred this session, so a fast poll
    // cadence cannot re-select and re-grant a row before the async DELIVERED flip commits. Combined
    // with the DB-side "WHERE status = 'SETTLED'" guard this makes delivery idempotent across the
    // process lifetime and across restarts.
    std::unordered_set<std::string> _deliveredThisSession;
    std::unordered_set<std::string> _deferredThisSession;
};

#define sPaymentMgr PaymentMgr::instance()

#endif // TRINITYCORE_PAYMENT_MGR_H
