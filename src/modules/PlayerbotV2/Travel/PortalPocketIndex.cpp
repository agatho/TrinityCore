#include "PortalPocketIndex.h"

#include "PortalIndex.h"
#include "DatabaseEnv.h"
#include "Log.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace Playerbot::V2::Travel {

PortalPocketIndex& PortalPocketIndex::Instance()
{
    static PortalPocketIndex instance;
    return instance;
}

void PortalPocketIndex::Initialize(PortalIndex const& portals)
{
    _pockets.clear();

    // --- 1. Entrance candidates: custom areatriggers whose on-enter action
    // casts a spell (ActionType = 2). On this realm the portal-room entrance is
    // a custom AT that casts a teleport spell on the entering unit; because the
    // cast is resolved server-side it fires for bots too. GROUP BY collapses the
    // 1..N action rows per spawn to one position. We deliberately do NOT filter
    // by spell here: any cast-on-enter AT is a candidate and the portal-cluster
    // test below is what actually qualifies it as a portal room.
    QueryResult res = WorldDatabase.Query(
        "SELECT a.MapId, a.PosX, a.PosY, a.PosZ "
        "FROM areatrigger a "
        "JOIN areatrigger_create_properties cp ON cp.Id = a.AreaTriggerCreatePropertiesId "
        "JOIN areatrigger_template_actions act ON act.AreaTriggerId = cp.AreaTriggerId "
        "WHERE a.IsCustom = 1 AND act.ActionType = 2 "
        "GROUP BY a.SpawnId, a.MapId, a.PosX, a.PosY, a.PosZ");

    if (!res)
    {
        TC_LOG_INFO("playerbot.v2",
            "[PortalPocket] no custom cast-on-enter areatriggers; 0 pockets");
        _initialized = true;
        return;
    }

    struct Cand { uint32 map; float x, y, z; };
    std::vector<Cand> cands;
    do
    {
        Field* f = res->Fetch();
        cands.push_back({ f[0].GetUInt32(), f[1].GetFloat(), f[2].GetFloat(), f[3].GetFloat() });
    } while (res->NextRow());

    // --- 2. For each candidate, find a cluster of >=3 cross-map portal anchors
    // on the same map within a short radius. That spatial signature = a portal
    // room reached via the teleporter. Dungeon-entrance triggers have no nearby
    // portal-GO cluster and are dropped.
    constexpr float kClusterR  = 250.0f;            // 2D radius around the trigger
    constexpr float kClusterR2 = kClusterR * kClusterR;
    constexpr float kMinPortals = 3;
    constexpr float kPadXY = 25.0f;                 // bbox horizontal padding
    constexpr float kPadZ  = 12.0f;                 // tight vertical band: the room
                                                    // floor only, not the street that
                                                    // shares the X/Y footprint far off in Z

    for (Cand const& c : cands)
    {
        float minx =  std::numeric_limits<float>::max(), maxx = -minx;
        float miny =  std::numeric_limits<float>::max(), maxy = -miny;
        float minz =  std::numeric_limits<float>::max(), maxz = -minz;
        int   count = 0;

        for (PortalAnchor const& a : portals.Anchors())
        {
            if (a.kind != PortalAnchor::Kind::Portal) continue;
            if (a.source_map != c.map) continue;
            float dx = a.x - c.x, dy = a.y - c.y;
            if (dx * dx + dy * dy > kClusterR2) continue;
            minx = std::min(minx, a.x); maxx = std::max(maxx, a.x);
            miny = std::min(miny, a.y); maxy = std::max(maxy, a.y);
            minz = std::min(minz, a.z); maxz = std::max(maxz, a.z);
            ++count;
        }
        if (count < kMinPortals) continue;

        PortalPocket p;
        p.map  = c.map;
        p.gw_x = c.x; p.gw_y = c.y; p.gw_z = c.z;
        p.min_x = minx - kPadXY; p.max_x = maxx + kPadXY;
        p.min_y = miny - kPadXY; p.max_y = maxy + kPadXY;
        p.min_z = minz - kPadZ;  p.max_z = maxz + kPadZ;

        // The gateway must be OUTSIDE the pocket it leads into. A candidate that
        // sits INSIDE the bbox is the room-side EXIT trigger (it takes you out),
        // not the entrance — reject it so we never strand a bot already in the
        // room by sending it to the exit.
        if (c.x >= p.min_x && c.x <= p.max_x &&
            c.y >= p.min_y && c.y <= p.max_y &&
            c.z >= p.min_z && c.z <= p.max_z)
            continue;

        _pockets.push_back(p);
        TC_LOG_INFO("playerbot.v2",
            "[PortalPocket] map={} gateway=({:.0f},{:.0f},{:.0f}) portals={} "
            "bbox x[{:.0f},{:.0f}] y[{:.0f},{:.0f}] z[{:.0f},{:.0f}]",
            p.map, p.gw_x, p.gw_y, p.gw_z, count,
            p.min_x, p.max_x, p.min_y, p.max_y, p.min_z, p.max_z);
    }

    _initialized = true;
    TC_LOG_INFO("playerbot.v2", "[PortalPocket] initialized {} portal pocket(s)",
        _pockets.size());
}

PortalPocket const* PortalPocketIndex::PocketContaining(uint32 map, float x, float y, float z) const
{
    for (PortalPocket const& p : _pockets)
    {
        if (p.map != map) continue;
        if (x >= p.min_x && x <= p.max_x &&
            y >= p.min_y && y <= p.max_y &&
            z >= p.min_z && z <= p.max_z)
            return &p;
    }
    return nullptr;
}

} // namespace Playerbot::V2::Travel
