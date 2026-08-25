#include "BotNamePool.h"

#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"

#include <fmt/format.h>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace Playerbot::V2::Fleet {

// ---- In-memory pool ---------------------------------------------------------
//
// Earlier version queried the DB on every Acquire (COUNT + SELECT-OFFSET +
// UPDATE + verify-SELECT × 8 retries = up to 32 sync queries per bot create).
// Under the boot-time spawn burst the world thread spent >60s in those
// queries and tripped the FreezeDetector. Worse: the verify SELECT races
// the async UPDATE (PExecute is non-blocking), so it constantly read the
// pre-update state and falsely concluded "someone took my name", driving
// the retry loop to exhaustion.
//
// Fix: keep the entire pool in memory. Boot loads is_used=0 rows once;
// Acquire/Release mutate the in-memory pool (O(1) under a single mutex)
// and fire-and-forget an async UPDATE to keep the DB row in sync. The
// async DB write doesn't have to land before the next Acquire — we
// already removed the entry from memory so we won't hand it out again.
namespace {

struct NameEntry
{
    uint32      name_id;
    std::string name;
    uint8       gender;     // 0 male, 1 female, 2 universal
};

std::mutex                                    g_mutex;
std::vector<NameEntry>                        g_male;
std::vector<NameEntry>                        g_female;
std::vector<NameEntry>                        g_universal;
// Reverse lookup: name → (name_id, gender) so Release knows which bucket
// to return the entry to. Names from the syllable-generator fallback
// aren't in this map; Release no-ops them.
std::unordered_map<std::string, std::pair<uint32, uint8>> g_name_to_meta;

thread_local std::mt19937 g_rng{std::random_device{}()};

// Picks a random index from a non-empty vector and swap-erases it.
// Returns the popped entry. Caller must hold g_mutex.
NameEntry PopRandom_locked(std::vector<NameEntry>& v)
{
    std::uniform_int_distribution<size_t> dist(0, v.size() - 1);
    size_t idx = dist(g_rng);
    NameEntry e = std::move(v[idx]);
    v[idx] = std::move(v.back());
    v.pop_back();
    return e;
}

// Single config-driven home for all instance-shared playerbot data
// (Playerbot.SharedDatabase, default "wowc_playerbot"). Read once. Used to
// qualify cross-DB queries so swapping the active world/character DB never
// strands the shared bot data. NOTE: the realm-specific `characters.characters`
// join below is intentionally NOT this schema.
std::string const& SharedDb()
{
    static std::string const db =
        sConfigMgr->GetStringDefault("Playerbot.SharedDatabase", "playerbot");
    return db;
}

std::string EscapeForSql(std::string const& s)
{
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s)
    {
        if (c == '\'') out += "''";
        else out += c;
    }
    return out;
}

// One-time load. Called from ReconcileOnBoot (which is the bootstrap path).
bool g_loaded = false;
void Load_locked()
{
    g_male.clear();
    g_female.clear();
    g_universal.clear();
    g_name_to_meta.clear();

    // Load every is_used=0 row. ORDER BY name_id keeps the load deterministic
    // for testing; the in-memory pick is random so distribution is uniform.
    auto res = CharacterDatabase.Query(fmt::format(
        "SELECT name_id, name, gender FROM {}.playerbots_names "
        "WHERE is_used=0 ORDER BY name_id", SharedDb()).c_str());
    if (!res)
    {
        TC_LOG_WARN("playerbot.v2", "[BotNamePool] Load: query returned no rows.");
        g_loaded = true;
        return;
    }

    do {
        Field* f = res->Fetch();
        NameEntry e;
        e.name_id = f[0].GetUInt32();
        e.name    = f[1].GetString();
        e.gender  = f[2].GetUInt8();
        g_name_to_meta[e.name] = {e.name_id, e.gender};
        switch (e.gender)
        {
            case 0:  g_male.push_back(std::move(e));      break;
            case 1:  g_female.push_back(std::move(e));    break;
            default: g_universal.push_back(std::move(e)); break;
        }
    } while (res->NextRow());

    g_loaded = true;
    TC_LOG_INFO("playerbot.v2",
        "[BotNamePool] Loaded in-memory pool: {} male, {} female, {} universal (total {}).",
        g_male.size(), g_female.size(), g_universal.size(),
        g_male.size() + g_female.size() + g_universal.size());
}

} // anonymous

std::string BotNamePool::Acquire(uint8 gender)
{
    std::lock_guard lk(g_mutex);
    if (!g_loaded)
    {
        // Defensive: caller forgot to call ReconcileOnBoot. Self-bootstrap
        // so the pool still works in fallback / test scenarios.
        Load_locked();
    }

    // Pick from gender-specific bucket first, then fall back to universal.
    // Gender=0 (male) → g_male → g_universal.
    // Gender=1 (female) → g_female → g_universal.
    // Gender=2 (any) → g_universal → g_male → g_female (any-leftover).
    std::vector<NameEntry>* tries[3] = {nullptr, nullptr, nullptr};
    if (gender == 0)      { tries[0] = &g_male;      tries[1] = &g_universal; }
    else if (gender == 1) { tries[0] = &g_female;    tries[1] = &g_universal; }
    else                  { tries[0] = &g_universal; tries[1] = &g_male; tries[2] = &g_female; }

    for (auto* bucket : tries)
    {
        if (!bucket || bucket->empty()) continue;
        NameEntry e = PopRandom_locked(*bucket);
        // Async-persist the claim. PExecute doesn't block; ordering is
        // fine because the in-memory pop already prevents re-issue.
        CharacterDatabase.Execute(fmt::format(
            "UPDATE {}.playerbots_names SET is_used=1 WHERE name_id={}",
            SharedDb(), e.name_id).c_str());
        // Keep the reverse map so Release can find this entry.
        g_name_to_meta[e.name] = {e.name_id, e.gender};
        return e.name;
    }

    TC_LOG_WARN("playerbot.v2", "[BotNamePool] Acquire: pool exhausted (all buckets empty).");
    return {};
}

void BotNamePool::BindToCharacter(std::string const& name, uint64 guid_low)
{
    // No mutex needed — purely a DB update; doesn't touch in-memory pool.
    CharacterDatabase.Execute(fmt::format(
        "UPDATE {}.playerbots_names SET used_by_guid={} WHERE name='{}'",
        SharedDb(), guid_low, EscapeForSql(name)).c_str());
}

void BotNamePool::Release(std::string const& name)
{
    if (name.empty()) return;
    std::lock_guard lk(g_mutex);
    auto it = g_name_to_meta.find(name);
    if (it == g_name_to_meta.end())
        return;     // not from this pool (syllable-gen fallback) — no-op

    const uint32 name_id = it->second.first;
    const uint8  gender  = it->second.second;

    // Re-add to the gender's bucket so Acquire can hand it out again.
    NameEntry e{name_id, name, gender};
    switch (gender)
    {
        case 0:  g_male.push_back(std::move(e));      break;
        case 1:  g_female.push_back(std::move(e));    break;
        default: g_universal.push_back(std::move(e)); break;
    }

    CharacterDatabase.Execute(fmt::format(
        "UPDATE {}.playerbots_names SET is_used=0, used_by_guid=NULL "
        "WHERE name_id={}",
        SharedDb(), name_id).c_str());
}

void BotNamePool::ReconcileOnBoot()
{
    // First: clean up DB orphans (is_used=1 but the character doesn't exist).
    // This catches SQL-wipes / mid-create crashes that bypassed Release.
    auto orphan_res = CharacterDatabase.Query(fmt::format(
        "SELECT pn.name_id FROM {}.playerbots_names pn "
        "LEFT JOIN characters.characters c ON c.guid = pn.used_by_guid "
        "WHERE pn.is_used = 1 AND (pn.used_by_guid IS NULL OR c.guid IS NULL)",
        SharedDb()).c_str());

    uint32 released = 0;
    if (orphan_res)
    {
        do {
            const uint32 name_id = orphan_res->Fetch()[0].GetUInt32();
            CharacterDatabase.Execute(fmt::format(
                "UPDATE {}.playerbots_names SET is_used=0, used_by_guid=NULL "
                "WHERE name_id={}", SharedDb(), name_id).c_str());
            ++released;
        } while (orphan_res->NextRow());
    }
    if (released)
        TC_LOG_INFO("playerbot.v2",
            "[BotNamePool] ReconcileOnBoot: released {} orphaned name(s).", released);
    else
        TC_LOG_INFO("playerbot.v2", "[BotNamePool] ReconcileOnBoot: nothing to release.");

    // Then load the (post-cleanup) is_used=0 set into memory.
    std::lock_guard lk(g_mutex);
    Load_locked();
}

BotNamePool::PoolStats BotNamePool::GetStats()
{
    std::lock_guard lk(g_mutex);
    PoolStats s{};
    s.available = uint32(g_male.size() + g_female.size() + g_universal.size());
    s.total = uint32(g_name_to_meta.size());
    return s;
}

} // namespace Playerbot::V2::Fleet
