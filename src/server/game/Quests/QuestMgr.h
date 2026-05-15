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

#ifndef TRINITYCORE_QUEST_MGR_H
#define TRINITYCORE_QUEST_MGR_H

#include "Define.h"
#include <span>
#include <string>

struct QuestLineXQuestEntry;
class Player;

namespace QuestMgr
{
void Load();

// QuestLine
TC_GAME_API std::span<QuestLineXQuestEntry const* const> GetQuestsForQuestLine(uint32 questLineId);

TC_GAME_API bool IsQuestLineQuestAvailableForPlayer(uint32 questLineId, Player const* player);

TC_GAME_API bool IsQuestLineQuestActiveForPlayer(uint32 questLineId, Player const* player);

TC_GAME_API bool IsQuestLineCompletedByPlayer(uint32 questLineId, Player const* player);

struct QuestLineStats { uint32 Completed = 0; uint32 Total = 0; };
TC_GAME_API QuestLineStats GetQuestLineStatsForPlayer(uint32 questLineId, Player const* player);

TC_GAME_API void SkipQuestLineForPlayer(uint32 questLineId, Player* player);

// Campaign
TC_GAME_API bool IsCampaignCompletedByPlayer(uint32 campaignId, Player const* player);

TC_GAME_API bool IsCampaignQuestStatusVisibleForPlayer(uint32 questId, Player const* player);

TC_GAME_API void SkipCampaignForPlayer(uint32 campaignId, Player* player);

// Phase 10F - campaign reward / stall-tooltip helpers.

// When a quest is completed: look up campaigns whose `Completed` quest field
// matches questId and, for each, if the campaign carries a RewardQuestID,
// auto-grant the reward quest to the player. Mirrors the retail behavior
// where finishing the campaign's completion quest hands the reward quest.
TC_GAME_API void OnQuestCompletedHandleCampaignReward(Player* player, uint32 completedQuestId);

// Look up the localized failure-reason string for a stalled campaign. Walks
// CampaignXCondition rows for the campaign in OrderIndex order and returns
// the first row whose PlayerConditionID is NOT met. Returns an empty
// string if the campaign is not stalled or no rows exist.
TC_GAME_API std::string GetCampaignStallFailureReason(uint32 campaignId, Player const* player);
}

#endif // TRINITYCORE_CAMPAIGN_MGR_H
