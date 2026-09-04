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

class Map;

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

    // Match phase. Only Prematch has a verified wire value (client state == 3); the others carry provisional
    // wire values (WireState below) - the client only receives them under GameRule::CharacterlessLogin, which
    // a normal realm never sets, so exact non-prematch values are not offline-decidable.
    enum class Phase : uint8 { Reserved = 0, Prematch = 1, Active = 2, Ended = 3 };

    struct MatchMember
    {
        ObjectGuid BnetAccountGuid;
        std::string Name;
    };

    // A hand-authored WoW Labs drop zone (the real set lives in encrypted WoW Labs DB2s - see class comment).
    struct DropZone
    {
        uint32 Id = 0;
        int32 Type = 0;               // Enum.WoWLabsAreaType (values not offline-decidable)
        float X = 0.0f;
        float Y = 0.0f;
        float Z = 0.0f;
    };

    // One shrink step of the storm circle: hold at FromRadius for HoldMs, then shrink to ToRadius over ShrinkMs,
    // centred on (CenterX, CenterY). The real timings/coords live in encrypted WoW Labs DB2s; this is a hand-
    // authored provisional schedule (GetCircleSchedule) so the server can enforce a real, shrinking play area.
    struct CirclePhase
    {
        uint32 HoldMs = 0;
        uint32 ShrinkMs = 0;
        float FromRadius = 0.0f;
        float ToRadius = 0.0f;
        float CenterX = 0.0f;
        float CenterY = 0.0f;
    };

    struct Match
    {
        uint64 Id = 0;
        uint32 InstanceId = 0;        // this match's instance of MAP_WOWLABS
        uint32 Token = 0;             // one-time fast-login token echoed by the client
        uint8 GameMode = 0;           // PartyPlaylistEntry (Solo/Duo/Trio/Training)
        Phase MatchPhase = Phase::Reserved;
        std::vector<MatchMember> Members;
        std::unordered_map<uint64 /*bnet counter*/, uint32 /*areaId*/> SelectedArea;   // each member's drop pick

        // Circle / timing state (P5).
        uint32 PrematchElapsedMs = 0;
        uint32 ActiveElapsedMs = 0;   // time since the match went Active - drives the circle schedule
        uint32 DamageAccumMs = 0;     // out-of-ring damage cadence accumulator

        // Win / placement state (P6).
        uint32 PeakPlayers = 0;                 // most alive players seen at once - the field size for placement
        std::vector<ObjectGuid> FinishOrder;    // players by death order (first death first); winner is not here

        // Combat economy (P7).
        std::unordered_map<uint64 /*player counter*/, uint32> Kills;          // kills scored this match
        std::unordered_map<uint64 /*player counter*/, uint32> PlunderEarned;  // Plunder gained in-match (kills)

        bool HasMember(ObjectGuid bnet) const;
    };

    // Reserve a match for a set of accepted members. Never returns nullptr.
    Match* CreateMatch(std::vector<MatchMember> members, uint8 gameMode);

    Match* FindByToken(uint32 token);
    Match* FindByInstanceId(uint32 instanceId);
    Match* FindByMember(ObjectGuid bnet);

    // The newest match still open to join (Reserved / Prematch / Active), or nullptr. Used by .wowlabs join so a
    // second player can drop into an existing match instead of spawning their own.
    Match* GetNewestJoinableMatch();

    // Add a member to a match's roster if not already present (so the area handlers, keyed by bnet, find them).
    void AddMemberToMatch(Match* match, ObjectGuid bnet, std::string const& name);

    // Tear a match down (all players left / match ended). Safe to call with an unknown id.
    void RemoveMatch(uint64 matchId);

    // This realm's virtual realm address - the fast-login destination realm for a single-realm handoff.
    uint32 OwnRealmAddress() const;

    // --- Prematch / area selection (P4) ---

    // Move a match into the pre-match / area-selection phase and tell every player in its instance.
    void BeginPrematch(Match* match);

    // The drop zones offered this match (currently one hand-authored set for MAP_WOWLABS).
    std::vector<DropZone> const& GetDropZones(Match const* match) const;

    // Record a member's drop-zone pick. Returns false if the area id is not one of the offered zones.
    bool SelectArea(Match* match, ObjectGuid bnet, uint32 areaId);
    uint32 GetSelectedArea(Match const* match, ObjectGuid bnet) const;   // 0 == nothing selected

    // The wire value the client expects for a phase (Prematch -> 3 verified; others provisional).
    static uint32 WireState(Phase phase);

    // --- Circle controller (P5) ---

    // Per-instance tick, driven from Map::Update on the map's own thread (so touching players is thread-safe).
    // Advances the pre-match timer, transitions Prematch -> Active, and applies out-of-ring storm damage.
    void UpdateInstance(Map* map, uint32 diff);

    // The current storm circle for a match, sampled from the schedule at its active elapsed time. Returns false
    // before the match is Active or with no schedule.
    bool ComputeCircle(Match const* match, float& centerX, float& centerY, float& radius) const;

    std::vector<CirclePhase> const& GetCircleSchedule() const;

    // --- Win condition / rewards (P6) ---

    // End an active match: rank the players (winner + FinishOrder), award Plunder by placement, tell the clients
    // (MATCH_STATE_CHANGED -> Ended, and per-player MATCH_END when WowLabs.SendMatchEnd is enabled).
    void EndMatch(Map* map, Match* match, ObjectGuid winner);

    // A player killed another player. If both are in the same active match, credit the kill and award the
    // per-kill Plunder bounty (the in-match half of the economy; loot/abilities are the encrypted-DB2 ceiling).
    void OnPlayerKill(Player* killer, Player* killed);

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
