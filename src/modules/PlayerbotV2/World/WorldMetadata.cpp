#include "WorldMetadata.h"

#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"

#include <fmt/format.h>
#include <algorithm>
#include <cstring>

namespace Playerbot::V2::World
{

namespace
{
// Config-driven shared playerbot DB (Playerbot.SharedDatabase, default
// wowc_playerbot). World metadata is instance-shared curated data, so it lives
// in the shared schema — NOT the swappable characters DB. Read once.
std::string const& SharedDb()
{
    static std::string const db =
        sConfigMgr->GetStringDefault("Playerbot.SharedDatabase", "playerbot");
    return db;
}
} // anonymous

// ---- enum mapping ----------------------------------------------------------

char const* KindToString(WorldMetadataKind k)
{
    switch (k)
    {
        case WorldMetadataKind::Road:      return "road";
        case WorldMetadataKind::Crossroad: return "crossroad";
        case WorldMetadataKind::City:      return "city";
        case WorldMetadataKind::Village:   return "village";
        case WorldMetadataKind::Hub:       return "hub";
        case WorldMetadataKind::Danger:    return "danger";
        case WorldMetadataKind::Vendor:    return "vendor";
        case WorldMetadataKind::Mailbox:   return "mailbox";
        case WorldMetadataKind::Innkeeper: return "innkeeper";
        case WorldMetadataKind::Other:     return "other";
        case WorldMetadataKind::Elevator:  return "elevator";
        case WorldMetadataKind::Dock:      return "dock";
        case WorldMetadataKind::Unknown:
        case WorldMetadataKind::Count_:
        default:                           return "unknown";
    }
}

WorldMetadataKind ParseKind(std::string const& s)
{
    // Case-insensitive compare, common abbreviations + prefix-match
    // accepted. Operator typed text in chat is sloppy; accept any
    // unambiguous prefix of a kind name (e.g. "ro" → road, "vi" →
    // village). Ambiguous prefixes ("c" matches city + crossroad) and
    // empty string return Unknown.
    std::string l = s;
    std::transform(l.begin(), l.end(), l.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (l.empty()) return WorldMetadataKind::Unknown;

    // Explicit aliases first (highest priority — exact match wins).
    if (l == "cross")              return WorldMetadataKind::Crossroad;
    if (l == "warn" || l == "warning")
                                   return WorldMetadataKind::Danger;
    if (l == "mail")               return WorldMetadataKind::Mailbox;
    if (l == "inn")                return WorldMetadataKind::Innkeeper;

    // Then exact match for canonical names.
    if (l == "road")               return WorldMetadataKind::Road;
    if (l == "crossroad")          return WorldMetadataKind::Crossroad;
    if (l == "city")               return WorldMetadataKind::City;
    if (l == "village")            return WorldMetadataKind::Village;
    if (l == "hub")                return WorldMetadataKind::Hub;
    if (l == "danger")             return WorldMetadataKind::Danger;
    if (l == "vendor")             return WorldMetadataKind::Vendor;
    if (l == "mailbox")            return WorldMetadataKind::Mailbox;
    if (l == "innkeeper")          return WorldMetadataKind::Innkeeper;
    if (l == "other")              return WorldMetadataKind::Other;
    if (l == "elevator")           return WorldMetadataKind::Elevator;
    if (l == "dock")               return WorldMetadataKind::Dock;

    // Prefix match — only accept if EXACTLY ONE kind starts with the
    // input. Ambiguous gets rejected (Unknown).
    char const* names[] = {
        "road", "crossroad", "city", "village", "hub",
        "danger", "vendor", "mailbox", "innkeeper", "other",
        "elevator", "dock"
    };
    WorldMetadataKind kinds[] = {
        WorldMetadataKind::Road, WorldMetadataKind::Crossroad,
        WorldMetadataKind::City, WorldMetadataKind::Village,
        WorldMetadataKind::Hub, WorldMetadataKind::Danger,
        WorldMetadataKind::Vendor, WorldMetadataKind::Mailbox,
        WorldMetadataKind::Innkeeper, WorldMetadataKind::Other,
        WorldMetadataKind::Elevator, WorldMetadataKind::Dock
    };
    WorldMetadataKind hit = WorldMetadataKind::Unknown;
    int n_matches = 0;
    for (size_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
    {
        size_t nlen = std::strlen(names[i]);
        if (l.size() <= nlen && std::memcmp(l.data(), names[i], l.size()) == 0)
        {
            hit = kinds[i];
            ++n_matches;
        }
    }
    return n_matches == 1 ? hit : WorldMetadataKind::Unknown;
}

// ---- store -----------------------------------------------------------------

WorldMetadataStore& WorldMetadataStore::Instance()
{
    static WorldMetadataStore inst;
    return inst;
}

int64 WorldMetadataStore::ReloadFromDb()
{
    QueryResult res = CharacterDatabase.Query(fmt::format(
        "SELECT id, map_id, zone_id, kind, pos_x, pos_y, pos_z, radius, "
        "       label, notes, created_by "
        "  FROM {}.playerbot_v2_world_metadata", SharedDb()).c_str());
    if (!res)
    {
        // Empty result is a legitimate "no rows" — distinguishable from a
        // hard error by the row count below being zero rather than negative.
        // Trinity's DatabaseWorkerPool returns nullptr for both empty rs
        // and SQL errors; we treat nullptr as "table missing or query
        // failed". Caller logs and falls back to empty cache.
        std::unique_lock lk(mtx_);
        by_id_.clear();
        loaded_at_ = std::chrono::system_clock::now();
        return 0;
    }

    std::unordered_map<uint64, WorldMetadataRecord> tmp;
    tmp.reserve(res->GetRowCount() * 2);
    do
    {
        Field* f = res->Fetch();
        WorldMetadataRecord r;
        r.id         = f[0].GetUInt64();
        r.map_id     = f[1].GetUInt32();
        r.zone_id    = f[2].GetUInt32();
        const uint8 k = f[3].GetUInt8();
        r.kind       = (k < uint8(WorldMetadataKind::Count_))
                       ? WorldMetadataKind(k)
                       : WorldMetadataKind::Unknown;
        r.x          = f[4].GetFloat();
        r.y          = f[5].GetFloat();
        r.z          = f[6].GetFloat();
        r.radius     = f[7].GetFloat();
        r.label      = f[8].GetString();
        r.notes      = f[9].GetString();
        r.created_by = f[10].GetString();
        tmp.emplace(r.id, std::move(r));
    } while (res->NextRow());

    const int64 n = int64(tmp.size());
    {
        std::unique_lock lk(mtx_);
        by_id_ = std::move(tmp);
        loaded_at_ = std::chrono::system_clock::now();
    }
    TC_LOG_INFO("playerbot.v2",
        "[WorldMetadata] loaded {} record(s) from {}.playerbot_v2_world_metadata",
        n, SharedDb());
    return n;
}

bool WorldMetadataStore::Insert(WorldMetadataRecord& r)
{
    // Persist first; on success cache the row in memory.
    // Trinity's PExecute is async — to get the auto_increment id back we
    // use a synchronous follow-up SELECT against (map_id, kind, x, y)
    // ORDER BY id DESC LIMIT 1 (uniquely identifies the just-inserted row
    // in the normal case; multi-GM race still picks up the newest, which
    // is the GM's own insert).
    //
    // SQL injection guard: label/notes/created_by come from operator input
    // (chat text, character name). MySQL real_escape_string via
    // CharacterDatabase.EscapeString covers single-quote / backslash /
    // control-char cases that would otherwise terminate the literal early.
    std::string elabel    = r.label;
    std::string enotes    = r.notes;
    std::string ecreated  = r.created_by;
    CharacterDatabase.EscapeString(elabel);
    CharacterDatabase.EscapeString(enotes);
    CharacterDatabase.EscapeString(ecreated);
    CharacterDatabase.DirectExecute(fmt::format(
        "INSERT INTO {}.playerbot_v2_world_metadata "
        "(map_id, zone_id, kind, pos_x, pos_y, pos_z, radius, "
        " label, notes, created_by) "
        "VALUES ({}, {}, {}, {}, {}, {}, {}, '{}', '{}', '{}')",
        SharedDb(), r.map_id, r.zone_id, uint32(r.kind),
        r.x, r.y, r.z, r.radius,
        elabel, enotes, ecreated).c_str());
    QueryResult res = CharacterDatabase.Query(fmt::format(
        "SELECT id FROM {}.playerbot_v2_world_metadata "
        "WHERE map_id={} AND kind={} AND pos_x={} AND pos_y={} "
        "ORDER BY id DESC LIMIT 1",
        SharedDb(), r.map_id, uint32(r.kind), r.x, r.y).c_str());
    if (!res)
    {
        TC_LOG_ERROR("playerbot.v2",
            "[WorldMetadata] Insert: row not found after INSERT "
            "(map={} kind={} x={:.1f} y={:.1f})",
            r.map_id, uint32(r.kind), r.x, r.y);
        return false;
    }
    r.id = res->Fetch()[0].GetUInt64();
    {
        std::unique_lock lk(mtx_);
        by_id_[r.id] = r;
    }
    return true;
}

bool WorldMetadataStore::Delete(uint64 id)
{
    if (id == 0) return false;
    CharacterDatabase.DirectExecute(fmt::format(
        "DELETE FROM {}.playerbot_v2_world_metadata WHERE id={}", SharedDb(), id).c_str());
    std::unique_lock lk(mtx_);
    return by_id_.erase(id) != 0;
}

bool WorldMetadataStore::UpdateRadius(uint64 id, float new_radius)
{
    if (id == 0 || new_radius <= 0.0f) return false;
    CharacterDatabase.DirectExecute(fmt::format(
        "UPDATE {}.playerbot_v2_world_metadata SET radius={} WHERE id={}",
        SharedDb(), new_radius, id).c_str());
    std::unique_lock lk(mtx_);
    auto it = by_id_.find(id);
    if (it == by_id_.end()) return false;
    it->second.radius = new_radius;
    return true;
}

bool WorldMetadataStore::UpdateLabel(uint64 id, std::string const& label)
{
    if (id == 0) return false;
    std::string e = label;
    CharacterDatabase.EscapeString(e);
    CharacterDatabase.DirectExecute(fmt::format(
        "UPDATE {}.playerbot_v2_world_metadata SET label='{}' WHERE id={}",
        SharedDb(), e, id).c_str());
    std::unique_lock lk(mtx_);
    auto it = by_id_.find(id);
    if (it == by_id_.end()) return false;
    it->second.label = label;
    return true;
}

bool WorldMetadataStore::UpdateNotes(uint64 id, std::string const& notes)
{
    if (id == 0) return false;
    std::string e = notes;
    CharacterDatabase.EscapeString(e);
    CharacterDatabase.DirectExecute(fmt::format(
        "UPDATE {}.playerbot_v2_world_metadata SET notes='{}' WHERE id={}",
        SharedDb(), e, id).c_str());
    std::unique_lock lk(mtx_);
    auto it = by_id_.find(id);
    if (it == by_id_.end()) return false;
    it->second.notes = notes;
    return true;
}

size_t WorldMetadataStore::Size() const
{
    std::shared_lock lk(mtx_);
    return by_id_.size();
}

std::vector<WorldMetadataRecord> WorldMetadataStore::Snapshot() const
{
    std::vector<WorldMetadataRecord> out;
    std::shared_lock lk(mtx_);
    out.reserve(by_id_.size());
    for (auto const& [_, r] : by_id_) out.push_back(r);
    return out;
}

std::vector<WorldMetadataRecord> WorldMetadataStore::RecordsForMap(uint32 map_id) const
{
    std::vector<WorldMetadataRecord> out;
    std::shared_lock lk(mtx_);
    out.reserve(64);
    for (auto const& [_, r] : by_id_)
        if (r.map_id == map_id)
            out.push_back(r);
    return out;
}

std::vector<WorldMetadataRecord>
WorldMetadataStore::RecordsForMapAndKind(uint32 map_id, WorldMetadataKind kind) const
{
    std::vector<WorldMetadataRecord> out;
    std::shared_lock lk(mtx_);
    out.reserve(64);
    for (auto const& [_, r] : by_id_)
        if (r.map_id == map_id && r.kind == kind)
            out.push_back(r);
    return out;
}

} // namespace Playerbot::V2::World
