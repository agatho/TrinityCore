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

#include "PerksProgramPackets.h"
#include "PacketOperators.h"

namespace WorldPackets::PerksProgram
{
void PerksProgramRequestPurchase::Read()
{
    _worldPacket >> PerksVendorItemID;
    _worldPacket >> VendorGUID;
}

void PerksProgramRequestRefund::Read()
{
    _worldPacket >> PerksVendorItemID;
    _worldPacket >> VendorGUID;
}

void PerksProgramSetFrozenVendorItem::Read()
{
    _worldPacket >> Bits<1>(Frozen);
    _worldPacket.ResetBitPos();
    _worldPacket >> PerksVendorItemID;
    _worldPacket >> NpcGUID;
}

WorldPacket const* ResponsePerkRecentPurchases::Write()
{
    _worldPacket << uint32(Purchases.size());
    for (RecentPurchase const& purchase : Purchases)
    {
        _worldPacket << int32(purchase.PerksVendorItemID);
        _worldPacket << uint64(purchase.PurchaseTime);
        _worldPacket << uint8(purchase.Refundable ? 0x80 : 0x00);   // client reads bit7
    }
    return &_worldPacket;
}

void PerksProgramRequestCartCheckout::Read()
{
    uint32 itemCount;
    _worldPacket >> itemCount;
    _worldPacket >> VendorGUID;

    // Reserve conservatively so a bogus count cannot force a huge up-front allocation; each read is
    // bounds-checked by the underlying buffer.
    PerksVendorItemIDs.reserve(std::min<uint32>(itemCount, 100));
    for (uint32 i = 0; i < itemCount; ++i)
        PerksVendorItemIDs.push_back(_worldPacket.read<int32>());
}

WorldPacket const* PerksProgramVendorUpdate::Write()
{
    _worldPacket << uint32(VendorItems.size());
    for (PerksVendorItem const& vendorItem : VendorItems)
        _worldPacket << vendorItem;

    return &_worldPacket;
}
}
