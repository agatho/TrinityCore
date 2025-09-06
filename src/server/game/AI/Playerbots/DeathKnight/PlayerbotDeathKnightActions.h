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

#ifndef TRINITY_PLAYERBOTDEATHKNIGHTACTIONS_H
#define TRINITY_PLAYERBOTDEATHKNIGHTACTIONS_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"

namespace DeathKnightSpells
{
    // Shared abilities
    constexpr uint32 DEATH_GRIP = 49576;
    constexpr uint32 DEATH_AND_DECAY = 43265;
    constexpr uint32 ANTI_MAGIC_SHELL = 48707;
    constexpr uint32 ICEBOUND_FORTITUDE = 48792;
    constexpr uint32 MIND_FREEZE = 47528;
    constexpr uint32 DARK_COMMAND = 56222;
    constexpr uint32 RAISE_DEAD = 46584;
    constexpr uint32 CONTROL_UNDEAD = 111673;
    constexpr uint32 PATH_OF_FROST = 3714;
    constexpr uint32 WRAITH_WALK = 212552;
    
    // Blood abilities
    constexpr uint32 HEART_STRIKE = 206930;
    constexpr uint32 BLOOD_BOIL = 50842;
    constexpr uint32 DEATH_STRIKE = 49998;
    constexpr uint32 MARROWREND = 195182;
    constexpr uint32 BONE_SHIELD = 195181;
    constexpr uint32 VAMPIRIC_BLOOD = 55233;
    constexpr uint32 DANCING_RUNE_WEAPON = 49028;
    constexpr uint32 TOMBSTONE = 219809;
    constexpr uint32 BONESTORM = 194844;
    constexpr uint32 BLOOD_TAP = 221699;
    constexpr uint32 RUNE_TAP = 194679;
    constexpr uint32 CONSUMPTION = 274156;
    constexpr uint32 GOREFIENDS_GRASP = 108199;
    constexpr uint32 MARK_OF_BLOOD = 206940;
    constexpr uint32 RED_THIRST = 205723;
    constexpr uint32 CRIMSON_SCOURGE = 81136;
    constexpr uint32 HEMOSTASIS = 273946;
    
    // Frost abilities
    constexpr uint32 OBLITERATE = 49020;
    constexpr uint32 FROST_STRIKE = 49143;
    constexpr uint32 HOWLING_BLAST = 49184;
    constexpr uint32 GLACIAL_ADVANCE = 194913;
    constexpr uint32 FROSTSCYTHE = 207230;
    constexpr uint32 REMORSELESS_WINTER = 196770;
    constexpr uint32 PILLAR_OF_FROST = 51271;
    constexpr uint32 EMPOWER_RUNE_WEAPON = 47568;
    constexpr uint32 HORN_OF_WINTER = 57330;
    constexpr uint32 BREATH_OF_SINDRAGOSA = 152279;
    constexpr uint32 FROSTWYRMS_FURY = 279302;
    constexpr uint32 CHILL_STREAK = 305392;
    constexpr uint32 OBLITERATION = 281238;
    constexpr uint32 KILLING_MACHINE = 51128;
    constexpr uint32 RIME = 59057;
    constexpr uint32 RUNIC_ATTENUATION = 207104;
    constexpr uint32 MURDEROUS_EFFICIENCY = 207061;
    constexpr uint32 FROZEN_PULSE = 194909;
    constexpr uint32 GATHERING_STORM = 194912;
    
    // Unholy abilities
    constexpr uint32 FESTERING_STRIKE = 85948;
    constexpr uint32 SCOURGE_STRIKE = 55090;
    constexpr uint32 DEATH_COIL = 47541;
    constexpr uint32 EPIDEMIC = 207317;
    constexpr uint32 OUTBREAK = 77575;
    constexpr uint32 DARK_TRANSFORMATION = 63560;
    constexpr uint32 APOCALYPSE = 275699;
    constexpr uint32 ARMY_OF_THE_DEAD = 42650;
    constexpr uint32 DEFILE = 152280;
    constexpr uint32 SOUL_REAPER = 343294;
    constexpr uint32 CLAWING_SHADOWS = 207311;
    constexpr uint32 NECROTIC_STRIKE = 223829;
    constexpr uint32 RAISE_ABOMINATION = 288853;
    constexpr uint32 VIRULENT_PLAGUE = 191587;
    constexpr uint32 FESTERING_WOUND = 194310;
    constexpr uint32 SUDDEN_DOOM = 49530;
    constexpr uint32 RUNIC_CORRUPTION = 51460;
    constexpr uint32 UNHOLY_PRESENCE = 48265;
    constexpr uint32 UNHOLY_FRENZY = 207289;
    constexpr uint32 DEATH_ROT = 377540;
    constexpr uint32 BURSTING_SORES = 207264;
    constexpr uint32 EBON_FEVER = 207269;
}

class TC_GAME_API PlayerbotDeathKnightAction : public PlayerbotSpellAction
{
public:
    PlayerbotDeathKnightAction(PlayerbotPlayerAI* ai, std::string const& name, uint32 spellId = 0)
        : PlayerbotSpellAction(ai, name, spellId) {}

protected:
    // Resource management
    uint32 GetRuneCount() const;
    uint32 GetRunicPowerAmount() const;
    bool HasEnoughRunes(uint32 cost) const;
    bool HasEnoughRunicPower(uint32 cost) const;
    bool IsRunicPowerCapped() const;
    bool ShouldConserveRunes() const;
    
    // Specialization detection
    bool isBlood() const;
    bool isFrost() const;
    bool isUnholy() const;
    
    // Common utility methods
    bool IsInMeleeRange(Unit* target) const;
    bool ShouldInterrupt() const;
    bool NeedsHealing() const;
    Unit* FindBestTarget() const;
    Unit* FindBestTankTarget() const;
    uint32 CountNearbyEnemies(float range = 10.0f) const;
    bool ShouldUseAoE() const;
    
    // Death Knight specific mechanics
    bool HasBoneShield() const;
    uint32 GetBoneShieldStacks() const;
    bool HasVirulentPlague(Unit* target) const;
    uint32 GetFesteringWoundStacks(Unit* target) const;
    bool HasFesteringWounds(Unit* target) const;
    bool HasKillingMachineProc() const;
    bool HasRimeProc() const;
    bool HasSuddenDoomProc() const;
    bool HasRunicCorruption() const;
    
    // Threat and positioning
    bool IsCurrentTarget(Unit* unit) const;
    bool ShouldTaunt(Unit* target) const;
    bool NeedsThreatOnTarget(Unit* target) const;
};

// Blood Death Knight Actions
class TC_GAME_API CastHeartStrike : public PlayerbotDeathKnightAction
{
public:
    CastHeartStrike(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "heart strike", DeathKnightSpells::HEART_STRIKE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastBloodBoil : public PlayerbotDeathKnightAction
{
public:
    CastBloodBoil(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "blood boil", DeathKnightSpells::BLOOD_BOIL) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastDeathStrike : public PlayerbotDeathKnightAction
{
public:
    CastDeathStrike(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "death strike", DeathKnightSpells::DEATH_STRIKE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastMarrowrend : public PlayerbotDeathKnightAction
{
public:
    CastMarrowrend(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "marrowrend", DeathKnightSpells::MARROWREND) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastVampiricBlood : public PlayerbotDeathKnightAction
{
public:
    CastVampiricBlood(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "vampiric blood", DeathKnightSpells::VAMPIRIC_BLOOD) {}
};

class TC_GAME_API CastDancingRuneWeapon : public PlayerbotDeathKnightAction
{
public:
    CastDancingRuneWeapon(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "dancing rune weapon", DeathKnightSpells::DANCING_RUNE_WEAPON) {}
};

class TC_GAME_API CastTombstone : public PlayerbotDeathKnightAction
{
public:
    CastTombstone(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "tombstone", DeathKnightSpells::TOMBSTONE) {}
};

class TC_GAME_API CastBonestorm : public PlayerbotDeathKnightAction
{
public:
    CastBonestorm(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "bonestorm", DeathKnightSpells::BONESTORM) {}
};

// Frost Death Knight Actions
class TC_GAME_API CastObliterate : public PlayerbotDeathKnightAction
{
public:
    CastObliterate(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "obliterate", DeathKnightSpells::OBLITERATE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastFrostStrike : public PlayerbotDeathKnightAction
{
public:
    CastFrostStrike(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "frost strike", DeathKnightSpells::FROST_STRIKE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastHowlingBlast : public PlayerbotDeathKnightAction
{
public:
    CastHowlingBlast(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "howling blast", DeathKnightSpells::HOWLING_BLAST) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastGlacialAdvance : public PlayerbotDeathKnightAction
{
public:
    CastGlacialAdvance(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "glacial advance", DeathKnightSpells::GLACIAL_ADVANCE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastFrostscythe : public PlayerbotDeathKnightAction
{
public:
    CastFrostscythe(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "frostscythe", DeathKnightSpells::FROSTSCYTHE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastRemorselessWinter : public PlayerbotDeathKnightAction
{
public:
    CastRemorselessWinter(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "remorseless winter", DeathKnightSpells::REMORSELESS_WINTER) {}
};

class TC_GAME_API CastPillarOfFrost : public PlayerbotDeathKnightAction
{
public:
    CastPillarOfFrost(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "pillar of frost", DeathKnightSpells::PILLAR_OF_FROST) {}
};

class TC_GAME_API CastEmpowerRuneWeapon : public PlayerbotDeathKnightAction
{
public:
    CastEmpowerRuneWeapon(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "empower rune weapon", DeathKnightSpells::EMPOWER_RUNE_WEAPON) {}
};

class TC_GAME_API CastBreathOfSindragosa : public PlayerbotDeathKnightAction
{
public:
    CastBreathOfSindragosa(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "breath of sindragosa", DeathKnightSpells::BREATH_OF_SINDRAGOSA) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastFrostwyrmsFury : public PlayerbotDeathKnightAction
{
public:
    CastFrostwyrmsFury(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "frostwyrms fury", DeathKnightSpells::FROSTWYRMS_FURY) {}
protected:
    Unit* GetSpellTarget() const override;
};

// Unholy Death Knight Actions
class TC_GAME_API CastFesteringStrike : public PlayerbotDeathKnightAction
{
public:
    CastFesteringStrike(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "festering strike", DeathKnightSpells::FESTERING_STRIKE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastScourgeStrike : public PlayerbotDeathKnightAction
{
public:
    CastScourgeStrike(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "scourge strike", DeathKnightSpells::SCOURGE_STRIKE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastDeathCoil : public PlayerbotDeathKnightAction
{
public:
    CastDeathCoil(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "death coil", DeathKnightSpells::DEATH_COIL) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastEpidemic : public PlayerbotDeathKnightAction
{
public:
    CastEpidemic(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "epidemic", DeathKnightSpells::EPIDEMIC) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastOutbreak : public PlayerbotDeathKnightAction
{
public:
    CastOutbreak(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "outbreak", DeathKnightSpells::OUTBREAK) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastDarkTransformation : public PlayerbotDeathKnightAction
{
public:
    CastDarkTransformation(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "dark transformation", DeathKnightSpells::DARK_TRANSFORMATION) {}
};

class TC_GAME_API CastApocalypse : public PlayerbotDeathKnightAction
{
public:
    CastApocalypse(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "apocalypse", DeathKnightSpells::APOCALYPSE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastArmyOfTheDead : public PlayerbotDeathKnightAction
{
public:
    CastArmyOfTheDead(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "army of the dead", DeathKnightSpells::ARMY_OF_THE_DEAD) {}
};

class TC_GAME_API CastSoulReaper : public PlayerbotDeathKnightAction
{
public:
    CastSoulReaper(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "soul reaper", DeathKnightSpells::SOUL_REAPER) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastDefile : public PlayerbotDeathKnightAction
{
public:
    CastDefile(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "defile", DeathKnightSpells::DEFILE) {}
protected:
    Unit* GetSpellTarget() const override;
};

// Shared utility actions
class TC_GAME_API CastDeathGrip : public PlayerbotDeathKnightAction
{
public:
    CastDeathGrip(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "death grip", DeathKnightSpells::DEATH_GRIP) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastDeathAndDecay : public PlayerbotDeathKnightAction
{
public:
    CastDeathAndDecay(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "death and decay", DeathKnightSpells::DEATH_AND_DECAY) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastAntiMagicShell : public PlayerbotDeathKnightAction
{
public:
    CastAntiMagicShell(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "anti magic shell", DeathKnightSpells::ANTI_MAGIC_SHELL) {}
};

class TC_GAME_API CastIceboundFortitude : public PlayerbotDeathKnightAction
{
public:
    CastIceboundFortitude(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "icebound fortitude", DeathKnightSpells::ICEBOUND_FORTITUDE) {}
};

class TC_GAME_API CastMindFreeze : public PlayerbotDeathKnightAction
{
public:
    CastMindFreeze(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "mind freeze", DeathKnightSpells::MIND_FREEZE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastDarkCommand : public PlayerbotDeathKnightAction
{
public:
    CastDarkCommand(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "dark command", DeathKnightSpells::DARK_COMMAND) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastRaiseDead : public PlayerbotDeathKnightAction
{
public:
    CastRaiseDead(PlayerbotPlayerAI* ai) : PlayerbotDeathKnightAction(ai, "raise dead", DeathKnightSpells::RAISE_DEAD) {}
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTDEATHKNIGHTACTIONS_H