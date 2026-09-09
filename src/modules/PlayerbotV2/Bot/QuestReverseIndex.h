// QuestReverseIndex - precomputed reverse-lookup tables for kill-credit
// aliasing and creature-label objective handling.
//
// Built once on first use (lazy init guarded by std::once_flag) by walking
// every CreatureTemplate in ObjectMgr. Both indices fit easily in memory:
// roughly 1 entry per template that has a non-zero KillCredit slot, and
// 1 entry per (template × label) pair. Read-only post-init; safe to query
// from any thread.
//
// Used by:
//  - QuestHasObjectiveBotCannotComplete: MONSTER objectives with zero
//    spawns on the listed entry are still achievable when *another*
//    creature aliases through KillCredit[i].
//  - BotSnapshotBuilder: populates per-objective `credit_alias_entries`
//    and `labeled_target_entries` so the kill rule can chase aliasing
//    or label-tagged mobs without a per-tick reverse-index walk.

#pragma once

#include "Define.h"
#include <vector>

namespace Playerbot {

// Returns the list of CreatureTemplate.entry values whose KillCredit[0..2]
// includes `credit_entry`. Empty span when no aliases exist (the common
// case — most quest mobs award credit on their own entry).
std::vector<uint32> const& KillCreditAliasesFor(uint32 credit_entry);

// Returns the list of CreatureTemplate.entry values whose creature_labels
// row contains `label`. Used by KILL_WITH_LABEL (type 21) objectives.
// Empty span when no creature carries the label (i.e., quest is content
// the bot can't reach without GM spawning).
std::vector<uint32> const& CreaturesWithLabel(int32 label);

// Force-build the indices. Idempotent. Called from the snapshot builder
// guard; explicit calls during module init also fine.
void EnsureQuestReverseIndicesBuilt();

// True when a quest carries an objective NO bot can ever satisfy through play:
// pet-battle objectives, blacklisted/zero-ender junk, or a MONSTER/TALKTO
// credit target that is an abstract NOT_SPECIFIED marker / has no world spawn
// and no KillCredit alias (e.g. the DK vehicle-credit proxies). Process-wide
// memoized; thread-safe to query. Defined in BotSnapshotBuilder.cpp alongside
// the reverse-index helpers it consumes. JunkQuestResolver uses it to route
// these to force-complete+reward (advance the chain) instead of abandon — see
// that file's note: abandoning a chain quest a bot can't do (DK 12779→12800)
// would strand the bot in its starting zone.
bool QuestHasObjectiveBotCannotComplete(uint32 quest_id);

} // namespace Playerbot
