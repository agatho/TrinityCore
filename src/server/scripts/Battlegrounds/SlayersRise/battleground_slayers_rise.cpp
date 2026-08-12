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

// ---------------------------------------------------------------------------
// Slayer's Rise — Midnight Season 1 40v40 Epic Battleground
//
//   BattlemasterList 1141  [DB2 12.0.7.68887]  "Slayer's Rise", Epic Battleground,
//                          Min 10 / Max 40, InstanceType 3, GroupsAllowed 1/5.
//   Map              2799  [DB2]  "Slayer's Rise" (Voidstorm), InstanceType 3,
//                          ExpansionID 11, WDT 6027385, LoadingScreen 666.
//
// Design (from Map.db2 2799 PvP descriptions — authoritative client strings):
//   Win condition : "Slay the enemy domanaar."
//     - Horde push the Grief Spire and slay Vidious (the Alliance domanaar).
//     - Alliance push the Hate Spire and slay Ziadan (the Horde domanaar).
//   Objectives    : Siege the opposing faction's Bastion; capture Shenzar
//                   Refinery to gain Ethereal assistance.
//
// Framework: reuses the modern BattlegroundScript map-script mechanism (same as
// Alterac Valley / Isle of Conquest). Registered by MapID via
// RegisterBattlegroundMapScript(..., 2799) and bound at runtime through the
// world DB `battleground_scripts` row (ships on this branch, see the SQL).
//
// STATUS: SKELETON. The reinforcement / worldstate spine is live. Node capture,
// the domanaar bosses, spawns and graveyards are TODO(CAPTURE-BLOCKED) — the
// exact spawn/GO coords and boss creature ids are not yet DB2/sniff-anchored.
// See C:\dumps\SLAYERS_RISE_BLUEPRINT.md for the evidence inventory + asks.
// ---------------------------------------------------------------------------

#include "Battleground.h"
#include "BattlegroundScript.h"
#include "Creature.h"
#include "Map.h"
#include "Player.h"
#include "ScriptMgr.h"
#include <array>

namespace SlayersRise
{
    // --- DB2-anchored identity (build 12.0.7.68887) --------------------------
    inline constexpr uint32 MAP_ID               = 2799; // Map.db2
    inline constexpr uint32 BATTLEMASTER_LIST_ID = 1141; // BattlemasterList.db2

    // --- Reinforcement model (mirrors AV/IoC). Cap is a PLACEHOLDER ----------
    // TODO(CAPTURE-BLOCKED): confirm the real starting reinforcement count.
    inline constexpr uint16 MAX_REINFORCEMENTS = 1500;

    // --- Node control worldstates [DB2 AreaPOI.WorldStateID, ContinentID 2799]
    inline constexpr int32 WS_BASTION_OF_VALOR = 29506; // AreaPOI 8378 (Alliance base)
    inline constexpr int32 WS_BASTION_OF_MIGHT = 29509; // AreaPOI 8379 (Horde base)
    inline constexpr int32 WS_SHENZAR_REFINERY = 29510; // AreaPOI 8382 (capturable — Ethereal assist)

    // --- Reinforcement display worldstates -----------------------------------
    // NOT YET CAPTURED. 0 == "unknown / do not push". Guarded at every use.
    // TODO(CAPTURE-BLOCKED): capture INIT_WORLD_STATES on map 2799 for the
    // per-faction reinforcement counters.
    inline constexpr int32 WS_ALLIANCE_REINFORCEMENTS = 0;
    inline constexpr int32 WS_HORDE_REINFORCEMENTS    = 0;

    // --- Domanaar bosses [DB2 Vignette]: Vidious 7169, Ziadan 7170 -----------
    // Creature ids live in the world DB (not client DB2). 0 == disabled.
    // TODO(CAPTURE-BLOCKED): capture the Vidious/Ziadan creature ids on map 2799.
    inline constexpr uint32 NPC_VIDIOUS = 0; // Alliance domanaar @ Grief Spire
    inline constexpr uint32 NPC_ZIADAN  = 0; // Horde domanaar @ Hate Spire

    // --- Objective centres [DB2 AreaPOI.Pos on ContinentID 2799] -------------
    // Grief Spire     AreaPOI 8375 : (3116.45, -1467.79,  -66.16)
    // Hate Spire      AreaPOI 8376 : (3749.13,  1004.42, -129.64)
    // Bastion of Valor AreaPOI 8378: (2902.60,  -825.52, -137.37)
    // Bastion of Might AreaPOI 8379: (3278.65,   514.06, -199.38)
    // Shenzar Refinery AreaPOI 8382: (3829.10,  -442.49, -204.99)
    // Path of Predation AreaPOI 8403:(3256.77,  -206.77, -207.57)
    // (Used as spawn anchors once GO/creature spawn SQL is authored.)

    inline constexpr uint32 RESOURCE_TICK_MS     = 45u * IN_MILLISECONDS; // placeholder cadence
    inline constexpr uint16 RESOURCE_TICK_AMOUNT = 1;
}

struct battleground_slayers_rise : BattlegroundScript
{
    explicit battleground_slayers_rise(BattlegroundMap* map) : BattlegroundScript(map),
        _reinforcements({ }), _resourceTimer(SlayersRise::RESOURCE_TICK_MS)
    {
        _reinforcements = { SlayersRise::MAX_REINFORCEMENTS, SlayersRise::MAX_REINFORCEMENTS };
    }

    void OnInit() override
    {
        BattlegroundScript::OnInit();
        // TODO(CAPTURE-BLOCKED): initialise node ownership (Shenzar Refinery neutral,
        // each Bastion owned by its faction) and spawn spirit guides / capture GOs
        // once the spawn coordinates are captured.
    }

    void OnStart() override
    {
        BattlegroundScript::OnStart();
        UpdateReinforcementWorldStates();
        // TODO: open the faction gates at Bastion of Valor / Bastion of Might.
    }

    void OnUpdate(uint32 diff) override
    {
        BattlegroundScript::OnUpdate(diff);
        if (battleground->GetStatus() != STATUS_IN_PROGRESS)
            return;

        if (_resourceTimer <= diff)
        {
            _resourceTimer = SlayersRise::RESOURCE_TICK_MS;
            // TODO(CAPTURE-BLOCKED): grant periodic resources / Ethereal assistance
            // for the faction controlling Shenzar Refinery.
        }
        else
            _resourceTimer -= diff;
    }

    void OnPlayerKilled(Player* player, Player* killer) override
    {
        BattlegroundScript::OnPlayerKilled(player, killer);
        if (battleground->GetStatus() != STATUS_IN_PROGRESS)
            return;

        TeamId const victimTeamId = Battleground::GetTeamIndexByTeamId(battleground->GetPlayerTeam(player->GetGUID()));
        if (_reinforcements[victimTeamId] > 0)
            _reinforcements[victimTeamId] -= 1;

        UpdateReinforcementWorldStates();

        if (_reinforcements[victimTeamId] < 1 && killer)
            battleground->EndBattleground(battleground->GetPlayerTeam(killer->GetGUID()));
    }

    void OnUnitKilled(Creature* unit, Unit* killer) override
    {
        BattlegroundScript::OnUnitKilled(unit, killer);
        if (battleground->GetStatus() != STATUS_IN_PROGRESS)
            return;

        uint32 const entry = unit->GetEntry();
        // Win condition [DB2 Map.db2 2799]: slay the enemy domanaar.
        if (SlayersRise::NPC_VIDIOUS && entry == SlayersRise::NPC_VIDIOUS)
            battleground->EndBattleground(HORDE);     // Horde slew the Alliance domanaar
        else if (SlayersRise::NPC_ZIADAN && entry == SlayersRise::NPC_ZIADAN)
            battleground->EndBattleground(ALLIANCE);  // Alliance slew the Horde domanaar
    }

    Team GetPrematureWinner() override
    {
        if (_reinforcements[TEAM_ALLIANCE] > _reinforcements[TEAM_HORDE])
            return ALLIANCE;
        if (_reinforcements[TEAM_HORDE] > _reinforcements[TEAM_ALLIANCE])
            return HORDE;
        return BattlegroundScript::GetPrematureWinner();
    }

private:
    void UpdateReinforcementWorldStates() const
    {
        if (SlayersRise::WS_ALLIANCE_REINFORCEMENTS)
            UpdateWorldState(SlayersRise::WS_ALLIANCE_REINFORCEMENTS, _reinforcements[TEAM_ALLIANCE]);
        if (SlayersRise::WS_HORDE_REINFORCEMENTS)
            UpdateWorldState(SlayersRise::WS_HORDE_REINFORCEMENTS, _reinforcements[TEAM_HORDE]);
    }

    std::array<uint16, PVP_TEAMS_COUNT> _reinforcements;
    uint32 _resourceTimer;
};

void AddSC_battleground_slayers_rise()
{
    RegisterBattlegroundMapScript(battleground_slayers_rise, 2799);
}
