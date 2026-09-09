#include "LearnFlightpaths.h"

#include "DB2Stores.h"
#include "Player.h"
#include "DBCEnums.h"

namespace Playerbot::V2::World {

uint32 LearnAllFactionFlightpaths(Player* bot)
{
    if (!bot) return 0;
    const bool alliance = bot->GetTeam() == ALLIANCE;
    uint32 unlocked = 0;
    // Walk the entire TaxiNodesStore. For each node, gate on faction:
    //   - Alliance bots get nodes with ShowOnAllianceMap or
    //     MountCreatureID[1] != 0
    //   - Horde bots get ShowOnHordeMap or MountCreatureID[0] != 0
    // Faction-neutral nodes (Neutral cities, BG/Arena hubs) typically
    // have both flags set; both factions unlock.
    for (TaxiNodesEntry const* node : sTaxiNodesStore)
    {
        if (!node) continue;
        bool allowed = false;
        if (alliance)
        {
            if (node->GetFlags().HasFlag(TaxiNodeFlags::ShowOnAllianceMap))
                allowed = true;
            if (node->MountCreatureID[1] != 0)
                allowed = true;
        }
        else
        {
            if (node->GetFlags().HasFlag(TaxiNodeFlags::ShowOnHordeMap))
                allowed = true;
            if (node->MountCreatureID[0] != 0)
                allowed = true;
        }
        if (!allowed) continue;
        if (bot->m_taxi.SetTaximaskNode(node->ID))
            ++unlocked;
    }
    return unlocked;
}

} // namespace Playerbot::V2::World
