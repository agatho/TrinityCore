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

/**
 * @brief Interface for FarmingCoordinator (Phase 5.2 - Per-Bot Pattern)
 *
 * All methods operate on the bot instance (no Player* parameters)
 */
class TC_GAME_API IFarmingCoordinator
{
public:
    virtual ~IFarmingCoordinator() = default;

    // Core farming coordination
    virtual void Initialize() = 0;
    virtual void Update(::Player* player, uint32 diff) = 0;  // Keep for BehaviorManager compatibility
    virtual void SetEnabled(bool enabled) = 0;
    virtual bool IsEnabled() const = 0;
    virtual void SetCoordinatorProfile(FarmingCoordinatorProfile const& profile) = 0;
    virtual FarmingCoordinatorProfile GetCoordinatorProfile() const = 0;

    // Skill analysis
    virtual bool NeedsFarming(ProfessionType profession) const = 0;
    virtual int32 GetSkillGap(ProfessionType profession) const = 0;
    virtual uint16 GetTargetSkillLevel(ProfessionType profession) const = 0;
    virtual std::vector<ProfessionType> GetProfessionsNeedingFarm() const = 0;
    virtual uint32 CalculateFarmingDuration(ProfessionType profession) const = 0;

    // Farming session management
    virtual bool StartFarmingSession(ProfessionType profession, FarmingSessionType sessionType) = 0;
    virtual void StopFarmingSession() = 0;
    virtual FarmingSession const* GetActiveFarmingSession() const = 0;
    virtual bool HasActiveFarmingSession() const = 0;
    virtual void UpdateFarmingSession(uint32 diff) = 0;
    virtual bool ShouldEndFarmingSession(FarmingSession const& session) const = 0;

    // Zone selection
    virtual FarmingZoneInfo const* GetOptimalFarmingZone(ProfessionType profession) const = 0;
    virtual std::vector<FarmingZoneInfo> GetSuitableZones(ProfessionType profession) const = 0;
    virtual float CalculateZoneScore(FarmingZoneInfo const& zone) const = 0;

    // Material management
    virtual bool HasReachedStockpileTarget(uint32 itemId) const = 0;
    virtual uint32 GetMaterialCount(uint32 itemId) const = 0;
    virtual std::vector<std::pair<uint32, uint32>> GetNeededMaterials(ProfessionType profession) const = 0;

    // Statistics
    virtual FarmingStatistics const& GetStatistics() const = 0;
    virtual void ResetStatistics() = 0;
};

} // namespace Playerbot
