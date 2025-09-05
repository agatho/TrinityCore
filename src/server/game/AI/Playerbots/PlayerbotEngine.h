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

#ifndef TRINITY_PLAYERBOTENGINE_H
#define TRINITY_PLAYERBOTENGINE_H

#ifdef WITH_PLAYERBOTS

#include <map>
#include <vector>
#include <string>
#include <list>
#include <memory>
#include "PlayerbotCommon.h"

class PlayerbotPlayerAI;
class PlayerbotAiObjectContext;
class PlayerbotAction;
class PlayerbotStrategy;
class PlayerbotTrigger;
class PlayerbotEvent;
class PlayerbotActionNode;
class PlayerbotMultiplier;
class Unit;

enum class PlayerbotActionResult : uint8
{
    UNKNOWN = 0,
    OK = 1,
    IMPOSSIBLE = 2,
    USELESS = 3,
    FAILED = 4
};

enum class PlayerbotStrategyType : uint32
{
    STRATEGY_TYPE_GENERIC = 1,
    STRATEGY_TYPE_COMBAT = 2,
    STRATEGY_TYPE_NONCOMBAT = 4,
    STRATEGY_TYPE_PULL = 8,
    STRATEGY_TYPE_BUFF = 16,
    STRATEGY_TYPE_CC = 32,
    STRATEGY_TYPE_CURE = 64,
    STRATEGY_TYPE_BOOST = 128,
    STRATEGY_TYPE_DPS = 256
};

/**
 * @brief Interface for listening to action execution events
 */
class TC_GAME_API PlayerbotActionExecutionListener
{
public:
    virtual ~PlayerbotActionExecutionListener() = default;

    virtual bool Before(PlayerbotAction* action, PlayerbotEvent const& event) = 0;
    virtual bool AllowExecution(PlayerbotAction* action, PlayerbotEvent const& event) = 0;
    virtual void After(PlayerbotAction* action, bool executed, PlayerbotEvent const& event) = 0;
    virtual bool OverrideResult(PlayerbotAction* action, bool executed, PlayerbotEvent const& event) = 0;
};

/**
 * @brief Manages multiple action execution listeners
 */
class TC_GAME_API PlayerbotActionExecutionListeners : public PlayerbotActionExecutionListener
{
public:
    virtual ~PlayerbotActionExecutionListeners();

    bool Before(PlayerbotAction* action, PlayerbotEvent const& event) override;
    bool AllowExecution(PlayerbotAction* action, PlayerbotEvent const& event) override;
    void After(PlayerbotAction* action, bool executed, PlayerbotEvent const& event) override;
    bool OverrideResult(PlayerbotAction* action, bool executed, PlayerbotEvent const& event) override;

    void Add(PlayerbotActionExecutionListener* listener);
    void Remove(PlayerbotActionExecutionListener* listener);

private:
    std::list<PlayerbotActionExecutionListener*> _listeners;
};

/**
 * @brief Core AI engine that manages strategies, actions, and decision-making
 * 
 * This is the heart of the playerbot AI system. It processes triggers from the game world,
 * evaluates strategies to determine appropriate actions, and executes those actions.
 * The engine uses a priority queue system to select the most relevant actions based
 * on current game state and configured strategies.
 */
class TC_GAME_API PlayerbotEngine
{
public:
    PlayerbotEngine(PlayerbotPlayerAI* ai, PlayerbotAiObjectContext* context);
    virtual ~PlayerbotEngine();

    // Initialization
    void Initialize();
    void Uninitialize();
    bool IsInitialized() const { return _initialized; }

    // Strategy management
    void AddStrategy(std::string const& name, bool initialize = true);
    void AddStrategies(std::string const& strategies);
    bool RemoveStrategy(std::string const& name, bool initialize = true);
    bool HasStrategy(std::string const& name) const;
    void RemoveAllStrategies();
    void ToggleStrategy(std::string const& name);
    std::string ListStrategies() const;
    std::vector<std::string> GetStrategies() const;
    bool ContainsStrategyType(PlayerbotStrategyType type) const;
    void ChangeStrategy(std::string const& strategies);

    // Main execution
    virtual bool DoNextAction(Unit* target = nullptr, uint32 depth = 0, bool minimal = false);
    PlayerbotActionResult ExecuteAction(std::string const& name, PlayerbotEvent const& event = PlayerbotEvent(), std::string const& qualifier = "");

    // Action execution listeners
    void AddActionExecutionListener(PlayerbotActionExecutionListener* listener);
    void RemoveActionExecutionListener(PlayerbotActionExecutionListener* listener);

    // State information
    std::string const& GetLastAction() const { return _lastAction; }
    float GetLastRelevance() const { return _lastRelevance; }
    uint32 GetStrategyTypeMask() const { return _strategyTypeMask; }

    // Configuration
    void SetTestMode(bool enabled) { _testMode = enabled; }
    bool IsTestMode() const { return _testMode; }

    PlayerbotPlayerAI* GetAI() const { return _ai; }
    PlayerbotAiObjectContext* GetContext() const { return _context; }

protected:
    // Engine internals
    void Reset();
    void ProcessTriggers(bool minimal);
    void PushDefaultActions();
    bool MultiplyAndPush(PlayerbotAction* action, float relevance, bool skipPrerequisites, PlayerbotEvent const& event);
    void PushAgain(PlayerbotActionNode* actionNode, float relevance, PlayerbotEvent const& event);
    
    PlayerbotActionNode* CreateActionNode(std::string const& name);
    PlayerbotAction* InitializeAction(PlayerbotActionNode* actionNode);
    bool ListenAndExecute(PlayerbotAction* action, PlayerbotEvent const& event);

    void LogAction(std::string const& message);
    void LogValues();

private:
    PlayerbotPlayerAI* _ai;
    PlayerbotAiObjectContext* _context;
    bool _initialized;
    bool _testMode;

    // Strategy management
    std::map<std::string, std::unique_ptr<PlayerbotStrategy>> _strategies;
    uint32 _strategyTypeMask;

    // Trigger and action management
    std::vector<std::unique_ptr<PlayerbotTrigger>> _triggers;
    std::vector<std::unique_ptr<PlayerbotMultiplier>> _multipliers;

    // Action queue and execution
    // TODO: Implement proper queue system
    std::string _lastAction;
    float _lastRelevance;

    // Event handling
    PlayerbotActionExecutionListeners _actionExecutionListeners;

    // Helper methods
    void InitializeStrategies();
    void InitializeTriggers();
    void InitializeMultipliers();
    void UpdateStrategyTypeMask();
    bool IsValidStrategy(std::string const& name) const;
};

#endif // WITH_PLAYERBOTS

#endif