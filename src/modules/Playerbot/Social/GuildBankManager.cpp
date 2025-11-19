#include "GuildBankManager.h"

namespace Playerbot
{

GuildBankManager::GuildBankManager(Player* bot) : _bot(bot)
{
    if (!_bot)
        TC_LOG_ERROR("playerbot", "GuildBankManager: null bot!");
}

GuildBankManager::~GuildBankManager() {}

} // namespace Playerbot
