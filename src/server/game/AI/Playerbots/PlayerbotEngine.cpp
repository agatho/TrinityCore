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

#include "PlayerbotEngine.h"
#include "PlayerbotPlayerAI.h"
#include "PlayerbotAiObjectContext.h"
#include "PlayerbotCommon.h"
#include "Log.h"
#include "Unit.h"

// PlayerbotActionExecutionListeners implementation
PlayerbotActionExecutionListeners::~PlayerbotActionExecutionListeners()
{
    _listeners.clear();
}

bool PlayerbotActionExecutionListeners::Before(PlayerbotAction* action, PlayerbotEvent const& event)
{
    bool result = true;
    for (auto listener : _listeners)
    {
        result &= listener->Before(action, event);
    }
    return result;
}

bool PlayerbotActionExecutionListeners::AllowExecution(PlayerbotAction* action, PlayerbotEvent const& event)
{
    bool result = true;
    for (auto listener : _listeners)
    {
        result &= listener->AllowExecution(action, event);
    }
    return result;
}

void PlayerbotActionExecutionListeners::After(PlayerbotAction* action, bool executed, PlayerbotEvent const& event)
{
    for (auto listener : _listeners)
    {
        listener->After(action, executed, event);
    }
}

bool PlayerbotActionExecutionListeners::OverrideResult(PlayerbotAction* action, bool executed, PlayerbotEvent const& event)
{
    bool result = executed;
    for (auto listener : _listeners)
    {
        result = listener->OverrideResult(action, result, event);
    }
    return result;
}

void PlayerbotActionExecutionListeners::Add(PlayerbotActionExecutionListener* listener)
{
    _listeners.push_back(listener);
}

void PlayerbotActionExecutionListeners::Remove(PlayerbotActionExecutionListener* listener)
{
    _listeners.remove(listener);
}

// PlayerbotEngine implementation
PlayerbotEngine::PlayerbotEngine(PlayerbotPlayerAI* ai, PlayerbotAiObjectContext* context)
    : _ai(ai), _context(context), _initialized(false), _testMode(false),
      _strategyTypeMask(0), _lastRelevance(0.0f)
{
}

PlayerbotEngine::~PlayerbotEngine()
{
    Uninitialize();
}

void PlayerbotEngine::Initialize()
{
    if (_initialized)
        return;

    if (!_ai || !_context)
    {
        TC_LOG_ERROR("playerbots", "PlayerbotEngine::Initialize: AI or Context is null");
        return;
    }

    try
    {
        InitializeStrategies();
        InitializeTriggers();
        InitializeMultipliers();
        
        _initialized = true;
        TC_LOG_DEBUG("playerbots", "PlayerbotEngine initialized successfully");
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("playerbots", "PlayerbotEngine::Initialize: Exception: {}", e.what());
        _initialized = false;
    }
}

void PlayerbotEngine::Uninitialize()
{
    if (!_initialized)
        return;

    RemoveAllStrategies();
    _triggers.clear();
    _multipliers.clear();
    
    _initialized = false;
    TC_LOG_DEBUG("playerbots", "PlayerbotEngine uninitialized");
}

void PlayerbotEngine::AddStrategy(std::string const& name, bool initialize)
{
    if (!_context)
        return;

    if (HasStrategy(name))
        return;

    auto strategy = std::unique_ptr<PlayerbotStrategy>(_context->CreateStrategy(name));
    if (strategy)
    {
        _strategies[name] = std::move(strategy);
        
        if (initialize)
        {
            _strategies[name]->InitializeActions();
            _strategies[name]->InitializeTriggers();
        }
        
        UpdateStrategyTypeMask();
        TC_LOG_DEBUG("playerbots", "Added strategy: {}", name);
    }
    else
    {
        TC_LOG_WARN("playerbots", "Failed to create strategy: {}", name);
    }
}

void PlayerbotEngine::AddStrategies(std::string const& strategies)
{
    auto strategyList = PlayerbotUtils::Split(strategies, ',');
    for (auto const& strategy : strategyList)
    {
        std::string trimmed = PlayerbotUtils::Trim(strategy);
        if (!trimmed.empty())
        {
            AddStrategy(trimmed);
        }
    }
}

bool PlayerbotEngine::RemoveStrategy(std::string const& name, bool initialize)
{
    auto it = _strategies.find(name);
    if (it != _strategies.end())
    {
        _strategies.erase(it);
        UpdateStrategyTypeMask();
        TC_LOG_DEBUG("playerbots", "Removed strategy: {}", name);
        return true;
    }
    return false;
}

bool PlayerbotEngine::HasStrategy(std::string const& name) const
{
    return _strategies.find(name) != _strategies.end();
}

void PlayerbotEngine::RemoveAllStrategies()
{
    _strategies.clear();
    _strategyTypeMask = 0;
    TC_LOG_DEBUG("playerbots", "Removed all strategies");
}

void PlayerbotEngine::ToggleStrategy(std::string const& name)
{
    if (HasStrategy(name))
        RemoveStrategy(name);
    else
        AddStrategy(name);
}

std::string PlayerbotEngine::ListStrategies() const
{
    std::vector<std::string> strategyNames;
    for (auto const& strategy : _strategies)
    {
        strategyNames.push_back(strategy.first);
    }
    return PlayerbotUtils::Join(strategyNames, ",");
}

std::vector<std::string> PlayerbotEngine::GetStrategies() const
{
    std::vector<std::string> result;
    for (auto const& strategy : _strategies)
    {
        result.push_back(strategy.first);
    }
    return result;
}

bool PlayerbotEngine::ContainsStrategyType(PlayerbotStrategyType type) const
{
    return (_strategyTypeMask & static_cast<uint32>(type)) != 0;
}

void PlayerbotEngine::ChangeStrategy(std::string const& strategies)
{
    RemoveAllStrategies();
    AddStrategies(strategies);
}

bool PlayerbotEngine::DoNextAction(Unit* target, uint32 depth, bool minimal)
{
    if (!_initialized)
    {
        TC_LOG_ERROR("playerbots", "PlayerbotEngine::DoNextAction: Engine not initialized");
        return false;
    }

    if (depth > 10) // Prevent infinite recursion
    {
        TC_LOG_WARN("playerbots", "PlayerbotEngine::DoNextAction: Maximum depth exceeded");
        return false;
    }

    try
    {
        ProcessTriggers(minimal);
        
        // For now, implement basic action selection
        // TODO: Implement proper action queue and selection logic
        
        return true;
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("playerbots", "PlayerbotEngine::DoNextAction: Exception: {}", e.what());
        return false;
    }
}

PlayerbotActionResult PlayerbotEngine::ExecuteAction(std::string const& name, PlayerbotEvent const& event, std::string const& qualifier)
{
    if (!_context)
        return PlayerbotActionResult::FAILED;

    auto action = std::unique_ptr<PlayerbotAction>(_context->CreateAction(name));
    if (!action)
    {
        TC_LOG_WARN("playerbots", "Failed to create action: {}", name);
        return PlayerbotActionResult::IMPOSSIBLE;
    }

    if (!action->isPossible())
        return PlayerbotActionResult::IMPOSSIBLE;

    if (!action->isUseful())
        return PlayerbotActionResult::USELESS;

    try
    {
        if (ListenAndExecute(action.get(), event))
        {
            _lastAction = name;
            return PlayerbotActionResult::OK;
        }
        else
        {
            return PlayerbotActionResult::FAILED;
        }
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("playerbots", "PlayerbotEngine::ExecuteAction: Exception executing {}: {}", name, e.what());
        return PlayerbotActionResult::FAILED;
    }
}

void PlayerbotEngine::AddActionExecutionListener(PlayerbotActionExecutionListener* listener)
{
    _actionExecutionListeners.Add(listener);
}

void PlayerbotEngine::RemoveActionExecutionListener(PlayerbotActionExecutionListener* listener)
{
    _actionExecutionListeners.Remove(listener);
}

void PlayerbotEngine::Reset()
{
    _lastAction.clear();
    _lastRelevance = 0.0f;
}

void PlayerbotEngine::ProcessTriggers(bool minimal)
{
    // TODO: Implement trigger processing logic
    // This should evaluate all triggers and queue appropriate actions
}

void PlayerbotEngine::PushDefaultActions()
{
    // TODO: Implement default action pushing
}

bool PlayerbotEngine::MultiplyAndPush(PlayerbotAction* action, float relevance, bool skipPrerequisites, PlayerbotEvent const& event)
{
    // TODO: Implement multiplier application and action queuing
    return true;
}

void PlayerbotEngine::PushAgain(PlayerbotActionNode* actionNode, float relevance, PlayerbotEvent const& event)
{
    // TODO: Implement action re-queuing logic
}

PlayerbotActionNode* PlayerbotEngine::CreateActionNode(std::string const& name)
{
    return new PlayerbotActionNode(name);
}

PlayerbotAction* PlayerbotEngine::InitializeAction(PlayerbotActionNode* actionNode)
{
    if (!actionNode || !_context)
        return nullptr;
    
    return _context->CreateAction(actionNode->GetAction());
}

bool PlayerbotEngine::ListenAndExecute(PlayerbotAction* action, PlayerbotEvent const& event)
{
    if (!action)
        return false;

    // Check execution listeners
    if (!_actionExecutionListeners.Before(action, event))
        return false;

    if (!_actionExecutionListeners.AllowExecution(action, event))
        return false;

    // Execute the action
    bool executed = false;
    try
    {
        executed = action->Execute(event);
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("playerbots", "Exception executing action {}: {}", action->GetName(), e.what());
        executed = false;
    }

    // Notify listeners
    _actionExecutionListeners.After(action, executed, event);
    executed = _actionExecutionListeners.OverrideResult(action, executed, event);

    return executed;
}

void PlayerbotEngine::LogAction(std::string const& message)
{
    if (_testMode)
    {
        TC_LOG_INFO("playerbots", "Action: {}", message);
    }
}

void PlayerbotEngine::LogValues()
{
    if (_testMode)
    {
        TC_LOG_INFO("playerbots", "Engine values logged");
    }
}

void PlayerbotEngine::InitializeStrategies()
{
    // Default strategies will be added here
    // For now, we'll keep it empty and let it be configured externally
}

void PlayerbotEngine::InitializeTriggers()
{
    // Initialize core triggers
    // TODO: Add basic triggers
}

void PlayerbotEngine::InitializeMultipliers()
{
    // Initialize multipliers
    // TODO: Add basic multipliers
}

void PlayerbotEngine::UpdateStrategyTypeMask()
{
    _strategyTypeMask = 0;
    for (auto const& strategy : _strategies)
    {
        _strategyTypeMask |= strategy.second->GetType();
    }
}

bool PlayerbotEngine::IsValidStrategy(std::string const& name) const
{
    return _context && _context->GetSupportedStrategies().find(name) != _context->GetSupportedStrategies().end();
}

#endif // WITH_PLAYERBOTS