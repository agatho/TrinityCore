/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "DeferredMovement.h"
#include "../Session/BotSession.h"
#include "Player.h"

namespace Playerbot
{

bool QueueDeferredMovePoint(Player* bot, uint32 pointId, float x, float y, float z)
{
    if (!bot)
        return false;

    if (auto* session = dynamic_cast<BotSession*>(bot->GetSession()))
    {
        session->QueueMovePoint(pointId, x, y, z);
        return true;
    }

    return false;
}

} // namespace Playerbot
