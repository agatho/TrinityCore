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

#include "RecentAllyPackets.h"

namespace WorldPackets::Social
{
// The note length is bit-packed in an 11-bit field (2048 = 1<<11 sentinel in the client readers); note bytes
// follow byte-aligned, matching TC's standard bit-length-string convention.
static constexpr uint32 RECENT_ALLY_NOTE_LEN_BITS = 11;

void SetAllowRecentAlliesSeeLocation::Read()
{
    _worldPacket.ResetBitPos();
    Allow = _worldPacket.ReadBit();
}

void RecentAllySetNote::Read()
{
    _worldPacket >> AllyGUID;                       // PackedGuid
    _worldPacket.ResetBitPos();
    uint32 noteLen = _worldPacket.ReadBits(RECENT_ALLY_NOTE_LEN_BITS);
    Flag = _worldPacket.ReadBit();
    // Guard against a malformed length over-reading the (length-framed) packet.
    noteLen = std::min<uint32>(noteLen, uint32(_worldPacket.size()));
    Note = _worldPacket.ReadString(noteLen);
}

WorldPacket const* RecentAllyDataResponse::Write()
{
    _worldPacket << uint32(Allies.size());
    for (RecentAllyInfo const& ally : Allies)
    {
        _worldPacket << ally.Guid;                  // PackedGuid
        _worldPacket << uint64(ally.WowAccount);
        _worldPacket << uint32(ally.Field68);
        _worldPacket << uint32(0);                  // shared-activity count (opaque sub-records -> none)

        // bit-block: 11-bit note length + three flag bits (semantics unresolved -> 0), then the note bytes.
        _worldPacket.WriteBits(uint32(ally.Note.length()), RECENT_ALLY_NOTE_LEN_BITS);
        _worldPacket.WriteBit(false);
        _worldPacket.WriteBit(false);
        _worldPacket.WriteBit(false);
        _worldPacket.FlushBits();
        _worldPacket.WriteString(ally.Note);
    }
    return &_worldPacket;
}

WorldPacket const* RecentAllyNoteUpdated::Write()
{
    _worldPacket << uint32(0);                      // Field32 (unresolved semantics -> 0)
    _worldPacket << AllyGUID;                       // PackedGuid
    _worldPacket << uint64(WowAccount);
    _worldPacket.WriteBits(uint32(Note.length()), RECENT_ALLY_NOTE_LEN_BITS);
    _worldPacket.FlushBits();
    _worldPacket.WriteString(Note);
    return &_worldPacket;
}
}
