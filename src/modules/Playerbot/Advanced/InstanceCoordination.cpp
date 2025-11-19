#include "InstanceCoordination.h"

namespace Playerbot
{

InstanceCoordination::InstanceCoordination(Player* bot) : _bot(bot)
{
    if (!_bot)
        TC_LOG_ERROR("playerbot.group", "InstanceCoordination: null bot!");
}

InstanceCoordination::~InstanceCoordination() {}

} // namespace Playerbot
