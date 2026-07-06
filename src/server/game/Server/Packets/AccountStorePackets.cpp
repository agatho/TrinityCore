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

#include "AccountStorePackets.h"

namespace WorldPackets::AccountStore
{
ByteBuffer& operator<<(ByteBuffer& data, AccountStoreItemState const& state)
{
    data << int32(state.AccountStoreItemID);
    data << uint8(state.Status);
    data << int32(state.Field8);
    data << uint64(state.Field10);
    data << uint8(state.Field18);
    data << uint8(state.Field5 ? 0x80 : 0x00);   // Field5 is the top bit of this trailing byte
    return data;
}

void AccountStoreBeginPurchaseOrRefund::Read()
{
    _worldPacket >> AccountStoreItemID;
    _worldPacket >> TransactionType;
    _worldPacket >> StoreFrontID;
}

WorldPacket const* AccountStoreResult::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint8(TransactionType);
    _worldPacket << int32(AccountStoreItemID);
    _worldPacket << ItemState;
    return &_worldPacket;
}
}
