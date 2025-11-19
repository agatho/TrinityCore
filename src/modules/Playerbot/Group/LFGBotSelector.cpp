#include "LFGBotSelector.h"

namespace Playerbot
{

LFGBotSelector::LFGBotSelector(Player* bot) : _bot(bot)
{
    if (!_bot)
        TC_LOG_ERROR("playerbot.group", "LFGBotSelector: null bot!");
}

LFGBotSelector::~LFGBotSelector() {}

} // namespace Playerbot
