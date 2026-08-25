#include "BotIdentityRegistry.h"
#include "DatabaseEnv.h"
#include "Log.h"

namespace Playerbot {

namespace {
auto& Db() { return CharacterDatabase; }
}

size_t BotIdentityRegistry::LoadFromDb()
{
    auto result = Db().Query("SELECT character_guid_low FROM playerbot_v2_character");
    std::unique_lock lk(mtx_);
    bots_.clear();
    if (!result) return 0;
    do {
        Field* f = result->Fetch();
        bots_.insert(f[0].GetUInt64());
    } while (result->NextRow());
    TC_LOG_INFO("playerbot.v2", "[PlayerbotV2] Loaded {} bot character ids.", bots_.size());
    return bots_.size();
}

bool BotIdentityRegistry::is_bot(BotId id) const
{
    std::shared_lock lk(mtx_);
    return bots_.count(id) != 0;
}

void BotIdentityRegistry::mark_as_bot(BotId id)
{
    {
        std::unique_lock lk(mtx_);
        if (!bots_.insert(id).second) return;
    }
    Db().DirectPExecute(
        "INSERT IGNORE INTO playerbot_v2_character "
        "(character_guid_low, rng_seed, spawn_state) VALUES ({}, 0, 0)",
        id);
}

void BotIdentityRegistry::unmark_as_bot(BotId id)
{
    {
        std::unique_lock lk(mtx_);
        if (bots_.erase(id) == 0) return;
    }
    Db().DirectPExecute(
        "DELETE FROM playerbot_v2_character WHERE character_guid_low = {}",
        id);
}

void BotIdentityRegistry::mark_as_bot_transient(BotId id)
{
    std::unique_lock lk(mtx_);
    bots_.insert(id);
}

void BotIdentityRegistry::unmark_as_bot_transient(BotId id)
{
    std::unique_lock lk(mtx_);
    bots_.erase(id);
}

size_t BotIdentityRegistry::size() const
{
    std::shared_lock lk(mtx_);
    return bots_.size();
}

std::vector<BotId> BotIdentityRegistry::snapshot_ids() const
{
    std::shared_lock lk(mtx_);
    std::vector<BotId> out;
    out.reserve(bots_.size());
    for (BotId id : bots_) out.push_back(id);
    return out;
}

} // namespace Playerbot
