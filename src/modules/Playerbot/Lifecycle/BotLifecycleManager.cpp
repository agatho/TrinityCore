#include "BotLifecycleManager.h"

namespace Playerbot
{

BotLifecycleManager::BotLifecycleManager(Player* bot) : _bot(bot)
{
    if (!_bot)
        TC_LOG_ERROR("playerbot", "BotLifecycleManager: null bot!");
}

BotLifecycleManager::~BotLifecycleManager() {}

} // namespace Playerbot
