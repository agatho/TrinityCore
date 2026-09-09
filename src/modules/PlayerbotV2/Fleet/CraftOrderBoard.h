// CraftOrderBoard - Bot-to-bot CRAFT-ORDER board + escrow ledger (#4B-2(a)).
//
// **What problem it solves:** the profession economy was a one-way street.
// Gatherers fed reagents into the auction house; crafters had recipes but no
// demand signal, so nothing got *made to order*. This board is the demand
// channel: a requester bot that wants an item it can't make itself POSTS an
// order (escrowing the payment up front), a crafter bot that KNOWS the recipe
// CLAIMS it, crafts the product, mails it to the requester, and the escrowed
// gold is released to the crafter. Gold circulates between bots; reagents get
// consumed; the loop closes.
//
// **Threading:** world-thread singleton, owned + ticked from the V2 module
// lifecycle exactly like BotGroupBuilder. PostOrder/ClaimOpenOrder/
// MarkDelivered/FailOrder mutate fleet state and touch Player gold, so they all
// run on the world thread. Snapshot projection (PopulateSnapshot) is also a
// world-thread read called from the snapshot builder. An internal mutex guards
// the in-memory order map so a future diagnostic reader on another thread is
// safe, but no public method is intended to be called off the world thread.
//
// **SECURITY (#4B-2 human-firewall):** this is a CLOSED bot-to-bot system.
// EVERY transition re-verifies that requester_low AND (when present)
// crafter_low are CURRENT fleet bots via Services::Registry().has(...). If
// either is not a registered bot the transition is REJECTED. There is no GM
// command, whisper, or any other human entry point — a real player cannot
// create, claim, fulfil, or extract value from an order. The verification is
// re-run at claim/deliver/fail time (not just at post time) because a bot can
// be unregistered between transitions; an order whose counterparty is no longer
// a bot is failed-and-refunded rather than settled.
//
// **ESCROW INVARIANT (single source of truth = the DB row `status`):**
//   payment_copper is removed from the requester's gold EXACTLY ONCE, at the
//   Open transition (PostOrder). It then has exactly one terminal fate:
//     * Delivered          -> paid to the crafter   (MarkDelivered)
//     * Failed / Cancelled -> refunded to requester  (FailOrder / CancelOrder)
//   Settlement may only happen while transitioning OUT of a non-terminal status
//   (Open/Claimed) INTO a terminal one, and the transition is committed to the
//   row in the SAME step. Terminal rows are never re-settled, so the gold can
//   never be double-released, double-refunded, or lost. If a Player* can't be
//   resolved at settlement time (bot logged out) the gold is mailed instead of
//   added directly, so the value still moves exactly once.

#pragma once

#include "ObjectGuid.h"
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

// CraftOrderState is defined in BotSnapshot.h; PopulateSnapshot is a header
// template that touches its members, so the full definition is required here.
#include "../Bot/BotSnapshot.h"

namespace Playerbot::V2 {

// Lifecycle status of a craft order. Mirrors the TINYINT `status` column in
// bot_craft_orders. Open/Claimed are non-terminal (escrow still held);
// Delivered/Failed/Cancelled are terminal (escrow already settled).
enum class CraftOrderStatus : uint8_t
{
    Open      = 0,   // posted, escrow held, waiting for a crafter
    Claimed   = 1,   // a crafter owns it, escrow still held, crafting in flight
    Delivered = 2,   // product mailed, escrow paid to crafter (terminal)
    Failed    = 3,   // timed out / counterparty gone, escrow refunded (terminal)
    Cancelled = 4    // requester withdrew, escrow refunded (terminal)
};

// In-memory mirror of one bot_craft_orders row. The DB row is the source of
// truth for the escrow status; this struct is the world-thread working copy.
struct CraftOrder
{
    uint64           id             = 0;
    uint64           requester_low  = 0;
    uint64           crafter_low    = 0;   // 0 while Open
    uint32           spell_id       = 0;
    uint32           item_entry     = 0;
    uint32           quantity       = 1;
    uint64           payment_copper = 0;
    CraftOrderStatus status         = CraftOrderStatus::Open;
    uint32           created_ms     = 0;   // GameTime::GetGameTimeMS at post
    uint32           claimed_ms     = 0;   // GameTime::GetGameTimeMS at claim (0 = unclaimed)
};

class CraftOrderBoard
{
public:
    CraftOrderBoard() = default;

    // Load open + claimed orders from bot_craft_orders into memory and prune
    // any rows whose counterparty is no longer a fleet bot (refund + Fail).
    // Called once at module init after the registry is populated. Terminal rows
    // are left in the DB for the Tick pruner to age out.
    void LoadFromDb();

    // ---- Transitions (all world-thread; all re-verify the human-firewall) ----

    // Post a new order. Escrows `payment` by debiting the requester's gold up
    // front and writing an Open row. Returns the new order id, or 0 on refusal
    // (requester not a fleet bot / not in world / can't afford payment+nothing
    // else / bad args). The gold leaves the requester HERE and exactly once.
    uint64 PostOrder(uint64 requester_low, uint32 spell_id, uint32 item_entry,
                     uint32 quantity, uint64 payment_copper);

    // Claim the oldest Open order whose recipe the crafter KNOWS (verified via
    // the crafter's live spellbook). Flips it to Claimed and stamps the
    // crafter. Returns the claimed order by value (id != 0) on success, or an
    // id==0 order when nothing claimable exists / the crafter isn't a fleet
    // bot / isn't in world. Escrow is untouched (still held) on claim.
    CraftOrder ClaimOpenOrder(uint64 crafter_low);

    // Settle a Claimed order as Delivered: release the escrow to the crafter
    // (direct gold add when in world, else mailed) and mark Delivered. Returns
    // true on a successful one-time release. Refuses (returns false, escrow
    // untouched) when the order isn't Claimed, the crafter doesn't match, or
    // the crafter is no longer a fleet bot. Idempotent against terminal rows:
    // a second call returns false without moving gold again.
    bool MarkDelivered(uint64 order_id, uint64 crafter_low);

    // Settle an Open/Claimed order as Failed: refund the escrow to the
    // requester (direct add when in world, else mailed) and mark Failed.
    // Returns true on a successful one-time refund; false (escrow untouched)
    // for already-terminal rows. Used by Tick timeouts + counterparty-gone.
    bool FailOrder(uint64 order_id);

    // Requester-initiated withdrawal of an OPEN order: refund + mark Cancelled.
    // Refuses a Claimed/terminal order (a claimed order is the crafter's to
    // finish or time out). Returns true on a one-time refund.
    bool CancelOrder(uint64 order_id, uint64 requester_low);

    // ---- World-thread maintenance ----

    // Age out stale Claimed orders (claimed longer than kClaimTimeoutMs ->
    // Fail+refund so the requester's gold isn't trapped behind a stuck
    // crafter) and prune finished (terminal) rows older than kPruneAgeMs from
    // both memory and the DB. Called on a slow cadence from the module tick.
    void Tick(uint32 now_ms);

    // ---- Snapshot projection (world-thread read) ----

    // Fill `out` for `bot_low`: open-order count it posted as requester,
    // whether the board has an Open order this bot can craft (recipe-known
    // check uses `bot_knows_recipe`), and the order it currently owns as
    // crafter. `bot_knows_recipe(spell_id)` is supplied by the builder (which
    // already has the bot's Player*/spellbook in hand) so the board doesn't
    // reach into Player state itself.
    template <class KnowsFn>
    void PopulateSnapshot(uint64 bot_low, CraftOrderState& out, KnowsFn&& bot_knows_recipe) const;

private:
    // Claimed orders idle longer than this are failed + refunded (crafter
    // stuck / logged out). 30 min — generous for a crafter to walk to reagents,
    // an auctioneer, and a mailbox, but bounded so escrow can't be trapped.
    static constexpr uint32 kClaimTimeoutMs = 30u * 60u * 1000u;
    // Terminal rows older than this are pruned from memory + DB. 1h keeps a
    // short audit tail for `/econ`-style diagnostics without unbounded growth.
    static constexpr uint32 kPruneAgeMs     = 60u * 60u * 1000u;
    // Settlement timestamp marker for terminal rows (memory-only; used for the
    // prune age check). Reuses claimed_ms's slot is avoided — we keep a
    // dedicated map so Open/Claimed semantics of claimed_ms stay clean.
    std::unordered_map<uint64, uint32> settled_ms_;

    // Verify a guid-low is a CURRENT fleet bot (human-firewall). Centralized so
    // every transition routes through the same check.
    static bool IsFleetBot(uint64 guid_low);

    // Pay `copper` to `bot_low`: direct ModifyMoney when the bot is in world,
    // else mail it. Returns true when the value moved. Used by both release
    // (to crafter) and refund (to requester) so the "exactly once" guarantee
    // has a single implementation.
    static bool PayBot(uint64 bot_low, uint64 copper, char const* reason);

    // DB writers (CharacterDatabase hosts the playerbot_v2 schema). All run on
    // the world thread; they use DirectPExecute so the row is durable before
    // the next transition can read it back on reload.
    static void DbInsertReturningId(CraftOrder& o);
    static void DbUpdateClaim(uint64 id, uint64 crafter_low, uint32 claimed_unix);
    static void DbUpdateStatus(uint64 id, CraftOrderStatus status);
    static void DbDelete(uint64 id);

    mutable std::mutex                       mtx_;
    std::unordered_map<uint64, CraftOrder>   orders_;   // keyed by order id
};

// ---- template impl (header so the builder can instantiate KnowsFn) ----

template <class KnowsFn>
void CraftOrderBoard::PopulateSnapshot(uint64 bot_low, CraftOrderState& out,
                                       KnowsFn&& bot_knows_recipe) const
{
    out = CraftOrderState{};
    if (bot_low == 0) return;

    std::lock_guard<std::mutex> g(mtx_);
    for (auto const& [id, o] : orders_)
    {
        if (o.status == CraftOrderStatus::Open)
        {
            if (o.requester_low == bot_low)
                ++out.my_open_order_count;
            // Only advertise as claimable to bots OTHER than the requester
            // (a bot never crafts its own order) that actually know the recipe.
            else if (!out.has_claimable_order && o.requester_low != bot_low
                     && bot_knows_recipe(o.spell_id))
                out.has_claimable_order = true;
        }
        else if (o.status == CraftOrderStatus::Claimed
                 && o.crafter_low == bot_low && out.claimed_order_id == 0)
        {
            out.claimed_order_id      = o.id;
            out.claimed_spell_id      = o.spell_id;
            out.claimed_item_entry    = o.item_entry;
            out.claimed_quantity      = o.quantity;
            out.claimed_requester_low = o.requester_low;
        }
    }
}

} // namespace Playerbot::V2
