/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "MountManager.h"
#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "World.h"
#include "Map.h"
#include "WorldSession.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Vehicle.h"
#include <algorithm>
#include <cmath>

namespace Playerbot
{

// ============================================================================
// Singleton Instance Management
// ============================================================================

MountManager* MountManager::instance()
{
    static MountManager instance;
    return &instance;
}

MountManager::MountManager()
{
    TC_LOG_INFO("server.loading", "Initializing MountManager system...");
    _globalMetrics.Reset();
}

// ============================================================================
// Core Mount Management
// ============================================================================

void MountManager::Initialize()
{
    // No lock needed - mount data is per-bot instance data

    TC_LOG_INFO("server.loading", "Loading mount database...");
    LoadMountDatabase();

    TC_LOG_INFO("server.loading", "MountManager initialized with {} mounts", _mountDatabase.size());
}

void MountManager::Update(::Player* player, uint32 diff)
{
    if (!player || !player->IsInWorld())
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    // Throttle updates
    auto lastUpdateItr = _lastUpdateTimes.find(playerGuid);
    if (lastUpdateItr != _lastUpdateTimes.end())
    {
        if (GameTime::GetGameTimeMS() - lastUpdateItr->second < MOUNT_UPDATE_INTERVAL)
            return;
    }
    _lastUpdateTimes[playerGuid] = GameTime::GetGameTimeMS();

    // Get automation profile
    MountAutomationProfile profile = GetAutomationProfile(playerGuid);

    if (!profile.autoMount)
        return;

    // Check if player should dismount (combat, indoors, etc.)
    if (IsMounted(player))
    {
        if (profile.dismountInCombat && IsInCombat(player))
        {
            DismountPlayer(player);
            return;
        }

        // Update mounted time tracking
        auto timestampItr = _mountTimestamps.find(playerGuid);
        if (timestampItr != _mountTimestamps.end())
        {
            uint64 mountedTime = GameTime::GetGameTimeMS() - timestampItr->second;
            _playerMetrics[playerGuid].totalMountedTime += diff;
            _globalMetrics.totalMountedTime += diff;
        }
    }
    else
    {
        // Check if player should remount after combat
        if (profile.remountAfterCombat && !IsInCombat(player))
        {
            // Check if player was recently in combat
            // This would require tracking combat exit time
            // For now, we rely on destination-based mounting
        }
    }
}

bool MountManager::MountPlayer(::Player* player)
{
    if (!player || !player->IsInWorld())
        return false;

    if (IsMounted(player))
        return true; // Already mounted

    if (!ValidateMountUsage(player))
        return false;

    MountInfo const* mount = GetBestMount(player);
    if (!mount)
    {
        TC_LOG_DEBUG("module.playerbot", "MountManager::MountPlayer - No suitable mount found for player {}",
            player->GetName());
        return false;
    }

    if (!CastMountSpell(player, mount->spellId))
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();
    // Track mount usage
    _activeMounts[playerGuid] = mount->spellId;
    _mountTimestamps[playerGuid] = GameTime::GetGameTimeMS();
    // Update metrics
    _playerMetrics[playerGuid].timesMounted++;
    _globalMetrics.timesMounted++;

    if (mount->isFlyingMount)
    {
        _playerMetrics[playerGuid].flyingMountUsage++;
        _globalMetrics.flyingMountUsage++;
    }

    if (mount->isDragonridingMount)
    {
        _playerMetrics[playerGuid].dragonridingUsage++;
        _globalMetrics.dragonridingUsage++;
    }

    TC_LOG_DEBUG("module.playerbot", "MountManager::MountPlayer - Player {} mounted on {} (spell {})",
        player->GetName(), mount->name, mount->spellId);
    return true;
}

bool MountManager::DismountPlayer(::Player* player)
{
    if (!player || !player->IsInWorld())
        return false;

    if (!IsMounted(player))
        return false;

    player->RemoveAurasByType(SPELL_AURA_MOUNTED);

    uint32 playerGuid = player->GetGUID().GetCounter();
    // Update metrics
    _playerMetrics[playerGuid].timesDismounted++;
    _globalMetrics.timesDismounted++;

    // Clear mount tracking
    _activeMounts.erase(playerGuid);
    _mountTimestamps.erase(playerGuid);

    TC_LOG_DEBUG("module.playerbot", "MountManager::DismountPlayer - Player {} dismounted",
        player->GetName());

    return true;
}

bool MountManager::IsMounted(::Player* player) const
{
    if (!player)
        return false;

    return player->IsMounted();
}

bool MountManager::ShouldAutoMount(::Player* player, Position const& destination) const
{
    if (!player)
        return false;

    MountAutomationProfile profile = GetAutomationProfile(player->GetGUID().GetCounter());
    if (!profile.autoMount)
        return false;

    // Calculate distance to destination
    float distance = player->GetExactDist(&destination);

    return distance >= profile.minDistanceForMount;
}

// ============================================================================
// Mount Selection
// ============================================================================

MountInfo const* MountManager::GetBestMount(::Player* player) const
{
    if (!player)
        return nullptr;

    // No lock needed - mount data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    auto playerMountsItr = _playerMounts.find(playerGuid);

    if (playerMountsItr == _playerMounts.end() || playerMountsItr->second.empty())
        return nullptr;

    MountAutomationProfile profile = GetAutomationProfile(playerGuid);

    // Priority 1: Dragonriding (if enabled and available)
    if (profile.useDragonriding && CanUseDragonriding(player))
    {
        MountInfo const* dragonMount = GetDragonridingMount(player);
        if (dragonMount)
            return dragonMount;
    }

    // Priority 2: Flying mount (if zone allows)
    if (profile.preferFlyingMount && CanUseFlyingMount(player))
    {
        MountInfo const* flyingMount = GetFlyingMount(player);
        if (flyingMount)
            return flyingMount;
    }

    // Priority 3: Aquatic mount (if underwater)
    if (IsPlayerUnderwater(player))
    {
        MountInfo const* aquaticMount = GetAquaticMount(player);
        if (aquaticMount)
            return aquaticMount;
    }

    // Priority 4: Ground mount (fallback)
    return GetGroundMount(player);
}

MountInfo const* MountManager::GetFlyingMount(::Player* player) const
{
    if (!player)
        return nullptr;

    // No lock needed - mount data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    auto playerMountsItr = _playerMounts.find(playerGuid);

    if (playerMountsItr == _playerMounts.end())
        return nullptr;

    MountSpeed maxSpeed = GetMaxMountSpeed(player);

    // Find fastest flying mount player knows
    MountInfo const* bestMount = nullptr;

    for (uint32 spellId : playerMountsItr->second)
    {
        auto mountItr = _mountDatabase.find(spellId);
        if (mountItr == _mountDatabase.end())
            continue;

        MountInfo const& mount = mountItr->second;

        if (!mount.isFlyingMount)
            continue;

        if (!CanUseMount(player, mount))
            continue;

        if (mount.speed > maxSpeed)
            continue;

        if (!bestMount || mount.speed > bestMount->speed)
            bestMount = &mount;
    }

    return bestMount;
}

MountInfo const* MountManager::GetGroundMount(::Player* player) const
{
    if (!player)
        return nullptr;

    // No lock needed - mount data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    auto playerMountsItr = _playerMounts.find(playerGuid);
    if (playerMountsItr == _playerMounts.end())
        return nullptr;

    MountSpeed maxSpeed = GetMaxMountSpeed(player);
    MountAutomationProfile profile = GetAutomationProfile(playerGuid);

    // Check for preferred mounts first
    if (!profile.preferredMounts.empty())
    {
        for (uint32 preferredSpellId : profile.preferredMounts)
        {
            if (playerMountsItr->second.find(preferredSpellId) != playerMountsItr->second.end())
            {
                auto mountItr = _mountDatabase.find(preferredSpellId);
                if (mountItr != _mountDatabase.end())
                {
                    MountInfo const& mount = mountItr->second;
                    if (mount.type == MountType::GROUND && CanUseMount(player, mount))
                        return &mount;
                }
            }
        }
    }

    // Find fastest ground mount
    MountInfo const* bestMount = nullptr;

    for (uint32 spellId : playerMountsItr->second)
    {
        auto mountItr = _mountDatabase.find(spellId);
        if (mountItr == _mountDatabase.end())
            continue;

        MountInfo const& mount = mountItr->second;

        if (mount.type != MountType::GROUND)
            continue;

        if (!CanUseMount(player, mount))
            continue;

        if (mount.speed > maxSpeed)
            continue;

        if (!bestMount || mount.speed > bestMount->speed)
            bestMount = &mount;
    }

    return bestMount;
}

MountInfo const* MountManager::GetAquaticMount(::Player* player) const
{
    if (!player)
        return nullptr;

    // No lock needed - mount data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    auto playerMountsItr = _playerMounts.find(playerGuid);

    if (playerMountsItr == _playerMounts.end())
        return nullptr;

    for (uint32 spellId : playerMountsItr->second)
    {
        auto mountItr = _mountDatabase.find(spellId);
        if (mountItr == _mountDatabase.end())
            continue;

        MountInfo const& mount = mountItr->second;

        if (!mount.isAquaticMount)
            continue;

        if (CanUseMount(player, mount))
            return &mount;
    }

    return nullptr;
}

MountInfo const* MountManager::GetDragonridingMount(::Player* player) const
{
    if (!player)
        return nullptr;

    // No lock needed - mount data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    auto playerMountsItr = _playerMounts.find(playerGuid);
    if (playerMountsItr == _playerMounts.end())
        return nullptr;

    for (uint32 spellId : playerMountsItr->second)
    {
        auto mountItr = _mountDatabase.find(spellId);
        if (mountItr == _mountDatabase.end())
            continue;

        MountInfo const& mount = mountItr->second;

        if (!mount.isDragonridingMount)
            continue;

        if (CanUseMount(player, mount))
            return &mount;
    }

    return nullptr;
}

bool MountManager::CanUseFlyingMount(::Player* player) const
{
    if (!player)
        return false;

    // Check if player has flying skill
    if (!HasRidingSkill(player))
        return false;

    uint32 ridingSkill = GetRidingSkill(player);
    if (ridingSkill < 150) // Requires expert riding (150)
        return false;

    // Check if zone allows flying
    return !IsInNoFlyZone(player);
}

bool MountManager::IsPlayerUnderwater(::Player* player) const
{
    if (!player)
        return false;

    return player->IsUnderWater();
}

bool MountManager::CanUseDragonriding(::Player* player) const
{
    if (!player)
        return false;

    // Check if player is in dragonriding zone
    return IsInDragonridingZone(player);
}

// ============================================================================
// Mount Collection
// ============================================================================

std::vector<MountInfo> MountManager::GetPlayerMounts(::Player* player) const
{
    std::vector<MountInfo> mounts;

    if (!player)
        return mounts;

    // No lock needed - mount data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    auto playerMountsItr = _playerMounts.find(playerGuid);

    if (playerMountsItr == _playerMounts.end())
        return mounts;

    for (uint32 spellId : playerMountsItr->second)
    {
        auto mountItr = _mountDatabase.find(spellId);
        if (mountItr != _mountDatabase.end())
            mounts.push_back(mountItr->second);
    }

    return mounts;
}

bool MountManager::KnowsMount(::Player* player, uint32 spellId) const
{
    if (!player)
        return false;

    return player->HasSpell(spellId);
}

bool MountManager::LearnMount(::Player* player, uint32 spellId)
{
    if (!player)
        return false;

    // No lock needed - mount data is per-bot instance data

    auto mountItr = _mountDatabase.find(spellId);
    if (mountItr == _mountDatabase.end())
    {
        TC_LOG_ERROR("module.playerbot", "MountManager::LearnMount - Unknown mount spell ID {}", spellId);
        return false;
    }

    if (!CanUseMount(player, mountItr->second))
    {
        TC_LOG_DEBUG("module.playerbot", "MountManager::LearnMount - Player {} cannot use mount {}",
            player->GetName(), spellId);
        return false;
    }
    if (player->HasSpell(spellId))
        return true; // Already knows
    player->LearnSpell(spellId, false);

    uint32 playerGuid = player->GetGUID().GetCounter();
    _playerMounts[playerGuid].insert(spellId);

    _playerMetrics[playerGuid].mountsLearned++;
    _globalMetrics.mountsLearned++;

    TC_LOG_INFO("module.playerbot", "MountManager::LearnMount - Player {} learned mount {} ({})",
        player->GetName(), mountItr->second.name, spellId);

    return true;
}

uint32 MountManager::GetMountCount(::Player* player) const
{
    if (!player)
        return 0;

    // No lock needed - mount data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    auto playerMountsItr = _playerMounts.find(playerGuid);

    if (playerMountsItr == _playerMounts.end())
        return 0;

    return static_cast<uint32>(playerMountsItr->second.size());
}

bool MountManager::CanUseMount(::Player* player, MountInfo const& mount) const
{
    if (!player)
        return false;

    // Check level requirement
    if (player->GetLevel() < mount.requiredLevel)
        return false;

    // Check riding skill requirement
    uint32 ridingSkill = GetRidingSkill(player);
    if (ridingSkill < mount.requiredSkill)
        return false;

    // Check zone restrictions
    if (!mount.zoneRestrictions.empty())
    {
        uint32 currentZone = GetCurrentZoneId(player);
        bool allowed = false;
        for (uint32 allowedZone : mount.zoneRestrictions)
        {
            if (currentZone == allowedZone)
            {
                allowed = true;
                break;
            }
        }
        if (!allowed)
            return false;
    }

    return true;
}

// ============================================================================
// Riding Skill
// ============================================================================

uint32 MountManager::GetRidingSkill(::Player* player) const
{
    if (!player)
        return 0;

    // Check for riding skill spells
    if (player->HasSpell(SPELL_MOUNT_RIDING_MASTER))
        return 300;
    if (player->HasSpell(SPELL_MOUNT_RIDING_ARTISAN))
        return 225;
    if (player->HasSpell(SPELL_MOUNT_RIDING_EXPERT))
        return 150;
    if (player->HasSpell(SPELL_MOUNT_RIDING_JOURNEYMAN))
        return 75;
    if (player->HasSpell(SPELL_MOUNT_RIDING_APPRENTICE))
        return 75;

    return 0;
}

bool MountManager::HasRidingSkill(::Player* player) const
{
    return GetRidingSkill(player) > 0;
}

bool MountManager::LearnRidingSkill(::Player* player, uint32 skillLevel)
{
    if (!player)
        return false;

    uint32 spellId = 0;

    switch (skillLevel)
    {
        case 75:
            spellId = SPELL_MOUNT_RIDING_APPRENTICE;
            break;
        case 150:
            spellId = SPELL_MOUNT_RIDING_EXPERT;
            break;
        case 225:
            spellId = SPELL_MOUNT_RIDING_ARTISAN;
            break;
        case 300:
            spellId = SPELL_MOUNT_RIDING_MASTER;
            break;
        default:
            TC_LOG_ERROR("module.playerbot", "MountManager::LearnRidingSkill - Invalid skill level {}", skillLevel);
            return false;
    }
    if (player->HasSpell(spellId))
        return true; // Already knows
    player->LearnSpell(spellId, false);

    TC_LOG_INFO("module.playerbot", "MountManager::LearnRidingSkill - Player {} learned riding skill {}",
        player->GetName(), skillLevel);

    return true;
}

MountSpeed MountManager::GetMaxMountSpeed(::Player* player) const
{
    uint32 ridingSkill = GetRidingSkill(player);

    if (ridingSkill >= 300)
        return MountSpeed::EPIC_PLUS;
    if (ridingSkill >= 225)
        return MountSpeed::EPIC;
    if (ridingSkill >= 150)
        return MountSpeed::FAST;
    if (ridingSkill >= 75)
        return MountSpeed::NORMAL;

    return MountSpeed::SLOW;
}

// ============================================================================
// Multi-Passenger Mounts
// ============================================================================

bool MountManager::IsMultiPassengerMount(MountInfo const& mount) const
{
    return mount.isMultiPassenger && mount.passengerCount > 1;
}

uint32 MountManager::GetAvailablePassengerSeats(::Player* player) const
{
    if (!player || !IsMounted(player))
        return 0;

    Vehicle* vehicle = player->GetVehicleKit();
    if (!vehicle)
        return 0;

    uint32 totalSeats = vehicle->GetAvailableSeatCount();
    uint32 occupiedSeats = 1; // Driver

    // Count passengers
    for (auto const& [seatId, seat] : vehicle->Seats)
    {
        if (seat.Passenger)
            occupiedSeats++;
    }

    return totalSeats > occupiedSeats ? (totalSeats - occupiedSeats) : 0;
}

bool MountManager::AddPassenger(::Player* mountedPlayer, ::Player* passenger)
{
    if (!mountedPlayer || !passenger)
        return false;

    if (!IsMounted(mountedPlayer))
        return false;

    Vehicle* vehicle = mountedPlayer->GetVehicleKit();
    if (!vehicle)
        return false;

    // Find empty seat
    for (auto const& [seatId, seat] : vehicle->Seats)
    {
        if (!seat.Passenger)
        {
            passenger->EnterVehicle(vehicle, seatId);
            return true;
        }
    }

    return false;
}

bool MountManager::RemovePassenger(::Player* passenger)
{
    if (!passenger)
        return false;

    Vehicle* vehicle = passenger->GetVehicle();
    if (!vehicle)
        return false;

    passenger->ExitVehicle();
    return true;
}

// ============================================================================
// Automation Profiles
// ============================================================================

void MountManager::SetAutomationProfile(uint32 playerGuid, MountAutomationProfile const& profile)
{
    // No lock needed - mount data is per-bot instance data
    _playerProfiles[playerGuid] = profile;
}

MountAutomationProfile MountManager::GetAutomationProfile(uint32 playerGuid) const
{
    // No lock needed - mount data is per-bot instance data

    auto profileItr = _playerProfiles.find(playerGuid);
    if (profileItr != _playerProfiles.end())
        return profileItr->second;

    // Return default profile
    return MountAutomationProfile();
}

// ============================================================================
// Metrics
// ============================================================================

MountManager::MountMetrics const& MountManager::GetPlayerMetrics(uint32 playerGuid) const
{
    // No lock needed - mount data is per-bot instance data

    auto metricsItr = _playerMetrics.find(playerGuid);
    if (metricsItr != _playerMetrics.end())
        return metricsItr->second;

    static MountMetrics emptyMetrics;
    return emptyMetrics;
}

MountManager::MountMetrics const& MountManager::GetGlobalMetrics() const
{
    return _globalMetrics;
}

// ============================================================================
// Initialization Helpers
// ============================================================================

void MountManager::LoadMountDatabase()
{
    // Clear existing database
    _mountDatabase.clear();

    // Load mounts from all expansions
    InitializeVanillaMounts();
    InitializeTBCMounts();
    InitializeWrathMounts();
    InitializeCataclysmMounts();
    InitializePandariaMounts();
    InitializeDraenorMounts();
    InitializeLegionMounts();
    InitializeBfAMounts();
    InitializeShadowlandsMounts();
    InitializeDragonflightMounts();
    InitializeWarWithinMounts();

    TC_LOG_INFO("server.loading", "MountManager::LoadMountDatabase - Loaded {} mount entries", _mountDatabase.size());
}
void MountManager::InitializeVanillaMounts()
{
    // Classic WoW ground mounts (60% and 100% speed)
    // This is a stub implementation - full mount database would load from DBC/DB2
    // Example: Brown Horse (Human racial mount)
    {
        MountInfo mount;
        mount.spellId = 458; // Brown Horse spell ID
        mount.displayId = 2404;
        mount.name = "Brown Horse";
        mount.type = MountType::GROUND;
        mount.speed = MountSpeed::SLOW;
        mount.requiredLevel = 20;
        mount.requiredSkill = 75;
        mount.isFlyingMount = false;
        mount.isAquaticMount = false;
        _mountDatabase[mount.spellId] = mount;
    }

    // Add more vanilla mounts here (60+ mounts in vanilla)
    // In a complete implementation, this would load from spell.dbc and mount.db2
}

void MountManager::InitializeTBCMounts()
{
    // TBC introduced flying mounts (150% and 280% speed)

    // Example: Swift Blue Gryphon (60% flying)
    {
        MountInfo mount;
        mount.spellId = 32242;
        mount.displayId = 17759;
        mount.name = "Swift Blue Gryphon";
        mount.type = MountType::FLYING;
        mount.speed = MountSpeed::FAST;
        mount.requiredLevel = 60;
        mount.requiredSkill = 150;
        mount.isFlyingMount = true;
        mount.isAquaticMount = false;
        _mountDatabase[mount.spellId] = mount;
    }

    // Add more TBC mounts here
}

void MountManager::InitializeWrathMounts()
{
    // Wrath mounts including 310% speed mounts
    // Stub implementation
}

void MountManager::InitializeCataclysmMounts()
{
    // Cataclysm mounts
    // Stub implementation
}

void MountManager::InitializePandariaMounts()
{
    // Mists of Pandaria mounts
    // Stub implementation
}

void MountManager::InitializeDraenorMounts()
{
    // Warlords of Draenor mounts
    // Stub implementation
}

void MountManager::InitializeLegionMounts()
{
    // Legion mounts
    // Stub implementation
}

void MountManager::InitializeBfAMounts()
{
    // Battle for Azeroth mounts
    // Stub implementation
}

void MountManager::InitializeShadowlandsMounts()
{
    // Shadowlands mounts
    // Stub implementation
}

void MountManager::InitializeDragonflightMounts()
{
    // Dragonflight mounts including dragonriding
    // Stub implementation
}

void MountManager::InitializeWarWithinMounts()
{
    // The War Within mounts (WoW 11.2)
    // Stub implementation
}

// ============================================================================
// Mount Casting Helpers
// ============================================================================

bool MountManager::CastMountSpell(::Player* player, uint32 spellId)
{
    if (!player)
        return false;

    if (!CanCastMountSpell(player, spellId))
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);

    // Cast mount spell
    player->CastSpell(player, spellId, false);

    HandleMountCastResult(player, spellId, true);

    return true;
}

bool MountManager::CanCastMountSpell(::Player* player, uint32 spellId) const
{
    if (!player)
        return false;

    if (!player->HasSpell(spellId))
        return false;

    if (player->IsMounted())
        return false;

    if (IsInCombat(player))
        return false;

    if (IsIndoors(player))
        return false;

    return true;
}

void MountManager::HandleMountCastResult(::Player* player, uint32 spellId, bool success)
{
    if (!player)
        return;

    if (success)
    {
        TC_LOG_DEBUG("module.playerbot", "MountManager::HandleMountCastResult - Player {} successfully cast mount spell {}",
            player->GetName(), spellId);
    }
    else
    {
        TC_LOG_WARN("module.playerbot", "MountManager::HandleMountCastResult - Player {} failed to cast mount spell {}",
            player->GetName(), spellId);
    }
}

// ============================================================================
// Zone Detection Helpers
// ============================================================================

bool MountManager::IsInNoFlyZone(::Player* player) const
{
    if (!player)
        return true;

    Map* map = player->GetMap();
    if (!map)
        return true;

    // Check if flying is disabled in this map
    return !map->IsFlyingAllowed();
}

bool MountManager::IsInDragonridingZone(::Player* player) const
{
    if (!player)
        return false;

    // Dragonriding is available in Dragon Isles zones (Dragonflight expansion)
    // This would check specific zone IDs
    uint32 zoneId = GetCurrentZoneId(player);

    // Example Dragon Isles zones
    std::vector<uint32> dragonIslesZones = {
        13644, // The Waking Shores
        13645, // Ohn'ahran Plains
        13646, // The Azure Span
        13647  // Thaldraszus
    };

    for (uint32 dragonZone : dragonIslesZones)
    {
        if (zoneId == dragonZone)
            return true;
    }

    return false;
}

bool MountManager::IsInAquaticZone(::Player* player) const
{
    if (!player)
        return false;

    return IsPlayerUnderwater(player);
}

uint32 MountManager::GetCurrentZoneId(::Player* player) const
{
    if (!player)
        return 0;

    return player->GetZoneId();
}

// ============================================================================
// Validation Helpers
// ============================================================================

bool MountManager::ValidateMountUsage(::Player* player) const
{
    if (!player)
        return false;

    if (IsInCombat(player))
        return false;

    if (IsIndoors(player))
        return false;

    if (IsInInstance(player))
    {
        // Some instances allow mounts, others don't
        // This would check instance-specific mount permissions
        return false;
    }

    return true;
}

bool MountManager::IsInCombat(::Player* player) const
{
    if (!player)
        return false;

    return player->IsInCombat();
}

bool MountManager::IsIndoors(::Player* player) const
{
    if (!player)
        return true;

    return !player->IsOutdoors();
}

bool MountManager::IsInInstance(::Player* player) const
{
    if (!player)
        return false;

    Map* map = player->GetMap();
    if (!map)
        return false;

    return map->IsDungeon() || map->IsRaid() || map->IsBattleground() || map->IsArena();
}

} // namespace Playerbot
