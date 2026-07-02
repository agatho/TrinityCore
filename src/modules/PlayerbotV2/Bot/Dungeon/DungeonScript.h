// DungeonScript — per-dungeon override hooks for the autonomous run
// system. Each registered script is keyed by `map_id` + (optional)
// difficulty; the Idle dungeon rules consult `DungeonScriptMgr` once
// per tick for advice on which adds to focus, which casts to always
// interrupt, which trash to CC, and which auras to step out from.
//
// Generic dungeon logic (tank pulls IsDungeonBoss, DPS assists tank,
// healer keeps tank up, all interrupt + CC marked targets) clears
// 80%+ of dungeons without any per-dungeon override. Scripts are
// pure overrides for the bespoke 20% — encounters where the optimal
// play is non-obvious from the snapshot alone (Cobrahn snake-form
// shift in WC, Greenskin's healer adds in Deadmines, etc.).

#pragma once

#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace Playerbot {

class BotSnapshotView;

// Advice bundle returned by a DungeonScript per tick. Empty fields
// mean "no opinion" — the AI uses generic logic. The advice struct
// is intentionally small and value-typed so the registry can hand
// it out by-value to the AI worker without locking.
struct DungeonAdvice
{
    // Creature_template entries the bot should pull first / focus
    // first when multiple targets are nearby. Higher index = higher
    // priority (front of vector = top priority).
    std::vector<uint32_t> high_priority_kill_entries;

    // Spell ids that MUST be interrupted whenever they're seen mid-
    // cast — overrides the generic "any interruptible cast" logic
    // so the bot doesn't waste interrupts on weak abilities while a
    // wipe-causing one goes off.
    std::vector<uint32_t> mandatory_interrupt_spells;

    // Creature entries that should be CCed (auto-marked moon) on
    // sight when the tank's first pull starts. The CC rule still
    // gates on class/spec compatibility (Mage poly = humanoid only,
    // Hunter trap = beast/humanoid, etc).
    std::vector<uint32_t> cc_priority_entries;

    // Aura spell ids the bot should step away from when applied to
    // self — typically boss ground-effect debuffs (Volcano, Death
    // and Decay, Whirling Blades) where staying in melee = death.
    std::vector<uint32_t> dangerous_auras;

    // ---- M+ affix / advanced raid primitives (added 2026-05-13) ----
    // These extend the script vocabulary so a single per-encounter or
    // per-affix advice block can express the full set of common
    // mechanics. Empty = "no opinion". Idle/combat rules consume only
    // the fields they understand; unknown fields are harmless.

    // Creature entries the DPS bots should kite at distance (M+ Spiteful
    // shade, certain trash adds, raid adds with high melee threat).
    std::vector<uint32_t> kite_creature_entries;
    // Aura spell ids that, when applied to SELF, require the bot to
    // move ≥ N yards from allies (M+ Volcanic / Quaking aftershock,
    // raid debuffs that explode for AoE).
    std::vector<uint32_t> spread_on_self_auras;
    // Boss spell ids that, when seen mid-cast, require all bots to
    // stack on the boss / a designated target (raid stack-soaks).
    std::vector<uint32_t> stack_on_cast_spells;
    // Dispellable debuffs that should be prioritized over normal
    // dispel order (M+ Bursting tick, raid one-shot debuffs).
    std::vector<uint32_t> dispel_priority_spells;
    // Creature entries that gain a power-up when their allies die nearby
    // (M+ Bolstering). Tanks should pull these one-at-a-time; DPS should
    // even out health bars before any single kill.
    std::vector<uint32_t> pull_separately_entries;
    // Enemy buff aura ids that flag a mob as "pull-separately" regardless
    // of its creature entry. Used for M+ Bolstering (the buff is applied
    // by the affix at runtime; we can't enumerate every possible carrier
    // creature). Tank-pull rule scans nearby_enemies[].auras for these
    // ids and applies the same single-pull constraint.
    std::vector<uint32_t> pull_separately_auras;
    // Enemy buff aura ids that should be dispelled / soothed off the
    // carrier (M+ Raging 228318, raid enrage buffs, demonic empower
    // buffs). Consumed by class-aware enrage-dispel hooks (Hunter
    // Tranq Shot 19801, Druid Soothe 2908, Mage Spellsteal 30449).
    // Builder samples mob auras into NearbyUnit.affix_buffs; class APL
    // checks intersection and dispatches the appropriate spell.
    std::vector<uint32_t> dispel_enemy_priority_spells;
    // Boss spell ids that the active tank should taunt-swap on
    // (debuff stacks, fixate cast). Off-tank takes over.
    std::vector<uint32_t> tank_swap_on_spells;
    // Spell ids the bot should soak (intercept by standing in the AoE).
    // Inverted polarity vs dangerous_auras: dangerous = move OUT;
    // soak = move IN. Used for raid orb-soaks, certain M+ mechanics.
    std::vector<uint32_t> soak_spells;

    // Boss creature entries in encounter order. Order is encounter-
    // progression so the dungeon AI can tell whether the run is complete
    // (all bosses dead) and which boss is "next". Used by tank-advance
    // and by diag commands. Live boss creature positions are read from
    // the map; if no waypoints are provided, advancement falls back to a
    // tight-radius Cell scan for these entries.
    std::vector<uint32_t> bosses;

    // Creature entries that bots should NEVER target for combat — purely
    // environmental encounter objects (fire platters, bunny stalkers, rope
    // anchors, etc.) that are alive but unkillable. Without this list,
    // bots get stuck in perpetual interrupted-spell loops against these
    // objects (observed: Glubtok Firewall Platter blocking advance for 90+s).
    std::vector<uint32_t> ignore_entries;

    // Per-dungeon progression waypoints — ordered positions that lead a
    // tank-spec bot through the canonical clear path. The tank-advance
    // rule walks them in sequence; each waypoint is meant to put the
    // tank within nearby_enemies range of the next mob pack / boss room.
    // Path-validation (Detour) still guards against unreachable points,
    // so an out-of-date coord just gets skipped. When empty, the rule
    // falls back to the boss-Cell-scan path (less reliable; can pick
    // a creature past geometry the navmesh has bad coverage for, which
    // produced the "tank ran through a wall" bug in RFC 2026-05-13).
    struct ProgressionPoint { float x; float y; float z; };
    std::vector<ProgressionPoint> progression_waypoints;

    // Per-dungeon INTERMEDIATE routing waypoints — on-navmesh stepping
    // stones used by the boss-navigator to walk a tank toward a boss that
    // is farther than the core 74-poly PathGenerator cap (MAX_PATH_LENGTH).
    // Unlike progression_waypoints these are NOT boss-aligned (no 1:1 index
    // mapping) and are NOT walked by the index fallback or the false-combat
    // escape — they exist only so the boss-nav can chunk a cap-far approach
    // through known-good navmesh points instead of string-pulling along a
    // raw truncated path (which on the Deadmines foundry->harbor descent
    // cuts across an off-mesh ledge and drops the tank). The navigator picks
    // the farthest-forward point that is genuinely closer to the boss AND
    // strictly reachable (clean <=74-poly NORMAL path), steps toward it, and
    // hands back to the direct boss approach once the boss itself is
    // strictly reachable. Detour validates every point, so a stale coord is
    // simply skipped.
    std::vector<ProgressionPoint> route_waypoints;

    // ---- Tight-engagement zone (added 2026-06-29; generalized from the
    // Deadmines harbor / Ripsnarl approach) ----
    // Some encounters end in a dangerous final approach — a chokepoint, boss
    // platform, or descent reached after the main clear — where the group must
    // stay TIGHT and FOCUS-KILL casters instead of CCing/spreading, and the
    // tank must not out-range its healer. When the tank descends BELOW this
    // world-Z the dungeon AI switches into "tight engagement" mode:
    //   * tighter advance-cohesion gate (healer ≤18y, every member ≤25y, ≥85% HP)
    //   * tighter follower regroup radius (14y vs the default 22y) so the group
    //     balls up INSIDE the advance gate — eliminates the 18-22y cohesion
    //     dead-band that otherwise stalls the advance forever
    //   * smaller advance step (10y) so the tank never out-ranges heals
    //   * pre-emptive CC suppressed in favor of focus-killing (DPS assist the
    //     tank's target; high_priority_kill_entries set the kill order)
    //   * proactive DPS-engage on the tank's actual target even in the brief,
    //     leash-prone combat of event-spawned packs
    // This is the reusable toolkit that beat the Deadmines harbor; future
    // instances enable it by setting this Z (and the route_waypoints that lead
    // into the zone). 0.0 (default) = feature OFF (the zone never triggers).
    // The trigger is `tank_z < tight_engage_below_z`, so it suits a tight area
    // that sits LOWER than the rest of the wing (Deadmines harbor floor = z<30,
    // gauntlet = z57-62 → set 30.0). Requires route_waypoints to be set too.
    float tight_engage_below_z = 0.0f;
};

// Abstract per-dungeon override. Subclasses live in
// Bot/Dungeon/Scripts/<region>/<dungeon>.cpp and self-register at
// module init via DungeonScriptMgr::Register.
class DungeonScript
{
public:
    virtual ~DungeonScript() = default;

    // The dungeon this script applies to. Difficulty 0 = any
    // difficulty (script-applicable to Normal AND Heroic AND
    // Mythic without redundant duplicates).
    virtual uint32_t map_id() const = 0;
    virtual uint32_t difficulty_id() const { return 0; }

    // Diagnostic name (used in BotInspector "Dungeon: script=X" line).
    virtual char const* name() const = 0;

    // Per-tick advice. Called by the Idle dungeon rules when the
    // bot is inside the dungeon. Snapshot view is read-only.
    virtual DungeonAdvice get_advice(BotSnapshotView const& s) const = 0;
};

// Registry of all loaded DungeonScripts, keyed by (map_id, difficulty).
// Created and populated at module init via Services::Dungeons().
// Lookup is O(1) via unordered_map.
class DungeonScriptMgr
{
public:
    DungeonScriptMgr() = default;

    // Takes ownership; called once per script at module init. The
    // registry must be assembled before any AI ticks consult it.
    void Register(std::unique_ptr<DungeonScript> script);

    // Register an "always-on" script whose advice is merged with the
    // per-dungeon script's advice on every GetAdvice call, regardless
    // of map_id. Used for M+ affix advice (Sanguine, Spiteful, etc.)
    // and raid-wide patterns. map_id() / difficulty_id() are ignored.
    void RegisterGlobal(std::unique_ptr<DungeonScript> script);

    // Returns the script for `map_id` (and optional matching
    // difficulty). Falls back to a difficulty-0 (any) script if no
    // exact match. nullptr when no script is registered for this
    // map — caller treats as "use generic logic".
    DungeonScript const* GetScriptFor(uint32_t map_id, uint32_t difficulty_id = 0) const;

    // Convenience: returns combined advice for the snapshot's
    // current map. Empty advice when no script is registered.
    DungeonAdvice GetAdvice(BotSnapshotView const& s) const;

    size_t size() const { return scripts_.size(); }

    // Load dungeon route_waypoints from the SHARED playerbot DB
    // ({Playerbot.SharedDatabase}.playerbot_dungeon_routes — populated by
    // gen_dungeon_routes.py and/or the world editor). Keyed by
    // (map_id<<8)|difficulty; INJECTED by GetAdvice for dungeons whose script
    // left route_waypoints empty (the DB is the single route source — no script
    // authors chains anymore). HOT-RELOADABLE: called once at module init and
    // again from `.playerbot reloadroutes` (SOAP) whenever the editor commits
    // route changes — builds the new table off to the side and atomically swaps
    // the shared_ptr under routes_mutex_, so concurrent GetAdvice readers on the
    // AI worker threads keep a consistent (old or new) snapshot and no restart
    // is needed. Returns the number of waypoints loaded.
    size_t LoadGeneratedRoutes();

    // Iterate every registered per-dungeon script (excludes globals).
    // Used by the static `.playerbot smoketest dungeon` validator that
    // walks the registry to verify each script's bosses / interrupts /
    // creature entries resolve through the live ObjectMgr + SpellMgr.
    template <class Fn>
    void for_each_script(Fn fn) const
    {
        for (auto const& [key, script] : scripts_)
            if (script) fn(*script);
    }

private:
    // Key encoding: (map_id << 8) | difficulty_id. Difficulty 0 is
    // the "any" wildcard probed second.
    std::unordered_map<uint64_t, std::unique_ptr<DungeonScript>> scripts_;
    // Global scripts merged on every GetAdvice call. Order of append
    // is preserved; advice is concatenated. Typical use: M+ affix
    // bundles, raid-wide pattern primitives.
    std::vector<std::unique_ptr<DungeonScript>> global_scripts_;
    // DB-sourced route waypoints keyed by (map_id<<8)|difficulty. The map is
    // IMMUTABLE once published; LoadGeneratedRoutes builds a fresh instance and
    // swaps the shared_ptr under routes_mutex_ (hot-reload). GetAdvice readers
    // (AI worker threads, every tick) take a brief shared_lock only to copy the
    // shared_ptr, then read the immutable map lock-free — an in-flight reader
    // keeps the old table alive via its shared_ptr during a swap.
    using RouteTable =
        std::unordered_map<uint64_t, std::vector<DungeonAdvice::ProgressionPoint>>;
    std::shared_ptr<RouteTable const> generated_routes_;
    mutable std::shared_mutex routes_mutex_;
};

} // namespace Playerbot
