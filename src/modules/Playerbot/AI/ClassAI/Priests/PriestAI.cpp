/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PriestAI.h"
#include "Player.h"
#include "Unit.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Map.h"
#include "Group.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "SpellAuras.h"
#include "MotionMaster.h"
#include "../BaselineRotationManager.h"
#include "../../Combat/CombatBehaviorIntegration.h"
#include <algorithm>
#include "../../../Spatial/SpatialGridQueryHelpers.h"  // PHASE 5F: Thread-safe queries
#include "../../../Movement/UnifiedMovementCoordinator.h  // Phase 2: Unified movement system"
#include "../../../Movement/Arbiter/MovementPriorityMapper.h"
#include "../../BotAI.h"
#include "UnitAI.h"
#include "GameTime.h"

namespace Playerbot
{

// Talent IDs for specialization detection
enum PriestTalents
{
    // Holy talents
    TALENT_SPIRIT_OF_REDEMPTION = 20711,
    TALENT_CIRCLE_OF_HEALING = 34861,
    TALENT_GUARDIAN_SPIRIT = 47788,

    // Discipline talents
    TALENT_POWER_INFUSION = 10060,
    TALENT_PAIN_SUPPRESSION = 33206,
    TALENT_PENANCE = 47540,

    // Shadow talents
    TALENT_SHADOWFORM = 15473,
    TALENT_VAMPIRIC_EMBRACE = 15286,
    TALENT_VAMPIRIC_TOUCH = 34914,
    TALENT_DISPERSION = 47585
};

// ============================================================================
// CONSTRUCTOR/DESTRUCTOR
// ============================================================================

PriestAI::PriestAI(Player* bot) : ClassAI(bot),    _manaSpent(0),
    _healingDone(0),
    _damageDealt(0),
    _playersHealed(0),
    _damagePrevented(0),
    _lastDispel(0),
    _lastFearWard(0),
    _lastPsychicScream(0),
    _lastInnerFire(0),
    _lastSilence(0),
    _lastMassDispel(0),
    _lastDesperatePrayer(0),
    _lastFade(0)
{
    TC_LOG_DEBUG("module.playerbot.ai", "PriestAI created for player {}",

                 bot ? bot->GetName() : "null");
}

PriestAI::~PriestAI() = default;

// ============================================================================
// UPDATE ROTATION - Core delegation logic
// ============================================================================

void PriestAI::UpdateRotation(::Unit* target)
{
    if (!GetBot() || !target)
        return;

    // Check if bot should use baseline rotation (levels 1-9 or no spec)
    if (BaselineRotationManager::ShouldUseBaselineRotation(GetBot()))
    {
        static BaselineRotationManager baselineManager;
        baselineManager.HandleAutoSpecialization(GetBot());

        if (baselineManager.ExecuteBaselineRotation(GetBot(), target))

            return;

        // Fallback: basic ranged attack
        if (!GetBot()->IsNonMeleeSpellCast(false))
        {

            if (target && GetBot()->GetDistance(target) <= 35.0f)

            {

                GetBot()->AttackerStateUpdate(target);

            }
        }
        return;
    }

    // Update shared priest mechanics
    UpdatePriestBuffs();

    // ========================================================================
    // COMBAT BEHAVIOR INTEGRATION - Full priority system (EXECUTES FIRST)
    // ========================================================================

    // Execute combat behavior priorities - if any return true, they handled the action
    if (HandleCombatBehaviorPriorities(target))
        return;

    // Track combat metrics
    if (GetBot()->IsInCombat())
    {
        _damageDealt += CalculateDamageDealt(target);
        _healingDone += CalculateHealingDone();
        _manaSpent += CalculateManaUsage();
    }
}

void PriestAI::UpdateBuffs()
{
    if (!GetBot())
        return;

    // Use baseline buffs for low-level bots
    if (BaselineRotationManager::ShouldUseBaselineRotation(GetBot()))
    {
        static BaselineRotationManager baselineManager;
        baselineManager.ApplyBaselineBuffs(GetBot());
        return;
    }

    UpdatePriestBuffs();
    UpdateFortitudeBuffs();
}

void PriestAI::UpdateCooldowns(uint32 diff)
{
    // Base cooldown tracking is handled by ClassAI
    ClassAI::UpdateCooldowns(diff);
}

bool PriestAI::CanUseAbility(uint32 spellId)
{
    if (!GetBot())
        return false;

    // Check basic requirements
    if (!this->IsSpellReady(spellId) || !HasEnoughResource(spellId))
        return false;

    // Check priest-specific conditions
    if (IsHealingSpell(spellId) && ShouldConserveMana())
        return false;

    return true;
}

void PriestAI::OnCombatStart(::Unit* target){
    if (!GetBot() || !target)
        return;

    TC_LOG_DEBUG("module.playerbot.ai", "Priest {} entering combat with {}",

                 GetBot()->GetName(), target->GetName());

    // Initialize combat tracking
    _manaSpent = 0;
    _healingDone = 0;
    _damageDealt = 0;
    _playersHealed = 0;
}

void PriestAI::OnCombatEnd()
{
    if (!GetBot())
        return;

    TC_LOG_DEBUG("module.playerbot.ai", "Priest {} leaving combat. Metrics - Healing: {}, Damage: {}, Mana Used: {}, Players Healed: {}",

                 GetBot()->GetName(), _healingDone, _damageDealt, _manaSpent, _playersHealed);

    // Post-combat healing
    HealGroupMembers();

    // Reapply buffs if needed
    UpdatePriestBuffs();

    // Log performance metrics
    AnalyzeHealingEfficiency();
}

bool PriestAI::HasEnoughResource(uint32 spellId)
{
    if (!GetBot())
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    // Check mana cost
    auto powerCosts = spellInfo->CalcPowerCost(GetBot(), spellInfo->GetSchoolMask());
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA && GetBot()->GetPower(POWER_MANA) < int32(cost.Amount))

            return false;
    }

    // Check for mana conservation mode
    if (ShouldConserveMana() && !IsEmergencyHeal(spellId))
    {
        float manaPercent = GetManaPercent();
        if (manaPercent < MANA_CONSERVATION_THRESHOLD * 100.0f)

            return false;
    }

    return true;
}

void PriestAI::ConsumeResource(uint32 spellId)
{
    if (!GetBot())
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    // Track mana consumption
    auto powerCosts = spellInfo->CalcPowerCost(GetBot(), spellInfo->GetSchoolMask());
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)

            _manaSpent += cost.Amount;
    }

    // Track specific spell usage
    if (IsHealingSpell(spellId))
    {
        _playersHealed++;
    }
    else if (IsDamageSpell(spellId))
    {
        _damageDealt += 100; // Simplified
    }
}

Position PriestAI::GetOptimalPosition(::Unit* target){
    if (!GetBot() || !target)
        return Position();

    // Default positioning for priests - maintain range
    float optimalRange = GetOptimalRange(target);
    float angle = GetBot()->GetAbsoluteAngle(target);

    // Position behind and to the side for safety
    angle += M_PI / 4; // 45 degrees offset

    float x = target->GetPositionX() - optimalRange * std::cos(angle);    float y = target->GetPositionY() - optimalRange * std::sin(angle);    float z = target->GetPositionZ();    return Position(x, y, z);
}

float PriestAI::GetOptimalRange(::Unit* target)
{
    if (!GetBot() || !target)
        return OPTIMAL_HEALING_RANGE;

    // Default range based on current specialization
    ChrSpecialization currentSpec = GetBot()->GetPrimarySpecialization();

    // Shadow priest prefers DPS range
    if (currentSpec == ChrSpecialization::PriestShadow)
        return OPTIMAL_DPS_RANGE;

    // Discipline (spec 0) and Holy (spec 1) prefer healing range
    return OPTIMAL_HEALING_RANGE;
}

// ============================================================================
// COMBAT BEHAVIOR PRIORITIES - Main dispatch handler
// ============================================================================

bool PriestAI::HandleCombatBehaviorPriorities(::Unit* target)
{
    if (!GetBot() || !target)
        return false;

    auto* behaviors = GetCombatBehaviors();
    if (!behaviors)
        return ExecuteNormalRotation(target);

    // Priority 1: Interrupts (Silence for Shadow spec)
    if (HandleInterruptPriority(target))
        return true;

    // Priority 2: Defensives (Dispersion, Desperate Prayer, Fade)
    if (HandleDefensivePriority())
        return true;

    // Priority 3: Positioning - Maintain max range for safety
    if (HandlePositioningPriority(target))
        return true;

    // Priority 4: Dispel - Mass Dispel, Purify for group members
    if (HandleDispelPriority())
        return true;

    // Priority 5: Target Switching - Priority targets
    if (HandleTargetSwitchPriority(target))
        return true;

    // Priority 6: Crowd Control - Psychic Scream for emergencies
    if (HandleCrowdControlPriority(target))
        return true;

    // Priority 7: AoE Decisions - Delegate to spec
    if (HandleAoEPriority(target))
        return true;

    // Priority 8: Offensive Cooldowns - Delegate to spec
    if (HandleCooldownPriority(target))
        return true;

    // Priority 9: Resource Management - Mana conservation
    if (HandleResourceManagement())
        return true;

    // Priority 10: Normal Rotation - Delegate to spec
    return ExecuteNormalRotation(target);
}

// ============================================================================
// PRIORITY HANDLER METHODS (10 handlers)
// ============================================================================

bool PriestAI::HandleInterruptPriority(::Unit* target)
{
    if (!GetBot() || !target)
        return false;

    auto* behaviors = GetCombatBehaviors();
    if (!behaviors || !behaviors->ShouldInterrupt(target))
        return false;

    // Shadow priests have Silence
    ChrSpecialization currentSpec = GetBot()->GetPrimarySpecialization();
    if (currentSpec == ChrSpecialization::PriestShadow)
    {
        Unit* interruptTarget = behaviors->GetInterruptTarget();
        if (!interruptTarget)

            interruptTarget = target;

        if (interruptTarget && interruptTarget->IsNonMeleeSpellCast(false))
        {
            // Use Silence

            if (this->IsSpellReady(SILENCE) && GameTime::GetGameTimeMS() - _lastSilence > 45000)

            {

                if (this->CastSpell(interruptTarget, SILENCE))

                {

                    _lastSilence = GameTime::GetGameTimeMS();

                    TC_LOG_DEBUG("module.playerbot.ai", "Priest {} silenced {}",

                                 GetBot()->GetName(), interruptTarget->GetName());

                    return true;

                }

            }
        }
    }

    return false;
}

bool PriestAI::HandleDefensivePriority()
{
    if (!GetBot())
        return false;

    auto* behaviors = GetCombatBehaviors();
    if (!behaviors || !behaviors->NeedsDefensive())
        return false;

    float healthPct = GetBot()->GetHealthPct();

    // Emergency: Dispersion for Shadow (90% damage reduction)
    ChrSpecialization currentSpec = GetBot()->GetPrimarySpecialization();
    if (currentSpec == ChrSpecialization::PriestShadow && healthPct < 20.0f)
    {
        if (this->IsSpellReady(DISPERSION))
        {

            if (this->CastSpell(GetBot(), DISPERSION))

            {

                TC_LOG_DEBUG("module.playerbot.ai", "Priest {} used Dispersion (emergency)", GetBot()->GetName());

                return true;

            }
        }
    }

    // Desperate Prayer (self-heal + damage reduction)
    if (healthPct < 30.0f && GameTime::GetGameTimeMS() - _lastDesperatePrayer > 90000)
    {
        if (this->IsSpellReady(DESPERATE_PRAYER))
        {

            if (this->CastSpell(GetBot(), DESPERATE_PRAYER))

            {

                _lastDesperatePrayer = GameTime::GetGameTimeMS();

                TC_LOG_DEBUG("module.playerbot.ai", "Priest {} used Desperate Prayer", GetBot()->GetName());

                return true;

            }
        }
    }

    // Fade (threat reduction)
    if (healthPct < 50.0f && GetBot()->GetThreatManager().GetThreatListSize() > 0)
    {
        if (GameTime::GetGameTimeMS() - _lastFade > 30000)
        {

            CastFade();

            return true;
        }
    }

    return false;
}

bool PriestAI::HandlePositioningPriority(::Unit* target)
{
    if (!GetBot() || !target)
        return false;

    auto* behaviors = GetCombatBehaviors();
    if (!behaviors || !behaviors->NeedsRepositioning())
        return false;

    float currentDistance = GetBot()->GetDistance(target);
    float optimalRange = GetOptimalRange(target);

    // Use Psychic Scream if enemies are too close
    if (currentDistance < 8.0f && GameTime::GetGameTimeMS() - _lastPsychicScream > PSYCHIC_SCREAM_COOLDOWN)
    {
        CastPsychicScream();
        return true;
    }

    // Maintain optimal range
    if (currentDistance < optimalRange * 0.8f || currentDistance > optimalRange * 1.2f)
    {
        Position optimalPos = GetOptimalPosition(target);
        if (optimalPos.IsPositionValid())
        {
            // PHASE 6C: Use Movement Arbiter with ROLE_POSITIONING priority (170)

            BotAI* botAI = dynamic_cast<BotAI*>(GetBot()->GetAI());

            if (botAI && botAI->GetUnifiedMovementCoordinator())

            {

                botAI->RequestPointMovement(

                    PlayerBotMovementPriority::ROLE_POSITIONING,

                    optimalPos,

                    "Priest optimal range positioning",

                    "PriestAI");

            }

            else

            {
                // FALLBACK: Direct MotionMaster if arbiter not available

                GetBot()->GetMotionMaster()->MovePoint(0, optimalPos);

            }

            return true;
        }
    }

    return false;
}

bool PriestAI::HandleDispelPriority()
{
    if (!GetBot())
        return false;

    // Check cooldown
    if (GameTime::GetGameTimeMS() - _lastDispel < DISPEL_COOLDOWN)
        return false;    // Check if there are dispellable debuffs in the group
    ::Unit* dispelTarget = GetBestDispelTarget();
    if (dispelTarget)
    {
        CastDispelMagic();
        return true;
    }

    return false;
}

bool PriestAI::HandleTargetSwitchPriority(::Unit*& target)
{
    if (!GetBot() || !target)
        return false;

    auto* behaviors = GetCombatBehaviors();
    if (!behaviors)
        return false;

    ::Unit* priorityTarget = behaviors->GetPriorityTarget();
    if (priorityTarget && priorityTarget != target)
    {
        target = priorityTarget;        TC_LOG_DEBUG("module.playerbot.ai", "Priest {} switching to priority target {}",

                     GetBot()->GetName(), target->GetName());
        return true;
    }

    return false;
}

bool PriestAI::HandleCrowdControlPriority(::Unit* target)
{
    if (!GetBot() || !target)
        return false;

    auto* behaviors = GetCombatBehaviors();
    if (!behaviors || !behaviors->ShouldUseCrowdControl())
        return false;

    // Use Psychic Scream for emergency crowd control
    if (GetBot()->GetHealthPct() < 40.0f && GameTime::GetGameTimeMS() - _lastPsychicScream > PSYCHIC_SCREAM_COOLDOWN)
    {
        CastPsychicScream();
        return true;
    }

    return false;
}

bool PriestAI::HandleAoEPriority(::Unit* target)
{
    if (!GetBot() || !target)
        return false;

    auto* behaviors = GetCombatBehaviors();
    if (!behaviors || !behaviors->ShouldAOE())
        return false;

    // AoE handling is done in normal rotation
    return false;
}

bool PriestAI::HandleCooldownPriority(::Unit* target)
{
    if (!GetBot() || !target)
        return false;

    auto* behaviors = GetCombatBehaviors();
    if (!behaviors || !behaviors->ShouldUseCooldowns())
        return false;

    // Cooldown usage is handled in normal rotation
    return false;
}

bool PriestAI::HandleResourceManagement()
{
    if (!GetBot())
        return false;

    // Check if we need mana regeneration
    if (ShouldConserveMana())
    {
        OptimizeManaUsage();
    }

    return false; // Don't block rotation, just optimize
}

bool PriestAI::ExecuteNormalRotation(::Unit* target)
{
    if (!GetBot() || !target)
        return false;

    // Use BaselineRotationManager for rotation execution
    // The refactored specializations are integrated via BaselineRotationManager
    static BaselineRotationManager rotationManager;
    return rotationManager.ExecuteBaselineRotation(GetBot(), target);
}


// ============================================================================
// SHARED UTILITIES (70 lines)
// ============================================================================

void PriestAI::UpdatePriestBuffs()
{
    if (!GetBot())
        return;

    // Maintain Inner Fire
    if (!GetBot()->HasAura(INNER_FIRE) || (GameTime::GetGameTimeMS() - _lastInnerFire > INNER_FIRE_DURATION))
    {
        CastInnerFire();
    }

    // Maintain Power Word: Fortitude
    if (!GetBot()->HasAura(POWER_WORD_FORTITUDE))
    {
        CastPowerWordFortitude();
    }

    // Maintain Divine Spirit if available
    if (GetBot()->HasSpell(DIVINE_SPIRIT) && !GetBot()->HasAura(DIVINE_SPIRIT))
    {
        this->CastSpell(GetBot(), DIVINE_SPIRIT);
    }
}

void PriestAI::CastInnerFire()
{
    if (!GetBot() || !this->IsSpellReady(INNER_FIRE))
        return;

    if (this->CastSpell(GetBot(), INNER_FIRE))
    {
        _lastInnerFire = GameTime::GetGameTimeMS();
    }
}

void PriestAI::UpdateFortitudeBuffs()
{
    if (!GetBot())
        return;

    // Check group members for fortitude
    if (Group* group = GetBot()->GetGroup())
    {
        uint32 unbuffedCount = CountUnbuffedGroupMembers(POWER_WORD_FORTITUDE);
        if (unbuffedCount > 0)
        {

            CastPowerWordFortitude();
        }
    }
}

void PriestAI::CastPowerWordFortitude()
{
    if (!GetBot() || !this->IsSpellReady(POWER_WORD_FORTITUDE))
        return;

    this->CastSpell(GetBot(), POWER_WORD_FORTITUDE);
}

bool PriestAI::HasEnoughMana(uint32 amount)
{
    return GetBot() && GetBot()->GetPower(POWER_MANA) >= int32(amount);
}

uint32 PriestAI::GetMana()
{
    return GetBot() ? GetBot()->GetPower(POWER_MANA) : 0;
}

uint32 PriestAI::GetMaxMana()
{
    return GetBot() ? GetBot()->GetMaxPower(POWER_MANA) : 0;
}

float PriestAI::GetManaPercent()
{
    uint32 maxMana = GetMaxMana();
    return maxMana > 0 ? (float(GetMana()) / float(maxMana) * 100.0f) : 0.0f;
}

void PriestAI::OptimizeManaUsage()
{
    if (!GetBot())
        return;

    float manaPercent = GetManaPercent();

    // Use mana regeneration abilities
    if (manaPercent < 20.0f)
    {
        UseManaRegeneration();
    }
}

bool PriestAI::ShouldConserveMana()
{
    return GetManaPercent() < MANA_CONSERVATION_THRESHOLD * 100.0f;
}

void PriestAI::UseManaRegeneration()
{
    if (!GetBot())
        return;

    // Use Hymn of Hope if available
    if (this->IsSpellReady(HYMN_OF_HOPE))
    {
        this->CastSpell(GetBot(), HYMN_OF_HOPE);
    }
}

void PriestAI::CastPsychicScream()
{
    if (!GetBot() || !this->IsSpellReady(PSYCHIC_SCREAM))
        return;

    if (GameTime::GetGameTimeMS() - _lastPsychicScream < PSYCHIC_SCREAM_COOLDOWN)
        return;

    if (this->CastSpell(GetBot(), PSYCHIC_SCREAM))
    {
        _lastPsychicScream = GameTime::GetGameTimeMS();
    }
}

void PriestAI::CastFade()
{
    if (!GetBot() || !this->IsSpellReady(FADE))
        return;

    if (this->CastSpell(GetBot(), FADE))
    {
        _lastFade = GameTime::GetGameTimeMS();
    }
}

void PriestAI::CastDispelMagic()
{
    if (!GetBot())
        return;

    ::Unit* target = GetBestDispelTarget();
    if (target && this->IsSpellReady(DISPEL_MAGIC))
    {
        if (GameTime::GetGameTimeMS() - _lastDispel > DISPEL_COOLDOWN)
        {

            if (this->CastSpell(target, DISPEL_MAGIC))

            {

                _lastDispel = GameTime::GetGameTimeMS();

            }
        }
    }
}

void PriestAI::CastFearWard()
{
    if (!GetBot() || !this->IsSpellReady(FEAR_WARD))
        return;

    if (GameTime::GetGameTimeMS() - _lastFearWard < FEAR_WARD_COOLDOWN)
        return;

    // Find best target for Fear Ward
    ::Unit* target = nullptr;
    if (Group* group = GetBot()->GetGroup())
    {
        target = FindGroupTank();
    }

    if (!target)
        target = GetBot();

    if (this->CastSpell(target, FEAR_WARD))
    {
        _lastFearWard = GameTime::GetGameTimeMS();
    }
}void PriestAI::CastDesperatePrayer()
{
    if (!GetBot() || !this->IsSpellReady(DESPERATE_PRAYER))
        return;

    if (this->CastSpell(GetBot(), DESPERATE_PRAYER))
    {
        _lastDesperatePrayer = GameTime::GetGameTimeMS();
    }
}

::Unit* PriestAI::GetBestHealTarget()
{
    if (!GetBot())
        return nullptr;

    ::Unit* lowestHealthTarget = nullptr;
    float lowestHealthPct = 100.0f;

    // Check self first
    if (GetBot()->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD * 100.0f)
        return GetBot();

    // Check group members
    if (Group* group = GetBot()->GetGroup())
    {
        for (GroupReference const& ref : group->GetMembers())
        {

            Player* player = ref.GetSource();
            if (!player || !player->IsAlive())

                continue;


            float healthPct = player->GetHealthPct();

            if (healthPct < lowestHealthPct && GetBot()->GetDistance(player) <= OPTIMAL_HEALING_RANGE)

            {

                lowestHealthPct = healthPct;

                lowestHealthTarget = player;

            }
        }
    }

    // Return target if they need healing
    if (lowestHealthTarget && lowestHealthPct < 80.0f)
        return lowestHealthTarget;

    return nullptr;
}

::Unit* PriestAI::GetBestDispelTarget()
{
    if (!GetBot())
        return nullptr;

    // Priority: Self > Tank > Healer > DPS

    // Check self for dispellable debuffs
    if (HasDispellableDebuff(GetBot()))
        return GetBot();

    // Check group members
    if (Group* group = GetBot()->GetGroup())
    {
        // First pass - tanks
        for (GroupReference const& ref : group->GetMembers())
        {

            Player* player = ref.GetSource();

            if (player && IsTank(player) && HasDispellableDebuff(player))

                return player;
        }

        // Second pass - healers
        for (GroupReference const& ref : group->GetMembers())
        {

            Player* player = ref.GetSource();

            if (player && IsHealer(player) && HasDispellableDebuff(player))

                return player;
        }

        // Third pass - any member
        for (GroupReference const& ref : group->GetMembers())
        {

            Player* player = ref.GetSource();
            if (player && HasDispellableDebuff(player))

                return player;
        }
    }

    return nullptr;
}

::Unit* PriestAI::FindGroupTank()
{
    if (!GetBot())
        return nullptr;

    if (Group* group = GetBot()->GetGroup())
    {
        for (GroupReference const& ref : group->GetMembers())
        {

            Player* player = ref.GetSource();

            if (player && IsTank(player))

                return player;
        }
    }

    return nullptr;
}

::Unit* PriestAI::GetLowestHealthAlly(float maxRange)
{
    if (!GetBot())
        return nullptr;

    ::Unit* lowestHealthTarget = nullptr;
    float lowestHealthPct = 100.0f;

    // Check self first
    if (GetBot()->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD * 100.0f)
        return GetBot();

    // Check group members
    if (Group* group = GetBot()->GetGroup())
    {
        for (GroupReference const& ref : group->GetMembers())
        {

            Player* player = ref.GetSource();
            if (!player || !player->IsAlive())
            continue;


            float healthPct = player->GetHealthPct();
            if (healthPct < lowestHealthPct && GetBot()->GetDistance(player) <= maxRange)

            {

                lowestHealthPct = healthPct;

                lowestHealthTarget = player;
                }
        }
    }

    // Return target if they need healing
    if (lowestHealthTarget && lowestHealthPct < 80.0f)
        return lowestHealthTarget;

    return nullptr;
}

bool PriestAI::HasDispellableDebuff(::Unit* unit)
{
    if (!unit)
        return false;

    // Check for magic debuffs
    Unit::AuraApplicationMap const& auras = unit->GetAppliedAuras();
    for (auto const& itr : auras)
    {
        Aura const* aura = itr.second->GetBase();
        if (aura && aura->GetSpellInfo()->Dispel == DISPEL_MAGIC && !itr.second->IsPositive())

            return true;
    }

    return false;
}

bool PriestAI::IsTank(::Unit* unit)
{
    if (!unit)
        return false;

    if (Player* player = unit->ToPlayer())    {
        uint8 playerClass = player->GetClass();        return (playerClass == CLASS_WARRIOR || playerClass == CLASS_PALADIN || playerClass == CLASS_DEATH_KNIGHT);
    }

    return false;
}

bool PriestAI::IsHealer(::Unit* unit)
{
    if (!unit)
        return false;

    if (Player* player = unit->ToPlayer())
    {
        uint8 playerClass = player->GetClass();        return (playerClass == CLASS_PRIEST || playerClass == CLASS_DRUID ||

                playerClass == CLASS_SHAMAN || playerClass == CLASS_PALADIN);
    }

    return false;
}

uint32 PriestAI::CountUnbuffedGroupMembers(uint32 spellId)
{
    if (!GetBot())
        return 0;

    uint32 count = 0;
    if (Group* group = GetBot()->GetGroup())
    {
        for (GroupReference const& ref : group->GetMembers())
        {

            Player* player = ref.GetSource();

            if (player && !player->HasAura(spellId))

                count++;
        }
    }

    return count;
}

bool PriestAI::IsHealingSpell(uint32 spellId)
{
    // Common healing spell IDs
    return (spellId == 2050 || spellId == 2060 || spellId == 2061 ||

            spellId == 139 || spellId == 596 || spellId == 33076 ||

            spellId == 47540 || spellId == 186263); // Heal, Greater Heal, Flash Heal, Renew, PoH, PoM, Penance, Shadow Mend
}

bool PriestAI::IsDamageSpell(uint32 spellId)
{
    // Common damage spell IDs
    return (spellId == 589 || spellId == 8092 || spellId == 15407 ||

            spellId == 34914 || spellId == 585 || spellId == 14914); // SWP, Mind Blast, Mind Flay, VT, Smite, Holy Fire
}

bool PriestAI::IsEmergencyHeal(uint32 spellId)
{
    // Fast heals for emergencies
    return (spellId == 2061 || spellId == 19236 || spellId == 47788); // Flash Heal, Desperate Prayer, Guardian Spirit
}

bool PriestAI::ShouldUseShadowProtection()
{
    // Would need encounter-specific logic
    return false;
}

uint32 PriestAI::CalculateDamageDealt(::Unit* target) const
{
    // Simplified damage tracking
    return 0;
}

uint32 PriestAI::CalculateHealingDone() const
{
    // Simplified healing tracking
    return 0;
}

uint32 PriestAI::CalculateManaUsage() const
{
    // Simplified mana tracking
    return 0;
}

void PriestAI::AnalyzeHealingEfficiency()
{
    // Performance analysis - could be expanded
}

void PriestAI::HealGroupMembers()
{
    // Post-combat group healing
    ::Unit* healTarget = GetBestHealTarget();
    if (healTarget)
    {
        // Execute healing rotation via BaselineRotationManager
        static BaselineRotationManager rotationManager;
        rotationManager.ExecuteBaselineRotation(GetBot(), healTarget);
    }
}

} // namespace Playerbot
