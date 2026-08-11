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
#include <limits>
#include <unordered_set>
#include <utility>
#include <vector>

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

void WorldSession::HandlePerksProgramStatusRequest(WorldPackets::PerksProgram::PerksProgramStatusRequest& /*packet*/)
{
    WorldPackets::PerksProgram::PerksProgramVendorUpdate vendorUpdate;
    vendorUpdate.VendorItems = sPerksProgramMgr->GetCurrentVendorItems();
    SendPacket(vendorUpdate.Write());

    SendPerksProgramActivityUpdate();
}

// Sends SMSG_PERKS_PROGRAM_ACTIVITY_UPDATE: the current Trading Post period plus the player's
// completed activities for it. Completed-activity tracking (deriving completion from each
// PerksActivity's CriteriaTree and awarding threshold tender) is a separate phase, so today the
// completed set is empty — the client still needs the period to show the activity countdown.
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

// Resolves an offered, grantable Trading Post vendor item WITHOUT charging or granting. Returns nullptr if the
// item is not currently offered, is disabled, has an invalid price, or resolves to no reward the server can grant
// (a battle pet / illusion / transmog set / warband scene, which BuildVendorList does not yet resolve -- see G2).
static WorldPackets::PerksProgram::PerksVendorItem const* ResolvePerksPurchase(int32 vendorItemId)
{
    WorldPackets::PerksProgram::PerksVendorItem const* item = sPerksProgramMgr->GetVendorItem(vendorItemId);
    if (!item || item->Disabled || item->Price < 0)
        return nullptr;

    if (!item->MountID && !item->ToyID && !item->ItemModifiedAppearanceID)
        return nullptr;

    return item;
}

// Grants the resolved collectible and records the purchase. Does NOT charge -- the caller deducts the price (a
// single purchase deducts one item; a cart deducts the pre-summed total once). A vendor item resolves to exactly
// one collectible.
static void GrantPerksPurchase(WorldSession* session, Player* player, int32 vendorItemId, WorldPackets::PerksProgram::PerksVendorItem const* item)
{
    CollectionMgr* collectionMgr = session->GetCollectionMgr();
    if (item->MountID)
        collectionMgr->AddMount(uint32(item->MountID), MOUNT_STATUS_NONE);
    if (item->ToyID)
        collectionMgr->AddToy(uint32(item->ToyID), false, false);
    if (item->ItemModifiedAppearanceID)
        if (ItemModifiedAppearanceEntry const* appearance = sItemModifiedAppearanceStore.LookupEntry(uint32(item->ItemModifiedAppearanceID)))
            collectionMgr->AddItemAppearance(appearance->ItemID, appearance->ItemAppearanceModifierID);

    // Record the purchase so it can later be refunded (price paid + the exact collectible to revoke + the
    // purchasing character, so only that character can refund it).
    collectionMgr->AddPerksProgramPurchase(vendorItemId, item->Price, item->MountID, item->ToyID, player->GetGUID().GetCounter());
}

// Validates a single Trading Post vendor item, deducts its Trader's Tender cost and grants the resolved
// collectible. Returns false (leaving the player untouched) if the item is not currently offered/grantable or the
// player cannot afford it.
static bool PerksProgramPurchaseItem(WorldSession* session, Player* player, int32 vendorItemId)
{
    WorldPackets::PerksProgram::PerksVendorItem const* item = ResolvePerksPurchase(vendorItemId);
    if (!item)
        return false;

    if (!player->HasCurrency(CURRENCY_TYPE_TRADERS_TENDER, uint32(item->Price)))
        return false;

    player->RemoveCurrency(CURRENCY_TYPE_TRADERS_TENDER, item->Price, CurrencyDestroyReason::Vendor);
    GrantPerksPurchase(session, player, vendorItemId, item);
    return true;
}

// Refunds a Trading Post purchase: revokes the granted collectible and returns the Trader's Tender that was paid.
// A refund is only honoured when we have a purchase record (so a collectible obtained elsewhere cannot be
// "refunded") and when the reward is cleanly revocable. Appearance/transmog rewards are append-only in the
// account collection and therefore stay non-refundable rather than returning currency while keeping the look.
void WorldSession::HandlePerksProgramGetRecentPurchases(WorldPackets::PerksProgram::PerksProgramGetRecentPurchases& /*packet*/)
{
    CollectionMgr* collectionMgr = GetCollectionMgr();
    Player* player = GetPlayer();
    uint64 playerGuid = player ? player->GetGUID().GetCounter() : 0;

    WorldPackets::PerksProgram::ResponsePerkRecentPurchases response;
    for (auto const& [vendorItemId, data] : collectionMgr->GetPerksProgramPurchases())
    {
        WorldPackets::PerksProgram::ResponsePerkRecentPurchases::RecentPurchase& entry = response.Purchases.emplace_back();
        entry.PerksVendorItemID = vendorItemId;
        entry.PurchaseTime = data.PurchaseTime;
        // A purchase is refundable only for the character that made it (the refund handler enforces the same
        // buyer scope) and while its reward is cleanly revocable (a mount or toy); appearance/transmog rewards
        // are append-only in the account collection, matching the refund handler's policy.
        entry.Refundable = (data.MountID != 0 || data.ToyID != 0) && data.BuyerGuid == playerGuid;
    }

    SendPacket(response.Write());
}

void WorldSession::HandlePerksProgramRequestRefund(WorldPackets::PerksProgram::PerksProgramRequestRefund& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    if (!HasActivePerksProgramVendor(player, packet.VendorGUID))
        return;

    CollectionMgr* collectionMgr = GetCollectionMgr();
    PerksProgramPurchaseData const* purchase = collectionMgr->GetPerksProgramPurchase(packet.PerksVendorItemID);
    if (!purchase)
        return;

    // A refund is only honoured for the character that made the purchase. Trader's Tender is account-wide, so a
    // cross-character refund could not duplicate currency anyway, but scoping the refund to the original buyer
    // matches the client's per-character "recent purchases" list and blocks refunding another character's record.
    if (purchase->BuyerGuid != player->GetGUID().GetCounter())
        return;

    // Enforce the retail 2-hour refund window server-side (the client only shows the countdown). A crafted refund
    // packet, or a record that has outlived the window, is rejected here. The revocable reward types (mount/toy
    // account-collection entries) have no separate "used/consumed" state to gate on beyond ownership, which the
    // confirmed-revoke check below already covers; appearances are non-refundable by policy.
    if (GameTime::GetGameTime() - time_t(purchase->PurchaseTime) > 2 * HOUR)
        return;

    // Revoke the reward and ONLY credit Trader's Tender when the collectible was actually removed. Gating the
    // credit on a confirmed revoke is what prevents creating currency by "refunding" a collectible that is already
    // gone (double-refund, or removed by another path). Only mounts and toys are cleanly revocable.
    bool revoked = false;
    if (purchase->MountID)
        revoked = collectionMgr->RemoveMount(uint32(purchase->MountID));
    else if (purchase->ToyID)
        revoked = collectionMgr->RemoveToy(uint32(purchase->ToyID));

    if (!revoked)
        return;

    if (purchase->Price > 0)
        player->AddCurrency(CURRENCY_TYPE_TRADERS_TENDER, uint32(purchase->Price), CurrencyGainSource::ItemRefund);

    collectionMgr->RemovePerksProgramPurchase(packet.PerksVendorItemID);
}

void WorldSession::HandlePerksProgramRequestPurchase(WorldPackets::PerksProgram::PerksProgramRequestPurchase& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    if (!HasActivePerksProgramVendor(player, packet.VendorGUID))
        return;

    PerksProgramPurchaseItem(this, player, packet.PerksVendorItemID);
}

void WorldSession::HandlePerksProgramRequestCartCheckout(WorldPackets::PerksProgram::PerksProgramRequestCartCheckout& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    if (!HasActivePerksProgramVendor(player, packet.VendorGUID))
        return;

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
            return; // duplicate id in the cart -> reject the whole checkout

        WorldPackets::PerksProgram::PerksVendorItem const* item = ResolvePerksPurchase(vendorItemId);
        if (!item)
            return;

        total += item->Price;
        resolved.emplace_back(vendorItemId, item);
    }

    if (total < 0 || total > int64(std::numeric_limits<int32>::max()) || !player->HasCurrency(CURRENCY_TYPE_TRADERS_TENDER, uint32(total)))
        return;

    if (total > 0)
        player->RemoveCurrency(CURRENCY_TYPE_TRADERS_TENDER, int32(total), CurrencyDestroyReason::Vendor);

    for (auto const& [vendorItemId, item] : resolved)
        GrantPerksPurchase(this, player, vendorItemId, item);
}

void WorldSession::HandlePerksProgramSetFrozenVendorItem(WorldPackets::PerksProgram::PerksProgramSetFrozenVendorItem& packet)
{
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
