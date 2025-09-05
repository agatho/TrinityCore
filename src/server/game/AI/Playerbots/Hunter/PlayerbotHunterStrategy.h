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

#ifndef TRINITY_PLAYERBOTHUNTERSTRATEGY_H
#define TRINITY_PLAYERBOTHUNTERSTRATEGY_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"
#include <memory>

class PlayerbotPlayerAI;

/**
 * @brief Base strategy for all hunter specializations
 */
class TC_GAME_API PlayerbotHunterStrategy : public PlayerbotStrategy
{
public:
    PlayerbotHunterStrategy(PlayerbotPlayerAI* ai, std::string const& name);

    virtual void InitializeActions() override;
    virtual void InitializeTriggers() override;

protected:
    // Common hunter actions available to all specs
    void RegisterCommonActions();
    void RegisterCommonTriggers();

    // Utility methods for subclasses
    bool ShouldUseHuntersMark() const;
    bool HasActivePet() const;
    bool ShouldCallPet() const;
    bool IsInExecuteRange(Unit* target) const;
};

/**
 * @brief Beast Mastery hunter strategy - focused on pet synergy and Kill Command
 */
class TC_GAME_API PlayerbotBeastMasteryHunterStrategy : public PlayerbotHunterStrategy
{
public:
    PlayerbotBeastMasteryHunterStrategy(PlayerbotPlayerAI* ai);

    virtual void InitializeActions() override;
    virtual void InitializeTriggers() override;
    virtual float GetActionPriority(std::string const& actionName) const override;

private:
    void RegisterBeastMasteryActions();
    void RegisterBeastMasteryTriggers();
};

/**
 * @brief Marksmanship hunter strategy - focused on Aimed Shot and ranged damage
 */
class TC_GAME_API PlayerbotMarksmanshipHunterStrategy : public PlayerbotHunterStrategy
{
public:
    PlayerbotMarksmanshipHunterStrategy(PlayerbotPlayerAI* ai);

    virtual void InitializeActions() override;
    virtual void InitializeTriggers() override;
    virtual float GetActionPriority(std::string const& actionName) const override;

private:
    void RegisterMarksmanshipActions();
    void RegisterMarksmanshipTriggers();
};

/**
 * @brief Survival hunter strategy - focused on melee combat and traps
 */
class TC_GAME_API PlayerbotSurvivalHunterStrategy : public PlayerbotHunterStrategy
{
public:
    PlayerbotSurvivalHunterStrategy(PlayerbotPlayerAI* ai);

    virtual void InitializeActions() override;
    virtual void InitializeTriggers() override;
    virtual float GetActionPriority(std::string const& actionName) const override;

private:
    void RegisterSurvivalActions();
    void RegisterSurvivalTriggers();
};

/**
 * @brief Factory class for creating hunter strategies
 */
class TC_GAME_API PlayerbotHunterStrategyFactory
{
public:
    static std::unique_ptr<PlayerbotHunterStrategy> CreateStrategy(
        PlayerbotPlayerAI* ai, 
        std::string const& strategyName);

    // Available strategy names
    static std::vector<std::string> GetAvailableStrategies();

private:
    static std::string DetermineSpecialization(PlayerbotPlayerAI* ai);
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTHUNTERSTRATEGY_H