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
#include "BattlegroundScore.h"
#include "CommentatorPackets.h"
#include "DB2Stores.h"
#include "Group.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "SpellHistory.h"
#include "SpellPackets.h"
#include "Util.h"
#include <unordered_map>
#include <unordered_set>

namespace
{
    // Builds SMSG_COMMENTATOR_PLAYER_INFO for every participant of the given arena, filling the scalar stats
    // from the live battleground scores. The tracked-cooldown arrays are left empty (see packet notes).
    void BuildCommentatorPlayerInfo(Battleground* arena, WorldPackets::Commentator::CommentatorPlayerInfo& packet)
    {
        for (auto const& [guid, bgPlayer] : arena->GetPlayers())
        {
            WorldPackets::Commentator::CommentatorPlayerInfo::PlayerData& data = packet.Players.emplace_back();
            data.UnitGUID = guid;
            data.Faction = uint8(Battleground::GetTeamIndexByTeamId(bgPlayer.Team));

            Player* member = ObjectAccessor::FindConnectedPlayer(guid);
            if (member)
                data.Specialization = AsUnderlyingType(member->GetPrimarySpecialization());

            if (BattlegroundScore const* score = member ? arena->GetBattlegroundScore(member) : nullptr)
            {
                data.Kills = uint16(score->GetKillingBlows());
                data.Deaths = uint16(score->GetDeaths());
                data.DamageDone = score->GetDamageDone();
                data.HealingDone = score->GetHealingDone();
                // DamageTaken / HealingTaken / SoloShuffle round tallies have no score getter - left 0 (honest).
            }
        }
    }

    // Fills a player-record's cooldown array (array A) from the target's live SpellHistory, restricted to the
    // spells the commentator asked to track. The 44-byte wire record is exactly WorldPackets::Spells::
    // SpellHistoryEntry (proven identical to SMSG_SEND_SPELL_HISTORY), so we reuse TrinityCore's own builder -
    // the produced timings are byte-identical to what the client already parses for spell history.
    void BuildPlayerCooldowns(Player* target, std::vector<WorldPackets::Commentator::CommentatorGetPlayerCooldowns::TrackedSpell> const& tracked,
        std::vector<WorldPackets::Spells::SpellHistoryEntry>& out)
    {
        WorldPackets::Spells::SendSpellHistory history;
        target->GetSpellHistory()->WritePacket(&history);

        std::unordered_set<uint32> wanted;
        for (auto const& trackedSpell : tracked)
            wanted.insert(trackedSpell.SpellID);

        for (WorldPackets::Spells::SpellHistoryEntry const& entry : history.Entries)
            if (wanted.empty() || wanted.count(entry.SpellID))
                out.push_back(entry);
    }

    // Resolves the arena the given session is currently spectating (entered via CMSG_COMMENTATOR_ENTER_INSTANCE).
    Battleground* GetSpectatedArena(Player* player)
    {
        Battleground* arena = sBattlegroundMgr->GetBattleground(player->GetBattlegroundId(), player->GetBattlegroundTypeId());
        if (arena && arena->HasSpectator(player->GetGUID()))
            return arena;
        return nullptr;
    }
}

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

void WorldSession::HandleCommentatorGetPlayerInfo(WorldPackets::Commentator::CommentatorGetPlayerInfo& /*getPlayerInfo*/)
{
    Player* player = GetPlayer();
    if (!player || !IsCommentator())
        return;

    // Answer for the arena we are currently spectating (the request's context fields are opaque offline;
    // the spectated instance unambiguously identifies the match).
    Battleground* arena = GetSpectatedArena(player);
    if (!arena)
        return;

    WorldPackets::Commentator::CommentatorPlayerInfo playerInfo;
    BuildCommentatorPlayerInfo(arena, playerInfo);
    SendPacket(playerInfo.Write());
}

void WorldSession::HandleCommentatorStartWargame(WorldPackets::Commentator::CommentatorStartWargame& startWargame)
{
    Player* player = GetPlayer();
    if (!player || !IsCommentator())
        return;

    // Both captains must be online, distinct, and lead distinct groups.
    Player* captainOne = ObjectAccessor::FindConnectedPlayerByName(startWargame.TeamOneCaptain);
    Player* captainTwo = ObjectAccessor::FindConnectedPlayerByName(startWargame.TeamTwoCaptain);
    if (!captainOne || !captainTwo || captainOne == captainTwo)
        return;

    Group* groupOne = captainOne->GetGroup();
    Group* groupTwo = captainTwo->GetGroup();
    if (!groupOne || !groupTwo || groupOne == groupTwo)
        return;

    // Resolve the arena template + level bracket for the requested list.
    BattlegroundTypeId const bgTypeId = sBattlegroundMgr->GetRandomBG(BattlegroundTypeId(startWargame.ListID));
    BattlegroundTemplate const* bgTemplate = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(bgTypeId);
    if (!bgTemplate || !bgTemplate->IsArena() || bgTemplate->MapIDs.empty())
        return;

    PVPDifficultyEntry const* bracket = DB2Manager::GetBattlegroundBracketByLevel(bgTemplate->MapIDs.front(), captainOne->GetLevel());
    if (!bracket)
        return;

    BattlegroundQueueTypeId queueId;
    queueId.BattlemasterListId = uint16(startWargame.ListID);
    queueId.Type = uint8(BattlegroundQueueIdType::Wargame);
    queueId.Rated = false;
    queueId.TeamSize = uint8(startWargame.TeamSize);

    Battleground* arena = sBattlegroundMgr->CreateNewBattleground(queueId, bracket->GetBracketId());
    if (!arena)
        return;

    sBattlegroundMgr->AddBattleground(arena);

    // Port each captain's group in as opposing sides; Battleground::AddPlayer fires automatically on map arrival.
    auto portGroup = [&](Group* group, Team team)
    {
        for (GroupReference const& itr : group->GetMembers())
        {
            Player* member = itr.GetSource();
            if (!member || !member->IsInWorld())
                continue;

            member->SetBattlegroundEntryPoint();
            member->SetBattlegroundId(arena->GetInstanceID(), arena->GetTypeID(), queueId);
            member->SetBGTeam(team);
            BattlegroundMgr::SendToBattleground(member, arena);
        }
    };
    portGroup(groupOne, ALLIANCE);
    portGroup(groupTwo, HORDE);

    arena->StartBattleground();
}

void WorldSession::HandleCommentatorGetPlayerCooldowns(WorldPackets::Commentator::CommentatorGetPlayerCooldowns& getPlayerCooldowns)
{
    Player* player = GetPlayer();
    if (!player || !IsCommentator())
        return;

    // Cooldown data rides in the PLAYER_INFO record's array A. Rebuild the scalar player info for the
    // spectated arena, then populate the requested player's cooldown list from their live SpellHistory.
    Battleground* arena = GetSpectatedArena(player);
    if (!arena)
        return;

    WorldPackets::Commentator::CommentatorPlayerInfo playerInfo;
    BuildCommentatorPlayerInfo(arena, playerInfo);

    // The request names one participant + the spells the commentator tracks. Fill that participant's record
    // only (the loop is scoped to arena members, so cooldowns of non-participants can never leak).
    if (Player* target = ObjectAccessor::FindConnectedPlayer(getPlayerCooldowns.Player))
    {
        for (WorldPackets::Commentator::CommentatorPlayerInfo::PlayerData& data : playerInfo.Players)
        {
            if (data.UnitGUID != target->GetGUID())
                continue;

            BuildPlayerCooldowns(target, getPlayerCooldowns.TrackedSpells, data.Cooldowns);
            break;
        }
    }

    SendPacket(playerInfo.Write());
}
