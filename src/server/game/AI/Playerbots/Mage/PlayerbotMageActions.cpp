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

#include "PlayerbotMageActions.h"
#include "PlayerbotPlayerAI.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "Group.h"

uint32 PlayerbotMageAction::GetManaAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_MANA) : 0;
}

uint32 PlayerbotMageAction::GetManaPercent() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 maxMana = bot->GetMaxPower(POWER_MANA);
    return maxMana > 0 ? (GetManaAmount() * 100) / maxMana : 0;
}

uint32 PlayerbotMageAction::GetArcaneCharges() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
        
    // Arcane Charge aura - check for stacks
    if (Aura* aura = bot->GetAura(36032)) // Arcane Charge aura ID
        return aura->GetStackAmount();
    
    return 0;
}

bool PlayerbotMageAction::HasEnoughMana(uint32 cost) const
{
    return GetManaAmount() >= cost;
}

bool PlayerbotMageAction::IsInCombat() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->IsInCombat();
}

bool PlayerbotMageAction::ShouldConserveMana() const
{
    return GetManaPercent() < 15;
}

bool PlayerbotMageAction::isArcane() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(MageSpells::ARCANE_BLAST);
}

bool PlayerbotMageAction::isFire() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(MageSpells::FIREBALL);
}

bool PlayerbotMageAction::isFrost() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(MageSpells::ICE_LANCE);
}

Unit* PlayerbotMageAction::FindBestTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;
    
    // First check current target
    Unit* currentTarget = bot->GetSelectedUnit();
    if (currentTarget && currentTarget->IsAlive() && bot->IsWithinLOSInMap(currentTarget))
        return currentTarget;
    
    // Find nearest hostile target
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot, 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, check);
    Cell::VisitAllObjects(bot, searcher, 30.0f);
    
    Unit* bestTarget = nullptr;
    float closestDistance = 30.0f;
    
    for (Unit* target : targets)
    {
        if (!target->IsAlive() || !bot->IsWithinLOSInMap(target))
            continue;
        
        float distance = bot->GetDistance(target);
        if (distance < closestDistance)
        {
            closestDistance = distance;
            bestTarget = target;
        }
    }
    
    return bestTarget;
}

uint32 PlayerbotMageAction::CountNearbyEnemies(float range) const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot, range);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, check);
    Cell::VisitAllObjects(bot, searcher, range);
    
    uint32 count = 0;
    for (Unit* target : targets)
    {
        if (target->IsAlive())
            count++;
    }
    
    return count;
}

bool PlayerbotMageAction::ShouldUseAoE() const
{
    return CountNearbyEnemies(8.0f) >= 3;
}

bool PlayerbotMageAction::HasArcaneIntellect() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MageSpells::ARCANE_INTELLECT);
}

bool PlayerbotMageAction::HasMageArmor() const
{
    Player* bot = _ai->GetBot();
    return bot && (bot->HasAura(MageSpells::FROST_ARMOR) || 
                   bot->HasAura(MageSpells::ICE_BARRIER) ||
                   bot->HasAura(MageSpells::BLAZING_BARRIER));
}

bool PlayerbotMageAction::IsTargetFrozen(Unit* target) const
{
    if (!target)
        return false;
        
    // Check for various freeze effects
    return target->HasAuraType(SPELL_AURA_MOD_ROOT) || 
           target->HasAuraType(SPELL_AURA_MOD_STUN) ||
           target->HasAura(MageSpells::FROST_NOVA) ||
           target->HasAura(MageSpells::DEEP_FREEZE);
}

bool PlayerbotMageAction::HasBrainFreeze() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MageSpells::BRAIN_FREEZE);
}

bool PlayerbotMageAction::HasFingersOfFrost() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MageSpells::FINGERS_OF_FROST);
}

bool PlayerbotMageAction::HasHotStreak() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MageSpells::HOT_STREAK);
}

bool PlayerbotMageAction::HasHeatingUp() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MageSpells::HEATING_UP);
}

float PlayerbotMageAction::GetIgniteDuration(Unit* target) const
{
    if (!target)
        return 0.0f;
        
    if (Aura* aura = target->GetAura(MageSpells::IGNITE))
        return float(aura->GetDuration()) / 1000.0f;
        
    return 0.0f;
}

bool PlayerbotMageAction::CanCastArcaneSpell(uint32 charges) const
{
    Player* bot = _ai->GetBot();
    if (!bot || !isArcane())
        return false;
    
    // Check mana requirements - higher charges cost more mana
    uint32 baseCost = 250;
    uint32 totalCost = baseCost + (charges * 50);
    
    return HasEnoughMana(totalCost);
}

bool PlayerbotMageAction::CanCastFireSpell() const
{
    Player* bot = _ai->GetBot();
    return bot && isFire() && HasEnoughMana(200);
}

bool PlayerbotMageAction::CanCastFrostSpell() const
{
    Player* bot = _ai->GetBot();
    return bot && isFrost() && HasEnoughMana(150);
}