/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRINITY_DELVES_COMPANION_H
#define TRINITY_DELVES_COMPANION_H

#include "Define.h"
#include "DelvesDefines.h"
#include "Position.h"

class Creature;
class InstanceMap;
class Player;
struct PlayerCompanionInfoEntry;

namespace Delves
{

struct CompanionState
{
    uint32 CompanionId = 0;
    uint32 Level = 1;
    uint32 Xp = 0;
    CompanionRole SelectedRole = CompanionRole::Dps;
    uint32 CombatCurioNodeId = 0;
    uint32 UtilityCurioNodeId = 0;
};

class TC_GAME_API DelvesCompanion
{
public:
    // Database persistence (account-wide)
    static void LoadFromDB(uint32 battlenetAccountId, CompanionState& state);
    static void SaveToDB(uint32 battlenetAccountId, CompanionState const& state);

    // In-instance companion spawning
    static Creature* SpawnCompanion(InstanceMap* map, Player* owner, CompanionState const& state, Position const& pos);
    static void DespawnCompanion(InstanceMap* map, ObjectGuid companionGuid);

    // Companion info lookup
    static PlayerCompanionInfoEntry const* GetDefaultCompanionInfo();
    static uint32 GetCompanionDisplayId(PlayerCompanionInfoEntry const* info);

    // XP and leveling
    static void AwardCompanionXP(uint32 battlenetAccountId, CompanionState& state, uint32 xpAmount);
    static uint32 GetXPForLevel(uint32 level);
    static uint32 GetMaxCompanionLevel();

    // AI script name for the companion creature
    static constexpr char const* COMPANION_AI_SCRIPT_NAME = "npc_brann_bronzebeard_delves";
};

} // namespace Delves

#endif // TRINITY_DELVES_COMPANION_H
