#include "RepairVendorIndex.h"

#include "Bot/BotSnapshot.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include "DBCEnums.h"          // FACTION_MASK_ALLIANCE / _HORDE
#include "Log.h"
#include "QueryResult.h"

#include <chrono>
#include <cmath>
#include <limits>

namespace Playerbot::V2::Travel {

namespace {

// Race-id -> team bit. MIRRORS QuestHubDatabase's race_team_bit (and
// QuestHub::FactionAllows) so the repair-vendor faction filter behaves
// identically to the quest-hub one. Bit0 Alliance, Bit1 Horde, Bit2 Neutral.
constexpr uint32 race_team_bit(uint8 race)
{
    switch (race)
    {
        // Alliance
        case 1:  case 3:  case 4:  case 7:  case 11: case 22: case 25:
        case 29: case 30: case 32: case 34: case 37: case 52: case 84:
            return 0x01;
        // Horde
        case 2:  case 5:  case 6:  case 8:  case 9:  case 10: case 26:
        case 27: case 28: case 31: case 35: case 36: case 70: case 85:
            return 0x02;
        // Neutral / unknown (Pandaren-neutral 24, Haranir 86)
        default:
            return 0x04;
    }
}

// Faction bitmask (QuestHub convention) for a repair vendor given its creature
// faction template. A vendor friendly to a faction's players sets that bit; a
// vendor hostile to a faction's players does NOT (a hostile-town vendor is
// useless/lethal). A neutral vendor (friendly to neither team specifically but
// not hostile) is usable by everyone. Computed ONCE at init — off the hot path.
uint32 FactionMaskForVendor(uint32 factionTemplateId)
{
    FactionTemplateEntry const* ft = sFactionTemplateStore.LookupEntry(factionTemplateId);
    if (!ft)
        return 0x07;   // unknown faction → admit (don't strand bots on bad data)

    // A vendor that is hostile to players of a team must never be offered to
    // that team. FactionGroup is the vendor's OWN team membership; a vendor in
    // FACTION_MASK_ALLIANCE is an Alliance vendor (usable by Alliance, hostile
    // to Horde), and vice-versa. Vendors with neither group bit are neutral
    // (goblin towns, sanctuaries) and usable by all non-hostile teams.
    const bool isAlliance = (ft->FactionGroup & FACTION_MASK_ALLIANCE) != 0;
    const bool isHorde    = (ft->FactionGroup & FACTION_MASK_HORDE)    != 0;

    uint32 mask = 0;
    if (isAlliance) mask |= 0x01;
    if (isHorde)    mask |= 0x02;
    if (!isAlliance && !isHorde)
        mask = 0x07;   // neutral: usable by Alliance, Horde, and neutral races
    else
        mask |= 0x04;  // factioned vendors are still usable by truly-neutral races
    return mask;
}

} // anonymous

bool RepairVendorIndex::Initialize()
{
    TC_LOG_INFO("playerbot.v2", "[RepairVendorIndex] Initializing repair-vendor index...");

    const auto t0 = std::chrono::steady_clock::now();
    try
    {
        const uint32 n = LoadFromDB();
        if (n == 0)
        {
            // Not fatal: bots fall back to the nearest quest hub (hubs have
            // vendors) when the index is empty. Warn so an operator notices a
            // bad dataset rather than silently degrading.
            TC_LOG_WARN("playerbot.v2",
                "[RepairVendorIndex] No repair vendors found in DB — bots will "
                "fall back to quest hubs for repair routing.");
        }

        // Release-store publishes the immutable _byMap to lock-free readers.
        _initialized.store(true, std::memory_order_release);

        const auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0).count();
        TC_LOG_INFO("playerbot.v2",
            "[RepairVendorIndex] Ready: {} repair vendors across {} maps in {} ms.",
            _count, _byMap.size(), dt);
        return true;
    }
    catch (std::exception const& ex)
    {
        TC_LOG_ERROR("playerbot.v2", "[RepairVendorIndex] Exception during init: {}", ex.what());
        return false;
    }
}

bool RepairVendorIndex::Reload()
{
    TC_LOG_INFO("playerbot.v2", "[RepairVendorIndex] Reloading...");
    {
        std::unique_lock lk(_mutex);
        _byMap.clear();
        _count = 0;
        _initialized.store(false, std::memory_order_release);
    }
    return Initialize();
}

size_t RepairVendorIndex::GetVendorCount() const
{
    std::shared_lock lk(_mutex);
    return _count;
}

uint32 RepairVendorIndex::LoadFromDB()
{
    // Reuses TC's WORLD_SEL_REPAIR_VENDOR_SPAWNS prepared statement (see
    // src/server/database/Database/Implementation/WorldDatabase.{h,cpp}).
    // Identical column layout to WORLD_SEL_QUEST_GIVER_SPAWNS:
    //   0:guid 1:creature_entry 2:x 3:y 4:z 5:map 6:faction 7:zoneId
    WorldDatabasePreparedStatement* stmt =
        WorldDatabase.GetPreparedStatement(WORLD_SEL_REPAIR_VENDOR_SPAWNS);
    PreparedQueryResult result = WorldDatabase.Query(stmt);

    std::unique_lock lk(_mutex);
    _byMap.clear();
    _count = 0;

    if (!result)
    {
        TC_LOG_WARN("playerbot.v2",
            "[RepairVendorIndex] WORLD_SEL_REPAIR_VENDOR_SPAWNS returned no rows.");
        return 0;
    }

    do
    {
        Field* f = result->Fetch();
        RepairVendorSpawn s;
        s.location.Relocate(f[2].GetFloat(), f[3].GetFloat(), f[4].GetFloat());
        const uint32 mapId          = f[5].GetUInt32();
        const uint32 factionTemplate = f[6].GetUInt32();
        s.factionMask = FactionMaskForVendor(factionTemplate);
        _byMap[mapId].push_back(s);
        ++_count;
    } while (result->NextRow());

    TC_LOG_INFO("playerbot.v2", "[RepairVendorIndex] Loaded {} repair vendors.", _count);
    return uint32(_count);
}

std::optional<RepairVendorHit> RepairVendorIndex::GetNearestRepairVendor(BotSnapshot const& snap) const
{
    if (!_initialized.load(std::memory_order_acquire)) return std::nullopt;
    std::shared_lock lk(_mutex);

    auto it = _byMap.find(snap.position.map_id);
    if (it == _byMap.end()) return std::nullopt;   // cross-map → caller falls back to hubs

    const uint32 botBit = race_team_bit(snap.identity.race);
    const float  bx = snap.position.x;
    const float  by = snap.position.y;
    const float  bz = snap.position.z;

    RepairVendorSpawn const* best = nullptr;
    float bestDist2 = std::numeric_limits<float>::max();
    for (auto const& s : it->second)
    {
        if (!(s.factionMask & botBit)) continue;   // hostile/wrong-faction vendor
        const float dx = bx - s.location.GetPositionX();
        const float dy = by - s.location.GetPositionY();
        const float d2 = dx * dx + dy * dy;
        // Vertical-level sanity. Selection is by HORIZONTAL distance, so a vendor
        // sharing the bot's x,y but sitting on a DIFFERENT FLOOR (a multi-level
        // city) scores as "nearest" yet is unreachable without the connecting ramp
        // — the pathfinder returns Incomplete and the bot pins forever. Observed:
        // Morthan stranded on the Ruins of Lordaeron surface (z≈+61) routed to a
        // repair vendor 124y straight DOWN in the Undercity (z≈-62); Velruun
        // routed to a z≈-132 vendor under the Bloodmyst coast. Skip a vendor whose
        // vertical gap is both large (>30y) AND dominates the horizontal distance
        // (a near-vertical offset = a different level, not a hill we can walk up).
        const float dz = std::fabs(bz - s.location.GetPositionZ());
        if (dz > 30.0f && (dz * dz) > d2)
            continue;   // different floor / unreachable layer — try the next vendor
        if (d2 < bestDist2) { bestDist2 = d2; best = &s; }
    }
    if (!best) return std::nullopt;

    RepairVendorHit hit;
    hit.map_id = snap.position.map_id;
    hit.x = best->location.GetPositionX();
    hit.y = best->location.GetPositionY();
    hit.z = best->location.GetPositionZ();
    return hit;
}

} // namespace Playerbot::V2::Travel
