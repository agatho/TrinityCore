/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "FarmingCoordinator.h"
#include "ProfessionManager.h"
#include "GatheringManager.h"
#include "Player.h"
#include "Log.h"
#include "Map.h"
#include <algorithm>

namespace Playerbot
{

// Singleton instance
FarmingCoordinator* FarmingCoordinator::instance()
{
    static FarmingCoordinator instance;
    return &instance;
}

FarmingCoordinator::FarmingCoordinator()
{
}

// ============================================================================
// CORE FARMING COORDINATION
// ============================================================================

void FarmingCoordinator::Initialize()
{
    TC_LOG_INFO("playerbots", "FarmingCoordinator: Initializing farming coordination system...");

    LoadFarmingZones();
    InitializeZoneDatabase();

    TC_LOG_INFO("playerbots", "FarmingCoordinator: Loaded farming zones for {} professions",
        _farmingZones.size());
}

void FarmingCoordinator::Update(::Player* player, uint32 diff)
{
    if (!player || !IsEnabled(player))
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    uint32 currentTime = GameTime::GetGameTimeMS();

    std::lock_guard lock(_mutex);

    // Check if player has active farming session
    auto sessionIt = _activeSessions.find(playerGuid);
    if (sessionIt != _activeSessions.end())
    {
        UpdateFarmingSession(player, diff);
        return;
    }

    // Check if enough time passed since last check
    static std::unordered_map<uint32, uint32> lastCheckTimes;
    if (currentTime - lastCheckTimes[playerGuid] < FARMING_CHECK_INTERVAL)
        return;

    lastCheckTimes[playerGuid] = currentTime;

    // Check if any profession needs farming
    auto professionsNeedingFarm = GetProfessionsNeedingFarm(player);
    if (!professionsNeedingFarm.empty())
    {
        // Start farming session for highest priority profession
        ProfessionType profession = professionsNeedingFarm.front();
        TC_LOG_INFO("playerbots", "FarmingCoordinator: Player {} needs farming for profession {}",
            player->GetName(), static_cast<uint16>(profession));

        StartFarmingSession(player, profession, FarmingSessionType::SKILL_CATCHUP);
    }
}

void FarmingCoordinator::SetEnabled(::Player* player, bool enabled)
{
    if (!player)
        return;

    std::lock_guard lock(_mutex);
    uint32 playerGuid = player->GetGUID().GetCounter();
    if (_profiles.find(playerGuid) == _profiles.end())
        _profiles[playerGuid] = FarmingCoordinatorProfile();

    _profiles[playerGuid].autoFarm = enabled;
}

bool FarmingCoordinator::IsEnabled(::Player* player) const
{
    if (!player)
        return false;

    std::lock_guard lock(_mutex);
    uint32 playerGuid = player->GetGUID().GetCounter();
    auto it = _profiles.find(playerGuid);
    if (it == _profiles.end())
        return false;

    return it->second.autoFarm;
}

void FarmingCoordinator::SetCoordinatorProfile(uint32 playerGuid, FarmingCoordinatorProfile const& profile)
{
    std::lock_guard lock(_mutex);
    _profiles[playerGuid] = profile;
}

FarmingCoordinatorProfile FarmingCoordinator::GetCoordinatorProfile(uint32 playerGuid) const
{
    std::lock_guard lock(_mutex);

    auto it = _profiles.find(playerGuid);
    if (it != _profiles.end())
        return it->second;

    return FarmingCoordinatorProfile();
}

// ============================================================================
// SKILL ANALYSIS
// ============================================================================

bool FarmingCoordinator::NeedsFarming(::Player* player, ProfessionType profession) const
{
    if (!player)
        return false;

    int32 skillGap = GetSkillGap(player, profession);
    FarmingCoordinatorProfile const& profile = GetCoordinatorProfile(player->GetGUID().GetCounter());
    return skillGap > static_cast<int32>(profile.skillGapThreshold);
}

int32 FarmingCoordinator::GetSkillGap(::Player* player, ProfessionType profession) const
{
    if (!player)
        return 0;

    uint16 currentSkill = ProfessionManager::instance()->GetProfessionSkill(player, profession);
    uint16 targetSkill = GetTargetSkillLevel(player, profession);

    return static_cast<int32>(targetSkill) - static_cast<int32>(currentSkill);
}

uint16 FarmingCoordinator::GetTargetSkillLevel(::Player* player, ProfessionType profession) const
{
    if (!player)
        return 0;

    FarmingCoordinatorProfile const& profile = GetCoordinatorProfile(player->GetGUID().GetCounter());
    uint16 charLevel = player->GetLevel();
    // Target = Character Level × skillLevelMultiplier (default: 5.0)
    return static_cast<uint16>(charLevel * profile.skillLevelMultiplier);
}

std::vector<ProfessionType> FarmingCoordinator::GetProfessionsNeedingFarm(::Player* player) const
{
    std::vector<ProfessionType> professions;

    if (!player)
        return professions;

    // Get all player professions
    auto playerProfessions = ProfessionManager::instance()->GetPlayerProfessions(player);

    // Check each profession for skill gap
    for (auto const& profInfo : playerProfessions)
    {
        if (NeedsFarming(player, profInfo.profession))
        {
            int32 skillGap = GetSkillGap(player, profInfo.profession);
            professions.push_back(profInfo.profession);
        }
    }

    // Sort by skill gap (largest gap first)
    std::sort(professions.begin(), professions.end(),
        [this, player](ProfessionType a, ProfessionType b)
        {
            return GetSkillGap(player, a) > GetSkillGap(player, b);
        });

    return professions;
}

uint32 FarmingCoordinator::CalculateFarmingDuration(::Player* player, ProfessionType profession) const
{
    if (!player)
        return 0;

    int32 skillGap = GetSkillGap(player, profession);
    if (skillGap <= 0)
        return 0;

    FarmingCoordinatorProfile const& profile = GetCoordinatorProfile(player->GetGUID().GetCounter());
    // Estimate: ~10 skill points per 5 minutes of farming
    uint32 estimatedDuration = (skillGap / 10) * 300000; // 5 minutes in ms

    // Clamp to min/max
    estimatedDuration = std::max(estimatedDuration, profile.minFarmingDuration);
    estimatedDuration = std::min(estimatedDuration, profile.maxFarmingDuration);

    return estimatedDuration;
}

// ============================================================================
// FARMING SESSION MANAGEMENT
// ============================================================================

bool FarmingCoordinator::StartFarmingSession(::Player* player, ProfessionType profession, FarmingSessionType sessionType)
{
    if (!player || !CanStartFarming(player))
        return false;

    std::lock_guard lock(_mutex);
    uint32 playerGuid = player->GetGUID().GetCounter();
    // Check if session already active
    if (_activeSessions.find(playerGuid) != _activeSessions.end())
    {
        TC_LOG_DEBUG("playerbots", "FarmingCoordinator: Player {} already has active farming session", player->GetName());
        return false;
    }

    // Get optimal farming zone
    FarmingZoneInfo const* zone = GetOptimalFarmingZone(player, profession);
    if (!zone)
    {
        TC_LOG_ERROR("playerbots", "FarmingCoordinator: No suitable farming zone found for profession {}",
            static_cast<uint16>(profession));
        return false;
    }

    // Create farming session
    FarmingSession session;
    session.sessionId = GenerateSessionId();
    session.playerGuid = playerGuid;
    session.sessionType = sessionType;
    session.profession = profession;
    session.zone = *zone;
    session.startTime = GameTime::GetGameTimeMS();
    session.duration = CalculateFarmingDuration(player, profession);
    session.startingSkill = ProfessionManager::instance()->GetProfessionSkill(player, profession);
    session.targetSkill = GetTargetSkillLevel(player, profession);
    session.nodesGathered = 0;
    session.materialsCollected = 0;
    session.isActive = true;
    session.originalPosition = player->GetPosition();

    _activeSessions[playerGuid] = session;

    // Travel to farming zone
    if (!TravelToFarmingZone(player, *zone))
    {
        TC_LOG_ERROR("playerbots", "FarmingCoordinator: Failed to travel to farming zone");
        _activeSessions.erase(playerGuid);
        return false;
    }

    TC_LOG_INFO("playerbots", "FarmingCoordinator: Started farming session {} for player {} (skill {} -> {})",
        session.sessionId, player->GetName(), session.startingSkill, session.targetSkill);

    return true;
}

void FarmingCoordinator::StopFarmingSession(::Player* player)
{
    if (!player)
        return;

    std::lock_guard lock(_mutex);
    uint32 playerGuid = player->GetGUID().GetCounter();
    auto it = _activeSessions.find(playerGuid);
    if (it == _activeSessions.end())
        return;

    FarmingSession& session = it->second;
    session.isActive = false;

    // Update statistics
    _playerStatistics[playerGuid].sessionsCompleted++;
    _playerStatistics[playerGuid].totalTimeSpent += (GameTime::GetGameTimeMS() - session.startTime);
    _playerStatistics[playerGuid].totalNodesGathered += session.nodesGathered;
    _playerStatistics[playerGuid].zonesVisited++;

    _globalStatistics.sessionsCompleted++;
    _globalStatistics.totalTimeSpent += (GameTime::GetGameTimeMS() - session.startTime);
    _globalStatistics.totalNodesGathered += session.nodesGathered;

    // Return to original position
    FarmingCoordinatorProfile const& profile = GetCoordinatorProfile(playerGuid);
    if (profile.returnToOriginalPosition)
        ReturnToOriginalPosition(player, session);

    // Record last farming time for cooldown
    _lastFarmingTimes[playerGuid] = GameTime::GetGameTimeMS();
    TC_LOG_INFO("playerbots", "FarmingCoordinator: Stopped farming session {} for player {} (gathered {} nodes)",
        session.sessionId, player->GetName(), session.nodesGathered);

    _activeSessions.erase(playerGuid);
}

FarmingSession const* FarmingCoordinator::GetActiveFarmingSession(uint32 playerGuid) const
{
    std::lock_guard lock(_mutex);

    auto it = _activeSessions.find(playerGuid);
    if (it != _activeSessions.end())
        return &it->second;

    return nullptr;
}

bool FarmingCoordinator::HasActiveFarmingSession(::Player* player) const
{
    if (!player)
        return false;

    std::lock_guard lock(_mutex);
    return _activeSessions.find(player->GetGUID().GetCounter()) != _activeSessions.end();
}

void FarmingCoordinator::UpdateFarmingSession(::Player* player, uint32 diff)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    auto it = _activeSessions.find(playerGuid);
    if (it == _activeSessions.end())
        return;

    FarmingSession& session = it->second;

    // Check if session should end
    if (ShouldEndFarmingSession(player, session))
    {
        StopFarmingSession(player);
        return;
    }

    // Gathering is now handled by GatheringManager via BotAI::UpdateManagers()
    // The manager is updated automatically for all bots through the BehaviorManager system
    // No need to call it here

    // Update session progress (gather stats from GatheringAutomation)
    // In full implementation, track nodes gathered during this session
}
bool FarmingCoordinator::ShouldEndFarmingSession(::Player* player, FarmingSession const& session) const
{
    if (!player)
        return true;

    // Check if skill target reached
    uint16 currentSkill = ProfessionManager::instance()->GetProfessionSkill(player, session.profession);
    if (currentSkill >= session.targetSkill)
    {
        TC_LOG_INFO("playerbots", "FarmingCoordinator: Skill target reached ({} >= {})",
            currentSkill, session.targetSkill);
        return true;
    }

    // Check if duration exceeded
    uint32 sessionTime = GameTime::GetGameTimeMS() - session.startTime;
    if (sessionTime >= session.duration)
    {
        TC_LOG_INFO("playerbots", "FarmingCoordinator: Session duration exceeded ({} ms)", sessionTime);
        return true;
    }

    // Check if player is in combat, dead, etc.
    if (player->IsInCombat() || !player->IsAlive())
        return true;

    return false;
}

// ============================================================================
// ZONE SELECTION
// ============================================================================

FarmingZoneInfo const* FarmingCoordinator::GetOptimalFarmingZone(::Player* player, ProfessionType profession) const
{
    if (!player)
        return nullptr;

    auto zones = GetSuitableZones(player, profession);
    if (zones.empty())
        return nullptr;

    // Find best zone based on score
    FarmingZoneInfo const* bestZone = nullptr;
    float bestScore = 0.0f;

    for (auto const& zone : zones)
    {
        float score = CalculateZoneScore(player, zone);
        if (score > bestScore)
        {
            bestScore = score;
            bestZone = &zone;
        }
    }

    std::lock_guard lock(_mutex);
    return bestZone;
}

std::vector<FarmingZoneInfo> FarmingCoordinator::GetSuitableZones(::Player* player, ProfessionType profession) const
{
    std::vector<FarmingZoneInfo> suitable;

    if (!player)
        return suitable;

    std::lock_guard lock(_mutex);

    auto it = _farmingZones.find(profession);
    if (it == _farmingZones.end())
        return suitable;

    uint16 skillLevel = ProfessionManager::instance()->GetProfessionSkill(player, profession);
    uint8 charLevel = player->GetLevel();
    // Filter zones by skill level and character level
    for (auto const& zone : it->second)
    {
        if (skillLevel >= zone.minSkillLevel && skillLevel <= zone.maxSkillLevel)
        {
            // Prefer zones near character level
            if (charLevel >= zone.recommendedCharLevel - 5 && charLevel <= zone.recommendedCharLevel + 10)
                suitable.push_back(zone);
        }
    }

    return suitable;
}

float FarmingCoordinator::CalculateZoneScore(::Player* player, FarmingZoneInfo const& zone) const
{
    if (!player)
        return 0.0f;

    float score = 100.0f;

    // Distance penalty (closer is better)
    float distance = player->GetDistance(zone.centerPosition);
    float distancePenalty = distance / 1000.0f; // Penalty per 1000 yards
    score -= distancePenalty * 10.0f;

    // Level match bonus
    uint8 charLevel = player->GetLevel();
    int8 levelDiff = std::abs(static_cast<int8>(charLevel) - static_cast<int8>(zone.recommendedCharLevel));
    if (levelDiff == 0)
        score += 20.0f;
    else
        score -= levelDiff * 2.0f;

    // PvP zone penalty
    if (zone.isContested)
        score -= 15.0f;

    return std::max(score, 0.0f);
}

// ============================================================================
// MATERIAL MANAGEMENT
// ============================================================================

bool FarmingCoordinator::HasReachedStockpileTarget(::Player* player, uint32 itemId) const
{
    uint32 currentCount = GetMaterialCount(player, itemId);
    FarmingCoordinatorProfile const& profile = GetCoordinatorProfile(player->GetGUID().GetCounter());
    return currentCount >= profile.materialStockpileTarget;
}

uint32 FarmingCoordinator::GetMaterialCount(::Player* player, uint32 itemId) const
{
    if (!player)
        return 0;

    return player->GetItemCount(itemId);
}

std::vector<std::pair<uint32, uint32>> FarmingCoordinator::GetNeededMaterials(::Player* player, ProfessionType profession) const
{
    std::vector<std::pair<uint32, uint32>> materials;

    if (!player)
        return materials;

    // In full implementation, query auction house targets from ProfessionAuctionBridge
    // For now, return empty list

    return materials;
}

// ============================================================================
// STATISTICS
// ============================================================================

FarmingStatistics const& FarmingCoordinator::GetPlayerStatistics(uint32 playerGuid) const
{
    std::lock_guard lock(_mutex);

    static FarmingStatistics emptyStats;
    auto it = _playerStatistics.find(playerGuid);
    if (it != _playerStatistics.end())
        return it->second;

    return emptyStats;
}

FarmingStatistics const& FarmingCoordinator::GetGlobalStatistics() const
{
    return _globalStatistics;
}

void FarmingCoordinator::ResetStatistics(uint32 playerGuid)
{
    std::lock_guard lock(_mutex);

    auto it = _playerStatistics.find(playerGuid);
    if (it != _playerStatistics.end())
        it->second.Reset();
}

// ============================================================================
// INITIALIZATION HELPERS
// ============================================================================

void FarmingCoordinator::LoadFarmingZones()
{
    InitializeMiningZones();
    InitializeHerbalismZones();
    InitializeSkinningZones();
}

void FarmingCoordinator::InitializeZoneDatabase()
{
    // Zones are initialized in profession-specific methods
    TC_LOG_INFO("playerbots", "FarmingCoordinator: Zone database initialization complete");
}

void FarmingCoordinator::InitializeMiningZones()
{
    std::vector<FarmingZoneInfo> miningZones;
    // Elwynn Forest (Alliance - Copper 1-75)
    FarmingZoneInfo elwynn;
    elwynn.zoneId = 12;
    elwynn.zoneName = "Elwynn Forest";
    elwynn.minSkillLevel = 1;
    elwynn.maxSkillLevel = 75;
    elwynn.profession = ProfessionType::MINING;
    elwynn.centerPosition.Relocate(-9449.0f, -1366.0f, 47.0f, 0.0f);
    elwynn.zoneRadius = 1000.0f;
    elwynn.recommendedCharLevel = 5;
    elwynn.isContested = false;
    miningZones.push_back(elwynn);
    // Westfall (Alliance - Copper/Tin 50-125)
    FarmingZoneInfo westfall;
    westfall.zoneId = 40;
    westfall.zoneName = "Westfall";
    westfall.minSkillLevel = 50;
    westfall.maxSkillLevel = 125;
    westfall.profession = ProfessionType::MINING;
    westfall.centerPosition.Relocate(-10684.0f, 1033.0f, 34.0f, 0.0f);
    westfall.zoneRadius = 1200.0f;
    westfall.recommendedCharLevel = 15;
    westfall.isContested = false;
    miningZones.push_back(westfall);

    _farmingZones[ProfessionType::MINING] = miningZones;
}

void FarmingCoordinator::InitializeHerbalismZones()
{
    std::vector<FarmingZoneInfo> herbalismZones;

    // Elwynn Forest (Alliance - Peacebloom/Silverleaf 1-75)
    FarmingZoneInfo elwynn;
    elwynn.zoneId = 12;
    elwynn.zoneName = "Elwynn Forest";
    elwynn.minSkillLevel = 1;
    elwynn.maxSkillLevel = 75;
    elwynn.profession = ProfessionType::HERBALISM;
    elwynn.centerPosition.Relocate(-9449.0f, -1366.0f, 47.0f, 0.0f);
    elwynn.zoneRadius = 1000.0f;
    elwynn.recommendedCharLevel = 5;
    elwynn.isContested = false;
    herbalismZones.push_back(elwynn);

    _farmingZones[ProfessionType::HERBALISM] = herbalismZones;
}

void FarmingCoordinator::InitializeSkinningZones()
{
    // Skinning works in any zone with beasts - no specific zone database needed
    TC_LOG_DEBUG("playerbots", "FarmingCoordinator: Skinning works in all zones with creatures");
}

// ============================================================================
// FARMING HELPERS
// ============================================================================

uint32 FarmingCoordinator::GenerateSessionId()
{
    return _nextSessionId.fetch_add(1);
}

bool FarmingCoordinator::TravelToFarmingZone(::Player* player, FarmingZoneInfo const& zone)
{
    if (!player)
        return false;

    // Simple movement to zone center
    // In full implementation, use proper pathfinding or teleport
    player->TeleportTo(player->GetMapId(), zone.centerPosition.GetPositionX(),
        zone.centerPosition.GetPositionY(), zone.centerPosition.GetPositionZ(),
        zone.centerPosition.GetOrientation());

    return true;
}

void FarmingCoordinator::ReturnToOriginalPosition(::Player* player, FarmingSession const& session)
{
    if (!player)
        return;

    player->TeleportTo(player->GetMapId(), session.originalPosition.GetPositionX(),
        session.originalPosition.GetPositionY(), session.originalPosition.GetPositionZ(),
        session.originalPosition.GetOrientation());
}

bool FarmingCoordinator::CanStartFarming(::Player* player) const
{
    if (!player)
        return false;

    // Check if in combat
    if (player->IsInCombat())
        return false;

    // Check if alive
    if (!player->IsAlive())
        return false;

    // Check if in group (optional - may want to farm while grouped)
    // if (player->GetGroup())
    //     return false;

    // Check farming cooldown
    std::lock_guard lock(_mutex);
    uint32 playerGuid = player->GetGUID().GetCounter();
    auto it = _lastFarmingTimes.find(playerGuid);
    if (it != _lastFarmingTimes.end())
    {
        FarmingCoordinatorProfile const& profile = GetCoordinatorProfile(playerGuid);
        uint32 timeSinceLastFarm = GameTime::GetGameTimeMS() - it->second;

        if (timeSinceLastFarm < profile.farmingCooldown)
            return false;
    }

    return true;
}

bool FarmingCoordinator::ValidateFarmingSession(::Player* player, FarmingSession const& session) const
{
    if (!player || !session.isActive)
        return false;

    // Validate player still has the profession
    if (!ProfessionManager::instance()->HasProfession(player, session.profession))
        return false;

    return true;
}

} // namespace Playerbot
