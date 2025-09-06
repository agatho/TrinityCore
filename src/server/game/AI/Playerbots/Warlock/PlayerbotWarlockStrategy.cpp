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

#include "PlayerbotWarlockStrategy.h"
#include "PlayerbotWarlockActions.h"
#include "PlayerbotPlayerAI.h"
#include "Player.h"
#include "SpellAuraEffects.h"
#include "SpellHistory.h"
#include "Group.h"

PlayerbotWarlockStrategy::PlayerbotWarlockStrategy(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotStrategy(ai, name) {}

void PlayerbotWarlockStrategy::InitializeActions()
{
    RegisterCommonActions();
}

void PlayerbotWarlockStrategy::InitializeTriggers()
{
    RegisterCommonTriggers();
}

float PlayerbotWarlockStrategy::GetActionPriority(std::string const& actionName) const
{
    // Emergency actions
    if (actionName == "unending resolve" && GetManaPercent() < 15) return 1.0f;
    if (actionName == "fear" && ShouldUseFear()) return 0.95f;
    
    // Pet management
    if (actionName == "summon imp" && ShouldSummonPet()) return 0.9f;
    
    // Utility
    if (actionName == "create healthstone" && ShouldCreateHealthstone()) return 0.85f;
    
    // Basic filler
    if (actionName == "shadow bolt") return 0.4f;
    
    return 0.3f;
}

void PlayerbotWarlockStrategy::RegisterCommonActions()
{
    RegisterAction("shadow bolt", std::make_unique<CastShadowBolt>(_ai));
    RegisterAction("drain life", std::make_unique<CastDrainLife>(_ai));
    RegisterAction("drain soul", std::make_unique<CastDrainSoul>(_ai));
    RegisterAction("fear", std::make_unique<CastFear>(_ai));
    RegisterAction("banish", std::make_unique<CastBanish>(_ai));
    RegisterAction("unending resolve", std::make_unique<CastUnendingResolve>(_ai));
    RegisterAction("dark pact", std::make_unique<CastDarkPact>(_ai));
    RegisterAction("summon imp", std::make_unique<CastSummonImp>(_ai));
    RegisterAction("summon voidwalker", std::make_unique<CastSummonVoidwalker>(_ai));
    RegisterAction("summon felhunter", std::make_unique<CastSummonFelhunter>(_ai));
    RegisterAction("summon succubus", std::make_unique<CastSummonSuccubus>(_ai));
    RegisterAction("create healthstone", std::make_unique<CastCreateHealthstone>(_ai));
    RegisterAction("create soulstone", std::make_unique<CastCreateSoulstone>(_ai));
    RegisterAction("demonic gateway", std::make_unique<CastDemonicGateway>(_ai));
}

void PlayerbotWarlockStrategy::RegisterCommonTriggers()
{
    // TODO: Implement when trigger system is available
}

bool PlayerbotWarlockStrategy::ShouldUseFear() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !bot->IsInCombat())
        return false;
        
    Unit* target = FindBestTarget();
    return target && bot->GetDistance(target) < 8.0f && bot->GetHealthPct() < 50.0f;
}

bool PlayerbotWarlockStrategy::ShouldUseBanish() const
{
    Unit* target = FindBestTarget();
    return target && target->GetCreatureType() == CREATURE_TYPE_ELEMENTAL;
}

bool PlayerbotWarlockStrategy::ShouldUseUnendingResolve() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetHealthPct() < 30.0f;
}

bool PlayerbotWarlockStrategy::ShouldUseDarkPact() const
{
    return GetManaPercent() < 20 && HasActivePet();
}

bool PlayerbotWarlockStrategy::ShouldSummonPet() const
{
    return !HasActivePet() && !IsInCombat();
}

bool PlayerbotWarlockStrategy::NeedsHealthstone() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetHealthPct() < 50.0f; // Simplified healthstone check
}

bool PlayerbotWarlockStrategy::ShouldCreateHealthstone() const
{
    Player* bot = _ai->GetBot();
    return bot && !IsInCombat(); // Simplified - should check inventory
}

uint32 PlayerbotWarlockStrategy::GetManaAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_MANA) : 0;
}

uint32 PlayerbotWarlockStrategy::GetManaPercent() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 maxMana = bot->GetMaxPower(POWER_MANA);
    return maxMana > 0 ? (GetManaAmount() * 100) / maxMana : 0;
}

uint32 PlayerbotWarlockStrategy::GetSoulShards() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_SOUL_SHARDS) : 0;
}

bool PlayerbotWarlockStrategy::ShouldConserveMana() const
{
    return GetManaPercent() < 25;
}

bool PlayerbotWarlockStrategy::HasEnoughSoulShards(uint32 cost) const
{
    return GetSoulShards() >= cost;
}

bool PlayerbotWarlockStrategy::isAffliction() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(WarlockSpells::MALEFIC_RAPTURE);
}

bool PlayerbotWarlockStrategy::isDemonology() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(WarlockSpells::HAND_OF_GULDAN);
}

bool PlayerbotWarlockStrategy::isDestruction() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(WarlockSpells::CHAOS_BOLT);
}

Unit* PlayerbotWarlockStrategy::FindBestTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;
    
    Unit* currentTarget = bot->GetSelectedUnit();
    if (currentTarget && currentTarget->IsAlive() && bot->IsWithinLOSInMap(currentTarget))
        return currentTarget;
        
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot, 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, check);
    Cell::VisitAllObjects(bot, searcher, 30.0f);
    
    for (Unit* target : targets)
    {
        if (target->IsAlive() && bot->IsWithinLOSInMap(target))
            return target;
    }
    
    return nullptr;
}

bool PlayerbotWarlockStrategy::IsInCombat() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->IsInCombat();
}

uint32 PlayerbotWarlockStrategy::CountNearbyEnemies(float range) const
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

bool PlayerbotWarlockStrategy::ShouldUseAoE() const
{
    return CountNearbyEnemies(8.0f) >= 3;
}

bool PlayerbotWarlockStrategy::HasActivePet() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetPet() && bot->GetPet()->IsAlive();
}

Unit* PlayerbotWarlockStrategy::GetPet() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPet() : nullptr;
}

bool PlayerbotWarlockStrategy::IsPetInCombat() const
{
    Unit* pet = GetPet();
    return pet && pet->IsInCombat();
}

bool PlayerbotWarlockStrategy::ShouldDismissPet() const
{
    return HasActivePet() && !IsInCombat(); // Simplified logic
}

bool PlayerbotWarlockStrategy::HasCorruption(Unit* target) const
{
    return target && target->HasAura(WarlockSpells::CORRUPTION);
}

bool PlayerbotWarlockStrategy::HasAgony(Unit* target) const
{
    return target && target->HasAura(WarlockSpells::AGONY);
}

bool PlayerbotWarlockStrategy::HasImmolate(Unit* target) const
{
    return target && target->HasAura(WarlockSpells::IMMOLATE);
}

bool PlayerbotWarlockStrategy::ShouldRefreshDoT(Unit* target, uint32 spellId, float threshold) const
{
    if (!target)
        return false;
        
    if (Aura* aura = target->GetAura(spellId))
        return float(aura->GetDuration()) / 1000.0f < threshold;
        
    return true;
}

float PlayerbotWarlockStrategy::GetDoTDuration(Unit* target, uint32 spellId) const
{
    if (!target)
        return 0.0f;
        
    if (Aura* aura = target->GetAura(spellId))
        return float(aura->GetDuration()) / 1000.0f;
        
    return 0.0f;
}

// ===== AFFLICTION WARLOCK STRATEGY =====

PlayerbotAfflictionWarlockStrategy::PlayerbotAfflictionWarlockStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotWarlockStrategy(ai, "affliction") {}

void PlayerbotAfflictionWarlockStrategy::InitializeActions()
{
    PlayerbotWarlockStrategy::InitializeActions();
    RegisterAfflictionActions();
}

void PlayerbotAfflictionWarlockStrategy::InitializeTriggers()
{
    PlayerbotWarlockStrategy::InitializeTriggers();
    RegisterAfflictionTriggers();
}

void PlayerbotAfflictionWarlockStrategy::RegisterAfflictionActions()
{
    RegisterAction("corruption", std::make_unique<CastCorruption>(_ai));
    RegisterAction("agony", std::make_unique<CastAgony>(_ai));
    RegisterAction("unstable affliction", std::make_unique<CastUnstableAffliction>(_ai));
    RegisterAction("malefic rapture", std::make_unique<CastMaleficRapture>(_ai));
    RegisterAction("seed of corruption", std::make_unique<CastSeedOfCorruption>(_ai));
    RegisterAction("vile taint", std::make_unique<CastVileTaint>(_ai));
    RegisterAction("phantom singularity", std::make_unique<CastPhantomSingularity>(_ai));
    RegisterAction("dark soul misery", std::make_unique<CastDarkSoulMisery>(_ai));
    RegisterAction("soul rot", std::make_unique<CastSoulRot>(_ai));
    RegisterAction("summon darkglare", std::make_unique<CastSummonDarkglare>(_ai));
    RegisterAction("haunt", std::make_unique<CastHaunt>(_ai));
    RegisterAction("drain soul", std::make_unique<CastDrainSoul>(_ai));
}

void PlayerbotAfflictionWarlockStrategy::RegisterAfflictionTriggers()
{
    // TODO: Implement when trigger system is available
}

float PlayerbotAfflictionWarlockStrategy::GetActionPriority(std::string const& actionName) const
{
    Unit* target = FindBestTarget();
    uint32 soulShards = GetSoulShards();
    bool shouldUseAoE = ShouldUseAoERotation();
    
    // Emergency and utility (highest priority)
    if (actionName == "unending resolve" && ShouldUseUnendingResolve()) return 1.0f;
    if (actionName == "fear" && ShouldUseFear()) return 0.95f;
    
    // Burst cooldowns
    if (actionName == "dark soul misery" && ShouldUseDarkSoulMisery()) return 0.9f;
    if (actionName == "summon darkglare" && ShouldUseSummonDarkglare()) return 0.85f;
    if (actionName == "soul rot" && ShouldUseSoulRot()) return 0.8f;
    
    // DoT application and maintenance (core rotation)
    if (target)
    {
        if (actionName == "agony" && ShouldRefreshAgony(target)) return 0.75f;
        if (actionName == "corruption" && ShouldRefreshCorruption(target)) return 0.7f;
        if (actionName == "unstable affliction" && ShouldUseUnstableAffliction()) return 0.65f;
        if (actionName == "haunt" && ShouldUseHaunt()) return 0.6f;
    }
    
    // Soul shard spenders
    if (actionName == "malefic rapture" && ShouldUseMaleficRapture()) return 0.55f;
    
    // AoE rotation
    if (shouldUseAoE)
    {
        if (actionName == "seed of corruption" && ShouldUseSeedOfCorruption()) return 0.5f;
        if (actionName == "vile taint" && ShouldUseVileTaint()) return 0.45f;
        if (actionName == "phantom singularity" && ShouldUsePhantomSingularity()) return 0.4f;
    }
    
    // Fillers and resource generation
    if (actionName == "drain soul" && ShouldUseDrainSoul()) return 0.35f;
    if (actionName == "shadow bolt") return 0.3f;
    
    return PlayerbotWarlockStrategy::GetActionPriority(actionName);
}

bool PlayerbotAfflictionWarlockStrategy::ShouldUseCorruption() const
{
    Unit* target = FindBestTarget();
    return target && ShouldRefreshCorruption(target);
}

bool PlayerbotAfflictionWarlockStrategy::ShouldUseAgony() const
{
    Unit* target = FindBestTarget();
    return target && ShouldRefreshAgony(target);
}

bool PlayerbotAfflictionWarlockStrategy::ShouldUseUnstableAffliction() const
{
    Unit* target = FindBestTarget();
    return target && HasEnoughSoulShards(1) && 
           ShouldRefreshDoT(target, WarlockSpells::UNSTABLE_AFFLICTION, DOT_REFRESH_THRESHOLD);
}

bool PlayerbotAfflictionWarlockStrategy::ShouldUseMaleficRapture() const
{
    return GetSoulShards() >= MALEFIC_RAPTURE_THRESHOLD && GetActiveDoTCount() >= MIN_DOTS_FOR_RAPTURE;
}

bool PlayerbotAfflictionWarlockStrategy::ShouldUseSeedOfCorruption() const
{
    return ShouldUseAoERotation() && HasEnoughSoulShards(1);
}

bool PlayerbotAfflictionWarlockStrategy::ShouldUseVileTaint() const
{
    Player* bot = _ai->GetBot();
    return bot && ShouldUseAoERotation() && HasEnoughSoulShards(1) &&
           !bot->GetSpellHistory()->HasCooldown(WarlockSpells::VILE_TAINT);
}

bool PlayerbotAfflictionWarlockStrategy::ShouldUsePhantomSingularity() const
{
    Player* bot = _ai->GetBot();
    return bot && ShouldUseAoERotation() &&
           !bot->GetSpellHistory()->HasCooldown(WarlockSpells::PHANTOM_SINGULARITY);
}

bool PlayerbotAfflictionWarlockStrategy::ShouldUseDarkSoulMisery() const
{
    Player* bot = _ai->GetBot();
    return bot && ShouldUseBurstCooldowns() &&
           !bot->GetSpellHistory()->HasCooldown(WarlockSpells::DARK_SOUL_MISERY);
}

bool PlayerbotAfflictionWarlockStrategy::ShouldUseSoulRot() const
{
    Player* bot = _ai->GetBot();
    return bot && ShouldUseBurstCooldowns() &&
           !bot->GetSpellHistory()->HasCooldown(WarlockSpells::SOUL_ROT);
}

bool PlayerbotAfflictionWarlockStrategy::ShouldUseSummonDarkglare() const
{
    Player* bot = _ai->GetBot();
    return bot && ShouldUseBurstCooldowns() && GetActiveDoTCount() >= 3 &&
           !bot->GetSpellHistory()->HasCooldown(WarlockSpells::SUMMON_DARKGLARE);
}

bool PlayerbotAfflictionWarlockStrategy::ShouldUseHaunt() const
{
    Player* bot = _ai->GetBot();
    Unit* target = FindBestTarget();
    return bot && target && HasEnoughSoulShards(1) && 
           !bot->GetSpellHistory()->HasCooldown(WarlockSpells::HAUNT);
}

bool PlayerbotAfflictionWarlockStrategy::ShouldUseDrainSoul() const
{
    Unit* target = FindBestTarget();
    return target && target->GetHealthPct() < 25.0f; // Execute phase
}

bool PlayerbotAfflictionWarlockStrategy::HasMaxDoTsOnTarget(Unit* target) const
{
    return target && HasCorruption(target) && HasAgony(target) &&
           target->HasAura(WarlockSpells::UNSTABLE_AFFLICTION);
}

uint32 PlayerbotAfflictionWarlockStrategy::GetActiveDoTCount() const
{
    // Simplified - count DoTs on current target
    Unit* target = FindBestTarget();
    if (!target)
        return 0;
        
    uint32 count = 0;
    if (HasCorruption(target)) count++;
    if (HasAgony(target)) count++;
    if (target->HasAura(WarlockSpells::UNSTABLE_AFFLICTION)) count++;
    
    return count;
}

bool PlayerbotAfflictionWarlockStrategy::ShouldUseAoERotation() const
{
    return CountNearbyEnemies(8.0f) >= AOE_TARGET_COUNT;
}

bool PlayerbotAfflictionWarlockStrategy::ShouldUseBurstCooldowns() const
{
    Unit* target = FindBestTarget();
    return target && target->GetHealthPct() > BURST_COOLDOWN_HEALTH;
}

bool PlayerbotAfflictionWarlockStrategy::ShouldPrioritizeDoTRefresh() const
{
    Unit* target = FindBestTarget();
    return target && (ShouldRefreshCorruption(target) || ShouldRefreshAgony(target));
}

bool PlayerbotAfflictionWarlockStrategy::ShouldSpendSoulShards() const
{
    return GetSoulShards() >= MALEFIC_RAPTURE_THRESHOLD || IsAtMaxSoulShards();
}

bool PlayerbotAfflictionWarlockStrategy::ShouldRefreshCorruption(Unit* target) const
{
    return ShouldRefreshDoT(target, WarlockSpells::CORRUPTION, DOT_REFRESH_THRESHOLD);
}

bool PlayerbotAfflictionWarlockStrategy::ShouldRefreshAgony(Unit* target) const
{
    return ShouldRefreshDoT(target, WarlockSpells::AGONY, DOT_REFRESH_THRESHOLD);
}

bool PlayerbotAfflictionWarlockStrategy::ShouldRefreshUnstableAffliction(Unit* target) const
{
    return ShouldRefreshDoT(target, WarlockSpells::UNSTABLE_AFFLICTION, DOT_REFRESH_THRESHOLD);
}

Unit* PlayerbotAfflictionWarlockStrategy::FindBestDoTTarget() const
{
    return FindBestTarget(); // Simplified
}

uint32 PlayerbotAfflictionWarlockStrategy::GetDoTStacksOnTarget(Unit* target, uint32 spellId) const
{
    if (!target)
        return 0;
        
    if (Aura* aura = target->GetAura(spellId))
        return aura->GetStackAmount();
        
    return 0;
}

bool PlayerbotAfflictionWarlockStrategy::ShouldConserveSoulShards() const
{
    return GetSoulShards() < 2;
}

bool PlayerbotAfflictionWarlockStrategy::CanAffordSoulShardSpell(uint32 cost) const
{
    return GetSoulShards() >= cost;
}

bool PlayerbotAfflictionWarlockStrategy::IsAtMaxSoulShards() const
{
    return GetSoulShards() >= MAX_SOUL_SHARDS;
}

// ===== DEMONOLOGY WARLOCK STRATEGY =====

PlayerbotDemonologyWarlockStrategy::PlayerbotDemonologyWarlockStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotWarlockStrategy(ai, "demonology") {}

void PlayerbotDemonologyWarlockStrategy::InitializeActions()
{
    PlayerbotWarlockStrategy::InitializeActions();
    RegisterDemonologyActions();
}

void PlayerbotDemonologyWarlockStrategy::InitializeTriggers()
{
    PlayerbotWarlockStrategy::InitializeTriggers();
    RegisterDemonologyTriggers();
}

void PlayerbotDemonologyWarlockStrategy::RegisterDemonologyActions()
{
    RegisterAction("demonbolt", std::make_unique<CastDemonbolt>(_ai));
    RegisterAction("hand of guldan", std::make_unique<CastHandOfGuldan>(_ai));
    RegisterAction("call dreadstalkers", std::make_unique<CastCallDreadstalkers>(_ai));
    RegisterAction("summon vilefiend", std::make_unique<CastSummonVilefiend>(_ai));
    RegisterAction("summon demonic tyrant", std::make_unique<CastSummonDemonicTyrant>(_ai));
    RegisterAction("demonic strength", std::make_unique<CastDemonicStrength>(_ai));
    RegisterAction("bilescourge bombers", std::make_unique<CastBilescourgeBombers>(_ai));
    RegisterAction("power siphon", std::make_unique<CastPowerSiphon>(_ai));
    RegisterAction("doom", std::make_unique<CastDoom>(_ai));
    RegisterAction("nether portal", std::make_unique<CastNetherPortal>(_ai));
    RegisterAction("grimoire felguard", std::make_unique<CastGrimoireFelguard>(_ai));
}

void PlayerbotDemonologyWarlockStrategy::RegisterDemonologyTriggers()
{
    // TODO: Implement when trigger system is available
}

float PlayerbotDemonologyWarlockStrategy::GetActionPriority(std::string const& actionName) const
{
    uint32 soulShards = GetSoulShards();
    bool shouldUseTyrant = ShouldUseTyrantSetup();
    bool inTyrantWindow = IsInTyrantWindow();
    
    // Emergency actions
    if (actionName == "unending resolve" && ShouldUseUnendingResolve()) return 1.0f;
    
    // Tyrant setup and execution (highest DPS priority)
    if (actionName == "summon demonic tyrant" && ShouldUseSummonDemonicTyrant()) return 0.95f;
    
    // Big demon summoning for Tyrant setup
    if (shouldUseTyrant)
    {
        if (actionName == "call dreadstalkers" && ShouldUseCallDreadstalkers()) return 0.9f;
        if (actionName == "summon vilefiend" && ShouldUseSummonVilefiend()) return 0.85f;
        if (actionName == "grimoire felguard" && ShouldUseGrimoireFelguard()) return 0.8f;
        if (actionName == "hand of guldan" && soulShards >= 2) return 0.75f;
    }
    
    // Burst cooldowns
    if (actionName == "nether portal" && ShouldUseNetherPortal()) return 0.7f;
    if (actionName == "bilescourge bombers" && ShouldUseBilescourgeBombers()) return 0.65f;
    
    // Core rotation
    if (actionName == "demonbolt" && HasDemonicCoreProc()) return 0.6f;
    if (actionName == "hand of guldan" && ShouldUseHandOfGuldan()) return 0.55f;
    if (actionName == "demonic strength" && ShouldUseDemonicStrength()) return 0.5f;
    
    // Utilities
    if (actionName == "power siphon" && ShouldUsePowerSiphon()) return 0.45f;
    if (actionName == "doom" && ShouldUseDoom()) return 0.4f;
    
    // Fillers
    if (actionName == "demonbolt") return 0.35f;
    if (actionName == "shadow bolt") return 0.3f;
    
    return PlayerbotWarlockStrategy::GetActionPriority(actionName);
}

bool PlayerbotDemonologyWarlockStrategy::ShouldUseShadowBolt() const
{
    return !HasDemonicCoreProc() && ShouldBuildSoulShards();
}

bool PlayerbotDemonologyWarlockStrategy::ShouldUseDemonbolt() const
{
    return HasDemonicCoreProc() || !ShouldBuildSoulShards();
}

bool PlayerbotDemonologyWarlockStrategy::ShouldUseHandOfGuldan() const
{
    return GetSoulShards() >= 2 && (ShouldUseTyrantSetup() || GetActiveWildImpCount() < OPTIMAL_IMP_COUNT);
}

bool PlayerbotDemonologyWarlockStrategy::ShouldUseCallDreadstalkers() const
{
    Player* bot = _ai->GetBot();
    return bot && HasEnoughSoulShards(2) && 
           !bot->GetSpellHistory()->HasCooldown(WarlockSpells::CALL_DREADSTALKERS);
}

bool PlayerbotDemonologyWarlockStrategy::ShouldUseSummonVilefiend() const
{
    Player* bot = _ai->GetBot();
    return bot && HasEnoughSoulShards(1) && 
           !bot->GetSpellHistory()->HasCooldown(WarlockSpells::SUMMON_VILEFIEND);
}

bool PlayerbotDemonologyWarlockStrategy::ShouldUseSummonDemonicTyrant() const
{
    Player* bot = _ai->GetBot();
    return bot && CanSetupTyrant() && HasBigDemonsActive() &&
           !bot->GetSpellHistory()->HasCooldown(WarlockSpells::SUMMON_DEMONIC_TYRANT);
}

bool PlayerbotDemonologyWarlockStrategy::ShouldUseDemonicStrength() const
{
    Player* bot = _ai->GetBot();
    Unit* pet = GetPet();
    return bot && pet && !bot->GetSpellHistory()->HasCooldown(WarlockSpells::DEMONIC_STRENGTH);
}

bool PlayerbotDemonologyWarlockStrategy::ShouldUseBilescourgeBombers() const
{
    Player* bot = _ai->GetBot();
    return bot && ShouldUseBurstCooldowns() && CountNearbyEnemies(8.0f) >= 2 &&
           !bot->GetSpellHistory()->HasCooldown(WarlockSpells::BILESCOURGE_BOMBERS);
}

bool PlayerbotDemonologyWarlockStrategy::ShouldUsePowerSiphon() const
{
    Player* bot = _ai->GetBot();
    return bot && GetActiveWildImpCount() >= 2 && 
           !bot->GetSpellHistory()->HasCooldown(WarlockSpells::POWER_SIPHON);
}

bool PlayerbotDemonologyWarlockStrategy::ShouldUseDoom() const
{
    Unit* target = FindBestTarget();
    return target && !target->HasAura(WarlockSpells::DOOM) && target->GetHealthPct() > 50.0f;
}

bool PlayerbotDemonologyWarlockStrategy::ShouldUseNetherPortal() const
{
    Player* bot = _ai->GetBot();
    return bot && ShouldUseBurstCooldowns() && 
           !bot->GetSpellHistory()->HasCooldown(WarlockSpells::NETHER_PORTAL);
}

bool PlayerbotDemonologyWarlockStrategy::ShouldUseGrimoireFelguard() const
{
    Player* bot = _ai->GetBot();
    return bot && ShouldUseTyrantSetup() && 
           !bot->GetSpellHistory()->HasCooldown(WarlockSpells::GRIMOIRE_FELGUARD);
}

bool PlayerbotDemonologyWarlockStrategy::HasDemonicCoreProc() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(WarlockSpells::DEMONIC_CALLING);
}

uint32 PlayerbotDemonologyWarlockStrategy::GetActiveDemonCount() const
{
    // Simplified demon counting - in real implementation would track all summoned demons
    return GetActiveWildImpCount() + (HasActivePet() ? 1 : 0);
}

uint32 PlayerbotDemonologyWarlockStrategy::GetActiveWildImpCount() const
{
    // Simplified - would need to track wild imp summons in real implementation
    return 3; // Placeholder
}

bool PlayerbotDemonologyWarlockStrategy::ShouldUseTyrantSetup() const
{
    return GetSoulShards() >= TYRANT_SETUP_SHARDS && GetDemonicTyrantCooldownRemaining() < 10.0f;
}

bool PlayerbotDemonologyWarlockStrategy::IsInTyrantWindow() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(265187); // Demonic Tyrant buff (simplified check)
}

bool PlayerbotDemonologyWarlockStrategy::ShouldUseBurstCooldowns() const
{
    Unit* target = FindBestTarget();
    return target && target->GetHealthPct() > BURST_COOLDOWN_HEALTH;
}

bool PlayerbotDemonologyWarlockStrategy::ShouldUseAoERotation() const
{
    return CountNearbyEnemies(8.0f) >= AOE_TARGET_COUNT;
}

bool PlayerbotDemonologyWarlockStrategy::HasBigDemonsActive() const
{
    // Simplified - would check for Dreadstalkers, Vilefiend, etc.
    return HasActivePet(); // Placeholder
}

bool PlayerbotDemonologyWarlockStrategy::ShouldExtendDemons() const
{
    return HasBigDemonsActive() && GetActiveWildImpCount() >= 4;
}

float PlayerbotDemonologyWarlockStrategy::GetDemonicTyrantCooldownRemaining() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0.0f;
        
    if (bot->GetSpellHistory()->HasCooldown(WarlockSpells::SUMMON_DEMONIC_TYRANT))
        return float(bot->GetSpellHistory()->GetRemainingCooldown(WarlockSpells::SUMMON_DEMONIC_TYRANT)) / 1000.0f;
        
    return 0.0f;
}

bool PlayerbotDemonologyWarlockStrategy::CanSetupTyrant() const
{
    return GetSoulShards() >= 2 && GetDemonicTyrantCooldownRemaining() == 0.0f;
}

bool PlayerbotDemonologyWarlockStrategy::ShouldBuildSoulShards() const
{
    return GetSoulShards() < TYRANT_SETUP_SHARDS;
}

bool PlayerbotDemonologyWarlockStrategy::ShouldSpendSoulShardsOnImps() const
{
    return GetSoulShards() >= 3 && GetActiveWildImpCount() < OPTIMAL_IMP_COUNT;
}

bool PlayerbotDemonologyWarlockStrategy::HasOptimalShardCount() const
{
    return GetSoulShards() >= TYRANT_SETUP_SHARDS;
}

// ===== DESTRUCTION WARLOCK STRATEGY =====

PlayerbotDestructionWarlockStrategy::PlayerbotDestructionWarlockStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotWarlockStrategy(ai, "destruction") {}

void PlayerbotDestructionWarlockStrategy::InitializeActions()
{
    PlayerbotWarlockStrategy::InitializeActions();
    RegisterDestructionActions();
}

void PlayerbotDestructionWarlockStrategy::InitializeTriggers()
{
    PlayerbotWarlockStrategy::InitializeTriggers();
    RegisterDestructionTriggers();
}

void PlayerbotDestructionWarlockStrategy::RegisterDestructionActions()
{
    RegisterAction("incinerate", std::make_unique<CastIncinerate>(_ai));
    RegisterAction("conflagrate", std::make_unique<CastConflagrate>(_ai));
    RegisterAction("immolate", std::make_unique<CastImmolate>(_ai));
    RegisterAction("chaos bolt", std::make_unique<CastChaosBolt>(_ai));
    RegisterAction("rain of fire", std::make_unique<CastRainOfFire>(_ai));
    RegisterAction("havoc", std::make_unique<CastHavoc>(_ai));
    RegisterAction("cataclysm", std::make_unique<CastCataclysm>(_ai));
    RegisterAction("channel demonfire", std::make_unique<CastChannelDemonfire>(_ai));
    RegisterAction("soul fire", std::make_unique<CastSoulFire>(_ai));
    RegisterAction("shadowburn", std::make_unique<CastShadowburn>(_ai));
    RegisterAction("dark soul instability", std::make_unique<CastDarkSoulInstability>(_ai));
    RegisterAction("summon infernal", std::make_unique<CastSummonInfernal>(_ai));
}

void PlayerbotDestructionWarlockStrategy::RegisterDestructionTriggers()
{
    // TODO: Implement when trigger system is available
}

float PlayerbotDestructionWarlockStrategy::GetActionPriority(std::string const& actionName) const
{
    Unit* target = FindBestTarget();
    uint32 soulShards = GetSoulShards();
    bool hasBackdraft = HasBackdraftProc();
    
    // Emergency actions
    if (actionName == "unending resolve" && ShouldUseUnendingResolve()) return 1.0f;
    
    // Burst cooldowns
    if (actionName == "dark soul instability" && ShouldUseDarkSoulInstability()) return 0.95f;
    if (actionName == "summon infernal" && ShouldUseSummonInfernal()) return 0.9f;
    
    // DoT maintenance (high priority)
    if (target && actionName == "immolate" && ShouldRefreshImmolate(target)) return 0.85f;
    
    // Soul shard spenders (core damage)
    if (actionName == "chaos bolt" && ShouldUseChaosBolt()) return 0.8f;
    if (actionName == "rain of fire" && ShouldUseRainOfFire()) return 0.75f;
    
    // AoE and cleave
    if (ShouldUseAoERotation())
    {
        if (actionName == "cataclysm" && ShouldUseCataclysm()) return 0.7f;
        if (actionName == "havoc" && ShouldUseHavoc()) return 0.65f;
        if (actionName == "channel demonfire" && ShouldUseChannelDemonfire()) return 0.6f;
    }
    
    // Soul shard generators
    if (actionName == "conflagrate" && ShouldUseConflagrate()) return 0.55f;
    if (actionName == "incinerate" && hasBackdraft) return 0.5f; // Backdraft priority
    
    // Execute and movement abilities
    if (actionName == "shadowburn" && ShouldUseShadowburn()) return 0.45f;
    if (actionName == "soul fire" && ShouldUseSoulFire()) return 0.4f;
    
    // Standard filler
    if (actionName == "incinerate") return 0.35f;
    
    return PlayerbotWarlockStrategy::GetActionPriority(actionName);
}

bool PlayerbotDestructionWarlockStrategy::ShouldUseIncinerate() const
{
    return !ShouldConserveMana();
}

bool PlayerbotDestructionWarlockStrategy::ShouldUseConflagrate() const
{
    return HasConflagrateCharges();
}

bool PlayerbotDestructionWarlockStrategy::ShouldUseImmolate() const
{
    Unit* target = FindBestTarget();
    return target && ShouldRefreshImmolate(target);
}

bool PlayerbotDestructionWarlockStrategy::ShouldUseChaosBolt() const
{
    return ShouldSpendSoulShardsOnChaosBolt() && HasEnoughSoulShards(CHAOS_BOLT_SHARD_COST);
}

bool PlayerbotDestructionWarlockStrategy::ShouldUseRainOfFire() const
{
    return ShouldUseAoERotation() && HasEnoughSoulShards(1);
}

bool PlayerbotDestructionWarlockStrategy::ShouldUseHavoc() const
{
    Player* bot = _ai->GetBot();
    return bot && ShouldUseCleaveRotation() && ShouldUseHavocTarget() &&
           !bot->GetSpellHistory()->HasCooldown(WarlockSpells::HAVOC);
}

bool PlayerbotDestructionWarlockStrategy::ShouldUseCataclysm() const
{
    Player* bot = _ai->GetBot();
    return bot && ShouldUseAoERotation() && HasEnoughSoulShards(1) &&
           !bot->GetSpellHistory()->HasCooldown(WarlockSpells::CATACLYSM);
}

bool PlayerbotDestructionWarlockStrategy::ShouldUseChannelDemonfire() const
{
    Unit* target = FindBestTarget();
    return target && HasImmolate(target) && CountNearbyEnemies(8.0f) >= 2;
}

bool PlayerbotDestructionWarlockStrategy::ShouldUseSoulFire() const
{
    Player* bot = _ai->GetBot();
    return bot && !bot->HasUnitState(UNIT_STATE_CASTING); // Movement ability
}

bool PlayerbotDestructionWarlockStrategy::ShouldUseShadowburn() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    return bot && target && target->GetHealthPct() < 20.0f && HasEnoughSoulShards(1) &&
           !bot->GetSpellHistory()->HasCooldown(WarlockSpells::SHADOWBURN);
}

bool PlayerbotDestructionWarlockStrategy::ShouldUseDarkSoulInstability() const
{
    Player* bot = _ai->GetBot();
    return bot && ShouldUseBurstCooldowns() &&
           !bot->GetSpellHistory()->HasCooldown(WarlockSpells::DARK_SOUL_INSTABILITY);
}

bool PlayerbotDestructionWarlockStrategy::ShouldUseSummonInfernal() const
{
    Player* bot = _ai->GetBot();
    return bot && ShouldUseBurstCooldowns() &&
           !bot->GetSpellHistory()->HasCooldown(WarlockSpells::SUMMON_INFERNAL);
}

bool PlayerbotDestructionWarlockStrategy::HasBackdraftProc() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(WarlockSpells::BACKDRAFT);
}

uint32 PlayerbotDestructionWarlockStrategy::GetBackdraftStacks() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
        
    if (Aura* aura = bot->GetAura(WarlockSpells::BACKDRAFT))
        return aura->GetStackAmount();
        
    return 0;
}

bool PlayerbotDestructionWarlockStrategy::HasEradication() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(WarlockSpells::ERADICATION);
}

bool PlayerbotDestructionWarlockStrategy::ShouldUseBurstCooldowns() const
{
    Unit* target = FindBestTarget();
    return target && target->GetHealthPct() > BURST_COOLDOWN_HEALTH;
}

bool PlayerbotDestructionWarlockStrategy::ShouldUseAoERotation() const
{
    return CountNearbyEnemies(8.0f) >= AOE_TARGET_COUNT;
}

bool PlayerbotDestructionWarlockStrategy::ShouldUseCleaveRotation() const
{
    return CountNearbyEnemies(8.0f) >= CLEAVE_TARGET_COUNT;
}

bool PlayerbotDestructionWarlockStrategy::ShouldRefreshImmolate(Unit* target) const
{
    return ShouldRefreshDoT(target, WarlockSpells::IMMOLATE, IMMOLATE_REFRESH_THRESHOLD);
}

Unit* PlayerbotDestructionWarlockStrategy::FindBestImmolateTarget() const
{
    return FindBestTarget(); // Simplified
}

bool PlayerbotDestructionWarlockStrategy::ShouldUseHavocTarget() const
{
    return CountNearbyEnemies(8.0f) >= 2;
}

Unit* PlayerbotDestructionWarlockStrategy::FindBestHavocTarget() const
{
    // Find secondary target for Havoc - simplified implementation
    return FindBestTarget();
}

bool PlayerbotDestructionWarlockStrategy::ShouldSpendSoulShardsOnChaosBolt() const
{
    return GetSoulShards() >= OPTIMAL_SHARD_COUNT || IsAtOptimalShardCount();
}

bool PlayerbotDestructionWarlockStrategy::ShouldConserveSoulShardsForBurst() const
{
    return GetSoulShards() < CHAOS_BOLT_SHARD_COST && ShouldUseBurstCooldowns();
}

bool PlayerbotDestructionWarlockStrategy::IsAtOptimalShardCount() const
{
    return GetSoulShards() >= OPTIMAL_SHARD_COUNT;
}

bool PlayerbotDestructionWarlockStrategy::ShouldPrioritizeShardGeneration() const
{
    return GetSoulShards() < CHAOS_BOLT_SHARD_COST;
}

bool PlayerbotDestructionWarlockStrategy::ShouldUseBackdraftIncinerate() const
{
    return HasBackdraftProc() && GetBackdraftStacks() > 0;
}

bool PlayerbotDestructionWarlockStrategy::HasConflagrateCharges() const
{
    return GetConflagrateCharges() > 0;
}

uint32 PlayerbotDestructionWarlockStrategy::GetConflagrateCharges() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
        
    // Simplified charge checking - would need spell charge system integration
    return bot->GetSpellHistory()->HasCooldown(WarlockSpells::CONFLAGRATE) ? 0 : 1;
}