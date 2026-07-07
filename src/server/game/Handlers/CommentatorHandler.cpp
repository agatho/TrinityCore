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
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "CommentatorPackets.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "Util.h"
#include <unordered_map>

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

void WorldSession::HandleCommentatorGetMapInfo(WorldPackets::Commentator::CommentatorGetMapInfo& /*getMapInfo*/)
{
    if (!IsCommentator())
        return;

    std::vector<Battleground*> arenas;
    sBattlegroundMgr->GetActiveArenas(arenas);

    // Group the active arenas by map into the client's map->instances catalogue.
    WorldPackets::Commentator::CommentatorMapInfo mapInfo;
    std::unordered_map<uint32, std::size_t> mapIndexByMapId;
    for (Battleground* arena : arenas)
    {
        uint32 const mapId = arena->GetMapId();
        auto [itr, inserted] = mapIndexByMapId.try_emplace(mapId, mapInfo.Maps.size());
        if (inserted)
        {
            WorldPackets::Commentator::CommentatorMapInfo::MapInfo& map = mapInfo.Maps.emplace_back();
            map.TeamSize = arena->GetArenaType();
            map.MinLevel = arena->GetMinLevel();
            map.MaxLevel = arena->GetMaxLevel();
        }

        WorldPackets::Commentator::CommentatorMapInfo::InstanceInfo& instance = mapInfo.Maps[itr->second].Instances.emplace_back();
        instance.MapID = mapId;
        instance.InstanceID = arena->GetInstanceID();
        instance.Status = arena->GetStatus();

        for (auto const& [guid, bgPlayer] : arena->GetPlayers())
        {
            TeamId const teamIndex = Battleground::GetTeamIndexByTeamId(bgPlayer.Team);
            if (teamIndex != TEAM_ALLIANCE && teamIndex != TEAM_HORDE)
                continue;

            WorldPackets::Commentator::CommentatorMapInfo::PlayerInfo& player = instance.Teams[teamIndex].Players.emplace_back();
            player.PlayerGUID = guid;
            player.Field3 = uint8(teamIndex);                              // hypothesis: faction (see dossier)
            if (Player* member = ObjectAccessor::FindConnectedPlayer(guid))
                player.Field1 = AsUnderlyingType(member->GetPrimarySpecialization());  // hypothesis: specID
        }
    }

    SendPacket(mapInfo.Write());
}

void WorldSession::HandleCommentatorEnterInstance(WorldPackets::Commentator::CommentatorEnterInstance& enterInstance)
{
    Player* player = GetPlayer();
    if (!player || !IsCommentator())
        return;

    // Arena instance ids are 32-bit; the high dword is unused for battlegrounds.
    uint32 const instanceId = enterInstance.InstanceIDLow;

    std::vector<Battleground*> arenas;
    sBattlegroundMgr->GetActiveArenas(arenas);
    Battleground* arena = nullptr;
    for (Battleground* bg : arenas)
    {
        if (bg->GetInstanceID() == instanceId && bg->GetMapId() == enterInstance.MapID)
        {
            arena = bg;
            break;
        }
    }

    if (!arena)
        return;

    // Enter as an observer: remember where we came from, satisfy the BG-map entry gate
    // (BattlegroundMap::CannotEnter requires GetBattlegroundId() == instanceId), and become an inert
    // game-master so participants don't see us and we can't affect the match.
    player->SetBattlegroundEntryPoint();
    player->SetBattlegroundId(arena->GetInstanceID(), arena->GetTypeID(), BATTLEGROUND_QUEUE_NONE);
    arena->AddSpectator(player->GetGUID());
    if (!player->IsGameMaster())
        player->SetGameMaster(true);

    if (WorldSafeLocsEntry const* start = arena->GetTeamStartPosition(TEAM_ALLIANCE))
        player->TeleportTo(start->Loc);
}

void WorldSession::HandleCommentatorExitInstance(WorldPackets::Commentator::CommentatorExitInstance& /*exitInstance*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Only act if we are actually spectating the arena we're currently bound to (not a real participant).
    Battleground* arena = sBattlegroundMgr->GetBattleground(player->GetBattlegroundId(), player->GetBattlegroundTypeId());
    if (!arena || !arena->HasSpectator(player->GetGUID()))
        return;

    arena->RemoveSpectator(player->GetGUID());
    player->SetSpectateTarget(ObjectGuid::Empty);
    if (player->IsGameMaster())
        player->SetGameMaster(false);
    player->TeleportToBGEntryPoint();
}

void WorldSession::HandleCommentatorSpectate(WorldPackets::Commentator::CommentatorSpectate& spectate)
{
    Player* player = GetPlayer();
    if (!player || !IsCommentator())
        return;

    // Follow the named player, but only if they are in the very arena we are spectating.
    Player* target = ObjectAccessor::FindConnectedPlayerByName(spectate.TargetName);
    if (target && player->GetBattlegroundId() != 0 && target->GetBattlegroundId() == player->GetBattlegroundId())
        player->SetSpectateTarget(target->GetGUID());
    else
        player->SetSpectateTarget(ObjectGuid::Empty);
}
