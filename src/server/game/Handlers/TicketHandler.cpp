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
#include "Common.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SupportMgr.h"
#include "TicketPackets.h"
#include "World.h"

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

// CMSG_CHAT_REPORT_FILTERED (0x2C0004). The client is telling us that IT hid an incoming whisper
// because the text hit one of the patterns we shipped with SMSG_EXPECTED_SPAM_RECORDS. That makes
// this a spam filter hit report about the SENDER, not a complaint filed by a user - so it is
// counted, not turned into a ticket. One ticket per hit (what origin/feature/bnet-presence does
// here) would let one client fill the ticket queue, and it misreads the trigger: the player never
// pressed anything.
//
// The count lives on the reported player's own session, keyed by distinct reporter, so a single
// modified client cannot inflate it and it disappears with the character. Enforcement is opt-in
// through Chat.SpamFilterReport.MuteThreshold; the default only logs.
void WorldSession::HandleChatReportFiltered(WorldPackets::Ticket::ChatReportFiltered& packet)
{
    Player* reporter = GetPlayer();
    if (!reporter || packet.SenderGUID.IsEmpty() || packet.SenderGUID == reporter->GetGUID())
        return;

    Player* reported = ObjectAccessor::FindConnectedPlayer(packet.SenderGUID);
    if (!reported)
        return;     // the sender logged out in the meantime - nothing to count against

    reported->GetSession()->AddChatSpamFilterReport(reporter->GetGUID());
}

// Counts one distinct reporter and, above the configured threshold, mutes for the same duration the
// flood filter uses.
void WorldSession::AddChatSpamFilterReport(ObjectGuid reporterGuid)
{
    if (!_chatSpamFilterReporters.insert(reporterGuid).second)
        return;     // already counted this reporter

    uint32 reportCount = uint32(_chatSpamFilterReporters.size());
    TC_LOG_INFO("chat.spam", "Client side spam filter report against {}: {} distinct reporter(s)",
        GetPlayerInfo(), reportCount);

    uint32 threshold = sWorld->getIntConfig(CONFIG_CHAT_SPAM_FILTER_REPORT_MUTE_THRESHOLD);
    if (!threshold || reportCount < threshold)
        return;

    time_t newMute = GameTime::GetGameTime() + sWorld->getIntConfig(CONFIG_CHATFLOOD_MUTE_TIME);
    if (m_muteTime < newMute)
        m_muteTime = newMute;

    TC_LOG_WARN("chat.spam", "Muted {} - {} distinct clients filtered its messages as spam",
        GetPlayerInfo(), reportCount);
}

void WorldSession::HandleComplaint(WorldPackets::Ticket::Complaint& packet)
{    // NOTE: all chat messages from this spammer are automatically ignored by the spam reporter until logout in case of chat spam.
     // if it's mail spam - ALL mails from this spammer are automatically removed by client

    WorldPackets::Ticket::ComplaintResult result;
    result.ComplaintType = packet.ComplaintType;
    result.Result = 0;
    SendPacket(result.Write());
}
