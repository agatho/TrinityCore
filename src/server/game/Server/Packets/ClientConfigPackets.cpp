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

#include "ClientConfigPackets.h"
#include "PacketOperators.h"

namespace WorldPackets::ClientConfig
{
WorldPacket const* AccountDataTimes::Write()
{
    _worldPacket << PlayerGuid;
    _worldPacket << ServerTime;
    for (Timestamp<> const& accountDataTime : AccountTimes)
        _worldPacket << accountDataTime;

    return &_worldPacket;
}

WorldPacket const* ClientCacheVersion::Write()
{
    _worldPacket << uint32(CacheVersion);

    return &_worldPacket;
}

WorldPacket const* CacheInfo::Write()
{
    // every length is six bits wide - a longer string would announce a truncated length and then
    // write all of its bytes, which desyncs the client's stream instead of merely losing characters
    auto clamp = [](std::string const& value) { return std::string_view(value).substr(0, (1 << 6) - 1); };

    _worldPacket << Size<uint32>(Entries);
    for (CacheInfoEntry const& entry : Entries)
    {
        std::string_view key = clamp(entry.Key);
        std::string_view value = clamp(entry.Value);

        // both lengths belong to one 12 bit group - a flush after each of them would emit a byte too many
        _worldPacket << SizedString::BitsSize<6>(key);
        _worldPacket << SizedString::BitsSize<6>(value);
        _worldPacket.FlushBits();

        _worldPacket << SizedString::Data(key);
        _worldPacket << SizedString::Data(value);
    }

    std::string_view prefix = clamp(Prefix);
    _worldPacket << SizedString::BitsSize<6>(prefix);
    _worldPacket.FlushBits();
    _worldPacket << SizedString::Data(prefix);

    return &_worldPacket;
}

void RequestAccountData::Read()
{
    _worldPacket >> PlayerGuid;
    _worldPacket >> DataType;
}

WorldPacket const* UpdateAccountData::Write()
{
    _worldPacket << Time;
    _worldPacket << uint32(Size);
    _worldPacket << Player;
    _worldPacket << int32(DataType);
    _worldPacket << Bytes::Size<uint32>(CompressedData);
    _worldPacket << Bytes::Data(CompressedData);

    return &_worldPacket;
}

void UserClientUpdateAccountData::Read()
{
    _worldPacket >> Time;
    _worldPacket >> Size;
    _worldPacket >> PlayerGuid;
    _worldPacket >> DataType;
    _worldPacket >> Bytes::Size<uint32>(CompressedData);
    _worldPacket >> Bytes::Data(CompressedData);
}

WorldPacket const* UpdateAccountDataComplete::Write()
{
    _worldPacket << Player;
    _worldPacket << int32(DataType);
    _worldPacket << int32(Result);

    return &_worldPacket;
}

void SetAdvancedCombatLogging::Read()
{
    _worldPacket >> Bits<1>(Enable);
}
}
