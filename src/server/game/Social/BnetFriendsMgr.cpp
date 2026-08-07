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

#include "BnetFriendsMgr.h"
#include "BattlenetRpcErrorCodes.h"
#include "BnetBlockListMgr.h"
#include "DatabaseEnv.h"
#include "FriendsService.h"
#include "GameTime.h"
#include "Log.h"
#include "Timer.h"
#include "World.h"
#include "WorldSession.h"
#include "WorldserverService.h"
#include "Client/api/client/v2/friends_listener.pb.h"
#include "Client/api/common/v1/invitation_types.pb.h"
#include <algorithm>
#include <cctype>

namespace
{
    std::vector<BnetFriendship> const EmptyFriendList;

    // Comma-separated "1,2,3" <-> vector<uint32>, for battlenet_account_friend.titleTags.
    std::vector<uint32> ParseTitleTags(std::string_view text)
    {
        std::vector<uint32> ids;
        size_t pos = 0;
        while (pos < text.size())
        {
            size_t comma = text.find(',', pos);
            std::string_view token = text.substr(pos, comma == std::string_view::npos ? std::string_view::npos : comma - pos);
            if (!token.empty())
            {
                uint32 value = 0;
                bool valid = true;
                for (char c : token)
                {
                    if (c < '0' || c > '9')
                    {
                        valid = false;
                        break;
                    }
                    value = value * 10 + uint32(c - '0');
                }

                if (valid)
                    ids.push_back(value);
            }

            if (comma == std::string_view::npos)
                break;

            pos = comma + 1;
        }

        return ids;
    }

    std::string FormatTitleTags(std::vector<uint32> const& ids)
    {
        std::string text;
        for (uint32 id : ids)
        {
            if (!text.empty())
                text += ',';
            text += std::to_string(id);
        }
        return text;
    }

    // Case-insensitive compare of the name half of a BattleTag.
    bool EqualsCaseInsensitive(std::string_view left, std::string_view right)
    {
        if (left.size() != right.size())
            return false;

        for (size_t i = 0; i < left.size(); ++i)
            if (std::tolower(static_cast<unsigned char>(left[i])) != std::tolower(static_cast<unsigned char>(right[i])))
                return false;

        return true;
    }

    // Splits "Name#1234". Returns false if the input is not in BattleTag form.
    bool SplitBattleTag(std::string_view battleTag, std::string_view& name, uint16& discriminator)
    {
        size_t hash = battleTag.rfind('#');
        if (hash == std::string_view::npos || hash == 0 || hash + 1 >= battleTag.size())
            return false;

        std::string_view discText = battleTag.substr(hash + 1);
        uint32 value = 0;
        for (char c : discText)
        {
            if (c < '0' || c > '9')
                return false;
            value = value * 10 + uint32(c - '0');
            if (value > 0xFFFF)
                return false;
        }

        name = battleTag.substr(0, hash);
        discriminator = uint16(value);
        return true;
    }

    // A BattleTag name derived from an email local part: letters and digits only, leading letter,
    // capitalised, capped at 12 characters. Used to give existing accounts a usable handle, because
    // TrinityCore has never stored one and without it nobody can be invited.
    std::string DeriveBattleTagName(std::string_view email)
    {
        std::string name;
        for (char c : email.substr(0, email.find('@')))
        {
            if (std::isalnum(static_cast<unsigned char>(c)))
                name += c;

            if (name.size() >= 12)
                break;
        }

        while (!name.empty() && std::isdigit(static_cast<unsigned char>(name.front())))
            name.erase(name.begin());

        if (name.empty())
            name = "Player";

        name[0] = char(std::toupper(static_cast<unsigned char>(name[0])));
        for (size_t i = 1; i < name.size(); ++i)
            name[i] = char(std::tolower(static_cast<unsigned char>(name[i])));

        return name;
    }
}

std::string BnetAccountIdentity::GetBattleTag() const
{
    if (!HasBattleTag())
        return std::string();

    return BattleTagName + '#' + std::to_string(BattleTagDiscriminator);
}

BnetFriendsMgr* BnetFriendsMgr::instance()
{
    static BnetFriendsMgr instance;
    return &instance;
}

void BnetFriendsMgr::Load()
{
    uint32 oldMSTime = getMSTime();

    _identities.clear();
    _friends.clear();
    _invitations.clear();
    _subscribers.clear();
    _nextInvitationId = 1;

    // ---- identities -------------------------------------------------------------------------
    if (QueryResult result = LoginDatabase.Query("SELECT id, email, battle_tag, battle_tag_disc FROM battlenet_accounts"))
    {
        do
        {
            Field* fields = result->Fetch();

            BnetAccountIdentity identity;
            identity.Id = fields[0].GetUInt32();
            identity.Email = fields[1].GetString();
            identity.BattleTagName = fields[2].GetStringOrNull().value_or(std::string());
            identity.BattleTagDiscriminator = uint16(fields[3].GetUInt16OrNull().value_or(0));

            _identities[identity.Id] = std::move(identity);
        }
        while (result->NextRow());
    }

    // Give every account that lacks one a BattleTag. Done after the whole table is in memory so the
    // discriminator search sees all existing tags at once.
    uint32 generated = 0;
    for (auto& [accountId, identity] : _identities)
    {
        if (identity.HasBattleTag())
            continue;

        AllocateBattleTag(identity);
        ++generated;
    }

    // ---- friend graph -----------------------------------------------------------------------
    uint32 skippedFriends = 0;
    if (QueryResult result = LoginDatabase.Query("SELECT accountId, friendId, level, note, titleTags, creationTime FROM battlenet_account_friend"))
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 accountId = fields[0].GetUInt32();

            BnetFriendship friendship;
            friendship.FriendId = fields[1].GetUInt32();
            friendship.Level = fields[2].GetUInt32();
            friendship.Note = fields[3].GetString();
            friendship.TitleTagIds = ParseTitleTags(fields[4].GetStringView());
            friendship.CreationTime = time_t(fields[5].GetUInt64());

            if (!_identities.count(accountId) || !_identities.count(friendship.FriendId))
            {
                ++skippedFriends;
                continue;
            }

            _friends[accountId].push_back(std::move(friendship));
        }
        while (result->NextRow());
    }

    // ---- invitations ------------------------------------------------------------------------
    time_t now = GameTime::GetGameTime();
    uint32 expired = 0;
    uint32 skippedInvites = 0;
    if (QueryResult result = LoginDatabase.Query("SELECT id, senderId, targetId, targetTag, level, note, titleTags, creationTime, expirationTime FROM battlenet_account_friend_invite"))
    {
        do
        {
            Field* fields = result->Fetch();

            BnetFriendInvitation invitation;
            invitation.Id = fields[0].GetUInt64();
            invitation.SenderId = fields[1].GetUInt32();
            invitation.TargetId = fields[2].GetUInt32();
            invitation.TargetTag = fields[3].GetString();
            invitation.Level = fields[4].GetUInt32();
            invitation.Note = fields[5].GetString();
            invitation.TitleTagIds = ParseTitleTags(fields[6].GetStringView());
            invitation.CreationTime = time_t(fields[7].GetUInt64());
            invitation.ExpirationTime = time_t(fields[8].GetUInt64());

            _nextInvitationId = std::max(_nextInvitationId, invitation.Id + 1);

            if (!_identities.count(invitation.SenderId) || !_identities.count(invitation.TargetId))
            {
                ++skippedInvites;
                LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_DEL_BNET_FRIEND_INVITE);
                stmt->setUInt64(0, invitation.Id);
                LoginDatabase.Execute(stmt);
                continue;
            }

            if (invitation.ExpirationTime != 0 && invitation.ExpirationTime <= now)
            {
                ++expired;
                LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_DEL_BNET_FRIEND_INVITE);
                stmt->setUInt64(0, invitation.Id);
                LoginDatabase.Execute(stmt);
                continue;
            }

            _invitations[invitation.Id] = std::move(invitation);
        }
        while (result->NextRow());
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} Battle.net account identities ({} BattleTags generated), {} friendship edges and {} pending invitations in {} ms",
        _identities.size(), generated, _friends.size(), _invitations.size(), GetMSTimeDiffToNow(oldMSTime));

    if (skippedFriends || skippedInvites || expired)
        TC_LOG_INFO("server.loading", ">> Battle.net friends: skipped {} friendship edges and {} invitations referencing missing accounts, dropped {} expired invitations",
            skippedFriends, skippedInvites, expired);
}

bool BnetFriendsMgr::IsBattleTagTaken(std::string_view name, uint16 discriminator) const
{
    return std::ranges::any_of(_identities, [&](decltype(_identities)::value_type const& pair)
    {
        return pair.second.BattleTagDiscriminator == discriminator && EqualsCaseInsensitive(pair.second.BattleTagName, name);
    });
}

BnetAccountIdentity& BnetFriendsMgr::AllocateBattleTag(BnetAccountIdentity& identity)
{
    std::string name = DeriveBattleTagName(identity.Email);

    uint16 discriminator = 0;
    for (uint16 candidate = 1000; candidate <= 9999; ++candidate)
    {
        if (!IsBattleTagTaken(name, candidate))
        {
            discriminator = candidate;
            break;
        }
    }

    if (!discriminator)
    {
        // 9000 accounts share one derived name. Nothing sane to do but leave the account tagless;
        // it simply cannot be invited by BattleTag until an administrator assigns one.
        TC_LOG_ERROR("server.loading", "Battle.net account {} could not be given a BattleTag: every discriminator for name '{}' is taken.",
            identity.Id, name);
        return identity;
    }

    identity.BattleTagName = std::move(name);
    identity.BattleTagDiscriminator = discriminator;

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_BNET_BATTLE_TAG);
    stmt->setString(0, identity.BattleTagName);
    stmt->setUInt16(1, identity.BattleTagDiscriminator);
    stmt->setUInt32(2, identity.Id);
    LoginDatabase.Execute(stmt);

    return identity;
}

BnetAccountIdentity const* BnetFriendsMgr::GetIdentity(uint32 bnetAccountId) const
{
    auto itr = _identities.find(bnetAccountId);
    return itr != _identities.end() ? &itr->second : nullptr;
}

BnetAccountIdentity const* BnetFriendsMgr::GetOrLoadIdentity(uint32 bnetAccountId)
{
    if (BnetAccountIdentity const* identity = GetIdentity(bnetAccountId))
        return identity;

    if (!bnetAccountId)
        return nullptr;

    // The account was created after this worldserver started (.bnetaccount create). One indexed row.
    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_BNET_ACCOUNT_IDENTITY);
    stmt->setUInt32(0, bnetAccountId);
    PreparedQueryResult result = LoginDatabase.Query(stmt);
    if (!result)
        return nullptr;

    Field* fields = result->Fetch();

    BnetAccountIdentity identity;
    identity.Id = fields[0].GetUInt32();
    identity.Email = fields[1].GetString();
    identity.BattleTagName = fields[2].GetStringOrNull().value_or(std::string());
    identity.BattleTagDiscriminator = uint16(fields[3].GetUInt16OrNull().value_or(0));

    uint32 id = identity.Id;
    BnetAccountIdentity& stored = _identities.emplace(id, std::move(identity)).first->second;
    if (!stored.HasBattleTag())
        AllocateBattleTag(stored);

    return &stored;
}

uint32 BnetFriendsMgr::FindAccountByBattleTag(std::string_view battleTag) const
{
    std::string_view name;
    uint16 discriminator = 0;
    if (!SplitBattleTag(battleTag, name, discriminator))
        return 0;

    for (auto const& [accountId, identity] : _identities)
        if (identity.BattleTagDiscriminator == discriminator && EqualsCaseInsensitive(identity.BattleTagName, name))
            return accountId;

    return 0;
}

uint32 BnetFriendsMgr::FindAccountByEmail(std::string_view email) const
{
    for (auto const& [accountId, identity] : _identities)
        if (EqualsCaseInsensitive(identity.Email, email))
            return accountId;

    return 0;
}

std::vector<BnetFriendship> const& BnetFriendsMgr::GetFriends(uint32 bnetAccountId) const
{
    auto itr = _friends.find(bnetAccountId);
    return itr != _friends.end() ? itr->second : EmptyFriendList;
}

BnetFriendship const* BnetFriendsMgr::GetFriendship(uint32 bnetAccountId, uint32 friendId) const
{
    for (BnetFriendship const& friendship : GetFriends(bnetAccountId))
        if (friendship.FriendId == friendId)
            return &friendship;

    return nullptr;
}

bool BnetFriendsMgr::IsFriend(uint32 bnetAccountId, uint32 otherAccountId) const
{
    return GetFriendship(bnetAccountId, otherAccountId) != nullptr;
}

std::vector<BnetFriendInvitation> BnetFriendsMgr::GetSentInvitations(uint32 bnetAccountId) const
{
    std::vector<BnetFriendInvitation> invitations;
    for (auto const& [invitationId, invitation] : _invitations)
        if (invitation.SenderId == bnetAccountId)
            invitations.push_back(invitation);

    std::ranges::sort(invitations, {}, &BnetFriendInvitation::Id);
    return invitations;
}

std::vector<BnetFriendInvitation> BnetFriendsMgr::GetReceivedInvitations(uint32 bnetAccountId) const
{
    std::vector<BnetFriendInvitation> invitations;
    for (auto const& [invitationId, invitation] : _invitations)
        if (invitation.TargetId == bnetAccountId)
            invitations.push_back(invitation);

    std::ranges::sort(invitations, {}, &BnetFriendInvitation::Id);
    return invitations;
}

BnetFriendInvitation const* BnetFriendsMgr::GetInvitation(uint64 invitationId) const
{
    auto itr = _invitations.find(invitationId);
    return itr != _invitations.end() ? &itr->second : nullptr;
}

void BnetFriendsMgr::AddFriendshipEdge(uint32 accountId, BnetFriendship friendship, bool persist)
{
    std::vector<BnetFriendship>& edges = _friends[accountId];

    auto existing = std::ranges::find(edges, friendship.FriendId, &BnetFriendship::FriendId);
    if (existing != edges.end())
        *existing = friendship;
    else
        edges.push_back(friendship);

    if (!persist)
        return;

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_REP_BNET_FRIEND);
    stmt->setUInt32(0, accountId);
    stmt->setUInt32(1, friendship.FriendId);
    stmt->setUInt32(2, friendship.Level);
    stmt->setString(3, friendship.Note);
    stmt->setString(4, FormatTitleTags(friendship.TitleTagIds));
    stmt->setUInt64(5, uint64(friendship.CreationTime));
    LoginDatabase.Execute(stmt);
}

void BnetFriendsMgr::EraseInvitation(uint64 invitationId, bool persist)
{
    _invitations.erase(invitationId);

    if (!persist)
        return;

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_DEL_BNET_FRIEND_INVITE);
    stmt->setUInt64(0, invitationId);
    LoginDatabase.Execute(stmt);
}

uint32 BnetFriendsMgr::SendInvitation(uint32 senderId, uint32 targetId, std::string_view targetTagTyped, uint32 level,
    std::string_view note, std::vector<uint32> titleTagIds)
{
    if (!senderId || !targetId)
        return ERROR_INVALID_TARGET_ID;

    if (senderId == targetId)
        return ERROR_FRIENDS_INVALID_INVITATION;

    if (!GetIdentity(senderId))
        return ERROR_INVALID_AGENT_ID;

    if (!GetIdentity(targetId))
        return ERROR_INVALID_TARGET_ID;

    if (IsFriend(senderId, targetId))
        return ERROR_FRIENDS_FRIENDSHIP_ALREADY_EXISTS;

    // block_list.v1: a blocked account cannot invite, in either direction. Checked before any of the
    // capacity limits so a blocked sender is told the truth rather than "the target's list is full".
    if (sBnetBlockListMgr->IsBlocked(targetId, senderId))
        return ERROR_FRIENDS_INVITER_IS_BLOCKED_BY_INVITEE;

    if (sBnetBlockListMgr->IsBlocked(senderId, targetId))
        return ERROR_FRIENDS_ACCOUNT_BLOCKED;

    if (GetFriends(senderId).size() >= MaxFriends)
        return ERROR_FRIENDS_INVITER_AT_MAX_FRIENDS;

    if (GetFriends(targetId).size() >= MaxFriends)
        return ERROR_FRIENDS_INVITEE_AT_MAX_FRIENDS;

    if (note.size() > 128)
        return ERROR_FRIENDS_NOTE_MAX_SIZE_EXCEEDED;

    // Mutual invitation: the target already invited the sender. Creating a second, opposing invitation
    // would leave both clients showing a pending invite that neither side can resolve, so this is
    // treated as the sender accepting - which is also what retail does.
    for (auto const& [existingId, existing] : _invitations)
    {
        if (existing.SenderId == targetId && existing.TargetId == senderId)
            return AcceptInvitation(senderId, existingId, level);
    }

    uint32 sentCount = 0;
    uint32 receivedByTarget = 0;
    for (auto const& [invitationId, invitation] : _invitations)
    {
        if (invitation.SenderId == senderId)
        {
            ++sentCount;
            if (invitation.TargetId == targetId)
                return ERROR_FRIENDS_INVITATION_ALREADY_EXISTS;
        }

        if (invitation.TargetId == targetId)
            ++receivedByTarget;
    }

    if (sentCount >= MaxSentInvitations)
        return ERROR_FRIENDS_TOO_MANY_SENT_INVITATIONS;

    if (receivedByTarget >= MaxReceivedInvitations)
        return ERROR_FRIENDS_TOO_MANY_RECEIVED_INVITATIONS;

    time_t now = GameTime::GetGameTime();

    BnetFriendInvitation invitation;
    invitation.Id = _nextInvitationId++;
    invitation.SenderId = senderId;
    invitation.TargetId = targetId;
    invitation.TargetTag.assign(targetTagTyped);
    invitation.Level = level;
    invitation.Note.assign(note);
    invitation.TitleTagIds = std::move(titleTagIds);
    invitation.CreationTime = now;
    invitation.ExpirationTime = now + InvitationLifetime;

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_INS_BNET_FRIEND_INVITE);
    stmt->setUInt64(0, invitation.Id);
    stmt->setUInt32(1, invitation.SenderId);
    stmt->setUInt32(2, invitation.TargetId);
    stmt->setString(3, invitation.TargetTag);
    stmt->setUInt32(4, invitation.Level);
    stmt->setString(5, invitation.Note);
    stmt->setString(6, FormatTitleTags(invitation.TitleTagIds));
    stmt->setUInt64(7, uint64(invitation.CreationTime));
    stmt->setUInt64(8, uint64(invitation.ExpirationTime));
    LoginDatabase.Execute(stmt);

    BnetFriendInvitation const& stored = (_invitations[invitation.Id] = std::move(invitation));

    // The sender's own client needs the invitation echoed back so it appears under "Pending"; the
    // target needs it so the invite toast fires without a relog.
    PushSentInvitationAdded(stored.SenderId, stored);
    PushReceivedInvitationAdded(stored.TargetId, stored);

    return ERROR_OK;
}

uint32 BnetFriendsMgr::AcceptInvitation(uint32 accepterId, uint64 invitationId, uint32 level)
{
    auto itr = _invitations.find(invitationId);
    if (itr == _invitations.end())
        return ERROR_FRIENDS_INVALID_INVITATION;

    BnetFriendInvitation invitation = itr->second;

    // Only the recipient may accept.
    if (invitation.TargetId != accepterId)
        return ERROR_INVALID_AGENT_ID;

    if (IsFriend(invitation.SenderId, invitation.TargetId))
    {
        EraseInvitation(invitationId, true);
        return ERROR_FRIENDS_FRIENDSHIP_ALREADY_EXISTS;
    }

    if (GetFriends(invitation.SenderId).size() >= MaxFriends)
        return ERROR_FRIENDS_INVITER_AT_MAX_FRIENDS;

    if (GetFriends(invitation.TargetId).size() >= MaxFriends)
        return ERROR_FRIENDS_INVITEE_AT_MAX_FRIENDS;

    time_t now = GameTime::GetGameTime();

    // Levels are per-direction and both come from the client, never from us:
    //  - the inviter's view of the new friend carries the level the accepter granted
    //    (AcceptInvitationOptions.level),
    //  - the accepter's view carries the level the inviter offered (SendInvitationOptions.level).
    BnetFriendship inviterView;
    inviterView.FriendId = invitation.TargetId;
    inviterView.Level = level;
    inviterView.Note = invitation.Note;   // the note the inviter attached to the invitation carries over
    inviterView.CreationTime = now;

    BnetFriendship accepterView;
    accepterView.FriendId = invitation.SenderId;
    accepterView.Level = invitation.Level;
    accepterView.CreationTime = now;

    AddFriendshipEdge(invitation.SenderId, inviterView, true);
    AddFriendshipEdge(invitation.TargetId, accepterView, true);

    EraseInvitation(invitationId, true);

    // Both ends drop the invitation and gain the friend, live.
    PushInvitationRemoved(invitation.SenderId, invitation.TargetId, invitationId, bgs::protocol::INVITATION_REMOVED_REASON_ACCEPTED);
    PushFriendAdded(invitation.SenderId, invitation.TargetId);
    PushFriendAdded(invitation.TargetId, invitation.SenderId);

    return ERROR_OK;
}

uint32 BnetFriendsMgr::RemoveInvitation(uint32 callerId, uint64 invitationId, uint32 reason, bool callerMustBeSender)
{
    auto itr = _invitations.find(invitationId);
    if (itr == _invitations.end())
        return ERROR_FRIENDS_INVALID_INVITATION;

    BnetFriendInvitation invitation = itr->second;

    // Revoke is the sender's verb, ignore/decline the recipient's.
    if (callerMustBeSender ? invitation.SenderId != callerId : invitation.TargetId != callerId)
        return ERROR_INVALID_AGENT_ID;

    EraseInvitation(invitationId, true);
    PushInvitationRemoved(invitation.SenderId, invitation.TargetId, invitationId, reason);

    return ERROR_OK;
}

uint32 BnetFriendsMgr::RevokeAllInvitations(uint32 senderId)
{
    std::vector<BnetFriendInvitation> revoked;
    for (auto const& [invitationId, invitation] : _invitations)
        if (invitation.SenderId == senderId)
            revoked.push_back(invitation);

    for (BnetFriendInvitation const& invitation : revoked)
        _invitations.erase(invitation.Id);

    if (!revoked.empty())
    {
        LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_DEL_BNET_FRIEND_INVITES_BY_SENDER);
        stmt->setUInt32(0, senderId);
        LoginDatabase.Execute(stmt);
    }

    for (BnetFriendInvitation const& invitation : revoked)
        PushInvitationRemoved(invitation.SenderId, invitation.TargetId, invitation.Id, bgs::protocol::INVITATION_REMOVED_REASON_REVOKED);

    return ERROR_OK;
}

uint32 BnetFriendsMgr::RemoveFriend(uint32 callerId, uint32 targetId)
{
    if (!IsFriend(callerId, targetId))
        return ERROR_FRIENDS_FRIENDSHIP_DOES_NOT_EXIST;

    // Friendship is symmetric on retail: removing drops both directions.
    if (auto itr = _friends.find(callerId); itr != _friends.end())
        std::erase_if(itr->second, [targetId](BnetFriendship const& f) { return f.FriendId == targetId; });

    if (auto itr = _friends.find(targetId); itr != _friends.end())
        std::erase_if(itr->second, [callerId](BnetFriendship const& f) { return f.FriendId == callerId; });

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_DEL_BNET_FRIEND_EDGE);
    stmt->setUInt32(0, callerId);
    stmt->setUInt32(1, targetId);
    stmt->setUInt32(2, targetId);
    stmt->setUInt32(3, callerId);
    LoginDatabase.Execute(stmt);

    PushFriendRemoved(callerId, targetId);
    PushFriendRemoved(targetId, callerId);

    return ERROR_OK;
}

uint32 BnetFriendsMgr::UpdateFriendState(uint32 callerId, uint32 targetId, bool hasNote, std::string_view note,
    bool hasTitleTags, std::vector<uint32> titleTagIds)
{
    if (!hasNote && !hasTitleTags)
    {
        // Nothing this server can store was supplied. Answering ERROR_OK here would be a silent ack.
        return ERROR_FRIENDS_UPDATE_FRIEND_STATE_FAILED;
    }

    if (hasNote && note.size() > 128)
        return ERROR_FRIENDS_NOTE_MAX_SIZE_EXCEEDED;

    auto listItr = _friends.find(callerId);
    if (listItr == _friends.end())
        return ERROR_FRIENDS_FRIENDSHIP_DOES_NOT_EXIST;

    auto edge = std::ranges::find(listItr->second, targetId, &BnetFriendship::FriendId);
    if (edge == listItr->second.end())
        return ERROR_FRIENDS_FRIENDSHIP_DOES_NOT_EXIST;

    if (hasNote)
        edge->Note.assign(note);

    if (hasTitleTags)
        edge->TitleTagIds = std::move(titleTagIds);

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_BNET_FRIEND_STATE);
    stmt->setString(0, edge->Note);
    stmt->setString(1, FormatTitleTags(edge->TitleTagIds));
    stmt->setUInt32(2, callerId);
    stmt->setUInt32(3, targetId);
    LoginDatabase.Execute(stmt);

    // The note is the caller's private view of the friend, so only the caller's own sessions are told.
    PushFriendStateUpdated(callerId, targetId);

    return ERROR_OK;
}

std::vector<WorldSession*> BnetFriendsMgr::GetOnlineSessions(uint32 bnetAccountId) const
{
    std::vector<WorldSession*> sessions;
    if (!bnetAccountId)
        return sessions;

    for (auto const& [gameAccountId, session] : sWorld->GetAllSessions())
        if (session && session->GetBattlenetAccountId() == bnetAccountId)
            sessions.push_back(session);

    return sessions;
}

std::vector<WorldSession*> BnetFriendsMgr::GetSubscribedSessions(uint32 bnetAccountId)
{
    std::vector<WorldSession*> sessions;

    auto itr = _subscribers.find(bnetAccountId);
    if (itr == _subscribers.end())
        return sessions;

    std::vector<uint32>& gameAccountIds = itr->second;
    std::erase_if(gameAccountIds, [&](uint32 gameAccountId)
    {
        WorldSession* session = sWorld->FindSession(gameAccountId);
        // Dropped or re-used by a different battlenet account - the subscription is dead either way.
        if (!session || session->GetBattlenetAccountId() != bnetAccountId)
            return true;

        sessions.push_back(session);
        return false;
    });

    if (gameAccountIds.empty())
        _subscribers.erase(itr);

    return sessions;
}

bool BnetFriendsMgr::IsSubscribed(WorldSession const* session) const
{
    auto itr = _subscribers.find(session->GetBattlenetAccountId());
    if (itr == _subscribers.end())
        return false;

    return std::ranges::find(itr->second, session->GetAccountId()) != itr->second.end();
}

bool BnetFriendsMgr::Unsubscribe(WorldSession* session)
{
    auto itr = _subscribers.find(session->GetBattlenetAccountId());
    if (itr == _subscribers.end())
        return false;

    size_t removed = std::erase(itr->second, session->GetAccountId());
    if (itr->second.empty())
        _subscribers.erase(itr);

    return removed != 0;
}

void BnetFriendsMgr::Subscribe(WorldSession* session)
{
    uint32 accountId = session->GetBattlenetAccountId();

    std::vector<uint32>& gameAccountIds = _subscribers[accountId];
    if (std::ranges::find(gameAccountIds, session->GetAccountId()) == gameAccountIds.end())
        gameAccountIds.push_back(session->GetAccountId());

    std::vector<BnetFriendship> const& friendList = GetFriends(accountId);
    if (!friendList.empty())
    {
        bgs::protocol::friends::v2::client::FriendAddedNotification notification;
        notification.set_agent_account_id(accountId);
        for (BnetFriendship const& friendship : friendList)
            Battlenet::Services::FriendsService::FillFriend(notification.add_friends(), friendship, true, true, true);

        Battlenet::WorldserverService<bgs::protocol::friends::v2::client::FriendsListener>(session).OnFriendAdded(&notification, true, true);
    }

    if (std::vector<BnetFriendInvitation> received = GetReceivedInvitations(accountId); !received.empty())
    {
        bgs::protocol::friends::v2::client::ReceivedInvitationAddedNotification notification;
        notification.set_agent_account_id(accountId);
        for (BnetFriendInvitation const& invitation : received)
            Battlenet::Services::FriendsService::FillReceivedInvitation(notification.add_invitations(), invitation);

        Battlenet::WorldserverService<bgs::protocol::friends::v2::client::FriendsListener>(session).OnReceivedInvitationAdded(&notification, true, true);
    }

    if (std::vector<BnetFriendInvitation> sent = GetSentInvitations(accountId); !sent.empty())
    {
        bgs::protocol::friends::v2::client::SentInvitationAddedNotification notification;
        notification.set_agent_account_id(accountId);
        for (BnetFriendInvitation const& invitation : sent)
            Battlenet::Services::FriendsService::FillSentInvitation(notification.add_invitations(), invitation);

        Battlenet::WorldserverService<bgs::protocol::friends::v2::client::FriendsListener>(session).OnSentInvitationAdded(&notification, true, true);
    }
}

void BnetFriendsMgr::PushFriendAdded(uint32 toAccountId, uint32 friendAccountId)
{
    std::vector<WorldSession*> sessions = GetSubscribedSessions(toAccountId);
    if (sessions.empty())
        return;

    BnetFriendship const* friendship = GetFriendship(toAccountId, friendAccountId);
    if (!friendship)
        return;

    bgs::protocol::friends::v2::client::FriendAddedNotification notification;
    notification.set_agent_account_id(toAccountId);
    Battlenet::Services::FriendsService::FillFriend(notification.add_friends(), *friendship, true, true, true);

    for (WorldSession* session : sessions)
        Battlenet::WorldserverService<bgs::protocol::friends::v2::client::FriendsListener>(session).OnFriendAdded(&notification, true, true);
}

void BnetFriendsMgr::PushFriendRemoved(uint32 toAccountId, uint32 friendAccountId)
{
    std::vector<WorldSession*> sessions = GetSubscribedSessions(toAccountId);
    if (sessions.empty())
        return;

    bgs::protocol::friends::v2::client::FriendRemovedNotification notification;
    notification.set_agent_account_id(toAccountId);
    notification.add_assignments()->set_id(friendAccountId);

    for (WorldSession* session : sessions)
        Battlenet::WorldserverService<bgs::protocol::friends::v2::client::FriendsListener>(session).OnFriendRemoved(&notification, true, true);
}

void BnetFriendsMgr::PushReceivedInvitationAdded(uint32 toAccountId, BnetFriendInvitation const& invitation)
{
    std::vector<WorldSession*> sessions = GetSubscribedSessions(toAccountId);
    if (sessions.empty())
        return;

    bgs::protocol::friends::v2::client::ReceivedInvitationAddedNotification notification;
    notification.set_agent_account_id(toAccountId);
    Battlenet::Services::FriendsService::FillReceivedInvitation(notification.add_invitations(), invitation);

    for (WorldSession* session : sessions)
        Battlenet::WorldserverService<bgs::protocol::friends::v2::client::FriendsListener>(session).OnReceivedInvitationAdded(&notification, true, true);
}

void BnetFriendsMgr::PushSentInvitationAdded(uint32 toAccountId, BnetFriendInvitation const& invitation)
{
    std::vector<WorldSession*> sessions = GetSubscribedSessions(toAccountId);
    if (sessions.empty())
        return;

    bgs::protocol::friends::v2::client::SentInvitationAddedNotification notification;
    notification.set_agent_account_id(toAccountId);
    Battlenet::Services::FriendsService::FillSentInvitation(notification.add_invitations(), invitation);

    for (WorldSession* session : sessions)
        Battlenet::WorldserverService<bgs::protocol::friends::v2::client::FriendsListener>(session).OnSentInvitationAdded(&notification, true, true);
}

void BnetFriendsMgr::PushInvitationRemoved(uint32 senderId, uint32 targetId, uint64 invitationId, uint32 reason)
{
    // The sender loses it from "Pending sent", the recipient from "Pending received". Two different
    // listener methods, same invitation id and removal reason.
    if (std::vector<WorldSession*> senderSessions = GetSubscribedSessions(senderId); !senderSessions.empty())
    {
        bgs::protocol::friends::v2::client::SentInvitationRemovedNotification notification;
        notification.set_agent_account_id(senderId);

        bgs::protocol::friends::v2::client::RemovedInvitationAssignment* assignment = notification.add_assignments();
        assignment->set_invitation_id(invitationId);
        assignment->set_reason(reason);

        for (WorldSession* session : senderSessions)
            Battlenet::WorldserverService<bgs::protocol::friends::v2::client::FriendsListener>(session).OnSentInvitationRemoved(&notification, true, true);
    }

    if (std::vector<WorldSession*> targetSessions = GetSubscribedSessions(targetId); !targetSessions.empty())
    {
        bgs::protocol::friends::v2::client::ReceivedInvitationRemovedNotification notification;
        notification.set_agent_account_id(targetId);

        bgs::protocol::friends::v2::client::RemovedInvitationAssignment* assignment = notification.add_assignments();
        assignment->set_invitation_id(invitationId);
        assignment->set_reason(reason);

        for (WorldSession* session : targetSessions)
            Battlenet::WorldserverService<bgs::protocol::friends::v2::client::FriendsListener>(session).OnReceivedInvitationRemoved(&notification, true, true);
    }
}

void BnetFriendsMgr::PushFriendStateUpdated(uint32 toAccountId, uint32 friendAccountId)
{
    std::vector<WorldSession*> sessions = GetSubscribedSessions(toAccountId);
    if (sessions.empty())
        return;

    BnetFriendship const* friendship = GetFriendship(toAccountId, friendAccountId);
    if (!friendship)
        return;

    bgs::protocol::friends::v2::client::UpdateFriendStateNotification notification;
    notification.set_agent_account_id(toAccountId);

    bgs::protocol::friends::v2::client::FriendStateAssignment* assignment = notification.add_assignments();
    assignment->set_id(friendship->FriendId);
    assignment->set_level(friendship->Level);
    assignment->set_note(friendship->Note);
    for (uint32 titleTagId : friendship->TitleTagIds)
        assignment->mutable_title_tag()->add_ids(titleTagId);

    for (WorldSession* session : sessions)
        Battlenet::WorldserverService<bgs::protocol::friends::v2::client::FriendsListener>(session).OnUpdateFriendState(&notification, true, true);
}
