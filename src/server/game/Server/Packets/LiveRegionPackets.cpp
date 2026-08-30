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

#include "LiveRegionPackets.h"
#include "PacketOperators.h"

namespace WorldPackets::LiveRegion
{
void GetAccountCharacterList::Read()
{
    // Writer 0x6AB3D0. Bit-Sektion: bits<9> RealmName, dann bits<6> CharacterName -> 2 Byte.
    // Die 9-Bit-Laenge erscheint im Dekompilat als Write<uint8>(len >> 1) plus ein
    // eingebettetes Bit (len & 1) - das ist KEIN uint8-Feld.
    _worldPacket >> Token;
    _worldPacket >> RegionID;

    _worldPacket >> SizedString::BitsSize<9>(RealmName);
    _worldPacket >> SizedString::BitsSize<6>(CharacterName);
    _worldPacket.ResetBitPos();

    _worldPacket >> SizedString::Data(RealmName);
    _worldPacket >> SizedString::Data(CharacterName);
}

void CharacterCopy::Read()
{
    // Writer 0x6AB580. Bit-Sektion VERTAUSCHT gegenueber 0x4300E5:
    // bits<6> CharacterName zuerst, dann bits<9> RealmName. Auch die Rohbytes sind vertauscht.
    _worldPacket >> Token;
    _worldPacket >> RegionID;
    _worldPacket >> VirtualRealmAddress;
    _worldPacket >> CharacterGUID;
    _worldPacket >> Option0;
    _worldPacket >> Option1;
    _worldPacket >> Option2;

    _worldPacket >> SizedString::BitsSize<6>(CharacterName);
    _worldPacket >> SizedString::BitsSize<9>(RealmName);
    _worldPacket.ResetBitPos();

    _worldPacket >> SizedString::Data(CharacterName);
    _worldPacket >> SizedString::Data(RealmName);
}

void AccountRestore::Read()
{
    // Writer 0x6AB800. Reihenfolge wie 0x4300E5: bits<9> RealmName, dann bits<6> CharacterName.
    _worldPacket >> Token;
    _worldPacket >> RegionID;
    _worldPacket >> CharacterGUID;
    _worldPacket >> Field368;

    _worldPacket >> SizedString::BitsSize<9>(RealmName);
    _worldPacket >> SizedString::BitsSize<6>(CharacterName);
    _worldPacket.ResetBitPos();

    _worldPacket >> SizedString::Data(RealmName);
    _worldPacket >> SizedString::Data(CharacterName);
}

void KeyBindingsCopy::Read()
{
    // Writer 0x6AB9D0. Reihenfolge wie 0x4300E6: bits<6> CharacterName, dann bits<9> RealmName.
    _worldPacket >> Token;
    _worldPacket >> RegionID;
    _worldPacket >> VirtualRealmAddress;
    _worldPacket >> CharacterGUID;

    _worldPacket >> SizedString::BitsSize<6>(CharacterName);
    _worldPacket >> SizedString::BitsSize<9>(RealmName);
    _worldPacket.ResetBitPos();

    _worldPacket >> SizedString::Data(CharacterName);
    _worldPacket >> SizedString::Data(RealmName);
}

// --------------------------------------------------------------------------------------------
// Antwortseite. Reihenfolge und Bitbreiten sind an den Readern nachgelesen, nicht abgeleitet:
//   0x45020C -> 0x5FBAB0 (Liste), Element 0x69F600
//   0x450218 -> 0x5FC2F0, 0x450219 -> 0x5FC380, 0x45021A -> 0x5FC410 (instruktionsgleich)
// Das Erfolgsflag liest der Client als (byte >> 7): es ist ein einzelnes Bit hinter dem letzten
// byte-alignten Feld, also genau ein Byte am Draht.
// --------------------------------------------------------------------------------------------

ByteBuffer& operator<<(ByteBuffer& data, AccountCharacterData const& character)
{
    data << character.CharacterGUID;
    data << character.AccountGUID;
    data << uint32(character.VirtualRealmAddress);
    data << uint8(character.RaceID);
    data << uint8(character.ClassID);
    data << uint8(character.SexID);
    data << uint8(character.ExperienceLevel);
    data << uint64(character.Field352);
    data << uint32(character.Field360);

    // Bit-Sektion des Elements: 6 + 9 = 15 Bit, das 16. Bit ist Fuellung.
    data << SizedString::BitsSize<6>(character.Name);
    data << SizedString::BitsSize<9>(character.RealmName);
    data.FlushBits();

    data << SizedString::Data(character.Name);
    data << SizedString::Data(character.RealmName);

    return data;
}

WorldPacket const* LiveRegionGetAccountCharacterListResult::Write()
{
    _worldPacket << uint32(Token);
    _worldPacket << Size<uint32>(Characters);
    for (AccountCharacterData const& character : Characters)
        _worldPacket << character;

    _worldPacket << Bits<1>(Success);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* LiveRegionCharacterCopyResult::Write()
{
    _worldPacket << uint32(Token);
    _worldPacket << Bits<1>(Success);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* LiveRegionAccountRestoreResult::Write()
{
    _worldPacket << uint32(Token);
    _worldPacket << Bits<1>(Success);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* LiveRegionKeyBindingsCopyResult::Write()
{
    _worldPacket << uint32(Token);
    _worldPacket << Bits<1>(Success);
    _worldPacket.FlushBits();

    return &_worldPacket;
}
}
