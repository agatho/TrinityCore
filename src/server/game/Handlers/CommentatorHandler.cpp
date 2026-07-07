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
#include "CommentatorPackets.h"
#include "Log.h"
#include "Player.h"
#include "RBAC.h"

void WorldSession::HandleCommentatorEnable(WorldPackets::Commentator::CommentatorEnable& packet)
{
    // Commentator mode is a privileged spectator capability - only accounts granted the permission may toggle it.
    if (!HasPermission(rbac::RBAC_PERM_USE_COMMENTATOR_MODE))
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleCommentatorEnable: {} tried to toggle commentator mode without permission",
            GetPlayerInfo());
        return;
    }

    bool const enabled = packet.Enable != 0;
    SetCommentator(enabled);

    // Confirm the new state to the client so its commentator UI can enable/disable.
    WorldPackets::Commentator::CommentatorStateChanged stateChanged;
    stateChanged.MatchGUID = _player ? _player->GetGUID() : ObjectGuid::Empty;
    stateChanged.Enabled = enabled;
    SendPacket(stateChanged.Write());
}
