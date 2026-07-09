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
#include "Containers.h"
#include "Creature.h"
#include "DB2Stores.h"
#include "GameTime.h"
#include "Group.h"
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
    if (Player* starterPlayer = ObjectAccessor::GetPlayer(*_instance, _starterGuid))
        if (Group* group = starterPlayer->GetGroup())
            group->StartCountdown(CountdownTimerType::ChallengeMode, Seconds(_timeLimitMs / IN_MILLISECONDS));

    BroadcastTimer(_timeLimitMs);

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

    TC_LOG_INFO("challengemode", "ChallengeMode start: instance {} challengeMode {} level {} timeLimit {}s",
        _instance->GetInstanceId(), mapChallengeModeId, keystoneLevel, _timeLimitMs / IN_MILLISECONDS);
}

void ChallengeMode::Reset()
{
    // Stop the client dungeon timer if a run was in progress.
    if (Player* starterPlayer = ObjectAccessor::GetPlayer(*_instance, _starterGuid))
        if (Group* group = starterPlayer->GetGroup())
            group->StartCountdown(CountdownTimerType::ChallengeMode, Seconds(0));

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
        {
            data->RecordRun(record);
            data->RecordWeeklyRun(record.ChallengeModeID, record.Level, record.CompletionDate);
        }
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

    // End-of-run crest reward: award the season crest of the tier matching the keystone level to each player.
    // Currency ids are extracted from CurrencyTypes.db2 (Midnight S1 Dawncrests); tier + amount are config-tunable.
    // Guarded on the currency existing, so a wrong/absent id is a safe no-op.
    if (uint32 crestId = sChallengeModeMgr.GetCrestCurrencyForLevel(_keystoneLevel))
    {
        if (sCurrencyTypesStore.LookupEntry(crestId))
        {
            uint32 const crestAmount = sChallengeModeMgr.GetCrestAmount();
            if (crestAmount)
                _instance->DoOnPlayers([crestId, crestAmount](Player* player)
                {
                    player->AddCurrency(crestId, crestAmount, CurrencyGainSource::Loot);
                });
        }
    }

    // End-of-run gear reward: roll the configured reward loot for each player and grant every item at the
    // authentic Mythic+ item level. The item-level scaling is real (ItemBonusMgr resolves the end-of-run context +
    // keystone level through the reward-sequence curves); the item POOL is server content
    // (reference_loot_template keyed by ChallengeMode.Reward.LootId). Disabled (0) or empty template -> no-op.
    if (uint32 rewardLootId = sChallengeModeMgr.GetGearRewardLootId())
    {
        if (LootTemplates_Reference.HaveLootFor(rewardLootId))
            _instance->DoOnPlayers([this, rewardLootId](Player* player)
            {
                AwardGearReward(player, rewardLootId);
            });
    }

    // Announce the result to the party (map/level/affixes + present players as members). The per-run
    // DungeonScoreData sub-lists are sent empty (not persisted server-side); the wire is exact (no desync).
    WorldPackets::ChallengeMode::ChallengeModeComplete completePacket;
    completePacket.MapSummary.MapChallengeModeID = _mapChallengeModeId;
    completePacket.MapSummary.BestLevel = _keystoneLevel;
    completePacket.MapSummary.DurationMs = effectiveTimeMs;
    completePacket.MapSummary.Affixes = _affixes;
    _instance->DoOnPlayers([&completePacket](Player* player)
    {
        WorldPackets::ChallengeMode::MythicPlusMapStatMember& member = completePacket.MapSummary.Members.emplace_back();
        member.PlayerGUID = player->GetGUID();

        // Names list: the party members present at completion, shown on the client's run-result screen.
        WorldPackets::ChallengeMode::ChallengeModeComplete::MemberName& name = completePacket.Names.emplace_back();
        name.PlayerGUID = player->GetGUID();
        name.IsEligibleForScore = true;     // present members completed the run
        name.Name = player->GetName();
    });
    _instance->SendToPlayers(completePacket.Write());

    TC_LOG_INFO("challengemode", "ChallengeMode complete: instance {} challengeMode {} level {} time {}s (+{}s deaths, limit {}s) -> +{} keystone, score {:.1f}",
        _instance->GetInstanceId(), _mapChallengeModeId, _keystoneLevel, GetElapsedMs() / IN_MILLISECONDS,
        (_deathCount * DEATH_TIME_PENALTY_MS) / IN_MILLISECONDS, _timeLimitMs / IN_MILLISECONDS, keystoneUpgrade, runScore);
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
