// AuctionRules - Refactor #3 pass 2. Migrates the two AH-related idle
// rules — `idle:ah_post_surplus` (post bag greens/blues/epics) and
// `idle:ah_cancel_undercut` (cancel listings undercut by 5%) — out of
// the State_Idle linear cascade into the IdleRuleRegistry. Verbatim
// behavior — pricing bands, undercut tolerance, level gate, lockouts.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotIntent.h"
#include "Services.h"
#include "Util/ConfigReader.h"
#include "UnitDefines.h"     // UNIT_NPC_FLAG_AUCTIONEER

#include <algorithm>

namespace Playerbot {

namespace {

constexpr float kInteractSq = 5.0f * 5.0f;

// EconProfile underlying values mirrored into ArchetypeState::econ_profile.
// (See BotArchetype.h EconProfile: 0=Hoarder, 1=Balanced, 2=Reseller.)
constexpr uint8 kEconHoarder  = 0;
constexpr uint8 kEconBalanced = 1;
constexpr uint8 kEconReseller = 2;

// Gold reserve (copper) a bot keeps untouched for repairs + reagent vendor
// buys before spending on AH buyouts. Scales with level so a low-level bot
// isn't gated out of cheap reagent buys while a max-level bot still keeps a
// meaningful cushion. Mirrors the "real players don't spend their last gold
// on the AH" instinct and keeps the buy loop from starving the vendor/repair
// path. Hoarders keep a fatter reserve; Resellers run leaner working capital.
uint64 GoldReserveFor(BotSnapshotView const& s, uint8 econ)
{
    const uint64 base = 50000ull /* 5g */ + uint64(s.level()) * 2000ull /* +20s/level */;
    switch (econ)
    {
        case kEconHoarder:  return base * 3;   // keep a deep cushion, buy minimally
        case kEconReseller: return base / 2;   // lean — capital works on the AH
        case kEconBalanced:
        default:            return base;       // Balanced (and any future profile)
    }
}

// ItemClass::ITEM_CLASS_TRADE_GOODS — herbs, ore, cloth, leather, gems,
// elemental motes, etc. These are the stackable materials real players
// gather and resell on the AH. (L-P2b)
constexpr uint8 kItemClassTradeGoods = 7;

// L-P2b: per-unit price floor for trade goods, keyed by item quality.
// The snapshot's InventoryItem carries no vendor sell price (see
// BotSnapshot.h:74-110 — no vendor_sell_price field), so the usual
// "vendor-price multiple" floor is unavailable. We fall back to a flat
// per-unit copper band by quality instead. Conservative on the low end:
// junk-tier mats still clear at a few silver/unit, rare reagents anchor
// higher. // L-P2b: needs vendor_sell_price on InventoryItem for a
// true vendor-multiple floor.
uint64 TradeGoodUnitFloor(uint8 quality)
{
    switch (quality)
    {
        case 0:  return 100;     //  1s  — grey vendor trash mats
        case 1:  return 250;     //  2s50 — common mats (most herbs/ore/cloth)
        case 2:  return 1000;    // 10s  — uncommon reagents
        case 3:  return 5000;    // 50s  — rare reagents / gems
        case 4:  return 25000;   //  2g50 — epic-tier mats
        default: return 1000;
    }
}

NearbyUnit const* InRangeAuctioneer(BotSnapshotView const& s)
{
    auto const* npc = s.nearest_npc_with_flag(UNIT_NPC_FLAG_AUCTIONEER);
    if (!npc) return nullptr;
    float bx, by, bz; s.position(bx, by, bz);
    const float dx = npc->x - bx, dy = npc->y - by, dz = npc->z - bz;
    return (dx*dx + dy*dy + dz*dz <= kInteractSq) ? npc : nullptr;
}

// Returns true when the bot currently owns a listing of `entry` that is
// failing to sell — already expired, or past its half-life with no bidder.
// This is the "the market isn't taking my ask" signal that feeds the dynamic
// relist discount. We treat past-halfway-no-bidder (not only fully expired) as
// failing so the price correction happens BEFORE the listing rots all the way
// out and mails back, mirroring a seller who cancels-and-relists a stale ask.
// Classify the prior-listing outcome for `entry` from the snapshot's owned
// auctions, to drive the dynamic relist memory:
//   -1 = no owned listing of this entry at all (it cleared: sold, or expired
//        and mailed back — either way the bot's current ask isn't sitting
//        unsold on the AH, so we DECAY the discount streak),
//    0 = owned listing present and still healthy (has a bidder, or plenty of
//        time left) — leave the streak untouched (no signal),
//   +1 = owned listing present but FAILING (expired unsold, or past half-life
//        with no bidder) — BUMP the discount streak.
// We only call observe() for -1/+1 so a healthy in-flight listing doesn't
// reset a discount the bot is still mid-correction on.
int PriorListingOutcome(BotSnapshotView const& s, uint32 entry)
{
    bool found = false;
    for (auto const& own : s.raw().auction.auctions_owned)
    {
        if (own.item_entry != entry) continue;
        found = true;
        if (own.has_bidder) continue;                 // a bid = the ask is working
        if (own.expires_in_sec <= 0) return +1;       // expired unsold
        // 24h listings dominate; "past half-life" ~ under 12h remaining with no
        // bidder reads as a stale ask the seller would re-price down.
        if (own.expires_in_sec < 12 * 60 * 60) return +1;
    }
    return found ? 0 : -1;
}

// Fold the prior-listing outcome into the per-bot sell memory for `entry`.
void RecordPriorListingOutcome(BotSnapshotView const& s, BotAI& ai, uint32 entry)
{
    const int outcome = PriorListingOutcome(s, entry);
    if (outcome > 0)      ai.observe_item_sale_outcome(entry, /*expired_unsold*/ true);
    else if (outcome < 0) ai.observe_item_sale_outcome(entry, /*expired_unsold*/ false);
    // outcome == 0: healthy in-flight listing — no change.
}

// Apply the per-bot sell-price memory + econ bias to a computed buyout, then
// clamp at the hard quality floor. `floor` is the quality-band minimum from
// AuctionRules — the dynamic discount can never push the ask below it, so the
// loop cannot dump an item at a loss no matter how many times it expires.
// Reseller undercuts harder (extra discount); Hoarder holds a higher ask
// (premium bias). Returns a silver-aligned buyout >= floor.
uint64 ApplyDynamicSellPrice(BotAI& ai, uint32 entry, uint64 buyout, uint64 floor, uint8 econ)
{
    int32 bias = 0;
    if (econ == kEconReseller) bias = -6;   // move volume — undercut a touch more
    else if (econ == kEconHoarder) bias = +8;   // patient seller — hold the line higher

    const int32 pct = ai.sell_price_adjust_pct(entry, bias);   // signed, e.g. -32..+13
    // buyout * (100 + pct) / 100 with overflow-safe int64 math.
    int64 adjusted = int64(buyout) + (int64(buyout) * pct) / 100;
    if (adjusted < int64(floor)) adjusted = int64(floor);
    uint64 result = uint64(adjusted);
    result -= (result % 100);                 // silver-align
    if (result < floor) result = floor;       // floor may already be silver-aligned
    if (result < 100) result = 100;
    return result;
}

// ---------- idle:ah_post_surplus ----------
bool PostSurplusGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&,uint32)
{
    // Quest-first (2026-06-16): yield to a reachable quest action.
    if (s.has_actionable_quest()) return false;
    if (s.level() < 25 || s.raw().movement.is_mounted) return false;
    if (s.raw().inventory.bag_items.empty()) return false;
    return InRangeAuctioneer(s) != nullptr;
}

bool PostSurplusFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32 now_ms)
{
    NearbyUnit const* npc = InRangeAuctioneer(s);
    if (!npc) return false;

    // L-P2b: trade-good stacks (gathered mats) get their own posting pass,
    // before the equippable-surplus pass below. Real players list their
    // gathered herbs/ore/cloth/leather; the old logic skipped everything
    // with count>1, so stacks of mats were never auctioned.
    for (auto const& it : s.raw().inventory.bag_items)
    {
        if (it.guid.IsEmpty()) continue;
        if (it.is_quest_item) continue;
        if (it.item_class != kItemClassTradeGoods) continue;   // only genuine trade goods
        if (it.count <= 1) continue;                            // singletons fall to the gear pass
        if (it.stats.is_soulbound || it.stats.bonding == 1) continue;

        const uint64 ah_key = uint64(it.entry);
        if (ai.action_recently_tried(BotAI::ActionKind::AhPostItem, ah_key, now_ms))
            continue;

        // Per-unit price = quality floor, then undercut the lowest competing
        // listing if the snapshot has price data. ah_competing_buyout for
        // commodities is the lowest per-unit buyout (matches the retail
        // commodity model), so we compare and undercut on a per-unit basis.
        uint64 unit_price = TradeGoodUnitFloor(it.quality);
        for (auto const& comp : s.raw().auction.ah_competing_buyout)
        {
            if (comp.item_entry != it.entry) continue;
            if (comp.lowest_buyout < 200) break;                // too cheap to undercut sanely
            uint64 undercut = comp.lowest_buyout - 100;         // shave 1 silver/unit
            undercut -= (undercut % 100);
            // Never undercut below the quality floor — keeps mats from being
            // dumped at a loss when a competitor is already rock-bottom.
            if (undercut < unit_price) undercut = unit_price;
            unit_price = undercut;
            break;
        }
        unit_price -= (unit_price % 100);                       // silver-align per unit
        if (unit_price < 100) unit_price = 100;                 // 1s/unit hard floor

        // #4B-1 Part 2: dynamic relist pricing. Fold the per-bot sell memory
        // (did the LAST listing of this entry sell or expire?) and the econ
        // bias into the per-unit ask, clamped at the quality floor. If the
        // bot currently owns a failing listing of this entry, record the
        // unsold outcome first so the discount applies THIS post.
        const uint8 econ_tg = s.archetype_econ_profile();
        RecordPriorListingOutcome(s, ai, it.entry);
        unit_price = ApplyDynamicSellPrice(ai, it.entry, unit_price,
                                           /*floor*/ TradeGoodUnitFloor(it.quality), econ_tg);

        // L-P2b: AuctionSellItemIntent / API::auction_sell_item is a single-
        // item lister and explicitly rejects commodities/stacks
        // (PlayerbotAPI.cpp:3925 MaxStackSize>1 -> Locked, :3931 count!=1 ->
        // Locked). The whole-stack buyout is therefore the conservative,
        // forward-compatible value to emit: unit_price * count.
        // // L-P2b: needs API::auction_sell_commodity (CMSG_AUCTION_SELL_COMMODITY
        // path) to actually list trade-good stacks; until that primitive
        // exists this intent will be rejected for count>1 items. Pricing
        // and selection logic below are complete and correct so they go
        // live the moment the commodity API lands.
        const uint64 stack_buyout = unit_price * uint64(it.count);
        const uint64 stack_min_bid = (stack_buyout * 80) / 100;
        emit.emit(AuctionIntent{AuctionSellItemIntent{
            npc->guid, it.guid, stack_min_bid, stack_buyout, /*run_time_minutes=*/24 * 60}});
        ai.note_action_retry(BotAI::ActionKind::AhPostItem, ah_key, now_ms);
        ai.note_item_listed(it.entry, now_ms);   // dynamic-pricing memory
        ai.set_last_rule_fired("idle:ah_post_surplus");
        return true;   // one post per fire — respects the per-cycle limit
    }

    for (auto const& it : s.raw().inventory.bag_items)
    {
        if (it.guid.IsEmpty()) continue;
        if (it.is_quest_item) continue;
        if (it.quality < 2 || it.quality > 4) continue;
        if (it.count > 1) continue;
        if (it.stats.is_soulbound || it.stats.bonding == 1) continue;

        const uint64 ah_key = uint64(it.entry);
        if (ai.action_recently_tried(BotAI::ActionKind::AhPostItem, ah_key, now_ms))
            continue;

        uint64 base_buyout = 0;
        switch (it.quality)
        {
            case 2:  base_buyout = 20000;  break;   //   2g
            case 3:  base_buyout = 100000; break;   //  10g
            case 4:  base_buyout = 500000; break;   //  50g
            default: continue;
        }
        const uint64 ilvl_mult = std::max<uint64>(1u, uint64(it.item_level) / 50u);
        uint64 buyout = std::min<uint64>(base_buyout * ilvl_mult, /*1000g*/ 10000000ULL);
        buyout -= (buyout % 100);

        // Hard floor for gear = 50% of the (ilvl-scaled) band. Both the
        // competitive undercut and the dynamic relist discount clamp here.
        const uint64 gear_floor = (buyout * 50) / 100;

        // Undercut competing listings by 1 silver; floor at 50% of band.
        for (auto const& comp : s.raw().auction.ah_competing_buyout)
        {
            if (comp.item_entry != it.entry) continue;
            if (comp.lowest_buyout < 200) break;
            uint64 undercut = comp.lowest_buyout - 100;
            undercut -= (undercut % 100);
            if (undercut < gear_floor) undercut = gear_floor;
            buyout = undercut;
            break;
        }

        // #4B-1 Part 2: dynamic relist pricing (same model as the trade-good
        // pass) — feed the unsold/sold memory + econ bias, clamp at gear_floor.
        const uint8 econ_gear = s.archetype_econ_profile();
        RecordPriorListingOutcome(s, ai, it.entry);
        buyout = ApplyDynamicSellPrice(ai, it.entry, buyout, gear_floor, econ_gear);

        const uint64 min_bid = (buyout * 80) / 100;
        emit.emit(AuctionIntent{AuctionSellItemIntent{
            npc->guid, it.guid, min_bid, buyout, /*run_time_minutes=*/24 * 60}});
        ai.note_action_retry(BotAI::ActionKind::AhPostItem, ah_key, now_ms);
        ai.note_item_listed(it.entry, now_ms);   // dynamic-pricing memory
        ai.set_last_rule_fired("idle:ah_post_surplus");
        return true;
    }
    return false;
}

// ---------- idle:ah_cancel_undercut ----------
bool CancelUndercutGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&,uint32)
{
    // Quest-first (2026-06-16): yield to a reachable quest action.
    if (s.has_actionable_quest()) return false;
    if (s.level() < 25 || s.raw().movement.is_mounted) return false;
    if (s.raw().auction.auctions_owned.empty()) return false;
    if (s.raw().auction.ah_competing_buyout.empty()) return false;
    return InRangeAuctioneer(s) != nullptr;
}

bool CancelUndercutFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32 now_ms)
{
    NearbyUnit const* npc = InRangeAuctioneer(s);
    if (!npc) return false;

    for (auto const& own : s.raw().auction.auctions_owned)
    {
        if (own.buyout == 0) continue;
        if (own.has_bidder) continue;
        uint64 competing = 0;
        for (auto const& comp : s.raw().auction.ah_competing_buyout)
        {
            if (comp.item_entry == own.item_entry)
            { competing = comp.lowest_buyout; break; }
        }
        if (competing == 0) continue;
        if (competing >= (own.buyout * 95) / 100) continue;
        const uint64 cancel_key = (uint64(1) << 63) | uint64(own.auction_id);
        if (ai.action_recently_tried(BotAI::ActionKind::AhPostItem, cancel_key, now_ms))
            continue;
        emit.emit(AuctionIntent{AuctionCancelIntent{npc->guid, own.auction_id}});
        ai.note_action_retry(BotAI::ActionKind::AhPostItem, cancel_key, now_ms);
        ai.set_last_rule_fired("idle:ah_cancel_undercut");
        return true;
    }
    return false;
}

// ---------- idle:ah_buy_reagents ----------
//
// Buy-side of the AH loop (#4B-1 Part 2). At an auctioneer, buy out the
// cheapest current listing for any item in the snapshot's buyable set —
// reagents the bot is SHORT ON for its known, still-skillable recipes
// (resolved world-thread in BotSnapshotBuilder into auction.buyable_listings).
// Resellers ALSO buy cheap flippable items to relist (the post-surplus rule
// then re-prices and re-lists them, so the markup is realized on sale).
//
// GOLD-SINK LOOP (documented for #4B-3 economy auditing):
//   bot A lists item -> pays AH deposit (gold destroyed)
//   bot B buys it out -> gold A<-B transfer (net zero between bots)
//   on the SALE, the AH takes its cut (gold destroyed) before A is paid
// So each completed post->buyout cycle destroys deposit + cut. The buy rule
// does NOT create gold; it only moves existing gold between bots, and every
// hop through the AH leaks a slice to the house cut. Net effect over time is
// gold DESTRUCTION, the intended sink — bots cannot accumulate infinitely by
// trading among themselves, because the cut bleeds the pool on every trade.
// (Server-side: ModifyMoney on buyout debits the buyer; AuctionHouseMgr applies
//  the consignment cut, see PlayerbotAPI::auction_buyout.)
//
// Affordability: the bot only spends gold ABOVE a level/econ-scaled reserve it
// keeps for repairs + vendor reagents (GoldReserveFor). Per-listing dedup is
// the AhBuyout ActionKind (30s) so a still-wanted listing re-fires within the
// visit but isn't double-spent before the executor settles.
bool BuyReagentsGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    if (!Services::Config().economy_buy_enabled()) return false;
    // Quest-first (2026-06-16): yield to a reachable quest action.
    if (s.has_actionable_quest()) return false;
    if (s.level() < 5 || s.raw().movement.is_mounted) return false;
    // Either the rare non-stackable listing OR (the common case) commodity
    // reagents must have something to buy. #4B-1 Part 3 added the commodity
    // path; before it, the gate keyed only on buyable_listings (almost always
    // empty for reagents) so the buy loop never fired.
    if (s.buyable_listings().empty() && s.buyable_commodities().empty()) return false;
    // Need at least the reserve plus a token amount to consider buying.
    const uint8 econ = s.archetype_econ_profile();
    if (uint64(s.gold()) <= GoldReserveFor(s, econ)) return false;
    return InRangeAuctioneer(s) != nullptr;
}

bool BuyReagentsFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32 now_ms)
{
    NearbyUnit const* npc = InRangeAuctioneer(s);
    if (!npc) return false;

    const uint8  econ        = s.archetype_econ_profile();
    const uint64 reserve     = GoldReserveFor(s, econ);
    const uint64 spendable   = uint64(s.gold()) > reserve ? uint64(s.gold()) - reserve : 0;
    if (spendable == 0) return false;

    // Per-econ cap on what fraction of spendable gold a SINGLE buyout may use.
    // Hoarders nibble (won't sink working capital into one listing); Resellers
    // commit more aggressively to flip volume; Balanced sits between.
    const uint64 per_buy_cap =
        econ == kEconReseller ? spendable                  // up to all spendable
      : econ == kEconHoarder  ? spendable / 4              // conservative
      :                         spendable / 2;             // Balanced

    for (auto const& bl : s.buyable_listings())
    {
        if (bl.auction_id == 0) continue;
        if (bl.buyout == 0) continue;                       // buy-side wants buyout
        // #4B-1(b) PRICE-PUMP guard: refuse a listing whose PER-UNIT price
        // (buyout / stack) exceeds the snapshot's fair-value ceiling (vendor
        // SellPrice * MaxReagentVendorMultiple, or a quality flat ceiling).
        // Without this a human could post one absurdly-priced reagent the bot
        // "needs" and bleed its gold one over-priced buyout at a time.
        if (bl.fair_value_ceiling != 0)
        {
            const uint32 stack = bl.stack > 0 ? bl.stack : 1u;
            const uint64 unit  = bl.buyout / uint64(stack);
            if (unit > bl.fair_value_ceiling) continue;     // over fair value — skip
        }
        if (bl.buyout > per_buy_cap) continue;              // out of single-buy budget
        if (bl.buyout > spendable) continue;                // can't afford at all

        // How short is the bot on this entry? buyable_listings is already the
        // wanted/short set (builder), but re-confirm against live snapshot
        // inventory so a reagent topped up by an earlier buy this visit is
        // skipped. item_count is the O(1) snapshot rollup.
        const uint32 have = s.item_count(bl.item_entry);

        // Hoarders buy ONLY when genuinely out (have 0); Balanced buys when
        // short (handled by the builder's wanted set — any surfaced entry is a
        // shortfall); Resellers additionally flip — they'll buy even an entry
        // they already hold some of, to relist. The builder only surfaces
        // shortfalls, so for Reseller the `have` check is relaxed; for the
        // others a non-zero stock means a partial top-up already happened.
        if (econ == kEconHoarder && have > 0) continue;

        // Per-listing dedup (AhBuyout 30s). The emitter applies the same guard
        // internally, but checking here avoids burning the one-action-per-fire
        // slot on a listing already in flight.
        if (ai.action_recently_tried(BotAI::ActionKind::AhBuyout, bl.auction_id, now_ms))
            continue;

        // Pay the listed buyout exactly. The server-side API re-validates the
        // price against the live auction (silver-aligned, still exists, not
        // own, HasEnoughMoney) and is a no-op on any mismatch, so emitting the
        // snapshot's buyout is safe even if the listing changed since the scan.
        // op.price is carried as a server-side max-price guard so a listing
        // re-priced UP since the scan can't overspend (#4B-1 Part 3 cleanup).
        if (emit.ah_buyout(npc->guid, bl.auction_id, bl.buyout))
        {
            ai.set_last_rule_fired("idle:ah_buy_reagents");
            return true;   // one buy per fire — paces spending, respects budget
        }
    }

    // ---- Commodity reagents (#4B-1 Part 3) ----
    // The dominant reagent case: stackable trade goods bought via the
    // commodity quote/buy path. Same affordability/reserve gating, one buy
    // per fire. The builder only surfaces reagents the bot is SHORT on, so any
    // entry here is a genuine shortfall; we size the buy by what the bot can
    // afford within the single-buy budget, bounded by available supply.
    for (auto const& bc : s.buyable_commodities())
    {
        if (bc.item_entry == 0) continue;
        if (bc.unit_price == 0 || bc.available_qty == 0) continue;
        // #4B-1(b) PRICE-PUMP guard: refuse a commodity whose per-unit price
        // exceeds the fair-value ceiling. Same anti-pump rationale as the
        // non-commodity listing path above — a human-controlled overpriced
        // commodity listing is skipped instead of draining the bot's gold.
        if (bc.fair_value_ceiling != 0 && bc.unit_price > bc.fair_value_ceiling)
            continue;
        if (bc.unit_price > spendable) continue;            // can't afford even one unit
        if (bc.unit_price > per_buy_cap) continue;          // one unit blows the single-buy budget

        // Hoarders only top up when genuinely out; Balanced/Reseller buy while
        // short (the builder's wanted set already encodes the shortfall).
        const uint32 have = s.item_count(bc.item_entry);
        if (econ == kEconHoarder && have > 0) continue;

        // Quantity = as much of the shortfall as the single-buy budget affords,
        // bounded by available supply. The snapshot carries cheapest unit +
        // total available (not the recipe need directly), so size the buy by
        // the budget: affordable_units = per_buy_cap / unit_price, capped to
        // available_qty. At least 1 (we already proved one unit is affordable).
        uint32 affordable = static_cast<uint32>(std::min<uint64>(
            per_buy_cap / bc.unit_price, uint64(bc.available_qty)));
        if (affordable == 0) affordable = 1;
        const uint32 qty = std::min<uint32>(affordable, bc.available_qty);

        // Per-item_entry dedup (AhBuyCommodity 30s). Checked here too so a
        // reagent already in flight doesn't burn the one-buy-per-fire slot.
        if (ai.action_recently_tried(BotAI::ActionKind::AhBuyCommodity, bc.item_entry, now_ms))
            continue;

        // Slippage margin: allow ~5% above the snapshot unit price so a small
        // upward move between the scan and execution doesn't fail the buy, but
        // a big jump still refuses (server-side max_total guard). Silver-align
        // is handled server-side; the commodity path quotes the live total.
        const uint64 base_total = bc.unit_price * uint64(qty);
        const uint64 max_total  = base_total + (base_total * 5) / 100;
        if (max_total > spendable) continue;                // margin pushes past budget

        if (emit.ah_buy_commodity(npc->guid, bc.item_entry, qty, max_total))
        {
            ai.set_last_rule_fired("idle:ah_buy_reagents");
            return true;   // one buy per fire — paces spending, respects budget
        }
    }
    return false;
}

} // anonymous namespace

void RegisterAuctionRules(IdleRuleRegistry& r)
{
    // Both auction rules sit at the same priority as the legacy cascade
    // position (right after vendor_visit). Cancel-undercut runs slightly
    // before post-surplus so a freshly-cancelled listing gets the next
    // tick's repost rather than competing with itself.
    {
        IdleRule rule;
        rule.name     = "idle:ah_cancel_undercut";
        rule.priority = 460;
        rule.gate     = &CancelUndercutGate;
        rule.fire     = &CancelUndercutFire;
        // Auction prices change on the minute scale; 10s gate-throttle.
        rule.min_interval_ms = 10000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:ah_post_surplus";
        rule.priority = 450;
        rule.gate     = &PostSurplusGate;
        rule.fire     = &PostSurplusFire;
        rule.min_interval_ms = 10000;
        r.register_rule(std::move(rule));
    }
    {
        // Buy-side closes the loop just below post-surplus: a bot at an
        // auctioneer lists its surplus first, then spends spare gold (above
        // its repair/reagent reserve) buying reagents it's short on. 10s
        // gate-throttle mirrors the other AH rules; the AhBuyout 30s dedup
        // bounds re-emits per listing.
        IdleRule rule;
        rule.name     = "idle:ah_buy_reagents";
        rule.priority = 445;
        rule.gate     = &BuyReagentsGate;
        rule.fire     = &BuyReagentsFire;
        rule.min_interval_ms = 10000;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot
