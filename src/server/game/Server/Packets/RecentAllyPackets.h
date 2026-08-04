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

#ifndef TRINITYCORE_RECENT_ALLY_PACKETS_H
#define TRINITYCORE_RECENT_ALLY_PACKETS_H

#include "Packet.h"
#include "ObjectGuid.h"
#include <string>
#include <vector>

namespace WorldPackets
{
namespace Social
{
    // CMSG_SET_ALLOW_RECENT_ALLIES_SEE_LOCATION (0x3A0308): a single bit toggle. Client serializer sub_7FF72914CCD0
    // writes just the bool (the opcode header + one packed bit). When cleared the player opts out of letting people
    // they recently grouped with see their location (client flag CHARACTER_FLAG_4_DISALLOW_RECENT_ALLIES...).
    class SetAllowRecentAlliesSeeLocation final : public ClientPacket
    {
    public:
        explicit SetAllowRecentAlliesSeeLocation(WorldPacket&& packet) : ClientPacket(CMSG_SET_ALLOW_RECENT_ALLIES_SEE_LOCATION, std::move(packet)) { }

        void Read() override;

        bool Allow = false;
    };

    // CMSG_RECENT_ALLY_REQUEST_DATA (0x400198): empty request; the client asks for its recent-allies list. Client
    // serializer sub_7FF7290805B0 writes no meaningful payload.
    class RecentAllyRequestData final : public ClientPacket
    {
    public:
        explicit RecentAllyRequestData(WorldPacket&& packet) : ClientPacket(CMSG_RECENT_ALLY_REQUEST_DATA, std::move(packet)) { }

        void Read() override { }
    };

    // CMSG_RECENT_ALLY_SET_NOTE (0x400199): set/clear a personal note on a recent ally. Client serializer
    // sub_7FF7290806C0 = { PackedGuid AllyGUID; bit-packed note (length + a trailing bit); note bytes }. The note
    // length is written in the client's bit block; the width is the same 11-bit field the response readers use
    // (2048 = 1<<11 sentinel), value = length (empty note = 0). One trailing bit rides the same block (unknown
    // semantics -> read to consume).
    class RecentAllySetNote final : public ClientPacket
    {
    public:
        explicit RecentAllySetNote(WorldPacket&& packet) : ClientPacket(CMSG_RECENT_ALLY_SET_NOTE, std::move(packet)) { }

        void Read() override;

        ObjectGuid AllyGUID;
        std::string Note;
        bool Flag = false;
    };

    // One recent ally on the SMSG_RECENT_ALLY_DATA_RESPONSE wire (client entry reader sub_7FF729162B00):
    //   { PackedGuid Guid; uint64 WowAccount; uint32 Field68; uint32 activityCount; activity[] {u8,u64,u32,u32};
    //     bit-block(11-bit note length + 3 flag bits); note bytes }.
    // The three flag bits and the 24-byte activity sub-records ("shared activities") have no offline-resolvable
    // semantics, so they are sent 0 / empty; the ally identity, account and note round-trip.
    struct RecentAllyInfo
    {
        ObjectGuid Guid;
        uint64 WowAccount = 0;
        uint32 Field68 = 0;
        std::string Note;
    };

    // SMSG_RECENT_ALLY_DATA_RESPONSE (0x420360, client reader sub_7FF7290BBE20): { uint32 count; entry[] }.
    class RecentAllyDataResponse final : public ServerPacket
    {
    public:
        RecentAllyDataResponse() : ServerPacket(SMSG_RECENT_ALLY_DATA_RESPONSE) { }

        WorldPacket const* Write() override;

        std::vector<RecentAllyInfo> Allies;
    };

    // SMSG_RECENT_ALLY_NOTE_UPDATED (0x420361, client reader sub_7FF7290BBF10): { uint32 Field32; PackedGuid AllyGUID;
    // uint64 WowAccount; bit-length note; note bytes }. Echoed after a successful CMSG_RECENT_ALLY_SET_NOTE so the
    // client refreshes the note in place. Field32 has no resolvable semantics offline -> sent 0.
    class RecentAllyNoteUpdated final : public ServerPacket
    {
    public:
        RecentAllyNoteUpdated() : ServerPacket(SMSG_RECENT_ALLY_NOTE_UPDATED) { }

        WorldPacket const* Write() override;

        ObjectGuid AllyGUID;
        uint64 WowAccount = 0;
        std::string Note;
    };
}
}

#endif // TRINITYCORE_RECENT_ALLY_PACKETS_H
