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

#include "Group.h"
#include "InstancePackets.h"
#include "MiscPackets.h"
#include "Player.h"
#include "WorldSession.h"

// C_PartyInfo.StartInstanceAbandonVote() - no arguments, HasRestrictions (PartyInfoDocumentation.lua:590).
// The wire packet still carries the optional PartyIndex the client fills in from the party context, so the
// group is resolved through it exactly as for CMSG_DO_READY_CHECK (GroupHandler.cpp HandleDoReadyCheckOpcode).
// The UI greys the entry out and shows the reason as a tooltip, so this path is only reachable through a
// macro, the /abandon slash command or a race. Both refusals therefore have to be answered with the game
// error the client already has a text for, otherwise the failure is silent (DEFINITION_OF_DONE D3).
void WorldSession::HandleStartInstanceAbandonVote(WorldPackets::Instance::StartInstanceAbandonVote& packet)
{
    Group* group = _player->GetGroup(packet.PartyIndex);
    if (!group)
    {
        SendPacket(WorldPackets::Misc::DisplayGameError(GameError::ERR_VOTE_TO_ABANDON_NOT_YET).Write());
        return;
    }

    GameError denyReason = GameError::ERR_VOTE_TO_ABANDON_NOT_YET;
    if (!group->CanStartInstanceAbandonVote(_player, &denyReason))
    {
        SendPacket(WorldPackets::Misc::DisplayGameError(denyReason).Write());
        return;
    }

    group->StartInstanceAbandonVote(_player->GetGUID());
}

// C_PartyInfo.SetInstanceAbandonVoteResponse(response) - one bool (PartyInfoDocumentation.lua:550), plus the
// optional PartyIndex from the party context, as in HandleReadyCheckResponseOpcode.
// The client answers optimistically (InstanceAbandon.lua:179-190) and never repeats a vote, so there is no
// error text for a rejected response; a double vote, a stale vote or a non member is simply dropped.
void WorldSession::HandleInstanceAbandonVoteResponse(WorldPackets::Instance::InstanceAbandonVoteResponse& packet)
{
    Group* group = _player->GetGroup(packet.PartyIndex);
    if (!group || !group->IsInstanceAbandonVoteInProgress())
        return;

    group->SetInstanceAbandonVoteResponse(_player->GetGUID(), packet.Accept);
}
