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
#include "GameTime.h"
#include "Group.h"
#include "Item.h"
#include "ItemDefines.h"
#include "Log.h"
#include "Map.h"
#include "MiscPackets.h"
#include "MythicPlusData.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Random.h"

ChallengeMode::ChallengeMode(InstanceMap* instance) : _instance(instance) { }
ChallengeMode::~ChallengeMode() = default;

void ChallengeMode::Start(uint32 mapChallengeModeId, uint32 keystoneLevel, std::array<uint32, 4> const& affixes, ObjectGuid starter, ObjectGuid keystone)
{
    _mapChallengeModeId = mapChallengeModeId;
    _keystoneLevel = keystoneLevel;
    _affixes = affixes;
    _starterGuid = starter;
    _keystoneGuid = keystone;
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
    _keystoneGuid.Clear();
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

    uint32 const effectiveTimeMs = GetEffectiveTimeMs();
    uint32 const keystoneUpgrade = sChallengeModeMgr.GetKeystoneUpgradeAmount(_mapChallengeModeId, effectiveTimeMs / IN_MILLISECONDS);

    uint32 affixCount = 0;
    for (uint32 affixId : _affixes)
        if (affixId)
            ++affixCount;

    float const runScore = sChallengeModeMgr.CalculateRunScore(_keystoneLevel, effectiveTimeMs, _timeLimitMs, affixCount);

    // Record the run for every player present at completion (keeps the best per dungeon).
    MythicPlusRunRecord record;
    record.ChallengeModeID = _mapChallengeModeId;
    record.Level = _keystoneLevel;
    record.DurationMs = effectiveTimeMs;
    record.Deaths = _deathCount;
    record.CompletionDate = GameTime::GetGameTime();
    record.Score = runScore;
    record.Affixes = _affixes;

    _instance->DoOnPlayers([&record](Player* player)
    {
        if (MythicPlusData* data = player->GetMythicPlusData())
            data->RecordRun(record);
    });

    if (Player* starterPlayer = ObjectAccessor::GetPlayer(*_instance, _starterGuid))
    {
        // Upgrade (or deplete) the activated keystone in place: a timed clear raises the level and rerolls the
        // dungeon; an over-time clear depletes it by one (floor +2). Blizzlike-equivalent to the retail
        // "receive a new keystone" reward, without depending on the seasonal keystone item entry.
        if (Item* keystone = starterPlayer->GetItemByGuid(_keystoneGuid))
        {
            uint32 const newLevel = keystoneUpgrade > 0 ? _keystoneLevel + keystoneUpgrade : std::max<uint32>(2, _keystoneLevel - 1);

            uint32 newChallengeModeId = _mapChallengeModeId;
            std::vector<uint32> const& pool = sChallengeModeMgr.GetSeasonMapChallengeModeIds();
            if (!pool.empty())
                newChallengeModeId = pool[urand(0, uint32(pool.size() - 1))];

            keystone->SetModifier(ITEM_MODIFIER_CHALLENGE_MAP_CHALLENGE_MODE_ID, newChallengeModeId);
            keystone->SetModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_LEVEL, newLevel);

            std::vector<uint32> const newAffixes = sChallengeModeMgr.GetActiveAffixes(newLevel);
            for (uint32 i = 0; i < 4; ++i)
                keystone->SetModifier(ItemModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_AFFIX_ID_1 + i), i < newAffixes.size() ? newAffixes[i] : 0u);

            keystone->SetState(ITEM_CHANGED, starterPlayer);
        }

        // Stop the client dungeon timer.
        if (Group* group = starterPlayer->GetGroup())
            group->StartCountdown(CountdownTimerType::ChallengeMode, Seconds(0));
    }

    TC_LOG_INFO("challengemode", "ChallengeMode complete: instance {} challengeMode {} level {} time {}s (+{}s deaths, limit {}s) -> +{} keystone, score {:.1f}",
        _instance->GetInstanceId(), _mapChallengeModeId, _keystoneLevel, GetElapsedMs() / IN_MILLISECONDS,
        (_deathCount * DEATH_TIME_PENALTY_MS) / IN_MILLISECONDS, _timeLimitMs / IN_MILLISECONDS, keystoneUpgrade, runScore);

    // The SMSG_CHALLENGE_MODE_COMPLETE reward packet, the keystone-upgrade item and the end-of-run / Great Vault
    // loot are applied in Phase 4.
}

void ChallengeMode::BroadcastTimer(uint32 timeLeftMs) const
{
    WorldPackets::Misc::StartTimer startTimer;
    startTimer.Type = CountdownTimerType::ChallengeMode;
    startTimer.TotalTime = Seconds(_timeLimitMs / IN_MILLISECONDS);
    startTimer.TimeLeft = Seconds(timeLeftMs / IN_MILLISECONDS);
    _instance->SendToPlayers(startTimer.Write());
}
