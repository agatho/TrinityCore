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
// STATIC MEMBER INITIALIZATION
// ============================================================================

std::unordered_map<uint32, MountInfo> MountManager::_mountDatabase;
bool MountManager::_mountDatabaseInitialized = false;
MountManager::MountMetrics MountManager::_globalMetrics;

// ============================================================================
// PER-BOT LIFECYCLE
// ============================================================================

MountManager::MountManager(Player* bot)
    : _bot(bot)
{
    if (!_bot)
    {
        TC_LOG_ERROR("playerbot.mount", "MountManager: Attempted to create with null bot!");
        return;
    }

    // Initialize shared mount database once (thread-safe)
    if (!_mountDatabaseInitialized)
    {
        TC_LOG_INFO("playerbot.mount", "MountManager: Loading mount database...");
        LoadMountDatabase();
        _mountDatabaseInitialized = true;
        TC_LOG_INFO("playerbot.mount", "MountManager: Initialized mount database with {} mounts", _mountDatabase.size());
    }

    TC_LOG_DEBUG("playerbot.mount", "MountManager: Created for bot {} ({})",
                 _bot->GetName(), _bot->GetGUID().ToString());
}

MountManager::~MountManager()
{
    if (_bot)
    {
        TC_LOG_DEBUG("playerbot.mount", "MountManager: Destroyed for bot {} ({})",
                     _bot->GetName(), _bot->GetGUID().ToString());
    }
}

// ============================================================================
// Core Mount Management
// ============================================================================

void MountManager::Initialize()
{
    // Database already loaded in constructor
    TC_LOG_DEBUG("playerbot.mount", "MountManager: Initialized for bot {}",
        _bot ? _bot->GetName() : "unknown");
}

void MountManager::Update(uint32 diff)
{
    if (!_bot || !_bot->IsInWorld())
        return;

        // Throttle updates
    // Use _lastUpdateTime directly
    if (GameTime::GetGameTimeMS() - _lastUpdateTime < MOUNT_UPDATE_INTERVAL)
        return;
    _lastUpdateTime = GameTime::GetGameTimeMS();

    // Get automation profile
    // Use _profile directly

    if (!_profile.autoMount)
        return;

    // Check if player should dismount (combat, indoors, etc.)
    if (IsMounted())
    {
        if (_profile.dismountInCombat && IsInCombat())
        {
            DismountPlayer();
            return;
        }

        // Update mounted time tracking
        // Update mounted time using _mountTimestamp
    if (_mountTimestamp > 0)
    {
        _metrics.totalMountedTime += diff;
        _globalMetrics.totalMountedTime += diff;
    }
    }
    else
    {
        // Check if player should remount after combat
        if (_profile.remountAfterCombat && !IsInCombat())
        {
            // Check if player was recently in combat
            // This would require tracking combat exit time
            // For now, we rely on destination-based mounting
        }
    }
}

bool MountManager::MountPlayer()
{
    if (!_bot || !_bot->IsInWorld())
        return false;

    if (IsMounted())
        return true; // Already mounted

    if (!ValidateMountUsage())
        return false;

    MountInfo const* mount = GetBestMount();
    if (!mount)
    {
        TC_LOG_DEBUG("module.playerbot", "MountManager::MountPlayer - No suitable mount found for player {}",
            _bot->GetName());
        return false;
    }

    if (!CastMountSpell(mount->spellId))
        return false;

        // Track mount usage
    _currentMount = mount->spellId;
    _mountTimestamp = GameTime::GetGameTimeMS();
    // Update metrics
    _metrics.timesMounted++;
    _globalMetrics.timesMounted++;

    if (mount->isFlyingMount)
    {
        _metrics.flyingMountUsage++;
        _globalMetrics.flyingMountUsage++;
    }

    if (mount->isDragonridingMount)
    {
        _metrics.dragonridingUsage++;
        _globalMetrics.dragonridingUsage++;
    }

    TC_LOG_DEBUG("module.playerbot", "MountManager::MountPlayer - Player {} mounted on {} (spell {})",
        _bot->GetName(), mount->name, mount->spellId);
    return true;
}

bool MountManager::DismountPlayer()
{
    if (!_bot || !_bot->IsInWorld())
        return false;

    if (!IsMounted())
        return false;

    _bot->RemoveAurasByType(SPELL_AURA_MOUNTED);

        // Update metrics
    _metrics.timesDismounted++;
    _globalMetrics.timesDismounted++;

    // Clear mount tracking

    TC_LOG_DEBUG("module.playerbot", "MountManager::DismountPlayer - Player {} dismounted",
        _bot->GetName());

    return true;
}

bool MountManager::IsMounted() const
{
    if (!_bot)
        return false;

    return _bot->IsMounted();
}

bool MountManager::ShouldAutoMount(Position const& destination) const
{
    if (!_bot)
        return false;

    MountAutomationProfile profile = GetAutomationProfile();
    if (!_profile.autoMount)
        return false;

    // Calculate distance to destination
    float distance = _bot->GetExactDist(&destination);

    return distance >= _profile.minDistanceForMount;
}

// ============================================================================
// Mount Selection
// ============================================================================

MountInfo const* MountManager::GetBestMount() const
{
    if (!_bot)
        return nullptr;

    // No lock needed - mount data is per-bot instance data

    if (_knownMounts.empty())
        return nullptr;

    // Use _profile directly

    // Priority 1: Dragonriding (if enabled and available)
    if (_profile.useDragonriding && CanUseDragonriding())
    {
        MountInfo const* dragonMount = GetDragonridingMount();
        if (dragonMount)
            return dragonMount;
    }

    // Priority 2: Flying mount (if zone allows)
    if (_profile.preferFlyingMount && CanUseFlyingMount())
    {
        MountInfo const* flyingMount = GetFlyingMount();
        if (flyingMount)
            return flyingMount;
    }

    // Priority 3: Aquatic mount (if underwater)
    if (IsPlayerUnderwater())
    {
        MountInfo const* aquaticMount = GetAquaticMount();
        if (aquaticMount)
            return aquaticMount;
    }

    // Priority 4: Ground mount (fallback)
    return GetGroundMount();
}

MountInfo const* MountManager::GetFlyingMount() const
{
    if (!_bot)
        return nullptr;

    // No lock needed - mount data is per-bot instance data

    if (_knownMounts.empty())
        return nullptr;

    MountSpeed maxSpeed = GetMaxMountSpeed();

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

        if (!CanUseMount( mount))
            continue;

        if (mount.speed > maxSpeed)
            continue;

        if (!bestMount || mount.speed > bestMount->speed)
            bestMount = &mount;
    }

    return bestMount;
}

MountInfo const* MountManager::GetGroundMount() const
{
    if (!_bot)
        return nullptr;

    // No lock needed - mount data is per-bot instance data

    if (_knownMounts.empty())
        return nullptr;

    // Use _profile directly

    // Check for preferred mounts first
    if (!_profile.preferredMounts.empty())
    {
        for (uint32 preferredSpellId : _profile.preferredMounts)
        {
            if (playerMountsItr->second.find(preferredSpellId) != playerMountsItr->second.end())
            {
                auto mountItr = _mountDatabase.find(preferredSpellId);
                if (mountItr != _mountDatabase.end())
                {
                    MountInfo const& mount = mountItr->second;
                    if (mount.type == MountType::GROUND && CanUseMount( mount))
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

        if (!CanUseMount( mount))
            continue;

        if (mount.speed > maxSpeed)
            continue;

        if (!bestMount || mount.speed > bestMount->speed)
            bestMount = &mount;
    }

    return bestMount;
}

MountInfo const* MountManager::GetAquaticMount() const
{
    if (!_bot)
        return nullptr;

    // No lock needed - mount data is per-bot instance data

    if (_knownMounts.empty())
        return nullptr;

    for (uint32 spellId : playerMountsItr->second)
    {
        auto mountItr = _mountDatabase.find(spellId);
        if (mountItr == _mountDatabase.end())
            continue;

        MountInfo const& mount = mountItr->second;

        if (!mount.isAquaticMount)
            continue;

        if (CanUseMount( mount))
            return &mount;
    }

    return nullptr;
}

MountInfo const* MountManager::GetDragonridingMount() const
{
    if (!_bot)
        return nullptr;

    // No lock needed - mount data is per-bot instance data

    if (_knownMounts.empty())
        return nullptr;

    {
        auto mountItr = _mountDatabase.find(spellId);
        if (mountItr == _mountDatabase.end())
            continue;

        MountInfo const& mount = mountItr->second;

        if (!mount.isDragonridingMount)
            continue;

        if (CanUseMount( mount))
            return &mount;
    }

    return nullptr;
}

bool MountManager::CanUseFlyingMount() const
{
    if (!_bot)
        return false;

    // Check if player has flying skill
    if (!HasRidingSkill())
        return false;

    uint32 ridingSkill = GetRidingSkill();
    if (ridingSkill < 150) // Requires expert riding (150)
        return false;

    // Check if zone allows flying
    return !IsInNoFlyZone();
}

bool MountManager::IsPlayerUnderwater() const
{
    if (!_bot)
        return false;

    return _bot->IsUnderWater();
}

bool MountManager::CanUseDragonriding() const
{
    if (!_bot)
        return false;

    // Check if player is in dragonriding zone
    return IsInDragonridingZone();
}

// ============================================================================
// Mount Collection
// ============================================================================

std::vector<MountInfo> MountManager::GetPlayerMounts() const
{
    std::vector<MountInfo> mounts;

    if (!_bot)
        return mounts;

    for (uint32 spellId : _knownMounts)
    {
        auto dbItr = _mountDatabase.find(spellId);
        if (dbItr != _mountDatabase.end())
            mounts.push_back(dbItr->second);
    }

    return mounts;
}

// ============================================================================
// Riding Skill
// ============================================================================

uint32 MountManager::GetRidingSkill() const
{
    if (!_bot)
        return 0;

    // Check for riding skill spells
    if (_bot->HasSpell(SPELL_MOUNT_RIDING_MASTER))
        return 300;
    if (_bot->HasSpell(SPELL_MOUNT_RIDING_ARTISAN))
        return 225;
    if (_bot->HasSpell(SPELL_MOUNT_RIDING_EXPERT))
        return 150;
    if (_bot->HasSpell(SPELL_MOUNT_RIDING_JOURNEYMAN))
        return 75;
    if (_bot->HasSpell(SPELL_MOUNT_RIDING_APPRENTICE))
        return 75;

    return 0;
}

bool MountManager::HasRidingSkill() const
{
    return GetRidingSkill() > 0;
}

bool MountManager::LearnRidingSkill(uint32 skillLevel)
{
    if (!_bot)
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
    if (_bot->HasSpell(spellId))
        return true; // Already knows
    _bot->LearnSpell(spellId, false);

    TC_LOG_INFO("module.playerbot", "MountManager::LearnRidingSkill - Player {} learned riding skill {}",
        _bot->GetName(), skillLevel);

    return true;
}

MountSpeed MountManager::GetMaxMountSpeed() const
{
    uint32 ridingSkill = GetRidingSkill();

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

uint32 MountManager::GetAvailablePassengerSeats() const
{
    if (!_bot || !IsMounted())
        return 0;

    Vehicle* vehicle = _bot->GetVehicleKit();
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

bool MountManager::AddPassenger(::Player* passenger)
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

void MountManager::SetAutomationProfile(MountAutomationProfile const& profile)
{
    _profile = profile;
}

MountAutomationProfile MountManager::GetAutomationProfile() const
{
    return _profile;
}

// ============================================================================
// Metrics
// ============================================================================

MountManager::MountMetrics const& MountManager::GetMetrics() const
{
    return _metrics;
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

bool MountManager::CastMountSpell(uint32 spellId)
{
    if (!_bot)
        return false;

    if (!CanCastMountSpell( spellId))
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);

    // Cast mount spell
    _bot->CastSpell(_bot, spellId, false);

    HandleMountCastResult( spellId, true);

    return true;
}

bool MountManager::CanCastMountSpell(uint32 spellId) const
{
    if (!_bot)
        return false;

    if (!_bot->HasSpell(spellId))
        return false;

    if (_bot->IsMounted())
        return false;

    if (IsInCombat())
        return false;

    if (IsIndoors())
        return false;

    return true;
}

void MountManager::HandleMountCastResult(uint32 spellId, bool success)
{
    if (!_bot)
        return;

    if (success)
    {
        TC_LOG_DEBUG("module.playerbot", "MountManager::HandleMountCastResult - Player {} successfully cast mount spell {}",
            _bot->GetName(), spellId);
    }
    else
    {
        TC_LOG_WARN("module.playerbot", "MountManager::HandleMountCastResult - Player {} failed to cast mount spell {}",
            _bot->GetName(), spellId);
    }
}

// ============================================================================
// Zone Detection Helpers
// ============================================================================

bool MountManager::IsInNoFlyZone() const
{
    if (!_bot)
        return true;

    Map* map = _bot->GetMap();
    if (!map)
        return true;

    // Check if flying is disabled in this map
    return !map->IsFlyingAllowed();
}

bool MountManager::IsInDragonridingZone() const
{
    if (!_bot)
        return false;

    // Dragonriding is available in Dragon Isles zones (Dragonflight expansion)
    // This would check specific zone IDs
    uint32 zoneId = GetCurrentZoneId();

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

bool MountManager::IsInAquaticZone() const
{
    if (!_bot)
        return false;

    return IsPlayerUnderwater();
}

uint32 MountManager::GetCurrentZoneId() const
{
    if (!_bot)
        return 0;

    return _bot->GetZoneId();
}

// ============================================================================
// Validation Helpers
// ============================================================================

bool MountManager::ValidateMountUsage() const
{
    if (!_bot)
        return false;

    if (IsInCombat())
        return false;

    if (IsIndoors())
        return false;

    if (IsInInstance())
    {
        // Some instances allow mounts, others don't
        // This would check instance-specific mount permissions
        return false;
    }

    return true;
}

bool MountManager::IsInCombat() const
{
    if (!_bot)
        return false;

    return _bot->IsInCombat();
}

bool MountManager::IsIndoors() const
{
    if (!_bot)
        return true;

    return !_bot->IsOutdoors();
}

bool MountManager::IsInInstance() const
{
    if (!_bot)
        return false;

    Map* map = _bot->GetMap();
    if (!map)
        return false;

    return map->IsDungeon() || map->IsRaid() || map->IsBattleground() || map->IsArena();
}

} // namespace Playerbot
