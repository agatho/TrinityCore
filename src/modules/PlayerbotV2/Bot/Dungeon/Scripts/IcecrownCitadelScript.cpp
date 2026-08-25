// IcecrownCitadelScript — Icecrown Citadel raid (map 631, WotLK 10/25).
// Final WotLK raid. 12 bosses across multiple wings:
//   Lower Spire: Lord Marrowgar, Lady Deathwhisper, Gunship Battle,
//                Deathbringer Saurfang
//   Plagueworks: Festergut, Rotface, Professor Putricide
//   Crimson Halls: Blood Prince Council, Blood-Queen Lana'thel
//   Frostwing Halls: Valithria Dreamwalker, Sindragosa
//   Final: The Lich King
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/IcecrownCitadel/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class IcecrownCitadelScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 631; }
    char const* name() const override { return "icecrown_citadel"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            37890,  // Cult Fanatic (Deathwhisper)
            37949,  // Cult Adherent (Deathwhisper)
            36619,  // Bone Spike (Marrowgar — frees impaled player)
            38508,  // Blood Beast (Saurfang)
            36701,  // Raging Spirit (Lich King)
            36609,  // Val'kyr Shadowguard (Lich King)
            37695,  // Drudge Ghoul (Lich King)
            37698,  // Shambling Horror (Lich King)
            36941,  // Risen Witch Doctor (Lich King)
        };
        a.mandatory_interrupt_spells = {
            // Lord Marrowgar
            69055, 69076, 69057,
            // Lady Deathwhisper
            71254, 71001, 71289, 71420, 72905, 71363, 72478,
            // Cult Adherents
            70670, 70674, 70901, 70594, 70906, 71237,
            // Deathbringer Saurfang
            72380,  // Blood Nova
            72385,  // Boiling Blood
            72410,  // Rune of Blood
            72172,  // Summon Blood Beast
            72737,  // Frenzy
            // Festergut
            69165,  // Inhale Blight
            69195,  // Pungent Blight
            69240,  // Vile Gas
            72219,  // Gastric Bloat
            69278,  // Gas Spore
            // Rotface
            69508,  // Slime Spray
            69674,  // Mutated Infection
            // Professor Putricide
            70341,  // Slime Puddle
            70351,  // Unstable Experiment
            70852,  // Malleable Goo
            71255,  // Choking Gas Bomb
            // Blood Prince Council
            71405,  // Shadow Lance (Keleseth)
            71815,  // Empowered Shadow Lance
            71806,  // Glittering Sparks (Taldaram)
            71718,  // Conjure Flame
            71945,  // Shock Vortex Periodic (Valanar)
            72080,  // Kinetic Bomb
            // Blood-Queen Lana'thel
            71726,  // Vampiric Bite
            71623,  // Delirious Slash
            71446,  // Twilight Bloodbolt
            71264,  // Swarming Shadows
            71340,  // Pact of the Darkfallen
            73070,  // Incite Terror
            // Valithria-room adds
            70759,  // Frostbolt Volley (Risen Archmage)
            70754,  // Fireball
            70588,  // Suppression
            70744,  // Acid Burst (Gluttonous Abomination)
            70633,  // Gut Spray
            69325,  // Ley Waste
            71179,  // Mana Void
            // Sindragosa
            69649,  // Frost Breath P1
            73061,  // Frost Breath P2
            69762,  // Unchained Magic
            70123,  // Blistering Cold
            71077,  // Tail Smash
            71376,  // Icy Blast (Rimefang)
            71337,  // Concussive Shock (Spinestalker)
            // The Lich King
            72133,  // Pain and Suffering
            70541,  // Infest
            70337,  // Necrotic Plague
            69242,  // Soul Shriek (Raging Spirit)
            72149,  // Shockwave
            69409,  // Soul Reaper
        };
        a.cc_priority_entries = {
            37890,  // Cult Fanatic
            37949,  // Cult Adherent
        };
        a.dangerous_auras = {
            // Marrowgar
            69140,  // Coldflame Normal
            72705,  // Coldflame Bone Storm
            69065,  // Impaled (trapped by spike)
            // Lady Deathwhisper
            70842,  // Mana Barrier (boss invuln phase)
            71204,  // Touch of Insignificance
            70895, 70896, 70897, 70900,  // Dark Transformation/Empowerment/Martyrdom
            70659,  // Necrotic Strike
            67767,  // Frost Fever
            // Saurfang
            72293,  // Mark of the Fallen Champion
            // Festergut
            69291,  // Inoculated
            71805,  // Plague Stench
            71127,  // Mortal Wound
            71123,  // Decimate
            // Rotface
            69750,  // Weak Radiating Ooze
            69760,  // Radiating Ooze (large puddle)
            69774,  // Sticky Ooze
            69839,  // Unstable Ooze Explosion
            // Putricide
            70672,  // Gaseous Bloat (ranged tank debuff)
            70447,  // Volatile Ooze Adhesive
            70911,  // Unbound Plague
            72451,  // Mutated Plague
            70492,  // Ooze Eruption
            69157, 69162, 69164,  // Gaseous Blight (room atmosphere)
            // Blood Prince Council
            72999,  // Shadow Prison Damage
            71393,  // Flames (ball of flames area)
            // Blood-Queen Lana'thel
            70986,  // Shroud of Sorrow
            70877,  // Frenzied Bloodthirst
            70994,  // Presence of the Darkfallen
            72132,  // Gushing Wound
            // Valithria room
            70873,  // Emerald Vigor (heal Valithria!)
            71970,  // Nightmare Cloud
            70702,  // Column of Frost Damage
            // Sindragosa
            70084,  // Frost Aura
            70109,  // Permaeating Chill
            70126,  // Frost Beacon
            69675,  // Ice Tomb
            70128,  // Mystic Buffet (stacking debuff — break LOS)
            70157,  // Ice Tomb Damage
            71665,  // Asphyxiation
            71370,  // Tail Sweep
            71386,  // Frost Breath (P2)
            71387,  // Frost Aura (Rimefang)
            // The Lich King
            68981,  // Remorseless Winter (P1.5)
            72259,  // Remorseless Winter (P2.5)
            73525,  // Shadow Trap Aura
            69091,  // Ice Pulse (Ice Sphere)
            69108,  // Ice Burst
            72743,  // Defile (growing void)
            72262,  // Quake
            70498,  // Vile Spirits
            72305,  // Soul Barrage
            71614,  // Ice Lock
        };
        // Progression waypoints — derived from world.creature spawn rows
        // on map 631 (one waypoint per boss with a DB spawn, encounter
        // order). Gunship Saurfang, the Blood Princes and Sindragosa are
        // script-spawned (no creature rows) and are skipped; the
        // pathfinder routes the corridors between the remaining anchors.
        a.progression_waypoints = {
            { -401.4f,  2211.1f,  42.1f },  // Lord Marrowgar
            { -634.7f,  2211.4f,  52.0f },  // Lady Deathwhisper
            { -461.5f,  2211.1f, 541.2f },  // Deathbringer Saurfang
            { 4267.9f,  3137.3f, 360.6f },  // Festergut
            { 4445.9f,  3137.3f, 360.6f },  // Rotface
            { 4356.2f,  3262.9f, 389.5f },  // Professor Putricide
            { 4624.9f,  2768.3f, 402.2f },  // Blood-Queen Lana'thel
            { 4203.6f,  2483.9f, 365.0f },  // Valithria Dreamwalker
            {  428.6f, -2123.9f, 865.0f },  // The Lich King
        };
        // Boss progression — NPC entries from TC's icecrown_citadel.h.
        // Blood Prince Council is 3 mini-bosses (Keleseth/Taldaram/
        // Valanar); listing all so tank-advance finds whichever is
        // currently active. Gunship Battle isn't a single creature
        // (escort event) so it's skipped.
        a.bosses = {
            // Lower Spire
            36612,  // Lord Marrowgar
            36855,  // Lady Deathwhisper
            37187,  // High Overlord Saurfang (Gunship — Horde)
            37813,  // Deathbringer Saurfang
            // Plagueworks
            36626,  // Festergut
            36627,  // Rotface
            36678,  // Professor Putricide
            // Crimson Halls
            37972,  // Prince Keleseth (Blood Prince Council)
            37973,  // Prince Taldaram (Blood Prince Council)
            37970,  // Prince Valanar (Blood Prince Council)
            37955,  // Blood-Queen Lana'thel
            // Frostwing Halls
            36789,  // Valithria Dreamwalker
            36853,  // Sindragosa
            // Final
            36597,  // The Lich King
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeIcecrownCitadelScript()
{
    return std::make_unique<IcecrownCitadelScript>();
}

} // namespace Playerbot
