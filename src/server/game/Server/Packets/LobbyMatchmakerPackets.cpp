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

#include "LobbyMatchmakerPackets.h"
#include "PacketOperators.h"

namespace WorldPackets::LobbyMatchmaker
{
// Serializer 0x6BEB30 (NICHT 0x68EB30 - siehe Kopfkommentar des Headers).
// Die beiden Zaehler stehen VORNE hintereinander, beide Elementbloecke kommen danach:
//   006BEB76  mov edx, dword ptr [r14 + 0x10]   -> Write<u32> CountA
//   006BEB82  mov edx, dword ptr [r14 + 0x28]   -> Write<u32> CountB
//   006BEBA0  <Schleife CountA>
//   006BEBD2  <Schleife CountB>
// Wer stattdessen CountA/PaareA/CountB/PaareB liest, verliert bei jeder nichtleeren Anpassung
// die Synchronisation. Unabhaengig bestaetigt an der Leseseite 0x6BEC20.
ByteBuffer& operator>>(ByteBuffer& data, CharacterCustomizationBlock& block)
{
    data >> block.RaceID;
    data >> block.ClassID;
    data >> block.SexID;

    data >> Size<uint32>(block.Customizations);
    data >> Size<uint32>(block.RandomCustomizations);

    for (Character::ChrCustomizationChoice& choice : block.Customizations)
        data >> choice;

    for (Character::ChrCustomizationChoice& choice : block.RandomCustomizations)
        data >> choice;

    return data;
}

// JamFastLoginDestination, Rumpf 0x6BEF20 Zeilen 0x6BEF6A..0x6BEF99 bzw. 0x6BEFAA..0x6BEFDC.
// gameMode ist ein uint8 (0x6BEF82: movzx edx, byte ptr [rdi+0x20] -> Write<uint8>), nicht uint32.
ByteBuffer& operator>>(ByteBuffer& data, FastLoginDestination& destination)
{
    data >> destination.RealmAddress;
    data >> destination.CharacterGUID;
    data >> destination.GameMode;
    data >> destination.MapID;
    data >> destination.Customization;

    return data;
}

void LobbyMatchmakerPartyInvite::Read()
{
    _worldPacket >> TargetGUID;
}

void LobbyMatchmakerAcceptPartyInvite::Read()
{
    _worldPacket >> TargetGUID;
}

void LobbyMatchmakerRejectPartyInvite::Read()
{
    _worldPacket >> TargetGUID;
}

void LobbyMatchmakerPartyUninvite::Read()
{
    _worldPacket >> TargetGUID;
}

void LobbyMatchmakerSetPartyPlaylistEntry::Read()
{
    _worldPacket >> PlaylistEntryID;
}

void LobbyMatchmakerSetPlayerReady::Read()
{
    _worldPacket >> Bits<1>(IsReady);
    _worldPacket.ResetBitPos();
}

void LobbyMatchmakerQueueProposalResponse::Read()
{
    _worldPacket >> Bits<1>(Accept);
    _worldPacket.ResetBitPos();
}

void LobbyMatchmakerEnterQueue::Read()
{
    // Writer 0x6B2150: Write<u32>, dann call 0x6BEB30, dann WritePackedGuid.
    _worldPacket >> PlaylistEntryID;
    _worldPacket >> Customization;
    _worldPacket >> CharacterGUID;
}

void LobbyMatchmakerCreateCharacter::Read()
{
    // Writer 0x6B23E0: zwei uint8, dann uint32 Count, dann Count Paare a 8 Byte.
    // Benutzt den gemeinsamen Anpassungsblock NICHT - es gibt nur EINE Liste und kein drittes
    // uint8.
    _worldPacket >> Field32;
    _worldPacket >> Field33;

    _worldPacket >> Size<uint32>(Customizations);
    for (Character::ChrCustomizationChoice& choice : Customizations)
        _worldPacket >> choice;
}

void RegisterFastLogin::Read()
{
    // Rumpf 0x6BEF20: das Bit und sein FlushBits stehen VOR allem anderen.
    _worldPacket >> Bits<1>(DoFastLogin);
    _worldPacket.ResetBitPos();

    _worldPacket >> ToDestination;
    _worldPacket >> FromDestination;
}

WorldPacket const* WowLabsPartyError::Write()
{
    // Reader 0x60DE70 liest ein ganzes Byte und nimmt b >> 4 - also bits<4>, MSB-first.
    _worldPacket << Bits<4>(Error);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* LobbyMatchmakerQueueResult::Write()
{
    // Reader 0x60D1A0 liest ein ganzes Byte und nimmt b >> 5 - also bits<3>, MSB-first.
    _worldPacket << Bits<3>(Status);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* LobbyMatchmakerReceiveInvite::Write()
{
    _worldPacket << InviterGuid;                                   // PackedGuid
    _worldPacket << uint8(InviterName.length() << 2);              // len in high 6 bits (parser does byte>>2)
    _worldPacket.append(InviterName.data(), InviterName.length());

    return &_worldPacket;
}

WorldPacket const* LobbyMatchmakerPartyInviteRejected::Write()
{
    _worldPacket << uint8(Name.length() << 2);
    _worldPacket.append(Name.data(), Name.length());

    return &_worldPacket;
}

WorldPacket const* LobbyMatchmakerPartyInfo::Write()
{
    auto writeMember = [this](LobbyMatchmakerPartyInfoMember const& m)
    {
        _worldPacket << uint8((m.Name.length() << 2) | (m.ReadyBit ? 0x2 : 0x0));   // nameLen<<2 | bit1
        _worldPacket << m.MemberGuid;                             // PackedGuid
        _worldPacket << m.AccountGuid;                            // PackedGuid
        _worldPacket << uint64(m.Field88);
        _worldPacket << uint8(m.Field97);
        _worldPacket << uint8(m.Field98);
        _worldPacket << uint32(0);                               // countA (sub-list A empty)
        for (uint32 v : m.Loadout)                              // 19x uint32 cosmetic loadout
            _worldPacket << uint32(v);
        _worldPacket << uint32(0);                               // countB (sub-list B empty)
        _worldPacket.append(m.Name.data(), m.Name.length());    // name body (after countB, per the parser)
    };

    _worldPacket << LeaderGuid;                                   // PackedGuid
    _worldPacket << uint32(PlaylistEntry);
    _worldPacket << uint32(Members.size());                       // count1 (flat u32)
    _worldPacket << uint32(Invited.size());                       // count2 (flat u32)
    _worldPacket << Guid3;                                        // PackedGuid (outer role unproven; empty)
    _worldPacket << Guid4;                                        // PackedGuid
    _worldPacket << uint8(FlagByte);
    for (LobbyMatchmakerPartyInfoMember const& m : Members)
        writeMember(m);
    for (LobbyMatchmakerPartyInfoMember const& m : Invited)
        writeMember(m);

    return &_worldPacket;
}
}

namespace WorldPackets::WowLabs
{
void QuerySelectedWowLabsArea::Read()
{
    // Writer 0x6D3810 - eine gepackte ObjectGuid, sonst nichts.
    _worldPacket >> PlayerGUID;
}

void QueryWowLabsAreaInfo::Read()
{
    // Writer 0x6D3860 - dieselbe Form.
    _worldPacket >> PlayerGUID;
}

void SelectWowLabsArea::Read()
{
    // Writer 0x6D38B0 - gepackte ObjectGuid, dann das Lua-Argument wowLabsAreaID.
    _worldPacket >> PlayerGUID;
    _worldPacket >> WowLabsAreaID;
}

WorldPacket const* WowLabsSetWowLabsAreaIdResponse::Write()
{
    // Reader 0x60D440: Read<uint8> und (b >> 7) - ein einzelnes Bit, genau 1 Byte.
    _worldPacket << Bits<1>(Success);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* QuerySelectedWowLabsAreaResponse::Write()
{
    // Reader 0x60D4B0 nimmt den Rest des Pakets als Zeiger und liest daraus vier Byte, ohne
    // Laengenpruefung - es MUESSEN genau vier sein.
    _worldPacket << int32(WowLabsAreaID);

    return &_worldPacket;
}

WorldPacket const* WowLabsAreaSelected::Write()
{
    // Reader 0x60D640 - byteweise gleich geformt wie 0x450329.
    _worldPacket << int32(WowLabsAreaID);

    return &_worldPacket;
}

WorldPacket const* QueryWowLabsAreaInfoResponse::Write()
{
    // Reader 0x60D530: Read<uint32> Count, dann Count Elemente a fuenf uint32/float.
    _worldPacket << Size<uint32>(Areas);

    for (WowLabsAreaOption const& area : Areas)
    {
        _worldPacket << int32(area.WowLabsAreaID);
        _worldPacket << int32(area.AreaType);
        _worldPacket << float(area.X);
        _worldPacket << float(area.Y);
        _worldPacket << float(area.Z);
    }

    return &_worldPacket;
}
}
