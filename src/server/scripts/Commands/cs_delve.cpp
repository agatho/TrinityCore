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

/* ScriptData
Name: delve_commandscript
%Complete: 80
Comment: GM commands for delves testing
Category: commandscripts
EndScriptData */

#include "ScriptMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "DelveMgr.h"
#include "DelvesCompanion.h"
#include "DelvesDefines.h"
#include "DelvesRewards.h"
#include "DelvesSeason.h"
#include "Language.h"
#include "Player.h"
#include "RBAC.h"
#include "WorldSession.h"

using namespace Trinity::ChatCommands;

class delve_commandscript : public CommandScript
{
public:
    delve_commandscript() : CommandScript("delve_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable delveCompanionTable =
        {
            { "level",    HandleDelveCompanionLevelCommand,   rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
            { "role",     HandleDelveCompanionRoleCommand,    rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
            { "info",     HandleDelveCompanionInfoCommand,    rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
        };

        static ChatCommandTable delveCommandTable =
        {
            { "info",       HandleDelveInfoCommand,          rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
            { "enter",      HandleDelveEnterCommand,         rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
            { "season",     HandleDelveSeasonCommand,        rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
            { "bountiful",  HandleDelveBountifulCommand,     rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
            { "progress",   HandleDelveProgressCommand,      rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
            { "settier",    HandleDelveSetTierCommand,       rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
            { "companion",  delveCompanionTable },
        };

        static ChatCommandTable commandTable =
        {
            { "delve", delveCommandTable },
        };

        return commandTable;
    }

    // .delve enter <mapId> - teleport directly into a delve instance
    static bool HandleDelveEnterCommand(ChatHandler* handler, uint32 mapId)
    {
        Player* player = handler->GetSession()->GetPlayer();

        Delves::DelveTemplate const* tmpl = sDelveMgr->GetDelveTemplate(mapId);
        if (!tmpl)
        {
            handler->PSendSysMessage("No delve template found for mapId {}. Available delves:", mapId);
            for (auto const& t : sDelveMgr->GetAllDelveTemplates())
                handler->PSendSysMessage("  mapId={} (id={})", t.MapId, t.Id);
            return false;
        }

        handler->PSendSysMessage("Teleporting to delve mapId={} ...", mapId);

        float x = tmpl->CompanionSpawnX;
        float y = tmpl->CompanionSpawnY;
        float z = tmpl->CompanionSpawnZ;
        float o = tmpl->CompanionSpawnO;

        // Use a default position if companion spawn isn't set
        if (x == 0.0f && y == 0.0f && z == 0.0f)
        {
            handler->PSendSysMessage("Warning: No spawn position configured for this delve. Using map origin.");
            // For testing, try a reasonable default
            x = 0.0f; y = 0.0f; z = 100.0f; o = 0.0f;
        }

        player->TeleportTo(mapId, x, y, z, o);
        return true;
    }

    static bool HandleDelveInfoCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        handler->PSendSysMessage("=== Delve System Info ===");
        handler->PSendSysMessage("Active Season: {}", Delves::DelvesSeason::GetCurrentSeasonNumber());
        handler->PSendSysMessage("Registered Templates: {}", sDelveMgr->GetAllDelveTemplates().size());
        handler->PSendSysMessage("Meets Level Req: {}", Delves::DelvesSeason::MeetsMinimumLevelRequirement(player) ? "Yes" : "No");

        // Show current map info
        if (Delves::DelveTemplate const* tmpl = sDelveMgr->GetDelveTemplate(player->GetMapId()))
        {
            handler->PSendSysMessage("Current map IS a delve: mapId={}, zoneId={}", tmpl->MapId, tmpl->ZoneId);
            if (sDelveMgr->IsDelveCurrentlyBountiful(tmpl->MapChallengeModeId))
                handler->PSendSysMessage("  This delve is currently BOUNTIFUL");
        }
        else
        {
            handler->PSendSysMessage("Current map is NOT a delve (mapId={})", player->GetMapId());
        }

        return true;
    }

    static bool HandleDelveSeasonCommand(ChatHandler* handler)
    {
        uint32 seasonId = Delves::DelvesSeason::GetCurrentSeasonNumber();
        handler->PSendSysMessage("=== Delve Season Info ===");
        handler->PSendSysMessage("Season ID: {}", seasonId);

        if (seasonId > 0)
        {
            handler->PSendSysMessage("Faction ID: {}", Delves::DelvesSeason::GetFactionIdForSeason(seasonId));
            std::vector<int32> spells = Delves::DelvesSeason::GetSeasonSpellIds(seasonId);
            handler->PSendSysMessage("Season Spells: {}", spells.size());
            for (int32 spellId : spells)
                handler->PSendSysMessage("  SpellID: {}", spellId);
        }

        return true;
    }

    static bool HandleDelveBountifulCommand(ChatHandler* handler)
    {
        handler->PSendSysMessage("=== Today's Bountiful Delves ===");
        std::vector<uint32> bountiful = sDelveMgr->GetTodaysBountifulDelves();

        if (bountiful.empty())
        {
            handler->PSendSysMessage("No bountiful delves configured (no templates loaded)");
            return true;
        }

        for (uint32 i = 0; i < bountiful.size(); ++i)
        {
            handler->PSendSysMessage("  [{}] MapChallengeModeId: {}", i + 1, bountiful[i]);
        }

        return true;
    }

    static bool HandleDelveProgressCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        uint32 accountId = player->GetSession()->GetBattlenetAccountId();

        Delves::DelveProgress progress;
        Delves::DelvesRewards::LoadProgress(accountId, progress);

        handler->PSendSysMessage("=== Delve Progress (Account {}) ===", accountId);
        handler->PSendSysMessage("Highest Tier Unlocked: {}", progress.HighestTierUnlocked);
        handler->PSendSysMessage("Weekly Completions: {}", progress.WeeklyCompletions);
        handler->PSendSysMessage("Highest Tier This Week: {}", progress.HighestTierThisWeek);
        handler->PSendSysMessage("Weekly Bountiful: {}", progress.WeeklyBountifulCount);
        handler->PSendSysMessage("Weekly Coffer Shards: {}/{}", progress.WeeklyCofferShards, Delves::MAX_COFFER_KEY_SHARDS_PER_WEEK);
        handler->PSendSysMessage("Great Vault Slots: {}", Delves::DelvesRewards::GetGreatVaultSlotCount(progress.WeeklyCompletions));

        return true;
    }

    static bool HandleDelveSetTierCommand(ChatHandler* handler, uint8 tier)
    {
        if (tier > Delves::MAX_DELVE_TIER)
        {
            handler->PSendSysMessage("Invalid tier {} (max: {})", tier, Delves::MAX_DELVE_TIER);
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();
        uint32 accountId = player->GetSession()->GetBattlenetAccountId();

        Delves::DelveProgress progress;
        Delves::DelvesRewards::LoadProgress(accountId, progress);
        progress.HighestTierUnlocked = tier;
        Delves::DelvesRewards::SaveProgress(accountId, progress);
        Delves::DelvesRewards::PublishProgress(player, progress);

        handler->PSendSysMessage("Set highest unlocked tier to {} for account {}", tier, accountId);
        return true;
    }

    static bool HandleDelveCompanionInfoCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        uint32 accountId = player->GetSession()->GetBattlenetAccountId();

        Delves::CompanionState state;
        Delves::DelvesCompanion::LoadFromDB(accountId, state);

        handler->PSendSysMessage("=== Companion Info (Account {}) ===", accountId);
        handler->PSendSysMessage("Companion ID: {}", state.CompanionId);
        handler->PSendSysMessage("Level: {} (XP: {})", state.Level, state.Xp);

        char const* roleNames[] = { "DPS", "Healer", "Tank" };
        uint8 roleIdx = static_cast<uint8>(state.SelectedRole);
        handler->PSendSysMessage("Role: {} ({})", roleIdx < 3 ? roleNames[roleIdx] : "Unknown", roleIdx);
        handler->PSendSysMessage("Combat Curio: {}", state.CombatCurioNodeId);
        handler->PSendSysMessage("Utility Curio: {}", state.UtilityCurioNodeId);
        handler->PSendSysMessage("Max Level: {}", Delves::DelvesCompanion::GetMaxCompanionLevel());

        return true;
    }

    static bool HandleDelveCompanionLevelCommand(ChatHandler* handler, uint32 level)
    {
        if (level == 0 || level > Delves::DelvesCompanion::GetMaxCompanionLevel())
        {
            handler->PSendSysMessage("Invalid level {} (max: {})", level, Delves::DelvesCompanion::GetMaxCompanionLevel());
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();
        uint32 accountId = player->GetSession()->GetBattlenetAccountId();

        Delves::CompanionState state;
        Delves::DelvesCompanion::LoadFromDB(accountId, state);
        state.Level = level;
        state.Xp = 0;
        Delves::DelvesCompanion::SaveToDB(accountId, state);

        handler->PSendSysMessage("Set companion level to {} for account {}", level, accountId);
        return true;
    }

    static bool HandleDelveCompanionRoleCommand(ChatHandler* handler, uint8 role)
    {
        if (role >= static_cast<uint8>(Delves::CompanionRole::Max))
        {
            handler->PSendSysMessage("Invalid role {} (0=DPS, 1=Healer, 2=Tank)", role);
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();
        uint32 accountId = player->GetSession()->GetBattlenetAccountId();

        Delves::CompanionState state;
        Delves::DelvesCompanion::LoadFromDB(accountId, state);
        state.SelectedRole = static_cast<Delves::CompanionRole>(role);
        Delves::DelvesCompanion::SaveToDB(accountId, state);

        char const* roleNames[] = { "DPS", "Healer", "Tank" };
        handler->PSendSysMessage("Set companion role to {} for account {}", roleNames[role], accountId);
        return true;
    }
};

void AddSC_delve_commandscript()
{
    new delve_commandscript();
}
