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

#include "VoidStoragePackets.h"
#include "PacketOperators.h"

namespace WorldPackets::VoidStorage
{
ByteBuffer& operator<<(ByteBuffer& data, VoidItem const& voidItem)
{
    data << voidItem.Guid;
    data << voidItem.Creator;
    data << uint32(voidItem.Slot);
    data << voidItem.Item;

    return data;
}

WorldPacket const* VoidStorageContents::Write()
{
    _worldPacket << Size<uint8>(Items);
    for (VoidItem const& voidItem : Items)
        _worldPacket << voidItem;

    return &_worldPacket;
}

WorldPacket const* VoidStorageTransferChanges::Write()
{
    // both counts share one byte - added in the high nibble, removed in the low one
    _worldPacket << BitsSize<4>(AddedItems);
    _worldPacket << BitsSize<4>(RemovedItems);
    _worldPacket.FlushBits();

    for (VoidItem const& voidItem : AddedItems)
        _worldPacket << voidItem;

    for (ObjectGuid const& itemGuid : RemovedItems)
        _worldPacket << itemGuid;

    return &_worldPacket;
}

WorldPacket const* VoidTransferResult::Write()
{
    _worldPacket << As<int32>(Result);

    return &_worldPacket;
}

WorldPacket const* VoidItemSwapResponse::Write()
{
    _worldPacket << VoidItemA;
    _worldPacket << uint32(VoidItemSlotA);
    _worldPacket << VoidItemB;
    _worldPacket << uint32(VoidItemSlotB);

    return &_worldPacket;
}
}
