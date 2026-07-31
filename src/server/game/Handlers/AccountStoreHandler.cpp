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
#include "GameTime.h"
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

    if (packet.TransactionType == uint8(AccountStoreTransactionType::Refund))
    {
        AccountStoreItemEntry const* refundItem = sAccountStoreItemStore.LookupEntry(uint32(packet.AccountStoreItemID));
        if (!refundItem)
        {
            sendResult(AccountStoreTransactionResult::ItemUnknown, AccountStoreItemStatus::Unowned);
            return;
        }

        CollectionMgr* collectionMgr = GetCollectionMgr();
        if (!collectionMgr->HasAccountStoreItem(refundItem->ID))
        {
            sendResult(AccountStoreTransactionResult::ItemNotOwned, AccountStoreItemStatus::Unowned);
            return;
        }

        // The currency was debited from the single character that bought the item; a refund credits currency here, so
        // it must go back to that SAME character. Refunding on a different character would mint currency that was
        // destroyed elsewhere (an account-wide-ownership + per-character-wallet transfer exploit). Only the paying
        // character may refund. (Legacy rows carry PayerGuid == 0 and are exempt from the scope check.)
        CollectionMgr::AccountStorePurchase const* purchase = collectionMgr->GetAccountStorePurchase(refundItem->ID);
        if (purchase && purchase->PayerGuid != 0 && purchase->PayerGuid != player->GetGUID().GetCounter())
        {
            sendResult(AccountStoreTransactionResult::NotSupported, AccountStoreItemStatus::Owned);
            return;
        }

        // RefundDuration == 0 means the item is non-refundable; otherwise the refund must fall inside the window.
        uint32 purchaseTime = collectionMgr->GetAccountStorePurchaseTime(refundItem->ID);
        if (refundItem->RefundDuration <= 0 || !purchaseTime
            || uint32(GameTime::GetGameTime()) - purchaseTime > uint32(refundItem->RefundDuration))
        {
            sendResult(AccountStoreTransactionResult::OwnedButRefundTimeExpired, AccountStoreItemStatus::Owned);
            return;
        }

        // Only cleanly-revocable rewards can be refunded. A transmog/appearance reward (TransmogSetID set) is
        // append-only in the account collection and cannot be revoked without risking other sources, so such items
        // are reported not-refundable rather than refunded into a keep-the-appearance exploit.
        if (refundItem->TransmogSetID != 0 || !refundItem->SpellID)
        {
            sendResult(AccountStoreTransactionResult::NotSupported, AccountStoreItemStatus::Owned);
            return;
        }

        // Revoke the reward - but only if THIS purchase actually taught it. If the payer already owned the mount/spell
        // from another source at purchase time (Granted == false), stripping it here would destroy a collectible this
        // purchase never granted; refund the currency but leave the collectible intact. A mount teaching spell resyncs
        // the account mount list; any other teaching spell is simply un-learned.
        if (!purchase || purchase->Granted)
        {
            if (sDB2Manager.GetMount(uint32(refundItem->SpellID)))
                collectionMgr->RemoveMount(uint32(refundItem->SpellID));
            else
                player->RemoveSpell(uint32(refundItem->SpellID));
        }

        if (refundItem->Price > 0 && refundItem->CurrencyTypesID)
            player->AddCurrency(uint32(refundItem->CurrencyTypesID), uint32(refundItem->Price), CurrencyGainSource::ItemRefund);

        collectionMgr->RemoveAccountStorePurchase(refundItem->ID);

        sendResult(AccountStoreTransactionResult::Success, AccountStoreItemStatus::Unowned);
        return;
    }

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

    // Only currency-purchasable rows can be bought through the worldserver. A row with no positive Price or no
    // CurrencyTypesID is a real-money / externally-billed (VAS) item whose entitlement is delivered out-of-band -
    // the world server must NOT hand it out for free just because the payment block would otherwise be skipped.
    if (item->Price <= 0 || !item->CurrencyTypesID)
    {
        sendResult(AccountStoreTransactionResult::Unavailable, AccountStoreItemStatus::Unowned);
        return;
    }

    if (!player->HasCurrency(uint32(item->CurrencyTypesID), uint32(item->Price)))
    {
        sendResult(AccountStoreTransactionResult::InsufficientFunds, AccountStoreItemStatus::Unowned);
        return;
    }

    player->RemoveCurrency(uint32(item->CurrencyTypesID), item->Price, CurrencyDestroyReason::Vendor);

    // Capture whether this purchase actually teaches the SpellID collectible BEFORE learning it. If the payer already
    // knows the spell/mount from another source, a later refund must not strip it (Granted == false records that).
    bool const granted = !item->SpellID || !player->HasSpell(uint32(item->SpellID));

    // Grant the reward. A teaching SpellID adds mounts/pets/toys via the standard learn path; a TransmogSetID
    // is added directly to the account collection.
    if (item->SpellID)
        player->LearnSpell(uint32(item->SpellID), false);
    if (item->TransmogSetID)
        collectionMgr->AddTransmogSet(uint32(item->TransmogSetID));

    collectionMgr->AddAccountStorePurchase(item->ID, player->GetGUID().GetCounter(), granted);

    sendResult(AccountStoreTransactionResult::Success, AccountStoreItemStatus::Owned);
}
