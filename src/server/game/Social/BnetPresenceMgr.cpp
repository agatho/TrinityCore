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

#include "BnetPresenceMgr.h"
#include "BattlenetRpcErrorCodes.h"
#include "BnetBlockListMgr.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Log.h"
#include "Player.h"
#include "PresenceService.h"
#include "Timer.h"
#include "World.h"
#include "WorldSession.h"
#include "WorldserverService.h"
#include "Client/presence_listener.pb.h"
#include "Client/presence_types.pb.h"
#include "Client/api/client/v2/presence_listener.pb.h"
#include "Client/api/client/v2/presence_types.pb.h"
#include <algorithm>

namespace
{
    std::vector<BnetPresenceField> const EmptyFieldList;

    uint64 NowMicroseconds()
    {
        return uint64(GameTime::GetGameTime()) * 1000000ull;
    }
}

BnetPresenceMgr* BnetPresenceMgr::instance()
{
    static BnetPresenceMgr instance;
    return &instance;
}

void BnetPresenceMgr::Load()
{
    _presence.clear();
    _richFieldsV1.clear();
    _richFieldsV2.clear();
    _subscriptions.clear();

    // Nothing in this worldserver is online yet, so any row still flagged online is a leftover from a
    // crash or an unclean shutdown. Clearing it keeps the bnetserver's character-select view honest.
    LoginDatabase.Execute(LoginDatabase.GetPreparedStatement(LOGIN_UPD_BNET_PRESENCE_ALL_OFFLINE));

    TC_LOG_INFO("server.loading", ">> Battle.net presence tracking initialised (stale online flags cleared)");
}

// ---------------------------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------------------------

BnetGameAccountPresence& BnetPresenceMgr::GetOrCreate(uint32 gameAccountId, uint32 bnetAccountId)
{
    BnetGameAccountPresence& presence = _presence[gameAccountId];
    presence.GameAccountId = gameAccountId;
    presence.BnetAccountId = bnetAccountId;
    return presence;
}

BnetGameAccountPresence const* BnetPresenceMgr::GetGameAccountPresence(uint32 gameAccountId) const
{
    auto itr = _presence.find(gameAccountId);
    return itr != _presence.end() ? &itr->second : nullptr;
}

uint32 BnetPresenceMgr::ResolveAccountIdFromEntity(uint64 entityHigh, uint64 entityLow) const
{
    // Every bgs EntityId is {high, low} where `low` is the id of the thing and `high` classifies it.
    // The two classifications that matter are the ones this project's own bnetserver hands the client
    // in authentication::v1::LogonResult (bnetserver/Services/AuthenticationService.cpp:294-300), so
    // neither constant is guessed.
    if (entityHigh == GameAccountEntityHigh)
    {
        if (BnetGameAccountPresence const* presence = GetGameAccountPresence(uint32(entityLow)))
            return presence->BnetAccountId;

        // A game account that is not connected to this worldserver. On a single-realm deployment there
        // is no second worldserver to ask, so it cannot be resolved.
        return 0;
    }

    if (entityHigh && entityHigh != AccountEntityHigh)
        TC_LOG_DEBUG("session.rpc", "bnet presence: unrecognised EntityId high 0x{:016X}, reading low ({}) as a battlenet account id", entityHigh, entityLow);

    return uint32(entityLow);
}

std::vector<BnetGameAccountPresence const*> BnetPresenceMgr::GetAccountPresence(uint32 bnetAccountId) const
{
    std::vector<BnetGameAccountPresence const*> result;
    if (!bnetAccountId)
        return result;

    for (auto const& [gameAccountId, presence] : _presence)
        if (presence.BnetAccountId == bnetAccountId && presence.IsOnline)
            result.push_back(&presence);

    return result;
}

bool BnetPresenceMgr::IsAccountOnline(uint32 bnetAccountId) const
{
    if (!bnetAccountId)
        return false;

    for (auto const& [gameAccountId, presence] : _presence)
        if (presence.BnetAccountId == bnetAccountId && presence.IsOnline)
            return true;

    return false;
}

void BnetPresenceMgr::Persist(BnetGameAccountPresence const& presence)
{
    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_REP_BNET_PRESENCE);
    stmt->setUInt32(0, presence.GameAccountId);
    stmt->setUInt32(1, presence.BnetAccountId);
    stmt->setUInt8(2, presence.IsOnline ? 1 : 0);
    stmt->setUInt32(3, presence.Character.RealmAddress);
    stmt->setUInt64(4, presence.Character.CharacterGuid);
    stmt->setString(5, presence.Character.CharacterName);
    stmt->setUInt8(6, presence.Character.Level);
    stmt->setUInt8(7, presence.Character.RaceId);
    stmt->setUInt8(8, presence.Character.ClassId);
    stmt->setUInt8(9, presence.Character.FactionId);
    stmt->setUInt32(10, presence.Character.ZoneId);
    stmt->setUInt32(11, presence.Character.AreaId);
    stmt->setUInt64(12, uint64(presence.UpdateTime));
    LoginDatabase.Execute(stmt);
}

// ---------------------------------------------------------------------------------------------
// events
// ---------------------------------------------------------------------------------------------

void BnetPresenceMgr::OnSessionOnline(WorldSession* session)
{
    uint32 bnetAccountId = session->GetBattlenetAccountId();
    if (!bnetAccountId)
        return;

    BnetGameAccountPresence& presence = GetOrCreate(session->GetAccountId(), bnetAccountId);
    presence.IsOnline = true;
    presence.OnlineTime = GameTime::GetGameTime();
    presence.UpdateTime = presence.OnlineTime;
    presence.HasCharacter = false;
    presence.Character = { };

    Persist(presence);
    PushStateToSubscribers(bnetAccountId);
}

void BnetPresenceMgr::OnSessionOffline(WorldSession* session)
{
    uint32 bnetAccountId = session->GetBattlenetAccountId();
    uint32 gameAccountId = session->GetAccountId();

    // Drop everything this session owned before anything else: subscriptions are keyed by game
    // account id, so a stale entry would keep resolving to whoever reuses the id next.
    UnsubscribeAll(session);

    auto itr = _presence.find(gameAccountId);
    if (itr == _presence.end())
        return;

    BnetGameAccountPresence& presence = itr->second;
    presence.IsOnline = false;
    presence.HasCharacter = false;
    presence.Character = { };
    presence.UpdateTime = GameTime::GetGameTime();

    Persist(presence);

    // The account may still be online through another game account, so the push carries whatever is
    // left rather than a blanket "offline".
    if (bnetAccountId)
    {
        PushStateToSubscribers(bnetAccountId);

        if (!IsAccountOnline(bnetAccountId))
            ClearRichFields(bnetAccountId);
    }

    _presence.erase(itr);
}

void BnetPresenceMgr::OnCharacterLogin(Player* player)
{
    WorldSession* session = player->GetSession();
    if (!session)
        return;

    uint32 bnetAccountId = session->GetBattlenetAccountId();
    if (!bnetAccountId)
        return;

    BnetGameAccountPresence& presence = GetOrCreate(session->GetAccountId(), bnetAccountId);
    presence.IsOnline = true;
    if (!presence.OnlineTime)
        presence.OnlineTime = GameTime::GetGameTime();

    presence.HasCharacter = true;
    presence.Character.CharacterGuid = player->GetGUID().GetCounter();
    presence.Character.CharacterName = player->GetName();
    presence.Character.RealmAddress = GetVirtualRealmAddress();
    presence.Character.Level = player->GetLevel();
    presence.Character.RaceId = player->GetRace();
    presence.Character.ClassId = player->GetClass();
    presence.Character.FactionId = uint8(player->GetEffectiveTeamId());
    presence.Character.ZoneId = player->GetZoneId();
    presence.Character.AreaId = player->GetAreaId();
    presence.UpdateTime = GameTime::GetGameTime();

    Persist(presence);
    PushStateToSubscribers(bnetAccountId);
}

void BnetPresenceMgr::OnCharacterLogout(Player* player)
{
    WorldSession* session = player->GetSession();
    if (!session)
        return;

    uint32 bnetAccountId = session->GetBattlenetAccountId();

    auto itr = _presence.find(session->GetAccountId());
    if (itr == _presence.end())
        return;

    BnetGameAccountPresence& presence = itr->second;
    presence.HasCharacter = false;
    presence.Character = { };
    presence.UpdateTime = GameTime::GetGameTime();

    Persist(presence);

    if (bnetAccountId)
        PushStateToSubscribers(bnetAccountId);
}

void BnetPresenceMgr::OnZoneChanged(Player* player, uint32 zoneId, uint32 areaId)
{
    WorldSession* session = player->GetSession();
    if (!session)
        return;

    auto itr = _presence.find(session->GetAccountId());
    if (itr == _presence.end() || !itr->second.HasCharacter)
        return;

    BnetGameAccountPresence& presence = itr->second;
    if (presence.Character.ZoneId == zoneId && presence.Character.AreaId == areaId
        && presence.Character.Level == player->GetLevel())
        return;

    presence.Character.ZoneId = zoneId;
    presence.Character.AreaId = areaId;
    presence.Character.Level = player->GetLevel();
    presence.UpdateTime = GameTime::GetGameTime();

    Persist(presence);

    if (uint32 bnetAccountId = session->GetBattlenetAccountId())
        PushStateToSubscribers(bnetAccountId);
}

// ---------------------------------------------------------------------------------------------
// subscriptions
// ---------------------------------------------------------------------------------------------

uint32 BnetPresenceMgr::Subscribe(WorldSession* subscriber, uint32 targetAccountId, bool useV2, uint64 entityHigh, uint64 entityLow)
{
    uint32 subscriberAccountId = subscriber->GetBattlenetAccountId();
    if (!subscriberAccountId)
        return ERROR_INVALID_AGENT_ID;

    if (!targetAccountId)
        return ERROR_INVALID_TARGET_ID;

    // Presence must not leak across a block in either direction.
    if (sBnetBlockListMgr->IsBlockedEitherWay(subscriberAccountId, targetAccountId))
        return ERROR_TARGET_IS_BLOCKING_AGENT;

    std::vector<BnetPresenceSubscription>& subscriptions = _subscriptions[targetAccountId];

    uint32 owned = 0;
    for (auto const& [watched, list] : _subscriptions)
        for (BnetPresenceSubscription const& subscription : list)
            if (subscription.SubscriberGameAccountId == subscriber->GetAccountId())
                ++owned;

    if (owned >= MaxSubscriptions)
        return ERROR_PRESENCE_TOO_MANY_SUBSCRIPTIONS;

    auto existing = std::ranges::find_if(subscriptions, [&](BnetPresenceSubscription const& subscription)
    {
        return subscription.SubscriberGameAccountId == subscriber->GetAccountId() && subscription.UseV2 == useV2;
    });

    if (existing == subscriptions.end())
    {
        BnetPresenceSubscription subscription;
        subscription.SubscriberGameAccountId = subscriber->GetAccountId();
        subscription.TargetAccountId = targetAccountId;
        subscription.UseV2 = useV2;
        subscription.EntityHigh = entityHigh;
        subscription.EntityLow = entityLow;
        subscriptions.push_back(subscription);
    }
    else
    {
        existing->EntityHigh = entityHigh;
        existing->EntityLow = entityLow;
    }

    // Both Subscribe responses are empty, so this push is the only way the current value can arrive.
    PushStateTo(subscriber, targetAccountId, useV2, entityHigh, entityLow);
    return ERROR_OK;
}

bool BnetPresenceMgr::Unsubscribe(WorldSession* subscriber, uint32 targetAccountId)
{
    auto itr = _subscriptions.find(targetAccountId);
    if (itr == _subscriptions.end())
        return false;

    size_t removed = std::erase_if(itr->second, [&](BnetPresenceSubscription const& subscription)
    {
        return subscription.SubscriberGameAccountId == subscriber->GetAccountId();
    });

    if (itr->second.empty())
        _subscriptions.erase(itr);

    return removed != 0;
}

void BnetPresenceMgr::UnsubscribeAll(WorldSession* subscriber)
{
    uint32 gameAccountId = subscriber->GetAccountId();

    for (auto itr = _subscriptions.begin(); itr != _subscriptions.end(); )
    {
        std::erase_if(itr->second, [gameAccountId](BnetPresenceSubscription const& subscription)
        {
            return subscription.SubscriberGameAccountId == gameAccountId;
        });

        itr = itr->second.empty() ? _subscriptions.erase(itr) : std::next(itr);
    }
}

std::vector<BnetPresenceSubscription> BnetPresenceMgr::CollectLiveSubscriptions(uint32 targetAccountId)
{
    std::vector<BnetPresenceSubscription> live;

    auto itr = _subscriptions.find(targetAccountId);
    if (itr == _subscriptions.end())
        return live;

    std::erase_if(itr->second, [&](BnetPresenceSubscription const& subscription)
    {
        WorldSession* session = sWorld->FindSession(subscription.SubscriberGameAccountId);
        // Dropped, or the game account id was reused by someone else - the subscription is dead either way.
        if (!session || !session->GetBattlenetAccountId())
            return true;

        // A block placed after the subscription was made must take effect immediately.
        if (sBnetBlockListMgr->IsBlockedEitherWay(session->GetBattlenetAccountId(), targetAccountId))
            return true;

        live.push_back(subscription);
        return false;
    });

    if (itr->second.empty())
        _subscriptions.erase(itr);

    return live;
}

// ---------------------------------------------------------------------------------------------
// client-authored rich fields
// ---------------------------------------------------------------------------------------------

std::vector<BnetPresenceField> const& BnetPresenceMgr::GetRichFields(uint32 bnetAccountId, bool v2) const
{
    auto const& store = v2 ? _richFieldsV2 : _richFieldsV1;
    auto itr = store.find(bnetAccountId);
    return itr != store.end() ? itr->second : EmptyFieldList;
}

void BnetPresenceMgr::ApplyRichField(uint32 bnetAccountId, BnetPresenceField field, bool v2, bool erase)
{
    if (!bnetAccountId)
        return;

    // The (group, field) numbering is Blizzard's and is not derivable offline. Logging every key the
    // client authors is how the real numbering gets recovered from a live client.
    TC_LOG_DEBUG("session.rpc", "presence.{} field {} for account {}: program {} group {} field {} unique_id {} ({} bytes)",
        v2 ? "v2" : "v1", erase ? "clear" : "set", bnetAccountId, field.Program, field.Group, field.Field,
        field.UniqueId, field.VariantBlob.size());

    auto& store = v2 ? _richFieldsV2 : _richFieldsV1;
    std::vector<BnetPresenceField>& fields = store[bnetAccountId];

    auto existing = std::ranges::find_if(fields, [&](BnetPresenceField const& stored) { return stored.SameKey(field); });

    if (erase)
    {
        if (existing != fields.end())
            fields.erase(existing);

        if (fields.empty())
            store.erase(bnetAccountId);

        return;
    }

    if (!field.UpdatedTimeUs)
        field.UpdatedTimeUs = NowMicroseconds();

    if (existing != fields.end())
        *existing = std::move(field);
    else
        fields.push_back(std::move(field));
}

void BnetPresenceMgr::ClearRichFields(uint32 bnetAccountId)
{
    _richFieldsV1.erase(bnetAccountId);
    _richFieldsV2.erase(bnetAccountId);
}

// ---------------------------------------------------------------------------------------------
// pushes
// ---------------------------------------------------------------------------------------------

void BnetPresenceMgr::PushStateTo(WorldSession* subscriber, uint32 targetAccountId, bool useV2, uint64 entityHigh, uint64 entityLow)
{
    uint32 subscriberAccountId = subscriber->GetBattlenetAccountId();
    if (!subscriberAccountId)
        return;

    if (sBnetBlockListMgr->IsBlockedEitherWay(subscriberAccountId, targetAccountId))
        return;

    if (useV2)
    {
        bgs::protocol::presence::v2::PresenceStateUpdatedNotification notification;
        notification.set_subscriber_id(subscriberAccountId);
        Battlenet::Services::PresenceService::FillAccountStates(notification.mutable_states(), targetAccountId);

        Battlenet::WorldserverService<bgs::protocol::presence::v2::PresenceListener>(subscriber).OnPresenceStateUpdated(&notification, true, true);
        return;
    }

    bgs::protocol::presence::v1::StateChangedNotification notification;
    notification.mutable_subscriber_id()->set_id(subscriberAccountId);
    notification.set_subscriber_program(ProgramWoW);
    Battlenet::Services::PresenceServiceV1::FillPresenceState(notification.add_state(), targetAccountId, entityHigh, entityLow);

    Battlenet::WorldserverService<bgs::protocol::presence::v1::PresenceListener>(subscriber).OnStateChanged(&notification, true, true);
}

void BnetPresenceMgr::PushStateToSubscribers(uint32 targetAccountId)
{
    std::vector<BnetPresenceSubscription> live = CollectLiveSubscriptions(targetAccountId);

    for (BnetPresenceSubscription const& subscription : live)
    {
        WorldSession* session = sWorld->FindSession(subscription.SubscriberGameAccountId);
        if (!session)
            continue;

        PushStateTo(session, targetAccountId, subscription.UseV2, subscription.EntityHigh, subscription.EntityLow);
    }
}
