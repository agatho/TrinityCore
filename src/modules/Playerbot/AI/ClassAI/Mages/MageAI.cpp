/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "MageAI.h"
#include "ArcaneMageRefactored.h"
#include "FireMageRefactored.h"
#include "FrostMageRefactored.h"
#include "../BaselineRotationManager.h"
#include "Player.h"
#include "Log.h"

namespace Playerbot
{

MageAI::MageAI(Player* bot) : ClassAI(bot){
    TC_LOG_DEBUG("module.playerbot.ai", "MageAI created for player {}", bot->GetName());
}

MageAI::~MageAI() = default;

void MageAI::UpdateRotation(::Unit* target)
{
    if (!target || !_bot)
        return;

    // Check if bot should use baseline rotation (levels 1-9 or no spec)
    if (BaselineRotationManager::ShouldUseBaselineRotation(_bot))
    {
        static BaselineRotationManager baselineManager;
        baselineManager.HandleAutoSpecialization(_bot);

        if (baselineManager.ExecuteBaselineRotation(_bot, target))
            return;

        // Fallback: basic ranged attack
        if (!_bot->IsNonMeleeSpellCast(false))
        {
            float rangeSq = 35.0f * 35.0f; // 1225.0f
            if (_bot->GetExactDistSq(target) <= rangeSq)
            {
                _bot->AttackerStateUpdate(target);
            }
        }
        return;
    }

    // Delegate to specialization-specific AI
    ChrSpecialization spec = _bot->GetPrimarySpecialization();

    switch (static_cast<uint32>(spec))
    {
        case 62: // Arcane
        {
            static ArcaneMageRefactored arcane(_bot);
            arcane.UpdateRotation(target);
            break;
        }
        case 63: // Fire
        {
            static FireMageRefactored fire(_bot);
            fire.UpdateRotation(target);
            break;
        }
        case 64: // Frost
        {
            static FrostMageRefactored frost(_bot);
            frost.UpdateRotation(target);
            break;
        }
        default:
            // No spec or unknown spec, use basic rotation
            if (CanUseAbility(FROSTBOLT))
                CastSpell(target, FROSTBOLT);
            break;
    }
}

void MageAI::UpdateBuffs()
{
    if (!_bot)
        return;

    // Delegate to specialization-specific AI
    ChrSpecialization spec = _bot->GetPrimarySpecialization();

    switch (static_cast<uint32>(spec))
    {
        case 62: // Arcane
        {
            static ArcaneMageRefactored arcane(_bot);
            arcane.UpdateBuffs();
            break;
        }
        case 63: // Fire
        {
            static FireMageRefactored fire(_bot);
            fire.UpdateBuffs();
            break;
        }
        case 64: // Frost
        {
            static FrostMageRefactored frost(_bot);
            frost.UpdateBuffs();
            break;
        }
        default:
            break;
    }
}

void MageAI::UpdateCooldowns(uint32 diff)
{
    // Base cooldown management handled by ClassAI
    ClassAI::UpdateCooldowns(diff);
}bool MageAI::CanUseAbility(uint32 spellId)
{
    if (!_bot)
        return false;    if (!_bot->HasSpell(spellId))
        return false;

    // Delegate to base ClassAI implementation for spell readiness and resource checks
    return ClassAI::CanUseAbility(spellId);
}

void MageAI::OnCombatStart(::Unit* target)
{
    ClassAI::OnCombatStart(target);
}

void MageAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();
}

bool MageAI::HasEnoughResource(uint32 spellId)
{
    // Mages use Mana
    if (!_bot)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return true;    // Calculate mana cost properly from PowerCosts vector
    uint32 manaCost = 0;
    for (SpellPowerCost const& cost : spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask()))
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }
    return _bot->GetPower(POWER_MANA) >= manaCost;}

void MageAI::ConsumeResource(uint32 spellId)
{
    // Resource consumption handled by spell casting system
}

Position MageAI::GetOptimalPosition(::Unit* target)
{
    if (!target)
        return _bot->GetPosition();    // Mages are ranged DPS, maintain maximum range
    // Calculate optimal position at maximum spell range (30-40 yards for most mage spells)
    float optimalRange = 35.0f;

    if (!target || target == _bot)        return _bot->GetPosition();

    // Get direction from bot to target (absolute angle)
    float angle = _bot->GetAbsoluteAngle(target);
    float distance = _bot->GetExactDist2d(target);

    // If too close, move back to optimal range
    if (distance < optimalRange)
    {
        // Get position near target at optimal range
        Position pos = target->GetNearPosition(optimalRange, angle);
        return pos;
    }

    // Already at good range
    return _bot->GetPosition();
}

float MageAI::GetOptimalRange(::Unit* target)
{
    // Mages are ranged casters with 30-35 yard range
    return 30.0f;
}

} // namespace Playerbot
