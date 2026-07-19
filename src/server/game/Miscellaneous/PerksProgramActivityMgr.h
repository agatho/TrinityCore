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

#ifndef TRINITYCORE_PERKS_PROGRAM_ACTIVITY_MGR_H
#define TRINITYCORE_PERKS_PROGRAM_ACTIVITY_MGR_H

#include "CriteriaHandler.h"
#include "DatabaseEnvFwd.h"
#include <unordered_set>

// Per-player tracker for Trading Post (Perks Program) monthly activities. Each PerksActivity row has
// a CriteriaTreeID; the criteria are routed here through Player::UpdateCriteria (like quest
// objective / scenario criteria). When an activity's criteria tree completes it is marked done,
// persisted, and the crossed PerksActivityThreshold rows award Trader's Tender. The completed set
// feeds SMSG_PERKS_PROGRAM_ACTIVITY_UPDATE.
class TC_GAME_API PerksProgramActivityMgr : public CriteriaHandler
{
public:
    explicit PerksProgramActivityMgr(Player* owner);
    ~PerksProgramActivityMgr();

    void Reset() override;

    static void DeleteFromDB(ObjectGuid const& guid);
    void LoadFromDB(PreparedQueryResult activityResult);
    void SaveToDB(CharacterDatabaseTransaction trans);

    void SendAllData(Player const* receiver) const override;

    std::unordered_set<uint32> const& GetCompletedActivities() const { return _completedActivities; }

protected:
    void SendCriteriaUpdate(Criteria const* entry, CriteriaProgress const* progress, Seconds timeElapsed, bool timedCompleted) const override;
    void SendCriteriaProgressRemoved(uint32 criteriaId) override;

    bool CanUpdateCriteriaTree(Criteria const* criteria, CriteriaTree const* tree, Player* referencePlayer) const override;
    bool CanCompleteCriteriaTree(CriteriaTree const* tree) override;
    void CompletedCriteriaTree(CriteriaTree const* tree, Player* referencePlayer) override;

    void SendPacket(WorldPacket const* data) const override;

    std::string GetOwnerInfo() const override;
    CriteriaList const& GetCriteriaByType(CriteriaType type, uint32 asset) const override;

    bool RequiredAchievementSatisfied(uint32 achievementId) const override;

private:
    // Marks the activity completed (idempotent), persists nothing directly (SaveToDB does that), then
    // awards any PerksActivityThreshold whose cumulative requirement is now met.
    void CompleteActivity(PerksActivityEntry const* activity);
    void AwardThresholds(int64 oldTotal, int64 newTotal);
    int64 ContributionTotal() const;

    Player* _owner;
    std::unordered_set<uint32> _completedActivities;
    bool _changed = false;
};

#endif // TRINITYCORE_PERKS_PROGRAM_ACTIVITY_MGR_H
