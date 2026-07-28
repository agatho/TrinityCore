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

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Chat.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "Garrison.h"
#include "Map.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Unit.h"

// XX - Garrison enter AreaTrigger
struct at_garrison_enter : AreaTriggerAI
{
    at_garrison_enter(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) { }

    void OnInitialize() override
    {
        at->setActive(true); // has to be active, otherwise the at is no longer updated before we are able to leave it
    }

    void OnUnitEnter(Unit* unit) override
    {
        Player* player = unit->ToPlayer();
        if (!player)
            return;

        Garrison* garrison = player->GetGarrison();
        if (!garrison)
            return;

        garrison->Enter();
    }
};

// XX - Garrison exit AreaTrigger
struct at_garrison_exit : AreaTriggerAI
{
    at_garrison_exit(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) { }

    void OnInitialize() override
    {
        at->setActive(true); // has to be active, otherwise the at is no longer updated before we are able to leave it
    }

    void OnUnitExit(Unit* unit, AreaTriggerExitReason /*reason*/) override
    {
        Player* player = unit->ToPlayer();
        if (!player)
            return;

        Garrison* garrison = player->GetGarrison();
        if (!garrison)
            return;

        garrison->Leave();
    }
};

// Garrison resource cache: the WoD cache GameObject (types "Garrison Cache" / "Hefty" / "Full") accrues
// Garrison Resources over time. Clicking it collects whatever has banked (Garrison::CollectGarrisonCache);
// the currency toast is the player's confirmation, plus a short message with the exact amount.
struct go_garrison_cache : GameObjectAI
{
    go_garrison_cache(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* player) override
    {
        Garrison* garrison = player->GetGarrison();
        if (!garrison || garrison->GetType() != GARRISON_TYPE_GARRISON)
            return false;

        if (uint32 collected = garrison->CollectGarrisonCache())
            ChatHandler(player->GetSession()).PSendSysMessage("You collect {} Garrison Resources from the cache.", collected);

        return true; // the cache is fully handled here — suppress the default goober behaviour
    }
};

void AddSC_garrison_generic()
{
    // AreaTrigger
    RegisterAreaTriggerAI(at_garrison_enter);
    RegisterAreaTriggerAI(at_garrison_exit);

    // GameObject
    RegisterGameObjectAI(go_garrison_cache);
}
