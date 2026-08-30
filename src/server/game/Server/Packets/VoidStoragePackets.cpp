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
#include "Log.h"
#include "PacketOperators.h"
#include <span>

namespace WorldPackets::VoidStorage
{
namespace
{
// The element counts leave in fields narrower than the vectors can hold: one byte for the contents,
// four bits for each half of the transfer. Size<uint8> and BitsSize<4> (PacketOperators.h:190
// and :221) both truncate with a plain static_cast and neither of them checks. An oversized list
// would therefore announce a cut down count and then still write every element; the client reads
// the announced number, stops, and takes the surplus for whatever field comes next - the stream
// desyncs instead of merely losing entries. Clamping keeps the announcement and the payload in
// agreement and leaves a line in the log. Same trade as CacheInfo::Write makes for its six bit
// string lengths.
template <typename T>
std::span<T const> ClampToWireLimit(std::vector<T> const& values, std::size_t limit,
    char const* opcodeName, char const* fieldName)
{
    if (values.size() <= limit)
        return values;

    TC_LOG_ERROR("network", "{}: {} holds {} entries but the wire field carries at most {}, the surplus is dropped",
        opcodeName, fieldName, values.size(), limit);

    return std::span(values).first(limit);
}
}

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
    std::span<VoidItem const> const items = ClampToWireLimit(Items, 0xFF, "SMSG_VOID_STORAGE_CONTENTS", "Items");

    _worldPacket << Size<uint8>(items);
    for (VoidItem const& voidItem : items)
        _worldPacket << voidItem;

    return &_worldPacket;
}

WorldPacket const* VoidStorageTransferChanges::Write()
{
    std::span<VoidItem const> const addedItems = ClampToWireLimit(AddedItems, 0xF, "SMSG_VOID_STORAGE_TRANSFER_CHANGES", "AddedItems");
    std::span<ObjectGuid const> const removedItems = ClampToWireLimit(RemovedItems, 0xF, "SMSG_VOID_STORAGE_TRANSFER_CHANGES", "RemovedItems");

    // both counts share one byte - added in the high nibble, removed in the low one
    _worldPacket << BitsSize<4>(addedItems);
    _worldPacket << BitsSize<4>(removedItems);
    _worldPacket.FlushBits();

    for (VoidItem const& voidItem : addedItems)
        _worldPacket << voidItem;

    for (ObjectGuid const& itemGuid : removedItems)
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
