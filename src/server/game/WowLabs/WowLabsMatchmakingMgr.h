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

#ifndef TRINITYCORE_WOWLABS_MATCHMAKING_MGR_H
#define TRINITYCORE_WOWLABS_MATCHMAKING_MGR_H

#include "Define.h"
#include "ObjectGuid.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class WorldSession;

// Plunderstorm / WoW Labs pre-login matchmaking party service (design: scratchpad plunderstorm_design.md, P0).
//
// The glue-screen lobby (C_WoWLabsMatchmaking) forms a party BEFORE the player is in the world: the sessions
// are authenticated and sitting at the Plunderstorm lobby, with no Player object. So a party here is a set of
// WorldSessions keyed by their Battle.net account guid - not an in-world Group. This manager owns those
// parties, the outstanding invites, and pushes the roster to every member via SMSG_LOBBY_MATCHMAKER_PARTY_INFO.
//
// It is a pure lobby bookkeeper: no queue, no match (those are later phases). Everything is main-thread only
// (lobby packets are PROCESS_THREADUNSAFE), so no locking.
class TC_GAME_API WowLabsMatchmakingMgr
{
public:
    static WowLabsMatchmakingMgr* instance();

    WowLabsMatchmakingMgr(WowLabsMatchmakingMgr const&) = delete;
    WowLabsMatchmakingMgr& operator=(WowLabsMatchmakingMgr const&) = delete;

    static constexpr uint8 MAX_PARTY_SIZE = 3;   // Trio; Solo/Duo are just smaller parties

    struct Member
    {
        ObjectGuid BnetAccountGuid;   // party key for this member
        ObjectGuid PlayerGuid;        // the selected character's guid (roster display), may be empty pre-select
        std::string Name;             // display name shown in the lobby roster
        WorldSession* Session = nullptr;
        bool Ready = false;
    };

    struct Party
    {
        enum QueueState : uint8 { NotQueued = 0, Queued = 1, Proposed = 2 };

        uint64 Id = 0;
        ObjectGuid LeaderBnetGuid;
        std::vector<Member> Members;
        uint32 PlaylistEntry = 0;     // PartyPlaylistEntry { Solo=0, Duo=1, Trio=2, Training=3 }
        uint8 State = NotQueued;

        Member* FindMember(ObjectGuid bnet);
        bool IsLeader(ObjectGuid bnet) const { return LeaderBnetGuid == bnet; }
    };

    // Lobby actions (called from the CMSG_LOBBY_MATCHMAKER_* handlers). Each keeps the roster and invite state
    // consistent and sends the client the SMSG it expects; error paths go through SMSG_WOW_LABS_PARTY_ERROR.
    void Invite(WorldSession* inviter, ObjectGuid targetBnetGuid);
    void AcceptInvite(WorldSession* invitee, ObjectGuid inviterBnetGuid);
    void RejectInvite(WorldSession* invitee, ObjectGuid inviterBnetGuid);
    void Uninvite(WorldSession* leader, ObjectGuid targetBnetGuid);
    void LeaveParty(WorldSession* session);
    void SetPlaylistEntry(WorldSession* leader, uint32 playlistEntry);
    void SetReady(WorldSession* session, bool ready);

    // Queue phase (P2): a whole party enters the queue; when enough players are queued a lobby is proposed to
    // them; each member accepts/declines; on full acceptance the lobby is acquired (the client then fast-logs
    // into the match). AbandonQueue pulls the party out at any point.
    void EnterQueue(WorldSession* session);
    void RespondToProposal(WorldSession* session, bool accept);
    void AbandonQueue(WorldSession* session);

    // A session dropping out of the lobby (disconnect / logout / entering world) must be pulled from its party.
    void OnSessionLeave(WorldSession* session);

    // Sessions that touch the lobby register here so an invite can resolve a target bnet guid to its session.
    void RegisterSession(WorldSession* session);

    Party* GetPartyByBnet(ObjectGuid bnet);

private:
    WowLabsMatchmakingMgr() = default;
    ~WowLabsMatchmakingMgr() = default;

    // Get the session's party, creating a solo party for it if it is not in one yet (the lobby treats a lone
    // player as a party of one - that is what SetPlaylist/SetReady/EnterQueue act on).
    Party* GetOrCreatePartyFor(WorldSession* session);
    void DisbandIfEmpty(Party* party);
    void BroadcastPartyInfo(Party const* party);
    void SendPartyInfoTo(WorldSession* session, Party const* party);   // an empty/solo roster when party==nullptr

    static ObjectGuid BnetGuidOf(WorldSession* session);
    static std::string NameOf(WorldSession* session);

    WorldSession* FindLobbySession(ObjectGuid bnet) const;

    // Queue helpers.
    void SendQueueResult(Party const* party, uint8 status);   // to every member of the party
    void TryFormLobby();                                       // propose a lobby when enough players are queued
    void FinalizeProposal();                                   // all accepted -> acquire the lobby
    void DissolveProposal(uint8 statusToSend);                // decline/leave -> tell the members and reset
    uint32 MinLobbyPlayers() const;                           // config, default 1 (solo pop) for a private realm

    std::unordered_map<uint64 /*bnet counter*/, std::shared_ptr<Party>> _partiesByBnet;   // member bnet -> party
    std::unordered_map<uint64 /*invitee bnet*/, std::vector<ObjectGuid /*inviter bnet*/>> _pendingInvites;
    std::unordered_map<uint64 /*bnet counter*/, WorldSession*> _lobbySessions;

    std::vector<ObjectGuid> _queuedLeaders;                   // party leaders currently in queue
    std::vector<ObjectGuid> _proposalLeaders;                 // party leaders in the pending proposal
    std::unordered_map<uint64, bool> _proposalAccepted;       // member bnet counter -> accepted
    uint64 _nextPartyId = 1;
};

#define sWowLabsMatchmakingMgr WowLabsMatchmakingMgr::instance()

#endif // TRINITYCORE_WOWLABS_MATCHMAKING_MGR_H
