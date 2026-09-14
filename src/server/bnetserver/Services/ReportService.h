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

#ifndef TRINITYCORE_BNET_REPORT_SERVICE_H
#define TRINITYCORE_BNET_REPORT_SERVICE_H

#include "Service.h"
#include "Client/api/client/v3/report_service.pb.h"

namespace Battlenet::Services
{
    namespace V3
    {
        // report.v3 was registered as a bare transport wrapper, so every Battle.net-UI report
        // (right-click a BattleTag / a club message / an entity -> Report) was answered
        // ERROR_RPC_NOT_IMPLEMENTED and logged at TC_LOG_ERROR - i.e. discarded. This subclass persists
        // the report to `battlenet_account_report` so it survives as reviewable state.
        class Report : public Service<report::v3::client::ReportService>
        {
            typedef Service<report::v3::client::ReportService> ReportService;

        public:
            explicit Report(Session* session);

            uint32 HandleSubmitReport(report::v3::client::SubmitReportRequest const* request, NoData* response,
                std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
        };
    }
}

#endif // TRINITYCORE_BNET_REPORT_SERVICE_H
