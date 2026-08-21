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

#ifndef TRINITYCORE_VOID_STORAGE_PACKETS_H
#define TRINITYCORE_VOID_STORAGE_PACKETS_H

#include "ItemPacketsCommon.h"
#include "ObjectGuid.h"
#include "Packet.h"

namespace WorldPackets
{
    namespace VoidStorage
    {
        // Client family 0x68, dispatcher RVA 0x752320 (build 12.1.0.69382). The five messages below
        // are still parsed and consumed by the retail client (handlers registered at RVA 0x2110A0),
        // but the client side of the protocol is gone: 12.1 has no CMSG_QUERY_VOID_STORAGE,
        // CMSG_UNLOCK_VOID_STORAGE, CMSG_VOID_STORAGE_TRANSFER or CMSG_SWAP_VOID_ITEM any more, and
        // no Lua UI registers for void storage. Nothing in the client can therefore start a void
        // storage session on its own - these packets are server driven only.

        // 152 bytes in the client (JamVoidItem). The client rejects Slot >= 160 (RVA 0x1ED97A0).
        struct VoidItem
        {
            ObjectGuid Guid;
            ObjectGuid Creator;
            uint32 Slot = 0;
            Item::ItemInstance Item;
        };

        // Values the client distinguishes, read from handler RVA 0x1ED9650. Everything outside
        // 1..9 is silent; note that the client closes the void storage interaction for every
        // value, including Ok, so an error code alone does not keep the window open.
        enum class VoidTransferError : int32
        {
            Ok                  = 0,    ///< no message
            InternalError       = 1,    ///< ERR_VOID_TRANSFER_INTERNAL_ERROR (GameError 936)
            InternalError2      = 2,    ///< same text as InternalError
            StorageFull         = 3,    ///< ERR_VOID_TRANSFER_STORAGE_FULL (GameError 934)
            InternalError3      = 4,    ///< same text as InternalError
            InternalError4      = 5,    ///< same text as InternalError
            NotEnoughMoney      = 6,    ///< ERR_NOT_ENOUGH_MONEY (GameError 47)
            InventoryFull       = 7,    ///< ERR_VOID_TRANSFER_INV_FULL (GameError 935)
            ItemInvalid         = 8,    ///< ERR_VOID_TRANSFER_ITEM_INVALID (GameError 937)
            TransferUnknown     = 9     ///< broadcasts the bare string "VOID_TRANSFER_UNKNOWN", not a GameError
        };

        class VoidStorageFailed final : public ServerPacket
        {
        public:
            explicit VoidStorageFailed() : ServerPacket(SMSG_VOID_STORAGE_FAILED, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        // UNVERIFIED: what the client does with this. The wire format is read straight from the
        // dispatcher (case 6815745), but the consumer cannot be followed: 0x1ED9540 is a jmp to
        // 0x1DF32B0 and that function is obfuscated in this image - valid prologue, garbage after.
        // Reading it needs a different dump; until then the UI effect is unknown.
        class VoidStorageContents final : public ServerPacket
        {
        public:
            explicit VoidStorageContents() : ServerPacket(SMSG_VOID_STORAGE_CONTENTS) { }

            WorldPacket const* Write() override;

            // UNVERIFIED: written as a plain uint8. The codegen shows no shift or mask, so bits<8>
            // followed by a flush and a plain byte are indistinguishable here - both produce the
            // same wire bytes, and only a packet that put another bit field next to it would tell
            // them apart. If one ever does, this is the field to revisit.
            std::vector<VoidItem> Items;                ///< at most 255, count is a single byte
        };

        class VoidStorageTransferChanges final : public ServerPacket
        {
        public:
            explicit VoidStorageTransferChanges() : ServerPacket(SMSG_VOID_STORAGE_TRANSFER_CHANGES) { }

            WorldPacket const* Write() override;

            std::vector<VoidItem> AddedItems;           ///< at most 15, count is 4 bits
            std::vector<ObjectGuid> RemovedItems;       ///< at most 15, count is 4 bits
        };

        class VoidTransferResult final : public ServerPacket
        {
        public:
            explicit VoidTransferResult() : ServerPacket(SMSG_VOID_TRANSFER_RESULT, 4) { }

            WorldPacket const* Write() override;

            VoidTransferError Result = VoidTransferError::Ok;
        };

        class VoidItemSwapResponse final : public ServerPacket
        {
        public:
            explicit VoidItemSwapResponse() : ServerPacket(SMSG_VOID_ITEM_SWAP_RESPONSE, 16 + 4 + 16 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid VoidItemA;
            uint32 VoidItemSlotA = 0;
            ObjectGuid VoidItemB;
            uint32 VoidItemSlotB = 0;
        };

        ByteBuffer& operator<<(ByteBuffer& data, VoidItem const& voidItem);
    }
}

#endif // TRINITYCORE_VOID_STORAGE_PACKETS_H
