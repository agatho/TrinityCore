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

        // JamVoidItem, 152 bytes in the client. The field order below is read out of the client
        // binary, not inherited from the deleted pre-11.2 TrinityCore definition it happens to
        // match: the element loop is inlined into the family dispatcher (RVA 0x752320, image base
        // 0x7FF780FD0000) and reads, per element, in this order
        //
        //     0x75241D  call 0x36012B0   ReadPackedGuid  -> element+0x00   Guid
        //     0x752429  call 0x36012B0   ReadPackedGuid  -> element+0x10   Creator
        //     0x752438  call 0x35AF190   Read<uint32>    -> element+0x20   Slot
        //     0x75244A  call 0x6BFC30    ItemInstance    -> element+0x28   Item
        //     0x75244F  add r14, 0x98    -> next element, stride 152
        //
        // The primitives are the ones named in BEFUND_wire_primitiven_69382: 0x36012B0
        // ReadPackedGuid, 0x35AF190 Read<uint32>. 0x6BFC30 is the ItemInstance reader - it reads
        // ItemID, then Modifications, then the ItemBonus presence bit (bit 7 of a byte, so
        // MSB-first), which is the same order this unit measured for ItemInstance at 204 packets
        // (K5, 147:0 decisive). Cross-checked against raw capstone disassembly of wow_dump.bin, so
        // the sequence does not rest on Hex-Rays output. The read order IS the wire order; the
        // offsets only say where each field lands in the 152-byte element.
        //
        // SMSG_VOID_STORAGE_TRANSFER_CHANGES uses the identical element: its loop at 0x7525A2
        // .. 0x7525E1 is the same four calls with the same offsets and the same 0x98 stride, which
        // is why both opcodes share operator<<(ByteBuffer&, VoidItem const&).
        //
        // No round-trip is possible: no recording of family 0x68 exists (0 packets in all twelve
        // 12.1 captures), so the disassembly is the whole proof. The client rejects Slot >= 160
        // (RVA 0x1ED97A0).
        struct VoidItem
        {
            ObjectGuid Guid;                            ///< reader 0x36012B0 at element+0x00
            ObjectGuid Creator;                         ///< reader 0x36012B0 at element+0x10
            uint32 Slot = 0;                            ///< reader 0x35AF190 at element+0x20
            Item::ItemInstance Item;                    ///< reader 0x6BFC30  at element+0x28
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

        // Wire read from the dispatcher, case 6815746: one byte (0x75256E call 0x35AF050
        // Read<uint8>) carrying bits<4> AddedCount in the high nibble and bits<4> RemovedCount in
        // the low one - 0x75257D shr 4 sizes the JamVoidItem vector, 0x75258C and 0xF sizes the
        // WOWGUID vector - then AddedCount x JamVoidItem, then RemovedCount x PackedGuid. The
        // element field order is proven at VoidItem above: same four calls, same offsets, same
        // 152-byte stride as SMSG_VOID_STORAGE_CONTENTS, so nothing here is assumed.
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
