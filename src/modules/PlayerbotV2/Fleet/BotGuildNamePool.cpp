// BotGuildNamePool - see header for design overview.

#include "BotGuildNamePool.h"

#include "DatabaseEnv.h"
#include "GuildMgr.h"
#include "Log.h"

#include <algorithm>

namespace Playerbot::V2 {

namespace {

// Per-faction static name pools. Mix of theme variations + capital
// references so a populated server feels like organic guild diversity.
// Roughly 30 per faction. All names < 24 chars (TC's guild name limit
// is 24 with most clients; we leave a buffer).
std::vector<std::string> const kAllianceNames = {
    "Iron Hearts",
    "Stormwind Vanguard",
    "Lionhearts of Azeroth",
    "Knights of the Silver Hand",
    "Westfall Brigade",
    "Wintergrasp Veterans",
    "Stranglethorn Adventurers",
    "Eastvale Rangers",
    "Goldshire Regulars",
    "Theramore Loyalists",
    "Wildhammer Tribe",
    "Gilnean Wolves",
    "Draenei Exodar Guard",
    "Sentinels of Teldrassil",
    "Ironforge Guard",
    "Highmountain Alliance",
    "Bards of Stormwind",
    "Stormpike Brotherhood",
    "Alterac Reclaimers",
    "Northshire Initiates",
    "The Argent Watch",
    "Heroes of the Light",
    "Defenders of Lordaeron",
    "Crusaders of Azeroth",
    "The Sapphire Order",
    "Wardens of Hyjal",
    "Boralus Marines",
    "Kul Tiran Trading Co",
    "Drustvar Sentinels",
    "Aerie Peak Wings",
};

// NOTE: Apostrophes and other punctuation are rejected by TC's
// ObjectMgr::IsValidCharterName -> isValidString. Lore-accurate names
// like "Sons of Lo'Gosh" produce `BotBuyGuildCharter -> name_invalid`
// at phase 2 and the FSM cleans up + retries. Keep only A-Za-z-space-
// dash characters here.
std::vector<std::string> const kHordeNames = {
    "Warband Vanguard",
    "Orgrimmar Reavers",
    "Bloodspear Tribe",
    "Sons of Doomhammer",
    "Shadowblades of Silvermoon",
    "Sindorei Magisters",
    "Darkspear Rebellion",
    "Maghar Outcasts",
    "Forsaken Apothecaries",
    "Undercity Plague Lords",
    "Goblin Trade Cartel",
    "Steamwheedle Profiteers",
    "Tauren Wargroups",
    "Mulgore Wanderers",
    "Bilgewater Buccaneers",
    "Highmountain Tauren",
    "Thunder Bluff Elders",
    "Echo Isles Warriors",
    "Silvermoon Resurgent",
    "Sunwell Faithful",
    "Frostwolf Veterans",
    "Warsong Outriders",
    "Horde Spearhead",
    "Brokenfang Tribe",
    "Crimson Talon",
    "Zandalari Loa-Sworn",
    "Voldun Sandstalkers",
    "Nazmir Bloodthirsty",
    "Dazaralor Honor Guard",
    "Orgrimmar Tide",
};

} // anonymous

void BotGuildNamePool::LoadFromDb()
{
    std::lock_guard<std::mutex> g(mtx_);
    reserved_.clear();

    QueryResult result = CharacterDatabase.Query(
        "SELECT name FROM bot_guild_name_reserved");

    if (!result)
    {
        TC_LOG_INFO("playerbot.v2",
            "[BotGuildNamePool] No pre-existing name reservations "
            "(table missing or empty)");
        return;
    }

    do
    {
        Field* f = result->Fetch();
        reserved_.push_back(f[0].GetString());
    } while (result->NextRow());

    TC_LOG_INFO("playerbot.v2",
        "[BotGuildNamePool] Loaded {} in-flight name reservations",
        reserved_.size());
}

std::vector<std::string> const& BotGuildNamePool::candidate_pool(Faction f) const
{
    return f == FACTION_HORDE ? kHordeNames : kAllianceNames;
}

std::string BotGuildNamePool::PickAndReserve(Faction f, uint64 founder_low)
{
    std::lock_guard<std::mutex> g(mtx_);

    for (std::string const& candidate : candidate_pool(f))
    {
        // Skip if already taken by a live guild on the server (TC's
        // sGuildMgr is authoritative — covers both player guilds and
        // bot guilds already in `guild` table from prior sessions).
        if (sGuildMgr->GetGuildByName(candidate))
            continue;

        // Skip if currently reserved by another in-flight charter.
        if (std::find(reserved_.begin(), reserved_.end(), candidate) != reserved_.end())
            continue;

        // Reserve it via a synchronous insert. The pool's mutex prevents
        // two concurrent founders in the same process from picking the
        // same name; cluster-level races (multi-realm) are caught by the
        // PRIMARY KEY on `name` — duplicate INSERT becomes a noop and
        // the next tick will see the row via LoadFromDb/SweepStale.
        //
        // EscapeString is mandatory here: guild names contain apostrophes
        // ("Sons of Lo'Gosh", "Knights of the Burning Blade", etc.) which
        // would otherwise break out of the SQL literal and trigger an
        // ABORTED Core fix required at MySQLConnection.cpp. PExecute's
        // fmt substitution does NOT auto-escape strings.
        std::string escaped = candidate;
        CharacterDatabase.EscapeString(escaped);
        CharacterDatabase.DirectPExecute(
            "INSERT IGNORE INTO bot_guild_name_reserved (name, faction, founder_low) "
            "VALUES ('{}', {}, {})",
            escaped,
            static_cast<uint32>(f),
            founder_low);

        reserved_.push_back(candidate);
        return candidate;
    }

    // Pool exhausted relative to active reservations. Caller should
    // back off and retry on next tick — reservations age out via
    // SweepStale().
    TC_LOG_INFO("playerbot.v2",
        "[BotGuildNamePool] Pool exhausted for faction={} founder_low={}; "
        "deferring founder election",
        static_cast<uint32>(f), founder_low);
    return {};
}

void BotGuildNamePool::Release(std::string const& name)
{
    if (name.empty()) return;
    std::lock_guard<std::mutex> g(mtx_);
    auto it = std::find(reserved_.begin(), reserved_.end(), name);
    if (it == reserved_.end()) return;
    reserved_.erase(it);

    std::string escaped = name;
    CharacterDatabase.EscapeString(escaped);
    CharacterDatabase.DirectPExecute(
        "DELETE FROM bot_guild_name_reserved WHERE name = '{}'", escaped);
}

void BotGuildNamePool::SweepStale(uint32 max_age_sec)
{
    if (max_age_sec == 0) return;
    std::lock_guard<std::mutex> g(mtx_);

    QueryResult result = CharacterDatabase.PQuery(
        "SELECT name FROM bot_guild_name_reserved "
        "WHERE reserved_at < DATE_SUB(NOW(), INTERVAL {} SECOND)",
        max_age_sec);

    if (!result) return;

    do
    {
        Field* f = result->Fetch();
        std::string name = f[0].GetString();
        auto it = std::find(reserved_.begin(), reserved_.end(), name);
        if (it != reserved_.end()) reserved_.erase(it);

        std::string esc_name = name;
        CharacterDatabase.EscapeString(esc_name);
        CharacterDatabase.DirectPExecute(
            "DELETE FROM bot_guild_name_reserved WHERE name = '{}'", esc_name);
    } while (result->NextRow());
}

} // namespace Playerbot::V2
