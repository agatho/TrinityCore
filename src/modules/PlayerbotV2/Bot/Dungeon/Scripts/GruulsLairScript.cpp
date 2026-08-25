// GruulsLairScript — Gruul's Lair raid (map 565, TBC 25-man).
// 2 bosses: High King Maulgar (+4 adds), Gruul the Dragonkiller.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Outland/GruulsLair/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class GruulsLairScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 565; }
    char const* name() const override { return "gruuls_lair"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            18835,  // Kiggler the Crazed (Maulgar mage add)
            18836,  // Blindeye the Seer (priest healer add)
            18834,  // Olm the Summoner (warlock add)
            18832,  // Krosh Firehand (caster add)
        };
        a.mandatory_interrupt_spells = {
            // Maulgar
            33230,  // Mighty Blow
            33238,  // Whirlwind
            39144,  // Arcing Smash
            33232,  // Flurry
            // Olm the Summoner
            33129,  // Dark Decay
            33130,  // Death Coil
            33131,  // Summon Wild Felhunter
            // Kiggler the Crazed
            33173,  // Greater Polymorph
            36152,  // Lightning Bolt
            33175,  // Arcane Shock
            33237,  // Arcane Explosion
            // Blindeye the Seer
            33147,  // Greater Power Word: Shield
            33144,  // Heal
            33152,  // Prayer of Healing
            // Krosh Firehand
            33051,  // Greater Fireball
            33054,  // Spellshield
            33061,  // Blast Wave
            // Gruul
            33525,  // Ground Slam
            36297,  // Reverberation
            33813,  // Hurtful Strike
            33654,  // Shatter
        };
        a.cc_priority_entries = {
            18835,  // Kiggler the Crazed
            18836,  // Blindeye the Seer
            18834,  // Olm the Summoner
        };
        a.dangerous_auras = {
            // Maulgar
            33238,  // Whirlwind
            26561,  // Berserker Charge
            16508,  // Intimidating Roar
            29651,  // Dual Wield (boss buff — informational)
            // Gruul
            33671,  // Shatter Effect (the big knockback+rooted+damage)
            36240,  // Cave In
            33813,  // Hurtful Strike
            33652,  // Stoned (Petrify aura)
            36300,  // Growth (boss stacking damage buff)
            33525,  // Ground Slam
        };
        // Boss progression — Maulgar entry from TC's gruuls_lair.h;
        // Gruul entry 19044 from boss_gruul.cpp (standard TBC).
        a.bosses = {
            18831,  // High King Maulgar
            19044,  // Gruul the Dragonkiller (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeGruulsLairScript()
{
    return std::make_unique<GruulsLairScript>();
}

} // namespace Playerbot
