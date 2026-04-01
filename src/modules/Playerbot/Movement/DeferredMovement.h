/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Thread-safe deferred movement for bot worker threads.
 * Lightweight header — no BotSession.h dependency.
 */

#pragma once

#include "Define.h"

class Player;

namespace Playerbot
{

/**
 * @brief Queue a MovePoint to execute on the main thread.
 *
 * MotionMaster::MovePoint() is NOT thread-safe — calling it from worker
 * threads corrupts the motion generator. This function queues the movement
 * to BotSession's pending move queue, which ProcessPendingMoves() executes
 * on the main thread.
 *
 * @param bot       The bot player
 * @param pointId   Movement point ID
 * @param x, y, z   Destination coordinates
 * @return true if queued successfully, false if bot has no BotSession
 */
bool QueueDeferredMovePoint(Player* bot, uint32 pointId, float x, float y, float z);

} // namespace Playerbot
