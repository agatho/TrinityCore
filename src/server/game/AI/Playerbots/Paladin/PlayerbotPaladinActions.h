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

#ifndef TRINITY_PLAYERBOTPALADINACTIONS_H
#define TRINITY_PLAYERBOTPALADINACTIONS_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"

// Paladin Spell IDs (The War Within 11.2 - Current Retail WoW)
namespace PaladinSpells
{
    // Shared abilities
    constexpr uint32 JUDGMENT = 20271;
    constexpr uint32 CONSECRATION = 26573;
    constexpr uint32 DIVINE_SHIELD = 642;
    constexpr uint32 LAY_ON_HANDS = 633;
    constexpr uint32 DEVOTION_AURA = 465;
    constexpr uint32 WORD_OF_GLORY = 85673;
    
    // Retribution abilities
    constexpr uint32 CRUSADER_STRIKE = 35395;
    constexpr uint32 TEMPLAR_VERDICT = 85256;        // Holy Power spender
    constexpr uint32 FINAL_VERDICT = 383328;         // Enhanced Templar's Verdict
    constexpr uint32 DIVINE_STORM = 53385;           // AoE Holy Power spender
    constexpr uint32 BLADE_OF_JUSTICE = 184575;      // Holy Power generator
    constexpr uint32 HAMMER_OF_WRATH = 24275;        // Ranged/execute ability
    constexpr uint32 EXECUTION_SENTENCE = 343527;    // Major damage cooldown
    constexpr uint32 AVENGING_WRATH = 31884;         // DPS cooldown
    constexpr uint32 CRUSADE = 231895;               // Alternative to Avenging Wrath
    constexpr uint32 DIVINE_HAMMER = 198034;         // AoE ability
    constexpr uint32 WAKE_OF_ASHES = 255937;         // Holy Power generator + buff
    constexpr uint32 DIVINE_TOLL = 375576;           // AoE burst generator
    constexpr uint32 FINAL_RECKONING = 343721;       // AoE damage cooldown
    constexpr uint32 HAMMER_OF_LIGHT = 427453;       // Holy Power spender
    constexpr uint32 EXPURGATION = 383344;           // DoT effect
    
    // Protection abilities
    constexpr uint32 SHIELD_OF_THE_RIGHTEOUS = 53600; // Primary defensive ability
    constexpr uint32 AVENGERS_SHIELD = 31935;         // Ranged attack/interrupt
    constexpr uint32 BLESSED_HAMMER = 204019;         // Holy Power generator
    constexpr uint32 HAMMER_OF_THE_RIGHTEOUS = 53595; // Melee Holy Power generator
    constexpr uint32 GUARDIAN_OF_ANCIENT_KINGS = 86659; // Major defensive cooldown
    constexpr uint32 EYE_OF_TYR = 387174;             // Defensive cooldown
    constexpr uint32 ARDENT_DEFENDER = 31850;         // Emergency defensive
    constexpr uint32 BLESSING_OF_SPELLWARDING = 204018; // Spell immunity
    
    // Holy abilities
    constexpr uint32 HOLY_SHOCK = 20473;              // Core healing/damage spell
    constexpr uint32 HOLY_LIGHT = 635;                // Basic heal
    constexpr uint32 FLASH_OF_LIGHT = 19750;          // Fast heal
    constexpr uint32 LIGHT_OF_DAWN = 85222;           // AoE heal (Holy Power)
    constexpr uint32 BEACON_OF_LIGHT = 53563;         // Healing transfer
    constexpr uint32 DIVINE_FAVOR = 210294;           // Healing buff
    constexpr uint32 AURA_MASTERY = 31821;            // Aura enhancement
    constexpr uint32 BLESSING_OF_FREEDOM = 1044;      // Movement impairment removal
    constexpr uint32 BLESSING_OF_PROTECTION = 1022;   // Physical immunity
    constexpr uint32 CLEANSE = 4987;                   // Dispel debuffs
    
    // Utility and buffs
    constexpr uint32 BLESSING_OF_MIGHT = 19740;       // Attack power buff
    constexpr uint32 BLESSING_OF_WISDOM = 19742;      // Mana regeneration buff
    constexpr uint32 SEAL_OF_LIGHT = 20165;           // Weapon imbue (older expansions)
    constexpr uint32 RIGHTEOUS_FURY = 25780;          // Threat/damage modifier
}

/**
 * @brief Base class for paladin-specific actions
 */
class TC_GAME_API PlayerbotPaladinAction : public PlayerbotSpellAction
{
public:
    PlayerbotPaladinAction(PlayerbotPlayerAI* ai, std::string const& name, uint32 spellId = 0)
        : PlayerbotSpellAction(ai, name, spellId) {}

protected:
    // Holy Power management
    uint32 GetHolyPowerAmount() const;
    bool HasEnoughHolyPower(uint32 cost) const;
    bool ShouldSaveHolyPower() const;
    
    // Specialization detection
    bool IsRetribution() const;
    bool IsProtection() const;
    bool IsHoly() const;
    
    // Common utility
    bool IsInMeleeRange(Unit* target) const;
    bool HasDebuff(Unit* target, uint32 spellId) const;
    bool HasBuff(Unit* target, uint32 spellId) const;
    uint32 CountNearbyEnemies(float range = 8.0f) const;
    Unit* FindHealingTarget() const;
    Unit* GetTankTarget() const;
    
    // Resource management
    uint32 GetManaPercent() const;
    bool HasEnoughMana(uint32 manaCost) const;
    
    // Defensive utilities
    bool IsHealthLow(float threshold = 0.3f) const;
    bool IsGroupMemberLow(float threshold = 0.5f) const;
    bool NeedsHealing() const;
};

// Retribution Core Abilities
class TC_GAME_API PlayerbotJudgmentAction : public PlayerbotPaladinAction
{
public:
    PlayerbotJudgmentAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "judgment", PaladinSpells::JUDGMENT) {}
    
    bool isUseful() override;
    float GetRelevance() const;
};

class TC_GAME_API PlayerbotCrusaderStrikeAction : public PlayerbotPaladinAction
{
public:
    PlayerbotCrusaderStrikeAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "crusader strike", PaladinSpells::CRUSADER_STRIKE) {}
    
    bool isUseful() override;
    float GetRelevance() const;
};

class TC_GAME_API PlayerbotTemplarVerdictAction : public PlayerbotPaladinAction
{
public:
    PlayerbotTemplarVerdictAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "templar verdict", PaladinSpells::TEMPLAR_VERDICT) {}
    
    bool isUseful() override;
    float GetRelevance() const;
};

class TC_GAME_API PlayerbotFinalVerdictAction : public PlayerbotPaladinAction
{
public:
    PlayerbotFinalVerdictAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "final verdict", PaladinSpells::FINAL_VERDICT) {}
    
    bool isUseful() override;
    float GetRelevance() const;
};

class TC_GAME_API PlayerbotDivineStormAction : public PlayerbotPaladinAction
{
public:
    PlayerbotDivineStormAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "divine storm", PaladinSpells::DIVINE_STORM) {}
    
    bool isUseful() override;
    float GetRelevance() const;
};

class TC_GAME_API PlayerbotBladeOfJusticeAction : public PlayerbotPaladinAction
{
public:
    PlayerbotBladeOfJusticeAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "blade of justice", PaladinSpells::BLADE_OF_JUSTICE) {}
    
    bool isUseful() override;
    float GetRelevance() const;
private:
    bool HasExpurgation(Unit* target) const;
};

class TC_GAME_API PlayerbotHammerOfWrathAction : public PlayerbotPaladinAction
{
public:
    PlayerbotHammerOfWrathAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "hammer of wrath", PaladinSpells::HAMMER_OF_WRATH) {}
    
    bool isUseful() override;
    float GetRelevance() const;
private:
    bool IsTargetInExecuteRange(Unit* target) const; // 20% health
};

class TC_GAME_API PlayerbotExecutionSentenceAction : public PlayerbotPaladinAction
{
public:
    PlayerbotExecutionSentenceAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "execution sentence", PaladinSpells::EXECUTION_SENTENCE) {}
    
    bool isUseful() override;
    float GetRelevance() const;
};

class TC_GAME_API PlayerbotAvengingWrathAction : public PlayerbotPaladinAction
{
public:
    PlayerbotAvengingWrathAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "avenging wrath", PaladinSpells::AVENGING_WRATH) {}
    
    bool isUseful() override;
    float GetRelevance() const;
};

class TC_GAME_API PlayerbotWakeOfAshesAction : public PlayerbotPaladinAction
{
public:
    PlayerbotWakeOfAshesAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "wake of ashes", PaladinSpells::WAKE_OF_ASHES) {}
    
    bool isUseful() override;
    float GetRelevance() const;
};

class TC_GAME_API PlayerbotDivineTollAction : public PlayerbotPaladinAction
{
public:
    PlayerbotDivineTollAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "divine toll", PaladinSpells::DIVINE_TOLL) {}
    
    bool isUseful() override;
    float GetRelevance() const;
};

// Protection Core Abilities
class TC_GAME_API PlayerbotShieldOfTheRighteousAction : public PlayerbotPaladinAction
{
public:
    PlayerbotShieldOfTheRighteousAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "shield of the righteous", PaladinSpells::SHIELD_OF_THE_RIGHTEOUS) {}
    
    bool isUseful() override;
    float GetRelevance() const;
};

class TC_GAME_API PlayerbotAvengersShieldAction : public PlayerbotPaladinAction
{
public:
    PlayerbotAvengersShieldAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "avengers shield", PaladinSpells::AVENGERS_SHIELD) {}
    
    bool isUseful() override;
    float GetRelevance() const;
};

class TC_GAME_API PlayerbotBlessedHammerAction : public PlayerbotPaladinAction
{
public:
    PlayerbotBlessedHammerAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "blessed hammer", PaladinSpells::BLESSED_HAMMER) {}
    
    bool isUseful() override;
    float GetRelevance() const;
};

class TC_GAME_API PlayerbotGuardianOfAncientKingsAction : public PlayerbotPaladinAction
{
public:
    PlayerbotGuardianOfAncientKingsAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "guardian of ancient kings", PaladinSpells::GUARDIAN_OF_ANCIENT_KINGS) {}
    
    bool isUseful() override;
    float GetRelevance() const;
};

// Holy Core Abilities
class TC_GAME_API PlayerbotHolyShockAction : public PlayerbotPaladinAction
{
public:
    PlayerbotHolyShockAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "holy shock", PaladinSpells::HOLY_SHOCK) {}
    
    Unit* GetSpellTarget() const override;
    bool isUseful() override;
    float GetRelevance() const;
private:
    bool ShouldUseOffensively() const;
    Unit* FindBestHealingTarget() const;
};

class TC_GAME_API PlayerbotHolyLightAction : public PlayerbotPaladinAction
{
public:
    PlayerbotHolyLightAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "holy light", PaladinSpells::HOLY_LIGHT) {}
    
    Unit* GetSpellTarget() const override;
    bool isUseful() override;
    float GetRelevance() const;
};

class TC_GAME_API PlayerbotWordOfGloryAction : public PlayerbotPaladinAction
{
public:
    PlayerbotWordOfGloryAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "word of glory", PaladinSpells::WORD_OF_GLORY) {}
    
    Unit* GetSpellTarget() const override;
    bool isUseful() override;
    float GetRelevance() const;
};

class TC_GAME_API PlayerbotLightOfDawnAction : public PlayerbotPaladinAction
{
public:
    PlayerbotLightOfDawnAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "light of dawn", PaladinSpells::LIGHT_OF_DAWN) {}
    
    bool isUseful() override;
    float GetRelevance() const;
private:
    uint32 CountInjuredGroupMembers() const;
};

class TC_GAME_API PlayerbotBeaconOfLightAction : public PlayerbotPaladinAction
{
public:
    PlayerbotBeaconOfLightAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "beacon of light", PaladinSpells::BEACON_OF_LIGHT) {}
    
    Unit* GetSpellTarget() const override;
    bool isUseful() override;
    float GetRelevance() const;
private:
    Unit* FindBestBeaconTarget() const;
};

// Shared Utility Actions
class TC_GAME_API PlayerbotConsecrationAction : public PlayerbotPaladinAction
{
public:
    PlayerbotConsecrationAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "consecration", PaladinSpells::CONSECRATION) {}
    
    bool isUseful() override;
    float GetRelevance() const;
};

class TC_GAME_API PlayerbotDivineShieldAction : public PlayerbotPaladinAction
{
public:
    PlayerbotDivineShieldAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "divine shield", PaladinSpells::DIVINE_SHIELD) {}
    
    bool isUseful() override;
    float GetRelevance() const;
};

class TC_GAME_API PlayerbotLayOnHandsAction : public PlayerbotPaladinAction
{
public:
    PlayerbotLayOnHandsAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "lay on hands", PaladinSpells::LAY_ON_HANDS) {}
    
    Unit* GetSpellTarget() const override;
    bool isUseful() override;
    float GetRelevance() const;
private:
    Unit* FindEmergencyTarget() const;
};

class TC_GAME_API PlayerbotBlessingOfFreedomAction : public PlayerbotPaladinAction
{
public:
    PlayerbotBlessingOfFreedomAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "blessing of freedom", PaladinSpells::BLESSING_OF_FREEDOM) {}
    
    Unit* GetSpellTarget() const override;
    bool isUseful() override;
    float GetRelevance() const;
private:
    Unit* FindSlowedTarget() const;
};

class TC_GAME_API PlayerbotCleanseAction : public PlayerbotPaladinAction
{
public:
    PlayerbotCleanseAction(PlayerbotPlayerAI* ai) 
        : PlayerbotPaladinAction(ai, "cleanse", PaladinSpells::CLEANSE) {}
    
    Unit* GetSpellTarget() const override;
    bool isUseful() override;
    float GetRelevance() const;
private:
    Unit* FindDispelTarget() const;
    bool HasDispellableDebuff(Unit* target) const;
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTPALADINACTIONS_H