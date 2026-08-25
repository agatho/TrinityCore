// AtalDazarScript — Atal'Dazar (map 1763, BfA 110-120).
// 4 bosses: Priestess Alun'za, Vol'kaal, Rezan, Yazma.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Zandalar/AtalDazar/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class AtalDazarScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1763; }
    char const* name() const override { return "atal_dazar"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
        };
        a.mandatory_interrupt_spells = {
            // Priestess Alun'za
            258386,  // Ritual
            255615,  // Agitate
            255583,  // Molten Gold Missile
            255577,  // Transfusion
            258703,  // Corrupted Gold
            // Rezan
            255434,  // Serrated Teeth
            255421,  // Devour
            255371,  // Terrifying Visage
            257407,  // Pursuit
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Alun'za
            255579,  // Gilded Claws
            255836,  // Transfusion Damage
            255558,  // Tainted Blood Damage
            255559,  // Tainted Blood AreaTrigger
            259205,  // Spirit of Gold
            258709,  // Corrupted Gold Damage
            259032,  // Corrupt
            259123,  // Fatally Corrupted
            // Rezan
            255373,  // Tail Damage
            256608,  // Pile of Bones Spawn
            256606,  // Pile of Bones Slow
        };
        // Boss progression — entries from TC's atal_dazar.h.
        // Progression waypoints — Atal'Dazar is a Zandalari pyramid
        // with a central plaza and 3 stairs.
        a.progression_waypoints = {
            {  890.0f, 1404.0f, 36.0f },   // entry
            { 1029.0f, 1361.0f, 71.0f },   // Alun'za blood pool
            { 1191.0f, 1330.0f, 73.0f },   // Vol'kaal arena
            { 1234.0f, 1410.0f, 96.0f },   // Rezan platform
            { 1110.0f, 1483.0f, 184.0f },  // Yazma top
        };
        a.bosses = {
            122967,  // Priestess Alun'za
            122965,  // Vol'kaal
            122963,  // Rezan
            122968,  // Yazma (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeAtalDazarScript()
{
    return std::make_unique<AtalDazarScript>();
}

} // namespace Playerbot
