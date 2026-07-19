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

#include "PerksProgramActivityMgr.h"
#include "AchievementPackets.h"
#include "CriteriaHandler.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include "DBCEnums.h"
#include "GameTime.h"
#include "Log.h"
#include "PerksProgramMgr.h"
#include "Player.h"
#include "SharedDefines.h"
#include "WorldSession.h"
#include <algorithm>

PerksProgramActivityMgr::PerksProgramActivityMgr(Player* owner) : _owner(owner)
{
}

PerksProgramActivityMgr::~PerksProgramActivityMgr() = default;

void PerksProgramActivityMgr::Reset()
{
    for (auto& criteriaProgress : _criteriaProgress)
        SendCriteriaProgressRemoved(criteriaProgress.first);

    _criteriaProgress.clear();
    _completedActivities.clear();
    _changed = true;

    DeleteFromDB(_owner->GetGUID());
}

uint64 PerksProgramActivityMgr::CurrentPeriodStart() const
{
    uint64 start = 0;
    uint64 end = 0;
    sPerksProgramMgr->GetCurrentPeriod(start, end);
    return start;
}

void PerksProgramActivityMgr::DeleteFromDB(ObjectGuid const& guid)
{
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_PERKS_ACTIVITY);
    stmt->setUInt64(0, guid.GetCounter());
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_PERKS_ACTIVITY_CRITERIA);
    stmt->setUInt64(0, guid.GetCounter());
    trans->Append(stmt);

    CharacterDatabase.CommitTransaction(trans);
}

void PerksProgramActivityMgr::LoadFromDB(PreparedQueryResult activityResult, PreparedQueryResult criteriaResult)
{
    _periodStart = CurrentPeriodStart();
    bool rolledOver = false;

    if (activityResult)
    {
        do
        {
            Field* fields = activityResult->Fetch();
            uint32 activityId = fields[0].GetUInt32();
            uint64 rowPeriod = fields[1].GetUInt64();
            if (rowPeriod != _periodStart)
            {
                rolledOver = true;
                continue;
            }
            if (sPerksActivityStore.LookupEntry(activityId))
                _completedActivities.insert(activityId);
        } while (activityResult->NextRow());
    }

    if (criteriaResult)
    {
        do
        {
            Field* fields = criteriaResult->Fetch();
            uint32 criteriaId = fields[0].GetUInt32();
            uint64 counter = fields[1].GetUInt64();
            time_t date = fields[2].GetInt64();
            uint64 rowPeriod = fields[3].GetUInt64();
            if (rowPeriod != _periodStart)
            {
                rolledOver = true;
                continue;
            }
            if (!sCriteriaMgr->GetCriteria(criteriaId))
                continue;

            CriteriaProgress& progress = _criteriaProgress[criteriaId];
            progress.Counter = counter;
            progress.Date = date;
            progress.Changed = false;
        } while (criteriaResult->NextRow());
    }

    // Any stored row from a previous Trading Post interval means the whole set is stale: wipe the
    // completions and criteria so the new month starts fresh (and clean up the old rows).
    if (rolledOver)
    {
        _completedActivities.clear();
        _criteriaProgress.clear();
        DeleteFromDB(_owner->GetGUID());
    }
}

void PerksProgramActivityMgr::SaveToDB(CharacterDatabaseTransaction trans)
{
    if (!_periodStart)
        _periodStart = CurrentPeriodStart();

    if (_changed)
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_PERKS_ACTIVITY);
        stmt->setUInt64(0, _owner->GetGUID().GetCounter());
        trans->Append(stmt);

        for (uint32 activityId : _completedActivities)
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHAR_PERKS_ACTIVITY);
            stmt->setUInt64(0, _owner->GetGUID().GetCounter());
            stmt->setUInt32(1, activityId);
            stmt->setUInt64(2, _periodStart);
            trans->Append(stmt);
        }

        _changed = false;
    }

    // Persist changed in-progress criteria counters (mirrors QuestObjectiveCriteriaMgr).
    for (auto& criteriaProgress : _criteriaProgress)
    {
        if (!criteriaProgress.second.Changed)
            continue;

        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_PERKS_ACTIVITY_CRITERIA_BY_CRITERIA);
        stmt->setUInt64(0, _owner->GetGUID().GetCounter());
        stmt->setUInt32(1, criteriaProgress.first);
        trans->Append(stmt);

        if (criteriaProgress.second.Counter)
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHAR_PERKS_ACTIVITY_CRITERIA);
            stmt->setUInt64(0, _owner->GetGUID().GetCounter());
            stmt->setUInt32(1, criteriaProgress.first);
            stmt->setUInt64(2, criteriaProgress.second.Counter);
            stmt->setInt64(3, criteriaProgress.second.Date);
            stmt->setUInt64(4, _periodStart);
            trans->Append(stmt);
        }

        criteriaProgress.second.Changed = false;
    }
}

void PerksProgramActivityMgr::SendAllData(Player const* /*receiver*/) const
{
    for (auto const& criteriaProgress : _criteriaProgress)
    {
        WorldPackets::Achievement::CriteriaUpdate criteriaUpdate;
        criteriaUpdate.CriteriaID = criteriaProgress.first;
        criteriaUpdate.Quantity = criteriaProgress.second.Counter;
        criteriaUpdate.PlayerGUID = _owner->GetGUID();
        criteriaUpdate.Flags = 0;
        criteriaUpdate.CurrentTime.SetUtcTimeFromUnixTime(criteriaProgress.second.Date);
        criteriaUpdate.CurrentTime += _owner->GetSession()->GetTimezoneOffset();
        criteriaUpdate.CreationTime = 0;
        SendPacket(criteriaUpdate.Write());
    }
}

void PerksProgramActivityMgr::SendCriteriaUpdate(Criteria const* criteria, CriteriaProgress const* progress, Seconds timeElapsed, bool /*timedCompleted*/) const
{
    WorldPackets::Achievement::CriteriaUpdate criteriaUpdate;
    criteriaUpdate.CriteriaID = criteria->ID;
    criteriaUpdate.Quantity = progress->Counter;
    criteriaUpdate.PlayerGUID = _owner->GetGUID();
    criteriaUpdate.Flags = 0;
    criteriaUpdate.CurrentTime.SetUtcTimeFromUnixTime(progress->Date);
    criteriaUpdate.CurrentTime += _owner->GetSession()->GetTimezoneOffset();
    criteriaUpdate.ElapsedTime = timeElapsed;
    criteriaUpdate.CreationTime = 0;
    SendPacket(criteriaUpdate.Write());
}

void PerksProgramActivityMgr::SendCriteriaProgressRemoved(uint32 criteriaId)
{
    WorldPackets::Achievement::CriteriaDeleted criteriaDeleted;
    criteriaDeleted.CriteriaID = criteriaId;
    SendPacket(criteriaDeleted.Write());
}

bool PerksProgramActivityMgr::CanUpdateCriteriaTree(Criteria const* criteria, CriteriaTree const* tree, Player* referencePlayer) const
{
    PerksActivityEntry const* activity = tree->PerksActivity;
    if (!activity)
        return false;

    if (_completedActivities.contains(activity->ID))
        return false;

    return CriteriaHandler::CanUpdateCriteriaTree(criteria, tree, referencePlayer);
}

bool PerksProgramActivityMgr::CanCompleteCriteriaTree(CriteriaTree const* tree)
{
    if (!tree->PerksActivity)
        return false;

    return CriteriaHandler::CanCompleteCriteriaTree(tree);
}

void PerksProgramActivityMgr::CompletedCriteriaTree(CriteriaTree const* tree, Player* /*referencePlayer*/)
{
    PerksActivityEntry const* activity = tree->PerksActivity;
    if (!activity)
        return;

    // Only mark the activity done once its entire criteria tree (rooted at PerksActivity.CriteriaTreeID)
    // is satisfied.
    CriteriaTree const* activityTree = sCriteriaMgr->GetCriteriaTree(activity->CriteriaTreeID);
    if (activityTree && IsCompletedCriteriaTree(activityTree))
        CompleteActivity(activity);
}

int64 PerksProgramActivityMgr::ContributionTotal() const
{
    int64 total = 0;
    for (uint32 activityId : _completedActivities)
        if (PerksActivityEntry const* activity = sPerksActivityStore.LookupEntry(activityId))
            total += activity->ThresholdContributionAmount;
    return total;
}

void PerksProgramActivityMgr::CompleteActivity(PerksActivityEntry const* activity)
{
    if (_completedActivities.contains(activity->ID))
        return;

    int64 const oldTotal = ContributionTotal();
    _completedActivities.insert(activity->ID);
    _changed = true;
    int64 const newTotal = oldTotal + activity->ThresholdContributionAmount;

    TC_LOG_INFO("criteria", "PerksProgramActivityMgr::CompleteActivity({}). {}", activity->ID, GetOwnerInfo());

    AwardThresholds(oldTotal, newTotal);

    // Refresh the client's Trading Post activity state with the new completion.
    _owner->GetSession()->SendPerksProgramActivityUpdate();
}

// Awards Trader's Tender for every current-interval threshold newly crossed by moving the running
// contribution from oldTotal to newTotal. Because completions only ever move the total upward and
// LoadFromDB seeds the completed set without calling this, each threshold is granted exactly once.
void PerksProgramActivityMgr::AwardThresholds(int64 oldTotal, int64 newTotal)
{
    // Current Trading Post interval = the threshold group(s) with the highest PerksMonth.
    int32 currentMonth = -1;
    for (PerksActivityThresholdGroupEntry const* group : sPerksActivityThresholdGroupStore)
        currentMonth = std::max(currentMonth, group->PerksMonth);
    if (currentMonth < 0)
        return;

    std::unordered_set<uint32> currentGroups;
    for (PerksActivityThresholdGroupEntry const* group : sPerksActivityThresholdGroupStore)
        if (group->PerksMonth == currentMonth)
            currentGroups.insert(group->ID);

    for (PerksActivityThresholdEntry const* threshold : sPerksActivityThresholdStore)
    {
        if (!currentGroups.contains(uint32(threshold->PerksActivityThresholdGroupID)))
            continue;

        if (threshold->BonusTendies <= 0)
            continue;

        if (int64(threshold->Threshold) > oldTotal && int64(threshold->Threshold) <= newTotal)
            _owner->AddCurrency(CURRENCY_TYPE_TRADERS_TENDER, uint32(threshold->BonusTendies), CurrencyGainSource::Script);
    }
}

void PerksProgramActivityMgr::SendPacket(WorldPacket const* data) const
{
    _owner->SendDirectMessage(data);
}

std::string PerksProgramActivityMgr::GetOwnerInfo() const
{
    return Trinity::StringFormat("{} {}", _owner->GetGUID().ToString(), _owner->GetName());
}

CriteriaList const& PerksProgramActivityMgr::GetCriteriaByType(CriteriaType type, uint32 /*asset*/) const
{
    return sCriteriaMgr->GetPerksActivityCriteriaByType(type);
}

bool PerksProgramActivityMgr::RequiredAchievementSatisfied(uint32 achievementId) const
{
    return _owner->HasAchieved(achievementId);
}
