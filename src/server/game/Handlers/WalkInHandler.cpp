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
#include "DB2Structure.h"
#include "Map.h"
#include "Player.h"
#include "WalkInPackets.h"

void WorldSession::SendWalkInResult(WorldPackets::WalkIn::WalkInResultCode result)
{
    WorldPackets::WalkIn::WalkInResult walkInResult;
    walkInResult.Result = result;

    SendPacket(walkInResult.Write());
}

// LeaveWalkInParty() in the client is C_PartyInfo.DelveTeleportOut() (LFGUtil.lua:39); it asks the
// server to end the private, queue free instance session and put the player back where they walked
// in. The client already refuses to send while dead, falling, on a transport or fatigued
// (RVA 0x12DCAC0), so everything below is the server side re-check plus the two conditions only the
// server can judge - whether there is a recorded entrance to return to at all.
void WorldSession::HandleDelveTeleportOut(WorldPackets::WalkIn::DelveTeleportOut& /*packet*/)
{
    Player* player = GetPlayer();

    if (!player->IsAlive())
    {
        SendWalkInResult(WorldPackets::WalkIn::WalkInResultCode::PlayerDead);
        return;
    }

    if (player->IsFalling())
    {
        SendWalkInResult(WorldPackets::WalkIn::WalkInResultCode::NotWhileFalling);
        return;
    }

    if (!player->GetMap()->IsDungeon())
    {
        SendWalkInResult(WorldPackets::WalkIn::WalkInResultCode::InvalidTeleportLocation);
        return;
    }

    WorldSafeLocsEntry const* entrance = player->GetInstanceEntrance(player->GetMapId());
    if (!entrance)
    {
        SendWalkInResult(WorldPackets::WalkIn::WalkInResultCode::InvalidTeleportLocation);
        return;
    }

    // Success is silent on the client (handler RVA 0x21938A0 returns immediately for 0), it is sent
    // so that every path of the pair answers.
    SendWalkInResult(WorldPackets::WalkIn::WalkInResultCode::Success);

    player->TeleportTo({ .Location = entrance->Loc });
}
