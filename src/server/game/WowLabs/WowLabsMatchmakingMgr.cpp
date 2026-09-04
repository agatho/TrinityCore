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

#include "WowLabsMatchmakingMgr.h"
#include "LobbyMatchmakerPackets.h"
#include "Log.h"
#include "WorldSession.h"
#include <algorithm>

namespace
{
    void SendError(WorldSession* session, WorldPackets::LobbyMatchmaker::WowLabsPartyError::ErrorType error)
    {
        WorldPackets::LobbyMatchmaker::WowLabsPartyError err;
        err.Error = error;
        session->SendPacket(err.Write());
    }
}

WowLabsMatchmakingMgr* WowLabsMatchmakingMgr::instance()
{
    static WowLabsMatchmakingMgr instance;
    return &instance;
}

ObjectGuid WowLabsMatchmakingMgr::BnetGuidOf(WorldSession* session)
{
    return session->GetBattlenetAccountGUID();
}

std::string WowLabsMatchmakingMgr::NameOf(WorldSession* session)
{
    // Roster display name. Pre-login there is no Player loaded, so P0 uses the account name as a placeholder;
    // sourcing the selected character's name is a later refinement (the wire carries whatever string we set).
    return session->GetAccountName();
}

WowLabsMatchmakingMgr::Member* WowLabsMatchmakingMgr::Party::FindMember(ObjectGuid bnet)
{
    for (Member& m : Members)
        if (m.BnetAccountGuid == bnet)
            return &m;
    return nullptr;
}

void WowLabsMatchmakingMgr::RegisterSession(WorldSession* session)
{
    if (session)
        _lobbySessions[session->GetBattlenetAccountGUID().GetCounter()] = session;
}

WorldSession* WowLabsMatchmakingMgr::FindLobbySession(ObjectGuid bnet) const
{
    auto it = _lobbySessions.find(bnet.GetCounter());
    return it != _lobbySessions.end() ? it->second : nullptr;
}

WowLabsMatchmakingMgr::Party* WowLabsMatchmakingMgr::GetPartyByBnet(ObjectGuid bnet)
{
    auto it = _partiesByBnet.find(bnet.GetCounter());
    return it != _partiesByBnet.end() ? it->second.get() : nullptr;
}

WowLabsMatchmakingMgr::Party* WowLabsMatchmakingMgr::GetOrCreatePartyFor(WorldSession* session)
{
    RegisterSession(session);
    ObjectGuid const bnet = BnetGuidOf(session);
    if (Party* existing = GetPartyByBnet(bnet))
        return existing;

    auto party = std::make_shared<Party>();
    party->Id = _nextPartyId++;
    party->LeaderBnetGuid = bnet;
    Member self;
    self.BnetAccountGuid = bnet;
    self.PlayerGuid = session->GetBattlenetAccountGUID();   // stand-in until the selected character is known
    self.Name = NameOf(session);
    self.Session = session;
    party->Members.push_back(std::move(self));
    _partiesByBnet[bnet.GetCounter()] = party;
    return party.get();
}

void WowLabsMatchmakingMgr::DisbandIfEmpty(Party* party)
{
    if (!party || party->Members.size() > 1)
        return;

    // A lone remaining member is just a solo party again - keep it. Only drop a truly empty party.
    if (party->Members.empty())
        for (auto it = _partiesByBnet.begin(); it != _partiesByBnet.end();)
            it = (it->second.get() == party) ? _partiesByBnet.erase(it) : std::next(it);
}

void WowLabsMatchmakingMgr::Invite(WorldSession* inviter, ObjectGuid targetBnetGuid)
{
    using namespace WorldPackets::LobbyMatchmaker;
    RegisterSession(inviter);

    WorldSession* target = FindLobbySession(targetBnetGuid);
    if (!target || target == inviter)
    {
        SendError(inviter, WowLabsPartyError::PARTY_INVITE_INVALID);
        return;
    }

    Party* party = GetOrCreatePartyFor(inviter);
    if (party->Members.size() >= MAX_PARTY_SIZE)
    {
        SendError(inviter, WowLabsPartyError::PARTY_IS_FULL);
        return;
    }

    if (party->FindMember(targetBnetGuid))
    {
        SendError(inviter, WowLabsPartyError::PLAYER_ALREADY_INVITED);
        return;
    }

    std::vector<ObjectGuid>& invites = _pendingInvites[targetBnetGuid.GetCounter()];
    ObjectGuid const inviterBnet = BnetGuidOf(inviter);
    if (std::find(invites.begin(), invites.end(), inviterBnet) != invites.end())
    {
        SendError(inviter, WowLabsPartyError::PLAYER_ALREADY_INVITED);
        return;
    }
    invites.push_back(inviterBnet);

    LobbyMatchmakerReceiveInvite invite;
    invite.InviterGuid = inviterBnet;
    invite.InviterName = NameOf(inviter);
    target->SendPacket(invite.Write());

    TC_LOG_DEBUG("network", "WowLabs: {} invited bnet {} to lobby party {}.", inviter->GetAccountName(),
        targetBnetGuid.ToString(), party->Id);
}

void WowLabsMatchmakingMgr::AcceptInvite(WorldSession* invitee, ObjectGuid inviterBnetGuid)
{
    using namespace WorldPackets::LobbyMatchmaker;
    RegisterSession(invitee);

    ObjectGuid const inviteeBnet = BnetGuidOf(invitee);
    std::vector<ObjectGuid>& invites = _pendingInvites[inviteeBnet.GetCounter()];
    auto it = std::find(invites.begin(), invites.end(), inviterBnetGuid);
    if (it == invites.end())
    {
        SendError(invitee, WowLabsPartyError::PARTY_INVITE_INVALID);
        return;
    }
    invites.erase(it);

    Party* party = GetPartyByBnet(inviterBnetGuid);
    if (!party || party->Members.size() >= MAX_PARTY_SIZE)
    {
        SendError(invitee, party ? WowLabsPartyError::PARTY_IS_FULL : WowLabsPartyError::PARTY_INVITE_INVALID);
        return;
    }

    // Leave any prior party first, then join the inviter's.
    LeaveParty(invitee);

    Member m;
    m.BnetAccountGuid = inviteeBnet;
    m.PlayerGuid = invitee->GetBattlenetAccountGUID();
    m.Name = NameOf(invitee);
    m.Session = invitee;
    party->Members.push_back(std::move(m));
    _partiesByBnet[inviteeBnet.GetCounter()] = _partiesByBnet[inviterBnetGuid.GetCounter()];

    BroadcastPartyInfo(party);
    TC_LOG_DEBUG("network", "WowLabs: {} joined lobby party {}.", invitee->GetAccountName(), party->Id);
}

void WowLabsMatchmakingMgr::RejectInvite(WorldSession* invitee, ObjectGuid inviterBnetGuid)
{
    using namespace WorldPackets::LobbyMatchmaker;
    RegisterSession(invitee);

    ObjectGuid const inviteeBnet = BnetGuidOf(invitee);
    std::vector<ObjectGuid>& invites = _pendingInvites[inviteeBnet.GetCounter()];
    invites.erase(std::remove(invites.begin(), invites.end(), inviterBnetGuid), invites.end());

    if (WorldSession* inviter = FindLobbySession(inviterBnetGuid))
    {
        LobbyMatchmakerPartyInviteRejected rejected;
        rejected.Name = NameOf(invitee);
        inviter->SendPacket(rejected.Write());
    }
}

void WowLabsMatchmakingMgr::Uninvite(WorldSession* leader, ObjectGuid targetBnetGuid)
{
    using namespace WorldPackets::LobbyMatchmaker;
    Party* party = GetPartyByBnet(BnetGuidOf(leader));
    if (!party || !party->IsLeader(BnetGuidOf(leader)))
    {
        SendError(leader, WowLabsPartyError::PARTY_INVITE_INVALID);
        return;
    }

    // Cancel a still-pending invite, or remove an already-joined member.
    std::vector<ObjectGuid>& invites = _pendingInvites[targetBnetGuid.GetCounter()];
    invites.erase(std::remove(invites.begin(), invites.end(), party->LeaderBnetGuid), invites.end());

    if (WorldSession* removed = FindLobbySession(targetBnetGuid))
        if (party->FindMember(targetBnetGuid))
            LeaveParty(removed);

    BroadcastPartyInfo(party);
}

void WowLabsMatchmakingMgr::LeaveParty(WorldSession* session)
{
    ObjectGuid const bnet = BnetGuidOf(session);
    Party* party = GetPartyByBnet(bnet);
    if (!party)
        return;

    bool const wasLeader = party->IsLeader(bnet);
    std::erase_if(party->Members, [bnet](Member const& m) { return m.BnetAccountGuid == bnet; });
    _partiesByBnet.erase(bnet.GetCounter());

    if (party->Members.empty())
    {
        DisbandIfEmpty(party);
        return;
    }

    if (wasLeader)
        party->LeaderBnetGuid = party->Members.front().BnetAccountGuid;   // promote the next member

    BroadcastPartyInfo(party);
}

void WowLabsMatchmakingMgr::SetPlaylistEntry(WorldSession* leader, uint32 playlistEntry)
{
    using namespace WorldPackets::LobbyMatchmaker;
    Party* party = GetOrCreatePartyFor(leader);
    if (!party->IsLeader(BnetGuidOf(leader)))
    {
        SendError(leader, WowLabsPartyError::PARTY_INVITE_INVALID);
        return;
    }

    party->PlaylistEntry = playlistEntry;
    BroadcastPartyInfo(party);
}

void WowLabsMatchmakingMgr::SetReady(WorldSession* session, bool ready)
{
    Party* party = GetOrCreatePartyFor(session);
    if (Member* m = party->FindMember(BnetGuidOf(session)))
        m->Ready = ready;
    BroadcastPartyInfo(party);
}

void WowLabsMatchmakingMgr::OnSessionLeave(WorldSession* session)
{
    if (!session)
        return;
    LeaveParty(session);
    _lobbySessions.erase(session->GetBattlenetAccountGUID().GetCounter());
    _pendingInvites.erase(session->GetBattlenetAccountGUID().GetCounter());
}

void WowLabsMatchmakingMgr::BroadcastPartyInfo(Party const* party)
{
    if (!party)
        return;
    for (Member const& recipient : party->Members)
        if (recipient.Session)
            SendPartyInfoTo(recipient.Session, party);
}

void WowLabsMatchmakingMgr::SendPartyInfoTo(WorldSession* session, Party const* party)
{
    WorldPackets::LobbyMatchmaker::LobbyMatchmakerPartyInfo info;
    if (party)
    {
        info.LeaderGuid = party->LeaderBnetGuid;
        info.PlaylistEntry = party->PlaylistEntry;
        ObjectGuid const recipientBnet = BnetGuidOf(session);
        for (Member const& m : party->Members)
        {
            WorldPackets::LobbyMatchmaker::LobbyMatchmakerPartyInfoMember out;
            out.MemberGuid = m.PlayerGuid;
            out.AccountGuid = m.BnetAccountGuid;
            out.Name = m.Name;
            out.ReadyBit = m.Ready;                                      // isReady
            out.Field97 = party->IsLeader(m.BnetAccountGuid) ? 1 : 0;    // isPartyLeader
            out.Field98 = (m.BnetAccountGuid == recipientBnet) ? 1 : 0;  // isLocalPlayer (per recipient)
            info.Members.push_back(std::move(out));
        }
    }
    session->SendPacket(info.Write());
}
