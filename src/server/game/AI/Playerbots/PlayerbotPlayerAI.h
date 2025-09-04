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

#ifndef TRINITY_PLAYERBOTPLAYERAI_H
#define TRINITY_PLAYERBOTPLAYERAI_H

#ifdef WITH_PLAYERBOTS

#include "PlayerAI.h"

class PlayerbotEngine;
class PlayerbotAiObjectContext;

enum class PlayerbotState : uint8
{
    INACTIVE = 0,
    INITIALIZING = 1,
    ACTIVE = 2,
    PAUSED = 3,
    ERROR = 4
};

/**
 * @brief Extended PlayerAI that integrates the mod-playerbots Engine system
 * 
 * This class serves as the bridge between TrinityCore's PlayerAI framework
 * and the sophisticated Strategy-Action-Engine architecture from mod-playerbots.
 * It maintains compatibility with existing TrinityCore PlayerAI while adding
 * advanced bot intelligence capabilities.
 */
class TC_GAME_API PlayerbotPlayerAI : public PlayerAI
{
public:
    explicit PlayerbotPlayerAI(Player* player);
    virtual ~PlayerbotPlayerAI();

    // Core TrinityCore PlayerAI interface
    void UpdateAI(uint32 diff) override;
    void OnCharmed(bool isNew) override;
    void Reset() override;
    bool CanAIAttack(Unit const* who) const override;

    // Playerbot-specific methods
    bool Initialize();
    void Uninitialize();
    
    PlayerbotState GetState() const { return _state; }
    bool IsActive() const { return _state == PlayerbotState::ACTIVE; }
    
    // Engine integration
    PlayerbotEngine* GetEngine() const { return _engine; }
    PlayerbotAiObjectContext* GetAiObjectContext() const { return _aiObjectContext; }
    
    // Strategy management
    void ChangeStrategy(std::string const& strategies);
    std::string ListStrategies() const;
    bool HasStrategy(std::string const& name) const;
    
    // Action execution
    bool ExecuteAction(std::string const& action, std::string const& qualifier = "");
    
    // Configuration
    void SetUpdateFrequency(uint32 frequency) { _updateFrequency = frequency; }
    uint32 GetUpdateFrequency() const { return _updateFrequency; }
    
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return _enabled; }

protected:
    // TrinityCore PlayerAI integration
    Unit* SelectAttackTarget() const override;
    void DoRangedAttackIfReady() override;
    void DoAutoAttackIfReady() override;

private:
    // Core components
    PlayerbotEngine* _engine;
    PlayerbotAiObjectContext* _aiObjectContext;
    
    // State management
    PlayerbotState _state;
    bool _enabled;
    uint32 _lastUpdate;
    uint32 _updateFrequency; // milliseconds between updates
    
    // Error handling
    uint32 _errorCount;
    uint32 _lastError;
    static constexpr uint32 MAX_ERROR_COUNT = 10;
    static constexpr uint32 ERROR_COOLDOWN = 30000; // 30 seconds
    
    // Initialization helpers
    bool InitializeEngine();
    void InitializeDefaultStrategies();
    void HandleError(std::string const& error);
    void ResetErrorState();
    
    // Update helpers
    bool ShouldUpdate(uint32 diff) const;
    void UpdateEngine(uint32 diff);
    void UpdateState();
    
    // Integration helpers
    bool IsValidForBotting() const;
    void SynchronizeWithPlayerAI();
};

#endif // WITH_PLAYERBOTS

#endif