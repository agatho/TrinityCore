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

#include "PlayerbotWarlockActions.h"
#include "PlayerbotPlayerAI.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "Group.h"

uint32 PlayerbotWarlockAction::GetManaAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_MANA) : 0;
}

uint32 PlayerbotWarlockAction::GetManaPercent() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 maxMana = bot->GetMaxPower(POWER_MANA);
    return maxMana > 0 ? (GetManaAmount() * 100) / maxMana : 0;
}

uint32 PlayerbotWarlockAction::GetSoulShards() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_SOUL_SHARDS) : 0;
}

bool PlayerbotWarlockAction::HasEnoughMana(uint32 cost) const
{
    return GetManaAmount() >= cost;
}

bool PlayerbotWarlockAction::HasEnoughSoulShards(uint32 cost) const
{
    return GetSoulShards() >= cost;
}

bool PlayerbotWarlockAction::IsInCombat() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->IsInCombat();
}

bool PlayerbotWarlockAction::ShouldConserveMana() const
{
    return GetManaPercent() < 20;
}

bool PlayerbotWarlockAction::isAffliction() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(WarlockSpells::MALEFIC_RAPTURE);
}

bool PlayerbotWarlockAction::isDemonology() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(WarlockSpells::HAND_OF_GULDAN);
}

bool PlayerbotWarlockAction::isDestruction() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(WarlockSpells::CHAOS_BOLT);
}

bool PlayerbotWarlockAction::HasActivePet() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetPet() && bot->GetPet()->IsAlive();
}

bool PlayerbotWarlockAction::ShouldSummonPet() const
{
    return !HasActivePet() && !IsInCombat();
}

Unit* PlayerbotWarlockAction::GetPet() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPet() : nullptr;
}

bool PlayerbotWarlockAction::IsPetInCombat() const
{
    Unit* pet = GetPet();
    return pet && pet->IsInCombat();
}

Unit* PlayerbotWarlockAction::FindBestTarget() const
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

uint32 PlayerbotWarlockAction::CountNearbyEnemies(float range) const
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

bool PlayerbotWarlockAction::ShouldUseAoE() const
{
    return CountNearbyEnemies(8.0f) >= 3;
}

float PlayerbotWarlockAction::GetCorruptionDuration(Unit* target) const
{
    if (!target)
        return 0.0f;
        
    if (Aura* aura = target->GetAura(WarlockSpells::CORRUPTION))
        return float(aura->GetDuration()) / 1000.0f;
        
    return 0.0f;
}

float PlayerbotWarlockAction::GetAgonyDuration(Unit* target) const
{
    if (!target)
        return 0.0f;
        
    if (Aura* aura = target->GetAura(WarlockSpells::AGONY))
        return float(aura->GetDuration()) / 1000.0f;
        
    return 0.0f;
}

float PlayerbotWarlockAction::GetImmolateDuration(Unit* target) const
{
    if (!target)
        return 0.0f;
        
    if (Aura* aura = target->GetAura(WarlockSpells::IMMOLATE))
        return float(aura->GetDuration()) / 1000.0f;
        
    return 0.0f;
}

bool PlayerbotWarlockAction::HasCorruption(Unit* target) const
{
    return target && target->HasAura(WarlockSpells::CORRUPTION);
}

bool PlayerbotWarlockAction::HasAgony(Unit* target) const
{
    return target && target->HasAura(WarlockSpells::AGONY);
}

bool PlayerbotWarlockAction::HasImmolate(Unit* target) const
{
    return target && target->HasAura(WarlockSpells::IMMOLATE);
}

bool PlayerbotWarlockAction::ShouldRefreshDoT(Unit* target, uint32 spellId, float threshold) const
{
    if (!target)
        return false;
        
    if (Aura* aura = target->GetAura(spellId))
        return float(aura->GetDuration()) / 1000.0f < threshold;
        
    return true; // No DoT present, should apply
}

bool PlayerbotWarlockAction::HasBackdraft() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(WarlockSpells::BACKDRAFT);
}

bool PlayerbotWarlockAction::HasEradication() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(WarlockSpells::ERADICATION);
}

bool PlayerbotWarlockAction::HasDemonicCalling() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(WarlockSpells::DEMONIC_CALLING);
}

uint32 PlayerbotWarlockAction::GetBackdraftStacks() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
        
    if (Aura* aura = bot->GetAura(WarlockSpells::BACKDRAFT))
        return aura->GetStackAmount();
        
    return 0;
}

bool PlayerbotWarlockAction::CanCastAfflictionSpell() const
{
    Player* bot = _ai->GetBot();
    return bot && isAffliction() && HasEnoughMana(200);
}

bool PlayerbotWarlockAction::CanCastDemonologySpell() const
{
    Player* bot = _ai->GetBot();
    return bot && isDemonology() && HasEnoughMana(250);
}

bool PlayerbotWarlockAction::CanCastDestructionSpell() const
{
    Player* bot = _ai->GetBot();
    return bot && isDestruction() && HasEnoughMana(300);
}