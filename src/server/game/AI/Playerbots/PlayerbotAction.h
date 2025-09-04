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

#ifndef TRINITY_PLAYERBOTACTION_H
#define TRINITY_PLAYERBOTACTION_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"
#include <string>
#include <vector>

class PlayerbotPlayerAI;
class Unit;
class Player;

enum class PlayerbotEvent : uint8
{
    NONE = 0,
    COMBAT_START = 1,
    COMBAT_END = 2,
    TARGET_CHANGED = 3,
    HEALTH_LOW = 4,
    MANA_LOW = 5,
    SPELL_CAST = 6,
    DAMAGE_TAKEN = 7,
    ENEMY_HEALED = 8,
    PARTY_MEMBER_LOW_HEALTH = 9,
    PARTY_MEMBER_LOW_MANA = 10
};

/**
 * @brief Represents a potential follow-up action with priority
 */
class TC_GAME_API PlayerbotNextAction
{
public:
    PlayerbotNextAction(std::string const& name, float relevance = 0.0f)
        : _name(name), _relevance(relevance) {}

    std::string const& GetName() const { return _name; }
    float GetRelevance() const { return _relevance; }

private:
    std::string _name;
    float _relevance;
};

/**
 * @brief Base class for all bot actions
 *
 * Actions represent individual behaviors that bots can execute,
 * such as casting spells, moving, or interacting with objects.
 */
class TC_GAME_API PlayerbotAction
{
public:
    enum class ThreatType : uint8
    {
        NONE = 0,
        SINGLE = 1,
        AOE = 2
    };

    PlayerbotAction(PlayerbotPlayerAI* ai, std::string const& name);
    virtual ~PlayerbotAction() = default;

    // Core action interface
    virtual bool Execute(PlayerbotEvent event) = 0;
    virtual bool IsPossible() const { return true; }
    virtual bool IsUseful() const { return true; }

    // Action chaining
    virtual std::vector<PlayerbotNextAction> GetPrerequisites() const { return {}; }
    virtual std::vector<PlayerbotNextAction> GetAlternatives() const { return {}; }
    virtual std::vector<PlayerbotNextAction> GetContinuers() const { return {}; }

    // Action properties
    virtual ThreatType GetThreatType() const { return ThreatType::NONE; }
    virtual uint32 GetCooldown() const { return 0; }
    virtual float GetRelevance() const { return 1.0f; }

    // Action lifecycle
    virtual void Update() {}
    virtual void Reset() {}

    // Getters
    std::string const& GetName() const { return _name; }
    PlayerbotPlayerAI* GetAI() const { return _ai; }
    Player* GetBot() const;

protected:
    // Utility methods for derived classes
    bool CanCastSpell(uint32 spellId) const;
    bool IsInCombat() const;
    Unit* GetTarget() const;
    Unit* GetCurrentTarget() const;
    
    // Logging
    void LogAction(std::string const& message) const;
    void LogError(std::string const& error) const;

private:
    PlayerbotPlayerAI* _ai;
    std::string _name;
};

/**
 * @brief Action that involves movement
 */
class TC_GAME_API PlayerbotMovementAction : public PlayerbotAction
{
public:
    PlayerbotMovementAction(PlayerbotPlayerAI* ai, std::string const& name);

    virtual bool Execute(PlayerbotEvent event) override;

protected:
    virtual bool ExecuteMovement(Unit* target) = 0;
    
    bool MoveTo(float x, float y, float z);
    bool MoveTo(Unit* target, float distance = 0.0f);
    bool IsInRange(Unit* target, float distance) const;
    float GetDistance(Unit* target) const;

private:
    uint32 _lastMovement;
    static constexpr uint32 MOVEMENT_COOLDOWN = 1000; // 1 second
};

/**
 * @brief Action that involves spell casting
 */
class TC_GAME_API PlayerbotSpellAction : public PlayerbotAction
{
public:
    PlayerbotSpellAction(PlayerbotPlayerAI* ai, std::string const& name, uint32 spellId);

    virtual bool Execute(PlayerbotEvent event) override;
    virtual bool IsPossible() const override;
    virtual uint32 GetCooldown() const override;

protected:
    virtual Unit* GetSpellTarget() const;
    virtual bool CanCastSpell() const;
    
    bool CastSpell(Unit* target = nullptr);
    bool CastSpell(uint32 spellId, Unit* target = nullptr);
    
    uint32 GetSpellId() const { return _spellId; }
    uint32 GetManaCost() const;
    uint32 GetCastTime() const;
    float GetSpellRange() const;

private:
    uint32 _spellId;
};

/**
 * @brief Action that involves attacking
 */
class TC_GAME_API PlayerbotAttackAction : public PlayerbotMovementAction
{
public:
    PlayerbotAttackAction(PlayerbotPlayerAI* ai, std::string const& name);

    virtual bool Execute(PlayerbotEvent event) override;
    virtual bool IsUseful() const override;
    virtual ThreatType GetThreatType() const override { return ThreatType::SINGLE; }

protected:
    virtual bool ExecuteMovement(Unit* target) override;
    virtual Unit* GetAttackTarget() const;
    
    bool Attack(Unit* target, bool usePet = true);
    bool IsInMeleeRange(Unit* target) const;

private:
    bool StartAttack(Unit* target);
    bool ShouldUsePet() const;
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTACTION_H