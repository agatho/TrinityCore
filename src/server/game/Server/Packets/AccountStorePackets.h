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

#ifndef TRINITYCORE_ACCOUNT_STORE_PACKETS_H
#define TRINITYCORE_ACCOUNT_STORE_PACKETS_H

#include "Packet.h"
#include <vector>

namespace WorldPackets::AccountStore
{
// Extracted from the client enum registrar (12.0.7.68275).
enum class AccountStoreTransactionType : uint8
{
    Undefined           = 0,
    Purchase            = 1,
    Refund              = 2,
    DebugResetHistory   = 3,
    DebugRemoveItem     = 4
};

enum class AccountStoreTransactionResult : uint8
{
    Success                     = 0,
    Incomplete                  = 1,
    UnknownError                = 2,
    TransactionInProgress       = 3,
    InsufficientFunds           = 4,
    ItemUnknown                 = 5,
    ItemAlreadyOwned            = 6,
    ItemNotOwned                = 7,
    InvalidCurrencyType         = 8,
    OwnedButRefundTimeExpired   = 9,
    NotSupported                = 10,
    Unavailable                 = 11,
    ProxyError                  = 12
};

enum class AccountStoreItemStatus : uint8
{
    Unowned     = 1,
    Refundable  = 2,
    Owned       = 3
};

// The 24-byte item-state element (client reader sub_7FF729138780), shared by the RESULT and FRONT_UPDATE packets.
struct AccountStoreItemState
{
    int32 AccountStoreItemID = 0;
    uint8 Status = 0;                 // AccountStoreItemStatus
    int32 Field8 = 0;
    uint64 Field10 = 0;               // transaction timestamp
    uint8 Field18 = 0;
    bool Field5 = false;              // packed in the top bit of the trailing byte
};

ByteBuffer& operator<<(ByteBuffer& data, AccountStoreItemState const& state);

// CMSG_ACCOUNT_STORE_BEGIN_PURCHASE_OR_REFUND (0x4000C1) — client serializer sub_7FF729078610:
//   uint32 AccountStoreItemID, uint8 TransactionType, uint32 StoreFrontID.
class AccountStoreBeginPurchaseOrRefund final : public ClientPacket
{
public:
    explicit AccountStoreBeginPurchaseOrRefund(WorldPacket&& packet) : ClientPacket(CMSG_ACCOUNT_STORE_BEGIN_PURCHASE_OR_REFUND, std::move(packet)) { }

    void Read() override;

    int32 AccountStoreItemID = 0;
    uint8 TransactionType = 0;
    int32 StoreFrontID = 0;
};

// SMSG_ACCOUNT_STORE_RESULT (0x42032E) — client reader sub_7FF7290B8F70:
//   uint8 Result, uint8 TransactionType, uint32 AccountStoreItemID, item-state element.
class AccountStoreResult final : public ServerPacket
{
public:
    AccountStoreResult() : ServerPacket(SMSG_ACCOUNT_STORE_RESULT) { }

    WorldPacket const* Write() override;

    uint8 Result = 0;                 // AccountStoreTransactionResult
    uint8 TransactionType = 0;
    int32 AccountStoreItemID = 0;
    AccountStoreItemState ItemState;
};

// One entry of the FRONT_UPDATE currency-state vector (12B, three uint32s; exact field semantics not offline-confirmed).
struct AccountStoreCurrencyState
{
    uint32 Field0 = 0;
    uint32 Field4 = 0;
    uint32 Field8 = 0;
};

// SMSG_ACCOUNT_STORE_FRONT_UPDATE (0x42032C) — client reader sub_7FF7290B8C20:
//   uint8 Flags, uint32 Field24, uint32 currencyCount, uint32 itemCount, currencyCount x {u32,u32,u32},
//   uint8 (two packed bits), itemCount x item-state. Flags = AccountStoreFrontFlag (the client ctor defaults it to
//   Enabled=1), so the store front carries the item catalogue with per-item ownership status.
class AccountStoreFrontUpdate final : public ServerPacket
{
public:
    AccountStoreFrontUpdate() : ServerPacket(SMSG_ACCOUNT_STORE_FRONT_UPDATE) { }

    WorldPacket const* Write() override;

    uint8 Flags = 0;                  // AccountStoreFrontFlag
    uint32 Field24 = 0;
    std::vector<AccountStoreCurrencyState> Currencies;
    bool Field58 = false;
    bool Field59 = false;
    std::vector<AccountStoreItemState> Items;
};
}

#endif // TRINITYCORE_ACCOUNT_STORE_PACKETS_H
