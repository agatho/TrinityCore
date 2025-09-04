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

#ifndef TRINITY_PLAYERBOTAIOBJECTCONTEXT_H
#define TRINITY_PLAYERBOTAIOBJECTCONTEXT_H

#ifdef WITH_PLAYERBOTS

#include <map>
#include <set>
#include <string>
#include <vector>
#include <memory>
#include <functional>

class PlayerbotPlayerAI;
class PlayerbotStrategy;
class PlayerbotAction;
class PlayerbotTrigger;
class PlayerbotUntypedValue;

template<typename T>
class PlayerbotValue;

// Factory function types for creating AI components
typedef std::function<PlayerbotStrategy*(PlayerbotPlayerAI*)> StrategyCreator;
typedef std::function<PlayerbotAction*(PlayerbotPlayerAI*)> ActionCreator;
typedef std::function<PlayerbotTrigger*(PlayerbotPlayerAI*)> TriggerCreator;
typedef std::function<PlayerbotUntypedValue*(PlayerbotPlayerAI*)> ValueCreator;

/**
 * @brief Factory and context manager for all bot AI components
 * 
 * This class manages the creation and lifetime of strategies, actions,
 * triggers, and values used by the bot AI system. It serves as the
 * central registry for all AI components.
 */
class TC_GAME_API PlayerbotAiObjectContext
{
public:
    explicit PlayerbotAiObjectContext(PlayerbotPlayerAI* ai);
    virtual ~PlayerbotAiObjectContext();

    // Component creation
    PlayerbotStrategy* CreateStrategy(std::string const& name);
    PlayerbotAction* CreateAction(std::string const& name);
    PlayerbotTrigger* CreateTrigger(std::string const& name);
    PlayerbotUntypedValue* CreateUntypedValue(std::string const& name);

    template<typename T>
    PlayerbotValue<T>* CreateValue(std::string const& name)
    {
        return dynamic_cast<PlayerbotValue<T>*>(CreateUntypedValue(name));
    }

    template<typename T>
    PlayerbotValue<T>* CreateValue(std::string const& name, std::string const& param)
    {
        return CreateValue<T>(name + "::" + param);
    }

    // Component registration
    void RegisterStrategy(std::string const& name, StrategyCreator creator);
    void RegisterAction(std::string const& name, ActionCreator creator);
    void RegisterTrigger(std::string const& name, TriggerCreator creator);
    void RegisterValue(std::string const& name, ValueCreator creator);

    // Information queries
    std::set<std::string> GetSupportedStrategies() const;
    std::set<std::string> GetSupportedActions() const;
    std::set<std::string> GetSupportedTriggers() const;
    std::set<std::string> GetSupportedValues() const;

    // Initialization
    void Initialize();
    bool IsInitialized() const { return _initialized; }

    PlayerbotPlayerAI* GetAI() const { return _ai; }

protected:
    // Initialize component factories
    virtual void InitializeStrategies();
    virtual void InitializeActions();
    virtual void InitializeTriggers();
    virtual void InitializeValues();

private:
    PlayerbotPlayerAI* _ai;
    bool _initialized;

    // Component factories
    std::map<std::string, StrategyCreator> _strategyCreators;
    std::map<std::string, ActionCreator> _actionCreators;
    std::map<std::string, TriggerCreator> _triggerCreators;
    std::map<std::string, ValueCreator> _valueCreators;

    // Created objects (for cleanup)
    std::vector<std::unique_ptr<PlayerbotStrategy>> _createdStrategies;
    std::vector<std::unique_ptr<PlayerbotAction>> _createdActions;
    std::vector<std::unique_ptr<PlayerbotTrigger>> _createdTriggers;
    std::vector<std::unique_ptr<PlayerbotUntypedValue>> _createdValues;
};

#endif // WITH_PLAYERBOTS

#endif