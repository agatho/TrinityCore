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

#ifndef TRINITY_DELVES_SEASON_H
#define TRINITY_DELVES_SEASON_H

#include "Define.h"
#include <vector>

class Player;

namespace Delves
{

class TC_GAME_API DelvesSeason
{
public:
    // Season queries
    static uint32 GetCurrentSeasonNumber();
    static uint32 GetFactionIdForSeason(uint32 seasonId);
    static std::vector<int32> GetSeasonSpellIds(uint32 seasonId);

    // Season spell management (applied on login / delve entry)
    static void ApplySeasonSpells(Player* player);
    static void RemoveSeasonSpells(Player* player);

    // Minimum level check (uses ContentTuning ID 2677)
    static bool MeetsMinimumLevelRequirement(Player const* player);
};

} // namespace Delves

#endif // TRINITY_DELVES_SEASON_H
