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

#ifndef TRINITY_PLAYERBOTCOMBATACTIONS_H
#define TRINITY_PLAYERBOTCOMBATACTIONS_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"

/**
 * @brief Base class for combat actions that target enemies
 */
class TC_GAME_API PlayerbotAttackAction : public PlayerbotAction
{
public:
    PlayerbotAttackAction(PlayerbotPlayerAI* ai, std::string const& name) 
        : PlayerbotAction(ai, name) {}

    virtual bool IsInMeleeRange(Unit* target) const;
    virtual bool MoveTo(Unit* target, float distance = 0.0f);
    virtual bool Attack(Unit* target);
    virtual Unit* GetAttackTarget() const;
    virtual uint32 GetCooldown() const { return 0; }
};

/**
 * @brief Basic melee attack action
 */
class TC_GAME_API PlayerbotMeleeAction : public PlayerbotAttackAction
{
public:
    PlayerbotMeleeAction(PlayerbotPlayerAI* ai) : PlayerbotAttackAction(ai, "melee") {}

    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
    virtual float GetRelevance() const override;

protected:
    virtual Unit* GetAttackTarget() const override;
};

/**
 * @brief Switch to melee combat strategy
 */
class TC_GAME_API PlayerbotSwitchToMeleeAction : public PlayerbotAction
{
public:
    PlayerbotSwitchToMeleeAction(PlayerbotPlayerAI* ai) : PlayerbotAction(ai, "switch to melee") {}

    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
};

/**
 * @brief Switch to ranged combat strategy
 */
class TC_GAME_API PlayerbotSwitchToRangedAction : public PlayerbotAction
{
public:
    PlayerbotSwitchToRangedAction(PlayerbotPlayerAI* ai) : PlayerbotAction(ai, "switch to ranged") {}

    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
};

/**
 * @brief Auto attack action (weapon attacks)
 */
class TC_GAME_API PlayerbotAutoAttackAction : public PlayerbotAttackAction
{
public:
    PlayerbotAutoAttackAction(PlayerbotPlayerAI* ai) : PlayerbotAttackAction(ai, "auto attack") {}

    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
    virtual uint32 GetCooldown() const override { return 100; } // 100ms between checks
};

/**
 * @brief Pet attack command
 */
class TC_GAME_API PlayerbotPetAttackAction : public PlayerbotAction
{
public:
    PlayerbotPetAttackAction(PlayerbotPlayerAI* ai) : PlayerbotAction(ai, "pet attack") {}

    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
    virtual bool isPossible() override;

private:
    bool HasPet() const;
    bool IsPetInCombat() const;
};

/**
 * @brief Flee from combat when health is critically low
 */
class TC_GAME_API PlayerbotFleeAction : public PlayerbotMovementAction
{
public:
    PlayerbotFleeAction(PlayerbotPlayerAI* ai) : PlayerbotMovementAction(ai, "flee") {}

    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
    virtual float GetRelevance() const override;

protected:
    virtual bool ExecuteMovement(Unit* target) override;

private:
    bool ShouldFlee() const;
    bool FindSafePosition(float& x, float& y, float& z) const;
};

/**
 * @brief Base class for combat spell actions
 */
class TC_GAME_API PlayerbotCombatSpellAction : public PlayerbotSpellAction
{
public:
    PlayerbotCombatSpellAction(PlayerbotPlayerAI* ai, std::string const& name, uint32 spellId)
        : PlayerbotSpellAction(ai, name, spellId) {}

    virtual bool isUseful() override;
    virtual ThreatType GetThreatType() const override;

protected:
    virtual Unit* GetSpellTarget() const override;
    
    // Combat-specific target selection
    Unit* GetBestHostileTarget() const;
    Unit* GetNearestHostileTarget() const;
    Unit* GetWeakestHostileTarget() const;

protected:
    bool IsValidCombatTarget(Unit* target) const;
};

/**
 * @brief Defensive spell action (shields, buffs, etc.)
 */
class TC_GAME_API PlayerbotDefensiveSpellAction : public PlayerbotSpellAction
{
public:
    PlayerbotDefensiveSpellAction(PlayerbotPlayerAI* ai, std::string const& name, uint32 spellId)
        : PlayerbotSpellAction(ai, name, spellId) {}

    virtual bool isUseful() override;
    virtual float GetRelevance() const override;

protected:
    virtual Unit* GetSpellTarget() const override;

private:
    bool ShouldUseDefensiveSpell() const;
    float GetHealthPercentage() const;
};

/**
 * @brief Healing spell action
 */
class TC_GAME_API PlayerbotHealAction : public PlayerbotSpellAction
{
public:
    PlayerbotHealAction(PlayerbotPlayerAI* ai, std::string const& name, uint32 spellId)
        : PlayerbotSpellAction(ai, name, spellId) {}

    virtual bool isUseful() override;
    virtual float GetRelevance() const override;

protected:
    virtual Unit* GetSpellTarget() const override;

private:
    Unit* GetBestHealTarget() const;
    Unit* GetMostWoundedTarget() const;
    bool NeedsHealing(Unit* target) const;
    float GetHealingThreshold() const;
};

/**
 * @brief Area of Effect (AoE) spell action
 */
class TC_GAME_API PlayerbotAoEAction : public PlayerbotCombatSpellAction
{
public:
    PlayerbotAoEAction(PlayerbotPlayerAI* ai, std::string const& name, uint32 spellId)
        : PlayerbotCombatSpellAction(ai, name, spellId) {}

    virtual bool isUseful() override;
    virtual ThreatType GetThreatType() const override { return ThreatType::AOE; }
    virtual float GetRelevance() const override;

private:
    uint32 CountNearbyEnemies() const;
    uint32 GetMinEnemyCount() const { return 3; } // Minimum enemies to use AoE
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTCOMBATACTIONS_H