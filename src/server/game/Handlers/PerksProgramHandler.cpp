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
#include "Player.h"
#include "PerksProgramActivityMgr.h"
#include "PerksProgramMgr.h"
#include "PerksProgramPackets.h"

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

// Validates a single Trading Post vendor item, deducts its Trader's Tender cost and grants the
// resolved collectible. Returns false (leaving the player untouched) if the item is not currently
// offered or the player cannot afford it.
static bool PerksProgramPurchaseItem(WorldSession* session, Player* player, int32 vendorItemId)
{
    WorldPackets::PerksProgram::PerksVendorItem const* item = sPerksProgramMgr->GetVendorItem(vendorItemId);
    if (!item || item->Disabled)
        return false;

    if (item->Price < 0 || !player->HasCurrency(CURRENCY_TYPE_TRADERS_TENDER, uint32(item->Price)))
        return false;

    player->RemoveCurrency(CURRENCY_TYPE_TRADERS_TENDER, item->Price, CurrencyDestroyReason::Vendor);

    // Grant the resolved collectible. A vendor item resolves to exactly one of these.
    CollectionMgr* collectionMgr = session->GetCollectionMgr();
    if (item->MountID)
        collectionMgr->AddMount(uint32(item->MountID), MOUNT_STATUS_NONE);
    if (item->ToyID)
        collectionMgr->AddToy(uint32(item->ToyID), false, false);
    if (item->ItemModifiedAppearanceID)
        if (ItemModifiedAppearanceEntry const* appearance = sItemModifiedAppearanceStore.LookupEntry(uint32(item->ItemModifiedAppearanceID)))
            collectionMgr->AddItemAppearance(appearance->ItemID, appearance->ItemAppearanceModifierID);

    // Record the purchase so it can later be refunded (price paid + the exact collectible to revoke).
    collectionMgr->AddPerksProgramPurchase(vendorItemId, item->Price, item->MountID, item->ToyID);

    return true;
}

// Refunds a Trading Post purchase: revokes the granted collectible and returns the Trader's Tender that was paid.
// A refund is only honoured when we have a purchase record (so a collectible obtained elsewhere cannot be
// "refunded") and when the reward is cleanly revocable. Appearance/transmog rewards are append-only in the
// account collection and therefore stay non-refundable rather than returning currency while keeping the look.
void WorldSession::HandlePerksProgramGetRecentPurchases(WorldPackets::PerksProgram::PerksProgramGetRecentPurchases& /*packet*/)
{
    CollectionMgr* collectionMgr = GetCollectionMgr();

    WorldPackets::PerksProgram::ResponsePerkRecentPurchases response;
    for (auto const& [vendorItemId, data] : collectionMgr->GetPerksProgramPurchases())
    {
        WorldPackets::PerksProgram::ResponsePerkRecentPurchases::RecentPurchase& entry = response.Purchases.emplace_back();
        entry.PerksVendorItemID = vendorItemId;
        entry.PurchaseTime = data.PurchaseTime;
        // A purchase is refundable while its reward is cleanly revocable (a mount or toy); appearance/transmog
        // rewards are append-only in the account collection, matching the refund handler's policy.
        entry.Refundable = (data.MountID != 0 || data.ToyID != 0);
    }

    SendPacket(response.Write());
}

void WorldSession::HandlePerksProgramRequestRefund(WorldPackets::PerksProgram::PerksProgramRequestRefund& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    CollectionMgr* collectionMgr = GetCollectionMgr();
    PerksProgramPurchaseData const* purchase = collectionMgr->GetPerksProgramPurchase(packet.PerksVendorItemID);
    if (!purchase)
        return;

    // Revoke the reward. Only mounts and toys can be cleanly removed; anything else is not refundable.
    if (purchase->MountID)
        collectionMgr->RemoveMount(uint32(purchase->MountID));
    else if (purchase->ToyID)
        collectionMgr->RemoveToy(uint32(purchase->ToyID));
    else
        return;

    if (purchase->Price > 0)
        player->AddCurrency(CURRENCY_TYPE_TRADERS_TENDER, uint32(purchase->Price), CurrencyGainSource::ItemRefund);

    collectionMgr->RemovePerksProgramPurchase(packet.PerksVendorItemID);
}

void WorldSession::HandlePerksProgramRequestPurchase(WorldPackets::PerksProgram::PerksProgramRequestPurchase& packet)
{
    if (Player* player = GetPlayer())
        PerksProgramPurchaseItem(this, player, packet.PerksVendorItemID);
}

void WorldSession::HandlePerksProgramRequestCartCheckout(WorldPackets::PerksProgram::PerksProgramRequestCartCheckout& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Each item is validated + charged independently; an unaffordable entry is simply skipped so the
    // rest of the cart still goes through (mirrors buying them one by one).
    for (int32 vendorItemId : packet.PerksVendorItemIDs)
        PerksProgramPurchaseItem(this, player, vendorItemId);
}

void WorldSession::HandlePerksProgramSetFrozenVendorItem(WorldPackets::PerksProgram::PerksProgramSetFrozenVendorItem& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Freeze pins the chosen Trading Post item so it carries to next rotation (client shows the frozen indicator);
    // unfreeze clears it. An unknown item id resolves to nullptr, which clears the pin -- a safe no-op.
    if (packet.Frozen)
        player->SetFrozenPerksProgramVendorItem(sPerksProgramMgr->GetVendorItem(packet.PerksVendorItemID));
    else
        player->SetFrozenPerksProgramVendorItem(nullptr);
}
