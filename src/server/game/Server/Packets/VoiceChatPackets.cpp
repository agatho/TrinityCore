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

#include "VoiceChatPackets.h"
#include "PacketOperators.h"

namespace WorldPackets::VoiceChat
{
namespace
{
// Die Antworten dieses Blocks benutzen ReadDynString (0x347D750) - Laenge INKLUSIVE NUL, NUL am
// Draht, und Laenge <= 1 verbraucht NULL Byte. Das gilt hier auch fuer schmale Laengenfelder
// (6, 7 und 9 Bit), nicht nur fuer die 24-Bit-JamDynamicString.
//
//   if (a3 > 1) { consume a3 bytes; if (buf[a3-1] != 0) return 0; }   // NUL Pflicht
//   else        { *dst = 0; return 1; }                               // NICHTS verbraucht
//
// Wer fuer den leeren String len = 1 plus ein NUL-Byte schreibt, verschiebt alles Nachfolgende
// um genau ein Byte. Leer heisst len = 0 UND null Bytes.
template<std::size_t Bits>
void WriteDynStringLength(ByteBuffer& data, std::string const& value)
{
    data.WriteBits(value.empty() ? 0u : uint32(value.length() + 1), Bits);
}

void WriteDynStringData(ByteBuffer& data, std::string const& value)
{
    if (value.empty())
        return;

    data.WriteString(value);
    data << uint8(0);
}
}

void VoiceChannelSttTokenRequest::Read()
{
    // Writer 0x6AFED0: bits<7> Len (strnlen-Zaehler 71), FLUSH, dann die Bytes OHNE NUL.
    // Gegenlaeufig zur Antwortseite - hier ist die NUL nicht am Draht.
    _worldPacket >> SizedString::BitsSize<7>(ChannelId);
    _worldPacket.ResetBitPos();
    _worldPacket >> SizedString::Data(ChannelId);
}

void VoiceChatJoinChannel::Read()
{
    // Writer 0x6AFFA0: ein einzelnes uint8, ohne Schiebeausdruck.
    _worldPacket >> ChannelType;
}

WorldPacket const* VoiceLoginResponse::Write()
{
    // Reader 0x606A60. Bit-Sektion 7 + 6 + 9 = 22 Bit -> 3 Byte mit 2 Fuellbits.
    _worldPacket << uint8(Status);
    _worldPacket << uint32(PlatformCode);

    WriteDynStringLength<7>(_worldPacket, Field1);
    WriteDynStringLength<6>(_worldPacket, Field2);
    WriteDynStringLength<9>(_worldPacket, Field3);
    _worldPacket.FlushBits();

    WriteDynStringData(_worldPacket, Field1);
    WriteDynStringData(_worldPacket, Field2);
    WriteDynStringData(_worldPacket, Field3);

    return &_worldPacket;
}

WorldPacket const* VoiceChannelInfoResponse::Write()
{
    // Reader 0x606DD0 -> Rumpf 0x606C10. Bit-Sektion 7 + 9 + 7 = 23 Bit -> 3 Byte, 1 Fuellbit.
    // Field48 / Field56 / Field64 werden vom Konsumenten nicht gelesen, muessen aber geschrieben
    // werden, damit der Strom ausgerichtet bleibt.
    _worldPacket << uint8(Status);
    _worldPacket << uint32(PlatformCode);
    _worldPacket << uint8(ChannelType);
    _worldPacket << uint64(Field48);
    _worldPacket << uint64(Field56);
    _worldPacket << Field64;

    WriteDynStringLength<7>(_worldPacket, Field80);
    WriteDynStringLength<9>(_worldPacket, Field120);
    WriteDynStringLength<7>(_worldPacket, Field160);
    _worldPacket.FlushBits();

    WriteDynStringData(_worldPacket, Field80);
    WriteDynStringData(_worldPacket, Field120);
    WriteDynStringData(_worldPacket, Field160);

    return &_worldPacket;
}

WorldPacket const* VoiceChannelSttTokenResponse::Write()
{
    // Reader 0x60C070. Bit-Sektion 7 + 9 = 16 Bit -> genau 2 Byte, KEINE Fuellbits; der
    // FlushBits schreibt hier nichts. Wer ein Fuellbyte einplant, verschiebt beide Zeichenketten.
    _worldPacket << uint32(ErrorCode);

    WriteDynStringLength<7>(_worldPacket, ChannelId);
    WriteDynStringLength<9>(_worldPacket, Token);
    _worldPacket.FlushBits();

    WriteDynStringData(_worldPacket, ChannelId);
    WriteDynStringData(_worldPacket, Token);

    return &_worldPacket;
}
}
