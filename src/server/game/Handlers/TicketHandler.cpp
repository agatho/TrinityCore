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
#include "ClubFinderMgr.h"
#include "Common.h"
#include "DatabaseEnv.h"
#include "SupportMgr.h"
#include "TicketPackets.h"

void WorldSession::HandleGMTicketGetCaseStatusOpcode(WorldPackets::Ticket::GMTicketGetCaseStatus& /*packet*/)
{
    // TODO: Implement GmCase and handle this packet properly
    WorldPackets::Ticket::GMTicketCaseStatus status;
    SendPacket(status.Write());
}

void WorldSession::HandleGMTicketSystemStatusOpcode(WorldPackets::Ticket::GMTicketGetSystemStatus& /*packet*/)
{
    // Note: This only disables the ticket UI at client side and is not fully reliable
    // Note: This disables the whole customer support UI after trying to send a ticket in disabled state (MessageBox: "GM Help Tickets are currently unavaiable."). UI remains disabled until the character relogs.
    WorldPackets::Ticket::GMTicketSystemStatus response;
    response.Status = sSupportMgr->GetSupportSystemStatus() ? GMTICKET_QUEUE_STATUS_ENABLED : GMTICKET_QUEUE_STATUS_DISABLED;
    SendPacket(response.Write());
}

void WorldSession::HandleSubmitUserFeedback(WorldPackets::Ticket::SubmitUserFeedback& userFeedback)
{
    if (userFeedback.IsSuggestion)
    {
        if (!sSupportMgr->GetSuggestionSystemStatus())
            return;

        SuggestionTicket* ticket = new SuggestionTicket(GetPlayer());
        ticket->SetPosition(userFeedback.Header.MapID, userFeedback.Header.Position);
        ticket->SetFacing(userFeedback.Header.Facing);
        ticket->SetNote(userFeedback.Note);

        sSupportMgr->AddTicket(ticket);
    }
    else
    {
        if (!sSupportMgr->GetBugSystemStatus())
            return;

        BugTicket* ticket = new BugTicket(GetPlayer());
        ticket->SetPosition(userFeedback.Header.MapID, userFeedback.Header.Position);
        ticket->SetFacing(userFeedback.Header.Facing);
        ticket->SetNote(userFeedback.Note);

        sSupportMgr->AddTicket(ticket);
    }
}

void WorldSession::HandleSupportTicketSubmitComplaint(WorldPackets::Ticket::SupportTicketSubmitComplaint& packet)
{
    if (!sSupportMgr->GetComplaintSystemStatus())
        return;

    ComplaintTicket* comp = new ComplaintTicket(GetPlayer());
    comp->SetPosition(packet.Header.MapID, packet.Header.Position);
    comp->SetFacing(packet.Header.Facing);
    comp->SetChatLog(packet.ChatLog);
    comp->SetTargetCharacterGuid(packet.TargetCharacterGUID);
    comp->SetReportType(ReportType(packet.ReportType));
    comp->SetMajorCategory(ReportMajorCategory(packet.MajorCategory));
    comp->SetMinorCategoryFlags(ReportMinorCategory(packet.MinorCategoryFlags));
    comp->SetNote(packet.Note);

    sSupportMgr->AddTicket(comp);

    // A reported Club Finder posting is flagged for review. The client reads this back through
    // C_ClubFinder.GetStatusOfPostingFromClubId, so the report becomes visible state rather than only a
    // GM ticket, and a reviewer can escalate it to Banned or a forced name/description change. Without
    // this the posting id the client took the trouble to send is simply discarded.
    if (packet.ClubFinderInfo)
    {
        ReportType const reportType = ReportType(packet.ReportType);
        if (reportType == ReportType::ClubFinderPosting || reportType == ReportType::ClubFinderApplicant)
            if (sClubFinderMgr->AddPostingDisplayFlags(uint32(packet.ClubFinderInfo->PostingID), CLUB_FINDER_POSTING_FLAG_UNDER_REVIEW))
                TC_LOG_INFO("network", "ClubFinder: posting {} (club {}) flagged under review after a report by {}.",
                    packet.ClubFinderInfo->PostingID, packet.ClubFinderInfo->ClubID, GetPlayerInfo());
    }
}

void WorldSession::HandleBugReportOpcode(WorldPackets::Ticket::BugReport& bugReport)
{
    // Note: There is no way to trigger this with standard UI except /script ReportBug("text")
    if (!sSupportMgr->GetBugSystemStatus())
        return;

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_BUG_REPORT);
    stmt->setString(0, bugReport.Text);
    stmt->setString(1, bugReport.DiagInfo);
    CharacterDatabase.Execute(stmt);
}

void WorldSession::HandleComplaint(WorldPackets::Ticket::Complaint& packet)
{    // NOTE: all chat messages from this spammer are automatically ignored by the spam reporter until logout in case of chat spam.
     // if it's mail spam - ALL mails from this spammer are automatically removed by client

    WorldPackets::Ticket::ComplaintResult result;
    result.ComplaintType = packet.ComplaintType;
    result.Result = 0;
    SendPacket(result.Write());
}
