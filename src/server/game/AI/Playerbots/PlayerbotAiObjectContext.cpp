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

#include "PlayerbotAiObjectContext.h"
#include "PlayerbotPlayerAI.h"
#include "PlayerbotCommon.h"
#include "Log.h"

PlayerbotAiObjectContext::PlayerbotAiObjectContext(PlayerbotPlayerAI* ai)
    : _ai(ai), _initialized(false)
{
}

PlayerbotAiObjectContext::~PlayerbotAiObjectContext()
{
    _createdStrategies.clear();
    _createdActions.clear();
    _createdTriggers.clear();
    _createdValues.clear();
}

void PlayerbotAiObjectContext::Initialize()
{
    if (_initialized)
        return;

    try
    {
        InitializeStrategies();
        InitializeActions();
        InitializeTriggers();
        InitializeValues();
        
        _initialized = true;
        TC_LOG_DEBUG("playerbots", "PlayerbotAiObjectContext initialized");
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("playerbots", "PlayerbotAiObjectContext::Initialize: Exception: {}", e.what());
        _initialized = false;
    }
}

PlayerbotStrategy* PlayerbotAiObjectContext::CreateStrategy(std::string const& name)
{
    auto it = _strategyCreators.find(name);
    if (it == _strategyCreators.end())
    {
        TC_LOG_WARN("playerbots", "Unknown strategy: {}", name);
        return nullptr;
    }

    try
    {
        PlayerbotStrategy* strategy = it->second(_ai);
        if (strategy)
        {
            _createdStrategies.push_back(std::unique_ptr<PlayerbotStrategy>(strategy));
            return strategy;
        }
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("playerbots", "Exception creating strategy {}: {}", name, e.what());
    }
    
    return nullptr;
}

PlayerbotAction* PlayerbotAiObjectContext::CreateAction(std::string const& name)
{
    auto it = _actionCreators.find(name);
    if (it == _actionCreators.end())
    {
        TC_LOG_WARN("playerbots", "Unknown action: {}", name);
        return nullptr;
    }

    try
    {
        PlayerbotAction* action = it->second(_ai);
        if (action)
        {
            _createdActions.push_back(std::unique_ptr<PlayerbotAction>(action));
            return action;
        }
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("playerbots", "Exception creating action {}: {}", name, e.what());
    }
    
    return nullptr;
}

PlayerbotTrigger* PlayerbotAiObjectContext::CreateTrigger(std::string const& name)
{
    auto it = _triggerCreators.find(name);
    if (it == _triggerCreators.end())
    {
        TC_LOG_WARN("playerbots", "Unknown trigger: {}", name);
        return nullptr;
    }

    try
    {
        PlayerbotTrigger* trigger = it->second(_ai);
        if (trigger)
        {
            _createdTriggers.push_back(std::unique_ptr<PlayerbotTrigger>(trigger));
            return trigger;
        }
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("playerbots", "Exception creating trigger {}: {}", name, e.what());
    }
    
    return nullptr;
}

PlayerbotUntypedValue* PlayerbotAiObjectContext::CreateUntypedValue(std::string const& name)
{
    auto it = _valueCreators.find(name);
    if (it == _valueCreators.end())
    {
        TC_LOG_WARN("playerbots", "Unknown value: {}", name);
        return nullptr;
    }

    try
    {
        PlayerbotUntypedValue* value = it->second(_ai);
        if (value)
        {
            _createdValues.push_back(std::unique_ptr<PlayerbotUntypedValue>(value));
            return value;
        }
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("playerbots", "Exception creating value {}: {}", name, e.what());
    }
    
    return nullptr;
}

void PlayerbotAiObjectContext::RegisterStrategy(std::string const& name, StrategyCreator creator)
{
    _strategyCreators[name] = creator;
}

void PlayerbotAiObjectContext::RegisterAction(std::string const& name, ActionCreator creator)
{
    _actionCreators[name] = creator;
}

void PlayerbotAiObjectContext::RegisterTrigger(std::string const& name, TriggerCreator creator)
{
    _triggerCreators[name] = creator;
}

void PlayerbotAiObjectContext::RegisterValue(std::string const& name, ValueCreator creator)
{
    _valueCreators[name] = creator;
}

std::set<std::string> PlayerbotAiObjectContext::GetSupportedStrategies() const
{
    std::set<std::string> result;
    for (auto const& creator : _strategyCreators)
    {
        result.insert(creator.first);
    }
    return result;
}

std::set<std::string> PlayerbotAiObjectContext::GetSupportedActions() const
{
    std::set<std::string> result;
    for (auto const& creator : _actionCreators)
    {
        result.insert(creator.first);
    }
    return result;
}

std::set<std::string> PlayerbotAiObjectContext::GetSupportedTriggers() const
{
    std::set<std::string> result;
    for (auto const& creator : _triggerCreators)
    {
        result.insert(creator.first);
    }
    return result;
}

std::set<std::string> PlayerbotAiObjectContext::GetSupportedValues() const
{
    std::set<std::string> result;
    for (auto const& creator : _valueCreators)
    {
        result.insert(creator.first);
    }
    return result;
}

void PlayerbotAiObjectContext::InitializeStrategies()
{
    // Register basic default strategy
    RegisterStrategy("default", [](PlayerbotPlayerAI* ai) -> PlayerbotStrategy* {
        return new PlayerbotStrategy(ai, "default");
    });
    
    TC_LOG_DEBUG("playerbots", "Strategies initialized: {} registered", _strategyCreators.size());
}

void PlayerbotAiObjectContext::InitializeActions()
{
    // Register basic default actions
    RegisterAction("wait", [](PlayerbotPlayerAI* ai) -> PlayerbotAction* {
        class WaitAction : public PlayerbotAction {
        public:
            WaitAction(PlayerbotPlayerAI* ai) : PlayerbotAction(ai, "wait") {}
            bool Execute(PlayerbotEvent const& event) override { return true; }
        };
        return new WaitAction(ai);
    });
    
    TC_LOG_DEBUG("playerbots", "Actions initialized: {} registered", _actionCreators.size());
}

void PlayerbotAiObjectContext::InitializeTriggers()
{
    // Register basic default triggers
    RegisterTrigger("timer", [](PlayerbotPlayerAI* ai) -> PlayerbotTrigger* {
        class TimerTrigger : public PlayerbotTrigger {
        public:
            TimerTrigger(PlayerbotPlayerAI* ai) : PlayerbotTrigger(ai, "timer") {}
            bool IsActive() override { return false; } // Placeholder
        };
        return new TimerTrigger(ai);
    });
    
    TC_LOG_DEBUG("playerbots", "Triggers initialized: {} registered", _triggerCreators.size());
}

void PlayerbotAiObjectContext::InitializeValues()
{
    // Register basic default values
    RegisterValue("health", [](PlayerbotPlayerAI* ai) -> PlayerbotUntypedValue* {
        return new PlayerbotUntypedValue(ai, "health");
    });
    
    TC_LOG_DEBUG("playerbots", "Values initialized: {} registered", _valueCreators.size());
}

#endif // WITH_PLAYERBOTS