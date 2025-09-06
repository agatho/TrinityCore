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

#ifndef TRINITY_PLAYERBOTROGUESTRATEGY_H
#define TRINITY_PLAYERBOTROGUESTRATEGY_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"

/**
 * @brief Base class for all Rogue strategies
 * 
 * Provides common functionality shared between Assassination, Outlaw, and Subtlety specs.
 * Manages energy/combo point resources and shared utility like stealth and interrupts.
 */
class TC_GAME_API PlayerbotRogueStrategy : public PlayerbotStrategy
{
public:
    PlayerbotRogueStrategy(PlayerbotPlayerAI* ai, std::string const& name);

    void InitializeActions() override;
    void InitializeTriggers() override;

    // Common strategy methods
    virtual float GetActionPriority(std::string const& actionName) const;

protected:
    // Common action and trigger registration
    void RegisterCommonActions();
    void RegisterCommonTriggers();

    // Common utility methods
    bool ShouldUseStealth() const;
    bool ShouldUseVanish() const;
    bool ShouldUseKick() const;
    bool ShouldUseFeint() const;
    bool ShouldUseCloakOfShadows() const;
    bool ShouldUseEvasion() const;
    bool ShouldUseCrimsonVial() const;
    bool ShouldUseSprint() const;
    bool NeedsHealing() const;
    
    // Resource management
    uint32 GetEnergyPercent() const;
    uint32 GetComboPoints() const;
    uint32 GetComboPoints(Unit* target) const;
    bool HasMaxComboPoints() const;
    bool IsNearMaxComboPoints(uint32 threshold = 4) const;
    
    // Specialization detection
    bool isAssassination() const;
    bool isOutlaw() const;
    bool isSubtlety() const;
    
    // Target evaluation
    Unit* FindBestTarget() const;
    bool IsInCombat() const;
    bool IsStealthed() const;
    bool IsBehindTarget(Unit* target) const;
    uint32 CountNearbyEnemies(float range = 8.0f) const;
    bool ShouldUseAoE() const;
};

/**
 * @brief Assassination Rogue strategy - The War Within 11.2
 * 
 * Implements DPS rotation focused on DoT maintenance and burst windows:
 * - Maintain DoTs: Garrote (from stealth) and Rupture on target
 * - Generate combo points with Mutilate as primary builder
 * - Spend combo points on Envenom to maintain slice and dice
 * - Use major cooldowns: Vendetta, Deathmark, Kingsbane during burst
 * - Shiv usage for debuff application and combo point generation
 */
class TC_GAME_API PlayerbotAssassinationRogueStrategy : public PlayerbotRogueStrategy
{
public:
    PlayerbotAssassinationRogueStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Assassination-specific registration
    void RegisterAssassinationActions();
    void RegisterAssassinationTriggers();

    // Rotation logic methods
    bool ShouldMaintainGarrote() const;
    bool ShouldMaintainRupture() const;
    bool ShouldUseMutilate() const;
    bool ShouldUseEnvenom() const;
    bool ShouldUseVendetta() const;
    bool ShouldUseDeathmark() const;
    bool ShouldUseKingsbane() const;
    bool ShouldUseShiv() const;
    bool ShouldUseFanOfKnives() const;
    bool ShouldUseColdBlood() const;
    
    // State evaluation
    bool IsInBurstWindow() const;
    bool ShouldEnterBurstMode() const;
    bool HasGarroteOnTarget(Unit* target) const;
    bool HasRuptureOnTarget(Unit* target) const;
    bool HasDeadlyPoison() const;
    bool HasWoundPoison() const;
    bool ShouldRefreshPoisons() const;
    
    // DoT management
    float GetGarroteRemainingDuration(Unit* target) const;
    float GetRuptureRemainingDuration(Unit* target) const;
    bool ShouldRefreshGarrote(Unit* target) const;
    bool ShouldRefreshRupture(Unit* target) const;
    
private:
    static constexpr float DOT_REFRESH_THRESHOLD = 4.8f; // Refresh DoTs with <4.8s remaining
    static constexpr uint32 ENVENOM_MIN_CP = 4;          // Use Envenom with 4+ combo points
    static constexpr uint32 RUPTURE_MIN_CP = 4;          // Use Rupture with 4+ combo points
    static constexpr float BURST_HEALTH_THRESHOLD = 0.35f; // Use burst when target >35% HP
};

/**
 * @brief Outlaw Rogue strategy - The War Within 11.2
 * 
 * Implements DPS rotation focused on Roll the Bones buff management:
 * - Generate combo points with Sinister Strike as primary builder
 * - Maintain Roll the Bones buffs (reroll if <2 buffs)
 * - Use Between the Eyes and Dispatch as primary finishers
 * - Pistol Shot on Opportunity procs or at range
 * - Major cooldowns: Adrenaline Rush, Blade Flurry for AoE, Killing Spree
 */
class TC_GAME_API PlayerbotOutlawRogueStrategy : public PlayerbotRogueStrategy
{
public:
    PlayerbotOutlawRogueStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Outlaw-specific registration
    void RegisterOutlawActions();
    void RegisterOutlawTriggers();

    // Roll the Bones management
    bool ShouldRollTheBones() const;
    bool ShouldRerollBones() const;
    bool HasGoodRollTheBonesBuffs() const;
    uint32 CountRollTheBonesBuffs() const;
    bool HasRollTheBonesBuffs() const;
    
    // Rotation logic methods
    bool ShouldUseSinisterStrike() const;
    bool ShouldUseDispatch() const;
    bool ShouldUsePistolShot() const;
    bool ShouldUseBetweenTheEyes() const;
    bool ShouldUseAdrenalineRush() const;
    bool ShouldUseBladeFlurry() const;
    bool ShouldUseKillingSpree() const;
    
    // State evaluation
    bool HasOpportunityProc() const;
    bool IsInAdrenalineRush() const;
    bool ShouldUseBurstCooldowns() const;
    bool IsTargetInExecuteRange(Unit* target) const;
    
    // AoE evaluation
    bool ShouldUseAoERotation() const;
    bool HasBladeFlurry() const;
    
private:
    static constexpr uint32 MIN_ROLL_BONES_BUFFS = 2;    // Minimum buffs before rerolling
    static constexpr uint32 BTE_MIN_CP = 5;              // Use Between the Eyes with 5 CP
    static constexpr uint32 DISPATCH_MIN_CP = 5;         // Use Dispatch with 5 CP
    static constexpr float EXECUTE_THRESHOLD = 0.35f;    // Execute threshold for abilities
    static constexpr uint32 AOE_TARGET_COUNT = 2;        // Minimum targets for AoE abilities
};

/**
 * @brief Subtlety Rogue strategy - The War Within 11.2
 * 
 * Implements DPS rotation focused on Shadow Dance burst windows:
 * - Generate combo points with Backstab or Shadowstrike (in stealth)
 * - Maintain Rupture DoT on target
 * - Use Shadow Dance for burst windows with enhanced abilities
 * - Symbols of Death for damage buff and energy regeneration
 * - Eviscerate as primary finisher, Black Powder for AoE
 */
class TC_GAME_API PlayerbotSubtletyRogueStrategy : public PlayerbotRogueStrategy
{
public:
    PlayerbotSubtletyRogueStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Subtlety-specific registration
    void RegisterSubtletyActions();
    void RegisterSubtletyTriggers();

    // Shadow Dance and stealth management
    bool ShouldUseShadowDance() const;
    bool ShouldUseSymbolsOfDeath() const;
    bool IsInShadowDance() const;
    bool ShouldUseStealth() const;
    bool ShouldPrepareBurstWindow() const;
    
    // Rotation logic methods
    bool ShouldUseBackstab() const;
    bool ShouldUseShadowstrike() const;
    bool ShouldUseEviscerate() const;
    bool ShouldUseShadowClone() const;
    bool ShouldUseShurikenStorm() const;
    bool ShouldUseBlackPowder() const;
    bool ShouldMaintainRupture() const;
    
    // State evaluation
    bool IsInBurstWindow() const;
    bool HasSymbolsOfDeath() const;
    bool HasShadowTechniques() const;
    bool ShouldEnterBurstMode() const;
    bool HasRuptureOnTarget(Unit* target) const;
    bool ShouldRefreshRupture(Unit* target) const;
    
    // Positioning and utility
    bool ShouldUseShadowstep() const;
    bool IsBehindTarget(Unit* target) const;
    
private:
    static constexpr float RUPTURE_REFRESH_THRESHOLD = 7.2f; // Refresh Rupture with <7.2s remaining
    static constexpr uint32 EVISCERATE_MIN_CP = 4;          // Use Eviscerate with 4+ combo points
    static constexpr uint32 BLACK_POWDER_MIN_CP = 4;        // Use Black Powder with 4+ combo points
    static constexpr uint32 RUPTURE_MIN_CP = 4;             // Use Rupture with 4+ combo points
    static constexpr float SYMBOLS_DURATION = 10.0f;        // Symbols of Death duration
    static constexpr uint32 AOE_TARGET_COUNT = 3;           // Minimum targets for AoE abilities
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTROGUESTRATEGY_H