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

#include "BlockListService.h"
#include "BattlenetRpcErrorCodes.h"
#include "BnetBlockListMgr.h"
#include "BnetPresenceMgr.h"
#include "Client/api/client/v1/block_list_types.pb.h"

namespace Battlenet::Services
{
BlockListService::BlockListService(WorldSession* session) : BaseService(session) { }

void BlockListService::FillBlockListState(block_list::v1::client::BlockListState* out, uint32 bnetAccountId)
{
    for (BnetBlockedPlayer const& blocked : sBnetBlockListMgr->GetBlockList(bnetAccountId))
    {
        block_list::v1::client::BlockedPlayer* wire = out->add_player();
        wire->set_id(blocked.AccountId);
        if (!blocked.BattleTag.empty())
            wire->set_battle_tag(blocked.BattleTag);
        wire->set_creation_time_us(uint64(blocked.CreationTime) * 1000000ull);
        wire->set_modified_time_us(uint64(blocked.ModifiedTime) * 1000000ull);
    }
}

uint32 BlockListService::HandleSubscribe(block_list::v1::client::SubscribeRequest const* /*request*/, block_list::v1::client::SubscribeResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    uint32 accountId = _session->GetBattlenetAccountId();
    if (!accountId)
        return ERROR_INVALID_AGENT_ID;

    sBnetBlockListMgr->Subscribe(_session);

    // Unlike friends.v2, the block list Subscribe response does carry the full state, so the initial
    // list does not have to arrive as a notification.
    FillBlockListState(response->mutable_state(), accountId);
    return ERROR_OK;
}

uint32 BlockListService::HandleUnsubscribe(block_list::v1::client::UnsubscribeRequest const* /*request*/, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    sBnetBlockListMgr->Unsubscribe(_session);
    return ERROR_OK;
}

uint32 BlockListService::HandleGetState(block_list::v1::client::GetStateRequest const* /*request*/, block_list::v1::client::GetStateResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    uint32 accountId = _session->GetBattlenetAccountId();
    if (!accountId)
        return ERROR_INVALID_AGENT_ID;

    FillBlockListState(response->mutable_state(), accountId);
    return ERROR_OK;
}

uint32 BlockListService::HandleBlockPlayer(block_list::v1::client::BlockPlayerRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    uint32 accountId = _session->GetBattlenetAccountId();
    if (!accountId)
        return ERROR_INVALID_AGENT_ID;

    if (!request->has_options() || !request->options().has_account_id())
        return ERROR_RPC_MALFORMED_REQUEST;

    uint32 targetAccountId = uint32(request->options().account_id());

    uint32 result = sBnetBlockListMgr->BlockPlayer(accountId, targetAccountId, false);
    if (result != ERROR_OK)
        return result;

    // A block severs presence in both directions immediately; leaving the old subscription in place
    // would keep leaking state until the client happened to unsubscribe.
    sBnetPresenceMgr->Unsubscribe(_session, targetAccountId);
    return ERROR_OK;
}

uint32 BlockListService::HandleUnblockPlayer(block_list::v1::client::UnblockPlayerRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    uint32 accountId = _session->GetBattlenetAccountId();
    if (!accountId)
        return ERROR_INVALID_AGENT_ID;

    if (!request->has_options() || !request->options().has_account_id())
        return ERROR_RPC_MALFORMED_REQUEST;

    return sBnetBlockListMgr->UnblockPlayer(accountId, uint32(request->options().account_id()));
}

uint32 BlockListService::HandleBlockPlayerForSession(block_list::v1::client::BlockPlayerRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    uint32 accountId = _session->GetBattlenetAccountId();
    if (!accountId)
        return ERROR_INVALID_AGENT_ID;

    if (!request->has_options() || !request->options().has_account_id())
        return ERROR_RPC_MALFORMED_REQUEST;

    uint32 targetAccountId = uint32(request->options().account_id());

    // Same effect as BlockPlayer for as long as the account stays connected, but never persisted.
    uint32 result = sBnetBlockListMgr->BlockPlayer(accountId, targetAccountId, true);
    if (result != ERROR_OK)
        return result;

    sBnetPresenceMgr->Unsubscribe(_session, targetAccountId);
    return ERROR_OK;
}
}
