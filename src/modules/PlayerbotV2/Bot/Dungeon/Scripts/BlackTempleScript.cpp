// BlackTempleScript — Black Temple raid (map 564, TBC 25-man).
// 9 bosses: Naj'entus, Supremus, Shade of Akama, Teron Gorefiend,
// Gurtogg Bloodboil, Reliquary of Souls, Mother Shahraz, Illidari Council, Illidan.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Outland/BlackTemple/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class BlackTempleScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 564; }
    char const* name() const override { return "black_temple"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            22996,  // Blade of Azzinoth
            22997,  // Flame of Azzinoth (Illidan)
            23498,  // Parasitic Shadowfiend (NPC_PARASITIC_SHADOWFIEND, black_temple.h)
            22844,  // Ashtongue Battlelord (Shade of Akama adds)
            22845,  // Ashtongue Mystic (Shade of Akama adds)
            22846,  // Ashtongue Stormcaller (Shade of Akama adds)
            // Naj'entus' Impaling Spine is a GameObject (185584), not a creature.
        };
        a.mandatory_interrupt_spells = {
            // Naj'entus
            39835,  // Needle Spine
            39872,  // Tidal Shield
            39878,  // Tidal Burst
            // Supremus
            40126,  // Molten Punch
            42055,  // Volcanic Geyser
            41581,  // Charge
            // Teron Gorefiend
            40239,  // Incinerate
            40185,  // Shadowbolt
            // Bloodboil
            42005,  // Bloodboil
            40508,  // Fel Acid Breath
            40486,  // Eject
            // Reliquary of Souls
            41426,  // Spirit Shock
            41410,  // Deaden
            41545,  // Soul Scream
            41376,  // Spite
            41303,  // Soul Drain
            // Mother Shahraz
            40823,  // Silencing Shriek
            // Illidari Council
            41468,  // Hammer of Justice (Veras)
            41524,  // Arcane Explosion (Capernian-style)
            41481,  // Flamestrike
            41482,  // Blizzard
            41483,  // Arcane Bolt
            41478,  // Dampen Magic
            41471,  // Empowered Smite (Reliquary-like adviser)
            41475,  // Reflective Shield
            41541,  // Consecration
            41472,  // Divine Wrath
            41487,  // Envenom
            // Illidan
            40017,  // Eye Blast
            40685,  // Shadow Strike
            39869,  // Uncaged Wrath
            40631,  // Flame Blast
            41268,  // (Akama Door Channel — informational)
        };
        a.cc_priority_entries = {};
        a.dangerous_auras = {
            // Naj'entus
            39837,  // Impaling Spine
            39872,  // Tidal Shield
            39878,  // Tidal Burst
            // Supremus
            40980,  // Molten Flame
            40117,  // Volcanic Eruption
            40126,  // Molten Punch
            41922,  // Snare Self (during charge)
            // Teron Gorefiend
            40251,  // Shadow of Death
            40243,  // Crushing Shadows
            40239,  // Incinerate
            // Bloodboil
            42005,  // Bloodboil
            40594,  // Fel Rage Self
            40569,  // Fel Geyser
            40593,  // Fel Geyser 2
            40508,  // Fel Acid Breath
            40618,  // Insignifigance
            // Reliquary
            41292,  // Aura of Suffering
            41350,  // Aura of Desire
            41337,  // Aura of Anger
            41303,  // Soul Drain
            41410,  // Deaden
            41376,  // Spite
            41545,  // Soul Scream
            // Mother Shahraz
            41001,  // Fatal Attraction
            40859,  // Beam Sinister
            40860,  // Beam Vile
            40862, 40863, 40865, 40866, 40867,  // Sinful/Sinister/Vile/Wicked Periodics
            43690,  // Saber Lash Immunity
            // Council
            41475,  // Reflective Shield
            41541,  // Consecration
            41452,  // Devotion Aura
            41453,  // Chromatic Aura
            // Illidan
            41268,  // Demon Form / Akama channel
            40694,  // Cage Trap
            41917,  // Shadow Demon (parasitic)
        };
        // Progression waypoints — boss spawn positions from world.creature
        // (map 564), in encounter order. The Illidari Council quartet has
        // no static spawn rows (instance-script spawned), so that leg is
        // omitted; bots route Shahraz -> Illidan and pick the Council up
        // via the boss-Cell-scan fallback.
        a.progression_waypoints = {
            {  434.9f, 739.3f,  15.1f },   // High Warlord Naj'entus
            {  702.1f, 650.7f,  75.0f },   // Supremus
            {  449.6f, 401.2f, 118.6f },   // Shade of Akama
            {  606.6f, 402.2f, 187.2f },   // Teron Gorefiend
            {  744.3f, 277.1f,  63.8f },   // Gurtogg Bloodboil
            {  497.8f, 184.2f,  94.6f },   // Reliquary of Souls
            {  945.3f, 149.1f, 197.2f },   // Mother Shahraz
            {  705.7f, 305.0f, 353.9f },   // Illidan Stormrage
        };
        // Boss progression — NPC entries from TC's black_temple.h.
        // Illidari Council is a 4-mob encounter (Gathios/Zerevor/Malande/
        // Veras); listing all four. Mother Shahraz is missing from TC
        // header but standard entry is 22947.
        a.bosses = {
            22887,  // High Warlord Naj'entus
            22898,  // Supremus
            22841,  // Shade of Akama
            22871,  // Teron Gorefiend
            22948,  // Gurtogg Bloodboil
            22856,  // Reliquary of Souls
            22947,  // Mother Shahraz
            22949,  // Gathios the Shatterer (Illidari Council)
            22950,  // High Nethermancer Zerevor (Illidari Council)
            22951,  // Lady Malande (Illidari Council)
            22952,  // Veras Darkshadow (Illidari Council)
            22917,  // Illidan Stormrage (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeBlackTempleScript()
{
    return std::make_unique<BlackTempleScript>();
}

} // namespace Playerbot
