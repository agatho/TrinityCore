#include "TradeSystem.h"

namespace Playerbot
{

TradeSystem::TradeSystem(Player* bot) : _bot(bot)
{
    if (!_bot)
        TC_LOG_ERROR("playerbot", "TradeSystem: null bot!");
}

TradeSystem::~TradeSystem() {}

} // namespace Playerbot
