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

#ifndef TRINITYCORE_FRIENDS_SERVICE_H
#define TRINITYCORE_FRIENDS_SERVICE_H

#include "WorldserverService.h"
#include "Client/api/client/v2/friends_service.pb.h"

struct BnetFriendInvitation;
struct BnetFriendship;

namespace Battlenet::Services
{
// bgs.protocol.friends.v2.client.FriendsService (OriginalHash 0x5869BE8C).
//
// Reached over the game socket tunnel: CMSG_BATTLENET_REQUEST (0x400124) -> WorldSession::
// HandleBattlenetRequest -> Battlenet::WorldserverServiceDispatcher, answered with
// SMSG_BATTLENET_RESPONSE (0x4202AD) and pushed with SMSG_BATTLENET_NOTIFICATION (0x4202AE).
// Before this class existed the service was registered as a bare WorldserverService<T> transport
// wrapper, so all fourteen methods returned ERROR_RPC_NOT_IMPLEMENTED after logging at TC_LOG_ERROR.
//
// All fourteen generated server methods are overridden. State lives in BnetFriendsMgr.
class FriendsService : public WorldserverService<friends::v2::client::FriendsService>
{
    typedef WorldserverService<friends::v2::client::FriendsService> BaseService;

public:
    FriendsService(WorldSession* session);

    uint32 HandleSubscribe(friends::v2::client::SubscribeRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleUnsubscribe(friends::v2::client::UnsubscribeRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleGetSentInvitations(friends::v2::client::GetSentInvitationsRequest const* request, friends::v2::client::GetSentInvitationsResponse* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleGetReceivedInvitations(friends::v2::client::GetReceivedInvitationsRequest const* request, friends::v2::client::GetReceivedInvitationsResponse* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleGetFriends(friends::v2::client::GetFriendsRequest const* request, friends::v2::client::GetFriendsResponse* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleIsFriend(friends::v2::client::IsFriendRequest const* request, friends::v2::client::IsFriendResponse* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleViewFriends(friends::v2::client::ViewFriendsRequest const* request, friends::v2::client::ViewFriendsResponse* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleSendInvitation(friends::v2::client::SendInvitationRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleAcceptInvitation(friends::v2::client::AcceptInvitationRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleRevokeInvitation(friends::v2::client::RevokeInvitationRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleRevokeAllInvitations(friends::v2::client::RevokeAllInvitationsRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleIgnoreInvitation(friends::v2::client::IgnoreInvitationRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleRemoveFriend(friends::v2::client::RemoveFriendRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleUpdateFriendState(friends::v2::client::UpdateFriendStateRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;

    // ---- wire fill helpers, shared with BnetFriendsMgr's push path ----------------------------
    // Kept here rather than in BnetFriendsMgr.h so the manager header stays free of protobuf, the
    // same split ClubService/ClubMembershipService already use.

    static void FillFriend(friends::v2::client::Friend* out, BnetFriendship const& friendship,
        bool fetchNames, bool fetchNotes, bool fetchTitleTags);
    static void FillReceivedInvitation(friends::v2::client::ReceivedInvitation* out, BnetFriendInvitation const& invitation);
    static void FillSentInvitation(friends::v2::client::SentInvitation* out, BnetFriendInvitation const& invitation);
    static void FillUserDescription(friends::v2::client::UserDescription* out, uint32 bnetAccountId);

    // bgs program id for World of Warcraft: 'W','o','W' packed big-endian = 0x576F57 = 5730135.
    // Same constant ClubService::CreateGuildClubType already puts on the wire.
    static constexpr uint32 ProgramWoW = 5730135;
};
}

#endif // TRINITYCORE_FRIENDS_SERVICE_H
