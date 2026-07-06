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
#include "AccountStorePackets.h"
#include "CollectionMgr.h"
#include "DB2Stores.h"
#include "Player.h"

void WorldSession::SendAccountStoreFrontUpdate()
{
    using namespace WorldPackets::AccountStore;

    Player* player = GetPlayer();
    if (!player)
        return;

    AccountStoreFrontUpdate front;
    // AccountStoreFrontFlag: Enabled (1) | PurchaseEnabled (2). The client constructor defaults the flag byte to
    // Enabled=1; enabling purchase lets the store front accept BEGIN_PURCHASE transactions. Refund stays off.
    front.Flags = 0x03;

    CollectionMgr* collectionMgr = GetCollectionMgr();
    for (AccountStoreItemEntry const* item : sAccountStoreItemStore)
    {
        AccountStoreItemState state;
        state.AccountStoreItemID = int32(item->ID);
        state.Status = uint8(collectionMgr->HasAccountStoreItem(item->ID) ? AccountStoreItemStatus::Owned : AccountStoreItemStatus::Unowned);
        front.Items.push_back(state);
    }

    SendPacket(front.Write());
}

void WorldSession::HandleAccountStoreBeginPurchaseOrRefund(WorldPackets::AccountStore::AccountStoreBeginPurchaseOrRefund& packet)
{
    using namespace WorldPackets::AccountStore;

    Player* player = GetPlayer();
    if (!player)
        return;

    auto sendResult = [&](AccountStoreTransactionResult result, AccountStoreItemStatus status)
    {
        AccountStoreResult response;
        response.Result = uint8(result);
        response.TransactionType = packet.TransactionType;
        response.AccountStoreItemID = packet.AccountStoreItemID;
        response.ItemState.AccountStoreItemID = packet.AccountStoreItemID;
        response.ItemState.Status = uint8(status);
        SendPacket(response.Write());
    };

    // Only purchases are handled; refunds need the purchase-history / refund-window backend (not yet built),
    // so they get an accurate NotSupported result rather than a silent drop.
    if (packet.TransactionType != uint8(AccountStoreTransactionType::Purchase))
    {
        sendResult(AccountStoreTransactionResult::NotSupported, AccountStoreItemStatus::Unowned);
        return;
    }

    AccountStoreItemEntry const* item = sAccountStoreItemStore.LookupEntry(uint32(packet.AccountStoreItemID));
    if (!item)
    {
        sendResult(AccountStoreTransactionResult::ItemUnknown, AccountStoreItemStatus::Unowned);
        return;
    }

    CollectionMgr* collectionMgr = GetCollectionMgr();
    if (collectionMgr->HasAccountStoreItem(item->ID))
    {
        sendResult(AccountStoreTransactionResult::ItemAlreadyOwned, AccountStoreItemStatus::Owned);
        return;
    }

    // Only SpellID (teaching spell) and TransmogSetID rewards are resolvable server-side. Some rows (e.g. certain
    // Plunderstorm pets/mounts) carry SpellID == 0 && TransmogSetID == 0 and reference the collectible through a
    // display-info / link that is not resolvable offline yet. Refuse those rather than charge currency for an item
    // we cannot actually deliver.
    if (!item->SpellID && !item->TransmogSetID)
    {
        sendResult(AccountStoreTransactionResult::Unavailable, AccountStoreItemStatus::Unowned);
        return;
    }

    if (item->Price > 0)
    {
        if (!item->CurrencyTypesID || !player->HasCurrency(uint32(item->CurrencyTypesID), uint32(item->Price)))
        {
            sendResult(AccountStoreTransactionResult::InsufficientFunds, AccountStoreItemStatus::Unowned);
            return;
        }

        player->RemoveCurrency(uint32(item->CurrencyTypesID), item->Price, CurrencyDestroyReason::Vendor);
    }

    // Grant the reward. A teaching SpellID adds mounts/pets/toys via the standard learn path; a TransmogSetID
    // is added directly to the account collection.
    if (item->SpellID)
        player->LearnSpell(uint32(item->SpellID), false);
    if (item->TransmogSetID)
        collectionMgr->AddTransmogSet(uint32(item->TransmogSetID));

    collectionMgr->AddAccountStorePurchase(item->ID);

    sendResult(AccountStoreTransactionResult::Success, AccountStoreItemStatus::Owned);
}
