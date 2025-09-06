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

#ifndef TRINITY_PLAYERBOTMONK_STRATEGY_H
#define TRINITY_PLAYERBOTMONK_STRATEGY_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"

/**
 * @brief Base class for all Monk strategies
 * 
 * Provides common functionality shared between Brewmaster, Mistweaver, and Windwalker specs.
 * Manages energy/chi resources and shared utility like mobility and crowd control.
 */
class TC_GAME_API PlayerbotMonkStrategy : public PlayerbotStrategy
{
public:
    PlayerbotMonkStrategy(PlayerbotPlayerAI* ai, std::string const& name);

    void InitializeActions() override;
    void InitializeTriggers() override;

    // Common strategy methods
    virtual float GetActionPriority(std::string const& actionName) const;

protected:
    // Common action and trigger registration
    void RegisterCommonActions();
    void RegisterCommonTriggers();

    // Common utility methods
    bool ShouldUseParalysis() const;
    bool ShouldUseLegSweep() const;
    bool ShouldUseTranscendence() const;
    bool ShouldUseRoll() const;
    bool ShouldUseSpearHandStrike() const;
    bool ShouldUseFortifyingBrew() const;
    bool ShouldUseDetox() const;
    bool ShouldUseProvoke() const;
    
    // Resource management
    uint32 GetEnergyAmount() const;
    uint32 GetEnergyPercent() const;
    uint32 GetChi() const;
    bool ShouldConserveEnergy() const;
    bool HasEnoughEnergy(uint32 cost) const;
    bool HasEnoughChi(uint32 cost) const;
    
    // Specialization detection
    bool isBrewmaster() const;
    bool isMistweaver() const;
    bool isWindwalker() const;
    
    // Target evaluation
    Unit* FindBestTarget() const;
    Unit* FindBestHealingTarget() const;
    bool IsInCombat() const;
    uint32 CountNearbyEnemies(float range = 8.0f) const;
    uint32 CountInjuredGroupMembers(float threshold = 0.8f) const;
    bool ShouldUseAoE() const;
    bool ShouldUseAoEHealing() const;
    
    // Buff and debuff management
    bool HasShuffle() const;
    bool HasStagger() const;
    float GetStaggerDamage() const;
    bool HasTeachingsOfTheMonastery() const;
    uint32 GetTeachingsStacks() const;
    bool HasDanceOfChiJi() const;
    bool HasComboBreaker() const;
    bool HasMarkOfTheCrane(Unit* target) const;
    uint32 GetMarkOfTheCraneCount() const;
    bool HasHitCombo() const;
    bool IsInMeleeRange(Unit* target) const;
};

/**
 * @brief Brewmaster Monk strategy - The War Within 11.2
 * 
 * Implements tanking rotation focused on stagger management and active mitigation:
 * - Maintain threat with Keg Smash and Tiger Palm
 * - Use Breath of Fire for AoE threat and debuff
 * - Manage brews (Ironskin and Purifying) for damage mitigation
 * - Monitor stagger levels and purify heavy stagger
 * - Use Blackout Kick for Shuffle buff maintenance
 * - Major cooldowns: Celestial Brew, Fortifying Brew, Invoke Niuzao
 */
class TC_GAME_API PlayerbotBrewmasterMonkStrategy : public PlayerbotMonkStrategy
{
public:
    PlayerbotBrewmasterMonkStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Brewmaster-specific registration
    void RegisterBrewmasterActions();
    void RegisterBrewmasterTriggers();

    // Rotation logic methods
    bool ShouldUseKegSmash() const;
    bool ShouldUseTigerPalm() const;
    bool ShouldUseBreathOfFire() const;
    bool ShouldUseBlackoutKick() const;
    bool ShouldUseSpinningCraneKick() const;
    bool ShouldUseIronskinBrew() const;
    bool ShouldUsePurifyingBrew() const;
    bool ShouldUseCelestialBrew() const;
    bool ShouldUseInvokeNiuzao() const;
    bool ShouldUseBlackOxBrew() const;
    bool ShouldUseExpelHarm() const;
    
    // Threat and mitigation evaluation
    bool ShouldPrioritizeThreat() const;
    bool HasHighStagger() const;
    bool ShouldPurifyStagger() const;
    bool ShouldUseEmergencyMitigation() const;
    bool NeedsShuffle() const;
    bool ShouldUseAoEThreat() const;
    
    // Brew management
    bool HasIronskinBrewCharges() const;
    bool HasPurifyingBrewCharges() const;
    uint32 GetBrewCharges() const;
    bool ShouldConserveBrews() const;
    
    // Stagger management
    float GetStaggerPercent() const;
    bool IsStaggerHeavy() const;
    bool IsStaggerModerate() const;
    uint32 GetStaggerTicksRemaining() const;
    
private:
    static constexpr float HEAVY_STAGGER_THRESHOLD = 0.6f;      // Purify at >60% of max health in stagger
    static constexpr float MODERATE_STAGGER_THRESHOLD = 0.4f;   // Consider purifying at >40%
    static constexpr float LOW_HEALTH_THRESHOLD = 0.4f;        // Emergency mitigation below 40% HP
    static constexpr uint32 AOE_THREAT_COUNT = 3;              // Use AoE abilities with 3+ enemies
    static constexpr float SHUFFLE_REFRESH_THRESHOLD = 4.0f;   // Refresh Shuffle with <4s remaining
    static constexpr uint32 MIN_BREW_CHARGES = 1;             // Keep at least 1 charge for emergencies
};

/**
 * @brief Mistweaver Monk strategy - The War Within 11.2
 * 
 * Implements healing rotation focused on mana efficiency and HoT management:
 * - Maintain Renewing Mist on injured targets
 * - Use Essence Font for AoE healing and buff application
 * - Vivify as primary single-target heal
 * - Enveloping Mist for heavy single-target healing
 * - Thunder Focus Tea for enhanced abilities
 * - Major cooldowns: Revival, Invoke Yu'lon, Sheilun's Gift
 * - Fistweaving option for damage while healing
 */
class TC_GAME_API PlayerbotMistweaverMonkStrategy : public PlayerbotMonkStrategy
{
public:
    PlayerbotMistweaverMonkStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Mistweaver-specific registration
    void RegisterMistweaverActions();
    void RegisterMistweaverTriggers();

    // Rotation logic methods
    bool ShouldUseRenewingMist() const;
    bool ShouldUseEssenceFont() const;
    bool ShouldUseVivify() const;
    bool ShouldUseEnvelopingMist() const;
    bool ShouldUseThunderFocusTea() const;
    bool ShouldUseRevival() const;
    bool ShouldUseInvokeYulon() const;
    bool ShouldUseSheilunsGift() const;
    bool ShouldUseSoothingMist() const;
    bool ShouldUseLifeCocoon() const;
    bool ShouldUseManaTea() const;
    
    // Fistweaving abilities
    bool ShouldUseRisingSunKick() const;
    bool ShouldUseBlackoutKickMW() const;
    bool ShouldUseTigerPalmMW() const;
    bool ShouldUseSpinningCraneKickMW() const;
    
    // Healing evaluation
    Unit* FindCriticalHealingTarget() const;
    Unit* FindBestRenewingMistTarget() const;
    Unit* FindBestEnvelopingMistTarget() const;
    bool ShouldUseAoEHealing() const;
    bool ShouldUseEmergencyHealing() const;
    bool ShouldUseFistweaving() const;
    
    // Resource management
    uint32 GetManaAmount() const;
    uint32 GetManaPercent() const;
    bool ShouldConserveMana() const;
    bool HasEnoughMana(uint32 cost) const;
    
    // HoT and buff management
    bool HasRenewingMist(Unit* target) const;
    bool HasEnvelopingMist(Unit* target) const;
    bool HasEssenceFontBuff(Unit* target) const;
    uint32 GetActiveRenewingMistCount() const;
    bool ShouldRefreshRenewingMist(Unit* target) const;
    
    // Thunder Focus Tea optimization
    bool HasThunderFocusTea() const;
    bool ShouldUseThunderFocusTeaOnVivify() const;
    bool ShouldUseThunderFocusTeaOnEnvelopingMist() const;
    bool ShouldUseThunderFocusTeaOnRenewingMist() const;
    
private:
    static constexpr float CRITICAL_HEALTH_THRESHOLD = 0.3f;    // Emergency healing below 30%
    static constexpr float LOW_HEALTH_THRESHOLD = 0.6f;        // Normal healing below 60%
    static constexpr float AOE_HEALING_THRESHOLD = 0.7f;       // AoE healing when 3+ below 70%
    static constexpr uint32 AOE_HEALING_COUNT = 3;             // Minimum injured for AoE healing
    static constexpr uint32 LOW_MANA_THRESHOLD = 25;           // Conserve mana below 25%
    static constexpr float HOT_REFRESH_THRESHOLD = 4.0f;       // Refresh HoTs with <4s remaining
    static constexpr uint32 MAX_RENEWING_MIST_TARGETS = 6;     // Maximum RM targets to maintain
    static constexpr uint32 FISTWEAVING_MANA_THRESHOLD = 60;   // Fistweave above 60% mana
};

/**
 * @brief Windwalker Monk strategy - The War Within 11.2
 * 
 * Implements DPS rotation focused on mastery and resource management:
 * - Build and spend Chi efficiently
 * - Maintain Hit Combo by not repeating abilities
 * - Use Rising Sun Kick for debuff and damage
 * - Blackout Kick for consistent damage and combo maintenance  
 * - Fists of Fury as primary Chi spender
 * - Mark of the Crane management for Spinning Crane Kick
 * - Major cooldowns: Touch of Death, Invoke Xuen, Storm, Earth, and Fire
 */
class TC_GAME_API PlayerbotWindwalkerMonkStrategy : public PlayerbotMonkStrategy
{
public:
    PlayerbotWindwalkerMonkStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Windwalker-specific registration
    void RegisterWindwalkerActions();
    void RegisterWindwalkerTriggers();

    // Rotation logic methods
    bool ShouldUseTigerPalmWW() const;
    bool ShouldUseRisingSunKickWW() const;
    bool ShouldUseBlackoutKickWW() const;
    bool ShouldUseFistsOfFury() const;
    bool ShouldUseWhirlingDragonPunch() const;
    bool ShouldUseSpinningCraneKickWW() const;
    bool ShouldUseFlyingSerpentKick() const;
    bool ShouldUseStrikeOfTheWindlord() const;
    bool ShouldUseTouchOfDeath() const;
    bool ShouldUseInvokeXuen() const;
    bool ShouldUseStormEarthAndFire() const;
    bool ShouldUseSerenity() const;
    
    // Resource and combo management
    bool ShouldBuildChi() const;
    bool ShouldSpendChi() const;
    bool HasOptimalChiCount() const;
    uint32 GetLastUsedAbility() const;
    bool CanMaintainHitCombo(std::string const& actionName) const;
    bool ShouldPrioritizeHitCombo() const;
    
    // State evaluation
    bool ShouldUseAoERotation() const;
    bool ShouldUseBurstCooldowns() const;
    bool IsInBurstWindow() const;
    bool HasTeachingsOfTheMonasteryStacks() const;
    uint32 GetComboStrikeBonus() const;
    
    // Mark of the Crane management
    bool ShouldApplyMarkOfTheCrane() const;
    uint32 GetOptimalMarkTargets() const;
    bool ShouldUseSpinningCraneKickForMarks() const;
    Unit* FindBestMarkTarget() const;
    
    // Cooldown optimization
    bool ShouldUseMajorCooldowns() const;
    bool IsTargetSuitableForTouchOfDeath() const;
    bool ShouldSetupStormEarthAndFire() const;
    float GetSerenityValue() const;
    float GetStormEarthAndFireValue() const;
    
    // Position and mobility
    bool ShouldUseMobilityAbilities() const;
    bool IsOptimallyPositioned() const;
    bool ShouldUseFlyingSerpentKickForGap() const;
    
private:
    static constexpr uint32 OPTIMAL_CHI_COUNT = 4;             // Optimal Chi before spending
    static constexpr uint32 MAX_CHI_COUNT = 6;                 // Maximum Chi capacity
    static constexpr uint32 AOE_ROTATION_COUNT = 3;            // Switch to AoE at 3+ targets
    static constexpr uint32 MARK_COUNT_FOR_SCK = 3;           // Use SCK with 3+ marked targets
    static constexpr float BURST_COOLDOWN_HEALTH = 0.7f;      // Use burst when target >70% HP
    static constexpr float TOUCH_OF_DEATH_THRESHOLD = 0.15f;   // ToD when target <15% HP
    static constexpr uint32 HIT_COMBO_PRIORITY = 8;           // High priority for Hit Combo maintenance
    static constexpr float MOBILITY_RANGE = 15.0f;            // Use mobility beyond 15 yards
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTMONK_STRATEGY_H