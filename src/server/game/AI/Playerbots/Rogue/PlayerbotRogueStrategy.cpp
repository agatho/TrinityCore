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

#include "PlayerbotRogueStrategy.h"
#include "PlayerbotRogueActions.h"
#include "PlayerbotPlayerAI.h"
#include "Player.h"
#include "SpellAuraEffects.h"
#include "SpellHistory.h"
#include "Group.h"

PlayerbotRogueStrategy::PlayerbotRogueStrategy(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotStrategy(ai, name) {}

void PlayerbotRogueStrategy::InitializeActions()
{
    RegisterCommonActions();
}

void PlayerbotRogueStrategy::InitializeTriggers()
{
    RegisterCommonTriggers();
}

void PlayerbotRogueStrategy::RegisterCommonActions()
{
    // Utility actions
    RegisterAction("stealth", std::make_unique<CastStealth>(_ai));
    RegisterAction("vanish", std::make_unique<CastVanish>(_ai));
    RegisterAction("kick", std::make_unique<CastKick>(_ai));
    RegisterAction("feint", std::make_unique<CastFeint>(_ai));
    RegisterAction("cloak of shadows", std::make_unique<CastCloakOfShadows>(_ai));
    RegisterAction("evasion", std::make_unique<CastEvasion>(_ai));
    RegisterAction("crimson vial", std::make_unique<CastCrimsonVial>(_ai));
    RegisterAction("sprint", std::make_unique<CastSprint>(_ai));
}

void PlayerbotRogueStrategy::RegisterCommonTriggers()
{
    // TODO: Implement trigger system when base trigger framework is available
}

float PlayerbotRogueStrategy::GetActionPriority(std::string const& actionName) const
{
    // Common utility priorities
    if (actionName == "kick" && ShouldUseKick())
        return 0.99f;
    else if (actionName == "feint" && ShouldUseFeint())
        return 0.95f;
    else if (actionName == "cloak of shadows" && ShouldUseCloakOfShadows())
        return 0.93f;
    else if (actionName == "evasion" && ShouldUseEvasion())
        return 0.91f;
    else if (actionName == "crimson vial" && ShouldUseCrimsonVial())
        return 0.89f;
    else if (actionName == "vanish" && ShouldUseVanish())
        return 0.87f;
    else if (actionName == "stealth" && ShouldUseStealth())
        return 0.85f;
    else if (actionName == "sprint" && ShouldUseSprint())
        return 0.83f;
    
    return 0.0f;
}

bool PlayerbotRogueStrategy::ShouldUseStealth() const
{
    Player* bot = _ai->GetBot();
    if (!bot || bot->IsInCombat() || bot->HasStealthAura())
        return false;
    
    return bot->HasSpell(RogueSpells::STEALTH) && 
           !bot->GetSpellHistory()->HasCooldown(RogueSpells::STEALTH);
}

bool PlayerbotRogueStrategy::ShouldUseVanish() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !bot->IsInCombat() || bot->HasStealthAura())
        return false;
    
    // Use vanish when low health or to reset combat
    return bot->GetHealthPct() < 25.0f && 
           bot->HasSpell(RogueSpells::VANISH) && 
           !bot->GetSpellHistory()->HasCooldown(RogueSpells::VANISH);
}

bool PlayerbotRogueStrategy::ShouldUseKick() const
{
    Unit* target = FindBestTarget();
    if (!target || !target->IsNonMeleeSpellCast(false))
        return false;
    
    Player* bot = _ai->GetBot();
    return bot->IsWithinMeleeRange(target) && 
           bot->HasSpell(RogueSpells::KICK) && 
           !bot->GetSpellHistory()->HasCooldown(RogueSpells::KICK);
}

bool PlayerbotRogueStrategy::ShouldUseFeint() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !bot->IsInCombat())
        return false;
    
    return bot->GetHealthPct() < 50.0f && 
           bot->HasSpell(RogueSpells::FEINT) && 
           !bot->GetSpellHistory()->HasCooldown(RogueSpells::FEINT);
}

bool PlayerbotRogueStrategy::ShouldUseCloakOfShadows() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !bot->IsInCombat())
        return false;
    
    // Use against magical debuffs or when low health
    return (bot->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE) || bot->GetHealthPct() < 40.0f) &&
           bot->HasSpell(RogueSpells::CLOAK_OF_SHADOWS) && 
           !bot->GetSpellHistory()->HasCooldown(RogueSpells::CLOAK_OF_SHADOWS);
}

bool PlayerbotRogueStrategy::ShouldUseEvasion() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !bot->IsInCombat())
        return false;
    
    return bot->GetHealthPct() < 35.0f && CountNearbyEnemies(8.0f) >= 2 &&
           bot->HasSpell(RogueSpells::EVASION) && 
           !bot->GetSpellHistory()->HasCooldown(RogueSpells::EVASION);
}

bool PlayerbotRogueStrategy::ShouldUseCrimsonVial() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetHealthPct() < 60.0f && 
           bot->HasSpell(RogueSpells::CRIMSON_VIAL) && 
           !bot->GetSpellHistory()->HasCooldown(RogueSpells::CRIMSON_VIAL);
}

bool PlayerbotRogueStrategy::ShouldUseSprint() const
{
    Player* bot = _ai->GetBot();
    Unit* target = FindBestTarget();
    
    if (!bot || !target)
        return false;
    
    // Use sprint to get to target or escape
    float distance = bot->GetDistance(target);
    return (distance > 10.0f || bot->GetHealthPct() < 30.0f) &&
           bot->HasSpell(RogueSpells::SPRINT) && 
           !bot->GetSpellHistory()->HasCooldown(RogueSpells::SPRINT);
}

uint32 PlayerbotRogueStrategy::GetEnergyPercent() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 currentEnergy = bot->GetPower(POWER_ENERGY);
    uint32 maxEnergy = bot->GetMaxPower(POWER_ENERGY);
    
    return maxEnergy > 0 ? (currentEnergy * 100 / maxEnergy) : 0;
}

uint32 PlayerbotRogueStrategy::GetComboPoints() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_COMBO_POINTS) : 0;
}

uint32 PlayerbotRogueStrategy::GetComboPoints(Unit* target) const
{
    Player* bot = _ai->GetBot();
    return (bot && target) ? bot->GetPower(POWER_COMBO_POINTS) : 0;
}

bool PlayerbotRogueStrategy::HasMaxComboPoints() const
{
    return GetComboPoints() >= 5;
}

bool PlayerbotRogueStrategy::IsNearMaxComboPoints(uint32 threshold) const
{
    return GetComboPoints() >= threshold;
}

bool PlayerbotRogueStrategy::isAssassination() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(RogueSpells::MUTILATE);
}

bool PlayerbotRogueStrategy::isOutlaw() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(RogueSpells::SINISTER_STRIKE);
}

bool PlayerbotRogueStrategy::isSubtlety() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(RogueSpells::SHADOWSTRIKE);
}

Unit* PlayerbotRogueStrategy::FindBestTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;
        
    Unit* currentTarget = bot->GetSelectedUnit();
    if (currentTarget && currentTarget->IsAlive() && bot->IsValidAttackTarget(currentTarget))
        return currentTarget;
    
    Unit* target = bot->GetVictim();
    if (target && target->IsAlive())
        return target;
    
    return nullptr;
}

bool PlayerbotRogueStrategy::IsInCombat() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->IsInCombat();
}

bool PlayerbotRogueStrategy::IsStealthed() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasStealthAura();
}

bool PlayerbotRogueStrategy::IsBehindTarget(Unit* target) const
{
    Player* bot = _ai->GetBot();
    return bot && target && bot->isInBack(target);
}

uint32 PlayerbotRogueStrategy::CountNearbyEnemies(float range) const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 count = 0;
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot, range);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, check);
    Cell::VisitAllObjects(bot, searcher, range);
    
    for (Unit* unit : targets)
    {
        if (unit->IsAlive() && bot->IsValidAttackTarget(unit))
            count++;
    }
    
    return count;
}

bool PlayerbotRogueStrategy::ShouldUseAoE() const
{
    return CountNearbyEnemies(8.0f) >= 2;
}

// Assassination Rogue Strategy
PlayerbotAssassinationRogueStrategy::PlayerbotAssassinationRogueStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotRogueStrategy(ai, "assassination") {}

void PlayerbotAssassinationRogueStrategy::InitializeActions()
{
    PlayerbotRogueStrategy::InitializeActions();
    RegisterAssassinationActions();
}

void PlayerbotAssassinationRogueStrategy::InitializeTriggers()
{
    PlayerbotRogueStrategy::InitializeTriggers();
    RegisterAssassinationTriggers();
}

void PlayerbotAssassinationRogueStrategy::RegisterAssassinationActions()
{
    RegisterAction("mutilate", std::make_unique<CastMutilate>(_ai));
    RegisterAction("envenom", std::make_unique<CastEnvenom>(_ai));
    RegisterAction("garrote", std::make_unique<CastGarrote>(_ai));
    RegisterAction("rupture", std::make_unique<CastRupture>(_ai));
    RegisterAction("vendetta", std::make_unique<CastVendetta>(_ai));
    RegisterAction("deathmark", std::make_unique<CastDeathmark>(_ai));
    RegisterAction("kingsbane", std::make_unique<CastKingsbane>(_ai));
    RegisterAction("shiv", std::make_unique<CastShiv>(_ai));
    RegisterAction("fan of knives", std::make_unique<CastFanOfKnives>(_ai));
}

void PlayerbotAssassinationRogueStrategy::RegisterAssassinationTriggers()
{
    // TODO: Implement when trigger system is available
}

float PlayerbotAssassinationRogueStrategy::GetActionPriority(std::string const& actionName) const
{
    // Check parent priorities first
    float parentPriority = PlayerbotRogueStrategy::GetActionPriority(actionName);
    if (parentPriority > 0.0f)
        return parentPriority;
    
    // Assassination rotation priorities
    if (actionName == "garrote" && ShouldMaintainGarrote())
        return 0.95f;
    else if (actionName == "rupture" && ShouldMaintainRupture())
        return 0.9f;
    else if (actionName == "deathmark" && ShouldUseDeathmark())
        return 0.88f;
    else if (actionName == "kingsbane" && ShouldUseKingsbane())
        return 0.86f;
    else if (actionName == "vendetta" && ShouldUseVendetta())
        return 0.84f;
    else if (actionName == "envenom" && ShouldUseEnvenom())
        return 0.82f;
    else if (actionName == "shiv" && ShouldUseShiv())
        return 0.8f;
    else if (actionName == "fan of knives" && ShouldUseFanOfKnives())
        return 0.75f;
    else if (actionName == "mutilate" && ShouldUseMutilate())
        return 0.7f;
    
    return 0.0f;
}

bool PlayerbotAssassinationRogueStrategy::ShouldMaintainGarrote() const
{
    Unit* target = FindBestTarget();
    if (!target)
        return false;
    
    Player* bot = _ai->GetBot();
    if (!bot->IsWithinMeleeRange(target))
        return false;
    
    // Prefer from stealth or if DoT needs refreshing
    return IsStealthed() || ShouldRefreshGarrote(target);
}

bool PlayerbotAssassinationRogueStrategy::ShouldMaintainRupture() const
{
    Unit* target = FindBestTarget();
    if (!target || GetComboPoints() < RUPTURE_MIN_CP)
        return false;
    
    return ShouldRefreshRupture(target);
}

bool PlayerbotAssassinationRogueStrategy::ShouldUseMutilate() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !bot->IsWithinMeleeRange(target))
        return false;
    
    return GetComboPoints() < 4 && bot->GetPower(POWER_ENERGY) >= 55;
}

bool PlayerbotAssassinationRogueStrategy::ShouldUseEnvenom() const
{
    Unit* target = FindBestTarget();
    if (!target || GetComboPoints() < ENVENOM_MIN_CP)
        return false;
    
    Player* bot = _ai->GetBot();
    return bot && bot->IsWithinMeleeRange(target);
}

bool PlayerbotAssassinationRogueStrategy::ShouldUseVendetta() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return target->GetHealthPct() > BURST_HEALTH_THRESHOLD && 
           bot->IsWithinMeleeRange(target) &&
           bot->HasSpell(RogueSpells::VENDETTA) &&
           !bot->GetSpellHistory()->HasCooldown(RogueSpells::VENDETTA);
}

bool PlayerbotAssassinationRogueStrategy::ShouldUseDeathmark() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return IsInBurstWindow() &&
           bot->IsWithinMeleeRange(target) &&
           bot->HasSpell(RogueSpells::DEATHMARK) &&
           !bot->GetSpellHistory()->HasCooldown(RogueSpells::DEATHMARK);
}

bool PlayerbotAssassinationRogueStrategy::ShouldUseKingsbane() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return IsInBurstWindow() &&
           bot->IsWithinMeleeRange(target) &&
           bot->HasSpell(RogueSpells::KINGSBANE) &&
           !bot->GetSpellHistory()->HasCooldown(RogueSpells::KINGSBANE);
}

bool PlayerbotAssassinationRogueStrategy::ShouldUseShiv() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !bot->IsWithinMeleeRange(target))
        return false;
    
    return GetComboPoints() < 4 &&
           bot->HasSpell(RogueSpells::SHIV) &&
           !bot->GetSpellHistory()->HasCooldown(RogueSpells::SHIV);
}

bool PlayerbotAssassinationRogueStrategy::ShouldUseFanOfKnives() const
{
    return CountNearbyEnemies(8.0f) >= 2;
}

bool PlayerbotAssassinationRogueStrategy::IsInBurstWindow() const
{
    Player* bot = _ai->GetBot();
    return bot && (bot->HasAura(RogueSpells::VENDETTA) || 
                   bot->HasAura(RogueSpells::COLD_BLOOD));
}

bool PlayerbotAssassinationRogueStrategy::HasGarroteOnTarget(Unit* target) const
{
    return target && target->HasAura(RogueSpells::GARROTE, _ai->GetBot()->GetGUID());
}

bool PlayerbotAssassinationRogueStrategy::HasRuptureOnTarget(Unit* target) const
{
    return target && target->HasAura(RogueSpells::RUPTURE, _ai->GetBot()->GetGUID());
}

bool PlayerbotAssassinationRogueStrategy::ShouldRefreshGarrote(Unit* target) const
{
    if (!HasGarroteOnTarget(target))
        return true;
    
    return GetGarroteRemainingDuration(target) <= DOT_REFRESH_THRESHOLD;
}

bool PlayerbotAssassinationRogueStrategy::ShouldRefreshRupture(Unit* target) const
{
    if (!HasRuptureOnTarget(target))
        return true;
    
    return GetRuptureRemainingDuration(target) <= DOT_REFRESH_THRESHOLD;
}

float PlayerbotAssassinationRogueStrategy::GetGarroteRemainingDuration(Unit* target) const
{
    if (Aura* aura = target->GetAura(RogueSpells::GARROTE, _ai->GetBot()->GetGUID()))
        return float(aura->GetDuration()) / 1000.0f;
    return 0.0f;
}

float PlayerbotAssassinationRogueStrategy::GetRuptureRemainingDuration(Unit* target) const
{
    if (Aura* aura = target->GetAura(RogueSpells::RUPTURE, _ai->GetBot()->GetGUID()))
        return float(aura->GetDuration()) / 1000.0f;
    return 0.0f;
}

// Outlaw Rogue Strategy
PlayerbotOutlawRogueStrategy::PlayerbotOutlawRogueStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotRogueStrategy(ai, "outlaw") {}

void PlayerbotOutlawRogueStrategy::InitializeActions()
{
    PlayerbotRogueStrategy::InitializeActions();
    RegisterOutlawActions();
}

void PlayerbotOutlawRogueStrategy::InitializeTriggers()
{
    PlayerbotRogueStrategy::InitializeTriggers();
    RegisterOutlawTriggers();
}

void PlayerbotOutlawRogueStrategy::RegisterOutlawActions()
{
    RegisterAction("sinister strike", std::make_unique<CastSinisterStrike>(_ai));
    RegisterAction("dispatch", std::make_unique<CastDispatch>(_ai));
    RegisterAction("pistol shot", std::make_unique<CastPistolShot>(_ai));
    RegisterAction("between the eyes", std::make_unique<CastBetweenTheEyes>(_ai));
    RegisterAction("roll the bones", std::make_unique<CastRollTheBones>(_ai));
    RegisterAction("adrenaline rush", std::make_unique<CastAdrenalineRush>(_ai));
    RegisterAction("blade flurry", std::make_unique<CastBladeFlurry>(_ai));
    RegisterAction("killing spree", std::make_unique<CastKillingSpree>(_ai));
}

void PlayerbotOutlawRogueStrategy::RegisterOutlawTriggers()
{
    // TODO: Implement when trigger system is available
}

float PlayerbotOutlawRogueStrategy::GetActionPriority(std::string const& actionName) const
{
    // Check parent priorities first
    float parentPriority = PlayerbotRogueStrategy::GetActionPriority(actionName);
    if (parentPriority > 0.0f)
        return parentPriority;
    
    // Outlaw rotation priorities
    if (actionName == "roll the bones" && ShouldRollTheBones())
        return 0.95f;
    else if (actionName == "adrenaline rush" && ShouldUseAdrenalineRush())
        return 0.9f;
    else if (actionName == "killing spree" && ShouldUseKillingSpree())
        return 0.88f;
    else if (actionName == "blade flurry" && ShouldUseBladeFlurry())
        return 0.86f;
    else if (actionName == "between the eyes" && ShouldUseBetweenTheEyes())
        return 0.84f;
    else if (actionName == "dispatch" && ShouldUseDispatch())
        return 0.82f;
    else if (actionName == "pistol shot" && ShouldUsePistolShot())
        return 0.8f;
    else if (actionName == "sinister strike" && ShouldUseSinisterStrike())
        return 0.7f;
    
    return 0.0f;
}

bool PlayerbotOutlawRogueStrategy::ShouldRollTheBones() const
{
    return !HasRollTheBonesBuffs() || ShouldRerollBones();
}

bool PlayerbotOutlawRogueStrategy::ShouldRerollBones() const
{
    return HasRollTheBonesBuffs() && CountRollTheBonesBuffs() < MIN_ROLL_BONES_BUFFS;
}

bool PlayerbotOutlawRogueStrategy::HasGoodRollTheBonesBuffs() const
{
    return CountRollTheBonesBuffs() >= MIN_ROLL_BONES_BUFFS;
}

uint32 PlayerbotOutlawRogueStrategy::CountRollTheBonesBuffs() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 count = 0;
    if (bot->HasAura(RogueSpells::BROADSIDE)) count++;
    if (bot->HasAura(RogueSpells::BURIED_TREASURE)) count++;
    if (bot->HasAura(RogueSpells::GRAND_MELEE)) count++;
    if (bot->HasAura(RogueSpells::RUTHLESS_PRECISION)) count++;
    if (bot->HasAura(RogueSpells::SKULL_AND_CROSSBONES)) count++;
    if (bot->HasAura(RogueSpells::TRUE_BEARING)) count++;
    
    return count;
}

bool PlayerbotOutlawRogueStrategy::HasRollTheBonesBuffs() const
{
    return CountRollTheBonesBuffs() > 0;
}

bool PlayerbotOutlawRogueStrategy::ShouldUseSinisterStrike() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !bot->IsWithinMeleeRange(target))
        return false;
    
    return GetComboPoints() < 4 && bot->GetPower(POWER_ENERGY) >= 45;
}

bool PlayerbotOutlawRogueStrategy::ShouldUseDispatch() const
{
    Unit* target = FindBestTarget();
    if (!target || GetComboPoints() < DISPATCH_MIN_CP)
        return false;
    
    return IsTargetInExecuteRange(target);
}

bool PlayerbotOutlawRogueStrategy::ShouldUsePistolShot() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return HasOpportunityProc() || !bot->IsWithinMeleeRange(target);
}

bool PlayerbotOutlawRogueStrategy::ShouldUseBetweenTheEyes() const
{
    Unit* target = FindBestTarget();
    return target && GetComboPoints() >= BTE_MIN_CP;
}

bool PlayerbotOutlawRogueStrategy::ShouldUseAdrenalineRush() const
{
    Player* bot = _ai->GetBot();
    Unit* target = FindBestTarget();
    
    if (!bot || !target)
        return false;
    
    return ShouldUseBurstCooldowns() &&
           bot->HasSpell(RogueSpells::ADRENALINE_RUSH) &&
           !bot->GetSpellHistory()->HasCooldown(RogueSpells::ADRENALINE_RUSH);
}

bool PlayerbotOutlawRogueStrategy::ShouldUseBladeFlurry() const
{
    return ShouldUseAoERotation() && !HasBladeFlurry();
}

bool PlayerbotOutlawRogueStrategy::ShouldUseKillingSpree() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return ShouldUseBurstCooldowns() &&
           bot->IsWithinMeleeRange(target) &&
           bot->HasSpell(RogueSpells::KILLING_SPREE) &&
           !bot->GetSpellHistory()->HasCooldown(RogueSpells::KILLING_SPREE);
}

bool PlayerbotOutlawRogueStrategy::HasOpportunityProc() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(RogueSpells::OPPORTUNITY);
}

bool PlayerbotOutlawRogueStrategy::IsInAdrenalineRush() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(RogueSpells::ADRENALINE_RUSH);
}

bool PlayerbotOutlawRogueStrategy::ShouldUseBurstCooldowns() const
{
    Unit* target = FindBestTarget();
    return target && (target->GetHealthPct() > 50.0f || IsInCombat());
}

bool PlayerbotOutlawRogueStrategy::IsTargetInExecuteRange(Unit* target) const
{
    return target && target->GetHealthPct() <= (EXECUTE_THRESHOLD * 100.0f);
}

bool PlayerbotOutlawRogueStrategy::ShouldUseAoERotation() const
{
    return CountNearbyEnemies(8.0f) >= AOE_TARGET_COUNT;
}

bool PlayerbotOutlawRogueStrategy::HasBladeFlurry() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(RogueSpells::BLADE_FLURRY);
}

// Subtlety Rogue Strategy
PlayerbotSubtletyRogueStrategy::PlayerbotSubtletyRogueStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotRogueStrategy(ai, "subtlety") {}

void PlayerbotSubtletyRogueStrategy::InitializeActions()
{
    PlayerbotRogueStrategy::InitializeActions();
    RegisterSubtletyActions();
}

void PlayerbotSubtletyRogueStrategy::InitializeTriggers()
{
    PlayerbotRogueStrategy::InitializeTriggers();
    RegisterSubtletyTriggers();
}

void PlayerbotSubtletyRogueStrategy::RegisterSubtletyActions()
{
    RegisterAction("backstab", std::make_unique<CastBackstab>(_ai));
    RegisterAction("eviscerate", std::make_unique<CastEviscerate>(_ai));
    RegisterAction("shadowstrike", std::make_unique<CastShadowstrike>(_ai));
    RegisterAction("shadow dance", std::make_unique<CastShadowDance>(_ai));
    RegisterAction("symbols of death", std::make_unique<CastSymbolsOfDeath>(_ai));
    RegisterAction("shadow clone", std::make_unique<CastShadowClone>(_ai));
    RegisterAction("black powder", std::make_unique<CastBlackPowder>(_ai));
    RegisterAction("shuriken storm", std::make_unique<CastShurikenStorm>(_ai));
}

void PlayerbotSubtletyRogueStrategy::RegisterSubtletyTriggers()
{
    // TODO: Implement when trigger system is available
}

float PlayerbotSubtletyRogueStrategy::GetActionPriority(std::string const& actionName) const
{
    // Check parent priorities first
    float parentPriority = PlayerbotRogueStrategy::GetActionPriority(actionName);
    if (parentPriority > 0.0f)
        return parentPriority;
    
    // Subtlety rotation priorities
    if (actionName == "symbols of death" && ShouldUseSymbolsOfDeath())
        return 0.95f;
    else if (actionName == "shadow dance" && ShouldUseShadowDance())
        return 0.93f;
    else if (actionName == "shadowstrike" && ShouldUseShadowstrike())
        return 0.9f;
    else if (actionName == "eviscerate" && ShouldUseEviscerate())
        return 0.85f;
    else if (actionName == "black powder" && ShouldUseBlackPowder())
        return 0.82f;
    else if (actionName == "shadow clone" && ShouldUseShadowClone())
        return 0.8f;
    else if (actionName == "shuriken storm" && ShouldUseShurikenStorm())
        return 0.75f;
    else if (actionName == "backstab" && ShouldUseBackstab())
        return 0.7f;
    
    return 0.0f;
}

bool PlayerbotSubtletyRogueStrategy::ShouldUseShadowDance() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    return ShouldPrepareBurstWindow() &&
           bot->HasSpell(RogueSpells::SHADOW_DANCE) &&
           !bot->GetSpellHistory()->HasCooldown(RogueSpells::SHADOW_DANCE);
}

bool PlayerbotSubtletyRogueStrategy::ShouldUseSymbolsOfDeath() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    return ShouldEnterBurstMode() &&
           bot->HasSpell(RogueSpells::SYMBOLS_OF_DEATH) &&
           !bot->GetSpellHistory()->HasCooldown(RogueSpells::SYMBOLS_OF_DEATH);
}

bool PlayerbotSubtletyRogueStrategy::IsInShadowDance() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(RogueSpells::SHADOW_DANCE);
}

bool PlayerbotSubtletyRogueStrategy::ShouldPrepareBurstWindow() const
{
    Unit* target = FindBestTarget();
    return target && IsInCombat() && !IsInBurstWindow();
}

bool PlayerbotSubtletyRogueStrategy::ShouldUseBackstab() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !bot->IsWithinMeleeRange(target))
        return false;
    
    return GetComboPoints() < 4 && bot->GetPower(POWER_ENERGY) >= 40;
}

bool PlayerbotSubtletyRogueStrategy::ShouldUseShadowstrike() const
{
    Unit* target = FindBestTarget();
    return target && IsStealthed();
}

bool PlayerbotSubtletyRogueStrategy::ShouldUseEviscerate() const
{
    Unit* target = FindBestTarget();
    if (!target || GetComboPoints() < EVISCERATE_MIN_CP)
        return false;
    
    Player* bot = _ai->GetBot();
    return bot && bot->IsWithinMeleeRange(target);
}

bool PlayerbotSubtletyRogueStrategy::ShouldUseShadowClone() const
{
    return IsInBurstWindow();
}

bool PlayerbotSubtletyRogueStrategy::ShouldUseShurikenStorm() const
{
    return CountNearbyEnemies(8.0f) >= AOE_TARGET_COUNT;
}

bool PlayerbotSubtletyRogueStrategy::ShouldUseBlackPowder() const
{
    return CountNearbyEnemies(8.0f) >= AOE_TARGET_COUNT && 
           GetComboPoints() >= BLACK_POWDER_MIN_CP;
}

bool PlayerbotSubtletyRogueStrategy::ShouldMaintainRupture() const
{
    Unit* target = FindBestTarget();
    if (!target || GetComboPoints() < RUPTURE_MIN_CP)
        return false;
    
    return ShouldRefreshRupture(target);
}

bool PlayerbotSubtletyRogueStrategy::IsInBurstWindow() const
{
    return HasSymbolsOfDeath() || IsInShadowDance();
}

bool PlayerbotSubtletyRogueStrategy::HasSymbolsOfDeath() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(RogueSpells::SYMBOLS_OF_DEATH);
}

bool PlayerbotSubtletyRogueStrategy::ShouldEnterBurstMode() const
{
    Unit* target = FindBestTarget();
    return target && IsInCombat() && !IsInBurstWindow();
}

bool PlayerbotSubtletyRogueStrategy::HasRuptureOnTarget(Unit* target) const
{
    return target && target->HasAura(RogueSpells::RUPTURE_SUB, _ai->GetBot()->GetGUID());
}

bool PlayerbotSubtletyRogueStrategy::ShouldRefreshRupture(Unit* target) const
{
    if (!HasRuptureOnTarget(target))
        return true;
    
    if (Aura* aura = target->GetAura(RogueSpells::RUPTURE_SUB, _ai->GetBot()->GetGUID()))
    {
        float remaining = float(aura->GetDuration()) / 1000.0f;
        return remaining <= RUPTURE_REFRESH_THRESHOLD;
    }
    
    return true;
}