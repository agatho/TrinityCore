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

#ifndef TRINITYCORE_WORLDSERVER_REPORT_SERVICE_H
#define TRINITYCORE_WORLDSERVER_REPORT_SERVICE_H

#include "WorldserverService.h"
#include "Client/api/client/v3/report_service.pb.h"

namespace Battlenet::Services
{
// In-game Battle.net-UI reports (right-click a BattleTag / a club message / an entity -> Report) tunnel through
// CMSG_BATTLENET_REQUEST to this service. It was registered as a bare transport wrapper, so every report was
// answered ERROR_RPC_NOT_IMPLEMENTED and logged at TC_LOG_ERROR - i.e. discarded. Reports are now persisted to
// `battlenet_account_report` in the auth DB, the same sink the bnetserver copy writes to.
class WorldserverReportService : public WorldserverService<report::v3::client::ReportService>
{
    typedef WorldserverService<report::v3::client::ReportService> BaseService;

public:
    explicit WorldserverReportService(WorldSession* session);

    uint32 HandleSubmitReport(report::v3::client::SubmitReportRequest const* request, NoData* response,
        std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
};
}

#endif // TRINITYCORE_WORLDSERVER_REPORT_SERVICE_H
