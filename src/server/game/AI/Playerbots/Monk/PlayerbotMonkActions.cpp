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

#include "PlayerbotMonkActions.h"
#include "PlayerbotPlayerAI.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "Group.h"

uint32 PlayerbotMonkAction::GetEnergyAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_ENERGY) : 0;
}

uint32 PlayerbotMonkAction::GetEnergyPercent() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 maxEnergy = bot->GetMaxPower(POWER_ENERGY);
    return maxEnergy > 0 ? (GetEnergyAmount() * 100) / maxEnergy : 0;
}

uint32 PlayerbotMonkAction::GetChi() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_CHI) : 0;
}

bool PlayerbotMonkAction::HasEnoughEnergy(uint32 cost) const
{
    return GetEnergyAmount() >= cost;
}

bool PlayerbotMonkAction::HasEnoughChi(uint32 cost) const
{
    return GetChi() >= cost;
}

bool PlayerbotMonkAction::IsInCombat() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->IsInCombat();
}

bool PlayerbotMonkAction::isBrewmaster() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(MonkSpells::KEG_SMASH);
}

bool PlayerbotMonkAction::isMistweaver() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(MonkSpells::ESSENCE_FONT);
}

bool PlayerbotMonkAction::isWindwalker() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(MonkSpells::FISTS_OF_FURY);
}

Unit* PlayerbotMonkAction::FindBestTarget() const
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

Unit* PlayerbotMonkAction::FindBestHealingTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;
    
    // Check self first
    if (bot->GetHealthPct() < 80.0f)
        return bot;
    
    // Check group members
    Group* group = bot->GetGroup();
    if (group)
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (member && member->IsAlive() && member->GetHealthPct() < 80.0f &&
                bot->IsWithinDistInMap(member, 40.0f))
                return member;
        }
    }
    
    return nullptr;
}

uint32 PlayerbotMonkAction::CountNearbyEnemies(float range) const
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

uint32 PlayerbotMonkAction::CountInjuredGroupMembers(float threshold) const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 count = 0;
    
    // Check self
    if (bot->GetHealthPct() < threshold * 100.0f)
        count++;
    
    // Check group members
    Group* group = bot->GetGroup();
    if (group)
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (member && member->IsAlive() && member->GetHealthPct() < threshold * 100.0f &&
                bot->IsWithinDistInMap(member, 40.0f))
                count++;
        }
    }
    
    return count;
}

bool PlayerbotMonkAction::ShouldUseAoE() const
{
    return CountNearbyEnemies(8.0f) >= 3;
}

bool PlayerbotMonkAction::ShouldUseAoEHealing() const
{
    return CountInjuredGroupMembers(0.8f) >= 3;
}

bool PlayerbotMonkAction::HasShuffle() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MonkSpells::SHUFFLE);
}

bool PlayerbotMonkAction::HasStagger() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MonkSpells::STAGGER);
}

float PlayerbotMonkAction::GetStaggerDamage() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0.0f;
        
    if (Aura* aura = bot->GetAura(MonkSpells::STAGGER))
    {
        // Simplified stagger damage calculation
        return float(aura->GetStackAmount()) * 100.0f;
    }
    
    return 0.0f;
}

bool PlayerbotMonkAction::HasTeachingsOfTheMonastery() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MonkSpells::TEACHINGS_OF_THE_MONASTERY);
}

uint32 PlayerbotMonkAction::GetTeachingsStacks() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
        
    if (Aura* aura = bot->GetAura(MonkSpells::TEACHINGS_OF_THE_MONASTERY))
        return aura->GetStackAmount();
        
    return 0;
}

bool PlayerbotMonkAction::HasDanceOfChiJi() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MonkSpells::DANCE_OF_CHI_JI);
}

bool PlayerbotMonkAction::HasComboBreaker() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MonkSpells::COMBO_BREAKER);
}

bool PlayerbotMonkAction::HasMarkOfTheCrane(Unit* target) const
{
    return target && target->HasAura(MonkSpells::MARK_OF_THE_CRANE);
}

uint32 PlayerbotMonkAction::GetMarkOfTheCraneCount() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    // Count nearby enemies with Mark of the Crane
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot, 8.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, check);
    Cell::VisitAllObjects(bot, searcher, 8.0f);
    
    uint32 count = 0;
    for (Unit* target : targets)
    {
        if (target->IsAlive() && HasMarkOfTheCrane(target))
            count++;
    }
    
    return count;
}

bool PlayerbotMonkAction::HasHitCombo() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MonkSpells::HIT_COMBO);
}

bool PlayerbotMonkAction::IsInMeleeRange(Unit* target) const
{
    Player* bot = _ai->GetBot();
    return bot && target && bot->GetDistance(target) <= 5.0f;
}

bool PlayerbotMonkAction::ShouldUseRoll() const
{
    Player* bot = _ai->GetBot();
    Unit* target = FindBestTarget();
    
    // Use Roll to close distance or escape danger
    return bot && target && (bot->GetDistance(target) > 15.0f || bot->GetHealthPct() < 30.0f);
}

bool PlayerbotMonkAction::ShouldUseTranscendence() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetHealthPct() < 20.0f;
}

bool PlayerbotMonkAction::CanCastBrewmasterSpell() const
{
    Player* bot = _ai->GetBot();
    return bot && isBrewmaster() && HasEnoughEnergy(40);
}

bool PlayerbotMonkAction::CanCastMistweaverSpell() const
{
    Player* bot = _ai->GetBot();
    return bot && isMistweaver() && bot->GetPower(POWER_MANA) > 1000;
}

bool PlayerbotMonkAction::CanCastWindwalkerSpell() const
{
    Player* bot = _ai->GetBot();
    return bot && isWindwalker() && HasEnoughEnergy(40);
}