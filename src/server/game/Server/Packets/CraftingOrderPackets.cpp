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

WorldPacket const* CraftingOrderActionResult::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint64(CraftingOrderID);

    return &_worldPacket;
}
}
