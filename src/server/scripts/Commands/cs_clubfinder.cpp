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

#include "ClubFinderMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Language.h"
#include "RBAC.h"
#include "ScriptMgr.h"

using namespace Trinity::ChatCommands;

// Moderation for Club Finder recruitment postings.
//
// A report already marks a posting UnderReview (see TicketHandler). These commands are how a reviewer
// then acts on it: ForceNameChange and ForceDescriptionChange make the guild edit the offending text
// before it can list again - both the client and the server refuse a re-post that leaves it unchanged -
// while Banned and Delisted take a posting out of search entirely.
class clubfinder_commandscript : public CommandScript
{
public:
    clubfinder_commandscript() : CommandScript("clubfinder_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "clubfinder list",   HandleClubFinderListCommand,   rbac::RBAC_PERM_COMMAND_CLUB_FINDER, Console::Yes },
            { "clubfinder info",   HandleClubFinderInfoCommand,   rbac::RBAC_PERM_COMMAND_CLUB_FINDER, Console::Yes },
            { "clubfinder flag",   HandleClubFinderFlagCommand,   rbac::RBAC_PERM_COMMAND_CLUB_FINDER, Console::Yes },
            { "clubfinder unflag", HandleClubFinderUnflagCommand, rbac::RBAC_PERM_COMMAND_CLUB_FINDER, Console::Yes },
        };
        return commandTable;
    }

    static uint32 ParseFlag(std::string_view name)
    {
        if (name == "review")      return CLUB_FINDER_POSTING_FLAG_UNDER_REVIEW;
        if (name == "rename")      return CLUB_FINDER_POSTING_FLAG_FORCE_NAME_CHANGE;
        if (name == "rewrite")     return CLUB_FINDER_POSTING_FLAG_FORCE_DESCRIPTION_CHANGE;
        if (name == "banned")      return CLUB_FINDER_POSTING_FLAG_BANNED;
        if (name == "fake")        return CLUB_FINDER_POSTING_FLAG_FAKE_POST;
        if (name == "delisted")    return CLUB_FINDER_POSTING_FLAG_POST_DELISTED;
        if (name == "pendingdel")  return CLUB_FINDER_POSTING_FLAG_PENDING_DELETE;
        return 0;
    }

    static std::string DescribeFlags(uint32 flags)
    {
        std::string out;
        auto add = [&](uint32 flag, char const* name)
        {
            if (!(flags & flag))
                return;
            if (!out.empty())
                out += ", ";
            out += name;
        };

        add(CLUB_FINDER_POSTING_FLAG_NEEDS_CACHE_UPDATE, "NeedsCacheUpdate");
        add(CLUB_FINDER_POSTING_FLAG_FORCE_DESCRIPTION_CHANGE, "ForceDescriptionChange");
        add(CLUB_FINDER_POSTING_FLAG_FORCE_NAME_CHANGE, "ForceNameChange");
        add(CLUB_FINDER_POSTING_FLAG_UNDER_REVIEW, "UnderReview");
        add(CLUB_FINDER_POSTING_FLAG_BANNED, "Banned");
        add(CLUB_FINDER_POSTING_FLAG_FAKE_POST, "FakePost");
        add(CLUB_FINDER_POSTING_FLAG_PENDING_DELETE, "PendingDelete");
        add(CLUB_FINDER_POSTING_FLAG_POST_DELISTED, "PostDelisted");

        return out.empty() ? "none" : out;
    }

    static bool HandleClubFinderListCommand(ChatHandler* handler)
    {
        std::vector<ClubFinderPosting const*> postings = sClubFinderMgr->GetAllPostings();
        if (postings.empty())
        {
            handler->PSendSysMessage("No club finder postings.");
            return true;
        }

        for (ClubFinderPosting const* posting : postings)
        {
            Guild* guild = sGuildMgr->GetGuildById(posting->ClubId);
            handler->PSendSysMessage("Posting {} | guild {} ({}) | \"{}\"{} | flags: {}",
                posting->PostingId, posting->ClubId, guild ? guild->GetName() : "<missing>", posting->Name,
                ClubFinderMgr::IsPostingExpired(*posting) ? " [expired]" : "", DescribeFlags(posting->DisplayFlags));
        }

        return true;
    }

    static bool HandleClubFinderInfoCommand(ChatHandler* handler, uint32 postingId)
    {
        ClubFinderPosting const* posting = sClubFinderMgr->GetPosting(postingId);
        if (!posting)
        {
            handler->PSendSysMessage("No posting {}.", postingId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        Guild* guild = sGuildMgr->GetGuildById(posting->ClubId);
        handler->PSendSysMessage("Posting {} for guild {} ({})", posting->PostingId, posting->ClubId,
            guild ? guild->GetName() : "<missing>");
        handler->PSendSysMessage("  Name: {}", posting->Name);
        handler->PSendSysMessage("  Description: {}", posting->Description);
        handler->PSendSysMessage("  Item level: {} | specs: 0x{:X} | settings: 0x{:X}",
            posting->ItemLevelRequirement, posting->RecruitingSpecs, posting->RecruitmentFlags);
        handler->PSendSysMessage("  Flags: {}{}", DescribeFlags(posting->DisplayFlags),
            ClubFinderMgr::IsPostingExpired(*posting) ? " [expired]" : "");
        handler->PSendSysMessage("  Applications: {}", sClubFinderMgr->GetApplicationsForPosting(postingId).size());
        return true;
    }

    static bool HandleClubFinderFlagCommand(ChatHandler* handler, uint32 postingId, std::string flagName)
    {
        uint32 const flag = ParseFlag(flagName);
        if (!flag)
        {
            handler->PSendSysMessage("Unknown flag. Use: review, rename, rewrite, banned, fake, delisted, pendingdel.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!sClubFinderMgr->AddPostingDisplayFlags(postingId, flag))
        {
            handler->PSendSysMessage("No posting {}, or it already has that flag.", postingId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Posting {} flagged: {}", postingId, flagName);
        return true;
    }

    static bool HandleClubFinderUnflagCommand(ChatHandler* handler, uint32 postingId, std::string flagName)
    {
        uint32 const flag = ParseFlag(flagName);
        if (!flag)
        {
            handler->PSendSysMessage("Unknown flag. Use: review, rename, rewrite, banned, fake, delisted, pendingdel.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!sClubFinderMgr->RemovePostingDisplayFlags(postingId, flag))
        {
            handler->PSendSysMessage("No posting {}, or it does not have that flag.", postingId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Posting {} cleared: {}", postingId, flagName);
        return true;
    }
};

void AddSC_clubfinder_commandscript()
{
    new clubfinder_commandscript();
}
