// JunkQuestResolver — clears quest-log pollution that strands bots in the
// "no actionable quest nearby" state (the dominant GoalUnreachable wedge,
// 2026-06-15: 177 distinct bots; root cause = the 25/35-slot log filling with
// quests the bot can never resolve, so the objective picker reports
// picker_none and the bot falls through to local idle rules forever).
//
// Two classes of pollution, both resolved here on the world thread (the
// resolver mutates the live Player, so it runs from the intent executor):
//
//   1. AUTO-GRANTED FEATURE quests (Quest::IsAutoPush()): e.g. 55660 "Time
//      Trials" (held by 12,205 bot chars), 84224 "To Delves!". TC core
//      Player::PushQuests() force-adds every QUEST_FLAGS_EX_AUTO_PUSH quest at
//      EVERY login. They have no walkable turn-in, so a bot can never complete
//      them through play, and ABANDONING them does not stick — the next login
//      re-pushes them (CanTakeQuest passes for a non-rewarded quest). The fix
//      is to force-COMPLETE them to REWARDED: SatisfyQuestStatus then rejects
//      QUEST_STATUS_REWARDED, so PushQuests skips them forever after.
//
//   2. PROFESSION-SPECIALIZATION choice quests (e.g. Alchemy's Potion/Elixir/
//      Transmutation Master, Engineering's Goblin/Gnomish): a bot accepts one
//      in profession mode but never resolves the mutually-exclusive choice, so
//      they sit in the log. Per the 2026-06-15 directive: if the bot HAS the
//      profession, pick ONE specialization, force-complete it, abandon the
//      rest; if it lacks the profession, abandon all held ones.
//
// Classic (non-feature, non-spec) quests are intentionally LEFT ALONE — bots
// should travel to do them; travel-engagement is observed separately.
//
// One reward-causing action per call (Player::RewardQuest force-saves the
// character row; batching N rewards would stack N full saves on the DB row
// lock — see StarterQuestAutocomplete's same discipline). The caller re-fires
// until the log is clean.

#pragma once

#include "Bot/BotTypes.h"
#include <cstdint>

class Player;

namespace Playerbot::V2::Fleet {

struct JunkResolveResult
{
    uint32 rewarded;    // feature/spec quests force-completed this call (0 or 1)
    uint32 abandoned;   // spec quests abandoned this call
    bool   done;        // true when no resolvable junk remains in the log
};

class JunkQuestResolver
{
public:
    // Process the bot's quest log once. Idempotent and safe to call every
    // few seconds from the idle path — returns done=true cheaply when the log
    // holds no resolvable junk. Must run on the world thread (mutates Player).
    static JunkResolveResult RunFor(Player* bot);

    // Cheap predicate for the snapshot builder / idle-rule gate: does this
    // quest_id need the resolver (auto-push feature OR a known profession-spec
    // choice)? Cached. Thread-safe (read-only sObjectMgr lookups).
    static bool IsResolvableJunk(uint32 quest_id);
};

} // namespace Playerbot::V2::Fleet
