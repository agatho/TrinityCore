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

#ifndef TRINITY_PLAYERBOTSHAMAN_STRATEGY_H
#define TRINITY_PLAYERBOTSHAMAN_STRATEGY_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"

/**
 * @brief Base class for all Shaman strategies
 * 
 * Provides common functionality shared between Elemental, Enhancement, and Restoration specs.
 * Manages mana/Maelstrom resources and shared utility like totems and interrupts.
 */
class TC_GAME_API PlayerbotShamanStrategy : public PlayerbotStrategy
{
public:
    PlayerbotShamanStrategy(PlayerbotPlayerAI* ai, std::string const& name);

    void InitializeActions() override;
    void InitializeTriggers() override;

    // Common strategy methods
    virtual float GetActionPriority(std::string const& actionName) const;

protected:
    // Common action and trigger registration
    void RegisterCommonActions();
    void RegisterCommonTriggers();

    // Common utility methods
    bool ShouldUseGhostWolf() const;
    bool ShouldUseAstralShift() const;
    bool ShouldUseWindShear() const;
    bool ShouldUsePurge() const;
    bool ShouldUseCleanseSpirit() const;
    bool ShouldUseTremorTotem() const;
    bool ShouldUseWindfuryTotem() const;
    bool ShouldUseEarthElemental() const;
    bool ShouldUseFireElemental() const;
    bool NeedsHealing() const;
    
    // Resource management
    uint32 GetManaAmount() const;
    uint32 GetManaPercent() const;
    uint32 GetMaelstromPower() const;
    uint32 GetMaelstromWeaponStacks() const;
    bool HasMaelstromWeaponStacks(uint32 minStacks = 1) const;
    bool HasMaxMaelstromWeaponStacks() const;
    bool ShouldSpendMaelstromWeapon() const;
    bool ShouldSpendMaelstrom() const;
    
    // Specialization detection
    bool isElemental() const;
    bool isEnhancement() const;
    bool isRestoration() const;
    
    // Target evaluation
    Unit* FindBestTarget() const;
    Unit* FindBestHealingTarget() const;
    bool IsInCombat() const;
    bool IsInMeleeRange(Unit* target) const;
    uint32 CountNearbyEnemies(float range = 8.0f) const;
    uint32 CountInjuredGroupMembers(float threshold = 0.8f) const;
    bool ShouldUseAoE() const;
    bool ShouldUseAoEHealing() const;
    
    // Shaman specific mechanics
    bool HasFlameShock(Unit* target) const;
    bool ShouldRefreshFlameShock(Unit* target) const;
    bool HasLavaSurgeProc() const;
    bool HasTidalWaves() const;
    bool HasMasterOfElementsBuff() const;
    bool ShouldUseMasterOfElements() const;
};

/**
 * @brief Elemental Shaman strategy - The War Within 11.2
 * 
 * Implements ranged DPS rotation focused on Maelstrom and proc management:
 * - Maintain Flame Shock for Lava Burst and Lava Surge procs
 * - Generate Maelstrom with Lightning Bolt and spend with Earth Shock/Elemental Blast  
 * - Use Lava Burst on cooldown (instant with Flame Shock active)
 * - Chain Lightning and Earthquake for AoE situations
 * - Major cooldowns: Ascendance, Stormkeeper, Storm Elemental
 */
class TC_GAME_API PlayerbotElementalShamanStrategy : public PlayerbotShamanStrategy
{
public:
    PlayerbotElementalShamanStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Elemental-specific registration
    void RegisterElementalActions();
    void RegisterElementalTriggers();

    // Rotation logic methods
    bool ShouldUseFlameShock() const;
    bool ShouldUseLavaBurst() const;
    bool ShouldUseLightningBolt() const;
    bool ShouldUseChainLightning() const;
    bool ShouldUseEarthShock() const;
    bool ShouldUseElementalBlast() const;
    bool ShouldUseEarthquake() const;
    bool ShouldUseIcefury() const;
    bool ShouldUseFrostShock() const;
    bool ShouldUseStormkeeper() const;
    bool ShouldUseAscendance() const;
    bool ShouldUseStormElemental() const;
    bool ShouldUsePrimordialWave() const;
    
    // State evaluation
    bool HasFlameShockOnTarget(Unit* target) const;
    bool ShouldUseLavaSurgeProc() const;
    bool ShouldUseBurstCooldowns() const;
    bool IsInAoESituation() const;
    bool ShouldPrioritizeMaelstromSpending() const;
    bool HasIcefuryBuff() const;
    
    // Resource optimization
    bool ShouldConserveMana() const;
    bool CanCastExpensiveSpell() const;
    
private:
    static constexpr float FLAME_SHOCK_REFRESH_THRESHOLD = 6.0f; // Refresh with <6s remaining
    static constexpr uint32 MAELSTROM_SPENDER_THRESHOLD = 60;    // Spend Maelstrom at 60+
    static constexpr uint32 AOE_TARGET_COUNT = 3;               // Minimum targets for AoE rotation
    static constexpr float MANA_CONSERVATION_THRESHOLD = 0.25f;  // Conserve mana below 25%
    static constexpr float BURST_COOLDOWN_HEALTH = 0.8f;        // Use burst when target >80% HP
};

/**
 * @brief Enhancement Shaman strategy - The War Within 11.2
 * 
 * Implements melee DPS rotation focused on Maelstrom Weapon procs:
 * - Generate Maelstrom Weapon stacks with Stormstrike and other abilities
 * - Spend stacks on instant Lightning Bolt/Chain Lightning (5+ stacks)
 * - Maintain weapon imbues (Windfury/Flametongue) 
 * - Use Lava Lash with Hot Hand procs and Sundering on cooldown
 * - Major cooldowns: Doom Winds, Feral Spirit, Ascendance
 */
class TC_GAME_API PlayerbotEnhancementShamanStrategy : public PlayerbotShamanStrategy
{
public:
    PlayerbotEnhancementShamanStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Enhancement-specific registration
    void RegisterEnhancementActions();
    void RegisterEnhancementTriggers();

    // Rotation logic methods
    bool ShouldUseStormstrike() const;
    bool ShouldUseLavaLash() const;
    bool ShouldUseCrashLightning() const;
    bool ShouldUseSundering() const;
    bool ShouldUseIceStrike() const;
    bool ShouldUseInstantLightningBolt() const;
    bool ShouldUseInstantChainLightning() const;
    bool ShouldUseElementalBlast() const;
    bool ShouldUseDoomWinds() const;
    bool ShouldUseFeralSpirit() const;
    bool ShouldUseAscendance() const;
    bool ShouldUsePrimordialWave() const;
    
    // State evaluation
    bool HasHotHandProc() const;
    bool HasStormsurgeProc() const;
    bool IsInDoomWinds() const;
    bool ShouldUseBurstCooldowns() const;
    bool ShouldUseAoEAbilities() const;
    bool ShouldPrioritizeMaelstromSpending() const;
    
    // Weapon imbue management
    bool ShouldUseWindfuryWeapon() const;
    bool ShouldUseFlametongueWeapon() const;
    bool HasWindfuryWeapon() const;
    bool HasFlametongueWeapon() const;
    bool NeedsWeaponImbues() const;
    
private:
    static constexpr uint32 MAELSTROM_WEAPON_SPEND_THRESHOLD = 5; // Spend at 5+ stacks
    static constexpr uint32 AOE_TARGET_COUNT = 2;                // Minimum targets for AoE abilities
    static constexpr float BURST_COOLDOWN_HEALTH = 0.6f;         // Use burst when target >60% HP
    static constexpr float LAVA_LASH_PRIORITY_THRESHOLD = 0.4f;  // Lava Lash priority with Hot Hand
};

/**
 * @brief Restoration Shaman strategy - The War Within 11.2
 * 
 * Implements healing rotation focused on smart target selection and Tidal Waves:
 * - Maintain Riptide on multiple targets for Tidal Waves procs
 * - Use Chain Heal for efficient group healing
 * - Healing Surge for emergency healing, Healing Wave with Tidal Waves
 * - Place totems strategically: Healing Stream, Spirit Link, Healing Tide
 * - Major cooldowns: Ascendance, Ancestral Guidance, Spirit Link Totem
 */
class TC_GAME_API PlayerbotRestorationShamanStrategy : public PlayerbotShamanStrategy
{
public:
    PlayerbotRestorationShamanStrategy(PlayerbotPlayerAI* ai);

    void InitializeActions() override;
    void InitializeTriggers() override;
    float GetActionPriority(std::string const& actionName) const override;

protected:
    // Restoration-specific registration
    void RegisterRestorationActions();
    void RegisterRestorationTriggers();

    // Healing priority methods
    bool ShouldUseRiptide() const;
    bool ShouldUseChainHeal() const;
    bool ShouldUseHealingSurge() const;
    bool ShouldUseHealingWave() const;
    bool ShouldUseHealingRain() const;
    bool ShouldUseHealingStreamTotem() const;
    bool ShouldUseSpiritLinkTotem() const;
    bool ShouldUseHealingTideTotem() const;
    bool ShouldUseAncestralGuidance() const;
    bool ShouldUseAscendance() const;
    bool ShouldUseEarthShield() const;
    bool ShouldUsePrimordialWave() const;
    
    // State evaluation
    bool HasTidalWavesBuff() const;
    uint32 GetTidalWavesStacks() const;
    bool ShouldUseTidalWavesHealing() const;
    bool NeedsEmergencyHealing() const;
    bool ShouldUseGroupHealing() const;
    bool ShouldUseMajorCooldowns() const;
    
    // Target evaluation
    Unit* FindBestRiptideTarget() const;
    Unit* FindBestChainHealTarget() const;
    Unit* FindEarthShieldTarget() const;
    uint32 CountMembersNeedingHealing(float threshold = 0.8f) const;
    bool IsGroupStacked() const;
    
    // Totem management
    bool ShouldPlaceHealingTotem() const;
    bool HasActiveHealingTotem() const;
    bool ShouldReplaceTotem() const;
    
    // Damage rotation (when healing not needed)
    bool ShouldDPS() const;
    bool ShouldUseLightningBolt() const;
    bool ShouldUseFlameShock() const;
    bool ShouldUseLavaBurst() const;
    
private:
    static constexpr float HEALING_THRESHOLD = 0.85f;           // Start healing below 85%
    static constexpr float EMERGENCY_THRESHOLD = 0.35f;        // Emergency healing below 35%
    static constexpr float CHAIN_HEAL_THRESHOLD = 0.75f;       // Chain Heal when multiple below 75%
    static constexpr float SPIRIT_LINK_THRESHOLD = 0.5f;       // Spirit Link when members below 50%
    static constexpr uint32 MIN_CHAIN_HEAL_TARGETS = 2;        // Minimum targets for Chain Heal
    static constexpr uint32 MIN_AOE_HEAL_TARGETS = 3;          // Minimum targets for AoE healing
    static constexpr float MANA_CONSERVATION_THRESHOLD = 0.2f; // Conserve mana below 20%
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTSHAMAN_STRATEGY_H