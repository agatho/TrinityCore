/*
 * HandcraftedRoadStorage - worldserver-side loader / accessor.
 * See header for design notes.
 */

#include "HandcraftedRoadStorage.h"

#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "StringFormat.h"
#include "Timer.h"

#include <algorithm>

std::unordered_map<uint32, std::vector<HandcraftedRoadSegment>> HandcraftedRoadStorage::_byMap;
std::vector<HandcraftedRoadSegment> HandcraftedRoadStorage::_empty;
std::atomic<uint32> HandcraftedRoadStorage::_generation{0};

void HandcraftedRoadStorage::LoadFromDB()
{
    uint32 const oldMSTime = getMSTime();

    _byMap.clear();
    // Bump first so any consumer that rebuilds mid-reload still ends up newer
    // than its last-built generation and rebuilds again on the next request.
    _generation.fetch_add(1, std::memory_order_acq_rel);

    // Shared playerbot DB (Playerbot.SharedDatabase, default wowc_playerbot) so
    // road data is authored once and reused across every world-DB variant — it
    // does NOT live in the swappable world DB. Cross-DB qualified read on the
    // WorldDatabase connection (the configured user has access to all schemas).
    // SELECT order matches the column ordinals used below; keep them in lock-step.
    std::string const sharedDb = sConfigMgr->GetStringDefault("Playerbot.SharedDatabase", "playerbot");
    QueryResult result = WorldDatabase.Query(Trinity::StringFormat(
        "SELECT id, mapId, fromX, fromY, toX, toY, width, "
        "COALESCE(comment, ''), verified "
        "FROM {}.handcrafted_road ORDER BY mapId, id", sharedDb).c_str());

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 handcrafted road segments. Table `handcrafted_road` is empty or missing.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        HandcraftedRoadSegment seg;
        seg.id       = fields[0].GetUInt32();
        seg.mapId    = fields[1].GetUInt32();
        seg.fromX    = fields[2].GetFloat();
        seg.fromY    = fields[3].GetFloat();
        seg.toX      = fields[4].GetFloat();
        seg.toY      = fields[5].GetFloat();
        seg.width    = fields[6].GetFloat();
        seg.comment  = fields[7].GetString();
        seg.verified = fields[8].GetUInt8() != 0;

        // Zero-or-negative width is bogus geometry; skip rather than tag the
        // entire navmesh with a degenerate segment.
        if (seg.width <= 0.0f)
        {
            TC_LOG_ERROR("sql.sql", "handcrafted_road id={} has non-positive width={}, skipped.", seg.id, seg.width);
            continue;
        }

        _byMap[seg.mapId].push_back(std::move(seg));
        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} handcrafted road segments across {} map(s) in {} ms",
        count, uint32(_byMap.size()), GetMSTimeDiffToNow(oldMSTime));
}

std::vector<HandcraftedRoadSegment> const& HandcraftedRoadStorage::GetForMap(uint32 mapId)
{
    auto it = _byMap.find(mapId);
    return (it != _byMap.end()) ? it->second : _empty;
}

size_t HandcraftedRoadStorage::SegmentCount()
{
    size_t n = 0;
    for (auto const& [_, segs] : _byMap)
        n += segs.size();
    return n;
}

size_t HandcraftedRoadStorage::MapCount()
{
    return _byMap.size();
}

std::vector<uint32> HandcraftedRoadStorage::MapIds()
{
    std::vector<uint32> ids;
    ids.reserve(_byMap.size());
    for (auto const& [mapId, _] : _byMap)
        ids.push_back(mapId);
    std::sort(ids.begin(), ids.end());
    return ids;
}

uint32 HandcraftedRoadStorage::Generation()
{
    return _generation.load(std::memory_order_acquire);
}
