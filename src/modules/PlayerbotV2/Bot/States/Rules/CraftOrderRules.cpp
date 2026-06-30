// CraftOrderRules - #4B-2(a) part 2. The two bot-to-bot craft-order idle rules:
//
//   idle:craft_order_post     (low priority, never preempts combat/quest/travel)
//       A bot that wants a CRAFTED intermediate it cannot make itself posts an
//       order to the board with a market-derived payment, escrowed up front.
//       The "want" is resolved world-thread in BotSnapshotBuilder into
//       craft_orders.want_* (a reagent the bot is short on for its OWN known
//       recipes that is itself the product of a recipe the bot does NOT know).
//       Frequency is biased by archetype: Social / Profession bots post more
//       readily (they're the "use the economy" archetypes); a combat/PvP bot
//       posts only occasionally. Bounded to a small number of open orders/bot.
//
//   idle:craft_order_fulfill  (priority ABOVE idle:craft_skillup so commissions
//       beat grind). If the board has an Open order whose recipe THIS bot knows,
//       claim it; if the bot already holds a Claimed order, fulfil it by emitting
//       CraftFulfill (crafts + mails the product to the requester + releases the
//       escrow via MarkDelivered). Material shortfalls are NOT blocked here — the
//       existing gather + #4B-1 reagent-buy idle paths compose over subsequent
//       ticks to top the crafter up; if mats never arrive the board times the
//       claim out and refunds the requester.
//
// THREADING: these rules run on the AI WORKER thread and only EMIT intents. The
// actual board mutations (PostOrder / ClaimOpenOrder / MarkDelivered) and the
// craft+mail all run on the WORLD thread inside the intent executor, where the
// escrow debit, the live spellbook check, and the fleet-bot human-firewall live.
//
// SECURITY (#4B-2 human-firewall): nothing here can be reached by a human. The
// want/claimable signals come from the snapshot (built only for registered fleet
// bots), and every board transition the executor performs re-verifies BOTH
// counterparties are current fleet bots via Services::Registry(). A real player
// is never in the registry, never gets a snapshot, and never has an idle tick.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotIntent.h"

namespace Playerbot {

namespace {

// ArchetypeActivity underlying values mirrored into ArchetypeState
// ::dominant_activity. (See BotArchetype.h ActivityPref: 0=Solo, 1=Group,
// 2=Pvp, 3=Profession, 4=Social.) The post rule biases on these so the bots
// that "play the economy" (Profession/Social) drive most order demand.
constexpr uint8 kActProfession = 3;
constexpr uint8 kActSocial     = 4;

// Cap on concurrent Open orders a single bot may have outstanding as requester.
// Keeps one bot from flooding the board; the board's my_open_order_count
// projection is the authoritative count (no per-tick board lock needed here).
constexpr uint32 kMaxOpenOrdersPerBot = 2;

// Per-tick post probability is realized via a stable hash so the decision is
// deterministic for a given (bot, snapshot) and doesn't thrash: a bot biased to
// post will clear the threshold, one biased against will not, and the want
// signal persists across snapshots so a genuine want eventually posts even for a
// low-bias bot once the hash bucket aligns. Returns true when the bot SHOULD
// post this tick given its archetype lean.
bool ArchetypeWantsToPost(BotSnapshotView const& s, uint32 now_ms)
{
    const uint8 act = s.archetype_dominant_activity();
    // Bias: Profession/Social bots post ~most ticks they have a want; everyone
    // else posts on a slow rotation so the economy still circulates without a
    // combat bot detouring into market behaviour every chance it gets.
    //   Profession/Social -> ~3 of 4 eligible ticks
    //   others            -> ~1 of 8 eligible ticks
    const uint32 threshold = (act == kActProfession || act == kActSocial) ? 3u : 1u;
    const uint32 modulus   = (act == kActProfession || act == kActSocial) ? 4u : 8u;
    // Stable per-bot, per-5s-bucket hash (mirrors the wander-angle fan-out).
    const uint64 bot = static_cast<uint64>(s.raw().bot_id);
    const uint32 bucket = now_ms / 5000u;
    uint64 h = bot * 1099511628211ull;
    h ^= (h >> 27);
    h += static_cast<uint64>(bucket) * 1469598103934665603ull;
    h ^= (h >> 31);
    return static_cast<uint32>(h % modulus) < threshold;
}

// ---------- idle:craft_order_post ----------
bool PostOrderGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    if (s.raw().movement.is_mounted) return false;
    if (s.is_casting() || s.in_combat()) return false;
    // Quest-first (2026-06-16): yield to a reachable quest action. Previously
    // gated on has_current_objective(), which is ALSO true for R7 relocation
    // goals and over-suppressed posting during long cross-map travel;
    // has_actionable_quest() excludes relocation. The post is purely
    // opportunistic idle behaviour.
    if (s.has_actionable_quest()) return false;
    if (!s.raw().quest_discovery.quest_turnins.empty()) return false;
    if (!s.raw().quest_discovery.quest_offers.empty()) return false;

    CraftOrderState const& co = s.craft_orders();
    if (co.want_spell_id == 0 || co.want_item_entry == 0) return false;
    if (co.my_open_order_count >= kMaxOpenOrdersPerBot) return false;
    // Must be able to afford the escrow payment (the server re-checks at
    // PostOrder time and refuses otherwise, but gating here avoids a wasted
    // intent + a needless 2-min dedup lockout on an unaffordable want).
    if (co.want_payment_copper > 0 &&
        static_cast<uint64>(s.gold()) < co.want_payment_copper)
        return false;
    return true;
}

bool PostOrderFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,
                   BotIntentEmitter& emit, uint32 now_ms)
{
    CraftOrderState const& co = s.craft_orders();
    if (co.want_spell_id == 0 || co.want_item_entry == 0) return false;

    // Archetype frequency bias — Profession/Social bots drive most demand.
    if (!ArchetypeWantsToPost(s, now_ms)) return false;

    // Per-product dedup (CraftOrderPost, 2min). The board's my_open_order_count
    // (next snapshot) is the real anti-duplicate gate; this lockout just bounds
    // re-emit on the persistent want until the projection reflects the new row.
    const uint64 key = uint64(co.want_item_entry);
    if (ai.action_recently_tried(BotAI::ActionKind::CraftOrderPost, key, now_ms))
        return false;

    if (emit.craft_post(co.want_spell_id, co.want_item_entry,
                        co.want_quantity == 0 ? 1u : co.want_quantity,
                        co.want_payment_copper))
    {
        ai.note_action_retry(BotAI::ActionKind::CraftOrderPost, key, now_ms);
        ai.set_last_rule_fired("idle:craft_order_post");
        return true;
    }
    return false;
}

// ---------- idle:craft_order_fulfill ----------
bool FulfillGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    if (s.raw().movement.is_mounted) return false;
    if (s.is_casting() || s.in_combat()) return false;
    // Never chase a crafting commission while inside an instance — the
    // crafting station / trade hub is in the open world, so fulfilling would
    // walk the bot out of the dungeon/BG. (Matches the dungeon gate on the
    // other world-travel idle rules.)
    if (s.is_in_dungeon() || s.in_battleground()) return false;
    CraftOrderState const& co = s.craft_orders();
    // Quest-first (2026-06-16): yield to a reachable quest action — but ONLY on
    // the claim-NEW-work arm. A bot that already holds a claimed order MUST
    // finish it (else the requester's escrow is stranded), so don't yield when
    // claimed_order_id != 0.
    if (co.claimed_order_id == 0 && s.has_actionable_quest()) return false;
    // Work to do iff we already own a Claimed order OR the board holds an Open
    // order we can claim. Both signals are board-verified against the live
    // spellbook, so this never advertises an order for a recipe we can't make.
    return co.claimed_order_id != 0 || co.has_claimable_order;
}

bool FulfillFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,
                 BotIntentEmitter& emit, uint32 now_ms)
{
    CraftOrderState const& co = s.craft_orders();

    // 1) Already hold a Claimed order -> fulfil it. craft_fulfill crafts the
    //    product, mails it to the requester, and (on Ok) releases the escrow via
    //    MarkDelivered. We do NOT block on material shortfalls: if the crafter is
    //    short, craft_fulfill_order fails server-side this tick (order stays
    //    Claimed, escrow held) and the bot's existing gather / #4B-1 reagent-buy
    //    idle rules top it up over subsequent ticks; the next fulfil tick retries.
    //    If mats never arrive, the board times the claim out and refunds.
    //    Double-check the recipe is KNOWN (defence in depth — the board already
    //    enforces this at claim; a respec between claim and now would be caught
    //    server-side, but skipping the emit here avoids a wasted intent).
    if (co.claimed_order_id != 0 && co.claimed_spell_id != 0)
    {
        if (!s.knows_spell(co.claimed_spell_id))
            return false;   // lost the recipe since claim; let the board time it out + refund
        // Per-order dedup (CraftOrderClaim ActionKind, 30s) so a craft that
        // fails this tick for missing mats doesn't re-emit every snapshot before
        // gather/buy catches up. The order stays Claimed across the lockout
        // (escrow held). The dedup key is the order id (>= 1); the claim path
        // below keys on target 0, so the two never collide under the same kind.
        const uint64 fkey = co.claimed_order_id;
        if (ai.action_recently_tried(BotAI::ActionKind::CraftOrderClaim, fkey, now_ms))
            return false;
        if (emit.craft_fulfill(co.claimed_order_id, co.claimed_spell_id,
                               co.claimed_item_entry,
                               co.claimed_quantity == 0 ? 1u : co.claimed_quantity,
                               co.claimed_requester_low))
        {
            ai.note_action_retry(BotAI::ActionKind::CraftOrderClaim, fkey, now_ms);
            ai.set_last_rule_fired("idle:craft_order_fulfill");
            return true;
        }
        return false;
    }

    // 2) No claimed order yet but the board has an Open order we can craft ->
    //    claim it. ClaimOpenOrder runs world-thread (re-verifies live spellbook +
    //    fleet-bot firewall, flips the row to Claimed); the claimed order surfaces
    //    in the NEXT snapshot's claimed_* fields, which case (1) above then
    //    fulfils. One claim attempt per 30s (per-bot) so the bot doesn't claim a
    //    burst of orders before its first claim shows up as work.
    if (co.has_claimable_order)
    {
        if (ai.action_recently_tried(BotAI::ActionKind::CraftOrderClaim, 0, now_ms))
            return false;
        if (emit.craft_claim())
        {
            ai.note_action_retry(BotAI::ActionKind::CraftOrderClaim, 0, now_ms);
            ai.set_last_rule_fired("idle:craft_order_fulfill");
            return true;
        }
    }
    return false;
}

} // anonymous namespace

void RegisterCraftOrderRules(IdleRuleRegistry& r)
{
    // Fulfil sits ABOVE idle:craft_skillup (which lives in the State_Idle inline
    // cascade, dispatched after the bottom-of-tick registry pass) so a commission
    // always beats plain skill-up grind: a bot with both a claimable order and a
    // skillable recipe claims/crafts the order first. It's still well below
    // questing/vendor/gather priorities (those gate it off via in_combat /
    // has_current_objective), so it only fires when the bot is genuinely idle.
    {
        IdleRule rule;
        rule.name     = "idle:craft_order_fulfill";
        rule.priority = 440;   // just below ah_buy_reagents (445); above craft_skillup
        rule.gate     = &FulfillGate;
        rule.fire     = &FulfillFire;
        // Board state changes on the minute scale (claims/timeouts); the
        // CraftFulfill/CraftClaim dedup lockouts bound re-emit, so a coarse
        // gate-throttle keeps this off the 5 Hz hot path.
        rule.min_interval_ms = 5000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:craft_order_post";
        rule.priority = 435;   // low — never preempts combat/quest/travel
        rule.gate     = &PostOrderGate;
        rule.fire     = &PostOrderFire;
        // Posting is rare per bot (capped open orders + archetype bias + 2min
        // dedup); a 10s gate-throttle is plenty and keeps it off the hot path.
        rule.min_interval_ms = 10000;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot
