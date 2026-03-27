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

#ifndef TRINITY_DELVE_INSTANCE_H
#define TRINITY_DELVE_INSTANCE_H

#include "Define.h"
#include "DelvesDefines.h"
#include "ObjectGuid.h"
#include "Position.h"
#include <unordered_set>

class InstanceMap;
class Player;

namespace Delves
{

class TC_GAME_API DelveInstance
{
public:
    DelveInstance(InstanceMap* map, uint8 tier, DelveTemplate const* tmpl);
    ~DelveInstance();

    // Lifecycle
    void OnPlayerEnter(Player* player);
    void OnPlayerExit(Player* player);
    void OnPlayerDeath(Player* player);
    void OnScenarioComplete();
    void Update(uint32 diff);

    // Accessors
    uint8 GetTier() const { return _tier; }
    uint8 GetRemainingRevives() const { return _remainingRevives; }
    bool HasRevivesRemaining() const { return _remainingRevives == DELVE_REVIVES_UNLIMITED || _remainingRevives > 0; }
    DelveState GetState() const { return _state; }
    bool IsBountiful() const { return _bountiful; }
    InstanceMap* GetMap() const { return _map; }
    DelveTemplate const* GetTemplate() const { return _template; }

    // Checkpoint management
    void SetCheckpointPosition(Position const& pos) { _checkpointPos = pos; }
    Position const& GetCheckpointPosition() const { return _checkpointPos; }
    bool HasCheckpoint() const { return _hasCheckpoint; }

    // Update field management
    void PopulateDelveData(Player* player);
    void ClearDelveData(Player* player);

    // Owner checks
    bool IsOwner(ObjectGuid guid) const { return _owners.count(guid) > 0; }

private:
    InstanceMap* _map;
    DelveTemplate const* _template;
    uint8 _tier;
    uint8 _remainingRevives;
    DelveState _state;
    bool _bountiful;

    GuidUnorderedSet _owners;
    ObjectGuid _companionGuid;

    Position _checkpointPos;
    bool _hasCheckpoint = false;
};

} // namespace Delves

#endif // TRINITY_DELVE_INSTANCE_H
