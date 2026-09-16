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

#ifndef TRINITYCORE_LOREWALKING_MGR_H
#define TRINITYCORE_LOREWALKING_MGR_H

#include "Define.h"
#include <unordered_set>

class Quest;

namespace Lorewalking
{
// Aura every story applies while the character is Lorewalking (aura 510 sets CTROptions conditional flag 13)
inline constexpr uint32 SPELL_LOREWALKING = 463943;

// QuestInfo.db2 Modifiers bit of quests that stay in the visible quest log in every Lorewalking state
// (Emissary, Island Weekly, Hidden and Threat Emissary quests)
inline constexpr int32 QUEST_INFO_MODIFIER_KEEP_IN_QUEST_LOG = 0x10;

struct Story
{
    uint32 ID = 0;
    uint32 ResponseBegin = 0;
    uint32 ResponseContinue = 0;
    uint32 ResponseStartOver = 0;
    uint32 LaunchSpellID = 0;
    uint32 EnterSpellID = 0;
    uint32 IntroSpellID = 0;
    uint32 AskQuestionSpellID = 0;
    uint32 FirstQuestID = 0;
    std::unordered_set<int32> Maps;    // instance maps the story itself takes the player into
};

void Load();

TC_GAME_API Story const* GetStory(uint32 storyId);
TC_GAME_API Story const* GetStoryByResponse(uint32 responseId);
TC_GAME_API Story const* GetStoryByEnterSpell(uint32 spellId);

// Quests that never leave the visible quest log (see QUEST_INFO_MODIFIER_KEEP_IN_QUEST_LOG)
TC_GAME_API bool IsQuestKeptInQuestLog(Quest const* quest);
}

#endif // TRINITYCORE_LOREWALKING_MGR_H
