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

#include "PlayerbotEvokerStrategy.h"
#include "PlayerbotEvokerActions.h"

#ifdef WITH_PLAYERBOTS

// ===========================================
// PlayerbotEvokerStrategy (Base Class)
// ===========================================

PlayerbotEvokerStrategy::PlayerbotEvokerStrategy(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotStrategy(ai, name)
{
}

void PlayerbotEvokerStrategy::InitializeActions()
{
    PlayerbotStrategy::InitializeActions();
    RegisterCommonActions();
}

void PlayerbotEvokerStrategy::InitializeTriggers()
{
    PlayerbotStrategy::InitializeTriggers();
    RegisterCommonTriggers();
}

void PlayerbotEvokerStrategy::RegisterCommonActions()
{
    // Utility actions commented for later trigger implementation
    // _ai->AddAction("quell", std::make_unique<PlayerbotQuellAction>(_ai));
    // _ai->AddAction("rescue", std::make_unique<PlayerbotRescueAction>(_ai));
    // _ai->AddAction("expunge", std::make_unique<PlayerbotExpungeAction>(_ai));
    // _ai->AddAction("naturalize", std::make_unique<PlayerbotNaturalizeAction>(_ai));
    // _ai->AddAction("hover", std::make_unique<PlayerbotHoverAction>(_ai));
    // _ai->AddAction("deep breath", std::make_unique<PlayerbotDeepBreathAction>(_ai));
}

void PlayerbotEvokerStrategy::RegisterCommonTriggers()
{
    // Common triggers commented for later implementation
    // _ai->AddTrigger("enemy casting interrupt", std::make_unique<PlayerbotEnemyCastingInterruptTrigger>(_ai));
    // _ai->AddTrigger("party member low health rescue", std::make_unique<PlayerbotPartyMemberLowHealthRescueTrigger>(_ai));
    // _ai->AddTrigger("need dispel magic", std::make_unique<PlayerbotNeedDispelMagicTrigger>(_ai));
    // _ai->AddTrigger("need dispel disease", std::make_unique<PlayerbotNeedDispelDiseaseTrigger>(_ai));
    // _ai->AddTrigger("need dispel poison", std::make_unique<PlayerbotNeedDispelPoisonTrigger>(_ai));
}

float PlayerbotEvokerStrategy::GetActionPriority(std::string const& actionName) const
{
    if (actionName == "quell" && ShouldUseQuell()) return 1.0f;
    if (actionName == "rescue" && ShouldUseRescue()) return 0.95f;
    if (actionName == "expunge" && ShouldUseExpunge()) return 0.9f;
    if (actionName == "naturalize" && ShouldUseNaturalize()) return 0.85f;
    if (actionName == "sleep walk" && ShouldUseSleep()) return 0.8f;
    if (actionName == "time spiral" && ShouldUseTimeSpiral()) return 0.75f;
    if (actionName == "hover" && ShouldUseHover()) return 0.7f;
    if (actionName == "landslide" && ShouldUseLandslide()) return 0.65f;
    if (actionName == "tail swipe" && ShouldUseTailSwipe()) return 0.6f;
    if (actionName == "wing buffet" && ShouldUseWingBuffet()) return 0.55f;
    if (actionName == "deep breath" && ShouldUseDeepBreath()) return 0.5f;
    
    return PlayerbotStrategy::GetActionPriority(actionName);
}

bool PlayerbotEvokerStrategy::ShouldUseQuell() const
{
    Unit* target = _ai->GetBot()->GetVictim();
    return target && target->IsNonMeleeSpellCast(false) && IsInRange(target, 25.0f);
}

bool PlayerbotEvokerStrategy::ShouldUseTailSwipe() const
{
    return CountNearbyEnemies(6.0f) >= 2;
}

bool PlayerbotEvokerStrategy::ShouldUseWingBuffet() const
{
    Unit* target = _ai->GetBot()->GetVictim();
    return target && IsInRange(target, 15.0f) && CountNearbyEnemies(15.0f) >= 3;
}

bool PlayerbotEvokerStrategy::ShouldUseExpunge() const
{
    Player* bot = _ai->GetBot();
    if (!bot->GetGroup())
        return false;
        
    Group::MemberSlotList const& groupSlot = bot->GetGroup()->GetMemberSlots();
    for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); ++itr)
    {
        Player* member = ObjectAccessor::FindPlayer(itr->guid);
        if (member && member->IsAlive() && bot->IsInRange(member, 30.0f))
        {
            if (member->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE) || 
                member->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE_PERCENT))
                return true;
        }
    }
    return false;
}

bool PlayerbotEvokerStrategy::ShouldUseNaturalize() const
{
    Player* bot = _ai->GetBot();
    if (!bot->GetGroup())
        return false;
        
    Group::MemberSlotList const& groupSlot = bot->GetGroup()->GetMemberSlots();
    for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); ++itr)
    {
        Player* member = ObjectAccessor::FindPlayer(itr->guid);
        if (member && member->IsAlive() && bot->IsInRange(member, 30.0f))
        {
            if (member->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE) || 
                member->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED))
                return true;
        }
    }
    return false;
}

bool PlayerbotEvokerStrategy::ShouldUseSleep() const
{
    Unit* target = _ai->GetBot()->GetVictim();
    return target && !target->HasAuraType(SPELL_AURA_MOD_FEAR) && 
           CountNearbyEnemies(30.0f) >= 3 && IsInRange(target, 30.0f);
}

bool PlayerbotEvokerStrategy::ShouldUseRescue() const
{
    Player* bot = _ai->GetBot();
    if (!bot->GetGroup())
        return false;
        
    Group::MemberSlotList const& groupSlot = bot->GetGroup()->GetMemberSlots();
    for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); ++itr)
    {
        Player* member = ObjectAccessor::FindPlayer(itr->guid);
        if (member && member->IsAlive() && bot->IsInRange(member, 40.0f))
        {
            if (member->GetHealthPct() < 30.0f && member->GetVictim())
                return true;
        }
    }
    return false;
}

bool PlayerbotEvokerStrategy::ShouldUseTimeSpiral() const
{
    Player* bot = _ai->GetBot();
    return bot->GetHealthPct() < 40.0f && IsInCombat();
}

bool PlayerbotEvokerStrategy::ShouldUseHover() const
{
    Unit* target = _ai->GetBot()->GetVictim();
    return target && !IsInRange(target, 15.0f) && IsInRange(target, 30.0f);
}

bool PlayerbotEvokerStrategy::ShouldUseLandslide() const
{
    return CountNearbyEnemies(8.0f) >= 2;
}

bool PlayerbotEvokerStrategy::ShouldUseDeepBreath() const
{
    return CountNearbyEnemies(50.0f) >= 5 && ShouldUseBurstCooldowns();
}

bool PlayerbotEvokerStrategy::ShouldUseSoar() const
{
    return false;
}

uint32 PlayerbotEvokerStrategy::GetEssenceAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_ESSENCE) : 0;
}

uint32 PlayerbotEvokerStrategy::GetEssencePercent() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 maxEssence = bot->GetMaxPower(POWER_ESSENCE);
    if (maxEssence == 0)
        return 0;
    
    return (GetEssenceAmount() * 100) / maxEssence;
}

bool PlayerbotEvokerStrategy::HasEnoughEssence(uint32 cost) const
{
    return GetEssenceAmount() >= cost;
}

bool PlayerbotEvokerStrategy::ShouldConserveEssence() const
{
    return GetEssenceAmount() <= 2 && IsInCombat();
}

bool PlayerbotEvokerStrategy::isDevastation() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(EvokerSpells::DISINTEGRATE);
}

bool PlayerbotEvokerStrategy::isPreservation() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(EvokerSpells::VERDANT_EMBRACE);
}

bool PlayerbotEvokerStrategy::isAugmentation() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(EvokerSpells::ERUPTION);
}

Unit* PlayerbotEvokerStrategy::FindBestTarget() const
{
    Unit* currentTarget = _ai->GetBot()->GetVictim();
    if (currentTarget && currentTarget->IsAlive() && IsInRange(currentTarget, 25.0f))
        return currentTarget;
    
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_ai->GetBot(), _ai->GetBot(), 25.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_ai->GetBot(), targets, u_check);
    Cell::VisitAllObjects(_ai->GetBot(), searcher, 25.0f);
    
    if (!targets.empty())
        return targets.front();
    
    return nullptr;
}

bool PlayerbotEvokerStrategy::IsInCombat() const
{
    return _ai->GetBot()->IsInCombat();
}

uint32 PlayerbotEvokerStrategy::CountNearbyEnemies(float range) const
{
    uint32 count = 0;
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_ai->GetBot(), _ai->GetBot(), range);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_ai->GetBot(), targets, u_check);
    Cell::VisitAllObjects(_ai->GetBot(), searcher, range);
    return static_cast<uint32>(targets.size());
}

uint32 PlayerbotEvokerStrategy::CountNearbyAllies(float range) const
{
    uint32 count = 0;
    Player* bot = _ai->GetBot();
    if (!bot->GetGroup())
        return 1;
        
    Group::MemberSlotList const& groupSlot = bot->GetGroup()->GetMemberSlots();
    for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); ++itr)
    {
        Player* member = ObjectAccessor::FindPlayer(itr->guid);
        if (member && member->IsAlive() && bot->IsInRange(member, range))
            count++;
    }
    return count;
}

uint32 PlayerbotEvokerStrategy::CountInjuredAllies(float healthPercent, float range) const
{
    uint32 count = 0;
    Player* bot = _ai->GetBot();
    if (!bot->GetGroup())
        return bot->GetHealthPct() < healthPercent ? 1 : 0;
        
    Group::MemberSlotList const& groupSlot = bot->GetGroup()->GetMemberSlots();
    for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); ++itr)
    {
        Player* member = ObjectAccessor::FindPlayer(itr->guid);
        if (member && member->IsAlive() && bot->IsInRange(member, range) && 
            member->GetHealthPct() < healthPercent)
            count++;
    }
    return count;
}

bool PlayerbotEvokerStrategy::ShouldUseAoE() const
{
    return CountNearbyEnemies(15.0f) >= 3;
}

bool PlayerbotEvokerStrategy::IsInRange(Unit* target, float range) const
{
    return target && _ai->GetBot()->IsInRange(target, range);
}

bool PlayerbotEvokerStrategy::ShouldChargeEmpoweredSpell() const
{
    return HasTimeForEmpower() && HasEnoughEssence(3);
}

bool PlayerbotEvokerStrategy::HasTimeForEmpower() const
{
    Unit* target = _ai->GetBot()->GetVictim();
    return target && !target->IsNonMeleeSpellCast(false) && IsInCombat();
}

uint32 PlayerbotEvokerStrategy::GetOptimalEmpowerLevel() const
{
    if (!IsInCombat())
        return 4;
    
    uint32 enemyCount = CountNearbyEnemies(25.0f);
    if (enemyCount >= 5)
        return 4;
    else if (enemyCount >= 3)
        return 3;
    else
        return 2;
}

bool PlayerbotEvokerStrategy::IsChannelingEmpoweredSpell() const
{
    Player* bot = _ai->GetBot();
    Spell* currentSpell = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
    return currentSpell != nullptr;
}

bool PlayerbotEvokerStrategy::HasDragonrage() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(EvokerSpells::DRAGONRAGE);
}

bool PlayerbotEvokerStrategy::HasSnapfire() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(EvokerSpells::SNAPFIRE);
}

bool PlayerbotEvokerStrategy::HasBurnout() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(EvokerSpells::BURNOUT);
}

bool PlayerbotEvokerStrategy::HasChargedBlast() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(EvokerSpells::CHARGED_BLAST);
}

bool PlayerbotEvokerStrategy::HasIridescenceRed() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(EvokerSpells::IRIDESCENCE_RED);
}

bool PlayerbotEvokerStrategy::HasIridescenceBlue() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(EvokerSpells::IRIDESCENCE_BLUE);
}

bool PlayerbotEvokerStrategy::HasEssenceBurst() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(EvokerSpells::ESSENCE_BURST);
}

bool PlayerbotEvokerStrategy::HasLeapingFlames(Unit* target) const
{
    if (!target)
        target = _ai->GetBot()->GetVictim();
    
    return target && target->HasAura(EvokerSpells::LEAPING_FLAMES);
}

bool PlayerbotEvokerStrategy::HasCallOfYsera() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(EvokerSpells::CALL_OF_YSERA);
}

bool PlayerbotEvokerStrategy::HasEmeraldTrance() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(EvokerSpells::EMERALD_TRANCE);
}

bool PlayerbotEvokerStrategy::ShouldUseBurstCooldowns() const
{
    Unit* target = _ai->GetBot()->GetVictim();
    return target && (target->GetHealthPct() > 80.0f || CountNearbyEnemies(15.0f) >= 3);
}

bool PlayerbotEvokerStrategy::IsInDragonrageWindow() const
{
    return HasDragonrage();
}

bool PlayerbotEvokerStrategy::NeedsHealing() const
{
    return _ai->GetBot()->GetHealthPct() < 70.0f;
}

bool PlayerbotEvokerStrategy::IsInEmergency() const
{
    return _ai->GetBot()->GetHealthPct() < 30.0f;
}

// ===========================================
// PlayerbotDevastationEvokerStrategy
// ===========================================

PlayerbotDevastationEvokerStrategy::PlayerbotDevastationEvokerStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerStrategy(ai, "devastation")
{
}

void PlayerbotDevastationEvokerStrategy::InitializeActions()
{
    PlayerbotEvokerStrategy::InitializeActions();
    RegisterDevastationActions();
}

void PlayerbotDevastationEvokerStrategy::InitializeTriggers()
{
    PlayerbotEvokerStrategy::InitializeTriggers();
    RegisterDevastationTriggers();
}

void PlayerbotDevastationEvokerStrategy::RegisterDevastationActions()
{
    // Devastation actions commented for later trigger implementation
    // _ai->AddAction("azure strike", std::make_unique<PlayerbotAzureStrikeAction>(_ai));
    // _ai->AddAction("living flame", std::make_unique<PlayerbotLivingFlameAction>(_ai));
    // _ai->AddAction("disintegrate", std::make_unique<PlayerbotDisintegrateAction>(_ai));
    // _ai->AddAction("fire breath", std::make_unique<PlayerbotFireBreathAction>(_ai));
    // _ai->AddAction("eternity surge", std::make_unique<PlayerbotEternitySurgeAction>(_ai));
    // _ai->AddAction("pyre", std::make_unique<PlayerbotPyreAction>(_ai));
    // _ai->AddAction("dragonrage", std::make_unique<PlayerbotDragonrageAction>(_ai));
    // _ai->AddAction("tip the scales", std::make_unique<PlayerbotTipTheScalesAction>(_ai));
    // _ai->AddAction("time skip", std::make_unique<PlayerbotTimeSkipAction>(_ai));
}

void PlayerbotDevastationEvokerStrategy::RegisterDevastationTriggers()
{
    // Devastation triggers commented for later implementation
    // _ai->AddTrigger("essence management", std::make_unique<PlayerbotEssenceManagementTrigger>(_ai));
    // _ai->AddTrigger("dragonrage window", std::make_unique<PlayerbotDragonrageWindowTrigger>(_ai));
    // _ai->AddTrigger("empowered spell ready", std::make_unique<PlayerbotEmpoweredSpellReadyTrigger>(_ai));
    // _ai->AddTrigger("dot refresh needed", std::make_unique<PlayerbotDotRefreshNeededTrigger>(_ai));
    // _ai->AddTrigger("aoe rotation needed", std::make_unique<PlayerbotAoeRotationNeededTrigger>(_ai));
}

float PlayerbotDevastationEvokerStrategy::GetActionPriority(std::string const& actionName) const
{
    // Major cooldowns (highest priority)
    if (actionName == "dragonrage" && ShouldUseDragonrage()) return 1.0f;
    if (actionName == "tip the scales" && ShouldUseTipTheScales()) return 0.95f;
    if (actionName == "time skip" && ShouldUseTimeSkip()) return 0.9f;
    
    // Empowered spells (high priority)
    if (actionName == "fire breath" && ShouldUseFireBreath()) return 0.85f;
    if (actionName == "eternity surge" && ShouldUseEternitySurge()) return 0.8f;
    
    // Core rotation
    if (actionName == "disintegrate" && ShouldUseDisintegrate()) return 0.75f;
    if (actionName == "pyre" && ShouldUsePyre()) return 0.7f;
    if (actionName == "shattershard" && ShouldUseShattershard()) return 0.65f;
    if (actionName == "living flame" && ShouldUseLivingFlame()) return 0.6f;
    if (actionName == "azure strike" && ShouldUseAzureStrike()) return 0.55f;
    
    // Utility and minor abilities
    if (actionName == "snapfire" && ShouldUseSnapfire()) return 0.5f;
    if (actionName == "firestorm" && ShouldUseFirestorm()) return 0.45f;
    if (actionName == "engulf" && ShouldUseEngulf()) return 0.4f;
    if (actionName == "onyx legacy" && ShouldUseOnyxLegacy()) return 0.35f;
    
    return PlayerbotEvokerStrategy::GetActionPriority(actionName);
}

bool PlayerbotDevastationEvokerStrategy::ShouldUseAzureStrike() const
{
    return ShouldBuildEssence() && !ShouldConserveEssence();
}

bool PlayerbotDevastationEvokerStrategy::ShouldUseLivingFlame() const
{
    Unit* target = _ai->GetBot()->GetVictim();
    return target && (!HasLivingFlameDoT(target) || ShouldRefreshDoTs()) && IsInRange(target, 25.0f);
}

bool PlayerbotDevastationEvokerStrategy::ShouldUseDisintegrate() const
{
    return HasEnoughEssence(3) && ShouldSpendEssence() && ShouldUseSingleTargetRotation();
}

bool PlayerbotDevastationEvokerStrategy::ShouldUseFireBreath() const
{
    return HasEnoughEssence(4) && ShouldUseEmpoweredSpells() && HasTimeForEmpower();
}

bool PlayerbotDevastationEvokerStrategy::ShouldUseEternitySurge() const
{
    return HasEnoughEssence(3) && ShouldUseEmpoweredSpells() && HasTimeForEmpower() && 
           (CountNearbyEnemies(25.0f) >= 2 || IsInDragonrageWindow());
}

bool PlayerbotDevastationEvokerStrategy::ShouldUsePyre() const
{
    return HasEnoughEssence(3) && ShouldUseAoERotation() && CountNearbyEnemies(8.0f) >= 3;
}

bool PlayerbotDevastationEvokerStrategy::ShouldUseShattershard() const
{
    return HasEnoughEssence(2) && CountNearbyEnemies(15.0f) >= 2;
}

bool PlayerbotDevastationEvokerStrategy::ShouldUseDragonrage() const
{
    return ShouldUseBurstCooldowns() && HasEnoughEssence(DRAGONRAGE_ESSENCE_THRESHOLD) && 
           !HasDragonrage() && IsInCombat();
}

bool PlayerbotDevastationEvokerStrategy::ShouldUseTipTheScales() const
{
    return IsInDragonrageWindow() && ShouldUseEmpoweredSpells() && HasEnoughEssence(4);
}

bool PlayerbotDevastationEvokerStrategy::ShouldUseTimeSkip() const
{
    return ShouldUseBurstCooldowns() && IsInCombat() && HasEnoughEssence(2);
}

bool PlayerbotDevastationEvokerStrategy::ShouldUseSnapfire() const
{
    return HasSnapfire() && IsInCombat();
}

bool PlayerbotDevastationEvokerStrategy::ShouldUseFirestorm() const
{
    return CountNearbyEnemies(12.0f) >= 4 && HasEnoughEssence(3);
}

bool PlayerbotDevastationEvokerStrategy::ShouldUseEngulf() const
{
    Unit* target = _ai->GetBot()->GetVictim();
    return target && target->GetHealthPct() < 35.0f && HasEnoughEssence(3);
}

bool PlayerbotDevastationEvokerStrategy::ShouldUseOnyxLegacy() const
{
    return CountNearbyEnemies(8.0f) >= 2 && HasEnoughEssence(2);
}

bool PlayerbotDevastationEvokerStrategy::ShouldBuildEssence() const
{
    return GetEssenceAmount() < OPTIMAL_ESSENCE_COUNT;
}

bool PlayerbotDevastationEvokerStrategy::ShouldSpendEssence() const
{
    return GetEssenceAmount() >= OPTIMAL_ESSENCE_COUNT || IsAtMaxEssence();
}

bool PlayerbotDevastationEvokerStrategy::HasOptimalEssenceCount() const
{
    return GetEssenceAmount() >= OPTIMAL_ESSENCE_COUNT;
}

bool PlayerbotDevastationEvokerStrategy::IsAtMaxEssence() const
{
    return GetEssenceAmount() >= MAX_ESSENCE_COUNT;
}

bool PlayerbotDevastationEvokerStrategy::ShouldUseAoERotation() const
{
    return CountNearbyEnemies(15.0f) >= AOE_ROTATION_COUNT;
}

bool PlayerbotDevastationEvokerStrategy::ShouldUseSingleTargetRotation() const
{
    return !ShouldUseAoERotation();
}

bool PlayerbotDevastationEvokerStrategy::IsInDragonrageWindow() const
{
    return HasDragonrage();
}

bool PlayerbotDevastationEvokerStrategy::ShouldUseEmpoweredSpells() const
{
    return HasTimeForEmpower() && (IsInDragonrageWindow() || ShouldUseBurstCooldowns());
}

bool PlayerbotDevastationEvokerStrategy::ShouldUseMajorCooldowns() const
{
    Unit* target = _ai->GetBot()->GetVictim();
    return target && target->GetHealthPct() > BURST_COOLDOWN_HEALTH;
}

bool PlayerbotDevastationEvokerStrategy::ShouldMaintainLivingFlame() const
{
    Unit* target = _ai->GetBot()->GetVictim();
    return target && IsInRange(target, 25.0f);
}

bool PlayerbotDevastationEvokerStrategy::ShouldApplyLeapingFlames() const
{
    return CountNearbyEnemies(8.0f) >= 2;
}

bool PlayerbotDevastationEvokerStrategy::HasLivingFlameDoT(Unit* target) const
{
    if (!target)
        target = _ai->GetBot()->GetVictim();
    
    return target && target->HasAura(EvokerSpells::LIVING_FLAME_DOT);
}

bool PlayerbotDevastationEvokerStrategy::ShouldRefreshDoTs() const
{
    Unit* target = _ai->GetBot()->GetVictim();
    if (!target)
        return false;
    
    if (Aura* aura = target->GetAura(EvokerSpells::LIVING_FLAME_DOT))
        return aura->GetDuration() < (aura->GetMaxDuration() * DOT_REFRESH_THRESHOLD);
    
    return true;
}

bool PlayerbotDevastationEvokerStrategy::ShouldMaintainSnapfire() const
{
    return HasSnapfire() || (IsInDragonrageWindow() && HasEnoughEssence(1));
}

bool PlayerbotDevastationEvokerStrategy::ShouldActivateBurnout() const
{
    return HasBurnout() && IsInCombat();
}

bool PlayerbotDevastationEvokerStrategy::HasEssenceBurstStacks() const
{
    Player* bot = _ai->GetBot();
    if (Aura* aura = bot->GetAura(EvokerSpells::ESSENCE_BURST))
        return aura->GetStackAmount() >= 2;
    
    return false;
}

bool PlayerbotDevastationEvokerStrategy::ShouldConsumeIridescence() const
{
    return HasIridescenceRed() || HasIridescenceBlue();
}

bool PlayerbotDevastationEvokerStrategy::ShouldPrioritizeDisintegrate() const
{
    return ShouldUseSingleTargetRotation() && HasOptimalEssenceCount();
}

bool PlayerbotDevastationEvokerStrategy::ShouldPrioritizeFireBreath() const
{
    return IsInDragonrageWindow() && HasEnoughEssence(4);
}

bool PlayerbotDevastationEvokerStrategy::ShouldPrioritizeEternitySurge() const
{
    return CountNearbyEnemies(25.0f) >= 2 && HasEnoughEssence(3);
}

bool PlayerbotDevastationEvokerStrategy::ShouldPrioritizePyre() const
{
    return ShouldUseAoERotation() && HasEnoughEssence(3);
}

bool PlayerbotDevastationEvokerStrategy::IsOptimallyPositioned() const
{
    Unit* target = _ai->GetBot()->GetVictim();
    return target && IsInRange(target, OPTIMAL_RANGE) && !IsInRange(target, 8.0f);
}

bool PlayerbotDevastationEvokerStrategy::ShouldRepositionForCleave() const
{
    return CountNearbyEnemies(8.0f) >= 2 && !IsOptimallyPositioned();
}

bool PlayerbotDevastationEvokerStrategy::IsInOptimalRange() const
{
    Unit* target = _ai->GetBot()->GetVictim();
    return target && IsInRange(target, OPTIMAL_RANGE);
}

// ===========================================
// PlayerbotPreservationEvokerStrategy
// ===========================================

PlayerbotPreservationEvokerStrategy::PlayerbotPreservationEvokerStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerStrategy(ai, "preservation")
{
}

void PlayerbotPreservationEvokerStrategy::InitializeActions()
{
    PlayerbotEvokerStrategy::InitializeActions();
    RegisterPreservationActions();
}

void PlayerbotPreservationEvokerStrategy::InitializeTriggers()
{
    PlayerbotEvokerStrategy::InitializeTriggers();
    RegisterPreservationTriggers();
}

void PlayerbotPreservationEvokerStrategy::RegisterPreservationActions()
{
    // Preservation actions commented for later trigger implementation
    // _ai->AddAction("emerald blossom", std::make_unique<PlayerbotEmeraldBlossomAction>(_ai));
    // _ai->AddAction("verdant embrace", std::make_unique<PlayerbotVerdantEmbraceAction>(_ai));
    // _ai->AddAction("dream breath", std::make_unique<PlayerbotDreamBreathAction>(_ai));
    // _ai->AddAction("spiritbloom", std::make_unique<PlayerbotSpiritbloomAction>(_ai));
    // _ai->AddAction("life givers flame", std::make_unique<PlayerbotLifeGiversFlameAction>(_ai));
    // _ai->AddAction("reversion", std::make_unique<PlayerbotReversionAction>(_ai));
    // _ai->AddAction("echo", std::make_unique<PlayerbotEchoAction>(_ai));
    // _ai->AddAction("stasis", std::make_unique<PlayerbotStasisAction>(_ai));
    // _ai->AddAction("rewind", std::make_unique<PlayerbotRewindAction>(_ai));
}

void PlayerbotPreservationEvokerStrategy::RegisterPreservationTriggers()
{
    // Preservation triggers commented for later implementation
    // _ai->AddTrigger("party member low health", std::make_unique<PlayerbotPartyMemberLowHealthTrigger>(_ai));
    // _ai->AddTrigger("party member critical health", std::make_unique<PlayerbotPartyMemberCriticalHealthTrigger>(_ai));
    // _ai->AddTrigger("tank low health", std::make_unique<PlayerbotTankLowHealthTrigger>(_ai));
    // _ai->AddTrigger("party needs aoe healing", std::make_unique<PlayerbotPartyNeedsAoeHealingTrigger>(_ai));
    // _ai->AddTrigger("echo effect available", std::make_unique<PlayerbotEchoEffectAvailableTrigger>(_ai));
    // _ai->AddTrigger("hot refresh needed", std::make_unique<PlayerbotHotRefreshNeededTrigger>(_ai));
}

float PlayerbotPreservationEvokerStrategy::GetActionPriority(std::string const& actionName) const
{
    // Emergency healing (highest priority)
    if (actionName == "stasis" && ShouldUseStasis()) return 1.0f;
    if (actionName == "rewind" && ShouldUseRewind()) return 0.95f;
    if (actionName == "life givers flame" && ShouldUseLifeGiversFlame()) return 0.9f;
    
    // Major healing cooldowns
    if (actionName == "time skip healing" && ShouldUseTimeSkipHealing()) return 0.85f;
    if (actionName == "dream breath" && ShouldUseDreamBreath()) return 0.8f;
    if (actionName == "spiritbloom" && ShouldUseSpiritbloom()) return 0.75f;
    
    // Core healing rotation
    if (actionName == "verdant embrace" && ShouldUseVerdantEmbrace()) return 0.7f;
    if (actionName == "emerald blossom" && ShouldUseEmeraldBlossom()) return 0.65f;
    if (actionName == "reversion" && ShouldUseReversion()) return 0.6f;
    if (actionName == "echo" && ShouldUseEcho()) return 0.55f;
    
    // Utility and support
    if (actionName == "temporal anomaly" && ShouldUseTemporalAnomaly()) return 0.5f;
    if (actionName == "blessing of the bronze" && ShouldUseBlessingOfTheBronze()) return 0.45f;
    if (actionName == "zephyr" && ShouldUseZephyr()) return 0.4f;
    if (actionName == "chrono" && ShouldUseChrono()) return 0.35f;
    if (actionName == "flow state" && ShouldUseFlowState()) return 0.3f;
    if (actionName == "field of dreams" && ShouldUseFieldOfDreams()) return 0.25f;
    
    return PlayerbotEvokerStrategy::GetActionPriority(actionName);
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseEmeraldBlossom() const
{
    return CountInjuredAllies(AOE_HEALING_THRESHOLD) >= AOE_HEALING_COUNT && 
           HasEnoughEssence(3) && !ShouldConserveEssenceForEmergency();
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseVerdantEmbrace() const
{
    Unit* healTarget = FindBestHealTarget();
    return healTarget && healTarget->GetHealthPct() < 80.0f && HasEnoughEssence(3);
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseDreamBreath() const
{
    return CountInjuredAllies(70.0f) >= 3 && HasEnoughEssence(4) && 
           HasTimeForEmpower() && ShouldUseEmpoweredHealing();
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseSpiritbloom() const
{
    return CountInjuredAllies(60.0f) >= 2 && HasEnoughEssence(3) && HasTimeForEmpower();
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseLifeGiversFlame() const
{
    Unit* healTarget = FindLowestHealthAlly();
    return healTarget && healTarget->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD * 100.0f;
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseReversion() const
{
    Unit* healTarget = FindBestHealTarget();
    return healTarget && !HasReversionHoT(healTarget) && 
           healTarget->GetHealthPct() < 70.0f && HasEnoughEssence(1);
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseEcho() const
{
    Unit* healTarget = FindBestHealTarget();
    return healTarget && !HasEchoEffect(healTarget) && IsInCombat() && HasEnoughEssence(2);
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseTemporalAnomaly() const
{
    return IsInCombat() && CountNearbyAllies(40.0f) >= 3 && HasEnoughEssence(3);
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseStasis() const
{
    return IsInEmergencyHealingMode() && HasEnoughEssence(4);
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseRewind() const
{
    return CountInjuredAllies(EMERGENCY_HEALTH_THRESHOLD) >= 2 && IsInCombat();
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseTimeSkipHealing() const
{
    return ShouldUseMajorHealingCooldowns() && CountInjuredAllies(50.0f) >= 3;
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseBlessingOfTheBronze() const
{
    Unit* tankTarget = FindTankTarget();
    return tankTarget && tankTarget->GetHealthPct() < 60.0f && IsInCombat();
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseZephyr() const
{
    return CountNearbyAllies(15.0f) >= 3 && IsInCombat();
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseChrono() const
{
    return IsInCombat() && HasEnoughEssence(2);
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseFlowState() const
{
    return GetPartyHealthPercentage() < 60.0f && HasEnoughEssence(3);
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseFieldOfDreams() const
{
    return CountInjuredAllies(70.0f) >= 4 && IsInCombat() && HasEnoughEssence(4);
}

Unit* PlayerbotPreservationEvokerStrategy::FindBestHealTarget() const
{
    Unit* lowestHealthTarget = FindLowestHealthAlly();
    Unit* tankTarget = FindTankTarget();
    
    if (tankTarget && tankTarget->GetHealthPct() < TANK_PRIORITY_THRESHOLD * 100.0f)
        return tankTarget;
    
    return lowestHealthTarget;
}

Unit* PlayerbotPreservationEvokerStrategy::FindLowestHealthAlly() const
{
    Player* bot = _ai->GetBot();
    Unit* lowestHealthTarget = nullptr;
    float lowestHealthPct = 100.0f;
    
    if (bot->GetHealthPct() < lowestHealthPct)
    {
        lowestHealthPct = bot->GetHealthPct();
        lowestHealthTarget = bot;
    }
    
    if (!bot->GetGroup())
        return lowestHealthTarget;
    
    Group::MemberSlotList const& groupSlot = bot->GetGroup()->GetMemberSlots();
    for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); ++itr)
    {
        Player* member = ObjectAccessor::FindPlayer(itr->guid);
        if (member && member->IsAlive() && bot->IsInRange(member, 40.0f) && 
            member->GetHealthPct() < lowestHealthPct)
        {
            lowestHealthPct = member->GetHealthPct();
            lowestHealthTarget = member;
        }
    }
    
    return lowestHealthTarget;
}

Unit* PlayerbotPreservationEvokerStrategy::FindTankTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot->GetGroup())
        return nullptr;
    
    Group::MemberSlotList const& groupSlot = bot->GetGroup()->GetMemberSlots();
    for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); ++itr)
    {
        Player* member = ObjectAccessor::FindPlayer(itr->guid);
        if (member && member->IsAlive() && bot->IsInRange(member, 40.0f))
        {
            if (member->GetPrimaryTalentTree() == TALENT_TREE_PALADIN_PROTECTION ||
                member->GetPrimaryTalentTree() == TALENT_TREE_WARRIOR_PROTECTION ||
                member->GetPrimaryTalentTree() == TALENT_TREE_DEATH_KNIGHT_BLOOD ||
                member->GetPrimaryTalentTree() == TALENT_TREE_DEMON_HUNTER_VENGEANCE ||
                member->GetPrimaryTalentTree() == TALENT_TREE_DRUID_BEAR ||
                member->GetPrimaryTalentTree() == TALENT_TREE_MONK_BREWMASTER)
                return member;
        }
    }
    
    return nullptr;
}

std::vector<Unit*> PlayerbotPreservationEvokerStrategy::FindHealingTargets(uint32 maxTargets) const
{
    std::vector<Unit*> healingTargets;
    Player* bot = _ai->GetBot();
    
    if (bot->GetHealthPct() < 90.0f)
        healingTargets.push_back(bot);
    
    if (!bot->GetGroup())
        return healingTargets;
    
    Group::MemberSlotList const& groupSlot = bot->GetGroup()->GetMemberSlots();
    for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end() && 
         healingTargets.size() < maxTargets; ++itr)
    {
        Player* member = ObjectAccessor::FindPlayer(itr->guid);
        if (member && member->IsAlive() && bot->IsInRange(member, 40.0f) && 
            member->GetHealthPct() < 90.0f && member != bot)
        {
            healingTargets.push_back(member);
        }
    }
    
    return healingTargets;
}

bool PlayerbotPreservationEvokerStrategy::ShouldPrioritizeTank() const
{
    Unit* tankTarget = FindTankTarget();
    return tankTarget && tankTarget->GetHealthPct() < TANK_PRIORITY_THRESHOLD * 100.0f;
}

bool PlayerbotPreservationEvokerStrategy::ShouldBuildEssenceForHealing() const
{
    return GetEssenceAmount() < OPTIMAL_ESSENCE_FOR_HEALING && !IsInEmergencyHealingMode();
}

bool PlayerbotPreservationEvokerStrategy::ShouldSpendEssenceOnHealing() const
{
    return GetEssenceAmount() >= OPTIMAL_ESSENCE_FOR_HEALING || 
           IsInEmergencyHealingMode() || CountInjuredAllies(50.0f) >= 3;
}

bool PlayerbotPreservationEvokerStrategy::HasOptimalEssenceForHealing() const
{
    return GetEssenceAmount() >= OPTIMAL_ESSENCE_FOR_HEALING;
}

bool PlayerbotPreservationEvokerStrategy::ShouldConserveEssenceForEmergency() const
{
    return GetEssenceAmount() <= EMERGENCY_ESSENCE_RESERVE && IsInEmergencyHealingMode();
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseAoEHealing() const
{
    return CountInjuredAllies(AOE_HEALING_THRESHOLD) >= AOE_HEALING_COUNT;
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseSingleTargetHealing() const
{
    return !ShouldUseAoEHealing() || IsInEmergencyHealingMode();
}

bool PlayerbotPreservationEvokerStrategy::IsInEmergencyHealingMode() const
{
    return CountInjuredAllies(EMERGENCY_HEALTH_THRESHOLD) >= 1;
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseEmpoweredHealing() const
{
    return HasTimeForEmpower() && (CountInjuredAllies(EMPOWERED_HEALING_THRESHOLD) >= 2 || 
           IsInEmergencyHealingMode());
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseMajorHealingCooldowns() const
{
    return IsInEmergencyHealingMode() || GetPartyHealthPercentage() < 50.0f;
}

bool PlayerbotPreservationEvokerStrategy::ShouldMaintainReversion(Unit* target) const
{
    return target && target->GetHealthPct() < 80.0f && !HasReversionHoT(target);
}

bool PlayerbotPreservationEvokerStrategy::ShouldApplyEcho(Unit* target) const
{
    return target && !HasEchoEffect(target) && target->GetHealthPct() < 70.0f;
}

bool PlayerbotPreservationEvokerStrategy::HasReversionHoT(Unit* target) const
{
    return target && target->HasAura(EvokerSpells::REVERSION);
}

bool PlayerbotPreservationEvokerStrategy::HasEchoEffect(Unit* target) const
{
    return target && target->HasAura(EvokerSpells::ECHO);
}

bool PlayerbotPreservationEvokerStrategy::ShouldRefreshHoTs() const
{
    std::vector<Unit*> healingTargets = FindHealingTargets(5);
    for (Unit* target : healingTargets)
    {
        if (Aura* aura = target->GetAura(EvokerSpells::REVERSION))
        {
            if (aura->GetDuration() < (aura->GetMaxDuration() * HoT_REFRESH_THRESHOLD))
                return true;
        }
    }
    return false;
}

bool PlayerbotPreservationEvokerStrategy::IsPartyInDanger() const
{
    return CountInjuredAllies(40.0f) >= 2;
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseEmergencyHealing() const
{
    return IsInEmergencyHealingMode();
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseRescueUtility() const
{
    return ShouldUseRescue();
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseTemporalUtility() const
{
    return IsPartyInDanger() && IsInCombat();
}

bool PlayerbotPreservationEvokerStrategy::ShouldPrioritizeVerdantEmbrace() const
{
    Unit* healTarget = FindBestHealTarget();
    return healTarget && healTarget->GetHealthPct() < 50.0f;
}

bool PlayerbotPreservationEvokerStrategy::ShouldPrioritizeDreamBreath() const
{
    return CountInjuredAllies(EMPOWERED_HEALING_THRESHOLD) >= 3 && HasTimeForEmpower();
}

bool PlayerbotPreservationEvokerStrategy::ShouldPrioritizeSpiritbloom() const
{
    return CountInjuredAllies(60.0f) >= 2;
}

bool PlayerbotPreservationEvokerStrategy::ShouldPrioritizeEmeraldBlossom() const
{
    return CountInjuredAllies(AOE_HEALING_THRESHOLD) >= AOE_HEALING_COUNT;
}

bool PlayerbotPreservationEvokerStrategy::HasSufficientMana() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetPowerPct(POWER_MANA) > 20.0f;
}

bool PlayerbotPreservationEvokerStrategy::ShouldUseInnervate() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetPowerPct(POWER_MANA) < 30.0f && IsInCombat();
}

float PlayerbotPreservationEvokerStrategy::GetPartyHealthPercentage() const
{
    Player* bot = _ai->GetBot();
    float totalHealthPct = bot->GetHealthPct();
    uint32 memberCount = 1;
    
    if (!bot->GetGroup())
        return totalHealthPct;
    
    Group::MemberSlotList const& groupSlot = bot->GetGroup()->GetMemberSlots();
    for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); ++itr)
    {
        Player* member = ObjectAccessor::FindPlayer(itr->guid);
        if (member && member->IsAlive() && bot->IsInRange(member, 40.0f) && member != bot)
        {
            totalHealthPct += member->GetHealthPct();
            memberCount++;
        }
    }
    
    return totalHealthPct / memberCount;
}

#endif // WITH_PLAYERBOTS