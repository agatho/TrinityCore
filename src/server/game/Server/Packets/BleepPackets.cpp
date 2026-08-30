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

#include "BleepPackets.h"
#include "PacketOperators.h"

namespace WorldPackets::Bleep
{
namespace
{
// Die bits<24>-Laenge einer JamDynamicString ist "len ? len + 1 : 0" - fuer den leeren String
// also 0 und NICHT 1. TrinityCores SizedCString schreibt an dieser Stelle immer length()+1;
// das waere fuer den leeren Fall ein Byte zuviel. Deshalb hier ausgeschrieben.
void WriteDynStringLength(ByteBuffer& data, std::string const& value)
{
    data.WriteBits(value.empty() ? 0u : uint32(value.length() + 1), 24);
}

void WriteDynStringData(ByteBuffer& data, std::string const& value)
{
    if (value.empty())
        return;

    data.WriteString(value);
    data << uint8(0);
}

uint32 ReadDynStringLength(ByteBuffer& data)
{
    uint32 lengthWithNul = data.ReadBits(24);
    return lengthWithNul ? lengthWithNul - 1 : 0;
}

void ReadDynStringData(ByteBuffer& data, std::string& value, uint32 length)
{
    if (!length)
        return;

    value = data.ReadString(length);
    if (data.read<uint8>() != 0)
        throw ByteBufferInvalidValueException("JamDynamicString", "fehlender NUL-Abschluss");
}

// Ein BleepToken am Draht - identisch in CMSG 0x4301A2 / 0x4301A3 (Writer 0x6B2F80) und in
// SMSG 0x450384 (Element-Reader 0x6B9060). Bit-Sektion 5 + 24 + 6 = 35 Bit -> genau 5 Byte,
// danach das uint64, erst danach die drei Rohbloecke.
//
// HIER greift die Eingangsvalidierung aus BleepPackets.h, und sie muss hier greifen: die
// Reader-Bitbreiten sind WEITER als die Zielpuffer des Clients (Address bits<6> = bis 63 gegen
// Puffer 46, Token bits<5> = bis 31 gegen Puffer 25, ProxyId bits<24> gegen Inline-char[256]).
// Der einzige Weg, auf dem ein zu langer Wert wieder hinausgeht, ist die Spiegelung der
// eingereichten Tokens in SMSG_REFRESH_BLEEP_TOKENS_RESPONSE - er wird an dieser Stelle
// abgeschnitten, nicht erst dort. Ueberlaenge verwirft das ganze Paket: die
// ByteBufferInvalidValueException faengt WorldSession::Update als ByteBufferException.
void ReadBleepToken(ByteBuffer& data, BleepToken& token)
{
    data >> SizedString::BitsSize<5>(token.Token);
    uint32 proxyIdLength = ReadDynStringLength(data);
    data >> SizedString::BitsSize<6>(token.Address);
    data.ResetBitPos();

    if (token.Token.length() > MaxProxyTokenLength)
        throw ByteBufferInvalidValueException("BleepToken::Token", "laenger als der Clientpuffer");

    if (proxyIdLength > MaxProxyIdLength)
        throw ByteBufferInvalidValueException("BleepToken::ProxyId", "laenger als der Clientpuffer");

    if (token.Address.length() > MaxProxyAddressLength)
        throw ByteBufferInvalidValueException("BleepToken::Address", "laenger als der Clientpuffer");

    data >> token.TokenLifespanNanoSecs;

    data >> SizedString::Data(token.Token);
    ReadDynStringData(data, token.ProxyId, proxyIdLength);
    data >> SizedString::Data(token.Address);
}

void WriteBleepToken(ByteBuffer& data, BleepToken const& token)
{
    data << SizedString::BitsSize<5>(token.Token);
    WriteDynStringLength(data, token.ProxyId);
    data << SizedString::BitsSize<6>(token.Address);
    data.FlushBits();

    data << uint64(token.TokenLifespanNanoSecs);

    data << SizedString::Data(token.Token);
    WriteDynStringData(data, token.ProxyId);
    data << SizedString::Data(token.Address);
}
}

void BleepPong::Read()
{
    // Writer 0x6B2E00. Element 280 Byte (24 + Puffer 256).
    _worldPacket >> Size<uint32>(PingData);

    for (BleepPingData& ping : PingData)
    {
        _worldPacket >> ping.ClientPing;
        _worldPacket >> ping.PingsSent;
        _worldPacket >> ping.PongsReceived;

        // Hier ist das uint8 wirklich ein Byte: Laengenfeld bei leerem Akkumulator, der
        // FlushBits danach schreibt nichts. Eine Schranke gegen MaxProxyIdLength braucht es an
        // dieser Stelle nicht: bits<8> laesst hoechstens 255 zu, und das IST MaxProxyIdLength.
        _worldPacket >> SizedString::BitsSize<8>(ping.ProxyId);
        _worldPacket.ResetBitPos();
        _worldPacket >> SizedString::Data(ping.ProxyId);
    }
}

void RefreshBleepTokens::Read()
{
    // Writer 0x6B2F80 - derselbe Rumpf wie CMSG_EXPIRE_BLEEP_TOKENS.
    _worldPacket >> Size<uint32>(Tokens);

    for (BleepToken& token : Tokens)
        ReadBleepToken(_worldPacket, token);
}

void ExpireBleepTokens::Read()
{
    // Writer 0x6B2F80 - byteweise identisch zu CMSG_REFRESH_BLEEP_TOKENS.
    _worldPacket >> Size<uint32>(Tokens);

    for (BleepToken& token : Tokens)
        ReadBleepToken(_worldPacket, token);
}

WorldPacket const* FetchBleepProxiesResponse::Write()
{
    // Reader 0x612BD0. Gegen sechs echte Pakete byteweise nachgerechnet, Rest jeweils 0.
    _worldPacket << Size<uint32>(Proxies);

    for (BleepProxy const& proxy : Proxies)
    {
        // Bit-Sektion 6 + 5 + 8 = 19 Bit -> 3 Byte, MSB-first.
        _worldPacket << SizedString::BitsSize<6>(proxy.Address);
        _worldPacket << SizedString::BitsSize<5>(proxy.PingToken);
        _worldPacket << SizedString::BitsSize<8>(proxy.ProxyId);
        _worldPacket.FlushBits();

        _worldPacket << uint64(proxy.PingPort);
        _worldPacket << uint64(proxy.PingTokenValidDuration);
        _worldPacket << uint64(proxy.Port);

        _worldPacket << SizedString::Data(proxy.Address);
        _worldPacket << SizedString::Data(proxy.PingToken);
        _worldPacket << SizedString::Data(proxy.ProxyId);
    }

    return &_worldPacket;
}

WorldPacket const* RefreshBleepTokensResponse::Write()
{
    // Reader 0x612FB0 -> 0x612DD0. Count == 0 heisst "alle Erneuerungen erfolgreich"; die
    // Nachricht traegt ausschliesslich die Fehlschlaege.
    _worldPacket << Size<uint32>(Failures);
    _worldPacket << uint64(TokenLifespanNanoSecs);
    _worldPacket << uint32(Field64);

    for (BleepToken const& token : Failures)
        WriteBleepToken(_worldPacket, token);

    return &_worldPacket;
}
}
