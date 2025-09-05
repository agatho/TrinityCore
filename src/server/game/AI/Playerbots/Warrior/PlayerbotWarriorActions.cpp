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

#include "PlayerbotWarriorActions.h"
#include "PlayerbotPlayerAI.h"
#include "PlayerbotCommon.h"
#include "Player.h"
#include "Item.h"
#include "SpellAuras.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Group.h"
#include "Log.h"

// PlayerbotWarriorAction Base Class Implementation
bool PlayerbotWarriorAction::IsInDefensiveStance() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(WarriorSpells::DEFENSIVE_STANCE);
}

bool PlayerbotWarriorAction::IsInBerserkerStance() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(WarriorSpells::BERSERKER_STANCE);
}

bool PlayerbotWarriorAction::HasEnrage() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(WarriorSpells::ENRAGE);
}

uint32 PlayerbotWarriorAction::GetRageAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_RAGE) : 0;
}

bool PlayerbotWarriorAction::IsExecuteRange(Unit* target) const
{
    if (!target)
        return false;
    
    float healthPct = GetTargetHealthPercent(target);
    
    // Modern execute thresholds: 35% for Arms, 20% for Fury/Protection
    if (_ai->GetBot()->HasSpell(WarriorSpells::MORTAL_STRIKE)) // Arms spec
        return healthPct <= 0.35f;
    else
        return healthPct <= 0.20f;
}

bool PlayerbotWarriorAction::HasColossusSmash(Unit* target) const
{
    return target && target->HasAura(WarriorSpells::COLOSSUS_SMASH);
}

float PlayerbotWarriorAction::GetTargetHealthPercent(Unit* target) const
{
    if (!target)
        return 0.0f;
    
    return float(target->GetHealth()) / float(target->GetMaxHealth());
}

uint32 PlayerbotWarriorAction::CountNearbyEnemies(float range) const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;

    uint32 count = 0;
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck checker(bot, bot, range);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, checker);
    Cell::VisitAllObjects(bot, searcher, range);

    for (Unit* target : targets)
    {
        if (target->IsAlive() && bot->IsValidAttackTarget(target))
            count++;
    }

    return count;
}

bool PlayerbotWarriorAction::HasShield() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;

    Item* offhand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    return offhand && offhand->GetTemplate()->GetInventoryType() == INVTYPE_SHIELD;
}

bool PlayerbotWarriorAction::HasTwoHandedWeapon() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;

    Item* mainhand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!mainhand)
        return false;

    uint32 inventoryType = mainhand->GetTemplate()->GetInventoryType();
    return inventoryType == INVTYPE_2HWEAPON;
}

bool PlayerbotWarriorAction::SwitchToStance(uint32 stanceSpellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;
    
    // Check if already in desired stance
    if (bot->HasAura(stanceSpellId))
        return true;
    
    // Cast the stance spell using proper spell casting
    return CastSpell(bot, false);
}

bool PlayerbotWarriorAction::IsInBattleStance() const
{
    // In modern WoW, there's no separate Battle Stance - it's baseline
    // Return true if not in Defensive or Berserker stance
    return !IsInDefensiveStance() && !IsInBerserkerStance();
}

// Modern Arms Combat Actions
bool PlayerbotColossusSmashAction::isUseful()
{
    Unit* target = GetCurrentTarget();
    if (!target || !IsInCombat())
        return false;

    // Use Colossus Smash on cooldown for 30% damage increase
    return !HasColossusSmash(target);
}

float PlayerbotColossusSmashAction::GetRelevance() const
{
    // Highest priority for Arms - enables burst window
    return 0.95f;
}

bool PlayerbotExecuteAction::isUseful()
{
    Unit* target = GetCurrentTarget();
    if (!target || !IsInCombat())
        return false;

    return IsExecuteRange(target) && GetRageAmount() >= 20;
}

bool PlayerbotExecuteAction::IsExecutePhase(Unit* target) const
{
    return IsExecuteRange(target);
}

float PlayerbotExecuteAction::GetRelevance() const
{
    Unit* target = GetCurrentTarget();
    if (!target || !IsExecuteRange(target))
        return 0.0f;
    
    // Very high priority in execute phase
    return 0.9f;
}

bool PlayerbotSlamAction::isUseful()
{
    Unit* target = GetCurrentTarget();
    if (!target || !IsInCombat())
        return false;

    // Use Slam as rage dump when other abilities are on cooldown
    return GetRageAmount() >= 20 && IsInMeleeRange(target);
}

float PlayerbotSlamAction::GetRelevance() const
{
    // Lower priority filler ability
    return 0.4f;
}


bool PlayerbotRendAction::isUseful()
{
    Unit* target = GetCurrentTarget();
    if (!target || !IsInCombat())
        return false;

    // Apply rend if target doesn't have it
    return !target->HasAura(WarriorSpells::REND);
}

float PlayerbotRendAction::GetRelevance() const
{
    Unit* target = GetCurrentTarget();
    if (!target)
        return 0.0f;

    // High relevance if target will live long enough
    float targetHealthPct = float(target->GetHealth()) / float(target->GetMaxHealth());
    return targetHealthPct > 0.7f ? 0.8f : 0.3f;
}

bool PlayerbotChargeAction::Execute(PlayerbotEvent const& event)
{
    Unit* target = GetSpellTarget();
    if (!target)
        return false;

    // Check if we're in charge range  
    if (!IsInChargeRange(target))
        return false;

    // Use base class spell casting
    return CastSpell(target);
}

bool PlayerbotChargeAction::isUseful()
{
    Unit* target = GetCurrentTarget();
    if (!target)
        return false;

    return IsInChargeRange(target) && !IsInMeleeRange(target);
}

float PlayerbotChargeAction::GetRelevance() const
{
    Unit* target = GetCurrentTarget();
    if (!target)
        return 0.0f;

    float distance = GetDistance(target);
    // Higher relevance when target is in perfect charge range
    if (distance >= 8.0f && distance <= 25.0f)
        return 0.9f;
    
    return 0.1f;
}

bool PlayerbotChargeAction::IsInChargeRange(Unit* target) const
{
    if (!target)
        return false;

    float distance = GetDistance(target);
    return distance >= 8.0f && distance <= 25.0f;
}

bool PlayerbotMortalStrikeAction::isUseful()
{
    return IsInCombat() && IsInMeleeRange(GetCurrentTarget());
}

bool PlayerbotMortalStrikeAction::isPossible()
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;

    // Requires Arms specialization (simplified check)
    return bot->HasSpell(WarriorSpells::MORTAL_STRIKE) && PlayerbotWarriorAction::isPossible();
}

bool PlayerbotOverpowerAction::isUseful()
{
    Unit* target = GetCurrentTarget();
    if (!target)
        return false;

    // Use overpower after enemy dodge (simplified - would need dodge tracking)
    return IsInMeleeRange(target);
}

bool PlayerbotOverpowerAction::isPossible()
{
    return PlayerbotWarriorAction::isPossible();
}

// Protection Combat Actions
bool PlayerbotSunderArmorAction::isUseful()
{
    Unit* target = GetCurrentTarget();
    if (!target || !IsInCombat())
        return false;

    // Apply sunder armor if stacks are low
    return GetSunderArmorStacks(target) < 5;
}

float PlayerbotSunderArmorAction::GetRelevance() const
{
    Unit* target = GetCurrentTarget();
    if (!target)
        return 0.0f;

    uint32 stacks = GetSunderArmorStacks(target);
    return (5 - stacks) / 5.0f; // Higher relevance with fewer stacks
}

uint32 PlayerbotSunderArmorAction::GetSunderArmorStacks(Unit* target) const
{
    if (!target)
        return 0;

    if (Aura* aura = target->GetAura(WarriorSpells::SUNDER_ARMOR))
        return aura->GetStackAmount();
    
    return 0;
}

bool PlayerbotTauntAction::isUseful()
{
    Unit* target = GetCurrentTarget();
    if (!target || !IsInDefensiveStance())
        return false;

    // Use taunt if enemy is not targeting us
    return target->GetVictim() != GetBot();
}

bool PlayerbotTauntAction::isPossible()
{
    return IsInDefensiveStance() && PlayerbotWarriorAction::isPossible();
}

Unit* PlayerbotTauntAction::GetSpellTarget() const
{
    // Find enemy attacking party members
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;

    if (Group* group = bot->GetGroup())
    {
        // TODO: Fix Group API - for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
        // {
        //     Player* member = itr->GetSource();
        //     if (!member || member == bot || !member->IsAlive())
        //         continue;

        //     if (Unit* attacker = member->GetAttackerForHelper())
        //     {
        //         if (attacker->IsValidAttackTarget(bot))
        //             return attacker;
        //     }
        // }
    }

    return GetCurrentTarget();
}

bool PlayerbotRevengeAction::isUseful()
{
    return IsInDefensiveStance() && IsInCombat() && HasShield();
}

bool PlayerbotRevengeAction::isPossible()
{
    return IsInDefensiveStance() && HasShield() && PlayerbotWarriorAction::isPossible();
}

bool PlayerbotShieldSlamAction::isUseful()
{
    return IsInDefensiveStance() && IsInCombat() && HasShield();
}

bool PlayerbotShieldSlamAction::isPossible()
{
    return IsInDefensiveStance() && HasShield() && PlayerbotWarriorAction::isPossible();
}

// Fury Combat Actions
bool PlayerbotBloodthirstAction::isUseful()
{
    return IsInBerserkerStance() && IsInCombat() && IsInMeleeRange(GetCurrentTarget());
}

bool PlayerbotBloodthirstAction::isPossible()
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;

    return bot->HasSpell(WarriorSpells::BLOODTHIRST) && PlayerbotWarriorAction::isPossible();
}

bool PlayerbotWhirlwindAction::isUseful()
{
    if (!IsInBerserkerStance() || !IsInCombat())
        return false;

    // Use whirlwind when surrounded by multiple enemies
    return CountNearbyEnemies() >= 3;
}

bool PlayerbotWhirlwindAction::isPossible()
{
    return IsInBerserkerStance() && PlayerbotWarriorAction::isPossible();
}

// Utility Actions
bool PlayerbotBattleShoutAction::isUseful()
{
    return NeedsBattleShout();
}

Unit* PlayerbotBattleShoutAction::GetSpellTarget() const
{
    return GetBot(); // Battle shout affects the caster
}

bool PlayerbotBattleShoutAction::NeedsBattleShout() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;

    // Check if bot or nearby allies need battle shout
    if (!bot->HasAura(WarriorSpells::BATTLE_SHOUT) && 
        !bot->HasAura(WarriorSpells::COMMANDING_SHOUT))
        return true;

    // TODO: Check group members for buff status
    // if (Group* group = bot->GetGroup())
    // {
    //     for (GroupReference const& itr : group->GetMembers())
    //     {
    //         Player* member = itr.GetSource();
    //         if (!member || !member->IsAlive())
    //             continue;
    //
    //         if (!member->HasAura(WarriorSpells::BATTLE_SHOUT) && 
    //             !member->HasAura(WarriorSpells::COMMANDING_SHOUT))
    //             return true;
    //     }
    // }

    return false;
}

bool PlayerbotDemoralizingShoutAction::isUseful()
{
    if (!IsInCombat())
        return false;

    // Use demoralizing shout when surrounded by enemies
    return CountNearbyEnemies() >= 2;
}

bool PlayerbotBloodrageAction::isUseful()
{
    return IsRageNeeded();
}

Unit* PlayerbotBloodrageAction::GetSpellTarget() const
{
    return GetBot(); // Bloodrage affects the caster
}

bool PlayerbotBloodrageAction::IsRageNeeded() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;

    uint32 rage = bot->GetPower(POWER_RAGE);
    return rage < 20; // Use bloodrage when rage is low
}

// Interrupt Action
bool PlayerbotPummelAction::Execute(PlayerbotEvent const& event)
{
    Unit* target = FindCastingTarget();
    if (!target)
        return false;

    // Switch to berserker stance if needed for Pummel
    if (!IsInBerserkerStance())
    {
        if (!SwitchToStance(WarriorSpells::BERSERKER_STANCE))
            return false;
    }

    // Use base class spell casting
    return CastSpell(target);
}

bool PlayerbotPummelAction::isUseful()
{
    Unit* target = FindCastingTarget();
    return target && ShouldInterrupt(target);
}

bool PlayerbotPummelAction::isPossible()
{
    return IsInBerserkerStance() && PlayerbotWarriorAction::isPossible();
}

float PlayerbotPummelAction::GetRelevance() const
{
    Unit* target = FindCastingTarget();
    if (!target)
        return 0.0f;

    // Very high relevance for interrupting dangerous spells
    return 0.95f;
}

Unit* PlayerbotPummelAction::GetSpellTarget() const
{
    return FindCastingTarget();
}

Unit* PlayerbotPummelAction::FindCastingTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;

    // First check current target
    Unit* currentTarget = GetCurrentTarget();
    if (currentTarget && currentTarget->IsNonMeleeSpellCast(false))
        return currentTarget;

    // Search for nearby casting enemies
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck checker(bot, bot, 5.0f); // Pummel range
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, checker);
    Cell::VisitAllObjects(bot, searcher, 5.0f);

    for (Unit* target : targets)
    {
        if (target->IsNonMeleeSpellCast(false) && ShouldInterrupt(target))
            return target;
    }

    return nullptr;
}

bool PlayerbotPummelAction::ShouldInterrupt(Unit* target) const
{
    if (!target || !target->IsNonMeleeSpellCast(false))
        return false;

    // Interrupt healing spells and dangerous damage spells
    Spell const* spell = target->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!spell)
        spell = target->GetCurrentSpell(CURRENT_CHANNELED_SPELL);

    if (!spell)
        return false;

    SpellInfo const* spellInfo = spell->GetSpellInfo();
    if (!spellInfo)
        return false;

    // Interrupt heals and high-damage spells
    return spellInfo->HasEffect(SPELL_EFFECT_HEAL) || 
           spellInfo->HasEffect(SPELL_EFFECT_SCHOOL_DAMAGE) ||
           spellInfo->GetMaxRange() > 5.0f; // Ranged spells are usually worth interrupting
}

// Modern Fury Combat Actions
bool PlayerbotRampageAction::isUseful()
{
    Unit* target = GetCurrentTarget();
    if (!target || !IsInCombat())
        return false;

    // Use Rampage to trigger Enrage - highest priority for Fury
    return GetRageAmount() >= 85 && !HasEnrage();
}

bool PlayerbotRampageAction::isPossible()
{
    return GetRageAmount() >= 85 && PlayerbotWarriorAction::isPossible();
}

float PlayerbotRampageAction::GetRelevance() const
{
    if (!HasEnrage())
        return 0.95f; // Highest priority to trigger Enrage
    
    return 0.7f; // Still high priority when Enrage is active
}

bool PlayerbotRagingBlowAction::isUseful()
{
    Unit* target = GetCurrentTarget();
    if (!target || !IsInCombat())
        return false;

    // Core Fury ability - use on cooldown when Enraged
    return IsInMeleeRange(target) && HasEnrage();
}

bool PlayerbotRagingBlowAction::isPossible()
{
    return GetRageAmount() >= 25 && PlayerbotWarriorAction::isPossible();
}

float PlayerbotRagingBlowAction::GetRelevance() const
{
    if (HasEnrage())
        return 0.8f; // High priority when Enraged
    
    return 0.5f; // Lower priority without Enrage
}

bool PlayerbotRecklessnessAction::isUseful()
{
    Unit* target = GetCurrentTarget();
    if (!target || !IsInCombat())
        return false;

    // Major cooldown - use when we have a good target and low Enrage uptime
    return !HasEnrage() && GetTargetHealthPercent(target) > 0.5f;
}

bool PlayerbotRecklessnessAction::isPossible()
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(WarriorSpells::RECKLESSNESS) && 
           PlayerbotWarriorAction::isPossible();
}

float PlayerbotRecklessnessAction::GetRelevance() const
{
    // High relevance for major DPS cooldown
    return 0.85f;
}

// Modern Protection Combat Actions
bool PlayerbotIgnorePainAction::isUseful()
{
    Player* bot = _ai->GetBot();
    if (!bot || !IsInCombat())
        return false;

    // Use Ignore Pain when taking damage and have rage
    float healthPct = float(bot->GetHealth()) / float(bot->GetMaxHealth());
    return healthPct < 0.8f && GetRageAmount() >= 40;
}

bool PlayerbotIgnorePainAction::isPossible()
{
    return GetRageAmount() >= 40 && PlayerbotWarriorAction::isPossible();
}

float PlayerbotIgnorePainAction::GetRelevance() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0.0f;
    
    float healthPct = float(bot->GetHealth()) / float(bot->GetMaxHealth());
    // Higher relevance when lower on health
    return 1.0f - healthPct;
}

bool PlayerbotAvatarAction::isUseful()
{
    if (!IsInCombat())
        return false;

    // Major Protection cooldown - use when facing multiple enemies or tough fights
    return CountNearbyEnemies(8.0f) >= 2;
}

bool PlayerbotAvatarAction::isPossible()
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(WarriorSpells::AVATAR) && 
           PlayerbotWarriorAction::isPossible();
}

float PlayerbotAvatarAction::GetRelevance() const
{
    uint32 enemyCount = CountNearbyEnemies(8.0f);
    // Higher relevance with more enemies
    return std::min(enemyCount / 3.0f, 0.9f);
}

#endif // WITH_PLAYERBOTS