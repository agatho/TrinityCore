#include "LFGGroupCoordinator.h"

namespace Playerbot
{

LFGGroupCoordinator::LFGGroupCoordinator(Player* bot) : _bot(bot)
{
    if (!_bot)
        TC_LOG_ERROR("playerbot.group", "LFGGroupCoordinator: null bot!");
}

LFGGroupCoordinator::~LFGGroupCoordinator() {}

} // namespace Playerbot
