// BotCharacterFactory - One-shot character creation for V2 testing.
//
// The core HandleCharCreateOpcode flow is ~400 LOC of validation +
// chained async DB callbacks meant for client-driven creation. For GM
// commands ("here, give me a fresh test bot"), we run a direct synchronous
// path: validate → Player::Create → SaveToDB → mark-as-bot. The result is a
// character row that can be loaded by `BotSessionMgr::LoginBot`.
//
// All work runs on the world thread (the only thread that calls GM
// commands), which is also the only legal Player::Create caller.

#pragma once

#include "ObjectGuid.h"
#include <cstdint>
#include <string>

class WorldSession;

namespace Playerbot::V2 {

class BotCharacterFactory
{
public:
    struct Result
    {
        bool        ok = false;
        std::string reason;     // populated when ok == false
        ObjectGuid  guid;       // populated when ok == true
    };

    // Create a bot character on a pool account (NOT ownerSession's account).
    // ownerSession is used only for locale resolution during name validation
    // and as audit info in the log line. Pass nullptr from headless callers
    // (auto-spawn-on-boot) — the factory falls back to LOCALE_enUS.
    //
    // Defaults: gender=0 (male). The character spawns at the race/class
    // start location at the start level for that race+class.
    static Result Create(
        WorldSession*      ownerSession,
        std::string const& name,
        uint8              race,
        uint8              charClass,
        uint8              gender = 0);
};

} // namespace Playerbot::V2
