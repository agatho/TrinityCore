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
#include "Log.h"
#include "World.h"
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
