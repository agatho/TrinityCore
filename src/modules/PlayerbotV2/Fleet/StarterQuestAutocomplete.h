// StarterQuestAutocomplete - Programmatically completes the racial / class
// intro questline a distribution-leveled bot would otherwise skip.
//
// The problem: when BotPopulationManager spawns a new DK / DH / Evoker /
// allied-race bot, BotCharacterFactory creates them at the class's start
// level (DK=8, DH=8, Evoker=10, allied=10), then GiveLevel jumps them to
// the bracket midpoint. The intervening starter questline — which grants
// baseline class abilities (Death Coil, Death Grip, Eye Beam, Living
// Flame, etc.) and racial heritage rewards — never runs. The bot ends up
// with the wrong-shaped spell book.
//
// This class walks `quest_template`, picks quests whose `AllowableClasses`
// or `AllowableRaces` masks match the bot, completes them in PrevQuestID
// dependency order, and fires the standard reward path so spell rewards
// land in the spellbook and item rewards land in the bag.
//
// Runs as a step inside BotSetupPipeline::DoSetLevel before GiveLevel —
// quests are picked up at the natural starter level so the level / quest
// gating in `Player::CanTakeQuest` is satisfied.

#pragma once

#include "Bot/BotTypes.h"

class Player;

namespace Playerbot::V2::Fleet {

struct RunForResult
{
    uint32 completed;     // count of quests we rewarded this call
    bool   all_processed; // false if we bailed on the 5s wall-clock budget
};

class StarterQuestAutocomplete
{
public:
    // Walks all quest_template entries and auto-completes any whose race /
    // class mask match the bot AND whose minLevel is within the bot's
    // current level. Returns count + whether we processed every candidate
    // (false when we hit the 5s wall-clock budget mid-loop). The pipeline
    // step uses all_processed to decide whether to retry the bot.
    //
    // Idempotent — quests already in REWARDED state are skipped.
    // Loop terminates when no new progress is made or after kMaxRounds.
    static RunForResult RunFor(Player* bot);
};

} // namespace Playerbot::V2::Fleet
