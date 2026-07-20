// BattlegroundScript — per-battleground override hooks for the
// autonomous BG-run system. Mirrors the DungeonScript pattern;
// each registered script is keyed by `bg_type_id` (BattlemasterList.dbc).
//
// Generic BG logic (fight nearest enemy, heal lowest-HP teammate)
// works for combat positioning but doesn't know objectives. Scripts
// fill in: which objective to push, who carries the flag, when to
// chase enemy carrier, role assignments per bot index.

#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Playerbot {

class BotSnapshotView;

// Role assigned to a bot for the duration of the BG. Determined by
// script + bot's class/spec/index. Drives objective-aware actions.
enum class BgRole : uint8_t
{
    Free       = 0,   // No role — fall back to generic combat.
    FlagCarrier= 1,   // Pick up + carry the flag (CTF).
    FCEscort   = 2,   // Stay with FC; intercept enemies.
    Defender   = 3,   // Stay at home objective.
    Attacker   = 4,   // Push enemy objective.
    Roamer     = 5,   // Mid-map roving (interrupt enemy moves).
    Healer     = 6,   // Healer-spec — focus FC if applicable.
    // OrbCarrier — Kotmogu orb mechanic. Like FlagCarrier, picks up an
    // enemy_flag-tagged GO; UNLIKE FlagCarrier, does NOT return home to
    // cap. After pickup, holds at home_base (map center) to accumulate
    // the SmallAura distance-from-center score multiplier. Multiple
    // OrbCarrier slots are expected (ToK has up to 4 carriers per side).
    OrbCarrier = 7,
};

struct BattlegroundAdvice
{
    // Per-bot role assignment indexed by formation_slot.
    // Empty = no role; bot uses generic logic.
    std::vector<BgRole> role_by_slot;

    // Should the bot chase the enemy flag carrier (CTF only)?
    // Honored only when bg_enemy_flag_carrier is non-empty.
    bool  chase_enemy_carrier = false;

    // Should the bot escort the friendly flag carrier?
    bool  escort_friendly_carrier = false;

    // CTF-specific: the bot's own-flag pickup position (where the
    // friendly faction flag spawns) and the enemy flag pickup position
    // (where to go grab the cap target). {0,0,0} sentinel = no advice.
    // Populated by WSG/TP/BfG scripts. Faction selection (which one is
    // "friendly" vs "enemy") is the script's responsibility — it reads
    // the snapshot's faction context inside get_advice.
    float own_flag_x = 0.f, own_flag_y = 0.f, own_flag_z = 0.f;
    float enemy_flag_x = 0.f, enemy_flag_y = 0.f, enemy_flag_z = 0.f;

    // Default home-base hold position for Defender role. Same {0,0,0}
    // sentinel meaning. Set by node-race scripts (AB, BfG, AV, EotS).
    float home_base_x = 0.f, home_base_y = 0.f, home_base_z = 0.f;

    // Score lead/deficit (my_score - enemy_score) at which the tactical
    // bias flips to Turtle (>= +threshold) / AllIn (<= -threshold).
    // Units are whatever the BG's score worldstates count: resource
    // points for AB/BfG/EotS/Kotmogu (max 1500-2000 -- default 200 is a
    // "noticeable lead"), but FLAG CAPS for WSG/TP (max 3) where 200
    // could never trigger (BG audit N55/N65) -- CTF scripts set 2.
    int32_t score_bias_threshold = 200;

    // Endgame target — used by Attacker / Roamer roles when match is
    // in "all-in" tactical state (score-bias = AllIn or BG-specific
    // late-phase trigger). For AV it points at the enemy keep / boss;
    // for IoC the enemy fortress door. {0,0,0} = no endgame override
    // (Attackers continue with normal node-push logic).
    float endgame_target_x = 0.f, endgame_target_y = 0.f, endgame_target_z = 0.f;

    // When true, the Attacker endgame redirect to endgame_target fires
    // UNCONDITIONALLY (not only under the Bias_AllIn score state). Used by
    // round-based objective BGs whose score worldstates stay 0/0 all match
    // so the score-derived bias never leaves Normal (SoTA: AddPoint is never
    // called, so attackers were never driven to the breach / relic and the
    // match could not end — BG audit SoTA blocker). Default false keeps the
    // score-gated behavior for point-race BGs (AV/IoC).
    bool endgame_unconditional = false;

    // Optional NPC entry of the boss/general whose kill ends the match
    // (AV Drek'thar/Vandar, IoC Halford/Agmar, Ashran Volrath/Tremblade).
    // When set AND a NearbyUnit with this entry is visible to the bot,
    // the Attacker endgame rule chases the live unit position instead of
    // the static endgame_target coord. Handles bosses that walk around /
    // path into adjacent rooms; the static coord is the fallback used
    // until the bot is close enough to acquire the unit.
    uint32_t endgame_creature_entry = 0;

    // CTF enemy-flag-carrier chase gate. When true AND chase_enemy_carrier
    // is true, only melee classes (Warrior, Rogue, DK, Monk, Feral Druid,
    // Ret Pala, Enh Sham, DH, Hunter) chase — clothie casters stay on
    // their objective. Without this, mages/priests sprint into a moving
    // FC at 1v1 range and feed honor across the map. Default false keeps
    // pre-existing chase behavior (everyone chases).
    bool chase_melee_only = false;

    // Preferred classes for the FlagCarrier role. When non-empty, the
    // BG dispatcher applies a deterministic, per-bot role override:
    //   * Bots whose class IS in this list get my_role = FlagCarrier
    //     regardless of their hashed slot (so stealth classes always
    //     take the FC job in WSG/TP).
    //   * Bots whose class is NOT in this list AND whose hashed slot
    //     WOULD have given them FlagCarrier get demoted to Roamer
    //     (so the FC role is never wasted on a clothie that can't
    //     survive the kite). The acts_as_fc dynamic-handoff path
    //     handles the case where no preferred-class bot is present.
    // Class ids match SharedDefines::CLASSMASK_* (Warrior=1 ... Evoker=13).
    // Empty = no override (default behavior — slot 0 is whichever class
    // happens to hash there).
    std::vector<uint8_t> fc_class_preference;

    // Per-node positions for node-race BGs (AB/BfG/EotS/Gorge/IoC/AV).
    // When non-empty, takes precedence over `home_base_*` for Defender
    // role: each Defender slot is assigned a node (round-robin over
    // node count) and moves to that node's coords. Attackers and
    // Roamers iterate through these positions to push undefended /
    // contested nodes.
    //
    // `priority`: 0 = default; >0 = high-value target. The Attacker rule
    // applies it as a tie-breaker when two nodes have the same
    // ownership-priority bucket (neutral / contested / enemy-held).
    // Used by AV to bias toward towers/bunkers (which drain more
    // reinforcements per cap than graveyards). 0 for all script's
    // current nodes leaves behavior unchanged.
    struct Node {
        float x = 0.f, y = 0.f, z = 0.f;
        char const* name = "";
        uint8_t priority = 0;
        // When non-zero, consumer should resolve the node's LIVE position
        // by scanning nearby_friends + nearby_enemies for the closest Unit
        // with this creature entry and using its m_position. Falls back
        // to the static x/y/z when no live unit visible. Used for moving
        // objectives (Silvershard cart entry 60140) — the static coord is
        // the cart's spawn point but the cart moves on rails over 30s+.
        uint32_t follow_creature_entry = 0;
    };
    std::vector<Node> nodes;

    // Auto-use any GO of these types when within ~5 yards. Lets the
    // bot hit AB/EotS/BfG flagstands and AV banners on contact without
    // per-script coords. Empty = generic logic only. Common values:
    //   * GAMEOBJECT_TYPE_FLAGSTAND (24) — capturable nodes.
    //   * GAMEOBJECT_TYPE_FLAGDROP  (26) — dropped flags (return).
    std::vector<uint32_t> auto_use_go_types;

    // Auto-use any GO with one of these ENTRIES when within ~5 yards
    // (audit B26): AV / IoC / SotA banners are GO type 1 (BUTTON) and
    // type 10 (GOOBER) — blanket-auto-using those TYPES is unsafe (doors,
    // levers, quest props share them), so these maps enumerate the exact
    // DB-verified banner / relic entries instead. Without this, bots on
    // the epic BGs could never assault/defend a node or cap the SotA
    // Titan Relic (the literal win condition). Empty = type matching only.
    std::vector<uint32_t> auto_use_go_entries;

    // Creature entries the bot should mount when within range (8y).
    // Lets SoTA bots hop into demolishers, IoC bots into siege engines /
    // glaives / catapults. The bot picks the closest matching nearby
    // friendly Creature with an empty driver seat. Empty = no auto-mount.
    std::vector<uint32_t> vehicle_creature_entries;

    // Default spell ID the bot fires from its current vehicle seat once
    // mounted and an enemy is in range/LoS. Typically the seat 0 driver
    // ability (boulder hurl, glaive throw etc). 0 = no automatic seat
    // fire — the bot mounts but waits for owner direction. Used as
    // fallback when no entry-specific override is registered.
    uint32_t vehicle_seat_spell = 0;

    // Per-vehicle-entry spell override. Looked up by the bot's current
    // vehicle creature entry; falls back to vehicle_seat_spell on miss.
    // Used by IoC where Demolisher / Siege Engine / Glaive Thrower /
    // Catapult / Keep Cannon each fire a different primary ability.
    std::unordered_map<uint32_t, uint32_t> vehicle_seat_spell_by_entry;

    // DESTRUCTIBLE_BUILDING GameObject entries a SIEGE VEHICLE should fire
    // its seat spell AT (cast_vehicle_at the gate's live position). These are
    // the ENEMY gates the bot's team must breach — IoC enemy keep gates,
    // SoTA defense-line gates on the attacker round. When non-empty and the
    // bot is in a vehicle with a seat spell, the bg_vehicle_fire_gate rule
    // targets the closest standing (is_destroyed==false) matching gate before
    // it looks for unit targets. Empty = no gate-fire (unit fire only).
    // Without this, the seat-fire rule only saw Units (nearby_enemies) and
    // never the gate GOs, so gates never fell and the General was never
    // reached (BG audit IoC / SoTA siege blockers).
    std::vector<uint32_t> siege_target_go_entries;

    // Arena tactical positions — pillars / cubbies for LoS breaks +
    // hazard zones to avoid. Distinct from `nodes[]` (which feeds the
    // Attacker/Defender objective-push pipeline) — arena pillars/hazards
    // ONLY drive the `idle:arena_position` rule. Empty for all non-arena
    // BGs (no behavioural change there).
    struct ArenaPillar {
        float       x = 0.f, y = 0.f, z = 0.f;
        char const* name = "";
        // 0 = LoS pillar (ranged hides behind, healer LoS-breaks melee)
        // 1 = cubby      (corner cover, also a fallback when in hazard)
        // 2 = high-ground (perch — preferred starting position for ranged)
        uint8_t     kind = 0;
    };
    std::vector<ArenaPillar> arena_pillars;

    struct ArenaHazard {
        float       x = 0.f, y = 0.f, z = 0.f;
        float       radius = 5.f;
        char const* name   = "";
        // Time-gated activation (ms since match start). 0 = always
        // active. Ring of Valor pillar elevators: active_after_ms=60000.
        uint32_t    active_after_ms = 0;
        uint32_t    active_until_ms = 0;  // 0 = forever
    };
    std::vector<ArenaHazard> arena_hazards;

    // Where to advance when the starting gate drops. {0,0,0} sentinel =
    // don't override (default arena-center movement applies).
    float opening_rally_x = 0.f, opening_rally_y = 0.f, opening_rally_z = 0.f;
};

class BattlegroundScript
{
public:
    virtual ~BattlegroundScript() = default;
    virtual uint16_t bg_type_id() const = 0;
    virtual char const* name() const = 0;

    // Per-tick advice. Snapshot view is read-only.
    virtual BattlegroundAdvice get_advice(BotSnapshotView const& s) const = 0;
};

class BattlegroundScriptMgr
{
public:
    BattlegroundScriptMgr() = default;

    void Register(std::unique_ptr<BattlegroundScript> script);
    BattlegroundScript const* GetScriptFor(uint16_t bg_type_id) const;
    BattlegroundAdvice GetAdvice(BotSnapshotView const& s) const;
    size_t size() const { return scripts_.size(); }

    // Iterate every registered base script (variants registered via
    // AliasToBaseBg are not in the map). Used by the static
    // `.playerbot smoketest bg` validator that walks the registry
    // to verify each script's coords / GO types / vehicle entries
    // resolve through ObjectMgr + a sane data range.
    template <class Fn>
    void for_each_script(Fn fn) const
    {
        for (auto const& [key, script] : scripts_)
            if (script) fn(*script);
    }

    // Resolve a bg_type_id to its base script WITHOUT emitting the
    // "no script registered" WARN. Used by alias-coverage assertions
    // that intentionally probe IDs which may legitimately have no
    // base (queue meta-IDs like RB/RATED/RANDOM).
    BattlegroundScript const* TryGetScriptFor(uint16_t bg_type_id) const;

private:
    std::unordered_map<uint16_t, std::unique_ptr<BattlegroundScript>> scripts_;
};

// Cross-bot callout coordinator. When 40 AV bots all simultaneously
// detect "FC down", a per-bot 20s lockout doesn't dedup the chat — 30
// "FC down" lines hit raid-chat in the same tick. This shared registry
// lets the FIRST bot per (callout_kind, key) claim the slot for a
// configurable window; everyone else suppresses their emit. The key
// space is callouts identified by a stable 64-bit packed value (kind
// in high bits, target/node identifier in low bits) so two distinct
// nodes can each have an active callout but the same node can't be
// re-shouted by 40 bots.
//
// Threading: callout claims happen on the AI worker thread (one bot
// per worker per tick); the registry uses a shared_mutex for read-
// dominant lookups + an upgrade write when claiming/expiring entries.
//
// Cardinality: bounded by callout-types × distinct keys (~6 × ~16
// nodes-per-BG) = O(100) entries; trivial cost.
class BgCalloutCoordinator
{
public:
    // Returns true if THIS bot wins the claim for (kind, key) with the
    // given lockout window. False = another bot already shouted within
    // the window; suppress this emit.
    static bool TryClaim(uint32_t kind, uint64_t key, uint32_t now_ms,
                         uint32_t lockout_ms);
};

} // namespace Playerbot
