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

#ifndef TRINITY_PLAYERBOTEVOKER_STRATEGY_H
#define TRINITY_PLAYERBOTEVOKER_STRATEGY_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"

/**
 * @brief Base class for all Evoker strategies
 * 
 * Provides common functionality shared between Devastation and Preservation specs.
 * Manages Essence resources and shared utility like empowered spells and mobility.
 */
class TC_GAME_API PlayerbotEvokerStrategy : public PlayerbotStrategy
{
public:
    PlayerbotEvokerStrategy(PlayerbotPlayerAI* ai, std::string const& name);

    void InitializeActions() override;
    void InitializeTriggers() override;

    // Common strategy methods
    virtual float GetActionPriority(std::string const& actionName) const;

protected:
    // Common action and trigger registration
    void RegisterCommonActions();
    void RegisterCommonTriggers();

    // Common utility methods
    bool ShouldUseQuell() const;
    bool ShouldUseTailSwipe() const;
    bool ShouldUseWingBuffet() const;
    bool ShouldUseExpunge() const;
    bool ShouldUseNaturalize() const;
    bool ShouldUseSleep() const;
    bool ShouldUseRescue() const;
    bool ShouldUseTimeSpiral() const;
    
    // Mobility
    bool ShouldUseHover() const;
    bool ShouldUseLandslide() const;
    bool ShouldUseDeepBreath() const;
    bool ShouldUseSoar() const;
    
    // Resource management
    uint32 GetEssenceAmount() const;
    uint32 GetEssencePercent() const;
    bool HasEnoughEssence(uint32 cost) const;
    bool ShouldConserveEssence() const;
    
    // Specialization detection
    bool isDevastation() const;
    bool isPreservation() const;
    bool isAugmentation() const;
    
    // Target evaluation
    Unit* FindBestTarget() const;
    bool IsInCombat() const;
    uint32 CountNearbyEnemies(float range = 25.0f) const;
    uint32 CountNearbyAllies(float range = 30.0f) const;
    uint32 CountInjuredAllies(float healthPercent = 80.0f, float range = 30.0f) const;
    bool ShouldUseAoE() const;
    bool IsInRange(Unit* target, float range = 25.0f) const;
    
    // Empowered spell management
    bool ShouldChargeEmpoweredSpell() const;
    bool HasTimeForEmpower() const;
    uint32 GetOptimalEmpowerLevel() const;
    bool IsChannelingEmpoweredSpell() const;
    
    // Buff and debuff management
    bool HasDragonrage() const;
    bool HasSnapfire() const;
    bool HasBurnout() const;
    bool HasChargedBlast() const;
    bool HasIridescenceRed() const;
    bool HasIridescenceBlue() const;
    bool HasEssenceBurst() const;
    bool HasLeapingFlames(Unit* target = nullptr) const;
    bool HasCallOfYsera() const;
    bool HasEmeraldTrance() const;
    
    // Combat state evaluation
    bool ShouldUseBurstCooldowns() const;
    bool IsInDragonrageWindow() const;
    bool NeedsHealing() const;
    bool IsInEmergency() const;
};

/**
 * @brief Devastation Evoker strategy - The War Within 11.2
 * 
 * Implements DPS rotation focused on Essence management and empowered spells:
 * - Build Essence with Azure Strike and Living Flame
 * - Spend Essence on Disintegrate and empowered spells
 * - Use Dragonrage for burst windows with empowered Fire Breath
 * - Maintain Leaping Flames and other DoTs
 * - Pyre for AoE situations
 * - Major cooldowns: Dragonrage, Tip the Scales, Time Skip
 * - Empowered spells: Fire Breath, Eternity Surge
 */
class TC_GAME_API PlayerbotDevastationEvokerStrategy : public PlayerbotEvokerStrategy
{
public:
    PlayerbotDevastationEvokerStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Devastation-specific registration
    void RegisterDevastationActions();
    void RegisterDevastationTriggers();

    // Rotation logic methods
    bool ShouldUseAzureStrike() const;
    bool ShouldUseLivingFlame() const;
    bool ShouldUseDisintegrate() const;
    bool ShouldUseFireBreath() const;
    bool ShouldUseEternitySurge() const;
    bool ShouldUsePyre() const;
    bool ShouldUseShattershard() const;
    bool ShouldUseDragonrage() const;
    bool ShouldUseTipTheScales() const;
    bool ShouldUseTimeSkip() const;
    bool ShouldUseSnapfire() const;
    bool ShouldUseFirestorm() const;
    bool ShouldUseEngulf() const;
    bool ShouldUseOnyxLegacy() const;
    
    // Resource optimization
    bool ShouldBuildEssence() const;
    bool ShouldSpendEssence() const;
    bool HasOptimalEssenceCount() const;
    bool IsAtMaxEssence() const;
    
    // State evaluation
    bool ShouldUseAoERotation() const;
    bool ShouldUseSingleTargetRotation() const;
    bool IsInDragonrageWindow() const;
    bool ShouldUseEmpoweredSpells() const;
    bool ShouldUseMajorCooldowns() const;
    
    // DoT and debuff management
    bool ShouldMaintainLivingFlame() const;
    bool ShouldApplyLeapingFlames() const;
    bool HasLivingFlameDoT(Unit* target = nullptr) const;
    bool ShouldRefreshDoTs() const;
    
    // Buff management
    bool ShouldMaintainSnapfire() const;
    bool ShouldActivateBurnout() const;
    bool HasEssenceBurstStacks() const;
    bool ShouldConsumeIridescence() const;
    
    // Priority evaluation
    bool ShouldPrioritizeDisintegrate() const;
    bool ShouldPrioritizeFireBreath() const;
    bool ShouldPrioritizeEternitySurge() const;
    bool ShouldPrioritizePyre() const;
    
    // Positioning and range
    bool IsOptimallyPositioned() const;
    bool ShouldRepositionForCleave() const;
    bool IsInOptimalRange() const;
    
private:
    static constexpr uint32 OPTIMAL_ESSENCE_COUNT = 4;            // Optimal Essence before spending
    static constexpr uint32 MAX_ESSENCE_COUNT = 5;               // Maximum Essence capacity
    static constexpr uint32 AOE_ROTATION_COUNT = 3;              // Switch to AoE at 3+ targets
    static constexpr float BURST_COOLDOWN_HEALTH = 0.8f;         // Use burst when target >80% HP
    static constexpr float OPTIMAL_RANGE = 25.0f;                // Optimal casting range
    static constexpr uint32 DRAGONRAGE_ESSENCE_THRESHOLD = 3;    // Minimum Essence for Dragonrage
    static constexpr uint32 EMPOWER_PRIORITY = 8;                // High priority for empowered spells
    static constexpr float DRAGONRAGE_DURATION = 18.0f;          // Duration of Dragonrage buff
    static constexpr float DOT_REFRESH_THRESHOLD = 0.3f;         // Refresh DoTs at 30% remaining
};

/**
 * @brief Preservation Evoker strategy - The War Within 11.2
 * 
 * Implements healing rotation focused on Essence efficiency and Echo mechanics:
 * - Build Essence with Emerald Blossom and Azure Strike
 * - Heal with Verdant Embrace and Echo effects
 * - Use empowered Dream Breath and Spiritbloom for AoE healing
 * - Maintain Lifebind and Temporal Anomaly
 * - Major cooldowns: Stasis, Rewind, Time Skip
 * - Emergency abilities: Life-Giver's Flame, Rescue
 * - Utility: Blessing of the Bronze, Zephyr
 */
class TC_GAME_API PlayerbotPreservationEvokerStrategy : public PlayerbotEvokerStrategy
{
public:
    PlayerbotPreservationEvokerStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Preservation-specific registration
    void RegisterPreservationActions();
    void RegisterPreservationTriggers();

    // Rotation logic methods
    bool ShouldUseEmeraldBlossom() const;
    bool ShouldUseVerdantEmbrace() const;
    bool ShouldUseDreamBreath() const;
    bool ShouldUseSpiritbloom() const;
    bool ShouldUseLifeGiversFlame() const;
    bool ShouldUseReversion() const;
    bool ShouldUseEcho() const;
    bool ShouldUseTemporalAnomaly() const;
    bool ShouldUseStasis() const;
    bool ShouldUseRewind() const;
    bool ShouldUseTimeSkipHealing() const;
    bool ShouldUseBlessingOfTheBronze() const;
    bool ShouldUseZephyr() const;
    bool ShouldUseChrono() const;
    bool ShouldUseFlowState() const;
    bool ShouldUseFieldOfDreams() const;
    
    // Healing target selection
    Unit* FindBestHealTarget() const;
    Unit* FindLowestHealthAlly() const;
    Unit* FindTankTarget() const;
    std::vector<Unit*> FindHealingTargets(uint32 maxTargets = 5) const;
    bool ShouldPrioritizeTank() const;
    
    // Resource optimization for healing
    bool ShouldBuildEssenceForHealing() const;
    bool ShouldSpendEssenceOnHealing() const;
    bool HasOptimalEssenceForHealing() const;
    bool ShouldConserveEssenceForEmergency() const;
    
    // State evaluation
    bool ShouldUseAoEHealing() const;
    bool ShouldUseSingleTargetHealing() const;
    bool IsInEmergencyHealingMode() const;
    bool ShouldUseEmpoweredHealing() const;
    bool ShouldUseMajorHealingCooldowns() const;
    
    // HoT and buff management
    bool ShouldMaintainReversion(Unit* target) const;
    bool ShouldApplyEcho(Unit* target) const;
    bool HasReversionHoT(Unit* target) const;
    bool HasEchoEffect(Unit* target) const;
    bool ShouldRefreshHoTs() const;
    
    // Emergency and utility
    bool IsPartyInDanger() const;
    bool ShouldUseEmergencyHealing() const;
    bool ShouldUseRescueUtility() const;
    bool ShouldUseTemporalUtility() const;
    
    // Priority evaluation
    bool ShouldPrioritizeVerdantEmbrace() const;
    bool ShouldPrioritizeDreamBreath() const;
    bool ShouldPrioritizeSpiritbloom() const;
    bool ShouldPrioritizeEmeraldBlossom() const;
    
    // Mana and resource management
    bool HasSufficientMana() const;
    bool ShouldUseInnervate() const;
    float GetPartyHealthPercentage() const;
    
private:
    static constexpr uint32 OPTIMAL_ESSENCE_FOR_HEALING = 3;     // Optimal Essence for healing
    static constexpr uint32 EMERGENCY_ESSENCE_RESERVE = 2;       // Reserve Essence for emergencies
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 0.3f;    // Emergency healing below 30% HP
    static constexpr float AOE_HEALING_THRESHOLD = 0.7f;         // AoE heal when party <70% avg HP
    static constexpr float TANK_PRIORITY_THRESHOLD = 0.6f;       // Prioritize tank below 60% HP
    static constexpr uint32 AOE_HEALING_COUNT = 3;               // Use AoE healing for 3+ injured
    static constexpr float EMPOWERED_HEALING_THRESHOLD = 0.5f;   // Use empowered heals <50% HP
    static constexpr uint32 ECHO_PRIORITY = 7;                   // High priority for Echo effects
    static constexpr float REVERSION_DURATION = 12.0f;           // Duration of Reversion HoT
    static constexpr float HoT_REFRESH_THRESHOLD = 0.3f;         // Refresh HoTs at 30% remaining
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTEVOKER_STRATEGY_H