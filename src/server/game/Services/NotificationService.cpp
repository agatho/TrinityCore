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

#include "NotificationService.h"
#include "BattlenetRpcErrorCodes.h"
#include "BnetBlockListMgr.h"
#include "BnetFriendsMgr.h"
#include "BnetPresenceMgr.h"
#include "GameTime.h"
#include "Log.h"
#include "World.h"
#include "Client/api/client/v2/notification_listener.pb.h"
#include "Client/api/client/v2/notification_types.pb.h"
#include "Client/notification_types.pb.h"
#include <algorithm>
#include <unordered_map>

namespace Battlenet::Services
{
namespace
{
    // notification.v1 subscriptions: watched battlenet account id -> subscriber game account ids.
    // File scope rather than a manager because nothing outside this relay ever reads it.
    std::unordered_map<uint32, std::vector<uint32>> _v1Subscriptions;

    std::vector<WorldSession*> ResolveTargetSessions(uint32 targetAccountId, uint32 targetGameAccountId)
    {
        std::vector<WorldSession*> sessions = sBnetFriendsMgr->GetOnlineSessions(targetAccountId);

        // notification.v2 SendNotificationOptions.filter.game_account narrows delivery to one game
        // account of the target.
        if (targetGameAccountId)
            std::erase_if(sessions, [targetGameAccountId](WorldSession* session) { return session->GetAccountId() != targetGameAccountId; });

        return sessions;
    }
}

// =============================================================================================
// notification.v2
// =============================================================================================

NotificationService::NotificationService(WorldSession* session) : BaseService(session) { }

uint32 NotificationService::Route(uint32 senderAccountId, uint32 targetAccountId, uint32 targetGameAccountId,
    notification::v2::client::Notification const& notification)
{
    if (!targetAccountId)
        return ERROR_INVALID_TARGET_ID;

    // A blocked account must not be able to raise a toast on the blocker's screen.
    if (senderAccountId && sBnetBlockListMgr->IsBlockedEitherWay(senderAccountId, targetAccountId))
        return ERROR_TARGET_IS_BLOCKING_AGENT;

    std::vector<WorldSession*> sessions = ResolveTargetSessions(targetAccountId, targetGameAccountId);
    if (sessions.empty())
    {
        // Notifications are transient and this server keeps no offline queue, so saying ERROR_OK here
        // would be claiming a delivery that never happened.
        TC_LOG_DEBUG("session.rpc", "notification.v2: target battlenet account {} has no session on this worldserver, notification '{}' not delivered",
            targetAccountId, notification.type());
        return ERROR_RPC_PEER_UNAVAILABLE;
    }

    notification::v2::client::NotificationReceivedNotification wire;
    wire.add_notifications()->CopyFrom(notification);

    for (WorldSession* session : sessions)
        WorldserverService<notification::v2::client::NotificationListener>(session).OnNotificationReceived(&wire, true, true);

    return ERROR_OK;
}

uint32 NotificationService::SendToAccount(uint32 senderAccountId, uint32 targetAccountId, std::string_view type,
    std::vector<std::pair<std::string, std::string>> const& stringAttributes)
{
    notification::v2::client::Notification notification;
    notification.set_type(std::string(type));
    if (senderAccountId)
        notification.mutable_sender()->set_account_id(senderAccountId);
    notification.mutable_target()->set_account_id(targetAccountId);
    notification.set_creation_time_ms(uint64(GameTime::GetGameTime()) * 1000ull);

    for (auto const& [name, value] : stringAttributes)
    {
        v2::Attribute* attribute = notification.add_attribute();
        attribute->set_name(name);
        attribute->mutable_value()->set_string_value(value);
    }

    return Route(senderAccountId, targetAccountId, 0, notification);
}

uint32 NotificationService::HandleSendNotification(notification::v2::client::SendNotificationRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    uint32 senderAccountId = _session->GetBattlenetAccountId();
    if (!senderAccountId)
        return ERROR_INVALID_AGENT_ID;

    if (!request->has_options())
        return ERROR_RPC_MALFORMED_REQUEST;

    notification::v2::client::SendNotificationOptions const& options = request->options();
    if (!options.has_type() || options.type().empty())
        return ERROR_NOTIFICATION_INVALID_NOTIFICATION_TYPE;

    uint32 targetAccountId = uint32(options.target_account_id());
    if (!targetAccountId)
        return ERROR_INVALID_TARGET_ID;

    uint32 targetGameAccountId = 0;
    if (options.has_filter() && options.filter().has_game_account())
        targetGameAccountId = uint32(options.filter().game_account().id());

    notification::v2::client::Notification notification;
    // `type` and `attribute` are opaque, client-defined payloads and are relayed byte for byte.
    notification.set_type(options.type());
    notification.mutable_attribute()->CopyFrom(options.attribute());
    notification.mutable_sender()->set_account_id(senderAccountId);
    notification.mutable_target()->set_account_id(targetAccountId);
    notification.set_creation_time_ms(uint64(GameTime::GetGameTime()) * 1000ull);

    return Route(senderAccountId, targetAccountId, targetGameAccountId, notification);
}

// =============================================================================================
// notification.v1
// =============================================================================================

NotificationServiceV1::NotificationServiceV1(WorldSession* session) : BaseService(session) { }

namespace
{
    // notification.v1 Target -> battlenet account id. Target.identity carries a typed AccountId, so
    // unlike the presence entity ids there is nothing to decode here.
    uint32 ResolveV1Target(notification::v1::Target const& target)
    {
        if (!target.has_identity())
            return 0;

        notification::v1::TargetIdentity const& identity = target.identity();
        if (identity.has_account())
            return identity.account().id();

        if (identity.has_game_account())
            if (BnetGameAccountPresence const* presence = sBnetPresenceMgr->GetGameAccountPresence(identity.game_account().id()))
                return presence->BnetAccountId;

        return 0;
    }

    uint32 DeliverV1(uint32 senderAccountId, uint32 targetAccountId, notification::v1::Notification const& notification,
        std::vector<WorldSession*> const& sessions)
    {
        if (sessions.empty())
        {
            TC_LOG_DEBUG("session.rpc", "notification.v1: no live session for battlenet account {}, notification '{}' not delivered",
                targetAccountId, notification.type());
            return ERROR_RPC_PEER_UNAVAILABLE;
        }

        for (WorldSession* session : sessions)
        {
            if (senderAccountId && sBnetBlockListMgr->IsBlockedEitherWay(senderAccountId, session->GetBattlenetAccountId()))
                continue;

            WorldserverService<notification::v1::NotificationListener>(session).OnNotificationReceived(&notification, true, true);
        }

        return ERROR_OK;
    }
}

uint32 NotificationServiceV1::HandleSendNotification(notification::v1::Notification const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    uint32 senderAccountId = _session->GetBattlenetAccountId();
    if (!senderAccountId)
        return ERROR_INVALID_AGENT_ID;

    if (!request->has_type() || request->type().empty())
        return ERROR_NOTIFICATION_INVALID_NOTIFICATION_TYPE;

    uint32 targetAccountId = 0;
    if (request->has_target_account_id())
        targetAccountId = sBnetPresenceMgr->ResolveAccountIdFromEntity(request->target_account_id().high(), request->target_account_id().low());
    else if (request->has_target_id())
        targetAccountId = sBnetPresenceMgr->ResolveAccountIdFromEntity(request->target_id().high(), request->target_id().low());

    if (!targetAccountId)
        return ERROR_INVALID_TARGET_ID;

    if (sBnetBlockListMgr->IsBlockedEitherWay(senderAccountId, targetAccountId))
        return ERROR_TARGET_IS_BLOCKING_AGENT;

    // Relayed verbatim, with only the sender identity stamped on - a client must not be able to
    // claim to be somebody else.
    notification::v1::Notification notification;
    notification.CopyFrom(*request);
    notification.mutable_sender_account_id()->set_high(BnetPresenceMgr::AccountEntityHigh);
    notification.mutable_sender_account_id()->set_low(senderAccountId);
    if (BnetAccountIdentity const* identity = sBnetFriendsMgr->GetIdentity(senderAccountId); identity && identity->HasBattleTag())
        notification.set_sender_battle_tag(identity->GetBattleTag());

    return DeliverV1(senderAccountId, targetAccountId, notification, sBnetFriendsMgr->GetOnlineSessions(targetAccountId));
}

uint32 NotificationServiceV1::HandleSubscribe(notification::v1::SubscribeRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    uint32 subscriberAccountId = _session->GetBattlenetAccountId();
    if (!subscriberAccountId)
        return ERROR_INVALID_AGENT_ID;

    if (!request->has_subscription())
        return ERROR_RPC_MALFORMED_REQUEST;

    for (notification::v1::Target const& target : request->subscription().target())
    {
        uint32 targetAccountId = ResolveV1Target(target);
        if (!targetAccountId)
            continue;

        if (sBnetBlockListMgr->IsBlockedEitherWay(subscriberAccountId, targetAccountId))
            continue;

        std::vector<uint32>& subscribers = _v1Subscriptions[targetAccountId];
        if (std::ranges::find(subscribers, _session->GetAccountId()) == subscribers.end())
            subscribers.push_back(_session->GetAccountId());
    }

    return ERROR_OK;
}

uint32 NotificationServiceV1::HandleUnsubscribe(notification::v1::UnsubscribeRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    if (!request->has_subscription())
        return ERROR_RPC_MALFORMED_REQUEST;

    for (notification::v1::Target const& target : request->subscription().target())
    {
        uint32 targetAccountId = ResolveV1Target(target);
        if (!targetAccountId)
            continue;

        auto itr = _v1Subscriptions.find(targetAccountId);
        if (itr == _v1Subscriptions.end())
            continue;

        std::erase(itr->second, _session->GetAccountId());
        if (itr->second.empty())
            _v1Subscriptions.erase(itr);
    }

    return ERROR_OK;
}

uint32 NotificationServiceV1::HandlePublish(notification::v1::PublishRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    uint32 senderAccountId = _session->GetBattlenetAccountId();
    if (!senderAccountId)
        return ERROR_INVALID_AGENT_ID;

    if (!request->has_target() || !request->has_notification())
        return ERROR_RPC_MALFORMED_REQUEST;

    uint32 targetAccountId = ResolveV1Target(request->target());
    if (!targetAccountId)
        return ERROR_INVALID_TARGET_ID;

    notification::v1::Notification notification;
    notification.CopyFrom(request->notification());
    notification.mutable_sender_account_id()->set_high(BnetPresenceMgr::AccountEntityHigh);
    notification.mutable_sender_account_id()->set_low(senderAccountId);
    if (BnetAccountIdentity const* identity = sBnetFriendsMgr->GetIdentity(senderAccountId); identity && identity->HasBattleTag())
        notification.set_sender_battle_tag(identity->GetBattleTag());

    // Publish goes to whoever subscribed to the target, not to the target itself.
    std::vector<WorldSession*> sessions;
    if (auto itr = _v1Subscriptions.find(targetAccountId); itr != _v1Subscriptions.end())
    {
        std::erase_if(itr->second, [&](uint32 gameAccountId)
        {
            WorldSession* session = sWorld->FindSession(gameAccountId);
            if (!session || !session->GetBattlenetAccountId())
                return true;

            sessions.push_back(session);
            return false;
        });

        if (itr->second.empty())
            _v1Subscriptions.erase(itr);
    }

    return DeliverV1(senderAccountId, targetAccountId, notification, sessions);
}

void NotificationServiceV1::OnSessionClosed(WorldSession* session)
{
    uint32 gameAccountId = session->GetAccountId();

    for (auto itr = _v1Subscriptions.begin(); itr != _v1Subscriptions.end(); )
    {
        std::erase(itr->second, gameAccountId);
        itr = itr->second.empty() ? _v1Subscriptions.erase(itr) : std::next(itr);
    }
}
}
