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

#include "WorldSession.h"
#include "BattlenetPackets.h"
#include "WorldserverServiceDispatcher.h"
#include "CryptoRandom.h"
#include "GameTime.h"
#include "ObjectDefines.h"
#include "RealmList.h"
#include "Util.h"
#include <algorithm>

namespace
{
    // Lifetime of an in-game realm-list ticket. Long enough that a player can sit on the realm-select screen and
    // still join, bounded so a captured ticket is not indefinitely replayable.
    constexpr Seconds RealmListTicketDuration = Seconds(3600);
}

void WorldSession::SetBattlenetRealmListTicket(std::string ticket, Seconds duration)
{
    _realmListTicket = std::move(ticket);
    _realmListTicketExpiry = GameTime::GetSystemTime() + duration;
}

bool WorldSession::IsBattlenetRealmListTicketValid(std::string_view presented) const
{
    if (_realmListTicket.empty() || GameTime::GetSystemTime() > _realmListTicketExpiry)
        return false;

    // The client does not echo the ticket on every command; when it does, it must match exactly.
    return presented.empty() || presented == _realmListTicket;
}

void WorldSession::HandleBattlenetChangeRealmTicket(WorldPackets::Battlenet::ChangeRealmTicket& changeRealmTicket)
{
    WorldPackets::Battlenet::ChangeRealmTicketResponse realmListTicket;
    realmListTicket.Token = changeRealmTicket.Token;

    // This used to answer Allow = true unconditionally with the constant literal "WorldserverRealmListTicket",
    // i.e. it handed a realm-change ticket to anyone who asked and the ticket was the same for every session, so
    // WorldserverGameUtilitiesService::JoinRealm could not distinguish a legitimate holder from a forged one.
    // Refuse when the client's join secret is unusable, or when this session has no realm it may join.
    bool secretUsable = std::ranges::any_of(changeRealmTicket.Secret, [](uint8 b) { return b != 0; });
    bool haveRealms = !sRealmList->GetRealmList(GetClientBuild(), GetSecurity(), "").empty();

    if (!secretUsable || !haveRealms)
    {
        realmListTicket.Allow = false;
        SendPacket(realmListTicket.Write());
        return;
    }

    SetRealmListSecret(changeRealmTicket.Secret);

    // 32 random bytes, hex-encoded, bound to this session with an expiry.
    std::string ticket = ByteArrayToHexStr(Trinity::Crypto::GetRandomBytes<32>());
    SetBattlenetRealmListTicket(ticket, RealmListTicketDuration);

    realmListTicket.Allow = true;
    realmListTicket.Ticket << ticket;

    SendPacket(realmListTicket.Write());
}

void WorldSession::HandleBattlenetRequest(WorldPackets::Battlenet::Request& request)
{
    sServiceDispatcher.Dispatch(this, request.Method.GetServiceHash(), request.Method.Token, request.Method.GetMethodId(), std::move(request.Data));
}

void WorldSession::SendBattlenetResponse(uint32 serviceHash, uint32 methodId, uint32 token, pb::Message const* response)
{
    WorldPackets::Battlenet::Response bnetResponse;
    bnetResponse.BnetStatus = ERROR_OK;
    bnetResponse.Method.Type = MAKE_PAIR64(methodId, serviceHash);
    bnetResponse.Method.ObjectId = 1;
    bnetResponse.Method.Token = token;

    if (int32 size = response->ByteSize(); size > 0)
    {
        bnetResponse.Data.resize(size);
        response->SerializePartialToArray(bnetResponse.Data.data(), size);
    }

    SendPacket(bnetResponse.Write());
}

void WorldSession::SendBattlenetResponse(uint32 serviceHash, uint32 methodId, uint32 token, uint32 status)
{
    WorldPackets::Battlenet::Response bnetResponse;
    bnetResponse.BnetStatus = BattlenetRpcErrorCode(status);
    bnetResponse.Method.Type = MAKE_PAIR64(methodId, serviceHash);
    bnetResponse.Method.ObjectId = 1;
    bnetResponse.Method.Token = token;

    SendPacket(bnetResponse.Write());
}

void WorldSession::SendBattlenetRequest(uint32 serviceHash, uint32 methodId, pb::Message const* request, std::function<void(MessageBuffer)> callback)
{
    _battlenetResponseCallbacks[_battlenetRequestToken] = std::move(callback);
    SendBattlenetRequest(serviceHash, methodId, request);
}

void WorldSession::SendBattlenetRequest(uint32 serviceHash, uint32 methodId, pb::Message const* request)
{
    WorldPackets::Battlenet::Notification notification;
    notification.Method.Type = MAKE_PAIR64(methodId, serviceHash);
    notification.Method.ObjectId = 1;
    notification.Method.Token = _battlenetRequestToken++;

    if (int32 size = request->ByteSize(); size > 0)
    {
        notification.Data.resize(size);
        request->SerializePartialToArray(notification.Data.data(), size);
    }

    SendPacket(notification.Write());
}
