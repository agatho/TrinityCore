#include "BotWorldSessionMgr.h"

namespace Playerbot
{

BotWorldSessionMgr::BotWorldSessionMgr(Player* bot) : _bot(bot)
{
    if (!_bot)
        TC_LOG_ERROR("playerbot", "BotWorldSessionMgr: null bot!");
}

BotWorldSessionMgr::~BotWorldSessionMgr() {}

} // namespace Playerbot
