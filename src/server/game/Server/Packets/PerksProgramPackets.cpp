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
#include <algorithm>

namespace WorldPackets::PerksProgram
{
void PerksProgramItemsRefreshed::Read()
{
    uint32 count;
    _worldPacket >> count;

    // Reserve conservatively so a bogus count cannot force a huge up-front allocation; each read is
    // bounds-checked by the underlying buffer.
    RequestedVendorItemIDs.reserve(std::min<uint32>(count, 100));
    for (uint32 i = 0; i < count; ++i)
        RequestedVendorItemIDs.push_back(_worldPacket.read<int32>());
}

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
    _worldPacket << Size<uint32>(Purchases);
    for (PerksRecentPurchase const& purchase : Purchases)
        _worldPacket << purchase;

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
    _worldPacket << uint32(PerksUIThemeID);
    for (uint32 activityId : CompletedActivityIDs)
        _worldPacket << uint32(activityId);

    return &_worldPacket;
}

WorldPacket const* PerksProgramResult::Write()
{
    // Header: four bits of type, two of error, the stamp flag, one spare bit, then flush -- exactly one byte.
    _worldPacket << Bits<4>(Type);
    _worldPacket << Bits<2>(Err);
    _worldPacket << OptionalInit(Stamp);
    _worldPacket << Bits<1>(0);
    _worldPacket.FlushBits();

    switch (Type)
    {
        case ResultTypePurchaseSuccess:
        case ResultTypeRefundSuccess:
            _worldPacket << int32(PerksVendorItemID);
            _worldPacket << Size<uint32>(Purchases);
            for (PerksRecentPurchase const& purchase : Purchases)
                _worldPacket << purchase;
            break;
        case ResultTypeTenderAwarded:
            // The first two int32 and the trailing int32 array are on the wire but the consumer never reads
            // them; the 66220 capture carries 3, 3 and an empty array. Only TenderAwarded is consumed.
            _worldPacket << int32(0);
            _worldPacket << int32(0);
            _worldPacket << int32(TenderAwarded);
            _worldPacket << uint32(0);
            break;
        case ResultTypeVendorOpen:
            _worldPacket << VendorGUID;
            _worldPacket << DisplayGUID;
            // Count BEFORE the seven int32, elements only after them -- see the header comment.
            _worldPacket << Size<uint32>(VendorItems);
            // Seven int32 no consumer reads. The 68974 capture carries 28 zero bytes here.
            for (std::size_t i = 0; i < 7; ++i)
                _worldPacket << int32(0);
            for (PerksVendorItem const& vendorItem : VendorItems)
                _worldPacket << vendorItem;
            break;
        case ResultTypeVendorMerge:
            _worldPacket << Size<uint32>(VendorItems);
            for (PerksVendorItem const& vendorItem : VendorItems)
                _worldPacket << vendorItem;
            break;
        case ResultTypeTenderGranted:
            _worldPacket << int32(TenderAwarded);
            break;
        case ResultTypeError:
        default:
            break;      // empty branch: header byte only
    }

    // Last field of the message, after the branch arrays -- reader RVA 0x74CE10 ends with this read.
    if (Stamp)
        _worldPacket << *Stamp;

    return &_worldPacket;
}

WorldPacket const* PerksProgramDisabled::Write()
{
    _worldPacket << Bits<1>(Disabled);
    _worldPacket.FlushBits();

    return &_worldPacket;
}
}
