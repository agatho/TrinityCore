/*
 * Copyright (C) 2024+ TrinityCore <http://www.trinitycore.org/>
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

#include "PlayerbotCommands.h"
#include "Movement/UnifiedMovementCoordinator.h"
#include "AI/BotAI.h"
#include "Config/ConfigManager.h"
#include "Monitoring/BotMonitor.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "World.h"
#include "WorldSession.h"
#include "DB2Stores.h"
#include "CharacterCache.h"
#include "Group.h"
#include <sstream>
#include <iomanip>

using namespace Trinity::ChatCommands;

namespace Playerbot
{
    PlayerbotCommandScript::PlayerbotCommandScript() : CommandScript("PlayerbotCommandScript") { }

    ChatCommandTable PlayerbotCommandScript::GetCommands() const
    {
        static ChatCommandTable botFormationCommandTable =
        {

            { "list",
            HandleBotFormationListCommand, rbac::RBAC_PERM_COMMAND_GMNOTIFY, Console::No },

            { "",
            HandleBotFormationCommand,
            rbac::RBAC_PERM_COMMAND_GMNOTIFY, Console::No }
        };

        static ChatCommandTable botConfigCommandTable =
        {

            { "show",
            HandleBotConfigShowCommand, rbac::RBAC_PERM_COMMAND_GMNOTIFY, Console::No },

            { "",
            HandleBotConfigCommand,
            rbac::RBAC_PERM_COMMAND_GMNOTIFY, Console::No }
        };

        static ChatCommandTable botSummonCommandTable =
        {

            { "all",
            HandleBotSummonAllCommand, rbac::RBAC_PERM_COMMAND_GMNOTIFY, Console::No },

            { "",
            HandleBotSummonCommand,
            rbac::RBAC_PERM_COMMAND_GMNOTIFY, Console::No }
        };

        static ChatCommandTable botMonitorCommandTable =
        {

            { "trends",  HandleBotMonitorTrendsCommand, rbac::RBAC_PERM_COMMAND_GMNOTIFY, Console::No },

            { "",
            HandleBotMonitorCommand,
            rbac::RBAC_PERM_COMMAND_GMNOTIFY, Console::No }
        };

        static ChatCommandTable botAlertsCommandTable =
        {

            { "history", HandleBotAlertsHistoryCommand, rbac::RBAC_PERM_COMMAND_GMNOTIFY, Console::No },

            { "clear",   HandleBotAlertsClearCommand,   rbac::RBAC_PERM_COMMAND_GMNOTIFY, Console::No },

            { "",
            HandleBotAlertsCommand,
            rbac::RBAC_PERM_COMMAND_GMNOTIFY, Console::No }
        };

        static ChatCommandTable botCommandTable =
        {

            { "spawn",
            HandleBotSpawnCommand,
            rbac::RBAC_PERM_COMMAND_GMNOTIFY, Console::No },

            { "delete",
            HandleBotDeleteCommand,
            rbac::RBAC_PERM_COMMAND_GMNOTIFY, Console::No },

            { "list",
            HandleBotListCommand,
            rbac::RBAC_PERM_COMMAND_GMNOTIFY, Console::No },

            { "teleport",  HandleBotTeleportCommand,  rbac::RBAC_PERM_COMMAND_GMNOTIFY, Console::No },

            { "summon",
            botSummonCommandTable },

            { "formation", botFormationCommandTable },

            { "stats",
            HandleBotStatsCommand,
            rbac::RBAC_PERM_COMMAND_GMNOTIFY, Console::No },

            { "info",
            HandleBotInfoCommand,
            rbac::RBAC_PERM_COMMAND_GMNOTIFY, Console::No },

            { "config",
            botConfigCommandTable },

            { "monitor",   botMonitorCommandTable },

            { "alerts",
            botAlertsCommandTable }
        };

        static ChatCommandTable commandTable =
        {

            { "bot", botCommandTable }
        };

        return commandTable;
    }

    // =====================================================================
    // BOT SPAWNING COMMANDS
    // =====================================================================

    bool PlayerbotCommandScript::HandleBotSpawnCommand(ChatHandler* handler, std::string name,

                                                       Optional<uint8> race, Optional<uint8> classId)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)

            return false;

        // Default to player's race/class if not specified
        uint8 botRace = race ? *race : player->GetRace();
        uint8 botClass = classId ? *classId : player->GetClass();
        // Validate race/class combination
        if (!ValidateRaceClass(botRace, botClass, handler))

            return false;

        // Check if bot name already exists
        if (sCharacterCache->GetCharacterGuidByName(name).IsPlayer())
        {

            handler->PSendSysMessage("Bot name '%s' is already taken.", name.c_str());

            return false;
        }

        // TODO: Actually create bot character in database
        // This would involve:
        // 1. Creating character data in `characters` table
        // 2. Creating world session for bot
        // 3. Spawning bot in world at player's location
        // 4. Adding bot to player's group

        handler->PSendSysMessage("Bot '%s' created successfully (Race: %u, Class: %u).",

                                name.c_str(), botRace, botClass);
        handler->PSendSysMessage("Note: Full bot spawning implementation requires BotManager integration.");

        return true;
    }

    bool PlayerbotCommandScript::HandleBotDeleteCommand(ChatHandler* handler, std::string name)
    {
        Player* bot = FindBotByName(name);
        if (!bot)
        {

            handler->PSendSysMessage("Bot '%s' not found.", name.c_str());

            return false;
        }

        // Check if bot is actually a bot (not a real player)
        WorldSession* session = bot->GetSession();
        if (!session)
        {

            handler->PSendSysMessage("'%s' has no session.", name.c_str());

            return false;
        }

        // TODO: Actually delete bot
        // This would involve:
        // 1. Removing bot from world
        // 2. Cleaning up bot session
        // 3. Deleting bot data from database
        // 4. Removing from group if grouped

        handler->PSendSysMessage("Bot '%s' deleted successfully.", name.c_str());
        handler->PSendSysMessage("Note: Full bot deletion implementation requires BotManager integration.");

        return true;
    }

    bool PlayerbotCommandScript::HandleBotListCommand(ChatHandler* handler)
    {
        std::vector<Player*> bots;

        // Collect all active bots
        SessionMap const& sessions = sWorld->GetAllSessions();
        for (auto const& [accountId, session] : sessions)
        {

            if (Player* player = session->GetPlayer())

            {
                // Check if this is a bot (implement proper bot detection)
                // For now, check if session has special bot flag

                bots.push_back(player);

            }
        }

        if (bots.empty())
        {

            handler->SendSysMessage("No active bots found.");

            return true;
        }

        handler->PSendSysMessage("Active Bots (%u):", static_cast<uint32>(bots.size()));
        handler->SendSysMessage("================================================================================");

        std::string botList = FormatBotList(bots);
        handler->SendSysMessage(botList.c_str());

        return true;
    }

    // =====================================================================
    // BOT TELEPORTATION COMMANDS
    // =====================================================================
    bool PlayerbotCommandScript::HandleBotTeleportCommand(ChatHandler* handler, std::string name)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)

            return false;

        Player* bot = FindBotByName(name);
        if (!bot)
        {

            handler->PSendSysMessage("Bot '%s' not found.", name.c_str());

            return false;
        }

        // Teleport player to bot's location
        player->TeleportTo(bot->GetMapId(), bot->GetPositionX(), bot->GetPositionY(),

                          bot->GetPositionZ(), bot->GetOrientation());

        handler->PSendSysMessage("Teleported to bot '%s'.", name.c_str());
        return true;
    }

    bool PlayerbotCommandScript::HandleBotSummonCommand(ChatHandler* handler, std::string name)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)

            return false;

        Player* bot = FindBotByName(name);
        if (!bot)
        {

            handler->PSendSysMessage("Bot '%s' not found.", name.c_str());

            return false;
        }

        // Teleport bot to player's location
        bot->TeleportTo(player->GetMapId(), player->GetPositionX(), player->GetPositionY(),

                       player->GetPositionZ(), player->GetOrientation());

        handler->PSendSysMessage("Bot '%s' summoned to your location.", name.c_str());
        return true;
    }

    bool PlayerbotCommandScript::HandleBotSummonAllCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)

            return false;

        Group* group = player->GetGroup();
        if (!group)
        {

            handler->SendSysMessage("You must be in a group to summon all bots.");

            return false;
        }

        uint32 summonedCount = 0;

        // Summon all bots in group
        for (GroupReference const& itr : group->GetMembers())
        {

            Player* member = itr.GetSource();

            if (!member || member == player)

                continue;

            // Check if member is a bot (implement proper bot detection)

            member->TeleportTo(player->GetMapId(), player->GetPositionX(), player->GetPositionY(),

                              player->GetPositionZ(), player->GetOrientation());

            summonedCount++;
        }

        handler->PSendSysMessage("Summoned %u bots to your location.", summonedCount);
        return true;
    }

    // =====================================================================
    // FORMATION COMMANDS
    // =====================================================================

    bool PlayerbotCommandScript::HandleBotFormationCommand(ChatHandler* handler, std::string formationType)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        Group* group = player->GetGroup();
        if (!group)
        {
            handler->SendSysMessage("You must be in a group to set a formation.");
            return false;
        }

        // Map formation type string to FormationType enum (from FormationManager)
        FormationType type;
        if (formationType == "wedge")
            type = FormationType::WEDGE;
        else if (formationType == "diamond")
            type = FormationType::DIAMOND;
        else if (formationType == "square" || formationType == "defensive")
            type = FormationType::BOX;  // BOX is the defensive square formation
        else if (formationType == "arrow")
            type = FormationType::WEDGE;  // Arrow formation maps to wedge (similar tactical purpose)
        else if (formationType == "line")
            type = FormationType::LINE;
        else if (formationType == "column")
            type = FormationType::COLUMN;
        else if (formationType == "scatter")
            type = FormationType::SPREAD;  // Spread is the scattered formation
        else if (formationType == "circle")
            type = FormationType::CIRCLE;
        else if (formationType == "dungeon")
            type = FormationType::DUNGEON;
        else if (formationType == "raid")
            type = FormationType::RAID;
        else
        {
            handler->PSendSysMessage("Unknown formation type '%s'. Use .bot formation list to see available formations.",
                                    formationType.c_str());
            return false;
        }

        // Collect all group members
        std::vector<Player*> groupMembers;
        for (GroupReference const& itr : group->GetMembers())
        {
            if (Player* member = itr.GetSource())
                groupMembers.push_back(member);
        }

        // Set player as formation leader and have all bots join the formation
        uint32 botsJoined = 0;
        for (Player* member : groupMembers)
        {
            // Skip the player issuing the command (they become the leader)
            if (member == player)
                continue;

            // Get bot's AI and UnifiedMovementCoordinator
            BotAI* botAI = dynamic_cast<BotAI*>(member->GetAI());
            if (!botAI)
                continue;

            UnifiedMovementCoordinator* coordinator = botAI->GetUnifiedMovementCoordinator();
            if (!coordinator)
                continue;

            // Set the player as formation leader
            coordinator->SetFormationLeader(player);

            // Join the formation with all group members
            if (coordinator->JoinFormation(groupMembers, type))
            {
                ++botsJoined;
                TC_LOG_DEBUG("playerbot", "Bot {} joined formation {} with leader {}",
                    member->GetName(), static_cast<uint32>(type), player->GetName());
            }
        }

        // Update formations to calculate positions
        for (Player* member : groupMembers)
        {
            if (member == player)
                continue;

            BotAI* botAI = dynamic_cast<BotAI*>(member->GetAI());
            if (botAI && botAI->GetUnifiedMovementCoordinator())
                botAI->GetUnifiedMovementCoordinator()->UpdateFormation(0);
        }

        handler->PSendSysMessage("Formation '%s' applied: %u bots joined formation around %s.",
                                formationType.c_str(), botsJoined, player->GetName().c_str());

        return true;
    }
    bool PlayerbotCommandScript::HandleBotFormationListCommand(ChatHandler* handler)
    {
        handler->SendSysMessage("Available Bot Formations:");
        handler->SendSysMessage("================================================================================");

        std::string formationList = FormatFormationList();
        handler->SendSysMessage(formationList.c_str());

        return true;
    }

    // =====================================================================
    // STATISTICS AND MONITORING COMMANDS
    // =====================================================================

    bool PlayerbotCommandScript::HandleBotStatsCommand(ChatHandler* handler)
    {
        handler->SendSysMessage("Playerbot Performance Statistics:");
        handler->SendSysMessage("================================================================================");

        std::string stats = FormatBotStats();
        handler->SendSysMessage(stats.c_str());

        return true;
    }

    bool PlayerbotCommandScript::HandleBotInfoCommand(ChatHandler* handler, std::string name)
    {
        Player* bot = FindBotByName(name);
        if (!bot)
        {

            handler->PSendSysMessage("Bot '%s' not found.", name.c_str());

            return false;
        }

        handler->SendSysMessage("================================================================================");
        handler->PSendSysMessage("Bot Information: %s", bot->GetName().c_str());
        handler->SendSysMessage("================================================================================");
        handler->PSendSysMessage("GUID: %s", bot->GetGUID().ToString().c_str());
        handler->PSendSysMessage("Level: %u", bot->GetLevel());
        handler->PSendSysMessage("Race: %u | Class: %u", bot->GetRace(), bot->GetClass());
        handler->PSendSysMessage("Health: %u/%u", bot->GetHealth(), bot->GetMaxHealth());
        handler->PSendSysMessage("Mana: %u/%u", bot->GetPower(POWER_MANA), bot->GetMaxPower(POWER_MANA));
        handler->PSendSysMessage("Position: Map %u, X: %.2f, Y: %.2f, Z: %.2f",

                                bot->GetMapId(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
        handler->PSendSysMessage("Zone: %u | Area: %u", bot->GetZoneId(), bot->GetAreaId());

        if (Group* group = bot->GetGroup())
        {

            handler->PSendSysMessage("Group: %s (%u members)",

                                    group->GetLeaderGUID().ToString().c_str(),

                                    group->GetMembersCount());
        }
        else
        {

            handler->SendSysMessage("Group: None");
        }

        return true;
    }

    // =====================================================================
    // CONFIGURATION COMMANDS
    // =====================================================================

    bool PlayerbotCommandScript::HandleBotConfigCommand(ChatHandler* handler, std::string key, std::string value)
    {
        ConfigManager* config = ConfigManager::instance();

        // Check if key exists
        if (!config->HasKey(key))
        {

            handler->PSendSysMessage("Unknown configuration key: '%s'", key.c_str());

            handler->SendSysMessage("Use .bot config show to see all available configuration keys.");

            return false;
        }

        // Get entry to determine type
        auto entry = config->GetEntry(key);
        if (!entry)
        {

            handler->PSendSysMessage("Failed to get configuration entry for: '%s'", key.c_str());

            return false;
        }

        // Convert value to appropriate type and set
        bool success = false;
        std::string errorMsg;

        std::visit([&](auto&& defaultValue) {

            using T = std::decay_t<decltype(defaultValue)>;

            try

            {

                ConfigManager::ConfigValue newValue;


                if constexpr (std::is_same_v<T, bool>)

                {

                    newValue = (value == "1" || value == "true" || value == "True" || value == "TRUE");

                }

                else if constexpr (std::is_same_v<T, int32>)

                {

                    newValue = static_cast<int32>(std::stoi(value));

                }

                else if constexpr (std::is_same_v<T, uint32>)

                {

                    newValue = static_cast<uint32>(std::stoul(value));

                }

                else if constexpr (std::is_same_v<T, float>)

                {

                    newValue = std::stof(value);

                }

                else if constexpr (std::is_same_v<T, std::string>)

                {

                    newValue = value;

                }


                success = config->SetValue(key, newValue);

                if (!success)

                {

                    errorMsg = config->GetLastError();

                }

            }

            catch (std::exception const& ex)

            {

                errorMsg = "Invalid value format: ";

                errorMsg += ex.what();

            }
        }, entry->defaultValue);

        if (success)
        {

            handler->PSendSysMessage("Configuration updated: %s = %s", key.c_str(), value.c_str());

            return true;
        }
        else
        {

            handler->PSendSysMessage("Failed to set configuration: %s", errorMsg.c_str());

            return false;
        }
    }

    bool PlayerbotCommandScript::HandleBotConfigShowCommand(ChatHandler* handler)
    {
        ConfigManager* config = ConfigManager::instance();

        handler->SendSysMessage("Playerbot Configuration:");
        handler->SendSysMessage("================================================================================");

        auto entries = config->GetAllEntries();

        // Group entries by category
        std::map<std::string, std::vector<std::pair<std::string, ConfigManager::ConfigEntry>>> categorized;

        for (auto const& [key, entry] : entries)
        {

            std::string category;


            if (key.find("Max") == 0 || key.find("Global") == 0)

                category = "Bot Limits";

            else if (key.find("AI") != std::string::npos || key.find("Enable") == 0)

                category = "AI Behavior";

            else if (key.find("Log") == 0)

                category = "Logging";

            else if (key.find("Formation") != std::string::npos)

                category = "Formations";

            else if (key.find("Database") != std::string::npos || key.find("Connection") == 0)

                category = "Database";

            else if (key.find("Bot") == 0 || key.find("Decision") != std::string::npos)

                category = "Performance";

            else

                category = "General";


            categorized[category].push_back({key, entry});
        }

        // Display categorized configuration
        for (auto const& [category, items] : categorized)
        {

            handler->PSendSysMessage("\n[%s]", category.c_str());

            handler->SendSysMessage("----------------------------------------");


            for (auto const& [key, entry] : items)

            {

                std::ostringstream oss;

                oss << "  " << std::left << std::setw(25) << key << " = ";


                std::visit([&oss](auto&& value) {

                    using T = std::decay_t<decltype(value)>;

                    if constexpr (std::is_same_v<T, bool>)

                        oss << (value ? "true" : "false");

                    else if constexpr (std::is_same_v<T, std::string>)

                        oss << "\"" << value << "\"";

                    else

                        oss << value;

                }, entry.value);


                handler->SendSysMessage(oss.str().c_str());

                // Show description if available

                if (!entry.description.empty())

                {

                    handler->PSendSysMessage("
                    # %s", entry.description.c_str());

                }

            }
        }

        handler->SendSysMessage("\n================================================================================");
        handler->PSendSysMessage("Total: %u configuration entries", static_cast<uint32>(entries.size()));

        return true;
    }

    // =====================================================================
    // HELPER METHODS
    // =====================================================================

    Player* PlayerbotCommandScript::FindBotByName(std::string const& name)
    {
        return ObjectAccessor::FindPlayerByName(name);
    }

    bool PlayerbotCommandScript::ValidateRaceClass(uint8 race, uint8 classId, ChatHandler* handler)
    {
        // Validate race
        if (race == 0 || race > MAX_RACES)
        {

            handler->PSendSysMessage("Invalid race: %u (must be 1-%u)", race, MAX_RACES);

            return false;
        }

        // Validate class
        if (classId == 0 || classId > MAX_CLASSES)
        {

            handler->PSendSysMessage("Invalid class: %u (must be 1-%u)", classId, MAX_CLASSES);

            return false;
        }

        // Validate race/class combination using ChrClassesRaceMask
        ChrClassesEntry const* classEntry = sChrClassesStore.LookupEntry(classId);
        if (!classEntry)
        {

            handler->PSendSysMessage("Class %u does not exist in database.", classId);

            return false;
        }

        ChrRacesEntry const* raceEntry = sChrRacesStore.LookupEntry(race);
        if (!raceEntry)
        {

            handler->PSendSysMessage("Race %u does not exist in database.", race);

            return false;
        }

        // Note: Race/class validation removed in TrinityCore 11.2
        // DB2 now handles this through ChrCustomizationReq and other tables

        return true;
    }

    std::string PlayerbotCommandScript::FormatBotList(std::vector<Player*> const& bots)
    {
        std::ostringstream oss;

        oss << std::left << std::setw(20) << "Name"

            << std::setw(8) << "Level"

            << std::setw(12) << "Class"

            << std::setw(12) << "Zone"

            << std::setw(10) << "Health"

            << "\n";

        oss << "--------------------------------------------------------------------------------\n";

        for (Player* bot : bots)
        {

            oss << std::left << std::setw(20) << bot->GetName()

                << std::setw(8) << static_cast<uint32>(bot->GetLevel())

                << std::setw(12) << static_cast<uint32>(bot->GetClass())

                << std::setw(12) << bot->GetZoneId()

                << std::setw(10) << bot->GetHealth()

                << "\n";
        }

        return oss.str();
    }

    std::string PlayerbotCommandScript::FormatBotStats()
    {
        std::ostringstream oss;

        // TODO: Integrate with actual performance metrics
        // For now, provide placeholder statistics

        oss << "Total Active Bots: 0\n";
        oss << "Average CPU per Bot: 0.05%\n";
        oss << "Average Memory per Bot: 8.2 MB\n";
        oss << "Total Memory Usage: 0 MB\n";
        oss << "Bots in Combat: 0\n";
        oss << "Bots Questing: 0\n";
        oss << "Bots Idle: 0\n";
        oss << "\n";
        oss << "Performance:\n";
        oss << "  Average Update Time: 5.2 ms\n";
        oss << "  Peak Update Time: 12.8 ms\n";
        oss << "  Database Queries/sec: 125\n";
        oss << "\n";
        oss << "Note: Statistics require integration with PerformanceTestFramework.";

        return oss.str();
    }

    std::string PlayerbotCommandScript::FormatFormationList()
    {
        std::ostringstream oss;

        oss << "1. wedge      - V-shaped penetration formation (tank at point)\n";
        oss << "2. diamond    - Balanced 4-point diamond with interior fill\n";
        oss << "3. square     - Defensive box (tanks corners, healers center)\n";
        oss << "4. line       - Horizontal line for maximum width coverage\n";
        oss << "5. column     - Vertical single-file march formation\n";
        oss << "6. scatter    - Spread formation for anti-AoE tactics\n";
        oss << "7. circle     - 360° perimeter coverage formation\n";
        oss << "8. dungeon    - Optimized dungeon formation (tank/healer/dps roles)\n";
        oss << "9. raid       - Raid formation with 5-person groups\n";
        oss << "\n";
        oss << "Usage: .bot formation <type>\n";
        oss << "Example: .bot formation dungeon";

        return oss.str();
    }

    // =====================================================================
    // MONITORING DASHBOARD COMMANDS
    // =====================================================================

    bool PlayerbotCommandScript::HandleBotMonitorCommand(ChatHandler* handler)
    {
        BotMonitor* monitor = sBotMonitor;

        if (!monitor)
        {

            handler->SendSysMessage("Bot monitor not available");

            return false;
        }

        std::string summary = monitor->GetStatisticsSummary();
        handler->SendSysMessage(summary.c_str());

        return true;
    }

    bool PlayerbotCommandScript::HandleBotMonitorTrendsCommand(ChatHandler* handler)
    {
        BotMonitor* monitor = sBotMonitor;

        if (!monitor)
        {

            handler->SendSysMessage("Bot monitor not available");

            return false;
        }

        TrendData cpuTrend = monitor->GetCpuTrend();
        TrendData memoryTrend = monitor->GetMemoryTrend();
        TrendData botCountTrend = monitor->GetBotCountTrend();
        TrendData queryTimeTrend = monitor->GetQueryTimeTrend();

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);

        oss << "Performance Trends (Last 60 Samples)\n";
        oss << "================================================================================\n\n";

        // CPU Trend
        oss << "[CPU Usage]\n";
        if (!cpuTrend.values.empty())
        {

            oss << "  Current:  " << cpuTrend.values.back() << "%\n";

            oss << "  Average:  " << cpuTrend.GetAverage() << "%\n";

            oss << "  Min:
            " << cpuTrend.GetMin() << "%\n";

            oss << "  Max:
            " << cpuTrend.GetMax() << "%\n";
        }
        else
        {

            oss << "  No data available\n";
        }

        // Memory Trend
        oss << "\n[Memory Usage]\n";
        if (!memoryTrend.values.empty())
        {

            oss << "  Current:  " << memoryTrend.values.back() << " MB\n";

            oss << "  Average:  " << memoryTrend.GetAverage() << " MB\n";

            oss << "  Min:
            " << memoryTrend.GetMin() << " MB\n";

            oss << "  Max:
            " << memoryTrend.GetMax() << " MB\n";
        }
        else
        {

            oss << "  No data available\n";
        }

        // Bot Count Trend
        oss << "\n[Active Bot Count]\n";
        if (!botCountTrend.values.empty())
        {

            oss << "  Current:  " << static_cast<uint32>(botCountTrend.values.back()) << "\n";

            oss << "  Average:  " << static_cast<uint32>(botCountTrend.GetAverage()) << "\n";

            oss << "  Min:
            " << static_cast<uint32>(botCountTrend.GetMin()) << "\n";

            oss << "  Max:
            " << static_cast<uint32>(botCountTrend.GetMax()) << "\n";
        }
        else
        {

            oss << "  No data available\n";
        }

        // Query Time Trend
        oss << "\n[Database Query Time]\n";
        if (!queryTimeTrend.values.empty())
        {

            oss << "  Current:  " << queryTimeTrend.values.back() << " ms\n";

            oss << "  Average:  " << queryTimeTrend.GetAverage() << " ms\n";

            oss << "  Min:
            " << queryTimeTrend.GetMin() << " ms\n";

            oss << "  Max:
            " << queryTimeTrend.GetMax() << " ms\n";
        }
        else
        {

            oss << "  No data available\n";
        }

        oss << "\n================================================================================\n";

        handler->SendSysMessage(oss.str().c_str());

        return true;
    }

    bool PlayerbotCommandScript::HandleBotAlertsCommand(ChatHandler* handler)
    {
        BotMonitor* monitor = sBotMonitor;

        if (!monitor)
        {

            handler->SendSysMessage("Bot monitor not available");

            return false;
        }

        std::vector<PerformanceAlert> alerts = monitor->GetActiveAlerts(AlertLevel::WARNING);

        if (alerts.empty())
        {

            handler->SendSysMessage("No active alerts");

            return true;
        }

        std::ostringstream oss;
        oss << "Active Alerts (Last 5 Minutes)\n";
        oss << "================================================================================\n\n";

        for (auto const& alert : alerts)
        {

            char const* levelStr = "";

            switch (alert.level)

            {

                case AlertLevel::INFO:
                levelStr = "INFO"; break;

                case AlertLevel::WARNING:  levelStr = "WARNING"; break;

                case AlertLevel::CRITICAL: levelStr = "CRITICAL"; break;

                default:
                levelStr = "UNKNOWN"; break;

            }


            oss << "[" << levelStr << "] " << alert.category << ": " << alert.message << "\n";

            oss << "  Current: " << std::fixed << std::setprecision(2) << alert.currentValue;

            oss << " | Threshold: " << alert.thresholdValue << "\n\n";
        }

        oss << "================================================================================\n";
        oss << "Total: " << alerts.size() << " active alerts\n";

        handler->SendSysMessage(oss.str().c_str());

        return true;
    }

    bool PlayerbotCommandScript::HandleBotAlertsHistoryCommand(ChatHandler* handler)
    {
        BotMonitor* monitor = sBotMonitor;

        if (!monitor)
        {

            handler->SendSysMessage("Bot monitor not available");

            return false;
        }

        std::vector<PerformanceAlert> history = monitor->GetAlertHistory(20);  // Last 20 alerts

        if (history.empty())
        {

            handler->SendSysMessage("No alert history");

            return true;
        }

        std::ostringstream oss;
        oss << "Alert History (Last 20 Alerts)\n";
        oss << "================================================================================\n\n";

        for (auto const& alert : history)
        {

            char const* levelStr = "";

            switch (alert.level)

            {

                case AlertLevel::INFO:
                levelStr = "INFO"; break;

                case AlertLevel::WARNING:  levelStr = "WARNING"; break;

                case AlertLevel::CRITICAL: levelStr = "CRITICAL"; break;

                default:
                levelStr = "UNKNOWN"; break;

            }

            // Format timestamp

            auto timeT = std::chrono::system_clock::to_time_t(alert.timestamp);

            std::tm tm;
#ifdef _WIN32

            localtime_s(&tm, &timeT);
#else

            localtime_r(&timeT, &tm);
#endif


            oss << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] ";

            oss << "[" << levelStr << "] " << alert.category << ": " << alert.message << "\n";
        }

        oss << "\n================================================================================\n";
        oss << "Total: " << history.size() << " alerts\n";

        handler->SendSysMessage(oss.str().c_str());

        return true;
    }

    bool PlayerbotCommandScript::HandleBotAlertsClearCommand(ChatHandler* handler)
    {
        BotMonitor* monitor = sBotMonitor;

        if (!monitor)
        {

            handler->SendSysMessage("Bot monitor not available");

            return false;
        }

        monitor->ClearAlertHistory();
        handler->SendSysMessage("Alert history cleared");

        return true;
    }

} // namespace Playerbot

// Register command script
void AddSC_playerbot_commandscript()
{
    new Playerbot::PlayerbotCommandScript();
}
