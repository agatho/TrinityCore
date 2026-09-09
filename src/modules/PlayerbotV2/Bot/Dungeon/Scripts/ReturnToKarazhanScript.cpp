// ReturnToKarazhanScript — Return to Karazhan (map 1651, Legion 110).
// Mega-dungeon. Lower: Maiden of Virtue, Opera Hall, Attumen, Moroes.
// Upper: The Curator, Shade of Medivh, Mana Devourer, Viz'aduum.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/EasternKingdoms/ReturnToKarazhan/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ReturnToKarazhanScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1651; }
    char const* name() const override { return "return_to_karazhan"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            114321,  // Lord Crispin Ference (Moroes dinner guest)
            114317,  // Lady Catriona Von'Indi (Moroes dinner guest)
            114318,  // Baron Rafe Dreuger (Moroes dinner guest)
            114320,  // Lord Robin Daris (Moroes dinner guest)
            114247,  // Burning Imp (Viz'aduum)
        };
        a.mandatory_interrupt_spells = {
            // Maiden of Virtue
            227809,  // Holy Bolt
            227800,  // Holy Shock
            227789,  // Sacred Ground
            227508,  // Mass Repentance
            227817,  // Holy Bulwark
            227823,  // Holy Wrath
            // Curator
            227466,  // Absorb Loose Mana
            227618,  // Arcane Bomb
            227297,  // Coalesce Power
            227507,  // Decimating Essence
            227457,  // Energy Discharge
            227523,  // Energy Void
            228577,  // Engulfing Power
            227502,  // Unstable Mana
            // Shade of Medivh
            227599,  // Basic Primer
            227615,  // Inferno Bolt
            227628,  // Piercing Missiles
            227592,  // Frostbite
            228237,  // Signature Primer
            228269,  // Flame Wreath Selector
            227779,  // Ceaseless Winter
        };
        a.cc_priority_entries = {
            114318,  // Baron Rafe Dreuger
            114320,  // Lord Robin Daris
            114247,
        };
        a.dangerous_auras = {
            // Maiden
            227848,  // Sacred Ground Damage
            227793,  // Sacred Ground Periodic
            // Curator
            227524,  // Energy Void Damage
            227528,  // Energy Void Drain Power
            // Shade of Medivh
            228257,  // Flame Wreath AreaTrigger
            228262,  // Flame Wreath Area Damage
            228261,  // Flame Wreath Periodic
            227806,  // Ceaseless Winter Damage
            228222,  // Ceaseless Winter Periodic
            228334,  // Guardian's Image
        };
        // Boss progression — Return to Karazhan has 9 encounters (TC's
        // EncounterCount=9, see return_to_karazhan.h). Verified entries:
        //   BOSS_MAIDEN_OF_VIRTUE_RTK = 113971
        //   BOSS_THE_CURATOR_RTK      = 114247
        //   BOSS_MANA_DEVOURER        = 114252
        // Other entries from public WoWHead dumps.
        a.bosses = {
            // Lower
            113971,  // Maiden of Virtue
            114261,  // Opera Hall: Toe Knee (Westfall Story, one of 3 randomized acts)
            114262,  // Attumen the Huntsman
            114312,  // Moroes
            // Upper
            114247,  // The Curator
            114350,  // Shade of Medivh
            114252,  // Mana Devourer
            114790,  // Viz'aduum the Watcher (final)
            // Nightbane (9th encounter, optional 2nd-floor flyer): TC's
            // RTKDataTypes::DATA_NIGHTBANE exists but no BOSS_NIGHTBANE
            // creature_id is exposed in the header. Omit until verified
            // against runtime — the validator will report progress=8/9
            // and the operator can plug the correct ID.
        };
        // Progression waypoints — boss spawn positions from world.creature
        // (map 1651), in encounter order. Opera Hall (114261) and Attumen
        // (114262) are script-spawned with no creature rows and are
        // skipped; the pathfinder routes the corridors between points.
        a.progression_waypoints = {
            { -10945.9f, -2103.5f,   92.8f },  // Maiden of Virtue
            { -10983.0f, -1880.8f,   81.8f },  // Moroes
            { -11085.2f, -1842.2f,  165.8f },  // The Curator
            {  -4599.1f, -2524.6f, 2876.6f },  // Shade of Medivh
            {  -4358.4f, -2628.6f,  153.4f },  // Mana Devourer
            {   3657.5f, -2125.8f,  815.7f },  // Viz'aduum the Watcher
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeReturnToKarazhanScript()
{
    return std::make_unique<ReturnToKarazhanScript>();
}

} // namespace Playerbot
