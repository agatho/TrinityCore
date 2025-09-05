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

#ifndef TRINITY_PLAYERBOTHUNTERACTIONS_H
#define TRINITY_PLAYERBOTHUNTERACTIONS_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"

// Hunter Spell IDs (The War Within 11.2 - Current Retail WoW)
namespace HunterSpells
{
    // Core shared abilities
    constexpr uint32 HUNTERS_MARK = 257284;
    constexpr uint32 CALL_OF_THE_WILD = 359844;
    constexpr uint32 MISDIRECTION = 34477;
    constexpr uint32 FEIGN_DEATH = 5384;
    
    // Beast Mastery abilities
    constexpr uint32 KILL_COMMAND = 34026;        // BM primary ability
    constexpr uint32 BARBED_SHOT = 217200;        // BM core ability - pet frenzy
    constexpr uint32 COBRA_SHOT = 193455;         // BM focus generator
    constexpr uint32 BESTIAL_WRATH = 19574;       // BM major cooldown
    constexpr uint32 INTIMIDATION = 19577;        // Pet stun
    constexpr uint32 ASPECT_OF_THE_WILD = 193530; // BM damage boost
    
    // Marksmanship abilities
    constexpr uint32 AIMED_SHOT = 19434;          // MM primary ability
    constexpr uint32 RAPID_FIRE = 257044;         // MM core ability
    constexpr uint32 ARCANE_SHOT = 185358;        // MM focus spender
    constexpr uint32 MULTI_SHOT = 257620;         // MM AoE ability
    constexpr uint32 TRUESHOT = 288613;           // MM major cooldown
    constexpr uint32 STEADY_SHOT = 56641;         // MM filler
    constexpr uint32 DOUBLE_TAP = 260402;         // MM burst ability
    
    // Survival abilities
    constexpr uint32 RAPTOR_STRIKE = 186270;      // Survival melee attack
    constexpr uint32 WILDFIRE_BOMB = 259495;      // Survival explosive
    constexpr uint32 MONGOOSE_BITE = 265888;      // Survival combo ability
    constexpr uint32 EXPLOSIVE_SHOT = 212431;     // Survival ranged ability
    constexpr uint32 KILL_SHOT = 53351;           // Execute ability
    constexpr uint32 COORDINATED_ASSAULT = 266779; // Survival major cooldown
    constexpr uint32 SERPENT_STING = 271788;      // Survival DoT
    
    // Pet management
    constexpr uint32 CALL_PET = 883;
    constexpr uint32 DISMISS_PET = 2641;
    constexpr uint32 REVIVE_PET = 982;
    constexpr uint32 MEND_PET = 136;
    
    // Traps and utility
    constexpr uint32 FREEZING_TRAP = 187650;
    constexpr uint32 TAR_TRAP = 187698;
    constexpr uint32 EXPLOSIVE_TRAP = 191433;
}

/**
 * @brief Base class for hunter-specific actions
 */
class TC_GAME_API PlayerbotHunterAction : public PlayerbotAction
{
public:
    PlayerbotHunterAction(PlayerbotPlayerAI* ai, std::string const& name)
        : PlayerbotAction(ai, name) {}

protected:
    // Modern hunter mechanics
    bool HasActivePet() const;
    bool IsInRangedRange(Unit* target) const;
    uint32 GetFocusAmount() const;
    bool HasHuntersMark(Unit* target) const;
    
    // Pet management
    Unit* GetPet() const;
    bool IsPetAlive() const;
    bool IsPetInCombat() const;
    
    // Specialization detection
    bool IsBeastMastery() const;
    bool IsMarksmanship() const;
    bool IsSurvival() const;
    
    // Focus and resource management
    bool HasEnoughFocus(uint32 focusCost) const;
    float GetTargetHealthPercent(Unit* target) const;
    uint32 CountNearbyEnemies(float range = 10.0f) const;
    
    // Buff and debuff tracking
    bool HasBestialWrath() const;          // BM buff tracking
    bool HasPetFrenzy() const;             // BM pet frenzy from Barbed Shot
    bool HasPreciseShots() const;          // MM buff tracking
    bool HasTipOfTheSpear() const;         // Survival buff tracking
};

// Beast Mastery Core Abilities
class TC_GAME_API PlayerbotKillCommandAction : public PlayerbotHunterAction
{
public:
    PlayerbotKillCommandAction(PlayerbotPlayerAI* ai) 
        : PlayerbotHunterAction(ai, "kill command") {}
    
    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
    virtual float GetRelevance() const;
};

class TC_GAME_API PlayerbotBarbedShotAction : public PlayerbotHunterAction
{
public:
    PlayerbotBarbedShotAction(PlayerbotPlayerAI* ai) 
        : PlayerbotHunterAction(ai, "barbed shot") {}
    
    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
    virtual float GetRelevance() const;
    
private:
    uint32 GetBarbedShotCharges() const;
};

class TC_GAME_API PlayerbotCobraShotAction : public PlayerbotHunterAction
{
public:
    PlayerbotCobraShotAction(PlayerbotPlayerAI* ai) 
        : PlayerbotHunterAction(ai, "cobra shot") {}
    
    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
    virtual float GetRelevance() const;
};

class TC_GAME_API PlayerbotBestialWrathAction : public PlayerbotHunterAction
{
public:
    PlayerbotBestialWrathAction(PlayerbotPlayerAI* ai) 
        : PlayerbotHunterAction(ai, "bestial wrath") {}
    
    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
    virtual float GetRelevance() const;
};

// Marksmanship Core Abilities
class TC_GAME_API PlayerbotAimedShotAction : public PlayerbotHunterAction
{
public:
    PlayerbotAimedShotAction(PlayerbotPlayerAI* ai) 
        : PlayerbotHunterAction(ai, "aimed shot") {}
    
    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
    virtual float GetRelevance() const;
    
private:
    bool HasStreamlineBuff() const;
};

class TC_GAME_API PlayerbotRapidFireAction : public PlayerbotHunterAction
{
public:
    PlayerbotRapidFireAction(PlayerbotPlayerAI* ai) 
        : PlayerbotHunterAction(ai, "rapid fire") {}
    
    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
    virtual float GetRelevance() const;
};

class TC_GAME_API PlayerbotArcaneShotAction : public PlayerbotHunterAction
{
public:
    PlayerbotArcaneShotAction(PlayerbotPlayerAI* ai) 
        : PlayerbotHunterAction(ai, "arcane shot") {}
    
    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
    virtual float GetRelevance() const;
};

class TC_GAME_API PlayerbotTrueshotAction : public PlayerbotHunterAction
{
public:
    PlayerbotTrueshotAction(PlayerbotPlayerAI* ai) 
        : PlayerbotHunterAction(ai, "trueshot") {}
    
    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
    virtual float GetRelevance() const;
};

// Survival Core Abilities
class TC_GAME_API PlayerbotRaptorStrikeAction : public PlayerbotHunterAction
{
public:
    PlayerbotRaptorStrikeAction(PlayerbotPlayerAI* ai) 
        : PlayerbotHunterAction(ai, "raptor strike") {}
    
    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
    virtual float GetRelevance() const;
};

class TC_GAME_API PlayerbotWildfireBombAction : public PlayerbotHunterAction
{
public:
    PlayerbotWildfireBombAction(PlayerbotPlayerAI* ai) 
        : PlayerbotHunterAction(ai, "wildfire bomb") {}
    
    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
    virtual float GetRelevance() const;
};

class TC_GAME_API PlayerbotCoordinatedAssaultAction : public PlayerbotHunterAction
{
public:
    PlayerbotCoordinatedAssaultAction(PlayerbotPlayerAI* ai) 
        : PlayerbotHunterAction(ai, "coordinated assault") {}
    
    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
    virtual float GetRelevance() const;
};

// Shared Hunter Abilities
class TC_GAME_API PlayerbotHuntersMarkAction : public PlayerbotHunterAction
{
public:
    PlayerbotHuntersMarkAction(PlayerbotPlayerAI* ai) 
        : PlayerbotHunterAction(ai, "hunters mark") {}
    
    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
    virtual float GetRelevance() const;
};

class TC_GAME_API PlayerbotKillShotAction : public PlayerbotHunterAction
{
public:
    PlayerbotKillShotAction(PlayerbotPlayerAI* ai) 
        : PlayerbotHunterAction(ai, "kill shot") {}
    
    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
    virtual float GetRelevance() const;
    
private:
    bool IsTargetInExecuteRange(Unit* target) const; // 20% health
};

// Pet Management
class TC_GAME_API PlayerbotCallPetAction : public PlayerbotHunterAction
{
public:
    PlayerbotCallPetAction(PlayerbotPlayerAI* ai) 
        : PlayerbotHunterAction(ai, "call pet") {}
    
    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTHUNTERACTIONS_H