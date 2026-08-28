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

#include "WorldSession.h"
#include "CollectionMgr.h"
#include "DB2Stores.h"
#include "DBCEnums.h"
#include "GameTime.h"
#include "GossipDef.h"
#include "Player.h"
#include "PerksProgramActivityMgr.h"
#include "PerksProgramMgr.h"
#include "PerksProgramPackets.h"
#include "UnitDefines.h"
#include "Util.h"
#include "World.h"
#include <ctime>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// Whether the Trading Post is switched on for this realm (worldserver.conf PerksProgram.Enabled).
static bool IsPerksProgramEnabled()
{
    return sWorld->getBoolConfig(CONFIG_PERKS_PROGRAM_ENABLED);
}

// Every mutating Trading Post request (purchase / refund / cart / freeze) carries the interacted vendor GUID.
// Validate it against an active PerksProgramVendor interaction the player actually opened, and re-check the NPC
// still exists in range with the perks-vendor flag. This blocks currency/collection mutation from a crafted
// packet sent with a zero or spoofed GUID from anywhere, bypassing the client's interaction gate.
static bool HasActivePerksProgramVendor(Player* player, ObjectGuid vendorGuid)
{
    if (vendorGuid.IsEmpty())
        return false;

    if (!player->PlayerTalkClass->GetInteractionData().IsInteractingWith(vendorGuid, PlayerInteractionType::PerksProgramVendor))
        return false;

    return player->GetNPCIfCanInteractWith(vendorGuid, UNIT_NPC_FLAG_NONE, UNIT_NPC_FLAG_2_PERKS_VENDOR) != nullptr;
}

// The automatic base monthly Trader's Tender granted by the Collector's Cache (web-sourced retail value: 500 per
// Trading Post interval, account-wide).
static constexpr uint32 PERKS_MONTHLY_CACHE_TENDER = 500;

// Calendar month of a Trading Post period start as a monotonically increasing index (UTC year * 12 + month).
//
// The stipend is keyed on THIS, not on the raw periodStart timestamp, because periodStart moves with the
// configurable PerksProgram.ResetHour: at hour 0 the August period starts 2026-08-01 00:00, at hour 4 it starts
// 04:00. Comparing timestamps, any change to that setting -- including the default itself moving to 4 -- makes
// the stored value differ from the recomputed one for the very same month, so the lock would open and every
// account would collect its 500 Trader's Tender a second time on a single unpayloaded status request. The month
// index does not move with the hour.
//
// A never-granted account stores 0, which maps to January 1970 and is therefore below every real month.
static uint32 PerksPeriodMonthIndex(uint64 periodStart)
{
    time_t periodTime = time_t(periodStart);
    tm date;
    gmtime_r(&periodTime, &date);
    return uint32(date.tm_year) * 12 + uint32(date.tm_mon);
}

// Grants the account's base monthly Trader's Tender once per Trading Post interval, keyed on the calendar month
// of the current period, so it is idempotent per account per month (Tender is account-wide after G6). Triggered
// on the first Trading Post interaction of the period. Note: two game accounts of one bnet account online at once
// can each grant once for the same period (bounded, non-repeatable) since account state is not live-synced between
// concurrent sessions -- consistent with how the rest of the account collection behaves.
static void GrantMonthlyPerksCache(WorldSession* session, Player* player)
{
    uint64 periodStart = 0;
    uint64 periodEnd = 0;
    sPerksProgramMgr->GetCurrentPeriod(periodStart, periodEnd);

    // Compared with >=, not !=: raising the reset hour moves the boundary later on the 1st, which puts "now"
    // back into the PREVIOUS month's period for a few hours. An inequality would read that as a fresh period and
    // pay a month that is already paid. A monotonic guard pays each calendar month exactly once and never walks
    // backwards, whatever the setting does.
    if (PerksPeriodMonthIndex(session->GetAccountPerksCacheGrantPeriod()) >= PerksPeriodMonthIndex(periodStart))
        return;

    // Mark the period granted BEFORE crediting so the balance-persist stamps the new period, then persist again
    // explicitly to guarantee the flag is saved even if the credit itself was a no-op.
    session->SetAccountPerksCacheGrantPeriod(periodStart);
    player->AddCurrency(CURRENCY_TYPE_TRADERS_TENDER, PERKS_MONTHLY_CACHE_TENDER, CurrencyGainSource::Script);
    session->StoreAccountPerksTender(player->GetCurrencyQuantity(CURRENCY_TYPE_TRADERS_TENDER));
    // Announce it like every other Tender the server credits on its own initiative -- same rule as
    // PerksProgramActivityMgr::AwardThresholds. Without it the stipend appears in the balance with no event and
    // no toast, and PERKS_PROGRAM_CURRENCY_AWARDED is the only thing that produces one.
    session->SendPerksProgramTenderAwarded(PERKS_MONTHLY_CACHE_TENDER);
}

// Sends SMSG_PERKS_PROGRAM_DISABLED. The body is a single bit (family dispatcher RVA 0x67A010 case 6488070
// reads one byte and takes bit 7). The client's message handler (RVA 0x253D660) fires
// PERKS_PROGRAM_CLOSE unconditionally and PERKS_PROGRAM_DISABLED only when the bit is set, so this both
// closes the Trading Post window and, with disabled = true, shows the "disabled" popup.
//
// It is the ANSWER to a Trading Post request that the realm refuses, not a broadcast: no capture in the 51
// available recordings carries this opcode at all, so there is no observed retail send order to copy, and
// sending it unsolicited with disabled = false would close a window the player never opened.
void WorldSession::SendPerksProgramDisabled(bool disabled)
{
    WorldPackets::PerksProgram::PerksProgramDisabled packet;
    packet.Disabled = disabled;
    SendPacket(packet.Write());
}

// Sends SMSG_PERKS_PROGRAM_VENDOR_UPDATE -- but only when the listing this session was last given has actually
// been rebuilt since -- which in this tree means one thing only: the Trading Post rotation rollover in
// PerksProgramMgr::EnsureCurrent. (PerksProgramMgr::Reload would rebuild too, but it has no caller anywhere in
// src/, so it is not a live trigger.) The message is a full REPLACE,
// not an update: it enters the client's merge routine (client RVA 0x253DC00) with flag 0, which clears the
// vendor map before refilling it.
//
// That is why it must not be sent on the status poll. The client polls CMSG_PERKS_PROGRAM_STATUS_REQUEST every
// 6000 ms while the Trading Post frame is open (AGENT_BRIEF_perks_2A_63.md section 10.2, from the captures), so
// a listing on every poll would clear the map every six seconds -- and with it the rows merged in for purchases
// from earlier rotations, which HandlePerksProgramItemsRefreshed supplies as type 9 (flag 1, additive). The client
// does not re-request those on its own: it derives the gap list only in PerksProgramMgr::SetRecentPurchases
// (client RVA 0x253E020), which a VENDOR_UPDATE does not reach. The refund display consults exactly that map.
//
// It is also what retail does. SMSG_PERKS_PROGRAM_VENDOR_UPDATE appears 0 times in the 51 available captures
// across 20 builds, while CMSG_PERKS_PROGRAM_STATUS_REQUEST -> SMSG_PERKS_PROGRAM_ACTIVITY_UPDATE is 1:1 in all
// 14 12.0 builds (same section). The listing itself arrives at window open as RESULT type 5.
//
// UNVERIFIED: no capture contains a rotation rollover at all, so the shape of this push is reasoned from the
// consumer, not observed. With the DB2 data alone the rebuilt listing is usually identical anyway -- see
// PerksProgramMgr::EnsureCurrent.
void WorldSession::SendPerksProgramVendorRefresh()
{
    uint32 generation = sPerksProgramMgr->GetListingGeneration();

    // 0 means this session was never handed a listing at all -- the status request came from the Traveler's Log,
    // which has no vendor. Pushing one would open nothing and clear nothing that exists.
    if (!_perksVendorListingGeneration || _perksVendorListingGeneration == generation)
        return;

    _perksVendorListingGeneration = generation;

    WorldPackets::PerksProgram::PerksProgramVendorUpdate vendorUpdate;
    vendorUpdate.VendorItems = sPerksProgramMgr->GetCurrentVendorItems();
    SendPacket(vendorUpdate.Write());

    // The replace just dropped every row merged in for purchases outside the listing, and the client will not
    // ask for them again by itself. Re-sending the purchase list drives PerksProgramMgr::SetRecentPurchases
    // (client RVA 0x253E020) once more; that re-derives the unresolvable ids and re-sends
    // CMSG_PERKS_PROGRAM_ITEMS_REFRESHED, so the type 9 merge is redone against the new listing.
    WorldPackets::PerksProgram::ResponsePerkRecentPurchases purchases;
    purchases.Purchases = BuildPerksRecentPurchases();
    SendPacket(purchases.Write());
}

void WorldSession::HandlePerksProgramStatusRequest(WorldPackets::PerksProgram::PerksProgramStatusRequest& /*packet*/)
{
    // Realm has the Trading Post switched off: answer the status request with the disabled message instead of a
    // listing. Staying silent is not an option -- the client polls this every 6000 ms while the frame is open
    // (AGENT_BRIEF_perks_2A_63.md section 10.2) and would keep an empty window up forever.
    if (!IsPerksProgramEnabled())
    {
        SendPerksProgramDisabled(true);
        return;
    }

    if (Player* player = GetPlayer())
        GrantMonthlyPerksCache(this, player);

    SendPerksProgramVendorRefresh();
    SendPerksProgramActivityUpdate();
}

// CMSG_PERKS_PROGRAM_ITEMS_REFRESHED: the client lists PerksVendorItem ids it holds a purchase record for but
// has no vendor-item data for -- items bought in an earlier Trading Post rotation. It builds that list in
// PerksProgramMgr::SetRecentPurchases (client RVA 0x253E020) and only sends the message when the list is
// non-empty, so this is a targeted "fill these gaps", not a request to resend the listing.
//
// The answer therefore has to be SMSG_PERKS_PROGRAM_RESULT type 9, which MERGES into the client's vendor map
// (client RVA 0x253DC00 with flag 1) and additionally records the rows in the map that a later refund consults.
// Answering with SMSG_PERKS_PROGRAM_VENDOR_UPDATE, as this handler used to, enters the same routine with flag 0
// and CLEARS the map first -- it would delete data instead of supplying it, and it could not carry the wanted
// ids anyway because they are by definition outside the current listing.
//
// No monthly-cache grant here -- filling in display data is not an interaction that should award currency.
void WorldSession::HandlePerksProgramItemsRefreshed(WorldPackets::PerksProgram::PerksProgramItemsRefreshed& packet)
{
    if (!IsPerksProgramEnabled())
    {
        SendPerksProgramDisabled(true);
        return;
    }

    // Bound the answer. WorldSocket accepts a client packet up to 0x10000 bytes, so a crafted request can carry
    // ~16380 ids, and each resolvable one appends a 49-byte JamPerksVendorItem -- an unbounded amplification if
    // every id were answered. Two constraints cut it down, both taken from what the client actually does:
    //   * ids repeat at most once. The client builds the array by walking its purchase map, whose keys are
    //     unique, so a repeated id can only come from a forged packet. Same rule as the cart handler, which
    //     rejects duplicates outright; here skipping is enough because nothing is charged.
    //   * ids come from the account's OWN purchase records. PerksProgramMgr::SetRecentPurchases (client RVA
    //     0x253E020) collects exactly the purchase-list entries it cannot resolve locally, and that purchase
    //     list is the one this server sent. Anything outside it is not a gap the client can have.
    // Together these cap the response at one element per stored purchase, independent of the request size.
    std::unordered_map<int32, PerksProgramPurchaseData> const& purchases = GetCollectionMgr()->GetPerksProgramPurchases();

    WorldPackets::PerksProgram::PerksProgramResult result(WorldPackets::PerksProgram::PerksProgramResult::ResultTypeVendorMerge);
    std::unordered_set<int32> seen;
    for (int32 vendorItemId : packet.RequestedVendorItemIDs)
    {
        if (!seen.insert(vendorItemId).second)
            continue;

        if (!purchases.contains(vendorItemId))
            continue;

        if (WorldPackets::PerksProgram::PerksVendorItem const* item = sPerksProgramMgr->GetCatalogueVendorItem(vendorItemId))
            result.VendorItems.push_back(*item);
    }

    // Nothing resolvable means every requested id is unknown to the server too. The client imposes no timeout
    // on this request, so staying silent is correct here -- an empty merge would only trigger a pointless
    // PERKS_PROGRAM_DATA_REFRESH.
    if (!result.VendorItems.empty())
        SendPacket(result.Write());
}

// Sends SMSG_PERKS_PROGRAM_ACTIVITY_UPDATE: the current Trading Post period plus the player's
// completed activities for it. Both parts are filled: the period comes from PerksProgramMgr, the completed
// set from PerksProgramActivityMgr, which derives completion from each PerksActivity's CriteriaTree and
// awards the threshold tender. The period alone already drives the client's activity countdown, so this is
// also sent while the completed set is still empty.
void WorldSession::SendPerksProgramActivityUpdate()
{
    WorldPackets::PerksProgram::PerksProgramActivityUpdate activityUpdate;
    sPerksProgramMgr->GetCurrentPeriod(activityUpdate.PeriodStart, activityUpdate.PeriodEnd);

    if (Player* player = GetPlayer())
    {
        std::unordered_set<uint32> const& completed = player->GetPerksActivityMgr()->GetCompletedActivities();
        activityUpdate.CompletedActivityIDs.assign(completed.begin(), completed.end());
    }

    SendPacket(activityUpdate.Write());
}

// Sends SMSG_PERKS_ANIM_TOGGLE_KILL_SWITCH: the two client-side feature switches behind
// C_PerksProgram.IsAttackAnimToggleEnabled() and C_PerksProgram.IsMountSpecialAnimToggleEnabled(). These gate
// the "hide/show weapon attack animation" and "mount special animation" toggles the Trading Post rewards use;
// they are a server-side kill switch, not per-character state, and every 12.0.7 capture (68275, 68453 and 68974
// alike) carries the same enabled/enabled byte. Sent from the login burst so the toggles exist before any UI
// that reads them is built.
void WorldSession::SendPerksAnimToggleKillSwitch()
{
    WorldPackets::PerksProgram::PerksAnimToggleKillSwitch killSwitch;
    killSwitch.AttackAnimToggleEnabled = true;
    killSwitch.MountSpecialAnimToggleEnabled = true;
    SendPacket(killSwitch.Write());
}

// CMSG_PERKS_PROGRAM_REQUEST_PENDING_REWARDS: the client asking for Trading Post rewards the account has EARNED
// but that have NOT been handed out yet -- it draws them as the glowing, uncollected Traveler's Log chest and as
// the "uncollected Tender" line on the currency tooltip.
//
// On this server that set is always empty, and empty is the correct answer rather than a placeholder:
// PerksProgramActivityMgr::AwardThresholds credits the Trader's Tender for a threshold in the same call that
// crosses it (Player::AddCurrency, once per threshold), and LoadFromDB only ever restores thresholds that were
// already paid, so no reward is ever left owing. Reporting one anyway would be worse than saying nothing --
// the 12.0.7 client has no "claim" opcode at all (the chest is a texture, not a button), so a pending entry
// the server invented could never be cleared and would leave the chest glowing forever. Retail sends this same
// four-byte Count = 0 body for a fully-collected account in the 68453 and 68974 captures.
//
// The response writer models the full earned-threshold record, so a future deferred-payout model only has to
// fill Rewards in here.
void WorldSession::HandlePerksProgramRequestPendingRewards(WorldPackets::PerksProgram::PerksProgramRequestPendingRewards& /*packet*/)
{
    if (!IsPerksProgramEnabled())
    {
        SendPerksProgramDisabled(true);
        return;
    }

    WorldPackets::PerksProgram::ResponsePerkPendingRewards response;
    SendPacket(response.Write());
}

// PerksVendorItem::MountID is a Mount.db2 row id -- that is what the client's mount journal calls expect and
// what PerksProgramMgr::BuildVendorList therefore writes. The account collection speaks the other id space:
// CollectionMgr::AddMount, GetAccountMounts, RemoveMount and the stored PerksProgramPurchaseData::MountID are all
// keyed by the TEACHING SPELL. This is the single translation point between the two; it returns 0 for "no mount"
// and for a mount row that teaches nothing.
static uint32 GetPerksMountSpell(int32 mountId)
{
    if (mountId <= 0)
        return 0;

    MountEntry const* mount = sDB2Manager.GetMountById(uint32(mountId));
    return mount && mount->SourceSpellID > 0 ? uint32(mount->SourceSpellID) : 0;
}

// Resolves an offered, grantable Trading Post vendor item WITHOUT charging or granting. Returns nullptr if the
// item is not currently offered, is disabled, has an invalid price, or resolves to no reward the server can grant
// (a battle pet / illusion / transmog set / warband scene, which BuildVendorList does not yet resolve -- see G2).
static WorldPackets::PerksProgram::PerksVendorItem const* ResolvePerksPurchase(int32 vendorItemId)
{
    WorldPackets::PerksProgram::PerksVendorItem const* item = sPerksProgramMgr->GetVendorItem(vendorItemId);
    if (!item || item->Disabled || item->Price < 0)
        return nullptr;

    // A mount only counts as grantable when its row still resolves to a teaching spell -- otherwise the grant
    // below would charge Trader's Tender and hand out nothing.
    if (!GetPerksMountSpell(item->MountID) && !item->ToyID && !item->ItemModifiedAppearanceID)
        return nullptr;

    return item;
}

// Whether the resolved reward is already known to the account. Retail hides/blocks owned items; buying one again
// would burn Trader's Tender and stack a redundant purchase record, so reject it (G11).
static bool IsPerksRewardOwned(CollectionMgr* collectionMgr, WorldPackets::PerksProgram::PerksVendorItem const* item)
{
    if (uint32 mountSpell = GetPerksMountSpell(item->MountID); mountSpell && collectionMgr->GetAccountMounts().contains(mountSpell))
        return true;
    if (item->ToyID && collectionMgr->HasToy(uint32(item->ToyID)))
        return true;
    if (item->ItemModifiedAppearanceID && collectionMgr->HasItemAppearance(uint32(item->ItemModifiedAppearanceID)).first)
        return true;
    return false;
}

// Grants the resolved collectible and records the purchase. Does NOT charge -- the caller deducts the price (a
// single purchase deducts one item; a cart deducts the pre-summed total once). A vendor item resolves to exactly
// one collectible.
static void GrantPerksPurchase(WorldSession* session, Player* player, int32 vendorItemId, WorldPackets::PerksProgram::PerksVendorItem const* item)
{
    CollectionMgr* collectionMgr = session->GetCollectionMgr();
    uint32 const mountSpell = GetPerksMountSpell(item->MountID);
    if (mountSpell)
        collectionMgr->AddMount(mountSpell, MOUNT_STATUS_NONE);
    if (item->ToyID)
        collectionMgr->AddToy(uint32(item->ToyID), false, false);
    if (item->ItemModifiedAppearanceID)
        if (ItemModifiedAppearanceEntry const* appearance = sItemModifiedAppearanceStore.LookupEntry(uint32(item->ItemModifiedAppearanceID)))
            collectionMgr->AddItemAppearance(appearance->ItemID, appearance->ItemAppearanceModifierID);

    // Record the purchase so it can later be refunded (price paid + the exact collectible to revoke + the
    // purchasing character, so only that character can refund it).
    collectionMgr->AddPerksProgramPurchase(vendorItemId, item->Price, int32(mountSpell), item->ToyID, player->GetGUID().GetCounter());
}

// Validates a single Trading Post vendor item, deducts its Trader's Tender cost and grants the resolved
// collectible. Returns false (leaving the player untouched) if the item is not currently offered/grantable or the
// player cannot afford it.
static bool PerksProgramPurchaseItem(WorldSession* session, Player* player, int32 vendorItemId)
{
    WorldPackets::PerksProgram::PerksVendorItem const* item = ResolvePerksPurchase(vendorItemId);
    if (!item)
        return false;

    if (IsPerksRewardOwned(session->GetCollectionMgr(), item))
        return false;

    if (!player->HasCurrency(CURRENCY_TYPE_TRADERS_TENDER, uint32(item->Price)))
        return false;

    player->RemoveCurrency(CURRENCY_TYPE_TRADERS_TENDER, item->Price, CurrencyDestroyReason::Vendor);
    GrantPerksPurchase(session, player, vendorItemId, item);
    return true;
}

// Builds the account's complete current Trading Post purchase list. Both SMSG_RESPONSE_PERK_RECENT_PURCHASES
// and the type 2 branch of SMSG_PERKS_PROGRAM_RESULT carry exactly this list, and the type 2 branch REPLACES the
// client's whole purchase map (client RVA 0x253E020), so it has to be complete rather than a delta.
std::vector<WorldPackets::PerksProgram::PerksRecentPurchase> WorldSession::BuildPerksRecentPurchases() const
{
    std::vector<WorldPackets::PerksProgram::PerksRecentPurchase> purchases;

    Player const* player = _player;
    uint64 playerGuid = player ? player->GetGUID().GetCounter() : 0;

    for (auto const& [vendorItemId, data] : _collectionMgr->GetPerksProgramPurchases())
    {
        WorldPackets::PerksProgram::PerksRecentPurchase& entry = purchases.emplace_back();
        entry.PerksVendorItemID = vendorItemId;
        entry.PurchaseTime = data.PurchaseTime;
        // A purchase is refundable only for the character that made it (the refund handler enforces the same
        // buyer scope) and while its reward is cleanly revocable (a mount or toy); appearance/transmog rewards
        // are append-only in the account collection, matching the refund handler's policy.
        entry.Refundable = (data.MountID != 0 || data.ToyID != 0) && data.BuyerGuid == playerGuid;
    }

    return purchases;
}

// Sends SMSG_PERKS_PROGRAM_RESULT type 5 -- the message that actually opens the Trading Post. It starts
// PlayerInteractionType::PerksProgramVendor (57) on the client, loads the vendor listing as a full replace and
// fires PERKS_PROGRAM_OPEN, which is the sole caller of ShowPerksProgramFrame(). Without it the window can
// never appear, no matter what else the server sends.
//
// Retail pushes it unprompted right after CMSG_GOSSIP_SELECT_OPTION, before the client asks for anything --
// which is also what the UI needs: PerksProgramMixin:OnLoad reads the categories synchronously while the frame
// builds, and a later PERKS_PROGRAM_DATA_REFRESH does not rebuild the category filter.
void WorldSession::SendPerksProgramVendorOpen(ObjectGuid const& vendorGuid)
{
    if (!IsPerksProgramEnabled())
    {
        SendPerksProgramDisabled(true);
        return;
    }

    WorldPackets::PerksProgram::PerksProgramResult result(WorldPackets::PerksProgram::PerksProgramResult::ResultTypeVendorOpen);
    result.VendorGUID = vendorGuid;
    // UNVERIFIED: the second guid feeds a model/scene setup in the client (RVA 0x253D6C0). Both captures carry
    // two DIFFERENT creature guids here, so it is not simply the vendor repeated, but nothing names the second
    // one. Sending the vendor guid keeps it a creature the client already has loaded.
    result.DisplayGUID = vendorGuid;
    result.VendorItems = sPerksProgramMgr->GetCurrentVendorItems();
    SendPacket(result.Write());

    // Type 5 is a full replace too, so this session now holds the listing as it stands. Stamping it here is what
    // keeps the status poll silent until the listing is actually rebuilt.
    _perksVendorListingGeneration = sPerksProgramMgr->GetListingGeneration();
}

// Sends SMSG_PERKS_PROGRAM_RESULT type 2 (purchase) or 3 (refund). Answering is not optional: without it the
// client spins for 10 seconds, raises PERKS_PROGRAM_SLOW_PURCHASE and then leaves the item marked
// isPurchasePending forever; the refund path escalates to a global error state after 45 seconds.
void WorldSession::SendPerksProgramPurchaseResult(int32 perksVendorItemId, bool refund)
{
    using WorldPackets::PerksProgram::PerksProgramResult;

    PerksProgramResult result(refund ? PerksProgramResult::ResultTypeRefundSuccess : PerksProgramResult::ResultTypePurchaseSuccess);
    result.PerksVendorItemID = perksVendorItemId;
    // Type 3 reads this array and discards it, but it still has to be on the wire; type 2 installs it as the
    // client's complete purchase map.
    result.Purchases = BuildPerksRecentPurchases();
    SendPacket(result.Write());
}

// Sends SMSG_PERKS_PROGRAM_RESULT type 8 -- "the server just credited you this much Trader's Tender".
// It is the announcement half of a credit the server makes on its own initiative (a monthly-activity threshold),
// as opposed to a purchase or refund, which carry their own type 2 / type 3 answer.
//
// D2, the consumer chain: client RVA 0x253CFE0 case 6 reads the amount at +240 and, only when it is > 0, fires
// event slot 12872 with hash 0xCC938319765BD2A9 = PERKS_PROGRAM_CURRENCY_AWARDED. Its documented payload is a
// single number "value" (PerksProgramDocumentation.lua:269-278). AlertFrames.lua:499 registers the event and
// :624-629 turns it into LootAlertSystem:AddAlert with lootSource = LOOT_SOURCE_TRADING_POST -- the only place
// in the UI that produces that toast.
//
// Err stays 0 and must: type 8 with Err != 0 indexes past the end of the client's 8-row GameError table.
// A zero or negative amount is dropped rather than sent, because the consumer would discard it anyway.
void WorldSession::SendPerksProgramTenderAwarded(int32 amount)
{
    if (amount <= 0)
        return;

    WorldPackets::PerksProgram::PerksProgramResult result(WorldPackets::PerksProgram::PerksProgramResult::ResultTypeTenderGranted);
    result.TenderAwarded = amount;
    SendPacket(result.Write());
}

// Sends a refused Trading Post request back as an empty error carrier. Type 0 with Err 1 resolves to
// ERR_CANT_DO_THAT_RIGHT_NOW through the client's GameError table and then raises PERKS_PROGRAM_RESULT_ERROR,
// which puts the frame into its server-error state: the buy and refund controls grey out
// (Blizzard_PerksProgramElements.lua:805) until the player reopens the window.
//
// What it does NOT do is cancel the client's pending-purchase timer -- CancelPurchaseTimer() runs only on
// PERKS_PROGRAM_PURCHASE_SUCCESS and _REFUND_SUCCESS (Blizzard_PerksProgram.lua:265-270), so a refused purchase
// still raises the PERKS_PROGRAM_SLOW_PURCHASE popup after 10 seconds. The protocol offers no way to retract a
// purchase cleanly; the only alternative would be to claim success, which would be a lie. An error is the best
// honest answer available, and it is strictly better than silence.
//
// The type is deliberately fixed to 0: Err != 0 combined with type 8 or 9 makes the client read past the end of
// that GameError table and crash, so the unsafe combination is not expressible here.
void WorldSession::SendPerksProgramResultError()
{
    WorldPackets::PerksProgram::PerksProgramResult result(WorldPackets::PerksProgram::PerksProgramResult::ResultTypeError);
    result.Err = WorldPackets::PerksProgram::PerksProgramResult::ResultErrorCantDoThat;
    SendPacket(result.Write());
}

void WorldSession::HandlePerksProgramGetRecentPurchases(WorldPackets::PerksProgram::PerksProgramGetRecentPurchases& /*packet*/)
{
    if (!IsPerksProgramEnabled())
    {
        SendPerksProgramDisabled(true);
        return;
    }

    WorldPackets::PerksProgram::ResponsePerkRecentPurchases response;
    response.Purchases = BuildPerksRecentPurchases();
    SendPacket(response.Write());
}

// Refunds a Trading Post purchase: revokes the granted collectible and returns the Trader's Tender that was paid.
// A refund is only honoured when we have a purchase record (so a collectible obtained elsewhere cannot be
// "refunded") and when the reward is cleanly revocable. Appearance/transmog rewards are append-only in the
// account collection and therefore stay non-refundable rather than returning currency while keeping the look.
//
// Every refusal below answers with SMSG_PERKS_PROGRAM_RESULT type 0 + Err rather than falling silent: the
// client gives an unanswered refund 45 seconds before it drops into a global error state that only reopening
// the frame clears, so silence is the worst of the available outcomes.
void WorldSession::HandlePerksProgramRequestRefund(WorldPackets::PerksProgram::PerksProgramRequestRefund& packet)
{
    if (!IsPerksProgramEnabled())
    {
        SendPerksProgramDisabled(true);
        return;
    }

    Player* player = GetPlayer();
    if (!player)
        return;

    if (!HasActivePerksProgramVendor(player, packet.VendorGUID))
    {
        SendPerksProgramResultError();
        return;
    }

    CollectionMgr* collectionMgr = GetCollectionMgr();
    PerksProgramPurchaseData const* purchase = collectionMgr->GetPerksProgramPurchase(packet.PerksVendorItemID);
    if (!purchase)
    {
        SendPerksProgramResultError();
        return;
    }

    // A refund is only honoured for the character that made the purchase. Trader's Tender is account-wide, so a
    // cross-character refund could not duplicate currency anyway, but scoping the refund to the original buyer
    // matches the client's per-character "recent purchases" list and blocks refunding another character's record.
    if (purchase->BuyerGuid != player->GetGUID().GetCounter())
    {
        SendPerksProgramResultError();
        return;
    }

    // Enforce the retail 2-hour refund window server-side (the client only shows the countdown). A crafted refund
    // packet, or a record that has outlived the window, is rejected here. The revocable reward types (mount/toy
    // account-collection entries) have no separate "used/consumed" state to gate on beyond ownership, which the
    // confirmed-revoke check below already covers; appearances are non-refundable by policy.
    if (GameTime::GetGameTime() - time_t(purchase->PurchaseTime) > 2 * HOUR)
    {
        SendPerksProgramResultError();
        return;
    }

    // Revoke the reward and ONLY credit Trader's Tender when the collectible was actually removed. Gating the
    // credit on a confirmed revoke is what prevents creating currency by "refunding" a collectible that is already
    // gone (double-refund, or removed by another path). Only mounts and toys are cleanly revocable.
    bool revoked = false;
    if (purchase->MountID)
        revoked = collectionMgr->RemoveMount(uint32(purchase->MountID));
    else if (purchase->ToyID)
        revoked = collectionMgr->RemoveToy(uint32(purchase->ToyID));

    if (!revoked)
    {
        SendPerksProgramResultError();
        return;
    }

    if (purchase->Price > 0)
        player->AddCurrency(CURRENCY_TYPE_TRADERS_TENDER, uint32(purchase->Price), CurrencyGainSource::ItemRefund);

    collectionMgr->RemovePerksProgramPurchase(packet.PerksVendorItemID);

    // Answer AFTER the record is gone so the purchase list in the message is the post-refund state.
    SendPerksProgramPurchaseResult(packet.PerksVendorItemID, true);
}

void WorldSession::HandlePerksProgramRequestPurchase(WorldPackets::PerksProgram::PerksProgramRequestPurchase& packet)
{
    if (!IsPerksProgramEnabled())
    {
        SendPerksProgramDisabled(true);
        return;
    }

    Player* player = GetPlayer();
    if (!player)
        return;

    if (!HasActivePerksProgramVendor(player, packet.VendorGUID))
    {
        SendPerksProgramResultError();
        return;
    }

    // An unanswered purchase leaves the item marked isPurchasePending in the client for the rest of the
    // session, so both outcomes have to go back on the wire.
    if (PerksProgramPurchaseItem(this, player, packet.PerksVendorItemID))
        SendPerksProgramPurchaseResult(packet.PerksVendorItemID, false);
    else
        SendPerksProgramResultError();
}

void WorldSession::HandlePerksProgramRequestCartCheckout(WorldPackets::PerksProgram::PerksProgramRequestCartCheckout& packet)
{
    if (!IsPerksProgramEnabled())
    {
        SendPerksProgramDisabled(true);
        return;
    }

    Player* player = GetPlayer();
    if (!player)
        return;

    if (!HasActivePerksProgramVendor(player, packet.VendorGUID))
    {
        SendPerksProgramResultError();
        return;
    }

    // Atomic checkout: resolve + validate every item and sum the total up front. If any entry is invalid or
    // duplicated, or the player cannot afford the full total, reject the whole cart -- no per-item silent skip,
    // no partial charge. Only once everything is validated and affordable do we deduct the total once and grant.
    std::vector<std::pair<int32, WorldPackets::PerksProgram::PerksVendorItem const*>> resolved;
    resolved.reserve(packet.PerksVendorItemIDs.size());
    std::unordered_set<int32> seen;
    int64 total = 0;
    for (int32 vendorItemId : packet.PerksVendorItemIDs)
    {
        if (!seen.insert(vendorItemId).second)
        {
            SendPerksProgramResultError();
            return; // duplicate id in the cart -> reject the whole checkout
        }

        WorldPackets::PerksProgram::PerksVendorItem const* item = ResolvePerksPurchase(vendorItemId);
        if (!item)
        {
            SendPerksProgramResultError();
            return;
        }

        if (IsPerksRewardOwned(GetCollectionMgr(), item))
        {
            SendPerksProgramResultError();
            return; // already-owned item in the cart -> reject the whole checkout (no charge)
        }

        total += item->Price;
        resolved.emplace_back(vendorItemId, item);
    }

    if (total < 0 || total > int64(std::numeric_limits<int32>::max()) || !player->HasCurrency(CURRENCY_TYPE_TRADERS_TENDER, uint32(total)))
    {
        SendPerksProgramResultError();
        return;
    }

    if (total > 0)
        player->RemoveCurrency(CURRENCY_TYPE_TRADERS_TENDER, int32(total), CurrencyDestroyReason::Vendor);

    for (auto const& [vendorItemId, item] : resolved)
        GrantPerksPurchase(this, player, vendorItemId, item);

    // isPurchasePending is tracked per item and only PERKS_PROGRAM_PURCHASE_SUCCESS(vendorItemID) clears it,
    // so a cart needs one answer per item, not one for the cart. Sent after every grant so each message
    // carries the final purchase list.
    for (auto const& [vendorItemId, item] : resolved)
        SendPerksProgramPurchaseResult(vendorItemId, false);
}

void WorldSession::HandlePerksProgramSetFrozenVendorItem(WorldPackets::PerksProgram::PerksProgramSetFrozenVendorItem& packet)
{
    if (!IsPerksProgramEnabled())
    {
        SendPerksProgramDisabled(true);
        return;
    }

    Player* player = GetPlayer();
    if (!player)
        return;

    if (!HasActivePerksProgramVendor(player, packet.NpcGUID))
        return;

    // Freeze pins the chosen Trading Post item so it carries to next rotation (client shows the frozen indicator);
    // unfreeze clears it. An unknown item id resolves to nullptr, which clears the pin -- a safe no-op.
    if (packet.Frozen)
        player->SetFrozenPerksProgramVendorItem(sPerksProgramMgr->GetVendorItem(packet.PerksVendorItemID));
    else
        player->SetFrozenPerksProgramVendorItem(nullptr);
}
