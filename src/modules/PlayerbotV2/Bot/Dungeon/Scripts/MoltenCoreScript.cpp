// MoltenCoreScript — Molten Core raid (map 409, classic 40-man).
// 10 bosses: Lucifron, Magmadar, Gehennas, Garr, Shazzrah, Baron Geddon,
// Sulfuron Harbinger, Golemagg, Majordomo Executus, Ragnaros.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/EasternKingdoms/BlackrockMountain/MoltenCore/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class MoltenCoreScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 409; }
    char const* name() const override { return "molten_core"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            // 11988 was previously listed here as "Firesworn (Garr/Golemagg
            // adds)" — that's actually NPC_GOLEMAGG_THE_INCINERATOR (already
            // in bosses[] below). The real Firesworn add is entry 12099.
            12099,  // Firesworn (Garr) / Core Hound
            11663,  // Flamewaker Healer (Sulfuron heal adds)
            11664,  // Flamewaker Elite (caster adds)
            11671,  // Core Rager (Majordomo)
        };
        a.mandatory_interrupt_spells = {
            // Lucifron
            19702,  // Impending Doom
            19703,  // Lucifron's Curse
            20603,  // Shadow Shock
            // Magmadar
            19408,  // Panic
            19428,  // Lava Bomb
            19449,  // Magma Spit
            // Gehennas
            19716,  // Gehennas's Curse
            19717,  // Rain of Fire
            19728,  // Shadow Bolt
            // Garr
            19492,  // Antimagic Pulse
            19496,  // Magma Shackles
            // Shazzrah
            19712,  // Arcane Explosion
            19713,  // Shazzrah's Curse
            19714,  // Magic Grounding
            19715,  // Counterspell
            // Baron Geddon
            19695,  // Inferno
            19659,  // Ignite Mana
            20475,  // Living Bomb
            // Sulfuron
            19777,  // Dark Strike
            19778,  // Demoralizing Shout
            19779,  // Inspire
            19780,  // Knockdown
            19781,  // Flamespear
            19775,  // Heal (adds)
            19776,  // Shadow Word: Pain
            // Golemagg
            20228,  // Pyroblast
            19798,  // Earthquake
            // Majordomo
            20229,  // Blast Wave
            // Ragnaros
            19780,  // Hand of Ragnaros — collision with Sulfuron knockdown id;
                    // the SpellInfo distinguishes per-caster.
            20566,  // Wrath of Ragnaros
            21158,  // Lava Burst
            20565,  // Magma Blast
        };
        a.cc_priority_entries = {
            11663,  // Flamewaker Priest
            11664,  // Flamewaker Shadowcaster
        };
        a.dangerous_auras = {
            // Lucifron
            19703,  // Curse of Lucifron
            19702,  // Impending Doom
            // Magmadar
            19449,  // Magma Spit
            19451,  // Frenzy
            // Gehennas
            19716,  // Curse of Gehennas
            19717,  // Rain of Fire
            // Garr
            19497,  // Eruption
            19516,  // Enrage
            23492,  // Separation Anxiety
            // Shazzrah
            19713,  // Shazzrah's Curse
            // Geddon
            19695,  // Inferno
            19698,  // Inferno Damage
            20475,  // Living Bomb
            20478,  // Armageddon
            // Sulfuron
            19781,  // Flamespear
            // Golemagg
            19798,  // Earthquake
            13879,  // Magma Splash
            20553,  // Golemagg's Trust
            // Majordomo
            20620,  // Aegis of Ragnaros
            20619,  // Magic Reflection
            21075,  // Damage Reflection
            // Ragnaros
            20566,  // Wrath of Ragnaros (knockback + dot)
            21158,  // Lava Burst (ground hazard)
            20565,  // Magma Blast
        };
        // Boss progression — entries from TC's molten_core.h.
        a.bosses = {
            12118,  // Lucifron
            11982,  // Magmadar
            12259,  // Gehennas
            12057,  // Garr
            12264,  // Shazzrah
            12056,  // Baron Geddon
            12098,  // Sulfuron Harbinger
            11988,  // Golemagg the Incinerator
            12018,  // Majordomo Executus
            11502,  // Ragnaros (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeMoltenCoreScript()
{
    return std::make_unique<MoltenCoreScript>();
}

} // namespace Playerbot
