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

#ifndef TRINITY_PLAYERBOTROGUEACTIONS_H
#define TRINITY_PLAYERBOTROGUEACTIONS_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"

namespace RogueSpells
{
    // Shared abilities
    constexpr uint32 STEALTH = 1784;
    constexpr uint32 VANISH = 1856;
    constexpr uint32 SPRINT = 2983;
    constexpr uint32 SHADOWSTEP = 36554;
    constexpr uint32 KICK = 1766;
    constexpr uint32 BLIND = 2094;
    constexpr uint32 FEINT = 1966;
    constexpr uint32 CLOAK_OF_SHADOWS = 31224;
    constexpr uint32 EVASION = 5277;
    constexpr uint32 CRIMSON_VIAL = 185311;
    
    // Assassination abilities
    constexpr uint32 MUTILATE = 1329;
    constexpr uint32 ENVENOM = 32645;
    constexpr uint32 GARROTE = 703;
    constexpr uint32 RUPTURE = 1943;
    constexpr uint32 POISON_BOMB = 255544;
    constexpr uint32 VENDETTA = 79140;
    constexpr uint32 DEATHMARK = 360194;
    constexpr uint32 KINGSBANE = 385627;
    constexpr uint32 SHIV = 5938;
    constexpr uint32 FAN_OF_KNIVES = 51723;
    constexpr uint32 SLICE_AND_DICE = 315496;
    constexpr uint32 COLD_BLOOD = 382245;
    constexpr uint32 IMPROVED_GARROTE = 381632;
    constexpr uint32 BLINDSIDE = 328085;
    constexpr uint32 MASTER_ASSASSIN = 256735;
    constexpr uint32 DEADLY_POISON = 2823;
    constexpr uint32 WOUND_POISON = 8679;
    constexpr uint32 CRIPPLING_POISON = 3408;
    
    // Outlaw abilities
    constexpr uint32 SINISTER_STRIKE = 193315;
    constexpr uint32 DISPATCH = 2098;
    constexpr uint32 PISTOL_SHOT = 185763;
    constexpr uint32 BETWEEN_THE_EYES = 315341;
    constexpr uint32 ROLL_THE_BONES = 315508;
    constexpr uint32 ADRENALINE_RUSH = 13750;
    constexpr uint32 BLADE_FLURRY = 13877;
    constexpr uint32 KILLING_SPREE = 51690;
    constexpr uint32 GHOSTLY_STRIKE = 196937;
    constexpr uint32 BLADE_RUSH = 271877;
    constexpr uint32 OPPORTUNITY = 279876;
    constexpr uint32 RESTLESS_BLADES = 79096;
    constexpr uint32 LOADED_DICE = 256170;
    constexpr uint32 SLEIGHT_OF_HAND = 381839;
    constexpr uint32 DREADBLADES = 343142;
    constexpr uint32 BROADSIDE = 193356;
    constexpr uint32 BURIED_TREASURE = 199600;
    constexpr uint32 GRAND_MELEE = 193358;
    constexpr uint32 RUTHLESS_PRECISION = 193357;
    constexpr uint32 SKULL_AND_CROSSBONES = 199603;
    constexpr uint32 TRUE_BEARING = 193359;
    
    // Subtlety abilities
    constexpr uint32 BACKSTAB = 53;
    constexpr uint32 EVISCERATE = 196819;
    constexpr uint32 SHADOWSTRIKE = 185438;
    constexpr uint32 SHADOW_DANCE = 185313;
    constexpr uint32 SYMBOLS_OF_DEATH = 212283;
    constexpr uint32 SHADOW_CLONE = 382121;
    constexpr uint32 RUPTURE_SUB = 1943;
    constexpr uint32 BLACK_POWDER = 319175;
    constexpr uint32 SHURIKEN_STORM = 197835;
    constexpr uint32 GLOOMBLADE = 200758;
    constexpr uint32 FIND_WEAKNESS = 91023;
    constexpr uint32 SHADOW_TECHNIQUES = 196911;
    constexpr uint32 PREMEDITATION = 343160;
    constexpr uint32 SHOT_IN_THE_DARK = 257505;
    constexpr uint32 DARK_SHADOW = 245687;
    constexpr uint32 SECRET_TECHNIQUE = 280719;
    constexpr uint32 SHURIKEN_TORNADO = 277925;
    constexpr uint32 FINALITY = 382511;
    constexpr uint32 SUBTERFUGE = 108208;
    constexpr uint32 NIGHTSTALKER = 14062;
    constexpr uint32 MASTER_OF_SHADOWS = 196976;
}

class TC_GAME_API PlayerbotRogueAction : public PlayerbotSpellAction
{
public:
    PlayerbotRogueAction(PlayerbotPlayerAI* ai, std::string const& name, uint32 spellId = 0)
        : PlayerbotSpellAction(ai, name, spellId) {}

protected:
    // Resource management
    uint32 GetEnergyAmount() const;
    uint32 GetComboPoints() const;
    uint32 GetComboPoints(Unit* target) const;
    bool HasEnoughEnergy(uint32 cost) const;
    bool HasMaxComboPoints() const;
    bool IsNearMaxComboPoints(uint32 threshold = 4) const;
    
    // Specialization detection
    bool isAssassination() const;
    bool isOutlaw() const;
    bool isSubtlety() const;
    
    // Stealth and positioning
    bool IsStealthed() const;
    bool IsBehindTarget(Unit* target) const;
    bool ShouldUseOpener() const;
    
    // DoT and buff management
    bool HasRuptureOnTarget(Unit* target) const;
    bool HasGarroteOnTarget(Unit* target) const;
    bool HasSliceAndDice() const;
    bool HasRollTheBonesBuffs() const;
    uint32 CountRollTheBonesBuffs() const;
    
    // Common utility
    bool IsInMeleeRange(Unit* target) const;
    bool ShouldInterrupt() const;
    bool NeedsHealing() const;
    Unit* FindBestTarget() const;
    uint32 CountNearbyEnemies(float range = 8.0f) const;
};

// Assassination Rogue Actions
class TC_GAME_API CastMutilate : public PlayerbotRogueAction
{
public:
    CastMutilate(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "mutilate", RogueSpells::MUTILATE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastEnvenom : public PlayerbotRogueAction
{
public:
    CastEnvenom(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "envenom", RogueSpells::ENVENOM) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastGarrote : public PlayerbotRogueAction
{
public:
    CastGarrote(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "garrote", RogueSpells::GARROTE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastRupture : public PlayerbotRogueAction
{
public:
    CastRupture(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "rupture", RogueSpells::RUPTURE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastVendetta : public PlayerbotRogueAction
{
public:
    CastVendetta(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "vendetta", RogueSpells::VENDETTA) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastDeathmark : public PlayerbotRogueAction
{
public:
    CastDeathmark(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "deathmark", RogueSpells::DEATHMARK) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastKingsbane : public PlayerbotRogueAction
{
public:
    CastKingsbane(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "kingsbane", RogueSpells::KINGSBANE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastShiv : public PlayerbotRogueAction
{
public:
    CastShiv(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "shiv", RogueSpells::SHIV) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastFanOfKnives : public PlayerbotRogueAction
{
public:
    CastFanOfKnives(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "fan of knives", RogueSpells::FAN_OF_KNIVES) {}
protected:
    Unit* GetSpellTarget() const override;
};

// Outlaw Rogue Actions
class TC_GAME_API CastSinisterStrike : public PlayerbotRogueAction
{
public:
    CastSinisterStrike(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "sinister strike", RogueSpells::SINISTER_STRIKE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastDispatch : public PlayerbotRogueAction
{
public:
    CastDispatch(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "dispatch", RogueSpells::DISPATCH) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastPistolShot : public PlayerbotRogueAction
{
public:
    CastPistolShot(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "pistol shot", RogueSpells::PISTOL_SHOT) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastBetweenTheEyes : public PlayerbotRogueAction
{
public:
    CastBetweenTheEyes(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "between the eyes", RogueSpells::BETWEEN_THE_EYES) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastRollTheBones : public PlayerbotRogueAction
{
public:
    CastRollTheBones(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "roll the bones", RogueSpells::ROLL_THE_BONES) {}
};

class TC_GAME_API CastAdrenalineRush : public PlayerbotRogueAction
{
public:
    CastAdrenalineRush(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "adrenaline rush", RogueSpells::ADRENALINE_RUSH) {}
};

class TC_GAME_API CastBladeFlurry : public PlayerbotRogueAction
{
public:
    CastBladeFlurry(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "blade flurry", RogueSpells::BLADE_FLURRY) {}
};

class TC_GAME_API CastKillingSpree : public PlayerbotRogueAction
{
public:
    CastKillingSpree(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "killing spree", RogueSpells::KILLING_SPREE) {}
protected:
    Unit* GetSpellTarget() const override;
};

// Subtlety Rogue Actions
class TC_GAME_API CastBackstab : public PlayerbotRogueAction
{
public:
    CastBackstab(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "backstab", RogueSpells::BACKSTAB) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastEviscerate : public PlayerbotRogueAction
{
public:
    CastEviscerate(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "eviscerate", RogueSpells::EVISCERATE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastShadowstrike : public PlayerbotRogueAction
{
public:
    CastShadowstrike(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "shadowstrike", RogueSpells::SHADOWSTRIKE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastShadowDance : public PlayerbotRogueAction
{
public:
    CastShadowDance(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "shadow dance", RogueSpells::SHADOW_DANCE) {}
};

class TC_GAME_API CastSymbolsOfDeath : public PlayerbotRogueAction
{
public:
    CastSymbolsOfDeath(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "symbols of death", RogueSpells::SYMBOLS_OF_DEATH) {}
};

class TC_GAME_API CastShadowClone : public PlayerbotRogueAction
{
public:
    CastShadowClone(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "shadow clone", RogueSpells::SHADOW_CLONE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastBlackPowder : public PlayerbotRogueAction
{
public:
    CastBlackPowder(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "black powder", RogueSpells::BLACK_POWDER) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastShurikenStorm : public PlayerbotRogueAction
{
public:
    CastShurikenStorm(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "shuriken storm", RogueSpells::SHURIKEN_STORM) {}
protected:
    Unit* GetSpellTarget() const override;
};

// Shared utility actions
class TC_GAME_API CastStealth : public PlayerbotRogueAction
{
public:
    CastStealth(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "stealth", RogueSpells::STEALTH) {}
};

class TC_GAME_API CastVanish : public PlayerbotRogueAction
{
public:
    CastVanish(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "vanish", RogueSpells::VANISH) {}
};

class TC_GAME_API CastKick : public PlayerbotRogueAction
{
public:
    CastKick(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "kick", RogueSpells::KICK) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastFeint : public PlayerbotRogueAction
{
public:
    CastFeint(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "feint", RogueSpells::FEINT) {}
};

class TC_GAME_API CastCloakOfShadows : public PlayerbotRogueAction
{
public:
    CastCloakOfShadows(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "cloak of shadows", RogueSpells::CLOAK_OF_SHADOWS) {}
};

class TC_GAME_API CastEvasion : public PlayerbotRogueAction
{
public:
    CastEvasion(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "evasion", RogueSpells::EVASION) {}
};

class TC_GAME_API CastCrimsonVial : public PlayerbotRogueAction
{
public:
    CastCrimsonVial(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "crimson vial", RogueSpells::CRIMSON_VIAL) {}
};

class TC_GAME_API CastSprint : public PlayerbotRogueAction
{
public:
    CastSprint(PlayerbotPlayerAI* ai) : PlayerbotRogueAction(ai, "sprint", RogueSpells::SPRINT) {}
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTROGUEACTIONS_H