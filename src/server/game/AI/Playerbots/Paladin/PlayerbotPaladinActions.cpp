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

#include "PlayerbotPaladinActions.h"
#include "PlayerbotPlayerAI.h"
#include "Player.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Group.h"
#include "Log.h"

// PlayerbotPaladinAction Base Class Implementation
uint32 PlayerbotPaladinAction::GetHolyPowerAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_HOLY_POWER) : 0;
}

bool PlayerbotPaladinAction::HasEnoughHolyPower(uint32 cost) const
{
    return GetHolyPowerAmount() >= cost;
}

bool PlayerbotPaladinAction::ShouldSaveHolyPower() const
{
    return GetHolyPowerAmount() >= 4; // Save when close to cap
}

bool PlayerbotPaladinAction::IsRetribution() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(PaladinSpells::TEMPLAR_VERDICT);
}

bool PlayerbotPaladinAction::IsProtection() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(PaladinSpells::SHIELD_OF_THE_RIGHTEOUS);
}

bool PlayerbotPaladinAction::IsHoly() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(PaladinSpells::HOLY_SHOCK);
}

bool PlayerbotPaladinAction::IsInMeleeRange(Unit* target) const
{
    Player* bot = _ai->GetBot();
    if (!bot || !target)
        return false;
    return bot->GetDistance(target) <= bot->GetMeleeRange(target);
}

bool PlayerbotPaladinAction::HasDebuff(Unit* target, uint32 spellId) const
{
    return target && target->HasAura(spellId);
}

bool PlayerbotPaladinAction::HasBuff(Unit* target, uint32 spellId) const
{
    return target && target->HasAura(spellId);
}

uint32 PlayerbotPaladinAction::CountNearbyEnemies(float range) const
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

Unit* PlayerbotPaladinAction::FindHealingTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;

    // Check self first
    if (bot->GetHealthPct() < 70.0f)
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
            if (healthPct < lowestHealth && healthPct < 80.0f)
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

Unit* PlayerbotPaladinAction::GetTankTarget() const
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

            // Simple tank detection - look for protection spec or tanking role
            if (groupMember->HasSpell(PaladinSpells::SHIELD_OF_THE_RIGHTEOUS) ||
                groupMember->GetClass() == CLASS_WARRIOR || 
                groupMember->GetClass() == CLASS_DEATH_KNIGHT)
            {
                return groupMember;
            }
        }
    }

    return bot; // Default to self if no tank found
}

uint32 PlayerbotPaladinAction::GetManaPercent() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    return bot->GetPower(POWER_MANA) * 100 / bot->GetMaxPower(POWER_MANA);
}

bool PlayerbotPaladinAction::HasEnoughMana(uint32 manaCost) const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetPower(POWER_MANA) >= manaCost;
}

bool PlayerbotPaladinAction::IsHealthLow(float threshold) const
{
    Player* bot = _ai->GetBot();
    return bot && (bot->GetHealthPct() < (threshold * 100.0f));
}

bool PlayerbotPaladinAction::IsGroupMemberLow(float threshold) const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;

    Group* group = bot->GetGroup();
    if (group)
    {
        for (auto const& member : group->GetMembers())
        {
            Player* groupMember = member.GetSource();
            if (!groupMember || !groupMember->IsAlive())
                continue;

            if (groupMember->GetHealthPct() < (threshold * 100.0f))
                return true;
        }
    }

    return false;
}

bool PlayerbotPaladinAction::NeedsHealing() const
{
    return IsHealthLow(0.7f) || IsGroupMemberLow(0.6f);
}

// Judgment Action Implementation
bool PlayerbotJudgmentAction::isUseful()
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && !HasDebuff(target, PaladinSpells::JUDGMENT);
}

float PlayerbotJudgmentAction::GetRelevance() const
{
    Unit* target = _ai->GetCurrentTarget();
    if (!target || HasDebuff(target, PaladinSpells::JUDGMENT))
        return 0.0f;
    return 0.9f; // High priority - empowers next ability by 20%
}

// Crusader Strike Action Implementation
bool PlayerbotCrusaderStrikeAction::isUseful()
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && IsInMeleeRange(target) && !ShouldSaveHolyPower();
}

float PlayerbotCrusaderStrikeAction::GetRelevance() const
{
    if (GetHolyPowerAmount() >= 4)
        return 0.2f; // Lower priority when close to cap
    return 0.6f; // Standard Holy Power generator
}

// Templar's Verdict Action Implementation
bool PlayerbotTemplarVerdictAction::isUseful()
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && IsRetribution() && 
           HasEnoughHolyPower(3) && IsInMeleeRange(target) &&
           CountNearbyEnemies(8.0f) <= 2; // Single target/small groups
}

float PlayerbotTemplarVerdictAction::GetRelevance() const
{
    if (GetHolyPowerAmount() >= 5)
        return 0.95f; // Very high priority at max Holy Power
    else if (GetHolyPowerAmount() >= 3)
        return 0.85f; // High priority with 3+ Holy Power
    return 0.0f;
}

// Final Verdict Action Implementation
bool PlayerbotFinalVerdictAction::isUseful()
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && IsRetribution() && 
           HasEnoughHolyPower(5) && IsInMeleeRange(target);
}

float PlayerbotFinalVerdictAction::GetRelevance() const
{
    if (GetHolyPowerAmount() >= 5)
        return 0.98f; // Highest priority spender at 5 Holy Power
    return 0.0f;
}

// Divine Storm Action Implementation
bool PlayerbotDivineStormAction::isUseful()
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && IsRetribution() && 
           HasEnoughHolyPower(3) && CountNearbyEnemies(8.0f) >= 2;
}

float PlayerbotDivineStormAction::GetRelevance() const
{
    uint32 enemies = CountNearbyEnemies(8.0f);
    if (enemies >= 3 && GetHolyPowerAmount() >= 5)
        return 0.95f; // Very high priority for AoE situations
    else if (enemies >= 2 && GetHolyPowerAmount() >= 3)
        return 0.85f; // High priority for 2+ enemies
    return 0.0f;
}

// Blade of Justice Action Implementation
bool PlayerbotBladeOfJusticeAction::isUseful()
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && IsRetribution() && 
           IsInMeleeRange(target) && !HasExpurgation(target) && !ShouldSaveHolyPower();
}

bool PlayerbotBladeOfJusticeAction::HasExpurgation(Unit* target) const
{
    return HasDebuff(target, PaladinSpells::EXPURGATION);
}

float PlayerbotBladeOfJusticeAction::GetRelevance() const
{
    Unit* target = _ai->GetCurrentTarget();
    if (!target || HasExpurgation(target))
        return 0.0f;
    
    if (GetHolyPowerAmount() <= 2)
        return 0.8f; // High priority when low on Holy Power
    return 0.5f; // Medium priority otherwise
}

// Hammer of Wrath Action Implementation
bool PlayerbotHammerOfWrathAction::isUseful()
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && IsTargetInExecuteRange(target);
}

bool PlayerbotHammerOfWrathAction::IsTargetInExecuteRange(Unit* target) const
{
    if (!target)
        return false;
    float healthPct = float(target->GetHealth()) / float(target->GetMaxHealth());
    return healthPct <= 0.20f; // 20% execute threshold
}

float PlayerbotHammerOfWrathAction::GetRelevance() const
{
    Unit* target = _ai->GetCurrentTarget();
    if (target && IsTargetInExecuteRange(target))
        return 0.9f; // High priority in execute range
    return 0.0f;
}

// Execution Sentence Action Implementation
bool PlayerbotExecutionSentenceAction::isUseful()
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && IsRetribution();
}

float PlayerbotExecutionSentenceAction::GetRelevance() const
{
    return 0.9f; // High priority major cooldown
}

// Avenging Wrath Action Implementation
bool PlayerbotAvengingWrathAction::isUseful()
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && (IsRetribution() || IsHoly());
}

float PlayerbotAvengingWrathAction::GetRelevance() const
{
    Unit* target = _ai->GetCurrentTarget();
    if (!target)
        return 0.0f;
    
    float healthPct = float(target->GetHealth()) / float(target->GetMaxHealth());
    if (healthPct > 0.7f) // Use on healthy targets
        return 0.85f;
    return 0.6f;
}

// Wake of Ashes Action Implementation
bool PlayerbotWakeOfAshesAction::isUseful()
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && IsRetribution() && GetHolyPowerAmount() <= 2;
}

float PlayerbotWakeOfAshesAction::GetRelevance() const
{
    if (GetHolyPowerAmount() <= 1)
        return 0.8f; // High priority when low on Holy Power
    return 0.5f;
}

// Divine Toll Action Implementation
bool PlayerbotDivineTollAction::isUseful()
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && CountNearbyEnemies(15.0f) >= 1;
}

float PlayerbotDivineTollAction::GetRelevance() const
{
    uint32 enemies = CountNearbyEnemies(15.0f);
    if (enemies >= 3)
        return 0.85f; // High priority for multiple enemies
    return 0.7f;
}

// Shield of the Righteous Action Implementation
bool PlayerbotShieldOfTheRighteousAction::isUseful()
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && IsProtection() && HasEnoughHolyPower(3);
}

float PlayerbotShieldOfTheRighteousAction::GetRelevance() const
{
    if (GetHolyPowerAmount() >= 5)
        return 0.95f; // Highest priority at max Holy Power
    else if (GetHolyPowerAmount() >= 3 && IsHealthLow(0.5f))
        return 0.9f; // High priority when low health
    return 0.8f;
}

// Avenger's Shield Action Implementation
bool PlayerbotAvengersShieldAction::isUseful()
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && IsProtection();
}

float PlayerbotAvengersShieldAction::GetRelevance() const
{
    uint32 enemies = CountNearbyEnemies(15.0f);
    if (enemies >= 2)
        return 0.75f; // Good for multiple enemies
    return 0.6f;
}

// Blessed Hammer Action Implementation
bool PlayerbotBlessedHammerAction::isUseful()
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && IsProtection() && !ShouldSaveHolyPower();
}

float PlayerbotBlessedHammerAction::GetRelevance() const
{
    if (GetHolyPowerAmount() <= 2)
        return 0.7f; // Good Holy Power generator
    return 0.4f;
}

// Guardian of Ancient Kings Action Implementation
bool PlayerbotGuardianOfAncientKingsAction::isUseful()
{
    return _ai->IsInCombat() && IsProtection() && IsHealthLow(0.4f);
}

float PlayerbotGuardianOfAncientKingsAction::GetRelevance() const
{
    if (IsHealthLow(0.3f))
        return 0.95f; // Emergency defensive
    return 0.8f;
}

// Holy Shock Action Implementation
Unit* PlayerbotHolyShockAction::GetSpellTarget() const
{
    if (ShouldUseOffensively())
        return _ai->GetCurrentTarget();
    return FindBestHealingTarget();
}

bool PlayerbotHolyShockAction::isUseful()
{
    if (IsHoly() && _ai->IsInCombat())
    {
        return ShouldUseOffensively() || FindBestHealingTarget() != nullptr;
    }
    return false;
}

bool PlayerbotHolyShockAction::ShouldUseOffensively() const
{
    Unit* target = _ai->GetCurrentTarget();
    return target && !NeedsHealing();
}

Unit* PlayerbotHolyShockAction::FindBestHealingTarget() const
{
    return FindHealingTarget();
}

float PlayerbotHolyShockAction::GetRelevance() const
{
    if (NeedsHealing())
        return 0.9f; // High priority for healing
    return 0.6f; // Medium priority for offense
}

// Holy Light Action Implementation
Unit* PlayerbotHolyLightAction::GetSpellTarget() const
{
    return FindHealingTarget();
}

bool PlayerbotHolyLightAction::isUseful()
{
    return IsHoly() && FindHealingTarget() != nullptr && HasEnoughMana(1000);
}

float PlayerbotHolyLightAction::GetRelevance() const
{
    Unit* target = FindHealingTarget();
    if (!target)
        return 0.0f;
    
    float healthPct = target->GetHealthPct();
    if (healthPct < 40.0f)
        return 0.8f; // High priority for low health
    return 0.5f;
}

// Word of Glory Action Implementation
Unit* PlayerbotWordOfGloryAction::GetSpellTarget() const
{
    return FindHealingTarget();
}

bool PlayerbotWordOfGloryAction::isUseful()
{
    return FindHealingTarget() != nullptr && HasEnoughHolyPower(1);
}

float PlayerbotWordOfGloryAction::GetRelevance() const
{
    Unit* target = FindHealingTarget();
    if (!target)
        return 0.0f;
    
    float healthPct = target->GetHealthPct();
    if (healthPct < 30.0f && GetHolyPowerAmount() >= 3)
        return 0.95f; // Emergency healing
    else if (healthPct < 50.0f)
        return 0.8f; // High priority healing
    return 0.6f;
}

// Light of Dawn Action Implementation
bool PlayerbotLightOfDawnAction::isUseful()
{
    return IsHoly() && HasEnoughHolyPower(3) && CountInjuredGroupMembers() >= 2;
}

uint32 PlayerbotLightOfDawnAction::CountInjuredGroupMembers() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;

    uint32 count = 0;
    Group* group = bot->GetGroup();
    if (group)
    {
        for (auto const& member : group->GetMembers())
        {
            Player* groupMember = member.GetSource();
            if (!groupMember || !groupMember->IsAlive())
                continue;

            if (groupMember->GetHealthPct() < 80.0f)
                count++;
        }
    }
    
    return count;
}

float PlayerbotLightOfDawnAction::GetRelevance() const
{
    uint32 injured = CountInjuredGroupMembers();
    if (injured >= 4)
        return 0.9f; // High priority for group healing
    else if (injured >= 2)
        return 0.7f;
    return 0.0f;
}

// Beacon of Light Action Implementation
Unit* PlayerbotBeaconOfLightAction::GetSpellTarget() const
{
    return FindBestBeaconTarget();
}

bool PlayerbotBeaconOfLightAction::isUseful()
{
    Unit* target = FindBestBeaconTarget();
    return target && !HasBuff(target, PaladinSpells::BEACON_OF_LIGHT);
}

Unit* PlayerbotBeaconOfLightAction::FindBestBeaconTarget() const
{
    Unit* tank = GetTankTarget();
    if (tank && !HasBuff(tank, PaladinSpells::BEACON_OF_LIGHT))
        return tank;
    return nullptr;
}

float PlayerbotBeaconOfLightAction::GetRelevance() const
{
    return 0.7f; // Good utility spell
}

// Consecration Action Implementation
bool PlayerbotConsecrationAction::isUseful()
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && IsInMeleeRange(target);
}

float PlayerbotConsecrationAction::GetRelevance() const
{
    uint32 enemies = CountNearbyEnemies(8.0f);
    if (enemies >= 3)
        return 0.8f; // High priority for multiple enemies
    else if (enemies >= 1)
        return 0.6f; // Good for damage/threat
    return 0.0f;
}

// Divine Shield Action Implementation
bool PlayerbotDivineShieldAction::isUseful()
{
    return _ai->IsInCombat() && IsHealthLow(0.2f);
}

float PlayerbotDivineShieldAction::GetRelevance() const
{
    if (IsHealthLow(0.15f))
        return 0.98f; // Emergency immunity
    return 0.85f;
}

// Lay on Hands Action Implementation
Unit* PlayerbotLayOnHandsAction::GetSpellTarget() const
{
    return FindEmergencyTarget();
}

bool PlayerbotLayOnHandsAction::isUseful()
{
    return FindEmergencyTarget() != nullptr;
}

Unit* PlayerbotLayOnHandsAction::FindEmergencyTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;

    // Check self first
    if (bot->GetHealthPct() < 15.0f)
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

            if (groupMember->GetHealthPct() < 15.0f)
                return groupMember;
        }
    }

    return nullptr;
}

float PlayerbotLayOnHandsAction::GetRelevance() const
{
    return 0.99f; // Highest priority emergency heal
}

// Blessing of Freedom Action Implementation
Unit* PlayerbotBlessingOfFreedomAction::GetSpellTarget() const
{
    return FindSlowedTarget();
}

bool PlayerbotBlessingOfFreedomAction::isUseful()
{
    return FindSlowedTarget() != nullptr;
}

Unit* PlayerbotBlessingOfFreedomAction::FindSlowedTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;

    // Simple implementation - check if bot is slowed
    // In a full implementation, you would check for movement impairing effects
    return nullptr; // Placeholder
}

float PlayerbotBlessingOfFreedomAction::GetRelevance() const
{
    return 0.7f; // Good utility
}

// Cleanse Action Implementation
Unit* PlayerbotCleanseAction::GetSpellTarget() const
{
    return FindDispelTarget();
}

bool PlayerbotCleanseAction::isUseful()
{
    return FindDispelTarget() != nullptr;
}

Unit* PlayerbotCleanseAction::FindDispelTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;

    // Check self first
    if (HasDispellableDebuff(bot))
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

            if (HasDispellableDebuff(groupMember))
                return groupMember;
        }
    }

    return nullptr;
}

bool PlayerbotCleanseAction::HasDispellableDebuff(Unit* target) const
{
    // Simplified implementation - in practice would check for specific debuff types
    return false; // Placeholder
}

float PlayerbotCleanseAction::GetRelevance() const
{
    return 0.8f; // High priority for debuff removal
}

#endif // WITH_PLAYERBOTS