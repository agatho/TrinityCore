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
#include "Player.h"
#include <vector>

class Player;

namespace Playerbot
{

// Forward declarations
enum class FarmingSessionType : uint8;
enum class ProfessionType : uint8;
struct FarmingZoneInfo;
struct FarmingSession;
struct FarmingCoordinatorProfile;
struct FarmingStatistics;

class TC_GAME_API IFarmingCoordinator
{
public:
    virtual ~IFarmingCoordinator() = default;

    // Core farming coordination
    virtual void Initialize() = 0;
    virtual void Update(::Player* player, uint32 diff) = 0;
    virtual void SetEnabled(::Player* player, bool enabled) = 0;
    virtual bool IsEnabled(::Player* player) const = 0;
    virtual void SetCoordinatorProfile(uint32 playerGuid, FarmingCoordinatorProfile const& profile) = 0;
    virtual FarmingCoordinatorProfile GetCoordinatorProfile(uint32 playerGuid) const = 0;

    // Skill analysis
    virtual bool NeedsFarming(::Player* player, ProfessionType profession) const = 0;
    virtual int32 GetSkillGap(::Player* player, ProfessionType profession) const = 0;
    virtual uint16 GetTargetSkillLevel(::Player* player, ProfessionType profession) const = 0;
    virtual std::vector<ProfessionType> GetProfessionsNeedingFarm(::Player* player) const = 0;
    virtual uint32 CalculateFarmingDuration(::Player* player, ProfessionType profession) const = 0;

    // Farming session management
    virtual bool StartFarmingSession(::Player* player, ProfessionType profession, FarmingSessionType sessionType) = 0;
    virtual void StopFarmingSession(::Player* player) = 0;
    virtual FarmingSession const* GetActiveFarmingSession(uint32 playerGuid) const = 0;
    virtual bool HasActiveFarmingSession(::Player* player) const = 0;
    virtual void UpdateFarmingSession(::Player* player, uint32 diff) = 0;
    virtual bool ShouldEndFarmingSession(::Player* player, FarmingSession const& session) const = 0;

    // Zone selection
    virtual FarmingZoneInfo const* GetOptimalFarmingZone(::Player* player, ProfessionType profession) const = 0;
    virtual std::vector<FarmingZoneInfo> GetSuitableZones(::Player* player, ProfessionType profession) const = 0;
    virtual float CalculateZoneScore(::Player* player, FarmingZoneInfo const& zone) const = 0;

    // Material management
    virtual bool HasReachedStockpileTarget(::Player* player, uint32 itemId) const = 0;
    virtual uint32 GetMaterialCount(::Player* player, uint32 itemId) const = 0;
    virtual std::vector<std::pair<uint32, uint32>> GetNeededMaterials(::Player* player, ProfessionType profession) const = 0;

    // Statistics
    virtual FarmingStatistics const& GetPlayerStatistics(uint32 playerGuid) const = 0;
    virtual FarmingStatistics const& GetGlobalStatistics() const = 0;
    virtual void ResetStatistics(uint32 playerGuid) = 0;
};

} // namespace Playerbot
