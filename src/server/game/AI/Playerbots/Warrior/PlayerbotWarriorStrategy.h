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

#ifndef TRINITY_PLAYERBOTWARRIORSTRATEGY_H
#define TRINITY_PLAYERBOTWARRIORSTRATEGY_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"
#include <memory>

class PlayerbotPlayerAI;

/**
 * @brief Base strategy for all warrior specializations
 */
class TC_GAME_API PlayerbotWarriorStrategy : public PlayerbotStrategy
{
public:
    PlayerbotWarriorStrategy(PlayerbotPlayerAI* ai, std::string const& name);

    virtual void InitializeActions() override;
    virtual void InitializeTriggers() override;

protected:
    // Common warrior actions available to all specs
    void RegisterCommonActions();
    void RegisterCommonTriggers();

    // Utility methods for subclasses
    bool ShouldUseBattleShout() const;
    bool ShouldUseDemoralizingShout() const;
    bool IsInTankingRole() const;
    bool IsInDamageRole() const;
};

/**
 * @brief Arms warrior strategy - focused on weapon damage and mortal strike
 */
class TC_GAME_API PlayerbotArmsWarriorStrategy : public PlayerbotWarriorStrategy
{
public:
    PlayerbotArmsWarriorStrategy(PlayerbotPlayerAI* ai);

    virtual void InitializeActions() override;
    virtual void InitializeTriggers() override;
    virtual float GetActionPriority(std::string const& actionName) const override;

private:
    void RegisterArmsActions();
    void RegisterArmsTriggers();
};

/**
 * @brief Fury warrior strategy - focused on dual-wield and berserker rage
 */
class TC_GAME_API PlayerbotFuryWarriorStrategy : public PlayerbotWarriorStrategy
{
public:
    PlayerbotFuryWarriorStrategy(PlayerbotPlayerAI* ai);

    virtual void InitializeActions() override;
    virtual void InitializeTriggers() override;
    virtual float GetActionPriority(std::string const& actionName) const override;

private:
    void RegisterFuryActions();
    void RegisterFuryTriggers();
};

/**
 * @brief Protection warrior strategy - focused on tanking and threat generation
 */
class TC_GAME_API PlayerbotProtectionWarriorStrategy : public PlayerbotWarriorStrategy
{
public:
    PlayerbotProtectionWarriorStrategy(PlayerbotPlayerAI* ai);

    virtual void InitializeActions() override;
    virtual void InitializeTriggers() override;
    virtual float GetActionPriority(std::string const& actionName) const override;

private:
    void RegisterProtectionActions();
    void RegisterProtectionTriggers();
    
    bool ShouldPrioritizeThreat() const;
    bool ShouldPrioritizeSurvival() const;
};

/**
 * @brief Factory class for creating warrior strategies
 */
class TC_GAME_API PlayerbotWarriorStrategyFactory
{
public:
    static std::unique_ptr<PlayerbotWarriorStrategy> CreateStrategy(
        PlayerbotPlayerAI* ai, 
        std::string const& strategyName);

    // Available strategy names
    static std::vector<std::string> GetAvailableStrategies();

private:
    static std::string DetermineSpecialization(PlayerbotPlayerAI* ai);
};

/**
 * @brief Trigger for warrior combat events
 */
class TC_GAME_API PlayerbotWarriorCombatTrigger : public PlayerbotTrigger
{
public:
    PlayerbotWarriorCombatTrigger(PlayerbotPlayerAI* ai, std::string const& name);

    virtual bool IsActive() override;

protected:
    virtual bool CheckCondition() const = 0;
    
    bool IsInCorrectStance(uint32 stanceSpellId) const;
    bool HasEnoughRage(uint32 rageCost) const;
    Unit* GetBestTarget() const;
};

/**
 * @brief Trigger for low health situations
 */
class TC_GAME_API PlayerbotWarriorLowHealthTrigger : public PlayerbotWarriorCombatTrigger
{
public:
    PlayerbotWarriorLowHealthTrigger(PlayerbotPlayerAI* ai, float threshold = 0.3f);

protected:
    virtual bool CheckCondition() const override;

private:
    float _healthThreshold;
};

/**
 * @brief Trigger for enemy casting interruption
 */
class TC_GAME_API PlayerbotWarriorInterruptTrigger : public PlayerbotWarriorCombatTrigger
{
public:
    PlayerbotWarriorInterruptTrigger(PlayerbotPlayerAI* ai);

protected:
    virtual bool CheckCondition() const override;

private:
    Unit* FindInterruptTarget() const;
    bool ShouldInterruptSpell(Unit* caster, SpellInfo const* spellInfo) const;
};

/**
 * @brief Trigger for stance management
 */
class TC_GAME_API PlayerbotWarriorStanceTrigger : public PlayerbotWarriorCombatTrigger
{
public:
    PlayerbotWarriorStanceTrigger(PlayerbotPlayerAI* ai, uint32 requiredStance, std::string const& name);

protected:
    virtual bool CheckCondition() const override;

private:
    uint32 _requiredStanceSpellId;
    bool ShouldSwitchStance() const;
};

/**
 * @brief Trigger for threat management (tanking)
 */
class TC_GAME_API PlayerbotWarriorThreatTrigger : public PlayerbotWarriorCombatTrigger
{
public:
    PlayerbotWarriorThreatTrigger(PlayerbotPlayerAI* ai);

protected:
    virtual bool CheckCondition() const override;

private:
    bool HasThreatOnTarget(Unit* target) const;
    bool ShouldTaunt() const;
    Unit* FindTauntTarget() const;
};

/**
 * @brief Trigger for rage management
 */
class TC_GAME_API PlayerbotWarriorRageTrigger : public PlayerbotWarriorCombatTrigger
{
public:
    PlayerbotWarriorRageTrigger(PlayerbotPlayerAI* ai, uint32 rageThreshold = 20);

protected:
    virtual bool CheckCondition() const override;

private:
    uint32 _rageThreshold;
    bool IsRageLow() const;
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTWARRIORSTRATEGY_H