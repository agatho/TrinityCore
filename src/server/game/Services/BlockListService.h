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

#ifndef TRINITYCORE_BLOCK_LIST_SERVICE_H
#define TRINITYCORE_BLOCK_LIST_SERVICE_H

#include "WorldserverService.h"
#include "Client/api/client/v1/block_list_service.pb.h"

namespace Battlenet::Services
{
// bgs.protocol.block_list.v1.client.BlockListService (OriginalHash 0x8E8F5FB0).
//
// Account-scope ignore: one battlenet account blocks another. State lives in BnetBlockListMgr and is
// durable in `battlenet_account_blocked`. Before this class existed the service was a bare
// WorldserverService<T> transport wrapper and every method returned ERROR_RPC_NOT_IMPLEMENTED.
//
// Six of the seven generated methods are overridden. HandlePassExternalBlockList is deliberately NOT
// overridden: it carries `blocked_external_ids`, opaque identity strings minted by a non-Battle.net
// platform (console / storefront account ids). This server has no mapping from those strings to
// anything, so any handling would be either a silent no-op or an invented one. It keeps the generated
// ERROR_RPC_NOT_IMPLEMENTED, which is the truthful answer.
class BlockListService : public WorldserverService<block_list::v1::client::BlockListService>
{
    typedef WorldserverService<block_list::v1::client::BlockListService> BaseService;

public:
    BlockListService(WorldSession* session);

    uint32 HandleSubscribe(block_list::v1::client::SubscribeRequest const* request, block_list::v1::client::SubscribeResponse* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleUnsubscribe(block_list::v1::client::UnsubscribeRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleGetState(block_list::v1::client::GetStateRequest const* request, block_list::v1::client::GetStateResponse* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleBlockPlayer(block_list::v1::client::BlockPlayerRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleUnblockPlayer(block_list::v1::client::UnblockPlayerRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    uint32 HandleBlockPlayerForSession(block_list::v1::client::BlockPlayerRequest const* request, NoData* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;

    // ---- wire fill helper --------------------------------------------------------------------
    static void FillBlockListState(block_list::v1::client::BlockListState* out, uint32 bnetAccountId);
};
}

#endif // TRINITYCORE_BLOCK_LIST_SERVICE_H
