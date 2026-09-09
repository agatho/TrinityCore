// MagistersTerraceScript — Magisters' Terrace (map 585, TBC 70).
// Sunwell-tier 5-man. 4 bosses: Selin Fireheart, Vexallus, Priestess
// Delrissa, Kael'thas Sunstrider.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/EasternKingdoms/MagistersTerrace/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class MagistersTerraceScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 585; }
    char const* name() const override { return "magisters_terrace"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            24745,  // Pure Energy (Vexallus)
            24674,  // Phoenix (Kael'thas)
        };
        a.mandatory_interrupt_spells = {
            // Kael'thas
            44189,  // Fireball
            46162,  // Flame Strike
            46165,  // Shock Barrier
            36819,  // Pyroblast
            44194,  // Phoenix summon
            // Delrissa adds (priest/shaman/rogue/lock variants)
            27609,  // Dispel Magic
            17843,  // Flash Heal
            14032,  // Shadow Word: Pain
            44291,  // Shield
            44174,  // Renew
            27621,  // Windfury Totem
            46026,  // War Stomp
            27626,  // Purge
            44256,  // Lesser Healing Wave
            21401,  // Frost Shock
            44257,  // Fire Nova Totem
            15786,  // Earthbind Totem
            27615,  // Kidney Shot
            12540,  // Gouge
            27613,  // Kick
            44290,  // Vanish
            27611,  // Eviscerate
            44267,  // Immolate
            12471,  // Shadow Bolt
        };
        a.cc_priority_entries = {
            24557,
            24558,
        };
        a.dangerous_auras = {
            // Kael'thas Gravity Lapse phase
            44224,  // Gravity Lapse Initial
            44227,  // Gravity Lapse Fly
            44265,  // Summon Arcane Sphere
            44190,  // Flame Strike Damage
            44197,  // Phoenix Burn
            44199,  // Ember Blast
        };
        // Boss progression — entries from TC boss .cpp files.
        // Priestess Delrissa fight is a 5-mob encounter; listing Delrissa
        // alone since the encounter resolves when she dies.
        a.bosses = {
            24723,  // Selin Fireheart
            24744,  // Vexallus
            24560,  // Priestess Delrissa
            24664,  // Kael'thas Sunstrider (final)
        };
        // Progression waypoints — MgT is a 4-room palace: entry
        // → Selin's crystal hall → Vexallus arena → Delrissa atrium
        // → Kael'thas throne (Sunwell prequel boss).
        a.progression_waypoints = {
            {  130.0f, -188.0f,    0.6f },   // entry
            {  178.0f, -297.0f,    2.0f },   // Selin's crystal hall
            {  246.0f, -332.0f,    0.6f },   // Vexallus arena
            {  159.0f, -432.0f,   -7.7f },   // Delrissa atrium
            {  225.6f, -546.0f,   17.5f },   // Kael'thas throne
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeMagistersTerraceScript()
{
    return std::make_unique<MagistersTerraceScript>();
}

} // namespace Playerbot
