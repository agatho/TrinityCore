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

#include "BnetBlockListMgr.h"
#include "BattlenetRpcErrorCodes.h"
#include "BnetFriendsMgr.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Log.h"
#include "Timer.h"
#include "World.h"
#include "WorldSession.h"
#include "WorldserverService.h"
#include "Client/api/client/v1/block_list_listener.pb.h"
#include <algorithm>

namespace
{
    std::vector<BnetBlockedPlayer> const EmptyBlockList;
}

BnetBlockListMgr* BnetBlockListMgr::instance()
{
    static BnetBlockListMgr instance;
    return &instance;
}

void BnetBlockListMgr::Load()
{
    uint32 oldMSTime = getMSTime();

    _blocked.clear();
    _sessionBlocked.clear();
    _subscribers.clear();

    uint32 count = 0;
    if (QueryResult result = LoginDatabase.Query("SELECT accountId, blockedAccountId, blockedBattleTag, creationTime, modifiedTime FROM battlenet_account_blocked"))
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 accountId = fields[0].GetUInt32();

            BnetBlockedPlayer player;
            player.AccountId = fields[1].GetUInt32();
            player.BattleTag = fields[2].GetString();
            player.CreationTime = time_t(fields[3].GetUInt64());
            player.ModifiedTime = time_t(fields[4].GetUInt64());

            if (!accountId || !player.AccountId || accountId == player.AccountId)
                continue;

            _blocked[accountId].push_back(std::move(player));
            ++count;
        }
        while (result->NextRow());
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} battle.net account block list entries in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

std::vector<BnetBlockedPlayer> const& BnetBlockListMgr::GetBlockList(uint32 bnetAccountId) const
{
    auto itr = _blocked.find(bnetAccountId);
    return itr != _blocked.end() ? itr->second : EmptyBlockList;
}

bool BnetBlockListMgr::IsBlocked(uint32 bnetAccountId, uint32 otherAccountId) const
{
    if (!bnetAccountId || !otherAccountId || bnetAccountId == otherAccountId)
        return false;

    if (auto itr = _blocked.find(bnetAccountId); itr != _blocked.end())
        if (std::ranges::find(itr->second, otherAccountId, &BnetBlockedPlayer::AccountId) != itr->second.end())
            return true;

    if (auto itr = _sessionBlocked.find(bnetAccountId); itr != _sessionBlocked.end())
        if (itr->second.contains(otherAccountId))
            return true;

    return false;
}

bool BnetBlockListMgr::IsBlockedEitherWay(uint32 accountA, uint32 accountB) const
{
    return IsBlocked(accountA, accountB) || IsBlocked(accountB, accountA);
}

uint32 BnetBlockListMgr::BlockPlayer(uint32 bnetAccountId, uint32 targetAccountId, bool sessionOnly)
{
    if (!bnetAccountId)
        return ERROR_INVALID_AGENT_ID;

    if (!targetAccountId)
        return ERROR_INVALID_TARGET_ID;

    if (bnetAccountId == targetAccountId)
        return ERROR_USER_MANAGER_CANNOT_BLOCK_SELF;

    if (!sBnetFriendsMgr->GetOrLoadIdentity(targetAccountId))
        return ERROR_INVALID_TARGET_ID;

    if (IsBlocked(bnetAccountId, targetAccountId))
        return ERROR_USER_MANAGER_ALREADY_BLOCKED;

    // Retail refuses to block someone who is still on the friend list; the client is expected to
    // remove the friendship first, and silently doing it here would delete data the player did not
    // ask us to delete.
    if (sBnetFriendsMgr->IsFriend(bnetAccountId, targetAccountId))
        return ERROR_USER_MANAGER_CANNOT_BLOCK_FRIEND;

    std::vector<BnetBlockedPlayer>& list = _blocked[bnetAccountId];
    if (list.size() >= MaxBlockedAccounts)
        return ERROR_USER_MANAGER_TOO_MANY_BLOCKED_ENTITIES;

    if (sessionOnly)
    {
        _sessionBlocked[bnetAccountId].insert(targetAccountId);

        // The client still wants the entry to appear in its block list for the rest of the session,
        // so it is pushed exactly like a durable one - it simply is not written to the database.
        BnetBlockedPlayer transient;
        transient.AccountId = targetAccountId;
        if (BnetAccountIdentity const* identity = sBnetFriendsMgr->GetIdentity(targetAccountId))
            if (identity->HasBattleTag())
                transient.BattleTag = identity->GetBattleTag();
        transient.CreationTime = GameTime::GetGameTime();
        transient.ModifiedTime = transient.CreationTime;

        PushBlockedPlayerAdded(bnetAccountId, transient);
        return ERROR_OK;
    }

    BnetBlockedPlayer player;
    player.AccountId = targetAccountId;
    if (BnetAccountIdentity const* identity = sBnetFriendsMgr->GetIdentity(targetAccountId))
        if (identity->HasBattleTag())
            player.BattleTag = identity->GetBattleTag();
    player.CreationTime = GameTime::GetGameTime();
    player.ModifiedTime = player.CreationTime;

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_REP_BNET_BLOCKED);
    stmt->setUInt32(0, bnetAccountId);
    stmt->setUInt32(1, player.AccountId);
    stmt->setString(2, player.BattleTag);
    stmt->setUInt64(3, uint64(player.CreationTime));
    stmt->setUInt64(4, uint64(player.ModifiedTime));
    LoginDatabase.Execute(stmt);

    list.push_back(player);

    PushBlockedPlayerAdded(bnetAccountId, player);
    return ERROR_OK;
}

uint32 BnetBlockListMgr::UnblockPlayer(uint32 bnetAccountId, uint32 targetAccountId)
{
    if (!bnetAccountId)
        return ERROR_INVALID_AGENT_ID;

    bool removed = false;

    if (auto itr = _sessionBlocked.find(bnetAccountId); itr != _sessionBlocked.end())
    {
        if (itr->second.erase(targetAccountId))
            removed = true;

        if (itr->second.empty())
            _sessionBlocked.erase(itr);
    }

    if (auto itr = _blocked.find(bnetAccountId); itr != _blocked.end())
    {
        if (std::erase_if(itr->second, [targetAccountId](BnetBlockedPlayer const& player) { return player.AccountId == targetAccountId; }))
        {
            removed = true;

            LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_DEL_BNET_BLOCKED);
            stmt->setUInt32(0, bnetAccountId);
            stmt->setUInt32(1, targetAccountId);
            LoginDatabase.Execute(stmt);
        }

        if (itr->second.empty())
            _blocked.erase(itr);
    }

    if (!removed)
        return ERROR_USER_MANAGER_NOT_BLOCKED;

    PushBlockedPlayerRemoved(bnetAccountId, targetAccountId);
    return ERROR_OK;
}

void BnetBlockListMgr::Subscribe(WorldSession* session)
{
    std::vector<uint32>& gameAccountIds = _subscribers[session->GetBattlenetAccountId()];
    if (std::ranges::find(gameAccountIds, session->GetAccountId()) == gameAccountIds.end())
        gameAccountIds.push_back(session->GetAccountId());
}

bool BnetBlockListMgr::Unsubscribe(WorldSession* session)
{
    auto itr = _subscribers.find(session->GetBattlenetAccountId());
    if (itr == _subscribers.end())
        return false;

    size_t removed = std::erase(itr->second, session->GetAccountId());
    if (itr->second.empty())
        _subscribers.erase(itr);

    return removed != 0;
}

void BnetBlockListMgr::OnSessionClosed(WorldSession* session)
{
    uint32 bnetAccountId = session->GetBattlenetAccountId();
    if (!bnetAccountId)
        return;

    if (auto itr = _subscribers.find(bnetAccountId); itr != _subscribers.end())
    {
        std::erase(itr->second, session->GetAccountId());
        if (itr->second.empty())
            _subscribers.erase(itr);
    }

    // Session-scope blocks belong to the battlenet account, so they survive as long as any of its
    // game accounts is still connected.
    for (auto const& [gameAccountId, other] : sWorld->GetAllSessions())
        if (other && other != session && other->GetBattlenetAccountId() == bnetAccountId)
            return;

    _sessionBlocked.erase(bnetAccountId);
}

std::vector<WorldSession*> BnetBlockListMgr::GetSubscribedSessions(uint32 bnetAccountId)
{
    std::vector<WorldSession*> sessions;

    auto itr = _subscribers.find(bnetAccountId);
    if (itr == _subscribers.end())
        return sessions;

    std::vector<uint32>& gameAccountIds = itr->second;
    std::erase_if(gameAccountIds, [&](uint32 gameAccountId)
    {
        WorldSession* session = sWorld->FindSession(gameAccountId);
        if (!session || session->GetBattlenetAccountId() != bnetAccountId)
            return true;

        sessions.push_back(session);
        return false;
    });

    if (gameAccountIds.empty())
        _subscribers.erase(itr);

    return sessions;
}

void BnetBlockListMgr::PushBlockedPlayerAdded(uint32 bnetAccountId, BnetBlockedPlayer const& player)
{
    std::vector<WorldSession*> sessions = GetSubscribedSessions(bnetAccountId);
    if (sessions.empty())
        return;

    bgs::protocol::block_list::v1::client::BlockedPlayerAddedNotification notification;
    notification.set_agent_account_id(bnetAccountId);

    bgs::protocol::block_list::v1::client::BlockedPlayer* wire = notification.add_player();
    wire->set_id(player.AccountId);
    if (!player.BattleTag.empty())
        wire->set_battle_tag(player.BattleTag);
    wire->set_creation_time_us(uint64(player.CreationTime) * 1000000ull);
    wire->set_modified_time_us(uint64(player.ModifiedTime) * 1000000ull);

    for (WorldSession* session : sessions)
        Battlenet::WorldserverService<bgs::protocol::block_list::v1::client::BlockListListener>(session).OnBlockedPlayerAdded(&notification, true, true);
}

void BnetBlockListMgr::PushBlockedPlayerRemoved(uint32 bnetAccountId, uint32 removedAccountId)
{
    std::vector<WorldSession*> sessions = GetSubscribedSessions(bnetAccountId);
    if (sessions.empty())
        return;

    bgs::protocol::block_list::v1::client::BlockedPlayerRemovedNotification notification;
    notification.set_agent_account_id(bnetAccountId);
    notification.add_player()->set_id(removedAccountId);

    for (WorldSession* session : sessions)
        Battlenet::WorldserverService<bgs::protocol::block_list::v1::client::BlockListListener>(session).OnBlockedPlayerRemoved(&notification, true, true);
}
