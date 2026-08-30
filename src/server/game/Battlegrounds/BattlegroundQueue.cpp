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

#include "BattlegroundQueue.h"
#include "ArenaTeam.h"
#include "ArenaTeamMgr.h"
#include "BattlegroundMgr.h"
#include "BattlegroundPackets.h"
#include "Chat.h"
#include "DB2Stores.h"
#include "GameTime.h"
#include "Group.h"
#include "Language.h"
#include "LFG.h"
#include "Log.h"
#include "MapUtils.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "World.h"
#include <algorithm>
#include <utility>

/*********************************************************/
/***            BATTLEGROUND QUEUE SYSTEM              ***/
/*********************************************************/

namespace
{
// Resolve one queued player to a single role.
//
// The specialization is the primary source: ChrSpecialization.db2 Role is a fact about the character, it
// exists for every member of a duo and not only for the queuer who sent the packet, and its numbering
// (Tank 0, Healer 1, Dps 2) is the very numbering the client expects back in the role block of
// SMSG_BATTLEFIELD_STATUS_WAIT_FOR_GROUPS.
//
// declaredRoles is the queuer's lfg::PLAYER_ROLE_* mask and is only used to NARROW that: a player whose spec
// role is not among the roles they offered is placed on one they did offer instead. It is deliberately not
// the primary source - it is self-reported, it is absent for every member except the queuer, and three of the
// join handlers accept a mask with several bits set at once.
ChrSpecializationRole ResolveQueueRole(Player const* player, uint8 declaredRoles)
{
    ChrSpecializationRole specRole = ChrSpecializationRole::Dps;
    if (ChrSpecializationEntry const* spec = player->GetPrimarySpecializationEntry())
        specRole = spec->GetRole();

    if (!declaredRoles)
        return specRole;

    auto declares = [declaredRoles](ChrSpecializationRole role)
    {
        switch (role)
        {
            case ChrSpecializationRole::Tank:   return (declaredRoles & lfg::PLAYER_ROLE_TANK) != 0;
            case ChrSpecializationRole::Healer: return (declaredRoles & lfg::PLAYER_ROLE_HEALER) != 0;
            case ChrSpecializationRole::Dps:    return (declaredRoles & lfg::PLAYER_ROLE_DAMAGE) != 0;
            default:                            return false;
        }
    };

    if (declares(specRole))
        return specRole;

    // Spec role was not offered. Take the scarcest role they did offer - a tank who queued as anything else
    // is far more useful to the matchmaker as that other thing than as a tank it cannot actually field.
    if (declares(ChrSpecializationRole::Tank))
        return ChrSpecializationRole::Tank;
    if (declares(ChrSpecializationRole::Healer))
        return ChrSpecializationRole::Healer;
    if (declares(ChrSpecializationRole::Dps))
        return ChrSpecializationRole::Dps;

    // Mask carried only PLAYER_ROLE_LEADER or nothing recognisable.
    return specRole;
}

std::size_t RoleIndex(ChrSpecializationRole role)
{
    return std::size_t(AsUnderlyingType(role));
}

// What one side of a solo-queue match must consist of. Damagers are whatever the other two quotas leave.
PvpRoleHeadcount SideQuota(uint32 playersPerTeam, uint32 tanksPerTeam, uint32 healersPerTeam)
{
    return
    {
        uint8(tanksPerTeam),
        uint8(healersPerTeam),
        uint8(playersPerTeam - tanksPerTeam - healersPerTeam)
    };
}

// A role the mode does not ask for is not a role in that mode: its players are damage dealers, which is both
// what retail does - the captured rated Blitz lobby wanted 0 tanks and reported every protection or blood
// spec in it as a damager - and the only sane alternative to leaving tanks queued forever behind a quota of
// zero they can never fill. It is also what turns both quotas being 0 into "no role balancing at all",
// because then every player folds into the damage bucket and the damage bucket is the whole team.
ChrSpecializationRole FoldRole(ChrSpecializationRole role, PvpRoleHeadcount const& quota)
{
    return quota[RoleIndex(role)] ? role : ChrSpecializationRole::Dps;
}
}

BattlegroundQueue::BattlegroundQueue(BattlegroundQueueTypeId queueId) : m_queueId(queueId)
{
    for (uint32 i = 0; i < PVP_TEAMS_COUNT; ++i)
    {
        for (uint32 j = 0; j < MAX_BATTLEGROUND_BRACKETS; ++j)
        {
            m_SumOfWaitTimes[i][j] = 0;
            m_WaitTimeLastPlayer[i][j] = 0;
            for (uint32 k = 0; k < COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME; ++k)
                m_WaitTimes[i][j][k] = 0;
        }
    }
}

BattlegroundQueue::~BattlegroundQueue()
{
    m_events.KillAllEvents(false);

    for (int i = 0; i < MAX_BATTLEGROUND_BRACKETS; ++i)
    {
        for (uint32 j = 0; j < BG_QUEUE_GROUP_TYPES_COUNT; ++j)
        {
            for (GroupsQueueType::iterator itr = m_QueuedGroups[i][j].begin(); itr!= m_QueuedGroups[i][j].end(); ++itr)
                delete (*itr);
        }
    }
}

/*********************************************************/
/***      BATTLEGROUND QUEUE SELECTION POOLS           ***/
/*********************************************************/

// selection pool initialization, used to clean up from prev selection
void BattlegroundQueue::SelectionPool::Init()
{
    SelectedGroups.clear();
    PlayerCount = 0;
}

// remove group info from selection pool
// returns true when we need to try to add new group to selection pool
// returns false when selection pool is ok or when we kicked smaller group than we need to kick
// sometimes it can be called on empty selection pool
bool BattlegroundQueue::SelectionPool::KickGroup(uint32 size)
{
    //find maxgroup or LAST group with size == size and kick it
    bool found = false;
    GroupsQueueType::iterator groupToKick = SelectedGroups.begin();
    for (GroupsQueueType::iterator itr = groupToKick; itr != SelectedGroups.end(); ++itr)
    {
        if (abs((int32)((*itr)->Players.size() - size)) <= 1)
        {
            groupToKick = itr;
            found = true;
        }
        else if (!found && (*itr)->Players.size() >= (*groupToKick)->Players.size())
            groupToKick = itr;
    }
    //if pool is empty, do nothing
    if (GetPlayerCount())
    {
        //update player count
        GroupQueueInfo* ginfo = (*groupToKick);
        SelectedGroups.erase(groupToKick);
        PlayerCount -= ginfo->Players.size();
        //return false if we kicked smaller group or there are enough players in selection pool
        if (ginfo->Players.size() <= size + 1)
            return false;
    }
    return true;
}

// add group to selection pool
// used when building selection pools
// returns true if we can invite more players, or when we added group to selection pool
// returns false when selection pool is full
bool BattlegroundQueue::SelectionPool::AddGroup(GroupQueueInfo* ginfo, uint32 desiredCount)
{
    //if group is larger than desired count - don't allow to add it to pool
    if (!ginfo->IsInvitedToBGInstanceGUID && desiredCount >= PlayerCount + ginfo->Players.size())
    {
        SelectedGroups.push_back(ginfo);
        // increase selected players count
        PlayerCount += ginfo->Players.size();
        return true;
    }
    if (PlayerCount < desiredCount)
        return true;
    return false;
}

/*********************************************************/
/***               BATTLEGROUND QUEUES                 ***/
/*********************************************************/

// add group or player (grp == NULL) to bg queue with the given leader and bg specifications
GroupQueueInfo* BattlegroundQueue::AddGroup(Player const* leader, Group const* group, Team team, PVPDifficultyEntry const* bracketEntry, bool isPremade, uint32 ArenaRating, uint32 MatchmakerRating, uint8 roles)
{
    BattlegroundBracketId bracketId = bracketEntry->GetBracketId();

    // create new ginfo
    GroupQueueInfo* ginfo            = new GroupQueueInfo;
    ginfo->IsInvitedToBGInstanceGUID = 0;
    ginfo->JoinTime                  = GameTime::GetGameTimeMS();
    ginfo->RemoveInviteTime          = 0;
    ginfo->Team                      = team;
    ginfo->ArenaTeamRating           = ArenaRating;
    ginfo->ArenaMatchmakerRating     = MatchmakerRating;
    ginfo->OpponentsTeamRating       = 0;
    ginfo->OpponentsMatchmakerRating = 0;
    ginfo->Roles                     = roles;

    ginfo->Players.clear();

    //compute index (if group is premade or joined a rated match) to queues
    uint32 index = 0;
    if (!m_queueId.Rated && !isPremade)
        index += PVP_TEAMS_COUNT;
    if (ginfo->Team == HORDE)
        index++;
    TC_LOG_DEBUG("bg.battleground", "Adding Group to BattlegroundQueue bgTypeId : {}, bracket_id : {}, index : {}", m_queueId.BattlemasterListId, bracketId, index);

    uint32 lastOnlineTime = GameTime::GetGameTimeMS();

    //announce world (this don't need mutex)
    if (m_queueId.Rated && sWorld->getBoolConfig(CONFIG_ARENA_QUEUE_ANNOUNCER_ENABLE))
    {
        ArenaTeam* team = sArenaTeamMgr->GetArenaTeamById(0);
        if (team)
            sWorld->SendWorldText(LANG_ARENA_QUEUE_ANNOUNCE_WORLD_JOIN, team->GetName().c_str(), m_queueId.TeamSize, m_queueId.TeamSize, ginfo->ArenaTeamRating);
    }

    //add players from group to ginfo
    if (group)
    {
        for (GroupReference const& itr : group->GetMembers())
        {
            Player* member = itr.GetSource();
            PlayerQueueInfo& pl_info = m_QueuedPlayers[member->GetGUID()];
            pl_info.LastOnlineTime   = lastOnlineTime;
            pl_info.GroupInfo        = ginfo;
            // Only the queuer's declared mask exists on the wire, so it narrows only the queuer's own role;
            // every other member is resolved from their specialization alone.
            pl_info.Role             = ResolveQueueRole(member, member == leader ? roles : uint8(0));
            // add the pinfo to ginfo's list
            ginfo->Players[member->GetGUID()]  = &pl_info;
        }
    }
    else
    {
        PlayerQueueInfo& pl_info = m_QueuedPlayers[leader->GetGUID()];
        pl_info.LastOnlineTime   = lastOnlineTime;
        pl_info.GroupInfo        = ginfo;
        pl_info.Role             = ResolveQueueRole(leader, roles);
        ginfo->Players[leader->GetGUID()]  = &pl_info;
    }

    //add GroupInfo to m_QueuedGroups
    {
        m_QueuedGroups[bracketId][index].push_back(ginfo);

        //announce to world, this code needs mutex
        if (!m_queueId.Rated && !isPremade && sWorld->getBoolConfig(CONFIG_BATTLEGROUND_QUEUE_ANNOUNCER_ENABLE))
        {
            if (BattlegroundTemplate const* bg = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BattlegroundTypeId(m_queueId.BattlemasterListId)))
            {
                uint32 MinPlayers = bg->GetMinPlayersPerTeam();
                uint32 qHorde = 0;
                uint32 qAlliance = 0;
                uint32 q_min_level = bracketEntry->MinLevel;
                uint32 q_max_level = bracketEntry->MaxLevel;
                GroupsQueueType::const_iterator itr;
                for (itr = m_QueuedGroups[bracketId][BG_QUEUE_NORMAL_ALLIANCE].begin(); itr != m_QueuedGroups[bracketId][BG_QUEUE_NORMAL_ALLIANCE].end(); ++itr)
                    if (!(*itr)->IsInvitedToBGInstanceGUID)
                        qAlliance += (*itr)->Players.size();
                for (itr = m_QueuedGroups[bracketId][BG_QUEUE_NORMAL_HORDE].begin(); itr != m_QueuedGroups[bracketId][BG_QUEUE_NORMAL_HORDE].end(); ++itr)
                    if (!(*itr)->IsInvitedToBGInstanceGUID)
                        qHorde += (*itr)->Players.size();

                // Show queue status to player only (when joining queue)
                if (sWorld->getBoolConfig(CONFIG_BATTLEGROUND_QUEUE_ANNOUNCER_PLAYERONLY))
                {
                    ChatHandler chatHandler(leader->GetSession());
                    chatHandler.PSendSysMessage(LANG_BG_QUEUE_ANNOUNCE_SELF, bg->BattlemasterEntry->Name[chatHandler.GetSessionDbcLocale()], q_min_level, q_max_level,
                        qAlliance, (MinPlayers > qAlliance) ? MinPlayers - qAlliance : (uint32)0, qHorde, (MinPlayers > qHorde) ? MinPlayers - qHorde : (uint32)0);
                }
                // System message
                else
                {
                    sWorld->SendWorldText(LANG_BG_QUEUE_ANNOUNCE_WORLD, bg->BattlemasterEntry->Name[sWorld->GetDefaultDbcLocale()], q_min_level, q_max_level,
                        qAlliance, (MinPlayers > qAlliance) ? MinPlayers - qAlliance : (uint32)0, qHorde, (MinPlayers > qHorde) ? MinPlayers - qHorde : (uint32)0);
                }
            }
        }
    }

    return ginfo;
}

void BattlegroundQueue::PlayerInvitedToBGUpdateAverageWaitTime(GroupQueueInfo* ginfo, BattlegroundBracketId bracket_id)
{
    uint32 timeInQueue = getMSTimeDiff(ginfo->JoinTime, GameTime::GetGameTimeMS());
    uint8 team_index = TEAM_ALLIANCE;                    //default set to TEAM_ALLIANCE - or non rated arenas!
    if (!m_queueId.TeamSize)
    {
        if (ginfo->Team == HORDE)
            team_index = TEAM_HORDE;
    }
    else
    {
        if (m_queueId.Rated)
            team_index = TEAM_HORDE;                     //for rated arenas use TEAM_HORDE
    }

    //store pointer to arrayindex of player that was added first
    uint32* lastPlayerAddedPointer = &(m_WaitTimeLastPlayer[team_index][bracket_id]);
    //remove his time from sum
    m_SumOfWaitTimes[team_index][bracket_id] -= m_WaitTimes[team_index][bracket_id][(*lastPlayerAddedPointer)];
    //set average time to new
    m_WaitTimes[team_index][bracket_id][(*lastPlayerAddedPointer)] = timeInQueue;
    //add new time to sum
    m_SumOfWaitTimes[team_index][bracket_id] += timeInQueue;
    //set index of last player added to next one
    (*lastPlayerAddedPointer)++;
    (*lastPlayerAddedPointer) %= COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME;
}

uint32 BattlegroundQueue::GetAverageQueueWaitTime(GroupQueueInfo* ginfo, BattlegroundBracketId bracket_id) const
{
    uint8 team_index = TEAM_ALLIANCE;                    //default set to TEAM_ALLIANCE - or non rated arenas!
    if (!m_queueId.TeamSize)
    {
        if (ginfo->Team == HORDE)
            team_index = TEAM_HORDE;
    }
    else
    {
        if (m_queueId.Rated)
            team_index = TEAM_HORDE;                     //for rated arenas use TEAM_HORDE
    }
    //check if there is enought values(we always add values > 0)
    if (m_WaitTimes[team_index][bracket_id][COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME - 1])
        return (m_SumOfWaitTimes[team_index][bracket_id] / COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME);
    else
        //if there aren't enough values return 0 - not available
        return 0;
}

//remove player from queue and from group info, if group info is empty then remove it too
void BattlegroundQueue::RemovePlayer(ObjectGuid guid, bool decreaseInvitedCount)
{
    int32 bracket_id = -1;                                     // signed for proper for-loop finish
    QueuedPlayersMap::iterator itr;

    //remove player from map, if he's there
    itr = m_QueuedPlayers.find(guid);
    if (itr == m_QueuedPlayers.end())
    {
        //This happens if a player logs out while in a bg because WorldSession::LogoutPlayer() notifies the bg twice
        std::string playerName = "Unknown";
        if (Player* player = ObjectAccessor::FindPlayer(guid))
            playerName = player->GetName();
        TC_LOG_DEBUG("bg.battleground", "BattlegroundQueue: couldn't find player {} ({})", playerName, guid.ToString());
        return;
    }

    GroupQueueInfo* group = itr->second.GroupInfo;
    GroupsQueueType::iterator group_itr;
    // mostly people with the highest levels are in battlegrounds, thats why
    // we count from MAX_BATTLEGROUND_QUEUES - 1 to 0

    uint32 index = (group->Team == HORDE) ? BG_QUEUE_PREMADE_HORDE : BG_QUEUE_PREMADE_ALLIANCE;

    for (int32 bracket_id_tmp = MAX_BATTLEGROUND_BRACKETS - 1; bracket_id_tmp >= 0 && bracket_id == -1; --bracket_id_tmp)
    {
        //we must check premade and normal team's queue - because when players from premade are joining bg,
        //they leave groupinfo so we can't use its players size to find out index
        for (uint32 j = index; j < BG_QUEUE_GROUP_TYPES_COUNT; j += PVP_TEAMS_COUNT)
        {
            GroupsQueueType::iterator k = m_QueuedGroups[bracket_id_tmp][j].begin();
            for (; k != m_QueuedGroups[bracket_id_tmp][j].end(); ++k)
            {
                if ((*k) == group)
                {
                    bracket_id = bracket_id_tmp;
                    group_itr = k;
                    //we must store index to be able to erase iterator
                    index = j;
                    break;
                }
            }
        }
    }

    //player can't be in queue without group, but just in case
    if (bracket_id == -1)
    {
        TC_LOG_ERROR("bg.battleground", "BattlegroundQueue: ERROR Cannot find groupinfo for {}", guid.ToString());
        return;
    }
    TC_LOG_DEBUG("bg.battleground", "BattlegroundQueue: Removing {}, from bracket_id {}", guid.ToString(), (uint32)bracket_id);

    // ALL variables are correctly set
    // We can ignore leveling up in queue - it should not cause crash
    // remove player from group
    // if only one player there, remove group

    // remove player queue info from group queue info
    std::map<ObjectGuid, PlayerQueueInfo*>::iterator pitr = group->Players.find(guid);
    if (pitr != group->Players.end())
        group->Players.erase(pitr);

    // if invited to bg, and should decrease invited count, then do it
    if (decreaseInvitedCount && group->IsInvitedToBGInstanceGUID)
        if (Battleground* bg = sBattlegroundMgr->GetBattleground(group->IsInvitedToBGInstanceGUID, BattlegroundTypeId(m_queueId.BattlemasterListId)))
            bg->DecreaseInvitedCount(group->Team);

    // remove player queue info
    m_QueuedPlayers.erase(itr);

    // announce to world if arena team left queue for rated match, show only once
    if (m_queueId.TeamSize && m_queueId.Rated && group->Players.empty() && sWorld->getBoolConfig(CONFIG_ARENA_QUEUE_ANNOUNCER_ENABLE))
        if (ArenaTeam* team = sArenaTeamMgr->GetArenaTeamById(0))
            sWorld->SendWorldText(LANG_ARENA_QUEUE_ANNOUNCE_WORLD_EXIT, team->GetName().c_str(), m_queueId.TeamSize, m_queueId.TeamSize, group->ArenaTeamRating);

    // if player leaves queue and he is invited to rated arena match, then he have to lose
    if (group->IsInvitedToBGInstanceGUID && m_queueId.Rated && decreaseInvitedCount)
    {
        if (ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(0))
        {
            TC_LOG_DEBUG("bg.battleground", "UPDATING memberLost's personal arena rating for {} by opponents rating: {}", guid.ToString(), group->OpponentsTeamRating);
            if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
                at->MemberLost(player, group->OpponentsMatchmakerRating);
            else
                at->OfflineMemberLost(guid, group->OpponentsMatchmakerRating);
            at->SaveToDB();
        }
    }

    // remove group queue info if needed
    if (group->Players.empty())
    {
        m_QueuedGroups[bracket_id][index].erase(group_itr);
        delete group;
        return;
    }

    // if group wasn't empty, so it wasn't deleted, and player have left a rated
    // queue -> everyone from the group should leave too
    // don't remove recursively if already invited to bg!
    if (!group->IsInvitedToBGInstanceGUID && m_queueId.Rated)
    {
        // remove next player, this is recursive
        // first send removal information
        if (Player* plr2 = ObjectAccessor::FindConnectedPlayer(group->Players.begin()->first))
        {
            uint32 queueSlot = plr2->GetBattlegroundQueueIndex(m_queueId);

            plr2->RemoveBattlegroundQueueId(m_queueId); // must be called this way, because if you move this call to
                                                            // queue->removeplayer, it causes bugs

            WorldPackets::Battleground::BattlefieldStatusNone battlefieldStatus;
            BattlegroundMgr::BuildBattlegroundStatusNone(&battlefieldStatus, plr2, queueSlot, plr2->GetBattlegroundQueueJoinTime(m_queueId));
            plr2->SendDirectMessage(battlefieldStatus.Write());
        }
        // then actually delete, this may delete the group as well!
        RemovePlayer(group->Players.begin()->first, decreaseInvitedCount);
    }
}

ChrSpecializationRole BattlegroundQueue::GetPlayerRole(ObjectGuid guid) const
{
    QueuedPlayersMap::const_iterator itr = m_QueuedPlayers.find(guid);
    return itr != m_QueuedPlayers.end() ? itr->second.Role : ChrSpecializationRole::Dps;
}

//returns true when player pl_guid is in queue and is invited to bgInstanceGuid
bool BattlegroundQueue::IsPlayerInvited(ObjectGuid pl_guid, const uint32 bgInstanceGuid, const uint32 removeTime)
{
    QueuedPlayersMap::const_iterator qItr = m_QueuedPlayers.find(pl_guid);
    return (qItr != m_QueuedPlayers.end()
        && qItr->second.GroupInfo->IsInvitedToBGInstanceGUID == bgInstanceGuid
        && qItr->second.GroupInfo->RemoveInviteTime == removeTime);
}

bool BattlegroundQueue::GetPlayerGroupInfoData(ObjectGuid guid, GroupQueueInfo* ginfo)
{
    QueuedPlayersMap::const_iterator qItr = m_QueuedPlayers.find(guid);
    if (qItr == m_QueuedPlayers.end())
        return false;
    *ginfo = *(qItr->second.GroupInfo);
    return true;
}

uint32 BattlegroundQueue::GetPlayersInQueue(TeamId id)
{
    return m_SelectionPools[id].GetPlayerCount();
}

bool BattlegroundQueue::InviteGroupToBG(GroupQueueInfo* ginfo, Battleground* bg, Team side, uint32 inviteTime /*= INVITE_ACCEPT_WAIT_TIME*/, bool proposalManaged /*= false*/)
{
    // set side if needed
    if (side)
        ginfo->Team = side;

    if (!ginfo->IsInvitedToBGInstanceGUID)
    {
        // not yet invited
        // set invitation
        ginfo->IsInvitedToBGInstanceGUID = bg->GetInstanceID();
        BattlegroundTypeId bgTypeId = BattlegroundTypeId(m_queueId.BattlemasterListId);
        BattlegroundQueueTypeId bgQueueTypeId = m_queueId;
        BattlegroundBracketId bracket_id = bg->GetBracketId();

        ginfo->RemoveInviteTime = GameTime::GetGameTimeMS() + inviteTime;

        // loop through the players
        for (std::map<ObjectGuid, PlayerQueueInfo*>::iterator itr = ginfo->Players.begin(); itr != ginfo->Players.end(); ++itr)
        {
            // get the player
            Player* player = ObjectAccessor::FindConnectedPlayer(itr->first);
            // if offline, skip him, this should not happen - player is removed from queue when he logs out
            if (!player)
                continue;

            // invite the player
            PlayerInvitedToBGUpdateAverageWaitTime(ginfo, bracket_id);
            //sBattlegroundMgr->InvitePlayer(player, bg, ginfo->Team);

            // set invited player counters
            bg->IncreaseInvitedCount(ginfo->Team);

            player->SetInviteForBattlegroundQueueType(bgQueueTypeId, ginfo->IsInvitedToBGInstanceGUID);

            // create remind invite events - but never under a proposal. Retail sends exactly ONE
            // SMSG_BATTLEFIELD_STATUS_NEED_CONFIRMATION per proposal window, the one at its start:
            // "C:/sniff/rated BG 12.0.7.pkt" has three windows and each carries a single 0x480000
            // (12.0.7 numbering throughout this capture note: 0x480000 = SMSG_BATTLEFIELD_STATUS_NEED_
            // CONFIRMATION, 0x48000D = ..._WAIT_FOR_GROUPS, 0x48000E = ..._GROUP_PROPOSAL_FAILED. 12.1
            // shifted the families up by three, so this tree registers them as 0x4B0000/0x4B000D/0x4B000E.)
            // (ticks 136714 / 864798 / 896705), followed only by 0x48000D until the window ends at
            // 166467 / 894382 / 916110 - 29,7 s, 29,6 s and an early STATUS_ACTIVE, never a second
            // confirmation. Without this gate the reminder would land INVITATION_REMIND_TIME before
            // the deadline, i.e. 8000 ms into the 28000 ms PROPOSAL_CONFIRM_WAIT_TIME window, and
            // overwrite the role display that SendProposalStatus is maintaining. Worse, it would go
            // out to members who already accepted: ProposalAccept only sets member.Accepted and
            // leaves ginfo->IsInvitedToBGInstanceGUID and ginfo->RemoveInviteTime alone, so the
            // IsPlayerInvited check in BGQueueInviteEvent::Execute stays true for the whole window.
            if (!proposalManaged && inviteTime > INVITATION_REMIND_TIME)
            // create remind invite events
            if (inviteTime > INVITATION_REMIND_TIME)
            {
                BGQueueInviteEvent* inviteEvent = new BGQueueInviteEvent(player->GetGUID(), ginfo->IsInvitedToBGInstanceGUID, bgTypeId, ginfo->RemoveInviteTime, m_queueId);
                m_events.AddEvent(inviteEvent, m_events.CalculateTime(Milliseconds(inviteTime - INVITATION_REMIND_TIME)));
            }
            // create automatic remove events - but never under a proposal, whose own deadline event removes
            // the players who did not accept and requeues the ones who did. Both firing on the same tick
            // would tear the lobby in half.
            if (!proposalManaged)
            {
                BGQueueRemoveEvent* removeEvent = new BGQueueRemoveEvent(player->GetGUID(), ginfo->IsInvitedToBGInstanceGUID, bgQueueTypeId, ginfo->RemoveInviteTime);
                m_events.AddEvent(removeEvent, m_events.CalculateTime(Milliseconds(inviteTime)));
            }

            uint32 queueSlot = player->GetBattlegroundQueueIndex(bgQueueTypeId);

            TC_LOG_DEBUG("bg.battleground", "Battleground: invited player {} {} to BG instance {} queueindex {} bgtype {}",
                 player->GetName(), player->GetGUID().ToString(), bg->GetInstanceID(), queueSlot, m_queueId.BattlemasterListId);

            WorldPackets::Battleground::BattlefieldStatusNeedConfirmation battlefieldStatus;
            BattlegroundMgr::BuildBattlegroundStatusNeedConfirmation(&battlefieldStatus, bg, player, queueSlot, player->GetBattlegroundQueueJoinTime(bgQueueTypeId), inviteTime, bgQueueTypeId, itr->second->Role);
            BattlegroundMgr::BuildBattlegroundStatusNeedConfirmation(&battlefieldStatus, bg, player, queueSlot, player->GetBattlegroundQueueJoinTime(bgQueueTypeId), inviteTime, bgQueueTypeId);
            player->SendDirectMessage(battlefieldStatus.Write());
        }
        return true;
    }

    return false;
}

/*
This function is inviting players to already running battlegrounds
Invitation type is based on config file
large groups are disadvantageous, because they will be kicked first if invitation type = 1
*/
void BattlegroundQueue::FillPlayersToBG(Battleground* bg, BattlegroundBracketId bracket_id)
{
    int32 hordeFree = bg->GetFreeSlotsForTeam(HORDE);
    int32 aliFree   = bg->GetFreeSlotsForTeam(ALLIANCE);
    uint32 aliCount   = m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE].size();
    uint32 hordeCount = m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_HORDE].size();

    // try to get even teams
    if (sWorld->getIntConfig(CONFIG_BATTLEGROUND_INVITATION_TYPE) == BG_QUEUE_INVITATION_TYPE_EVEN)
    {
        // check if the teams are even
        if (hordeFree == 1 && aliFree == 1)
        {
            // if we are here, the teams have the same amount of players
            // then we have to allow to join the same amount of players
            int32 hordeExtra = hordeCount - aliCount;
            int32 aliExtra   = aliCount - hordeCount;

            hordeExtra = std::max(hordeExtra, 0);
            aliExtra   = std::max(aliExtra, 0);

            if (aliCount != hordeCount)
            {
                aliFree   -= aliExtra;
                hordeFree -= hordeExtra;

                aliFree   = std::max(aliFree, 0);
                hordeFree = std::max(hordeFree, 0);
            }
        }
    }

    //iterator for iterating through bg queue
    GroupsQueueType::const_iterator Ali_itr = m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE].begin();
    //count of groups in queue - used to stop cycles

    //index to queue which group is current
    uint32 aliIndex = 0;
    for (; aliIndex < aliCount && m_SelectionPools[TEAM_ALLIANCE].AddGroup((*Ali_itr), aliFree); aliIndex++)
        ++Ali_itr;
    //the same thing for horde
    GroupsQueueType::const_iterator Horde_itr = m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_HORDE].begin();

    uint32 hordeIndex = 0;
    for (; hordeIndex < hordeCount && m_SelectionPools[TEAM_HORDE].AddGroup((*Horde_itr), hordeFree); hordeIndex++)
        ++Horde_itr;

    //if ofc like BG queue invitation is set in config, then we are happy
    if (sWorld->getIntConfig(CONFIG_BATTLEGROUND_INVITATION_TYPE) == BG_QUEUE_INVITATION_TYPE_NO_BALANCE)
        return;

    /*
    if we reached this code, then we have to solve NP - complete problem called Subset sum problem
    So one solution is to check all possible invitation subgroups, or we can use these conditions:
    1. Last time when BattlegroundQueue::Update was executed we invited all possible players - so there is only small possibility
        that we will invite now whole queue, because only 1 change has been made to queues from the last BattlegroundQueue::Update call
    2. Other thing we should consider is group order in queue
    */

    // At first we need to compare free space in bg and our selection pool
    int32 diffAli   = aliFree   - int32(m_SelectionPools[TEAM_ALLIANCE].GetPlayerCount());
    int32 diffHorde = hordeFree - int32(m_SelectionPools[TEAM_HORDE].GetPlayerCount());
    while (abs(diffAli - diffHorde) > 1 && (m_SelectionPools[TEAM_HORDE].GetPlayerCount() > 0 || m_SelectionPools[TEAM_ALLIANCE].GetPlayerCount() > 0))
    {
        //each cycle execution we need to kick at least 1 group
        if (diffAli < diffHorde)
        {
            //kick alliance group, add to pool new group if needed
            if (m_SelectionPools[TEAM_ALLIANCE].KickGroup(diffHorde - diffAli))
            {
                for (; aliIndex < aliCount && m_SelectionPools[TEAM_ALLIANCE].AddGroup((*Ali_itr), (aliFree >= diffHorde) ? aliFree - diffHorde : 0); aliIndex++)
                    ++Ali_itr;
            }
            //if ali selection is already empty, then kick horde group, but if there are less horde than ali in bg - break;
            if (!m_SelectionPools[TEAM_ALLIANCE].GetPlayerCount())
            {
                if (aliFree <= diffHorde + 1)
                    break;
                m_SelectionPools[TEAM_HORDE].KickGroup(diffHorde - diffAli);
            }
        }
        else
        {
            //kick horde group, add to pool new group if needed
            if (m_SelectionPools[TEAM_HORDE].KickGroup(diffAli - diffHorde))
            {
                for (; hordeIndex < hordeCount && m_SelectionPools[TEAM_HORDE].AddGroup((*Horde_itr), (hordeFree >= diffAli) ? hordeFree - diffAli : 0); hordeIndex++)
                    ++Horde_itr;
            }
            if (!m_SelectionPools[TEAM_HORDE].GetPlayerCount())
            {
                if (hordeFree <= diffAli + 1)
                    break;
                m_SelectionPools[TEAM_ALLIANCE].KickGroup(diffAli - diffHorde);
            }
        }
        //count diffs after small update
        diffAli   = aliFree   - int32(m_SelectionPools[TEAM_ALLIANCE].GetPlayerCount());
        diffHorde = hordeFree - int32(m_SelectionPools[TEAM_HORDE].GetPlayerCount());
    }
}

// this method checks if premade versus premade battleground is possible
// then after 30 mins (default) in queue it moves premade group to normal queue
// it tries to invite as much players as it can - to MaxPlayersPerTeam, because premade groups have more than MinPlayersPerTeam players
bool BattlegroundQueue::CheckPremadeMatch(BattlegroundBracketId bracket_id, uint32 MinPlayersPerTeam, uint32 MaxPlayersPerTeam)
{
    //check match
    if (!m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE].empty() && !m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_HORDE].empty())
    {
        //start premade match
        //if groups aren't invited
        GroupsQueueType::const_iterator ali_group, horde_group;
        for (ali_group = m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE].begin(); ali_group != m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE].end(); ++ali_group)
            if (!(*ali_group)->IsInvitedToBGInstanceGUID)
                break;
        for (horde_group = m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_HORDE].begin(); horde_group != m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_HORDE].end(); ++horde_group)
            if (!(*horde_group)->IsInvitedToBGInstanceGUID)
                break;

        if (ali_group != m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE].end() && horde_group != m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_HORDE].end())
        {
            m_SelectionPools[TEAM_ALLIANCE].AddGroup((*ali_group), MaxPlayersPerTeam);
            m_SelectionPools[TEAM_HORDE].AddGroup((*horde_group), MaxPlayersPerTeam);
            //add groups/players from normal queue to size of bigger group
            uint32 maxPlayers = std::min(m_SelectionPools[TEAM_ALLIANCE].GetPlayerCount(), m_SelectionPools[TEAM_HORDE].GetPlayerCount());
            GroupsQueueType::const_iterator itr;
            for (uint32 i = 0; i < PVP_TEAMS_COUNT; i++)
            {
                for (itr = m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE + i].begin(); itr != m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE + i].end(); ++itr)
                {
                    //if itr can join BG and player count is less that maxPlayers, then add group to selectionpool
                    if (!(*itr)->IsInvitedToBGInstanceGUID && !m_SelectionPools[i].AddGroup((*itr), maxPlayers))
                        break;
                }
            }
            //premade selection pools are set
            return true;
        }
    }
    // now check if we can move group from Premade queue to normal queue (timer has expired) or group size lowered!!
    // this could be 2 cycles but i'm checking only first team in queue - it can cause problem -
    // if first is invited to BG and seconds timer expired, but we can ignore it, because players have only 80 seconds to click to enter bg
    // and when they click or after 80 seconds the queue info is removed from queue
    //
    // For a RATED queue this demotion is a one-way trip out of the game. It is written for the unrated case,
    // where a premade that waited too long gives up its premade privilege and is dealt into the ordinary
    // player pool by CheckNormalMatch. A rated queue has no such pool: AddGroup routes every rated entry into
    // BG_QUEUE_PREMADE_* (`if (!m_queueId.Rated && !isPremade) index += PVP_TEAMS_COUNT`), CheckNormalMatch is
    // only reached from the `if (!m_queueId.Rated)` branch of BattlegroundQueueUpdate, and the two rated
    // matchmakers - the premade pairing above and CheckSoloQueueMatch - both read the premade lists only.
    // An entry moved to BG_QUEUE_NORMAL_* there is therefore never looked at again: the player keeps his
    // queue slot and his status frame, no SMSG_BATTLEFIELD_STATUS_NONE or _FAILED is ever sent, and the
    // queue only looks alive. A full rated battleground group hits this after
    // Battleground.PremadeGroupWaitForMatch (30 min by default) purely on the JoinTime half of the test.
    if (!m_queueId.Rated)
    {
        uint32 time_before = GameTime::GetGameTimeMS() - sWorld->getIntConfig(CONFIG_BATTLEGROUND_PREMADE_GROUP_WAIT_FOR_MATCH);
        for (uint32 i = 0; i < PVP_TEAMS_COUNT; i++)
        {
            if (!m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE + i].empty())
            {
                GroupsQueueType::iterator itr = m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE + i].begin();
                if (!(*itr)->IsInvitedToBGInstanceGUID && ((*itr)->JoinTime < time_before || (*itr)->Players.size() < MinPlayersPerTeam))
                {
                    //we must insert group to normal queue and erase pointer from premade queue
                    m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE + i].push_front((*itr));
                    m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE + i].erase(itr);
                }
            }
        }
    }
    //selection pools are not set
    return false;
}

// this method tries to create battleground or arena with MinPlayersPerTeam against MinPlayersPerTeam
bool BattlegroundQueue::CheckNormalMatch(BattlegroundBracketId bracket_id, uint32 minPlayers, uint32 maxPlayers)
{
    GroupsQueueType::const_iterator itr_team[PVP_TEAMS_COUNT];
    for (uint32 i = 0; i < PVP_TEAMS_COUNT; i++)
    {
        itr_team[i] = m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE + i].begin();
        for (; itr_team[i] != m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE + i].end(); ++(itr_team[i]))
        {
            if (!(*(itr_team[i]))->IsInvitedToBGInstanceGUID)
            {
                m_SelectionPools[i].AddGroup(*(itr_team[i]), maxPlayers);
                if (m_SelectionPools[i].GetPlayerCount() >= minPlayers)
                    break;
            }
        }
    }
    //try to invite same number of players - this cycle may cause longer wait time even if there are enough players in queue, but we want ballanced bg
    uint32 j = TEAM_ALLIANCE;
    if (m_SelectionPools[TEAM_HORDE].GetPlayerCount() < m_SelectionPools[TEAM_ALLIANCE].GetPlayerCount())
        j = TEAM_HORDE;
    if (sWorld->getIntConfig(CONFIG_BATTLEGROUND_INVITATION_TYPE) != BG_QUEUE_INVITATION_TYPE_NO_BALANCE
        && m_SelectionPools[TEAM_HORDE].GetPlayerCount() >= minPlayers && m_SelectionPools[TEAM_ALLIANCE].GetPlayerCount() >= minPlayers)
    {
        //we will try to invite more groups to team with less players indexed by j
        ++(itr_team[j]);                                         //this will not cause a crash, because for cycle above reached break;
        for (; itr_team[j] != m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE + j].end(); ++(itr_team[j]))
        {
            if (!(*(itr_team[j]))->IsInvitedToBGInstanceGUID)
                if (!m_SelectionPools[j].AddGroup(*(itr_team[j]), m_SelectionPools[(j + 1) % PVP_TEAMS_COUNT].GetPlayerCount()))
                    break;
        }
        // do not allow to start bg with more than 2 players more on 1 faction
        if (abs((int32)(m_SelectionPools[TEAM_HORDE].GetPlayerCount() - m_SelectionPools[TEAM_ALLIANCE].GetPlayerCount())) > 2)
            return false;
    }
    //allow 1v0 if debug bg
    if (sBattlegroundMgr->isTesting() && (m_SelectionPools[TEAM_ALLIANCE].GetPlayerCount() || m_SelectionPools[TEAM_HORDE].GetPlayerCount()))
        return true;
    //return true if there are enough players in selection pools - enable to work .debug bg command correctly
    return m_SelectionPools[TEAM_ALLIANCE].GetPlayerCount() >= minPlayers && m_SelectionPools[TEAM_HORDE].GetPlayerCount() >= minPlayers;
}

// this method will check if we can invite players to same faction skirmish match
bool BattlegroundQueue::CheckSkirmishForSameFaction(BattlegroundBracketId bracket_id, uint32 minPlayersPerTeam)
{
    if (m_SelectionPools[TEAM_ALLIANCE].GetPlayerCount() < minPlayersPerTeam && m_SelectionPools[TEAM_HORDE].GetPlayerCount() < minPlayersPerTeam)
        return false;
    TeamId teamIndex = TEAM_ALLIANCE;
    TeamId otherTeam = TEAM_HORDE;
    Team otherTeamId = HORDE;
    if (m_SelectionPools[TEAM_HORDE].GetPlayerCount() == minPlayersPerTeam)
    {
        teamIndex = TEAM_HORDE;
        otherTeam = TEAM_ALLIANCE;
        otherTeamId = ALLIANCE;
    }
    //clear other team's selection
    m_SelectionPools[otherTeam].Init();
    //store last ginfo pointer
    GroupQueueInfo* ginfo = m_SelectionPools[teamIndex].SelectedGroups.back();
    //set itr_team to group that was added to selection pool latest
    GroupsQueueType::iterator itr_team = m_QueuedGroups[bracket_id][uint8(BG_QUEUE_NORMAL_ALLIANCE) + uint8(teamIndex)].begin();
    for (; itr_team != m_QueuedGroups[bracket_id][uint8(BG_QUEUE_NORMAL_ALLIANCE) + uint8(teamIndex)].end(); ++itr_team)
        if (ginfo == *itr_team)
            break;
    if (itr_team == m_QueuedGroups[bracket_id][uint8(BG_QUEUE_NORMAL_ALLIANCE) + uint8(teamIndex)].end())
        return false;
    GroupsQueueType::iterator itr_team2 = itr_team;
    ++itr_team2;
    //invite players to other selection pool
    for (; itr_team2 != m_QueuedGroups[bracket_id][uint8(BG_QUEUE_NORMAL_ALLIANCE) + uint8(teamIndex)].end(); ++itr_team2)
    {
        //if selection pool is full then break;
        if (!(*itr_team2)->IsInvitedToBGInstanceGUID && !m_SelectionPools[otherTeam].AddGroup(*itr_team2, minPlayersPerTeam))
            break;
    }
    if (m_SelectionPools[otherTeam].GetPlayerCount() != minPlayersPerTeam)
        return false;

    //here we have correct 2 selections and we need to change one teams team and move selection pool teams to other team's queue
    for (GroupsQueueType::iterator itr = m_SelectionPools[otherTeam].SelectedGroups.begin(); itr != m_SelectionPools[otherTeam].SelectedGroups.end(); ++itr)
    {
        //set correct team
        (*itr)->Team = otherTeamId;
        //add team to other queue
        m_QueuedGroups[bracket_id][uint8(BG_QUEUE_NORMAL_ALLIANCE) + uint8(otherTeam)].push_front(*itr);
        //remove team from old queue
        GroupsQueueType::iterator itr2 = itr_team;
        ++itr2;
        for (; itr2 != m_QueuedGroups[bracket_id][uint8(BG_QUEUE_NORMAL_ALLIANCE) + uint8(teamIndex)].end(); ++itr2)
        {
            if (*itr2 == *itr)
            {
                m_QueuedGroups[bracket_id][uint8(BG_QUEUE_NORMAL_ALLIANCE) + uint8(teamIndex)].erase(itr2);
                break;
            }
        }
    }
    return true;
}

// Solo-queue matchmaker for Battleground Blitz (and any future rated solo-queue mode).
//
// Differs from CheckNormalMatch/CheckPremadeMatch in two ways:
//  - it draws from the RATED lists. AddGroup only pushes an entry into BG_QUEUE_NORMAL_* when the queue is
//    unrated (`if (!m_queueId.Rated && !isPremade) index += PVP_TEAMS_COUNT`), so for a rated queue every
//    entry - solo or duo - lives in BG_QUEUE_PREMADE_ALLIANCE / _HORDE.
//  - it is faction-blind. Blitz matches cross-faction, so entries are dealt into whichever pool needs them
//    and their Team is rewritten, exactly as CheckSkirmishForSameFaction does for same-faction skirmishes.
//
// tanksPerTeam and healersPerTeam are configured quotas; damagers are the remainder of playersPerTeam, so a
// team composition is fully specified by those two numbers. They are not client-derived: the wire says what
// a queuer is willing to play, not how retail balances a Blitz match. Setting both to 0 turns the role check
// off and leaves a pure headcount match. The defaults (0 tanks, 2 healers, 8 per side) reproduce the
// 0 tanks / 4 healers / 12 damagers composition the client was told in C:\sniff\rated BG 12.0.7.pkt.
//
// Roles come from PlayerQueueInfo::Role, so they are per player, not per queue entry - a duo whose members
// have different specs is counted as the two roles it really contains.
bool BattlegroundQueue::CheckSoloQueueMatch(BattlegroundBracketId bracket_id, uint32 playersPerTeam, uint32 tanksPerTeam, uint32 healersPerTeam)
{
    if (tanksPerTeam + healersPerTeam > playersPerTeam)
    {
        TC_LOG_ERROR("bg.battleground", "BattlegroundQueue: solo queue {} is configured with {} tanks + {} healers per team but only {} slots; refusing to match.",
            m_queueId.BattlemasterListId, tanksPerTeam, healersPerTeam, playersPerTeam);
        return false;
    }

    uint8 const ratedLists[PVP_TEAMS_COUNT] = { uint8(BG_QUEUE_PREMADE_ALLIANCE), uint8(BG_QUEUE_PREMADE_HORDE) };

    PvpRoleHeadcount const perSideQuota = SideQuota(playersPerTeam, tanksPerTeam, healersPerTeam);

    std::vector<GroupQueueInfo*> candidates;
    uint32 totalPlayers = 0;

    for (uint8 list : ratedLists)
    {
        for (GroupQueueInfo* ginfo : m_QueuedGroups[bracket_id][list])
        {
            if (ginfo->IsInvitedToBGInstanceGUID)
                continue;

            totalPlayers += uint32(ginfo->Players.size());
            candidates.push_back(ginfo);
        }
    }

    if (totalPlayers < playersPerTeam * PVP_TEAMS_COUNT)
        return false;

    // Longest waiting first, so the role quota never becomes a reason to jump the queue.
    std::stable_sort(candidates.begin(), candidates.end(), [](GroupQueueInfo const* left, GroupQueueInfo const* right)
    {
        return left->JoinTime < right->JoinTime;
    });

    for (uint32 i = 0; i < PVP_TEAMS_COUNT; ++i)
        m_SelectionPools[i].Init();

    // Remaining need of each side, drained as entries are placed.
    std::array<PvpRoleHeadcount, PVP_TEAMS_COUNT> need;
    need.fill(perSideQuota);

    for (GroupQueueInfo* ginfo : candidates)
    {
        if (m_SelectionPools[TEAM_ALLIANCE].GetPlayerCount() >= playersPerTeam
            && m_SelectionPools[TEAM_HORDE].GetPlayerCount() >= playersPerTeam)
            break;

        // What this entry brings, per role. A duo of a healer and a damager counts as one of each.
        PvpRoleHeadcount entryRoles = { };
        for (std::pair<ObjectGuid const, PlayerQueueInfo*> const& player : ginfo->Players)
            ++entryRoles[RoleIndex(FoldRole(player.second->Role, perSideQuota))];

        for (uint32 side = 0; side < PVP_TEAMS_COUNT; ++side)
        {
            // SelectionPool::AddGroup answers "may I keep filling", not "did I take it", so the fit has to be
            // decided here - otherwise the role budget below would be spent on an entry that was never added.
            if (m_SelectionPools[side].GetPlayerCount() + ginfo->Players.size() > playersPerTeam)
                continue;

            bool fits = true;
            for (std::size_t role = 0; role < PVP_QUEUE_ROLE_COUNT && fits; ++role)
                fits = entryRoles[role] <= need[side][role];

            if (!fits)
                continue;

            m_SelectionPools[side].AddGroup(ginfo, playersPerTeam);

            for (std::size_t role = 0; role < PVP_QUEUE_ROLE_COUNT; ++role)
                need[side][role] -= entryRoles[role];
            break;
        }
    }

    if (m_SelectionPools[TEAM_ALLIANCE].GetPlayerCount() != playersPerTeam
        || m_SelectionPools[TEAM_HORDE].GetPlayerCount() != playersPerTeam)
    {
        // Leave no half-built selection behind for the next caller.
        for (uint32 i = 0; i < PVP_TEAMS_COUNT; ++i)
            m_SelectionPools[i].Init();
        return false;
    }

    // Both pools are full. Re-home every entry that ended up on the other faction's side: set its Team, push it
    // into the destination rated list and erase it from the source list. Skipping the list move makes
    // RemovePlayer fail to find the entry and leaks queue slots - the same idiom as CheckSkirmishForSameFaction.
    for (uint32 i = 0; i < PVP_TEAMS_COUNT; ++i)
    {
        TeamId const poolTeam = TeamId(TEAM_ALLIANCE + i);
        Team const poolTeamId = poolTeam == TEAM_ALLIANCE ? ALLIANCE : HORDE;

        for (GroupQueueInfo* ginfo : m_SelectionPools[poolTeam].SelectedGroups)
        {
            if (ginfo->Team == poolTeamId)
                continue;

            uint8 const sourceList = ginfo->Team == ALLIANCE ? uint8(BG_QUEUE_PREMADE_ALLIANCE) : uint8(BG_QUEUE_PREMADE_HORDE);
            uint8 const destList   = poolTeam == TEAM_ALLIANCE ? uint8(BG_QUEUE_PREMADE_ALLIANCE) : uint8(BG_QUEUE_PREMADE_HORDE);

            ginfo->Team = poolTeamId;
            m_QueuedGroups[bracket_id][destList].push_front(ginfo);

            for (GroupsQueueType::iterator itr = m_QueuedGroups[bracket_id][sourceList].begin();
                 itr != m_QueuedGroups[bracket_id][sourceList].end(); ++itr)
            {
                if (*itr == ginfo)
                {
                    m_QueuedGroups[bracket_id][sourceList].erase(itr);
                    break;
                }
            }
        }
    }

    return true;
}

/*********************************************************/
/***          ALL-OR-NOTHING GROUP PROPOSALS           ***/
/*********************************************************/

BattlegroundProposal* BattlegroundQueue::FindProposalFor(ObjectGuid guid)
{
    return const_cast<BattlegroundProposal*>(std::as_const(*this).FindProposalFor(guid));
}

BattlegroundProposal const* BattlegroundQueue::FindProposalFor(ObjectGuid guid) const
{
    for (std::pair<uint32 const, BattlegroundProposal> const& pair : m_Proposals)
        for (BattlegroundProposalMember const& member : pair.second.Members)
            if (member.Guid == guid)
                return &pair.second;

    return nullptr;
}

bool BattlegroundQueue::IsInProposal(ObjectGuid guid) const
{
    return FindProposalFor(guid) != nullptr;
}

void BattlegroundQueue::StartProposal(Battleground* bg, BattlegroundBracketId bracketId, uint32 playersPerTeam, PvpRoleHeadcount const& perSideQuota)
{
    BattlegroundProposal proposal;
    proposal.BgInstanceGUID = bg->GetInstanceID();
    proposal.BgTypeId = bg->GetTypeID();
    proposal.BracketId = bracketId;
    proposal.MapId = bg->GetMapId();
    proposal.SlotsPerSide.fill(uint8(playersPerTeam));

    for (uint32 side = 0; side < PVP_TEAMS_COUNT; ++side)
    {
        for (GroupQueueInfo const* ginfo : m_SelectionPools[side].SelectedGroups)
        {
            for (std::pair<ObjectGuid const, PlayerQueueInfo*> const& player : ginfo->Players)
            {
                // Mirrors InviteGroupToBG, which only invites and only counts players it finds online.
                if (!ObjectAccessor::FindConnectedPlayer(player.first))
                    continue;

                BattlegroundProposalMember& member = proposal.Members.emplace_back();
                member.Guid = player.first;
                member.Side = TeamId(side);
                // Folded exactly as CheckSoloQueueMatch folded it, so the role block the client is sent
                // describes the same lobby the matcher built.
                member.Role = FoldRole(player.second->Role, perSideQuota);
            }
        }
    }

    if (proposal.Members.empty())
        return;

    uint32 const instanceId = proposal.BgInstanceGUID;
    BattlegroundProposal const& stored = m_Proposals.insert_or_assign(instanceId, std::move(proposal)).first->second;

    m_events.AddEvent(new BGQueueProposalTimeoutEvent(instanceId, m_queueId), m_events.CalculateTime(Milliseconds(PROPOSAL_ACCEPT_WAIT_TIME)));

    TC_LOG_DEBUG("bg.battleground", "Battleground: opened a {} ms group proposal over BG instance {} (bracket {}, map {}) for {} players.",
        uint32(PROPOSAL_ACCEPT_WAIT_TIME), instanceId, uint32(bracketId), stored.MapId, uint32(stored.Members.size()));

    SendProposalStatus(stored, BattlegroundProposalStatus::Waiting);
}

// Tells every member where the proposal stands.
//   Waiting - SMSG_BATTLEFIELD_STATUS_WAIT_FOR_GROUPS, accepted players as Secured, undecided ones as Awaited.
//   Failed  - SMSG_BATTLEFIELD_STATUS_GROUP_PROPOSAL_FAILED, which keeps Secured and moves the outstanding
//             players into Lost. That is the exact shape of both captured 0x48000E bodies.
//   Formed  - SMSG_BATTLEFIELD_STATUS_WAIT_FOR_GROUPS with every counter zeroed, matching the terminal
//             0x48000D body the capture ends on once the match exists.
void BattlegroundQueue::SendProposalStatus(BattlegroundProposal const& proposal, BattlegroundProposalStatus status) const
{
    WorldPackets::Battleground::PvpRoleQueueCounts counts;
    std::array<uint8, PVP_TEAMS_COUNT> awaitedPerSide = { };

    if (status != BattlegroundProposalStatus::Formed)
    {
        for (BattlegroundProposalMember const& member : proposal.Members)
        {
            std::size_t const role = RoleIndex(member.Role);
            if (member.Accepted)
            {
                ++counts.Secured[role];
                continue;
            }

            // Indexed by TeamId, the same order the selection pools above use. That choice is free rather
            // than guessed: the client attaches no faction to either index of this pair - its only reader is
            // GetWarGameQueueStatus, which hands both back as bare numbers - so nothing on the wire can tell
            // the two apart. The census is in the decode block above BattlefieldStatusWaitForGroups. Note
            // that this is NOT the convention of the sister field BattlegroundPoints.Team, which is a real
            // PvPTeamId because there the client indexes a faction-labelled array with it.
            ++awaitedPerSide[member.Side];
            if (status == BattlegroundProposalStatus::Failed)
                ++counts.Lost[role];
            else
                ++counts.Awaited[role];
        }
    }

    for (BattlegroundProposalMember const& member : proposal.Members)
    {
        // Counted above, but not told: the decliner is answered by their own leave-queue path with
        // SMSG_BATTLEFIELD_STATUS_NONE, which is the last word either way.
        if (member.Declined)
            continue;

        Player* player = ObjectAccessor::FindConnectedPlayer(member.Guid);
        if (!player)
            continue;

        uint32 const queueSlot = player->GetBattlegroundQueueIndex(m_queueId);
        if (queueSlot >= PLAYER_MAX_BATTLEGROUND_QUEUES)
            continue;

        uint32 const joinTime = player->GetBattlegroundQueueJoinTime(m_queueId);

        if (status == BattlegroundProposalStatus::Failed)
        {
            WorldPackets::Battleground::BattlefieldStatusGroupProposalFailed packet;
            BattlegroundMgr::BuildBattlegroundStatusGroupProposalFailed(&packet, player, queueSlot, joinTime, m_queueId, counts);
            player->SendDirectMessage(packet.Write());
        }
        else
        {
            WorldPackets::Battleground::BattlefieldStatusWaitForGroups packet;
            BattlegroundMgr::BuildBattlegroundStatusWaitForGroups(&packet, player, queueSlot, joinTime, m_queueId,
                proposal.MapId, PROPOSAL_ACCEPT_WAIT_TIME, proposal.SlotsPerSide, awaitedPerSide, counts);
            player->SendDirectMessage(packet.Write());
        }
    }
}

bool BattlegroundQueue::ProposalAccept(ObjectGuid guid)
{
    BattlegroundProposal* proposal = FindProposalFor(guid);
    if (!proposal)
        return false;

    bool everyoneAccepted = true;
    for (BattlegroundProposalMember& member : proposal->Members)
    {
        if (member.Guid == guid)
            member.Accepted = true;

        if (!member.Accepted)
            everyoneAccepted = false;
    }

    if (!everyoneAccepted)
    {
        // Held. The client is told who is still missing rather than being left on a dialog that did nothing.
        SendProposalStatus(*proposal, BattlegroundProposalStatus::Waiting);
        return true;
    }

    ResolveProposal(proposal->BgInstanceGUID, true);
    return true;
}

bool BattlegroundQueue::ProposalDecline(ObjectGuid guid)
{
    BattlegroundProposal* proposal = FindProposalFor(guid);
    if (!proposal)
        return false;

    TC_LOG_DEBUG("bg.battleground", "Battleground: {} declined the group proposal over BG instance {}; collapsing it.",
        guid.ToString(), proposal->BgInstanceGUID);

    // The decliner is marked, NOT removed. Removing them made the role block describe 15 of 16 players, and the
    // client adds Awaited+Secured+Lost per role to get the lobby size: 20 decoded bodies have that sum constant
    // at the team size, so a short sum is a wrong packet. They are counted as a loss (Accepted is cleared) and
    // skipped by the collapse below, because their own removal from the queue is the caller's ordinary
    // leave-queue path and must not happen twice.
    uint32 const instanceId = proposal->BgInstanceGUID;
    for (BattlegroundProposalMember& member : proposal->Members)
    {
        if (member.Guid != guid)
            continue;

        member.Accepted = false;
        member.Declined = true;
    }
    // The decliner is dropped from the proposal first: their own removal from the queue is the caller's
    // ordinary leave-queue path, and the collapse below must not touch them twice.
    uint32 const instanceId = proposal->BgInstanceGUID;
    std::erase_if(proposal->Members, [guid](BattlegroundProposalMember const& member) { return member.Guid == guid; });

    ResolveProposal(instanceId, false);
    return true;
}

void BattlegroundQueue::ProposalTimeout(uint32 bgInstanceGuid)
{
    if (m_Proposals.find(bgInstanceGuid) == m_Proposals.end())
        return;                                             // already resolved - completed, or collapsed by a decline

    TC_LOG_DEBUG("bg.battleground", "Battleground: group proposal over BG instance {} expired; collapsing it.", bgInstanceGuid);
    ResolveProposal(bgInstanceGuid, false);
}

void BattlegroundQueue::ResolveProposal(uint32 bgInstanceGuid, bool accepted)
{
    std::unordered_map<uint32, BattlegroundProposal>::iterator itr = m_Proposals.find(bgInstanceGuid);
    if (itr == m_Proposals.end())
        return;

    // Take the proposal out of the map before doing anything that can re-enter this queue: porting a player
    // calls RemovePlayer, which can free the GroupQueueInfo the pools point at.
    BattlegroundProposal proposal = std::move(itr->second);
    m_Proposals.erase(itr);

    Battleground* bg = sBattlegroundMgr->GetBattleground(proposal.BgInstanceGUID, proposal.BgTypeId);

    if (accepted)
    {
        if (!bg)
        {
            TC_LOG_ERROR("bg.battleground", "BattlegroundQueue: group proposal over BG instance {} completed but the battleground is gone.", proposal.BgInstanceGUID);
            return;
        }

        // The client clears its role display when the match forms - that is what the terminal 0x48000D body
        // in the capture is - so say so before the ports start rewriting queue state.
        SendProposalStatus(proposal, BattlegroundProposalStatus::Formed);

        for (BattlegroundProposalMember const& member : proposal.Members)
        {
            Player* player = ObjectAccessor::FindConnectedPlayer(member.Guid);
            if (!player)
                continue;                                   // logged out between accepting and the last accept

            uint32 const queueSlot = player->GetBattlegroundQueueIndex(m_queueId);
            if (queueSlot >= PLAYER_MAX_BATTLEGROUND_QUEUES)
                continue;

            Team const team = member.Side == TEAM_ALLIANCE ? ALLIANCE : HORDE;
            BattlegroundMgr::PortPlayerToBattleground(player, bg, team, m_queueId, queueSlot);
        }

        TC_LOG_DEBUG("bg.battleground", "Battleground: group proposal over BG instance {} was accepted by all {} members.",
            proposal.BgInstanceGUID, uint32(proposal.Members.size()));
        return;
    }

    // Collapse. Everyone still listed loses the invite; whether they keep their queue slot depends on whether
    // they accepted - and, for a duo, on how the other half answered.
    SendProposalStatus(proposal, BattlegroundProposalStatus::Failed);

    // Blitz may be entered as a duo (BattlemasterList 1101, MaxGroupSize 2), and below the proposal a queue
    // ENTRY is atomic: RemovePlayer drops every remaining member of a GroupQueueInfo the moment one of them
    // leaves a rated queue - the recursive branch above, "everyone from the group should leave too". A duo
    // whose halves answered differently can therefore not be split: the accepting half leaves with its
    // partner, and the invariant documented on BattlegroundProposal holds per queue ENTRY, not per player.
    // Which entries are about to break is settled here, before the first removal, instead of being left to
    // fall out of that cascade. Otherwise the accepting member is told BATTLEFIELD_STATUS_QUEUED and then, in
    // the same tick, BATTLEFIELD_STATUS_NONE by the cascade - and which of the two it ends up with depends on
    // the order the two members happen to have in proposal.Members. A member who declined breaks the entry
    // too: its own removal only happens after this function returns, in HandleBattleFieldPortOpcode.
    std::vector<GroupQueueInfo const*> brokenEntries;
    for (BattlegroundProposalMember const& member : proposal.Members)
        if (!member.Accepted)
            if (PlayerQueueInfo const* playerInfo = Trinity::Containers::MapGetValuePtr(m_QueuedPlayers, member.Guid))
                brokenEntries.push_back(playerInfo->GroupInfo);

    auto entryIsBroken = [&brokenEntries](GroupQueueInfo const* ginfo)
    {
        return std::find(brokenEntries.begin(), brokenEntries.end(), ginfo) != brokenEntries.end();
    };

    // The invite is revoked for everyone first, and only then is anyone removed: the first removal can delete
    // a GroupQueueInfo that the members listed after it still point at.
    for (BattlegroundProposalMember const& member : proposal.Members)
    {
        // The decliner is already being taken out of the queue by WorldSession::HandleBattleFieldPortOpcode,
        // whose RemovePlayer(guid, true) does this same invited-count bookkeeping. Doing it here as well would
        // decrement it twice.
        if (member.Declined)
            continue;

    // they accepted.
    SendProposalStatus(proposal, BattlegroundProposalStatus::Failed);

    for (BattlegroundProposalMember const& member : proposal.Members)
    {
        // The invited count was raised once per invited PLAYER, so it has to come back down once per member,
        // while the group's invite flag is a per-entry thing that is only cleared the first time.
        if (PlayerQueueInfo const* playerInfo = Trinity::Containers::MapGetValuePtr(m_QueuedPlayers, member.Guid))
        {
            if (GroupQueueInfo* ginfo = playerInfo->GroupInfo)
            {
                if (bg)
                    bg->DecreaseInvitedCount(ginfo->Team);

                if (ginfo->IsInvitedToBGInstanceGUID == proposal.BgInstanceGUID)
                {
                    ginfo->IsInvitedToBGInstanceGUID = 0;
                    ginfo->RemoveInviteTime = 0;
                }
            }
        }

        if (Player* player = ObjectAccessor::FindConnectedPlayer(member.Guid))
            player->SetInviteForBattlegroundQueueType(m_queueId, 0);
    }

    for (BattlegroundProposalMember const& member : proposal.Members)
    {
        if (member.Declined)
            continue;

        Player* player = ObjectAccessor::FindConnectedPlayer(member.Guid);
        if (!player)
            continue;

        // Removing one half of a duo takes the other half with it, so a member can already be out of the queue
        // when its own turn comes. That cascade sent it BATTLEFIELD_STATUS_NONE and there is nothing to add.
        PlayerQueueInfo const* playerInfo = Trinity::Containers::MapGetValuePtr(m_QueuedPlayers, member.Guid);
        if (!playerInfo)
            continue;
        player->SetInviteForBattlegroundQueueType(m_queueId, 0);

        uint32 const queueSlot = player->GetBattlegroundQueueIndex(m_queueId);
        if (queueSlot >= PLAYER_MAX_BATTLEGROUND_QUEUES)
            continue;

        uint32 const joinTime = player->GetBattlegroundQueueJoinTime(m_queueId);

        if (!member.Accepted || entryIsBroken(playerInfo->GroupInfo))
        {
            // Never answered, answered no, or accepted while sharing a queue entry with someone who did not.
            // Same outcome a plain invite timeout has: out of the queue. Where this removal cascades onto a
            // partner who declined, that player sees the cascade BATTLEFIELD_STATUS_NONE shortly before the
            // one its own session sends - the same ticket in the same state twice, a repeat rather than the
            // QUEUED-then-NONE contradiction this ordering exists to prevent.
        if (!member.Accepted)
        {
            // Never answered, or answered no. Same outcome a plain invite timeout has: out of the queue.
            player->RemoveBattlegroundQueueId(m_queueId);
            RemovePlayer(member.Guid, false);

            WorldPackets::Battleground::BattlefieldStatusNone packet;
            BattlegroundMgr::BuildBattlegroundStatusNone(&packet, player, queueSlot, joinTime);
            player->SendDirectMessage(packet.Write());
            continue;
        }

        // Accepted, and is being sent back through no fault of their own. Their GroupQueueInfo is left alone,
        // so JoinTime - and with it the position CheckSoloQueueMatch orders by - is exactly what it was.
        GroupQueueInfo groupInfo;
        bool const hasGroupInfo = GetPlayerGroupInfoData(member.Guid, &groupInfo);
        uint32 const avgWaitTime = hasGroupInfo ? GetAverageQueueWaitTime(&groupInfo, proposal.BracketId) : 0;

        // AsGroup is derived, not assumed: a Blitz duo (BattlemasterList 1101, MaxGroupSize 2) that both
        // accepted while a third member let the window lapse re-enters here with its queue entry intact, and
        // the very next CMSG_REQUEST_BATTLEFIELD_STATUS would answer true from the same Players.size() - so a
        // hardcoded false made the bit read true, false, true on one ticket within seconds.
        WorldPackets::Battleground::BattlefieldStatusQueued packet;
        BattlegroundMgr::BuildBattlegroundStatusQueued(&packet, player, queueSlot, joinTime, m_queueId, avgWaitTime,
            hasGroupInfo && groupInfo.Players.size() > 1);
        uint32 const avgWaitTime = GetPlayerGroupInfoData(member.Guid, &groupInfo)
            ? GetAverageQueueWaitTime(&groupInfo, proposal.BracketId) : 0;

        WorldPackets::Battleground::BattlefieldStatusQueued packet;
        BattlegroundMgr::BuildBattlegroundStatusQueued(&packet, player, queueSlot, joinTime, m_queueId, avgWaitTime, false);
        player->SendDirectMessage(packet.Write());
    }

    // The battleground was created for this proposal only. Drop it, so the next attempt picks a fresh map -
    // which is what retail does across the three proposal runs in the capture.
    if (bg)
    {
        bg->RemoveFromBGFreeSlotQueue();
        bg->SetDeleteThis();
    }

    sBattlegroundMgr->ScheduleQueueUpdate(0, m_queueId, proposal.BracketId);
}

void BattlegroundQueue::UpdateEvents(uint32 diff)
{
    m_events.Update(diff);
}

/*
this method is called when group is inserted, or player / group is removed from BG Queue - there is only one player's status changed, so we don't use while (true) cycles to invite whole queue
it must be called after fully adding the members of a group to ensure group joining
should be called from Battleground::RemovePlayer function in some cases
*/
void BattlegroundQueue::BattlegroundQueueUpdate(uint32 /*diff*/, BattlegroundBracketId bracket_id, uint32 arenaRating)
{
    //if no players in queue - do nothing
    if (m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE].empty() &&
        m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_HORDE].empty() &&
        m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE].empty() &&
        m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_HORDE].empty())
        return;

    // battleground with free slot for player should be always in the beggining of the queue
    // maybe it would be better to create bgfreeslotqueue for each bracket_id

    BattlegroundTemplate const* bg_template = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BattlegroundTypeId(m_queueId.BattlemasterListId));
    if (!bg_template)
    {
        TC_LOG_ERROR("bg.battleground", "Battleground: Update: bg template not found for {}", m_queueId.BattlemasterListId);
        return;
    }

    // loop over queues for every map
    for (int32 mapId : bg_template->MapIDs)
    {
        BGFreeSlotQueueContainer& bgQueues = sBattlegroundMgr->GetBGFreeSlotQueueStore(mapId);
        for (BGFreeSlotQueueContainer::iterator itr = bgQueues.begin(); itr != bgQueues.end();)
        {
            Battleground* bg = *itr; ++itr;
            // DO NOT allow queue manager to invite new player to rated games
            if (!bg->isRated() && bg->GetBracketId() == bracket_id &&
                bg->GetStatus() > STATUS_WAIT_QUEUE && bg->GetStatus() < STATUS_WAIT_LEAVE)
            {
                // clear selection pools
                m_SelectionPools[TEAM_ALLIANCE].Init();
                m_SelectionPools[TEAM_HORDE].Init();

                // call a function that does the job for us
                FillPlayersToBG(bg, bracket_id);

                // now everything is set, invite players
                for (GroupsQueueType::const_iterator citr = m_SelectionPools[TEAM_ALLIANCE].SelectedGroups.begin(); citr != m_SelectionPools[TEAM_ALLIANCE].SelectedGroups.end(); ++citr)
                    InviteGroupToBG((*citr), bg, (*citr)->Team);

                for (GroupsQueueType::const_iterator citr = m_SelectionPools[TEAM_HORDE].SelectedGroups.begin(); citr != m_SelectionPools[TEAM_HORDE].SelectedGroups.end(); ++citr)
                    InviteGroupToBG((*citr), bg, (*citr)->Team);

                if (!bg->HasFreeSlots())
                    bg->RemoveFromBGFreeSlotQueue();
            }
        }
    }

    // finished iterating through the bgs with free slots, maybe we need to create a new bg

    // get the min. players per team, properly for larger arenas as well. (must have full teams for arena matches!)
    uint32 MinPlayersPerTeam = bg_template->GetMinPlayersPerTeam();
    uint32 MaxPlayersPerTeam = bg_template->GetMaxPlayersPerTeam();

    if (bg_template->IsArena())
    {
        MaxPlayersPerTeam = m_queueId.TeamSize;
        MinPlayersPerTeam = sBattlegroundMgr->isArenaTesting() ? 1 : m_queueId.TeamSize;
    }
    else if (sBattlegroundMgr->isTesting())
        MinPlayersPerTeam = 1;

    m_SelectionPools[TEAM_ALLIANCE].Init();
    m_SelectionPools[TEAM_HORDE].Init();

    // A solo queue must NOT be offered to CheckPremadeMatch, even though its template is a battleground and
    // not an arena. CheckPremadeMatch takes one head entry per faction, adds it whole with
    // AddGroup(entry, MaxPlayersPerTeam) and then tops both sides up from the BG_QUEUE_NORMAL_* lists - which
    // for a rated queue are permanently empty, because AddGroup only routes an entry there when
    // `!m_queueId.Rated && !isPremade`. So with one solo entry queued per faction it would report a match of
    // one against one, create the instance and StartBattleground() it: a 1v1 "Battleground Blitz" past both
    // the role quota and the 8-per-side headcount, invited through the ordinary 90 s single-invite path
    // instead of the all-or-nothing proposal. Its second half is worse still - the demotion loop below moves
    // any entry smaller than MinPlayersPerTeam (6 for BattlemasterList 1101) out of the premade lists, and
    // those lists are the only ones CheckSoloQueueMatch reads, so a single pass would empty the solo queue
    // into a list nothing rated ever looks at again.
    // The RatedBattlegroundBlitz branch at the end of this function is this queue's matchmaker.
    bool const isSoloQueue = BattlegroundQueueIdType(m_queueId.Type) == BattlegroundQueueIdType::RatedBattlegroundBlitz;

    if (!bg_template->IsArena() && !isSoloQueue)
    {
        if (CheckPremadeMatch(bracket_id, MinPlayersPerTeam, MaxPlayersPerTeam))
        {
            // create new battleground
            Battleground* bg2 = sBattlegroundMgr->CreateNewBattleground(m_queueId, bracket_id);
            if (!bg2)
            {
                TC_LOG_ERROR("bg.battleground", "BattlegroundQueue::Update - Cannot create battleground: {}", m_queueId.BattlemasterListId);
                return;
            }
            // invite those selection pools
            for (uint32 i = 0; i < PVP_TEAMS_COUNT; i++)
                for (GroupsQueueType::const_iterator citr = m_SelectionPools[TEAM_ALLIANCE + i].SelectedGroups.begin(); citr != m_SelectionPools[TEAM_ALLIANCE + i].SelectedGroups.end(); ++citr)
                    InviteGroupToBG((*citr), bg2, (*citr)->Team);

            bg2->StartBattleground();
            //clear structures
            m_SelectionPools[TEAM_ALLIANCE].Init();
            m_SelectionPools[TEAM_HORDE].Init();
        }
    }

    // now check if there are in queues enough players to start new game of (normal battleground, or non-rated arena)
    if (!m_queueId.Rated)
    {
        // if there are enough players in pools, start new battleground or non rated arena
        if (CheckNormalMatch(bracket_id, MinPlayersPerTeam, MaxPlayersPerTeam)
            || (bg_template->IsArena() && CheckSkirmishForSameFaction(bracket_id, MinPlayersPerTeam)))
        {
            // we successfully created a pool
            Battleground* bg2 = sBattlegroundMgr->CreateNewBattleground(m_queueId, bracket_id);
            if (!bg2)
            {
                TC_LOG_ERROR("bg.battleground", "BattlegroundQueue::Update - Cannot create battleground: {}", m_queueId.BattlemasterListId);
                return;
            }

            // invite those selection pools
            for (uint32 i = 0; i < PVP_TEAMS_COUNT; i++)
                for (GroupsQueueType::const_iterator citr = m_SelectionPools[TEAM_ALLIANCE + i].SelectedGroups.begin(); citr != m_SelectionPools[TEAM_ALLIANCE + i].SelectedGroups.end(); ++citr)
                    InviteGroupToBG((*citr), bg2, (*citr)->Team);
            // start bg
            bg2->StartBattleground();
        }
    }
    else if (bg_template->IsArena())
    {
        // found out the minimum and maximum ratings the newly added team should battle against
        // arenaRating is the rating of the latest joined team, or 0
        // 0 is on (automatic update call) and we must set it to team's with longest wait time
        if (!arenaRating)
        {
            GroupQueueInfo* front1 = nullptr;
            GroupQueueInfo* front2 = nullptr;
            if (!m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE].empty())
            {
                front1 = m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE].front();
                arenaRating = front1->ArenaMatchmakerRating;
            }
            if (!m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_HORDE].empty())
            {
                front2 = m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_HORDE].front();
                arenaRating = front2->ArenaMatchmakerRating;
            }
            if (front1 && front2)
            {
                if (front1->JoinTime < front2->JoinTime)
                    arenaRating = front1->ArenaMatchmakerRating;
            }
            else if (!front1 && !front2)
                return; //queues are empty
        }

        //set rating range
        uint32 arenaMinRating = (arenaRating <= sBattlegroundMgr->GetMaxRatingDifference()) ? 0 : arenaRating - sBattlegroundMgr->GetMaxRatingDifference();
        uint32 arenaMaxRating = arenaRating + sBattlegroundMgr->GetMaxRatingDifference();
        // if max rating difference is set and the time past since server startup is greater than the rating discard time
        // (after what time the ratings aren't taken into account when making teams) then
        // the discard time is current_time - time_to_discard, teams that joined after that, will have their ratings taken into account
        // else leave the discard time on 0, this way all ratings will be discarded
        // this has to be signed value - when the server starts, this value would be negative and thus overflow
        int32 discardTime = GameTime::GetGameTimeMS() - sBattlegroundMgr->GetRatingDiscardTimer();

        // we need to find 2 teams which will play next game
        GroupsQueueType::iterator itr_teams[PVP_TEAMS_COUNT];
        uint8 found = 0;
        uint8 team = 0;

        for (uint8 i = BG_QUEUE_PREMADE_ALLIANCE; i < BG_QUEUE_NORMAL_ALLIANCE; i++)
        {
            // take the group that joined first
            GroupsQueueType::iterator itr2 = m_QueuedGroups[bracket_id][i].begin();
            for (; itr2 != m_QueuedGroups[bracket_id][i].end(); ++itr2)
            {
                // if group match conditions, then add it to pool
                if (!(*itr2)->IsInvitedToBGInstanceGUID
                    && (((*itr2)->ArenaMatchmakerRating >= arenaMinRating && (*itr2)->ArenaMatchmakerRating <= arenaMaxRating)
                        || (int32)(*itr2)->JoinTime < discardTime))
                {
                    itr_teams[found++] = itr2;
                    team = i;
                    break;
                }
            }
        }

        if (!found)
            return;

        if (found == 1)
        {
            for (GroupsQueueType::iterator itr3 = itr_teams[0]; itr3 != m_QueuedGroups[bracket_id][team].end(); ++itr3)
            {

                // disable this check to allow arena queue without other team
                if (itr3 == itr_teams[0])
                    continue; // skip already found team

                if (!(*itr3)->IsInvitedToBGInstanceGUID
                    && (((*itr3)->ArenaMatchmakerRating >= arenaMinRating && (*itr3)->ArenaMatchmakerRating <= arenaMaxRating)
                        || (int32)(*itr3)->JoinTime < discardTime))
                {
                    itr_teams[found++] = itr3;
                    break;
                }
            }
        }

        //if we have 2 teams, then start new arena and invite players!
        if (found == 2)
        {
            GroupQueueInfo* aTeam = *itr_teams[TEAM_ALLIANCE];
            GroupQueueInfo* hTeam = *itr_teams[TEAM_HORDE];
            Battleground* arena = sBattlegroundMgr->CreateNewBattleground(m_queueId, bracket_id);
            if (!arena)
            {
                TC_LOG_ERROR("bg.battleground", "BattlegroundQueue::Update couldn't create arena instance for rated arena match!");
                return;
            }

            aTeam->OpponentsTeamRating = hTeam->ArenaTeamRating;
            hTeam->OpponentsTeamRating = aTeam->ArenaTeamRating;
            aTeam->OpponentsMatchmakerRating = hTeam->ArenaMatchmakerRating;
            hTeam->OpponentsMatchmakerRating = aTeam->ArenaMatchmakerRating;
            TC_LOG_DEBUG("bg.battleground", "setting oposite teamrating for to {}", aTeam->OpponentsTeamRating);
            TC_LOG_DEBUG("bg.battleground", "setting oposite teamrating for to {}", hTeam->OpponentsTeamRating);

            // now we must move team if we changed its faction to another faction queue, because then we will spam log by errors in Queue::RemovePlayer
            if (aTeam->Team != ALLIANCE)
            {
                m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE].push_front(aTeam);
                m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_HORDE].erase(itr_teams[TEAM_ALLIANCE]);
            }
            if (hTeam->Team != HORDE)
            {
                m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_HORDE].push_front(hTeam);
                m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE].erase(itr_teams[TEAM_HORDE]);
            }

            arena->SetArenaMatchmakerRating(ALLIANCE, aTeam->ArenaMatchmakerRating);
            arena->SetArenaMatchmakerRating(   HORDE, hTeam->ArenaMatchmakerRating);
            InviteGroupToBG(aTeam, arena, ALLIANCE);
            InviteGroupToBG(hTeam, arena, HORDE);

            TC_LOG_DEBUG("bg.battleground", "Starting rated arena match!");
            arena->StartBattleground();
        }
    }
    else if (BattlegroundQueueIdType(m_queueId.Type) == BattlegroundQueueIdType::RatedBattlegroundBlitz)
    {
        // Rated + non-arena previously fell through here doing nothing at all, which is why a rated
        // battleground queue could never pop.
        uint32 const perTeam = sBattlegroundMgr->isTesting() ? 1 : MaxPlayersPerTeam;
        uint32 const tanks   = sBattlegroundMgr->isTesting() ? 0 : sWorld->getIntConfig(CONFIG_BATTLEGROUND_BLITZ_TANKS_PER_TEAM);
        uint32 const healers = sBattlegroundMgr->isTesting() ? 0 : sWorld->getIntConfig(CONFIG_BATTLEGROUND_BLITZ_HEALERS_PER_TEAM);

        // One proposal at a time per bracket: a second lobby built while the first is still deciding would be
        // drawing from entries the first one has already claimed.
        for (std::pair<uint32 const, BattlegroundProposal> const& pending : m_Proposals)
            if (pending.second.BracketId == bracket_id)
                return;

        if (CheckSoloQueueMatch(bracket_id, perTeam, tanks, healers))
        {
            // The battleground is created BEFORE anyone is asked to confirm, which is what makes the map id
            // and the per-side split real numbers rather than guesses - see the decode block in
            // BattlegroundPackets.h. A proposal that collapses drops this instance again.
            Battleground* bg2 = sBattlegroundMgr->CreateNewBattleground(m_queueId, bracket_id);
            if (!bg2)
            {
                TC_LOG_ERROR("bg.battleground", "BattlegroundQueue::Update - Cannot create Battleground Blitz instance: {}", m_queueId.BattlemasterListId);
                return;
            }

            // Registers the instance with BattlegroundMgr, which CMSG_BATTLEFIELD_PORT needs to resolve the
            // invite. Rated battlegrounds are skipped by the free-slot filler above, so this does not let
            // anyone else be dealt into the lobby while it is deciding.
            bg2->StartBattleground();

            for (uint32 i = 0; i < PVP_TEAMS_COUNT; ++i)
                for (GroupsQueueType::const_iterator citr = m_SelectionPools[TEAM_ALLIANCE + i].SelectedGroups.begin(); citr != m_SelectionPools[TEAM_ALLIANCE + i].SelectedGroups.end(); ++citr)
                    InviteGroupToBG((*citr), bg2, (*citr)->Team, PROPOSAL_CONFIRM_WAIT_TIME, true);
                    InviteGroupToBG((*citr), bg2, (*citr)->Team, PROPOSAL_ACCEPT_WAIT_TIME, true);

            TC_LOG_DEBUG("bg.battleground", "Proposing a Battleground Blitz match ({} per team: {} tanks, {} healers, {} damagers)",
                perTeam, tanks, healers, perTeam - tanks - healers);

            StartProposal(bg2, bracket_id, perTeam, SideQuota(perTeam, tanks, healers));

            m_SelectionPools[TEAM_ALLIANCE].Init();
            m_SelectionPools[TEAM_HORDE].Init();
        }
    }
}

/*********************************************************/
/***            BATTLEGROUND QUEUE EVENTS              ***/
/*********************************************************/

bool BGQueueInviteEvent::Execute(uint64 /*e_time*/, uint32 /*p_time*/)
{
    Player* player = ObjectAccessor::FindConnectedPlayer(m_PlayerGuid);
    // player logged off (we should do nothing, he is correctly removed from queue in another procedure)
    if (!player)
        return true;

    Battleground* bg = sBattlegroundMgr->GetBattleground(m_BgInstanceGUID, m_BgTypeId);
    //if battleground ended and its instance deleted - do nothing
    if (!bg)
        return true;

    uint32 queueSlot = player->GetBattlegroundQueueIndex(m_QueueId);
    if (queueSlot < PLAYER_MAX_BATTLEGROUND_QUEUES)         // player is in queue or in battleground
    {
        // check if player is invited to this bg
        BattlegroundQueue &bgQueue = sBattlegroundMgr->GetBattlegroundQueue(m_QueueId);
        if (bgQueue.IsPlayerInvited(m_PlayerGuid, m_BgInstanceGUID, m_RemoveTime))
        {
            // Timeout is the REMAINING life of the invitation, not a constant. InviteGroupToBG schedules this
            // reminder INVITATION_REMIND_TIME before ginfo->RemoveInviteTime, and that deadline is what
            // BGQueueRemoveEvent / BGQueueProposalTimeoutEvent actually act on - so it is the only honest
            // source for the number. A constant INVITE_ACCEPT_WAIT_TIME - INVITATION_REMIND_TIME would restart
            // the client's confirmation countdown at 70000 ms when 20000 ms are left, and would be wrong by a
            // further 60000 ms for the shorter PROPOSAL_ACCEPT_WAIT_TIME window of the solo queues, whose
            // SMSG_BATTLEFIELD_STATUS_WAIT_FOR_GROUPS already told the same client 30000 ms.
            uint32 now = GameTime::GetGameTimeMS();
            uint32 remaining = m_RemoveTime > now ? m_RemoveTime - now : 0;

            WorldPackets::Battleground::BattlefieldStatusNeedConfirmation battlefieldStatus;
            BattlegroundMgr::BuildBattlegroundStatusNeedConfirmation(&battlefieldStatus, bg, player, queueSlot, player->GetBattlegroundQueueJoinTime(m_QueueId), remaining, m_QueueId, bgQueue.GetPlayerRole(m_PlayerGuid));
            player->SendDirectMessage(battlefieldStatus.Write());
        }
    }
    return true;                                            //event will be deleted
}

void BGQueueInviteEvent::Abort(uint64 /*e_time*/)
{
    //do nothing
}

bool BGQueueProposalTimeoutEvent::Execute(uint64 /*e_time*/, uint32 /*p_time*/)
{
    // No-op when the proposal already resolved - it is looked up by instance id, which is never reused.
    sBattlegroundMgr->GetBattlegroundQueue(m_QueueId).ProposalTimeout(m_BgInstanceGUID);
    return true;                                            //event will be deleted
}

void BGQueueProposalTimeoutEvent::Abort(uint64 /*e_time*/)
{
    //do nothing
}

/*
    this event has many possibilities when it is executed:
    1. player is in battleground (he clicked enter on invitation window)
    2. player left battleground queue and he isn't there any more
    3. player left battleground queue and he joined it again and IsInvitedToBGInstanceGUID = 0
    4. player left queue and he joined again and he has been invited to same battleground again -> we should not remove him from queue yet
    5. player is invited to bg and he didn't choose what to do and timer expired - only in this condition we should call queue::RemovePlayer
    we must remove player in the 5. case even if battleground object doesn't exist!
*/
bool BGQueueRemoveEvent::Execute(uint64 /*e_time*/, uint32 /*p_time*/)
{
    Player* player = ObjectAccessor::FindConnectedPlayer(m_PlayerGuid);
    if (!player)
        // player logged off (we should do nothing, he is correctly removed from queue in another procedure)
        return true;

    Battleground* bg = sBattlegroundMgr->GetBattleground(m_BgInstanceGUID, BattlegroundTypeId(m_BgQueueTypeId.BattlemasterListId));
    //battleground can be deleted already when we are removing queue info
    //bg pointer can be NULL! so use it carefully!

    uint32 queueSlot = player->GetBattlegroundQueueIndex(m_BgQueueTypeId);
    if (queueSlot < PLAYER_MAX_BATTLEGROUND_QUEUES)         // player is in queue, or in Battleground
    {
        // check if player is in queue for this BG and if we are removing his invite event
        BattlegroundQueue &bgQueue = sBattlegroundMgr->GetBattlegroundQueue(m_BgQueueTypeId);
        if (bgQueue.IsPlayerInvited(m_PlayerGuid, m_BgInstanceGUID, m_RemoveTime))
        {
            TC_LOG_DEBUG("bg.battleground", "Battleground: removing player {} from bg queue for instance {} because of not pressing enter battle in time.", player->GetGUID().ToString(), m_BgInstanceGUID);

            player->RemoveBattlegroundQueueId(m_BgQueueTypeId);
            bgQueue.RemovePlayer(m_PlayerGuid, true);
            //update queues if battleground isn't ended
            if (bg && bg->isBattleground() && bg->GetStatus() != STATUS_WAIT_LEAVE)
                sBattlegroundMgr->ScheduleQueueUpdate(0, m_BgQueueTypeId, bg->GetBracketId());

            WorldPackets::Battleground::BattlefieldStatusNone battlefieldStatus;
            BattlegroundMgr::BuildBattlegroundStatusNone(&battlefieldStatus, player, queueSlot, player->GetBattlegroundQueueJoinTime(m_BgQueueTypeId));
            player->SendDirectMessage(battlefieldStatus.Write());
        }
    }

    //event will be deleted
    return true;
}

void BGQueueRemoveEvent::Abort(uint64 /*e_time*/)
{
    //do nothing
}
