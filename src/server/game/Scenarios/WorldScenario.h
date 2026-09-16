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

#ifndef WorldScenario_h__
#define WorldScenario_h__

#include "Scenario.h"

class WorldObject;

/*
    An open-world scenario: owned by a non-instanced map and shared by every player inside its area (the area or any
    of its sub-areas). Retail 12.1 examples are the Vaults of Atal'Utek temple events (map 2916) and Arcana Overload in
    Eversong Woods (map 0); their SMSG_SCENARIO_STATE GUIDs carry the open-world map id.

    Lifecycle as captured:
      - a player entering the area receives a full SMSG_SCENARIO_STATE,
      - a player leaving the area (or the map) receives SMSG_SCENARIO_VACATE with reason Left,
      - on completion every participant receives the completion sequence of Scenario::CompleteStep, then a final
        SMSG_SCENARIO_STATE naming the last step and flagged complete, then SMSG_SCENARIO_VACATE with reason Completed,
        and the scenario ends,
      - a scenario stopped before completion (Stop) vacates its players with reason Left.
    The owning map destroys ended scenarios on its next update.
*/
class TC_GAME_API WorldScenario : public Scenario
{
    public:
        WorldScenario(Map* map, ScenarioData const* scenarioData, uint32 areaId);

        uint32 GetAreaId() const { return _areaId; }
        bool IsInScenarioArea(WorldObject const* object) const;
        bool IsEnded() const { return _ended; }

        // adds or removes the player depending on whether playerAreaId lies inside the scenario area
        void UpdatePlayerMembership(Player* player, uint32 playerAreaId);

        void CompleteScenario() override;
        void Stop();

    protected:
        std::string GetOwnerInfo() const override;

    private:
        uint32 _areaId;
        bool _ended;
};

#endif // WorldScenario_h__
