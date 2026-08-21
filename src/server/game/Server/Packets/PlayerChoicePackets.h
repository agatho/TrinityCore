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

#ifndef TRINITYCORE_PLAYER_CHOICE_PACKETS_H
#define TRINITYCORE_PLAYER_CHOICE_PACKETS_H

#include "Packet.h"

namespace WorldPackets::PlayerChoice
{
// SMSG_PLAYER_CHOICE_CLEAR - client 12.1.0.69382 wire opcode 0x640006, dispatcher case in 0x67C100.
// Wire: uint32 ChoiceID; bits<1> MatchCurrentChoiceOnly; FlushBits  -> always 5 bytes.
// Source: dispatcher case reads Read<uint32> (0x35AF190) then Read<uint8> >> 7.
// Sniff (12.1.0.69273, 8 packets / 12.0.7, 35 packets): all exactly 5 bytes.
class PlayerChoiceClear final : public ServerPacket
{
public:
    explicit PlayerChoiceClear() : ServerPacket(SMSG_PLAYER_CHOICE_CLEAR, 4 + 1) { }

    WorldPacket const* Write() override;

    int32 ChoiceID = 0;
    // Subscriber 0x254BFE0: true  -> close only if ChoiceID is the choice currently on screen
    //                       false -> close unconditionally (this is what retail sends in the recorded case)
    bool MatchCurrentChoiceOnly = false;
};

// SMSG_PLAYER_CHOICE_DISPLAY_ERROR - client 12.1.0.69382 wire opcode 0x640005.
// Wire: empty. The dispatcher hands the subscriber a raw buffer pointer and the subscriber
// (0x254BFB0) is literally `return sub_7FF78306AD90(1147);` - it never touches the message.
// 1147 = ERR_PLAYER_CHOICE_ERROR_PENDING_CHOICE. There is no error code on the wire; the client
// always shows the same text.
class PlayerChoiceDisplayError final : public ServerPacket
{
public:
    explicit PlayerChoiceDisplayError() : ServerPacket(SMSG_PLAYER_CHOICE_DISPLAY_ERROR, 0) { }

    WorldPacket const* Write() override { return &_worldPacket; }
};
}

#endif // TRINITYCORE_PLAYER_CHOICE_PACKETS_H
