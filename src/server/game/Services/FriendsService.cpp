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

#include "FriendsService.h"
#include "BattlenetRpcErrorCodes.h"
#include "BnetFriendsMgr.h"
#include "Log.h"
#include "Client/api/common/v1/invitation_types.pb.h"

namespace Battlenet::Services
{
FriendsService::FriendsService(WorldSession* session) : BaseService(session) { }

// ---------------------------------------------------------------------------------------------
// wire fill helpers
// ---------------------------------------------------------------------------------------------

void FriendsService::FillUserDescription(friends::v2::client::UserDescription* out, uint32 bnetAccountId)
{
    out->set_account_id(bnetAccountId);

    if (BnetAccountIdentity const* identity = sBnetFriendsMgr->GetIdentity(bnetAccountId))
        if (identity->HasBattleTag())
            out->set_battle_tag(identity->GetBattleTag());

    // full_name is the Real ID display name. TrinityCore stores no real names, and inventing one would
    // put a fabricated identity in front of another player, so the field is deliberately left unset.
}

void FriendsService::FillFriend(friends::v2::client::Friend* out, BnetFriendship const& friendship,
    bool fetchNames, bool fetchNotes, bool fetchTitleTags)
{
    out->set_id(friendship.FriendId);
    out->set_level(friendship.Level);
    out->set_creation_time_s(uint64(friendship.CreationTime));

    if (fetchNames)
        if (BnetAccountIdentity const* identity = sBnetFriendsMgr->GetIdentity(friendship.FriendId))
            if (identity->HasBattleTag())
                out->set_battle_tag(identity->GetBattleTag());

    if (fetchNotes && !friendship.Note.empty())
        out->set_note(friendship.Note);

    if (fetchTitleTags && !friendship.TitleTagIds.empty())
        for (uint32 titleTagId : friendship.TitleTagIds)
            out->mutable_title_tag()->add_ids(titleTagId);
}

void FriendsService::FillReceivedInvitation(friends::v2::client::ReceivedInvitation* out, BnetFriendInvitation const& invitation)
{
    out->set_id(invitation.Id);
    FillUserDescription(out->mutable_inviter(), invitation.SenderId);
    out->set_level(invitation.Level);
    out->set_program(ProgramWoW);
    out->set_creation_time_s(uint64(invitation.CreationTime));
    out->set_expiration_time_s(uint64(invitation.ExpirationTime));

    // `invitee` is marked deprecated in the generated header and is therefore not filled.
}

void FriendsService::FillSentInvitation(friends::v2::client::SentInvitation* out, BnetFriendInvitation const& invitation)
{
    out->set_id(invitation.Id);
    out->set_level(invitation.Level);
    out->set_program(ProgramWoW);
    out->set_creation_time_s(uint64(invitation.CreationTime));

    // Prefer the recipient's current BattleTag; fall back to whatever the sender typed if the target
    // has since lost its tag.
    BnetAccountIdentity const* target = sBnetFriendsMgr->GetIdentity(invitation.TargetId);
    if (target && target->HasBattleTag())
        out->set_target_name(target->GetBattleTag());
    else if (!invitation.TargetTag.empty())
        out->set_target_name(invitation.TargetTag);

    if (!invitation.Note.empty())
        out->set_note(invitation.Note);

    for (uint32 titleTagId : invitation.TitleTagIds)
        out->mutable_title_tag()->add_ids(titleTagId);
}

// ---------------------------------------------------------------------------------------------
// server methods
// ---------------------------------------------------------------------------------------------

uint32 FriendsService::HandleSubscribe(friends::v2::client::SubscribeRequest const* /*request*/, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    // Makes sure the caller has a BattleTag even if the account was created after startup - without one
    // it can neither be found nor rendered.
    if (!sBnetFriendsMgr->GetOrLoadIdentity(_session->GetBattlenetAccountId()))
        return ERROR_INVALID_AGENT_ID;

    // Registers this session for listener push, then sends the whole current state as notifications.
    // SubscribeRequest has no fields and the response is NoData, so this is the only way the initial
    // friend list can reach the client.
    sBnetFriendsMgr->Subscribe(_session);

    return ERROR_OK;
}

uint32 FriendsService::HandleUnsubscribe(friends::v2::client::UnsubscribeRequest const* /*request*/, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    if (!sBnetFriendsMgr->Unsubscribe(_session))
        return ERROR_FRIENDS_NOT_SUBSCRIBED;

    return ERROR_OK;
}

uint32 FriendsService::HandleGetSentInvitations(friends::v2::client::GetSentInvitationsRequest const* /*request*/,
    friends::v2::client::GetSentInvitationsResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    // The whole list fits in one response - an account is capped at MaxSentInvitations - so the
    // `continuation` cursor is intentionally left unset, which tells the client there is no next page.
    for (BnetFriendInvitation const& invitation : sBnetFriendsMgr->GetSentInvitations(_session->GetBattlenetAccountId()))
        FillSentInvitation(response->add_invitations(), invitation);

    return ERROR_OK;
}

uint32 FriendsService::HandleGetReceivedInvitations(friends::v2::client::GetReceivedInvitationsRequest const* /*request*/,
    friends::v2::client::GetReceivedInvitationsResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    for (BnetFriendInvitation const& invitation : sBnetFriendsMgr->GetReceivedInvitations(_session->GetBattlenetAccountId()))
        FillReceivedInvitation(response->add_invitations(), invitation);

    return ERROR_OK;
}

uint32 FriendsService::HandleGetFriends(friends::v2::client::GetFriendsRequest const* request,
    friends::v2::client::GetFriendsResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    // When the client states what it wants, honour it exactly; when it sends no options at all, send
    // everything rather than an id-only list the UI could not render.
    bool fetchNames = true;
    bool fetchNotes = true;
    bool fetchTitleTags = true;

    if (request->has_options())
    {
        friends::v2::client::GetFriendsOptions const& options = request->options();
        fetchNames = options.fetch_names();
        fetchNotes = options.fetch_notes();
        fetchTitleTags = options.fetch_title_tags();
    }

    std::vector<BnetFriendship> const& friendList = sBnetFriendsMgr->GetFriends(_session->GetBattlenetAccountId());
    response->mutable_friends()->Reserve(int(friendList.size()));

    for (BnetFriendship const& friendship : friendList)
        FillFriend(response->add_friends(), friendship, fetchNames, fetchNotes, fetchTitleTags);

    return ERROR_OK;
}

uint32 FriendsService::HandleIsFriend(friends::v2::client::IsFriendRequest const* request,
    friends::v2::client::IsFriendResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    response->set_result(sBnetFriendsMgr->IsFriend(_session->GetBattlenetAccountId(), uint32(request->target_account_id())));

    return ERROR_OK;
}

uint32 FriendsService::HandleViewFriends(friends::v2::client::ViewFriendsRequest const* request,
    friends::v2::client::ViewFriendsResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    uint32 callerId = _session->GetBattlenetAccountId();
    uint32 targetId = uint32(request->target_account_id());

    if (!targetId)
        return ERROR_INVALID_TARGET_ID;

    // Retail gates this on the target's privacy settings. TrinityCore has no Battle.net privacy model,
    // so the conservative equivalent is applied: you may look at your own friend list, or at the friend
    // list of someone who has accepted you. Anything wider would leak the whole social graph.
    if (targetId != callerId && !sBnetFriendsMgr->IsFriend(callerId, targetId))
        return ERROR_DENIED;

    std::vector<BnetFriendship> const& friendList = sBnetFriendsMgr->GetFriends(targetId);
    response->mutable_friends()->Reserve(int(friendList.size()));

    for (BnetFriendship const& friendship : friendList)
    {
        friends::v2::client::FriendOfFriend* out = response->add_friends();
        out->set_id(friendship.FriendId);
        out->set_level(friendship.Level);

        if (BnetAccountIdentity const* identity = sBnetFriendsMgr->GetIdentity(friendship.FriendId))
            if (identity->HasBattleTag())
                out->set_battle_tag(identity->GetBattleTag());

        // full_name deliberately unset - see FillUserDescription.
    }

    return ERROR_OK;
}

uint32 FriendsService::HandleSendInvitation(friends::v2::client::SendInvitationRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    if (!request->has_target())
        return ERROR_RPC_MALFORMED_REQUEST;

    friends::v2::client::SendInvitationTarget const& target = request->target();

    // SendInvitationTarget is a oneof over name / account_id / email / battle_tag / phone_number.
    uint32 targetId = 0;
    std::string typedTarget;

    if (target.has_account_id())
    {
        targetId = uint32(target.account_id());
    }
    else if (target.has_battle_tag())
    {
        typedTarget = target.battle_tag();
        targetId = sBnetFriendsMgr->FindAccountByBattleTag(typedTarget);
    }
    else if (target.has_email())
    {
        typedTarget = target.email();
        targetId = sBnetFriendsMgr->FindAccountByEmail(typedTarget);
    }
    else if (target.has_name())
    {
        // The generic field the client fills when the player typed something free-form. Try both handles.
        typedTarget = target.name();
        targetId = sBnetFriendsMgr->FindAccountByBattleTag(typedTarget);
        if (!targetId)
            targetId = sBnetFriendsMgr->FindAccountByEmail(typedTarget);
    }
    else if (target.has_phone_number())
    {
        // There is no phone-number registry on this realm and nothing local can stand in for one.
        return ERROR_NOT_IMPLEMENTED;
    }
    else
    {
        return ERROR_RPC_MALFORMED_REQUEST;
    }

    if (!targetId)
        return ERROR_INVALID_TARGET_ID;

    uint32 level = 0;
    std::string note;
    std::vector<uint32> titleTagIds;

    if (request->has_options())
    {
        friends::v2::client::SendInvitationOptions const& options = request->options();
        level = options.level();
        note = options.note();

        for (int i = 0; i < options.title_tag().ids_size(); ++i)
            titleTagIds.push_back(options.title_tag().ids(i));

        if (options.attributes_size())
            TC_LOG_DEBUG("session.rpc", "{} sent a friend invitation carrying {} attribute(s); this realm stores no friend attributes.",
                _session->GetPlayerInfo(), options.attributes_size());
    }

    return sBnetFriendsMgr->SendInvitation(_session->GetBattlenetAccountId(), targetId, typedTarget, level, note, std::move(titleTagIds));
}

uint32 FriendsService::HandleAcceptInvitation(friends::v2::client::AcceptInvitationRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    if (!request->has_invitation_id())
        return ERROR_RPC_MALFORMED_REQUEST;

    // AcceptInvitationOptions carries only `level`. It is stored verbatim and echoed back on the
    // resulting Friend; the numbering is the client's, not ours, so it is never synthesised.
    uint32 level = request->has_options() ? request->options().level() : 0;

    return sBnetFriendsMgr->AcceptInvitation(_session->GetBattlenetAccountId(), request->invitation_id(), level);
}

uint32 FriendsService::HandleRevokeInvitation(friends::v2::client::RevokeInvitationRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    if (!request->has_invitation_id())
        return ERROR_RPC_MALFORMED_REQUEST;

    return sBnetFriendsMgr->RemoveInvitation(_session->GetBattlenetAccountId(), request->invitation_id(),
        bgs::protocol::INVITATION_REMOVED_REASON_REVOKED, true);
}

uint32 FriendsService::HandleRevokeAllInvitations(friends::v2::client::RevokeAllInvitationsRequest const* /*request*/, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    return sBnetFriendsMgr->RevokeAllInvitations(_session->GetBattlenetAccountId());
}

uint32 FriendsService::HandleIgnoreInvitation(friends::v2::client::IgnoreInvitationRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    if (!request->has_invitation_id())
        return ERROR_RPC_MALFORMED_REQUEST;

    // This is the recipient declining. block_list.v1 (backlog item #20) is what turns a decline into a
    // durable block; friends.v2 only drops the invitation.
    return sBnetFriendsMgr->RemoveInvitation(_session->GetBattlenetAccountId(), request->invitation_id(),
        bgs::protocol::INVITATION_REMOVED_REASON_IGNORED, false);
}

uint32 FriendsService::HandleRemoveFriend(friends::v2::client::RemoveFriendRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    if (!request->has_target_account_id())
        return ERROR_RPC_MALFORMED_REQUEST;

    return sBnetFriendsMgr->RemoveFriend(_session->GetBattlenetAccountId(), uint32(request->target_account_id()));
}

uint32 FriendsService::HandleUpdateFriendState(friends::v2::client::UpdateFriendStateRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    if (!request->has_target_account_id() || !request->has_options())
        return ERROR_RPC_MALFORMED_REQUEST;

    friends::v2::client::UpdateFriendStateOptions const& options = request->options();

    std::vector<uint32> titleTagIds;
    if (options.has_title_tag())
        for (int i = 0; i < options.title_tag().ids_size(); ++i)
            titleTagIds.push_back(options.title_tag().ids(i));

    if (options.attributes_size())
        TC_LOG_DEBUG("session.rpc", "{} tried to set {} friend attribute(s); this realm stores no friend attributes.",
            _session->GetPlayerInfo(), options.attributes_size());

    // A request carrying only attributes changes nothing that survives, and BnetFriendsMgr answers it
    // with ERROR_FRIENDS_UPDATE_FRIEND_STATE_FAILED rather than a success the client cannot rely on.
    return sBnetFriendsMgr->UpdateFriendState(_session->GetBattlenetAccountId(), uint32(request->target_account_id()),
        options.has_note(), options.note(), options.has_title_tag(), std::move(titleTagIds));
}
}
