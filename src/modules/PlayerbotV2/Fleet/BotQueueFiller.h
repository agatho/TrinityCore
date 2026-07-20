// BotQueueFiller - Phase D of WORLD_POPULATION_PLAN.
//
// When a player joins a BG / LFG / LFR queue alone, this service fills the
// remaining slots with bots within seconds. Online bots are preferred;
// shortfall is JIT-spawned via BotCharacterFactory + the setup pipeline,
// then queued.
//
// Composition rules per queue kind:
//   5-man dungeon:  1 tank + 1 healer + 3 dps
//   10-man raid:    2 tanks + 3 healers + 5 dps
//   20-man raid:    2 tanks + 4 healers + 14 dps
//   BG (10/15/40):  faction-balanced; dps-heavy with ~10% T+H
//
// The fill is invoked by the Module on hook events (Hooks::OnPlayerJoined*).

#pragma once

#include "Bot/BotTypes.h"
#include <cstdint>

class Player;

namespace Playerbot::V2::Fleet {

class BotQueueFiller
{
public:
    enum class QueueKind : uint8 { Bg, Dungeon5, Raid10, Raid20 };

    struct FillRequest
    {
        QueueKind kind;
        uint8     bracket;       // level bracket — used when target_level_override is 0
        uint32    faction;       // ALLIANCE / HORDE
        uint32    instance_id;   // bg_type_id or dungeon_id
        Player*   requesting_player;
        // When true, Fill() queues only ALREADY-ONLINE bots — no JIT spawn.
        // The periodic BG top-up uses this so it doesn't keep spawning
        // fresh bots that won't complete setup before the 90s invite
        // window expires (manifesting as "not pressing enter" kicks).
        bool      online_only = false;
        // Explicit target level. When non-zero, Fill() uses this instead
        // of BracketMidpoint(bracket). Needed for LFG where the queueing
        // player may be at a level that doesn't map cleanly to a bracket
        // (L21 → bracket 2 → midpoint 35, but the dungeon is L17-24 and
        // bots at L30+ get LFG_LOCKSTATUS_TOO_HIGH_LEVEL rejections).
        uint8     target_level_override = 0;
        // Hard cap on total bots queued in this Fill (T+H+D). When 0,
        // the per-kind NeedsFor() ratio applies (e.g. 3T/3H/30D = 36
        // for BG). When > 0, Fill() exits as soon as the sum of queued
        // bots reaches max_total_bots — used by the periodic running-BG
        // top-up to inject EXACTLY (max_per_team - current_count) bots
        // per faction, no more. Without this, TopUp queued 36 candidates
        // per side every cycle and the team count overshot max_per_team
        // (observed: WSG with 13 alliance vs 7 horde in a 10v10).
        uint8     max_total_bots = 0;
        // BG-specific: the PVPDifficulty bracket's actual MIN/MAX levels
        // (from DB2). Bots outside this range fail BG queue with
        // `no_bracket` (PVPDifficultyEntry lookup returns null). Without
        // these, the filler used a coarse ±15-window around the target
        // level, which for AV at L14 pulled in L10-17 bots — none of
        // which AV's bracket data accepts. When non-zero (both fields),
        // the filler clamps eligibility strictly to [min, max].
        uint8     bracket_min_level = 0;
        uint8     bracket_max_level = 0;
        // Per-role overrides for the periodic LFG top-up. When non-zero,
        // Fill uses this T/H/D triple instead of NeedsFor(kind). Used by
        // Module::TopUpPendingLfg to request ONLY the deficit (e.g., a
        // queue with 1T+1D already in just needs 1H+2D more, not the
        // full 1T+1H+3D composition). A bare 0/0/0 means "use the kind's
        // default composition".
        uint8     needs_tank_override   = 0;
        uint8     needs_healer_override = 0;
        uint8     needs_dps_override    = 0;
    };

    // Run a fill pass for the queue request. Synchronous: queues online
    // bots immediately, kicks off async JIT-spawns in the background.
    void Fill(FillRequest const& req);

    // Notify the filler that a JIT-tagged bot's match has ended; will
    // log the bot out after retention window. Wired from the hook for
    // OnBgMatchEnd / OnLfgGroupComplete (when those land).
    void OnMatchEnd(Player* bot);
};

} // namespace Playerbot::V2::Fleet
