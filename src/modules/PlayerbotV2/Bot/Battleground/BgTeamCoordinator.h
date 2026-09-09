// BgTeamCoordinator — team-level battleground strategy (BG audit N60).
//
// Before this, every bot decided greedily from its own snapshot: identical
// stimuli produced identical decisions (thundering herds), nobody held a
// quota (5 attackers on one node, zero on the next), escorts/hunters were
// whoever's guid hashed right, and there was no opening split or mid-match
// rebalancing. This service computes ONE plan per (battleground instance,
// team) on the WORLD THREAD and publishes per-bot orders; the snapshot
// builder copies each bot's order into its snapshot (BotSnapshot::BgState::
// BgOrder) and the AI executes it, falling back to the legacy per-bot role
// logic whenever no order is present — graceful degradation by design.
//
// Threading: Update() is the only WRITER of orders_ and runs on the WORLD
// THREAD, inline in OnWorldUpdate BEFORE the parallel snapshot-build barrier.
// OrderFor() is a pure const find() READER and (since #5 Phase 4) is called
// concurrently from the snapshot-build WORKER threads. Safe ONLY because the
// writer and the readers are temporally disjoint — orders_ is never mutated
// during the build phase, and concurrent reads of a non-mutating unordered_map
// are well-defined. Move Update() into the parallel phase and this needs a
// shared_mutex. The AI workers only ever see orders through their snapshots.
//
// Inputs (all already maintained elsewhere — this class adds no harvest):
//   * Roster: live Player objects of V2 bots in the BG (registry walk).
//   * Node states / carriers / score: the freshest team member's published
//     snapshot (node_states are map-global; the builder harvests them every
//     snapshot build, including the per-node player-pressure counts).
//   * Static script data: BattlegroundScriptMgr::GetAdvice computed HERE
//     on the world thread from that same immutable snapshot. Never read a
//     BotAI's BgAdviceCache from this class — it is owned by the AI worker
//     thread, which reassigns its heap vectors every ≤2s (use-after-free;
//     adversarial review 2026-06-10).
//
// Strategy families (selected from the advice shape):
//   * CTF        (enemy_flag set, no capture nodes): FC + escorts + EFC
//                hunt squad + mid pressure.
//   * Node race  (capture nodes, no enemy_flag): per-node defense quotas
//                scaled by live enemy pressure, ONE concentrated attack
//                squad, stop-the-cap emergencies, score/time bias.
//   * Orb/hybrid (enemy_flag AND nodes — Kotmogu, Deephaul, EotS): carrier
//                play from the CTF family + node play for the remainder.
// Endgame (advice endgame target + all-in conditions) overrides attackers.
//
// Stability: orders are sticky — a bot keeps its order unless it becomes
// invalid (target flipped to us / carrier died), an emergency outranks it,
// or the periodic full re-plan finds a materially better assignment.

#pragma once

#include "../BotSnapshot.h"
#include <unordered_map>
#include <vector>
#include <string>

class Battleground;

namespace Playerbot {

class BgTeamCoordinator
{
public:
    using BgOrder = BgState::BgOrder;

    BgTeamCoordinator() = default;

    // World tick driver. Re-plans every team of every active bot BG at
    // kPlanIntervalMs. Cheap when no BGs run (single registry walk).
    void Update(uint32 now_ms);

    // Order lookup for the snapshot builder. Returns nullptr when no
    // current plan covers the bot (consumer falls back to legacy logic).
    BgOrder const* OrderFor(uint64 bot_guid_low) const
    {
        auto it = orders_.find(bot_guid_low);
        return it == orders_.end() ? nullptr : &it->second;
    }

    // Human-readable plan dump for `.playerbot bgcoord`.
    std::string DebugDump() const;

private:
    struct Member
    {
        uint64 guid_low = 0;
        float  x = 0.f, y = 0.f, z = 0.f;
        uint8  cls = 0;
        bool   alive = false;
        bool   healer = false;
        bool   tank = false;
        bool   is_carrier = false;
        bool   assigned = false;   // set as the planner hands out orders
    };

    // Slimmed copy of the script's static node list — attack-target
    // priority lookup by position, plus distinct-orb runner targets on
    // Kotmogu (where nodes[] holds the 4 orb spawns).
    struct AdviceNode { float x = 0.f, y = 0.f, z = 0.f; uint8 priority = 0; };

    struct TeamPlanContext
    {
        std::vector<Member>       members;
        std::vector<BgNodeState>  nodes;        // live ownership (small copy)
        std::vector<AdviceNode>   advice_nodes; // static script nodes
        std::vector<uint8>        fc_class_preference;
        uint8  team = 0;                        // 1=alliance 2=horde
        uint16 type_id = 0;
        uint32 instance_id = 0;
        int32  score_delta = 0;                 // my_score - enemy_score
        uint32 in_progress_ms = 0;
        int32  score_bias_threshold = 200;
        // From the advice cache ({0,0,0} sentinel = unset):
        float  enemy_flag_x = 0.f, enemy_flag_y = 0.f, enemy_flag_z = 0.f;
        float  own_flag_x = 0.f, own_flag_y = 0.f, own_flag_z = 0.f;
        float  home_x = 0.f, home_y = 0.f, home_z = 0.f;
        float  endgame_x = 0.f, endgame_y = 0.f, endgame_z = 0.f;
        // AV endgame push: the script flags the boss push as unconditional
        // (enemy reinforcements low) and names the enemy general's entry
        // (Vanndar 11948 / Drek'Thar 11946); the priority-3 advice node is
        // the enemy captain (Galvangar 11947 / Balinda 11949), which the
        // script drops once the captain dies. Default 0/false elsewhere.
        bool   endgame_unconditional = false;
        uint32 endgame_creature_entry = 0;
        float  captain_x = 0.f, captain_y = 0.f, captain_z = 0.f;
        uint32 captain_creature_entry = 0;
        bool   captain_alive = false;
        // From the reference snapshot:
        ObjectGuid friendly_carrier;
        ObjectGuid enemy_carrier;
        float  enemy_carrier_x = 0.f, enemy_carrier_y = 0.f,
               enemy_carrier_z = 0.f;
        // Friendly carriers who are NOT V2 bots (human teammates). The
        // planner can't order them, but must not send a redundant pickup
        // runner on single-flag maps, and still owes them an escort.
        int    external_carriers = 0;
        bool   scalar_carrier_is_external = false;
        float  friendly_carrier_x = 0.f, friendly_carrier_y = 0.f,
               friendly_carrier_z = 0.f;
    };

    void PlanTeam(TeamPlanContext& ctx, uint32 now_ms);
    void PlanCtf(TeamPlanContext& ctx);
    void PlanNodeRace(TeamPlanContext& ctx);
    void AssignOrder(uint64 guid, uint8 kind, float x, float y, float z,
                     ObjectGuid focus = ObjectGuid::Empty, uint8 squad = 0,
                     uint32 target_entry = 0);
    int  PickNearest(std::vector<Member>& members, uint8 kind,
                     float tx, float ty, int healer_bias,
                     bool allow_carrier,
                     ObjectGuid focus = ObjectGuid::Empty) const;
    static uint8 NodePriorityFor(TeamPlanContext const& ctx,
                                 BgNodeState const& n);

    // Published plan (read by the builder via OrderFor) and the plan being
    // built this Update pass. Two maps so OrderFor stays valid mid-plan and
    // the sticky-discount can compare against the previous assignment.
    std::unordered_map<uint64, BgOrder> orders_;
    std::unordered_map<uint64, BgOrder> next_orders_;
    // Per-(instance,team) plan signature for change-only logging.
    std::unordered_map<uint64, uint64>  plan_sig_;
    // Per-(instance,team) STICKY endgame commitment. Once a team commits to the
    // captain->general push (lead_push fired with the captain alive), keep
    // pushing through reinforcement-lead dips until the endgame creature is dead
    // or the team is genuinely crushed — otherwise the AV reinforcement race
    // oscillates the lead around +-75 and the push fizzles before enough bots
    // pile into the captain's courtyard to kill him. Value = commit timestamp
    // (unused beyond presence); key packs (instance_id<<8 | team).
    std::unordered_map<uint64, uint32>  endgame_commit_;
    uint32 last_plan_ms_ = 0;
    std::string last_dump_;
};

} // namespace Playerbot
