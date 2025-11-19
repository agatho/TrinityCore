#include "GuildEventCoordinator.h"

namespace Playerbot
{

GuildEventCoordinator::GuildEventCoordinator(Player* bot) : _bot(bot)
{
    if (!_bot)
        TC_LOG_ERROR("playerbot", "GuildEventCoordinator: null bot!");
}

GuildEventCoordinator::~GuildEventCoordinator() {}

} // namespace Playerbot
