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

#include "BattlePayPackets.h"

namespace WorldPackets::BattlePay
{
WorldPacket const* ProductListResponse::Write()
{
    if (RawData && !RawData->empty())
        _worldPacket.append(RawData->data(), RawData->size());

    return &_worldPacket;
}

void StartPurchase::Read()
{
    _worldPacket >> ProductID;
    _worldPacket >> ScalarU64;
    Flag = _worldPacket.ReadBit();
}

void OpenCheckout::Read()
{
    _worldPacket >> DistributionID;
}

WorldPacket const* StartPurchaseResponse::Write()
{
    _worldPacket << ResultA;
    _worldPacket << ResultB;
    _worldPacket << PurchaseID;

    return &_worldPacket;
}

WorldPacket const* GetPurchaseListResponse::Write()
{
    _worldPacket << Result;
    _worldPacket << uint32(Purchases.size());
    for (PurchaseRecord const& p : Purchases)
    {
        _worldPacket << p.PurchaseID;
        _worldPacket << p.Status;
        _worldPacket << p.ResultCode;
        _worldPacket << p.ProductID;
        _worldPacket << uint8(0);       // walletName: empty (8-bit length primitive, value 0)
        _worldPacket << p.BasePrice;
        _worldPacket << p.UserPrice;
        _worldPacket << p.TimeCreated;
    }

    return &_worldPacket;
}

WorldPacket const* PurchaseUpdate::Write()
{
    // NO leading Result here. SMSG_BATTLE_PAY_PURCHASE_UPDATE (0x420231) begins straight with the record
    // count: its ctor (client RVA 0x6090D0) performs exactly ONE ReadUInt32 and feeds it directly to
    // vector_resize, then parses that many records.
    //
    // Its sibling SMSG_BATTLE_PAY_GET_PURCHASE_LIST_RESPONSE (0x42021B, ctor 0x607DA0) DOES lead with a
    // Result and performs TWO ReadUInt32. The two messages share the record type but not the header, and
    // the client structs prove it: the record vector sits at +0x20 in this message and at +0x28 in that
    // one - displaced by exactly the 4 bytes of Result.
    //
    // Writing Result here made the client read our always-zero Result AS THE COUNT, so it parsed zero
    // records and returned immediately (merge handler 0x23CD340, cmp/je on count == 0) with no error
    // anywhere. That silently broke the entire purchase confirmation handshake - see the commit message.
    _worldPacket << uint32(Purchases.size());
    for (PurchaseRecord const& p : Purchases)
    {
        _worldPacket << p.PurchaseID;
        _worldPacket << p.Status;
        _worldPacket << p.ResultCode;
        _worldPacket << p.ProductID;
        _worldPacket << uint8(0);       // walletName: empty (8-bit length primitive, value 0)
        _worldPacket << p.BasePrice;
        _worldPacket << p.UserPrice;
        _worldPacket << p.TimeCreated;
    }

    return &_worldPacket;
}
}
