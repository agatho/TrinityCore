// UnderrotScript — The Underrot (map 1841, BfA 110-120).
// 4 bosses: Elder Leaxa, Cragmaw the Infested, Sporecaller Zancha,
// Unbound Abomination.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Zandalar/Underrot/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class UnderrotScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1841; }
    char const* name() const override { return "underrot"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            134920,  // Bloodsworn Defiler (Leaxa adds)
            136591,  // Withered Spore (Zancha)
        };
        a.mandatory_interrupt_spells = {
            // Cragmaw
            260292,  // Charge Selector
            260793,  // Indigestion
            260333,  // Tantrum Initial
            260418,  // Destroy Larva
            260353,  // Summon Blood Tick
            // Sporecaller Zancha
            259537,  // Soul Anchor
            250192,  // Bad Voodoo
            250241,  // Rapid Decay
            250258,  // Toxic Leap
            // Elder Leaxa
            260455,  // Serrated Fangs
            // Unbound Abomination
            260685,  // Taint of G'huun
            260879,  // Blood Bolt
            264747,  // Sanguine Feast
            260889,  // Creeping Rot Selector
        };
        a.cc_priority_entries = {
            134920,
        };
        a.dangerous_auras = {
            // Zancha
            250585,  // Toxic Pool
            259572,  // Noxious Stench
            250372,  // Lingering Nausea
            250259,  // Toxic Leap Damage
            // Cragmaw
            278637,  // Blood Burst Damage
            271771,  // Power Display Tantrum
        };
        // Boss progression — entries from TC's underrot.h.
        // Progression waypoints — Underrot is a Nazmir blood cavern.
        a.progression_waypoints = {
            { 1015.0f, 1190.0f,  35.0f },   // entry
            { 1083.0f, 1188.0f,  35.0f },   // Elder Leaxa cavern
            { 1126.0f, 1166.0f,  17.0f },   // Cragmaw pit
            { 1183.0f, 1153.0f,  17.0f },   // Zancha mushroom hall
            { 1224.0f, 1107.0f,  35.0f },   // Unbound Abomination ritual
        };
        a.bosses = {
            131318,  // Elder Leaxa
            131383,  // Sporecaller Zancha
            131817,  // Cragmaw the Infested
            133007,  // Unbound Abomination (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeUnderrotScript()
{
    return std::make_unique<UnderrotScript>();
}

} // namespace Playerbot
