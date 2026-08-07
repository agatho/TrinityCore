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

#include "WorldserverServiceDispatcher.h"
#include "BattlenetRpcErrorCodes.h"

Battlenet::WorldserverServiceDispatcher::WorldserverServiceDispatcher()
{
    AddService<WorldserverService<account::v1::AccountService>>();
    AddService<WorldserverService<account::v2::client::AccountService>>();
    AddService<WorldserverService<authentication::v1::AuthenticationService>>();
    AddService<WorldserverService<authentication::v2::client::AuthenticationService>>();
    AddService<WorldserverService<block_list::v1::client::BlockListService>>();
    AddService<Services::ClubMembershipService>();
    AddService<Services::ClubService>();
    AddService<WorldserverService<connection::v1::ConnectionService>>();
    AddService<WorldserverService<friends::v1::FriendsService>>();
    // friends::v2 is what the 68275 client drives; the bare template wrapper it used to be registered
    // as answered every method with ERROR_RPC_NOT_IMPLEMENTED. friends::v1 above is left as transport
    // only on purpose - this client does not call it.
    AddService<Services::FriendsService>();
    AddService<WorldserverService<game_utilities::v1::GameUtilitiesService>>();
    AddService<Services::GameUtilitiesService>();
    AddService<WorldserverService<notification::v1::NotificationService>>();
    AddService<WorldserverService<notification::v2::client::NotificationService>>();
    AddService<WorldserverService<presence::v1::PresenceService>>();
    AddService<WorldserverService<presence::v2::client::PresenceService>>();
    AddService<WorldserverService<report::v1::ReportService>>();
    AddService<WorldserverService<report::v2::ReportService>>();
    AddService<Services::WorldserverReportService>();
    AddService<WorldserverService<resources::v1::ResourcesService>>();
    AddService<WorldserverService<whisper::v2::client::WhisperService>>();
}

void Battlenet::WorldserverServiceDispatcher::Dispatch(WorldSession* session, uint32 serviceHash, uint32 token, uint32 methodId, MessageBuffer buffer)
{
    auto itr = _dispatchers.find(serviceHash);
    if (itr != _dispatchers.end())
    {
        itr->second(session, token, methodId, std::move(buffer));
        return;
    }

    // Same defect as the bnetserver dispatcher: an unregistered service hash was logged and silently dropped,
    // leaving the client's RPC token outstanding forever. Answer with a real RPC error.
    TC_LOG_DEBUG("session.rpc", "{} tried to call invalid service 0x{:X}", session->GetPlayerInfo(), serviceHash);
    session->SendBattlenetResponse(serviceHash, methodId, token, uint32(ERROR_RPC_INVALID_SERVICE));
}

Battlenet::WorldserverServiceDispatcher& Battlenet::WorldserverServiceDispatcher::Instance()
{
    static WorldserverServiceDispatcher instance;
    return instance;
}
