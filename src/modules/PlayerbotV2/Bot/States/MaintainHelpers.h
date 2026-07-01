// MaintainHelpers - forward declarations for the State_Idle Maintain*
// family. Definitions live in Bot/States/State_Idle.cpp; this header
// lets per-subsystem rule files (e.g. Rules/MaintainRules.cpp) wrap each
// helper as a registered IdleRule without duplicating bodies.

#pragma once

#include "BotTypes.h"
#include "ObjectGuid.h"

namespace Playerbot {

class BotSnapshotView;
class GroupSnapshotView;
class BotAI;
class BotIntentEmitter;
enum class Role : uint8;
struct DungeonAdvice;
struct BattlegroundAdvice;

namespace States {

// Shared dungeon-combat positioning (BUG G-P0a). The avoidance / mechanic
// rules (step-out-of-fire, kite-fixate, soak, spread, stack, melee-behind)
// were originally inline in DungeonDispatch (the idle dungeon cascade), so
// they NEVER executed during combat — the FSM routes combat ticks to
// DispatchInCombat, not DispatchIdle. This single implementation is now
// called from BOTH paths: early in DispatchInCombat (so avoidance preempts
// the melee gap-close) and in DungeonDispatch (so idle ticks still react).
// Returns true if any sub-rule emitted a positioning action (the caller
// should consume the tick / suppress the chase). Cheaply returns false when
// the bot is not in an instance or the advice carries no positioning data.
bool DungeonCombatPositioning(BotSnapshotView const& s, BotAI& ai,
                              GroupSnapshotView const& g,
                              BotIntentEmitter& emit,
                              DungeonAdvice const& advice);

// Off-mesh-aware single step toward a dungeon target (the tank), GUID-keyed with
// float out-params. Resolves the bot from self_guid, then returns the next
// navmesh waypoint (capped at maxStep) toward (tx,ty,tz); sets is_offmesh when
// that step crosses an off-mesh connection (its SOLID far endpoint). Shared so
// the InGroup / InCombat dungeon-rejoins emit the IDENTICAL far-vertex goal the
// idle regroup-cross does — a per-state goal-key mismatch defeats the move_to
// spline dedup and the follower never completes the bridge hop (Gap-1 flip-flop).
// Definition in State_Idle.cpp. Returns false when the bot is gone or no NORMAL
// path exists (caller falls back to a plain move_to for the same-stratum case).
bool DungeonStepTowardTank(ObjectGuid self_guid, float tx, float ty, float tz,
                           float maxStep, float& ox, float& oy, float& oz,
                           bool& is_offmesh);

// Off-mesh crossing COMMITMENT honored in EVERY state (idle / InGroup / InCombat).
// When a follower starts crossing a Gap-1-style off-mesh bridge it stores a fixed
// far-vertex goal (BotAI::set_dungeon_cross). The MoveSpline traverses the off-mesh
// segment in one motion — but if combat (opener/assist) emits a competing move_to
// MID-JUMP, the spline is replaced, the bot stops ON the off-mesh poly over the
// void, and every later move_to NoPaths from there (it is on a valid poly, so the
// FARFROMPOLY recovery never fires either) — a permanent stall (observed live
// 2026-06-26: healer stuck at (-213,-533) 2.5min, tank died unhealed). This helper,
// called FIRST in each dispatch path, re-asserts the fixed far-vertex goal and
// REFRESHES the commit TTL until the bot lands (<=6y), so a slow combat-contested
// crossing can neither expire nor be interrupted — the opener can't pin a mid-jump
// follower. Returns true (caller consumes the tick) while crossing; clears the
// commit and returns false on landing or when no commit is active. now_ms is the
// snapshot publish time. Definition in State_Idle.cpp.
bool DungeonHonorCross(BotSnapshotView const& s, BotAI& ai,
                       BotIntentEmitter& emit, uint32 now_ms);

// Stranded-follower recovery (shared idle / InGroup / InCombat). Relocates a
// non-tank follower that cannot rejoin the group on foot — a navmesh-disconnected
// perch, a void pocket, or a death that respawns it at the far in-instance entrance
// graveyard from which every step NoPaths — onto the rally point (the living tank,
// else the nearest living same-map member, so a dead tank is still recoverable).
// Two no-progress tiers off BotAI's best-distance clock: FAST on a confirmed-
// unreachable path verdict, SLOW (longer dwell) on a "reachable" path the per-state
// rejoin logic demonstrably cannot traverse. Hard-gated (alive non-tank, not
// casting, not in REAL combat — stalker-only false-combat excluded, > kStrandFar).
// Definition in State_Idle.cpp. Run FIRST in each dungeon dispatch path so a
// stranded member relocates in ANY state instead of oscillating the per-state
// rejoin rules forever while the group holds for it (the harbor sole-rezzer stall).
bool DungeonRecoverStrandedFollower(BotSnapshotView const& s, BotAI& ai,
                                    GroupSnapshotView const& g,
                                    BotIntentEmitter& emit, uint32 now_ms);

// Dungeon converge-to-fight. A non-tank bot NOT in combat while its tank IS in
// combat with a real victim closes to the tank (off-mesh aware) so the existing
// assist/opener engages the tank's victim. Fixes the 37-40y dead-band where a
// lagging DPS can neither rejoin (>40y) nor strand-relocate (>40y) yet cannot
// reach the tank's mob on its own, leaving the tank to SOLO a shielded mob it
// cannot finish (the 30-min Helix-approach Envoker deadlock). Definition in
// State_Idle.cpp. Run right AFTER strand-recovery in each dungeon dispatch path:
// strand handles the >40y unreachable TELEPORT, this the reachable pull-in.
bool DungeonConvergeToFight(BotSnapshotView const& s, BotAI& ai,
                            GroupSnapshotView const& g,
                            BotIntentEmitter& emit, uint32 now_ms);

// Shared BG carrier-homeward resolution (BUG BG-P0a). When the bot is
// carrying the flag / an orb, resolve the capture destination (static own
// flag pedestal, else closest owned node, else Kotmogu home_base) and emit a
// run move_to toward it. Returns true if a homeward move was emitted (the
// caller should suppress the melee gap-close so the carrier keeps running
// home instead of chasing its attacker). Returns false when the bot is not a
// carrier or no destination can be resolved.
bool BgCarrierHomeward(BotSnapshotView const& s, BotAI& ai,
                       BotIntentEmitter& emit,
                       BattlegroundAdvice const& bg_advice);

// Shared BG objective-GO interaction (BG audit S1). Scans for the nearest
// opted-in objective GameObject (auto_use_go_types — flag pedestals 36,
// capture points 42, flagstands/orbs 24 — plus auto_use_go_entries for the
// banner BGs whose nodes are generic BUTTON/GOOBER types), skips own fully-
// held nodes, dismounts if needed, walks the dead-zone (5-12y), and emits
// use_game_object within 5y (3s per-GO retry lockout). Returns true if it
// emitted an approach / dismount / use (the caller consumes the tick).
//
// Extracted from the idle auto-use loop so it can ALSO run in combat: the
// idle caller gates on !in_combat(); the State_InCombat caller does NOT, so
// a bot fighting over a defended WSG flag / contested AB-EotS node can still
// click it (server allows BG-object use in combat — Player.cpp
// CanUseBattlegroundObject has no combat check; modern caps are instant, not
// channeled). The defended-flagroom combat-gate was the #1 "no bot picks up
// a flag" blocker. is_alive / casting gating is the caller's responsibility.
bool BgTryUseObjectiveGo(BotSnapshotView const& s, BotAI& ai,
                         BotIntentEmitter& emit,
                         BattlegroundAdvice const& bg_advice);

// Shared BG endgame "push to the boss" mover (AV captain / general; reusable for
// any far same-map objective). The enemy captain/general sits 1400-2300y across
// the AV valley from the home base — far beyond the navmesh PathGenerator's
// partial-path cap (~74 polys / ~292y), so a single move_to (or a straight-line
// ChunkedWalkToward over river/bridge terrain) never advances and the push
// squad froze at the 1-tower lead. This leapfrogs the bot through the script's
// node chain (the GYs + towers, spaced ~150-250y along the valley road): it
// hops to the forward-most node within one navmesh-routable leg that makes net
// progress toward the target, repeating each tick until within kDirect of the
// boss, then emits a full move_to so PathGenerator routes the final approach.
// Returns true if it emitted a move (caller consumes the tick); false only if
// already on top of the target (let the caller's engage logic open on it).
bool BgPushThroughNodes(BotSnapshotView const& s, BotAI& ai,
                        BotIntentEmitter& emit,
                        BattlegroundAdvice const& bg_advice,
                        float tx, float ty, float tz, uint32 now_ms);

// AV walled-boss ENTRANCE staging. The enemy captains/generals sit inside
// walled garrisons/keeps whose interior is navmesh-reachable ONLY through a
// directional entrance — the attackers' approach side dead-ends at the
// perimeter wall (Detour returns a partial; bots pile at the wall, no LoS, 0
// damage). Routing a no-LoS pusher to a LoS-CLEAR INTERIOR staging point just
// inside the real entrance funnels it through the doorway instead of piling at
// the wall; once inside it gains LoS and the engage-on-LoS rule fires. Returns
// true + fills the staging point for a known boss entry, false otherwise (caller
// keeps its direct-to-boss behavior). Coords are interior LoS-clear polys
// verified by mmap_probe + the [avlos] LoS probe.
bool BgBossStagingPoint(uint32 entry, float& sx, float& sy, float& sz);

// Pet-assist target selection (peel-before-assist). Walks pet_attackers for
// a mob nuking the pet while the pet is busy elsewhere; otherwise falls back
// to the pet's own victim. Returns Empty unless the chosen target is a live
// hostile visible in the bot's nearby_enemies. Defined in Rules/MaintainRules.cpp.
// Shared so State_InCombat can assist the pet too: when a pet pulls, the
// owner is flagged in-combat immediately, which routes ticks to
// DispatchInCombat and makes the idle:assist_pet rule (gated on !in_combat)
// unreachable — leaving a pet-class bot standing in combat with no victim.
ObjectGuid SelectAssistPetTarget(BotSnapshotView const& s);

bool MaintainSelfBuff(BotSnapshotView const& s, GroupSnapshotView const& g,
                      BotIntentEmitter& emit);
bool MaintainSoulstone(BotSnapshotView const& s, GroupSnapshotView const& g,
                       BotIntentEmitter& emit);
bool MaintainPet(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit);
bool MaintainGroupUtility(BotSnapshotView const& s, GroupSnapshotView const& g,
                          BotIntentEmitter& emit);
bool MaintainConjuredItem(BotSnapshotView const& s, BotIntentEmitter& emit);
bool MaintainOocFood(BotSnapshotView const& s, BotAI const& ai, BotIntentEmitter& emit);
bool MaintainOocDispel(BotSnapshotView const& s, GroupSnapshotView const& g,
                       BotIntentEmitter& emit);
bool MaintainAutoEquipUpgrades(BotSnapshotView const& s, BotAI& ai,
                               BotIntentEmitter& emit);
bool MaintainBagUpgrade(BotSnapshotView const& s, BotAI& ai,
                        BotIntentEmitter& emit);
bool MaintainContextTalents(BotSnapshotView const& s, BotAI& ai,
                            BotIntentEmitter& emit);
bool MaintainStarterTalents(BotSnapshotView const& s, BotAI& ai,
                            BotIntentEmitter& emit);
bool MaintainStarterTalentsExtend(BotSnapshotView const& s, BotAI& ai,
                                  BotIntentEmitter& emit);
bool MaintainOocHeal(BotSnapshotView const& s, GroupSnapshotView const& g,
                     BotIntentEmitter& emit, Role effective_role);
bool MaintainOocRez(BotSnapshotView const& s, GroupSnapshotView const& g,
                    BotIntentEmitter& emit);

// Dispatchers — extracted from the inline `if (ai.dungeon_active())` /
// `if (ai.bg_active() && s.in_battleground())` blocks in State_Idle.cpp
// (REFACTOR_3 pass 16). Each runs the corresponding sub-rule cascade and
// returns true when any sub-rule fired (consuming the tick).
bool DungeonDispatch(BotSnapshotView const& s, BotAI& ai,
                    GroupSnapshotView const& g,
                    BotIntentEmitter& emit);
bool BgDispatch(BotSnapshotView const& s, BotAI& ai,
                GroupSnapshotView const& g,
                BotIntentEmitter& emit);
bool AutoactDispatch(BotSnapshotView const& s, BotAI& ai,
                    GroupSnapshotView const& g,
                    BotIntentEmitter& emit);

// Extracted travel executors (REFACTOR_3 follow-up) — defined in
// State_Idle.cpp, reused by idle:far_same_map_travel (FarTravelRules.cpp).
bool DriveTravelPlanTo(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit,
                       float bx, float by, float bz,
                       uint32 to_map, float tx, float ty, float tz);
bool DriveRecommendedTaxi(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit,
                          float bx, float by, float bz, bool* out_fell_through);

bool LegacyVendorDispatch(BotSnapshotView const& s, BotAI& ai,
                          GroupSnapshotView const& g,
                          BotIntentEmitter& emit);

// Shared chunked walk toward a same-map point (2026-06-21). Replaces the single
// long emit.move_to() used by the capital / repair / vendor / far-travel walks,
// which climbed onto ledges and stalled on canals/slopes without recovering.
// Steps in personality-scaled chunks with blocked-bearing deflection that
// ESCALATES on a position stall (ledge/corner escape — see BotAI::walk_stall_note)
// and yields (returns false) once a goal is genuinely unreachable this episode so
// stuck-recovery / the watchdog can act. Returns true if it emitted a step (the
// caller consumes the tick), false on arrival (<9y) or give-up. Near targets with
// no stall get a full move_to so PathGenerator navmesh-routes the last leg.
bool ChunkedWalkToward(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit,
                       float tx, float ty, float tz, char const* tag, uint32 now_ms);

} // namespace States
} // namespace Playerbot
