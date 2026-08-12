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

#ifndef TRINITYCORE_BATTLE_PAY_PACKETS_H
#define TRINITYCORE_BATTLE_PAY_PACKETS_H

#include "Packet.h"
#include <vector>

namespace WorldPackets
{
    namespace BattlePay
    {
        // Client requests the shop catalog. Body carries a locale/region selector we do not need.
        class GetProductList final : public ClientPacket
        {
        public:
            explicit GetProductList(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PAY_GET_PRODUCT_LIST, std::move(packet)) { }

            void Read() override { }
        };

        // Client requests the account purchase/distribution list.
        class GetPurchaseList final : public ClientPacket
        {
        public:
            explicit GetPurchaseList(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PAY_GET_PURCHASE_LIST, std::move(packet)) { }

            void Read() override { }
        };

        // The 12.0.7 catalog is a nested reflection bitstream that cannot be re-serialized field-by-field
        // offline (see docs). For P0 we replay a byte-exact, client-validated catalog blob captured from a
        // real 68275 session, so the shop opens and displays real products. RawData is the message BODY
        // (opcode dword already stripped); the ServerPacket base prepends the opcode header.
        class ProductListResponse final : public ServerPacket
        {
        public:
            explicit ProductListResponse() : ServerPacket(SMSG_BATTLE_PAY_GET_PRODUCT_LIST_RESPONSE) { }

            WorldPacket const* Write() override;

            std::vector<uint8> const* RawData = nullptr;
        };

        // Client initiates an in-game purchase. Layout from the client Write method (0x5d9f90):
        // u32, u64, then a 1-bit bool. The u32 is the strong candidate for the productID (the setter is
        // Warden-obfuscated so the exact semantic is runtime-confirmed via the handler's diagnostic log).
        class StartPurchase final : public ClientPacket
        {
        public:
            explicit StartPurchase(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PAY_START_PURCHASE, std::move(packet)) { }

            void Read() override;

            uint32 ProductID = 0;   // candidate (scalar_u32)
            uint64 ScalarU64 = 0;   // candidate: target character GUID
            bool Flag = false;
        };

        // Client opens the checkout for a previously-created distribution (sniff-confirmed: u32 distributionID).
        class OpenCheckout final : public ClientPacket
        {
        public:
            explicit OpenCheckout(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PAY_OPEN_CHECKOUT, std::move(packet)) { }

            void Read() override;

            uint32 DistributionID = 0;
        };

        // Server ack for StartPurchase. Layout from the client read ctor (0x608ec0): u32, u32, u64.
        class StartPurchaseResponse final : public ServerPacket
        {
        public:
            explicit StartPurchaseResponse() : ServerPacket(SMSG_BATTLE_PAY_START_PURCHASE_RESPONSE, 16) { }

            WorldPacket const* Write() override;

            uint32 ResultA = 0;
            uint32 ResultB = 0;
            uint64 PurchaseID = 0;
        };

        // One JamBattlePayPurchase record (descriptor field order). walletName is sent empty (see .cpp).
        struct PurchaseRecord
        {
            uint64 PurchaseID = 0;
            int32 Status = 0;       // BattlepayPurchaseStatus: Done=3, Failed=4
            int32 ResultCode = 0;   // PurchaseResult: Ok=0
            uint32 ProductID = 0;
            uint64 BasePrice = 0;
            uint64 UserPrice = 0;
            int64 TimeCreated = 0;
        };

        // Server drives purchase progress/completion. Layout from client read ctor (0x6090d0):
        // u32 result, then a u32-counted vector of JamBattlePayPurchase. status=Done(3) signals completion
        // and the record echoes the productID delivered.
        class PurchaseUpdate final : public ServerPacket
        {
        public:
            explicit PurchaseUpdate() : ServerPacket(SMSG_BATTLE_PAY_PURCHASE_UPDATE) { }

            WorldPacket const* Write() override;

            uint32 Result = 0;
            std::vector<PurchaseRecord> Purchases;
        };

        // Reply to CMSG_BATTLE_PAY_GET_PURCHASE_LIST. Body layout is identical to
        // SMSG_BATTLE_PAY_PURCHASE_UPDATE: { uint32 Result, uint32 Count, Count x PurchaseRecord }.
        // Proven against a live sniff: a retail account with 9 purchases produced a 413-byte body, and
        // 8 (header) + 9 * 45 (PurchaseRecord = u64+i32+i32+u32+u8+u64+u64+i64) == 413 exactly.
        class GetPurchaseListResponse final : public ServerPacket
        {
        public:
            explicit GetPurchaseListResponse() : ServerPacket(SMSG_BATTLE_PAY_GET_PURCHASE_LIST_RESPONSE, 4 + 4) { }

            WorldPacket const* Write() override;

            uint32 Result = 0;
            std::vector<PurchaseRecord> Purchases;
        };

        // ---------------------------------------------------------------------------------------------
        // VAS (Value Added Services) - paid character services: transfer, rename, faction/race change.
        //
        // Only the two opcodes a real client actually sends are answered here. CMSG_UPDATE_VAS_PURCHASE_STATES
        // is emitted at character select in every one of the 19 captures on this machine, and
        // CMSG_VAS_GET_SERVICE_STATUS alongside it. Both have EMPTY bodies (client serializers RVA 0x5DC6D0
        // and 0x5DD670 write the opcode header and nothing else).
        //
        // The replies below are the truthful complete answers for a realm that has no VAS purchases in
        // flight, not placeholders: retail sends the very same single 0x00 byte for
        // SMSG_ENUM_VAS_PURCHASE_STATES_RESPONSE every session when a player has no pending purchase, and
        // the client's handler (RVA 0x23D0140) CLEARS and rebuilds its whole VASPurchaseState cache from
        // it. Not answering leaves that cache holding whatever it had.
        // ---------------------------------------------------------------------------------------------

        // CMSG_UPDATE_VAS_PURCHASE_STATES (0x400123) - empty body.
        class UpdateVasPurchaseStates final : public ClientPacket
        {
        public:
            explicit UpdateVasPurchaseStates(WorldPacket&& packet) : ClientPacket(CMSG_UPDATE_VAS_PURCHASE_STATES, std::move(packet)) { }

            void Read() override { }
        };

        // SMSG_ENUM_VAS_PURCHASE_STATES_RESPONSE (0x42029B):
        //   Bits<6> Count; FlushBits; Count x VASPurchaseState
        //
        // Count is SIX BITS, not a uint32 - confirmed by the client ctor at RVA 0x60EB40 and by 19 live
        // captures, every one of which is a single 0x00 byte. There is no ClientToken gate on this message
        // (unlike 0x420297 / 0x420298 / 0x4202C3, which silently drop unless the token is echoed).
        //
        // The per-entry VASPurchaseState struct is recovered but deliberately not modelled yet: its State
        // field indexes the LE_VAS_PURCHASE_STATE_* enum whose numeric values are NOT recoverable offline
        // (they are Lua globals, not an AddEnumConstant registrar). Emitting an entry would mean guessing a
        // state value that drives the client's purchase UI, so entries wait for the phase that can prove
        // them. An empty list needs none of that and is exactly what retail sends.
        class EnumVasPurchaseStatesResponse final : public ServerPacket
        {
        public:
            explicit EnumVasPurchaseStatesResponse() : ServerPacket(SMSG_ENUM_VAS_PURCHASE_STATES_RESPONSE, 1) { }

            WorldPacket const* Write() override;
        };

        // CMSG_VAS_GET_SERVICE_STATUS (0x400137) - empty body.
        class VasGetServiceStatus final : public ClientPacket
        {
        public:
            explicit VasGetServiceStatus(WorldPacket&& packet) : ClientPacket(CMSG_VAS_GET_SERVICE_STATUS, std::move(packet)) { }

            void Read() override { }
        };

        // SMSG_VAS_GET_SERVICE_STATUS_RESPONSE (0x4202C0) - exactly 1 byte:
        //   Bits<4> ServiceStatus (high nibble); Bits<4> Unknown (low nibble); FlushBits
        //
        // Client ctor RVA 0x6115E0, handler 0x23D0410 stores both nibbles and fires a Lua event. 0x00 is a
        // legal, complete body. We send 0 because this realm offers no VAS services - that is the accurate
        // status, not a stand-in. The meaning of the low nibble is genuinely unknown, so it stays 0 rather
        // than being given an invented value.
        class VasGetServiceStatusResponse final : public ServerPacket
        {
        public:
            explicit VasGetServiceStatusResponse() : ServerPacket(SMSG_VAS_GET_SERVICE_STATUS_RESPONSE, 1) { }

            WorldPacket const* Write() override;

            uint8 ServiceStatus = 0;
            uint8 Unknown = 0;
        };
    }
}

#endif // TRINITYCORE_BATTLE_PAY_PACKETS_H
