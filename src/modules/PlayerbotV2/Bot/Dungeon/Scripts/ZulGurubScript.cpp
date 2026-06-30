// ZulGurubScript — Zul'Gurub (map 859, Cata-rework 85).
// Originally vanilla raid; reworked to 5-man heroic. 4 bosses:
// High Priestess Kilnara, Bloodlord Mandokir, Jin'do the Godbreaker,
// Hakkar the Soulflayer.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/EasternKingdoms/ZulGurub/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ZulGurubScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 859; }
    char const* name() const override { return "zul_gurub"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            52456,  // Crazed Cat (Kilnara)
            52589,  // Spirit of Hakkar (Jin'do)
        };
        a.mandatory_interrupt_spells = {
            // Bloodlord Mandokir
            96717,  // Summon Ohgan
            96724,  // Reanimate Ohgan
            96682,  // Decapitate
            96776,  // Bloodletting
            96740,  // Devastating Slam
            96721,  // Ohgan Orders
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Mandokir
            96480,  // Bloodlord Aura
            96800,  // Frenzy
            96662,  // Level Up
            96777,  // Bloodletting Damage
            97385,  // Devastating Slam Damage
        };
        // Boss progression — Cata-rework ZG has 4 encounters
        // (the classic 40-man ZG had more, but the 5-man heroic uses these).
        a.bosses = {
            52059,  // High Priestess Kilnara
            52151,  // Bloodlord Mandokir
            52148,  // Jin'do the Godbreaker
            52053,  // Hakkar the Soulflayer (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeZulGurubScript()
{
    return std::make_unique<ZulGurubScript>();
}

} // namespace Playerbot
