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

#include "PlayerbotDemonHunterStrategy.h"
#include "PlayerbotDemonHunterActions.h"
#include "PlayerbotPlayerAI.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellHistory.h"
#include "Group.h"

// Base Demon Hunter Strategy Implementation
PlayerbotDemonHunterStrategy::PlayerbotDemonHunterStrategy(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotStrategy(ai, name)
{
}

void PlayerbotDemonHunterStrategy::InitializeActions()
{
    RegisterCommonActions();
}

void PlayerbotDemonHunterStrategy::InitializeTriggers()
{
    RegisterCommonTriggers();
}

void PlayerbotDemonHunterStrategy::RegisterCommonActions()
{
    RegisterAction("consume magic", std::make_unique<CastConsumeMagic>(_ai));
    RegisterAction("disrupt", std::make_unique<CastDisrupt>(_ai));
    RegisterAction("throw glaive", std::make_unique<CastThrowGlaive>(_ai));
    RegisterAction("imprison", std::make_unique<CastImprison>(_ai));
    RegisterAction("torment", std::make_unique<CastTorment>(_ai));
    RegisterAction("blur", std::make_unique<CastBlur>(_ai));
    RegisterAction("darkness", std::make_unique<CastDarkness>(_ai));
    RegisterAction("spectral sight", std::make_unique<CastSpectralSight>(_ai));
    RegisterAction("fel rush", std::make_unique<CastFelRush>(_ai));
    RegisterAction("vengeful retreat", std::make_unique<CastVengefulRetreat>(_ai));
}

void PlayerbotDemonHunterStrategy::RegisterCommonTriggers()
{
    // Common triggers will be implemented when trigger system is ready
}

float PlayerbotDemonHunterStrategy::GetActionPriority(std::string const& actionName) const
{
    // Emergency abilities (highest priority)
    if (actionName == "darkness" && ShouldUseDarkness()) return 1.0f;
    if (actionName == "blur" && ShouldUseBlur()) return 0.95f;
    if (actionName == "vengeful retreat" && ShouldUseVengefulRetreat()) return 0.9f;
    
    // Utility and crowd control
    if (actionName == "disrupt" && ShouldUseDisrupt()) return 0.85f;
    if (actionName == "consume magic" && ShouldUseConsumeMagic()) return 0.8f;
    if (actionName == "imprison" && ShouldUseImprison()) return 0.75f;
    
    // Mobility
    if (actionName == "fel rush" && ShouldUseFelRush()) return 0.3f;
    
    // Utility
    if (actionName == "throw glaive" && ShouldUseThrowGlaive()) return 0.4f;
    if (actionName == "torment" && ShouldUseTorment()) return 0.6f;
    if (actionName == "spectral sight" && ShouldUseSpectralSight()) return 0.1f;
    
    return 0.0f;
}

bool PlayerbotDemonHunterStrategy::ShouldUseConsumeMagic() const
{
    Unit* target = FindBestTarget();
    if (!target)
        return false;
    
    // Check if target has dispellable magic effects
    return target->HasAuraType(SPELL_AURA_SCHOOL_IMMUNITY) || 
           target->HasAuraType(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
}

bool PlayerbotDemonHunterStrategy::ShouldUseDisrupt() const
{
    Unit* target = FindBestTarget();
    return target && target->IsNonMeleeSpellCast(false);
}

bool PlayerbotDemonHunterStrategy::ShouldUseThrowGlaive() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return bot->GetDistance(target) > 8.0f && bot->GetDistance(target) <= 30.0f &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::THROW_GLAIVE);
}

bool PlayerbotDemonHunterStrategy::ShouldUseImprison() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return CountNearbyEnemies() >= 3 && bot->GetDistance(target) <= 30.0f &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::IMPRISON);
}

bool PlayerbotDemonHunterStrategy::ShouldUseTorment() const
{
    if (!isVengeance() || !NeedsThreat())
        return false;
    
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    return target && bot && bot->GetDistance(target) <= 30.0f &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::TORMENT);
}

bool PlayerbotDemonHunterStrategy::ShouldUseBlur() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return bot->GetHealthPct() < 50.0f && IsInCombat() &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::BLUR);
}

bool PlayerbotDemonHunterStrategy::ShouldUseDarkness() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return (bot->GetHealthPct() < 40.0f || CountNearbyAllies(20.0f) >= 3) && IsInCombat() &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::DARKNESS);
}

bool PlayerbotDemonHunterStrategy::ShouldUseSpectralSight() const
{
    Player* bot = _ai->GetBot();
    return bot && !IsInCombat() && !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::SPECTRAL_SIGHT);
}

bool PlayerbotDemonHunterStrategy::ShouldUseFelRush() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return bot->GetDistance(target) > 8.0f && bot->GetDistance(target) <= 15.0f &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::FEL_RUSH);
}

bool PlayerbotDemonHunterStrategy::ShouldUseVengefulRetreat() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetHealthPct() < 40.0f && IsInCombat() &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::VENGEFUL_RETREAT);
}

bool PlayerbotDemonHunterStrategy::ShouldUseGlide() const
{
    // Glide is mainly used for movement, not combat
    return false;
}

uint32 PlayerbotDemonHunterStrategy::GetFuryAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_FURY) : 0;
}

uint32 PlayerbotDemonHunterStrategy::GetFuryPercent() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 maxFury = bot->GetMaxPower(POWER_FURY);
    return maxFury > 0 ? (GetFuryAmount() * 100) / maxFury : 0;
}

uint32 PlayerbotDemonHunterStrategy::GetPainAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_PAIN) : 0;
}

uint32 PlayerbotDemonHunterStrategy::GetPainPercent() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 maxPain = bot->GetMaxPower(POWER_PAIN);
    return maxPain > 0 ? (GetPainAmount() * 100) / maxPain : 0;
}

bool PlayerbotDemonHunterStrategy::HasEnoughFury(uint32 cost) const
{
    return GetFuryAmount() >= cost;
}

bool PlayerbotDemonHunterStrategy::HasEnoughPain(uint32 cost) const
{
    return GetPainAmount() >= cost;
}

bool PlayerbotDemonHunterStrategy::ShouldConserveFury() const
{
    return GetFuryPercent() < 30;
}

bool PlayerbotDemonHunterStrategy::ShouldConservePain() const
{
    return GetPainPercent() < 30;
}

bool PlayerbotDemonHunterStrategy::isHavoc() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(DemonHunterSpells::CHAOS_STRIKE);
}

bool PlayerbotDemonHunterStrategy::isVengeance() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(DemonHunterSpells::SHEAR);
}

Unit* PlayerbotDemonHunterStrategy::FindBestTarget() const
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

bool PlayerbotDemonHunterStrategy::IsInCombat() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->IsInCombat();
}

uint32 PlayerbotDemonHunterStrategy::CountNearbyEnemies(float range) const
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

uint32 PlayerbotDemonHunterStrategy::CountNearbyAllies(float range) const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 count = 0;
    
    // Check self
    count++;
    
    // Check group members
    Group* group = bot->GetGroup();
    if (group)
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (member && member != bot && member->IsAlive() && bot->IsWithinDistInMap(member, range))
                count++;
        }
    }
    
    return count;
}

bool PlayerbotDemonHunterStrategy::ShouldUseAoE() const
{
    return CountNearbyEnemies(8.0f) >= 3;
}

bool PlayerbotDemonHunterStrategy::IsInMeleeRange(Unit* target) const
{
    Player* bot = _ai->GetBot();
    return bot && target && bot->GetDistance(target) <= 5.0f;
}

bool PlayerbotDemonHunterStrategy::HasMetamorphosis() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    return bot->HasAura(DemonHunterSpells::METAMORPHOSIS_HAVOC) || 
           bot->HasAura(DemonHunterSpells::METAMORPHOSIS_TANK);
}

bool PlayerbotDemonHunterStrategy::HasDemonSpikes() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DemonHunterSpells::DEMON_SPIKES);
}

bool PlayerbotDemonHunterStrategy::HasImmolationAura() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    return bot->HasAura(DemonHunterSpells::IMMOLATION_AURA) ||
           bot->HasAura(DemonHunterSpells::IMMOLATION_AURA_TANK);
}

bool PlayerbotDemonHunterStrategy::HasFieryBrand(Unit* target) const
{
    if (!target)
        target = FindBestTarget();
    
    return target && target->HasAura(DemonHunterSpells::FIERY_BRAND);
}

bool PlayerbotDemonHunterStrategy::HasPrepared() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DemonHunterSpells::PREPARED);
}

bool PlayerbotDemonHunterStrategy::HasMomentum() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DemonHunterSpells::MOMENTUM);
}

uint32 PlayerbotDemonHunterStrategy::GetSoulFragments() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    // Count soul fragments around the player
    if (Aura* aura = bot->GetAura(DemonHunterSpells::SOUL_FRAGMENTS))
        return aura->GetStackAmount();
    
    return 0;
}

bool PlayerbotDemonHunterStrategy::ShouldConsumeSoulFragments() const
{
    return GetSoulFragments() >= 4 || (isVengeance() && GetSoulFragments() >= 2);
}

bool PlayerbotDemonHunterStrategy::ShouldUseBurstCooldowns() const
{
    Unit* target = FindBestTarget();
    return target && target->GetHealthPct() > 70.0f && IsInCombat();
}

bool PlayerbotDemonHunterStrategy::IsInDemonicWindow() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DemonHunterSpells::INNER_DEMON);
}

bool PlayerbotDemonHunterStrategy::NeedsThreat() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    return target && bot && target->GetVictim() != bot;
}

// Havoc Demon Hunter Strategy Implementation
PlayerbotHavocDemonHunterStrategy::PlayerbotHavocDemonHunterStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotDemonHunterStrategy(ai, "havoc")
{
}

void PlayerbotHavocDemonHunterStrategy::InitializeActions()
{
    PlayerbotDemonHunterStrategy::InitializeActions();
    RegisterHavocActions();
}

void PlayerbotHavocDemonHunterStrategy::InitializeTriggers()
{
    PlayerbotDemonHunterStrategy::InitializeTriggers();
    RegisterHavocTriggers();
}

void PlayerbotHavocDemonHunterStrategy::RegisterHavocActions()
{
    RegisterAction("demons bite", std::make_unique<CastDemonsBite>(_ai));
    RegisterAction("chaos strike", std::make_unique<CastChaosStrike>(_ai));
    RegisterAction("annihilation", std::make_unique<CastAnnihilation>(_ai));
    RegisterAction("blade dance", std::make_unique<CastBladeDance>(_ai));
    RegisterAction("death sweep", std::make_unique<CastDeathSweep>(_ai));
    RegisterAction("eye beam", std::make_unique<CastEyeBeam>(_ai));
    RegisterAction("immolation aura havoc", std::make_unique<CastImmolationAuraHavoc>(_ai));
    RegisterAction("metamorphosis havoc", std::make_unique<CastMetamorphosisHavoc>(_ai));
    RegisterAction("essence break", std::make_unique<CastEssenceBreak>(_ai));
    RegisterAction("the hunt havoc", std::make_unique<CastTheHuntHavoc>(_ai));
    RegisterAction("glaive tempest", std::make_unique<CastGlaiveTempest>(_ai));
    RegisterAction("elysian decree havoc", std::make_unique<CastElysianDecreeHavoc>(_ai));
    RegisterAction("fel barrage", std::make_unique<CastFelBarrage>(_ai));
}

void PlayerbotHavocDemonHunterStrategy::RegisterHavocTriggers()
{
    // Triggers will be implemented when trigger system is ready
}

float PlayerbotHavocDemonHunterStrategy::GetActionPriority(std::string const& actionName) const
{
    // Get base priorities
    float basePriority = PlayerbotDemonHunterStrategy::GetActionPriority(actionName);
    if (basePriority > 0.0f)
        return basePriority;
    
    // Major cooldowns (highest priority)
    if (actionName == "metamorphosis havoc" && ShouldUseMetamorphosis()) return 1.0f;
    if (actionName == "the hunt havoc" && ShouldUseTheHunt()) return 0.95f;
    if (actionName == "elysian decree havoc" && ShouldUseElysianDecree()) return 0.9f;
    
    // Core rotation priority
    if (actionName == "eye beam" && ShouldUseEyeBeam()) return 0.85f;
    if (actionName == "essence break" && ShouldUseEssenceBreak()) return 0.8f;
    if (actionName == "death sweep" && ShouldUseDeathSweep()) return 0.75f;
    if (actionName == "annihilation" && ShouldUseAnnihilation()) return 0.7f;
    if (actionName == "blade dance" && ShouldUseBladeDance()) return 0.65f;
    if (actionName == "chaos strike" && ShouldUseChaosStrike()) return 0.6f;
    
    // AoE abilities
    if (actionName == "glaive tempest" && ShouldUseGlaiveTempest()) return 0.55f;
    if (actionName == "fel barrage" && ShouldUseFelBarrage()) return 0.5f;
    
    // Buff maintenance
    if (actionName == "immolation aura havoc" && ShouldUseImmolationAura()) return 0.45f;
    
    // Fury generation
    if (actionName == "demons bite" && ShouldUseDemonsBite()) return 0.3f;
    
    return 0.0f;
}

bool PlayerbotHavocDemonHunterStrategy::ShouldUseDemonsBite() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !IsInMeleeRange(target))
        return false;
    
    return ShouldBuildFury() && !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::DEMONS_BITE);
}

bool PlayerbotHavocDemonHunterStrategy::ShouldUseChaosStrike() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !IsInMeleeRange(target) || HasMetamorphosis())
        return false;
    
    return HasEnoughFury(40) && !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::CHAOS_STRIKE);
}

bool PlayerbotHavocDemonHunterStrategy::ShouldUseAnnihilation() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !IsInMeleeRange(target) || !HasMetamorphosis())
        return false;
    
    return HasEnoughFury(40) && !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::ANNIHILATION);
}

bool PlayerbotHavocDemonHunterStrategy::ShouldUseBladeDance() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot || HasMetamorphosis())
        return false;
    
    return HasEnoughFury(35) && !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::BLADE_DANCE);
}

bool PlayerbotHavocDemonHunterStrategy::ShouldUseDeathSweep() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot || !HasMetamorphosis())
        return false;
    
    return HasEnoughFury(35) && !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::DEATH_SWEEP);
}

bool PlayerbotHavocDemonHunterStrategy::ShouldUseEyeBeam() const
{
    Player* bot = _ai->GetBot();
    Unit* target = FindBestTarget();
    
    if (!target || !bot)
        return false;
    
    return HasEnoughFury(30) && bot->GetDistance(target) <= 20.0f &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::EYE_BEAM);
}

bool PlayerbotHavocDemonHunterStrategy::ShouldUseImmolationAura() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot || HasImmolationAura())
        return false;
    
    return HasEnoughFury(20) && IsInCombat() &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::IMMOLATION_AURA);
}

bool PlayerbotHavocDemonHunterStrategy::ShouldUseMetamorphosis() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot || HasMetamorphosis())
        return false;
    
    return ShouldUseBurstCooldowns() && 
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::METAMORPHOSIS_HAVOC);
}

bool PlayerbotHavocDemonHunterStrategy::ShouldUseEssenceBreak() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !IsInMeleeRange(target))
        return false;
    
    return HasEnoughFury(30) && ShouldUseBurstCooldowns() &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::ESSENCE_BREAK);
}

bool PlayerbotHavocDemonHunterStrategy::ShouldUseTheHunt() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return ShouldUseBurstCooldowns() && 
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::THE_HUNT);
}

bool PlayerbotHavocDemonHunterStrategy::ShouldUseGlaiveTempest() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return ShouldUseAoE() && HasEnoughFury(30) &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::GLAIVE_TEMPEST);
}

bool PlayerbotHavocDemonHunterStrategy::ShouldUseElysianDecree() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return ShouldUseBurstCooldowns() && 
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::ELYSIAN_DECREE);
}

bool PlayerbotHavocDemonHunterStrategy::ShouldUseFelBarrage() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return ShouldUseAoE() && HasEnoughFury(25) &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::FEL_BARRAGE);
}

bool PlayerbotHavocDemonHunterStrategy::ShouldBuildFury() const
{
    return GetFuryAmount() < OPTIMAL_FURY_COUNT;
}

bool PlayerbotHavocDemonHunterStrategy::ShouldSpendFury() const
{
    return GetFuryAmount() >= 40;
}

bool PlayerbotHavocDemonHunterStrategy::HasOptimalFuryCount() const
{
    uint32 fury = GetFuryAmount();
    return fury >= 40 && fury <= OPTIMAL_FURY_COUNT;
}

bool PlayerbotHavocDemonHunterStrategy::IsAtMaxFury() const
{
    return GetFuryAmount() >= MAX_FURY_COUNT;
}

bool PlayerbotHavocDemonHunterStrategy::ShouldUseAoERotation() const
{
    return CountNearbyEnemies() >= AOE_ROTATION_COUNT;
}

bool PlayerbotHavocDemonHunterStrategy::ShouldUseSingleTargetRotation() const
{
    return CountNearbyEnemies() < AOE_ROTATION_COUNT;
}

bool PlayerbotHavocDemonHunterStrategy::IsInMetamorphosis() const
{
    return HasMetamorphosis();
}

bool PlayerbotHavocDemonHunterStrategy::ShouldUseEssenceBreakWindow() const
{
    Unit* target = FindBestTarget();
    return target && target->HasAura(DemonHunterSpells::ESSENCE_BREAK);
}

bool PlayerbotHavocDemonHunterStrategy::ShouldUseMajorCooldowns() const
{
    return ShouldUseBurstCooldowns();
}

bool PlayerbotHavocDemonHunterStrategy::ShouldGenerateMomentum() const
{
    return !HasMomentum() && IsInCombat();
}

bool PlayerbotHavocDemonHunterStrategy::IsOptimallyPositioned() const
{
    Unit* target = FindBestTarget();
    return target && IsInMeleeRange(target);
}

bool PlayerbotHavocDemonHunterStrategy::ShouldUseMobilityForDamage() const
{
    return HasMomentum() || ShouldGenerateMomentum();
}

bool PlayerbotHavocDemonHunterStrategy::ShouldUseMobilityForSurvival() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetHealthPct() < 50.0f;
}

bool PlayerbotHavocDemonHunterStrategy::ShouldMaintainPrepared() const
{
    return !HasPrepared() && IsInCombat();
}

bool PlayerbotHavocDemonHunterStrategy::ShouldActivateMomentum() const
{
    return ShouldGenerateMomentum();
}

bool PlayerbotHavocDemonHunterStrategy::HasDemonSoulBuff() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DemonHunterSpells::DEMON_SOUL);
}

bool PlayerbotHavocDemonHunterStrategy::HasTacticalAdvantage() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DemonHunterSpells::TACTICAL_ADVANTAGE);
}

bool PlayerbotHavocDemonHunterStrategy::ShouldPrioritizeChaosStrike() const
{
    return ShouldUseSingleTargetRotation() && !HasMetamorphosis() && HasOptimalFuryCount();
}

bool PlayerbotHavocDemonHunterStrategy::ShouldPrioritizeBladeDance() const
{
    return ShouldUseAoERotation() && !HasMetamorphosis() && HasOptimalFuryCount();
}

bool PlayerbotHavocDemonHunterStrategy::ShouldPrioritizeEyeBeam() const
{
    Player* bot = _ai->GetBot();
    return bot && !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::EYE_BEAM);
}

bool PlayerbotHavocDemonHunterStrategy::ShouldPrioritizeEssenceBreak() const
{
    return ShouldUseBurstCooldowns() && !ShouldUseEssenceBreakWindow();
}

// Vengeance Demon Hunter Strategy Implementation
PlayerbotVengeanceDemonHunterStrategy::PlayerbotVengeanceDemonHunterStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotDemonHunterStrategy(ai, "vengeance")
{
}

void PlayerbotVengeanceDemonHunterStrategy::InitializeActions()
{
    PlayerbotDemonHunterStrategy::InitializeActions();
    RegisterVengeanceActions();
}

void PlayerbotVengeanceDemonHunterStrategy::InitializeTriggers()
{
    PlayerbotDemonHunterStrategy::InitializeTriggers();
    RegisterVengeanceTriggers();
}

void PlayerbotVengeanceDemonHunterStrategy::RegisterVengeanceActions()
{
    RegisterAction("demon spikes", std::make_unique<CastDemonSpikes>(_ai));
    RegisterAction("immolation aura tank", std::make_unique<CastImmolationAuraTank>(_ai));
    RegisterAction("infernal strike", std::make_unique<CastInfernalStrike>(_ai));
    RegisterAction("shear", std::make_unique<CastShear>(_ai));
    RegisterAction("fracture", std::make_unique<CastFracture>(_ai));
    RegisterAction("soul cleave", std::make_unique<CastSoulCleave>(_ai));
    RegisterAction("spirit bomb", std::make_unique<CastSpiritBomb>(_ai));
    RegisterAction("sigil of flame", std::make_unique<CastSigilOfFlame>(_ai));
    RegisterAction("sigil of silence", std::make_unique<CastSigilOfSilence>(_ai));
    RegisterAction("sigil of misery", std::make_unique<CastSigilOfMisery>(_ai));
    RegisterAction("sigil of chains", std::make_unique<CastSigilOfChains>(_ai));
    RegisterAction("fiery brand", std::make_unique<CastFieryBrand>(_ai));
    RegisterAction("metamorphosis tank", std::make_unique<CastMetamorphosisTank>(_ai));
    RegisterAction("fel devastation", std::make_unique<CastFelDevastation>(_ai));
    RegisterAction("soul barrier", std::make_unique<CastSoulBarrier>(_ai));
    RegisterAction("last resort", std::make_unique<CastLastResort>(_ai));
    RegisterAction("bulk extraction", std::make_unique<CastBulkExtraction>(_ai));
    RegisterAction("the hunt tank", std::make_unique<CastTheHuntTank>(_ai));
    RegisterAction("elysian decree tank", std::make_unique<CastElysianDecreeTank>(_ai));
}

void PlayerbotVengeanceDemonHunterStrategy::RegisterVengeanceTriggers()
{
    // Triggers will be implemented when trigger system is ready
}

float PlayerbotVengeanceDemonHunterStrategy::GetActionPriority(std::string const& actionName) const
{
    // Get base priorities
    float basePriority = PlayerbotDemonHunterStrategy::GetActionPriority(actionName);
    if (basePriority > 0.0f)
        return basePriority;
    
    // Emergency abilities (highest priority)
    if (actionName == "last resort" && ShouldUseLastResort()) return 1.0f;
    if (actionName == "soul barrier" && ShouldUseSoulBarrier()) return 0.95f;
    if (actionName == "metamorphosis tank" && ShouldUseMetamorphosisTank()) return 0.9f;
    
    // Active mitigation
    if (actionName == "demon spikes" && ShouldUseDemonSpikes()) return 0.85f;
    if (actionName == "fiery brand" && ShouldUseFieryBrand()) return 0.8f;
    
    // Major cooldowns
    if (actionName == "fel devastation" && ShouldUseFelDevastation()) return 0.75f;
    if (actionName == "the hunt tank" && ShouldUseTheHuntTank()) return 0.7f;
    if (actionName == "elysian decree tank" && ShouldUseElysianDecreeTank()) return 0.65f;
    
    // Pain spenders (healing and threat)
    if (actionName == "spirit bomb" && ShouldUseSpiritBomb()) return 0.6f;
    if (actionName == "soul cleave" && ShouldUseSoulCleave()) return 0.55f;
    
    // Mobility and positioning
    if (actionName == "infernal strike" && ShouldUseInfernalStrike()) return 0.5f;
    
    // Sigils for crowd control and damage
    if (actionName == "sigil of flame" && ShouldUseSigilOfFlame()) return 0.45f;
    if (actionName == "sigil of silence" && ShouldUseSigilOfSilence()) return 0.4f;
    if (actionName == "sigil of misery" && ShouldUseSigilOfMisery()) return 0.35f;
    if (actionName == "sigil of chains" && ShouldUseSigilOfChains()) return 0.3f;
    
    // AoE abilities
    if (actionName == "bulk extraction" && ShouldUseBulkExtraction()) return 0.25f;
    
    // Pain generators and buff maintenance
    if (actionName == "fracture" && ShouldUseFracture()) return 0.4f;
    if (actionName == "immolation aura tank" && ShouldUseImmolationAuraTank()) return 0.35f;
    if (actionName == "shear" && ShouldUseShear()) return 0.2f;
    
    return 0.0f;
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseShear() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !IsInMeleeRange(target))
        return false;
    
    return ShouldBuildPain() && !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::SHEAR);
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseFracture() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !IsInMeleeRange(target))
        return false;
    
    return HasEnoughPain(25) && ShouldGenerateSoulFragments() &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::FRACTURE);
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseSoulCleave() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return HasEnoughPain(30) && (GetSoulFragments() >= 2 || bot->GetHealthPct() < 80.0f) &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::SOUL_CLEAVE);
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseSpiritBomb() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return HasEnoughPain(MIN_PAIN_FOR_SPIRIT_BOMB) && GetSoulFragments() >= OPTIMAL_SOUL_FRAGMENTS && 
           CountNearbyEnemies() >= 2 && !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::SPIRIT_BOMB);
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseDemonSpikes() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return IsInCombat() && NeedsDemonSpikes() && 
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::DEMON_SPIKES);
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseImmolationAuraTank() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot || HasImmolationAura())
        return false;
    
    return HasEnoughPain(20) && IsInCombat() &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::IMMOLATION_AURA_TANK);
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseInfernalStrike() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return (ShouldUseInfernalStrikeForPositioning() || ShouldUseInfernalStrikeForThreat()) &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::INFERNAL_STRIKE_TANK);
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseSigilOfFlame() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return CountNearbyEnemies(8.0f) >= 1 && 
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::SIGIL_OF_FLAME);
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseSigilOfSilence() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return target->IsNonMeleeSpellCast(false) &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::SIGIL_OF_SILENCE);
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseSigilOfMisery() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return CountNearbyEnemies() >= 2 &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::SIGIL_OF_MISERY);
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseSigilOfChains() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return CountNearbyEnemies() >= 2 &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::SIGIL_OF_CHAINS);
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseFieryBrand() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return !HasFieryBrand(target) && (ShouldUseFieryBrandDefensively() || ShouldPrioritizeThreat()) &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::FIERY_BRAND);
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseMetamorphosisTank() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot || HasMetamorphosis())
        return false;
    
    return ShouldUseMajorDefensiveCooldowns() && 
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::METAMORPHOSIS_TANK);
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseFelDevastation() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return HasEnoughPain(50) && CountNearbyEnemies() >= 2 &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::FEL_DEVASTATION);
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseSoulBarrier() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return bot->GetHealthPct() < 60.0f && GetSoulFragments() >= 3 && IsInCombat() &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::SOUL_BARRIER);
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseLastResort() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return bot->GetHealthPct() < 20.0f && IsInCombat() &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::LAST_RESORT);
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseBulkExtraction() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return CountNearbyEnemies() >= AOE_THREAT_COUNT && HasEnoughPain(35) &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::BULK_EXTRACTION);
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseTheHuntTank() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return ShouldUseMajorDefensiveCooldowns() && 
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::THE_HUNT_TANK);
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseElysianDecreeTank() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return ShouldUseMajorDefensiveCooldowns() && 
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::ELYSIAN_DECREE_TANK);
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldPrioritizeThreat() const
{
    return NeedsThreat() || CountNearbyEnemies() >= 2;
}

bool PlayerbotVengeanceDemonHunterStrategy::HasAggroOnTarget(Unit* target) const
{
    Player* bot = _ai->GetBot();
    return target && bot && target->GetVictim() == bot;
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseAoEThreat() const
{
    return CountNearbyEnemies() >= AOE_THREAT_COUNT;
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseSingleTargetThreat() const
{
    return CountNearbyEnemies() < AOE_THREAT_COUNT;
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseActiveMitigation() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetHealthPct() < 80.0f && IsInCombat();
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseEmergencyMitigation() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetHealthPct() < LOW_HEALTH_THRESHOLD * 100.0f;
}

bool PlayerbotVengeanceDemonHunterStrategy::NeedsDemonSpikes() const
{
    return !HasDemonSpikes() && (ShouldUseActiveMitigation() || ShouldPrioritizeThreat());
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseFieryBrandDefensively() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetHealthPct() < 70.0f;
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseSigils() const
{
    return IsInCombat() && (CountNearbyEnemies() >= 2 || ShouldUseEmergencyMitigation());
}

bool PlayerbotVengeanceDemonHunterStrategy::HasOptimalSoulFragments() const
{
    return GetSoulFragments() >= 2;
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldPrioritizeSoulCleave() const
{
    Player* bot = _ai->GetBot();
    return bot && (bot->GetHealthPct() < 80.0f || CountNearbyEnemies() <= 2);
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldPrioritizeSpiritBomb() const
{
    return GetSoulFragments() >= OPTIMAL_SOUL_FRAGMENTS && CountNearbyEnemies() >= 2;
}

uint32 PlayerbotVengeanceDemonHunterStrategy::GetActiveSoulFragments() const
{
    return GetSoulFragments();
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldGenerateSoulFragments() const
{
    return GetSoulFragments() < 2 || ShouldPrioritizeSpiritBomb();
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldBuildPain() const
{
    return GetPainAmount() < OPTIMAL_PAIN_COUNT;
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldSpendPain() const
{
    return GetPainAmount() >= 30;
}

bool PlayerbotVengeanceDemonHunterStrategy::HasOptimalPainCount() const
{
    uint32 pain = GetPainAmount();
    return pain >= 30 && pain <= OPTIMAL_PAIN_COUNT;
}

bool PlayerbotVengeanceDemonHunterStrategy::IsAtMaxPain() const
{
    return GetPainAmount() >= MAX_PAIN_COUNT;
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseInfernalStrikeForPositioning() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    return target && bot && bot->GetDistance(target) > 10.0f && bot->GetDistance(target) <= 30.0f;
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseInfernalStrikeForThreat() const
{
    return NeedsThreat() && ShouldUseSingleTargetThreat();
}

bool PlayerbotVengeanceDemonHunterStrategy::IsOptimallyPositionedForTank() const
{
    Unit* target = FindBestTarget();
    return target && IsInMeleeRange(target) && HasAggroOnTarget(target);
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseMajorDefensiveCooldowns() const
{
    Player* bot = _ai->GetBot();
    return bot && (bot->GetHealthPct() < 50.0f || CountNearbyEnemies() >= 3) && IsInCombat();
}

bool PlayerbotVengeanceDemonHunterStrategy::ShouldUseMinorDefensiveCooldowns() const
{
    return ShouldUseActiveMitigation();
}

bool PlayerbotVengeanceDemonHunterStrategy::IsInEmergencyMode() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetHealthPct() < LOW_HEALTH_THRESHOLD * 100.0f;
}