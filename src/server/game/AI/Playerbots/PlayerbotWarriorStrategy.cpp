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

#ifdef WITH_PLAYERBOTS

#include "PlayerbotWarriorStrategy.h"
#include "PlayerbotWarriorActions.h"
#include "PlayerbotPlayerAI.h"
#include "Player.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Group.h"
#include "Log.h"

// PlayerbotWarriorStrategy Base Implementation
PlayerbotWarriorStrategy::PlayerbotWarriorStrategy(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotStrategy(ai, name)
{
}

void PlayerbotWarriorStrategy::InitializeActions()
{
    PlayerbotStrategy::InitializeActions();
    RegisterCommonActions();
}

void PlayerbotWarriorStrategy::InitializeTriggers()
{
    PlayerbotStrategy::InitializeTriggers();
    RegisterCommonTriggers();
}

void PlayerbotWarriorStrategy::RegisterCommonActions()
{
    // Modern shared utility actions
    RegisterAction("battle shout", std::make_unique<PlayerbotBattleShoutAction>(_ai));
    RegisterAction("execute", std::make_unique<PlayerbotExecuteAction>(_ai));
    RegisterAction("charge", std::make_unique<PlayerbotChargeAction>(_ai));
    
    // Interrupt
    RegisterAction("pummel", std::make_unique<PlayerbotPummelAction>(_ai));
}

void PlayerbotWarriorStrategy::RegisterCommonTriggers()
{
    // Battle shout buff maintenance
    RegisterTrigger("battle shout", [this]() { return ShouldUseBattleShout(); });
    
    // Demoralizing shout for damage reduction
    RegisterTrigger("demoralizing shout", [this]() { return ShouldUseDemoralizingShout(); });
    
    // Bloodrage for rage management
    RegisterTrigger("bloodrage", [this]() {
        Player* bot = _ai->GetBot();
        return bot && bot->GetPower(POWER_RAGE) < 20;
    });
    
    // Interrupt casting enemies
    RegisterTrigger("pummel", [this]() {
        // Look for casting enemies in range
        Unit* target = _ai->GetCurrentTarget();
        if (!target || !target->IsNonMeleeSpellCast(false))
            return false;
        
        float distance = _ai->GetBot()->GetDistance(target);
        return distance <= 5.0f; // Pummel range
    });
}

bool PlayerbotWarriorStrategy::ShouldUseBattleShout() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;

    // Check if bot needs battle shout
    if (!bot->HasAura(WarriorSpells::BATTLE_SHOUT) && 
        !bot->HasAura(WarriorSpells::COMMANDING_SHOUT))
        return true;

    // Check group members
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member || !member->IsAlive() || member->GetDistance(bot) > 30.0f)
                continue;

            if (!member->HasAura(WarriorSpells::BATTLE_SHOUT) && 
                !member->HasAura(WarriorSpells::COMMANDING_SHOUT))
                return true;
        }
    }

    return false;
}

bool PlayerbotWarriorStrategy::ShouldUseDemoralizingShout() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !_ai->IsInCombat())
        return false;

    // Count nearby enemies
    uint32 enemyCount = 0;
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck checker(bot, bot, 10.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, checker);
    Cell::VisitAllObjects(bot, searcher, 10.0f);

    for (Unit* target : targets)
    {
        if (target->IsAlive() && bot->IsValidAttackTarget(target))
            enemyCount++;
    }

    return enemyCount >= 2;
}

bool PlayerbotWarriorStrategy::IsInTankingRole() const
{
    return _ai->HasStrategy("tank") || _ai->HasStrategy("protection");
}

bool PlayerbotWarriorStrategy::IsInDamageRole() const
{
    return _ai->HasStrategy("dps") || _ai->HasStrategy("arms") || _ai->HasStrategy("fury");
}

// PlayerbotArmsWarriorStrategy Implementation
PlayerbotArmsWarriorStrategy::PlayerbotArmsWarriorStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotWarriorStrategy(ai, "arms")
{
}

void PlayerbotArmsWarriorStrategy::InitializeActions()
{
    PlayerbotWarriorStrategy::InitializeActions();
    RegisterArmsActions();
}

void PlayerbotArmsWarriorStrategy::InitializeTriggers()
{
    PlayerbotWarriorStrategy::InitializeTriggers();
    RegisterArmsTriggers();
}

void PlayerbotArmsWarriorStrategy::RegisterArmsActions()
{
    // Modern Arms combat abilities
    RegisterAction("colossus smash", std::make_unique<PlayerbotColossusSmashAction>(_ai));
    RegisterAction("mortal strike", std::make_unique<PlayerbotMortalStrikeAction>(_ai));
    RegisterAction("slam", std::make_unique<PlayerbotSlamAction>(_ai));
    RegisterAction("overpower", std::make_unique<PlayerbotOverpowerAction>(_ai));
    RegisterAction("rend", std::make_unique<PlayerbotRendAction>(_ai));
    RegisterAction("thunder clap", std::make_unique<PlayerbotDemoralizingShoutAction>(_ai));
}

void PlayerbotArmsWarriorStrategy::RegisterArmsTriggers()
{
    // Charge when enemy is out of melee range
    RegisterTrigger("charge", [this]() {
        Unit* target = _ai->GetCurrentTarget();
        if (!target)
            return false;
        
        float distance = _ai->GetBot()->GetDistance(target);
        return distance >= 8.0f && distance <= 25.0f && !_ai->IsInMeleeRange(target);
    });
    
    // Apply rend for DoT damage
    RegisterTrigger("rend", [this]() {
        Unit* target = _ai->GetCurrentTarget();
        if (!target || !_ai->IsInCombat())
            return false;
        
        return !target->HasAura(WarriorSpells::REND);
    });
    
    // Use mortal strike as primary ability
    RegisterTrigger("mortal strike", [this]() {
        return _ai->IsInCombat() && _ai->IsInMeleeRange(_ai->GetCurrentTarget());
    });
    
    // Use overpower after enemy dodge (simplified)
    RegisterTrigger("overpower", [this]() {
        return _ai->IsInCombat() && _ai->IsInMeleeRange(_ai->GetCurrentTarget());
    });
    
    // Use heroic strike with excess rage
    RegisterTrigger("heroic strike", [this]() {
        Player* bot = _ai->GetBot();
        if (!bot || !_ai->IsInCombat())
            return false;
        
        uint32 rage = bot->GetPower(POWER_RAGE);
        return rage >= 15 && _ai->IsInMeleeRange(_ai->GetCurrentTarget());
    });
}

float PlayerbotArmsWarriorStrategy::GetActionPriority(std::string const& actionName) const
{
    // Modern Arms rotation priorities based on The War Within
    if (actionName == "execute")
        return 0.95f;  // Highest priority in execute phase (35%)
    else if (actionName == "colossus smash")
        return 0.9f;   // Core ability - use on cooldown
    else if (actionName == "mortal strike")
        return 0.85f;  // Primary ability after Colossus Smash
    else if (actionName == "overpower")
        return 0.8f;   // High priority for rage generation
    else if (actionName == "slam")
        return 0.6f;   // Filler/rage dump
    else if (actionName == "rend")
        return 0.5f;   // Lower priority DoT
    else if (actionName == "charge")
        return 0.4f;   // Situational gap closer
    
    return PlayerbotWarriorStrategy::GetActionPriority(actionName);
}

// PlayerbotFuryWarriorStrategy Implementation
PlayerbotFuryWarriorStrategy::PlayerbotFuryWarriorStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotWarriorStrategy(ai, "fury")
{
}

void PlayerbotFuryWarriorStrategy::InitializeActions()
{
    PlayerbotWarriorStrategy::InitializeActions();
    RegisterFuryActions();
}

void PlayerbotFuryWarriorStrategy::InitializeTriggers()
{
    PlayerbotWarriorStrategy::InitializeTriggers();
    RegisterFuryTriggers();
}

void PlayerbotFuryWarriorStrategy::RegisterFuryActions()
{
    // Modern Fury combat abilities - Enrage-focused rotation
    RegisterAction("rampage", std::make_unique<PlayerbotRampageAction>(_ai));
    RegisterAction("raging blow", std::make_unique<PlayerbotRagingBlowAction>(_ai));
    RegisterAction("bloodthirst", std::make_unique<PlayerbotBloodthirstAction>(_ai));
    RegisterAction("recklessness", std::make_unique<PlayerbotRecklessnessAction>(_ai));
    RegisterAction("whirlwind", std::make_unique<PlayerbotWhirlwindAction>(_ai));
}

void PlayerbotFuryWarriorStrategy::RegisterFuryTriggers()
{
    // Use bloodthirst as primary ability
    RegisterTrigger("bloodthirst", [this]() {
        return _ai->IsInCombat() && _ai->IsInMeleeRange(_ai->GetCurrentTarget());
    });
    
    // Use whirlwind for AoE situations
    RegisterTrigger("whirlwind", [this]() {
        if (!_ai->IsInCombat())
            return false;
        
        Player* bot = _ai->GetBot();
        if (!bot)
            return false;
        
        // Count nearby enemies
        uint32 enemyCount = 0;
        std::list<Unit*> targets;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck checker(bot, bot, 8.0f);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, checker);
        Cell::VisitAllObjects(bot, searcher, 8.0f);

        for (Unit* target : targets)
        {
            if (target->IsAlive() && bot->IsValidAttackTarget(target))
                enemyCount++;
        }
        
        return enemyCount >= 3;
    });
}

float PlayerbotFuryWarriorStrategy::GetActionPriority(std::string const& actionName) const
{
    // Modern Fury rotation priorities - Enrage-focused
    if (actionName == "execute")
        return 0.95f;  // Highest priority in execute phase (20%)
    else if (actionName == "rampage")
        return 0.9f;   // Core ability - triggers Enrage
    else if (actionName == "raging blow")
        return 0.85f;  // High priority when Enraged  
    else if (actionName == "bloodthirst")
        return 0.8f;   // Rage generator + healing
    else if (actionName == "recklessness")
        return 0.75f;  // Major cooldown
    else if (actionName == "whirlwind")
        return 0.6f;   // AoE situations
    
    return PlayerbotWarriorStrategy::GetActionPriority(actionName);
}

// PlayerbotProtectionWarriorStrategy Implementation
PlayerbotProtectionWarriorStrategy::PlayerbotProtectionWarriorStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotWarriorStrategy(ai, "protection")
{
}

void PlayerbotProtectionWarriorStrategy::InitializeActions()
{
    PlayerbotWarriorStrategy::InitializeActions();
    RegisterProtectionActions();
}

void PlayerbotProtectionWarriorStrategy::InitializeTriggers()
{
    PlayerbotWarriorStrategy::InitializeTriggers();
    RegisterProtectionTriggers();
}

void PlayerbotProtectionWarriorStrategy::RegisterProtectionActions()
{
    // Modern Protection combat abilities
    RegisterAction("shield slam", std::make_unique<PlayerbotShieldSlamAction>(_ai));
    RegisterAction("revenge", std::make_unique<PlayerbotRevengeAction>(_ai));
    RegisterAction("ignore pain", std::make_unique<PlayerbotIgnorePainAction>(_ai));
    RegisterAction("avatar", std::make_unique<PlayerbotAvatarAction>(_ai));
    RegisterAction("taunt", std::make_unique<PlayerbotTauntAction>(_ai));
    RegisterAction("devastate", std::make_unique<PlayerbotSunderArmorAction>(_ai)); // Repurpose for Devastate
}

void PlayerbotProtectionWarriorStrategy::RegisterProtectionTriggers()
{
    // Apply sunder armor for threat and armor reduction
    RegisterTrigger("sunder armor", [this]() {
        Unit* target = _ai->GetCurrentTarget();
        if (!target || !_ai->IsInCombat())
            return false;
        
        // Check sunder armor stacks
        if (Aura* aura = target->GetAura(WarriorSpells::SUNDER_ARMOR))
            return aura->GetStackAmount() < 5;
        
        return true; // No sunder armor, apply it
    });
    
    // Use taunt to maintain aggro
    RegisterTrigger("taunt", [this]() {
        Unit* target = _ai->GetCurrentTarget();
        if (!target || !_ai->IsInCombat())
            return false;
        
        return target->GetVictim() != _ai->GetBot();
    });
    
    // Use revenge after blocking/parrying
    RegisterTrigger("revenge", [this]() {
        return _ai->IsInCombat() && _ai->IsInMeleeRange(_ai->GetCurrentTarget());
    });
    
    // Use shield slam for damage and threat
    RegisterTrigger("shield slam", [this]() {
        return _ai->IsInCombat() && _ai->IsInMeleeRange(_ai->GetCurrentTarget());
    });
}

float PlayerbotProtectionWarriorStrategy::GetActionPriority(std::string const& actionName) const
{
    if (ShouldPrioritizeThreat())
    {
        if (actionName == "taunt")
            return 0.95f;
        else if (actionName == "sunder armor")
            return 0.9f;
        else if (actionName == "shield slam")
            return 0.8f;
    }
    else if (ShouldPrioritizeSurvival())
    {
        if (actionName == "revenge")
            return 0.9f;
        else if (actionName == "shield slam")
            return 0.8f;
    }
    
    return PlayerbotWarriorStrategy::GetActionPriority(actionName);
}

bool PlayerbotProtectionWarriorStrategy::ShouldPrioritizeThreat() const
{
    Unit* target = _ai->GetCurrentTarget();
    if (!target)
        return false;
    
    // Prioritize threat if target is attacking someone else
    return target->GetVictim() != _ai->GetBot();
}

bool PlayerbotProtectionWarriorStrategy::ShouldPrioritizeSurvival() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    float healthPct = float(bot->GetHealth()) / float(bot->GetMaxHealth());
    return healthPct < 0.5f;
}

// PlayerbotWarriorStrategyFactory Implementation
std::unique_ptr<PlayerbotWarriorStrategy> PlayerbotWarriorStrategyFactory::CreateStrategy(
    PlayerbotPlayerAI* ai, 
    std::string const& strategyName)
{
    if (strategyName == "arms")
        return std::make_unique<PlayerbotArmsWarriorStrategy>(ai);
    else if (strategyName == "fury")
        return std::make_unique<PlayerbotFuryWarriorStrategy>(ai);
    else if (strategyName == "protection")
        return std::make_unique<PlayerbotProtectionWarriorStrategy>(ai);
    else
    {
        // Auto-determine specialization
        std::string autoSpec = DetermineSpecialization(ai);
        if (autoSpec == "arms")
            return std::make_unique<PlayerbotArmsWarriorStrategy>(ai);
        else if (autoSpec == "fury")
            return std::make_unique<PlayerbotFuryWarriorStrategy>(ai);
        else
            return std::make_unique<PlayerbotProtectionWarriorStrategy>(ai);
    }
}

std::vector<std::string> PlayerbotWarriorStrategyFactory::GetAvailableStrategies()
{
    return {"arms", "fury", "protection"};
}

std::string PlayerbotWarriorStrategyFactory::DetermineSpecialization(PlayerbotPlayerAI* ai)
{
    Player* bot = ai->GetBot();
    if (!bot)
        return "arms";

    // Simple specialization detection based on key spells
    if (bot->HasSpell(WarriorSpells::MORTAL_STRIKE))
        return "arms";
    else if (bot->HasSpell(WarriorSpells::BLOODTHIRST))
        return "fury";
    else if (bot->HasSpell(WarriorSpells::SHIELD_SLAM))
        return "protection";
    
    // Default to arms for low-level characters
    return "arms";
}

// PlayerbotWarriorCombatTrigger Implementation
PlayerbotWarriorCombatTrigger::PlayerbotWarriorCombatTrigger(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotTrigger(ai, name)
{
}

bool PlayerbotWarriorCombatTrigger::IsActive() const
{
    if (!_ai->IsInCombat())
        return false;
    
    return CheckCondition();
}

bool PlayerbotWarriorCombatTrigger::IsInCorrectStance(uint32 stanceSpellId) const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(stanceSpellId);
}

bool PlayerbotWarriorCombatTrigger::HasEnoughRage(uint32 rageCost) const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    return bot->GetPower(POWER_RAGE) >= rageCost;
}

Unit* PlayerbotWarriorCombatTrigger::GetBestTarget() const
{
    return _ai->GetCurrentTarget();
}

// PlayerbotWarriorLowHealthTrigger Implementation
PlayerbotWarriorLowHealthTrigger::PlayerbotWarriorLowHealthTrigger(PlayerbotPlayerAI* ai, float threshold)
    : PlayerbotWarriorCombatTrigger(ai, "low health"), _healthThreshold(threshold)
{
}

bool PlayerbotWarriorLowHealthTrigger::CheckCondition() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    float healthPct = float(bot->GetHealth()) / float(bot->GetMaxHealth());
    return healthPct <= _healthThreshold;
}

// PlayerbotWarriorInterruptTrigger Implementation
PlayerbotWarriorInterruptTrigger::PlayerbotWarriorInterruptTrigger(PlayerbotPlayerAI* ai)
    : PlayerbotWarriorCombatTrigger(ai, "interrupt")
{
}

bool PlayerbotWarriorInterruptTrigger::CheckCondition() const
{
    Unit* target = FindInterruptTarget();
    return target && ShouldInterruptSpell(target, nullptr);
}

Unit* PlayerbotWarriorInterruptTrigger::FindInterruptTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;

    // Check current target first
    Unit* currentTarget = _ai->GetCurrentTarget();
    if (currentTarget && currentTarget->IsNonMeleeSpellCast(false))
        return currentTarget;

    // Search for nearby casting enemies
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck checker(bot, bot, 5.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, checker);
    Cell::VisitAllObjects(bot, searcher, 5.0f);

    for (Unit* target : targets)
    {
        if (target->IsNonMeleeSpellCast(false))
            return target;
    }

    return nullptr;
}

bool PlayerbotWarriorInterruptTrigger::ShouldInterruptSpell(Unit* caster, SpellInfo const* spellInfo) const
{
    if (!caster || !caster->IsNonMeleeSpellCast(false))
        return false;

    // For now, interrupt all spells - could be made more selective
    return true;
}

// PlayerbotWarriorStanceTrigger Implementation
PlayerbotWarriorStanceTrigger::PlayerbotWarriorStanceTrigger(PlayerbotPlayerAI* ai, uint32 requiredStance, std::string const& name)
    : PlayerbotWarriorCombatTrigger(ai, name), _requiredStanceSpellId(requiredStance)
{
}

bool PlayerbotWarriorStanceTrigger::CheckCondition() const
{
    return ShouldSwitchStance();
}

bool PlayerbotWarriorStanceTrigger::ShouldSwitchStance() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    return !bot->HasAura(_requiredStanceSpellId);
}

// PlayerbotWarriorThreatTrigger Implementation
PlayerbotWarriorThreatTrigger::PlayerbotWarriorThreatTrigger(PlayerbotPlayerAI* ai)
    : PlayerbotWarriorCombatTrigger(ai, "threat")
{
}

bool PlayerbotWarriorThreatTrigger::CheckCondition() const
{
    return ShouldTaunt();
}

bool PlayerbotWarriorThreatTrigger::HasThreatOnTarget(Unit* target) const
{
    if (!target)
        return false;
    
    Player* bot = _ai->GetBot();
    return target->GetVictim() == bot;
}

bool PlayerbotWarriorThreatTrigger::ShouldTaunt() const
{
    Unit* target = FindTauntTarget();
    return target != nullptr;
}

Unit* PlayerbotWarriorThreatTrigger::FindTauntTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;

    // Look for enemies attacking group members
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member || member == bot || !member->IsAlive())
                continue;

            if (Unit* attacker = member->GetAttackerForHelper())
            {
                if (attacker->IsValidAttackTarget(bot) && !HasThreatOnTarget(attacker))
                    return attacker;
            }
        }
    }

    return nullptr;
}

// PlayerbotWarriorRageTrigger Implementation
PlayerbotWarriorRageTrigger::PlayerbotWarriorRageTrigger(PlayerbotPlayerAI* ai, uint32 rageThreshold)
    : PlayerbotWarriorCombatTrigger(ai, "rage"), _rageThreshold(rageThreshold)
{
}

bool PlayerbotWarriorRageTrigger::CheckCondition() const
{
    return IsRageLow();
}

bool PlayerbotWarriorRageTrigger::IsRageLow() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    return bot->GetPower(POWER_RAGE) < _rageThreshold;
}

#endif // WITH_PLAYERBOTS