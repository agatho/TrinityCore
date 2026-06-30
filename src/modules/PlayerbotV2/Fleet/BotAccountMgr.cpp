#include "BotAccountMgr.h"

#include "AccountMgr.h"
#include "BattlenetAccountMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"

#include <algorithm>
#include <fmt/format.h>
#include <random>
#include <string>

namespace Playerbot::V2 {

namespace {

// Username pattern: PBV2_NNNN. Trinity's MAX_ACCOUNT_STR is 16, so
// PBV2_ + up to 11 digits fits easily; we use 4-digit zero-padded
// indices and grow naturally beyond 9999 if a fleet ever needs it.
std::string PoolAccountName(uint32 idx)
{
    return fmt::format("PBV2_{:04}", idx);
}

// Generates a 16-char random alphanumeric password. Bots never log in
// interactively so the password is throwaway, but it must be valid for
// the SRP6 derivation in AccountMgr::CreateAccount and not be empty.
std::string GeneratePassword()
{
    static thread_local std::mt19937 rng{std::random_device{}()};
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::uniform_int_distribution<int> dist(0, sizeof(alphabet) - 2);
    std::string out;
    out.reserve(16);
    for (int i = 0; i < 16; ++i)
        out.push_back(alphabet[dist(rng)]);
    return out;
}

} // anonymous

void BotAccountMgr::LoadFromDb()
{
    std::unique_lock lk(mtx_);
    pool_.clear();

    // Load the V2 account pool. We then count bot characters per account
    // via a join on playerbot_v2_character so we get only V2-marked chars
    // (defensive: if a pool account ever held a non-bot char, it doesn't
    // throw off the slot accounting).
    auto rows = CharacterDatabase.Query("SELECT account_id FROM playerbot_v2_account");
    if (!rows)
    {
        TC_LOG_INFO("playerbot.v2", "[BotAccountMgr] No pool accounts found.");
        return;
    }
    do
    {
        Field* f = rows->Fetch();
        const uint32 acct = f[0].GetUInt32();
        pool_.emplace(acct, 0);
    } while (rows->NextRow());

    // Now walk characters once. For each character whose guid is in
    // playerbot_v2_character AND whose account is a pool account, bump
    // the count. Cheap on small fleets; on big fleets a smarter join
    // would help but this runs once at boot.
    if (pool_.empty()) return;

    auto chars = CharacterDatabase.Query(
        "SELECT c.account FROM characters c "
        "INNER JOIN playerbot_v2_character pv2 ON pv2.character_guid_low = c.guid");
    if (chars)
    {
        do
        {
            Field* cf = chars->Fetch();
            const uint32 acct = cf[0].GetUInt32();
            if (auto it = pool_.find(acct); it != pool_.end())
                ++it->second;
        } while (chars->NextRow());
    }

    uint32 total = 0;
    for (auto const& [_, c] : pool_) total += c;
    TC_LOG_INFO("playerbot.v2", "[BotAccountMgr] Loaded {} pool account(s) holding {} bot character(s).",
                pool_.size(), total);

    // Migration: backfill bnet_account linkage for existing pool accounts
    // created before V2 started provisioning bnet accounts. Without this,
    // any save keyed by GetBattlenetAccountId() (collections, item
    // appearances, transmog, battle pets, account-side player data)
    // FK-fails on insert. Idempotent — only fires for accounts whose
    // `account.battlenet_account` is currently NULL.
    BackfillBnetAccountsForPool_locked();
}

void BotAccountMgr::BackfillBnetAccountsForPool_locked()
{
    if (pool_.empty()) return;

    // Build the WHERE id IN (...) clause for a single auth-DB lookup.
    std::string idList;
    bool first = true;
    for (auto const& [acct, _] : pool_)
    {
        if (!first) idList.push_back(',');
        idList += std::to_string(acct);
        first = false;
    }

    auto rows = LoginDatabase.PQuery(
        "SELECT id, username FROM account WHERE id IN ({}) AND battlenet_account IS NULL",
        idList);
    if (!rows)
        return;  // every pool account already has a bnet linkage.

    uint32 backfilled = 0;
    do
    {
        Field* f = rows->Fetch();
        const uint32 acctId = f[0].GetUInt32();
        const std::string username = f[1].GetString();

        // Synthesize a deterministic bnet email per pool account so a
        // re-run of the migration is idempotent: if the bnet row already
        // exists from a previous half-completed run, CreateBattlenetAccount
        // returns AOR_NAME_ALREADY_EXIST and we just look up its id.
        const std::string bnetEmail = username + "@playerbot-v2.local";
        const std::string password  = GeneratePassword();

        std::string ignoredGameAccountName;
        AccountOpResult res = ::Battlenet::AccountMgr::CreateBattlenetAccount(
            bnetEmail, password, /*withGameAccount=*/ false, &ignoredGameAccountName);
        if (res != AccountOpResult::AOR_OK && res != AccountOpResult::AOR_NAME_ALREADY_EXIST)
        {
            TC_LOG_ERROR("playerbot.v2",
                "[BotAccountMgr] BackfillBnet: CreateBattlenetAccount('{}') failed (result={}); "
                "account_id={} will keep producing FK errors until resolved manually.",
                bnetEmail, static_cast<int>(res), acctId);
            continue;
        }

        const uint32 bnetId = ::Battlenet::AccountMgr::GetId(bnetEmail);
        if (!bnetId)
        {
            TC_LOG_ERROR("playerbot.v2",
                "[BotAccountMgr] BackfillBnet: bnet '{}' has no id post-create; account_id={}.",
                bnetEmail, acctId);
            continue;
        }

        // Link the existing auth account to the (new or existing) bnet.
        // battlenet_index = 1 mirrors what GameAccountMgr::CreateAccount
        // sets when withGameAccount=true.
        LoginDatabase.DirectPExecute(
            "UPDATE account SET battlenet_account = {}, battlenet_index = 1 WHERE id = {}",
            bnetId, acctId);
        ++backfilled;
        TC_LOG_INFO("playerbot.v2",
            "[BotAccountMgr] BackfillBnet: linked account_id={} -> bnet_id={} ('{}').",
            acctId, bnetId, bnetEmail);
    } while (rows->NextRow());

    if (backfilled)
        TC_LOG_INFO("playerbot.v2",
            "[BotAccountMgr] BackfillBnet: provisioned {} bnet linkage(s) for legacy pool accounts.",
            backfilled);
}

uint32 BotAccountMgr::AcquireSlot()
{
    std::unique_lock lk(mtx_);

    // First pass: any existing account with room?
    for (auto& [acct, count] : pool_)
    {
        if (count < MAX_CHARS_PER_ACCOUNT)
            return acct;
    }

    // No room — create a new pool account. The new account starts with
    // count=0 and is immediately returned; the caller adds the character
    // and calls note_character_added to bump the count to 1.
    return create_pool_account_locked();
}

void BotAccountMgr::note_character_added(uint32 accountId)
{
    std::unique_lock lk(mtx_);
    if (auto it = pool_.find(accountId); it != pool_.end())
        ++it->second;
}

void BotAccountMgr::note_character_removed(uint32 accountId)
{
    std::unique_lock lk(mtx_);
    if (auto it = pool_.find(accountId); it != pool_.end() && it->second > 0)
        --it->second;
}

size_t BotAccountMgr::pool_size() const
{
    std::lock_guard lk(mtx_);
    return pool_.size();
}

size_t BotAccountMgr::total_chars() const
{
    std::lock_guard lk(mtx_);
    size_t total = 0;
    for (auto const& [_, c] : pool_) total += c;
    return total;
}

uint32 BotAccountMgr::next_pseudo_idx_locked() const
{
    // We don't store pseudo_idx in the cache (only account_id), so query
    // the table for max(pseudo_account_idx) + 1. Cheap one-shot.
    auto r = CharacterDatabase.Query("SELECT COALESCE(MAX(pseudo_account_idx), 0) + 1 FROM playerbot_v2_account");
    if (!r) return 1;
    Field* f = r->Fetch();
    return f[0].GetUInt32();
}

uint32 BotAccountMgr::create_pool_account_locked()
{
    // Each V2 pool account is created as a full Battle.net account with
    // a linked game account. Without the bnet_account row, every save
    // path keyed by `WorldSession::GetBattlenetAccountId()` (toys,
    // heirlooms, mounts, item appearances, transmog, scenes, battle
    // pets, account-side player data) would FK-fail. Going through
    // Battlenet::AccountMgr::CreateBattlenetAccount mirrors how a real
    // Bnet-issued account is provisioned: it inserts the bnet_account
    // row first, then GameAccountMgr::CreateAccount creates the linked
    // entry in `account` with bnetAccountId/bnetIndex set correctly.
    // Up to a few retries in case the chosen username collides with an
    // existing account (unlikely with the PBV2_ prefix but defensive).
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        const uint32 idx = next_pseudo_idx_locked() + uint32(attempt);
        const std::string name = PoolAccountName(idx);
        const std::string password = GeneratePassword();
        // The bnet "email" must look like a deliverable address; we use
        // a synthetic <name>@playerbot-v2.local. It's never used for
        // anything outside the SRP+Bnet auth tables.
        const std::string bnetEmail = name + "@playerbot-v2.local";

        std::string gameAccountName;
        const AccountOpResult result = ::Battlenet::AccountMgr::CreateBattlenetAccount(
            bnetEmail, password, /*withGameAccount=*/ true, &gameAccountName);
        if (result == AccountOpResult::AOR_NAME_ALREADY_EXIST)
        {
            TC_LOG_INFO("playerbot.v2", "[BotAccountMgr] Pool bnet '{}' already exists; retrying.", bnetEmail);
            continue;
        }
        if (result != AccountOpResult::AOR_OK)
        {
            TC_LOG_ERROR("playerbot.v2", "[BotAccountMgr] CreateBattlenetAccount('{}') failed (result={}).",
                         bnetEmail, static_cast<int>(result));
            return 0;
        }

        // CreateBattlenetAccount auto-named the game account "{bnetId}#1".
        // Resolve the auth account id from that name — that's what the
        // pool tracks (the bot character's auth account_id).
        const uint32 newAcctId = sAccountMgr->GetId(gameAccountName);
        if (!newAcctId)
        {
            TC_LOG_ERROR("playerbot.v2", "[BotAccountMgr] CreateBattlenetAccount succeeded but GetId('{}') returned 0.",
                         gameAccountName);
            return 0;
        }

        // Persist V2-side metadata so the pool survives restart.
        CharacterDatabase.DirectPExecute(
            "INSERT INTO playerbot_v2_account (account_id, pseudo_account_idx) VALUES ({}, {})",
            newAcctId, idx);

        pool_.emplace(newAcctId, 0);
        TC_LOG_INFO("playerbot.v2", "[BotAccountMgr] Created pool bnet+game account '{}' (auth_id={}, pseudo_idx={}).",
                    gameAccountName, newAcctId, idx);
        return newAcctId;
    }

    TC_LOG_ERROR("playerbot.v2", "[BotAccountMgr] Exhausted retries creating pool account; pool starvation.");
    return 0;
}

} // namespace Playerbot::V2
