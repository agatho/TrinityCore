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
#include "Group.h"
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
// (RVA 0x12DCAC0), so everything below is the server side re-check plus the conditions only the
// server can judge - whether there is a recorded entrance to return to at all.
//
// The re-check follows LFGMgr::TeleportPlayer (LFGMgr.cpp:1385-1396), which guards the very same
// operation - pulling a player out of an instance - and is the only place in this core that already
// decides it. Its condition list is taken over one for one; only the result values differ, because
// SMSG_WALK_IN_RESULT has its own three bit enum instead of LfgTeleportResult:
//
//   !IsAlive()                                  -> PlayerDead          (client also pre-checks)
//   IsFalling() || UNIT_STATE_JUMPING           -> NotWhileFalling     (client also pre-checks)
//   IsMirrorTimerActive(FATIGUE_TIMER)          -> NotWhileFatigued    (client also pre-checks)
//   GetVehicle() / GetTransport()               -> LockedOut           (client pre-checks transport)
//   charmed, or Freeze (spell 9454)             -> LockedOut
//
// The last two have no dedicated wire value. LockedOut is the client's generic refusal: its text is
// ERR_CLIENT_LOCKED_OUT, which the UI itself uses for "you cannot do that right now" (equipment
// changes blocked by combat, PaperDollFrame.lua and EquipmentManager.lua). Refusing with a text the
// player can read is the whole point of the enum; teleporting a charmed or vehicle bound player
// instead would leave the vehicle or the charm behind on the instance map.
void WorldSession::HandleDelveTeleportOut(WorldPackets::WalkIn::DelveTeleportOut& /*packet*/)
{
    Player* player = GetPlayer();

    if (!player->IsAlive())
    {
        SendWalkInResult(WorldPackets::WalkIn::WalkInResultCode::PlayerDead);
        return;
    }

    if (player->IsFalling() || player->HasUnitState(UNIT_STATE_JUMPING))
    {
        SendWalkInResult(WorldPackets::WalkIn::WalkInResultCode::NotWhileFalling);
        return;
    }

    if (player->IsMirrorTimerActive(FATIGUE_TIMER))
    {
        SendWalkInResult(WorldPackets::WalkIn::WalkInResultCode::NotWhileFatigued);
        return;
    }

    if (player->GetVehicle() || player->GetTransport() || !player->GetCharmedGUID().IsEmpty() || player->HasAura(9454 /*Freeze*/))
    {
        SendWalkInResult(WorldPackets::WalkIn::WalkInResultCode::LockedOut);
        return;
    }

    if (!player->GetMap()->IsDungeon())
    {
        SendWalkInResult(WorldPackets::WalkIn::WalkInResultCode::InvalidTeleportLocation);
        return;
    }

    // Where to put the player. The order is the one this core already uses for leaving an instance
    // through its exit area trigger (MiscHandler.cpp:684-695): LFG entry point first, instance
    // entrance second, and only then a fixed location. GetInstanceEntrance answers only when an
    // instance script reports an entrance location or an active instance lock carries an
    // EntranceWorldSafeLocId; this core has no delve content at all, so for a walk-in that is
    // normally nothing, which is why the homebind fallback exists rather than a refusal.
    // UNVERIFIED: the return destination. The client only ever sees the one byte result code
    // (handler RVA 0x21938A0), so no dump can say where retail puts the player. The order above is
    // taken from this core's own instance exit; all three destinations are this core's choice.
    bool teleported = false;

    if (Group const* group = player->GetGroup())
        if (group->isLFGGroup())
            teleported = player->TeleportToBGEntryPoint();

    if (!teleported)
        if (WorldSafeLocsEntry const* entrance = player->GetInstanceEntrance(player->GetMapId()))
            teleported = player->TeleportTo({ .Location = entrance->Loc });

    if (!teleported)
        teleported = player->TeleportTo(player->m_homebind);

    // The result is sent AFTER the attempt, because Success is silent on the client: handler
    // RVA 0x21938A0 returns immediately for 0, so it shows no text and performs no movement of its
    // own. Announcing success first and then failing to move the player would leave him inside the
    // instance without a single hint that anything went wrong. Every TeleportTo overload returns
    // bool (Player.h:1250-1252) and every one of them is checked above.
    SendWalkInResult(teleported
        ? WorldPackets::WalkIn::WalkInResultCode::Success
        : WorldPackets::WalkIn::WalkInResultCode::InvalidTeleportLocation);
}
