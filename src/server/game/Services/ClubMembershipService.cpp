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

#include "ClubMembershipService.h"
#include "BattlenetRpcErrorCodes.h"
#include "CharacterCache.h"
#include "ClubService.h"
#include "ClubStreamHistoryMgr.h"
#include "ClubUtils.h"
#include "GameTime.h"
#include "Guild.h"
#include "Player.h"
#include <limits>

namespace Battlenet::Services
{
ClubMembershipService::ClubMembershipService(WorldSession* session) : BaseService(session) { }

uint32 ClubMembershipService::HandleSubscribe(club_membership::v1::client::SubscribeRequest const* /*request*/, club_membership::v1::client::SubscribeResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    Guild const* guild = player->GetGuild();

    if (!guild)
        return ERROR_OK;

    club_membership::v1::client::ClubMembershipDescription* description = response->mutable_state()->add_description();
    description->set_allocated_member_id(CreateClubMemberId(player->GetGUID()).release());

    club::v1::ClubDescription* club = description->mutable_club();
    club->set_id(guild->GetId());
    club->set_allocated_type(ClubService::CreateGuildClubType().release());
    club->set_name(guild->GetName());
    club->set_privacy_level(club::v1::PrivacyLevel::PRIVACY_LEVEL_OPEN);
    club->set_visibility_level(club::v1::VISIBILITY_LEVEL_PRIVATE);
    club->set_member_count(guild->GetMembersCount());
    club->set_creation_time(
        std::chrono::duration_cast<std::chrono::microseconds>(SystemTimePoint::clock::from_time_t(guild->GetCreatedDate()).time_since_epoch()).count());

    // Not setting these can cause issues.
    club->set_timezone("");
    club->set_locale("");

    club::v1::client::MemberDescription* leader = club->add_leader();

    leader->set_allocated_id(CreateClubMemberId(guild->GetLeaderGUID()).release());

    // The @mention badge is driven by this pair: the client shows it while last_message_time is newer
    // than last_read_time. Both were hardcoded to 0, so the badge could never appear and never clear.
    // Both are microseconds, the same unit as MessageId.epoch.
    ViewMarker* mentionView = response->mutable_state()->mutable_mention_view();
    mentionView->set_last_read_time(sClubStreamHistoryMgr->GetMentionViewTime(player->GetGUID()));
    mentionView->set_last_message_time(sClubStreamHistoryMgr->GetLastMentionTime(player->GetGUID()));

    return ERROR_OK;
}

uint32 ClubMembershipService::HandleUnsubscribe(club_membership::v1::client::UnsubscribeRequest const* /*request*/, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    // Unsubscribing from the membership drops every live stream subscription and focus this member held,
    // so a stale focus cannot keep silently marking an unattended stream as read.
    sClubStreamHistoryMgr->ClearSessionState(player->GetGUID());

    return ERROR_OK;
}

uint32 ClubMembershipService::HandleGetStreamMentions(club_membership::v1::client::GetStreamMentionsRequest const* request,
    club_membership::v1::client::GetStreamMentionsResponse* response, std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    // Same GetEventOptions contract as club.v1 GetStreamHistory: bounds on the event time in
    // microseconds, a page cap and a direction. EventOrder defaults to EVENT_DESCENDING (newest first).
    uint64 fetchFrom = 0;
    uint64 fetchUntil = std::numeric_limits<uint64>::max();
    uint32 maxEvents = sClubStreamHistoryMgr->GetMaxMessagesPerStream();
    bool ascending = false;

    if (request->has_options())
    {
        GetEventOptions const& options = request->options();

        if (options.has_fetch_from())
            fetchFrom = options.fetch_from();

        if (options.has_fetch_until())
            fetchUntil = options.fetch_until();

        if (options.has_max_events() && options.max_events())
            maxEvents = options.max_events();

        ascending = options.order() == EVENT_ASCENDING;
    }

    // Never let a paging request with history disabled collapse the page size to zero.
    if (!maxEvents)
        maxEvents = 100;

    for (ClubMemberMention const* mention : sClubStreamHistoryMgr->GetMentions(player->GetGUID(), fetchFrom, fetchUntil, maxEvents, ascending))
    {
        club::v1::client::StreamMention* wire = response->add_mention();

        wire->set_club_id(mention->ClubId);
        wire->set_stream_id(mention->StreamId);
        wire->set_allocated_club_type(ClubService::CreateGuildClubType().release());
        wire->set_allocated_member_id(CreateClubMemberId(mention->MemberGuid).release());

        wire->mutable_message_id()->set_epoch(mention->Epoch);
        wire->mutable_message_id()->set_position(mention->Position);

        // TimeSeriesId is the mention's own identity, and it is what RemoveStreamMentions sends back.
        // It is the identity of the message that produced the mention, so the two round trip exactly.
        wire->mutable_mention_id()->set_epoch(mention->Epoch);
        wire->mutable_mention_id()->set_position(mention->Position);

        club::v1::MemberId* authorId = wire->mutable_author()->mutable_id();
        authorId->set_account_id(mention->AuthorAccountId);
        authorId->set_unique_id(Clubs::CreateClubMemberId(mention->AuthorGuid));

        if (request->fetch_messages())
            if (ClubStreamMessage const* message = sClubStreamHistoryMgr->GetMessage(mention->ClubId, mention->StreamId, mention->Epoch, mention->Position))
                ClubService::FillStreamMessage(wire->mutable_message(), *message);
    }

    // GetStreamMentionsResponse.continuation is left unset for the same reason as the club.v1 history
    // response: GetStreamMentionsRequest carries only options and fetch_messages, so there is no field
    // the client could echo a token back into. Paging runs through options.fetch_from / fetch_until.

    return ERROR_OK;
}

uint32 ClubMembershipService::HandleRemoveStreamMentions(club_membership::v1::client::RemoveStreamMentionsRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    if (request->mention_id().empty())
        return ERROR_OK;

    std::vector<std::pair<uint64, uint64>> mentionIds;
    mentionIds.reserve(request->mention_id_size());

    for (TimeSeriesId const& id : request->mention_id())
        mentionIds.emplace_back(id.epoch(), id.position());

    sClubStreamHistoryMgr->RemoveMentions(player->GetGUID(), mentionIds);

    return ERROR_OK;
}

uint32 ClubMembershipService::HandleAdvanceStreamMentionViewTime(club_membership::v1::client::AdvanceStreamMentionViewTimeRequest const* /*request*/,
    NoData* /*response*/, std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    // The request message has no fields at all, so the only thing it can mean is "everything I have been
    // mentioned in is read, as of now". That is also why the marker is one value per member rather than
    // one per club or stream.
    sClubStreamHistoryMgr->AdvanceMentionViewTime(player->GetGUID(),
        uint64(std::chrono::duration_cast<std::chrono::microseconds>(GameTime::GetSystemTime().time_since_epoch()).count()));

    return ERROR_OK;
}

std::unique_ptr<club::v1::MemberId> ClubMembershipService::CreateClubMemberId(ObjectGuid guid)
{
    std::unique_ptr<club::v1::MemberId> id = std::make_unique<club::v1::MemberId>();
    id->set_account_id(sCharacterCache->GetCharacterAccountIdByGuid(guid));
    id->set_unique_id(Clubs::CreateClubMemberId(guid));
    return id;
}
}
