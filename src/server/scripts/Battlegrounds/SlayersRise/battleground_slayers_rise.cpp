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
// STATUS: BUILDABLE GAMEPLAY SLICE.
//   IMPLEMENTED (this pass):
//     * Capturable node state machine (neutral -> assaulted -> controlled per
//       faction) for Bastion of Valor / Bastion of Might / Shenzar Refinery,
//       modelled on the Isle of Conquest ICNodePoint state machine, driving the
//       DB2-confirmed AreaPOI worldstates WS 29506 / 29509 / 29510.
//     * Because the capture-flag GameObject ids are NOT datamineable, the node
//       captures run on a COORD-PROXIMITY STAND-IN (area-trigger substitute) at
//       the DB2 AreaPOI centres — clearly flagged; swap for the real capture GOs
//       + DoAction() banner path (IoC pattern) once the GO ids are captured.
//     * Reinforcement spine: player kills drain the victim team; every held
//       capturable node applies siege pressure draining the OPPOSING team; a team
//       reaching 0 ends the battleground for the other team (premature-winner
//       path also present).
//   STUB / TODO(CAPTURE-BLOCKED):
//     * Domanaar bosses (Vidious / Ziadan) creature ids — world DB, not DB2.
//       OnDomanaarKilled() -> EndBattleground() seam is wired and reads the (empty)
//       NPC id constants; it activates the moment the ids land. No fake spawns.
//     * Graveyards + start locations — WorldSafeLocs.db2 is not exposed for build
//       12.0.7.68887 ("Table not found"), so per-base graveyards + spirit guides +
//       real start locs cannot be seeded without invention. See the SQL + §7 asks.
//     * Reinforcement-counter display worldstate ids + starting count — need an
//       INIT_WORLD_STATES capture on map 2799 (AV's 600 used as a flagged default).
//     * Shenzar Refinery "Ethereal assistance" NPC set + capture-flag GO.
// See C:\dumps\SLAYERS_RISE_BLUEPRINT.md for the full evidence inventory + asks.
// ---------------------------------------------------------------------------

#include "Battleground.h"
#include "BattlegroundScript.h"
#include "Creature.h"
#include "Map.h"
#include "Player.h"
#include "Position.h"
#include "ScriptMgr.h"
#include <array>

namespace SlayersRise
{
    // --- DB2-anchored identity (build 12.0.7.68887) --------------------------
    inline constexpr uint32 MAP_ID               = 2799; // Map.db2
    inline constexpr uint32 BATTLEMASTER_LIST_ID = 1141; // BattlemasterList.db2

    // --- Reinforcement model (mirrors AV/IoC). Count is a PLACEHOLDER --------
    // TODO(CAPTURE-BLOCKED): confirm the real starting reinforcement count from
    // an INIT_WORLD_STATES capture on map 2799. AV ships 600; we use that as a
    // sensible flagged default (the blueprint noted 1500 — neither is captured).
    inline constexpr uint16 MAX_REINFORCEMENTS = 600;

    // --- Node control worldstates [DB2 AreaPOI.WorldStateID, ContinentID 2799]
    // Confirmed AreaPOI blip worldstates. Each AreaPOI exposes ONE worldstate id;
    // the client's value ENCODING (neutral / A / H / contested) is not yet
    // captured, so we push an internal NodeStateCode (documented below) and flag
    // it PLACEHOLDER until an INIT_WORLD_STATES / node-transition sniff confirms it.
    inline constexpr int32 WS_BASTION_OF_VALOR = 29506; // AreaPOI 8378 (Alliance base)
    inline constexpr int32 WS_BASTION_OF_MIGHT = 29509; // AreaPOI 8379 (Horde base)
    inline constexpr int32 WS_SHENZAR_REFINERY = 29510; // AreaPOI 8382 (capturable — Ethereal assist)

    // --- Additional DB2-datamined node worldstates on map 2799 (NOT modelled
    //     this pass — listed so the capture asks are complete). Their faction
    //     role / capture rules are not yet established, so we do not invent them:
    //       AreaPOI 8620 "Stareater Pavilion"  WS 29507
    //       AreaPOI 8621 "Shadowridge Outpost" WS 29508
    //       AreaPOI 8645 "Gates of Might"       WS 29511
    //       AreaPOI 8646 "Gates of Valor"       WS 29512
    //     Add them to _nodes[] once their behaviour is captured.

    // --- Reinforcement display worldstates -----------------------------------
    // NOT YET CAPTURED. 0 == "unknown / do not push". Guarded at every use.
    // TODO(CAPTURE-BLOCKED): capture INIT_WORLD_STATES on map 2799 for the
    // per-faction reinforcement counters.
    inline constexpr int32 WS_ALLIANCE_REINFORCEMENTS = 0;
    inline constexpr int32 WS_HORDE_REINFORCEMENTS    = 0;

    // --- Domanaar bosses [DB2 Vignette]: Vidious 7169, Ziadan 7170 -----------
    // Creature ids live in the world DB (not client DB2). 0 == disabled.
    // TODO(CAPTURE-BLOCKED): capture the Vidious/Ziadan creature ids on map 2799.
    // These constants are the (currently empty) source the win-condition seam
    // reads; the seam goes live automatically the moment real ids are filled in.
    inline constexpr uint32 NPC_VIDIOUS = 0; // Alliance domanaar @ Grief Spire (8375)
    inline constexpr uint32 NPC_ZIADAN  = 0; // Horde domanaar @ Hate Spire (8376)

    // --- Objective centres [DB2 AreaPOI.Pos on ContinentID 2799] -------------
    // Grief Spire      AreaPOI 8375 : (3116.45, -1467.79,  -66.16)
    // Hate Spire       AreaPOI 8376 : (3749.13,  1004.42, -129.64)
    // Path of Predation AreaPOI 8403: (3256.77,  -206.77, -207.57)
    // (Bastion/Refinery centres are on the Node table below.)

    // --- Node capture (COORD-PROXIMITY STAND-IN) -----------------------------
    // Placeholder tuning — flagged. Real values want a capture (capture GO radius
    // + cast time). Radius/time chosen to be playable, not authoritative.
    inline constexpr float CAPTURE_RADIUS      = 25.0f;             // yards (PLACEHOLDER)
    inline constexpr uint32 CAPTURE_CHECK_MS   = 1u * IN_MILLISECONDS;
    inline constexpr uint32 CAPTURE_TIME_MS    = 8u * IN_MILLISECONDS; // sole-occupancy dwell to flip (PLACEHOLDER)

    // --- Node-control siege pressure -> reinforcement drain -------------------
    inline constexpr uint32 RESOURCE_TICK_MS     = 15u * IN_MILLISECONDS; // placeholder cadence
    inline constexpr uint16 RESOURCE_DRAIN_BASE  = 1;  // per held capturable node, per tick (PLACEHOLDER)
    inline constexpr uint16 RESOURCE_DRAIN_REFINERY = 2; // Shenzar Refinery "Ethereal assistance" (PLACEHOLDER)

    // Internal node identifiers.
    enum NodeId : uint8
    {
        NODE_BASTION_OF_VALOR = 0, // Alliance base
        NODE_BASTION_OF_MIGHT,     // Horde base
        NODE_SHENZAR_REFINERY,     // neutral, capturable
        NODE_COUNT
    };

    // Full node state machine (mirrors IsleOfConquestNodeState).
    enum class NodeState : uint8
    {
        Neutral,
        ConflictA,   // Alliance assaulting
        ConflictH,   // Horde assaulting
        ControlledA,
        ControlledH
    };

    // Worldstate value ENCODING pushed to the AreaPOI worldstate. PLACEHOLDER —
    // the real client encoding is not captured. Kept small + documented so a
    // single capture can remap it.
    enum NodeStateCode : int32
    {
        NODE_CODE_NEUTRAL     = 0,
        NODE_CODE_CONTROLLED_A = 1,
        NODE_CODE_CONTROLLED_H = 2,
        NODE_CODE_CONFLICT_A   = 3,
        NODE_CODE_CONFLICT_H   = 4
    };
}

struct battleground_slayers_rise : BattlegroundScript
{
    // A single capturable node: DB2 AreaPOI centre + its worldstate + live state.
    struct Node
    {
        SlayersRise::NodeId Id;
        int32 WorldStateId;
        Position Center;
        bool Capturable;                 // false = base seldom flips but still sieged
        SlayersRise::NodeState State;
        TeamId Controller;               // TEAM_NEUTRAL if uncontrolled
        TeamId Assaulter;                // team currently contesting, else TEAM_NEUTRAL
        uint32 CaptureTimer;             // ms of sole-occupancy remaining to flip
    };

    explicit battleground_slayers_rise(BattlegroundMap* map) : BattlegroundScript(map),
        _reinforcements({ }), _resourceTimer(SlayersRise::RESOURCE_TICK_MS), _captureTimer(SlayersRise::CAPTURE_CHECK_MS)
    {
        _reinforcements = { SlayersRise::MAX_REINFORCEMENTS, SlayersRise::MAX_REINFORCEMENTS };

        // Node table — centres are DB2 AreaPOI positions on ContinentID 2799.
        // Bastions begin controlled by their owning faction; the Refinery neutral.
        _nodes = { {
            { SlayersRise::NODE_BASTION_OF_VALOR, SlayersRise::WS_BASTION_OF_VALOR,
              { 2902.60f, -825.52f, -137.37f, 0.0f }, /*Capturable*/ true,
              SlayersRise::NodeState::ControlledA, TEAM_ALLIANCE, TEAM_NEUTRAL, SlayersRise::CAPTURE_TIME_MS },
            { SlayersRise::NODE_BASTION_OF_MIGHT, SlayersRise::WS_BASTION_OF_MIGHT,
              { 3278.65f, 514.06f, -199.38f, 0.0f }, /*Capturable*/ true,
              SlayersRise::NodeState::ControlledH, TEAM_HORDE, TEAM_NEUTRAL, SlayersRise::CAPTURE_TIME_MS },
            { SlayersRise::NODE_SHENZAR_REFINERY, SlayersRise::WS_SHENZAR_REFINERY,
              { 3829.10f, -442.49f, -204.99f, 0.0f }, /*Capturable*/ true,
              SlayersRise::NodeState::Neutral, TEAM_NEUTRAL, TEAM_NEUTRAL, SlayersRise::CAPTURE_TIME_MS },
        } };
    }

    void OnInit() override
    {
        BattlegroundScript::OnInit();
        // Node ownership is initialised in the ctor (Bastions owned, Refinery neutral).
        // TODO(CAPTURE-BLOCKED): spawn spirit guides + capture-flag GameObjects
        // once their ids/coords are captured; until then captures run on the
        // coord-proximity stand-in in OnUpdate().
    }

    void OnStart() override
    {
        BattlegroundScript::OnStart();
        for (Node const& node : _nodes)
            UpdateNodeWorldState(node);
        UpdateReinforcementWorldStates();
        // TODO: open the faction gates at Bastion of Valor / Bastion of Might
        // (Gates of Valor 29512 / Gates of Might 29511) once the gate GOs are seeded.
    }

    void OnUpdate(uint32 diff) override
    {
        BattlegroundScript::OnUpdate(diff);
        if (battleground->GetStatus() != STATUS_IN_PROGRESS)
            return;

        // Node capture stand-in (coord proximity — area-trigger substitute).
        if (_captureTimer <= diff)
        {
            UpdateNodeCaptures(SlayersRise::CAPTURE_CHECK_MS);
            _captureTimer = SlayersRise::CAPTURE_CHECK_MS;
        }
        else
            _captureTimer -= diff;

        // Node-control siege pressure -> reinforcement drain.
        if (_resourceTimer <= diff)
        {
            _resourceTimer = SlayersRise::RESOURCE_TICK_MS;
            TickNodeSiegePressure();
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
        DrainReinforcements(victimTeamId, 1);
    }

    void OnUnitKilled(Creature* unit, Unit* killer) override
    {
        BattlegroundScript::OnUnitKilled(unit, killer);
        if (battleground->GetStatus() != STATUS_IN_PROGRESS)
            return;

        OnDomanaarKilled(unit->GetEntry());
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
    // --- Win condition seam (STUB until creature ids captured) ---------------
    // [DB2 Map.db2 2799]: slaying the enemy domanaar ends the BG. Reads the
    // (currently empty) NPC id constants; a 0 id can never match a real entry,
    // so this is inert until captured — no fake bosses, no invented ids.
    void OnDomanaarKilled(uint32 entry)
    {
        if (SlayersRise::NPC_VIDIOUS && entry == SlayersRise::NPC_VIDIOUS)
            battleground->EndBattleground(HORDE);     // Horde slew the Alliance domanaar
        else if (SlayersRise::NPC_ZIADAN && entry == SlayersRise::NPC_ZIADAN)
            battleground->EndBattleground(ALLIANCE);  // Alliance slew the Horde domanaar
    }

    // --- Reinforcement drain (shared by kills + node siege) ------------------
    void DrainReinforcements(TeamId victimTeamId, uint16 amount)
    {
        if (victimTeamId != TEAM_ALLIANCE && victimTeamId != TEAM_HORDE)
            return;

        if (_reinforcements[victimTeamId] > amount)
            _reinforcements[victimTeamId] -= amount;
        else
            _reinforcements[victimTeamId] = 0;

        UpdateReinforcementWorldStates();

        if (_reinforcements[victimTeamId] == 0)
        {
            // The team that ran the victim out of reinforcements wins.
            TeamId const winnerTeamId = victimTeamId == TEAM_ALLIANCE ? TEAM_HORDE : TEAM_ALLIANCE;
            battleground->EndBattleground(winnerTeamId == TEAM_ALLIANCE ? ALLIANCE : HORDE);
        }
    }

    void UpdateReinforcementWorldStates() const
    {
        if (SlayersRise::WS_ALLIANCE_REINFORCEMENTS)
            UpdateWorldState(SlayersRise::WS_ALLIANCE_REINFORCEMENTS, _reinforcements[TEAM_ALLIANCE]);
        if (SlayersRise::WS_HORDE_REINFORCEMENTS)
            UpdateWorldState(SlayersRise::WS_HORDE_REINFORCEMENTS, _reinforcements[TEAM_HORDE]);
    }

    // --- Node siege: each held capturable node drains the opposing team -------
    void TickNodeSiegePressure()
    {
        for (Node const& node : _nodes)
        {
            if (node.Controller == TEAM_NEUTRAL)
                continue;
            if (node.State != SlayersRise::NodeState::ControlledA && node.State != SlayersRise::NodeState::ControlledH)
                continue;

            uint16 const amount = node.Id == SlayersRise::NODE_SHENZAR_REFINERY
                ? SlayersRise::RESOURCE_DRAIN_REFINERY : SlayersRise::RESOURCE_DRAIN_BASE;

            TeamId const enemyTeamId = node.Controller == TEAM_ALLIANCE ? TEAM_HORDE : TEAM_ALLIANCE;
            DrainReinforcements(enemyTeamId, amount);

            if (battleground->GetStatus() != STATUS_IN_PROGRESS)
                return; // a drain just ended the BG
        }
    }

    // --- Node capture state machine (coord-proximity stand-in) ----------------
    void UpdateNodeCaptures(uint32 diff)
    {
        // Tally players of each team standing within CAPTURE_RADIUS of each node.
        std::array<std::array<uint32, PVP_TEAMS_COUNT>, SlayersRise::NODE_COUNT> occupancy = { };

        battlegroundMap->DoOnPlayers([&](Player* player)
        {
            if (!player->IsAlive())
                return;

            TeamId const teamId = Battleground::GetTeamIndexByTeamId(battleground->GetPlayerTeam(player->GetGUID()));
            if (teamId != TEAM_ALLIANCE && teamId != TEAM_HORDE)
                return;

            for (Node const& node : _nodes)
                if (player->GetExactDist2d(node.Center) <= SlayersRise::CAPTURE_RADIUS)
                    ++occupancy[node.Id][teamId];
        });

        for (Node& node : _nodes)
        {
            if (!node.Capturable)
                continue;

            uint32 const allies = occupancy[node.Id][TEAM_ALLIANCE];
            uint32 const horde  = occupancy[node.Id][TEAM_HORDE];

            // Contested by both, or empty: no progress (hold current state).
            if ((allies > 0 && horde > 0) || (allies == 0 && horde == 0))
                continue;

            TeamId const occupant = allies > 0 ? TEAM_ALLIANCE : TEAM_HORDE;
            AdvanceNodeCapture(node, occupant, diff);
        }
    }

    void AdvanceNodeCapture(Node& node, TeamId occupant, uint32 diff)
    {
        // Sole occupant already controls it: reset any enemy contest (defended).
        if (node.Controller == occupant)
        {
            if (node.Assaulter != TEAM_NEUTRAL)
            {
                node.Assaulter = TEAM_NEUTRAL;
                node.CaptureTimer = SlayersRise::CAPTURE_TIME_MS;
                SetNodeControlled(node, occupant);
            }
            return;
        }

        // New assaulter — start / restart the contest.
        if (node.Assaulter != occupant)
        {
            node.Assaulter = occupant;
            node.CaptureTimer = SlayersRise::CAPTURE_TIME_MS;
            node.State = occupant == TEAM_ALLIANCE ? SlayersRise::NodeState::ConflictA : SlayersRise::NodeState::ConflictH;
            // TODO(CAPTURE-BLOCKED): award a "node assaulted" PvP stat once the
            // Slayer's Rise PvP-stat id is captured (no DB2 anchor for it yet).
            UpdateNodeWorldState(node);
            return;
        }

        // Continued sole occupancy by the assaulter — tick down to capture.
        if (node.CaptureTimer > diff)
        {
            node.CaptureTimer -= diff;
            return;
        }

        node.CaptureTimer = SlayersRise::CAPTURE_TIME_MS;
        node.Assaulter = TEAM_NEUTRAL;
        SetNodeControlled(node, occupant);
    }

    void SetNodeControlled(Node& node, TeamId team)
    {
        node.Controller = team;
        node.State = team == TEAM_ALLIANCE ? SlayersRise::NodeState::ControlledA : SlayersRise::NodeState::ControlledH;
        UpdateNodeWorldState(node);
        // TODO(CAPTURE-BLOCKED): on Shenzar Refinery capture, spawn/empower the
        // "Ethereal assistance" NPC set (ids not datamineable). The siege-pressure
        // drain (TickNodeSiegePressure) already rewards holding it.
        battlegroundMap->UpdateSpawnGroupConditions();
    }

    // Push the AreaPOI worldstate. Value is a PLACEHOLDER encoding (see NodeStateCode)
    // pending an INIT_WORLD_STATES / node-transition capture on map 2799.
    void UpdateNodeWorldState(Node const& node) const
    {
        if (!node.WorldStateId)
            return;

        int32 code = SlayersRise::NODE_CODE_NEUTRAL;
        switch (node.State)
        {
            case SlayersRise::NodeState::ControlledA: code = SlayersRise::NODE_CODE_CONTROLLED_A; break;
            case SlayersRise::NodeState::ControlledH: code = SlayersRise::NODE_CODE_CONTROLLED_H; break;
            case SlayersRise::NodeState::ConflictA:   code = SlayersRise::NODE_CODE_CONFLICT_A;   break;
            case SlayersRise::NodeState::ConflictH:   code = SlayersRise::NODE_CODE_CONFLICT_H;   break;
            case SlayersRise::NodeState::Neutral:     code = SlayersRise::NODE_CODE_NEUTRAL;      break;
        }

        UpdateWorldState(node.WorldStateId, code);
    }

    std::array<uint16, PVP_TEAMS_COUNT> _reinforcements;
    std::array<Node, SlayersRise::NODE_COUNT> _nodes;
    uint32 _resourceTimer;
    uint32 _captureTimer;
};

void AddSC_battleground_slayers_rise()
{
    RegisterBattlegroundMapScript(battleground_slayers_rise, 2799);
}
