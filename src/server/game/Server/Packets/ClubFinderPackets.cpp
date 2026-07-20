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

#include "ClubFinderPackets.h"
#include "PacketOperators.h"

namespace WorldPackets::ClubFinder
{
void ClubFinderPost::Read()
{
    _worldPacket.ResetBitPos();

    _worldPacket >> SizedString::BitsSize<7>(Name);
    _worldPacket >> SizedString::BitsSize<12>(Description);
    _worldPacket >> Bits<3>(Type);
    _worldPacket >> Bits<1>(CrossFaction);

    // The first byte-aligned read below flushes the remaining bits of the block for us
    // (ByteBuffer::read<T> calls ResetBitPos), matching the client's explicit FlushBits.
    _worldPacket >> ClubId;
    _worldPacket >> RecruitingSpecs;
    _worldPacket >> RecruitmentFlags;
    _worldPacket >> ItemLevelRequirement;
    _worldPacket >> AvatarId;
    _worldPacket >> SizedString::Data(Name);
    _worldPacket >> SizedString::Data(Description);
}

WorldPacket const* ClubFinderResponsePostRecruitmentMessage::Write()
{
    _worldPacket << ClubFinderGUID;
    _worldPacket << Bits<3>(Result);
    _worldPacket << Bits<3>(Unused);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

void ClubFinderRequestSubscribedClubPostingIds::Read()
{
    _worldPacket >> Size<uint32>(ClubIds);
    for (uint64& clubId : ClubIds)
        _worldPacket >> clubId;
}

WorldPacket const* ClubFinderGetClubPostingIdsResponse::Write()
{
    _worldPacket << Size<uint32>(PostingIds);
    for (ClubPostingClubIDMap const& postingId : PostingIds)
    {
        _worldPacket << postingId.ClubID;
        _worldPacket << postingId.ClubPostingID;
        _worldPacket << postingId.PostingDisplayFlags;
    }

    return &_worldPacket;
}

// Element writer sub_7FF729143D20. The Bits<24> byte count is present only for the string-valued
// forms, and the client emits strlen + 1, so the count always equals the number of bytes that follow.
ByteBuffer& operator>>(ByteBuffer& data, ClubFinderPostingFilter& filter)
{
    data >> Bits<3>(filter.Type);
    data.ResetBitPos();

    data >> Bits<3>(filter.ValueType);

    uint32 byteCount = 0;
    if (filter.ValueType == 5 || filter.ValueType == 6)
        byteCount = data.ReadBits(24);

    data.ResetBitPos();

    switch (filter.ValueType)
    {
        case 1:
        case 2:
            data >> filter.UintValue;
            break;
        case 3:
        case 4:
            data >> filter.Uint64Value;
            break;
        case 5:
        case 6:
            if (byteCount)
            {
                filter.StringValue.resize(byteCount);
                data.read(reinterpret_cast<uint8*>(filter.StringValue.data()), byteCount);
                // The client counts the terminator; drop it so the value is a plain string.
                if (!filter.StringValue.empty() && filter.StringValue.back() == '\0')
                    filter.StringValue.pop_back();
            }
            break;
        default:
            break;
    }

    return data;
}

void ClubFinderRequestClubsData::Read()
{
    uint32 filterCount = 0;

    _worldPacket >> Size<uint32>(ClubPostingIDs);
    _worldPacket >> filterCount;
    for (uint32& clubPostingId : ClubPostingIDs)
        _worldPacket >> clubPostingId;

    _worldPacket >> Bits<3>(Type);
    _worldPacket >> Bits<1>(CrossFaction);
    _worldPacket.ResetBitPos();

    Filters.resize(filterCount);
    for (ClubFinderPostingFilter& filter : Filters)
        _worldPacket >> filter;
}

void ClubFinderRequestClubsList::Read()
{
    uint32 const searchStringLength = _worldPacket.ReadBits(9);
    _worldPacket >> Bits<3>(Type);
    _worldPacket >> Bits<1>(CrossFaction);
    _worldPacket.ResetBitPos();

    uint32 filterCount = 0;
    _worldPacket >> filterCount;
    _worldPacket >> ApplicantSettings;

    if (searchStringLength)
    {
        SearchString.resize(searchStringLength);
        _worldPacket.read(reinterpret_cast<uint8*>(SearchString.data()), searchStringLength);
    }

    Filters.resize(filterCount);
    for (ClubFinderPostingFilter& filter : Filters)
        _worldPacket >> filter;
}

WorldPacket const* ClubFinderLookupClubPostingsList::Write()
{
    _worldPacket << Size<uint32>(Postings);
    _worldPacket << Bits<3>(Type);
    _worldPacket << Bits<1>(Unknown);
    _worldPacket.FlushBits();

    for (ClubCacheData const& posting : Postings)
    {
        // One bit block per record: 7 + 12 + 6 = 25 bits, flushed to four whole bytes.
        _worldPacket << SizedString::BitsSize<7>(posting.ClubName);
        _worldPacket << SizedString::BitsSize<12>(posting.Comment);
        _worldPacket << SizedString::BitsSize<6>(posting.GuildLeader);
        _worldPacket.FlushBits();

        _worldPacket << posting.ClubFinderGUID;
        _worldPacket << posting.NumActiveMembers;
        _worldPacket << posting.RecruitingSpecs;
        _worldPacket << posting.RecruitmentFlags;
        _worldPacket << posting.MinIlvl;
        _worldPacket << posting.TabardInfo;          // precedes LastPosterGUID on the wire
        _worldPacket << posting.LastPosterGUID;
        _worldPacket << posting.ClubID;
        _worldPacket << posting.LastUpdatedTime;

        _worldPacket << SizedString::Data(posting.ClubName);
        _worldPacket << SizedString::Data(posting.Comment);
        _worldPacket << SizedString::Data(posting.GuildLeader);
    }

    return &_worldPacket;
}

WorldPacket const* ClubFinderErrorMessage::Write()
{
    _worldPacket << Bits<3>(Type);
    _worldPacket << Bits<4>(Error);
    _worldPacket.FlushBits();

    return &_worldPacket;
}
}
