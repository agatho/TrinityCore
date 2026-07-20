// BotCommandParser - Parses chat-driven bot commands ("follow", "stay",
// "attack", "release") and translates them into intents on the bot's queue.
//
// Runs on the world thread (called from the whisper hook). Keeps it lock-free
// by pushing into the per-bot IntentQueue, which the world drain consumes.

#pragma once

#include "BotTypes.h"
#include <string>

class Player;

namespace Playerbot {

class BotCommandParser
{
public:
    // Returns true if the message matched a known command (regardless of
    // success). A future iteration will return rich error info; the bool
    // shape is enough for "did the bot handle this?".
    //
    // Performs squad-address resolution at the top: if `msg` is prefixed
    // with `all:`, `tank:`, `Areon:`, etc., the command body is dispatched
    // to every bot the sender owns that matches the prefix, and the
    // whispered `bot_player` is used only as the messenger that replies
    // with a single summary line. The address resolver is in
    // BotAddressResolver.h.
    static bool Dispatch(Player* sender, Player* bot_player, std::string const& msg);

    // Single-bot dispatch. Skips the address resolver — used internally
    // by Dispatch when a prefix has already been parsed and a per-target
    // command needs to run, and by tests that bypass the prefix layer.
    static bool DispatchSingle(Player* sender, Player* bot_player, std::string const& msg);
};

} // namespace Playerbot
