// DeadminesScript — per-dungeon override for The Deadmines (map 36).
// Vanilla low-level dungeon (15-25); minor mechanic notes. Generic
// dungeon-run logic clears most of it; this script's value is in:
//   * Mr. Smite phase 2 (the equipment-swap stun) — bot DPS waits
//     during the unstunnable phase rather than burning cooldowns
//     into invulnerability.
//   * Captain Greenskin's Defias Pirates — adds; high-priority kill.
//   * Edwin VanCleef's adds (Vancleef's Distraction line) —
//     mandatory interrupt when a healer is in range.
// Most boss interrupts are routine; we list a few specific ones the
// generic interrupt rule already handles well.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class DeadminesScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 36; }   // The Deadmines
    char const* name() const override { return "deadmines"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        // Captain Greenskin (entry 647) summons Defias Pirate (734)
        // adds — kill those first so they don't gangbang the healer.
        // Mr. Smite (entry 646) is a tank-and-spank with phase
        // transitions; no add priority.
        // 734 = Captain Greenskin's Defias Pirate adds. 48418 = Defias
        // Envoker — the ELITE MAGE (unit_class 8) that spams bolt 91004
        // in the Cata Helix->FoeReaper foundry gauntlet. That gauntlet
        // (verified from wc_world: ~11 Defias Miner + 2 Digger + 2 elite
        // Overseer + 3 elite Envoker, spread over -315..-260 / -610..-557
        // on the upper foundry) is what wiped the pure-bot group on the
        // FoeReaper approach (2026-06-26). The melee bulk is sustained but
        // survivable; the 3 elite Envokers' ranged burst is the spike that
        // drops squishies. Focus-fire the Envokers FIRST so the burst is
        // removed fastest. We intentionally do NOT list the elite Overseers
        // (48421) — they are just tankier melee the tank holds; focusing
        // them would let the mages nuke freely while the group grinds HP.
        // 734/48418 = foundry-gauntlet adds/Envoker. The trailing five are the
        // HARBOR-FLOOR Defias casters (FoeReaper->Ripsnarl): once the DPS
        // assist-engage there (harbor_focus_kill suppresses pre-emptive CC, see
        // State_Idle), the combat AI focus-fires these ranged sources FIRST so
        // the burst that kills the tank is removed fastest — the tank can no
        // longer be left to solo them (live 2026-06-28: tank death-ground while
        // the DPS only sheeped these). 48417 Blood Wizard (bolt + pack-heal) and
        // 1732/48521 Squallshapers (ranged bolt) are the priority; 48505
        // Shadowguard / 48502 Enforcer round out the harbor pack. 48522 is the
        // bulk melee Defias of the deck pack (~25k HP, 5+ at once, the body that
        // piles on the tank — observed live 2026-06-29 [harbor_atk] v=ME d1-4):
        // without it on the list the DPS SPREAD across the pack and no single mob
        // dies fast enough, so the tank eats 5 melee streams and death-loops on
        // bad rolls. Listing it concentrates fire = fewer attackers sooner = the
        // tank's incoming drops as each dies (the high-variance harbor over-pull).
        a.high_priority_kill_entries = { 734, 48418,
            48417, 1732, 48521, 48505, 48502, 48522 };
        // Interrupt the Envoker's bolt (91004) — its repeating nuke is the
        // burst; kicking/silencing it cuts incoming damage on the gauntlet.
        a.mandatory_interrupt_spells = { 91004 };
        // CC casters on trash. Two zones:
        //   * Foundry gauntlet (Helix->FoeReaper): Defias Magician (657) and
        //     the Cata Defias Envoker (48418) — sheep/freeze so only 1-2 bolts
        //     land at a time.
        //   * Harbor floor (FoeReaper->Admiral Ripsnarl): the REAL burst that
        //     kills the tank is dense type-7 HUMANOID Defias (workflow DB audit
        //     2026-06-27), NOT the Mechanical Reapers (47403/47404, type 9,
        //     CC-immune to Polymorph -> BAD_TARGETS, and 295y off-floor anyway).
        //     The spread-CC loop (State_Idle dungeon_auto_cc_script) distributes
        //     mage Polymorph / rogue Sap / hunter Freezing Trap across distinct
        //     casters, dropping 1-2 simultaneous ranged sources per sub-pull —
        //     the casters the tank cannot body-block. Highest value: the Blood
        //     Wizard 48417 (bolt 90938 + Bloodwash 90946 pack-HEAL that prolongs
        //     the fight) and the Squallshapers (1732/48521 ranged bolt). All
        //     listed entries are DB-confirmed type=7 faction-17, fully CC-able.
        //     Do NOT add 47403/47404 (Mechanical) or 48266 (Cannon, NOT_SELECT)
        //     — a type-9 deny-guard in the CC pick loop is a second line of
        //     defense, but keeping them off the list is the first.
        a.cc_priority_entries = {
            657, 48418,                 // foundry gauntlet (Magician, Envoker)
            1732, 48521,                // Defias Squallshaper (ranged bolt)
            48417,                      // Defias Blood Wizard (ranged + pack-heal)
            48502,                      // Defias Enforcer (charge opener)
            48505,                      // Defias Shadowguard (shadowstep rogue)
        };
        // Mr. Smite Slam (6432) leaves the area dazed; dangerous
        // aura to step from for casters/healers.
        a.dangerous_auras = { 6432 };
        // Boss progression — Cata Normal + Heroic boss entries (diff=1/2,
        // map 36). LFG bracket 7-30 lands on Normal difficulty (diff=1):
        // Glubtok -> Helix -> Foe Reaper 5000 -> Ripsnarl -> Cookie.
        // Vanessa (49541) is Heroic-only; included so Heroic runs also
        // use this script. Foe Reaper 5000 (43778) was initially missing
        // and Helix had the wrong entry (49674 vs 47296).
        a.bosses = {
            47162,  // Glubtok
            47296,  // Helix Gearbreaker
            43778,  // Foe Reaper 5000
            47626,  // Admiral Ripsnarl
            47739,  // "Captain" Cookie
            49541,  // Vanessa VanCleef (heroic)
        };
        // Environmental encounter objects — alive but unkillable; bots must
        // never target these or they spiral in a spell-interrupted loop that
        // blocks the tank advance for minutes. Populated from the live world
        // DB (wc_world.creature_template), name-grouped:
        //   Glubtok Firewall Platter: 48974-48976, 49039-49042
        //   Glubtok Nightmare Fire Bunny: 51594
        //   Mining Powder (inert mine prop): 48284, 48835
        //   Deadmines Foe Reaper Targeting Bunny: 47468
        //   Vanessa encounter props: 49454 (Trap Bunny), 49520 (Lightning Platter),
        //                            49521 (Lightning Stalker), 49552 (Rope Anchor),
        //                            51624 (Anchor Bunny)
        a.ignore_entries = {
            48974, 48975, 48976,        // Glubtok Firewall Platter Level 1a/2a/orig
            49039, 49040, 49041, 49042, // Glubtok Firewall Platter Level 1b/1c/2b/2c
            51594,                      // Glubtok Nightmare Fire Bunny
            48284, 48835,               // Mining Powder variants
            47468,                      // Deadmines Foe Reaper Targeting Bunny
            49454, 49520, 49521, 49552, 51624, // Vanessa encounter props
            // Vanessa VanCleef PASSIVE CUTSCENE DOUBLE (49671) — DB-confirmed:
            // faction 17 hostile, unit_flags 512 (IMMUNE_TO_NPC), type 7, AIName
            // SmartAI but ZERO smart_scripts (no abilities), HealthModifier 20
            // (BOSS-scaled HP). Three static spawns incl. one IN the Helix foundry
            // at (-230,-563,z51). The bots auto-acquire her as a nearby hostile
            // and melee-grind a boss-HP marker that never dies (observed live
            // 2026-06-27: tank+rogue+hunter all stuck on guid …118 for 15+ min,
            // 9K dmg / 0 progress, boss frozen at 1/6 — the run never reached the
            // harbor). She is NOT a boss entry (the real Vanessa boss is 49541);
            // ignoring her is safe for both Normal and Heroic. 49541's own static
            // spawn (-66,-877) is the heroic boss-nav target and is left targetable.
            49671,                      // Vanessa VanCleef passive cutscene double
            // Foundry-approach props/critters the bots stuck-chased onto OFF-MESH
            // prop perches near the Gap-1 bridge (observed live 2026-06-25: a DPS
            // and the tank set StuckChase on these, walked off the navmesh to a
            // FARFROMPOLY_START sliver at (-214,-525,z50.7), and stranded —
            // grouped bots are exempt from stuck-rescue, so they never recovered
            // and the group-ready gate stalled the Helix advance). Both are
            // non-combat: "Mine Bunny" is an invisible marker (faction 2102),
            // "Mining Monkey" is a wild critter (faction 190, no unit flags).
            48338,                      // Mine Bunny ("Refreshments" marker)
            48442,                      // Mining Monkey (critter)
        };
        // Progression waypoints — fallback navigator used when the boss-advance
        // scan finds no reachable boss (e.g. between encounters or when a boss
        // has not yet been spawned by the server script). Coordinates are actual
        // Cata Deadmines (map 36) boss/creature spawn positions from the world DB,
        // so they're guaranteed to be on the navmesh or very close to it.
        // Two offmesh bridges in offmesh.txt (map 36, tiles 32,32 + 32,33) connect
        // the mine navmesh → foundry navmesh → dock navmesh so the full path below
        // is reachable after those tiles are regenerated.
        a.progression_waypoints = {
            { -193.4f, -441.8f, 53.6f },   // Glubtok room (normal spawn)
            // Helix Gearbreaker: runtime-spawned riding Oafguard (47297) on the
            // UPPER foundry level at z~52, ~84y WEST of the Gap-1 bridge's far
            // ledge. WITHOUT this waypoint the fallback navigator jumped from
            // Glubtok straight to Foe Reaper's z21 FLOOR (next entry) — pulling
            // the tank SOUTH off the bridge route into the mine→foundry gap,
            // where Helix is navmesh-unreachable (reach=0) and the tank wedged
            // (observed live 2026-06-25 at (-214,-524,z50.7)). The z19/z21 floor
            // is a DEAD-END for Helix (floor→Helix hits the 74-poly path cap).
            // This waypoint keeps the tank on the upper level: it routes across
            // the Gap-1 bridge to the z51 ledge then WEST to Helix (mmap_probe:
            // a complete 66-poly path), and boss_nav engages within 25y first.
            { -302.4f, -516.3f, 52.0f },   // Helix Gearbreaker (Oafguard 47297 pos, upper foundry)
            { -209.8f, -568.6f, 21.1f },   // Foe Reaper 5000 foundry (normal spawn)
            {  -62.3f, -822.8f, 42.8f },   // Admiral Ripsnarl ship deck (normal spawn)
            {  -88.1f, -819.3f, 39.2f },   // "Captain" Cookie galley (normal spawn)
            {  -66.8f, -877.0f, 15.6f },   // Vanessa VanCleef (heroic spawn pos)
        };
        // Route waypoints come from the shared DB (playerbot_dungeon_routes),
        // injected by DungeonScriptMgr::GetAdvice — the DB is the SINGLE source
        // so the world editor can author/fix routes and hot-reload them without
        // touching code. The previously-authored harbor-descent chain
        // ((-135,-633) -> (-118,-690) -> (-107,-787), the three on-navmesh
        // stepping stones that chunk the FoeReaper->Ripsnarl ~293y leg into
        // <=74-poly hops so the group walks the descent instead of falling off
        // the z51->z14 ledge) now lives in that table; the generator's dense
        // chain covers the same corridor. See tools/gen_dungeon_routes.py.
        // Tight-engagement zone = the harbor floor (z<30). The gauntlet sits at
        // z57-62, so this cleanly scopes the tight-cohesion / focus-kill /
        // proactive-assist toolkit to the Ripsnarl approach. See
        // DungeonAdvice::tight_engage_below_z. This is what beat the harbor.
        a.tight_engage_below_z = 30.0f;
        return a;
    }
};

} // anonymous

// Registered into DungeonScriptMgr at module init via
// Services::Dungeons().Register(std::make_unique<DeadminesScript>())
// (see Services.cpp). The function symbol exists so the registry
// can pull it in without a static-init-order dependency.
std::unique_ptr<DungeonScript> MakeDeadminesScript()
{
    return std::make_unique<DeadminesScript>();
}

} // namespace Playerbot
