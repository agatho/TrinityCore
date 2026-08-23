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

#include "PresenceService.h"
#include "BattlenetRpcErrorCodes.h"
#include "BnetBlockListMgr.h"
#include "BnetPresenceMgr.h"
#include "Log.h"
#include "Client/api/client/v2/presence_listener.pb.h"
#include "Client/api/client/v2/presence_types.pb.h"
#include "Client/api/common/v2/game_account_handle.pb.h"
#include "Client/presence_types.pb.h"

namespace Battlenet::Services
{
namespace
{
    uint32 ResolveBnetAccountId(bgs::protocol::EntityId const& entityId)
    {
        return sBnetPresenceMgr->ResolveAccountIdFromEntity(entityId.high(), entityId.low());
    }

    bool MatchesKeyFilterV2(::google::protobuf::RepeatedPtrField<bgs::protocol::presence::v2::PresenceFieldKey> const* filter,
        BnetPresenceField const& field)
    {
        if (!filter || filter->empty())
            return true;

        for (bgs::protocol::presence::v2::PresenceFieldKey const& key : *filter)
            if (key.title_id() == field.Program && key.group() == field.Group && key.field() == field.Field
                && (!key.has_unique_id() || key.unique_id() == field.UniqueId))
                return true;

        return false;
    }

    bool MatchesKeyFilterV1(::google::protobuf::RepeatedPtrField<bgs::protocol::presence::v1::FieldKey> const* filter,
        BnetPresenceField const& field)
    {
        if (!filter || filter->empty())
            return true;

        for (bgs::protocol::presence::v1::FieldKey const& key : *filter)
            if (key.program() == field.Program && key.group() == field.Group && key.field() == field.Field
                && (!key.has_unique_id() || key.unique_id() == field.UniqueId))
                return true;

        return false;
    }
}

// =============================================================================================
// presence.v2
// =============================================================================================

PresenceService::PresenceService(WorldSession* session) : BaseService(session) { }

void PresenceService::FillAccountStates(::google::protobuf::RepeatedPtrField<presence::v2::PresenceFieldState>* out,
    uint32 targetAccountId, bool includeGameAccounts,
    ::google::protobuf::RepeatedPtrField<presence::v2::PresenceFieldKey> const* keyFilter, uint64 sinceUs)
{
    std::vector<BnetPresenceField> const& fields = sBnetPresenceMgr->GetRichFields(targetAccountId, true);

    auto fillFields = [&](presence::v2::PresenceFieldState* state)
    {
        uint64 oldest = 0;
        for (BnetPresenceField const& field : fields)
        {
            if (sinceUs && field.UpdatedTimeUs <= sinceUs)
                continue;

            if (!MatchesKeyFilterV2(keyFilter, field))
                continue;

            presence::v2::PresenceField* wire = state->add_fields();
            presence::v2::PresenceFieldKey* key = wire->mutable_key();
            key->set_title_id(field.Program);
            key->set_group(field.Group);
            key->set_field(field.Field);
            if (field.UniqueId)
                key->set_unique_id(field.UniqueId);

            // The Variant arrived from the client and is stored serialised; it goes back out byte for byte.
            if (!field.VariantBlob.empty())
                wire->mutable_value()->ParseFromString(field.VariantBlob);

            wire->set_updated_time_us(field.UpdatedTimeUs);

            if (!oldest || field.UpdatedTimeUs < oldest)
                oldest = field.UpdatedTimeUs;
        }

        if (oldest)
            state->set_oldest_time_us(oldest);
    };

    std::vector<BnetGameAccountPresence const*> online;
    if (includeGameAccounts)
        online = sBnetPresenceMgr->GetAccountPresence(targetAccountId);

    if (online.empty())
    {
        // Offline, or an account-scope-only query: one state with no game_account attached.
        presence::v2::PresenceFieldState* state = out->Add();
        state->set_account_id(targetAccountId);
        fillFields(state);
        return;
    }

    for (BnetGameAccountPresence const* presence : online)
    {
        presence::v2::PresenceFieldState* state = out->Add();
        state->set_account_id(targetAccountId);

        // GameAccountHandle.title_id / .region use the same values this project's bnetserver already
        // puts in the game-account EntityId it sends at logon (0x0200000200576F57 = type 2, region 2,
        // program 0x576F57). Nothing here is a fresh guess.
        account::v2::GameAccountHandle* handle = state->mutable_game_account();
        handle->set_id(presence->GameAccountId);
        handle->set_title_id(BnetPresenceMgr::ProgramWoW);
        handle->set_region(BnetPresenceMgr::RegionId);

        fillFields(state);
    }
}

uint32 PresenceService::HandleBatchSubscribe(presence::v2::client::BatchSubscribeRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    if (!_session->GetBattlenetAccountId())
        return ERROR_INVALID_AGENT_ID;

    // Every account_ids entry is subscribed independently: BatchSubscribeRequest's response is NoData,
    // so a single failure must not take the whole batch down. Per-entry failures are reported through
    // presence::v2::PresenceListener::OnSubscribeFailure below.
    presence::v2::SubscribeFailureNotification failures;

    for (uint64 accountId : request->account_ids())
    {
        uint32 result = sBnetPresenceMgr->Subscribe(_session, uint32(accountId), true);
        if (result != ERROR_OK)
        {
            presence::v2::SubscribeFailureResult* failure = failures.add_failures();
            failure->set_account_id(accountId);
            failure->set_status(result);
        }
    }

    if (failures.failures_size())
        WorldserverService<presence::v2::PresenceListener>(_session).OnSubscribeFailure(&failures, true, true);

    return ERROR_OK;
}

uint32 PresenceService::HandleBatchUnsubscribe(presence::v2::client::BatchUnsubscribeRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    for (uint64 accountId : request->account_ids())
        sBnetPresenceMgr->Unsubscribe(_session, uint32(accountId));

    return ERROR_OK;
}

uint32 PresenceService::HandleQuery(presence::v2::client::QueryRequest const* request, presence::v2::client::QueryResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    uint32 callerAccountId = _session->GetBattlenetAccountId();
    if (!callerAccountId)
        return ERROR_INVALID_AGENT_ID;

    uint32 targetAccountId = uint32(request->account_id());
    if (!targetAccountId)
        return ERROR_INVALID_TARGET_ID;

    // Querying across a block must not answer with the blocked account's state.
    if (sBnetBlockListMgr->IsBlockedEitherWay(callerAccountId, targetAccountId))
        return ERROR_TARGET_IS_BLOCKING_AGENT;

    bool includeGameAccounts = !request->has_query_behavior()
        || request->query_behavior() != presence::v2::BATTLE_NET_ACCOUNT_ONLY;

    FillAccountStates(response->mutable_states(), targetAccountId, includeGameAccounts,
        &request->keys(), request->since_us());

    return ERROR_OK;
}

uint32 PresenceService::HandleUpdate(presence::v2::client::UpdateRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    uint32 callerAccountId = _session->GetBattlenetAccountId();
    if (!callerAccountId)
        return ERROR_INVALID_AGENT_ID;

    // presence.v2 Update carries no entity: a client may only update its own presence.
    for (presence::v2::PresenceFieldUpdate const& update : request->updates())
    {
        if (!update.has_key())
            return ERROR_RPC_MALFORMED_REQUEST;

        BnetPresenceField field;
        field.Program = update.key().title_id();
        field.Group = update.key().group();
        field.Field = update.key().field();
        field.UniqueId = update.key().unique_id();
        if (update.has_value())
            field.VariantBlob = update.value().SerializeAsString();

        sBnetPresenceMgr->ApplyRichField(callerAccountId, std::move(field), true, update.delete_());
    }

    if (request->updates_size())
        sBnetPresenceMgr->PushStateToSubscribers(callerAccountId);

    return ERROR_OK;
}

// =============================================================================================
// presence.v1
// =============================================================================================

PresenceServiceV1::PresenceServiceV1(WorldSession* session) : BaseService(session) { }

void PresenceServiceV1::FillFields(::google::protobuf::RepeatedPtrField<presence::v1::Field>* out, uint32 targetAccountId,
    ::google::protobuf::RepeatedPtrField<presence::v1::FieldKey> const* keyFilter)
{
    for (BnetPresenceField const& field : sBnetPresenceMgr->GetRichFields(targetAccountId, false))
    {
        if (!MatchesKeyFilterV1(keyFilter, field))
            continue;

        presence::v1::Field* wire = out->Add();
        presence::v1::FieldKey* key = wire->mutable_key();
        key->set_program(field.Program);
        key->set_group(field.Group);
        key->set_field(field.Field);
        if (field.UniqueId)
            key->set_unique_id(field.UniqueId);

        if (!field.VariantBlob.empty())
            wire->mutable_value()->ParseFromString(field.VariantBlob);
    }
}

void PresenceServiceV1::FillPresenceState(presence::v1::PresenceState* out, uint32 targetAccountId, uint64 entityHigh, uint64 entityLow)
{
    bgs::protocol::EntityId* entityId = out->mutable_entity_id();
    entityId->set_high(entityHigh ? entityHigh : BnetPresenceMgr::AccountEntityHigh);
    entityId->set_low(entityLow ? entityLow : uint64(targetAccountId));

    for (BnetPresenceField const& field : sBnetPresenceMgr->GetRichFields(targetAccountId, false))
    {
        presence::v1::FieldOperation* operation = out->add_field_operation();
        operation->set_operation(presence::v1::FieldOperation_OperationType_SET);

        presence::v1::Field* wire = operation->mutable_field();
        presence::v1::FieldKey* key = wire->mutable_key();
        key->set_program(field.Program);
        key->set_group(field.Group);
        key->set_field(field.Field);
        if (field.UniqueId)
            key->set_unique_id(field.UniqueId);

        if (!field.VariantBlob.empty())
            wire->mutable_value()->ParseFromString(field.VariantBlob);
    }
}

uint32 PresenceServiceV1::HandleSubscribe(presence::v1::SubscribeRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    if (!_session->GetBattlenetAccountId())
        return ERROR_INVALID_AGENT_ID;

    if (!request->has_entity_id())
        return ERROR_RPC_MALFORMED_REQUEST;

    uint32 targetAccountId = ResolveBnetAccountId(request->entity_id());
    if (!targetAccountId)
        return ERROR_INVALID_TARGET_ID;

    return sBnetPresenceMgr->Subscribe(_session, targetAccountId, false,
        request->entity_id().high(), request->entity_id().low());
}

uint32 PresenceServiceV1::HandleUnsubscribe(presence::v1::UnsubscribeRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    if (!request->has_entity_id())
        return ERROR_RPC_MALFORMED_REQUEST;

    uint32 targetAccountId = ResolveBnetAccountId(request->entity_id());
    if (!targetAccountId)
        return ERROR_INVALID_TARGET_ID;

    sBnetPresenceMgr->Unsubscribe(_session, targetAccountId);
    return ERROR_OK;
}

uint32 PresenceServiceV1::HandleUpdate(presence::v1::UpdateRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    uint32 callerAccountId = _session->GetBattlenetAccountId();
    if (!callerAccountId)
        return ERROR_INVALID_AGENT_ID;

    // presence.v1 Update names the entity being written. A client may only write its own presence -
    // either its battlenet account or one of that account's game accounts.
    if (request->has_entity_id())
    {
        uint32 targetAccountId = ResolveBnetAccountId(request->entity_id());
        if (targetAccountId != callerAccountId)
            return ERROR_DENIED;
    }

    for (presence::v1::FieldOperation const& operation : request->field_operation())
    {
        if (!operation.has_field() || !operation.field().has_key())
            return ERROR_RPC_MALFORMED_REQUEST;

        presence::v1::Field const& source = operation.field();

        BnetPresenceField field;
        field.Program = source.key().program();
        field.Group = source.key().group();
        field.Field = source.key().field();
        field.UniqueId = source.key().unique_id();
        if (source.has_value())
            field.VariantBlob = source.value().SerializeAsString();

        bool erase = operation.operation() == presence::v1::FieldOperation_OperationType_CLEAR;
        sBnetPresenceMgr->ApplyRichField(callerAccountId, std::move(field), false, erase);
    }

    if (request->field_operation_size())
        sBnetPresenceMgr->PushStateToSubscribers(callerAccountId);

    return ERROR_OK;
}

uint32 PresenceServiceV1::HandleQuery(presence::v1::QueryRequest const* request, presence::v1::QueryResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    uint32 callerAccountId = _session->GetBattlenetAccountId();
    if (!callerAccountId)
        return ERROR_INVALID_AGENT_ID;

    if (!request->has_entity_id())
        return ERROR_RPC_MALFORMED_REQUEST;

    uint32 targetAccountId = ResolveBnetAccountId(request->entity_id());
    if (!targetAccountId)
        return ERROR_INVALID_TARGET_ID;

    if (sBnetBlockListMgr->IsBlockedEitherWay(callerAccountId, targetAccountId))
        return ERROR_TARGET_IS_BLOCKING_AGENT;

    FillFields(response->mutable_field(), targetAccountId, &request->key());
    return ERROR_OK;
}

uint32 PresenceServiceV1::HandleBatchSubscribe(presence::v1::BatchSubscribeRequest const* request, presence::v1::BatchSubscribeResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    if (!_session->GetBattlenetAccountId())
        return ERROR_INVALID_AGENT_ID;

    // Unlike v2 the v1 batch response has a slot for per-entity failures, so they go there.
    for (bgs::protocol::EntityId const& entityId : request->entity_id())
    {
        uint32 targetAccountId = ResolveBnetAccountId(entityId);
        uint32 result = targetAccountId
            ? sBnetPresenceMgr->Subscribe(_session, targetAccountId, false, entityId.high(), entityId.low())
            : uint32(ERROR_INVALID_TARGET_ID);

        if (result != ERROR_OK)
        {
            presence::v1::SubscribeResult* failure = response->add_subscribe_failed();
            failure->mutable_entity_id()->CopyFrom(entityId);
            failure->set_result(result);
        }
    }

    return ERROR_OK;
}

uint32 PresenceServiceV1::HandleBatchUnsubscribe(presence::v1::BatchUnsubscribeRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    for (bgs::protocol::EntityId const& entityId : request->entity_id())
        if (uint32 targetAccountId = ResolveBnetAccountId(entityId))
            sBnetPresenceMgr->Unsubscribe(_session, targetAccountId);

    return ERROR_OK;
}
}
