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
#include <string>
#include <vector>
#include <cstdint>

// Forward declarations
class Player;

namespace Playerbot
{

// Forward declarations
struct BattlePetInfo;
struct PetTeam;
struct PetBattleAutomationProfile;
enum class PetQuality : uint8;
enum class PetFamily : uint8;

/**
 * @brief Interface for battle pet automation system
 *
 * Provides complete battle pet management including collection, battle AI,
 * leveling automation, team composition, and rare pet tracking.
 */
class TC_GAME_API IBattlePetManager
{
public:
    struct PetMetrics
    {
        uint32 petsCollected;
        uint32 battlesWon;
        uint32 battlesLost;
        uint32 raresCaptured;
        uint32 petsLeveled;
        uint32 totalXPGained;

        float GetWinRate() const
        {
            uint32 total = battlesWon + battlesLost;
            return total > 0 ? (float)battlesWon / total : 0.0f;
        }
    };

    virtual ~IBattlePetManager() = default;

    // Core pet management
    virtual void Initialize() = 0;
    virtual void Update(::Player* player, uint32 diff) = 0;
    virtual std::vector<BattlePetInfo> GetPlayerPets(::Player* player) const = 0;
    virtual bool OwnsPet(::Player* player, uint32 speciesId) const = 0;
    virtual bool CapturePet(::Player* player, uint32 speciesId, PetQuality quality) = 0;
    virtual bool ReleasePet(::Player* player, uint32 speciesId) = 0;
    virtual uint32 GetPetCount(::Player* player) const = 0;

    // Pet battle AI
    virtual bool StartPetBattle(::Player* player, uint32 targetNpcId) = 0;
    virtual bool ExecuteBattleTurn(::Player* player) = 0;
    virtual uint32 SelectBestAbility(::Player* player) const = 0;
    virtual bool SwitchActivePet(::Player* player, uint32 petIndex) = 0;
    virtual bool UseAbility(::Player* player, uint32 abilityId) = 0;
    virtual bool ShouldCapturePet(::Player* player) const = 0;
    virtual bool ForfeitBattle(::Player* player) = 0;

    // Pet leveling
    virtual void AutoLevelPets(::Player* player) = 0;
    virtual std::vector<BattlePetInfo> GetPetsNeedingLevel(::Player* player) const = 0;
    virtual uint32 GetXPRequiredForLevel(uint32 currentLevel) const = 0;
    virtual void AwardPetXP(::Player* player, uint32 speciesId, uint32 xp) = 0;
    virtual bool LevelUpPet(::Player* player, uint32 speciesId) = 0;

    // Team composition
    virtual bool CreatePetTeam(::Player* player, std::string const& teamName, std::vector<uint32> const& petSpeciesIds) = 0;
    virtual std::vector<PetTeam> GetPlayerTeams(::Player* player) const = 0;
    virtual bool SetActiveTeam(::Player* player, std::string const& teamName) = 0;
    virtual PetTeam GetActiveTeam(::Player* player) const = 0;
    virtual std::vector<uint32> OptimizeTeamForOpponent(::Player* player, PetFamily opponentFamily) const = 0;

    // Pet healing
    virtual bool HealAllPets(::Player* player) = 0;
    virtual bool HealPet(::Player* player, uint32 speciesId) = 0;
    virtual bool NeedsHealing(::Player* player, uint32 speciesId) const = 0;
    virtual uint32 FindNearestPetHealer(::Player* player) const = 0;

    // Rare pet tracking
    virtual void TrackRarePetSpawns(::Player* player) = 0;
    virtual bool IsRarePet(uint32 speciesId) const = 0;
    virtual std::vector<uint32> GetRarePetsInZone(::Player* player) const = 0;
    virtual bool NavigateToRarePet(::Player* player, uint32 speciesId) = 0;

    // Automation profiles
    virtual void SetAutomationProfile(uint32 playerGuid, PetBattleAutomationProfile const& profile) = 0;
    virtual PetBattleAutomationProfile GetAutomationProfile(uint32 playerGuid) const = 0;

    // Metrics
    virtual PetMetrics const& GetPlayerMetrics(uint32 playerGuid) const = 0;
    virtual PetMetrics const& GetGlobalMetrics() const = 0;
};

} // namespace Playerbot
