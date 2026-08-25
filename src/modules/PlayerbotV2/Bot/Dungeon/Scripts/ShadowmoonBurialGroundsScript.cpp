// ShadowmoonBurialGroundsScript — Shadowmoon Burial Grounds (map 1176,
// WoD 90-100). Shadowmoon Valley undead/void dungeon.
//   * Sadana Bloodfury — Shadow Bolt Volley + Crescent Strike.
//   * Nhallish — Possession + Void Devastation.
//   * Bonemaw — Body Slam (zone).
//   * Ner'zhul (final) — Ritual of Bones + Void Cleave.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ShadowmoonBurialGroundsScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1176; }
    char const* name() const override { return "shadowmoon_burial_grounds"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            75899,   // Possessed Soul (Nhallish — kill to reclaim body)
            76057,   // Carrion Worm (Bonemaw)
            75966,   // Defiled Spirit (Sadana adds)
        };
        a.mandatory_interrupt_spells = {
            153179,  // Shadow Bolt Volley (Sadana)
            152618,  // Possession (Nhallish)
            153555,  // Ritual of Bones (Ner'zhul)
            152724,  // Void Devastation (Nhallish)
            153373,  // Void Cleave (Ner'zhul)
        };
        a.cc_priority_entries = {
            75713,   // Shadowmoon Bone-Mender (healer trash)
            75506,   // Shadowmoon Loyalist (caster trash)
        };
        a.dangerous_auras = {
            153179,  // Shadow Bolt Volley zone
            152929,  // Body Slam (Bonemaw)
            153555,  // Ritual of Bones zone
            152724,  // Void Devastation pool
        };
        // Progression waypoints — Shadowmoon Burial Grounds is a
        // shadow priests' crypt: linear descent into ritual chambers.
        a.progression_waypoints = {
            {  -250.0f,  1335.0f,   58.0f },   // entry
            {  -210.0f,  1390.0f,   58.0f },   // Sadana chapel
            {  -150.0f,  1455.0f,   50.0f },   // Nhallish soul pit
            {   -77.0f,  1490.0f,   33.0f },   // Bonemaw pool
            {   -50.0f,  1570.0f,   33.0f },   // Ner'zhul ritual room
        };
        // Boss progression — entries verified against creature_template
        // (TC has no instance script for SMBG).
        a.bosses = {
            75509,  // Sadana Bloodfury
            75829,  // Nhallish
            75452,  // Bonemaw
            76407,  // Ner'zhul (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeShadowmoonBurialGroundsScript()
{
    return std::make_unique<ShadowmoonBurialGroundsScript>();
}

} // namespace Playerbot
