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

#ifndef TRINITY_PLAYERBOTWARLOCK_ACTIONS_H
#define TRINITY_PLAYERBOTWARLOCK_ACTIONS_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"

/**
 * @brief Spell IDs for Warlock abilities - The War Within 11.2
 */
namespace WarlockSpells
{
    // Common Warlock Spells
    constexpr uint32 SHADOW_BOLT = 686;
    constexpr uint32 DRAIN_LIFE = 234153;
    constexpr uint32 DRAIN_SOUL = 198590;
    constexpr uint32 FEAR = 5782;
    constexpr uint32 CREATE_HEALTHSTONE = 6201;
    constexpr uint32 CREATE_SOULSTONE = 693;
    constexpr uint32 UNENDING_RESOLVE = 104773;
    constexpr uint32 DARK_PACT = 108416;
    constexpr uint32 BANISH = 710;
    constexpr uint32 ENSLAVE_DEMON = 1098;
    constexpr uint32 SUMMON_IMP = 688;
    constexpr uint32 SUMMON_VOIDWALKER = 697;
    constexpr uint32 SUMMON_FELHUNTER = 691;
    constexpr uint32 SUMMON_SUCCUBUS = 712;
    constexpr uint32 RITUAL_OF_SUMMONING = 698;
    constexpr uint32 DEMONIC_GATEWAY = 111771;
    constexpr uint32 SOULSTONE = 20707;
    
    // Affliction Spells
    constexpr uint32 CORRUPTION = 172;
    constexpr uint32 AGONY = 980;
    constexpr uint32 UNSTABLE_AFFLICTION = 30108;
    constexpr uint32 SIPHON_SOUL = 63106;
    constexpr uint32 MALEFIC_RAPTURE = 324536;
    constexpr uint32 SEED_OF_CORRUPTION = 27243;
    constexpr uint32 VILE_TAINT = 278350;
    constexpr uint32 PHANTOM_SINGULARITY = 205179;
    constexpr uint32 DARK_SOUL_MISERY = 113860;
    constexpr uint32 SOUL_ROT = 386997;
    constexpr uint32 SUMMON_DARKGLARE = 205180;
    constexpr uint32 HAUNT = 48181;
    constexpr uint32 WRITHE_IN_AGONY = 196102;
    constexpr uint32 ABSOLUTE_CORRUPTION = 196103;
    
    // Demonology Spells
    constexpr uint32 DEMONBOLT = 264178;
    constexpr uint32 HAND_OF_GULDAN = 105174;
    constexpr uint32 CALL_DREADSTALKERS = 104316;
    constexpr uint32 SUMMON_VILEFIEND = 264119;
    constexpr uint32 SUMMON_DEMONIC_TYRANT = 265187;
    constexpr uint32 DEMONIC_STRENGTH = 267171;
    constexpr uint32 BILESCOURGE_BOMBERS = 267211;
    constexpr uint32 POWER_SIPHON = 264130;
    constexpr uint32 DOOM = 603;
    constexpr uint32 NETHER_PORTAL = 267217;
    constexpr uint32 GRIMOIRE_FELGUARD = 111898;
    constexpr uint32 SOUL_STRIKE = 264057;
    constexpr uint32 SHADOW_CLEAVE = 50581;
    constexpr uint32 DEMONIC_CALLING = 205145;
    constexpr uint32 WILD_IMP_SUMMON = 104317;
    
    // Destruction Spells
    constexpr uint32 INCINERATE = 29722;
    constexpr uint32 CONFLAGRATE = 17962;
    constexpr uint32 IMMOLATE = 348;
    constexpr uint32 CHAOS_BOLT = 116858;
    constexpr uint32 RAIN_OF_FIRE = 5740;
    constexpr uint32 HAVOC = 80240;
    constexpr uint32 CATACLYSM = 152108;
    constexpr uint32 CHANNEL_DEMONFIRE = 196447;
    constexpr uint32 SOUL_FIRE = 6353;
    constexpr uint32 SHADOWBURN = 17877;
    constexpr uint32 DARK_SOUL_INSTABILITY = 113858;
    constexpr uint32 SUMMON_INFERNAL = 1122;
    constexpr uint32 REVERSE_ENTROPY = 205148;
    constexpr uint32 INTERNAL_COMBUSTION = 266134;
    constexpr uint32 FLASHOVER = 267115;
    constexpr uint32 BACKDRAFT = 117828;
    constexpr uint32 ROARING_BLAZE = 205184;
    constexpr uint32 ERADICATION = 196412;
}

/**
 * @brief Base class for all Warlock actions
 */
class TC_GAME_API PlayerbotWarlockAction : public PlayerbotSpellAction
{
public:
    PlayerbotWarlockAction(PlayerbotPlayerAI* ai, std::string const& name, uint32 spellId = 0)
        : PlayerbotSpellAction(ai, name, spellId) {}
    
    // Common Warlock utility methods
    uint32 GetManaAmount() const;
    uint32 GetManaPercent() const;
    uint32 GetSoulShards() const;
    bool HasEnoughMana(uint32 cost) const;
    bool HasEnoughSoulShards(uint32 cost) const;
    bool IsInCombat() const;
    bool ShouldConserveMana() const;
    
    // Specialization detection
    bool isAffliction() const;
    bool isDemonology() const;
    bool isDestruction() const;
    
    // Pet management
    bool HasActivePet() const;
    bool ShouldSummonPet() const;
    Unit* GetPet() const;
    bool IsPetInCombat() const;
    
    // Target evaluation
    Unit* FindBestTarget() const;
    uint32 CountNearbyEnemies(float range = 8.0f) const;
    bool ShouldUseAoE() const;
    
    // DoT management
    float GetCorruptionDuration(Unit* target) const;
    float GetAgonyDuration(Unit* target) const;
    float GetImmolateDuration(Unit* target) const;
    bool HasCorruption(Unit* target) const;
    bool HasAgony(Unit* target) const;
    bool HasImmolate(Unit* target) const;
    bool ShouldRefreshDoT(Unit* target, uint32 spellId, float threshold = 4.0f) const;
    
    // Proc and buff management
    bool HasBackdraft() const;
    bool HasEradication() const;
    bool HasDemonicCalling() const;
    uint32 GetBackdraftStacks() const;
    
protected:
    // Resource validation for different spell types
    bool CanCastAfflictionSpell() const;
    bool CanCastDemonologySpell() const;
    bool CanCastDestructionSpell() const;
};

// Affliction Warlock Actions
class TC_GAME_API CastCorruption : public PlayerbotWarlockAction
{
public:
    CastCorruption(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "corruption", WarlockSpells::CORRUPTION) {}
};

class TC_GAME_API CastAgony : public PlayerbotWarlockAction
{
public:
    CastAgony(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "agony", WarlockSpells::AGONY) {}
};

class TC_GAME_API CastUnstableAffliction : public PlayerbotWarlockAction
{
public:
    CastUnstableAffliction(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "unstable affliction", WarlockSpells::UNSTABLE_AFFLICTION) {}
};

class TC_GAME_API CastMaleficRapture : public PlayerbotWarlockAction
{
public:
    CastMaleficRapture(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "malefic rapture", WarlockSpells::MALEFIC_RAPTURE) {}
};

class TC_GAME_API CastSeedOfCorruption : public PlayerbotWarlockAction
{
public:
    CastSeedOfCorruption(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "seed of corruption", WarlockSpells::SEED_OF_CORRUPTION) {}
};

class TC_GAME_API CastVileTaint : public PlayerbotWarlockAction
{
public:
    CastVileTaint(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "vile taint", WarlockSpells::VILE_TAINT) {}
};

class TC_GAME_API CastPhantomSingularity : public PlayerbotWarlockAction
{
public:
    CastPhantomSingularity(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "phantom singularity", WarlockSpells::PHANTOM_SINGULARITY) {}
};

class TC_GAME_API CastDarkSoulMisery : public PlayerbotWarlockAction
{
public:
    CastDarkSoulMisery(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "dark soul misery", WarlockSpells::DARK_SOUL_MISERY) {}
};

class TC_GAME_API CastSoulRot : public PlayerbotWarlockAction
{
public:
    CastSoulRot(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "soul rot", WarlockSpells::SOUL_ROT) {}
};

class TC_GAME_API CastSummonDarkglare : public PlayerbotWarlockAction
{
public:
    CastSummonDarkglare(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "summon darkglare", WarlockSpells::SUMMON_DARKGLARE) {}
};

class TC_GAME_API CastHaunt : public PlayerbotWarlockAction
{
public:
    CastHaunt(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "haunt", WarlockSpells::HAUNT) {}
};

// Demonology Warlock Actions
class TC_GAME_API CastDemonbolt : public PlayerbotWarlockAction
{
public:
    CastDemonbolt(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "demonbolt", WarlockSpells::DEMONBOLT) {}
};

class TC_GAME_API CastHandOfGuldan : public PlayerbotWarlockAction
{
public:
    CastHandOfGuldan(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "hand of guldan", WarlockSpells::HAND_OF_GULDAN) {}
};

class TC_GAME_API CastCallDreadstalkers : public PlayerbotWarlockAction
{
public:
    CastCallDreadstalkers(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "call dreadstalkers", WarlockSpells::CALL_DREADSTALKERS) {}
};

class TC_GAME_API CastSummonVilefiend : public PlayerbotWarlockAction
{
public:
    CastSummonVilefiend(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "summon vilefiend", WarlockSpells::SUMMON_VILEFIEND) {}
};

class TC_GAME_API CastSummonDemonicTyrant : public PlayerbotWarlockAction
{
public:
    CastSummonDemonicTyrant(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "summon demonic tyrant", WarlockSpells::SUMMON_DEMONIC_TYRANT) {}
};

class TC_GAME_API CastDemonicStrength : public PlayerbotWarlockAction
{
public:
    CastDemonicStrength(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "demonic strength", WarlockSpells::DEMONIC_STRENGTH) {}
};

class TC_GAME_API CastBilescourgeBombers : public PlayerbotWarlockAction
{
public:
    CastBilescourgeBombers(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "bilescourge bombers", WarlockSpells::BILESCOURGE_BOMBERS) {}
};

class TC_GAME_API CastPowerSiphon : public PlayerbotWarlockAction
{
public:
    CastPowerSiphon(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "power siphon", WarlockSpells::POWER_SIPHON) {}
};

class TC_GAME_API CastDoom : public PlayerbotWarlockAction
{
public:
    CastDoom(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "doom", WarlockSpells::DOOM) {}
};

class TC_GAME_API CastNetherPortal : public PlayerbotWarlockAction
{
public:
    CastNetherPortal(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "nether portal", WarlockSpells::NETHER_PORTAL) {}
};

class TC_GAME_API CastGrimoireFelguard : public PlayerbotWarlockAction
{
public:
    CastGrimoireFelguard(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "grimoire felguard", WarlockSpells::GRIMOIRE_FELGUARD) {}
};

// Destruction Warlock Actions
class TC_GAME_API CastIncinerate : public PlayerbotWarlockAction
{
public:
    CastIncinerate(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "incinerate", WarlockSpells::INCINERATE) {}
};

class TC_GAME_API CastConflagrate : public PlayerbotWarlockAction
{
public:
    CastConflagrate(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "conflagrate", WarlockSpells::CONFLAGRATE) {}
};

class TC_GAME_API CastImmolate : public PlayerbotWarlockAction
{
public:
    CastImmolate(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "immolate", WarlockSpells::IMMOLATE) {}
};

class TC_GAME_API CastChaosBolt : public PlayerbotWarlockAction
{
public:
    CastChaosBolt(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "chaos bolt", WarlockSpells::CHAOS_BOLT) {}
};

class TC_GAME_API CastRainOfFire : public PlayerbotWarlockAction
{
public:
    CastRainOfFire(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "rain of fire", WarlockSpells::RAIN_OF_FIRE) {}
};

class TC_GAME_API CastHavoc : public PlayerbotWarlockAction
{
public:
    CastHavoc(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "havoc", WarlockSpells::HAVOC) {}
};

class TC_GAME_API CastCataclysm : public PlayerbotWarlockAction
{
public:
    CastCataclysm(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "cataclysm", WarlockSpells::CATACLYSM) {}
};

class TC_GAME_API CastChannelDemonfire : public PlayerbotWarlockAction
{
public:
    CastChannelDemonfire(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "channel demonfire", WarlockSpells::CHANNEL_DEMONFIRE) {}
};

class TC_GAME_API CastSoulFire : public PlayerbotWarlockAction
{
public:
    CastSoulFire(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "soul fire", WarlockSpells::SOUL_FIRE) {}
};

class TC_GAME_API CastShadowburn : public PlayerbotWarlockAction
{
public:
    CastShadowburn(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "shadowburn", WarlockSpells::SHADOWBURN) {}
};

class TC_GAME_API CastDarkSoulInstability : public PlayerbotWarlockAction
{
public:
    CastDarkSoulInstability(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "dark soul instability", WarlockSpells::DARK_SOUL_INSTABILITY) {}
};

class TC_GAME_API CastSummonInfernal : public PlayerbotWarlockAction
{
public:
    CastSummonInfernal(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "summon infernal", WarlockSpells::SUMMON_INFERNAL) {}
};

// Common Warlock Actions
class TC_GAME_API CastShadowBolt : public PlayerbotWarlockAction
{
public:
    CastShadowBolt(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "shadow bolt", WarlockSpells::SHADOW_BOLT) {}
};

class TC_GAME_API CastDrainLife : public PlayerbotWarlockAction
{
public:
    CastDrainLife(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "drain life", WarlockSpells::DRAIN_LIFE) {}
};

class TC_GAME_API CastDrainSoul : public PlayerbotWarlockAction
{
public:
    CastDrainSoul(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "drain soul", WarlockSpells::DRAIN_SOUL) {}
};

class TC_GAME_API CastFear : public PlayerbotWarlockAction
{
public:
    CastFear(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "fear", WarlockSpells::FEAR) {}
};

class TC_GAME_API CastUnendingResolve : public PlayerbotWarlockAction
{
public:
    CastUnendingResolve(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "unending resolve", WarlockSpells::UNENDING_RESOLVE) {}
};

class TC_GAME_API CastDarkPact : public PlayerbotWarlockAction
{
public:
    CastDarkPact(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "dark pact", WarlockSpells::DARK_PACT) {}
};

class TC_GAME_API CastBanish : public PlayerbotWarlockAction
{
public:
    CastBanish(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "banish", WarlockSpells::BANISH) {}
};

class TC_GAME_API CastSummonImp : public PlayerbotWarlockAction
{
public:
    CastSummonImp(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "summon imp", WarlockSpells::SUMMON_IMP) {}
};

class TC_GAME_API CastSummonVoidwalker : public PlayerbotWarlockAction
{
public:
    CastSummonVoidwalker(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "summon voidwalker", WarlockSpells::SUMMON_VOIDWALKER) {}
};

class TC_GAME_API CastSummonFelhunter : public PlayerbotWarlockAction
{
public:
    CastSummonFelhunter(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "summon felhunter", WarlockSpells::SUMMON_FELHUNTER) {}
};

class TC_GAME_API CastSummonSuccubus : public PlayerbotWarlockAction
{
public:
    CastSummonSuccubus(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "summon succubus", WarlockSpells::SUMMON_SUCCUBUS) {}
};

class TC_GAME_API CastDemonicGateway : public PlayerbotWarlockAction
{
public:
    CastDemonicGateway(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "demonic gateway", WarlockSpells::DEMONIC_GATEWAY) {}
};

class TC_GAME_API CastCreateHealthstone : public PlayerbotWarlockAction
{
public:
    CastCreateHealthstone(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "create healthstone", WarlockSpells::CREATE_HEALTHSTONE) {}
};

class TC_GAME_API CastCreateSoulstone : public PlayerbotWarlockAction
{
public:
    CastCreateSoulstone(PlayerbotPlayerAI* ai) : PlayerbotWarlockAction(ai, "create soulstone", WarlockSpells::CREATE_SOULSTONE) {}
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTWARLOCK_ACTIONS_H