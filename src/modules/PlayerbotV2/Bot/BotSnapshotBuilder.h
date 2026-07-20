// BotSnapshotBuilder - Builds a BotSnapshot from a Player*. World thread only.
//
// First-iteration scope: identity, vitals, position, simple cooldowns, current
// target. Auras / nearby units / inventory will land as the corresponding
// PlayerbotAPI accessors gain coverage.

#pragma once

#include "BotSnapshot.h"
#include <memory>

class Player;

namespace Playerbot {

class BotAI;

class BotSnapshotBuilder
{
public:
    // Returns null if `p` is null or not in world. Otherwise returns a fresh
    // BotSnapshot populated from `p`. Pure read of Player (no mutation).
    //
    // `bot_ai` MUST be the BotAI* for `p` (i.e. what Registry().ai(p's id)
    // returns), pre-resolved by the caller ON THE WORLD THREAD and threaded in
    // here. Build runs on a parallel build-pool worker (#5 Phase 4), where
    // calling Registry().ai() is unsafe: ai() is an unlocked map lookup that
    // races a concurrent rehash from bot login/logout and can return a garbage
    // pointer. Resolving once on the world thread (where the registry is
    // quiescent w.r.t. this tick) eliminates that race while keeping behavior
    // byte-identical (same pointer, resolved once instead of ~32 times). May be
    // null if the bot has no registered AI (Build tolerates a null bot_ai
    // everywhere it is used).
    static std::shared_ptr<BotSnapshot const> Build(Player* p, BotAI* bot_ai, SnapshotVer next_version, TickId tick);
};

} // namespace Playerbot
