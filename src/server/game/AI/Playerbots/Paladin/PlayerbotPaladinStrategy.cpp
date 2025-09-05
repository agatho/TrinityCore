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

#include "PlayerbotPaladinStrategy.h"
#include "PlayerbotPaladinActions.h"
#include "PlayerbotPlayerAI.h"
#include "Player.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Group.h"
#include "Log.h"

// PlayerbotPaladinStrategy Base Implementation
PlayerbotPaladinStrategy::PlayerbotPaladinStrategy(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotStrategy(ai, name)
{
}

void PlayerbotPaladinStrategy::InitializeActions()
{
    PlayerbotStrategy::InitializeActions();
    RegisterCommonActions();
}

void PlayerbotPaladinStrategy::InitializeTriggers()
{
    PlayerbotStrategy::InitializeTriggers();
    RegisterCommonTriggers();
}

void PlayerbotPaladinStrategy::RegisterCommonActions()
{
    // Shared Paladin abilities
    RegisterAction("judgment", std::make_unique<PlayerbotJudgmentAction>(_ai));
    RegisterAction("consecration", std::make_unique<PlayerbotConsecrationAction>(_ai));
    RegisterAction("divine shield", std::make_unique<PlayerbotDivineShieldAction>(_ai));
    RegisterAction("lay on hands", std::make_unique<PlayerbotLayOnHandsAction>(_ai));
    RegisterAction("blessing of freedom", std::make_unique<PlayerbotBlessingOfFreedomAction>(_ai));
    RegisterAction("cleanse", std::make_unique<PlayerbotCleanseAction>(_ai));
    RegisterAction("word of glory", std::make_unique<PlayerbotWordOfGloryAction>(_ai));
}

void PlayerbotPaladinStrategy::RegisterCommonTriggers()
{
    // TODO: Implement proper trigger registration
    // Common triggers for all Paladin specs
    // RegisterTrigger("judgment", [this]() { return ShouldUseJudgment(); });
    // RegisterTrigger("divine shield", [this]() { return ShouldUseDivineShield(); });
    // RegisterTrigger("lay on hands", [this]() { return ShouldUseLayOnHands(); });
    // RegisterTrigger("cleanse", [this]() { 
    //     Unit* target = FindDispelTarget();
    //     return target != nullptr;
    // });
}

bool PlayerbotPaladinStrategy::ShouldUseJudgment() const
{
    Unit* target = _ai->GetCurrentTarget();
    if (!target || !_ai->IsInCombat())
        return false;

    return !target->HasAura(PaladinSpells::JUDGMENT);
}

bool PlayerbotPaladinStrategy::ShouldUseConsecration() const
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && IsInMeleeRange();
}

bool PlayerbotPaladinStrategy::ShouldUseDivineShield() const
{
    Player* bot = _ai->GetBot();
    return bot && _ai->IsInCombat() && bot->GetHealthPct() < 20.0f;
}

bool PlayerbotPaladinStrategy::ShouldUseLayOnHands() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;

    // Check self first
    if (bot->GetHealthPct() < 15.0f)
        return true;

    // Check group members
    Group* group = bot->GetGroup();
    if (group)
    {
        for (auto const& member : group->GetMembers())
        {
            Player* groupMember = member.GetSource();
            if (!groupMember || !groupMember->IsAlive())
                continue;

            if (groupMember->GetHealthPct() < 15.0f)
                return true;
        }
    }

    return false;
}

bool PlayerbotPaladinStrategy::NeedsHealing() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;

    if (bot->GetHealthPct() < 70.0f)
        return true;

    Group* group = bot->GetGroup();
    if (group)
    {
        for (auto const& member : group->GetMembers())
        {
            Player* groupMember = member.GetSource();
            if (!groupMember || !groupMember->IsAlive())
                continue;

            if (groupMember->GetHealthPct() < 60.0f)
                return true;
        }
    }

    return false;
}

bool PlayerbotPaladinStrategy::IsInMeleeRange() const
{
    Unit* target = _ai->GetCurrentTarget();
    Player* bot = _ai->GetBot();
    if (!bot || !target)
        return false;
    return bot->GetDistance(target) <= bot->GetMeleeRange(target);
}

uint32 PlayerbotPaladinStrategy::GetHolyPowerAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_HOLY_POWER) : 0;
}

bool PlayerbotPaladinStrategy::ShouldSaveHolyPower() const
{
    return GetHolyPowerAmount() >= 4; // Save when close to cap
}

uint32 PlayerbotPaladinStrategy::CountNearbyEnemies(float range) const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;

    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck checker(bot, bot, range);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, checker);
    Cell::VisitAllObjects(bot, searcher, range);

    uint32 enemyCount = 0;
    for (Unit* target : targets)
    {
        if (target->IsAlive() && bot->IsValidAttackTarget(target))
            enemyCount++;
    }
    
    return enemyCount;
}

bool PlayerbotPaladinStrategy::IsRetribution() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(PaladinSpells::TEMPLAR_VERDICT);
}

bool PlayerbotPaladinStrategy::IsProtection() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(PaladinSpells::SHIELD_OF_THE_RIGHTEOUS);
}

bool PlayerbotPaladinStrategy::IsHoly() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(PaladinSpells::HOLY_SHOCK);
}

float PlayerbotPaladinStrategy::GetActionPriority(std::string const& actionName) const
{
    // Base priorities for common actions
    if (actionName == "divine shield")
        return 0.99f;  // Emergency immunity
    else if (actionName == "lay on hands")
        return 0.98f;  // Emergency heal
    else if (actionName == "cleanse")
        return 0.85f;  // Debuff removal
    else if (actionName == "blessing of freedom")
        return 0.7f;   // Utility
    else if (actionName == "judgment")
        return 0.8f;   // High priority debuff
    else if (actionName == "consecration")
        return 0.6f;   // Good damage/threat

    return 0.5f; // Default priority
}

// PlayerbotRetributionPaladinStrategy Implementation
PlayerbotRetributionPaladinStrategy::PlayerbotRetributionPaladinStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotPaladinStrategy(ai, "retribution")
{
}

void PlayerbotRetributionPaladinStrategy::InitializeActions()
{
    PlayerbotPaladinStrategy::InitializeActions();
    RegisterRetributionActions();
}

void PlayerbotRetributionPaladinStrategy::InitializeTriggers()
{
    PlayerbotPaladinStrategy::InitializeTriggers();
    RegisterRetributionTriggers();
}

void PlayerbotRetributionPaladinStrategy::RegisterRetributionActions()
{
    // Retribution core abilities
    RegisterAction("crusader strike", std::make_unique<PlayerbotCrusaderStrikeAction>(_ai));
    RegisterAction("templar verdict", std::make_unique<PlayerbotTemplarVerdictAction>(_ai));
    RegisterAction("final verdict", std::make_unique<PlayerbotFinalVerdictAction>(_ai));
    RegisterAction("divine storm", std::make_unique<PlayerbotDivineStormAction>(_ai));
    RegisterAction("blade of justice", std::make_unique<PlayerbotBladeOfJusticeAction>(_ai));
    RegisterAction("hammer of wrath", std::make_unique<PlayerbotHammerOfWrathAction>(_ai));
    RegisterAction("execution sentence", std::make_unique<PlayerbotExecutionSentenceAction>(_ai));
    RegisterAction("avenging wrath", std::make_unique<PlayerbotAvengingWrathAction>(_ai));
    RegisterAction("wake of ashes", std::make_unique<PlayerbotWakeOfAshesAction>(_ai));
    RegisterAction("divine toll", std::make_unique<PlayerbotDivineTollAction>(_ai));
}

void PlayerbotRetributionPaladinStrategy::RegisterRetributionTriggers()
{
    // TODO: Implement proper trigger registration
    // High priority spenders
    // RegisterTrigger("final verdict", [this]() { return ShouldUseFinalVerdict(); });
    // RegisterTrigger("templar verdict", [this]() { return ShouldUseTemplarVerdict(); });
    // RegisterTrigger("divine storm", [this]() { return ShouldUseDivineStorm(); });
    
    // Generators and utilities
    // RegisterTrigger("blade of justice", [this]() { return ShouldUseBladeOfJustice(); });
    // RegisterTrigger("crusader strike", [this]() { return GetHolyPowerAmount() <= 3; });
    // RegisterTrigger("hammer of wrath", [this]() { return ShouldUseHammerOfWrath(); });
    
    // Cooldowns
    // RegisterTrigger("execution sentence", [this]() { return ShouldUseExecutionSentence(); });
    // RegisterTrigger("avenging wrath", [this]() { return ShouldUseAvengingWrath(); });
    // RegisterTrigger("wake of ashes", [this]() { return ShouldUseWakeOfAshes(); });
    // RegisterTrigger("divine toll", [this]() { return ShouldUseDivineToll(); });
}

bool PlayerbotRetributionPaladinStrategy::ShouldUseTemplarVerdict() const
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && GetHolyPowerAmount() >= 3 && !IsAoESituation();
}

bool PlayerbotRetributionPaladinStrategy::ShouldUseFinalVerdict() const
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && GetHolyPowerAmount() >= 5;
}

bool PlayerbotRetributionPaladinStrategy::ShouldUseDivineStorm() const
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && GetHolyPowerAmount() >= 3 && IsAoESituation();
}

bool PlayerbotRetributionPaladinStrategy::ShouldUseBladeOfJustice() const
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && !HasExpurgationDebuff(target) && GetHolyPowerAmount() <= 3;
}

bool PlayerbotRetributionPaladinStrategy::ShouldUseHammerOfWrath() const
{
    Unit* target = _ai->GetCurrentTarget();
    return target && IsTargetInExecuteRange(target);
}

bool PlayerbotRetributionPaladinStrategy::ShouldUseExecutionSentence() const
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat();
}

bool PlayerbotRetributionPaladinStrategy::ShouldUseWakeOfAshes() const
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && GetHolyPowerAmount() <= 2;
}

bool PlayerbotRetributionPaladinStrategy::ShouldUseDivineToll() const
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat();
}

bool PlayerbotRetributionPaladinStrategy::ShouldUseAvengingWrath() const
{
    Unit* target = _ai->GetCurrentTarget();
    if (!target || !_ai->IsInCombat())
        return false;
    
    float healthPct = float(target->GetHealth()) / float(target->GetMaxHealth());
    return healthPct > 0.7f; // Use on healthy targets for maximum benefit
}

bool PlayerbotRetributionPaladinStrategy::IsTargetInExecuteRange(Unit* target) const
{
    if (!target)
        return false;
    float healthPct = float(target->GetHealth()) / float(target->GetMaxHealth());
    return healthPct <= EXECUTE_THRESHOLD;
}

bool PlayerbotRetributionPaladinStrategy::HasExpurgationDebuff(Unit* target) const
{
    return target && target->HasAura(PaladinSpells::EXPURGATION);
}

bool PlayerbotRetributionPaladinStrategy::HasJudgmentDebuff(Unit* target) const
{
    return target && target->HasAura(PaladinSpells::JUDGMENT);
}

bool PlayerbotRetributionPaladinStrategy::IsAoESituation() const
{
    return CountNearbyEnemies(8.0f) >= 2;
}

float PlayerbotRetributionPaladinStrategy::GetActionPriority(std::string const& actionName) const
{
    // Modern Retribution rotation priorities - The War Within 11.2
    if (actionName == "hammer of wrath")
        return 0.95f;  // Highest priority in execute range
    else if (actionName == "final verdict")
        return 0.93f;  // Highest spender at 5 Holy Power
    else if (actionName == "execution sentence")
        return 0.9f;   // Major damage cooldown
    else if (actionName == "templar verdict")
        return 0.87f;  // Primary single target spender
    else if (actionName == "divine storm")
        return 0.85f;  // AoE spender (when 2+ targets)
    else if (actionName == "avenging wrath")
        return 0.83f;  // Major DPS cooldown
    else if (actionName == "blade of justice")
        return 0.8f;   // High priority generator without Expurgation
    else if (actionName == "wake of ashes")
        return 0.75f;  // Holy Power generator + buff
    else if (actionName == "divine toll")
        return 0.7f;   // AoE generator/burst
    else if (actionName == "crusader strike")
        return 0.6f;   // Basic generator
    else if (actionName == "word of glory")
        return 0.55f;  // Emergency healing
    
    return PlayerbotPaladinStrategy::GetActionPriority(actionName);
}

// PlayerbotProtectionPaladinStrategy Implementation
PlayerbotProtectionPaladinStrategy::PlayerbotProtectionPaladinStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotPaladinStrategy(ai, "protection")
{
}

void PlayerbotProtectionPaladinStrategy::InitializeActions()
{
    PlayerbotPaladinStrategy::InitializeActions();
    RegisterProtectionActions();
}

void PlayerbotProtectionPaladinStrategy::InitializeTriggers()
{
    PlayerbotPaladinStrategy::InitializeTriggers();
    RegisterProtectionTriggers();
}

void PlayerbotProtectionPaladinStrategy::RegisterProtectionActions()
{
    // Protection core abilities
    RegisterAction("shield of the righteous", std::make_unique<PlayerbotShieldOfTheRighteousAction>(_ai));
    RegisterAction("avengers shield", std::make_unique<PlayerbotAvengersShieldAction>(_ai));
    RegisterAction("blessed hammer", std::make_unique<PlayerbotBlessedHammerAction>(_ai));
    RegisterAction("guardian of ancient kings", std::make_unique<PlayerbotGuardianOfAncientKingsAction>(_ai));
}

void PlayerbotProtectionPaladinStrategy::RegisterProtectionTriggers()
{
    // TODO: Implement proper trigger registration
    // Priority: Shield of the Righteous for defense
    // RegisterTrigger("shield of the righteous", [this]() { return ShouldUseShieldOfTheRighteous(); });
    // RegisterTrigger("word of glory", [this]() { return ShouldUseWordOfGlory(); });
    
    // Threat and utility
    // RegisterTrigger("avengers shield", [this]() { return ShouldUseAvengersShield(); });
    // RegisterTrigger("blessed hammer", [this]() { return ShouldUseBlessedHammer(); });
    
    // Defensive cooldowns
    // RegisterTrigger("guardian of ancient kings", [this]() { return ShouldUseGuardianOfAncientKings(); });
}

bool PlayerbotProtectionPaladinStrategy::ShouldUseShieldOfTheRighteous() const
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && GetHolyPowerAmount() >= 3;
}

bool PlayerbotProtectionPaladinStrategy::ShouldUseAvengersShield() const
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat();
}

bool PlayerbotProtectionPaladinStrategy::ShouldUseBlessedHammer() const
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && GetHolyPowerAmount() <= 3;
}

bool PlayerbotProtectionPaladinStrategy::ShouldUseWordOfGlory() const
{
    Player* bot = _ai->GetBot();
    return bot && GetHolyPowerAmount() >= 1 && bot->GetHealthPct() < 50.0f;
}

bool PlayerbotProtectionPaladinStrategy::ShouldUseGuardianOfAncientKings() const
{
    Player* bot = _ai->GetBot();
    return bot && _ai->IsInCombat() && bot->GetHealthPct() < DEFENSIVE_COOLDOWN_THRESHOLD * 100.0f;
}

bool PlayerbotProtectionPaladinStrategy::ShouldUseEyeOfTyr() const
{
    Player* bot = _ai->GetBot();
    return bot && _ai->IsInCombat() && bot->GetHealthPct() < 60.0f;
}

bool PlayerbotProtectionPaladinStrategy::ShouldUseArdentDefender() const
{
    Player* bot = _ai->GetBot();
    return bot && _ai->IsInCombat() && bot->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD * 100.0f;
}

bool PlayerbotProtectionPaladinStrategy::IsInTankingRole() const
{
    return _ai->HasStrategy("tank") || _ai->HasStrategy("protection");
}

bool PlayerbotProtectionPaladinStrategy::HasAggro() const
{
    Unit* target = _ai->GetCurrentTarget();
    Player* bot = _ai->GetBot();
    return target && bot && target->GetVictim() == bot;
}

bool PlayerbotProtectionPaladinStrategy::NeedsEmergencyHealing() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD * 100.0f;
}

float PlayerbotProtectionPaladinStrategy::GetDamageTakenRecently() const
{
    // Simplified implementation - would track damage over time window
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0.0f;
    
    float missingHealth = 100.0f - bot->GetHealthPct();
    return missingHealth; // Rough approximation
}

float PlayerbotProtectionPaladinStrategy::GetActionPriority(std::string const& actionName) const
{
    // Modern Protection rotation priorities - The War Within 11.2
    if (actionName == "shield of the righteous")
        return 0.95f;  // Highest priority for defense
    else if (actionName == "word of glory")
        return 0.9f;   // High priority healing when low
    else if (actionName == "guardian of ancient kings")
        return 0.85f;  // Major defensive cooldown
    else if (actionName == "avengers shield")
        return 0.75f;  // Good threat/interrupt
    else if (actionName == "blessed hammer")
        return 0.7f;   // Holy Power generator
    
    return PlayerbotPaladinStrategy::GetActionPriority(actionName);
}

// PlayerbotHolyPaladinStrategy Implementation
PlayerbotHolyPaladinStrategy::PlayerbotHolyPaladinStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotPaladinStrategy(ai, "holy")
{
}

void PlayerbotHolyPaladinStrategy::InitializeActions()
{
    PlayerbotPaladinStrategy::InitializeActions();
    RegisterHolyActions();
}

void PlayerbotHolyPaladinStrategy::InitializeTriggers()
{
    PlayerbotPaladinStrategy::InitializeTriggers();
    RegisterHolyTriggers();
}

void PlayerbotHolyPaladinStrategy::RegisterHolyActions()
{
    // Holy core abilities
    RegisterAction("holy shock", std::make_unique<PlayerbotHolyShockAction>(_ai));
    RegisterAction("holy light", std::make_unique<PlayerbotHolyLightAction>(_ai));
    RegisterAction("light of dawn", std::make_unique<PlayerbotLightOfDawnAction>(_ai));
    RegisterAction("beacon of light", std::make_unique<PlayerbotBeaconOfLightAction>(_ai));
    
    // Override word of glory for Holy spec priority
    RegisterAction("word of glory", std::make_unique<PlayerbotWordOfGloryAction>(_ai));
}

void PlayerbotHolyPaladinStrategy::RegisterHolyTriggers()
{
    // TODO: Implement proper trigger registration
    // Healing priorities
    // RegisterTrigger("word of glory", [this]() { 
    //     return ShouldUseWordOfGlory() && GetHolyPowerAmount() >= 1; 
    // });
    // RegisterTrigger("light of dawn", [this]() { return ShouldUseLightOfDawn(); });
    // RegisterTrigger("holy shock", [this]() { return ShouldUseHolyShock(); });
    // RegisterTrigger("holy light", [this]() { return ShouldUseHolyLight(); });
    
    // Utility and buffs
    // RegisterTrigger("beacon of light", [this]() { return ShouldUseBeaconOfLight(); });
    // RegisterTrigger("avenging wrath", [this]() { return ShouldUseAvengingWrath(); });
}

bool PlayerbotHolyPaladinStrategy::ShouldUseHolyShock() const
{
    return _ai->IsInCombat() && (FindBestHealingTarget() != nullptr || ShouldDPS());
}

bool PlayerbotHolyPaladinStrategy::ShouldUseHolyLight() const
{
    Unit* target = FindBestHealingTarget();
    return target && target->GetHealthPct() < HEALING_THRESHOLD * 100.0f;
}

bool PlayerbotHolyPaladinStrategy::ShouldUseFlashOfLight() const
{
    Unit* target = FindBestHealingTarget();
    return target && target->GetHealthPct() < EMERGENCY_THRESHOLD * 100.0f;
}

bool PlayerbotHolyPaladinStrategy::ShouldUseWordOfGlory() const
{
    Unit* target = FindBestHealingTarget();
    return target && GetHolyPowerAmount() >= 1 && target->GetHealthPct() < HEALING_THRESHOLD * 100.0f;
}

bool PlayerbotHolyPaladinStrategy::ShouldUseLightOfDawn() const
{
    return GetHolyPowerAmount() >= 3 && ShouldUseAoEHealing();
}

bool PlayerbotHolyPaladinStrategy::ShouldUseBeaconOfLight() const
{
    Unit* target = FindBeaconTarget();
    return target && !target->HasAura(PaladinSpells::BEACON_OF_LIGHT);
}

bool PlayerbotHolyPaladinStrategy::ShouldUseCleanse() const
{
    return FindCleanseTarget() != nullptr;
}

bool PlayerbotHolyPaladinStrategy::ShouldUseBlessingOfFreedom() const
{
    return FindBlessingTarget() != nullptr;
}

bool PlayerbotHolyPaladinStrategy::ShouldUseBlessingOfProtection() const
{
    Unit* target = FindBlessingTarget();
    return target && target->GetHealthPct() < EMERGENCY_THRESHOLD * 100.0f;
}

bool PlayerbotHolyPaladinStrategy::ShouldUseAvengingWrath() const
{
    return _ai->IsInCombat() && IsGroupDamageHigh();
}

Unit* PlayerbotHolyPaladinStrategy::FindBestHealingTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;

    // Check self first
    if (bot->GetHealthPct() < HEALING_THRESHOLD * 100.0f)
        return bot;

    // Check group members
    Group* group = bot->GetGroup();
    if (group)
    {
        Unit* lowestTarget = nullptr;
        float lowestHealth = 100.0f;

        for (auto const& member : group->GetMembers())
        {
            Player* groupMember = member.GetSource();
            if (!groupMember || !groupMember->IsAlive() || groupMember->GetDistance(bot) > 30.0f)
                continue;

            float healthPct = groupMember->GetHealthPct();
            if (healthPct < lowestHealth && healthPct < HEALING_THRESHOLD * 100.0f)
            {
                lowestHealth = healthPct;
                lowestTarget = groupMember;
            }
        }

        if (lowestTarget)
            return lowestTarget;
    }

    return nullptr;
}

Unit* PlayerbotHolyPaladinStrategy::FindEmergencyTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;

    // Check self first
    if (bot->GetHealthPct() < EMERGENCY_THRESHOLD * 100.0f)
        return bot;

    // Check group members
    Group* group = bot->GetGroup();
    if (group)
    {
        for (auto const& member : group->GetMembers())
        {
            Player* groupMember = member.GetSource();
            if (!groupMember || !groupMember->IsAlive())
                continue;

            if (groupMember->GetHealthPct() < EMERGENCY_THRESHOLD * 100.0f)
                return groupMember;
        }
    }

    return nullptr;
}

Unit* PlayerbotHolyPaladinStrategy::FindBeaconTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;

    Group* group = bot->GetGroup();
    if (group)
    {
        for (auto const& member : group->GetMembers())
        {
            Player* groupMember = member.GetSource();
            if (!groupMember || !groupMember->IsAlive())
                continue;

            // Look for tank or high-threat target
            if (groupMember->HasSpell(PaladinSpells::SHIELD_OF_THE_RIGHTEOUS) ||
                groupMember->GetClass() == CLASS_WARRIOR || 
                groupMember->GetClass() == CLASS_DEATH_KNIGHT)
            {
                return groupMember;
            }
        }
    }

    return bot; // Default to self
}

Unit* PlayerbotHolyPaladinStrategy::FindCleanseTarget() const
{
    // Simplified implementation - would check for dispellable debuffs
    return nullptr;
}

Unit* PlayerbotHolyPaladinStrategy::FindBlessingTarget() const
{
    // Simplified implementation - would check for slowed/rooted targets
    return nullptr;
}

uint32 PlayerbotHolyPaladinStrategy::CountInjuredMembers(float threshold) const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;

    uint32 count = 0;
    
    if (bot->GetHealthPct() < threshold * 100.0f)
        count++;

    Group* group = bot->GetGroup();
    if (group)
    {
        for (auto const& member : group->GetMembers())
        {
            Player* groupMember = member.GetSource();
            if (!groupMember || !groupMember->IsAlive() || groupMember == bot)
                continue;

            if (groupMember->GetHealthPct() < threshold * 100.0f)
                count++;
        }
    }

    return count;
}

bool PlayerbotHolyPaladinStrategy::IsGroupDamageHigh() const
{
    return CountInjuredMembers(0.8f) >= 3; // 3+ members below 80% health
}

bool PlayerbotHolyPaladinStrategy::ShouldUseAoEHealing() const
{
    return CountInjuredMembers(AOE_HEALING_THRESHOLD) >= 3;
}

bool PlayerbotHolyPaladinStrategy::ShouldDPS() const
{
    Unit* target = _ai->GetCurrentTarget();
    return target && !NeedsHealing() && CountInjuredMembers(HEALING_THRESHOLD) == 0;
}

float PlayerbotHolyPaladinStrategy::GetActionPriority(std::string const& actionName) const
{
    // Modern Holy rotation priorities - The War Within 11.2
    if (actionName == "word of glory" && FindEmergencyTarget())
        return 0.95f;  // Emergency healing
    else if (actionName == "holy shock")
        return 0.9f;   // Core ability for healing/Holy Power
    else if (actionName == "light of dawn" && ShouldUseAoEHealing())
        return 0.85f;  // AoE healing when needed
    else if (actionName == "word of glory")
        return 0.8f;   // Single target Holy Power healing
    else if (actionName == "holy light")
        return 0.75f;  // Mana-efficient healing
    else if (actionName == "beacon of light")
        return 0.7f;   // Important utility
    else if (actionName == "avenging wrath" && IsGroupDamageHigh())
        return 0.85f;  // Boost during high damage
    
    return PlayerbotPaladinStrategy::GetActionPriority(actionName);
}

#endif // WITH_PLAYERBOTS