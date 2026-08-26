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

#ifndef __BATTLEGROUNDQUEUE_H
#define __BATTLEGROUNDQUEUE_H

#include "Common.h"
#include "DBCEnums.h"
#include "Battleground.h"
#include "EventProcessor.h"
#include <array>
#include <unordered_map>
#include <vector>

//this container can't be deque, because deque doesn't like removing the last element - if you remove it, it invalidates next iterator and crash appears
typedef std::list<Battleground*> BGFreeSlotQueueContainer;

#define COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME 10

struct GroupQueueInfo;                                      // type predefinition
struct PlayerQueueInfo                                      // stores information for players in queue
{
    uint32 LastOnlineTime;                                  // for tracking and removing offline players from queue after 5 minutes
    GroupQueueInfo* GroupInfo;                              // pointer to the associated groupqueueinfo
    // This player's own role, resolved once at AddGroup time. Tank and damager used to be indistinguishable
    // here because only lfg::PLAYER_ROLE_HEALER was ever tested anywhere under Battlegrounds/; they are told
    // apart by this field instead, and per PLAYER rather than per queue entry, so that the second member of a
    // duo does not have to inherit the queuer's role.
    // Primary source is the character's specialization (ChrSpecialization.db2 Role) because that is a fact
    // about the character rather than a claim on the wire, and because its numbering - Tank 0, Healer 1,
    // Dps 2 - is exactly the role order the client reads out of the role block in
    // SMSG_BATTLEFIELD_STATUS_WAIT_FOR_GROUPS. The queuer's declared lfg::PLAYER_ROLE_* mask narrows it when
    // the mask excludes the spec role (a Feral druid queueing as damage only, say).
    ChrSpecializationRole Role = ChrSpecializationRole::Dps;
};

struct GroupQueueInfo                                       // stores information about the group in queue (also used when joined as solo!)
{
    std::map<ObjectGuid, PlayerQueueInfo*> Players;         // player queue info map
    ::Team  Team;                                           // Player team (ALLIANCE/HORDE)
    uint32  JoinTime;                                       // time when group was added
    uint32  RemoveInviteTime;                               // time when we will remove invite for players in group
    uint32  IsInvitedToBGInstanceGUID;                      // was invited to certain BG
    uint32  ArenaTeamRating;                                // if rated match, inited to the rating of the team
    uint32  ArenaMatchmakerRating;                          // if rated match, inited to the rating of the team
    uint32  OpponentsTeamRating;                            // for rated arena matches
    uint32  OpponentsMatchmakerRating;                      // for rated arena matches
    uint8   Roles;                                          // lfg::PLAYER_ROLE_* mask from the join packet.
                                                            // For a group this is the QUEUER's mask only: the wire
                                                            // carries no per-member roles. Matchmaking therefore
                                                            // does NOT read this - it reads PlayerQueueInfo::Role,
                                                            // which exists for every member. This is kept as the
                                                            // record of what the queuer asked for, and is what
                                                            // narrows their own PlayerQueueInfo::Role.
};

enum BattlegroundQueueGroupTypes
{
    BG_QUEUE_PREMADE_ALLIANCE   = 0,
    BG_QUEUE_PREMADE_HORDE      = 1,
    BG_QUEUE_NORMAL_ALLIANCE    = 2,
    BG_QUEUE_NORMAL_HORDE       = 3
};
#define BG_QUEUE_GROUP_TYPES_COUNT 4

enum BattlegroundQueueInvitationType
{
    BG_QUEUE_INVITATION_TYPE_NO_BALANCE = 0, // no balance: N+M vs N players
    BG_QUEUE_INVITATION_TYPE_BALANCED   = 1, // teams balanced: N+1 vs N players
    BG_QUEUE_INVITATION_TYPE_EVEN       = 2  // teams even: N vs N players
};

std::size_t constexpr PVP_QUEUE_ROLE_COUNT = 3;             // ChrSpecializationRole: Tank, Healer, Dps

// Per-role headcounts, indexed by ChrSpecializationRole. Used both as the matchmaker's quota and as the
// payload of SMSG_BATTLEFIELD_STATUS_WAIT_FOR_GROUPS.
using PvpRoleHeadcount = std::array<uint8, PVP_QUEUE_ROLE_COUNT>;

struct BattlegroundProposalMember
{
    ObjectGuid Guid;
    TeamId Side = TEAM_ALLIANCE;
    ChrSpecializationRole Role = ChrSpecializationRole::Dps;
    bool Accepted = false;
    // Answered no. Such a member stays in Members so the role block keeps describing the whole lobby - the
    // client's own invariant, see the comment on BattlefieldStatusGroupProposalFailed - but is skipped by the
    // collapse, because leaving the queue is the declining session's own business.
    bool Declined = false;
};

/*
    An all-or-nothing group proposal, used by solo-queue modes (Battleground Blitz).

    The plain battleground invite is per player: everyone who presses Enter Battle ports in immediately and a
    decline only removes the decliner. That is wrong for a solo queue, where a 16 player lobby that loses one
    member is not a match. A proposal is therefore layered on top of the ordinary invite: the battleground is
    created and everyone is invited exactly as before, but nobody is ported until EVERY member has accepted.

    Collapse is all-or-nothing in both directions. If any member declines or the deadline passes:
      - the members who never accepted leave the queue, as a plain invite timeout would have done;
      - a member who actively declined stays listed as Declined so the role block still adds up to the lobby
        size, but is otherwise left to the leave-queue path of the session that declined;
      - the members who DID accept keep their GroupQueueInfo, and with it their JoinTime, so they return to
        the queue in the position they already held rather than at the back of it - they get their invite
        revoked, SMSG_BATTLEFIELD_STATUS_GROUP_PROPOSAL_FAILED, and then SMSG_BATTLEFIELD_STATUS_QUEUED again;
      - that last point holds per queue ENTRY, not per player. Blitz may be entered as a duo (BattlemasterList
        1101, MaxGroupSize 2), and a rated queue entry is atomic below the proposal: BattlegroundQueue::
        RemovePlayer drops every remaining member of a GroupQueueInfo as soon as one of them leaves it. A duo
        whose halves answered differently therefore leaves together, the accepting half included, with one
        SMSG_BATTLEFIELD_STATUS_NONE and no SMSG_BATTLEFIELD_STATUS_QUEUED before it. ResolveProposal settles
        that up front instead of letting it fall out of the removal order;
      - the battleground that was created for the proposal is dropped, so the next attempt picks a fresh map -
        which is exactly what retail does across the three proposal runs in C:\sniff\rated BG 12.0.7.pkt.
*/
struct BattlegroundProposal
{
    uint32 BgInstanceGUID = 0;
    BattlegroundTypeId BgTypeId = BATTLEGROUND_TYPE_NONE;
    BattlegroundBracketId BracketId = BattlegroundBracketId(0);
    uint32 MapId = 0;
    std::array<uint8, PVP_TEAMS_COUNT> SlotsPerSide = { };
    // Members are the players who were actually invited, i.e. the ones InviteGroupToBG found online. An
    // offline queue entry is neither invited nor able to accept, so counting it would deadlock the proposal
    // until the deadline.
    std::vector<BattlegroundProposalMember> Members;
};

// What SendProposalStatus should tell the members.
enum class BattlegroundProposalStatus
{
    Waiting,                                                // still collecting accepts
    Formed,                                                 // everyone accepted; clears the client's role display
    Failed                                                  // collapsed; outstanding members become losses
};

class Battleground;
class TC_GAME_API BattlegroundQueue
{
    public:
        BattlegroundQueue(BattlegroundQueueTypeId queueId);
        ~BattlegroundQueue();

        void BattlegroundQueueUpdate(uint32 diff, BattlegroundBracketId bracket_id, uint32 minRating = 0);
        void UpdateEvents(uint32 diff);

        void FillPlayersToBG(Battleground* bg, BattlegroundBracketId bracket_id);
        bool CheckPremadeMatch(BattlegroundBracketId bracket_id, uint32 MinPlayersPerTeam, uint32 MaxPlayersPerTeam);
        bool CheckNormalMatch(BattlegroundBracketId bracket_id, uint32 minPlayers, uint32 maxPlayers);
        bool CheckSkirmishForSameFaction(BattlegroundBracketId bracket_id, uint32 minPlayersPerTeam);
        // Solo-queue matchmaker for Battleground Blitz: fills both selection pools from the rated (premade-indexed)
        // lists, honouring a full per-role quota per team - tanks, healers and, as the remainder, damagers.
        // Returns false and leaves the pools untouched when the queue cannot yet field two full role-valid teams.
        bool CheckSoloQueueMatch(BattlegroundBracketId bracket_id, uint32 playersPerTeam, uint32 tanksPerTeam, uint32 healersPerTeam);
        GroupQueueInfo* AddGroup(Player const* leader, Group const* group, Team team, PVPDifficultyEntry const*  bracketEntry, bool isPremade, uint32 ArenaRating, uint32 MatchmakerRating, uint8 roles = 0);
        void RemovePlayer(ObjectGuid guid, bool decreaseInvitedCount);
        bool IsPlayerInvited(ObjectGuid pl_guid, const uint32 bgInstanceGuid, const uint32 removeTime);
        bool GetPlayerGroupInfoData(ObjectGuid guid, GroupQueueInfo* ginfo);
        void PlayerInvitedToBGUpdateAverageWaitTime(GroupQueueInfo* ginfo, BattlegroundBracketId bracket_id);
        uint32 GetAverageQueueWaitTime(GroupQueueInfo* ginfo, BattlegroundBracketId bracket_id) const;

        typedef std::map<ObjectGuid, PlayerQueueInfo> QueuedPlayersMap;
        QueuedPlayersMap m_QueuedPlayers;

        //do NOT use deque because deque.erase() invalidates ALL iterators
        typedef std::list<GroupQueueInfo*> GroupsQueueType;

        /*
        This two dimensional array is used to store All queued groups
        First dimension specifies the bgTypeId
        Second dimension specifies the player's group types -
             BG_QUEUE_PREMADE_ALLIANCE  is used for premade alliance groups and alliance rated arena teams
             BG_QUEUE_PREMADE_HORDE     is used for premade horde groups and horde rated arena teams
             BG_QUEUE_NORMAL_ALLIANCE   is used for normal (or small) alliance groups or non-rated arena matches
             BG_QUEUE_NORMAL_HORDE      is used for normal (or small) horde groups or non-rated arena matches
        */
        GroupsQueueType m_QueuedGroups[MAX_BATTLEGROUND_BRACKETS][BG_QUEUE_GROUP_TYPES_COUNT];

        // class to select and invite groups to bg
        class SelectionPool
        {
        public:
            SelectionPool(): PlayerCount(0) { }
            void Init();
            bool AddGroup(GroupQueueInfo* ginfo, uint32 desiredCount);
            bool KickGroup(uint32 size);
            uint32 GetPlayerCount() const {return PlayerCount;}
        public:
            GroupsQueueType SelectedGroups;
        private:
            uint32 PlayerCount;
        };

        //one selection pool for horde, other one for alliance
        SelectionPool m_SelectionPools[PVP_TEAMS_COUNT];
        uint32 GetPlayersInQueue(TeamId id);

        BattlegroundQueueTypeId GetQueueId() const { return m_queueId; }

        /* All-or-nothing group proposals - see BattlegroundProposal above. */

        // Records an accept. Returns false when the player is not in a proposal, in which case the caller
        // must fall back to the ordinary per-player invite handling. When the accept completes the proposal
        // this ports every member.
        bool ProposalAccept(ObjectGuid guid);
        // Records a decline and collapses the proposal around it. The decliner itself is NOT removed from the
        // queue here; the caller's ordinary leave-queue path still does that. Returns false when the player is
        // not in a proposal.
        bool ProposalDecline(ObjectGuid guid);
        // Deadline reached: collapses the proposal, dropping everyone who did not accept.
        void ProposalTimeout(uint32 bgInstanceGuid);

    private:

        BattlegroundQueueTypeId m_queueId;

        // inviteTime is how long the invited players have to answer. A proposal-managed invite passes
        // PROPOSAL_ACCEPT_WAIT_TIME and suppresses the per-player BGQueueRemoveEvent, because the proposal's
        // own deadline owns the removal - two independent removers racing on the same tick would strand half
        // the lobby.
        bool InviteGroupToBG(GroupQueueInfo* ginfo, Battleground* bg, Team side, uint32 inviteTime = INVITE_ACCEPT_WAIT_TIME, bool proposalManaged = false);

        // Opens a proposal over the two filled selection pools and tells its members it is running.
        void StartProposal(Battleground* bg, BattlegroundBracketId bracketId, uint32 playersPerTeam, PvpRoleHeadcount const& perSideQuota);
        // Ends a proposal. accepted == true ports everyone; otherwise the collapse described on
        // BattlegroundProposal runs. Erases the proposal either way.
        void ResolveProposal(uint32 bgInstanceGuid, bool accepted);
        void SendProposalStatus(BattlegroundProposal const& proposal, BattlegroundProposalStatus status) const;
        BattlegroundProposal* FindProposalFor(ObjectGuid guid);
        BattlegroundProposal const* FindProposalFor(ObjectGuid guid) const;

        // keyed by battleground instance guid; at most one proposal per created battleground
        std::unordered_map<uint32, BattlegroundProposal> m_Proposals;

        uint32 m_WaitTimes[PVP_TEAMS_COUNT][MAX_BATTLEGROUND_BRACKETS][COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME];
        uint32 m_WaitTimeLastPlayer[PVP_TEAMS_COUNT][MAX_BATTLEGROUND_BRACKETS];
        uint32 m_SumOfWaitTimes[PVP_TEAMS_COUNT][MAX_BATTLEGROUND_BRACKETS];

        // Event handler
        EventProcessor m_events;
};

/*
    This class is used to invite player to BG again, when minute lasts from his first invitation
    it is capable to solve all possibilities
*/
class BGQueueInviteEvent : public BasicEvent
{
    public:
        BGQueueInviteEvent(ObjectGuid pl_guid, uint32 BgInstanceGUID, BattlegroundTypeId BgTypeId, uint32 removeTime, BattlegroundQueueTypeId queueId)
            : m_PlayerGuid(pl_guid), m_BgInstanceGUID(BgInstanceGUID), m_BgTypeId(BgTypeId), m_RemoveTime(removeTime), m_QueueId(queueId)
        { }

        virtual bool Execute(uint64 e_time, uint32 p_time) override;
        virtual void Abort(uint64 e_time) override;
    private:
        ObjectGuid m_PlayerGuid;
        uint32 m_BgInstanceGUID;
        BattlegroundTypeId m_BgTypeId;
        uint32 m_RemoveTime;
        BattlegroundQueueTypeId m_QueueId;
};

/*
    Deadline of an all-or-nothing group proposal. One event per proposal, not per player: the whole lobby
    stands or falls together, so a single timer collapses it rather than PROPOSAL_ACCEPT_WAIT_TIME worth of
    per-player BGQueueRemoveEvents racing each other.
*/
class BGQueueProposalTimeoutEvent : public BasicEvent
{
    public:
        BGQueueProposalTimeoutEvent(uint32 bgInstanceGUID, BattlegroundQueueTypeId queueId)
            : m_BgInstanceGUID(bgInstanceGUID), m_QueueId(queueId)
        { }

        virtual bool Execute(uint64 e_time, uint32 p_time) override;
        virtual void Abort(uint64 e_time) override;
    private:
        uint32 m_BgInstanceGUID;
        BattlegroundQueueTypeId m_QueueId;
};

/*
    This class is used to remove player from BG queue after 1 minute 20 seconds from first invitation
    We must store removeInvite time in case player left queue and joined and is invited again
    We must store bgQueueTypeId, because battleground can be deleted already, when player entered it
*/
class BGQueueRemoveEvent : public BasicEvent
{
    public:
        BGQueueRemoveEvent(ObjectGuid pl_guid, uint32 bgInstanceGUID, BattlegroundQueueTypeId bgQueueTypeId, uint32 removeTime)
            : m_PlayerGuid(pl_guid), m_BgInstanceGUID(bgInstanceGUID), m_RemoveTime(removeTime), m_BgQueueTypeId(bgQueueTypeId)
        { }

        virtual bool Execute(uint64 e_time, uint32 p_time) override;
        virtual void Abort(uint64 e_time) override;
    private:
        ObjectGuid m_PlayerGuid;
        uint32 m_BgInstanceGUID;
        uint32 m_RemoveTime;
        BattlegroundQueueTypeId m_BgQueueTypeId;
};

#endif
