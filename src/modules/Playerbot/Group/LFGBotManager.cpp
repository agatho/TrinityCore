#include "LFGBotManager.h"

namespace Playerbot
{

LFGBotManager::LFGBotManager(Player* bot) : _bot(bot)
{
    if (!_bot)
        TC_LOG_ERROR("playerbot.group", "LFGBotManager: null bot!");
}

LFGBotManager::~LFGBotManager() {}

} // namespace Playerbot
