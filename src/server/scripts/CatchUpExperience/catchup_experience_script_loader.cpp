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

// Catch-Up Experience -- Arathi Highlands (server map 2927).
// Script loader for the map-2927 Catch-Up Experience scaffold, modeled
// directly on ExilesReach's exiles_reach_script_loader.cpp. This CANNOT be
// compiled in the authoring environment -- compilation and realm testing are
// an explicit Phase-K step (see CATCHUP_BLIZZLIKE_IMPLEMENTATION_PLAN
// task-7-brief.md).

// This is where scripts' loading functions should be declared:
void AddSC_zone_catchup_arathi();
void AddSC_instance_catchup_arathi();

// The name of this function should match:
// void Add${NameOfDirectory}Scripts()
void AddCatchUpExperienceScripts()
{
    AddSC_zone_catchup_arathi();
    AddSC_instance_catchup_arathi();
}
