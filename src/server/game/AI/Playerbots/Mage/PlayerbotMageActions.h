/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRINITY_PLAYERBOTMAGEACTIONS_H
#define TRINITY_PLAYERBOTMAGEACTIONS_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"

/**
 * @brief Spell IDs for Mage abilities - The War Within 11.2
 */
namespace MageSpells
{
    // Common Mage Spells
    constexpr uint32 ARCANE_INTELLECT = 1459;
    constexpr uint32 COUNTERSPELL = 2139;
    constexpr uint32 DISPEL_MAGIC = 30449;
    constexpr uint32 REMOVE_CURSE = 475;
    constexpr uint32 SLOW_FALL = 130;
    constexpr uint32 BLINK = 1953;
    constexpr uint32 TELEPORT_STORMWIND = 3561;
    constexpr uint32 PORTAL_STORMWIND = 10059;
    constexpr uint32 TIME_WARP = 80353;
    constexpr uint32 MIRROR_IMAGE = 55342;
    constexpr uint32 ICE_BLOCK = 45438;
    constexpr uint32 INVISIBILITY = 66;
    
    // Arcane Spells
    constexpr uint32 ARCANE_MISSILE = 5143;
    constexpr uint32 ARCANE_BLAST = 30451;
    constexpr uint32 ARCANE_EXPLOSION = 1449;
    constexpr uint32 ARCANE_BARRAGE = 44425;
    constexpr uint32 ARCANE_ORB = 153626;
    constexpr uint32 PRESENCE_OF_MIND = 205025;
    constexpr uint32 ARCANE_POWER = 12042;
    constexpr uint32 DISPLACEMENT = 212801;
    constexpr uint32 TOUCH_OF_THE_MAGI = 321507;
    constexpr uint32 EVOCATION = 12051;
    constexpr uint32 SUPERNOVA = 157980;
    constexpr uint32 ARCANE_FAMILIAR = 205022;
    
    // Fire Spells  
    constexpr uint32 FIREBALL = 133;
    constexpr uint32 FIRE_BLAST = 108853;
    constexpr uint32 SCORCH = 2948;
    constexpr uint32 PYROBLAST = 11366;
    constexpr uint32 FLAMESTRIKE = 2120;
    constexpr uint32 COMBUSTION = 190319;
    constexpr uint32 PHOENIX_FLAMES = 257541;
    constexpr uint32 DRAGON_BREATH = 31661;
    constexpr uint32 LIVING_BOMB = 44457;
    constexpr uint32 BLAZING_BARRIER = 235313;
    constexpr uint32 METEOR = 153561;
    constexpr uint32 IGNITE = 12654;
    constexpr uint32 HOT_STREAK = 48108;
    constexpr uint32 HEATING_UP = 48107;
    
    // Frost Spells
    constexpr uint32 FROSTBOLT = 116;
    constexpr uint32 FROST_NOVA = 122;
    constexpr uint32 CONE_OF_COLD = 120;
    constexpr uint32 BLIZZARD = 190356;
    constexpr uint32 ICE_LANCE = 30455;
    constexpr uint32 FLURRY = 44614;
    constexpr uint32 FROZEN_ORB = 84714;
    constexpr uint32 COMET_STORM = 153595;
    constexpr uint32 GLACIAL_SPIKE = 199786;
    constexpr uint32 RAY_OF_FROST = 205021;
    constexpr uint32 FROST_ARMOR = 168;
    constexpr uint32 ICE_BARRIER = 11426;
    constexpr uint32 BRAIN_FREEZE = 190446;
    constexpr uint32 FINGERS_OF_FROST = 44544;
    constexpr uint32 WINTERS_CHILL = 228358;
    constexpr uint32 SHATTER = 12982;
    constexpr uint32 DEEP_FREEZE = 44572;
}

/**
 * @brief Base class for all Mage actions
 */
class TC_GAME_API PlayerbotMageAction : public PlayerbotSpellAction
{
public:
    PlayerbotMageAction(PlayerbotPlayerAI* ai, std::string const& name, uint32 spellId = 0)
        : PlayerbotSpellAction(ai, name, spellId) {}
    
    // Common Mage utility methods
    uint32 GetManaAmount() const;
    uint32 GetManaPercent() const;
    uint32 GetArcaneCharges() const;
    bool HasEnoughMana(uint32 cost) const;
    bool IsInCombat() const;
    bool ShouldConserveMana() const;
    
    // Specialization detection
    bool isArcane() const;
    bool isFire() const;
    bool isFrost() const;
    
    // Target evaluation
    Unit* FindBestTarget() const;
    uint32 CountNearbyEnemies(float range = 8.0f) const;
    bool ShouldUseAoE() const;
    
    // Buff/debuff management
    bool HasArcaneIntellect() const;
    bool HasMageArmor() const;
    bool IsTargetFrozen(Unit* target) const;
    bool HasBrainFreeze() const;
    bool HasFingersOfFrost() const;
    bool HasHotStreak() const;
    bool HasHeatingUp() const;
    float GetIgniteDuration(Unit* target) const;
    
protected:
    // Resource validation for different spell types
    bool CanCastArcaneSpell(uint32 charges = 0) const;
    bool CanCastFireSpell() const;
    bool CanCastFrostSpell() const;
};

// Arcane Mage Actions
class TC_GAME_API CastArcaneMissile : public PlayerbotMageAction
{
public:
    CastArcaneMissile(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "arcane missile", MageSpells::ARCANE_MISSILE) {}
};

class TC_GAME_API CastArcaneBlast : public PlayerbotMageAction
{
public:
    CastArcaneBlast(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "arcane blast", MageSpells::ARCANE_BLAST) {}
};

class TC_GAME_API CastArcaneBarrage : public PlayerbotMageAction
{
public:
    CastArcaneBarrage(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "arcane barrage", MageSpells::ARCANE_BARRAGE) {}
};

class TC_GAME_API CastArcaneExplosion : public PlayerbotMageAction
{
public:
    CastArcaneExplosion(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "arcane explosion", MageSpells::ARCANE_EXPLOSION) {}
};

class TC_GAME_API CastArcaneOrb : public PlayerbotMageAction
{
public:
    CastArcaneOrb(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "arcane orb", MageSpells::ARCANE_ORB) {}
};

class TC_GAME_API CastPresenceOfMind : public PlayerbotMageAction
{
public:
    CastPresenceOfMind(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "presence of mind", MageSpells::PRESENCE_OF_MIND) {}
};

class TC_GAME_API CastArcanePower : public PlayerbotMageAction
{
public:
    CastArcanePower(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "arcane power", MageSpells::ARCANE_POWER) {}
};

class TC_GAME_API CastTouchOfTheMagi : public PlayerbotMageAction
{
public:
    CastTouchOfTheMagi(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "touch of the magi", MageSpells::TOUCH_OF_THE_MAGI) {}
};

class TC_GAME_API CastEvocation : public PlayerbotMageAction
{
public:
    CastEvocation(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "evocation", MageSpells::EVOCATION) {}
};

class TC_GAME_API CastSupernova : public PlayerbotMageAction
{
public:
    CastSupernova(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "supernova", MageSpells::SUPERNOVA) {}
};

// Fire Mage Actions
class TC_GAME_API CastFireball : public PlayerbotMageAction
{
public:
    CastFireball(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "fireball", MageSpells::FIREBALL) {}
};

class TC_GAME_API CastFireBlast : public PlayerbotMageAction
{
public:
    CastFireBlast(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "fire blast", MageSpells::FIRE_BLAST) {}
};

class TC_GAME_API CastScorch : public PlayerbotMageAction
{
public:
    CastScorch(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "scorch", MageSpells::SCORCH) {}
};

class TC_GAME_API CastPyroblast : public PlayerbotMageAction
{
public:
    CastPyroblast(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "pyroblast", MageSpells::PYROBLAST) {}
};

class TC_GAME_API CastFlamestrike : public PlayerbotMageAction
{
public:
    CastFlamestrike(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "flamestrike", MageSpells::FLAMESTRIKE) {}
};

class TC_GAME_API CastCombustion : public PlayerbotMageAction
{
public:
    CastCombustion(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "combustion", MageSpells::COMBUSTION) {}
};

class TC_GAME_API CastPhoenixFlames : public PlayerbotMageAction
{
public:
    CastPhoenixFlames(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "phoenix flames", MageSpells::PHOENIX_FLAMES) {}
};

class TC_GAME_API CastDragonBreath : public PlayerbotMageAction
{
public:
    CastDragonBreath(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "dragon breath", MageSpells::DRAGON_BREATH) {}
};

class TC_GAME_API CastLivingBomb : public PlayerbotMageAction
{
public:
    CastLivingBomb(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "living bomb", MageSpells::LIVING_BOMB) {}
};

class TC_GAME_API CastMeteor : public PlayerbotMageAction
{
public:
    CastMeteor(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "meteor", MageSpells::METEOR) {}
};

class TC_GAME_API CastBlazingBarrier : public PlayerbotMageAction
{
public:
    CastBlazingBarrier(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "blazing barrier", MageSpells::BLAZING_BARRIER) {}
};

// Frost Mage Actions
class TC_GAME_API CastFrostbolt : public PlayerbotMageAction
{
public:
    CastFrostbolt(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "frostbolt", MageSpells::FROSTBOLT) {}
};

class TC_GAME_API CastIceLance : public PlayerbotMageAction
{
public:
    CastIceLance(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "ice lance", MageSpells::ICE_LANCE) {}
};

class TC_GAME_API CastFlurry : public PlayerbotMageAction
{
public:
    CastFlurry(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "flurry", MageSpells::FLURRY) {}
};

class TC_GAME_API CastFrostNova : public PlayerbotMageAction
{
public:
    CastFrostNova(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "frost nova", MageSpells::FROST_NOVA) {}
};

class TC_GAME_API CastConeOfCold : public PlayerbotMageAction
{
public:
    CastConeOfCold(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "cone of cold", MageSpells::CONE_OF_COLD) {}
};

class TC_GAME_API CastBlizzard : public PlayerbotMageAction
{
public:
    CastBlizzard(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "blizzard", MageSpells::BLIZZARD) {}
};

class TC_GAME_API CastFrozenOrb : public PlayerbotMageAction
{
public:
    CastFrozenOrb(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "frozen orb", MageSpells::FROZEN_ORB) {}
};

class TC_GAME_API CastCometStorm : public PlayerbotMageAction
{
public:
    CastCometStorm(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "comet storm", MageSpells::COMET_STORM) {}
};

class TC_GAME_API CastGlacialSpike : public PlayerbotMageAction
{
public:
    CastGlacialSpike(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "glacial spike", MageSpells::GLACIAL_SPIKE) {}
};

class TC_GAME_API CastRayOfFrost : public PlayerbotMageAction
{
public:
    CastRayOfFrost(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "ray of frost", MageSpells::RAY_OF_FROST) {}
};

class TC_GAME_API CastIceBarrier : public PlayerbotMageAction
{
public:
    CastIceBarrier(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "ice barrier", MageSpells::ICE_BARRIER) {}
};

// Common Mage Actions
class TC_GAME_API CastCounterspell : public PlayerbotMageAction
{
public:
    CastCounterspell(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "counterspell", MageSpells::COUNTERSPELL) {}
};

class TC_GAME_API CastArcaneIntellect : public PlayerbotMageAction
{
public:
    CastArcaneIntellect(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "arcane intellect", MageSpells::ARCANE_INTELLECT) {}
};

class TC_GAME_API CastTimeWarp : public PlayerbotMageAction
{
public:
    CastTimeWarp(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "time warp", MageSpells::TIME_WARP) {}
};

class TC_GAME_API CastMirrorImage : public PlayerbotMageAction
{
public:
    CastMirrorImage(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "mirror image", MageSpells::MIRROR_IMAGE) {}
};

class TC_GAME_API CastIceBlock : public PlayerbotMageAction
{
public:
    CastIceBlock(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "ice block", MageSpells::ICE_BLOCK) {}
};

class TC_GAME_API CastInvisibility : public PlayerbotMageAction
{
public:
    CastInvisibility(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "invisibility", MageSpells::INVISIBILITY) {}
};

class TC_GAME_API CastBlink : public PlayerbotMageAction
{
public:
    CastBlink(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "blink", MageSpells::BLINK) {}
};

class TC_GAME_API CastDispelMagic : public PlayerbotMageAction
{
public:
    CastDispelMagic(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "dispel magic", MageSpells::DISPEL_MAGIC) {}
};

class TC_GAME_API CastRemoveCurse : public PlayerbotMageAction
{
public:
    CastRemoveCurse(PlayerbotPlayerAI* ai) : PlayerbotMageAction(ai, "remove curse", MageSpells::REMOVE_CURSE) {}
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTMAGEACTIONS_H