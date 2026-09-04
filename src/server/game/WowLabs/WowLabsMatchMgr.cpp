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

#include "WowLabsMatchMgr.h"
#include "LobbyMatchmakerPackets.h"
#include "Log.h"
#include "Map.h"
#include "MapManager.h"
#include "Player.h"
#include "World.h"
#include <algorithm>
#include <random>

bool WowLabsMatchMgr::Match::HasMember(ObjectGuid bnet) const
{
    for (MatchMember const& m : Members)
        if (m.BnetAccountGuid == bnet)
            return true;
    return false;
}

WowLabsMatchMgr* WowLabsMatchMgr::instance()
{
    static WowLabsMatchMgr instance;
    return &instance;
}

uint32 WowLabsMatchMgr::GenerateInstanceId()
{
    return _nextInstanceId++;
}

uint32 WowLabsMatchMgr::GenerateToken()
{
    // A non-zero opaque handle the client echoes back; only needs to be unpredictable within a run.
    static std::mt19937 rng{ std::random_device{}() };
    uint32 token = 0;
    do
        token = rng();
    while (token == 0);
    return token;
}

WowLabsMatchMgr::Match* WowLabsMatchMgr::CreateMatch(std::vector<MatchMember> members, uint8 gameMode)
{
    uint64 const id = _nextMatchId++;
    Match& match = _matches[id];
    match.Id = id;
    match.InstanceId = GenerateInstanceId();
    match.Token = GenerateToken();
    match.GameMode = gameMode;
    match.Members = std::move(members);

    TC_LOG_DEBUG("network", "WowLabs: reserved match {} (instance {}, token {}, mode {}) for {} players.",
        id, match.InstanceId, match.Token, uint32(gameMode), match.Members.size());
    return &match;
}

WowLabsMatchMgr::Match* WowLabsMatchMgr::FindByToken(uint32 token)
{
    if (!token)
        return nullptr;
    for (auto& kv : _matches)
        if (kv.second.Token == token)
            return &kv.second;
    return nullptr;
}

WowLabsMatchMgr::Match* WowLabsMatchMgr::FindByInstanceId(uint32 instanceId)
{
    if (!instanceId)
        return nullptr;
    for (auto& kv : _matches)
        if (kv.second.InstanceId == instanceId)
            return &kv.second;
    return nullptr;
}

WowLabsMatchMgr::Match* WowLabsMatchMgr::FindByMember(ObjectGuid bnet)
{
    for (auto& kv : _matches)
        if (kv.second.HasMember(bnet))
            return &kv.second;
    return nullptr;
}

void WowLabsMatchMgr::RemoveMatch(uint64 matchId)
{
    _matches.erase(matchId);
}

uint32 WowLabsMatchMgr::OwnRealmAddress() const
{
    return GetVirtualRealmAddress();
}

uint32 WowLabsMatchMgr::WireState(Phase phase)
{
    // Only Prematch is verified against the client (state == 3). The rest are provisional - they only ever
    // reach the client under GameRule::CharacterlessLogin, which a normal realm never sets.
    switch (phase)
    {
        case Phase::Prematch: return 3;   // verified
        case Phase::Active:   return 4;   // provisional
        case Phase::Ended:    return 5;   // provisional
        case Phase::Reserved:
        default:              return 0;
    }
}

std::vector<WowLabsMatchMgr::DropZone> const& WowLabsMatchMgr::GetDropZones(Match const* /*match*/) const
{
    // Hand-authored drop zones for MAP_WOWLABS. The real set lives in encrypted WoW Labs DB2s and is not
    // offline-obtainable; these are provisional coordinates purely so the area-selection flow has real,
    // validatable options. One set is shared by every match for now.
    static std::vector<DropZone> const zones =
    {
        { 1, 0,  1000.0f,  1000.0f, 200.0f },
        { 2, 0,  1000.0f, -1000.0f, 200.0f },
        { 3, 0, -1000.0f,  1000.0f, 200.0f },
        { 4, 0, -1000.0f, -1000.0f, 200.0f },
        { 5, 0,     0.0f,     0.0f, 250.0f },
        { 6, 0,  1500.0f,     0.0f, 200.0f },
        { 7, 0, -1500.0f,     0.0f, 200.0f },
        { 8, 0,     0.0f,  1500.0f, 200.0f },
    };
    return zones;
}

bool WowLabsMatchMgr::SelectArea(Match* match, ObjectGuid bnet, uint32 areaId)
{
    if (!match || !areaId)
        return false;

    std::vector<DropZone> const& zones = GetDropZones(match);
    bool const valid = std::any_of(zones.begin(), zones.end(), [areaId](DropZone const& z) { return z.Id == areaId; });
    if (!valid)
        return false;

    match->SelectedArea[bnet.GetCounter()] = areaId;
    return true;
}

uint32 WowLabsMatchMgr::GetSelectedArea(Match const* match, ObjectGuid bnet) const
{
    if (!match)
        return 0;
    auto itr = match->SelectedArea.find(bnet.GetCounter());
    return itr != match->SelectedArea.end() ? itr->second : 0;
}

void WowLabsMatchMgr::BeginPrematch(Match* match)
{
    if (!match)
        return;

    bool const firstEntry = match->MatchPhase != Phase::Prematch;
    match->MatchPhase = Phase::Prematch;

    // Tell every player currently in the match instance. Broadcasting on every entry (not only the first) means
    // a player who arrives into an already-running pre-match is told the state too; re-sending the same phase to
    // players already there is harmless. (Pre-login characterless sessions are not on the map; reaching them is
    // a live-client concern the offline build cannot exercise.)
    Map* map = sMapMgr->FindMap(MAP_ID, match->InstanceId);
    if (!map)
        return;

    WorldPackets::WowLabs::WowLabsNotifyPlayersMatchStateChanged packet;
    packet.State = WireState(Phase::Prematch);
    WorldPacket const* built = packet.Write();

    for (MapReference const& ref : map->GetPlayers())
        if (Player* player = ref.GetSource())
            player->SendDirectMessage(built);

    if (firstEntry)
        TC_LOG_DEBUG("network", "WowLabs: match {} (instance {}) entered pre-match.", match->Id, match->InstanceId);
}
