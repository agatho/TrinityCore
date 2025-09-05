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

#ifndef TRINITY_PLAYERBOTPALADINSTRATEGY_H
#define TRINITY_PLAYERBOTPALADINSTRATEGY_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"

/**
 * @brief Base class for all Paladin strategies
 * 
 * Provides common functionality shared between Holy, Protection, and Retribution specs.
 * Manages Holy Power resource system and shared abilities like Judgment and Consecration.
 */
class TC_GAME_API PlayerbotPaladinStrategy : public PlayerbotStrategy
{
public:
    PlayerbotPaladinStrategy(PlayerbotPlayerAI* ai, std::string const& name);

    void InitializeActions() override;
    void InitializeTriggers() override;

    // Common strategy methods
    virtual float GetActionPriority(std::string const& actionName) const;

protected:
    // Common action and trigger registration
    void RegisterCommonActions();
    void RegisterCommonTriggers();

    // Common utility methods
    bool ShouldUseJudgment() const;
    bool ShouldUseConsecration() const;
    bool ShouldUseDivineShield() const;
    bool ShouldUseLayOnHands() const;
    bool NeedsHealing() const;
    bool IsInMeleeRange() const;
    
    // Resource management
    uint32 GetHolyPowerAmount() const;
    bool ShouldSaveHolyPower() const;
    uint32 CountNearbyEnemies(float range = 8.0f) const;
    
    // Specialization detection
    bool IsRetribution() const;
    bool IsProtection() const;
    bool IsHoly() const;
};

/**
 * @brief Retribution Paladin strategy - The War Within 11.2
 * 
 * Implements DPS rotation focused on Holy Power management:
 * - Generate Holy Power with Judgment, Crusader Strike, Blade of Justice
 * - Spend on Templar's Verdict (single target) or Divine Storm (AoE)
 * - Use major cooldowns like Execution Sentence and Avenging Wrath
 * - Priority system based on current retail meta
 */
class TC_GAME_API PlayerbotRetributionPaladinStrategy : public PlayerbotPaladinStrategy
{
public:
    PlayerbotRetributionPaladinStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Retribution-specific registration
    void RegisterRetributionActions();
    void RegisterRetributionTriggers();

    // Rotation logic methods
    bool ShouldUseTemplarVerdict() const;
    bool ShouldUseFinalVerdict() const;
    bool ShouldUseDivineStorm() const;
    bool ShouldUseBladeOfJustice() const;
    bool ShouldUseHammerOfWrath() const;
    bool ShouldUseExecutionSentence() const;
    bool ShouldUseWakeOfAshes() const;
    bool ShouldUseDivineToll() const;
    bool ShouldUseAvengingWrath() const;
    
    // Target evaluation
    bool IsTargetInExecuteRange(Unit* target) const;
    bool HasExpurgationDebuff(Unit* target) const;
    bool HasJudgmentDebuff(Unit* target) const;
    bool IsAoESituation() const;
    
private:
    static constexpr float EXECUTE_THRESHOLD = 0.20f; // 20% health for Hammer of Wrath
};

/**
 * @brief Protection Paladin strategy - The War Within 11.2
 * 
 * Implements tanking rotation focused on survivability and threat:
 * - Generate Holy Power with Judgment, Blessed Hammer, Hammer of the Righteous
 * - Spend on Shield of the Righteous for mitigation
 * - Use Avenger's Shield for threat/interrupts
 * - Maintain Consecration for damage and threat
 * - Manage defensive cooldowns proactively
 */
class TC_GAME_API PlayerbotProtectionPaladinStrategy : public PlayerbotPaladinStrategy
{
public:
    PlayerbotProtectionPaladinStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Protection-specific registration
    void RegisterProtectionActions();
    void RegisterProtectionTriggers();

    // Tanking logic methods
    bool ShouldUseShieldOfTheRighteous() const;
    bool ShouldUseAvengersShield() const;
    bool ShouldUseBlessedHammer() const;
    bool ShouldUseWordOfGlory() const;
    bool ShouldUseGuardianOfAncientKings() const;
    bool ShouldUseEyeOfTyr() const;
    bool ShouldUseArdentDefender() const;
    
    // Threat and positioning
    bool IsInTankingRole() const;
    bool HasAggro() const;
    bool NeedsEmergencyHealing() const;
    float GetDamageTakenRecently() const;
    
private:
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 0.3f;
    static constexpr float DEFENSIVE_COOLDOWN_THRESHOLD = 0.5f;
};

/**
 * @brief Holy Paladin strategy - The War Within 11.2
 * 
 * Implements healing rotation focused on reactive healing:
 * - Use Holy Shock as primary tool (healing/damage/Holy Power generation)
 * - Spend Holy Power on Word of Glory (single target) or Light of Dawn (AoE)
 * - Manage Beacon of Light for efficient healing
 * - Use major cooldowns like Avenging Wrath during damage spikes
 * - Prioritize keeping group alive over personal DPS
 */
class TC_GAME_API PlayerbotHolyPaladinStrategy : public PlayerbotPaladinStrategy
{
public:
    PlayerbotHolyPaladinStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Holy-specific registration
    void RegisterHolyActions();
    void RegisterHolyTriggers();

    // Healing logic methods
    bool ShouldUseHolyShock() const;
    bool ShouldUseHolyLight() const;
    bool ShouldUseFlashOfLight() const;
    bool ShouldUseWordOfGlory() const;
    bool ShouldUseLightOfDawn() const;
    bool ShouldUseBeaconOfLight() const;
    bool ShouldUseCleanse() const;
    bool ShouldUseBlessingOfFreedom() const;
    bool ShouldUseBlessingOfProtection() const;
    bool ShouldUseAvengingWrath() const;
    
    // Healing target evaluation
    Unit* FindBestHealingTarget() const;
    Unit* FindEmergencyTarget() const;
    Unit* FindBeaconTarget() const;
    Unit* FindCleanseTarget() const;
    Unit* FindBlessingTarget() const;
    
    // Group assessment
    uint32 CountInjuredMembers(float threshold = 0.8f) const;
    bool IsGroupDamageHigh() const;
    bool ShouldUseAoEHealing() const;
    bool ShouldDPS() const; // Use Holy Shock offensively when healing not needed
    
private:
    static constexpr float HEALING_THRESHOLD = 0.8f;      // Start healing below 80%
    static constexpr float EMERGENCY_THRESHOLD = 0.3f;    // Emergency healing below 30%
    static constexpr float AOE_HEALING_THRESHOLD = 0.7f;   // AoE heal when multiple below 70%
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTPALADINSTRATEGY_H