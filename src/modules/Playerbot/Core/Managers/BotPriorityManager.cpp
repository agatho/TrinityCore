#include "BotPriorityManager.h"

namespace Playerbot
{

BotPriorityManager::BotPriorityManager(Player* bot) : _bot(bot)
{
    if (!_bot)
        TC_LOG_ERROR("playerbot", "BotPriorityManager: null bot!");
}

BotPriorityManager::~BotPriorityManager() {}

} // namespace Playerbot
