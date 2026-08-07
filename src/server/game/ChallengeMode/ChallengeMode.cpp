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
#include "ChallengeModePackets.h"
#include "CharacterDatabase.h"
#include "Config.h"
#include "Containers.h"
#include "Creature.h"
#include "DB2Stores.h"
#include "DatabaseEnv.h"
#include "DBCEnums.h"
#include "ElapsedTimerMgr.h"
#include "GameTime.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Item.h"
#include "ItemBonusMgr.h"
#include "ItemDefines.h"
#include "Loot.h"
#include "LootMgr.h"
#include "Log.h"
#include "Mail.h"
#include "Map.h"
#include "MiscPackets.h"
#include "MythicPlusData.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Random.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "WeeklyRewardsMgr.h"
#include <algorithm>
#include <vector>

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
    if (Player* starterPlayer = ObjectAccessor::GetPlayer(_instance, _starterGuid))
    {
        if (Group* group = starterPlayer->GetGroup())
            group->StartCountdown(CountdownTimerType::ChallengeMode, Seconds(_timeLimitMs / IN_MILLISECONDS));

        // Retail consumes the keystone at Font activation and hands the result key out at the end of the run.
        // Equivalent here: stamp the depleted result (one level lower, rerolled dungeon, fresh weekly affixes)
        // into the item immediately, so abandoning / disconnecting / unloading the instance can never dodge
        // depletion. A timed completion re-stamps the upgrade from the original level (see Complete()).
        // Depletion respects the player's Resilient Keystone floor (same-level reroll at the floor).
        if (Item* keystone = starterPlayer->GetItemByGuid(_keystoneGuid))
        {
            uint32 const floorLevel = std::max(sChallengeModeMgr.GetKeystoneMinLevel(), sChallengeModeMgr.GetKeystoneFloor(starterPlayer));
            sChallengeModeMgr.StampKeystone(keystone, sChallengeModeMgr.RollSeasonDungeon(_mapChallengeModeId),
                std::max(floorLevel, _keystoneLevel > 0 ? _keystoneLevel - 1 : 0));
        }
    }

    BroadcastTimer(_timeLimitMs);

    // Count-UP run timer for the objective tracker. This is the WorldElapsedTimer.db2 row the
    // client maps to Enum.WorldElapsedTimerTypes.ChallengeMode, which is what makes
    // ScenarioTimerMixin populate from the server instead of estimating locally.
    sElapsedTimerMgr->StartTimerForMap(_instance, WORLD_ELAPSED_TIMER_CHALLENGE_MODE, 0s);

    // Announce the run (map / level / affixes) to the party UI. Member roster is omitted for now (see packet note).
    WorldPackets::ChallengeMode::ChallengeModeStart startPacket;
    if (MapChallengeModeEntry const* challengeMode = sMapChallengeModeStore.LookupEntry(_mapChallengeModeId))
        startPacket.MapID = challengeMode->MapID;
    startPacket.MapChallengeModeID = _mapChallengeModeId;
    startPacket.KeystoneLevel = _keystoneLevel;
    startPacket.Affixes = _affixes;
    _instance->SendToPlayers(startPacket.Write());

    // Re-apply stats to already-spawned creatures so they pick up the keystone scaling immediately
    // (creatures spawned/reset after this point read the level directly in Get{Max,Base}...ForLevel).
    for (auto const& [spawnId, creature] : _instance->GetCreatureBySpawnIdStore())
        if (creature && creature->IsAlive())
            creature->UpdateLevelDependantStats();

    // Lindormi's Guidance: highlight the marked trash set right as the run begins.
    if (HasAffix(ChallengeModeAffix::LindormisGuidance))
        ApplyGuidanceMarks();

    // Run-wide combat-res pool (retail: 1 charge at activation, +1 per interval; shared by all battle-res
    // spells). Encounters do not reset it during an active run (see InstanceScript::SetBossState).
    if (InstanceScript* script = _instance->GetInstanceScript())
        script->InitializeCombatResurrections(uint8(sConfigMgr->GetIntDefault("ChallengeMode.CombatRes.InitialCharges", 1)),
            uint32(sConfigMgr->GetIntDefault("ChallengeMode.CombatRes.IntervalMs", 10 * MINUTE * IN_MILLISECONDS)));

    TC_LOG_INFO("challengemode", "ChallengeMode start: instance {} challengeMode {} level {} timeLimit {}s",
        _instance->GetInstanceId(), mapChallengeModeId, keystoneLevel, _timeLimitMs / IN_MILLISECONDS);
}

void ChallengeMode::Reset()
{
    // Stop the client dungeon timer if a run was in progress.
    if (Player* starterPlayer = ObjectAccessor::GetPlayer(_instance, _starterGuid))
        if (Group* group = starterPlayer->GetGroup())
            group->StartCountdown(CountdownTimerType::ChallengeMode, Seconds(0));

    // Drop the run-wide combat-res pool.
    if (InstanceScript* script = _instance->GetInstanceScript())
        script->ResetCombatResurrections();

    if (_active || _completed)
    {
        // Cancel the countdown widget outright rather than leaving it sitting at zero, and drop the
        // run timer without keeping the value on screen - the run is being abandoned, not finished.
        WorldPackets::Misc::StopTimer stopTimer;
        stopTimer.Type = CountdownTimerType::ChallengeMode;
        _instance->SendToPlayers(stopTimer.Write());

        sElapsedTimerMgr->StopTimerForMap(_instance, WORLD_ELAPSED_TIMER_CHALLENGE_MODE, false);
    }

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
    _enemyKills = 0;
    _awaitingEnemyForces = false;
}

void ChallengeMode::Update(uint32 diff)
{
    if (!IsActive())
        return;

    _elapsedMs += diff;

    _affixTickTimer += diff;
    if (_affixTickTimer >= AFFIX_TICK_INTERVAL_MS)
    {
        _affixTickTimer = 0;
        UpdateHealthThresholdAffixes();
    }

    _spawnTickTimer += diff;
    if (_spawnTickTimer >= SPAWN_TICK_INTERVAL_MS)
    {
        _spawnTickTimer = 0;
        UpdateSpawnAffixes();
    }

    // Xal'atath's Bargain: a 60s cadence event while the party is fighting. The timer accumulates freely but
    // only fires (and resets) when someone is in combat, matching the retail "every 60 seconds in combat" rhythm.
    _bargainTickTimer += diff;
    uint32 const bargainInterval = uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Bargain.IntervalMs", 60 * IN_MILLISECONDS));
    if (_bargainTickTimer >= bargainInterval)
    {
        _bargainTickTimer = bargainInterval;    // hold at the threshold until it can fire in combat

        bool anyBargain = HasAffix(ChallengeModeAffix::XalatathsBargainAscendant)
            || HasAffix(ChallengeModeAffix::XalatathsBargainVoidbound)
            || HasAffix(ChallengeModeAffix::XalatathsBargainDevour)
            || HasAffix(ChallengeModeAffix::XalatathsBargainPulsar);
        if (!anyBargain)
        {
            _bargainTickTimer = 0;
            return;
        }

        bool anyInCombat = false;
        _instance->DoOnPlayers([&anyInCombat](Player* player)
        {
            if (player->IsAlive() && player->IsInCombat())
                anyInCombat = true;
        });

        if (anyInCombat)
        {
            _bargainTickTimer = 0;
            TriggerBargainEvent();
        }
    }
}

void ChallengeMode::TriggerBargainEvent()
{
    // Ascendant: a wave of Orbs of Ascendance (world-DB creature casting Cosmic Ascension) around a random
    // fighting player. Voidbound: a single Void Emissary (shielded Dark Prayer caster). Both no-op without a
    // configured creature entry, like every spawn affix.
    for (uint32 affixId : { ChallengeModeAffix::XalatathsBargainAscendant, ChallengeModeAffix::XalatathsBargainVoidbound })
    {
        if (!HasAffix(affixId))
            continue;

        uint32 const creatureId = sChallengeModeMgr.GetAffixCreatureId(affixId);
        if (!creatureId)
            continue;

        std::vector<Player*> combatants;
        _instance->DoOnPlayers([&combatants](Player* player)
        {
            if (player->IsAlive() && player->IsInCombat())
                combatants.push_back(player);
        });
        if (combatants.empty())
            continue;

        Player* anchor = Trinity::Containers::SelectRandomContainerElement(combatants);
        uint32 const spawnCount = affixId == ChallengeModeAffix::XalatathsBargainAscendant
            ? uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Ascendant.SpawnCount", 10)) : 1u;
        for (uint32 i = 0; i < spawnCount; ++i)
        {
            Position pos = anchor->GetRandomNearPosition(12.0f);
            anchor->SummonCreature(creatureId, pos, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 60s);
        }
    }

    // Devour: every player gains the Devouring Rift debuff (dispel it or heal it off for the boon; the boon /
    // enemy-buff consequences ride on the spell scripts). Pulsar: every player gets an orbiting Void Pulsar.
    for (uint32 affixId : { ChallengeModeAffix::XalatathsBargainDevour, ChallengeModeAffix::XalatathsBargainPulsar })
    {
        if (!HasAffix(affixId))
            continue;

        uint32 const spellId = sChallengeModeMgr.GetAffixSpellId(affixId);
        if (!spellId || !sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
            continue;

        _instance->DoOnPlayers([spellId](Player* player)
        {
            if (player->IsAlive())
                player->CastSpell(player, spellId, true);
        });
    }
}

void ChallengeMode::ApplyGuidanceMarks()
{
    // Marks MarkCount random alive non-boss enemies with the Temporal Sands highlight. The -5% health/damage
    // component and the enemy-forces completion rule ride on the mark spell / forces tracking; the mark itself
    // is the visible, functional part and no-ops without a valid spell (established affix pattern).
    uint32 const spellId = sChallengeModeMgr.GetAffixSpellId(ChallengeModeAffix::LindormisGuidance);
    if (!spellId || !sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
        return;

    std::vector<Creature*> candidates;
    for (auto const& [spawnId, creature] : _instance->GetCreatureBySpawnIdStore())
        if (creature && creature->IsAlive() && !creature->IsDungeonBoss() && creature->IsHostileToPlayers())
            candidates.push_back(creature);

    uint32 const markCount = uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Guidance.MarkCount", 8));
    Trinity::Containers::RandomResize(candidates, markCount);
    for (Creature* creature : candidates)
        creature->CastSpell(creature, spellId, true);
}

uint32 ChallengeMode::GetDeathPenaltyMs() const
{
    // Retail 12.x banding: no penalty while Lindormi's Guidance is active (low keys), 15s under Xal'atath's
    // Guile (+12+), 5s baseline otherwise. Values in seconds, config-tunable.
    if (HasAffix(ChallengeModeAffix::LindormisGuidance))
        return uint32(sConfigMgr->GetIntDefault("ChallengeMode.DeathPenalty.GuidanceSeconds", 0)) * IN_MILLISECONDS;
    if (HasAffix(ChallengeModeAffix::XalatathsGuile))
        return uint32(sConfigMgr->GetIntDefault("ChallengeMode.DeathPenalty.GuileSeconds", 15)) * IN_MILLISECONDS;
    return uint32(sConfigMgr->GetIntDefault("ChallengeMode.DeathPenalty.BaseSeconds", 5)) * IN_MILLISECONDS;
}

void ChallengeMode::UpdateSpawnAffixes()
{
    // Periodic in-combat add affixes (Incorporeal, Afflicted). Spiteful spawns on death, handled separately.
    static constexpr uint32 spawnAffixes[] = { ChallengeModeAffix::Incorporeal, ChallengeModeAffix::Afflicted };

    bool anyActive = false;
    for (uint32 affixId : spawnAffixes)
        if (HasAffix(affixId) && sChallengeModeMgr.GetAffixCreatureId(affixId))
            anyActive = true;
    if (!anyActive)
        return;

    // Anchor the spawn on a random player who is currently fighting.
    std::vector<Player*> combatants;
    _instance->DoOnPlayers([&combatants](Player* player)
    {
        if (player->IsAlive() && player->IsInCombat())
            combatants.push_back(player);
    });
    if (combatants.empty())
        return;

    Player* anchor = Trinity::Containers::SelectRandomContainerElement(combatants);
    for (uint32 affixId : spawnAffixes)
    {
        if (!HasAffix(affixId))
            continue;
        if (uint32 creatureId = sChallengeModeMgr.GetAffixCreatureId(affixId))
        {
            Position pos = anchor->GetRandomNearPosition(8.0f);
            anchor->SummonCreature(creatureId, pos, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 20s);
        }
    }
}

void ChallengeMode::UpdateHealthThresholdAffixes()
{
    // Raging: wounded (<=30% HP) non-boss enemies enrage until defeated. The enrage spell is applied once and
    // persists, so re-applying is gated on the aura already being present.
    if (HasAffix(ChallengeModeAffix::Raging))
    {
        if (uint32 spellId = sChallengeModeMgr.GetAffixSpellId(ChallengeModeAffix::Raging))
            if (sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
                for (auto const& [spawnId, creature] : _instance->GetCreatureBySpawnIdStore())
                    if (creature && creature->IsAlive() && creature->IsInCombat() && !creature->IsDungeonBoss()
                        && creature->IsHostileToPlayers() && creature->GetHealthPct() <= 30.0f && !creature->HasAura(spellId))
                        creature->CastSpell(creature, spellId, true);
    }

    // Grievous: players below 90% HP take a lingering bleed; healing back to 90%+ clears it.
    if (HasAffix(ChallengeModeAffix::Grievous))
    {
        if (uint32 spellId = sChallengeModeMgr.GetAffixSpellId(ChallengeModeAffix::Grievous))
            if (sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
                _instance->DoOnPlayers([spellId](Player* player)
                {
                    if (player->IsAlive() && player->GetHealthPct() < 90.0f)
                    {
                        if (!player->HasAura(spellId))
                            player->CastSpell(player, spellId, true);
                    }
                    else
                        player->RemoveAurasDueToSpell(spellId);
                });
    }
}

void ChallengeMode::OnPlayerDeath(Player* /*player*/)
{
    if (!IsActive())
        return;

    // Each death adds DEATH_TIME_PENALTY_MS to the effective run time (applied at completion via GetEffectiveTimeMs).
    ++_deathCount;

    // Keep the client's live death counter (and its displayed time penalty) in sync.
    WorldPackets::ChallengeMode::ChallengeModeUpdateDeathCount deathCountPacket;
    deathCountPacket.DeathCount = _deathCount;
    _instance->SendToPlayers(deathCountPacket.Write());
}

bool ChallengeMode::HasAffix(uint32 affixId) const
{
    return std::find(_affixes.begin(), _affixes.end(), affixId) != _affixes.end();
}

void ChallengeMode::OnCreatureDeath(Creature* victim)
{
    // On-death affixes only trigger off regular hostile trash, never bosses, pets or friendly summons.
    if (!IsActive() || !victim || victim->IsDungeonBoss() || victim->IsPet() || victim->IsControlledByPlayer())
        return;

    // Enemy forces: every hostile trash death counts one kill; when all bosses are already down, the kill that
    // reaches 100% completes the run.
    if (victim->IsHostileToPlayers())
    {
        ++_enemyKills;
        if (_awaitingEnemyForces && AreEnemyForcesMet())
        {
            Complete();
            return;
        }
    }

    // Bolstering: the death cry empowers nearby surviving non-boss enemies (buff spell handles the % itself).
    if (HasAffix(ChallengeModeAffix::Bolstering))
    {
        if (uint32 spellId = sChallengeModeMgr.GetAffixSpellId(ChallengeModeAffix::Bolstering))
            if (sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
                for (auto const& [spawnId, other] : _instance->GetCreatureBySpawnIdStore())
                    if (other && other != victim && other->IsAlive() && !other->IsDungeonBoss()
                        && other->IsHostileToPlayers() && other->IsWithinDist(victim, 30.0f))
                        other->CastSpell(other, spellId, true);
    }

    // Bursting: slain enemies inflict a stacking damage-over-time on the whole party.
    if (HasAffix(ChallengeModeAffix::Bursting))
    {
        if (uint32 spellId = sChallengeModeMgr.GetAffixSpellId(ChallengeModeAffix::Bursting))
            if (sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
                _instance->DoOnPlayers([victim, spellId](Player* player)
                {
                    victim->CastSpell(player, spellId, true);
                });
    }

    // Sanguine: the corpse leaves a lingering ichor pool (areatrigger-creating spell) that heals allies / hurts players.
    if (HasAffix(ChallengeModeAffix::Sanguine))
    {
        if (uint32 spellId = sChallengeModeMgr.GetAffixSpellId(ChallengeModeAffix::Sanguine))
            if (sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
                victim->CastSpell(victim, spellId, true);
    }

    // Spiteful: the corpse rises as a Spiteful Shade that fixates a survivor. The summoned creature's world-DB AI
    // (fixate + self-decay) drives the behaviour; SummonCreature simply no-ops if the entry is not in the DB.
    if (HasAffix(ChallengeModeAffix::Spiteful))
    {
        if (uint32 creatureId = sChallengeModeMgr.GetAffixCreatureId(ChallengeModeAffix::Spiteful))
            victim->SummonCreature(creatureId, victim->GetPosition(), TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 30s);
    }
}

bool ChallengeMode::AreEnemyForcesMet() const
{
    uint32 const required = sChallengeModeMgr.GetEnemyForcesRequiredKills(_mapChallengeModeId);
    return !required || _enemyKills >= required;
}

void ChallengeMode::OnAllEncountersDone()
{
    if (!IsActive())
        return;

    if (AreEnemyForcesMet())
    {
        Complete();
        return;
    }

    // Bosses are down but trash remains: arm completion on the kill that reaches 100% enemy forces.
    _awaitingEnemyForces = true;
    TC_LOG_DEBUG("challengemode", "ChallengeMode: all encounters done, awaiting enemy forces ({}/{}).",
        _enemyKills, sChallengeModeMgr.GetEnemyForcesRequiredKills(_mapChallengeModeId));
}

void ChallengeMode::Complete()
{
    if (!IsActive())
        return;

    _active = false;
    _completed = true;

    // Freeze the run timer at its final value (keepTimer) so the completion time stays on screen,
    // and cancel the count-down widget, which is now meaningless.
    sElapsedTimerMgr->StopTimerForMap(_instance, WORLD_ELAPSED_TIMER_CHALLENGE_MODE, true);

    WorldPackets::Misc::StopTimer stopCountdown;
    stopCountdown.Type = CountdownTimerType::ChallengeMode;
    _instance->SendToPlayers(stopCountdown.Write());

    uint32 const effectiveTimeMs = GetEffectiveTimeMs();
    uint32 const keystoneUpgrade = sChallengeModeMgr.GetKeystoneUpgradeAmount(_mapChallengeModeId, effectiveTimeMs / IN_MILLISECONDS);

    float const runScore = sChallengeModeMgr.CalculateRunScore(_keystoneLevel, effectiveTimeMs, _timeLimitMs);

    // Record the run for every player present at completion (keeps the best per dungeon).
    MythicPlusRunRecord record;
    record.ChallengeModeID = _mapChallengeModeId;
    record.Level = _keystoneLevel;
    record.DurationMs = effectiveTimeMs;
    record.Deaths = _deathCount;
    record.CompletionDate = GameTime::GetGameTime();
    record.Score = runScore;
    record.Affixes = _affixes;

    bool const timed = keystoneUpgrade > 0;
    uint32 const completedMapId = _instance->GetId();
    _instance->DoOnPlayers([&record, timed, completedMapId](Player* player)
    {
        if (MythicPlusData* data = player->GetMythicPlusData())
        {
            bool const newBest = data->RecordRun(record);
            data->RecordWeeklyRun(record.ChallengeModeID, record.Level, timed, record.CompletionDate);

            // Push the refreshed rating to the client (Mythic+ UI / party frames read the update fields).
            player->UpdateDungeonScore();

            // Retail sends the personal + weekly record packets back-to-back on a new best (sniff-verified 12B each).
            if (newBest)
            {
                WorldPackets::ChallengeMode::ChallengeModeNewPlayerRecord playerRecord;
                playerRecord.MapChallengeModeID = record.ChallengeModeID;
                playerRecord.CompletionMs = record.DurationMs;
                playerRecord.KeystoneLevel = record.Level;
                player->SendDirectMessage(playerRecord.Write());

                WorldPackets::ChallengeMode::MythicPlusNewWeekRecord weekRecord;
                weekRecord.MapChallengeModeID = record.ChallengeModeID;
                weekRecord.CompletionMs = record.DurationMs;
                weekRecord.KeystoneLevel = record.Level;
                player->SendDirectMessage(weekRecord.Write());
            }
        }

        // Great Vault: a completed Mythic+ keystone credits the Dungeon row at its true keystone level (the level
        // drives the reward tier). This is the only thing that feeds the Dungeon row — regular/heroic/mythic-0
        // dungeon boss kills do not (see InstanceScript encounter DONE, which only credits the Raid row).
        sWeeklyRewardsMgr.RecordActivity(player, WeeklyRewards::ActivityType::Dungeon, record.Level);
        // Achievement criteria. All three are keyed off the run that just finished; the run-scoped
        // ModifierTree nodes (MythicPlusKeystoneLevelEqualOrGreaterThan 247, MythicPlusCompletedInTime 248,
        // MythicPlusMapChallengeMode 249) read the still-live ChallengeMode off the player's map, so these
        // must be fired here - while the players are still inside the instance - and not on a later tick.
        //   216 MythicPlusCompleted        : no asset; miscValue1 = keystone level, miscValue2 = MapChallengeMode
        //   71  CompleteChallengeMode      : Asset = MapID
        //   22  CompleteAnyChallengeMode   : no asset
        player->UpdateCriteria(CriteriaType::MythicPlusCompleted, record.Level, record.ChallengeModeID);
        player->UpdateCriteria(CriteriaType::CompleteChallengeMode, completedMapId, record.Level);
        player->UpdateCriteria(CriteriaType::CompleteAnyChallengeMode, record.Level, record.ChallengeModeID);

        // 230 MythicPlusRatingAttained: no asset - the required rating lives in the CriteriaTree Amount, so
        // this is a highest-watermark of the player's overall Mythic+ rating (sum of best runs).
        if (MythicPlusData const* data = player->GetMythicPlusData())
            player->UpdateCriteria(CriteriaType::MythicPlusRatingAttained, uint64(data->GetOverallScore()));
    });

    if (Player* starterPlayer = ObjectAccessor::GetPlayer(_instance, _starterGuid))
    {
        // The keystone already carries its depleted result (stamped at Start()). A timed clear re-stamps the
        // upgrade from the original level, keeping the dungeon rolled at activation -- retail-equivalent of
        // receiving the upgraded keystone at the end of the run.
        if (timed)
        {
            if (Item* keystone = starterPlayer->GetItemByGuid(_keystoneGuid))
            {
                uint32 dungeon = keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_MAP_CHALLENGE_MODE_ID);
                if (!dungeon)
                    dungeon = sChallengeModeMgr.RollSeasonDungeon(_mapChallengeModeId);
                sChallengeModeMgr.StampKeystone(keystone, dungeon, _keystoneLevel + keystoneUpgrade);
            }
        }

        // Stop the client dungeon timer.
        if (Group* group = starterPlayer->GetGroup())
            group->StartCountdown(CountdownTimerType::ChallengeMode, Seconds(0));
    }

    // Retail grants every present player without a keystone a fresh key at the end of the run, each rolled to
    // a random dungeon other than the one just completed (sniff: three ITEM_PUSHes, per-player dungeons).
    _instance->DoOnPlayers([this](Player* player)
    {
        if (!sChallengeModeMgr.GetKeystone(player))
        {
            uint32 const floorLevel = std::max(sChallengeModeMgr.GetKeystoneMinLevel(), sChallengeModeMgr.GetKeystoneFloor(player));
            if (uint32 dungeon = sChallengeModeMgr.RollSeasonDungeon(_mapChallengeModeId))
                sChallengeModeMgr.CreateOrUpdateKeystone(player, dungeon, floorLevel);
        }
    });

    // End-of-run crest reward: the bracketed Dawncrest amount (+2/level within the bracket, capped, reduced when
    // over time) to each player. Guarded on the currency existing, so a wrong/absent id is a safe no-op.
    if (uint32 crestId = sChallengeModeMgr.GetCrestCurrencyForLevel(_keystoneLevel))
    {
        if (sCurrencyTypesStore.LookupEntry(crestId))
        {
            uint32 const crestAmount = sChallengeModeMgr.GetCrestAmountForLevel(_keystoneLevel, timed);
            if (crestAmount)
                _instance->DoOnPlayers([crestId, crestAmount](Player* player)
                {
                    player->AddCurrency(crestId, crestAmount, CurrencyGainSource::Loot);
                });
        }
    }

    // End-of-run gear: retail awards a fixed number of items for the GROUP (2 timed / 1 untimed), personal-loot
    // distributed to random present players at the authentic Mythic+ item level. The item POOL is server content
    // (reference_loot_template keyed by ChallengeMode.Reward.LootId). Disabled (0) or empty template -> no-op.
    if (uint32 rewardLootId = sChallengeModeMgr.GetGearRewardLootId())
    {
        if (LootTemplates_Reference.HaveLootFor(rewardLootId))
        {
            std::vector<Player*> presentPlayers;
            _instance->DoOnPlayers([&presentPlayers](Player* player)
            {
                presentPlayers.push_back(player);
            });

            uint32 itemCount = timed
                ? uint32(sConfigMgr->GetIntDefault("ChallengeMode.Reward.ItemsTimed", 2))
                : uint32(sConfigMgr->GetIntDefault("ChallengeMode.Reward.ItemsUntimed", 1));

            if (!presentPlayers.empty())
                for (uint32 i = 0; i < itemCount; ++i)
                    AwardGearReward(Trinity::Containers::SelectRandomContainerElement(presentPlayers), rewardLootId);
        }
    }

    // Announce the result to the party (sniff-exact layout: run summary + score pair + member names).
    WorldPackets::ChallengeMode::ChallengeModeComplete completePacket;
    completePacket.MapChallengeModeID = _mapChallengeModeId;
    completePacket.KeystoneLevel = _keystoneLevel;
    completePacket.CompletionMs = effectiveTimeMs;
    completePacket.CompletionDate = record.CompletionDate;
    for (std::size_t i = 0; i < _affixes.size(); ++i)
        completePacket.Affixes[i] = _affixes[i];
    completePacket.Score = runScore;
    completePacket.BestScore = runScore;
    _instance->DoOnPlayers([&completePacket, runScore](Player* player)
    {
        if (MythicPlusData const* data = player->GetMythicPlusData())
            if (MythicPlusRunRecord const* best = data->GetBestRun(completePacket.MapChallengeModeID))
                completePacket.BestScore = std::max(runScore, best->Score);

        // Names list: the party members present at completion, shown on the client's run-result screen.
        WorldPackets::ChallengeMode::ChallengeModeComplete::MemberName& name = completePacket.Names.emplace_back();
        name.PlayerGUID = player->GetGUID();
        name.IsEligibleForScore = true;     // present members completed the run
        name.Name = player->GetName();
    });
    _instance->SendToPlayers(completePacket.Write());

    TC_LOG_INFO("challengemode", "ChallengeMode complete: instance {} challengeMode {} level {} time {}s (+{}s deaths, limit {}s) -> +{} keystone, score {:.1f}",
        _instance->GetInstanceId(), _mapChallengeModeId, _keystoneLevel, GetElapsedMs() / IN_MILLISECONDS,
        (_deathCount * GetDeathPenaltyMs()) / IN_MILLISECONDS, _timeLimitMs / IN_MILLISECONDS, keystoneUpgrade, runScore);
}

void ChallengeMode::AwardGearReward(Player* player, uint32 rewardLootId) const
{
    // Roll the operator-provided reward pool as personal loot tagged with the end-of-run context.
    Loot loot(player->GetMap(), ObjectGuid::Empty, LOOT_NONE, nullptr);
    loot.FillLoot(rewardLootId, LootTemplates_Reference, player, true /*personal*/, true /*noEmptyError*/,
        LOOT_MODE_DEFAULT, ItemContext::MythicPlus_End_of_Run);

    for (LootItem const& lootItem : loot.items)
    {
        if (!lootItem.itemid || !lootItem.count)
            continue;

        // Authentic Mythic+ item level: bonuses resolved from the end-of-run context + the keystone level
        // (ItemBonusMgr walks the reward-sequence curves 62951/62952/62954 by keystone band).
        std::vector<int32> bonuses = ItemBonusMgr::GetBonusListsForItem(lootItem.itemid,
            ItemBonusMgr::ItemBonusGenerationParams(ItemContext::MythicPlus_End_of_Run, int32(_keystoneLevel)));

        ItemPosCountVec dest;
        if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, lootItem.itemid, lootItem.count) == EQUIP_ERR_OK)
        {
            player->StoreNewItem(dest, lootItem.itemid, true, 0, GuidSet(), ItemContext::MythicPlus_End_of_Run, &bonuses);
        }
        else if (Item* item = Item::CreateItem(lootItem.itemid, lootItem.count, ItemContext::MythicPlus_End_of_Run, player, false))
        {
            // Bags full -> mail the reward (Blizzlike), carrying the same scaled bonuses.
            item->SetBonuses(bonuses);
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            item->SaveToDB(trans);
            MailDraft("Mythic Keystone Reward", "Your reward for completing a Mythic Keystone dungeon.")
                .AddItem(item)
                .SendMailTo(trans, player, MailSender(player, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED);
            CharacterDatabase.CommitTransaction(trans);
        }
    }
}

void ChallengeMode::BroadcastTimer(uint32 timeLeftMs) const
{
    WorldPackets::Misc::StartTimer startTimer;
    startTimer.Type = CountdownTimerType::ChallengeMode;
    startTimer.TotalTime = Seconds(_timeLimitMs / IN_MILLISECONDS);
    startTimer.TimeLeft = Seconds(timeLeftMs / IN_MILLISECONDS);
    _instance->SendToPlayers(startTimer.Write());
}
