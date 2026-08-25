// LearnFlightpaths - Walks `TaxiNodesStore` and unlocks every node the
// bot's faction can use. Lets distribution-spawned bots taxi anywhere
// without grinding flight paths organically (which would take days of
// foot travel before they could populate the world).
//
// Server-thread only. Synchronous. Idempotent (re-running on a bot
// that already has the masks costs ~600 cheap bit-ops).

#pragma once

#include "Bot/BotTypes.h"

class Player;

namespace Playerbot::V2::World {

// Sets every taxi-mask bit the bot's faction is permitted to use.
// Returns the count of newly-unlocked nodes (0 if already had them all).
uint32 LearnAllFactionFlightpaths(Player* bot);

} // namespace Playerbot::V2::World
