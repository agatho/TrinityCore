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

#ifndef TRINITY_PLAYERBOTDEMONHUNTER_STRATEGY_H
#define TRINITY_PLAYERBOTDEMONHUNTER_STRATEGY_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"

/**
 * @brief Base class for all Demon Hunter strategies
 * 
 * Provides common functionality shared between Havoc and Vengeance specs.
 * Manages Fury/Pain resources and shared utility like mobility and crowd control.
 */
class TC_GAME_API PlayerbotDemonHunterStrategy : public PlayerbotStrategy
{
public:
    PlayerbotDemonHunterStrategy(PlayerbotPlayerAI* ai, std::string const& name);

    void InitializeActions() override;
    void InitializeTriggers() override;

    // Common strategy methods
    virtual float GetActionPriority(std::string const& actionName) const;

protected:
    // Common action and trigger registration
    void RegisterCommonActions();
    void RegisterCommonTriggers();

    // Common utility methods
    bool ShouldUseConsumeMagic() const;
    bool ShouldUseDisrupt() const;
    bool ShouldUseThrowGlaive() const;
    bool ShouldUseImprison() const;
    bool ShouldUseTorment() const;
    bool ShouldUseBlur() const;
    bool ShouldUseDarkness() const;
    bool ShouldUseSpectralSight() const;
    
    // Mobility
    bool ShouldUseFelRush() const;
    bool ShouldUseVengefulRetreat() const;
    bool ShouldUseGlide() const;
    
    // Resource management
    uint32 GetFuryAmount() const;
    uint32 GetFuryPercent() const;
    uint32 GetPainAmount() const;
    uint32 GetPainPercent() const;
    bool HasEnoughFury(uint32 cost) const;
    bool HasEnoughPain(uint32 cost) const;
    bool ShouldConserveFury() const;
    bool ShouldConservePain() const;
    
    // Specialization detection
    bool isHavoc() const;
    bool isVengeance() const;
    
    // Target evaluation
    Unit* FindBestTarget() const;
    bool IsInCombat() const;
    uint32 CountNearbyEnemies(float range = 8.0f) const;
    uint32 CountNearbyAllies(float range = 30.0f) const;
    bool ShouldUseAoE() const;
    bool IsInMeleeRange(Unit* target) const;
    
    // Buff and debuff management
    bool HasMetamorphosis() const;
    bool HasDemonSpikes() const;
    bool HasImmolationAura() const;
    bool HasFieryBrand(Unit* target = nullptr) const;
    bool HasPrepared() const;
    bool HasMomentum() const;
    uint32 GetSoulFragments() const;
    bool ShouldConsumeSoulFragments() const;
    
    // Combat state evaluation
    bool ShouldUseBurstCooldowns() const;
    bool IsInDemonicWindow() const;
    bool NeedsThreat() const;
};

/**
 * @brief Havoc Demon Hunter strategy - The War Within 11.2
 * 
 * Implements DPS rotation focused on Fury generation and spending:
 * - Build Fury with Demon's Bite and abilities
 * - Spend Fury on Chaos Strike/Annihilation and Blade Dance/Death Sweep  
 * - Use Eye Beam on cooldown to trigger Demonic transformation
 * - Maintain momentum with mobility abilities
 * - Essence Break windows for burst damage
 * - Major cooldowns: Metamorphosis, The Hunt, Elysian Decree
 * - AoE with Blade Dance, Eye Beam, and Glaive Tempest
 */
class TC_GAME_API PlayerbotHavocDemonHunterStrategy : public PlayerbotDemonHunterStrategy
{
public:
    PlayerbotHavocDemonHunterStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Havoc-specific registration
    void RegisterHavocActions();
    void RegisterHavocTriggers();

    // Rotation logic methods
    bool ShouldUseDemonsBite() const;
    bool ShouldUseChaosStrike() const;
    bool ShouldUseAnnihilation() const;
    bool ShouldUseBladeDance() const;
    bool ShouldUseDeathSweep() const;
    bool ShouldUseEyeBeam() const;
    bool ShouldUseImmolationAura() const;
    bool ShouldUseMetamorphosis() const;
    bool ShouldUseEssenceBreak() const;
    bool ShouldUseTheHunt() const;
    bool ShouldUseGlaiveTempest() const;
    bool ShouldUseElysianDecree() const;
    bool ShouldUseFelBarrage() const;
    
    // Resource optimization
    bool ShouldBuildFury() const;
    bool ShouldSpendFury() const;
    bool HasOptimalFuryCount() const;
    bool IsAtMaxFury() const;
    
    // State evaluation
    bool ShouldUseAoERotation() const;
    bool ShouldUseSingleTargetRotation() const;
    bool IsInMetamorphosis() const;
    bool ShouldUseEssenceBreakWindow() const;
    bool ShouldUseMajorCooldowns() const;
    
    // Momentum and positioning
    bool ShouldGenerateMomentum() const;
    bool IsOptimallyPositioned() const;
    bool ShouldUseMobilityForDamage() const;
    bool ShouldUseMobilityForSurvival() const;
    
    // Buff management
    bool ShouldMaintainPrepared() const;
    bool ShouldActivateMomentum() const;
    bool HasDemonSoulBuff() const;
    bool HasTacticalAdvantage() const;
    
    // Priority evaluation
    bool ShouldPrioritizeChaosStrike() const;
    bool ShouldPrioritizeBladeDance() const;
    bool ShouldPrioritizeEyeBeam() const;
    bool ShouldPrioritizeEssenceBreak() const;
    
private:
    static constexpr uint32 OPTIMAL_FURY_COUNT = 80;           // Optimal Fury before spending
    static constexpr uint32 MAX_FURY_COUNT = 120;             // Maximum Fury capacity
    static constexpr uint32 AOE_ROTATION_COUNT = 3;           // Switch to AoE at 3+ targets
    static constexpr float BURST_COOLDOWN_HEALTH = 0.7f;      // Use burst when target >70% HP
    static constexpr float MOBILITY_RANGE = 15.0f;            // Use mobility beyond 15 yards
    static constexpr uint32 ESSENCE_BREAK_FURY = 30;          // Fury cost for Essence Break
    static constexpr uint32 EYE_BEAM_PRIORITY = 9;            // High priority for Eye Beam
    static constexpr float DEMONIC_WINDOW_DURATION = 8.0f;    // Duration of Demonic transformation
};

/**
 * @brief Vengeance Demon Hunter strategy - The War Within 11.2
 * 
 * Implements tank rotation focused on Pain generation and mitigation:
 * - Generate Pain with Shear and Fracture
 * - Consume Soul Fragments with Soul Cleave for healing
 * - Use Spirit Bomb for AoE damage and healing
 * - Maintain Demon Spikes for physical damage reduction
 * - Apply Fiery Brand for damage reduction debuff
 * - Use Sigils for AoE damage and crowd control
 * - Major cooldowns: Metamorphosis, Fel Devastation, Last Resort
 * - Emergency abilities: Soul Barrier, Darkness
 */
class TC_GAME_API PlayerbotVengeanceDemonHunterStrategy : public PlayerbotDemonHunterStrategy
{
public:
    PlayerbotVengeanceDemonHunterStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Vengeance-specific registration
    void RegisterVengeanceActions();
    void RegisterVengeanceTriggers();

    // Rotation logic methods
    bool ShouldUseShear() const;
    bool ShouldUseFracture() const;
    bool ShouldUseSoulCleave() const;
    bool ShouldUseSpiritBomb() const;
    bool ShouldUseDemonSpikes() const;
    bool ShouldUseImmolationAuraTank() const;
    bool ShouldUseInfernalStrike() const;
    bool ShouldUseSigilOfFlame() const;
    bool ShouldUseSigilOfSilence() const;
    bool ShouldUseSigilOfMisery() const;
    bool ShouldUseSigilOfChains() const;
    bool ShouldUseFieryBrand() const;
    bool ShouldUseMetamorphosisTank() const;
    bool ShouldUseFelDevastation() const;
    bool ShouldUseSoulBarrier() const;
    bool ShouldUseLastResort() const;
    bool ShouldUseBulkExtraction() const;
    bool ShouldUseTheHuntTank() const;
    bool ShouldUseElysianDecreeTank() const;
    
    // Threat management
    bool ShouldPrioritizeThreat() const;
    bool HasAggroOnTarget(Unit* target) const;
    bool ShouldUseAoEThreat() const;
    bool ShouldUseSingleTargetThreat() const;
    
    // Defensive management
    bool ShouldUseActiveMitigation() const;
    bool ShouldUseEmergencyMitigation() const;
    bool NeedsDemonSpikes() const;
    bool ShouldUseFieryBrandDefensively() const;
    bool ShouldUseSigils() const;
    
    // Soul Fragment management
    bool HasOptimalSoulFragments() const;
    bool ShouldPrioritizeSoulCleave() const;
    bool ShouldPrioritizeSpiritBomb() const;
    uint32 GetActiveSoulFragments() const;
    bool ShouldGenerateSoulFragments() const;
    
    // Pain optimization
    bool ShouldBuildPain() const;
    bool ShouldSpendPain() const;
    bool HasOptimalPainCount() const;
    bool IsAtMaxPain() const;
    
    // Positioning and mobility
    bool ShouldUseInfernalStrikeForPositioning() const;
    bool ShouldUseInfernalStrikeForThreat() const;
    bool IsOptimallyPositionedForTank() const;
    
    // Cooldown management
    bool ShouldUseMajorDefensiveCooldowns() const;
    bool ShouldUseMinorDefensiveCooldowns() const;
    bool IsInEmergencyMode() const;
    
private:
    static constexpr uint32 OPTIMAL_PAIN_COUNT = 60;           // Optimal Pain before spending
    static constexpr uint32 MAX_PAIN_COUNT = 100;             // Maximum Pain capacity
    static constexpr uint32 AOE_THREAT_COUNT = 3;             // Use AoE abilities with 3+ enemies
    static constexpr float LOW_HEALTH_THRESHOLD = 0.4f;       // Emergency mitigation below 40% HP
    static constexpr uint32 OPTIMAL_SOUL_FRAGMENTS = 4;       // Optimal fragments for Spirit Bomb
    static constexpr float FIERY_BRAND_DURATION = 8.0f;       // Duration of Fiery Brand debuff
    static constexpr uint32 MIN_PAIN_FOR_SPIRIT_BOMB = 40;    // Minimum Pain for Spirit Bomb
    static constexpr uint32 DEMON_SPIKES_CHARGES = 2;         // Maximum Demon Spikes charges
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTDEMONHUNTER_STRATEGY_H