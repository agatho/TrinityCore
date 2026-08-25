// BotIdentityRegistry - Tracks which character GUIDs are V2 bots vs. real
// players. The ground truth lives in the playerbot_v2_character table; this
// class keeps an in-memory cache for O(1) login-time checks.
//
// World-thread mutations only (mark/unmark from GM commands and spawner).
// Reads from any thread are safe via the shared_mutex.

#pragma once

#include "Bot/BotTypes.h"
#include <shared_mutex>
#include <unordered_set>
#include <vector>

namespace Playerbot {

class BotIdentityRegistry
{
public:
    // Loads the bot-id set from playerbot_v2_character. Returns the count.
    size_t LoadFromDb();

    bool is_bot(BotId id) const;

    // Mark/unmark a character at runtime. Writes through to the DB so the
    // mapping survives restart. Idempotent.
    void mark_as_bot(BotId id);
    void unmark_as_bot(BotId id);

    // Transient mark: adds to the in-memory set only, NO DB write.
    // Used by .playerbot self on so a real human character is treated as
    // an AI bot for the duration of the session, but NOT persisted across
    // restart — otherwise BotPopulationManager::LoginExisting would grab
    // the character on next boot and the human couldn't log in.
    // The corresponding unmark just removes from memory; if the same id
    // was ever persistently marked, the DB row stays untouched.
    void mark_as_bot_transient(BotId id);
    void unmark_as_bot_transient(BotId id);

    size_t size() const;

    // Snapshot of the marked-bot id set for diagnostic walks (e.g.
    // .playerbot list). Returns a copy so callers don't hold the lock.
    std::vector<BotId> snapshot_ids() const;

private:
    mutable std::shared_mutex     mtx_;
    std::unordered_set<BotId>     bots_;
};

} // namespace Playerbot
