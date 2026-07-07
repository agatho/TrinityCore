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

#ifndef TRINITYCORE_COMMENTATOR_PACKETS_H
#define TRINITYCORE_COMMENTATOR_PACKETS_H

#include "Packet.h"
#include "ObjectGuid.h"

namespace WorldPackets
{
    namespace Commentator
    {
        class CommentatorEnable final : public ClientPacket
        {
        public:
            explicit CommentatorEnable(WorldPacket&& packet) : ClientPacket(CMSG_COMMENTATOR_ENABLE, std::move(packet)) { }

            void Read() override;

            uint32 Enable = 0;
        };

        class CommentatorStateChanged final : public ServerPacket
        {
        public:
            explicit CommentatorStateChanged() : ServerPacket(SMSG_COMMENTATOR_STATE_CHANGED, 16 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid MatchGUID;
            bool Enabled = false;
        };
    }
}

#endif // TRINITYCORE_COMMENTATOR_PACKETS_H
