// RazorfenKraulScript — Razorfen Kraul (map 47, vanilla 25-35).
// Quilboar / pig-people dungeon. Notable:
//   * Charlga Razorflank's Earthbind Totem + Curse of Blood — totem
//     priority-kill, debuff dispel.
//   * Aggem Thorncurse summons Razorfen pests; light add priority.
//   * Death Speaker Jargba's Bone Shield + Power Word: Shield —
//     interruptible cast.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class RazorfenKraulScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 47; }
    char const* name() const override { return "razorfen_kraul"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            7437,   // Earthbind Totem (Charlga)
        };
        a.mandatory_interrupt_spells = {
            12544,  // Frost Armor (Razorfen Defenders)
            7657,   // Bone Shield (Death Speaker)
        };
        // Boss progression — aligned to the LIVE instance_razorfen_kraul
        // encounter list (RFKBossIds order). The pre-revamp entries this list
        // used to carry (Aggem Thorncurse 4424, Death Speaker Jargba 4428) are
        // NOT tracked encounters in the modern RFK script and 4424 has no static
        // spawn at all, so the tank-advance rule was steering to a creature that
        // never exists. All five below are verified map-47 spawns and match the
        // InstanceScript's SetBossState credit creatures.
        a.bosses = {
            75001,  // Hunter Bonetusk
            6168,   // Roogug
            74462,  // Warlord Ramtusk
            75247,  // Groyat, the Blind Hunter
            4421,   // Charlga Razorflank (final)
        };
        // Progression waypoints — Razorfen Kraul is a Barrens quilboar
        // open-air dungeon with linear path.
        a.progression_waypoints = {
            { 1942.0f, 1546.0f,  77.0f },   // entry
            { 2000.0f, 1610.0f,  82.0f },   // Roogug courtyard
            { 2058.0f, 1638.0f,  92.0f },   // Aggem altar
            { 2095.0f, 1740.0f,  92.0f },   // Jargba ritual
            { 2050.0f, 1812.0f,  93.0f },   // Charlga throne
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeRazorfenKraulScript()
{
    return std::make_unique<RazorfenKraulScript>();
}

} // namespace Playerbot
