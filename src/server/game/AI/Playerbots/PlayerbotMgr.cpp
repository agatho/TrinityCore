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

#ifdef WITH_PLAYERBOTS

#include "PlayerbotMgr.h"
#include "PlayerbotPlayerAI.h"
#include "PlayerbotCommon.h"
#include "Player.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "AccountMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Chat.h"
#include "Language.h"
#include "WorldPacket.h"
#include "Opcodes.h"

// Static member definitions
PlayerbotMgr* PlayerbotMgr::_instance = nullptr;
std::set<ObjectGuid> BotInitGuard::_botsBeingInitialized;

PlayerbotMgr::PlayerbotMgr()
    : _enabled(false)
    , _maxBots(100)
    , _updateInterval(1000)
    , _lastUpdate(0)
    , _sessionUpdateCounter(0)
{
    LoadConfig();
}

PlayerbotMgr::~PlayerbotMgr()
{
    LogoutAllBots();
}

PlayerbotMgr* PlayerbotMgr::instance()
{
    if (!_instance)
        _instance = new PlayerbotMgr();
    return _instance;
}

void PlayerbotMgr::LoadConfig()
{
    _enabled = sWorld->getBoolConfig(CONFIG_PLAYERBOT_ENABLED);
    _maxBots = sWorld->getIntConfig(CONFIG_PLAYERBOT_MAX_BOTS);
    _updateInterval = sWorld->getIntConfig(CONFIG_PLAYERBOT_UPDATE_INTERVAL);
    
    TC_LOG_INFO("playerbots", "PlayerbotMgr: Enabled={}, MaxBots={}, UpdateInterval={}ms", 
                _enabled, _maxBots, _updateInterval);
}

void PlayerbotMgr::AddPlayerBot(ObjectGuid guid, uint32 masterAccountId)
{
    if (!_enabled)
    {
        TC_LOG_DEBUG("playerbots", "PlayerbotMgr::AddPlayerBot: Playerbots are disabled");
        return;
    }

    if (_playerBots.size() >= _maxBots)
    {
        TC_LOG_WARN("playerbots", "PlayerbotMgr::AddPlayerBot: Maximum bot limit ({}) reached", _maxBots);
        return;
    }

    if (_playerBots.find(guid) != _playerBots.end())
    {
        TC_LOG_DEBUG("playerbots", "PlayerbotMgr::AddPlayerBot: Bot {} already exists", guid.ToString());
        return;
    }

    BotInitGuard guard(guid);
    if (!guard.IsActive())
    {
        TC_LOG_WARN("playerbots", "PlayerbotMgr::AddPlayerBot: Bot {} is already being initialized", guid.ToString());
        return;
    }

    try
    {
        if (!ValidateBot(guid, masterAccountId))
        {
            TC_LOG_ERROR("playerbots", "PlayerbotMgr::AddPlayerBot: Bot validation failed for {}", guid.ToString());
            return;
        }

        // Create bot session and initialize
        Player* bot = ObjectAccessor::FindPlayer(guid);
        if (!bot)
        {
            TC_LOG_ERROR("playerbots", "PlayerbotMgr::AddPlayerBot: Could not find player {}", guid.ToString());
            return;
        }

        // Add to bot map and initialize AI
        _playerBots[guid] = bot;
        
        // TODO: Initialize bot AI if not already present - need TrinityCore Player API integration
        // if (!bot->GetPlayerAI())
        // {
        //     PlayerbotPlayerAI* botAI = new PlayerbotPlayerAI(bot);
        //     bot->SetPlayerAI(botAI);
        //     botAI->Initialize();
        // }

        OnBotLogin(bot);
        TC_LOG_INFO("playerbots", "PlayerbotMgr: Added bot {} (Master Account: {})", 
                    guid.ToString(), masterAccountId);
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("playerbots", "PlayerbotMgr::AddPlayerBot: Exception for {}: {}", 
                     guid.ToString(), e.what());
        RemoveFromPlayerbotsMap(guid);
    }
}

void PlayerbotMgr::LogoutPlayerBot(ObjectGuid guid)
{
    auto it = _playerBots.find(guid);
    if (it == _playerBots.end())
        return;

    Player* bot = it->second;
    if (!bot)
    {
        RemoveFromPlayerbotsMap(guid);
        return;
    }

    TC_LOG_DEBUG("playerbots", "PlayerbotMgr: Logging out bot {}", guid.ToString());
    
    try
    {
        HandleBotLogout(bot);
        RemoveFromPlayerbotsMap(guid);
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("playerbots", "PlayerbotMgr::LogoutPlayerBot: Exception for {}: {}", 
                     guid.ToString(), e.what());
    }
}

void PlayerbotMgr::DisablePlayerBot(ObjectGuid guid)
{
    LogoutPlayerBot(guid);
}

void PlayerbotMgr::RemoveFromPlayerbotsMap(ObjectGuid guid)
{
    auto it = _playerBots.find(guid);
    if (it != _playerBots.end())
    {
        _playerBots.erase(it);
        TC_LOG_DEBUG("playerbots", "PlayerbotMgr: Removed bot {} from map", guid.ToString());
    }
}

Player* PlayerbotMgr::GetPlayerBot(ObjectGuid guid) const
{
    auto it = _playerBots.find(guid);
    return it != _playerBots.end() ? it->second : nullptr;
}

Player* PlayerbotMgr::GetPlayerBot(ObjectGuid::LowType lowGuid) const
{
    ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(lowGuid);
    return GetPlayerBot(guid);
}

void PlayerbotMgr::UpdateSessions(uint32 diff)
{
    if (!_enabled || _playerBots.empty())
        return;

    _lastUpdate += diff;
    if (_lastUpdate < _updateInterval)
        return;

    _lastUpdate = 0;

    try
    {
        ProcessBotUpdates(diff);
        CleanupDisconnectedBots();
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("playerbots", "PlayerbotMgr::UpdateSessions: Exception: {}", e.what());
    }
}

void PlayerbotMgr::HandleBotPackets(WorldSession* session)
{
    if (!session)
        return;

    // Process bot packets if any
    // This is a placeholder for packet handling logic
    // In TrinityCore, most bot actions are handled through the AI system
    // rather than packet simulation
}

void PlayerbotMgr::LogoutAllBots()
{
    if (_playerBots.empty())
        return;

    TC_LOG_INFO("playerbots", "PlayerbotMgr: Logging out all {} bots", _playerBots.size());

    // Create a copy of the bot map to avoid iterator invalidation
    PlayerBotMap botsCopy = _playerBots;
    
    for (auto const& [guid, bot] : botsCopy)
    {
        try
        {
            LogoutPlayerBot(guid);
        }
        catch (std::exception const& e)
        {
            TC_LOG_ERROR("playerbots", "PlayerbotMgr::LogoutAllBots: Exception for {}: {}", 
                         guid.ToString(), e.what());
        }
    }

    _playerBots.clear();
}

void PlayerbotMgr::OnBotLogin(Player* const bot)
{
    if (!bot)
        return;

    TC_LOG_DEBUG("playerbots", "PlayerbotMgr::OnBotLogin: Bot {} logged in", bot->GetName());

    // Initialize bot-specific settings
    if (PlayerbotPlayerAI* botAI = dynamic_cast<PlayerbotPlayerAI*>(bot->AI()))
    {
        botAI->OnBotLogin();
    }
}

std::vector<std::string> PlayerbotMgr::HandlePlayerbotCommand(char const* args, Player* master)
{
    std::vector<std::string> result;
    
    if (!_enabled)
    {
        result.push_back("Playerbots are currently disabled");
        return result;
    }

    std::string command = args ? args : "";
    if (command.empty())
    {
        result.push_back("Available commands: list, add <name>, remove <name>, removeall");
        return result;
    }

    // Parse and execute commands
    // This is a simplified version - full implementation would handle more commands
    if (command == "list")
    {
        result.push_back("Active bots: " + std::to_string(_playerBots.size()) + "/" + std::to_string(_maxBots));
        for (auto const& [guid, bot] : _playerBots)
        {
            if (bot)
                result.push_back("- " + bot->GetName());
        }
    }
    else
    {
        result.push_back("Command not implemented yet: " + command);
    }

    return result;
}

std::string PlayerbotMgr::ProcessBotCommand(std::string const& cmd, ObjectGuid guid, ObjectGuid masterguid, bool admin, uint32 masterAccountId)
{
    // Process individual bot commands
    // This is a placeholder for command processing logic
    return "Command processed: " + cmd;
}

bool PlayerbotMgr::IsPlayerBot(Player* player) const
{
    if (!player)
        return false;
    
    return _playerBots.find(player->GetGUID()) != _playerBots.end();
}

bool PlayerbotMgr::IsPlayerBot(ObjectGuid guid) const
{
    return _playerBots.find(guid) != _playerBots.end();
}

void PlayerbotMgr::AddPlayerBotError(std::string const& name, std::string const& error)
{
    _playerBotErrors[name].insert(error);
}

void PlayerbotMgr::RemovePlayerBotError(std::string const& name)
{
    _playerBotErrors.erase(name);
}

bool PlayerbotMgr::HasPlayerBotError(std::string const& name) const
{
    auto it = _playerBotErrors.find(name);
    return it != _playerBotErrors.end() && !it->second.empty();
}

std::set<std::string> PlayerbotMgr::GetPlayerBotErrors(std::string const& name) const
{
    auto it = _playerBotErrors.find(name);
    return it != _playerBotErrors.end() ? it->second : std::set<std::string>();
}

WorldSession* PlayerbotMgr::CreateBotSession(uint32 accountId, std::string const& characterName)
{
    // Create a virtual WorldSession for the bot
    // This is a simplified implementation - full version would need proper session setup
    return nullptr; // Placeholder
}

void PlayerbotMgr::InitializeBotSession(WorldSession* session, Player* bot)
{
    // Initialize bot session with proper settings
    // Placeholder for session initialization
}

void PlayerbotMgr::CleanupBotSession(WorldSession* session)
{
    // Clean up bot session resources
    // Placeholder for session cleanup
}

bool PlayerbotMgr::IsAccountLinked(uint32 accountId, uint32 masterAccountId)
{
    // Check if bot account is linked to master account
    // For now, allow any account (should be configurable)
    return true;
}

bool PlayerbotMgr::ValidateBot(ObjectGuid guid, uint32 masterAccountId)
{
    // Validate bot before adding
    Player* bot = ObjectAccessor::FindPlayer(guid);
    if (!bot)
        return false;

    if (!IsAccountLinked(bot->GetSession()->GetAccountId(), masterAccountId))
        return false;

    return true;
}

void PlayerbotMgr::CleanupDisconnectedBots()
{
    // Remove bots whose sessions are no longer valid
    std::vector<ObjectGuid> toRemove;

    for (auto const& [guid, bot] : _playerBots)
    {
        if (!bot || !bot->GetSession() || !bot->IsInWorld())
        {
            toRemove.push_back(guid);
        }
    }

    for (ObjectGuid guid : toRemove)
    {
        LogoutPlayerBot(guid);
    }
}

void PlayerbotMgr::ProcessBotUpdates(uint32 diff)
{
    // Update bot AI systems
    for (auto const& [guid, bot] : _playerBots)
    {
        if (!bot || !bot->IsInWorld())
            continue;

        try
        {
            if (PlayerbotPlayerAI* botAI = dynamic_cast<PlayerbotPlayerAI*>(bot->AI()))
            {
                botAI->UpdateAI(diff);
            }
        }
        catch (std::exception const& e)
        {
            TC_LOG_ERROR("playerbots", "PlayerbotMgr::ProcessBotUpdates: Exception for {}: {}", 
                         guid.ToString(), e.what());
        }
    }
}

void PlayerbotMgr::HandleBotLogout(Player* bot)
{
    if (!bot)
        return;

    // Perform cleanup before bot logout
    if (PlayerbotPlayerAI* botAI = dynamic_cast<PlayerbotPlayerAI*>(bot->AI()))
    {
        botAI->OnBotLogout();
    }

    // Remove from any groups/guilds if needed
    // Save bot data
    bot->SaveToDB();
}

// BotInitGuard implementation
BotInitGuard::BotInitGuard(ObjectGuid guid)
    : _guid(guid), _active(false)
{
    if (_botsBeingInitialized.find(guid) == _botsBeingInitialized.end())
    {
        _botsBeingInitialized.insert(guid);
        _active = true;
    }
}

BotInitGuard::~BotInitGuard()
{
    if (_active)
        _botsBeingInitialized.erase(_guid);
}

#endif // WITH_PLAYERBOTS