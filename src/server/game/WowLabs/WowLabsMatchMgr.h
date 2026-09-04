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

#ifndef TRINITYCORE_WOWLABS_MATCH_MGR_H
#define TRINITYCORE_WOWLABS_MATCH_MGR_H

#include "Define.h"
#include "ObjectGuid.h"
#include <string>
#include <unordered_map>
#include <vector>

// Plunderstorm / WoW Labs live-match registry (design: scratchpad plunderstorm_design.md, P3).
//
// A match is reserved the moment a lobby is acquired (every proposal member accepted). It owns:
//   - the roster (by Battle.net account guid), carried across the client's fast-login reconnect;
//   - a per-match instance of MAP_WOWLABS (map 2695) - Player::GetWowLabsInstanceId() returns it and
//     MapManager::CreateMap uses it to hand the player their own instance;
//   - a one-time fast-login token the client echoes in CMSG_REGISTER_FAST_LOGIN, so the server can tie
//     the reconnecting session back to the match it was proposed.
//
// The client handoff is: server sends SMSG_LOBBY_MATCHMAKER_LOBBY_ACQUIRED_SERVER (realm addr / token /
// game mode / map), the client creates its throwaway WoW Labs character, registers the fast-login target,
// then disconnects and reconnects to that realm+map. On a single-realm private server the realm address is
// simply our own (GetVirtualRealmAddress()); the reconnect lands on the same worldserver.
//
// Everything here runs on the main thread (lobby + world update), so no locking.
class TC_GAME_API WowLabsMatchMgr
{
public:
    static WowLabsMatchMgr* instance();

    WowLabsMatchMgr(WowLabsMatchMgr const&) = delete;
    WowLabsMatchMgr& operator=(WowLabsMatchMgr const&) = delete;

    static constexpr uint32 MAP_ID = 2695;   // MAP_WOWLABS

    struct MatchMember
    {
        ObjectGuid BnetAccountGuid;
        std::string Name;
    };

    struct Match
    {
        uint64 Id = 0;
        uint32 InstanceId = 0;        // this match's instance of MAP_WOWLABS
        uint32 Token = 0;             // one-time fast-login token echoed by the client
        uint8 GameMode = 0;           // PartyPlaylistEntry (Solo/Duo/Trio/Training)
        std::vector<MatchMember> Members;

        bool HasMember(ObjectGuid bnet) const;
    };

    // Reserve a match for a set of accepted members. Never returns nullptr.
    Match* CreateMatch(std::vector<MatchMember> members, uint8 gameMode);

    Match* FindByToken(uint32 token);
    Match* FindByInstanceId(uint32 instanceId);
    Match* FindByMember(ObjectGuid bnet);

    // Tear a match down (all players left / match ended). Safe to call with an unknown id.
    void RemoveMatch(uint64 matchId);

    // This realm's virtual realm address - the fast-login destination realm for a single-realm handoff.
    uint32 OwnRealmAddress() const;

private:
    WowLabsMatchMgr() = default;
    ~WowLabsMatchMgr() = default;

    uint32 GenerateInstanceId();
    uint32 GenerateToken();

    std::unordered_map<uint64 /*match id*/, Match> _matches;
    uint64 _nextMatchId = 1;
    // MAP_WOWLABS instances are keyed by (mapId, instanceId); since map 2695 is only ever created through the
    // WowLabs branch, a private counter is unique for it. Start high to stay clear of any hand-set ids.
    uint32 _nextInstanceId = 0x10000000;
};

#define sWowLabsMatchMgr WowLabsMatchMgr::instance()

#endif // TRINITYCORE_WOWLABS_MATCH_MGR_H
