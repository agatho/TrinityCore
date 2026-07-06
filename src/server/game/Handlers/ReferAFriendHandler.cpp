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

#include "WorldSession.h"
#include "ReferAFriendPackets.h"

// Recruit-A-Friend account info. The client opens the RAF panel by requesting this; the server answers with the
// account's recruit roster + reward state. The full packet is a large nested structure (see RAF_WIRE_DOSSIER);
// with no recruitment backend data yet, the correct answer for every account is the empty-state form, which is
// exactly what SMSG_RAF_ACCOUNT_INFO encodes with all vectors at count 0. The recruit list is populated from the
// recruitment backend in a later phase.
void WorldSession::HandleGetRafAccountInfo(WorldPackets::RaF::GetRafAccountInfo& packet)
{
    WorldPackets::RaF::RafAccountInfo response;
    response.Field20 = packet.Field;   // echo the client's leading field
    SendPacket(response.Write());
}
