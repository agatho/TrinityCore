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

BattlePetManager* BattlePetManager::instance()
{
    static BattlePetManager instance;
    return &instance;
}

BattlePetManager::BattlePetManager()
{
    TC_LOG_INFO("playerbot", "BattlePetManager initialized");
}

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

void BattlePetManager::Update(::Player* player, uint32 diff)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    uint32 currentTime = GameTime::GetGameTimeMS();

    // Throttle updates
    if (_lastUpdateTimes.count(playerGuid))
    {
        uint32 timeSinceLastUpdate = currentTime - _lastUpdateTimes[playerGuid];
        if (timeSinceLastUpdate < PET_UPDATE_INTERVAL)
            return;
    }

    _lastUpdateTimes[playerGuid] = currentTime;

    // No lock needed - battle pet data is per-bot instance data

    // Get automation profile
    PetBattleAutomationProfile profile = GetAutomationProfile(playerGuid);

    // Auto-level pets if enabled
    if (profile.autoLevel)
        AutoLevelPets(player);

    // Track rare pet spawns if enabled
    if (profile.collectRares)
        TrackRarePetSpawns(player);

    // Heal pets if needed
    if (profile.healBetweenBattles)
    {
        for (auto const& [speciesId, petInfo] : _playerPetInstances[playerGuid])
        {
            if (NeedsHealing(player, speciesId))
                HealPet(player, speciesId);
        }
    }
}

std::vector<BattlePetInfo> BattlePetManager::GetPlayerPets(::Player* player) const
{
    if (!player)
        return {};

    // No lock needed - battle pet data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    std::vector<BattlePetInfo> pets;

    if (_playerPetInstances.count(playerGuid))
    {
        for (auto const& [speciesId, petInfo] : _playerPetInstances.at(playerGuid))
            pets.push_back(petInfo);
    }

    return pets;
}

bool BattlePetManager::OwnsPet(::Player* player, uint32 speciesId) const
{
    if (!player)
        return false;

    // No lock needed - battle pet data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    if (!_playerPets.count(playerGuid))
        return false;

    return _playerPets.at(playerGuid).count(speciesId) > 0;
}

bool BattlePetManager::CapturePet(::Player* player, uint32 speciesId, PetQuality quality)
{
    if (!player)
        return false;

    // No lock needed - battle pet data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    // Check if pet exists in database
    if (!_petDatabase.count(speciesId))
    {
        TC_LOG_ERROR("playerbot", "BattlePetManager: Cannot capture pet {} - not found in database", speciesId);
        return false;
    }

    // Check if player already owns pet (if avoid duplicates enabled)
    PetBattleAutomationProfile profile = GetAutomationProfile(playerGuid);
    if (profile.avoidDuplicates && OwnsPet(player, speciesId))
    {
        TC_LOG_DEBUG("playerbot", "BattlePetManager: Player {} already owns pet {}, skipping capture", playerGuid, speciesId);
        return false;
    }

    // Create pet instance
    BattlePetInfo petInfo = _petDatabase[speciesId];
    petInfo.quality = quality;

    // Add to player's collection
    _playerPets[playerGuid].insert(speciesId);
    _playerPetInstances[playerGuid][speciesId] = petInfo;
    // Update metrics
    _playerMetrics[playerGuid].petsCollected++;
    _globalMetrics.petsCollected++;

    if (quality == PetQuality::RARE || petInfo.isRare)
    {
        _playerMetrics[playerGuid].raresCaptured++;
        _globalMetrics.raresCaptured++;
    }

    TC_LOG_INFO("playerbot", "BattlePetManager: Player {} captured pet {} (species {}, quality {})",
        playerGuid, petInfo.name, speciesId, static_cast<uint32>(quality));

    return true;
}

bool BattlePetManager::ReleasePet(::Player* player, uint32 speciesId)
{
    if (!player)
        return false;

    // No lock needed - battle pet data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    if (!OwnsPet(player, speciesId))
        return false;

    // Remove from collection
    _playerPets[playerGuid].erase(speciesId);
    _playerPetInstances[playerGuid].erase(speciesId);

    TC_LOG_INFO("playerbot", "BattlePetManager: Player {} released pet {}", playerGuid, speciesId);

    return true;
}

uint32 BattlePetManager::GetPetCount(::Player* player) const
{
    if (!player)
        return 0;

    // No lock needed - battle pet data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    if (!_playerPets.count(playerGuid))
        return 0;

    return static_cast<uint32>(_playerPets.at(playerGuid).size());
}

// ============================================================================
// PET BATTLE AI
// ============================================================================

bool BattlePetManager::StartPetBattle(::Player* player, uint32 targetNpcId)
{
    if (!player)
        return false;

    // Validate player has pets
    if (GetPetCount(player) == 0)
    {
        TC_LOG_DEBUG("playerbot", "BattlePetManager: Player {} has no pets for battle", player->GetGUID().GetCounter());
        return false;
    }

    // Get active team
    PetTeam activeTeam = GetActiveTeam(player);
    if (activeTeam.petSpeciesIds.empty())
    {
        TC_LOG_DEBUG("playerbot", "BattlePetManager: Player {} has no active pet team", player->GetGUID().GetCounter());
        return false;
    }

    // Start battle (integrate with TrinityCore battle pet system)
    TC_LOG_INFO("playerbot", "BattlePetManager: Player {} starting battle with NPC {}",
        player->GetGUID().GetCounter(), targetNpcId);

    // Full implementation: Call TrinityCore battle pet API to start battle
    return true;
}

bool BattlePetManager::ExecuteBattleTurn(::Player* player)
{
    if (!player)
        return false;

    // Select best ability
    uint32 abilityId = SelectBestAbility(player);
    if (abilityId == 0)
    {
        TC_LOG_DEBUG("playerbot", "BattlePetManager: No valid ability found for player {}", player->GetGUID().GetCounter());
        return false;
    }

    // Use ability
    return UseAbility(player, abilityId);
}

uint32 BattlePetManager::SelectBestAbility(::Player* player) const
{
    if (!player)
        return 0;

    // No lock needed - battle pet data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    // Get automation profile
    PetBattleAutomationProfile profile = GetAutomationProfile(playerGuid);
    if (!profile.useOptimalAbilities)
        return 0; // Let player choose manually

    // Get current active pet (stub - full implementation queries battle state)
    PetTeam activeTeam = GetActiveTeam(player);
    if (activeTeam.petSpeciesIds.empty())
        return 0;

    uint32 activePetSpecies = activeTeam.petSpeciesIds[0];
    if (!_playerPetInstances.count(playerGuid) ||
        !_playerPetInstances.at(playerGuid).count(activePetSpecies))
        return 0;

    BattlePetInfo const& activePet = _playerPetInstances.at(playerGuid).at(activePetSpecies);

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

bool BattlePetManager::SwitchActivePet(::Player* player, uint32 petIndex)
{
    if (!player)
        return false;

    TC_LOG_INFO("playerbot", "BattlePetManager: Player {} switching to pet index {}",
        player->GetGUID().GetCounter(), petIndex);

    // Full implementation: Call TrinityCore battle pet API to switch pet
    return true;
}

bool BattlePetManager::UseAbility(::Player* player, uint32 abilityId)
{
    if (!player)
        return false;

    TC_LOG_DEBUG("playerbot", "BattlePetManager: Player {} using ability {}",
        player->GetGUID().GetCounter(), abilityId);

    // Full implementation: Call TrinityCore battle pet API to use ability
    return true;
}

bool BattlePetManager::ShouldCapturePet(::Player* player) const
{
    if (!player)
        return false;

    // No lock needed - battle pet data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    PetBattleAutomationProfile profile = GetAutomationProfile(playerGuid);

    if (!profile.autoBattle)
        return false;

    // Full implementation: Query battle state
    // - Check opponent health is low enough (typically <35%)
    // - Check if opponent is rare quality (if collectRares enabled)
    // - Check if player already owns pet (if avoidDuplicates enabled)

    return profile.collectRares; // Simplified
}

bool BattlePetManager::ForfeitBattle(::Player* player)
{
    if (!player)
        return false;

    TC_LOG_INFO("playerbot", "BattlePetManager: Player {} forfeiting battle", player->GetGUID().GetCounter());

    // Full implementation: Call TrinityCore battle pet API to forfeit
    return true;
}

// ============================================================================
// PET LEVELING
// ============================================================================

void BattlePetManager::AutoLevelPets(::Player* player)
{
    if (!player)
        return;

    std::vector<BattlePetInfo> petsNeedingLevel = GetPetsNeedingLevel(player);
    if (petsNeedingLevel.empty())
        return;

    TC_LOG_DEBUG("playerbot", "BattlePetManager: Player {} has {} pets needing leveling",
        player->GetGUID().GetCounter(), petsNeedingLevel.size());

    // Full implementation: Queue battles with appropriate opponents to level pets
}

std::vector<BattlePetInfo> BattlePetManager::GetPetsNeedingLevel(::Player* player) const
{
    if (!player)
        return {};

    // No lock needed - battle pet data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    PetBattleAutomationProfile profile = GetAutomationProfile(playerGuid);

    std::vector<BattlePetInfo> result;

    if (!_playerPetInstances.count(playerGuid))
        return result;

    for (auto const& [speciesId, petInfo] : _playerPetInstances.at(playerGuid))
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

void BattlePetManager::AwardPetXP(::Player* player, uint32 speciesId, uint32 xp)
{
    if (!player)
        return;

    // No lock needed - battle pet data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    if (!_playerPetInstances.count(playerGuid) ||
        !_playerPetInstances[playerGuid].count(speciesId))
        return;

    BattlePetInfo& petInfo = _playerPetInstances[playerGuid][speciesId];
    petInfo.xp += xp;

    // Update metrics
    _playerMetrics[playerGuid].totalXPGained += xp;
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

bool BattlePetManager::LevelUpPet(::Player* player, uint32 speciesId)
{
    if (!player)
        return false;

    // No lock needed - battle pet data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    if (!_playerPetInstances.count(playerGuid) ||
        !_playerPetInstances[playerGuid].count(speciesId))
        return false;

    BattlePetInfo& petInfo = _playerPetInstances[playerGuid][speciesId];

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
    _playerMetrics[playerGuid].petsLeveled++;
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

    uint32 playerGuid = player->GetGUID().GetCounter();
    // Validate player owns all pets
    for (uint32 speciesId : petSpeciesIds)
    {
        if (!OwnsPet(player, speciesId))
        {
            TC_LOG_ERROR("playerbot", "BattlePetManager: Player {} does not own pet {}",
                playerGuid, speciesId);
            return false;
        }
    }

    PetTeam team;
    team.teamName = teamName;
    team.petSpeciesIds = petSpeciesIds;
    team.isActive = false;

    _playerTeams[playerGuid].push_back(team);

    TC_LOG_INFO("playerbot", "BattlePetManager: Player {} created team '{}' with {} pets",
        playerGuid, teamName, petSpeciesIds.size());

    return true;
}

std::vector<PetTeam> BattlePetManager::GetPlayerTeams(::Player* player) const
{
    if (!player)
        return {};

    // No lock needed - battle pet data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    if (!_playerTeams.count(playerGuid))
        return {};

    return _playerTeams.at(playerGuid);
}

bool BattlePetManager::SetActiveTeam(::Player* player, std::string const& teamName)
{
    if (!player)
        return false;

    // No lock needed - battle pet data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    if (!_playerTeams.count(playerGuid))
        return false;

    // Deactivate all teams first
    for (PetTeam& team : _playerTeams[playerGuid])
        team.isActive = false;

    // Activate requested team
    for (PetTeam& team : _playerTeams[playerGuid])
    {
        if (team.teamName == teamName)
        {
            team.isActive = true;
            _activeTeams[playerGuid] = teamName;
            TC_LOG_INFO("playerbot", "BattlePetManager: Player {} activated team '{}'",
                playerGuid, teamName);

            return true;
        }
    }

    return false;
}

PetTeam BattlePetManager::GetActiveTeam(::Player* player) const
{
    if (!player)
        return PetTeam();

    // No lock needed - battle pet data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    if (!_playerTeams.count(playerGuid))
        return PetTeam();

    for (PetTeam const& team : _playerTeams.at(playerGuid))
    {
        if (team.isActive)
            return team;
    }

    return PetTeam();
}

std::vector<uint32> BattlePetManager::OptimizeTeamForOpponent(::Player* player,
    PetFamily opponentFamily) const
{
    if (!player)
        return {};

    // No lock needed - battle pet data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    std::vector<uint32> optimizedTeam;

    if (!_playerPetInstances.count(playerGuid))
        return optimizedTeam;

    // Score each pet based on type effectiveness against opponent
    std::vector<std::pair<uint32, float>> petScores;

    for (auto const& [speciesId, petInfo] : _playerPetInstances.at(playerGuid))
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

bool BattlePetManager::HealAllPets(::Player* player)
{
    if (!player)
        return false;

    // No lock needed - battle pet data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    if (!_playerPetInstances.count(playerGuid))
        return false;

    uint32 healedCount = 0;

    for (auto& [speciesId, petInfo] : _playerPetInstances[playerGuid])
    {
        if (petInfo.health < petInfo.maxHealth)
        {
            petInfo.health = petInfo.maxHealth;
            healedCount++;
        }
    }

    TC_LOG_INFO("playerbot", "BattlePetManager: Healed {} pets for player {}",
        healedCount, playerGuid);

    return healedCount > 0;
}
bool BattlePetManager::HealPet(::Player* player, uint32 speciesId)
{
    if (!player)
        return false;

    // No lock needed - battle pet data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    if (!_playerPetInstances.count(playerGuid) ||
        !_playerPetInstances[playerGuid].count(speciesId))
        return false;

    BattlePetInfo& petInfo = _playerPetInstances[playerGuid][speciesId];

    if (petInfo.health >= petInfo.maxHealth)
        return false;

    petInfo.health = petInfo.maxHealth;

    TC_LOG_DEBUG("playerbot", "BattlePetManager: Healed pet {} for player {}",
        speciesId, playerGuid);

    return true;
}

bool BattlePetManager::NeedsHealing(::Player* player, uint32 speciesId) const
{
    if (!player)
        return false;

    // No lock needed - battle pet data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    PetBattleAutomationProfile profile = GetAutomationProfile(playerGuid);

    if (!_playerPetInstances.count(playerGuid) ||
        !_playerPetInstances.at(playerGuid).count(speciesId))
        return false;

    BattlePetInfo const& petInfo = _playerPetInstances.at(playerGuid).at(speciesId);

    float healthPercent = (static_cast<float>(petInfo.health) / petInfo.maxHealth) * 100.0f;

    return healthPercent < profile.minHealthPercent;
}

uint32 BattlePetManager::FindNearestPetHealer(::Player* player) const
{
    if (!player)
        return 0;

    // Full implementation: Query creature database for battle pet healers
    // Search in radius around player
    // Return creature entry ID

    return 0; // Stub
}

// ============================================================================
// RARE PET TRACKING
// ============================================================================

void BattlePetManager::TrackRarePetSpawns(::Player* player)
{
    if (!player)
        return;

    std::vector<uint32> rarePetsInZone = GetRarePetsInZone(player);
    if (rarePetsInZone.empty())
        return;

    TC_LOG_DEBUG("playerbot", "BattlePetManager: Found {} rare pets in zone for player {}",
        rarePetsInZone.size(), player->GetGUID().GetCounter());

    // Full implementation: Navigate to nearest rare pet spawn
}

bool BattlePetManager::IsRarePet(uint32 speciesId) const
{
    // No lock needed - battle pet data is per-bot instance data

    if (!_petDatabase.count(speciesId))
        return false;

    return _petDatabase.at(speciesId).isRare;
}

std::vector<uint32> BattlePetManager::GetRarePetsInZone(::Player* player) const
{
    if (!player)
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

bool BattlePetManager::NavigateToRarePet(::Player* player, uint32 speciesId)
{
    if (!player)
        return false;

    // No lock needed - battle pet data is per-bot instance data

    if (!_rarePetSpawns.count(speciesId) || _rarePetSpawns[speciesId].empty())
        return false;

    Position const& spawnPos = _rarePetSpawns[speciesId][0];

    TC_LOG_INFO("playerbot", "BattlePetManager: Navigating player {} to rare pet {} at ({}, {}, {})",
        player->GetGUID().GetCounter(), speciesId, spawnPos.GetPositionX(),
        spawnPos.GetPositionY(), spawnPos.GetPositionZ());

    // Full implementation: Use PathGenerator to navigate to spawn location

    return true;
}

// ============================================================================
// AUTOMATION PROFILES
// ============================================================================

void BattlePetManager::SetAutomationProfile(uint32 playerGuid,
    PetBattleAutomationProfile const& profile)
{
    // No lock needed - battle pet data is per-bot instance data
    _playerProfiles[playerGuid] = profile;
}

PetBattleAutomationProfile BattlePetManager::GetAutomationProfile(uint32 playerGuid) const
{
    // No lock needed - battle pet data is per-bot instance data

    if (_playerProfiles.count(playerGuid))
        return _playerProfiles.at(playerGuid);

    return PetBattleAutomationProfile(); // Default profile
}

// ============================================================================
// METRICS
// ============================================================================

BattlePetManager::PetMetrics const& BattlePetManager::GetPlayerMetrics(uint32 playerGuid) const
{
    // No lock needed - battle pet data is per-bot instance data

    if (!_playerMetrics.count(playerGuid))
    {
        static PetMetrics emptyMetrics;
        return emptyMetrics;
    }

    return _playerMetrics.at(playerGuid);
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

bool BattlePetManager::ShouldSwitchPet(::Player* player) const
{
    if (!player)
        return false;

    // Full implementation: Analyze battle state
    // - Check if current pet is weak against opponent
    // - Check if current pet is low health
    // - Check if better pet available in team

    return false; // Stub
}

uint32 BattlePetManager::SelectBestSwitchTarget(::Player* player) const
{
    if (!player)
        return 0;

    // Full implementation: Select best pet to switch to based on:
    // - Type effectiveness
    // - Pet health
    // - Pet level
    // - Ability cooldowns

    return 0; // Stub
}

} // namespace Playerbot
