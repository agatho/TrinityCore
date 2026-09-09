// BlackrockCavernsScript — Blackrock Caverns (map 645, Cata 80-85).
// 5 bosses: Rom'ogg Bonecrusher, Corla Herald of Twilight, Karsh
// Steelbender, Beauty, Ascendant Lord Obsidius.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/EasternKingdoms/BlackrockMountain/BlackrockCaverns/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class BlackrockCavernsScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 645; }
    char const* name() const override { return "blackrock_caverns"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            39732,  // Twilight Acolyte (Corla beam)
            41227,  // Shadow of Obsidius
        };
        a.mandatory_interrupt_spells = {
            // Beauty (pet boss)
            76028,  // Terrifying Roar
            76030,  // Berserker Charge
            76031,  // Magma Spit
            76032,  // Flamebreak
            // Corla
            75610,  // Evolution
            75645,  // Drain Essence
            // Rom'ogg
            82137,  // Call for Help
            75571,  // Wounding Strike
            75543,  // Skullcracker
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Rom'ogg
            75539,  // Chains of Woe (pull)
            75272,  // Quake
            // Karsh
            75842,  // Quicksilver Armor
            75846,  // Superheated Quicksilver Armor
        };
        // Boss progression — Blackrock Caverns has 5 encounters.
        a.bosses = {
            39665,  // Rom'ogg Bonecrusher
            39679,  // Corla, Herald of Twilight
            39698,  // Karsh Steelbender
            39700,  // Beauty
            39705,  // Ascendant Lord Obsidius (final)
        };
        // Progression waypoints — BRC is a linear Twilight cult dungeon.
        a.progression_waypoints = {
            {  255.0f,  984.0f,  73.0f },   // entry
            {  280.0f, 1090.0f,  73.0f },   // Rom'ogg arena
            {  185.0f, 1245.0f,  35.5f },   // Corla zealot ramp
            {   80.0f, 1267.0f,  20.0f },   // Karsh forge
            {  146.0f, 1080.0f,  39.5f },   // Beauty's den
            {  213.0f,  974.0f,  73.6f },   // Obsidius cavern
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeBlackrockCavernsScript()
{
    return std::make_unique<BlackrockCavernsScript>();
}

} // namespace Playerbot
