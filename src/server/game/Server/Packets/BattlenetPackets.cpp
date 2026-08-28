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

#include "BattlenetPackets.h"
#include "PacketOperators.h"
#include "PacketUtilities.h"

namespace WorldPackets::Battlenet
{
ByteBuffer& operator<<(ByteBuffer& data, MethodCall const& method)
{
    data << uint64(method.Type);
    data << uint64(method.ObjectId);
    data << uint32(method.Token);
    return data;
}

ByteBuffer& operator>>(ByteBuffer& data, MethodCall& method)
{
    data >> method.Type;
    data >> method.ObjectId;
    data >> method.Token;
    return data;
}

WorldPacket const* Notification::Write()
{
    _worldPacket << Method;
    _worldPacket << Size<uint32>(Data);
    _worldPacket.append(Data);

    return &_worldPacket;
}

WorldPacket const* Response::Write()
{
    _worldPacket << uint32(BnetStatus);
    _worldPacket << Method;
    _worldPacket << Size<uint32>(Data);
    _worldPacket.append(Data);

    return &_worldPacket;
}

WorldPacket const* ConnectionStatus::Write()
{
    _worldPacket << Bits<2>(State);
    _worldPacket << Bits<1>(SuppressNotification);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* ChangeRealmTicketResponse::Write()
{
    _worldPacket << uint32(Token);
    _worldPacket << Bits<1>(Allow);
    _worldPacket << Size<uint32>(Ticket);
    _worldPacket.append(Ticket);

    return &_worldPacket;
}

void Request::Read()
{
    uint32 protoSize;

    _worldPacket >> Method;
    _worldPacket >> protoSize;

    if (protoSize > 0xFFFF)
        OnInvalidArraySize(protoSize, 0xFFFF);

    if (protoSize)
    {
        Data.Resize(protoSize);
        _worldPacket.read(Data.GetWritePointer(), Data.GetRemainingSpace());
        Data.WriteCompleted(protoSize);
    }
}

void ChangeRealmTicket::Read()
{
    _worldPacket >> Token;
    _worldPacket.read(Secret.data(), Secret.size());
}

// ----------------------------------------------------------------------------------------
// Einheit w4_cmsg_43_3D - Block B4, Phase A. Belege: Client-Serializer 12.1.0.69382.
// ----------------------------------------------------------------------------------------

void AddBattlenetFriend::Read()
{
    // 0x6A8950: u64, u32, guid, u32, dann Bit-Sektion { 1 Bit, bits<6>, bits<6> } = 2 Byte.
    _worldPacket >> Field32;
    _worldPacket >> Field44;
    _worldPacket >> PlayerGUID;
    _worldPacket >> Field64;

    _worldPacket >> Bits<1>(Flag);
    _worldPacket >> SizedString::BitsSize<6>(Field68);
    _worldPacket >> SizedString::BitsSize<6>(Field117);
    _worldPacket.ResetBitPos();

    _worldPacket >> SizedString::Data(Field68);
    _worldPacket >> SizedString::Data(Field117);
}

void BattlenetChallengeResponse::Read()
{
    // 0x6ACD70: das Laengenfeld und die Zeichenkette existieren nur, wenn Tag == 5.
    _worldPacket >> ChallengeID;
    _worldPacket >> Bits<3>(Tag);

    if (Tag == TagWithResponse)
        _worldPacket >> SizedString::BitsSize<6>(Response);

    _worldPacket.ResetBitPos();

    if (Tag == TagWithResponse)
        _worldPacket >> SizedString::Data(Response);
}

void ClubPresenceSubscribe::Read()
{
    // 0x6AF0E0
    _worldPacket >> Bits<1>(Subscribe);
    _worldPacket.ResetBitPos();

    _worldPacket >> Size<uint32>(ClubIDs);
    for (uint64& clubId : ClubIDs)
        _worldPacket >> clubId;
}

void SendCharacterClubInvitation::Read()
{
    // 0x6AF2D0: zwei getrennte Bit-Sektionen zu je einer 9-Bit-Laenge (je 2 Byte).
    _worldPacket >> Field32;
    _worldPacket >> Field40;
    _worldPacket >> Field48;
    _worldPacket >> Token;
    _worldPacket >> CharacterGUID;
    _worldPacket >> Field80;

    _worldPacket >> SizedString::BitsSize<9>(Field88);
    _worldPacket.ResetBitPos();
    _worldPacket >> SizedString::Data(Field88);

    _worldPacket >> SizedString::BitsSize<9>(Field394);
    _worldPacket.ResetBitPos();
    _worldPacket >> SizedString::Data(Field394);

    uint32 blobSize = 0;
    _worldPacket >> blobSize;
    if (blobSize > _worldPacket.size() - _worldPacket.rpos())
        _worldPacket.OnInvalidPosition(_worldPacket.rpos(), blobSize);

    Blob.resize(blobSize);
    if (blobSize)
        _worldPacket.read(Blob.data(), blobSize);
}

void RequestCharacterGuildFollowInfo::Read()
{
    // 0x6AA740
    _worldPacket >> CharacterGUID;
    _worldPacket >> VirtualRealmAddress;
}

void SaveAccountDataExport::Read()
{
    // 0x6B22C0
    _worldPacket >> Size<uint32>(Characters);
    for (ObjectGuid& character : Characters)
        _worldPacket >> character;
}

void RequestRealmGuildMasterInfo::Read()
{
    // 0x6B2750
    _worldPacket >> VirtualRealmAddress;

    _worldPacket >> Size<uint32>(Characters);
    for (ObjectGuid& character : Characters)
        _worldPacket >> character;
}
}
