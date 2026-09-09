// QuestDoable — the ONE shared "can this bot still take this quest" predicate,
// used everywhere a leveling-starved bot is routed to (or parked at) a quest
// hub. Centralizing it guarantees the offers builder, the R7 relocation
// synthesis (BotSnapshotBuilder), the cross-continent selector
// (QuestHubDatabase::SelectLevelingHub, which receives this predicate as a
// callback and invokes it AFTER releasing its _mutex), and the two
// reactive idle rules (travel_to_hub / walk_to_known_hub) all agree on which
// hubs are "exhausted" — so the builder and the rules can't disagree and make
// the bot oscillate, and a just-picked hub can't fail the very next reuse check.
//
// THREADING: both helpers call LIVE Player APIs (CanTakeQuest, GetSkillValue,
// GetQuestRewardStatus). They MUST only be invoked where the live Player is
// owned by the calling thread — i.e. the snapshot BUILD path (world thread) and
// the idle dispatch (world thread). They MUST NOT be called from the AI worker
// hot path, and in particular MUST NOT run while QuestHubDatabase's _mutex is
// held (SelectLevelingHub takes this predicate as a callback and runs it only
// after dropping the lock — see QuestHubDatabase::SelectLevelingHub).

#pragma once

#include "Define.h"

class Player;

namespace Playerbot::V2::Travel { struct QuestHub; }

namespace Playerbot {

// True iff the bot does NOT already have the quest rewarded, the template
// exists, it is not pure-repeatable, has no structurally-uncompletable
// objective, the bot knows the required skill (if any), and CanTakeQuest
// accepts it. Mirrors the offers builder's per-quest acceptance gate so the two
// can never drift — the offers builder routes through this exact predicate.
[[nodiscard]] bool BotCanStillTakeQuestForHub(Player* p, uint32 questId);

// True at the FIRST doable quest in the hub (early-exit; O(first-doable), not a
// full scan). False when the hub is exhausted for this bot.
[[nodiscard]] bool HubHasDoableQuest(Player* p, ::Playerbot::V2::Travel::QuestHub const& h);

} // namespace Playerbot
