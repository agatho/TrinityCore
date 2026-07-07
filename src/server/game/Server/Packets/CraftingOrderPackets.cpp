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

#include "CraftingOrderPackets.h"

namespace WorldPackets::CraftingOrders
{
void ClientContext::Read(ByteBuffer& data)
{
    data.ResetBitPos();
    // both string lengths and both flags are packed together, then the bytes follow (matches sub_7FF72906D6F0)
    uint32 len1 = data.ReadBits(10);
    uint32 len2 = data.ReadBits(10);
    Flag1 = data.ReadBit();
    Flag2 = data.ReadBit();
    auto readBytes = [&data](uint32 encoded) -> std::string
    {
        if (!encoded)
            return {};
        std::string s;
        s.resize(encoded - 1);
        for (uint32 i = 0; i < encoded - 1; ++i)
            s[i] = data.read<char>();
        return s;
    };
    String1 = readBytes(len1);
    String2 = readBytes(len2);
}

void CraftingOrderCreate::Read()
{
    _worldPacket >> SkillLineAbilityID;
    _worldPacket >> OrderType;
    _worldPacket >> MinQuality;
    _worldPacket >> TipAmount;

    uint32 counts[4];
    for (uint32& c : counts)
        _worldPacket >> c;

    _worldPacket >> TargetGUID;                    // PackedGuid

    if (OrderType == 1 || OrderType == 2)
        SecondaryId = _worldPacket.read<uint32>();

    // --- length/flag block: a byte-aligned high byte for the notes length, then the bit accumulator ---
    uint8 notesLenHigh = _worldPacket.read<uint8>();
    _worldPacket.ResetBitPos();
    uint32 notesLenLow = _worldPacket.ReadBits(2);
    Flag1 = _worldPacket.ReadBit();
    Flag2 = _worldPacket.ReadBit();
    uint32 notesLen = (uint32(notesLenHigh) << 2) | notesLenLow;

    uint32 recraftNoteLen = 0;
    if (OrderType == 2)
    {
        // orderType 2 (personal/recraft) also carries a second length here (same byte+2bit split)
        uint8 recraftHigh = _worldPacket.read<uint8>();
        _worldPacket.ResetBitPos();
        recraftNoteLen = (uint32(recraftHigh) << 2) | _worldPacket.ReadBits(2);
    }

    auto readSlot = [this](std::vector<CraftingReagentSlot>& out, uint32 count, bool withReagentBase)
    {
        out.resize(count);
        for (CraftingReagentSlot& slot : out)
        {
            _worldPacket >> slot.Field1;
            _worldPacket >> slot.Field2;
            if (withReagentBase)
                _worldPacket >> slot.Reagent;          // WorldPackets::Crafting::CraftingReagentBase operator>>
            _worldPacket.ResetBitPos();
            if (_worldPacket.ReadBit())
                slot.Extra = _worldPacket.read<uint8>();
        }
    };

    // Mirror the serializer's exact interleave: vec[0] elements, then the notes bytes + optional guid +
    // recraft-note bytes, then vec[1..3] elements, then the optional trailing context.
    readSlot(Vectors[0], counts[0], false);

    if (notesLen)
    {
        CustomerNotes.resize(notesLen);
        for (uint32 i = 0; i < notesLen; ++i)
            CustomerNotes[i] = _worldPacket.read<char>();
    }

    if (Flag1)
    {
        ObjectGuid guid;
        _worldPacket >> guid;
        OptionalGuid = guid;
    }

    if (OrderType == 2 && recraftNoteLen)
    {
        RecraftNote.resize(recraftNoteLen);
        for (uint32 i = 0; i < recraftNoteLen; ++i)
            RecraftNote[i] = _worldPacket.read<char>();
    }

    readSlot(Vectors[1], counts[1], false);
    readSlot(Vectors[2], counts[2], true);
    readSlot(Vectors[3], counts[3], true);

    if (Flag2)
        Context.Read(_worldPacket);
}

void CraftingOrderClaim::Read()
{
    _worldPacket >> OrderID;
    _worldPacket >> Field2;
    _worldPacket.ResetBitPos();
    HasContext = _worldPacket.ReadBit();
    if (HasContext)
        Context.Read(_worldPacket);
}

void CraftingOrderCancel::Read()
{
    _worldPacket >> NpcGUID;                        // PackedGuid
    _worldPacket >> OrderID;
    _worldPacket.ResetBitPos();
    HasContext = _worldPacket.ReadBit();
    if (HasContext)
        Context.Read(_worldPacket);
}

void CraftingOrderRelease::Read()
{
    _worldPacket >> OrderID;
    _worldPacket >> Field2;
    _worldPacket.ResetBitPos();
    HasContext = _worldPacket.ReadBit();
    if (HasContext)
        Context.Read(_worldPacket);
}

void CraftingOrderReject::Read()
{
    _worldPacket >> OrderID;
    _worldPacket >> Field2;

    // The reason string's length is packed here (byte-aligned high byte + 2 accumulator bits), together with
    // the hasContext bit; the reason bytes themselves follow after the optional context (matches sub_7FF7291552B0).
    uint8 reasonLenHigh = _worldPacket.read<uint8>();
    _worldPacket.ResetBitPos();
    uint32 reasonLen = (uint32(reasonLenHigh) << 2) | _worldPacket.ReadBits(2);
    HasContext = _worldPacket.ReadBit();

    if (HasContext)
        Context.Read(_worldPacket);

    if (reasonLen)
    {
        Reason.resize(reasonLen);
        for (uint32 i = 0; i < reasonLen; ++i)
            Reason[i] = _worldPacket.read<char>();
    }
}

void CraftingOrderFulfill::Read()
{
    // Wire is byte-identical to CraftingOrderReject (client serializers sub_7FF729155000 == sub_7FF7291552B0).
    _worldPacket >> OrderID;
    _worldPacket >> Field2;

    uint8 noteLenHigh = _worldPacket.read<uint8>();
    _worldPacket.ResetBitPos();
    uint32 noteLen = (uint32(noteLenHigh) << 2) | _worldPacket.ReadBits(2);
    HasContext = _worldPacket.ReadBit();

    if (HasContext)
        Context.Read(_worldPacket);

    if (noteLen)
    {
        Note.resize(noteLen);
        for (uint32 i = 0; i < noteLen; ++i)
            Note[i] = _worldPacket.read<char>();
    }
}

WorldPacket const* CraftingOrderActionResult::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint64(CraftingOrderID);

    return &_worldPacket;
}

WorldPacket const* CraftingOrderFulfillResult::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint64(CraftingOrderID);
    _worldPacket << uint64(0);                     // Field2 (unknown semantics)
    _worldPacket << uint8(0);                      // Field3 (unknown semantics)
    _worldPacket << ObjectGuid::Empty;             // Field4 PackedGuid (delivered-item/crafter guid; not resolvable)
    _worldPacket << uint32(0);                     // Field5
    _worldPacket << uint32(0);                     // Field6
    _worldPacket << uint32(0);                     // Field7

    return &_worldPacket;
}

void CraftingOrderListCrafterOrders::Read()
{
    _worldPacket >> NpcCraftOrderStation;          // PackedGuid (the crafting station / NPC)
    _worldPacket >> SkillLineAbilityID;            // recipe filter; remaining filter fields are not needed
}

ByteBuffer& operator<<(ByteBuffer& data, CraftingOrderData const& order)
{
    // Scalar head, byte-exact per client reader sub_7FF729160490 (offsets confirmed vs jam_reflection_FINAL_68275.json).
    data << int32(order.Version);
    data << uint64(order.OrderID);
    data << int32(order.SkillLineAbilityID);
    data << int32(order.OrderState);
    data << uint8(order.OrderType);
    data << uint8(order.MinQuality);
    data << int64(order.EndDate);
    data << int64(order.ClaimEndDate);
    data << uint64(order.TipAmount);
    data << uint64(order.HouseCutAmount);
    data << int32(order.Flags);
    data << order.CustomerGUID;                    // PackedGuid (targetGUID)
    data << order.CrafterGUID;                     // PackedGuid
    data << int32(order.NpcCraftingOrderSetID);
    data << int32(order.NpcTreasureID);

    // reagent count, then the split notes-length + presence byte. notesLen = (byte1 << 2) | (byte2 >> 6);
    // byte2 bits 5..2 are the four optional-presence flags: bit5 (0x20) customerPlayer, bit4 (0x10) customerNpc,
    // bit3 (0x08) outputOrderItem, bit2 (0x04) outputItem (bit assignment confirmed from reader sub_7FF729160490).
    // customerPlayer is set for player orders; customerNpc/output sub-structs are not produced yet. The reagent
    // vector (each a JamCraftingOrderItem) is emitted between the two note bytes and the notes text — the exact
    // position the client reader expects (validated byte-exact vs the live sniff).
    uint32 notesLen = std::min<uint32>(uint32(order.CustomerNotes.length()), 1023);
    uint8 presence = order.HasCustomerPlayer ? 0x20 : 0;
    data << uint32(uint32(order.Reagents.size()));
    data << uint8(notesLen >> 2);
    data << uint8(uint8((notesLen & 0x3) << 6) | presence);

    for (CraftingOrderReagentData const& reagent : order.Reagents)
    {
        // JamCraftingOrderItem, mirror of client reader sub_7FF72915FF20 (validated byte-exact vs the sniff).
        data << uint64(reagent.OrderItemID);
        data << int32(reagent.OrderItemType);
        data << reagent.ItemGUID;                  // PackedGuid (empty for a posting)
        data << reagent.OwnerGUID;                 // PackedGuid (the customer)
        data << uint32(reagent.Quantity);
        data << int32(reagent.CraftingQualityID);
        data << int32(reagent.Flags);

        // reagent base (client sub_7FF7291B42A0): presence byte { bit7 = has itemID, bit6 = has currencyID },
        // then the present values as uint32.
        uint8 basePresence = (reagent.ReagentItemID ? 0x80 : 0) | (reagent.ReagentCurrencyID ? 0x40 : 0);
        data << uint8(basePresence);
        if (reagent.ReagentItemID)
            data << int32(reagent.ReagentItemID);
        if (reagent.ReagentCurrencyID)
            data << int32(reagent.ReagentCurrencyID);

        // optional slot (client tail of sub_7FF72915FF20): presence byte bit7, then the u8 slot. Customer orders
        // always carry a dataSlotIndex, so it is emitted present.
        data << uint8(0x80);
        data << uint8(reagent.Slot);
    }

    if (notesLen)
        data.append(order.CustomerNotes.data(), notesLen);

    // Optional sub-structs, in reader order (customerPlayer, customerNpc, outputOrderItem, outputItem). Only
    // customerPlayer is emitted (when present); it is two PackedGuids: the customer and their WoW-account guid.
    if (order.HasCustomerPlayer)
    {
        data << order.CustomerGUID;                // JamCraftingOrderCustomerPlayer.guid
        data << order.CustomerWowAccount;          // JamCraftingOrderCustomerPlayer.wowAccount
    }

    // wrapper header byte (client sub_7FF7291611C0): recraftItem presence bit + packed recraft sub-vector counts,
    // all zero for a basic order (no recraft item, enchants, or gems).
    data << uint8(0);
    return data;
}

WorldPacket const* CraftingOrderUpdateState::Write()
{
    _worldPacket << uint64(OrderID);
    _worldPacket << uint8(0);                      // Field8 (observed 0)
    _worldPacket << uint8(OrderState);
    _worldPacket << uint16(0);                     // Field10 (observed 0)
    _worldPacket << CrafterGUID;                   // PackedGuid
    _worldPacket << int32(SkillLineAbilityID);
    _worldPacket << uint32(0);                     // Field25 (observed 0)
    _worldPacket << uint8(OrderType);
    _worldPacket << uint32(0);                     // Field30 (unknown semantics -> 0)
    _worldPacket << uint32(0);                     // Field34 (unknown semantics -> 0)

    return &_worldPacket;
}

WorldPacket const* CraftingOrderListOrdersResponse::Write()
{
    _worldPacket << uint8(ContextFlag);
    _worldPacket << uint32(0);                     // recipe-summary vector count (none)
    _worldPacket << uint32(Orders.size());         // order count
    _worldPacket << uint32(Field58);
    _worldPacket << uint32(Field5C);
    _worldPacket << uint8(0);                       // two packed bits (unconfirmed semantics) — cleared
    _worldPacket << uint8(Field64);
    _worldPacket << uint32(Field6C);
    _worldPacket << uint8(0);                       // four packed bits (unconfirmed semantics) — cleared

    for (CraftingOrderData const& order : Orders)
        _worldPacket << order;

    return &_worldPacket;
}
}
