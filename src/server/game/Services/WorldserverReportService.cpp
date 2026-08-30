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

#include "WorldserverReportService.h"
#include "BattlenetRpcErrorCodes.h"
#include "DatabaseEnv.h"
#include "Log.h"

namespace Battlenet::Services
{
WorldserverReportService::WorldserverReportService(WorldSession* session) : BaseService(session)
{
}

uint32 WorldserverReportService::HandleSubmitReport(report::v3::client::SubmitReportRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& /*continuation*/)
{
    // SubmitReportRequest carries exactly one of the three option groups (a oneof), plus a free-text description.
    uint64 targetAccountId = 0;
    uint32 issueType = 0;
    uint32 source = 0;
    uint64 clubId = 0;
    uint64 streamId = 0;
    std::string entityId;
    std::string entityType;

    if (request->has_user_options())
    {
        report::v3::client::UserOptions const& options = request->user_options();
        targetAccountId = options.target_account_id();
        issueType = options.type();
        source = options.source();
    }
    else if (request->has_club_options())
    {
        report::v3::client::ClubOptions const& options = request->club_options();
        clubId = options.club_id();
        streamId = options.stream_id();
        issueType = options.type();
        source = options.source();
    }
    else if (request->has_entity_options())
    {
        report::v3::client::EntityOptions const& options = request->entity_options();
        entityId = options.entity_id();
        entityType = options.entity_type();
    }
    else
    {
        // Nothing identifies what is being reported - refuse rather than record an empty row.
        return ERROR_RPC_MALFORMED_REQUEST;
    }

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_INS_BNET_REPORT);
    stmt->setUInt32(0, _session->GetBattlenetAccountId());
    stmt->setUInt64(1, targetAccountId);
    stmt->setUInt32(2, issueType);
    stmt->setUInt32(3, source);
    stmt->setUInt64(4, clubId);
    stmt->setUInt64(5, streamId);
    stmt->setString(6, entityId);
    stmt->setString(7, entityType);
    stmt->setString(8, request->user_description());
    LoginDatabase.Execute(stmt);

    TC_LOG_INFO("session.rpc", "{} submitted a Battle.net report (issueType {}, source {}, target account {}, club {})",
        _session->GetPlayerInfo(), issueType, source, targetAccountId, clubId);

    return ERROR_OK;
}
}
