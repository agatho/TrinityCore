// PortalPocketIndex — locates "portal rooms" that players reach by walking
// through a teleporter, not by walking in.
//
// Modern capital portal rooms (e.g. the Stormwind Mage Tower "Wizard's
// Sanctum") are physically a navmesh-DISCONNECTED pocket: the portal GOs sit
// on a floor (Stormwind: z~68) that no walkable path reaches from the city.
// You enter by stepping onto a server-side teleporter areatrigger up in the
// Mage Tower (Stormwind: z~148) whose on-enter action casts a teleport spell
// on the entering unit. Because a bot is a real server-side Unit, that
// enter-handler fires for bots too — so a bot that simply WALKS ONTO the
// entrance trigger is teleported into the room, after which the normal portal
// GOs are a short internal walk away.
//
// This index detects those pockets data-drivenly at boot:
//   1. Enumerate custom areatriggers whose on-enter action casts a spell
//      (areatrigger_template_actions.ActionType = 2).
//   2. For each, look for a CLUSTER of >=3 cross-map portal anchors
//      (PortalIndex, kind=Portal) on the same map within a short radius — the
//      signature of a portal room. Dungeon/raid entrance triggers (the bulk of
//      cast-teleport areatriggers) have no portal-GO cluster nearby, so they
//      never form a pocket.
//   3. The gateway must lie OUTSIDE the resulting pocket bbox — that rejects the
//      room-side EXIT trigger (which is inside the room) and keeps the external
//      ENTRANCE.
//
// State_Idle's walk_to_known_portal consults this: if the chosen portal anchor
// is inside a pocket and the bot is not, it walks the bot onto the pocket's
// entrance gateway instead of at the unreachable portal dais.

#pragma once

#include <cstdint>
#include <vector>

namespace Playerbot::V2::Travel {

class PortalIndex;

struct PortalPocket
{
    uint32 map = 0;
    // Entrance teleporter areatrigger position. Walk a bot ONTO this and the
    // server casts the trigger's teleport spell on it, dropping it in the room.
    float gw_x = 0.f, gw_y = 0.f, gw_z = 0.f;
    // Axis-aligned bbox enclosing the room's portal-GO cluster (padded). A unit
    // is "inside the pocket" iff its position is within this box. The Z band is
    // padded tightly so it identifies the room floor without bleeding into the
    // city street that shares the same X/Y footprint far below/above.
    float min_x = 0.f, max_x = 0.f;
    float min_y = 0.f, max_y = 0.f;
    float min_z = 0.f, max_z = 0.f;
};

class PortalPocketIndex
{
public:
    static PortalPocketIndex& Instance();

    // Build from the world's custom teleport areatriggers + the already-built
    // PortalIndex. Call once at boot AFTER PortalIndex::Initialize(). Runs a
    // single WorldDatabase query (like QuestHubDatabase/PortalIndex) then a
    // small in-memory cluster scan; read-only afterwards.
    void Initialize(PortalIndex const& portals);

    bool   IsInitialized() const { return _initialized; }
    size_t Count() const { return _pockets.size(); }

    // Pocket whose bbox contains (x,y,z) on `map`, or nullptr. O(pockets) — a
    // handful of entries — safe to call per-tick from the AI workers.
    PortalPocket const* PocketContaining(uint32 map, float x, float y, float z) const;

    std::vector<PortalPocket> const& Pockets() const { return _pockets; }

private:
    std::vector<PortalPocket> _pockets;
    bool _initialized = false;
};

} // namespace Playerbot::V2::Travel
