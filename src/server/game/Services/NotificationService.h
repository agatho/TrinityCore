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

#ifndef TRINITYCORE_NOTIFICATION_SERVICE_H
#define TRINITYCORE_NOTIFICATION_SERVICE_H

#include "WorldserverService.h"
#include "Client/notification_service.pb.h"
#include "Client/api/client/v2/notification_service.pb.h"
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Battlenet::Services
{
// bgs.protocol.notification.v2.client.NotificationService (OriginalHash 0xF8E1EB98).
//
// A relay, nothing more: resolve the target battlenet account -> its live WorldSessions -> push
// notification::v2::client::NotificationListener::OnNotificationReceived down the game socket tunnel
// (SMSG_BATTLENET_NOTIFICATION 0x4202AE). Before this class existed the service was a bare
// WorldserverService<T> transport wrapper and SendNotification returned ERROR_RPC_NOT_IMPLEMENTED.
//
// Everything the sender supplies - the notification `type` string and the whole `attribute` list -
// is relayed verbatim. Those are opaque, client-defined payloads; rewriting or synthesising them
// would be inventing semantics.
class NotificationService : public WorldserverService<notification::v2::client::NotificationService>
{
    typedef WorldserverService<notification::v2::client::NotificationService> BaseService;

public:
    NotificationService(WorldSession* session);

    uint32 HandleSendNotification(notification::v2::client::SendNotificationRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;

    // ---- server-side entry points ------------------------------------------------------------

    // Delivers `notification` to every live session of `targetAccountId`, or only to the session of
    // `targetGameAccountId` when that is non-zero (notification.v2 SendNotificationOptions.filter
    // .game_account). Returns a BattlenetRpcErrorCodes value; ERROR_RPC_PEER_UNAVAILABLE when the
    // target account has no session on this worldserver, because notifications are transient and
    // there is no offline queue to accept them.
    static uint32 Route(uint32 senderAccountId, uint32 targetAccountId, uint32 targetGameAccountId,
        notification::v2::client::Notification const& notification);

    // Convenience for server-side producers that only need string attributes.
    static uint32 SendToAccount(uint32 senderAccountId, uint32 targetAccountId, std::string_view type,
        std::vector<std::pair<std::string, std::string>> const& stringAttributes = { });
};

// bgs.protocol.notification.v1.NotificationService (OriginalHash 0x0CBE3C43).
//
// Two delivery modes, both real:
//   SendNotification - point to point, addressed by Notification.target_id / .target_account_id.
//   Subscribe / Unsubscribe / Publish - a client registers interest in a target entity and every
//   Publish against that target reaches the registered sessions.
//
// Subscriptions are held as game account ids, never as WorldSession*, so a session that drops
// without unsubscribing cannot leave a dangling pointer behind.
class NotificationServiceV1 : public WorldserverService<notification::v1::NotificationService>
{
    typedef WorldserverService<notification::v1::NotificationService> BaseService;

public:
    NotificationServiceV1(WorldSession* session);

    uint32 HandleSendNotification(notification::v1::Notification const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleSubscribe(notification::v1::SubscribeRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleUnsubscribe(notification::v1::UnsubscribeRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandlePublish(notification::v1::PublishRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;

    // Drops every v1 subscription owned by a session that is going away.
    static void OnSessionClosed(WorldSession* session);
};
}

#endif // TRINITYCORE_NOTIFICATION_SERVICE_H
