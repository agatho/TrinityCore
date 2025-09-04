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

#include "PlayerbotAction.h"
#include "PlayerbotPlayerAI.h"
#include "PlayerbotCommon.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Log.h"

// PlayerbotAction Implementation
PlayerbotAction::PlayerbotAction(PlayerbotPlayerAI* ai, std::string const& name)
    : _ai(ai), _name(name)
{
}

Player* PlayerbotAction::GetBot() const
{
    return _ai ? _ai->GetPlayer() : nullptr;
}

bool PlayerbotAction::CanCastSpell(uint32 spellId) const
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    // Check if bot knows the spell
    if (!bot->HasSpell(spellId))
        return false;

    // Check spell cooldown
    if (bot->HasSpellCooldown(spellId))
        return false;

    // Check mana/energy cost
    if (bot->GetPower(POWER_MANA) < spellInfo->ManaCost)
        return false;

    return true;
}

bool PlayerbotAction::IsInCombat() const
{
    Player* bot = GetBot();
    return bot && bot->IsInCombat();
}

Unit* PlayerbotAction::GetTarget() const
{
    Player* bot = GetBot();
    return bot ? bot->GetSelectedUnit() : nullptr;
}

Unit* PlayerbotAction::GetCurrentTarget() const
{
    Player* bot = GetBot();
    return bot ? bot->GetVictim() : nullptr;
}

void PlayerbotAction::LogAction(std::string const& message) const
{
    Player* bot = GetBot();
    TC_LOG_DEBUG("playerbots", "Bot {}: Action '{}' - {}", 
                 bot ? bot->GetName() : "Unknown", _name, message);
}

void PlayerbotAction::LogError(std::string const& error) const
{
    Player* bot = GetBot();
    TC_LOG_ERROR("playerbots", "Bot {}: Action '{}' ERROR - {}", 
                 bot ? bot->GetName() : "Unknown", _name, error);
}

// PlayerbotMovementAction Implementation
PlayerbotMovementAction::PlayerbotMovementAction(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotAction(ai, name), _lastMovement(0)
{
}

bool PlayerbotMovementAction::Execute(PlayerbotEvent event)
{
    Unit* target = GetTarget();
    if (!target)
    {
        LogError("No target for movement action");
        return false;
    }

    // Rate limit movement commands
    uint32 now = getMSTime();
    if (now - _lastMovement < MOVEMENT_COOLDOWN)
        return false;

    _lastMovement = now;

    try
    {
        return ExecuteMovement(target);
    }
    catch (std::exception const& e)
    {
        LogError("Exception in movement execution: " + std::string(e.what()));
        return false;
    }
}

bool PlayerbotMovementAction::MoveTo(float x, float y, float z)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    // Use TrinityCore's movement system
    bot->GetMotionMaster()->MovePoint(0, x, y, z);
    LogAction("Moving to position (" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")");
    return true;
}

bool PlayerbotMovementAction::MoveTo(Unit* target, float distance)
{
    if (!target)
        return false;

    Position pos = target->GetPosition();
    
    // Calculate position at specified distance from target
    if (distance > 0.0f)
    {
        float angle = target->GetAngle(GetBot());
        pos.m_positionX += cos(angle) * distance;
        pos.m_positionY += sin(angle) * distance;
    }

    return MoveTo(pos.m_positionX, pos.m_positionY, pos.m_positionZ);
}

bool PlayerbotMovementAction::IsInRange(Unit* target, float distance) const
{
    return GetDistance(target) <= distance;
}

float PlayerbotMovementAction::GetDistance(Unit* target) const
{
    Player* bot = GetBot();
    if (!bot || !target)
        return std::numeric_limits<float>::max();

    return bot->GetDistance(target);
}

// PlayerbotSpellAction Implementation
PlayerbotSpellAction::PlayerbotSpellAction(PlayerbotPlayerAI* ai, std::string const& name, uint32 spellId)
    : PlayerbotAction(ai, name), _spellId(spellId)
{
}

bool PlayerbotSpellAction::Execute(PlayerbotEvent event)
{
    if (!CanCastSpell())
    {
        LogAction("Cannot cast spell " + std::to_string(_spellId));
        return false;
    }

    Unit* target = GetSpellTarget();
    if (!target)
    {
        LogError("No target for spell " + std::to_string(_spellId));
        return false;
    }

    return CastSpell(target);
}

bool PlayerbotSpellAction::IsPossible() const
{
    return CanCastSpell(_spellId);
}

uint32 PlayerbotSpellAction::GetCooldown() const
{
    Player* bot = GetBot();
    if (!bot)
        return 0;

    return bot->GetSpellCooldownDelay(_spellId);
}

Unit* PlayerbotSpellAction::GetSpellTarget() const
{
    return GetTarget();
}

bool PlayerbotSpellAction::CanCastSpell() const
{
    return PlayerbotAction::CanCastSpell(_spellId);
}

bool PlayerbotSpellAction::CastSpell(Unit* target)
{
    return CastSpell(_spellId, target);
}

bool PlayerbotSpellAction::CastSpell(uint32 spellId, Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    // Create spell cast
    Spell* spell = new Spell(bot, spellInfo, TRIGGERED_NONE);
    if (!spell)
        return false;

    // Set target if specified
    if (target)
    {
        SpellCastTargets targets;
        targets.SetUnitTarget(target);
        spell->m_targets = targets;
    }

    // Execute the spell cast
    SpellCastResult result = spell->prepare();
    
    if (result == SPELL_CAST_OK)
    {
        LogAction("Successfully cast spell " + std::to_string(spellId) + " on " + 
                 (target ? target->GetName() : "self"));
        return true;
    }
    else
    {
        LogError("Failed to cast spell " + std::to_string(spellId) + ", result: " + std::to_string(result));
        delete spell;
        return false;
    }
}

uint32 PlayerbotSpellAction::GetManaCost() const
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(_spellId);
    return spellInfo ? spellInfo->ManaCost : 0;
}

uint32 PlayerbotSpellAction::GetCastTime() const
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(_spellId);
    return spellInfo ? spellInfo->CastTime : 0;
}

float PlayerbotSpellAction::GetSpellRange() const
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(_spellId);
    return spellInfo ? spellInfo->GetMaxRange() : 0.0f;
}

// PlayerbotAttackAction Implementation
PlayerbotAttackAction::PlayerbotAttackAction(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotMovementAction(ai, name)
{
}

bool PlayerbotAttackAction::Execute(PlayerbotEvent event)
{
    Unit* target = GetAttackTarget();
    if (!target)
    {
        LogError("No attack target");
        return false;
    }

    // If not in melee range, move closer
    if (!IsInMeleeRange(target))
    {
        return ExecuteMovement(target);
    }

    // Start attacking
    return Attack(target);
}

bool PlayerbotAttackAction::IsUseful() const
{
    Unit* target = GetAttackTarget();
    return target && target->IsAlive() && target->IsHostileTo(GetBot());
}

bool PlayerbotAttackAction::ExecuteMovement(Unit* target)
{
    if (!target)
        return false;

    // Move to melee range
    float meleeRange = GetBot()->GetMeleeReach() + target->GetCombatReach();
    return MoveTo(target, meleeRange * 0.8f); // Move slightly closer than max range
}

Unit* PlayerbotAttackAction::GetAttackTarget() const
{
    Unit* target = GetCurrentTarget();
    if (!target)
        target = GetTarget();
    
    return target;
}

bool PlayerbotAttackAction::Attack(Unit* target, bool usePet)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return false;

    // Start attack if not already attacking
    if (!bot->IsInCombat() || bot->GetVictim() != target)
    {
        return StartAttack(target);
    }

    // Continue attacking - let the core combat system handle auto-attacks
    LogAction("Continuing attack on " + target->GetName());
    return true;
}

bool PlayerbotAttackAction::IsInMeleeRange(Unit* target) const
{
    Player* bot = GetBot();
    if (!bot || !target)
        return false;

    float meleeRange = bot->GetMeleeReach() + target->GetCombatReach();
    return GetDistance(target) <= meleeRange;
}

bool PlayerbotAttackAction::StartAttack(Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return false;

    // Set the target and start attacking
    bot->SetTarget(target->GetGUID());
    bot->Attack(target, true);
    
    LogAction("Started attacking " + target->GetName());
    return true;
}

bool PlayerbotAttackAction::ShouldUsePet() const
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    // Check if bot has a pet and it's alive
    Pet* pet = bot->GetPet();
    return pet && pet->IsAlive();
}

#endif // WITH_PLAYERBOTS