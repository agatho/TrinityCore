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
#include "Player.h"
#include "RecentAlliesMgr.h"
#include "RecentAllyPackets.h"

void WorldSession::HandleSetAllowRecentAlliesSeeLocation(WorldPackets::Social::SetAllowRecentAlliesSeeLocation& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    RecentAllies::SetAllowSeeLocation(player->GetGUID(), packet.Allow);
}

void WorldSession::HandleRecentAllyRequestData(WorldPackets::Social::RecentAllyRequestData& /*packet*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    WorldPackets::Social::RecentAllyDataResponse response;
    for (RecentAllies::AllyRecord const& record : RecentAllies::GetAllies(player->GetGUID()))
    {
        WorldPackets::Social::RecentAllyInfo& info = response.Allies.emplace_back();
        info.Guid = record.Guid;
        info.WowAccount = record.WowAccount;
        info.Note = record.Note;
    }

    SendPacket(response.Write());
}

void WorldSession::HandleRecentAllySetNote(WorldPackets::Social::RecentAllySetNote& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    RecentAllies::SetNote(player->GetGUID(), packet.AllyGUID, packet.Note);

    // Echo the change back so the client updates the note in place.
    WorldPackets::Social::RecentAllyNoteUpdated updated;
    updated.AllyGUID = packet.AllyGUID;
    updated.Note = packet.Note;
    SendPacket(updated.Write());
}
