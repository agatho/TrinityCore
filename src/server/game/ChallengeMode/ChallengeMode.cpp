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

#include "ChallengeMode.h"
#include "ChallengeModeMgr.h"
#include "Creature.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "MiscPackets.h"
#include "ObjectAccessor.h"
#include "Player.h"

ChallengeMode::ChallengeMode(InstanceMap* instance) : _instance(instance) { }
ChallengeMode::~ChallengeMode() = default;

void ChallengeMode::Start(uint32 mapChallengeModeId, uint32 keystoneLevel, std::array<uint32, 4> const& affixes, ObjectGuid starter)
{
    _mapChallengeModeId = mapChallengeModeId;
    _keystoneLevel = keystoneLevel;
    _affixes = affixes;
    _starterGuid = starter;
    _timeLimitMs = sChallengeModeMgr.GetTimeLimit(mapChallengeModeId) * IN_MILLISECONDS;
    _elapsedMs = 0;
    _deathCount = 0;
    _active = true;
    _completed = false;

    // Drive the client dungeon timer via the group's ChallengeMode countdown slot (the one C_ChallengeMode reads).
    if (Player* starterPlayer = ObjectAccessor::GetPlayer(*_instance, _starterGuid))
        if (Group* group = starterPlayer->GetGroup())
            group->StartCountdown(CountdownTimerType::ChallengeMode, Seconds(_timeLimitMs / IN_MILLISECONDS));

    BroadcastTimer(_timeLimitMs);

    // Re-apply stats to already-spawned creatures so they pick up the keystone scaling immediately
    // (creatures spawned/reset after this point read the level directly in Get{Max,Base}...ForLevel).
    for (auto const& [spawnId, creature] : _instance->GetCreatureBySpawnIdStore())
        if (creature && creature->IsAlive())
            creature->UpdateLevelDependantStats();

    TC_LOG_INFO("challengemode", "ChallengeMode start: instance {} challengeMode {} level {} timeLimit {}s",
        _instance->GetInstanceId(), mapChallengeModeId, keystoneLevel, _timeLimitMs / IN_MILLISECONDS);
}

void ChallengeMode::Reset()
{
    _active = false;
    _completed = false;
    _mapChallengeModeId = 0;
    _keystoneLevel = 0;
    _affixes = { };
    _starterGuid.Clear();
    _timeLimitMs = 0;
    _elapsedMs = 0;
    _deathCount = 0;
}

void ChallengeMode::Update(uint32 diff)
{
    if (!IsActive())
        return;

    _elapsedMs += diff;
}

void ChallengeMode::OnPlayerDeath(Player* /*player*/)
{
    if (!IsActive())
        return;

    // Each death adds DEATH_TIME_PENALTY_MS to the effective run time (applied at completion via GetEffectiveTimeMs).
    ++_deathCount;
}

void ChallengeMode::Complete()
{
    if (!IsActive())
        return;

    _active = false;
    _completed = true;

    uint32 const effectiveSeconds = GetEffectiveTimeMs() / IN_MILLISECONDS;
    uint32 const keystoneUpgrade = sChallengeModeMgr.GetKeystoneUpgradeAmount(_mapChallengeModeId, effectiveSeconds);

    // Stop the client dungeon timer.
    if (Player* starterPlayer = ObjectAccessor::GetPlayer(*_instance, _starterGuid))
        if (Group* group = starterPlayer->GetGroup())
            group->StartCountdown(CountdownTimerType::ChallengeMode, Seconds(0));

    TC_LOG_INFO("challengemode", "ChallengeMode complete: instance {} challengeMode {} level {} time {}s (+{}s deaths, limit {}s) -> +{} keystone level",
        _instance->GetInstanceId(), _mapChallengeModeId, _keystoneLevel, GetElapsedMs() / IN_MILLISECONDS,
        (_deathCount * DEATH_TIME_PENALTY_MS) / IN_MILLISECONDS, _timeLimitMs / IN_MILLISECONDS, keystoneUpgrade);

    // Dungeon-score computation, the SMSG_CHALLENGE_MODE_COMPLETE reward packet, the keystone-upgrade item and
    // end-of-run / Great Vault loot are applied in later phases (P3/P4).
}

void ChallengeMode::BroadcastTimer(uint32 timeLeftMs) const
{
    WorldPackets::Misc::StartTimer startTimer;
    startTimer.Type = CountdownTimerType::ChallengeMode;
    startTimer.TotalTime = Seconds(_timeLimitMs / IN_MILLISECONDS);
    startTimer.TimeLeft = Seconds(timeLeftMs / IN_MILLISECONDS);
    _instance->SendToPlayers(startTimer.Write());
}
