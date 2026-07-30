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
#include "GarrisonMap.h"
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
// Garrison Resources over time. Clicking it collects whatever has banked (Garrison::CollectGarrisonCache).
// Retail gives only the currency-gain toast as confirmation, so we don't emit a system chat message.
struct go_garrison_cache : GameObjectAI
{
    go_garrison_cache(GameObject* go) : GameObjectAI(go) { }

    uint32 _displayTimer = 0;

    Garrison* GetOwnerGarrison() const
    {
        if (Map* map = me->GetMap())
            if (map->IsGarrison())
                return static_cast<GarrisonMap*>(map)->GetGarrison();
        return nullptr;
    }

    // The resource cache swaps its model as Garrison Resources bank up: Normal (< 200), Hefty (200-499),
    // Full (>= 500, capped). DisplayInfoIDs are the per-faction Garrison Cache / Hefty / Full GO templates
    // (Alliance 23775/23773/23777, Horde 23774/23772/23776). Collecting empties it back to the Normal model.
    void RefreshDisplay()
    {
        Garrison* garrison = GetOwnerGarrison();
        if (!garrison || garrison->GetType() != GARRISON_TYPE_GARRISON)
            return;

        uint32 const banked = garrison->GetPendingCacheResources();
        bool const alliance = garrison->GetFaction() == GARRISON_FACTION_INDEX_ALLIANCE;

        uint32 displayId;
        if (banked >= 500)
            displayId = alliance ? 23777 : 23776; // Full
        else if (banked >= 200)
            displayId = alliance ? 23773 : 23772; // Hefty
        else
            displayId = alliance ? 23775 : 23774; // Normal

        if (me->GetDisplayId() != displayId)
            me->SetDisplayId(displayId);
    }

    void UpdateAI(uint32 diff) override
    {
        _displayTimer += diff;
        if (_displayTimer < 5000)
            return;
        _displayTimer = 0;
        RefreshDisplay();
    }

    bool OnGossipHello(Player* player) override
    {
        Garrison* garrison = player->GetGarrison();
        if (!garrison || garrison->GetType() != GARRISON_TYPE_GARRISON)
            return false;

        garrison->CollectGarrisonCache(); // grants the currency (client shows the standard gain toast)
        me->SendCustomAnim(0);            // play the cache's use animation for loot feedback
        RefreshDisplay();                 // banked resources reset to 0 -> revert to the empty (Normal) model
        return true; // the cache is fully handled here — suppress the default goober behaviour
    }
};

// NOTE: the building work-order crate (GAMEOBJECT_TYPE_GARRISON_SHIPMENT) is handled entirely in core
// (GameObject::Use -> Garrison::SendOpenShipmentUI); it needs no GameObject script here.

void AddSC_garrison_generic()
{
    // AreaTrigger
    RegisterAreaTriggerAI(at_garrison_enter);
    RegisterAreaTriggerAI(at_garrison_exit);

    // GameObject
    RegisterGameObjectAI(go_garrison_cache);
}
