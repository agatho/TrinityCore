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

#ifndef TRINITYCORE_CRAFTING_ORDER_PACKETS_H
#define TRINITYCORE_CRAFTING_ORDER_PACKETS_H

#include "Packet.h"
#include "CraftingPacketsCommon.h"
#include "ObjectGuid.h"
#include "Optional.h"
#include <array>
#include <string>
#include <vector>

namespace WorldPackets
{
namespace CraftingOrders
{
    // Optional trailing "context" struct present on most crafting-order CMSGs (client sub_7FF72906D6F0):
    // two length-prefixed strings + two bit flags. The server does not need it (it has the GUIDs); it is
    // read only to consume the bytes. Length encoding: 10-bit value V; V==0 means empty, else actual length = V-1.
    struct ClientContext
    {
        std::string String1;
        std::string String2;
        bool Flag1 = false;
        bool Flag2 = false;

        void Read(ByteBuffer& data);
    };

    // CMSG_CRAFTING_ORDER_CREATE (0x3B0117). Wire recovered via Ghidra from serializer sub_7FF729154130.
    // Scalar labels are deduced-certain by type + elimination (see CRAFTING_ORDERS_PLAN_68275.md).
    // The four reagent-ish vectors: [0]=reagents, [1..3]=recraft enchants/gems/modifications (empty for a new order).
    // Reagent element field semantics (ItemID vs count vs slot) are not yet confirmed, so they are read raw here and
    // detailed reagent/escrow handling is deferred to P3 — the wire is consumed byte-exact regardless.
    struct CraftingReagentSlot
    {
        uint32 Field1 = 0;
        uint32 Field2 = 0;
        Optional<uint8> Extra;
        Crafting::CraftingReagentBase Reagent;   // only populated for vectors [2],[3]
    };

    class CraftingOrderCreate final : public ClientPacket
    {
    public:
        explicit CraftingOrderCreate(WorldPacket&& packet) : ClientPacket(CMSG_CRAFTING_ORDER_CREATE, std::move(packet)) { }

        void Read() override;

        int32 SkillLineAbilityID = 0;
        uint8 OrderType = 0;
        uint8 MinQuality = 0;
        uint64 TipAmount = 0;
        ObjectGuid TargetGUID;                 // crafter target for personal orders
        Optional<uint32> SecondaryId;          // only sent for guild/personal orders (orderType 1/2); semantics unconfirmed
        std::string CustomerNotes;
        std::string RecraftNote;               // only for orderType 2
        Optional<ObjectGuid> OptionalGuid;     // written only when the first flag bit is set
        bool Flag1 = false;
        bool Flag2 = false;
        std::array<std::vector<CraftingReagentSlot>, 4> Vectors;
        ClientContext Context;
    };

    // CMSG_CRAFTING_ORDER_CLAIM (0x3B011B): { u64 OrderID; u8; bit hasContext; [ClientContext] }
    class CraftingOrderClaim final : public ClientPacket
    {
    public:
        explicit CraftingOrderClaim(WorldPacket&& packet) : ClientPacket(CMSG_CRAFTING_ORDER_CLAIM, std::move(packet)) { }

        void Read() override;

        uint64 OrderID = 0;
        uint8 Field2 = 0;
        ClientContext Context;
        bool HasContext = false;
    };

    // CMSG_CRAFTING_ORDER_CANCEL (0x3B011E): { PackedGuid; u64 OrderID; bit hasContext; [ClientContext] }
    class CraftingOrderCancel final : public ClientPacket
    {
    public:
        explicit CraftingOrderCancel(WorldPacket&& packet) : ClientPacket(CMSG_CRAFTING_ORDER_CANCEL, std::move(packet)) { }

        void Read() override;

        ObjectGuid NpcGUID;                    // customer/station GUID the order lives at
        uint64 OrderID = 0;
        ClientContext Context;
        bool HasContext = false;
    };

    // CMSG_CRAFTING_ORDER_RELEASE (0x3B011C): { u64 OrderID; u8; bit hasContext; [ClientContext] }
    // Crafter voluntarily gives up a claimed order; it returns to the pool. Serializer sub_7FF729154E00.
    class CraftingOrderRelease final : public ClientPacket
    {
    public:
        explicit CraftingOrderRelease(WorldPacket&& packet) : ClientPacket(CMSG_CRAFTING_ORDER_RELEASE, std::move(packet)) { }

        void Read() override;

        uint64 OrderID = 0;
        uint8 Field2 = 0;
        ClientContext Context;
        bool HasContext = false;
    };

    // CMSG_CRAFTING_ORDER_REJECT (0x3B011F): { u64 OrderID; u8; string Reason (len in bit block, bytes after ctx);
    // bit hasContext; [ClientContext] }. Crafter declines an order. Serializer sub_7FF7291552B0.
    class CraftingOrderReject final : public ClientPacket
    {
    public:
        explicit CraftingOrderReject(WorldPacket&& packet) : ClientPacket(CMSG_CRAFTING_ORDER_REJECT, std::move(packet)) { }

        void Read() override;

        uint64 OrderID = 0;
        uint8 Field2 = 0;
        std::string Reason;
        ClientContext Context;
        bool HasContext = false;
    };

    // Client enum ClientCrafting::CraftingOrderResult (12.0.7.68275, extracted from the client enum registrar).
    enum class CraftingOrderResult : uint8
    {
        Ok                       = 0,
        Aborted                  = 1,
        AlreadyClaimed           = 2,
        AlreadyCrafted           = 3,
        CannotBeOrdered          = 4,
        CannotCancel             = 5,
        CannotClaim              = 6,
        CannotClaimOwnOrder      = 7,
        CannotCraft              = 8,
        CannotCreate             = 9,
        CannotFulfill            = 10,
        CannotRecraft            = 11,
        CannotReject             = 12,
        CannotRelease            = 13,
        CrafterIsIgnored         = 14,
        DatabaseError            = 15,
        Expired                  = 16,
        Locked                   = 17,
        InvalidDuration          = 18,
        InvalidMinQuality        = 19,
        InvalidNotes             = 20,
        InvalidReagent           = 21,
        InvalidRealm             = 22,
        InvalidRecipe            = 23,
        InvalidRecraftItem       = 24,
        InvalidSort              = 25,
        InvalidTarget            = 26,
        InvalidType              = 27,
        MaxOrdersReached         = 28,
        MissingCraftingTable     = 29,
        MissingCurrency          = 30,
        MissingItem              = 31,
        MissingNpc               = 32,
        MissingOrder             = 33,
        MissingRecraftItem       = 34,
        NoAccountItems           = 35,
        NotClaimed               = 36,
        NotCrafted               = 37,
        NotInGuild               = 38,
        NotYetImplemented        = 39,
        OutOfPublicOrderCapacity = 40,
        ServerIsNotAvailable     = 41,
        ThrottleViolation        = 42,
        TargetCannotCraft        = 43,
        TargetLocked             = 44,
        Timeout                  = 45,
        TooManyCurrencies        = 46,
        TooManyItems             = 47,
        WrongVersion             = 48
    };

    // The create/claim/cancel/release/reject responses share the same wire (12.0.7.68275, from the client
    // deserializers sub_7FF7290B92D0 / _9640 / _9A90 / _96E0 / _9B90): uint8 result, then uint64 order id.
    class CraftingOrderActionResult : public ServerPacket
    {
    public:
        explicit CraftingOrderActionResult(OpcodeServer opcode) : ServerPacket(opcode, 1 + 8) { }

        WorldPacket const* Write() override;

        CraftingOrderResult Result = CraftingOrderResult::Ok;
        uint64 CraftingOrderID = 0;
    };

    class CraftingOrderCreateResult final : public CraftingOrderActionResult
    {
    public:
        CraftingOrderCreateResult() : CraftingOrderActionResult(SMSG_CRAFTING_ORDER_CREATE_RESULT) { }
    };

    class CraftingOrderClaimResult final : public CraftingOrderActionResult
    {
    public:
        CraftingOrderClaimResult() : CraftingOrderActionResult(SMSG_CRAFTING_ORDER_CLAIM_RESULT) { }
    };

    class CraftingOrderCancelResult final : public CraftingOrderActionResult
    {
    public:
        CraftingOrderCancelResult() : CraftingOrderActionResult(SMSG_CRAFTING_ORDER_CANCEL_RESULT) { }
    };

    class CraftingOrderReleaseResult final : public CraftingOrderActionResult
    {
    public:
        CraftingOrderReleaseResult() : CraftingOrderActionResult(SMSG_CRAFTING_ORDER_RELEASE_RESULT) { }
    };

    class CraftingOrderRejectResult final : public CraftingOrderActionResult
    {
    public:
        CraftingOrderRejectResult() : CraftingOrderActionResult(SMSG_CRAFTING_ORDER_REJECT_RESULT) { }
    };

    // CMSG_CRAFTING_ORDER_LIST_MY_ORDERS (0x3B0118): browse the requesting player's own posted orders. The full
    // wire carries filter/sort fields, but the server only needs the requester (it is the packet's sender), so
    // Read consumes nothing — the packet is length-framed, so trailing filter bytes are harmless.
    class CraftingOrderListMyOrders final : public ClientPacket
    {
    public:
        explicit CraftingOrderListMyOrders(WorldPacket&& packet) : ClientPacket(CMSG_CRAFTING_ORDER_LIST_MY_ORDERS, std::move(packet)) { }

        void Read() override { }
    };

    // CMSG_CRAFTING_ORDER_LIST_CRAFTER_ORDERS (0x3B0119): browse claimable orders a crafter may take. Leading
    // wire (client sub_7FF72915___): PackedGUID NpcCraftOrderStation, uint32 SkillLineAbilityID, then trailing
    // filter/sort scalars + a packed-int filter vector that the server does not need (length-framed packet).
    class CraftingOrderListCrafterOrders final : public ClientPacket
    {
    public:
        explicit CraftingOrderListCrafterOrders(WorldPacket&& packet) : ClientPacket(CMSG_CRAFTING_ORDER_LIST_CRAFTER_ORDERS, std::move(packet)) { }

        void Read() override;

        ObjectGuid NpcCraftOrderStation;
        int32 SkillLineAbilityID = 0;
    };

    // One order on the SMSG_CRAFTING_ORDER_LIST_ORDERS_RESPONSE wire (the client's JamCraftingOrder, reader
    // sub_7FF7291611C0 -> scalar head sub_7FF729160490). Field order + types are byte-exact and confirmed against
    // the reflection offset table (jam_reflection_FINAL_68275.json). Customer-provided reagents and the four
    // presence-gated optional sub-structs (customerPlayer / customerNpc / outputOrderItem / outputItem) are sent
    // absent for a basic browsed order, which is byte-exact for a public order without recraft/output data.
    struct CraftingOrderData
    {
        int32 Version = 0;
        uint64 OrderID = 0;
        int32 SkillLineAbilityID = 0;
        int32 OrderState = 0;
        uint8 OrderType = 0;
        uint8 MinQuality = 0;
        int64 EndDate = 0;
        int64 ClaimEndDate = 0;
        uint64 TipAmount = 0;
        uint64 HouseCutAmount = 0;
        int32 Flags = 0;
        ObjectGuid CustomerGUID;
        ObjectGuid CrafterGUID;
        int32 NpcCraftingOrderSetID = 0;
        int32 NpcTreasureID = 0;
        std::string CustomerNotes;
    };

    ByteBuffer& operator<<(ByteBuffer& data, CraftingOrderData const& order);

    // SMSG_CRAFTING_ORDER_LIST_ORDERS_RESPONSE (0x420333, client reader sub_7FF7290B9350). Header scalars whose
    // semantics are not offline-confirmable (ContextFlag / Field* / the two packed-bit bytes) are sent 0; the
    // recipe-summary vector is sent empty. Orders follow.
    class CraftingOrderListOrdersResponse final : public ServerPacket
    {
    public:
        CraftingOrderListOrdersResponse() : ServerPacket(SMSG_CRAFTING_ORDER_LIST_ORDERS_RESPONSE) { }

        WorldPacket const* Write() override;

        uint8 ContextFlag = 0;
        uint32 Field58 = 0;
        uint32 Field5C = 0;
        uint8 Field64 = 0;
        uint32 Field6C = 0;
        std::vector<CraftingOrderData> Orders;
    };
}
}

#endif // TRINITYCORE_CRAFTING_ORDER_PACKETS_H
