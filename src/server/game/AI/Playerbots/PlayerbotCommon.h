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

#ifndef TRINITY_PLAYERBOTCOMMON_H
#define TRINITY_PLAYERBOTCOMMON_H

#ifdef WITH_PLAYERBOTS

#include <string>
#include <vector>

class PlayerbotPlayerAI;
class Unit;
class Player;

/**
 * @brief Threat assessment for combat actions
 */
enum class ThreatType : uint8
{
    NONE = 0,
    SINGLE = 1,
    AOE = 2,
    DEFENSIVE = 3
};

/**
 * @brief Base class for all playerbot AI components that need access to the AI instance
 */
class TC_GAME_API PlayerbotAIAware
{
public:
    explicit PlayerbotAIAware(PlayerbotPlayerAI* ai) : _ai(ai) {}
    virtual ~PlayerbotAIAware() = default;

protected:
    PlayerbotPlayerAI* _ai;
    PlayerbotPlayerAI* GetAI() const { return _ai; }
    
    // Common utility methods
    Player* GetBot() const;
    Unit* GetTarget() const;
    Unit* GetCurrentTarget() const;
    bool IsInCombat() const;
    bool IsInMeleeRange(Unit* target) const;
    float GetDistance(Unit* target) const;
    void LogAction(std::string const& message) const;
    void LogError(std::string const& message) const;
    bool HasPet() const;
    bool IsPetInCombat() const;
    bool MoveTo(float x, float y, float z) const;
};

/**
 * @brief Represents a game event that triggers bot reactions
 */
class TC_GAME_API PlayerbotEvent
{
public:
    PlayerbotEvent() : _source(nullptr), _owner(nullptr), _packet(nullptr) {}
    PlayerbotEvent(std::string const& source) : _source(nullptr), _owner(nullptr), _packet(nullptr), _sourceStr(source) {}

    // Event properties
    void* GetSource() const { return _source; }
    void* GetOwner() const { return _owner; }
    void* GetPacket() const { return _packet; }
    std::string const& GetParam() const { return _param; }

    void SetSource(void* source) { _source = source; }
    void SetOwner(void* owner) { _owner = owner; }
    void SetPacket(void* packet) { _packet = packet; }
    void SetParam(std::string const& param) { _param = param; }

    bool Empty() const { return _sourceStr.empty() && !_source; }

private:
    void* _source;
    void* _owner;
    void* _packet;
    std::string _sourceStr;
    std::string _param;
};

/**
 * @brief Base class for bot actions
 */
class TC_GAME_API PlayerbotAction : public PlayerbotAIAware
{
public:
    explicit PlayerbotAction(PlayerbotPlayerAI* ai, std::string const& name = "unknown");
    virtual ~PlayerbotAction() = default;

    virtual bool Execute(PlayerbotEvent const& event) = 0;
    virtual bool isUseful() { return true; }
    virtual bool isPossible() { return true; }
    virtual float GetRelevance() const { return 0.5f; }
    virtual uint32 GetCooldown() const { return 0; }
    virtual ThreatType GetThreatType() const { return ThreatType::SINGLE; }

    std::string const& GetName() const { return _name; }
    void SetName(std::string const& name) { _name = name; }

protected:
    std::string _name;
};

/**
 * @brief Base class for spell-casting actions
 */
class TC_GAME_API PlayerbotSpellAction : public PlayerbotAction
{
public:
    PlayerbotSpellAction(PlayerbotPlayerAI* ai, std::string const& name, uint32 spellId) 
        : PlayerbotAction(ai, name), _spellId(spellId) {}

    virtual Unit* GetSpellTarget() const;
    uint32 GetSpellId() const { return _spellId; }

    // Default Execute implementation using spell casting
    virtual bool Execute(PlayerbotEvent const& event) override;

protected:
    // Core spell casting method using TrinityCore patterns
    bool CastSpell(Unit* target = nullptr, bool triggered = false);
    
    // Spell validation methods
    bool CanCastSpell(Unit* target = nullptr) const;
    bool IsSpellReady() const;
    bool HasEnoughMana() const;
    bool IsInRange(Unit* target) const;

    uint32 _spellId;
};

/**
 * @brief Base class for movement-based actions
 */
class TC_GAME_API PlayerbotMovementAction : public PlayerbotAction
{
public:
    PlayerbotMovementAction(PlayerbotPlayerAI* ai, std::string const& name) 
        : PlayerbotAction(ai, name) {}

    virtual bool ExecuteMovement(Unit* target) { return false; }
};

/**
 * @brief Base class for bot strategies
 */
class TC_GAME_API PlayerbotStrategy : public PlayerbotAIAware
{
public:
    explicit PlayerbotStrategy(PlayerbotPlayerAI* ai, std::string const& name = "unknown");
    virtual ~PlayerbotStrategy() = default;

    virtual void InitializeActions() {}
    virtual void InitializeTriggers() {}
    virtual float GetActionPriority(std::string const& actionName) const { return 0.5f; }
    
    // Action registration (placeholder for now)  
    void RegisterAction(std::string const& name, std::unique_ptr<PlayerbotAction> action) {}
    template<typename T>
    void RegisterTrigger(std::string const& name, std::unique_ptr<T> trigger) {}

    std::string const& GetName() const { return _name; }
    void SetName(std::string const& name) { _name = name; }

    uint32 GetType() const { return _type; }
    void SetType(uint32 type) { _type = type; }

protected:
    std::string _name;
    uint32 _type;
};

/**
 * @brief Base class for bot triggers
 */
class TC_GAME_API PlayerbotTrigger : public PlayerbotAIAware
{
public:
    explicit PlayerbotTrigger(PlayerbotPlayerAI* ai, std::string const& name = "unknown");
    virtual ~PlayerbotTrigger() = default;

    virtual bool IsActive() { return false; }
    virtual PlayerbotEvent Check() { return PlayerbotEvent(); }

    std::string const& GetName() const { return _name; }
    void SetName(std::string const& name) { _name = name; }

protected:
    std::string _name;
};


/**
 * @brief Base class for bot values
 */
class TC_GAME_API PlayerbotUntypedValue : public PlayerbotAIAware
{
public:
    explicit PlayerbotUntypedValue(PlayerbotPlayerAI* ai, std::string const& name = "unknown");
    virtual ~PlayerbotUntypedValue() = default;

    std::string const& GetName() const { return _name; }
    void SetName(std::string const& name) { _name = name; }

protected:
    std::string _name;
};

/**
 * @brief Typed value template
 */
template<typename T>
class TC_GAME_API PlayerbotValue : public PlayerbotUntypedValue
{
public:
    explicit PlayerbotValue(PlayerbotPlayerAI* ai, std::string const& name = "unknown") 
        : PlayerbotUntypedValue(ai, name) {}

    virtual T Calculate() = 0;
    virtual void Set(T value) {}

    T Get() { return Calculate(); }
};

/**
 * @brief Action node for action queue system
 */
class TC_GAME_API PlayerbotActionNode
{
public:
    explicit PlayerbotActionNode(std::string const& action) : _action(action), _relevance(0.0f) {}

    std::string const& GetAction() const { return _action; }
    float GetRelevance() const { return _relevance; }
    void SetRelevance(float relevance) { _relevance = relevance; }

private:
    std::string _action;
    float _relevance;
};

/**
 * @brief Base class for multipliers that modify action relevance
 */
class TC_GAME_API PlayerbotMultiplier : public PlayerbotAIAware
{
public:
    explicit PlayerbotMultiplier(PlayerbotPlayerAI* ai, std::string const& name = "unknown");
    virtual ~PlayerbotMultiplier() = default;

    virtual float GetValue(PlayerbotAction* action) = 0;

    std::string const& GetName() const { return _name; }

protected:
    std::string _name;
};

// Utility functions
namespace PlayerbotUtils
{
    std::vector<std::string> Split(std::string const& str, char delimiter);
    std::string Join(std::vector<std::string> const& strings, std::string const& delimiter);
    std::string ToLower(std::string const& str);
    std::string Trim(std::string const& str);
}

#endif // WITH_PLAYERBOTS

#endif