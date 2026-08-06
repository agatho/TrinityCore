/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ChallengeMode_h__
#define ChallengeMode_h__

#include "Define.h"
#include "ObjectGuid.h"
#include <array>

class Creature;
class InstanceMap;
class Player;

// Per-run Mythic Keystone state, owned by the InstanceMap it runs in (see InstanceMap::GetChallengeMode).
// Holds the active keystone level, affixes, the timer and the death count, and drives run start/completion.
// Static data (pool, par times, scaling curve) lives in the global ChallengeModeMgr.
class TC_GAME_API ChallengeMode
{
public:
    // How often the health-threshold affixes (Raging, Grievous) re-scan the instance.
    static constexpr uint32 AFFIX_TICK_INTERVAL_MS = 1 * IN_MILLISECONDS;
    // How often the periodic in-combat spawn affixes (Incorporeal, Afflicted) add a new mob.
    static constexpr uint32 SPAWN_TICK_INTERVAL_MS = 20 * IN_MILLISECONDS;

    explicit ChallengeMode(InstanceMap* instance);
    ~ChallengeMode();

    // Begin a run from a slotted keystone. affixes are the (up to 4) KeystoneAffix IDs, 0 = empty slot.
    // keystone is the item GUID that was activated; it is upgraded/depleted in place on completion.
    void Start(uint32 mapChallengeModeId, uint32 keystoneLevel, std::array<uint32, 4> const& affixes, ObjectGuid starter, ObjectGuid keystone);
    void Reset();

    void Update(uint32 diff);
    void OnPlayerDeath(Player* player);
    // A non-boss enemy died: drive the on-death affix behaviours (Bolstering empowers survivors, Bursting
    // stacks a DoT on the party, ...). No-op when the run is inactive or the victim is a boss/summon.
    void OnCreatureDeath(Creature* victim);

    bool HasAffix(uint32 affixId) const;
    // Called when a dungeon encounter reaches DONE; completes the run once every encounter is defeated.
    void Complete();

    bool IsActive() const { return _active && !_completed; }
    bool IsCompleted() const { return _completed; }

    uint32 GetMapChallengeModeId() const { return _mapChallengeModeId; }
    uint32 GetKeystoneLevel() const { return _keystoneLevel; }
    std::array<uint32, 4> const& GetAffixes() const { return _affixes; }
    ObjectGuid GetStarterGuid() const { return _starterGuid; }

    uint32 GetElapsedMs() const { return _elapsedMs; }
    uint32 GetDeathCount() const { return _deathCount; }
    // Per-death time penalty for THIS run, banded per retail 12.x: 0s while Lindormi's Guidance is active,
    // 15s under Xal'atath's Guile (+12+), 5s otherwise (all config-tunable, ChallengeMode.DeathPenalty.*).
    uint32 GetDeathPenaltyMs() const;
    // Elapsed wall-clock plus the per-death time penalty; this is the time compared against the par thresholds.
    uint32 GetEffectiveTimeMs() const { return _elapsedMs + _deathCount * GetDeathPenaltyMs(); }
    uint32 GetTimeLimitMs() const { return _timeLimitMs; }

private:
    void BroadcastTimer(uint32 timeLeftMs) const;
    // Periodic (AFFIX_TICK_INTERVAL_MS) scan for the health-threshold affixes: Raging enrages wounded enemies,
    // Grievous bleeds wounded players until they heal back up.
    void UpdateHealthThresholdAffixes();
    // Periodic (SPAWN_TICK_INTERVAL_MS) in-combat spawn of the add affixes (Incorporeal, Afflicted).
    void UpdateSpawnAffixes();
    // Xal'atath's Bargain 60s in-combat event (Midnight 12.x): Ascendant orb wave, Voidbound emissary,
    // Devour rift debuff, Pulsar orbiter. Fires only while at least one player is fighting.
    void TriggerBargainEvent();
    // Lindormi's Guidance: marks a set of non-boss enemies at run start (Temporal Sands highlight spell).
    void ApplyGuidanceMarks();
    // Rolls the configured end-of-run reward loot for one player and grants each item at the Mythic+ item level.
    void AwardGearReward(Player* player, uint32 rewardLootId) const;

    InstanceMap* _instance;
    uint32 _mapChallengeModeId = 0;
    uint32 _keystoneLevel = 0;
    std::array<uint32, 4> _affixes = { };
    ObjectGuid _starterGuid;
    ObjectGuid _keystoneGuid;

    uint32 _timeLimitMs = 0;
    uint32 _elapsedMs = 0;
    uint32 _deathCount = 0;
    uint32 _affixTickTimer = 0;
    uint32 _spawnTickTimer = 0;
    uint32 _bargainTickTimer = 0;
    bool _active = false;
    bool _completed = false;
};

#endif // ChallengeMode_h__
