/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include "Threading/LockHierarchy.h"
#include "Player.h"
#include "ObjectGuid.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace Playerbot
{
    // Interface
    #include "Core/DI/Interfaces/IBattlePetManager.h"


/**
 * @brief Pet quality levels (WoW battle pet system)
 */
enum class PetQuality : uint8
{
    POOR = 0,      // Grey
    COMMON = 1,    // White
    UNCOMMON = 2,  // Green
    RARE = 3,      // Blue
    EPIC = 4,      // Purple
    LEGENDARY = 5  // Orange
};

/**
 * @brief Pet family types
 */
enum class PetFamily : uint8
{
    HUMANOID = 1,
    DRAGONKIN = 2,
    FLYING = 3,
    UNDEAD = 4,
    CRITTER = 5,
    MAGIC = 6,
    ELEMENTAL = 7,
    BEAST = 8,
    AQUATIC = 9,
    MECHANICAL = 10
};

/**
 * @brief Battle pet information
 */
struct BattlePetInfo
{
    uint32 speciesId;
    std::string name;
    PetFamily family;
    uint32 level;
    uint32 xp;
    PetQuality quality;
    uint32 health;
    uint32 maxHealth;
    uint32 power;
    uint32 speed;
    bool isFavorite;
    bool isRare;
    bool isTradeable;
    std::vector<uint32> abilities; // Ability IDs

    BattlePetInfo() : speciesId(0), family(PetFamily::BEAST), level(1), xp(0),
        quality(PetQuality::COMMON), health(100), maxHealth(100), power(10), speed(10),
        isFavorite(false), isRare(false), isTradeable(true) {}
};

/**
 * @brief Pet team composition for battles
 */
struct PetTeam
{
    std::string teamName;
    std::vector<uint32> petSpeciesIds; // Up to 3 pets
    bool isActive;

    PetTeam() : isActive(false) {}
};

/**
 * @brief Pet battle automation profile
 */
struct PetBattleAutomationProfile
{
    bool autoBattle = true;           // Auto-engage pet battles
    bool autoLevel = true;             // Auto-level low-level pets
    bool collectRares = true;          // Prioritize capturing rare pets
    bool avoidDuplicates = false;      // Don't capture pets already owned
    uint32 maxPetLevel = 25;           // Max level to train pets to
    bool useOptimalAbilities = true;   // Use best ability rotation
    bool healBetweenBattles = true;    // Heal pets between battles
    uint32 minHealthPercent = 30;      // Min health % before healing

    PetBattleAutomationProfile() = default;
};

/**
 * @brief Battle Pet Manager - Complete battle pet automation for bots
 *
 * **Phase 6.3: Per-Bot Instance Pattern (26th Manager)**
 *
 * Features:
 * - Battle pet collection
 * - Pet battle AI
 * - Pet leveling automation
 * - Pet team composition
 * - Rare pet tracking
 * - Pet quality assessment
 * - Automatic pet healing
 * - Optimal ability usage
 * - Performance optimized (per-bot isolation, zero mutex)
 *
 * **Ownership:**
 * - Owned by GameSystemsManager (26th manager)
 * - Each bot has independent pet collection and battle state
 * - Shared pet/ability database across all bots (static)
 */
class TC_GAME_API BattlePetManager final : public IBattlePetManager
{
public:
    // ============================================================================
    // CORE PET MANAGEMENT
    // ============================================================================

    /**
     * Initialize battle pet system on server startup
     */
    void Initialize() override;

    /**
     * Update pet automation for player (called periodically)
     */
    void Update(uint32 diff) override;

    /**
     * Get all pets player owns
     */
    std::vector<BattlePetInfo> GetPlayerPets() const override;

    /**
     * Check if player owns pet
     */
    bool OwnsPet(uint32 speciesId) const override;

    /**
     * Capture pet (after battle)
     */
    bool CapturePet(uint32 speciesId, PetQuality quality) override;

    /**
     * Release pet from collection
     */
    bool ReleasePet(uint32 speciesId) override;

    /**
     * Get pet count for player
     */
    uint32 GetPetCount() const override;

    // ============================================================================
    // PET BATTLE AI
    // ============================================================================

    /**
     * Start pet battle
     */
    bool StartPetBattle(uint32 targetNpcId) override;

    /**
     * Execute pet battle turn
     */
    bool ExecuteBattleTurn() override;

    /**
     * Select best ability for current turn
     */
    uint32 SelectBestAbility() const override;

    /**
     * Switch active pet during battle
     */
    bool SwitchActivePet(uint32 petIndex) override;

    /**
     * Use ability in battle
     */
    bool UseAbility(uint32 abilityId) override;

    /**
     * Check if should capture opponent pet
     */
    bool ShouldCapturePet() const override;

    /**
     * Forfeit battle
     */
    bool ForfeitBattle() override;

    // ============================================================================
    // PET LEVELING
    // ============================================================================

    /**
     * Auto-level pets to max level
     */
    void AutoLevelPets() override;

    /**
     * Get pets that need leveling
     */
    std::vector<BattlePetInfo> GetPetsNeedingLevel() const override;

    /**
     * Calculate XP required for next level
     */
    uint32 GetXPRequiredForLevel(uint32 currentLevel) const override;

    /**
     * Award XP to pet after battle
     */
    void AwardPetXP(uint32 speciesId, uint32 xp) override;

    /**
     * Level up pet
     */
    bool LevelUpPet(uint32 speciesId) override;

    // ============================================================================
    // TEAM COMPOSITION
    // ============================================================================

    /**
     * Create pet team
     */
    bool CreatePetTeam(std::string const& teamName, std::vector<uint32> const& petSpeciesIds) override;

    /**
     * Get all pet teams
     */
    std::vector<PetTeam> GetPlayerTeams() const override;

    /**
     * Set active team
     */
    bool SetActiveTeam(std::string const& teamName) override;

    /**
     * Get active team
     */
    PetTeam GetActiveTeam() const override;

    /**
     * Optimize team composition based on opponent
     */
    std::vector<uint32> OptimizeTeamForOpponent(PetFamily opponentFamily) const override;

    // ============================================================================
    // PET HEALING
    // ============================================================================

    /**
     * Heal all pets
     */
    bool HealAllPets() override;

    /**
     * Heal specific pet
     */
    bool HealPet(uint32 speciesId) override;

    /**
     * Check if pet needs healing
     */
    bool NeedsHealing(uint32 speciesId) const override;

    /**
     * Get nearest pet healer NPC
     */
    uint32 FindNearestPetHealer() const override;

    // ============================================================================
    // RARE PET TRACKING
    // ============================================================================

    /**
     * Track rare pet spawns
     */
    void TrackRarePetSpawns() override;

    /**
     * Check if pet is rare
     */
    bool IsRarePet(uint32 speciesId) const override;

    /**
     * Get rare pets in current zone
     */
    std::vector<uint32> GetRarePetsInZone() const override;

    /**
     * Navigate to rare pet spawn
     */
    bool NavigateToRarePet(uint32 speciesId) override;

    // ============================================================================
    // AUTOMATION PROFILES
    // ============================================================================

    void SetAutomationProfile(PetBattleAutomationProfile const& profile) override;
    PetBattleAutomationProfile GetAutomationProfile() const override;

    // ============================================================================
    // METRICS
    // ============================================================================

    struct PetMetrics
    {
        std::atomic<uint32> petsCollected{0};
        std::atomic<uint32> battlesWon{0};
        std::atomic<uint32> battlesLost{0};
        std::atomic<uint32> raresCaptured{0};
        std::atomic<uint32> petsLeveled{0};
        std::atomic<uint32> totalXPGained{0};

        void Reset()
        {
            petsCollected = 0;
            battlesWon = 0;
            battlesLost = 0;
            raresCaptured = 0;
            petsLeveled = 0;
            totalXPGained = 0;
        }

        float GetWinRate() const
        {
            uint32 total = battlesWon.load() + battlesLost.load();
            return total > 0 ? (float)battlesWon.load() / total : 0.0f;
        }
    };

    PetMetrics const& GetMetrics() const override;
    PetMetrics const& GetGlobalMetrics() const override;

public:
    /**
     * @brief Construct battle pet manager for specific bot
     * @param bot The bot player this manager serves
     */
    explicit BattlePetManager(Player* bot);
    ~BattlePetManager();

    // Non-copyable
    BattlePetManager(BattlePetManager const&) = delete;
    BattlePetManager& operator=(BattlePetManager const&) = delete;

private:

    // ============================================================================
    // INITIALIZATION HELPERS
    // ============================================================================

    void LoadPetDatabase();
    void InitializeAbilityDatabase();
    void LoadRarePetList();

    // ============================================================================
    // BATTLE AI HELPERS
    // ============================================================================

    uint32 CalculateAbilityScore(uint32 abilityId, PetFamily opponentFamily) const;
    bool IsAbilityStrongAgainst(PetFamily abilityFamily, PetFamily opponentFamily) const;
    float CalculateTypeEffectiveness(PetFamily attackerFamily, PetFamily defenderFamily) const;
    bool ShouldSwitchPet() const;
    uint32 SelectBestSwitchTarget() const;

    // ============================================================================
    // DATA STRUCTURES
    // ============================================================================

    // Bot reference (non-owning)
    Player* _bot;

    // Per-bot instance data
    std::unordered_set<uint32> _ownedPets;              // Set of species IDs owned by this bot
    std::unordered_map<uint32, BattlePetInfo> _petInstances;  // speciesId -> pet info
    std::vector<PetTeam> _petTeams;                     // Pet teams for this bot
    std::string _activeTeam;                            // Currently active team name
    PetBattleAutomationProfile _profile;                // Automation settings
    PetMetrics _metrics;                                // Per-bot metrics
    uint32 _lastUpdateTime{0};                          // Last update timestamp

    // Shared static data (all bots)
    static std::unordered_map<uint32, BattlePetInfo> _petDatabase;
    static std::unordered_map<uint32, std::vector<Position>> _rarePetSpawns;
    static std::unordered_map<uint32, AbilityInfo> _abilityDatabase;
    static PetMetrics _globalMetrics;
    static bool _databaseInitialized;

    // Ability info structure
    struct AbilityInfo
    {
        uint32 abilityId;
        std::string name;
        PetFamily family;
        uint32 damage;
        uint32 cooldown;
        bool isMultiTurn;
    };

    // Update intervals
    static constexpr uint32 PET_UPDATE_INTERVAL = 5000;  // 5 seconds

    // Type effectiveness chart
    static constexpr float TYPE_STRONG = 1.5f;    // 50% bonus damage
    static constexpr float TYPE_WEAK = 0.67f;     // 33% reduced damage
    static constexpr float TYPE_NEUTRAL = 1.0f;   // Normal damage
};

} // namespace Playerbot
