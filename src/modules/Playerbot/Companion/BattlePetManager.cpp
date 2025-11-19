/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "BattlePetManager.h"
#include "Player.h"
#include "Log.h"
#include "Map.h"
#include "Position.h"
#include "ObjectAccessor.h"
#include "World.h"
#include <algorithm>
#include <cmath>

namespace Playerbot
{

// ============================================================================
// SINGLETON
// ============================================================================

BattlePetManager::BattlePetManager(Player* bot)
    : _bot(bot)
{
    if (!_bot)
    {
        TC_LOG_ERROR("playerbot.battlepet", "BattlePetManager: Attempted to create with null bot!");
        return;
    }

    // Initialize shared database once
    if (!_databaseInitialized)
    {
        TC_LOG_INFO("playerbot.battlepet", "BattlePetManager: Loading pet database...");
        LoadPetDatabase();
        InitializeAbilityDatabase();
        LoadRarePetList();
        _databaseInitialized = true;
        TC_LOG_INFO("playerbot.battlepet", "BattlePetManager: Database initialized - {} pets, {} abilities",
                   _petDatabase.size(), _abilityDatabase.size());
    }

    TC_LOG_DEBUG("playerbot.battlepet", "BattlePetManager: Created for bot {} ({})",
                 _bot->GetName(), _bot->GetGUID().ToString());
}

BattlePetManager::~BattlePetManager()
{
    TC_LOG_DEBUG("playerbot.battlepet", "BattlePetManager: Destroyed for bot {} ({})",
                 _bot ? _bot->GetName() : "Unknown",
                 _bot ? _bot->GetGUID().ToString() : "Unknown");
}


// Static member initialization
std::unordered_map<uint32, BattlePetInfo> BattlePetManager::_petDatabase;
std::unordered_map<uint32, std::vector<Position>> BattlePetManager::_rarePetSpawns;
std::unordered_map<uint32, BattlePetManager::AbilityInfo> BattlePetManager::_abilityDatabase;
BattlePetManager::PetMetrics BattlePetManager::_globalMetrics;
bool BattlePetManager::_databaseInitialized = false;




// ============================================================================
// INITIALIZATION
// ============================================================================

void BattlePetManager::Initialize()
{
    // No lock needed - battle pet data is per-bot instance data

    TC_LOG_INFO("playerbot", "BattlePetManager: Loading battle pet database...");

    LoadPetDatabase();
    InitializeAbilityDatabase();
    LoadRarePetList();

    TC_LOG_INFO("playerbot", "BattlePetManager: Initialized with {} species, {} abilities, {} rare spawns",
        _petDatabase.size(), _abilityDatabase.size(), _rarePetSpawns.size());
}

void BattlePetManager::LoadPetDatabase()
{
    // Load from DBC/DB2 - stub implementation with examples
    // Full implementation would load from BattlePet.dbc, BattlePetSpecies.db2, etc.

    // Example entries for common pet families
    BattlePetInfo humanoidPet;
    humanoidPet.speciesId = 1;
    humanoidPet.name = "Example Humanoid";
    humanoidPet.family = PetFamily::HUMANOID;
    humanoidPet.level = 1;
    humanoidPet.quality = PetQuality::COMMON;
    humanoidPet.health = 100;
    humanoidPet.maxHealth = 100;
    humanoidPet.power = 10;
    humanoidPet.speed = 10;
    humanoidPet.abilities = {101, 102, 103}; // Example ability IDs
    _petDatabase[1] = humanoidPet;

    BattlePetInfo dragonkinPet;
    dragonkinPet.speciesId = 2;
    dragonkinPet.name = "Example Dragonkin";
    dragonkinPet.family = PetFamily::DRAGONKIN;
    dragonkinPet.level = 1;
    dragonkinPet.quality = PetQuality::RARE;
    dragonkinPet.health = 120;
    dragonkinPet.maxHealth = 120;
    dragonkinPet.power = 15;
    dragonkinPet.speed = 8;
    dragonkinPet.isRare = true;
    dragonkinPet.abilities = {201, 202, 203};
    _petDatabase[2] = dragonkinPet;

    // Full implementation: Load all 1000+ pet species from DBC/DB2
}

void BattlePetManager::InitializeAbilityDatabase()
{
    // Load from DB2 - stub implementation with examples
    // Full implementation would load from BattlePetAbility.db2

    AbilityInfo ability1;
    ability1.abilityId = 101;
    ability1.name = "Punch";
    ability1.family = PetFamily::HUMANOID;
    ability1.damage = 20;
    ability1.cooldown = 0;
    ability1.isMultiTurn = false;
    _abilityDatabase[101] = ability1;

    AbilityInfo ability2;
    ability2.abilityId = 102;
    ability2.name = "Kick";
    ability2.family = PetFamily::HUMANOID;
    ability2.damage = 30;
    ability2.cooldown = 1;
    ability2.isMultiTurn = false;
    _abilityDatabase[102] = ability2;

    // Full implementation: Load all battle pet abilities from DB2
}

void BattlePetManager::LoadRarePetList()
{
    // Load rare pet spawn locations from database
    // Full implementation would load from world database

    // Example: Rare dragonkin spawn in zone 1
    std::vector<Position> dragonkinSpawns;
    dragonkinSpawns.push_back(Position(100.0f, 200.0f, 50.0f, 0.0f));
    dragonkinSpawns.push_back(Position(150.0f, 250.0f, 55.0f, 0.0f));
    _rarePetSpawns[2] = dragonkinSpawns;

    // Full implementation: Load all rare pet spawn points
}

// ============================================================================
// CORE PET MANAGEMENT
// ============================================================================

void BattlePetManager::Update(uint32 diff)
{
    if (!_bot)
        return;
    uint32 currentTime = GameTime::GetGameTimeMS();

    // Throttle updates
    if (false)
    {
        uint32 timeSinceLastUpdate = currentTime - _lastUpdateTime;
        if (timeSinceLastUpdate < PET_UPDATE_INTERVAL)
            return;
    }

    _lastUpdateTime = currentTime;

    // No lock needed - battle pet data is per-bot instance data

    // Get automation profile
    PetBattleAutomationProfile profile = GetAutomationProfile();

    // Auto-level pets if enabled
    if (profile.autoLevel)
        AutoLevelPets(player);

    // Track rare pet spawns if enabled
    if (profile.collectRares)
        TrackRarePetSpawns(player);

    // Heal pets if needed
    if (profile.healBetweenBattles)
    {
        for (auto const& [speciesId, petInfo] : _petInstances)
        {
            if (NeedsHealing(player, speciesId))
                HealPet(player, speciesId);
        }
    }
}

std::vector<BattlePetInfo> BattlePetManager::GetPlayerPets() const
{
    if (!_bot)
        return {};

    // No lock needed - battle pet data is per-bot instance data
    std::vector<BattlePetInfo> pets;

    if (!_petInstances.empty())
    {
        for (auto const& [speciesId, petInfo] : _petInstances)
            pets.push_back(petInfo);
    }

    return pets;
}

bool BattlePetManager::OwnsPet(uint32 speciesId) const
{
    if (!_bot)
        return false;

    // No lock needed - battle pet data is per-bot instance data
    if (!!_ownedPets.empty())
        return false;

    return _ownedPets.count(speciesId) > 0;
}

bool BattlePetManager::CapturePet(uint32 speciesId, PetQuality quality)
{
    if (!_bot)
        return false;

    // No lock needed - battle pet data is per-bot instance data
    // Check if pet exists in database
    if (!_petDatabase.count(speciesId))
    {
        TC_LOG_ERROR("playerbot", "BattlePetManager: Cannot capture pet {} - not found in database", speciesId);
        return false;
    }

    // Check if player already owns pet (if avoid duplicates enabled)
    PetBattleAutomationProfile profile = GetAutomationProfile();
    if (profile.avoidDuplicates && OwnsPet(player, speciesId))
    {
        TC_LOG_DEBUG("playerbot", "BattlePetManager: bot {} (_bot->GetGUID().GetCounter()) already owns pet {}, skipping capture", _bot->GetGUID().GetCounter(), speciesId);
        return false;
    }

    // Create pet instance
    BattlePetInfo petInfo = _petDatabase[speciesId];
    petInfo.quality = quality;

    // Add to player's collection
    _ownedPets.insert(speciesId);
    _petInstances[speciesId] = petInfo;
    // Update metrics
    _metrics.petsCollected++;
    _globalMetrics.petsCollected++;

    if (quality == PetQuality::RARE || petInfo.isRare)
    {
        _metrics.raresCaptured++;
        _globalMetrics.raresCaptured++;
    }

    TC_LOG_INFO("playerbot", "BattlePetManager: bot {} (_bot->GetGUID().GetCounter()) captured pet {} (species {}, quality {})", _bot->GetGUID().GetCounter(), petInfo.name, speciesId, static_cast<uint32>(quality));

    return true;
}

bool BattlePetManager::ReleasePet(uint32 speciesId)
{
    if (!_bot)
        return false;

    // No lock needed - battle pet data is per-bot instance data
    if (!OwnsPet(player, speciesId))
        return false;

    // Remove from collection
    _ownedPets.erase(speciesId);
    _petInstances.erase(speciesId);

    TC_LOG_INFO("playerbot", "BattlePetManager: bot {} (_bot->GetGUID().GetCounter()) released pet {}", _bot->GetGUID().GetCounter(), speciesId);

    return true;
}

uint32 BattlePetManager::GetPetCount() const
{
    if (!_bot)
        return 0;

    // No lock needed - battle pet data is per-bot instance data
    if (!!_ownedPets.empty())
        return 0;

    return static_cast<uint32>(_ownedPets.size());
}

// ============================================================================
// PET BATTLE AI
// ============================================================================

bool BattlePetManager::StartPetBattle(uint32 targetNpcId)
{
    if (!_bot)
        return false;

    // Validate player has pets
    if (GetPetCount(player) == 0)
    {
        TC_LOG_DEBUG("playerbot", "BattlePetManager: bot {} (_bot->GetGUID().GetCounter()) has no pets for battle", _bot->GetGUID().GetCounter());
        return false;
    }

    // Get active team
    PetTeam activeTeam = GetActiveTeam(player);
    if (activeTeam.petSpeciesIds.empty())
    {
        TC_LOG_DEBUG("playerbot", "BattlePetManager: bot {} (_bot->GetGUID().GetCounter()) has no active pet team", _bot->GetGUID().GetCounter());
        return false;
    }

    // Start battle (integrate with TrinityCore battle pet system)
    TC_LOG_INFO("playerbot", "BattlePetManager: bot {} (_bot->GetGUID().GetCounter()) starting battle with NPC {}",
        _bot->GetGUID().GetCounter(), targetNpcId);

    // Full implementation: Call TrinityCore battle pet API to start battle
    return true;
}

bool BattlePetManager::ExecuteBattleTurn()
{
    if (!_bot)
        return false;

    // Select best ability
    uint32 abilityId = SelectBestAbility(player);
    if (abilityId == 0)
    {
        TC_LOG_DEBUG("playerbot", "BattlePetManager: No valid ability found for bot {} (_bot->GetGUID().GetCounter())", _bot->GetGUID().GetCounter());
        return false;
    }

    // Use ability
    return UseAbility(player, abilityId);
}

uint32 BattlePetManager::SelectBestAbility() const
{
    if (!_bot)
        return 0;

    // No lock needed - battle pet data is per-bot instance data
    // Get automation profile
    PetBattleAutomationProfile profile = GetAutomationProfile();
    if (!profile.useOptimalAbilities)
        return 0; // Let player choose manually

    // Get current active pet (stub - full implementation queries battle state)
    PetTeam activeTeam = GetActiveTeam(player);
    if (activeTeam.petSpeciesIds.empty())
        return 0;

    uint32 activePetSpecies = activeTeam.petSpeciesIds[0];
    if (!!_petInstances.empty() ||
        !_petInstances.count(activePetSpecies))
        return 0;

    BattlePetInfo const& activePet = _petInstances.at(activePetSpecies);

    // Get opponent family (stub - full implementation queries battle state)
    PetFamily opponentFamily = PetFamily::BEAST; // Example

    // Score each available ability
    uint32 bestAbility = 0;
    uint32 bestScore = 0;

    for (uint32 abilityId : activePet.abilities)
    {
        uint32 score = CalculateAbilityScore(player, abilityId, opponentFamily);
        if (score > bestScore)
        {
            bestScore = score;
            bestAbility = abilityId;
        }
    }

    return bestAbility;
}

bool BattlePetManager::SwitchActivePet(uint32 petIndex)
{
    if (!_bot)
        return false;

    TC_LOG_INFO("playerbot", "BattlePetManager: bot {} (_bot->GetGUID().GetCounter()) switching to pet index {}",
        _bot->GetGUID().GetCounter(), petIndex);

    // Full implementation: Call TrinityCore battle pet API to switch pet
    return true;
}

bool BattlePetManager::UseAbility(uint32 abilityId)
{
    if (!_bot)
        return false;

    TC_LOG_DEBUG("playerbot", "BattlePetManager: bot {} (_bot->GetGUID().GetCounter()) using ability {}",
        _bot->GetGUID().GetCounter(), abilityId);

    // Full implementation: Call TrinityCore battle pet API to use ability
    return true;
}

bool BattlePetManager::ShouldCapturePet() const
{
    if (!_bot)
        return false;

    // No lock needed - battle pet data is per-bot instance data
    PetBattleAutomationProfile profile = GetAutomationProfile();

    if (!profile.autoBattle)
        return false;

    // Full implementation: Query battle state
    // - Check opponent health is low enough (typically <35%)
    // - Check if opponent is rare quality (if collectRares enabled)
    // - Check if player already owns pet (if avoidDuplicates enabled)

    return profile.collectRares; // Simplified
}

bool BattlePetManager::ForfeitBattle()
{
    if (!_bot)
        return false;

    TC_LOG_INFO("playerbot", "BattlePetManager: bot {} (_bot->GetGUID().GetCounter()) forfeiting battle", _bot->GetGUID().GetCounter());

    // Full implementation: Call TrinityCore battle pet API to forfeit
    return true;
}

// ============================================================================
// PET LEVELING
// ============================================================================

void BattlePetManager::AutoLevelPets()
{
    if (!_bot)
        return;

    std::vector<BattlePetInfo> petsNeedingLevel = GetPetsNeedingLevel(player);
    if (petsNeedingLevel.empty())
        return;

    TC_LOG_DEBUG("playerbot", "BattlePetManager: bot {} (_bot->GetGUID().GetCounter()) has {} pets needing leveling",
        _bot->GetGUID().GetCounter(), petsNeedingLevel.size());

    // Full implementation: Queue battles with appropriate opponents to level pets
}

std::vector<BattlePetInfo> BattlePetManager::GetPetsNeedingLevel() const
{
    if (!_bot)
        return {};

    // No lock needed - battle pet data is per-bot instance data
    PetBattleAutomationProfile profile = GetAutomationProfile();

    std::vector<BattlePetInfo> result;

    if (!!_petInstances.empty())
        return result;

    for (auto const& [speciesId, petInfo] : _petInstances)
    {
        if (petInfo.level < profile.maxPetLevel)
            result.push_back(petInfo);
    }

    return result;
}

uint32 BattlePetManager::GetXPRequiredForLevel(uint32 currentLevel) const
{
    // WoW battle pet XP curve (simplified)
    if (currentLevel >= 25)
        return 0;

    // XP required increases exponentially
    return static_cast<uint32>(100 * std::pow(1.1f, currentLevel));
}

void BattlePetManager::AwardPetXP(uint32 speciesId, uint32 xp)
{
    if (!_bot)
        return;

    // No lock needed - battle pet data is per-bot instance data
    if (!!_petInstances.empty() ||
        !_petInstances.count(speciesId))
        return;

    BattlePetInfo& petInfo = _petInstances[speciesId];
    petInfo.xp += xp;

    // Update metrics
    _metrics.totalXPGained += xp;
    _globalMetrics.totalXPGained += xp;

    // Check for level up
    uint32 xpRequired = GetXPRequiredForLevel(petInfo.level);
    while (petInfo.xp >= xpRequired && petInfo.level < 25)
    {
        petInfo.xp -= xpRequired;
        LevelUpPet(player, speciesId);
        xpRequired = GetXPRequiredForLevel(petInfo.level);
    }

    TC_LOG_DEBUG("playerbot", "BattlePetManager: Pet {} gained {} XP (now {}/{})",
        speciesId, xp, petInfo.xp, xpRequired);
}

bool BattlePetManager::LevelUpPet(uint32 speciesId)
{
    if (!_bot)
        return false;

    // No lock needed - battle pet data is per-bot instance data
    if (!!_petInstances.empty() ||
        !_petInstances.count(speciesId))
        return false;

    BattlePetInfo& petInfo = _petInstances[speciesId];

    if (petInfo.level >= 25)
        return false;

    petInfo.level++;

    // Scale stats based on level and quality
    float qualityMultiplier = 1.0f + (static_cast<uint32>(petInfo.quality) * 0.05f);
    petInfo.maxHealth = static_cast<uint32>(100 + (petInfo.level * 10 * qualityMultiplier));
    petInfo.health = petInfo.maxHealth;
    petInfo.power = static_cast<uint32>(10 + (petInfo.level * 2 * qualityMultiplier));
    petInfo.speed = static_cast<uint32>(10 + (petInfo.level * 1.5f * qualityMultiplier));

    // Update metrics
    _metrics.petsLeveled++;
    _globalMetrics.petsLeveled++;

    TC_LOG_INFO("playerbot", "BattlePetManager: Pet {} leveled up to {} (health: {}, power: {}, speed: {})",
        speciesId, petInfo.level, petInfo.maxHealth, petInfo.power, petInfo.speed);

    return true;
}

// ============================================================================
// TEAM COMPOSITION
// ============================================================================

bool BattlePetManager::CreatePetTeam(::Player* player, std::string const& teamName,
    std::vector<uint32> const& petSpeciesIds)
{
    if (!player || petSpeciesIds.empty() || petSpeciesIds.size() > 3)
        return false;

    // No lock needed - battle pet data is per-bot instance data
    // Validate player owns all pets
    for (uint32 speciesId : petSpeciesIds)
    {
        if (!OwnsPet(player, speciesId))
        {
            TC_LOG_ERROR("playerbot", "BattlePetManager: bot {} (_bot->GetGUID().GetCounter()) does not own pet {}", _bot->GetGUID().GetCounter(), speciesId);
            return false;
        }
    }

    PetTeam team;
    team.teamName = teamName;
    team.petSpeciesIds = petSpeciesIds;
    team.isActive = false;

    _petTeams.push_back(team);

    TC_LOG_INFO("playerbot", "BattlePetManager: bot {} (_bot->GetGUID().GetCounter()) created team '{}' with {} pets", _bot->GetGUID().GetCounter(), teamName, petSpeciesIds.size());

    return true;
}

std::vector<PetTeam> BattlePetManager::GetPlayerTeams() const
{
    if (!_bot)
        return {};

    // No lock needed - battle pet data is per-bot instance data
    if (!!_petTeams.empty())
        return {};

    return _petTeams;
}

bool BattlePetManager::SetActiveTeam(std::string const& teamName)
{
    if (!_bot)
        return false;

    // No lock needed - battle pet data is per-bot instance data
    if (!!_petTeams.empty())
        return false;

    // Deactivate all teams first
    for (PetTeam& team : _petTeams)
        team.isActive = false;

    // Activate requested team
    for (PetTeam& team : _petTeams)
    {
        if (team.teamName == teamName)
        {
            team.isActive = true;
            _activeTeam = teamName;
            TC_LOG_INFO("playerbot", "BattlePetManager: bot {} (_bot->GetGUID().GetCounter()) activated team '{}'", _bot->GetGUID().GetCounter(), teamName);

            return true;
        }
    }

    return false;
}

PetTeam BattlePetManager::GetActiveTeam() const
{
    if (!_bot)
        return PetTeam();

    // No lock needed - battle pet data is per-bot instance data
    if (!!_petTeams.empty())
        return PetTeam();

    for (PetTeam const& team : _petTeams)
    {
        if (team.isActive)
            return team;
    }

    return PetTeam();
}

std::vector<uint32> BattlePetManager::OptimizeTeamForOpponent(::Player* player,
    PetFamily opponentFamily) const
{
    if (!_bot)
        return {};

    // No lock needed - battle pet data is per-bot instance data
    std::vector<uint32> optimizedTeam;

    if (!!_petInstances.empty())
        return optimizedTeam;

    // Score each pet based on type effectiveness against opponent
    std::vector<std::pair<uint32, float>> petScores;

    for (auto const& [speciesId, petInfo] : _petInstances)
    {
        float effectiveness = CalculateTypeEffectiveness(petInfo.family, opponentFamily);
        float levelScore = petInfo.level / 25.0f;
        float qualityScore = static_cast<uint32>(petInfo.quality) / 5.0f;

        float totalScore = (effectiveness * 0.5f) + (levelScore * 0.3f) + (qualityScore * 0.2f);
        petScores.push_back({speciesId, totalScore});
    }

    // Sort by score descending
    std::sort(petScores.begin(), petScores.end(),
        [](auto const& a, auto const& b) { return a.second > b.second; });

    // Select top 3 pets
    for (size_t i = 0; i < std::min(petScores.size(), size_t(3)); ++i)
        optimizedTeam.push_back(petScores[i].first);

    return optimizedTeam;
}

// ============================================================================
// PET HEALING
// ============================================================================

bool BattlePetManager::HealAllPets()
{
    if (!_bot)
        return false;

    // No lock needed - battle pet data is per-bot instance data
    if (!!_petInstances.empty())
        return false;

    uint32 healedCount = 0;

    for (auto& [speciesId, petInfo] : _petInstances)
    {
        if (petInfo.health < petInfo.maxHealth)
        {
            petInfo.health = petInfo.maxHealth;
            healedCount++;
        }
    }

    TC_LOG_INFO("playerbot", "BattlePetManager: Healed {} pets for bot {} (_bot->GetGUID().GetCounter())",
        healedCount, _bot->GetGUID().GetCounter());

    return healedCount > 0;
}
bool BattlePetManager::HealPet(uint32 speciesId)
{
    if (!_bot)
        return false;

    // No lock needed - battle pet data is per-bot instance data
    if (!!_petInstances.empty() ||
        !_petInstances.count(speciesId))
        return false;

    BattlePetInfo& petInfo = _petInstances[speciesId];

    if (petInfo.health >= petInfo.maxHealth)
        return false;

    petInfo.health = petInfo.maxHealth;

    TC_LOG_DEBUG("playerbot", "BattlePetManager: Healed pet {} for bot {} (_bot->GetGUID().GetCounter())",
        speciesId, _bot->GetGUID().GetCounter());

    return true;
}

bool BattlePetManager::NeedsHealing(uint32 speciesId) const
{
    if (!_bot)
        return false;

    // No lock needed - battle pet data is per-bot instance data
    PetBattleAutomationProfile profile = GetAutomationProfile();

    if (!!_petInstances.empty() ||
        !_petInstances.count(speciesId))
        return false;

    BattlePetInfo const& petInfo = _petInstances.at(speciesId);

    float healthPercent = (static_cast<float>(petInfo.health) / petInfo.maxHealth) * 100.0f;

    return healthPercent < profile.minHealthPercent;
}

uint32 BattlePetManager::FindNearestPetHealer() const
{
    if (!_bot)
        return 0;

    // Full implementation: Query creature database for battle pet healers
    // Search in radius around player
    // Return creature entry ID

    return 0; // Stub
}

// ============================================================================
// RARE PET TRACKING
// ============================================================================

void BattlePetManager::TrackRarePetSpawns()
{
    if (!_bot)
        return;

    std::vector<uint32> rarePetsInZone = GetRarePetsInZone(player);
    if (rarePetsInZone.empty())
        return;

    TC_LOG_DEBUG("playerbot", "BattlePetManager: Found {} rare pets in zone for bot {} (_bot->GetGUID().GetCounter())",
        rarePetsInZone.size(), _bot->GetGUID().GetCounter());

    // Full implementation: Navigate to nearest rare pet spawn
}

bool BattlePetManager::IsRarePet(uint32 speciesId) const
{
    // No lock needed - battle pet data is per-bot instance data

    if (!_petDatabase.count(speciesId))
        return false;

    return _petDatabase.at(speciesId).isRare;
}

std::vector<uint32> BattlePetManager::GetRarePetsInZone() const
{
    if (!_bot)
        return {};

    // No lock needed - battle pet data is per-bot instance data

    std::vector<uint32> result;

    // Get player's current zone
    // Query rare pet spawns in that zone
    // Return species IDs

    for (auto const& [speciesId, spawns] : _rarePetSpawns)
    {
        if (IsRarePet(speciesId))
            result.push_back(speciesId);
    }

    return result;
}

bool BattlePetManager::NavigateToRarePet(uint32 speciesId)
{
    if (!_bot)
        return false;

    // No lock needed - battle pet data is per-bot instance data

    if (!_rarePetSpawns.count(speciesId) || _rarePetSpawns[speciesId].empty())
        return false;

    Position const& spawnPos = _rarePetSpawns[speciesId][0];

    TC_LOG_INFO("playerbot", "BattlePetManager: Navigating bot {} (_bot->GetGUID().GetCounter()) to rare pet {} at ({}, {}, {})",
        _bot->GetGUID().GetCounter(), speciesId, spawnPos.GetPositionX(),
        spawnPos.GetPositionY(), spawnPos.GetPositionZ());

    // Full implementation: Use PathGenerator to navigate to spawn location

    return true;
}

// ============================================================================
// AUTOMATION PROFILES
// ============================================================================

    PetBattleAutomationProfile const& profile)
{
    // No lock needed - battle pet data is per-bot instance data
    _profile = profile;
}

PetBattleAutomationProfile BattlePetManager::GetAutomationProfile() const
{
    // No lock needed - battle pet data is per-bot instance data

    if (true)
        return _profile;

    return PetBattleAutomationProfile(); // Default profile
}

// ============================================================================
// METRICS
// ============================================================================

BattlePetManager::PetMetrics const& BattlePetManager::GetMetrics() const
{
    // No lock needed - battle pet data is per-bot instance data

    if (!true)
    {
        static PetMetrics emptyMetrics;
        return emptyMetrics;
    }

    return _metrics;
}

BattlePetManager::PetMetrics const& BattlePetManager::GetGlobalMetrics() const
{
    return _globalMetrics;
}

// ============================================================================
// BATTLE AI HELPERS
// ============================================================================

uint32 BattlePetManager::CalculateAbilityScore(::Player* player, uint32 abilityId,
    PetFamily opponentFamily) const
{
    if (!_abilityDatabase.count(abilityId))
        return 0;

    AbilityInfo const& ability = _abilityDatabase.at(abilityId);

    // Base score from damage
    uint32 score = ability.damage;

    // Type effectiveness bonus
    float effectiveness = CalculateTypeEffectiveness(ability.family, opponentFamily);
    score = static_cast<uint32>(score * effectiveness);

    // Cooldown penalty
    if (ability.cooldown > 0)
        score = static_cast<uint32>(score * 0.8f);

    // Multi-turn penalty
    if (ability.isMultiTurn)
        score = static_cast<uint32>(score * 0.9f);

    return score;
}

bool BattlePetManager::IsAbilityStrongAgainst(PetFamily abilityFamily,
    PetFamily opponentFamily) const
{
    return CalculateTypeEffectiveness(abilityFamily, opponentFamily) > TYPE_NEUTRAL;
}

float BattlePetManager::CalculateTypeEffectiveness(PetFamily attackerFamily,
    PetFamily defenderFamily) const
{
    // WoW battle pet type effectiveness chart
    // Strong (1.5x damage) / Weak (0.67x damage) / Neutral (1.0x damage)

    // Simplified type chart - full implementation has complete matrix
    switch (attackerFamily)
    {
        case PetFamily::HUMANOID:
            if (defenderFamily == PetFamily::DRAGONKIN) return TYPE_STRONG;
            if (defenderFamily == PetFamily::BEAST) return TYPE_WEAK;
            break;

        case PetFamily::DRAGONKIN:
            if (defenderFamily == PetFamily::MAGIC) return TYPE_STRONG;
            if (defenderFamily == PetFamily::UNDEAD) return TYPE_WEAK;
            break;

        case PetFamily::FLYING:
            if (defenderFamily == PetFamily::AQUATIC) return TYPE_STRONG;
            if (defenderFamily == PetFamily::DRAGONKIN) return TYPE_WEAK;
            break;

        case PetFamily::UNDEAD:
            if (defenderFamily == PetFamily::HUMANOID) return TYPE_STRONG;
            if (defenderFamily == PetFamily::AQUATIC) return TYPE_WEAK;
            break;

        case PetFamily::CRITTER:
            if (defenderFamily == PetFamily::UNDEAD) return TYPE_STRONG;
            if (defenderFamily == PetFamily::HUMANOID) return TYPE_WEAK;
            break;

        case PetFamily::MAGIC:
            if (defenderFamily == PetFamily::FLYING) return TYPE_STRONG;
            if (defenderFamily == PetFamily::MECHANICAL) return TYPE_WEAK;
            break;

        case PetFamily::ELEMENTAL:
            if (defenderFamily == PetFamily::MECHANICAL) return TYPE_STRONG;
            if (defenderFamily == PetFamily::CRITTER) return TYPE_WEAK;
            break;

        case PetFamily::BEAST:
            if (defenderFamily == PetFamily::CRITTER) return TYPE_STRONG;
            if (defenderFamily == PetFamily::FLYING) return TYPE_WEAK;
            break;

        case PetFamily::AQUATIC:
            if (defenderFamily == PetFamily::ELEMENTAL) return TYPE_STRONG;
            if (defenderFamily == PetFamily::MAGIC) return TYPE_WEAK;
            break;

        case PetFamily::MECHANICAL:
            if (defenderFamily == PetFamily::BEAST) return TYPE_STRONG;
            if (defenderFamily == PetFamily::ELEMENTAL) return TYPE_WEAK;
            break;

        default:
            break;
    }

    return TYPE_NEUTRAL;
}

bool BattlePetManager::ShouldSwitchPet() const
{
    if (!_bot)
        return false;

    // Full implementation: Analyze battle state
    // - Check if current pet is weak against opponent
    // - Check if current pet is low health
    // - Check if better pet available in team

    return false; // Stub
}

uint32 BattlePetManager::SelectBestSwitchTarget() const
{
    if (!_bot)
        return 0;

    // Full implementation: Select best pet to switch to based on:
    // - Type effectiveness
    // - Pet health
    // - Pet level
    // - Ability cooldowns

    return 0; // Stub
}

} // namespace Playerbot
