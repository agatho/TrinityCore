/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"

class Player;

namespace Playerbot
{

class BotAI;
class IGameSystemsManager;

/**
 * @brief Helper function to safely get BotAI from a Player
 *
 * This helper safely casts Player's AI to BotAI, returning nullptr if:
 * - Player is null
 * - Player has no AI
 * - Player's AI is not a BotAI instance
 *
 * @param player The player to get BotAI from
 * @return BotAI* if player is a valid bot, nullptr otherwise
 *
 * @example
 * if (BotAI* botAI = GetBotAI(player))
 * {
 *     // Safe to use botAI here
 *     botAI->GetGameSystems()->GetQuestManager()->DoSomething();
 * }
 */
inline BotAI* GetBotAI(Player* player)
{
    if (!player)
        return nullptr;

    return dynamic_cast<BotAI*>(player->GetAI());
}

/**
 * @brief Helper function to safely get GameSystemsManager from a Player
 *
 * This is a convenience helper that combines GetBotAI() with GetGameSystems().
 * Returns nullptr if the player is not a valid bot or has no game systems.
 *
 * @param player The player to get GameSystemsManager from
 * @return IGameSystemsManager* if player is a valid bot with game systems, nullptr otherwise
 *
 * @example
 * if (IGameSystemsManager* systems = GetGameSystems(player))
 * {
 *     systems->GetQuestManager()->DoSomething();
 * }
 */
inline IGameSystemsManager* GetGameSystems(Player* player);

} // namespace Playerbot

// Include BotAI.h here to resolve the inline implementation
// This must come AFTER the forward declarations to avoid circular dependencies
#include "AI/BotAI.h"

namespace Playerbot
{

inline IGameSystemsManager* GetGameSystems(Player* player)
{
    BotAI* botAI = GetBotAI(player);
    if (!botAI)
        return nullptr;

    return botAI->GetGameSystems();
}

} // namespace Playerbot
