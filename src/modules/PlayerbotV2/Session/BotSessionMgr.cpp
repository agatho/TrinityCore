#include "BotSessionMgr.h"
#include "BotSession.h"
#include "../Fleet/BotIdentityRegistry.h"
#include "../Services.h"

#include "AccountMgr.h"
#include "CharacterCache.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "WorldSession.h"

#include <fmt/format.h>

namespace Playerbot::V2 {

BotSessionMgr::~BotSessionMgr()
{
    // Sessions are owned by shared_ptr; clearing the map runs each session's
    // destructor which logs out + saves the player. Ordering note: this runs
    // at module Shutdown, after the AI worker pool has stopped, so no AI
    // worker is still pushing intents into a session being torn down.
    std::lock_guard lk(mtx_);
    sessions_.clear();
}

BotSessionMgr::LoginResult BotSessionMgr::LoginBot(ObjectGuid playerGuid)
{
    if (playerGuid.IsEmpty())
        return {false, "empty character guid"};

    // Must be marked as a bot — refuse to headlessly login arbitrary characters.
    if (!Services::Lifecycle().is_bot(playerGuid.GetCounter()))
        return {false, "character is not marked as a V2 bot (use .playerbot mark first)"};

    // Already in-world?
    if (ObjectAccessor::FindConnectedPlayer(playerGuid))
        return {false, "character already in-world"};

    // Already in our session table (login in flight)?
    {
        std::lock_guard lk(mtx_);
        if (sessions_.count(playerGuid))
            return {false, "login already in flight"};
    }

    // Resolve the character's account id via the in-memory CharacterCache
    // (loaded at server boot, kept in sync with the characters table on
    // create/delete/transfer). Avoids a DB roundtrip per login.
    const uint32 accountId = sCharacterCache->GetCharacterAccountIdByGuid(playerGuid);
    if (!accountId)
        return {false, "characters.account is 0 (cache miss / orphan)"};

    // Pull the account name (used by WorldSession diagnostics / chat tags).
    std::string accountName;
    if (!sAccountMgr->GetName(accountId, accountName))
        accountName = fmt::format("acct{}", accountId);

    auto session = std::make_shared<BotSession>(accountId, std::move(accountName));
    if (!session->BeginLogin(playerGuid))
        return {false, "BotSession::BeginLogin failed (see server log)"};

    {
        std::lock_guard lk(mtx_);
        sessions_.emplace(playerGuid, session);
    }
    TC_LOG_INFO("playerbot.v2", "[BotSessionMgr] Spawn-login submitted for {} (account {})",
                playerGuid.ToString(), accountId);
    return {true, {}};
}

bool BotSessionMgr::IsHeadless(ObjectGuid playerGuid) const
{
    std::lock_guard lk(mtx_);
    return sessions_.count(playerGuid) != 0;
}

bool BotSessionMgr::LogoutBot(ObjectGuid playerGuid)
{
    std::shared_ptr<BotSession> sess;
    {
        std::lock_guard lk(mtx_);
        auto it = sessions_.find(playerGuid);
        if (it == sessions_.end()) return false;
        sess = it->second;
    }
    if (!sess) return false;
    // KickPlayer flips forceExit; the next Update tick observes that and
    // unwinds the session via base WorldSession::LogoutPlayer.
    sess->KickPlayer("V2 .playerbot logout");
    return true;
}

BotSessionMgr::BatchResult BotSessionMgr::LoginAll(uint32 cap)
{
    BatchResult r{};
    auto ids = Services::Lifecycle().snapshot_ids();
    for (BotId id : ids)
    {
        if (active_count() >= cap)
            break;
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        if (ObjectAccessor::FindConnectedPlayer(g))
        {
            ++r.skipped_already_in_world;
            continue;
        }
        ++r.attempted;
        if (LoginBot(g).ok)
            ++r.succeeded;
    }
    return r;
}

uint32 BotSessionMgr::LogoutAll()
{
    std::vector<std::shared_ptr<BotSession>> snap;
    {
        std::lock_guard lk(mtx_);
        snap.reserve(sessions_.size());
        for (auto const& [_, sess] : sessions_)
            snap.push_back(sess);
    }
    uint32 kicked = 0;
    for (auto const& sess : snap)
    {
        if (sess) { sess->KickPlayer("V2 .playerbot logout_all"); ++kicked; }
    }
    return kicked;
}

void BotSessionMgr::Update(uint32 diff)
{
    // Snapshot active sessions under lock; tick + remove dead ones outside
    // the lock so a session destructor that interacts back with the mgr
    // (none today, but safer) can't deadlock.
    std::vector<std::pair<ObjectGuid, std::shared_ptr<BotSession>>> snap;
    {
        std::lock_guard lk(mtx_);
        snap.reserve(sessions_.size());
        for (auto const& [guid, sess] : sessions_)
            snap.emplace_back(guid, sess);
    }

    std::vector<ObjectGuid> dead;
    for (auto& [guid, sess] : snap)
    {
        WorldSessionFilter filter(sess.get());
        if (!sess->Update(diff, filter))
            dead.push_back(guid);
    }

    if (!dead.empty())
    {
        std::lock_guard lk(mtx_);
        for (ObjectGuid g : dead)
            sessions_.erase(g);
    }
}

size_t BotSessionMgr::active_count() const
{
    std::lock_guard lk(mtx_);
    return sessions_.size();
}

} // namespace Playerbot
