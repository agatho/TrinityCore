// BotAccountMgr - Pool of dedicated bot accounts.
//
// Each pool account ("PBV2_NNNN") holds up to MAX_CHARS_PER_ACCOUNT bot
// characters. AcquireSlot returns the next account id with room, creating
// a new pool account on demand when all existing ones are full. This
// keeps bot characters off the operator's GM account so the operator can
// log in to their own characters without colliding with bots and so the
// bot fleet can scale beyond 10 chars without manual account management.
//
// World-thread only. The synchronous DB queries (CharacterDatabase::Query
// for char counts; LoginDatabase::Execute for account creation) are cheap
// and acceptable on the world thread for the rare AcquireSlot path
// (called from `.playerbot create` only). Login traffic uses pre-existing
// account ids and never touches this manager.

#pragma once

#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace Playerbot::V2 {

class BotAccountMgr
{
public:
    // Hardcoded character cap per pool account. Matches WoW's standard
    // character-select limit; raising it only buys bigger fleets per
    // account, with no behavior changes.
    static constexpr uint32 MAX_CHARS_PER_ACCOUNT = 10;

    BotAccountMgr() = default;

    // Reads playerbot_v2_account rows + their current characters.account
    // counts into the in-memory cache. Call once at Services::Init.
    void LoadFromDb();

    // Find an existing pool account with < MAX_CHARS_PER_ACCOUNT bot chars,
    // or create a new pool account. Returns 0 on failure (account creation
    // refused, name collision exhaustion, DB error). On success the caller
    // is expected to immediately add a character to that account; bump the
    // local cache by calling note_character_added(accountId).
    uint32 AcquireSlot();

    // Increment in-memory char count for accountId. Called by
    // BotCharacterFactory after a successful SaveToDB. Idempotent on
    // unknown accounts (no-op).
    void note_character_added(uint32 accountId);

    // Decrement in-memory char count. Called by `.playerbot delete` after
    // Player::DeleteFromDB succeeds. Idempotent on unknown accounts.
    void note_character_removed(uint32 accountId);

    // Diagnostic counts for `.playerbot status` etc.
    size_t pool_size() const;       // accounts known to the manager
    size_t total_chars() const;     // sum of cached char counts

    // True when accountId is one of the dedicated PBV2 pool accounts.
    // False = a real player's account: a bot character there is an ALT
    // the player marked/summoned (`.playerbot summon`, or a manual
    // mark+login) and must never be treated as population-shaper
    // inventory (no overflow/hygiene/excess kicks).
    bool is_pool_account(uint32 accountId) const
    {
        std::lock_guard<std::mutex> lk(mtx_);
        return pool_.find(accountId) != pool_.end();
    }

private:
    // Generates the next pseudo_account_idx (max + 1 from cache).
    uint32 next_pseudo_idx_locked() const;

    // Synchronously creates a new pool account via sAccountMgr, inserts
    // the row in playerbot_v2_account, and returns its account id. 0 on
    // failure (e.g., name already exists in the legacy `account` table).
    uint32 create_pool_account_locked();

    // One-shot migration: walks the cached pool, looks up each auth
    // account's battlenet_account column, and provisions a bnet account
    // + linkage for any pool account that was created before V2 started
    // creating proper bnet accounts. Idempotent. Caller must hold mtx_.
    void   BackfillBnetAccountsForPool_locked();

    mutable std::mutex                     mtx_;
    // accountId -> current bot-character count.
    std::unordered_map<uint32, uint32>     pool_;
};

} // namespace Playerbot::V2
