// SunwellPlateauScript — Sunwell Plateau raid (map 580, TBC 25-man).
// 6 bosses: Kalecgos, Brutallus, Felmyst, Eredar Twins (Sacrolash + Alythess),
// M'uru, Kil'jaeden.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/EasternKingdoms/SunwellPlateau/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class SunwellPlateauScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 580; }
    char const* name() const override { return "sunwell_plateau"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            24891,  // Sathrovarr the Corruptor (Kalecgos spectral realm)
            25744,  // Shadowsword Berserker (M'uru)
            25798,  // Void Sentinel (M'uru)
            25801,  // Sinister Reflection (KJ)
            25840,  // Dark Fiend (M'uru)
        };
        a.mandatory_interrupt_spells = {
            // Kalecgos
            44869,  // Spectral Blast
            45018,  // Arcane Buffet
            44799,  // Frost Breath
            45122,  // Tail Lash
            45029,  // Corruption Strike (demon)
            45031,  // Shadow Bolt
            45026,  // Heroic Strike
            45027,  // Revitalize
            // Brutallus
            45150,  // Meteor Slash
            45185,  // Stomp
            // Felmyst
            45866,  // Corrosion
            45855,  // Gas Nova
            19983,  // Cleave
            // Twins (Sacrolash)
            45248,  // Shadow Blades
            45271,  // Dark Strike
            45329,  // Shadow Nova
            45256,  // Confounding Blow
            45270,  // Shadow Fury
            // Twins (Alythess)
            45230,  // Pyrogenics
            45342,  // Conflagration
            46771,  // Flame Sear
            // M'uru
            45998,  // Darkness Periodic
            46009,  // Negative Energy Periodic
            // Kil'jaeden
            45442,  // Soul Flay
            45664,  // Legion Lightning
            45641,  // Fire Bloom
            45657,  // Darkness of a Thousand Souls
            45737,  // Flame Dart
        };
        a.cc_priority_entries = {};
        a.dangerous_auras = {
            // Kalecgos
            45018,  // Arcane Buffet (stacks)
            45032,  // Agony Curse
            44799,  // Frost Breath
            44869,  // Spectral Blast (cast triggers realm swap)
            // Brutallus
            46394,  // Burn
            45150,  // Meteor Slash
            45185,  // Stomp
            // Felmyst
            45866,  // Corrosion
            45855,  // Gas Nova
            45495,  // Fog Breath
            45717,  // Fog Charm
            45402,  // Trail Damage
            46931,  // Vapor Damage
            // Twins
            45347,  // Dark Touched
            45348,  // Flame Touched
            45345,  // Dark Flame (both touched stacks → DoT)
            45366,  // Empower
            45235,  // Blaze
            // M'uru
            45998,  // Darkness
            46009,  // Negative Energy
            46177,  // Open All Portals (phase 2)
            46269,  // Darkness Entropius
            // Kil'jaeden
            45785,  // Sinister Reflection (mirror image)
            46605,  // Darkness of a Thousand Souls (channel)
        };
        // Boss progression — NPC entries from TC's sunwell_plateau.h.
        // Kalecgos phase 1 (dragon) is 24850; Eredar Twins are
        // Alythess + Sacrolash listed separately so either resolves
        // the encounter for tank-advance.
        a.bosses = {
            24850,  // Kalecgos (dragon phase)
            24882,  // Brutallus
            25038,  // Felmyst
            25166,  // Grand Warlock Alythess (Eredar Twins)
            25165,  // Lady Sacrolash (Eredar Twins)
            25741,  // M'uru
            25315,  // Kil'jaeden (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeSunwellPlateauScript()
{
    return std::make_unique<SunwellPlateauScript>();
}

} // namespace Playerbot
