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

    // CMSG_CRAFTING_ORDER_FULFILL (0x3B011D): the crafter confirms a claimed order is crafted and delivers it.
    // Serializer sub_7FF729155000 is BYTE-IDENTICAL to CMSG_CRAFTING_ORDER_REJECT's (sub_7FF7291552B0), so the wire
    // is the same: { u64 OrderID; u8 Field2; string (the reject/note slot, empty for a plain fulfil); bit hasContext;
    // [ClientContext] }. No crafted-item payload rides the wire — the server derives the output from the order's
    // recipe (SkillLineAbility -> spell -> SPELL_EFFECT_CREATE_ITEM), matching the client's craft-then-fulfil flow.
    class CraftingOrderFulfill final : public ClientPacket
    {
    public:
        explicit CraftingOrderFulfill(WorldPacket&& packet) : ClientPacket(CMSG_CRAFTING_ORDER_FULFILL, std::move(packet)) { }

        void Read() override;

        uint64 OrderID = 0;
        uint8 Field2 = 0;
        std::string Note;
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
    // SNIFF-CONFIRMED byte-exact (C:\sniff\ingame-shop_ordersCrafting_professions.pkt, SMSG_CRAFTING_ORDER_CLAIM_RESULT
    // 0x420334: "00 c112bb3600000000" = {Result=Ok(0), OrderID=0x36bb12c1}). No longer a hypothesis.
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

    // SMSG_CRAFTING_ORDER_FULFILL_RESULT (0x420337): unlike the other results this carries a richer body. Recovered
    // byte-exact from the client reader sub_7FF7290B9950 (all reads are plain — the "CompressedUInt32" reader
    // 0x7FF72BE6C410 is ReadUInt32): { u8 Result; u64 OrderID; u64 Field2; u8 Field3; PackedGUID Field4; u32 Field5;
    // u32 Field6; u32 Field7 }. Only Result + OrderID have offline-confirmable meaning; Field2..7 (likely the
    // delivered item guid / quality / counts) have no resolvable semantics and are sent 0/empty (honest, structurally
    // exact — the client reads a valid packet; the fulfilment itself is also signalled via UPDATE_STATE).
    class CraftingOrderFulfillResult final : public ServerPacket
    {
    public:
        CraftingOrderFulfillResult() : ServerPacket(SMSG_CRAFTING_ORDER_FULFILL_RESULT, 1 + 8 + 8 + 1 + 2 + 4 + 4 + 4) { }

        WorldPacket const* Write() override;

        CraftingOrderResult Result = CraftingOrderResult::Ok;
        uint64 CraftingOrderID = 0;
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

    // CMSG_NPC_CRAFTING_ORDER_REQUEST (0x3B012F): the client asks for the list of available NPC (patron) work orders
    // when opening a crafting-order NPC. SNIFF-CONFIRMED empty body (size 4 = bare opcode, 4 captures in
    // C:\sniff\ingame-shop_ordersCrafting_professions.pkt), so Read consumes nothing. The server answers with a
    // SMSG_CRAFTING_ORDER_LIST_ORDERS_RESPONSE carrying the OrderType::Npc orders (empty until content is authored).
    class NpcCraftingOrderRequest final : public ClientPacket
    {
    public:
        explicit NpcCraftingOrderRequest(WorldPacket&& packet) : ClientPacket(CMSG_NPC_CRAFTING_ORDER_REQUEST, std::move(packet)) { }

        void Read() override { }
    };

    // One customer-provided reagent on the JamCraftingOrder wire (the client's JamCraftingOrderItem, reader
    // sub_7FF72915FF20). Recovered byte-exact and VALIDATED against the live sniff: every one of the 23 orders in
    // C:\sniff\ingame-shop_ordersCrafting_professions.pkt parses cleanly with this layout (the whole 2054-byte and
    // 1875-byte responses consume to the byte, 0 leftover). The block is entirely flat scalars/GUIDs — the earlier
    // "needs full ItemInstance serialization" note was a misattribution: the ItemInstance readers (sub_7FF7291CBD40)
    // belong to the JamCliCraftingOrder *recraft* fields (recraftItem / recraftItemGems), NOT to an order's reagents.
    // For a customer posting the wire carries: orderItemID/type/itemGUID/qualityID = 0, ownerGUID = the customer's
    // GUID, flags = 1, reagent = { itemID (present), currencyID (absent) }, quantity, and an optional u8 slot.
    struct CraftingOrderReagentData
    {
        uint64 OrderItemID = 0;
        int32 OrderItemType = 0;
        ObjectGuid ItemGUID;
        ObjectGuid OwnerGUID;                   // the customer who provided the reagent
        uint32 Quantity = 0;
        int32 CraftingQualityID = 0;
        int32 Flags = 1;                        // 1 = customer-provided (constant in the capture)
        int32 ReagentItemID = 0;
        int32 ReagentCurrencyID = 0;
        uint8 Slot = 0;
    };

    // One order on the SMSG_CRAFTING_ORDER_LIST_ORDERS_RESPONSE wire (the client's JamCraftingOrder, reader
    // sub_7FF7291611C0 -> scalar head sub_7FF729160490). Field order + types are byte-exact and confirmed against
    // the reflection offset table (jam_reflection_FINAL_68275.json) AND against a live sniff
    // (C:\sniff\ingame-shop_ordersCrafting_professions.pkt): the scalar head decodes cleanly for every order in a
    // 12-order response (e.g. OID 0x36bb12bc, SkillLineAbilityID 52199, OrderState 2, OrderType 3, MinQuality 1,
    // TipAmount 739311, Flags 4). Customer-provided reagents are now emitted (see Reagents); the four presence-gated
    // optional sub-structs (customerPlayer / customerNpc / outputOrderItem / outputItem) are still sent absent, which
    // is byte-exact for a public player order without recraft/output data.
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
        std::vector<CraftingOrderReagentData> Reagents;
        // customerPlayer sub-struct (client @1176, JamCraftingOrderCustomerPlayer = { PackedGuid guid, PackedGuid
        // wowAccount }): present for a player-placed order so the client can show who ordered. Emitted when
        // HasCustomerPlayer is set; NPC/patron orders would instead carry customerNpc (not produced yet).
        bool HasCustomerPlayer = false;
        ObjectGuid CustomerWowAccount;
    };

    ByteBuffer& operator<<(ByteBuffer& data, CraftingOrderData const& order);

    // SMSG_CRAFTING_ORDER_UPDATE_STATE (0x42033D): pushed to the customer + crafter when an order changes state
    // (claimed / released / rejected / cancelled / fulfilled) so open browse windows refresh live. Layout is
    // sniff-resolved (C:\sniff\ingame-shop_ordersCrafting_professions.pkt, cross-checked against the same order in
    // LIST_ORDERS_RESPONSE — OrderID/State/CrafterGUID/SkillLineAbilityID/OrderType all match byte-exact):
    //   { u64 OrderID; u8 (0); u8 OrderState; u16 (0); PackedGUID CrafterGUID; u32 SkillLineAbilityID; u32 (0);
    //     u8 OrderType; u32 Field30; u32 Field34 }.
    // Field30/Field34 carry small per-order values in the capture (~96 / ~38) that map to none of the order's known
    // fields (tip/cut/dates/quality); their semantics are not offline-resolvable, so they are sent 0 (honest unknown).
    class CraftingOrderUpdateState final : public ServerPacket
    {
    public:
        CraftingOrderUpdateState() : ServerPacket(SMSG_CRAFTING_ORDER_UPDATE_STATE, 8 + 1 + 1 + 2 + 9 + 4 + 4 + 1 + 4 + 4) { }

        WorldPacket const* Write() override;

        uint64 OrderID = 0;
        uint8 OrderState = 0;
        ObjectGuid CrafterGUID;
        int32 SkillLineAbilityID = 0;
        uint8 OrderType = 0;
    };

    // SMSG_CRAFTING_ORDER_LIST_ORDERS_RESPONSE (0x420333, client reader sub_7FF7290B9350). Header layout is
    // SNIFF-CONFIRMED byte-exact (C:\sniff\ingame-shop_ordersCrafting_professions.pkt): { u8 ContextFlag; u32
    // recipeSummaryCount; u32 orderCount; u32 Field58; u32 Field5C; u8 packedBits2; u8 Field64; u32 Field6C;
    // u8 packedBits4; orders[] }. An empty response (orderCount 0) parses to exactly the 24-byte header; a 12-order
    // response parses cleanly with orderCount 12. The Field58/Field5C/Field64 + packed-bit scalars carry live values
    // in the capture (informational; their semantics are still not resolvable offline) and are sent 0 here, which the
    // client accepts. The recipe-summary vector is sent empty.
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

    // CMSG_CRAFTING_ORDER_GET_NPC_REWARD_INFO (0x3B011A): the client asks for the reward preview of a set of NPC
    // (patron) work orders it is currently browsing. Wire recovered byte-exact from a live sniff
    // (C:\sniff\ingame-shop_ordersCrafting_professions.pkt, one 252-byte capture parses to the byte): a u32 count, a
    // u32 context field (echoed back in the response — 1 in the capture), then count records of
    // { u64 OrderID; u32 Field1; u32 Field2; u32 Field3 }. Only OrderID has offline-confirmable meaning (it matches
    // the order ids in the paired SMSG_CRAFTING_ORDER_NPC_REWARD_INFO); Field1..3 are read byte-exact but their
    // semantics (likely recipe/quality/set hints the client already knows) are not needed server-side.
    struct NpcRewardInfoRequest
    {
        uint64 OrderID = 0;
        uint32 Field1 = 0;
        uint32 Field2 = 0;
        uint32 Field3 = 0;
    };

    class CraftingOrderGetNpcRewardInfo final : public ClientPacket
    {
    public:
        explicit CraftingOrderGetNpcRewardInfo(WorldPacket&& packet) : ClientPacket(CMSG_CRAFTING_ORDER_GET_NPC_REWARD_INFO, std::move(packet)) { }

        void Read() override;

        uint32 ContextField = 0;
        std::vector<NpcRewardInfoRequest> Orders;
    };

    // CMSG_CRAFTING_ORDER_UPDATE_IGNORE_LIST (0x3B0121): the player pushes their full crafting-order ignore list.
    // Wire recovered from the client serializer (sub_7FF7291556E0) by disassembly (no sniff needed): after the
    // opcode, a 6-bit packed count, then count PackedGuids. Decoding evidence: the serializer writes the opcode as a
    // leading u32 (0x3B0121), then the count via the 6-bit bit-count writer sub_7FF729064CE0 (masks to 0x3F; the
    // >=64 path is a bare return, so the field is a fixed 6-bit count, max 63), flushes bits, then loops count times
    // writing each 16-byte guid via the PackedGuid serializer sub_7FF72BEBDED0 (18-byte reserve + mask-per-nonzero-
    // byte). The client re-sends the whole list on login/change, so the server stores it wholesale (no delta, no
    // persistence needed).
    class CraftingOrderUpdateIgnoreList final : public ClientPacket
    {
    public:
        explicit CraftingOrderUpdateIgnoreList(WorldPacket&& packet) : ClientPacket(CMSG_CRAFTING_ORDER_UPDATE_IGNORE_LIST, std::move(packet)) { }

        void Read() override;

        std::vector<ObjectGuid> IgnoredPlayers;
    };

    // SMSG_CRAFTING_ORDER_NPC_REWARD_INFO (0x42033E): the paired reply, byte-exact from the same sniff (430-byte
    // capture). Header: { u32 count; u32 ContextField (echoes the request) }, then count records of
    // { u64 OrderID; u32 rewardCount; rewardCount x <reward blob> }. The reward blob is a reflection-serialized,
    // variable-length record carrying the patron's item/currency rewards — that is authored NPC-order CONTENT (the
    // specific per-order rewards live on Blizzard's servers, not in any offline data), so it is NOT synthesized here.
    // The server answers with the NPC orders it actually knows about (from CraftingOrderMgr); each such order emits
    // rewardCount=0 until reward content is attached, which is byte-exact (the client accepts a zero-reward order) and
    // honest. With no NPC-order content configured the reply is a bare header (count 0) — the truthful "no NPC orders"
    // answer, exactly like an empty browse response.
    struct NpcRewardInfoEntry
    {
        uint64 OrderID = 0;
        // Rewards are authored content (see class note); none are emitted yet, so rewardCount is written as 0.
    };

    class CraftingOrderNpcRewardInfo final : public ServerPacket
    {
    public:
        CraftingOrderNpcRewardInfo() : ServerPacket(SMSG_CRAFTING_ORDER_NPC_REWARD_INFO, 4 + 4) { }

        WorldPacket const* Write() override;

        uint32 ContextField = 0;
        std::vector<NpcRewardInfoEntry> Entries;
    };
}
}

#endif // TRINITYCORE_CRAFTING_ORDER_PACKETS_H
