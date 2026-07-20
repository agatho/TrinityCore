// AuchenaiCryptsScript — Auchenai Crypts (map 558, TBC 64-70).
// Auchindoun wing. 2 bosses: Shirrak the Dead Watcher, Exarch Maladaar.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Outland/Auchindoun/AuchenaiCrypts/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class AuchenaiCryptsScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 558; }
    char const* name() const override { return "auchenai_crypts"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            18371,  // Avatar of the Martyred (Maladaar summons)
        };
        a.mandatory_interrupt_spells = {
            // Exarch Maladaar
            32421,  // Soul Scream
            32422,  // Ribbon of Souls
            32346,  // Stolen Soul
            // Stolen Soul avatar — various class casts
            37328,  // Moonfire (Avatar)
            37329,  // Fireball
            37330,  // Mind Flay
            37331,  // Hemorrhage
            37332,  // Frost Shock
            37334,  // Curse of Agony
            37335,  // Mortal Strike
            37368,  // Freezing Trap
            37369,  // Hammer of Justice
            58839,  // Plague Strike
        };
        a.cc_priority_entries = {
            18406,  // Auchenai Soulpriest
        };
        a.dangerous_auras = {
            // Shirrak
            32264,  // Inhibit Magic (silence aura)
            32265,  // Attract Magic
            32302,  // Fiery Blast zone
            // Maladaar
            32395,  // Stolen Soul Visual
        };
        // Boss progression — Auchenai Crypts has 2 bosses.
        a.bosses = {
            18371,  // Shirrak the Dead Watcher
            18373,  // Exarch Maladaar (final)
        };
        // Progression waypoints — Auchenai Crypts is the northern
        // Auchindoun wing: a 2-room linear path — entry corridor →
        // Shirrak's antechamber → Maladaar's sanctum.
        a.progression_waypoints = {
            {  31.6f,   13.0f, -19.3f },   // entry
            {  37.5f,  -71.0f, -22.8f },   // Shirrak antechamber
            {  35.0f, -150.0f, -23.3f },   // Maladaar sanctum
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeAuchenaiCryptsScript()
{
    return std::make_unique<AuchenaiCryptsScript>();
}

} // namespace Playerbot
