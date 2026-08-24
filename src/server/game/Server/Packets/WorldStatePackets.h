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

#ifndef TRINITYCORE_WORLD_STATE_PACKETS_H
#define TRINITYCORE_WORLD_STATE_PACKETS_H

#include "Packet.h"

namespace WorldPackets
{
    namespace WorldState
    {
        class TC_GAME_API InitWorldStates final : public ServerPacket
        {
        public:
            struct WorldStateInfo
            {
                WorldStateInfo(int32 variableID, int32 value)
                    : VariableID(variableID), Value(value) { }

                int32 VariableID;
                int32 Value;
            };

            explicit InitWorldStates() : ServerPacket(SMSG_INIT_WORLD_STATES, 16) { }

            WorldPacket const* Write() override;

            int32 AreaID    = 0; ///< ZoneId
            int32 SubareaID = 0; ///< AreaId
            int32 MapID     = 0; ///< MapId

            std::vector<WorldStateInfo> Worldstates;
        };

        class UpdateWorldState final : public ServerPacket
        {
        public:
            explicit UpdateWorldState() : ServerPacket(SMSG_UPDATE_WORLD_STATE, 4 + 1 + 4) { }

            WorldPacket const* Write() override;

            int32 Value       = 0;
            bool Hidden   = false; ///< @todo: research
            uint32 VariableID = 0;
        };

        // Scheduled/recurring world states (area-POI + repeating game-event cycles). Duration is the whole cycle
        // length; the client derives the countdown from StartTime + Duration. Restored per AreaPoi/world-quest slice.
        struct ScheduledWorldStateInfo
        {
            ScheduledWorldStateInfo(time_t startTime, uint32 duration, uint32 variableID, int32 value)
                : StartTime(startTime), Duration(duration), VariableID(variableID), Value(value) { }

            Timestamp<> StartTime;
            uint32 Duration;
            uint32 VariableID;
            int32 Value;
        };

        class TC_GAME_API ActiveScheduledWorldStateInfo final : public ServerPacket
        {
        public:
            explicit ActiveScheduledWorldStateInfo() : ServerPacket(SMSG_ACTIVE_SCHEDULED_WORLD_STATE_INFO, 4) { }

            WorldPacket const* Write() override;

            std::vector<ScheduledWorldStateInfo> Schedules;
        };
    }
}

#endif // TRINITYCORE_WORLD_STATE_PACKETS_H
