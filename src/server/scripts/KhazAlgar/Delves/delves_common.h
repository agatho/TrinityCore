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

#ifndef TRINITY_DELVES_COMMON_H
#define TRINITY_DELVES_COMMON_H

#include "DelveInstance.h"
#include "InstanceScript.h"

namespace Delves
{

/*
 * Base InstanceScript for all delve instances.
 * Extends InstanceScript with delve-specific lifecycle (revives, companion, checkpoints).
 * Per-delve scripts inherit from this and define boss encounters, objectives, and environmental mechanics.
 */
class DelveInstanceScript : public InstanceScript
{
public:
    DelveInstanceScript(InstanceMap* map, uint8 tier);
    ~DelveInstanceScript() override;

    // InstanceScript overrides
    void OnPlayerEnter(Player* player) override;
    void OnPlayerLeave(Player* player) override;
    void Update(uint32 diff) override;

    // Delve-specific hooks for subclasses to override
    virtual void OnDelveStart() { }
    virtual void OnDelveComplete() { }
    virtual void OnDelveFailed() { }
    virtual void OnCheckpointReached(uint32 checkpointId) { }

    // Scenario integration
    void OnScenarioComplete();

    // Death handling
    void OnPlayerDeath(Player* player);

    // Accessors
    DelveInstance* GetDelveInstance() { return _delveInstance.get(); }
    DelveInstance const* GetDelveInstance() const { return _delveInstance.get(); }

protected:
    std::unique_ptr<DelveInstance> _delveInstance;
};

} // namespace Delves

#endif // TRINITY_DELVES_COMMON_H
