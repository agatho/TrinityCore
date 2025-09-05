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

#ifndef TRINITY_PLAYERBOTWARRIORACTIONS_H
#define TRINITY_PLAYERBOTWARRIORACTIONS_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCombatActions.h"

// Warrior Spell IDs (The War Within 11.2 - Current Retail WoW)
namespace WarriorSpells
{
    // Modern stance system (simplified, mostly passive)
    constexpr uint32 DEFENSIVE_STANCE = 71;
    constexpr uint32 BERSERKER_STANCE = 2458;
    
    // Core Arms abilities
    constexpr uint32 MORTAL_STRIKE = 12294;
    constexpr uint32 COLOSSUS_SMASH = 167105;  // Modern Arms core ability
    constexpr uint32 OVERPOWER = 7384;
    constexpr uint32 EXECUTE = 5308;  // Modern execute (35% threshold for Arms)
    constexpr uint32 SLAM = 1464;
    constexpr uint32 REND = 772;
    constexpr uint32 THUNDER_CLAP = 6343;
    constexpr uint32 CHARGE = 100;
    constexpr uint32 BLADESTORM = 46924;
    
    // Core Fury abilities 
    constexpr uint32 RAMPAGE = 184367;  // Modern Fury Rampage spell ID
    constexpr uint32 BLOODTHIRST = 23881;
    constexpr uint32 RAGING_BLOW = 85288;  // Modern Fury core ability
    constexpr uint32 WHIRLWIND = 1680;
    constexpr uint32 ENRAGE = 184361;  // Critical Fury buff
    constexpr uint32 RECKLESSNESS = 1719;  // Major Fury cooldown
    
    // Core Protection abilities
    constexpr uint32 SHIELD_SLAM = 23922;
    constexpr uint32 REVENGE = 6572;
    constexpr uint32 DEVASTATE = 20243;
    constexpr uint32 TAUNT = 355;
    constexpr uint32 SHIELD_WALL = 871;
    constexpr uint32 IGNORE_PAIN = 190456;  // Modern Protection defensive
    constexpr uint32 AVATAR = 107574;  // Modern Protection cooldown
    
    // Utility and support abilities
    constexpr uint32 PUMMEL = 6552;  // Interrupt
    constexpr uint32 BATTLE_SHOUT = 6673;
    constexpr uint32 COMMANDING_SHOUT = 469;
    constexpr uint32 INTIMIDATING_SHOUT = 5246;
    constexpr uint32 DEMORALIZING_SHOUT = 1160;
    
    // Legacy abilities (may not be available in current expansion)
    constexpr uint32 HEROIC_STRIKE = 78;      // Removed in modern WoW
    constexpr uint32 SUNDER_ARMOR = 7386;     // Replaced by other abilities
    constexpr uint32 BLOODRAGE = 2687;        // Removed in modern WoW
    constexpr uint32 HAMSTRING = 1715;        // Less commonly used
}

/**
 * @brief Base class for warrior-specific actions
 */
class TC_GAME_API PlayerbotWarriorAction : public PlayerbotCombatSpellAction
{
public:
    PlayerbotWarriorAction(PlayerbotPlayerAI* ai, std::string const& name, uint32 spellId)
        : PlayerbotCombatSpellAction(ai, name, spellId) {}

protected:
    // Modern warrior mechanics (simplified stance system)
    bool IsInDefensiveStance() const;
    bool IsInBerserkerStance() const;
    bool IsInBattleStance() const;
    bool SwitchToStance(uint32 stanceSpellId);
    bool HasShield() const;
    bool HasTwoHandedWeapon() const;
    
    // Modern resource and buff management
    bool HasEnrage() const;              // Critical for Fury warriors
    uint32 GetRageAmount() const;        // Current rage amount
    bool IsExecuteRange(Unit* target) const;  // 35% for Arms, 20% for Fury
    bool HasColossusSmash(Unit* target) const;  // Arms debuff tracking
    
    // Utility methods for modern rotations
    float GetTargetHealthPercent(Unit* target) const;
    uint32 CountNearbyEnemies(float range = 8.0f) const;
};

// Modern Arms Specialization Actions
class TC_GAME_API PlayerbotColossusSmashAction : public PlayerbotWarriorAction
{
public:
    PlayerbotColossusSmashAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "colossus smash", WarriorSpells::COLOSSUS_SMASH) {}
    
    virtual bool isUseful() override;
    virtual float GetRelevance() const override;
};

class TC_GAME_API PlayerbotExecuteAction : public PlayerbotWarriorAction
{
public:
    PlayerbotExecuteAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "execute", WarriorSpells::EXECUTE) {}
    
    virtual bool isUseful() override;
    virtual float GetRelevance() const override;
    
protected:
    virtual bool IsExecutePhase(Unit* target) const;
};

class TC_GAME_API PlayerbotSlamAction : public PlayerbotWarriorAction
{
public:
    PlayerbotSlamAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "slam", WarriorSpells::SLAM) {}
    
    virtual bool isUseful() override;
    virtual float GetRelevance() const override;
};

class TC_GAME_API PlayerbotRendAction : public PlayerbotWarriorAction
{
public:
    PlayerbotRendAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "rend", WarriorSpells::REND) {}
    
    virtual bool isUseful() override;
    virtual float GetRelevance() const override;
};

class TC_GAME_API PlayerbotChargeAction : public PlayerbotWarriorAction
{
public:
    PlayerbotChargeAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "charge", WarriorSpells::CHARGE) {}
    
    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
    virtual float GetRelevance() const override;

private:
    bool IsInChargeRange(Unit* target) const;
};

class TC_GAME_API PlayerbotMortalStrikeAction : public PlayerbotWarriorAction
{
public:
    PlayerbotMortalStrikeAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "mortal strike", WarriorSpells::MORTAL_STRIKE) {}
    
    virtual bool isUseful() override;
    virtual bool isPossible() override;
};

class TC_GAME_API PlayerbotOverpowerAction : public PlayerbotWarriorAction
{
public:
    PlayerbotOverpowerAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "overpower", WarriorSpells::OVERPOWER) {}
    
    virtual bool isUseful() override;
    virtual bool isPossible() override;
};

// Combat Actions - Protection Specialization
class TC_GAME_API PlayerbotSunderArmorAction : public PlayerbotWarriorAction
{
public:
    PlayerbotSunderArmorAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "sunder armor", WarriorSpells::SUNDER_ARMOR) {}
    
    virtual bool isUseful() override;
    virtual float GetRelevance() const override;

private:
    uint32 GetSunderArmorStacks(Unit* target) const;
};

class TC_GAME_API PlayerbotTauntAction : public PlayerbotWarriorAction
{
public:
    PlayerbotTauntAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "taunt", WarriorSpells::TAUNT) {}
    
    virtual bool isUseful() override;
    virtual bool isPossible() override;

protected:
    virtual Unit* GetSpellTarget() const override;
};

class TC_GAME_API PlayerbotRevengeAction : public PlayerbotWarriorAction
{
public:
    PlayerbotRevengeAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "revenge", WarriorSpells::REVENGE) {}
    
    virtual bool isUseful() override;
    virtual bool isPossible() override;
};

class TC_GAME_API PlayerbotShieldSlamAction : public PlayerbotWarriorAction
{
public:
    PlayerbotShieldSlamAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "shield slam", WarriorSpells::SHIELD_SLAM) {}
    
    virtual bool isUseful() override;
    virtual bool isPossible() override;
};

// Modern Protection Core Abilities
class TC_GAME_API PlayerbotIgnorePainAction : public PlayerbotWarriorAction
{
public:
    PlayerbotIgnorePainAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "ignore pain", WarriorSpells::IGNORE_PAIN) {}
    
    virtual bool isUseful() override;
    virtual bool isPossible() override;
    virtual float GetRelevance() const override;
};

class TC_GAME_API PlayerbotAvatarAction : public PlayerbotWarriorAction
{
public:
    PlayerbotAvatarAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "avatar", WarriorSpells::AVATAR) {}
    
    virtual bool isUseful() override;
    virtual bool isPossible() override;
    virtual float GetRelevance() const override;
};

// Combat Actions - Fury Specialization
class TC_GAME_API PlayerbotBloodthirstAction : public PlayerbotWarriorAction
{
public:
    PlayerbotBloodthirstAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "bloodthirst", WarriorSpells::BLOODTHIRST) {}
    
    virtual bool isUseful() override;
    virtual bool isPossible() override;
};

class TC_GAME_API PlayerbotWhirlwindAction : public PlayerbotWarriorAction
{
public:
    PlayerbotWhirlwindAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "whirlwind", WarriorSpells::WHIRLWIND) {}
    
    virtual bool isUseful() override;
    virtual bool isPossible() override;
    virtual ThreatType GetThreatType() const override { return ThreatType::AOE; }
};

// Modern Fury Core Abilities
class TC_GAME_API PlayerbotRampageAction : public PlayerbotWarriorAction
{
public:
    PlayerbotRampageAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "rampage", WarriorSpells::RAMPAGE) {}
    
    virtual bool isUseful() override;
    virtual bool isPossible() override;
    virtual float GetRelevance() const override;
};

class TC_GAME_API PlayerbotRagingBlowAction : public PlayerbotWarriorAction
{
public:
    PlayerbotRagingBlowAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "raging blow", WarriorSpells::RAGING_BLOW) {}
    
    virtual bool isUseful() override;
    virtual bool isPossible() override;
    virtual float GetRelevance() const override;
};

class TC_GAME_API PlayerbotRecklessnessAction : public PlayerbotWarriorAction
{
public:
    PlayerbotRecklessnessAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "recklessness", WarriorSpells::RECKLESSNESS) {}
    
    virtual bool isUseful() override;
    virtual bool isPossible() override;
    virtual float GetRelevance() const override;
};

// Utility Actions
class TC_GAME_API PlayerbotBattleShoutAction : public PlayerbotWarriorAction
{
public:
    PlayerbotBattleShoutAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "battle shout", WarriorSpells::BATTLE_SHOUT) {}
    
    virtual bool isUseful() override;

protected:
    virtual Unit* GetSpellTarget() const override;

private:
    bool NeedsBattleShout() const;
};

class TC_GAME_API PlayerbotDemoralizingShoutAction : public PlayerbotWarriorAction
{
public:
    PlayerbotDemoralizingShoutAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "demoralizing shout", WarriorSpells::DEMORALIZING_SHOUT) {}
    
    virtual bool isUseful() override;
    virtual ThreatType GetThreatType() const override { return ThreatType::AOE; }
};

class TC_GAME_API PlayerbotBloodrageAction : public PlayerbotWarriorAction
{
public:
    PlayerbotBloodrageAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "bloodrage", WarriorSpells::BLOODRAGE) {}
    
    virtual bool isUseful() override;

protected:
    virtual Unit* GetSpellTarget() const override;

private:
    bool IsRageNeeded() const;
};

/**
 * @brief Warrior interrupt action
 */
class TC_GAME_API PlayerbotPummelAction : public PlayerbotWarriorAction
{
public:
    PlayerbotPummelAction(PlayerbotPlayerAI* ai) 
        : PlayerbotWarriorAction(ai, "pummel", WarriorSpells::PUMMEL) {}
    
    virtual bool Execute(PlayerbotEvent const& event) override;
    virtual bool isUseful() override;
    virtual bool isPossible() override;
    virtual float GetRelevance() const override;

protected:
    virtual Unit* GetSpellTarget() const override;

private:
    Unit* FindCastingTarget() const;
    bool ShouldInterrupt(Unit* target) const;
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTWARRIORACTIONS_H