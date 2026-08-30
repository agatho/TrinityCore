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

#ifndef TRINITYCORE_BNET_PRESENCE_MGR_H
#define TRINITYCORE_BNET_PRESENCE_MGR_H

#include "Define.h"
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

class Player;
class WorldSession;

// Battle.net presence, backing bgs.protocol.presence.v1.PresenceService (OriginalHash 0xFA0796FF)
// and bgs.protocol.presence.v2.client.PresenceService (OriginalHash 0x138D200C).
//
// This is the presence seam BnetFriendsMgr deliberately left open: friends.v2 carries no presence
// field at all, so who is online, on which character, in which zone is a separate service keyed by
// the same battlenet account id.
//
// ---------------------------------------------------------------------------------------------
// WHAT IS AND IS NOT PUT ON THE WIRE
// ---------------------------------------------------------------------------------------------
// Both protocol versions carry presence as an untyped bag of {key, Variant} pairs. The key is
// presence.v1 FieldKey{program, group, field, unique_id} / presence.v2 PresenceFieldKey{title_id,
// group, field, unique_id}. That (group, field) numbering is assigned by Blizzard and is not
// derivable from the client binary offline; a wrong number renders as a *blank* line under the
// friend's name rather than as a visible failure, which would look exactly like a working feature.
//
// So this manager keeps two separate things apart:
//
//   1. Server-derived state (online flag, character name / level / race / class / faction / zone).
//      Tracked here, pushed as an event, and persisted to `battlenet_game_account_presence` so the
//      bnetserver's character-select view can read it. It is NOT serialised into presence fields,
//      because that would require inventing key numbers.
//
//   2. Client-authored rich presence fields. The client itself calls Update / v2 Update with
//      PresenceFieldKey values it chose. Those are stored verbatim, echoed verbatim on Query, and
//      fanned out verbatim to subscribers. Nothing is invented: every key on the wire came off the
//      wire.
//
// Every key the client sends is logged at TC_LOG_DEBUG("session.rpc") so the real numbering can be
// harvested from a live client instead of from a packet capture.
//
// Threading: CMSG_BATTLENET_REQUEST is PROCESS_THREADUNSAFE, so every entry point runs on the world
// update thread, like SocialMgr. No locking.

// The character a game account is currently logged in as, if any.
struct BnetCharacterPresence
{
    uint64 CharacterGuid = 0;   // ObjectGuid low part (the character's DB guid)
    std::string CharacterName;
    uint32 RealmAddress  = 0;   // virtual realm address, as sent in SMSG_LOGIN_VERIFY_WORLD's world
    uint8 Level          = 0;
    uint8 RaceId         = 0;
    uint8 ClassId        = 0;
    uint8 FactionId      = 0;   // TeamId: 0 alliance, 1 horde, 2 neutral
    uint32 ZoneId        = 0;
    uint32 AreaId        = 0;
};

// One live game account of a battlenet account.
struct BnetGameAccountPresence
{
    uint32 GameAccountId = 0;
    uint32 BnetAccountId = 0;
    bool IsOnline        = false;
    time_t OnlineTime    = 0;   // when the session came up; presence.v2 PresenceOnlineGameAccount.online_time_us / 1000000
    time_t UpdateTime    = 0;
    bool HasCharacter    = false;
    BnetCharacterPresence Character;
};

// One client-authored presence field, stored exactly as it arrived.
struct BnetPresenceField
{
    // presence.v1 FieldKey.program / presence.v2 PresenceFieldKey.title_id
    uint32 Program       = 0;
    uint32 Group         = 0;   // FieldKey.group / PresenceFieldKey.group
    uint32 Field         = 0;   // FieldKey.field / PresenceFieldKey.field
    uint64 UniqueId      = 0;   // FieldKey.unique_id / PresenceFieldKey.unique_id
    uint64 UpdatedTimeUs = 0;   // presence.v2 PresenceField.updated_time_us
    // Serialised bgs.protocol.Variant (v1 store) or bgs.protocol.v2.Variant (v2 store). Kept as a
    // blob so this header stays free of protobuf, the same split BnetFriendsMgr / FriendsService use.
    std::string VariantBlob;

    bool SameKey(BnetPresenceField const& other) const
    {
        return Program == other.Program && Group == other.Group && Field == other.Field && UniqueId == other.UniqueId;
    }
};

// One live subscription. Held as a game account id, never as a WorldSession*, so a session that
// drops without unsubscribing can never leave a dangling pointer behind.
struct BnetPresenceSubscription
{
    uint32 SubscriberGameAccountId = 0;
    uint32 TargetAccountId         = 0;   // battlenet account being watched
    bool UseV2                     = true;
    // presence.v1 only: the EntityId the client subscribed with, echoed verbatim in every
    // PresenceState it is sent back. Never reinterpreted beyond reading `low` as the account id.
    uint64 EntityHigh              = 0;
    uint64 EntityLow               = 0;
};

class TC_GAME_API BnetPresenceMgr
{
public:
    static BnetPresenceMgr* instance();

    void Load();

    // ---- events ------------------------------------------------------------------------------
    // Each of these updates the tracked state, persists it, and pushes a presence state update to
    // every subscriber entitled to see it.

    void OnSessionOnline(WorldSession* session);
    void OnSessionOffline(WorldSession* session);
    void OnCharacterLogin(Player* player);
    void OnCharacterLogout(Player* player);
    void OnZoneChanged(Player* player, uint32 zoneId, uint32 areaId);

    // ---- reads -------------------------------------------------------------------------------

    BnetGameAccountPresence const* GetGameAccountPresence(uint32 gameAccountId) const;

    // bgs.protocol.EntityId{high, low} -> battlenet account id, 0 when it cannot be resolved.
    // Takes the two halves rather than the message so this header stays free of protobuf.
    uint32 ResolveAccountIdFromEntity(uint64 entityHigh, uint64 entityLow) const;
    std::vector<BnetGameAccountPresence const*> GetAccountPresence(uint32 bnetAccountId) const;
    bool IsAccountOnline(uint32 bnetAccountId) const;

    // ---- subscriptions -----------------------------------------------------------------------

    // Returns a BattlenetRpcErrorCodes value. Subscribing immediately pushes the target's current
    // state, which is the only way the initial value can reach the client - both Subscribe responses
    // are NoData / an empty BatchSubscribeResponse.
    uint32 Subscribe(WorldSession* subscriber, uint32 targetAccountId, bool useV2, uint64 entityHigh = 0, uint64 entityLow = 0);
    bool Unsubscribe(WorldSession* subscriber, uint32 targetAccountId);
    void UnsubscribeAll(WorldSession* subscriber);

    // ---- client-authored rich fields ---------------------------------------------------------

    std::vector<BnetPresenceField> const& GetRichFields(uint32 bnetAccountId, bool v2) const;
    // erase maps presence.v1 FieldOperation CLEAR / presence.v2 PresenceFieldUpdate.delete_.
    void ApplyRichField(uint32 bnetAccountId, BnetPresenceField field, bool v2, bool erase);
    void ClearRichFields(uint32 bnetAccountId);

    // ---- pushes ------------------------------------------------------------------------------

    // Pushes the target's full current state to one specific subscriber session.
    void PushStateTo(WorldSession* subscriber, uint32 targetAccountId, bool useV2, uint64 entityHigh, uint64 entityLow);
    // Pushes the target's full current state to every subscriber that is allowed to see it.
    void PushStateToSubscribers(uint32 targetAccountId);

    // Retail caps how many accounts one client may watch.
    static constexpr uint32 MaxSubscriptions = 512;

    // Entity / program constants, taken from what this project's own bnetserver already hands the
    // client in authentication::v1::LogonResult (bnetserver/Services/AuthenticationService.cpp:294-300):
    //   account      EntityId.high = 0x0100000000000000
    //   game account EntityId.high = 0x0200000200576F57 = (type 2 << 56) | (region 2 << 32) | 0x576F57
    // so neither the program id nor the region id is invented here.
    static constexpr uint64 AccountEntityHigh     = UI64LIT(0x100000000000000);
    static constexpr uint64 GameAccountEntityHigh = UI64LIT(0x200000200576F57);
    static constexpr uint32 ProgramWoW            = 5730135;   // 'W','o','W' packed big-endian = 0x576F57
    static constexpr uint32 RegionId              = 2;

private:
    BnetPresenceMgr() = default;

    BnetGameAccountPresence& GetOrCreate(uint32 gameAccountId, uint32 bnetAccountId);
    void Persist(BnetGameAccountPresence const& presence);
    std::vector<BnetPresenceSubscription> CollectLiveSubscriptions(uint32 targetAccountId);

    // game account id -> presence
    std::unordered_map<uint32, BnetGameAccountPresence> _presence;
    // battlenet account id -> client-authored fields, one store per protocol version because the
    // Variant blobs are different message types (bgs.protocol.Variant vs bgs.protocol.v2.Variant).
    std::unordered_map<uint32, std::vector<BnetPresenceField>> _richFieldsV1;
    std::unordered_map<uint32, std::vector<BnetPresenceField>> _richFieldsV2;
    // watched battlenet account id -> subscriptions
    std::unordered_map<uint32, std::vector<BnetPresenceSubscription>> _subscriptions;
};

#define sBnetPresenceMgr BnetPresenceMgr::instance()

#endif // TRINITYCORE_BNET_PRESENCE_MGR_H
