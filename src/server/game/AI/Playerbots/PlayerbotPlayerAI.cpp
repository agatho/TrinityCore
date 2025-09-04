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

#include "PlayerbotPlayerAI.h"
#include "PlayerbotEngine.h"
#include "PlayerbotAiObjectContext.h"
#include "PlayerbotCommon.h"
#include "Player.h"
#include "Log.h"
#include "Unit.h"

PlayerbotPlayerAI::PlayerbotPlayerAI(Player* player)
    : PlayerAI(player), _engine(nullptr), _aiObjectContext(nullptr),
      _state(PlayerbotState::INACTIVE), _enabled(false), _lastUpdate(0),
      _updateFrequency(1000), _errorCount(0), _lastError(0)
{
    TC_LOG_DEBUG("playerbots", "PlayerbotPlayerAI created for player {}", player->GetName());
}

PlayerbotPlayerAI::~PlayerbotPlayerAI()
{
    Uninitialize();
    TC_LOG_DEBUG("playerbots", "PlayerbotPlayerAI destroyed");
}

void PlayerbotPlayerAI::UpdateAI(uint32 diff)
{
    if (!_enabled || _state == PlayerbotState::INACTIVE)
        return;

    // Handle error state
    if (_state == PlayerbotState::ERROR)
    {
        if (_lastError + ERROR_COOLDOWN < getMSTime())
        {
            ResetErrorState();
        }
        else
        {
            return; // Still in cooldown
        }
    }

    // Check if we should update
    if (!ShouldUpdate(diff))
        return;

    try
    {
        // Update state
        UpdateState();
        
        if (_state == PlayerbotState::ACTIVE)
        {
            // Update the engine
            UpdateEngine(diff);
            
            // Synchronize with base PlayerAI
            SynchronizeWithPlayerAI();
        }
        
        _lastUpdate = getMSTime();
    }
    catch (std::exception const& e)
    {
        HandleError("UpdateAI exception: " + std::string(e.what()));
    }
}

void PlayerbotPlayerAI::OnCharmed(bool isNew)
{
    if (isNew)
    {
        SetEnabled(false);
        TC_LOG_DEBUG("playerbots", "Player {} charmed - disabling playerbot AI", me->GetName());
    }
    else
    {
        SetEnabled(true);
        TC_LOG_DEBUG("playerbots", "Player {} charm removed - enabling playerbot AI", me->GetName());
    }
    
    PlayerAI::OnCharmed(isNew);
}

void PlayerbotPlayerAI::Reset()
{
    if (_engine)
        _engine->DoNextAction(nullptr, 0, true);
    
    PlayerAI::Reset();
}

bool PlayerbotPlayerAI::CanAIAttack(Unit const* who) const
{
    if (!_enabled || _state != PlayerbotState::ACTIVE)
        return false;
    
    return PlayerAI::CanAIAttack(who);
}

bool PlayerbotPlayerAI::Initialize()
{
    if (_state != PlayerbotState::INACTIVE)
        return _state == PlayerbotState::ACTIVE;

    _state = PlayerbotState::INITIALIZING;
    
    try
    {
        // Create AI object context
        _aiObjectContext = new PlayerbotAiObjectContext(this);
        if (!_aiObjectContext)
        {
            HandleError("Failed to create AI object context");
            return false;
        }

        // Initialize context
        _aiObjectContext->Initialize();
        if (!_aiObjectContext->IsInitialized())
        {
            HandleError("Failed to initialize AI object context");
            return false;
        }

        // Create and initialize engine
        if (!InitializeEngine())
        {
            HandleError("Failed to initialize engine");
            return false;
        }

        // Initialize default strategies
        InitializeDefaultStrategies();

        _state = PlayerbotState::ACTIVE;
        TC_LOG_INFO("playerbots", "PlayerbotPlayerAI initialized successfully for player {}", me->GetName());
        return true;
    }
    catch (std::exception const& e)
    {
        HandleError("Initialize exception: " + std::string(e.what()));
        return false;
    }
}

void PlayerbotPlayerAI::Uninitialize()
{
    _state = PlayerbotState::INACTIVE;
    _enabled = false;

    if (_engine)
    {
        _engine->Uninitialize();
        delete _engine;
        _engine = nullptr;
    }

    if (_aiObjectContext)
    {
        delete _aiObjectContext;
        _aiObjectContext = nullptr;
    }

    TC_LOG_DEBUG("playerbots", "PlayerbotPlayerAI uninitialized");
}

void PlayerbotPlayerAI::ChangeStrategy(std::string const& strategies)
{
    if (!_engine)
        return;

    _engine->ChangeStrategy(strategies);
    TC_LOG_DEBUG("playerbots", "Player {} changed strategies to: {}", me->GetName(), strategies);
}

std::string PlayerbotPlayerAI::ListStrategies() const
{
    if (!_engine)
        return "No engine";

    return _engine->ListStrategies();
}

bool PlayerbotPlayerAI::HasStrategy(std::string const& name) const
{
    if (!_engine)
        return false;

    return _engine->HasStrategy(name);
}

bool PlayerbotPlayerAI::ExecuteAction(std::string const& action, std::string const& qualifier)
{
    if (!_engine)
        return false;

    PlayerbotActionResult result = _engine->ExecuteAction(action, PlayerbotEvent(), qualifier);
    return result == PlayerbotActionResult::OK;
}

void PlayerbotPlayerAI::SetEnabled(bool enabled)
{
    if (_enabled == enabled)
        return;

    _enabled = enabled;

    if (_enabled)
    {
        if (_state == PlayerbotState::INACTIVE)
        {
            Initialize();
        }
        else if (_state == PlayerbotState::PAUSED)
        {
            _state = PlayerbotState::ACTIVE;
        }
    }
    else
    {
        if (_state == PlayerbotState::ACTIVE)
        {
            _state = PlayerbotState::PAUSED;
        }
    }

    TC_LOG_DEBUG("playerbots", "Player {} AI {}", me->GetName(), enabled ? "enabled" : "disabled");
}

Unit* PlayerbotPlayerAI::SelectAttackTarget() const
{
    // Use engine if available, otherwise fall back to base implementation
    if (_engine && _state == PlayerbotState::ACTIVE)
    {
        // TODO: Implement engine-based target selection
        // For now, use base implementation
    }
    
    return PlayerAI::SelectAttackTarget();
}

void PlayerbotPlayerAI::DoRangedAttackIfReady()
{
    if (_engine && _state == PlayerbotState::ACTIVE)
    {
        // Let engine handle combat actions
        _engine->DoNextAction(me->GetVictim(), 0, false);
    }
    else
    {
        PlayerAI::DoRangedAttackIfReady();
    }
}

void PlayerbotPlayerAI::DoAutoAttackIfReady()
{
    if (_engine && _state == PlayerbotState::ACTIVE)
    {
        // Let engine handle combat actions
        _engine->DoNextAction(me->GetVictim(), 0, false);
    }
    else
    {
        PlayerAI::DoAutoAttackIfReady();
    }
}

bool PlayerbotPlayerAI::InitializeEngine()
{
    if (!_aiObjectContext)
        return false;

    _engine = new PlayerbotEngine(this, _aiObjectContext);
    if (!_engine)
        return false;

    _engine->Initialize();
    return _engine->IsInitialized();
}

void PlayerbotPlayerAI::InitializeDefaultStrategies()
{
    if (!_engine)
        return;

    // Add basic default strategies based on player class/spec
    // TODO: Implement class-specific default strategies
    
    _engine->AddStrategy("default");
    
    TC_LOG_DEBUG("playerbots", "Default strategies initialized for player {}", me->GetName());
}

void PlayerbotPlayerAI::HandleError(std::string const& error)
{
    _errorCount++;
    _lastError = getMSTime();
    _state = PlayerbotState::ERROR;

    TC_LOG_ERROR("playerbots", "PlayerbotPlayerAI error for player {}: {} (Error count: {})", 
                me->GetName(), error, _errorCount);

    if (_errorCount >= MAX_ERROR_COUNT)
    {
        TC_LOG_ERROR("playerbots", "Max error count reached for player {} - disabling AI", me->GetName());
        SetEnabled(false);
    }
}

void PlayerbotPlayerAI::ResetErrorState()
{
    if (_errorCount > 0)
        _errorCount--;
    
    if (_errorCount == 0)
    {
        _state = _enabled ? PlayerbotState::ACTIVE : PlayerbotState::PAUSED;
        TC_LOG_DEBUG("playerbots", "Error state reset for player {}", me->GetName());
    }
}

bool PlayerbotPlayerAI::ShouldUpdate(uint32 diff) const
{
    return (getMSTime() - _lastUpdate) >= _updateFrequency;
}

void PlayerbotPlayerAI::UpdateEngine(uint32 diff)
{
    if (!_engine)
        return;

    try
    {
        _engine->DoNextAction(me->GetVictim(), 0, false);
    }
    catch (std::exception const& e)
    {
        HandleError("Engine update exception: " + std::string(e.what()));
    }
}

void PlayerbotPlayerAI::UpdateState()
{
    if (!IsValidForBotting())
    {
        if (_state == PlayerbotState::ACTIVE)
            _state = PlayerbotState::PAUSED;
    }
    else
    {
        if (_state == PlayerbotState::PAUSED && _enabled)
            _state = PlayerbotState::ACTIVE;
    }
}

bool PlayerbotPlayerAI::IsValidForBotting() const
{
    if (!me)
        return false;
        
    // Check if player is in a valid state for botting
    if (me->IsBeingTeleported() || me->IsInFlight())
        return false;
        
    if (me->HasUnitState(UNIT_STATE_CONTROLLED))
        return false;
        
    return true;
}

void PlayerbotPlayerAI::SynchronizeWithPlayerAI()
{
    // Ensure bot actions don't conflict with base PlayerAI
    // This is a placeholder for future synchronization logic
}

#endif // WITH_PLAYERBOTS