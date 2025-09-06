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

#ifndef TRINITY_PLAYERBOTWARLOCK_STRATEGY_H
#define TRINITY_PLAYERBOTWARLOCK_STRATEGY_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"

/**
 * @brief Base class for all Warlock strategies
 * 
 * Provides common functionality shared between Affliction, Demonology, and Destruction specs.
 * Manages mana/soul shard resources and shared utility like fear and pet management.
 */
class TC_GAME_API PlayerbotWarlockStrategy : public PlayerbotStrategy
{
public:
    PlayerbotWarlockStrategy(PlayerbotPlayerAI* ai, std::string const& name);

    void InitializeActions() override;
    void InitializeTriggers() override;

    // Common strategy methods
    virtual float GetActionPriority(std::string const& actionName) const;

protected:
    // Common action and trigger registration
    void RegisterCommonActions();
    void RegisterCommonTriggers();

    // Common utility methods
    bool ShouldUseFear() const;
    bool ShouldUseBanish() const;
    bool ShouldUseUnendingResolve() const;
    bool ShouldUseDarkPact() const;
    bool ShouldSummonPet() const;
    bool NeedsHealthstone() const;
    bool ShouldCreateHealthstone() const;
    
    // Resource management
    uint32 GetManaAmount() const;
    uint32 GetManaPercent() const;
    uint32 GetSoulShards() const;
    bool ShouldConserveMana() const;
    bool HasEnoughSoulShards(uint32 cost) const;
    
    // Specialization detection
    bool isAffliction() const;
    bool isDemonology() const;
    bool isDestruction() const;
    
    // Target evaluation
    Unit* FindBestTarget() const;
    bool IsInCombat() const;
    uint32 CountNearbyEnemies(float range = 8.0f) const;
    bool ShouldUseAoE() const;
    
    // Pet management
    bool HasActivePet() const;
    Unit* GetPet() const;
    bool IsPetInCombat() const;
    bool ShouldDismissPet() const;
    
    // DoT management
    bool HasCorruption(Unit* target) const;
    bool HasAgony(Unit* target) const;
    bool HasImmolate(Unit* target) const;
    bool ShouldRefreshDoT(Unit* target, uint32 spellId, float threshold = 4.0f) const;
    float GetDoTDuration(Unit* target, uint32 spellId) const;
};

/**
 * @brief Affliction Warlock strategy - The War Within 11.2
 * 
 * Implements DoT-based DPS rotation focused on soul shard generation and spending:
 * - Maintain Corruption and Agony on all targets
 * - Generate soul shards with DoT ticks and drain effects
 * - Spend shards on Malefic Rapture for burst damage
 * - Use Unstable Affliction for additional DoT pressure
 * - Major cooldowns: Dark Soul, Summon Darkglare, Soul Rot
 * - AoE with Seed of Corruption and Vile Taint
 */
class TC_GAME_API PlayerbotAfflictionWarlockStrategy : public PlayerbotWarlockStrategy
{
public:
    PlayerbotAfflictionWarlockStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Affliction-specific registration
    void RegisterAfflictionActions();
    void RegisterAfflictionTriggers();

    // Rotation logic methods
    bool ShouldUseCorruption() const;
    bool ShouldUseAgony() const;
    bool ShouldUseUnstableAffliction() const;
    bool ShouldUseMaleficRapture() const;
    bool ShouldUseSeedOfCorruption() const;
    bool ShouldUseVileTaint() const;
    bool ShouldUsePhantomSingularity() const;
    bool ShouldUseDarkSoulMisery() const;
    bool ShouldUseSoulRot() const;
    bool ShouldUseSummonDarkglare() const;
    bool ShouldUseHaunt() const;
    bool ShouldUseDrainSoul() const;
    
    // State evaluation
    bool HasMaxDoTsOnTarget(Unit* target) const;
    uint32 GetActiveDoTCount() const;
    bool ShouldUseAoERotation() const;
    bool ShouldUseBurstCooldowns() const;
    bool ShouldPrioritizeDoTRefresh() const;
    bool ShouldSpendSoulShards() const;
    
    // DoT management
    bool ShouldRefreshCorruption(Unit* target) const;
    bool ShouldRefreshAgony(Unit* target) const;
    bool ShouldRefreshUnstableAffliction(Unit* target) const;
    Unit* FindBestDoTTarget() const;
    uint32 GetDoTStacksOnTarget(Unit* target, uint32 spellId) const;
    
    // Soul shard optimization
    bool ShouldConserveSoulShards() const;
    bool CanAffordSoulShardSpell(uint32 cost) const;
    bool IsAtMaxSoulShards() const;
    
private:
    static constexpr float DOT_REFRESH_THRESHOLD = 4.0f;       // Refresh DoTs with <4s remaining
    static constexpr uint32 MAX_SOUL_SHARDS = 5;              // Maximum soul shard capacity
    static constexpr uint32 MALEFIC_RAPTURE_THRESHOLD = 2;    // Use Malefic Rapture with 2+ shards
    static constexpr uint32 AOE_TARGET_COUNT = 3;             // Minimum targets for AoE rotation
    static constexpr float BURST_COOLDOWN_HEALTH = 0.8f;      // Use burst when target >80% HP
    static constexpr uint32 MIN_DOTS_FOR_RAPTURE = 2;         // Minimum DoTs on target for Rapture
};

/**
 * @brief Demonology Warlock strategy - The War Within 11.2
 * 
 * Implements pet-focused DPS rotation with Demonic Tyrant synergy:
 * - Build soul shards with Shadow Bolt/Demonbolt
 * - Spend shards on Hand of Gul'dan to summon Wild Imps
 * - Summon Dreadstalkers and Vilefiend for big demons
 * - Use Demonic Tyrant to extend all demon durations
 * - Manage Demonic Core procs for instant Demonbolts
 * - Major cooldowns: Nether Portal, Grimoire: Felguard, Bilescourge Bombers
 */
class TC_GAME_API PlayerbotDemonologyWarlockStrategy : public PlayerbotWarlockStrategy
{
public:
    PlayerbotDemonologyWarlockStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Demonology-specific registration
    void RegisterDemonologyActions();
    void RegisterDemonologyTriggers();

    // Rotation logic methods
    bool ShouldUseShadowBolt() const;
    bool ShouldUseDemonbolt() const;
    bool ShouldUseHandOfGuldan() const;
    bool ShouldUseCallDreadstalkers() const;
    bool ShouldUseSummonVilefiend() const;
    bool ShouldUseSummonDemonicTyrant() const;
    bool ShouldUseDemonicStrength() const;
    bool ShouldUseBilescourgeBombers() const;
    bool ShouldUsePowerSiphon() const;
    bool ShouldUseDoom() const;
    bool ShouldUseNetherPortal() const;
    bool ShouldUseGrimoireFelguard() const;
    
    // State evaluation
    bool HasDemonicCoreProc() const;
    uint32 GetActiveDemonCount() const;
    uint32 GetActiveWildImpCount() const;
    bool ShouldUseTyrantSetup() const;
    bool IsInTyrantWindow() const;
    bool ShouldUseBurstCooldowns() const;
    bool ShouldUseAoERotation() const;
    
    // Demon management
    bool HasBigDemonsActive() const;
    bool ShouldExtendDemons() const;
    float GetDemonicTyrantCooldownRemaining() const;
    bool CanSetupTyrant() const;
    
    // Resource optimization
    bool ShouldBuildSoulShards() const;
    bool ShouldSpendSoulShardsOnImps() const;
    bool HasOptimalShardCount() const;
    
private:
    static constexpr uint32 TYRANT_SETUP_SHARDS = 4;          // Shards needed for Tyrant setup
    static constexpr uint32 OPTIMAL_IMP_COUNT = 6;            // Optimal Wild Imp count for Tyrant
    static constexpr float TYRANT_WINDOW_DURATION = 15.0f;    // Tyrant buff duration
    static constexpr uint32 AOE_TARGET_COUNT = 3;             // Minimum targets for AoE rotation
    static constexpr float BURST_COOLDOWN_HEALTH = 0.7f;      // Use burst when target >70% HP
    static constexpr uint32 MAX_WILD_IMPS = 10;               // Maximum Wild Imps from Hand of Gul'dan
};

/**
 * @brief Destruction Warlock strategy - The War Within 11.2
 * 
 * Implements burst-focused DPS rotation with soul shard management:
 * - Generate soul shards with Incinerate and Conflagrate
 * - Maintain Immolate DoT on all targets
 * - Spend shards on Chaos Bolt for high damage
 * - Use Backdraft procs for faster Incinerate casts
 * - Rain of Fire and Havoc for AoE and cleave situations
 * - Major cooldowns: Dark Soul, Summon Infernal, Cataclysm
 */
class TC_GAME_API PlayerbotDestructionWarlockStrategy : public PlayerbotWarlockStrategy
{
public:
    PlayerbotDestructionWarlockStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Destruction-specific registration
    void RegisterDestructionActions();
    void RegisterDestructionTriggers();

    // Rotation logic methods
    bool ShouldUseIncinerate() const;
    bool ShouldUseConflagrate() const;
    bool ShouldUseImmolate() const;
    bool ShouldUseChaosBolt() const;
    bool ShouldUseRainOfFire() const;
    bool ShouldUseHavoc() const;
    bool ShouldUseCataclysm() const;
    bool ShouldUseChannelDemonfire() const;
    bool ShouldUseSoulFire() const;
    bool ShouldUseShadowburn() const;
    bool ShouldUseDarkSoulInstability() const;
    bool ShouldUseSummonInfernal() const;
    
    // State evaluation
    bool HasBackdraftProc() const;
    uint32 GetBackdraftStacks() const;
    bool HasEradication() const;
    bool ShouldUseBurstCooldowns() const;
    bool ShouldUseAoERotation() const;
    bool ShouldUseCleaveRotation() const;
    
    // DoT and debuff management
    bool ShouldRefreshImmolate(Unit* target) const;
    Unit* FindBestImmolateTarget() const;
    bool ShouldUseHavocTarget() const;
    Unit* FindBestHavocTarget() const;
    
    // Soul shard optimization
    bool ShouldSpendSoulShardsOnChaosBolt() const;
    bool ShouldConserveSoulShardsForBurst() const;
    bool IsAtOptimalShardCount() const;
    bool ShouldPrioritizeShardGeneration() const;
    
    // Cooldown and proc management
    bool ShouldUseBackdraftIncinerate() const;
    bool HasConflagrateCharges() const;
    uint32 GetConflagrateCharges() const;
    
private:
    static constexpr float IMMOLATE_REFRESH_THRESHOLD = 5.0f;  // Refresh Immolate with <5s remaining
    static constexpr uint32 CHAOS_BOLT_SHARD_COST = 2;        // Soul shard cost for Chaos Bolt
    static constexpr uint32 OPTIMAL_SHARD_COUNT = 3;          // Optimal shards for rotation
    static constexpr uint32 AOE_TARGET_COUNT = 3;             // Minimum targets for AoE rotation
    static constexpr uint32 CLEAVE_TARGET_COUNT = 2;          // Minimum targets for cleave rotation
    static constexpr float BURST_COOLDOWN_HEALTH = 0.6f;      // Use burst when target >60% HP
    static constexpr uint32 MAX_BACKDRAFT_STACKS = 2;         // Maximum Backdraft stacks
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTWARLOCK_STRATEGY_H