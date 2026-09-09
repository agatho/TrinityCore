// BotInspector - Produces a human-readable inspection report for a bot.
// Used by the `.playerbot inspect <name>` GM command. CONTRACTS.md §8.2.

#pragma once

#include "Bot/BotTypes.h"
#include <string>

namespace Playerbot::Diagnostics {

// Returns a multi-line human-readable description of a bot's current state.
// Empty string if the bot doesn't exist.
std::string Inspect(BotId id);

// System-wide perf snapshot as text.
std::string SystemStatus();

// Fleet activity histogram — counts every registered bot's last fired rule
// bucketed into coarse activity categories (Engaged / Wandering / Questing /
// Vendoring / Resting / Idle / etc). Lets a GM verify the autonomous loop
// is firing across the fleet without inspecting each bot individually.
std::string FleetActivity();

// Fleet level histogram — counts every marked bot character (from Lifecycle
// snapshot_ids, joined to character cache for level) bucketed into 5-level
// bins. Lets a GM verify the autonomous loop is actually granting XP /
// leveling bots over time.
std::string FleetLevels();

// Fleet zone distribution — counts every in-world bot's snapshot zone and
// produces a sorted histogram with resolved zone names. Lets a GM verify
// that bots have spread across the world rather than clumping at one
// spawn location, and spot zones that need attention (e.g. all bots stuck
// in starter zones means the level-up loop isn't progressing).
std::string FleetMaps();

// Fleet quest engagement — total quests in flight, fleet-wide objective
// type breakdown (KILL / ITEM / GAMEOBJECT / TALKTO / AREATRIGGER /
// completed-awaiting-turnin), and top-10 most-active quest IDs. Verifies
// the autonomous quest loop is engaging the fleet at scale.
std::string FleetQuests();

// Fleet crafting state — per-profession counts of bots having that skill,
// avg/max skill values, and total known-recipe count. Verifies the
// autonomous craft loop is leveling skills across the fleet.
std::string FleetCrafting();

// Stuck bots — lists bots whose objective_track says blacklisted (stuck
// detection fired after 5 min without progress on the same objective).
// Surfaces bots that picked an unreachable / impossible quest objective
// so the GM can investigate (broken quest data, missing creature spawns,
// cross-map POI, etc).
std::string FleetStuck();

// Fleet BG state — counts bots queued and in-BG, per BG type id, with
// alliance/horde score, time remaining, and FC carrier presence. Lets
// a GM verify the BG queue + auto-port + bg_run flow at fleet scale
// without joining every match.
std::string FleetBg();

// Per-bot diagnostic dump used by the `diag` whisper. Combines snapshot
// identity, intent execution ring, rule-fire history, pipeline state, and
// recent pipeline failures. Empty when the bot id isn't registered.
std::string DiagBot(BotId id);

// Fleet-wide health summary used by `.playerbot health`. Population
// targets, login/spawn budget, intent throughput, setup_pipeline_state
// distribution, DB queue depth (best-effort).
std::string HealthReport();

// Single-screen operator-friendly fleet health summary used by
// `.playerbot fleethealth`. Distills the multi-section HealthReport +
// per-faction/role/queue counts + top-3 stuck bots + TickPerf into ~20
// lines so the operator sees "is the fleet OK" at a glance. Designed for
// the in-game tooltip box (single SendSysMessage call's worth of text).
std::string FleetHealthOneScreen();

// Per-(class, spec, bracket) BG outcome breakdown. Surfaces the data
// recorded by PerfCounters::record_bg_outcome. Sorted by (sample count
// desc, win_rate desc) so the most-represented buckets show first. Used
// by `.playerbot bgstats` as the foundation for future peer-learning:
// owner can see "Sub Rogue at L60-69 wins 70% of WSGs" → confidence that
// the rotation is solid for that bucket.
std::string BgOutcomeReport(size_t max_rows = 30);

// Per-live-BG diagnostic dump. Walks BattlemasterListXMap → free-slot
// queue → each live Battleground, showing team player counts, invited
// counts, free-slot calculations, and queue depths. Used by
// `.playerbot bginfo` to investigate team-imbalance issues
// (alli=13/10 horde=7/10 etc.) without needing direct queue-internal
// access.
std::string BgInfoReport();

// #1B runtime wedge digest used by `.playerbot wedges`. Prints the
// WedgeWatchdog's current active-wedge list grouped by root-cause category
// (Navmesh / OffMesh / Travel / CombatLoop / PickerNone / GoalUnreachable),
// each row showing bot name, map + resolved zone, coords, duration, and
// objective, followed by the per-category lifetime episode totals. Reads the
// watchdog's world-thread-only state, so the command handler must invoke it on
// the world thread (it does — GM command dispatch runs there).
std::string WedgesReport();

// Dump per-bot intent ring + pipeline failure ring + HealthReport to a
// timestamped file under the configured logs dir. Called from the
// FreezeDetector immediately before ABORT_MSG so the forensic state
// survives the crash. Defensively coded - no allocations on the hot
// path beyond what's necessary; missing services are tolerated; any
// thrown exception is caught and dropped (we're already aborting).
//
// `reason` shows up at the top of the file - typically the freeze
// duration, but any short string works.
//
// Returns the dumped file path (empty if dump skipped or failed).
std::string DumpFreezeForensics(std::string const& reason);

} // namespace Playerbot::Diagnostics
