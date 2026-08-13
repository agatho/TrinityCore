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

WorldPacket const* ResponsePerkPendingRewards::Write()
{
    _worldPacket << uint32(Rewards.size());
    for (PendingReward const& reward : Rewards)
    {
        // The discriminant sits in the top three bits of a byte of its own: the client reads one byte and
        // shifts it right by five, so the remaining five bits are padding.
        _worldPacket << Bits<3>(TransactionTypeActivityThreshold);
        _worldPacket.FlushBits();
        _worldPacket << reward.Owner;
        _worldPacket << int32(reward.Amount);
        _worldPacket << int32(reward.ActivityMonthID);
        _worldPacket << int32(reward.ThresholdOrderIndex);
    }

    return &_worldPacket;
}

WorldPacket const* PerksAnimToggleKillSwitch::Write()
{
    _worldPacket << Bits<1>(AttackAnimToggleEnabled);
    _worldPacket << Bits<1>(MountSpecialAnimToggleEnabled);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* PerksProgramActivityComplete::Write()
{
    _worldPacket << uint32(PerksActivityID);

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

WorldPacket const* PerksProgramActivityUpdate::Write()
{
    _worldPacket << uint32(CompletedActivityIDs.size());
    _worldPacket << uint64(PeriodEnd);
    _worldPacket << uint64(PeriodStart);
    _worldPacket << uint32(Unused);
    for (uint32 activityId : CompletedActivityIDs)
        _worldPacket << uint32(activityId);

    return &_worldPacket;
}
}
