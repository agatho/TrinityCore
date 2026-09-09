// CraftOrderBoard - see header for the design + the escrow invariant.

#include "CraftOrderBoard.h"

#include "../Services.h"
#include "../Bot/BotRegistry.h"

#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Mail.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "Log.h"

#include <atomic>

namespace Playerbot::V2 {

namespace {

// World-process id allocator. Seeded at LoadFromDb from MAX(id) so the ids we
// mint are monotonic across the whole table and never collide with existing
// rows. World-thread-only writers; atomic only to be safe against a stray
// diagnostic read. We allocate the id ourselves (rather than relying on a
// LAST_INSERT_ID round-trip) so PostOrder stays a single synchronous step with
// no read-back race — the escrow debit and the row write happen together.
std::atomic<uint64> g_next_order_id{1};

// Convert GameTimeMS deltas safely (handles the documented ms-counter wrap by
// treating a backwards delta as "just now").
uint32 AgeMs(uint32 now_ms, uint32 then_ms)
{
    return (then_ms == 0 || now_ms < then_ms) ? 0u : (now_ms - then_ms);
}

} // anonymous

bool CraftOrderBoard::IsFleetBot(uint64 guid_low)
{
    if (guid_low == 0) return false;
    if (!Services::Initialized()) return false;
    // Registry::has is the authoritative "this guid is a registered fleet bot"
    // check — the human-firewall. A real player is never in the registry.
    return Services::Registry().has(static_cast<BotId>(guid_low));
}

bool CraftOrderBoard::PayBot(uint64 bot_low, uint64 copper, char const* reason)
{
    if (bot_low == 0) return false;
    if (copper == 0) return true;   // nothing to move — treat as a no-op success

    ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(bot_low);
    if (Player* p = ObjectAccessor::FindConnectedPlayer(guid))
    {
        p->ModifyMoney(static_cast<int64>(copper));
        TC_LOG_INFO("playerbot.v2",
            "[CraftOrderBoard] {} {} copper -> bot_low={} (direct)",
            reason, copper, bot_low);
        return true;
    }

    // Bot is offline: deliver the value via mail so it still moves exactly once.
    // SendMailTo with money + no items mirrors how the AH refunds an offline
    // bidder. The sender is the system (no MailSender Player needed for a
    // money-only mail from a neutral source).
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    MailDraft("Craft Order", "Craft order settlement.")
        .AddMoney(copper)
        .SendMailTo(trans, MailReceiver(bot_low), MailSender(MAIL_NORMAL, 0),
                    MAIL_CHECK_MASK_NONE);
    CharacterDatabase.CommitTransaction(trans);
    TC_LOG_INFO("playerbot.v2",
        "[CraftOrderBoard] {} {} copper -> bot_low={} (mailed, offline)",
        reason, copper, bot_low);
    return true;
}

void CraftOrderBoard::DbInsertReturningId(CraftOrder& o)
{
    // claimed_at left NULL on insert (always Open at post time).
    CharacterDatabase.DirectPExecute(
        "INSERT INTO bot_craft_orders "
        "(id, requester_low, crafter_low, spell_id, item_entry, quantity, "
        " payment_copper, status, created_at, claimed_at) "
        "VALUES ({}, {}, NULL, {}, {}, {}, {}, {}, CURRENT_TIMESTAMP, NULL)",
        o.id, o.requester_low, o.spell_id, o.item_entry, o.quantity,
        o.payment_copper, static_cast<uint32>(o.status));
}

void CraftOrderBoard::DbUpdateClaim(uint64 id, uint64 crafter_low, uint32 /*claimed_unix*/)
{
    CharacterDatabase.DirectPExecute(
        "UPDATE bot_craft_orders SET crafter_low = {}, status = {}, "
        "claimed_at = CURRENT_TIMESTAMP WHERE id = {}",
        crafter_low, static_cast<uint32>(CraftOrderStatus::Claimed), id);
}

void CraftOrderBoard::DbUpdateStatus(uint64 id, CraftOrderStatus status)
{
    CharacterDatabase.DirectPExecute(
        "UPDATE bot_craft_orders SET status = {} WHERE id = {}",
        static_cast<uint32>(status), id);
}

void CraftOrderBoard::DbDelete(uint64 id)
{
    CharacterDatabase.DirectPExecute(
        "DELETE FROM bot_craft_orders WHERE id = {}", id);
}

void CraftOrderBoard::LoadFromDb()
{
    std::lock_guard<std::mutex> g(mtx_);
    orders_.clear();
    settled_ms_.clear();

    uint64 max_id = 0;
    const uint32 now_ms = GameTime::GetGameTimeMS();

    // Reconcile Open (0) + Claimed (1) rows into memory; terminal rows stay in
    // the DB for the Tick pruner. We also need MAX(id) over ALL rows so minted
    // ids never collide with a still-present terminal row.
    if (QueryResult maxres = CharacterDatabase.Query(
            "SELECT MAX(id) FROM bot_craft_orders"))
    {
        Field* f = maxres->Fetch();
        if (!f[0].IsNull())
            max_id = f[0].GetUInt64();
    }

    if (QueryResult res = CharacterDatabase.Query(
            "SELECT id, requester_low, crafter_low, spell_id, item_entry, "
            "quantity, payment_copper, status FROM bot_craft_orders "
            "WHERE status IN (0, 1)"))
    {
        do
        {
            Field* f = res->Fetch();
            CraftOrder o;
            o.id             = f[0].GetUInt64();
            o.requester_low  = f[1].GetUInt64();
            o.crafter_low    = f[2].IsNull() ? 0 : f[2].GetUInt64();
            o.spell_id       = f[3].GetUInt32();
            o.item_entry     = f[4].GetUInt32();
            o.quantity       = f[5].GetUInt32();
            o.payment_copper = f[6].GetUInt64();
            o.status         = static_cast<CraftOrderStatus>(f[7].GetUInt8());
            // We don't persist the ms timestamps (they're process-relative);
            // re-stamp on load so the claim-timeout clock restarts cleanly.
            o.created_ms     = now_ms;
            o.claimed_ms     = (o.status == CraftOrderStatus::Claimed) ? now_ms : 0;

            // NOTE: the human-firewall is NOT applied here. At module init the
            // bot registry is empty (bots log in afterwards), so an IsFleetBot
            // check at load would falsely fail every order. The firewall is
            // enforced at the live transition points instead — ClaimOpenOrder /
            // MarkDelivered re-verify the counterparties, and a Claimed order
            // whose crafter never comes back online ages out via the Tick
            // timeout (Fail + refund to requester). The escrow stays held in
            // the meantime, so no gold is lost or double-moved across a restart.
            orders_.emplace(o.id, o);
        } while (res->NextRow());
    }

    g_next_order_id.store(max_id + 1, std::memory_order_relaxed);

    TC_LOG_INFO("playerbot.v2",
        "[CraftOrderBoard] Loaded {} active order(s); next_id={}",
        orders_.size(), g_next_order_id.load(std::memory_order_relaxed));
}

uint64 CraftOrderBoard::PostOrder(uint64 requester_low, uint32 spell_id,
                                  uint32 item_entry, uint32 quantity,
                                  uint64 payment_copper)
{
    if (quantity == 0) quantity = 1;
    // Human-firewall: only a current fleet bot may post.
    if (!IsFleetBot(requester_low))
        return 0;
    if (spell_id == 0 || item_entry == 0)
        return 0;

    ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(requester_low);
    Player* requester = ObjectAccessor::FindConnectedPlayer(guid);
    if (!requester || !requester->IsInWorld())
        return 0;

    // ESCROW DEBIT (exactly once). If the requester can't afford the payment,
    // no order is created and no gold moves.
    if (payment_copper > 0)
    {
        if (!requester->HasEnoughMoney(payment_copper))
            return 0;
        requester->ModifyMoney(-static_cast<int64>(payment_copper));
    }

    CraftOrder o;
    o.id             = g_next_order_id.fetch_add(1, std::memory_order_relaxed);
    o.requester_low  = requester_low;
    o.crafter_low    = 0;
    o.spell_id       = spell_id;
    o.item_entry     = item_entry;
    o.quantity       = quantity;
    o.payment_copper = payment_copper;
    o.status         = CraftOrderStatus::Open;
    o.created_ms     = GameTime::GetGameTimeMS();
    o.claimed_ms     = 0;

    DbInsertReturningId(o);

    {
        std::lock_guard<std::mutex> g2(mtx_);
        orders_.emplace(o.id, o);
    }

    TC_LOG_INFO("playerbot.v2",
        "[CraftOrderBoard] PostOrder id={} requester_low={} spell={} item={} qty={} pay={} (escrow debited)",
        o.id, requester_low, spell_id, item_entry, quantity, payment_copper);
    return o.id;
}

CraftOrder CraftOrderBoard::ClaimOpenOrder(uint64 crafter_low)
{
    CraftOrder none;   // id == 0 == "nothing claimed"
    // Human-firewall: only a current fleet bot may claim.
    if (!IsFleetBot(crafter_low))
        return none;

    ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(crafter_low);
    Player* crafter = ObjectAccessor::FindConnectedPlayer(guid);
    if (!crafter || !crafter->IsInWorld())
        return none;

    std::lock_guard<std::mutex> g(mtx_);

    // Pick the OLDEST Open order this crafter can fulfil (recipe known, not
    // their own order). Oldest-first keeps the board FIFO-fair.
    CraftOrder* best = nullptr;
    for (auto& [id, o] : orders_)
    {
        if (o.status != CraftOrderStatus::Open) continue;
        if (o.requester_low == crafter_low) continue;   // never craft your own
        // Recipe-known check via the live spellbook (HasSpell). Passive/known
        // recipe spells satisfy this; a recipe the crafter hasn't learned is
        // skipped so the order stays Open for a crafter who does know it.
        if (!crafter->HasSpell(o.spell_id)) continue;
        if (!best || o.id < best->id)
            best = &o;
    }
    if (!best)
        return none;

    best->status     = CraftOrderStatus::Claimed;
    best->crafter_low = crafter_low;
    best->claimed_ms = GameTime::GetGameTimeMS();

    DbUpdateClaim(best->id, crafter_low, /*claimed_unix*/ 0);

    TC_LOG_INFO("playerbot.v2",
        "[CraftOrderBoard] ClaimOpenOrder id={} crafter_low={} spell={} (now Claimed)",
        best->id, crafter_low, best->spell_id);
    return *best;
}

bool CraftOrderBoard::MarkDelivered(uint64 order_id, uint64 crafter_low)
{
    std::lock_guard<std::mutex> g(mtx_);
    auto it = orders_.find(order_id);
    if (it == orders_.end())
        return false;   // unknown / already pruned (terminal) — no double-release
    CraftOrder& o = it->second;

    // Only a Claimed order owned by THIS crafter can be delivered. Terminal
    // rows (Delivered/Failed/Cancelled) aren't in `orders_` once pruned, but a
    // not-yet-pruned terminal row is guarded here too — status must be Claimed.
    if (o.status != CraftOrderStatus::Claimed)
        return false;
    if (o.crafter_low != crafter_low)
        return false;
    // Human-firewall: the crafter being paid must still be a fleet bot.
    if (!IsFleetBot(crafter_low))
        return false;

    // ESCROW RELEASE (exactly once) — transition OUT of Claimed INTO Delivered
    // and pay in the same step. Status is flipped BEFORE we drop the lock so a
    // concurrent (future) reader can't observe a Claimed-but-already-paid row.
    o.status = CraftOrderStatus::Delivered;
    DbUpdateStatus(o.id, CraftOrderStatus::Delivered);
    settled_ms_[o.id] = GameTime::GetGameTimeMS();

    PayBot(o.crafter_low, o.payment_copper, "deliver-release");

    TC_LOG_INFO("playerbot.v2",
        "[CraftOrderBoard] MarkDelivered id={} crafter_low={} pay={} (escrow released)",
        o.id, crafter_low, o.payment_copper);
    return true;
}

bool CraftOrderBoard::FailOrder(uint64 order_id)
{
    std::lock_guard<std::mutex> g(mtx_);
    auto it = orders_.find(order_id);
    if (it == orders_.end())
        return false;
    CraftOrder& o = it->second;

    // Only non-terminal (Open/Claimed) orders can be failed-and-refunded.
    if (o.status != CraftOrderStatus::Open && o.status != CraftOrderStatus::Claimed)
        return false;

    // ESCROW REFUND (exactly once) — transition INTO Failed + refund together.
    o.status = CraftOrderStatus::Failed;
    DbUpdateStatus(o.id, CraftOrderStatus::Failed);
    settled_ms_[o.id] = GameTime::GetGameTimeMS();

    PayBot(o.requester_low, o.payment_copper, "fail-refund");

    TC_LOG_INFO("playerbot.v2",
        "[CraftOrderBoard] FailOrder id={} requester_low={} pay={} (escrow refunded)",
        o.id, o.requester_low, o.payment_copper);
    return true;
}

bool CraftOrderBoard::CancelOrder(uint64 order_id, uint64 requester_low)
{
    std::lock_guard<std::mutex> g(mtx_);
    auto it = orders_.find(order_id);
    if (it == orders_.end())
        return false;
    CraftOrder& o = it->second;

    // A requester may only cancel their OWN order, and only while it's still
    // Open — once Claimed it belongs to the crafter to finish or time out.
    if (o.requester_low != requester_low)
        return false;
    if (o.status != CraftOrderStatus::Open)
        return false;

    // ESCROW REFUND (exactly once).
    o.status = CraftOrderStatus::Cancelled;
    DbUpdateStatus(o.id, CraftOrderStatus::Cancelled);
    settled_ms_[o.id] = GameTime::GetGameTimeMS();

    PayBot(o.requester_low, o.payment_copper, "cancel-refund");

    TC_LOG_INFO("playerbot.v2",
        "[CraftOrderBoard] CancelOrder id={} requester_low={} pay={} (escrow refunded)",
        o.id, requester_low, o.payment_copper);
    return true;
}

void CraftOrderBoard::Tick(uint32 now_ms)
{
    std::vector<uint64> to_fail;     // stale Claimed -> fail+refund
    std::vector<uint64> to_prune;    // terminal + aged -> drop from memory + DB

    {
        std::lock_guard<std::mutex> g(mtx_);
        for (auto const& [id, o] : orders_)
        {
            if (o.status == CraftOrderStatus::Claimed
                && AgeMs(now_ms, o.claimed_ms) > kClaimTimeoutMs)
            {
                to_fail.push_back(id);
            }
        }
        // Prune terminal rows that have lingered past the audit window. These
        // are rows still present in `orders_` only transiently (FailOrder /
        // CancelOrder / MarkDelivered keep the in-memory entry until prune so
        // settled_ms_ has a home), plus their DB rows.
        for (auto const& [id, when] : settled_ms_)
        {
            if (AgeMs(now_ms, when) > kPruneAgeMs)
                to_prune.push_back(id);
        }
    }

    // FailOrder takes the lock itself; call it outside the loop above to avoid
    // re-entrant locking. Each is an exactly-once refund + status flip.
    for (uint64 id : to_fail)
        FailOrder(id);

    if (!to_prune.empty())
    {
        std::lock_guard<std::mutex> g(mtx_);
        for (uint64 id : to_prune)
        {
            DbDelete(id);
            orders_.erase(id);
            settled_ms_.erase(id);
        }
    }

    if (!to_fail.empty() || !to_prune.empty())
        TC_LOG_INFO("playerbot.v2",
            "[CraftOrderBoard] Tick: timed-out {} claimed order(s), pruned {} finished row(s)",
            to_fail.size(), to_prune.size());
}

} // namespace Playerbot::V2
