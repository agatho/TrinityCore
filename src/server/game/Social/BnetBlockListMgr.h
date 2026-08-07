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

#ifndef TRINITYCORE_BNET_BLOCK_LIST_MGR_H
#define TRINITYCORE_BNET_BLOCK_LIST_MGR_H

#include "Define.h"
#include <ctime>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class WorldSession;

// Battle.net block list, backing bgs.protocol.block_list.v1.client.BlockListService (OriginalHash
// 0x8E8F5FB0).
//
// Scope: a *battlenet account* blocks another *battlenet account*. This is one level above
// PlayerSocial's per-character ignore list (SOCIAL_FLAG_IGNORED), which stays untouched: a player
// can still ignore a single character without blocking the whole account behind it.
//
// Loaded once at startup and kept in memory for the same reason BnetFriendsMgr is: the block-list
// RPC handlers are synchronous and are answered at STATUS_AUTHED, before any Player exists.
//
// Threading: CMSG_BATTLENET_REQUEST is PROCESS_THREADUNSAFE, so every entry point runs on the world
// update thread. No locking.

// block_list.v1 BlockedPlayer.
struct BnetBlockedPlayer
{
    uint32 AccountId    = 0;    // block_list.v1 BlockedPlayer.id (the blocked battlenet account)
    std::string BattleTag;      // block_list.v1 BlockedPlayer.battle_tag, snapshotted when the block was made
    time_t CreationTime = 0;    // block_list.v1 BlockedPlayer.creation_time_us / 1000000
    time_t ModifiedTime = 0;    // block_list.v1 BlockedPlayer.modified_time_us / 1000000
};

class TC_GAME_API BnetBlockListMgr
{
public:
    static BnetBlockListMgr* instance();

    void Load();

    // ---- reads ------------------------------------------------------------------------------

    std::vector<BnetBlockedPlayer> const& GetBlockList(uint32 bnetAccountId) const;

    // True when `bnetAccountId` has blocked `otherAccountId` - durably or for this session only.
    bool IsBlocked(uint32 bnetAccountId, uint32 otherAccountId) const;

    // True when either side blocks the other. This is the predicate every suppression site wants:
    // blocking is not a one-way mute on retail, it severs the pair.
    bool IsBlockedEitherWay(uint32 accountA, uint32 accountB) const;

    // ---- mutations --------------------------------------------------------------------------
    // Each returns a BattlenetRpcErrorCodes value (ERROR_OK on success) and does its own persistence
    // and its own listener push to every subscribed session of the blocking account.

    // sessionOnly maps HandleBlockPlayerForSession: the block is real but is dropped when the last
    // session of the account goes away, and is never written to the auth database.
    uint32 BlockPlayer(uint32 bnetAccountId, uint32 targetAccountId, bool sessionOnly);
    uint32 UnblockPlayer(uint32 bnetAccountId, uint32 targetAccountId);

    // ---- subscriptions ----------------------------------------------------------------------
    // Held as game account ids for the same reason BnetFriendsMgr holds them that way: a session that
    // drops without unsubscribing can never leave a dangling WorldSession* behind.

    void Subscribe(WorldSession* session);
    bool Unsubscribe(WorldSession* session);

    // Called when a session goes away, to drop its session-scope blocks once the account has no
    // sessions left.
    void OnSessionClosed(WorldSession* session);

    // Retail caps the Battle.net block list; without a cap a client could grow it without bound.
    static constexpr uint32 MaxBlockedAccounts = 100;

private:
    BnetBlockListMgr() = default;

    std::vector<WorldSession*> GetSubscribedSessions(uint32 bnetAccountId);
    void PushBlockedPlayerAdded(uint32 bnetAccountId, BnetBlockedPlayer const& player);
    void PushBlockedPlayerRemoved(uint32 bnetAccountId, uint32 removedAccountId);

    std::unordered_map<uint32, std::vector<BnetBlockedPlayer>> _blocked;
    // Session-scope blocks (BlockPlayerForSession). Keyed by battlenet account id and cleared when
    // that account's last session closes.
    std::unordered_map<uint32, std::unordered_set<uint32>> _sessionBlocked;
    // battlenet account id -> game account ids subscribed to block_list.v1 notifications.
    std::unordered_map<uint32, std::vector<uint32>> _subscribers;
};

#define sBnetBlockListMgr BnetBlockListMgr::instance()

#endif // TRINITYCORE_BNET_BLOCK_LIST_MGR_H
