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
#include "PerksProgramMgr.h"
#include "PerksProgramPackets.h"

void WorldSession::HandlePerksProgramStatusRequest(WorldPackets::PerksProgram::PerksProgramStatusRequest& /*packet*/)
{
    WorldPackets::PerksProgram::PerksProgramVendorUpdate vendorUpdate;
    vendorUpdate.VendorItems = sPerksProgramMgr->GetCurrentVendorItems();
    SendPacket(vendorUpdate.Write());
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

    return true;
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
