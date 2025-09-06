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

#ifndef TRINITY_PLAYERBOTDEATHKNIGHTSTRATEGY_H
#define TRINITY_PLAYERBOTDEATHKNIGHTSTRATEGY_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"

/**
 * @brief Base class for all Death Knight strategies
 * 
 * Provides common functionality shared between Blood, Frost, and Unholy specs.
 * Manages rune/runic power resources and shared utility like Death Grip and interrupts.
 */
class TC_GAME_API PlayerbotDeathKnightStrategy : public PlayerbotStrategy
{
public:
    PlayerbotDeathKnightStrategy(PlayerbotPlayerAI* ai, std::string const& name);

    void InitializeActions() override;
    void InitializeTriggers() override;

    // Common strategy methods
    virtual float GetActionPriority(std::string const& actionName) const;

protected:
    // Common action and trigger registration
    void RegisterCommonActions();
    void RegisterCommonTriggers();

    // Common utility methods
    bool ShouldUseDeathGrip() const;
    bool ShouldUseDeathAndDecay() const;
    bool ShouldUseAntiMagicShell() const;
    bool ShouldUseIceboundFortitude() const;
    bool ShouldUseMindFreeze() const;
    bool ShouldUseDarkCommand() const;
    bool ShouldUseRaiseDead() const;
    bool NeedsHealing() const;
    
    // Resource management
    uint32 GetRuneCount() const;
    uint32 GetRunicPowerAmount() const;
    uint32 GetRunicPowerPercent() const;
    bool HasEnoughRunes(uint32 cost) const;
    bool HasEnoughRunicPower(uint32 cost) const;
    bool IsRunicPowerCapped() const;
    bool ShouldConserveRunes() const;
    
    // Specialization detection
    bool isBlood() const;
    bool isFrost() const;
    bool isUnholy() const;
    
    // Target evaluation
    Unit* FindBestTarget() const;
    Unit* FindBestTankTarget() const;
    bool IsInCombat() const;
    bool IsInMeleeRange(Unit* target) const;
    uint32 CountNearbyEnemies(float range = 10.0f) const;
    bool ShouldUseAoE() const;
    bool ShouldTaunt(Unit* target) const;
};

/**
 * @brief Blood Death Knight strategy - The War Within 11.2
 * 
 * Implements tanking rotation focused on threat generation and survivability:
 * - Build/maintain Bone Shield with Marrowrend (priority when <5 stacks)
 * - Heart Strike as primary threat generator and rune spender
 * - Blood Boil for AoE threat and to proc Crimson Scourge
 * - Death Strike for self-healing when health is low
 * - Use major cooldowns: Vampiric Blood, Dancing Rune Weapon, Tombstone
 */
class TC_GAME_API PlayerbotBloodDeathKnightStrategy : public PlayerbotDeathKnightStrategy
{
public:
    PlayerbotBloodDeathKnightStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Blood-specific registration
    void RegisterBloodActions();
    void RegisterBloodTriggers();

    // Rotation logic methods
    bool ShouldUseMarrowrend() const;
    bool ShouldUseHeartStrike() const;
    bool ShouldUseBloodBoil() const;
    bool ShouldUseDeathStrike() const;
    bool ShouldUseVampiricBlood() const;
    bool ShouldUseDancingRuneWeapon() const;
    bool ShouldUseTombstone() const;
    bool ShouldUseBonestorm() const;
    bool ShouldUseConsumption() const;
    
    // State evaluation
    bool HasBoneShield() const;
    uint32 GetBoneShieldStacks() const;
    bool NeedsBoneShieldRefresh() const;
    bool IsInDanger() const;
    bool HasCrimsonScourgeProc() const;
    bool ShouldUseMajorCooldowns() const;
    
    // Tanking evaluation
    bool NeedsThreat() const;
    bool ShouldPullWithDeathGrip() const;
    bool IsActiveTanking() const;
    
private:
    static constexpr uint32 MIN_BONE_SHIELD_STACKS = 5;  // Maintain at least 5 stacks
    static constexpr float DEATH_STRIKE_THRESHOLD = 0.6f; // Use Death Strike below 60%
    static constexpr float DANGER_THRESHOLD = 0.4f;      // Major cooldowns below 40%
    static constexpr uint32 BONESTORM_MIN_STACKS = 5;    // Min Bone Shield stacks for Bonestorm
};

/**
 * @brief Frost Death Knight strategy - The War Within 11.2
 * 
 * Implements DPS rotation focused on Killing Machine and Rime procs:
 * - Obliterate as primary rune spender (with Killing Machine priority)
 * - Frost Strike to spend Runic Power and generate Rime procs
 * - Howling Blast on Rime procs or for AoE
 * - Frostscythe over Obliterate for 2+ enemies
 * - Major cooldowns: Pillar of Frost, Breath of Sindragosa, Frostwyrm's Fury
 */
class TC_GAME_API PlayerbotFrostDeathKnightStrategy : public PlayerbotDeathKnightStrategy
{
public:
    PlayerbotFrostDeathKnightStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Frost-specific registration
    void RegisterFrostActions();
    void RegisterFrostTriggers();

    // Rotation logic methods
    bool ShouldUseObliterate() const;
    bool ShouldUseFrostStrike() const;
    bool ShouldUseHowlingBlast() const;
    bool ShouldUseGlacialAdvance() const;
    bool ShouldUseFrostscythe() const;
    bool ShouldUseRemorselessWinter() const;
    bool ShouldUsePillarOfFrost() const;
    bool ShouldUseEmpowerRuneWeapon() const;
    bool ShouldUseBreathOfSindragosa() const;
    bool ShouldUseFrostwyrmsFury() const;
    
    // State evaluation
    bool HasKillingMachineProc() const;
    bool HasRimeProc() const;
    bool IsInBreathOfSindragosa() const;
    bool ShouldUseBurstCooldowns() const;
    bool ShouldUseAoERotation() const;
    
    // Resource optimization
    bool ShouldSpendRunicPower() const;
    bool ShouldPrioritizeKillingMachine() const;
    
private:
    static constexpr uint32 FROST_STRIKE_RP_COST = 25;   // Runic Power cost for Frost Strike
    static constexpr uint32 GLACIAL_ADVANCE_RP_COST = 35; // Runic Power cost for Glacial Advance
    static constexpr uint32 BREATH_RP_COST = 60;         // Initial Runic Power cost for Breath
    static constexpr uint32 AOE_TARGET_COUNT = 2;        // Minimum targets for AoE abilities
    static constexpr float BURST_HEALTH_THRESHOLD = 0.5f; // Target health for burst cooldowns
};

/**
 * @brief Unholy Death Knight strategy - The War Within 11.2
 * 
 * Implements DPS rotation focused on disease management and Festering Wounds:
 * - Maintain Virulent Plague with Outbreak
 * - Apply Festering Wounds with Festering Strike (when target has ≤2)
 * - Burst wounds with Scourge Strike and Apocalypse
 * - Death Coil for Runic Power spending (prioritize Sudden Doom procs)
 * - Major cooldowns: Dark Transformation/Apocalypse, Army of the Dead
 */
class TC_GAME_API PlayerbotUnholyDeathKnightStrategy : public PlayerbotDeathKnightStrategy
{
public:
    PlayerbotUnholyDeathKnightStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Unholy-specific registration
    void RegisterUnholyActions();
    void RegisterUnholyTriggers();

    // Rotation logic methods
    bool ShouldUseOutbreak() const;
    bool ShouldUseFesteringStrike() const;
    bool ShouldUseScourgeStrike() const;
    bool ShouldUseDeathCoil() const;
    bool ShouldUseEpidemic() const;
    bool ShouldUseDarkTransformation() const;
    bool ShouldUseApocalypse() const;
    bool ShouldUseArmyOfTheDead() const;
    bool ShouldUseSoulReaper() const;
    bool ShouldUseDefile() const;
    bool ShouldUseClawingShadows() const;
    
    // State evaluation
    bool HasVirulentPlague(Unit* target) const;
    uint32 GetFesteringWoundStacks(Unit* target) const;
    bool HasFesteringWounds(Unit* target) const;
    bool HasSuddenDoomProc() const;
    bool HasRunicCorruption() const;
    bool IsDeathRotExpiring() const;
    bool ShouldBurstFesteringWounds(Unit* target) const;
    bool ShouldUseBurstCooldowns() const;
    bool ShouldUseAoERotation() const;
    
    // Disease and wound management
    bool NeedsDiseaseRefresh(Unit* target) const;
    bool ShouldApplyFesteringWounds(Unit* target) const;
    float GetVirulentPlagueRemainingDuration(Unit* target) const;
    
private:
    static constexpr float DISEASE_REFRESH_THRESHOLD = 5.4f; // Refresh disease with <5.4s remaining
    static constexpr uint32 MAX_FESTERING_WOUNDS = 6;        // Maximum Festering Wound stacks
    static constexpr uint32 MIN_FESTERING_FOR_BURST = 4;     // Minimum wounds for Apocalypse
    static constexpr uint32 DEATH_COIL_RP_THRESHOLD = 80;    // Use Death Coil with 80+ RP
    static constexpr uint32 EPIDEMIC_RP_COST = 30;           // Runic Power cost for Epidemic
    static constexpr float SOUL_REAPER_THRESHOLD = 0.35f;    // Use Soul Reaper below 35%
    static constexpr uint32 AOE_TARGET_COUNT = 2;            // Minimum targets for AoE abilities
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTDEATHKNIGHTSTRATEGY_H