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

#include "PlayerbotCommon.h"
#include "PlayerbotPlayerAI.h"
#include "Player.h"
#include "Pet.h"
#include "MotionMaster.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellHistory.h"
#include "Log.h"

// PlayerbotAction Implementation
PlayerbotAction::PlayerbotAction(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotAIAware(ai), _name(name)
{
}

// PlayerbotSpellAction Implementation
Unit* PlayerbotSpellAction::GetSpellTarget() const
{
    return _ai->GetCurrentTarget();
}

bool PlayerbotSpellAction::Execute(PlayerbotEvent const& event)
{
    Unit* target = GetSpellTarget();
    
    // Allow actions to override target selection
    if (!target)
        return false;
    
    return CastSpell(target);
}

bool PlayerbotSpellAction::CastSpell(Unit* target, bool triggered)
{
    Player* bot = GetBot();
    if (!bot || !_spellId)
        return false;
    
    // Use provided target or get default target
    if (!target)
        target = GetSpellTarget();
    
    if (!target)
        return false;
    
    // Validate spell can be cast
    if (!CanCastSpell(target))
        return false;
    
    // Use TrinityCore's spell casting system
    SpellCastResult result = bot->CastSpell(target, _spellId, triggered);
    
    LogAction("Cast spell " + std::to_string(_spellId) + " on " + target->GetName());
    
    return result == SPELL_CAST_OK;
}

bool PlayerbotSpellAction::CanCastSpell(Unit* target) const
{
    if (!target || !IsSpellReady())
        return false;
    
    if (!HasEnoughMana())
        return false;
    
    if (!IsInRange(target))
        return false;
    
    return true;
}

bool PlayerbotSpellAction::IsSpellReady() const
{
    Player* bot = GetBot();
    if (!bot || !_spellId)
        return false;
    
    // Check if spell is known
    if (!bot->HasSpell(_spellId))
        return false;
    
    // Check if spell is on cooldown using SpellHistory
    if (bot->GetSpellHistory()->HasCooldown(_spellId))
        return false;
    
    return true;
}

bool PlayerbotSpellAction::HasEnoughMana() const
{
    Player* bot = GetBot();
    if (!bot || !_spellId)
        return false;
    
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(_spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;
    
    // Check power costs using modern TrinityCore API
    std::vector<SpellPowerCost> costs = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
    
    for (SpellPowerCost const& cost : costs)
    {
        if (cost.Amount > 0 && bot->GetPower(cost.Power) < cost.Amount)
            return false;
    }
    
    return true;
}

bool PlayerbotSpellAction::IsInRange(Unit* target) const
{
    Player* bot = GetBot();
    if (!bot || !target || !_spellId)
        return false;
    
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(_spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;
    
    float distance = bot->GetDistance(target);
    float maxRange = spellInfo->GetMaxRange(false);
    float minRange = spellInfo->GetMinRange(false);
    
    return distance >= minRange && distance <= maxRange;
}

// PlayerbotStrategy Implementation
PlayerbotStrategy::PlayerbotStrategy(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotAIAware(ai), _name(name), _type(0)
{
}

// PlayerbotTrigger Implementation
PlayerbotTrigger::PlayerbotTrigger(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotAIAware(ai), _name(name)
{
}

// PlayerbotAIAware utility method implementations
Player* PlayerbotAIAware::GetBot() const
{
    return _ai->GetBot();
}

Unit* PlayerbotAIAware::GetTarget() const
{
    Player* bot = GetBot();
    return bot ? bot->GetVictim() : nullptr;
}

Unit* PlayerbotAIAware::GetCurrentTarget() const
{
    return GetTarget(); // Same as GetTarget for now
}

bool PlayerbotAIAware::IsInCombat() const
{
    Player* bot = GetBot();
    return bot ? bot->IsInCombat() : false;
}

bool PlayerbotAIAware::IsInMeleeRange(Unit* target) const
{
    Player* bot = GetBot();
    if (!bot || !target)
        return false;
    return bot->GetDistance(target) <= bot->GetMeleeRange(target);
}

float PlayerbotAIAware::GetDistance(Unit* target) const
{
    Player* bot = GetBot();
    if (!bot || !target)
        return 100000.0f; // Very large distance if invalid
    return bot->GetDistance(target);
}

void PlayerbotAIAware::LogAction(std::string const& message) const
{
    Player* bot = GetBot();
    if (bot)
        TC_LOG_DEBUG("playerbots", "Bot {}: {}", bot->GetName(), message);
    else
        TC_LOG_DEBUG("playerbots", "Unknown Bot: {}", message);
}

void PlayerbotAIAware::LogError(std::string const& message) const
{
    Player* bot = GetBot();
    if (bot)
        TC_LOG_ERROR("playerbots", "Bot {}: {}", bot->GetName(), message);
    else
        TC_LOG_ERROR("playerbots", "Unknown Bot: {}", message);
}

bool PlayerbotAIAware::HasPet() const
{
    Player* bot = GetBot();
    if (!bot)
        return false;
    Pet* pet = bot->GetPet();
    return pet && pet->IsAlive();
}

bool PlayerbotAIAware::IsPetInCombat() const
{
    Player* bot = GetBot();
    if (!bot)
        return false;
    Pet* pet = bot->GetPet();
    return pet && pet->IsInCombat();
}

bool PlayerbotAIAware::MoveTo(float x, float y, float z) const
{
    Player* bot = GetBot();
    if (!bot)
        return false;
    
    // Use TrinityCore's movement system
    bot->GetMotionMaster()->MovePoint(0, x, y, z);
    return true;
}

#endif // WITH_PLAYERBOTS