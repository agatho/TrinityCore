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

#include "ClubService.h"
#include "BattlenetRpcErrorCodes.h"
#include "ClubMembershipService.h"
#include "ClubUtils.h"
#include "GameTime.h"
#include "Guild.h"
#include "Player.h"
#include "SocialMgr.h"
#include "api/client/v1/club_listener.pb.h"
#include <algorithm>
#include <limits>

namespace Battlenet::Services
{
ClubService::ClubService(WorldSession* session) : BaseService(session) { }

uint64 ClubService::ResolveReadableStream(uint64 clubId, uint64 streamId, uint32& errorCode) const
{
    errorCode = ERROR_OK;

    Player const* player = _session->GetPlayer();
    if (!player)
    {
        errorCode = ERROR_INTERNAL;
        return 0;
    }

    Guild const* guild = player->GetGuild();
    if (!guild)
    {
        errorCode = ERROR_CLUB_NO_CLUB;
        return 0;
    }

    if (clubId && clubId != guild->GetId())
    {
        errorCode = ERROR_CLUB_NO_CLUB;
        return 0;
    }

    Guild::Member const* member = guild->GetMember(player->GetGUID());
    if (!member)
    {
        errorCode = ERROR_CLUB_NOT_MEMBER;
        return 0;
    }

    // Only the two synthetic guild streams exist until full communities land.
    if (streamId != AsUnderlyingType(ClubStreamType::Guild) && streamId != AsUnderlyingType(ClubStreamType::Officer))
    {
        errorCode = ERROR_CLUB_STREAM_NO_STREAM;
        return 0;
    }

    // Officer scrollback is rank gated exactly like live officer chat is, otherwise any member could
    // read it simply by asking for the history of stream 2.
    if (streamId == AsUnderlyingType(ClubStreamType::Officer) && !guild->HasAnyRankRight(member->GetRankId(), GR_RIGHT_OFFCHATLISTEN))
    {
        errorCode = ERROR_CLUB_INSUFFICIENT_PRIVILEGES;
        return 0;
    }

    return streamId;
}

uint32 ClubService::HandleGetClubType(club::v1::client::GetClubTypeRequest const* request, club::v1::client::GetClubTypeResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    // We only support guilds for now.
    if (request->type().name() == "guild")
    {
        response->set_allocated_type(CreateGuildClubType().release());
        return ERROR_OK;
    }

    return ERROR_NOT_IMPLEMENTED;
}

uint32 ClubService::HandleSubscribe(club::v1::client::SubscribeRequest const* /*request*/, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    Guild const* guild = player->GetGuild();

    if (!guild)
        return ERROR_CLUB_NO_CLUB;

    // Subscibe the client to it's own guild club.
    club::v1::client::SubscribeNotification subscribeNotification;

    Guild::Member const* guildMember = guild->GetMember(player->GetGUID());

    if (!guildMember)
        return ERROR_CLUB_NOT_MEMBER;

    Guild::Member const* guildLeader = guild->GetMember(guild->GetLeaderGUID());

    if (!guildLeader)
        return ERROR_CLUB_NO_SUCH_MEMBER;

    subscribeNotification.set_club_id(guild->GetId());
    subscribeNotification.set_allocated_agent_id(ClubMembershipService::CreateClubMemberId(player->GetGUID()).release());

    club::v1::client::Club* guildClub = subscribeNotification.mutable_club();

    guildClub->set_id(guild->GetId());
    guildClub->set_allocated_type(CreateGuildClubType().release());
    guildClub->set_name(guild->GetName());

    // These are not related to normal guild functionality so we hardcode them for now.
    guildClub->set_privacy_level(club::v1::PrivacyLevel::PRIVACY_LEVEL_OPEN);
    guildClub->set_visibility_level(club::v1::VISIBILITY_LEVEL_PRIVATE);

    guildClub->set_member_count(guild->GetMembersCount());

    // Set the club leader, guild master in this case.
    club::v1::client::MemberDescription* guildLeaderDescription = guildClub->add_leader();

    guildLeaderDescription->mutable_id()->set_account_id(guildLeader->GetAccountId());
    guildLeaderDescription->mutable_id()->set_unique_id(guildLeader->GetGUID().GetCounter());

    club::v1::client::Member* subscriber = subscribeNotification.mutable_member();

    // The member sending the notification data.
    subscriber->set_allocated_id(ClubMembershipService::CreateClubMemberId(player->GetGUID()).release());

    // Community/Club default roles have slightly different values.
    // Also this is required to set the current leader/guild master symbol in the interface.
    // 1 = Owner, 4 = Member. Once communities are fully implemented these will go into a new database table.
    if (guildMember->IsRank(GuildRankId::GuildMaster))
        subscriber->add_role(AsUnderlyingType(ClubRoleIdentifier::Owner));
    else if (guild->HasAnyRankRight(guildMember->GetRankId(), GuildRankRights(GR_RIGHT_OFFCHATLISTEN | GR_RIGHT_OFFCHATSPEAK)))
        subscriber->add_role(AsUnderlyingType(ClubRoleIdentifier::Moderator));
    else
        subscriber->add_role(AsUnderlyingType(ClubRoleIdentifier::Member));

    subscriber->set_presence_level(club::v1::client::PRESENCE_LEVEL_RICH);
    subscriber->set_whisper_level(club::v1::client::WHISPER_LEVEL_OPEN);

    // Member is online and active.
    subscriber->set_active(true);

    WorldserverService<club::v1::client::ClubListener>(_session).OnSubscribe(&subscribeNotification, true, true);

    // Notify the client about the changed club state.
    club::v1::client::SubscriberStateChangedNotification subscriberStateChangedNotification;

    subscriberStateChangedNotification.set_club_id(guild->GetId());

    club::v1::client::SubscriberStateAssignment* assignment = subscriberStateChangedNotification.add_assignment();

    assignment->set_allocated_member_id(ClubMembershipService::CreateClubMemberId(player->GetGUID()).release());

    // Member is online and active.
    assignment->set_active(true);

    WorldserverService<club::v1::client::ClubListener>(_session).OnSubscriberStateChanged(&subscriberStateChangedNotification, true, true);

    return ERROR_OK;
}

uint32 ClubService::HandleGetMembers(club::v1::client::GetMembersRequest const* /*request*/, club::v1::client::GetMembersResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    Guild const* guild = player->GetGuild();

    if (!guild)
        return ERROR_CLUB_NO_CLUB;

    response->mutable_member()->Reserve(guild->GetMembersCount());

    for (auto const& [guid, member] : guild->GetMembers())
    {
        club::v1::client::Member* clubMember = response->add_member();

        clubMember->set_allocated_id(ClubMembershipService::CreateClubMemberId(guid).release());

        // Community/Club default roles have slightly different values.
        // When communities are implemented those are going to be database fields.
        if (member.IsRank(GuildRankId::GuildMaster))
            clubMember->add_role(AsUnderlyingType(ClubRoleIdentifier::Owner));
        else if (guild->HasAnyRankRight(member.GetRankId(), GuildRankRights(GR_RIGHT_OFFCHATLISTEN | GR_RIGHT_OFFCHATSPEAK)))
            clubMember->add_role(AsUnderlyingType(ClubRoleIdentifier::Moderator));
        else
            clubMember->add_role(AsUnderlyingType(ClubRoleIdentifier::Member));

        clubMember->set_presence_level(club::v1::client::PresenceLevel::PRESENCE_LEVEL_RICH);
        clubMember->set_whisper_level(club::v1::client::WhisperLevel::WHISPER_LEVEL_OPEN);
        std::string_view publicNote = member.GetPublicNote();
        clubMember->set_note(publicNote.data(), publicNote.size());
        clubMember->set_active(member.IsOnline());
    }

    return ERROR_OK;
}

uint32 ClubService::HandleGetStreams(club::v1::client::GetStreamsRequest const* /*request*/, club::v1::client::GetStreamsResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    Guild const* guild = player->GetGuild();

    if (!guild)
        return ERROR_CLUB_NO_CLUB;

    // General guild channel.
    club::v1::client::Stream* generalGuildChannelStream = response->add_stream();

    generalGuildChannelStream->set_club_id(guild->GetId());
    generalGuildChannelStream->set_id(AsUnderlyingType(ClubStreamType::Guild));

    v2::Attribute* generalStreamAttribute = generalGuildChannelStream->add_attribute();

    generalStreamAttribute->set_name("global_strings_tag");
    generalStreamAttribute->mutable_value()->set_string_value("COMMUNITIES_GUILD_GENERAL_CHANNEL_NAME");

    generalGuildChannelStream->set_name("Guild");

    // All roles got access to this channel.
    // Club roles are currently guild role + 1.
    // With a complete club/community system those will be handled differently.
    generalGuildChannelStream->mutable_access()->add_role(AsUnderlyingType(ClubRoleIdentifier::Owner));
    generalGuildChannelStream->mutable_access()->add_role(AsUnderlyingType(ClubRoleIdentifier::Leader));
    generalGuildChannelStream->mutable_access()->add_role(AsUnderlyingType(ClubRoleIdentifier::Moderator));
    generalGuildChannelStream->mutable_access()->add_role(AsUnderlyingType(ClubRoleIdentifier::Member));

    // No voice support.
    generalGuildChannelStream->set_voice_level(club::v1::client::StreamVoiceLevel::VOICE_LEVEL_DISABLED);

    // Officer guild channel.
    club::v1::client::Stream* officerGuildChannelStream = response->add_stream();

    officerGuildChannelStream->set_club_id(guild->GetId());
    officerGuildChannelStream->set_id(AsUnderlyingType(ClubStreamType::Officer));

    v2::Attribute* officerStreamAttribute = officerGuildChannelStream->add_attribute();

    officerStreamAttribute->set_name("global_strings_tag");
    officerStreamAttribute->mutable_value()->set_string_value("COMMUNITIES_GUILD_OFFICER_CHANNEL_NAME");

    officerGuildChannelStream->set_name("Officer");

    // All roles got access to this channel.
    // Club roles are currently guild role + 1.
    // With a complete club/community system those will be handled differently.
    officerGuildChannelStream->mutable_access()->add_role(AsUnderlyingType(ClubRoleIdentifier::Owner));
    officerGuildChannelStream->mutable_access()->add_role(AsUnderlyingType(ClubRoleIdentifier::Leader));
    officerGuildChannelStream->mutable_access()->add_role(AsUnderlyingType(ClubRoleIdentifier::Moderator));

    // No voice support.
    officerGuildChannelStream->set_voice_level(club::v1::client::StreamVoiceLevel::VOICE_LEVEL_DISABLED);

    // Enable channel view.
    //
    // ViewMarker is what drives the unread dot: the client compares last_read_time against
    // last_message_time. Both were left unset until now, so a stream could never be shown as unread.
    // Both are microseconds, the same unit as MessageId.epoch.
    club::v1::client::StreamView* generalView = response->add_view();

    generalView->set_club_id(guild->GetId());
    generalView->set_stream_id(AsUnderlyingType(ClubStreamType::Guild));
    FillStreamViewMarker(generalView->mutable_marker(), guild->GetId(), AsUnderlyingType(ClubStreamType::Guild), player->GetGUID());

    club::v1::client::StreamView* officerView = response->add_view();

    officerView->set_club_id(guild->GetId());
    officerView->set_stream_id(AsUnderlyingType(ClubStreamType::Officer));
    FillStreamViewMarker(officerView->mutable_marker(), guild->GetId(), AsUnderlyingType(ClubStreamType::Officer), player->GetGUID());

    return ERROR_OK;
}

void ClubService::FillStreamViewMarker(ViewMarker* marker, uint64 clubId, uint64 streamId, ObjectGuid member)
{
    marker->set_last_read_time(sClubStreamHistoryMgr->GetStreamViewTime(clubId, streamId, member));
    marker->set_last_message_time(sClubStreamHistoryMgr->GetLastMessageTime(clubId, streamId));
}

uint32 ClubService::HandleSubscribeStream(club::v1::client::SubscribeStreamRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    Guild const* guild = player->GetGuild();

    if (!guild)
        return ERROR_CLUB_NO_CLUB;

    if (request->stream_id().empty())
        return ERROR_CLUB_STREAM_NO_STREAM;

    // GetStreams advertises both the Guild and the Officer stream to every member, so a rank without the
    // officer listen right will still ask to subscribe to both. Failing the whole batch would take guild
    // chat down for ordinary members: subscribe to what the player may read and only error when nothing
    // in the request is readable.
    std::vector<uint64> streams;
    streams.reserve(request->stream_id_size());

    uint32 lastError = ERROR_CLUB_STREAM_NO_STREAM;

    for (uint64 streamId : request->stream_id())
    {
        uint32 errorCode = ERROR_OK;
        if (!ResolveReadableStream(request->club_id(), streamId, errorCode))
        {
            lastError = errorCode;
            continue;
        }

        streams.push_back(streamId);
    }

    if (streams.empty())
        return lastError;

    // Record the subscription. The live fan-out in HandleCreateMessage is deliberately NOT gated on this
    // set - it has always reached every member with the listen right and gating it could silently break
    // guild chat rendering for a client that subscribes differently than we assume. The set is used to
    // decide whether a focused stream counts as actively read.
    sClubStreamHistoryMgr->SetStreamSubscribed(guild->GetId(), player->GetGUID(), streams, true);

    return ERROR_OK;
}

uint32 ClubService::HandleUnsubscribeStream(club::v1::client::UnsubscribeStreamRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    Guild const* guild = player->GetGuild();

    if (!guild)
        return ERROR_CLUB_NO_CLUB;

    std::vector<uint64> streams(request->stream_id().begin(), request->stream_id().end());

    // Leaving a stream also drops its focus: a stream that is no longer subscribed cannot be being read.
    for (uint64 streamId : streams)
        sClubStreamHistoryMgr->SetStreamFocus(guild->GetId(), streamId, player->GetGUID(), false);

    sClubStreamHistoryMgr->SetStreamSubscribed(guild->GetId(), player->GetGUID(), streams, false);

    return ERROR_OK;
}

uint32 ClubService::HandleSetStreamFocus(club::v1::client::SetStreamFocusRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    uint32 errorCode = ERROR_OK;
    uint64 streamId = ResolveReadableStream(request->club_id(), request->stream_id(), errorCode);
    if (!streamId)
        return errorCode;

    Player const* player = _session->GetPlayer();
    Guild const* guild = player->GetGuild();

    sClubStreamHistoryMgr->SetStreamFocus(guild->GetId(), streamId, player->GetGUID(), request->focus());

    // Focusing a stream means the member is looking at it, so everything already in it has been seen.
    // Defocusing leaves the marker where it is; the client sends AdvanceStreamViewTime for that.
    if (request->focus())
        sClubStreamHistoryMgr->AdvanceStreamViewTime(guild->GetId(), streamId, player->GetGUID(),
            sClubStreamHistoryMgr->GetLastMessageTime(guild->GetId(), streamId));

    return ERROR_OK;
}

uint32 ClubService::HandleAdvanceStreamViewTime(club::v1::client::AdvanceStreamViewTimeRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    Guild const* guild = player->GetGuild();

    if (!guild)
        return ERROR_CLUB_NOT_MEMBER;

    // The request carries no time of its own, so "advance" means "up to now". Field 4 is the current
    // repeated stream_id list; field 3 (stream_id_deprecated) is still honoured because a client that
    // only sets the old field would otherwise never clear anything.
    std::vector<uint64> streams(request->stream_id().begin(), request->stream_id().end());
    if (streams.empty() && request->has_stream_id_deprecated())
        streams.push_back(request->stream_id_deprecated());

    if (streams.empty())
        return ERROR_CLUB_STREAM_NO_STREAM;

    uint64 now = uint64(std::chrono::duration_cast<std::chrono::microseconds>(GameTime::GetSystemTime().time_since_epoch()).count());

    // As in HandleSubscribeStream: the client batches both advertised streams, so an unreadable one is
    // skipped rather than failing the whole request.
    uint32 lastError = ERROR_CLUB_STREAM_NO_STREAM;
    bool advancedAny = false;

    for (uint64 streamId : streams)
    {
        uint32 errorCode = ERROR_OK;
        if (!ResolveReadableStream(request->club_id(), streamId, errorCode))
        {
            lastError = errorCode;
            continue;
        }

        sClubStreamHistoryMgr->AdvanceStreamViewTime(guild->GetId(), streamId, player->GetGUID(), now);
        advancedAny = true;
    }

    return advancedAny ? uint32(ERROR_OK) : lastError;
}

uint32 ClubService::HandleGetStreamHistory(club::v1::client::GetStreamHistoryRequest const* request, club::v1::client::GetStreamHistoryResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    uint32 errorCode = ERROR_OK;
    uint64 streamId = ResolveReadableStream(request->club_id(), request->stream_id(), errorCode);
    if (!streamId)
        return errorCode;

    Guild const* guild = _session->GetPlayer()->GetGuild();

    // bgs.protocol.GetEventOptions: fetch_from / fetch_until are bounds on the event time (here
    // MessageId.epoch, microseconds), max_events caps the page and order picks the direction. Unset
    // bounds mean "unbounded"; the store normalises the pair so an inverted range still works.
    uint64 fetchFrom = 0;
    uint64 fetchUntil = std::numeric_limits<uint64>::max();
    uint32 maxEvents = sClubStreamHistoryMgr->GetMaxMessagesPerStream();
    bool ascending = false;    // EventOrder default is EVENT_DESCENDING (0) - newest first.

    if (request->has_options())
    {
        GetEventOptions const& options = request->options();

        if (options.has_fetch_from())
            fetchFrom = options.fetch_from();

        if (options.has_fetch_until())
            fetchUntil = options.fetch_until();

        if (options.has_max_events() && options.max_events())
            maxEvents = std::min(options.max_events(), sClubStreamHistoryMgr->GetMaxMessagesPerStream());

        ascending = options.order() == EVENT_ASCENDING;
    }

    std::vector<ClubStreamMessage const*> page = sClubStreamHistoryMgr->GetHistory(guild->GetId(), streamId,
        fetchFrom, fetchUntil, maxEvents, ascending);

    response->mutable_message()->Reserve(int(page.size()));

    for (ClubStreamMessage const* message : page)
        FillStreamMessage(response->add_message(), *message);

    // GetStreamHistoryResponse.continuation is deliberately left unset: GetStreamHistoryRequest has no
    // field to echo a continuation token back into (only agent_id, club_id, stream_id and options), so
    // what the client would do with one cannot be determined from the generated protobuf. Paging
    // backwards works through options.fetch_from / fetch_until, which the client does have.

    return ERROR_OK;
}

uint32 ClubService::HandleCreateMessage(club::v1::client::CreateMessageRequest const* request, club::v1::client::CreateMessageResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& continuation)
{
    // Basic sanity check until full communities are implemented.
    // 1 - Guild, 2 - Officer chat stream.
    if (request->stream_id() != AsUnderlyingType(ClubStreamType::Guild) && request->stream_id() != AsUnderlyingType(ClubStreamType::Officer))
        return ERROR_CLUB_STREAM_NO_STREAM;

    // Just some sanity checks. We do not care about the requested stream for now since we only have two.
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    Guild const* guild = player->GetGuild();

    if (!guild)
        return ERROR_CLUB_NO_CLUB;

    GuildRankRights requiredRights = { };
    ChatMessageResult result = { };

    switch (ClubStreamType(request->stream_id()))
    {
        case ClubStreamType::Guild:
            requiredRights = GR_RIGHT_GCHATLISTEN;
            result = _session->HandleChatMessage(CHAT_MSG_GUILD, LANG_UNIVERSAL, request->options().content());
            break;
        case ClubStreamType::Officer:
            requiredRights = GR_RIGHT_OFFCHATLISTEN;
            result = _session->HandleChatMessage(CHAT_MSG_OFFICER, LANG_UNIVERSAL, request->options().content());
            break;
        default:
            return ERROR_CLUB_STREAM_NO_STREAM;
    }

    if (result == ChatMessageResult::Ok)
    {
        // HandleChatMessage has already reached Guild::BroadcastToGuild, which is where the message was
        // persisted and given its MessageId. Echoing that identifier - instead of minting a second,
        // different one here - is what lets the client match a live message against the same message when
        // it later pages through GetStreamHistory.
        ClubStreamMessage const* stored = sClubStreamHistoryMgr->GetLatestMessage(guild->GetId(), request->stream_id());

        // BroadcastToGuild drops the line when the sender lacks the speak right, and history can be
        // turned off outright, so confirm the newest stored message really is this one before echoing it.
        if (stored && (stored->AuthorGuid != player->GetGUID() || stored->Content != request->options().content()))
            stored = nullptr;

        std::chrono::microseconds messageTime = std::chrono::duration_cast<std::chrono::microseconds>(GameTime::GetSystemTime().time_since_epoch());

        if (stored)
            FillStreamMessage(response->mutable_message(), *stored);
        else
            FillStreamMessage(response->mutable_message(), request->options().content(), messageTime, player->GetGUID());

        club::v1::client::StreamMessageAddedNotification messageAddedNotification;
        messageAddedNotification.set_allocated_agent_id(ClubMembershipService::CreateClubMemberId(player->GetGUID()).release());
        messageAddedNotification.set_club_id(guild->GetId());
        messageAddedNotification.set_stream_id(request->stream_id());

        if (stored)
            FillStreamMessage(messageAddedNotification.mutable_message(), *stored);
        else
            FillStreamMessage(messageAddedNotification.mutable_message(), request->options().content(), messageTime, player->GetGUID());

        guild->BroadcastWorker([&](Player const* receiver)
        {
            Guild::Member const* receiverMember = guild->GetMember(receiver->GetGUID());
            if (!guild->HasAnyRankRight(receiverMember->GetRankId(), requiredRights))
                return;

            if (receiver->GetSocial()->HasIgnore(player->GetGUID(), _session->GetAccountGUID()))
                return;

            WorldserverService<club::v1::client::ClubListener>(receiver->GetSession()).OnStreamMessageAdded(&messageAddedNotification, true, true);
        }, player);

        return ERROR_OK;
    }

    // If the message is empty there should never be a response to message request.
    continuation = nullptr;

    return ERROR_CLUB_STREAM_NO_SUCH_MESSAGE;
}

std::unique_ptr<club::v1::UniqueClubType> ClubService::CreateGuildClubType()
{
    std::unique_ptr<club::v1::UniqueClubType> type = std::make_unique<club::v1::UniqueClubType>();
    type->set_program(5730135);
    type->set_name("guild");
    return type;
}

void ClubService::FillStreamMessage(club::v1::client::StreamMessage* message, std::string_view msg, std::chrono::microseconds messageTime, ObjectGuid author)
{
    message->mutable_id()->set_epoch(messageTime.count());
    message->mutable_id()->set_position(0);

    message->mutable_author()->set_allocated_id(ClubMembershipService::CreateClubMemberId(author).release());

    club::v1::client::ContentChain* contentChain = message->add_content_chain();

    contentChain->set_content(msg.data(), msg.size());
    contentChain->set_edit_time(messageTime.count());
}

void ClubService::FillStreamMessage(club::v1::client::StreamMessage* message, ClubStreamMessage const& stored)
{
    // MessageId.position now carries the stored per-stream sequence instead of a constant 0, so two
    // messages sent inside the same microsecond are no longer the same message as far as the client
    // is concerned.
    message->mutable_id()->set_epoch(stored.Epoch);
    message->mutable_id()->set_position(stored.Position);

    club::v1::MemberId* authorId = message->mutable_author()->mutable_id();
    authorId->set_account_id(stored.AuthorAccountId);
    authorId->set_unique_id(Clubs::CreateClubMemberId(stored.AuthorGuid));

    club::v1::client::ContentChain* contentChain = message->add_content_chain();

    contentChain->set_content(stored.Content);
    contentChain->set_edit_time(stored.Epoch);
}
}
