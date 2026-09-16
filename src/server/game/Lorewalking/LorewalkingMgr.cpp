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

#include "LorewalkingMgr.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "Log.h"
#include "MapUtils.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Timer.h"
#include <unordered_map>

namespace
{
std::unordered_map<uint32, Lorewalking::Story> Stories;
std::unordered_map<uint32, Lorewalking::Story const*> StoriesByResponse;
std::unordered_map<uint32, Lorewalking::Story const*> StoriesByEnterSpell;
}

void Lorewalking::Load()
{
    uint32 oldMSTime = getMSTime();

    Stories.clear();
    StoriesByResponse.clear();
    StoriesByEnterSpell.clear();

    //                                                    0     1              2                 3                  4
    QueryResult result = WorldDatabase.Query("SELECT StoryID, ResponseBegin, ResponseContinue, ResponseStartOver, LaunchSpellID, "
    //   5             6             7                   8
        "EnterSpellID, IntroSpellID, AskQuestionSpellID, FirstQuestID FROM lorewalking_story");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 Lorewalking stories. DB table `lorewalking_story` is empty.");
        return;
    }

    auto checkSpell = [](uint32 storyId, char const* column, uint32 spellId)
    {
        if (spellId && !sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
        {
            TC_LOG_ERROR("sql.sql", "Table `lorewalking_story` StoryID {} has non-existing spell {} in {}, ignored.", storyId, spellId, column);
            return 0u;
        }
        return spellId;
    };

    do
    {
        Field* fields = result->Fetch();

        Story story;
        story.ID = fields[0].GetUInt32();
        story.ResponseBegin = fields[1].GetUInt32();
        story.ResponseContinue = fields[2].GetUInt32();
        story.ResponseStartOver = fields[3].GetUInt32();
        story.LaunchSpellID = checkSpell(story.ID, "LaunchSpellID", fields[4].GetUInt32());
        story.EnterSpellID = checkSpell(story.ID, "EnterSpellID", fields[5].GetUInt32());
        story.IntroSpellID = checkSpell(story.ID, "IntroSpellID", fields[6].GetUInt32());
        story.AskQuestionSpellID = checkSpell(story.ID, "AskQuestionSpellID", fields[7].GetUInt32());
        story.FirstQuestID = fields[8].GetUInt32();

        if (!story.LaunchSpellID || !story.EnterSpellID)
        {
            TC_LOG_ERROR("sql.sql", "Table `lorewalking_story` StoryID {} has no valid LaunchSpellID or EnterSpellID, skipped.", story.ID);
            continue;
        }

        if (!sObjectMgr->GetQuestTemplate(story.FirstQuestID))
        {
            TC_LOG_ERROR("sql.sql", "Table `lorewalking_story` StoryID {} has non-existing FirstQuestID {}, skipped.", story.ID, story.FirstQuestID);
            continue;
        }

        Stories.emplace(story.ID, std::move(story));
    } while (result->NextRow());

    //                                        0        1
    if (QueryResult maps = WorldDatabase.Query("SELECT StoryID, MapID FROM lorewalking_story_map"))
    {
        do
        {
            Field* fields = maps->Fetch();
            uint32 storyId = fields[0].GetUInt32();
            int32 mapId = fields[1].GetInt32();

            Story* story = Trinity::Containers::MapGetValuePtr(Stories, storyId);
            if (!story)
            {
                TC_LOG_ERROR("sql.sql", "Table `lorewalking_story_map` has non-existing StoryID {}, skipped.", storyId);
                continue;
            }

            if (!sMapStore.LookupEntry(mapId))
            {
                TC_LOG_ERROR("sql.sql", "Table `lorewalking_story_map` StoryID {} has non-existing MapID {}, skipped.", storyId, mapId);
                continue;
            }

            story->Maps.insert(mapId);
        } while (maps->NextRow());
    }

    for (auto const& [storyId, story] : Stories)
    {
        for (uint32 responseId : { story.ResponseBegin, story.ResponseContinue, story.ResponseStartOver })
            if (responseId)
                StoriesByResponse[responseId] = &story;

        StoriesByEnterSpell[story.EnterSpellID] = &story;
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} Lorewalking stories in {} ms", Stories.size(), GetMSTimeDiffToNow(oldMSTime));
}

Lorewalking::Story const* Lorewalking::GetStory(uint32 storyId)
{
    return Trinity::Containers::MapGetValuePtr(Stories, storyId);
}

Lorewalking::Story const* Lorewalking::GetStoryByResponse(uint32 responseId)
{
    return Trinity::Containers::MapGetValuePtr(StoriesByResponse, responseId);
}

Lorewalking::Story const* Lorewalking::GetStoryByEnterSpell(uint32 spellId)
{
    return Trinity::Containers::MapGetValuePtr(StoriesByEnterSpell, spellId);
}

bool Lorewalking::IsQuestKeptInQuestLog(Quest const* quest)
{
    if (QuestInfoEntry const* questInfo = sQuestInfoStore.LookupEntry(quest->GetQuestInfoID()))
        return (questInfo->Modifiers & QUEST_INFO_MODIFIER_KEEP_IN_QUEST_LOG) != 0;

    return false;
}
