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
#include "ObjectMgr.h"
#include "Player.h"
#include "SupportMgr.h"
#include "TicketPackets.h"
#include "World.h"

namespace
{
// How long after delivery a whisper can still back a CMSG_CHAT_REPORT_FILTERED. The client reports
// the moment its SPAMCHECK hits the incoming line, so this only has to cover the trip; it is kept
// short so a report can never be attached to a conversation from hours ago.
// UNVERIFIED: this server's choice - retail answers the opcode with nothing, so no recording can
// show what window it uses, if any.
constexpr time_t FILTERABLE_WHISPER_WINDOW = 5 * MINUTE;

// How long a counted report keeps counting towards Chat.SpamFilterReport.MuteThreshold. Without a
// window the reports of a whole session would add up and a threshold of N could be reached by N
// people over many hours - which is normal traffic for a busy character, not a spam burst.
// UNVERIFIED: same reason as above.
constexpr time_t SPAM_FILTER_REPORT_WINDOW = 15 * MINUTE;
}

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
//
// THE SERVER SIDE PRECONDITION. Every condition this opcode has lives in the client - the writer
// 0x7477A0 is reached only from the SMSG_CHAT handler 0x20A7880 on a SPAMCHECK hit for ChatType 7 -
// so a report that arrives unasked proves nothing on its own. Without a check of our own, any
// session that knows a GUID could report any online player, and N own characters would reach any
// threshold against a victim who never spoke to any of them. So the report is only counted when
// THIS server actually delivered a whisper from the reported player to the reporter, inside
// FILTERABLE_WHISPER_WINDOW, and the entry is taken with the report: one whisper is worth at most
// one report. That is a fact the reporter cannot manufacture, which is what closes the hole.
//
// Deliberately NOT part of the precondition: that the whisper matched one of our own compiled
// patterns. We ship the pattern strings, but the client's SPAMCHECK is its own code, and nothing
// available here proves that std::regex reproduces it - a pattern that fails to compile server side
// is still shipped (ObjectMgr::LoadChatSpamRecords logs it and carries on). Gating on our match
// would turn every such divergence into a filter that silently never fires. The realm having no
// patterns at all IS checked, because then no legitimate report can exist.
//
// UNVERIFIED: what retail DOES with the report, and the two windows at the top of this file. Every source that exists
// for this opcode - the writer at 0x7477A0, its trigger 0x20A7880, the SPAMCHECK hit, ChatType 7 -
// proves only WHEN the CLIENT sends it;
// there is no Lua binding (no hit for "ReportFiltered" anywhere in wow-ui-source or
// wow-ui-source-12.0.5), no DB2 field and no documented reaction, and the packet is answered with
// nothing on the wire, so no recording can show the effect either. Counting one report per
// distinct reporter and muting at a configured threshold is this server's own policy, chosen
// because it is the conservative reading of a report the player never triggered by hand; it is
// off by default. A retail-faithful behaviour cannot be derived from anything available here.
void WorldSession::HandleChatReportFiltered(WorldPackets::Ticket::ChatReportFiltered& packet)
{
    Player* reporter = GetPlayer();
    if (!reporter || packet.SenderGUID.IsEmpty() || packet.SenderGUID == reporter->GetGUID())
        return;

    Player* reported = ObjectAccessor::FindConnectedPlayer(packet.SenderGUID);
    if (!reported)
        return;     // the sender logged out in the meantime - nothing to count against

    // The reported player must have whispered the reporter, recently, and that whisper must not
    // have been reported already. Anything else is an unsolicited claim about a third party.
    if (!reported->GetSession()->ConsumeFilterableWhisper(reporter->GetGUID()))
    {
        TC_LOG_DEBUG("chat.spam", "Discarded a spam filter report from {} against {}: no whisper was "
            "delivered between them inside the report window", GetPlayerInfo(),
            reported->GetSession()->GetPlayerInfo());
        return;
    }

    reported->GetSession()->AddChatSpamFilterReport(reporter->GetGUID());
}

// Called for every whisper this session actually delivers, so that a later
// CMSG_CHAT_REPORT_FILTERED naming this session can be checked against something. Only recorded
// while the realm ships patterns at all - with an empty `chat_spam_record` the client has nothing
// to filter against and no legitimate report can arrive.
void WorldSession::NoteFilterableWhisper(ObjectGuid receiverGuid)
{
    if (sObjectMgr->GetChatSpamRecords().empty())
        return;

    time_t const now = GameTime::GetGameTime();

    // Bounded by the window, not by the number of people spoken to over a session.
    std::erase_if(_filterableWhispers, [now](std::pair<ObjectGuid const, time_t> const& entry) { return entry.second <= now; });

    _filterableWhispers[receiverGuid] = now + FILTERABLE_WHISPER_WINDOW;
}

// True exactly once per delivered whisper: takes the entry with it, so one whisper cannot back two
// reports.
bool WorldSession::ConsumeFilterableWhisper(ObjectGuid reporterGuid)
{
    auto itr = _filterableWhispers.find(reporterGuid);
    if (itr == _filterableWhispers.end())
        return false;

    bool const stillOpen = itr->second > GameTime::GetGameTime();
    _filterableWhispers.erase(itr);
    return stillOpen;
}

// Counts one distinct reporter and, above the configured threshold, mutes for the same duration the
// flood filter uses. Reports age out after SPAM_FILTER_REPORT_WINDOW, so it takes that many distinct
// clients within one window - reports scattered over a whole session do not add up.
// UNVERIFIED: threshold, window and mute duration alike - see HandleChatReportFiltered above.
// Neither the count-once-per-reporter rule nor reusing ChatFlood.MuteTime has a retail source; both
// are this server's choice.
void WorldSession::AddChatSpamFilterReport(ObjectGuid reporterGuid)
{
    time_t const now = GameTime::GetGameTime();

    std::erase_if(_chatSpamFilterReporters, [now](std::pair<ObjectGuid const, time_t> const& entry) { return entry.second <= now; });

    // Refreshing an existing reporter must not add a second count, so insert first and only report
    // the size once - the map holds one entry per reporter either way.
    _chatSpamFilterReporters[reporterGuid] = now + SPAM_FILTER_REPORT_WINDOW;

    uint32 reportCount = uint32(_chatSpamFilterReporters.size());
    TC_LOG_INFO("chat.spam", "Client side spam filter report against {}: {} distinct reporter(s) in the last {} seconds",
        GetPlayerInfo(), reportCount, uint32(SPAM_FILTER_REPORT_WINDOW));

    uint32 threshold = sWorld->getIntConfig(CONFIG_CHAT_SPAM_FILTER_REPORT_MUTE_THRESHOLD);
    if (!threshold || reportCount < threshold)
        return;

    time_t newMute = now + sWorld->getIntConfig(CONFIG_CHATFLOOD_MUTE_TIME);
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
