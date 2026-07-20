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

void ClubFinderRequestClubsData::Read()
{
    _worldPacket >> Size<uint32>(ClubPostingIDs);
    _worldPacket >> FilterCount;
    for (uint32& clubPostingId : ClubPostingIDs)
        _worldPacket >> clubPostingId;

    _worldPacket >> Bits<3>(Type);
    _worldPacket >> Bits<1>(Unknown);
    _worldPacket.ResetBitPos();

    // Any trailing ClubFinderPostingFilter records are deliberately not parsed: their wire layout is
    // not derived, and every captured request carries FilterCount == 0. The handler reports a
    // non-zero count rather than misreading the tail.
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
