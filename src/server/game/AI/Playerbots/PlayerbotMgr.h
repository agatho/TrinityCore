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

#ifndef TRINITY_PLAYERBOTMGR_H
#define TRINITY_PLAYERBOTMGR_H

#ifdef WITH_PLAYERBOTS

#include "Define.h"
#include "ObjectGuid.h"
#include "PlayerbotCommon.h"
#include <map>
#include <set>
#include <vector>
#include <string>
#include <memory>

class Player;
class PlayerbotPlayerAI;
class WorldSession;
class WorldPacket;

typedef std::map<ObjectGuid, Player*> PlayerBotMap;
typedef std::map<std::string, std::set<std::string>> PlayerBotErrorMap;

/**
 * @brief Manages bot sessions and lifecycle for TrinityCore
 * 
 * This class handles creation, management, and cleanup of bot sessions,
 * providing virtual WorldSession handling for AI-controlled players.
 */
class TC_GAME_API PlayerbotMgr
{
public:
    PlayerbotMgr();
    ~PlayerbotMgr();

    // Singleton access
    static PlayerbotMgr* instance();

    // Bot lifecycle management
    void AddPlayerBot(ObjectGuid guid, uint32 masterAccountId);
    void LogoutPlayerBot(ObjectGuid guid);
    void DisablePlayerBot(ObjectGuid guid);
    void RemoveFromPlayerbotsMap(ObjectGuid guid);
    
    // Bot retrieval
    Player* GetPlayerBot(ObjectGuid guid) const;
    Player* GetPlayerBot(ObjectGuid::LowType lowGuid) const;
    PlayerBotMap::const_iterator GetPlayerBotsBegin() const { return _playerBots.begin(); }
    PlayerBotMap::const_iterator GetPlayerBotsEnd() const { return _playerBots.end(); }
    
    // Session management
    void UpdateSessions(uint32 diff);
    void HandleBotPackets(WorldSession* session);
    void LogoutAllBots();
    void OnBotLogin(Player* const bot);
    
    // Command handling
    std::vector<std::string> HandlePlayerbotCommand(char const* args, Player* master = nullptr);
    std::string ProcessBotCommand(std::string const& cmd, ObjectGuid guid, ObjectGuid masterguid, bool admin, uint32 masterAccountId);
    
    // Utility functions
    bool IsPlayerBot(Player* player) const;
    bool IsPlayerBot(ObjectGuid guid) const;
    uint32 GetPlayerBotCount() const { return uint32(_playerBots.size()); }
    
    // Configuration
    void LoadConfig();
    bool IsEnabled() const { return _enabled; }
    uint32 GetMaxBots() const { return _maxBots; }
    
    // Error handling
    void AddPlayerBotError(std::string const& name, std::string const& error);
    void RemovePlayerBotError(std::string const& name);
    bool HasPlayerBotError(std::string const& name) const;
    std::set<std::string> GetPlayerBotErrors(std::string const& name) const;

protected:
    // Session creation and management
    WorldSession* CreateBotSession(uint32 accountId, std::string const& characterName);
    void InitializeBotSession(WorldSession* session, Player* bot);
    void CleanupBotSession(WorldSession* session);
    
    // Account validation
    bool IsAccountLinked(uint32 accountId, uint32 masterAccountId);
    bool ValidateBot(ObjectGuid guid, uint32 masterAccountId);

private:
    static PlayerbotMgr* _instance;
    
    PlayerBotMap _playerBots;
    PlayerBotErrorMap _playerBotErrors;
    
    // Configuration
    bool _enabled;
    uint32 _maxBots;
    uint32 _updateInterval;
    
    // State tracking
    std::set<ObjectGuid> _botsBeingInitialized;
    std::set<ObjectGuid> _botsBeingRemoved;
    
    // Session management
    uint32 _lastUpdate;
    uint32 _sessionUpdateCounter;
    
    // Helper methods
    void CleanupDisconnectedBots();
    void ProcessBotUpdates(uint32 diff);
    void HandleBotLogout(Player* bot);
    
    // Prevent copying
    PlayerbotMgr(PlayerbotMgr const&) = delete;
    PlayerbotMgr& operator=(PlayerbotMgr const&) = delete;
};

/**
 * @brief RAII guard for bot initialization to prevent recursive calls
 */
class TC_GAME_API BotInitGuard
{
public:
    explicit BotInitGuard(ObjectGuid guid);
    ~BotInitGuard();
    
    bool IsActive() const { return _active; }

private:
    ObjectGuid _guid;
    bool _active;
    static std::set<ObjectGuid> _botsBeingInitialized;
};

// Global accessor
#define sPlayerbotMgr PlayerbotMgr::instance()

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTMGR_H