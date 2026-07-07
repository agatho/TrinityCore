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

#ifndef TRINITYCORE_CONTENT_TRACKING_PACKETS_H
#define TRINITYCORE_CONTENT_TRACKING_PACKETS_H

#include "Packet.h"

// "Content tracking": the client asks the server to remember what map content the player is tracking (a collectable
// source, activity, etc.) so it persists across sessions/devices. The server mirrors the tracked set into the
// ActivePlayer.TrackedCollectableSources update field (CollectableSourceTrackedData = TargetType/TargetID/
// CollectableSourceInfoID). Wire recovered from the client serializers sub_7FF72914C020 / sub_7FF72914C130; field
// meaning from the C_ContentTracking Lua API (StartTracking(type,id) / StopTracking(type,id,stopType)).
namespace WorldPackets
{
namespace ContentTracking
{
    // CMSG_CONTENT_TRACKING_START_TRACKING (0x3A02DC): { u32 TargetType, u32 TargetID, u32 CollectableSourceInfoID, bit }.
    class StartTracking final : public ClientPacket
    {
    public:
        explicit StartTracking(WorldPacket&& packet) : ClientPacket(CMSG_CONTENT_TRACKING_START_TRACKING, std::move(packet)) { }

        void Read() override;

        int32 TargetType = 0;
        int32 TargetID = 0;
        int32 CollectableSourceInfoID = 0;
        bool Flag = false;
    };

    // CMSG_CONTENT_TRACKING_STOP_TRACKING (0x3A02DD): { u32 TargetType, u32 TargetID, u32 CollectableSourceInfoID,
    // u32 StopType, bit }.
    class StopTracking final : public ClientPacket
    {
    public:
        explicit StopTracking(WorldPacket&& packet) : ClientPacket(CMSG_CONTENT_TRACKING_STOP_TRACKING, std::move(packet)) { }

        void Read() override;

        int32 TargetType = 0;
        int32 TargetID = 0;
        int32 CollectableSourceInfoID = 0;
        int32 StopType = 0;
        bool Flag = false;
    };
}
}

#endif // TRINITYCORE_CONTENT_TRACKING_PACKETS_H
