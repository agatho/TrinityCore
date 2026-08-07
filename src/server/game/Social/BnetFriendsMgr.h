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

#ifndef TRINITYCORE_BNET_FRIENDS_MGR_H
#define TRINITYCORE_BNET_FRIENDS_MGR_H

#include "Define.h"
#include <ctime>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class WorldSession;

// Battle.net friend graph, backing bgs.protocol.friends.v2.client.FriendsService.
//
// Why this lives here and not behind a per-session query:
//   The friends.v2 RPC handlers are synchronous - they fill the response message and return a status
//   in one call - and they are answered at STATUS_AUTHED, i.e. while the player is still at character
//   select and no Player exists. A per-call auth-database round trip is therefore not available. The
//   whole graph is small (one row per friendship direction), so it is loaded once at startup and kept
//   in memory, with every mutation written back asynchronously.
//
// Single-realm deployment: every live player is in this worldserver process, so the live half of the
// graph - who is online, who must be pushed a notification - is entirely in-process. The auth database
// holds only the durable half.
//
// Threading: CMSG_BATTLENET_REQUEST is PROCESS_THREADUNSAFE (Opcodes.cpp), so every entry point here
// runs on the world update thread, like SocialMgr. No locking.

// One direction of a friendship. Two of these exist per accepted invitation.
struct BnetFriendship
{
    uint32 FriendId      = 0;   // friends.v2 Friend.id
    uint32 Level         = 0;   // friends.v2 Friend.level - echoed from the client, never synthesised
    std::string Note;           // friends.v2 Friend.note
    std::vector<uint32> TitleTagIds; // friends.v2 Friend.title_tag.ids
    time_t CreationTime  = 0;   // friends.v2 Friend.creation_time_s
};

// A pending invitation, in the shape both friends.v2 ReceivedInvitation and SentInvitation need.
struct BnetFriendInvitation
{
    uint64 Id             = 0;  // friends.v2 *Invitation.id
    uint32 SenderId       = 0;
    uint32 TargetId       = 0;
    std::string TargetTag;      // friends.v2 SentInvitation.target_name
    uint32 Level          = 0;  // friends.v2 SendInvitationOptions.level
    std::string Note;           // friends.v2 SendInvitationOptions.note / SentInvitation.note
    std::vector<uint32> TitleTagIds; // friends.v2 SendInvitationOptions.title_tag / SentInvitation.title_tag
    time_t CreationTime   = 0;  // friends.v2 *Invitation.creation_time_s
    time_t ExpirationTime = 0;  // friends.v2 ReceivedInvitation.expiration_time_s
};

// Everything needed to describe an account to another account.
//
// PRESENCE SEAM: presence.v1/v2 (backlog item #18) attaches here. The rich presence fields the client
// renders under a friend's name - online flag, character name, level, race/class, zone - belong on this
// struct or on a parallel BnetPresenceMgr keyed by the same bnet account id. Nothing in this file
// invents them; friends.v2 carries no presence field at all (verified against friends_types.pb.h:
// Friend has exactly id, level, battle_tag, full_name, attributes, creation_time_s, note, title_tag).
struct BnetAccountIdentity
{
    uint32 Id = 0;
    std::string BattleTagName;              // "Name" half of Name#1234
    uint16 BattleTagDiscriminator = 0;      // "1234" half
    std::string Email;

    // friends.v2 UserDescription.battle_tag / Friend.battle_tag wire form.
    std::string GetBattleTag() const;
    bool HasBattleTag() const { return !BattleTagName.empty() && BattleTagDiscriminator != 0; }
};

class TC_GAME_API BnetFriendsMgr
{
public:
    static BnetFriendsMgr* instance();

    void Load();

    // ---- identity ---------------------------------------------------------------------------

    // Returns nullptr if the account is unknown to this worldserver.
    BnetAccountIdentity const* GetIdentity(uint32 bnetAccountId) const;

    // Like GetIdentity, but falls back to a synchronous auth-database read (and, if the account still
    // has no BattleTag, allocates one) for accounts created after startup. Used on the subscribe path.
    BnetAccountIdentity const* GetOrLoadIdentity(uint32 bnetAccountId);

    // "Name#1234" -> bnet account id, 0 if no such BattleTag. Case-insensitive on the name half,
    // matching the client, which upper-cases nothing but compares insensitively.
    uint32 FindAccountByBattleTag(std::string_view battleTag) const;
    uint32 FindAccountByEmail(std::string_view email) const;

    // ---- graph reads ------------------------------------------------------------------------

    std::vector<BnetFriendship> const& GetFriends(uint32 bnetAccountId) const;
    bool IsFriend(uint32 bnetAccountId, uint32 otherAccountId) const;
    BnetFriendship const* GetFriendship(uint32 bnetAccountId, uint32 friendId) const;

    std::vector<BnetFriendInvitation> GetSentInvitations(uint32 bnetAccountId) const;
    std::vector<BnetFriendInvitation> GetReceivedInvitations(uint32 bnetAccountId) const;
    BnetFriendInvitation const* GetInvitation(uint64 invitationId) const;

    // ---- graph mutations --------------------------------------------------------------------
    // Each returns a BattlenetRpcErrorCodes value (ERROR_OK on success) and performs its own
    // persistence and its own listener push to every affected online session.

    uint32 SendInvitation(uint32 senderId, uint32 targetId, std::string_view targetTagTyped, uint32 level,
        std::string_view note, std::vector<uint32> titleTagIds);
    uint32 AcceptInvitation(uint32 accepterId, uint64 invitationId, uint32 level);
    // reason must be an InvitationRemovedReason (invitation_types.pb.h). callerMustBeSender separates
    // RevokeInvitation (the sender's verb) from IgnoreInvitation (the recipient's).
    uint32 RemoveInvitation(uint32 callerId, uint64 invitationId, uint32 reason, bool callerMustBeSender);
    uint32 RevokeAllInvitations(uint32 senderId);
    uint32 RemoveFriend(uint32 callerId, uint32 targetId);
    uint32 UpdateFriendState(uint32 callerId, uint32 targetId, bool hasNote, std::string_view note,
        bool hasTitleTags, std::vector<uint32> titleTagIds);

    // ---- live half --------------------------------------------------------------------------

    // Every online session belonging to a bnet account. A bnet account can have several game accounts
    // logged in at once, and each one is a separate RPC endpoint.
    // PRESENCE SEAM: this is the fan-out primitive presence.v1/v2 needs; it is public for that reason.
    std::vector<WorldSession*> GetOnlineSessions(uint32 bnetAccountId) const;

    // friends.v2 Subscribe/Unsubscribe. Subscribing registers the session for listener push and
    // immediately sends the caller's full current state (friends + both invitation directions) down
    // the tunnel: SubscribeRequest carries no fields and its response is NoData, so the initial list
    // can only arrive as listener notifications.
    void Subscribe(WorldSession* session);
    bool Unsubscribe(WorldSession* session);
    bool IsSubscribed(WorldSession const* session) const;

    // Maximum simultaneous outstanding invitations in either direction, per account.
    static constexpr uint32 MaxSentInvitations     = 50;
    static constexpr uint32 MaxReceivedInvitations = 50;
    static constexpr uint32 MaxFriends             = 200;
    // Retail expires a friend invitation after 30 days.
    static constexpr time_t InvitationLifetime     = 30 * 24 * 60 * 60;

private:
    BnetFriendsMgr() = default;

    void AddFriendshipEdge(uint32 accountId, BnetFriendship friendship, bool persist);
    void EraseInvitation(uint64 invitationId, bool persist);
    BnetAccountIdentity& AllocateBattleTag(BnetAccountIdentity& identity);
    bool IsBattleTagTaken(std::string_view name, uint16 discriminator) const;

    // Sessions of an account that have called friends.v2 Subscribe and are therefore entitled to
    // listener notifications. Subscriptions are held as game-account ids, not WorldSession pointers,
    // so a session that logs out without unsubscribing can never leave a dangling pointer behind -
    // the id simply stops resolving and is pruned here.
    std::vector<WorldSession*> GetSubscribedSessions(uint32 bnetAccountId);

    // ---- push helpers (see BnetFriendsMgr.cpp for the notification shapes) --------------------
    void PushFriendAdded(uint32 toAccountId, uint32 friendAccountId);
    void PushFriendRemoved(uint32 toAccountId, uint32 friendAccountId);
    void PushReceivedInvitationAdded(uint32 toAccountId, BnetFriendInvitation const& invitation);
    void PushSentInvitationAdded(uint32 toAccountId, BnetFriendInvitation const& invitation);
    void PushInvitationRemoved(uint32 senderId, uint32 targetId, uint64 invitationId, uint32 reason);
    void PushFriendStateUpdated(uint32 toAccountId, uint32 friendAccountId);

    std::unordered_map<uint32, BnetAccountIdentity> _identities;
    std::unordered_map<uint32, std::vector<BnetFriendship>> _friends;
    std::unordered_map<uint64, BnetFriendInvitation> _invitations;
    // bnet account id -> game account ids subscribed to friends.v2 notifications.
    std::unordered_map<uint32, std::vector<uint32>> _subscribers;
    uint64 _nextInvitationId = 1;
};

#define sBnetFriendsMgr BnetFriendsMgr::instance()

#endif // TRINITYCORE_BNET_FRIENDS_MGR_H
