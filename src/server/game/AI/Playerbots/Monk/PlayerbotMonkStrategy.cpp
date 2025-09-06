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

#include "PlayerbotMonkStrategy.h"
#include "PlayerbotMonkActions.h"
#include "PlayerbotPlayerAI.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellHistory.h"
#include "Group.h"

// Base Monk Strategy Implementation
PlayerbotMonkStrategy::PlayerbotMonkStrategy(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotStrategy(ai, name)
{
}

void PlayerbotMonkStrategy::InitializeActions()
{
    RegisterCommonActions();
}

void PlayerbotMonkStrategy::InitializeTriggers()
{
    RegisterCommonTriggers();
}

void PlayerbotMonkStrategy::RegisterCommonActions()
{
    RegisterAction("paralysis", std::make_unique<CastParalysisAction>(_ai));
    RegisterAction("leg sweep", std::make_unique<CastLegSweepAction>(_ai));
    RegisterAction("transcendence", std::make_unique<CastTranscendenceAction>(_ai));
    RegisterAction("roll", std::make_unique<CastRollAction>(_ai));
    RegisterAction("spear hand strike", std::make_unique<CastSpearHandStrikeAction>(_ai));
    RegisterAction("fortifying brew", std::make_unique<CastFortifyingBrewAction>(_ai));
    RegisterAction("detox", std::make_unique<CastDetoxAction>(_ai));
    RegisterAction("provoke", std::make_unique<CastProvokeAction>(_ai));
}

void PlayerbotMonkStrategy::RegisterCommonTriggers()
{
    // Common triggers will be implemented when trigger system is ready
}

float PlayerbotMonkStrategy::GetActionPriority(std::string const& actionName) const
{
    // Emergency abilities (highest priority)
    if (actionName == "fortifying brew" && ShouldUseFortifyingBrew()) return 1.0f;
    if (actionName == "transcendence" && ShouldUseTranscendence()) return 0.95f;
    
    // Utility and crowd control
    if (actionName == "spear hand strike" && ShouldUseSpearHandStrike()) return 0.9f;
    if (actionName == "paralysis" && ShouldUseParalysis()) return 0.85f;
    if (actionName == "leg sweep" && ShouldUseLegSweep()) return 0.8f;
    
    // Mobility and positioning
    if (actionName == "roll" && ShouldUseRoll()) return 0.3f;
    
    // Utility
    if (actionName == "detox" && ShouldUseDetox()) return 0.7f;
    if (actionName == "provoke" && ShouldUseProvoke()) return 0.6f;
    
    return 0.0f;
}

bool PlayerbotMonkStrategy::ShouldUseParalysis() const
{
    Unit* target = FindBestTarget();
    if (!target || !IsInCombat())
        return false;
    
    Player* bot = _ai->GetBot();
    return bot && bot->GetDistance(target) <= 20.0f && !target->HasAura(MonkSpells::PARALYSIS);
}

bool PlayerbotMonkStrategy::ShouldUseLegSweep() const
{
    return CountNearbyEnemies(5.0f) >= 2;
}

bool PlayerbotMonkStrategy::ShouldUseTranscendence() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetHealthPct() < 20.0f;
}

bool PlayerbotMonkStrategy::ShouldUseRoll() const
{
    Player* bot = _ai->GetBot();
    Unit* target = FindBestTarget();
    
    // Use Roll to close distance or escape danger
    return bot && target && (bot->GetDistance(target) > 15.0f || bot->GetHealthPct() < 30.0f);
}

bool PlayerbotMonkStrategy::ShouldUseSpearHandStrike() const
{
    Unit* target = FindBestTarget();
    return target && target->IsNonMeleeSpellCasted(false);
}

bool PlayerbotMonkStrategy::ShouldUseFortifyingBrew() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetHealthPct() < 40.0f && IsInCombat();
}

bool PlayerbotMonkStrategy::ShouldUseDetox() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    // Check self for dispellable debuffs
    if (bot->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE) || bot->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED))
        return true;
    
    // Check group members
    Group* group = bot->GetGroup();
    if (group)
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (member && member->IsAlive() && bot->IsWithinDistInMap(member, 40.0f))
            {
                if (member->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE) || member->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED))
                    return true;
            }
        }
    }
    
    return false;
}

bool PlayerbotMonkStrategy::ShouldUseProvoke() const
{
    if (!isBrewmaster())
        return false;
    
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    return target && bot && target->GetVictim() != bot && bot->GetDistance(target) <= 30.0f;
}

uint32 PlayerbotMonkStrategy::GetEnergyAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_ENERGY) : 0;
}

uint32 PlayerbotMonkStrategy::GetEnergyPercent() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 maxEnergy = bot->GetMaxPower(POWER_ENERGY);
    return maxEnergy > 0 ? (GetEnergyAmount() * 100) / maxEnergy : 0;
}

uint32 PlayerbotMonkStrategy::GetChi() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_CHI) : 0;
}

bool PlayerbotMonkStrategy::ShouldConserveEnergy() const
{
    return GetEnergyPercent() < 30;
}

bool PlayerbotMonkStrategy::HasEnoughEnergy(uint32 cost) const
{
    return GetEnergyAmount() >= cost;
}

bool PlayerbotMonkStrategy::HasEnoughChi(uint32 cost) const
{
    return GetChi() >= cost;
}

bool PlayerbotMonkStrategy::isBrewmaster() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(MonkSpells::KEG_SMASH);
}

bool PlayerbotMonkStrategy::isMistweaver() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(MonkSpells::ESSENCE_FONT);
}

bool PlayerbotMonkStrategy::isWindwalker() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(MonkSpells::FISTS_OF_FURY);
}

Unit* PlayerbotMonkStrategy::FindBestTarget() const
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

Unit* PlayerbotMonkStrategy::FindBestHealingTarget() const
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

bool PlayerbotMonkStrategy::IsInCombat() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->IsInCombat();
}

uint32 PlayerbotMonkStrategy::CountNearbyEnemies(float range) const
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

uint32 PlayerbotMonkStrategy::CountInjuredGroupMembers(float threshold) const
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

bool PlayerbotMonkStrategy::ShouldUseAoE() const
{
    return CountNearbyEnemies(8.0f) >= 3;
}

bool PlayerbotMonkStrategy::ShouldUseAoEHealing() const
{
    return CountInjuredGroupMembers(0.8f) >= 3;
}

bool PlayerbotMonkStrategy::HasShuffle() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MonkSpells::SHUFFLE);
}

bool PlayerbotMonkStrategy::HasStagger() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MonkSpells::STAGGER);
}

float PlayerbotMonkStrategy::GetStaggerDamage() const
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

bool PlayerbotMonkStrategy::HasTeachingsOfTheMonastery() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MonkSpells::TEACHINGS_OF_THE_MONASTERY);
}

uint32 PlayerbotMonkStrategy::GetTeachingsStacks() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
        
    if (Aura* aura = bot->GetAura(MonkSpells::TEACHINGS_OF_THE_MONASTERY))
        return aura->GetStackAmount();
        
    return 0;
}

bool PlayerbotMonkStrategy::HasDanceOfChiJi() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MonkSpells::DANCE_OF_CHI_JI);
}

bool PlayerbotMonkStrategy::HasComboBreaker() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MonkSpells::COMBO_BREAKER);
}

bool PlayerbotMonkStrategy::HasMarkOfTheCrane(Unit* target) const
{
    return target && target->HasAura(MonkSpells::MARK_OF_THE_CRANE);
}

uint32 PlayerbotMonkStrategy::GetMarkOfTheCraneCount() const
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

bool PlayerbotMonkStrategy::HasHitCombo() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MonkSpells::HIT_COMBO);
}

bool PlayerbotMonkStrategy::IsInMeleeRange(Unit* target) const
{
    Player* bot = _ai->GetBot();
    return bot && target && bot->GetDistance(target) <= 5.0f;
}

// Brewmaster Monk Strategy Implementation
PlayerbotBrewmasterMonkStrategy::PlayerbotBrewmasterMonkStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotMonkStrategy(ai, "brewmaster")
{
}

void PlayerbotBrewmasterMonkStrategy::InitializeActions()
{
    PlayerbotMonkStrategy::InitializeActions();
    RegisterBrewmasterActions();
}

void PlayerbotBrewmasterMonkStrategy::InitializeTriggers()
{
    PlayerbotMonkStrategy::InitializeTriggers();
    RegisterBrewmasterTriggers();
}

void PlayerbotBrewmasterMonkStrategy::RegisterBrewmasterActions()
{
    RegisterAction("keg smash", std::make_unique<CastKegSmashAction>(_ai));
    RegisterAction("tiger palm bm", std::make_unique<CastTigerPalmBrewmasterAction>(_ai));
    RegisterAction("breath of fire", std::make_unique<CastBreathOfFireAction>(_ai));
    RegisterAction("blackout kick bm", std::make_unique<CastBlackoutKickBrewmasterAction>(_ai));
    RegisterAction("spinning crane kick bm", std::make_unique<CastSpinningCraneKickBrewmasterAction>(_ai));
    RegisterAction("ironskin brew", std::make_unique<CastIronskinBrewAction>(_ai));
    RegisterAction("purifying brew", std::make_unique<CastPurifyingBrewAction>(_ai));
    RegisterAction("celestial brew", std::make_unique<CastCelestialBrewAction>(_ai));
    RegisterAction("invoke niuzao", std::make_unique<CastInvokeNiuzaoAction>(_ai));
    RegisterAction("black ox brew", std::make_unique<CastBlackOxBrewAction>(_ai));
    RegisterAction("expel harm", std::make_unique<CastExpelHarmAction>(_ai));
}

void PlayerbotBrewmasterMonkStrategy::RegisterBrewmasterTriggers()
{
    // Triggers will be implemented when trigger system is ready
}

float PlayerbotBrewmasterMonkStrategy::GetActionPriority(std::string const& actionName) const
{
    // Get base priorities
    float basePriority = PlayerbotMonkStrategy::GetActionPriority(actionName);
    if (basePriority > 0.0f)
        return basePriority;
    
    // Emergency mitigation (highest priority)
    if (actionName == "celestial brew" && ShouldUseCelestialBrew()) return 1.0f;
    if (actionName == "purifying brew" && ShouldPurifyStagger()) return 0.95f;
    if (actionName == "ironskin brew" && ShouldUseEmergencyMitigation()) return 0.9f;
    if (actionName == "expel harm" && ShouldUseExpelHarm()) return 0.85f;
    
    // Active mitigation
    if (actionName == "ironskin brew" && ShouldUseIronskinBrew()) return 0.8f;
    if (actionName == "purifying brew" && ShouldUsePurifyingBrew()) return 0.75f;
    
    // Major cooldowns
    if (actionName == "invoke niuzao" && ShouldUseInvokeNiuzao()) return 0.7f;
    if (actionName == "black ox brew" && ShouldUseBlackOxBrew()) return 0.65f;
    
    // Threat generation
    if (actionName == "keg smash" && ShouldUseKegSmash()) return 0.6f;
    if (actionName == "breath of fire" && ShouldUseBreathOfFire()) return 0.55f;
    
    // Buff maintenance and basic rotation
    if (actionName == "blackout kick bm" && NeedsShuffle()) return 0.5f;
    if (actionName == "tiger palm bm" && ShouldUseTigerPalm()) return 0.45f;
    if (actionName == "spinning crane kick bm" && ShouldUseSpinningCraneKick()) return 0.4f;
    if (actionName == "blackout kick bm" && ShouldUseBlackoutKick()) return 0.35f;
    
    return 0.0f;
}

bool PlayerbotBrewmasterMonkStrategy::ShouldUseKegSmash() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !IsInMeleeRange(target))
        return false;
    
    return HasEnoughEnergy(40) && !bot->GetSpellHistory()->HasCooldown(MonkSpells::KEG_SMASH);
}

bool PlayerbotBrewmasterMonkStrategy::ShouldUseTigerPalm() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !IsInMeleeRange(target))
        return false;
    
    return HasEnoughEnergy(50) && !bot->GetSpellHistory()->HasCooldown(MonkSpells::TIGER_PALM_BM);
}

bool PlayerbotBrewmasterMonkStrategy::ShouldUseBreathOfFire() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return HasEnoughChi(2) && CountNearbyEnemies(8.0f) >= 1 && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::BREATH_OF_FIRE);
}

bool PlayerbotBrewmasterMonkStrategy::ShouldUseBlackoutKick() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !IsInMeleeRange(target))
        return false;
    
    return HasEnoughChi(1) && !bot->GetSpellHistory()->HasCooldown(MonkSpells::BLACKOUT_KICK_BM);
}

bool PlayerbotBrewmasterMonkStrategy::ShouldUseSpinningCraneKick() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return HasEnoughChi(2) && CountNearbyEnemies(8.0f) >= 3 && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::SPINNING_CRANE_KICK_BM);
}

bool PlayerbotBrewmasterMonkStrategy::ShouldUseIronskinBrew() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot || !HasIronskinBrewCharges())
        return false;
    
    return IsInCombat() && (!HasShuffle() || bot->GetHealthPct() < 80.0f);
}

bool PlayerbotBrewmasterMonkStrategy::ShouldUsePurifyingBrew() const
{
    return HasPurifyingBrewCharges() && (HasHighStagger() || ShouldPurifyStagger());
}

bool PlayerbotBrewmasterMonkStrategy::ShouldUseCelestialBrew() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return bot->GetHealthPct() < 50.0f && IsInCombat() && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::CELESTIAL_BREW);
}

bool PlayerbotBrewmasterMonkStrategy::ShouldUseInvokeNiuzao() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return bot->GetHealthPct() < 60.0f && CountNearbyEnemies() >= 3 && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::INVOKE_NIUZAO);
}

bool PlayerbotBrewmasterMonkStrategy::ShouldUseBlackOxBrew() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return GetBrewCharges() == 0 && IsInCombat() && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::BLACK_OX_BREW);
}

bool PlayerbotBrewmasterMonkStrategy::ShouldUseExpelHarm() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return bot->GetHealthPct() < 70.0f && HasEnoughChi(1) && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::EXPEL_HARM);
}

bool PlayerbotBrewmasterMonkStrategy::ShouldPrioritizeThreat() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    return target && bot && target->GetVictim() != bot;
}

bool PlayerbotBrewmasterMonkStrategy::HasHighStagger() const
{
    return GetStaggerPercent() > HEAVY_STAGGER_THRESHOLD;
}

bool PlayerbotBrewmasterMonkStrategy::ShouldPurifyStagger() const
{
    return HasStagger() && (IsStaggerHeavy() || (IsStaggerModerate() && GetBrewCharges() >= 2));
}

bool PlayerbotBrewmasterMonkStrategy::ShouldUseEmergencyMitigation() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetHealthPct() < LOW_HEALTH_THRESHOLD * 100.0f;
}

bool PlayerbotBrewmasterMonkStrategy::NeedsShuffle() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !HasShuffle())
        return true;
    
    Aura* shuffleAura = bot->GetAura(MonkSpells::SHUFFLE);
    return shuffleAura && shuffleAura->GetDuration() < SHUFFLE_REFRESH_THRESHOLD * 1000;
}

bool PlayerbotBrewmasterMonkStrategy::ShouldUseAoEThreat() const
{
    return CountNearbyEnemies() >= AOE_THREAT_COUNT;
}

bool PlayerbotBrewmasterMonkStrategy::HasIronskinBrewCharges() const
{
    Player* bot = _ai->GetBot();
    return bot && !bot->GetSpellHistory()->HasCooldown(MonkSpells::IRONSKIN_BREW);
}

bool PlayerbotBrewmasterMonkStrategy::HasPurifyingBrewCharges() const
{
    Player* bot = _ai->GetBot();
    return bot && !bot->GetSpellHistory()->HasCooldown(MonkSpells::PURIFYING_BREW);
}

uint32 PlayerbotBrewmasterMonkStrategy::GetBrewCharges() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    // Simplified brew charge calculation
    uint32 charges = 0;
    if (!bot->GetSpellHistory()->HasCooldown(MonkSpells::IRONSKIN_BREW))
        charges++;
    if (!bot->GetSpellHistory()->HasCooldown(MonkSpells::PURIFYING_BREW))
        charges++;
    
    return charges;
}

bool PlayerbotBrewmasterMonkStrategy::ShouldConserveBrews() const
{
    return GetBrewCharges() <= MIN_BREW_CHARGES;
}

float PlayerbotBrewmasterMonkStrategy::GetStaggerPercent() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !HasStagger())
        return 0.0f;
    
    return GetStaggerDamage() / bot->GetMaxHealth();
}

bool PlayerbotBrewmasterMonkStrategy::IsStaggerHeavy() const
{
    return GetStaggerPercent() > HEAVY_STAGGER_THRESHOLD;
}

bool PlayerbotBrewmasterMonkStrategy::IsStaggerModerate() const
{
    float staggerPercent = GetStaggerPercent();
    return staggerPercent > MODERATE_STAGGER_THRESHOLD && staggerPercent <= HEAVY_STAGGER_THRESHOLD;
}

uint32 PlayerbotBrewmasterMonkStrategy::GetStaggerTicksRemaining() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !HasStagger())
        return 0;
    
    Aura* staggerAura = bot->GetAura(MonkSpells::STAGGER);
    return staggerAura ? staggerAura->GetDuration() / 1000 : 0;
}

// Mistweaver Monk Strategy Implementation
PlayerbotMistweaverMonkStrategy::PlayerbotMistweaverMonkStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotMonkStrategy(ai, "mistweaver")
{
}

void PlayerbotMistweaverMonkStrategy::InitializeActions()
{
    PlayerbotMonkStrategy::InitializeActions();
    RegisterMistweaverActions();
}

void PlayerbotMistweaverMonkStrategy::InitializeTriggers()
{
    PlayerbotMonkStrategy::InitializeTriggers();
    RegisterMistweaverTriggers();
}

void PlayerbotMistweaverMonkStrategy::RegisterMistweaverActions()
{
    RegisterAction("renewing mist", std::make_unique<CastRenewingMistAction>(_ai));
    RegisterAction("essence font", std::make_unique<CastEssenceFontAction>(_ai));
    RegisterAction("vivify", std::make_unique<CastVivifyAction>(_ai));
    RegisterAction("enveloping mist", std::make_unique<CastEnvelopingMistAction>(_ai));
    RegisterAction("thunder focus tea", std::make_unique<CastThunderFocusTeaAction>(_ai));
    RegisterAction("revival", std::make_unique<CastRevivalAction>(_ai));
    RegisterAction("invoke yulon", std::make_unique<CastInvokeYulonAction>(_ai));
    RegisterAction("sheiluns gift", std::make_unique<CastSheilunsGiftAction>(_ai));
    RegisterAction("soothing mist", std::make_unique<CastSoothingMistAction>(_ai));
    RegisterAction("life cocoon", std::make_unique<CastLifeCocoonAction>(_ai));
    RegisterAction("mana tea", std::make_unique<CastManaTeaAction>(_ai));
    
    // Fistweaving abilities
    RegisterAction("rising sun kick mw", std::make_unique<CastRisingSunKickMistweaverAction>(_ai));
    RegisterAction("blackout kick mw", std::make_unique<CastBlackoutKickMistweaverAction>(_ai));
    RegisterAction("tiger palm mw", std::make_unique<CastTigerPalmMistweaverAction>(_ai));
    RegisterAction("spinning crane kick mw", std::make_unique<CastSpinningCraneKickMistweaverAction>(_ai));
}

void PlayerbotMistweaverMonkStrategy::RegisterMistweaverTriggers()
{
    // Triggers will be implemented when trigger system is ready
}

float PlayerbotMistweaverMonkStrategy::GetActionPriority(std::string const& actionName) const
{
    // Get base priorities
    float basePriority = PlayerbotMonkStrategy::GetActionPriority(actionName);
    if (basePriority > 0.0f)
        return basePriority;
    
    // Emergency healing (highest priority)
    if (actionName == "life cocoon" && ShouldUseLifeCocoon()) return 1.0f;
    if (actionName == "revival" && ShouldUseRevival()) return 0.95f;
    if (actionName == "vivify" && ShouldUseEmergencyHealing()) return 0.9f;
    if (actionName == "enveloping mist" && ShouldUseEmergencyHealing()) return 0.85f;
    
    // Major cooldowns
    if (actionName == "invoke yulon" && ShouldUseInvokeYulon()) return 0.8f;
    if (actionName == "sheiluns gift" && ShouldUseSheilunsGift()) return 0.75f;
    
    // Resource management
    if (actionName == "mana tea" && ShouldUseManaTea()) return 0.7f;
    if (actionName == "thunder focus tea" && ShouldUseThunderFocusTea()) return 0.65f;
    
    // Priority healing
    if (actionName == "enveloping mist" && ShouldUseEnvelopingMist()) return 0.6f;
    if (actionName == "vivify" && ShouldUseVivify()) return 0.55f;
    if (actionName == "essence font" && ShouldUseEssenceFont()) return 0.5f;
    if (actionName == "renewing mist" && ShouldUseRenewingMist()) return 0.45f;
    if (actionName == "soothing mist" && ShouldUseSoothingMist()) return 0.4f;
    
    // Fistweaving (when appropriate)
    if (actionName == "rising sun kick mw" && ShouldUseRisingSunKick()) return 0.35f;
    if (actionName == "blackout kick mw" && ShouldUseBlackoutKickMW()) return 0.3f;
    if (actionName == "tiger palm mw" && ShouldUseTigerPalmMW()) return 0.25f;
    if (actionName == "spinning crane kick mw" && ShouldUseSpinningCraneKickMW()) return 0.2f;
    
    return 0.0f;
}

bool PlayerbotMistweaverMonkStrategy::ShouldUseRenewingMist() const
{
    Unit* target = FindBestRenewingMistTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return HasEnoughMana(2000) && GetActiveRenewingMistCount() < MAX_RENEWING_MIST_TARGETS &&
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::RENEWING_MIST);
}

bool PlayerbotMistweaverMonkStrategy::ShouldUseEssenceFont() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return HasEnoughMana(6000) && ShouldUseAoEHealing() && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::ESSENCE_FONT);
}

bool PlayerbotMistweaverMonkStrategy::ShouldUseVivify() const
{
    Unit* target = FindBestHealingTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return HasEnoughMana(3000) && target->GetHealthPct() < LOW_HEALTH_THRESHOLD * 100.0f &&
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::VIVIFY);
}

bool PlayerbotMistweaverMonkStrategy::ShouldUseEnvelopingMist() const
{
    Unit* target = FindBestEnvelopingMistTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return HasEnoughMana(5000) && target->GetHealthPct() < LOW_HEALTH_THRESHOLD * 100.0f &&
           !HasEnvelopingMist(target) && !bot->GetSpellHistory()->HasCooldown(MonkSpells::ENVELOPING_MIST);
}

bool PlayerbotMistweaverMonkStrategy::ShouldUseThunderFocusTea() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot || HasThunderFocusTea())
        return false;
    
    return (ShouldUseThunderFocusTeaOnVivify() || ShouldUseThunderFocusTeaOnEnvelopingMist() || 
            ShouldUseThunderFocusTeaOnRenewingMist()) && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::THUNDER_FOCUS_TEA);
}

bool PlayerbotMistweaverMonkStrategy::ShouldUseRevival() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return CountInjuredGroupMembers(CRITICAL_HEALTH_THRESHOLD) >= 3 && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::REVIVAL);
}

bool PlayerbotMistweaverMonkStrategy::ShouldUseInvokeYulon() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return ShouldUseAoEHealing() && HasEnoughMana(8000) && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::INVOKE_YULON);
}

bool PlayerbotMistweaverMonkStrategy::ShouldUseSheilunsGift() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return CountInjuredGroupMembers(LOW_HEALTH_THRESHOLD) >= 2 && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::SHEILUNS_GIFT);
}

bool PlayerbotMistweaverMonkStrategy::ShouldUseSoothingMist() const
{
    Unit* target = FindCriticalHealingTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return HasEnoughMana(1000) && target->GetHealthPct() < CRITICAL_HEALTH_THRESHOLD * 100.0f;
}

bool PlayerbotMistweaverMonkStrategy::ShouldUseLifeCocoon() const
{
    Unit* target = FindCriticalHealingTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return target->GetHealthPct() < CRITICAL_HEALTH_THRESHOLD * 100.0f && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::LIFE_COCOON);
}

bool PlayerbotMistweaverMonkStrategy::ShouldUseManaTea() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return GetManaPercent() < LOW_MANA_THRESHOLD && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::MANA_TEA);
}

bool PlayerbotMistweaverMonkStrategy::ShouldUseRisingSunKick() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !ShouldUseFistweaving())
        return false;
    
    return HasEnoughChi(2) && IsInMeleeRange(target) && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::RISING_SUN_KICK_MW);
}

bool PlayerbotMistweaverMonkStrategy::ShouldUseBlackoutKickMW() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !ShouldUseFistweaving())
        return false;
    
    return HasEnoughChi(1) && IsInMeleeRange(target) && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::BLACKOUT_KICK_MW);
}

bool PlayerbotMistweaverMonkStrategy::ShouldUseTigerPalmMW() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !ShouldUseFistweaving())
        return false;
    
    return HasEnoughEnergy(50) && IsInMeleeRange(target) && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::TIGER_PALM_MW);
}

bool PlayerbotMistweaverMonkStrategy::ShouldUseSpinningCraneKickMW() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot || !ShouldUseFistweaving())
        return false;
    
    return HasEnoughChi(2) && CountNearbyEnemies(8.0f) >= 3;
}

Unit* PlayerbotMistweaverMonkStrategy::FindCriticalHealingTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;
    
    // Check self first
    if (bot->GetHealthPct() < CRITICAL_HEALTH_THRESHOLD * 100.0f)
        return bot;
    
    // Check group members
    Group* group = bot->GetGroup();
    if (group)
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (member && member->IsAlive() && member->GetHealthPct() < CRITICAL_HEALTH_THRESHOLD * 100.0f &&
                bot->IsWithinDistInMap(member, 40.0f))
                return member;
        }
    }
    
    return nullptr;
}

Unit* PlayerbotMistweaverMonkStrategy::FindBestRenewingMistTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;
    
    // Check self first
    if (bot->GetHealthPct() < LOW_HEALTH_THRESHOLD * 100.0f && !HasRenewingMist(bot))
        return bot;
    
    // Check group members
    Group* group = bot->GetGroup();
    if (group)
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (member && member->IsAlive() && member->GetHealthPct() < LOW_HEALTH_THRESHOLD * 100.0f &&
                !HasRenewingMist(member) && bot->IsWithinDistInMap(member, 40.0f))
                return member;
        }
    }
    
    return nullptr;
}

Unit* PlayerbotMistweaverMonkStrategy::FindBestEnvelopingMistTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;
    
    Unit* lowestTarget = nullptr;
    float lowestHealth = 100.0f;
    
    // Check self first
    if (bot->GetHealthPct() < LOW_HEALTH_THRESHOLD * 100.0f && !HasEnvelopingMist(bot))
    {
        lowestHealth = bot->GetHealthPct();
        lowestTarget = bot;
    }
    
    // Check group members
    Group* group = bot->GetGroup();
    if (group)
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (member && member->IsAlive() && member->GetHealthPct() < LOW_HEALTH_THRESHOLD * 100.0f &&
                member->GetHealthPct() < lowestHealth && !HasEnvelopingMist(member) &&
                bot->IsWithinDistInMap(member, 40.0f))
            {
                lowestHealth = member->GetHealthPct();
                lowestTarget = member;
            }
        }
    }
    
    return lowestTarget;
}

bool PlayerbotMistweaverMonkStrategy::ShouldUseEmergencyHealing() const
{
    return FindCriticalHealingTarget() != nullptr;
}

bool PlayerbotMistweaverMonkStrategy::ShouldUseFistweaving() const
{
    return GetManaPercent() > FISTWEAVING_MANA_THRESHOLD && !ShouldUseEmergencyHealing() &&
           CountInjuredGroupMembers(LOW_HEALTH_THRESHOLD) <= 1;
}

uint32 PlayerbotMistweaverMonkStrategy::GetManaAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_MANA) : 0;
}

uint32 PlayerbotMistweaverMonkStrategy::GetManaPercent() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 maxMana = bot->GetMaxPower(POWER_MANA);
    return maxMana > 0 ? (GetManaAmount() * 100) / maxMana : 0;
}

bool PlayerbotMistweaverMonkStrategy::ShouldConserveMana() const
{
    return GetManaPercent() < LOW_MANA_THRESHOLD;
}

bool PlayerbotMistweaverMonkStrategy::HasEnoughMana(uint32 cost) const
{
    return GetManaAmount() >= cost;
}

bool PlayerbotMistweaverMonkStrategy::HasRenewingMist(Unit* target) const
{
    return target && target->HasAura(MonkSpells::RENEWING_MIST);
}

bool PlayerbotMistweaverMonkStrategy::HasEnvelopingMist(Unit* target) const
{
    return target && target->HasAura(MonkSpells::ENVELOPING_MIST);
}

bool PlayerbotMistweaverMonkStrategy::HasEssenceFontBuff(Unit* target) const
{
    return target && target->HasAura(MonkSpells::ESSENCE_FONT_BUFF);
}

uint32 PlayerbotMistweaverMonkStrategy::GetActiveRenewingMistCount() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 count = 0;
    
    // Check self
    if (HasRenewingMist(bot))
        count++;
    
    // Check group members
    Group* group = bot->GetGroup();
    if (group)
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (member && member->IsAlive() && HasRenewingMist(member) &&
                bot->IsWithinDistInMap(member, 40.0f))
                count++;
        }
    }
    
    return count;
}

bool PlayerbotMistweaverMonkStrategy::ShouldRefreshRenewingMist(Unit* target) const
{
    if (!target || !HasRenewingMist(target))
        return false;
    
    Aura* aura = target->GetAura(MonkSpells::RENEWING_MIST);
    return aura && aura->GetDuration() < HOT_REFRESH_THRESHOLD * 1000;
}

bool PlayerbotMistweaverMonkStrategy::HasThunderFocusTea() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MonkSpells::THUNDER_FOCUS_TEA);
}

bool PlayerbotMistweaverMonkStrategy::ShouldUseThunderFocusTeaOnVivify() const
{
    Unit* target = FindBestHealingTarget();
    return target && target->GetHealthPct() < LOW_HEALTH_THRESHOLD * 100.0f;
}

bool PlayerbotMistweaverMonkStrategy::ShouldUseThunderFocusTeaOnEnvelopingMist() const
{
    Unit* target = FindCriticalHealingTarget();
    return target != nullptr;
}

bool PlayerbotMistweaverMonkStrategy::ShouldUseThunderFocusTeaOnRenewingMist() const
{
    return GetActiveRenewingMistCount() < 3 && CountInjuredGroupMembers(LOW_HEALTH_THRESHOLD) >= 2;
}

// Windwalker Monk Strategy Implementation
PlayerbotWindwalkerMonkStrategy::PlayerbotWindwalkerMonkStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotMonkStrategy(ai, "windwalker")
{
}

void PlayerbotWindwalkerMonkStrategy::InitializeActions()
{
    PlayerbotMonkStrategy::InitializeActions();
    RegisterWindwalkerActions();
}

void PlayerbotWindwalkerMonkStrategy::InitializeTriggers()
{
    PlayerbotMonkStrategy::InitializeTriggers();
    RegisterWindwalkerTriggers();
}

void PlayerbotWindwalkerMonkStrategy::RegisterWindwalkerActions()
{
    RegisterAction("tiger palm ww", std::make_unique<CastTigerPalmWindwalkerAction>(_ai));
    RegisterAction("rising sun kick ww", std::make_unique<CastRisingSunKickWindwalkerAction>(_ai));
    RegisterAction("blackout kick ww", std::make_unique<CastBlackoutKickWindwalkerAction>(_ai));
    RegisterAction("fists of fury", std::make_unique<CastFistsOfFuryAction>(_ai));
    RegisterAction("whirling dragon punch", std::make_unique<CastWhirlingDragonPunchAction>(_ai));
    RegisterAction("spinning crane kick ww", std::make_unique<CastSpinningCraneKickWindwalkerAction>(_ai));
    RegisterAction("flying serpent kick", std::make_unique<CastFlyingSerpentKickAction>(_ai));
    RegisterAction("strike of the windlord", std::make_unique<CastStrikeOfTheWindlordAction>(_ai));
    RegisterAction("touch of death", std::make_unique<CastTouchOfDeathAction>(_ai));
    RegisterAction("invoke xuen", std::make_unique<CastInvokeXuenAction>(_ai));
    RegisterAction("storm earth and fire", std::make_unique<CastStormEarthAndFireAction>(_ai));
    RegisterAction("serenity", std::make_unique<CastSerenityAction>(_ai));
}

void PlayerbotWindwalkerMonkStrategy::RegisterWindwalkerTriggers()
{
    // Triggers will be implemented when trigger system is ready
}

float PlayerbotWindwalkerMonkStrategy::GetActionPriority(std::string const& actionName) const
{
    // Get base priorities
    float basePriority = PlayerbotMonkStrategy::GetActionPriority(actionName);
    if (basePriority > 0.0f)
        return basePriority;
    
    // Emergency and major cooldowns
    if (actionName == "touch of death" && ShouldUseTouchOfDeath()) return 1.0f;
    if (actionName == "invoke xuen" && ShouldUseInvokeXuen()) return 0.95f;
    if (actionName == "storm earth and fire" && ShouldUseStormEarthAndFire()) return 0.9f;
    if (actionName == "serenity" && ShouldUseSerenity()) return 0.85f;
    
    // Signature abilities
    if (actionName == "fists of fury" && ShouldUseFistsOfFury()) return 0.8f;
    if (actionName == "whirling dragon punch" && ShouldUseWhirlingDragonPunch()) return 0.75f;
    if (actionName == "strike of the windlord" && ShouldUseStrikeOfTheWindlord()) return 0.7f;
    
    // Core rotation
    if (actionName == "rising sun kick ww" && ShouldUseRisingSunKickWW()) return 0.65f;
    if (actionName == "blackout kick ww" && ShouldUseBlackoutKickWW()) return 0.6f;
    if (actionName == "spinning crane kick ww" && ShouldUseSpinningCraneKickWW()) return 0.55f;
    if (actionName == "tiger palm ww" && ShouldUseTigerPalmWW()) return 0.5f;
    
    // Mobility
    if (actionName == "flying serpent kick" && ShouldUseFlyingSerpentKick()) return 0.3f;
    
    return 0.0f;
}

bool PlayerbotWindwalkerMonkStrategy::ShouldUseTigerPalmWW() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !IsInMeleeRange(target))
        return false;
    
    return HasEnoughEnergy(50) && ShouldBuildChi() && 
           CanMaintainHitCombo("tiger palm ww") && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::TIGER_PALM_WW);
}

bool PlayerbotWindwalkerMonkStrategy::ShouldUseRisingSunKickWW() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !IsInMeleeRange(target))
        return false;
    
    return HasEnoughChi(2) && CanMaintainHitCombo("rising sun kick ww") && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::RISING_SUN_KICK_WW);
}

bool PlayerbotWindwalkerMonkStrategy::ShouldUseBlackoutKickWW() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !IsInMeleeRange(target))
        return false;
    
    return HasEnoughChi(1) && CanMaintainHitCombo("blackout kick ww") && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::BLACKOUT_KICK_WW);
}

bool PlayerbotWindwalkerMonkStrategy::ShouldUseFistsOfFury() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !IsInMeleeRange(target))
        return false;
    
    return HasEnoughChi(3) && CanMaintainHitCombo("fists of fury") && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::FISTS_OF_FURY);
}

bool PlayerbotWindwalkerMonkStrategy::ShouldUseWhirlingDragonPunch() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return HasEnoughChi(2) && CanMaintainHitCombo("whirling dragon punch") && 
           bot->GetSpellHistory()->HasCooldown(MonkSpells::RISING_SUN_KICK_WW) &&
           bot->GetSpellHistory()->HasCooldown(MonkSpells::FISTS_OF_FURY) &&
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::WHIRLING_DRAGON_PUNCH);
}

bool PlayerbotWindwalkerMonkStrategy::ShouldUseSpinningCraneKickWW() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return HasEnoughChi(2) && (ShouldUseAoERotation() || GetMarkOfTheCraneCount() >= MARK_COUNT_FOR_SCK) &&
           CanMaintainHitCombo("spinning crane kick ww");
}

bool PlayerbotWindwalkerMonkStrategy::ShouldUseFlyingSerpentKick() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return bot->GetDistance(target) > MOBILITY_RANGE && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::FLYING_SERPENT_KICK);
}

bool PlayerbotWindwalkerMonkStrategy::ShouldUseStrikeOfTheWindlord() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !IsInMeleeRange(target))
        return false;
    
    return HasEnoughChi(2) && CanMaintainHitCombo("strike of the windlord") && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::STRIKE_OF_THE_WINDLORD);
}

bool PlayerbotWindwalkerMonkStrategy::ShouldUseTouchOfDeath() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return IsTargetSuitableForTouchOfDeath() && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::TOUCH_OF_DEATH);
}

bool PlayerbotWindwalkerMonkStrategy::ShouldUseInvokeXuen() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return ShouldUseMajorCooldowns() && 
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::INVOKE_XUEN);
}

bool PlayerbotWindwalkerMonkStrategy::ShouldUseStormEarthAndFire() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return ShouldUseMajorCooldowns() && ShouldSetupStormEarthAndFire() &&
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::STORM_EARTH_AND_FIRE);
}

bool PlayerbotWindwalkerMonkStrategy::ShouldUseSerenity() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return ShouldUseMajorCooldowns() && GetSerenityValue() > GetStormEarthAndFireValue() &&
           !bot->GetSpellHistory()->HasCooldown(MonkSpells::SERENITY);
}

bool PlayerbotWindwalkerMonkStrategy::ShouldBuildChi() const
{
    return GetChi() < OPTIMAL_CHI_COUNT;
}

bool PlayerbotWindwalkerMonkStrategy::ShouldSpendChi() const
{
    return GetChi() >= 2;
}

bool PlayerbotWindwalkerMonkStrategy::HasOptimalChiCount() const
{
    uint32 chi = GetChi();
    return chi >= 2 && chi <= OPTIMAL_CHI_COUNT;
}

uint32 PlayerbotWindwalkerMonkStrategy::GetLastUsedAbility() const
{
    // This would need to be implemented with proper tracking
    // For now, return a default value
    return 0;
}

bool PlayerbotWindwalkerMonkStrategy::CanMaintainHitCombo(std::string const& actionName) const
{
    if (!HasHitCombo() && !ShouldPrioritizeHitCombo())
        return true;
    
    // Simple implementation - in reality would need to track last used ability
    return true;
}

bool PlayerbotWindwalkerMonkStrategy::ShouldPrioritizeHitCombo() const
{
    return HasHitCombo() || (GetChi() >= 3 && CountNearbyEnemies() <= 2);
}

bool PlayerbotWindwalkerMonkStrategy::ShouldUseAoERotation() const
{
    return CountNearbyEnemies() >= AOE_ROTATION_COUNT;
}

bool PlayerbotWindwalkerMonkStrategy::ShouldUseBurstCooldowns() const
{
    Unit* target = FindBestTarget();
    return target && target->GetHealthPct() > BURST_COOLDOWN_HEALTH * 100.0f;
}

bool PlayerbotWindwalkerMonkStrategy::IsInBurstWindow() const
{
    Player* bot = _ai->GetBot();
    return bot && (bot->HasAura(MonkSpells::STORM_EARTH_AND_FIRE) || bot->HasAura(MonkSpells::SERENITY));
}

bool PlayerbotWindwalkerMonkStrategy::HasTeachingsOfTheMonasteryStacks() const
{
    return GetTeachingsStacks() >= 3;
}

uint32 PlayerbotWindwalkerMonkStrategy::GetComboStrikeBonus() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !HasHitCombo())
        return 0;
    
    if (Aura* aura = bot->GetAura(MonkSpells::HIT_COMBO))
        return aura->GetStackAmount();
    
    return 0;
}

bool PlayerbotWindwalkerMonkStrategy::ShouldApplyMarkOfTheCrane() const
{
    return CountNearbyEnemies() >= 2 && GetMarkOfTheCraneCount() < CountNearbyEnemies();
}

uint32 PlayerbotWindwalkerMonkStrategy::GetOptimalMarkTargets() const
{
    uint32 enemies = CountNearbyEnemies();
    return enemies > 5 ? 5 : enemies;  // Cap at 5 marks
}

bool PlayerbotWindwalkerMonkStrategy::ShouldUseSpinningCraneKickForMarks() const
{
    return GetMarkOfTheCraneCount() >= MARK_COUNT_FOR_SCK;
}

Unit* PlayerbotWindwalkerMonkStrategy::FindBestMarkTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;
    
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot, 8.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, check);
    Cell::VisitAllObjects(bot, searcher, 8.0f);
    
    for (Unit* target : targets)
    {
        if (target->IsAlive() && !HasMarkOfTheCrane(target))
            return target;
    }
    
    return nullptr;
}

bool PlayerbotWindwalkerMonkStrategy::ShouldUseMajorCooldowns() const
{
    Unit* target = FindBestTarget();
    return target && ShouldUseBurstCooldowns() && IsInCombat();
}

bool PlayerbotWindwalkerMonkStrategy::IsTargetSuitableForTouchOfDeath() const
{
    Unit* target = FindBestTarget();
    return target && target->GetHealthPct() <= TOUCH_OF_DEATH_THRESHOLD * 100.0f;
}

bool PlayerbotWindwalkerMonkStrategy::ShouldSetupStormEarthAndFire() const
{
    return CountNearbyEnemies() >= 2 && HasOptimalChiCount();
}

float PlayerbotWindwalkerMonkStrategy::GetSerenityValue() const
{
    // Serenity is better for single target
    return CountNearbyEnemies() == 1 ? 1.0f : 0.5f;
}

float PlayerbotWindwalkerMonkStrategy::GetStormEarthAndFireValue() const
{
    // Storm, Earth, and Fire is better for multiple targets
    uint32 enemies = CountNearbyEnemies();
    return enemies >= 2 ? 1.0f : 0.3f;
}

bool PlayerbotWindwalkerMonkStrategy::ShouldUseMobilityAbilities() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    return target && bot && bot->GetDistance(target) > MOBILITY_RANGE;
}

bool PlayerbotWindwalkerMonkStrategy::IsOptimallyPositioned() const
{
    Unit* target = FindBestTarget();
    return target && IsInMeleeRange(target);
}

bool PlayerbotWindwalkerMonkStrategy::ShouldUseFlyingSerpentKickForGap() const
{
    return ShouldUseMobilityAbilities();
}