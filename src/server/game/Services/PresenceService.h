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

#ifndef TRINITYCORE_PRESENCE_SERVICE_H
#define TRINITYCORE_PRESENCE_SERVICE_H

#include "WorldserverService.h"
#include "Client/presence_service.pb.h"
#include "Client/api/client/v2/presence_service.pb.h"

namespace Battlenet::Services
{
// bgs.protocol.presence.v2.client.PresenceService (OriginalHash 0x138D200C).
//
// Reached over the game socket tunnel: CMSG_BATTLENET_REQUEST (0x400124) -> WorldSession::
// HandleBattlenetRequest -> Battlenet::WorldserverServiceDispatcher, answered with
// SMSG_BATTLENET_RESPONSE (0x4202AD) and pushed with SMSG_BATTLENET_NOTIFICATION (0x4202AE).
// Before this class existed the service was a bare WorldserverService<T> transport wrapper, so all
// four methods returned ERROR_RPC_NOT_IMPLEMENTED after logging at TC_LOG_ERROR.
//
// All four generated server methods are overridden. State lives in BnetPresenceMgr.
class PresenceService : public WorldserverService<presence::v2::client::PresenceService>
{
    typedef WorldserverService<presence::v2::client::PresenceService> BaseService;

public:
    PresenceService(WorldSession* session);

    uint32 HandleBatchSubscribe(presence::v2::client::BatchSubscribeRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleBatchUnsubscribe(presence::v2::client::BatchUnsubscribeRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleQuery(presence::v2::client::QueryRequest const* request, presence::v2::client::QueryResponse* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleUpdate(presence::v2::client::UpdateRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;

    // ---- wire fill helpers, shared with BnetPresenceMgr's push path ---------------------------
    // Kept here rather than in BnetPresenceMgr.h so the manager header stays free of protobuf, the
    // same split BnetFriendsMgr / FriendsService already use.

    // Appends one PresenceFieldState per online game account of `targetAccountId`, or a single
    // account-scope state with no game_account attached when the account is offline.
    //
    // `fields` are only ever the ones the client itself authored through Update. No server-derived
    // value (online flag, character, zone) is serialised, because presence.v2 has no typed slot for
    // any of them - every value is a Variant behind a PresenceFieldKey{title_id, group, field,
    // unique_id} whose numbering is assigned by Blizzard and is not derivable offline.
    //
    // `keyFilter` honours QueryRequest.keys (nullptr or empty = every field), `sinceUs` honours
    // QueryRequest.since_us.
    static void FillAccountStates(::google::protobuf::RepeatedPtrField<presence::v2::PresenceFieldState>* out,
        uint32 targetAccountId, bool includeGameAccounts = true,
        ::google::protobuf::RepeatedPtrField<presence::v2::PresenceFieldKey> const* keyFilter = nullptr,
        uint64 sinceUs = 0);
};

// bgs.protocol.presence.v1.PresenceService (OriginalHash 0xFA0796FF).
//
// The 68275 client drives v2, exactly as it does for friends, but v1 is registered in both
// dispatchers and a v1 call used to be answered ERROR_RPC_NOT_IMPLEMENTED, so it is implemented
// against the same BnetPresenceMgr state.
//
// All six generated server methods are overridden.
class PresenceServiceV1 : public WorldserverService<presence::v1::PresenceService>
{
    typedef WorldserverService<presence::v1::PresenceService> BaseService;

public:
    PresenceServiceV1(WorldSession* session);

    uint32 HandleSubscribe(presence::v1::SubscribeRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleUnsubscribe(presence::v1::UnsubscribeRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleUpdate(presence::v1::UpdateRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleQuery(presence::v1::QueryRequest const* request, presence::v1::QueryResponse* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleBatchSubscribe(presence::v1::BatchSubscribeRequest const* request, presence::v1::BatchSubscribeResponse* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleBatchUnsubscribe(presence::v1::BatchUnsubscribeRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;

    // ---- wire fill helpers -------------------------------------------------------------------

    // `entity_id` is echoed back exactly as the subscriber sent it; only `low` is ever interpreted,
    // as the battlenet account id, which is the form this project's own bnetserver puts on the wire
    // in authentication::v1::LogonResult.account_id (high 0x0100000000000000, low = account id).
    static void FillPresenceState(presence::v1::PresenceState* out, uint32 targetAccountId, uint64 entityHigh, uint64 entityLow);
    static void FillFields(::google::protobuf::RepeatedPtrField<presence::v1::Field>* out, uint32 targetAccountId,
        ::google::protobuf::RepeatedPtrField<presence::v1::FieldKey> const* keyFilter = nullptr);
};
}

#endif // TRINITYCORE_PRESENCE_SERVICE_H
