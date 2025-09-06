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

#ifndef TRINITY_PLAYERBOTMAGESTRATEGY_H
#define TRINITY_PLAYERBOTMAGESTRATEGY_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"

/**
 * @brief Base class for all Mage strategies
 * 
 * Provides common functionality shared between Arcane, Fire, and Frost specs.
 * Manages mana resources and shared utility like counterspells and blink.
 */
class TC_GAME_API PlayerbotMageStrategy : public PlayerbotStrategy
{
public:
    PlayerbotMageStrategy(PlayerbotPlayerAI* ai, std::string const& name);

    void InitializeActions() override;
    void InitializeTriggers() override;

    // Common strategy methods
    virtual float GetActionPriority(std::string const& actionName) const;

protected:
    // Common action and trigger registration
    void RegisterCommonActions();
    void RegisterCommonTriggers();

    // Common utility methods
    bool ShouldUseBlink() const;
    bool ShouldUseCounterspell() const;
    bool ShouldUseMirrorImage() const;
    bool ShouldUseIceBlock() const;
    bool ShouldUseInvisibility() const;
    bool ShouldUseTimeWarp() const;
    bool NeedsArcaneIntellect() const;
    bool NeedsMageArmor() const;
    
    // Resource management
    uint32 GetManaAmount() const;
    uint32 GetManaPercent() const;
    bool ShouldConserveMana() const;
    bool ShouldEvocate() const;
    
    // Specialization detection
    bool isArcane() const;
    bool isFire() const;
    bool isFrost() const;
    
    // Target evaluation
    Unit* FindBestTarget() const;
    bool IsInCombat() const;
    uint32 CountNearbyEnemies(float range = 8.0f) const;
    bool ShouldUseAoE() const;
    bool IsTargetInRange(Unit* target, float range) const;
    
    // Mage specific mechanics
    bool HasTimeWarpActive() const;
    bool IsTargetCastingInterruptible(Unit* target) const;
    bool ShouldDispelMagic(Unit* target) const;
    bool ShouldRemoveCurse(Unit* target) const;
};

/**
 * @brief Arcane Mage strategy - The War Within 11.2
 * 
 * Implements DPS rotation focused on Arcane Charge management and mana efficiency:
 * - Build Arcane Charges with Arcane Blast (up to 4 stacks)
 * - Spend charges with Arcane Barrage or Arcane Missiles
 * - Use Arcane Orb for AoE and charge generation
 * - Major cooldowns: Arcane Power, Touch of the Magi, Time Warp
 * - Mana management with Evocation and careful charge spending
 */
class TC_GAME_API PlayerbotArcaneMageStrategy : public PlayerbotMageStrategy
{
public:
    PlayerbotArcaneMageStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Arcane-specific registration
    void RegisterArcaneActions();
    void RegisterArcaneTriggers();

    // Rotation logic methods
    bool ShouldUseArcaneBlast() const;
    bool ShouldUseArcaneBarrage() const;
    bool ShouldUseArcaneMissiles() const;
    bool ShouldUseArcaneOrb() const;
    bool ShouldUseArcaneExplosion() const;
    bool ShouldUsePresenceOfMind() const;
    bool ShouldUseArcanePower() const;
    bool ShouldUseTouchOfTheMagi() const;
    bool ShouldUseSupernova() const;
    
    // State evaluation
    uint32 GetArcaneCharges() const;
    bool HasMaxArcaneCharges() const;
    bool ShouldBuildCharges() const;
    bool ShouldSpendCharges() const;
    bool IsInBurnPhase() const;
    bool IsInConservePhase() const;
    bool ShouldUseBurstCooldowns() const;
    bool IsInAoESituation() const;
    
    // Resource optimization
    bool ShouldUseManaEfficiently() const;
    bool CanAffordArcaneBlast() const;
    float GetOptimalChargeCount() const;
    
private:
    static constexpr uint32 MAX_ARCANE_CHARGES = 4;            // Maximum Arcane Charge stacks
    static constexpr uint32 OPTIMAL_CHARGE_THRESHOLD = 3;      // Optimal charges before spending
    static constexpr float MANA_BURN_THRESHOLD = 0.7f;        // Start burn phase above 70% mana
    static constexpr float MANA_CONSERVE_THRESHOLD = 0.3f;     // Enter conserve phase below 30% mana
    static constexpr uint32 AOE_TARGET_COUNT = 3;              // Minimum targets for AoE rotation
    static constexpr float BURST_COOLDOWN_HEALTH = 0.8f;       // Use burst when target >80% HP
};

/**
 * @brief Fire Mage strategy - The War Within 11.2
 * 
 * Implements DPS rotation focused on Hot Streak procs and Ignite management:
 * - Generate Heating Up and Hot Streak with critical strikes
 * - Use instant Pyroblast on Hot Streak procs
 * - Maintain Ignite DoT with Fire Blast and Phoenix Flames
 * - Use Combustion for burst windows with guaranteed crits
 * - Major cooldowns: Combustion, Meteor, Time Warp
 */
class TC_GAME_API PlayerbotFireMageStrategy : public PlayerbotMageStrategy
{
public:
    PlayerbotFireMageStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Fire-specific registration
    void RegisterFireActions();
    void RegisterFireTriggers();

    // Rotation logic methods
    bool ShouldUseFireball() const;
    bool ShouldUsePyroblast() const;
    bool ShouldUseFireBlast() const;
    bool ShouldUsePhoenixFlames() const;
    bool ShouldUseScorch() const;
    bool ShouldUseFlamestrike() const;
    bool ShouldUseCombustion() const;
    bool ShouldUseMeteor() const;
    bool ShouldUseDragonBreath() const;
    bool ShouldUseLivingBomb() const;
    
    // State evaluation
    bool HasHotStreak() const;
    bool HasHeatingUp() const;
    bool IsInCombustion() const;
    bool ShouldUseBurstCooldowns() const;
    bool ShouldUseAoERotation() const;
    bool HasIgniteOnTarget(Unit* target) const;
    bool ShouldRefreshIgnite(Unit* target) const;
    
    // Proc management
    bool ShouldUseInstantPyroblast() const;
    bool ShouldGenerateHotStreak() const;
    uint32 GetPhoenixFlamesCharges() const;
    bool HasPhoenixFlamesCharges() const;
    
    // Target evaluation
    bool ShouldPrioritizeIgniteTarget() const;
    Unit* FindBestIgniteTarget() const;
    
private:
    static constexpr float IGNITE_REFRESH_THRESHOLD = 4.0f;    // Refresh Ignite with <4s remaining
    static constexpr uint32 AOE_TARGET_COUNT = 3;             // Minimum targets for AoE rotation
    static constexpr float COMBUSTION_DURATION = 12.0f;       // Combustion duration
    static constexpr float BURST_COOLDOWN_HEALTH = 0.6f;      // Use burst when target >60% HP
    static constexpr uint32 MAX_PHOENIX_FLAMES_CHARGES = 3;   // Maximum Phoenix Flames charges
};

/**
 * @brief Frost Mage strategy - The War Within 11.2
 * 
 * Implements DPS rotation focused on Shatter combos and proc management:
 * - Use Frostbolt as main filler spell
 * - Shatter combos with Flurry -> Ice Lance on Winter's Chill
 * - Use Brain Freeze procs for instant Flurry casts
 * - Use Fingers of Frost procs for Ice Lance without setup
 * - Major cooldowns: Frozen Orb, Comet Storm, Ray of Frost
 */
class TC_GAME_API PlayerbotFrostMageStrategy : public PlayerbotMageStrategy
{
public:
    PlayerbotFrostMageStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Frost-specific registration
    void RegisterFrostActions();
    void RegisterFrostTriggers();

    // Rotation logic methods
    bool ShouldUseFrostbolt() const;
    bool ShouldUseIceLance() const;
    bool ShouldUseFlurry() const;
    bool ShouldUseFrozenOrb() const;
    bool ShouldUseCometStorm() const;
    bool ShouldUseGlacialSpike() const;
    bool ShouldUseRayOfFrost() const;
    bool ShouldUseBlizzard() const;
    bool ShouldUseFrostNova() const;
    bool ShouldUseConeOfCold() const;
    
    // State evaluation
    bool HasBrainFreeze() const;
    bool HasFingersOfFrost() const;
    bool HasWintersChillOnTarget(Unit* target) const;
    bool IsTargetFrozen(Unit* target) const;
    bool ShouldUseShatterCombo() const;
    bool ShouldUseBurstCooldowns() const;
    bool ShouldUseAoERotation() const;
    
    // Proc optimization
    bool ShouldUseInstantFlurry() const;
    bool ShouldUseInstantIceLance() const;
    uint32 GetFingersOfFrostStacks() const;
    bool ShouldConserveFingersOfFrost() const;
    
    // Positioning and utility
    bool ShouldUseMovementAbility() const;
    bool NeedsToKite() const;
    bool ShouldUseCrowdControl() const;
    
private:
    static constexpr float WINTERS_CHILL_DURATION = 6.0f;      // Winter's Chill debuff duration
    static constexpr uint32 MAX_FINGERS_OF_FROST_STACKS = 2;   // Maximum Fingers of Frost stacks
    static constexpr uint32 AOE_TARGET_COUNT = 3;              // Minimum targets for AoE rotation
    static constexpr float BURST_COOLDOWN_HEALTH = 0.7f;       // Use burst when target >70% HP
    static constexpr float KITING_DISTANCE = 10.0f;           // Distance to maintain when kiting
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTMAGESTRATEGY_H