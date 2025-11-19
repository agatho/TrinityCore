/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#pragma once

#include "Define.h"
#include <vector>

class Player;
class Position;

namespace Playerbot
{

struct MountInfo;
struct MountAutomationProfile;
enum class MountSpeed : uint8;

/**
 * @brief Interface for mount management system
 *
 * Provides comprehensive mount automation including flying, ground,
 * aquatic, dragonriding, multi-passenger support, and collection tracking.
 */
class TC_GAME_API IMountManager
{
public:
    virtual ~IMountManager() = default;

    // Core mount management
    virtual void Initialize() = 0;
    virtual void Update(::Player* player, uint32 diff) = 0;
    virtual bool MountPlayer(::Player* player) = 0;
    virtual bool DismountPlayer(::Player* player) = 0;
    virtual bool IsMounted(::Player* player) const = 0;
    virtual bool ShouldAutoMount(::Player* player, Position const& destination) const = 0;

    // Mount selection
    virtual MountInfo const* GetBestMount(::Player* player) const = 0;
    virtual MountInfo const* GetFlyingMount(::Player* player) const = 0;
    virtual MountInfo const* GetGroundMount(::Player* player) const = 0;
    virtual MountInfo const* GetAquaticMount(::Player* player) const = 0;
    virtual MountInfo const* GetDragonridingMount(::Player* player) const = 0;
    virtual bool CanUseFlyingMount(::Player* player) const = 0;
    virtual bool IsPlayerUnderwater(::Player* player) const = 0;
    virtual bool CanUseDragonriding(::Player* player) const = 0;

    // Mount collection
    virtual std::vector<MountInfo> GetPlayerMounts(::Player* player) const = 0;
    virtual bool KnowsMount(::Player* player, uint32 spellId) const = 0;
    virtual bool LearnMount(::Player* player, uint32 spellId) = 0;
    virtual uint32 GetMountCount(::Player* player) const = 0;
    virtual bool CanUseMount(::Player* player, MountInfo const& mount) const = 0;

    // Riding skill
    virtual uint32 GetRidingSkill(::Player* player) const = 0;
    virtual bool HasRidingSkill(::Player* player) const = 0;
    virtual bool LearnRidingSkill(::Player* player, uint32 skillLevel) = 0;
    virtual MountSpeed GetMaxMountSpeed(::Player* player) const = 0;

    // Multi-passenger mounts
    virtual bool IsMultiPassengerMount(MountInfo const& mount) const = 0;
    virtual uint32 GetAvailablePassengerSeats(::Player* player) const = 0;
    virtual bool AddPassenger(::Player* mountedPlayer, ::Player* passenger) = 0;
    virtual bool RemovePassenger(::Player* passenger) = 0;

    // Automation
    virtual void SetAutomationProfile(uint32 playerGuid, MountAutomationProfile const& profile) = 0;
    virtual MountAutomationProfile GetAutomationProfile(uint32 playerGuid) const = 0;

    // Database initialization
    virtual void LoadMountDatabase() = 0;
    virtual void InitializeVanillaMounts() = 0;
    virtual void InitializeTBCMounts() = 0;
    virtual void InitializeWrathMounts() = 0;
    virtual void InitializeCataclysmMounts() = 0;
    virtual void InitializePandariaMounts() = 0;
    virtual void InitializeDraenorMounts() = 0;
    virtual void InitializeLegionMounts() = 0;
    virtual void InitializeBfAMounts() = 0;
    virtual void InitializeShadowlandsMounts() = 0;
    virtual void InitializeDragonflightMounts() = 0;
};

} // namespace Playerbot
