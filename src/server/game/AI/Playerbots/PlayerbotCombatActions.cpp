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

#include "PlayerbotCombatActions.h"
#include "PlayerbotPlayerAI.h"
#include "PlayerbotCommon.h"
#include "Player.h"
#include "Pet.h"
#include "Unit.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Log.h"
#include "Group.h"

// PlayerbotAttackAction Base Implementation  
bool PlayerbotAttackAction::IsInMeleeRange(Unit* target) const
{
    Player* bot = _ai->GetBot();
    if (!bot || !target)
        return false;
    
    float distance = bot->GetDistance(target);
    return distance <= bot->GetMeleeRange(target);
}

bool PlayerbotAttackAction::MoveTo(Unit* target, float distance)
{
    Player* bot = _ai->GetBot();
    if (!bot || !target)
        return false;
    
    // Simple movement - in a real implementation this would use the movement system
    return true; // Placeholder
}

bool PlayerbotAttackAction::Attack(Unit* target)
{
    Player* bot = _ai->GetBot();
    if (!bot || !target)
        return false;
    
    bot->Attack(target, true);
    return true;
}

Unit* PlayerbotAttackAction::GetAttackTarget() const
{
    return _ai->GetCurrentTarget();
}

// PlayerbotMeleeAction Implementation
bool PlayerbotMeleeAction::Execute(PlayerbotEvent const& event)
{
    Unit* target = GetAttackTarget();
    if (!target)
        return false;

    Player* bot = _ai->GetBot();
    if (!bot)
        return false;

    // Move to melee range if needed
    if (!IsInMeleeRange(target))
    {
        return MoveTo(target, bot->GetMeleeRange(target) * 0.8f);
    }

    // Start melee attack
    return Attack(target);
}

bool PlayerbotMeleeAction::isUseful()
{
    Unit* target = GetAttackTarget();
    if (!target || !target->IsAlive())
        return false;

    Player* bot = _ai->GetBot();
    if (!bot)
        return false;

    // Only useful if we can melee (not pacified, not casting, etc.)
    return !bot->HasUnitState(UNIT_STATE_CANNOT_AUTOATTACK) && 
           !bot->HasUnitState(UNIT_STATE_CASTING);
}

float PlayerbotMeleeAction::GetRelevance() const
{
    Unit* target = GetAttackTarget();
    if (!target)
        return 0.0f;

    // Higher relevance when target is close
    float distance = _ai->GetBot()->GetDistance(target);
    float meleeRange = _ai->GetBot()->GetMeleeRange(target) + target->GetCombatReach();
    
    if (distance <= meleeRange)
        return 1.0f;
    else if (distance <= meleeRange * 2.0f)
        return 0.5f;
    else
        return 0.1f;
}

Unit* PlayerbotMeleeAction::GetAttackTarget() const
{
    return PlayerbotAttackAction::GetAttackTarget();
}

// PlayerbotSwitchToMeleeAction Implementation
bool PlayerbotSwitchToMeleeAction::Execute(PlayerbotEvent const& event)
{
    PlayerbotPlayerAI* ai = GetAI();
    if (!ai)
        return false;

    // Remove ranged strategy, add close combat strategy
    ai->RemoveStrategy("ranged");
    ai->AddStrategy("close");
    
    LogAction("Switched to melee combat");
    return true;
}

bool PlayerbotSwitchToMeleeAction::isUseful()
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    PlayerbotPlayerAI* ai = GetAI();
    if (!ai)
        return false;

    // Useful if we have a ranged strategy active and target is close
    return ai->HasStrategy("ranged") && GetDistance(target) <= 10.0f;
}

// PlayerbotSwitchToRangedAction Implementation
bool PlayerbotSwitchToRangedAction::Execute(PlayerbotEvent const& event)
{
    PlayerbotPlayerAI* ai = GetAI();
    if (!ai)
        return false;

    // Remove close combat strategy, add ranged strategy
    ai->RemoveStrategy("close");
    ai->AddStrategy("ranged");
    
    LogAction("Switched to ranged combat");
    return true;
}

bool PlayerbotSwitchToRangedAction::isUseful()
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    PlayerbotPlayerAI* ai = GetAI();
    if (!ai)
        return false;

    Player* bot = _ai->GetBot();
    if (!bot)
        return false;

    // Useful if we have close strategy active but should be at range
    // (low health, ranged weapon equipped, etc.)
    if (!ai->HasStrategy("close"))
        return false;

    // Switch to ranged if health is low
    float healthPct = float(bot->GetHealth()) / float(bot->GetMaxHealth());
    return healthPct < 0.3f || GetDistance(target) > 20.0f;
}

// PlayerbotAutoAttackAction Implementation
bool PlayerbotAutoAttackAction::Execute(PlayerbotEvent const& event)
{
    Player* bot = _ai->GetBot();
    Unit* target = GetAttackTarget();
    
    if (!bot || !target)
        return false;

    // Ensure we're attacking the right target
    if (bot->GetVictim() != target)
    {
        bot->SetTarget(target->GetGUID());
        bot->Attack(target, true);
    }

    // Make sure auto attack is enabled
    if (!bot->GetVictim() || bot->GetVictim() != target)
    {
        bot->Attack(target, true);
        LogAction("Started auto attacking " + target->GetName());
        return true;
    }

    return false; // Auto attack is already active
}

bool PlayerbotAutoAttackAction::isUseful()
{
    Player* bot = _ai->GetBot();
    Unit* target = GetAttackTarget();
    
    if (!bot || !target)
        return false;

    // Useful if we're not auto attacking but should be
    return bot->IsInCombat() && (!bot->GetVictim() || bot->GetVictim() != target) && 
           IsInMeleeRange(target);
}

// PlayerbotPetAttackAction Implementation
bool PlayerbotPetAttackAction::Execute(PlayerbotEvent const& event)
{
    if (!HasPet())
        return false;

    Player* bot = _ai->GetBot();
    Pet* pet = bot->GetPet();
    Unit* target = GetTarget();

    if (!pet || !target)
        return false;

    // Command pet to attack target
    pet->SetTarget(target->GetGUID());
    pet->Attack(target, true);
    
    LogAction("Pet attacking " + target->GetName());
    return true;
}

bool PlayerbotPetAttackAction::isUseful()
{
    return HasPet() && !IsPetInCombat() && IsInCombat();
}

bool PlayerbotPetAttackAction::isPossible()
{
    return HasPet();
}

bool PlayerbotPetAttackAction::HasPet() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;

    Pet* pet = bot->GetPet();
    return pet && pet->IsAlive();
}

bool PlayerbotPetAttackAction::IsPetInCombat() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;

    Pet* pet = bot->GetPet();
    return pet && pet->IsInCombat();
}

// PlayerbotFleeAction Implementation
bool PlayerbotFleeAction::Execute(PlayerbotEvent const& event)
{
    if (!ShouldFlee())
        return false;

    float x, y, z;
    if (!FindSafePosition(x, y, z))
    {
        LogError("Could not find safe position to flee to");
        return false;
    }

    LogAction("Fleeing to safety");
    return MoveTo(x, y, z);
}

bool PlayerbotFleeAction::isUseful()
{
    return ShouldFlee();
}

float PlayerbotFleeAction::GetRelevance() const
{
    if (!ShouldFlee())
        return 0.0f;

    Player* bot = _ai->GetBot();
    if (!bot)
        return 0.0f;

    // Higher relevance when health is lower
    float healthPct = float(bot->GetHealth()) / float(bot->GetMaxHealth());
    return (1.0f - healthPct) * 2.0f; // Max relevance when at very low health
}

bool PlayerbotFleeAction::ExecuteMovement(Unit* target)
{
    // This is called by parent class, but we override Execute directly
    return false;
}

bool PlayerbotFleeAction::ShouldFlee() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !bot->IsInCombat())
        return false;

    // Flee if health is critically low
    float healthPct = float(bot->GetHealth()) / float(bot->GetMaxHealth());
    return healthPct < 0.15f; // Flee when below 15% health
}

bool PlayerbotFleeAction::FindSafePosition(float& x, float& y, float& z) const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;

    Position pos = bot->GetPosition();
    
    // Find a position away from enemies
    // Simple implementation: move back from current facing direction
    float distance = 20.0f;
    float angle = bot->GetOrientation() + M_PI; // Opposite direction
    
    x = pos.m_positionX + cos(angle) * distance;
    y = pos.m_positionY + sin(angle) * distance;
    z = pos.m_positionZ;

    return true;
}

// PlayerbotCombatSpellAction Implementation
bool PlayerbotCombatSpellAction::isUseful()
{
    return IsInCombat() && PlayerbotSpellAction::isUseful();
}

ThreatType PlayerbotCombatSpellAction::GetThreatType() const
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(GetSpellId(), DIFFICULTY_NONE);
    if (!spellInfo)
        return ThreatType::NONE;

    // Simple heuristic: if spell has area effect, it's AoE
    if (spellInfo->HasAreaAuraEffect() || spellInfo->IsTargetingArea())
        return ThreatType::AOE;

    return ThreatType::SINGLE;
}

Unit* PlayerbotCombatSpellAction::GetSpellTarget() const
{
    return GetBestHostileTarget();
}

Unit* PlayerbotCombatSpellAction::GetBestHostileTarget() const
{
    Unit* currentTarget = GetCurrentTarget();
    if (currentTarget && IsValidCombatTarget(currentTarget))
        return currentTarget;

    return GetNearestHostileTarget();
}

Unit* PlayerbotCombatSpellAction::GetNearestHostileTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;

    Unit* nearest = nullptr;
    float nearestDist = std::numeric_limits<float>::max();

    // Search for nearby hostile units
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck checker(bot, bot, 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, checker);
    Cell::VisitAllObjects(bot, searcher, 30.0f);

    for (Unit* target : targets)
    {
        if (!IsValidCombatTarget(target))
            continue;

        float dist = bot->GetDistance(target);
        if (dist < nearestDist)
        {
            nearest = target;
            nearestDist = dist;
        }
    }

    return nearest;
}

Unit* PlayerbotCombatSpellAction::GetWeakestHostileTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;

    Unit* weakest = nullptr;
    float lowestHealthPct = 1.0f;

    // Search for nearby hostile units
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck checker(bot, bot, 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, checker);
    Cell::VisitAllObjects(bot, searcher, 30.0f);

    for (Unit* target : targets)
    {
        if (!IsValidCombatTarget(target))
            continue;

        float healthPct = float(target->GetHealth()) / float(target->GetMaxHealth());
        if (healthPct < lowestHealthPct)
        {
            weakest = target;
            lowestHealthPct = healthPct;
        }
    }

    return weakest;
}

bool PlayerbotCombatSpellAction::IsValidCombatTarget(Unit* target) const
{
    if (!target || !target->IsAlive())
        return false;

    Player* bot = _ai->GetBot();
    if (!bot)
        return false;

    return target->IsHostileTo(bot) && bot->IsValidAttackTarget(target);
}

// PlayerbotDefensiveSpellAction Implementation
bool PlayerbotDefensiveSpellAction::isUseful()
{
    return ShouldUseDefensiveSpell() && PlayerbotSpellAction::isUseful();
}

float PlayerbotDefensiveSpellAction::GetRelevance() const
{
    float healthPct = GetHealthPercentage();
    
    // Higher relevance when health is lower
    if (healthPct < 0.3f)
        return 1.0f;
    else if (healthPct < 0.5f)
        return 0.7f;
    else if (healthPct < 0.8f)
        return 0.3f;
    else
        return 0.1f;
}

Unit* PlayerbotDefensiveSpellAction::GetSpellTarget() const
{
    return GetBot(); // Defensive spells usually target self
}

bool PlayerbotDefensiveSpellAction::ShouldUseDefensiveSpell() const
{
    return GetHealthPercentage() < 0.8f; // Use when below 80% health
}

float PlayerbotDefensiveSpellAction::GetHealthPercentage() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 1.0f;

    return float(bot->GetHealth()) / float(bot->GetMaxHealth());
}

// PlayerbotHealAction Implementation
bool PlayerbotHealAction::isUseful()
{
    Unit* target = GetBestHealTarget();
    return target && NeedsHealing(target) && PlayerbotSpellAction::isUseful();
}

float PlayerbotHealAction::GetRelevance() const
{
    Unit* target = GetBestHealTarget();
    if (!target)
        return 0.0f;

    float healthPct = float(target->GetHealth()) / float(target->GetMaxHealth());
    
    // Higher relevance for lower health
    if (healthPct < 0.2f)
        return 1.0f;
    else if (healthPct < 0.4f)
        return 0.8f;
    else if (healthPct < 0.6f)
        return 0.5f;
    else
        return 0.2f;
}

Unit* PlayerbotHealAction::GetSpellTarget() const
{
    return GetBestHealTarget();
}

Unit* PlayerbotHealAction::GetBestHealTarget() const
{
    // Priority: self, then group members, then nearby friendlies
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;

    // Check self first
    if (NeedsHealing(bot))
        return bot;

    // Check group members
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference const& itr : group->GetMembers())
        {
            Player* member = itr.GetSource();
            if (member && member != bot && NeedsHealing(member))
                return member;
        }
    }

    return GetMostWoundedTarget();
}

Unit* PlayerbotHealAction::GetMostWoundedTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;

    Unit* mostWounded = nullptr;
    float lowestHealthPct = 1.0f;

    // Search for nearby friendly units
    std::list<Unit*> targets;
    Trinity::AnyFriendlyUnitInObjectRangeCheck checker(bot, bot, 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyFriendlyUnitInObjectRangeCheck> searcher(bot, targets, checker);
    Cell::VisitAllObjects(bot, searcher, 30.0f);

    for (Unit* target : targets)
    {
        if (!NeedsHealing(target))
            continue;

        float healthPct = float(target->GetHealth()) / float(target->GetMaxHealth());
        if (healthPct < lowestHealthPct)
        {
            mostWounded = target;
            lowestHealthPct = healthPct;
        }
    }

    return mostWounded;
}

bool PlayerbotHealAction::NeedsHealing(Unit* target) const
{
    if (!target || !target->IsAlive())
        return false;

    float healthPct = float(target->GetHealth()) / float(target->GetMaxHealth());
    return healthPct < GetHealingThreshold();
}

float PlayerbotHealAction::GetHealingThreshold() const
{
    return 0.8f; // Heal when below 80% health
}

// PlayerbotAoEAction Implementation
bool PlayerbotAoEAction::isUseful()
{
    return CountNearbyEnemies() >= GetMinEnemyCount() && 
           PlayerbotCombatSpellAction::isUseful();
}

float PlayerbotAoEAction::GetRelevance() const
{
    uint32 enemyCount = CountNearbyEnemies();
    if (enemyCount < GetMinEnemyCount())
        return 0.0f;

    // Higher relevance for more enemies
    return std::min(enemyCount / 10.0f, 1.0f);
}

uint32 PlayerbotAoEAction::CountNearbyEnemies() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;

    uint32 count = 0;

    // Count hostile units within AoE range
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck checker(bot, bot, 15.0f); // 15 yard AoE range
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, checker);
    Cell::VisitAllObjects(bot, searcher, 15.0f);

    for (Unit* target : targets)
    {
        if (IsValidCombatTarget(target))
            count++;
    }

    return count;
}

#endif // WITH_PLAYERBOTS