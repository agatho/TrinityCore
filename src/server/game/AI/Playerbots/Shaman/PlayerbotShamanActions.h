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

#ifndef TRINITY_PLAYERBOTSHAMAN_ACTIONS_H
#define TRINITY_PLAYERBOTSHAMAN_ACTIONS_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"

namespace ShamanSpells
{
    // Shared abilities
    constexpr uint32 GHOST_WOLF = 2645;
    constexpr uint32 ASTRAL_SHIFT = 108271;
    constexpr uint32 WIND_SHEAR = 57994;
    constexpr uint32 PURGE = 370;
    constexpr uint32 CLEANSE_SPIRIT = 51886;
    constexpr uint32 TREMOR_TOTEM = 8143;
    constexpr uint32 POISON_CLEANSING_TOTEM = 383013;
    constexpr uint32 WINDFURY_TOTEM = 8512;
    constexpr uint32 SKYFURY_TOTEM = 204330;
    constexpr uint32 EARTH_ELEMENTAL = 198103;
    constexpr uint32 FIRE_ELEMENTAL = 198067;
    
    // Elemental abilities
    constexpr uint32 LIGHTNING_BOLT = 188196;
    constexpr uint32 CHAIN_LIGHTNING = 188443;
    constexpr uint32 LAVA_BURST = 51505;
    constexpr uint32 FLAME_SHOCK = 188389;
    constexpr uint32 EARTH_SHOCK = 8042;
    constexpr uint32 ELEMENTAL_BLAST = 117014;
    constexpr uint32 EARTHQUAKE = 61882;
    constexpr uint32 ICEFURY = 210714;
    constexpr uint32 FROST_SHOCK = 196840;
    constexpr uint32 STORM_ELEMENTAL = 192249;
    constexpr uint32 STORMKEEPER = 191634;
    constexpr uint32 ASCENDANCE_ELEMENTAL = 114050;
    constexpr uint32 LIQUID_MAGMA_TOTEM = 192222;
    constexpr uint32 TEMPEST_STRIKES = 428071;
    constexpr uint32 PRIMORDIAL_WAVE = 375982;
    constexpr uint32 DEEPLY_ROOTED_ELEMENTS = 378270;
    constexpr uint32 MASTER_OF_THE_ELEMENTS = 16166;
    constexpr uint32 LAVA_SURGE = 77762;
    constexpr uint32 POWER_OF_THE_MAELSTROM = 191877;
    constexpr uint32 ECHOES_OF_GREAT_SUNDERING = 384087;
    constexpr uint32 ELEMENTAL_SPIRITS = 262624;
    constexpr uint32 PRIMAL_ELEMENTALIST = 117013;
    
    // Enhancement abilities
    constexpr uint32 STORMSTRIKE = 17364;
    constexpr uint32 LAVA_LASH = 60103;
    constexpr uint32 CRASH_LIGHTNING = 187874;
    constexpr uint32 SUNDERING = 197214;
    constexpr uint32 ICE_STRIKE = 342240;
    constexpr uint32 DOOM_WINDS = 384352;
    constexpr uint32 FERAL_SPIRIT = 51533;
    constexpr uint32 ELEMENTAL_SPIRITS_ENH = 262624;
    constexpr uint32 PRIMORDIAL_WAVE_ENH = 375982;
    constexpr uint32 WINDFURY_WEAPON = 33757;
    constexpr uint32 FLAMETONGUE_WEAPON = 318038;
    constexpr uint32 MAELSTROM_WEAPON = 187880;
    constexpr uint32 STORMSURGE = 201845;
    constexpr uint32 HOT_HAND = 201900;
    constexpr uint32 MOLTEN_ASSAULT = 334033;
    constexpr uint32 ELEMENTAL_BLAST_ENH = 117014;
    constexpr uint32 CHAIN_LIGHTNING_ENH = 188443;
    constexpr uint32 LIGHTNING_BOLT_ENH = 188196;
    constexpr uint32 ASCENDANCE_ENHANCEMENT = 114051;
    constexpr uint32 ALPHA_WOLF = 198434;
    constexpr uint32 DOOM_VORTEX = 384014;
    constexpr uint32 LEGACY_OF_THE_FROST_WITCH = 384450;
    constexpr uint32 ELEMENTAL_CYCLE = 339302;
    constexpr uint32 CRASH_OF_ELEMENTS = 334308;
    
    // Restoration abilities
    constexpr uint32 HEALING_WAVE = 331;
    constexpr uint32 HEALING_SURGE = 8004;
    constexpr uint32 CHAIN_HEAL = 1064;
    constexpr uint32 RIPTIDE = 61295;
    constexpr uint32 HEALING_RAIN = 73920;
    constexpr uint32 HEALING_STREAM_TOTEM = 5394;
    constexpr uint32 CLOUDBURST_TOTEM = 157153;
    constexpr uint32 SPIRIT_LINK_TOTEM = 98008;
    constexpr uint32 HEALING_TIDE_TOTEM = 108280;
    constexpr uint32 ANCESTRAL_GUIDANCE = 108281;
    constexpr uint32 SPIRITWALKERS_GRACE = 79206;
    constexpr uint32 NATURE_GUARDIAN = 30884;
    constexpr uint32 UNLEASH_LIFE = 73685;
    constexpr uint32 WELLSPRING = 197995;
    constexpr uint32 DOWNPOUR = 207778;
    constexpr uint32 ASCENDANCE_RESTORATION = 114052;
    constexpr uint32 CLOUDBURST_TOTEM_HEAL = 157503;
    constexpr uint32 TIDAL_WAVES = 53390;
    constexpr uint32 RESURGENCE = 16196;
    constexpr uint32 DEEP_HEALING = 77226;
    constexpr uint32 ANCESTRAL_VIGOR = 207401;
    constexpr uint32 EARTH_SHIELD = 974;
    constexpr uint32 WATER_SHIELD = 52127;
    constexpr uint32 LASHING_FLAMES = 334046;
    constexpr uint32 POISON_CLEANSING = 383013;
    constexpr uint32 PRIMORDIAL_WAVE_RESTO = 375982;
}

class TC_GAME_API PlayerbotShamanAction : public PlayerbotSpellAction
{
public:
    PlayerbotShamanAction(PlayerbotPlayerAI* ai, std::string const& name, uint32 spellId = 0)
        : PlayerbotSpellAction(ai, name, spellId) {}

protected:
    // Resource management
    uint32 GetManaAmount() const;
    uint32 GetManaPercent() const;
    uint32 GetMaelstromWeaponStacks() const;
    bool HasMaelstromWeaponStacks(uint32 minStacks = 1) const;
    bool HasMaxMaelstromWeaponStacks() const;
    bool ShouldSpendMaelstromWeapon() const;
    
    // Specialization detection
    bool isElemental() const;
    bool isEnhancement() const;
    bool isRestoration() const;
    
    // Common utility methods
    bool IsInMeleeRange(Unit* target) const;
    bool ShouldInterrupt() const;
    bool NeedsHealing() const;
    Unit* FindBestTarget() const;
    Unit* FindBestHealingTarget() const;
    uint32 CountNearbyEnemies(float range = 8.0f) const;
    uint32 CountInjuredGroupMembers(float threshold = 0.8f) const;
    bool ShouldUseAoE() const;
    
    // Shaman specific mechanics
    bool HasFlameShock(Unit* target) const;
    float GetFlameShockRemainingDuration(Unit* target) const;
    bool ShouldRefreshFlameShock(Unit* target) const;
    bool HasLavaSurgeProc() const;
    bool HasStormsurgeProc() const;
    bool HasHotHandProc() const;
    bool HasTidalWaves() const;
    uint32 GetTidalWavesStacks() const;
    
    // Totem management
    bool HasActiveTotem(uint32 spellId) const;
    bool ShouldPlaceTotem() const;
    bool HasWeaponImbue() const;
    
    // Elemental specific
    bool HasMasterOfElementsBuff() const;
    bool ShouldUseMasterOfElements() const;
    uint32 GetMaelstromPower() const;
    bool ShouldSpendMaelstrom() const;
    
    // Enhancement specific
    bool HasWindfuryWeapon() const;
    bool HasFlametongueWeapon() const;
    bool IsInDoomWinds() const;
    bool ShouldUseInstantSpell() const;
    
    // Restoration specific
    bool HasRiptide(Unit* target) const;
    bool ShouldUseChainHeal() const;
    bool ShouldUseAoEHealing() const;
    Unit* FindRiptideTarget() const;
    bool HasEarthShield(Unit* target) const;
};

// Elemental Shaman Actions
class TC_GAME_API CastLightningBolt : public PlayerbotShamanAction
{
public:
    CastLightningBolt(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "lightning bolt", ShamanSpells::LIGHTNING_BOLT) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastChainLightning : public PlayerbotShamanAction
{
public:
    CastChainLightning(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "chain lightning", ShamanSpells::CHAIN_LIGHTNING) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastLavaBurst : public PlayerbotShamanAction
{
public:
    CastLavaBurst(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "lava burst", ShamanSpells::LAVA_BURST) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastFlameShock : public PlayerbotShamanAction
{
public:
    CastFlameShock(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "flame shock", ShamanSpells::FLAME_SHOCK) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastEarthShock : public PlayerbotShamanAction
{
public:
    CastEarthShock(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "earth shock", ShamanSpells::EARTH_SHOCK) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastElementalBlast : public PlayerbotShamanAction
{
public:
    CastElementalBlast(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "elemental blast", ShamanSpells::ELEMENTAL_BLAST) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastEarthquake : public PlayerbotShamanAction
{
public:
    CastEarthquake(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "earthquake", ShamanSpells::EARTHQUAKE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastIcefury : public PlayerbotShamanAction
{
public:
    CastIcefury(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "icefury", ShamanSpells::ICEFURY) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastFrostShock : public PlayerbotShamanAction
{
public:
    CastFrostShock(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "frost shock", ShamanSpells::FROST_SHOCK) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastStormElemental : public PlayerbotShamanAction
{
public:
    CastStormElemental(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "storm elemental", ShamanSpells::STORM_ELEMENTAL) {}
};

class TC_GAME_API CastStormkeeper : public PlayerbotShamanAction
{
public:
    CastStormkeeper(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "stormkeeper", ShamanSpells::STORMKEEPER) {}
};

class TC_GAME_API CastAscendanceElemental : public PlayerbotShamanAction
{
public:
    CastAscendanceElemental(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "ascendance elemental", ShamanSpells::ASCENDANCE_ELEMENTAL) {}
};

class TC_GAME_API CastPrimordialWave : public PlayerbotShamanAction
{
public:
    CastPrimordialWave(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "primordial wave", ShamanSpells::PRIMORDIAL_WAVE) {}
protected:
    Unit* GetSpellTarget() const override;
};

// Enhancement Shaman Actions
class TC_GAME_API CastStormstrike : public PlayerbotShamanAction
{
public:
    CastStormstrike(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "stormstrike", ShamanSpells::STORMSTRIKE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastLavaLash : public PlayerbotShamanAction
{
public:
    CastLavaLash(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "lava lash", ShamanSpells::LAVA_LASH) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastCrashLightning : public PlayerbotShamanAction
{
public:
    CastCrashLightning(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "crash lightning", ShamanSpells::CRASH_LIGHTNING) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastSundering : public PlayerbotShamanAction
{
public:
    CastSundering(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "sundering", ShamanSpells::SUNDERING) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastIceStrike : public PlayerbotShamanAction
{
public:
    CastIceStrike(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "ice strike", ShamanSpells::ICE_STRIKE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastDoomWinds : public PlayerbotShamanAction
{
public:
    CastDoomWinds(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "doom winds", ShamanSpells::DOOM_WINDS) {}
};

class TC_GAME_API CastFeralSpirit : public PlayerbotShamanAction
{
public:
    CastFeralSpirit(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "feral spirit", ShamanSpells::FERAL_SPIRIT) {}
};

class TC_GAME_API CastAscendanceEnhancement : public PlayerbotShamanAction
{
public:
    CastAscendanceEnhancement(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "ascendance enhancement", ShamanSpells::ASCENDANCE_ENHANCEMENT) {}
};

class TC_GAME_API CastWindfuryWeapon : public PlayerbotShamanAction
{
public:
    CastWindfuryWeapon(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "windfury weapon", ShamanSpells::WINDFURY_WEAPON) {}
};

class TC_GAME_API CastFlametongueWeapon : public PlayerbotShamanAction
{
public:
    CastFlametongueWeapon(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "flametongue weapon", ShamanSpells::FLAMETONGUE_WEAPON) {}
};

// Restoration Shaman Actions
class TC_GAME_API CastHealingWave : public PlayerbotShamanAction
{
public:
    CastHealingWave(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "healing wave", ShamanSpells::HEALING_WAVE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastHealingSurge : public PlayerbotShamanAction
{
public:
    CastHealingSurge(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "healing surge", ShamanSpells::HEALING_SURGE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastChainHeal : public PlayerbotShamanAction
{
public:
    CastChainHeal(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "chain heal", ShamanSpells::CHAIN_HEAL) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastRiptide : public PlayerbotShamanAction
{
public:
    CastRiptide(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "riptide", ShamanSpells::RIPTIDE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastHealingRain : public PlayerbotShamanAction
{
public:
    CastHealingRain(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "healing rain", ShamanSpells::HEALING_RAIN) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastHealingStreamTotem : public PlayerbotShamanAction
{
public:
    CastHealingStreamTotem(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "healing stream totem", ShamanSpells::HEALING_STREAM_TOTEM) {}
};

class TC_GAME_API CastSpiritLinkTotem : public PlayerbotShamanAction
{
public:
    CastSpiritLinkTotem(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "spirit link totem", ShamanSpells::SPIRIT_LINK_TOTEM) {}
};

class TC_GAME_API CastHealingTideTotem : public PlayerbotShamanAction
{
public:
    CastHealingTideTotem(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "healing tide totem", ShamanSpells::HEALING_TIDE_TOTEM) {}
};

class TC_GAME_API CastAncestralGuidance : public PlayerbotShamanAction
{
public:
    CastAncestralGuidance(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "ancestral guidance", ShamanSpells::ANCESTRAL_GUIDANCE) {}
};

class TC_GAME_API CastAscendanceRestoration : public PlayerbotShamanAction
{
public:
    CastAscendanceRestoration(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "ascendance restoration", ShamanSpells::ASCENDANCE_RESTORATION) {}
};

class TC_GAME_API CastEarthShield : public PlayerbotShamanAction
{
public:
    CastEarthShield(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "earth shield", ShamanSpells::EARTH_SHIELD) {}
protected:
    Unit* GetSpellTarget() const override;
};

// Shared utility actions
class TC_GAME_API CastGhostWolf : public PlayerbotShamanAction
{
public:
    CastGhostWolf(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "ghost wolf", ShamanSpells::GHOST_WOLF) {}
};

class TC_GAME_API CastAstralShift : public PlayerbotShamanAction
{
public:
    CastAstralShift(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "astral shift", ShamanSpells::ASTRAL_SHIFT) {}
};

class TC_GAME_API CastWindShear : public PlayerbotShamanAction
{
public:
    CastWindShear(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "wind shear", ShamanSpells::WIND_SHEAR) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastPurge : public PlayerbotShamanAction
{
public:
    CastPurge(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "purge", ShamanSpells::PURGE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastCleanseSpirit : public PlayerbotShamanAction
{
public:
    CastCleanseSpirit(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "cleanse spirit", ShamanSpells::CLEANSE_SPIRIT) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastTremorTotem : public PlayerbotShamanAction
{
public:
    CastTremorTotem(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "tremor totem", ShamanSpells::TREMOR_TOTEM) {}
};

class TC_GAME_API CastWindfuryTotem : public PlayerbotShamanAction
{
public:
    CastWindfuryTotem(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "windfury totem", ShamanSpells::WINDFURY_TOTEM) {}
};

class TC_GAME_API CastEarthElemental : public PlayerbotShamanAction
{
public:
    CastEarthElemental(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "earth elemental", ShamanSpells::EARTH_ELEMENTAL) {}
};

class TC_GAME_API CastFireElemental : public PlayerbotShamanAction
{
public:
    CastFireElemental(PlayerbotPlayerAI* ai) : PlayerbotShamanAction(ai, "fire elemental", ShamanSpells::FIRE_ELEMENTAL) {}
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTSHAMAN_ACTIONS_H