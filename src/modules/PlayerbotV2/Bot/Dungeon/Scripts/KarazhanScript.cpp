// KarazhanScript — Karazhan raid (map 532, TBC 10-man).
// 10+ bosses: Attumen, Moroes, Maiden of Virtue, Big Bad Wolf / Romulo & Julianne /
// Wizard of Oz / Opera House, Curator, Shade of Aran, Terestian Illhoof,
// Netherspite, Prince Malchezaar, Nightbane (optional).
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/EasternKingdoms/Karazhan/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class KarazhanScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 532; }
    char const* name() const override { return "karazhan"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            17096,  // Astral Flare (Curator)
            17650,  // Prince Malchezaar's Axes
            17646,  // Netherspite Infernal
            17229,  // Kil'rek (Illhoof's imp)
            17267,  // Fiendish Imp (Illhoof)
            17248,  // Demon Chains (Illhoof — free the Sacrifice target)
            // Moroes' dinner guests (Adds[6] in TC boss_moroes.cpp):
            17007,  // Lady Keira Berrybuck
            19872,  // Lady Catriona Von'Indi
            19873,  // Lord Crispin Ference
            19874,  // Baron Rafe Dreuger
            19875,  // Baroness Dorothea Millstipe
            19876,  // Lord Robin Daris
        };
        a.mandatory_interrupt_spells = {
            // Maiden of Virtue
            29522,  // Holy Fire
            32445,  // Holy Wrath
            29511,  // Repentance
            // Curator
            30383,  // Hateful Bolt
            30254,  // Evocation
            // Moroes
            37066,  // Garrote
            34694,  // Blind
            29570,  // Mind Flay
            34441,  // Shadow Word: Pain
            29562,  // Holy Light
            29563,  // Holy Fire
            29564,  // Greater Heal
            29572,  // Mortal Strike
            // Shade of Aran
            29954,  // Frostbolt
            29953,  // Fireball
            29955,  // Arcane Missiles
            29964,  // Dragon's Breath
            30035,  // Mass Slow
            29963,  // Mass Polymorph
            29978,  // AoE Pyroblast
            29973,  // Arcane Explosion
            29951,  // Circular Blizzard
            // Terestian Illhoof
            30055,  // Shadow Bolt
            30115,  // Sacrifice
            // Prince Malchezaar
            30843,  // Enfeeble
            30852,  // Shadow Nova
            30854,  // Shadow Word: Pain
            // Nightbane
            36922,  // Bellowing Roar
            30210,  // Smoldering Breath
            30131,  // Cleave
            // Netherspite
            38523,  // Netherbreath
        };
        a.cc_priority_entries = {};
        a.dangerous_auras = {
            // Maiden
            29523,  // Holy Ground (avoid puddle)
            // Curator
            30403,  // Arcane Infusion (boss enrage)
            // Aran
            29946,  // Flame Wreath
            29991,  // Chains of Ice
            // Illhoof
            30206,  // Demon Chains (debuff while sacrificed)
            30053,  // Amplify Flames
            // Netherspite
            30522,  // Netherburn Aura
            38688,  // Nether Infusion (enrage)
            37063,  // Void Zone
            // Malchezaar
            39095,  // Amplify Damage
            30859,  // Hellfire (Infernal aura)
            30901,  // Sunder Armor
            // Nightbane
            30129,  // Charred Earth
            30130,  // Distracting Ash
            37098,  // Rain of Bones
        };
        // Progression waypoints — boss spawn positions from world.creature
        // (map 532), in encounter order. Attumen has no static spawn row
        // (spawned by the Midnight stable event), so his waypoint is
        // omitted; bots start at Moroes and pick Attumen up via the
        // boss-Cell-scan fallback.
        a.progression_waypoints = {
            { -10982.7f, -1877.9f,  81.8f },   // Moroes
            { -10945.9f, -2103.8f,  92.8f },   // Maiden of Virtue
            { -11187.9f, -1883.4f, 156.0f },   // The Curator
            { -11165.5f, -1911.7f, 232.0f },   // Shade of Aran
            { -11134.0f, -1582.8f, 278.8f },   // Netherspite
            { -11003.7f, -1760.2f, 140.3f },   // Nightbane
            { -11240.6f, -1704.3f, 179.3f },   // Terestian Illhoof
            { -10962.2f, -2018.7f, 275.4f },   // Prince Malchezaar
        };
        // Boss progression — entries from karazhan.h + WoWHead for the
        // ones TC's header omits (Maiden/Curator/Aran/Netherspite/Malch).
        a.bosses = {
            16152,  // Attumen the Huntsman
            15687,  // Moroes
            16457,  // Maiden of Virtue
            15691,  // The Curator
            16524,  // Shade of Aran
            15689,  // Netherspite
            17225,  // Nightbane (optional)
            15688,  // Terestian Illhoof (optional)
            15690,  // Prince Malchezaar (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeKarazhanScript()
{
    return std::make_unique<KarazhanScript>();
}

} // namespace Playerbot
