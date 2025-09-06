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

#include "PlayerbotDemonHunterActions.h"
#include "PlayerbotPlayerAI.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "SpellHistory.h"
#include "Group.h"

uint32 PlayerbotDemonHunterAction::GetFuryAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_FURY) : 0;
}

uint32 PlayerbotDemonHunterAction::GetFuryPercent() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 maxFury = bot->GetMaxPower(POWER_FURY);
    return maxFury > 0 ? (GetFuryAmount() * 100) / maxFury : 0;
}

uint32 PlayerbotDemonHunterAction::GetPainAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_PAIN) : 0;
}

uint32 PlayerbotDemonHunterAction::GetPainPercent() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 maxPain = bot->GetMaxPower(POWER_PAIN);
    return maxPain > 0 ? (GetPainAmount() * 100) / maxPain : 0;
}

bool PlayerbotDemonHunterAction::HasEnoughFury(uint32 cost) const
{
    return GetFuryAmount() >= cost;
}

bool PlayerbotDemonHunterAction::HasEnoughPain(uint32 cost) const
{
    return GetPainAmount() >= cost;
}

bool PlayerbotDemonHunterAction::isHavoc() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(DemonHunterSpells::CHAOS_STRIKE);
}

bool PlayerbotDemonHunterAction::isVengeance() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(DemonHunterSpells::SHEAR);
}

Unit* PlayerbotDemonHunterAction::FindBestTarget() const
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

bool PlayerbotDemonHunterAction::IsInCombat() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->IsInCombat();
}

bool PlayerbotDemonHunterAction::IsInMeleeRange(Unit* target) const
{
    Player* bot = _ai->GetBot();
    return bot && target && bot->GetDistance(target) <= 5.0f;
}

uint32 PlayerbotDemonHunterAction::CountNearbyEnemies(float range) const
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

uint32 PlayerbotDemonHunterAction::CountNearbyAllies(float range) const
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

bool PlayerbotDemonHunterAction::ShouldUseAoE() const
{
    return CountNearbyEnemies(8.0f) >= 3;
}

bool PlayerbotDemonHunterAction::HasMetamorphosis() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    return bot->HasAura(DemonHunterSpells::METAMORPHOSIS_HAVOC) || 
           bot->HasAura(DemonHunterSpells::METAMORPHOSIS_TANK);
}

bool PlayerbotDemonHunterAction::HasDemonSpikes() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DemonHunterSpells::DEMON_SPIKES);
}

bool PlayerbotDemonHunterAction::HasImmolationAura() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    return bot->HasAura(DemonHunterSpells::IMMOLATION_AURA) ||
           bot->HasAura(DemonHunterSpells::IMMOLATION_AURA_TANK);
}

bool PlayerbotDemonHunterAction::HasFieryBrand(Unit* target) const
{
    if (!target)
        target = FindBestTarget();
    
    return target && target->HasAura(DemonHunterSpells::FIERY_BRAND);
}

bool PlayerbotDemonHunterAction::HasSigil(uint32 sigilSpell) const
{
    // Check if there's an active sigil effect in the area
    // This is simplified - in practice would check ground effects
    return false;
}

bool PlayerbotDemonHunterAction::HasPrepared() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DemonHunterSpells::PREPARED);
}

bool PlayerbotDemonHunterAction::HasMomentum() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DemonHunterSpells::MOMENTUM);
}

uint32 PlayerbotDemonHunterAction::GetSoulFragments() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    // Count soul fragments around the player
    // This is simplified - would need to check actual soul fragment objects
    if (Aura* aura = bot->GetAura(DemonHunterSpells::SOUL_FRAGMENTS))
        return aura->GetStackAmount();
    
    return 0;
}

bool PlayerbotDemonHunterAction::ShouldConsumeSoulFragments() const
{
    return GetSoulFragments() >= 4 || (isVengeance() && GetSoulFragments() >= 2);
}

bool PlayerbotDemonHunterAction::ShouldUseEssenceBreak() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !IsInMeleeRange(target))
        return false;
    
    return HasEnoughFury(30) && !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::ESSENCE_BREAK);
}

bool PlayerbotDemonHunterAction::ShouldUseBladeDance() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    uint32 spellId = HasMetamorphosis() ? DemonHunterSpells::DEATH_SWEEP : DemonHunterSpells::BLADE_DANCE;
    return HasEnoughFury(35) && !bot->GetSpellHistory()->HasCooldown(spellId);
}

bool PlayerbotDemonHunterAction::ShouldUseChaosStrike() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !IsInMeleeRange(target))
        return false;
    
    uint32 spellId = HasMetamorphosis() ? DemonHunterSpells::ANNIHILATION : DemonHunterSpells::CHAOS_STRIKE;
    return HasEnoughFury(40) && !bot->GetSpellHistory()->HasCooldown(spellId);
}

bool PlayerbotDemonHunterAction::ShouldUseEyeBeam() const
{
    Player* bot = _ai->GetBot();
    Unit* target = FindBestTarget();
    
    if (!target || !bot)
        return false;
    
    return HasEnoughFury(30) && bot->GetDistance(target) <= 20.0f &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::EYE_BEAM);
}

bool PlayerbotDemonHunterAction::IsInDemonicWindow() const
{
    // Check if we recently used Eye Beam and have the Demonic buff
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DemonHunterSpells::INNER_DEMON);
}

bool PlayerbotDemonHunterAction::ShouldUseBurstCooldowns() const
{
    Unit* target = FindBestTarget();
    return target && target->GetHealthPct() > 70.0f && IsInCombat();
}

bool PlayerbotDemonHunterAction::ShouldUseDemonSpikes() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot || !isVengeance())
        return false;
    
    return IsInCombat() && !HasDemonSpikes() && 
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::DEMON_SPIKES);
}

bool PlayerbotDemonHunterAction::ShouldUseInfernalStrike() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !isVengeance())
        return false;
    
    return bot->GetDistance(target) > 10.0f && bot->GetDistance(target) <= 30.0f &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::INFERNAL_STRIKE_TANK);
}

bool PlayerbotDemonHunterAction::ShouldUseSigilOfFlame() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot || !isVengeance())
        return false;
    
    return CountNearbyEnemies(8.0f) >= 1 && 
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::SIGIL_OF_FLAME);
}

bool PlayerbotDemonHunterAction::ShouldUseFieryBrand() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !isVengeance())
        return false;
    
    return !HasFieryBrand(target) && bot->GetDistance(target) <= 30.0f &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::FIERY_BRAND);
}

bool PlayerbotDemonHunterAction::NeedsThreat() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    return target && bot && target->GetVictim() != bot;
}

bool PlayerbotDemonHunterAction::ShouldUseSoulCleave() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot || !isVengeance())
        return false;
    
    return HasEnoughPain(30) && (GetSoulFragments() >= 2 || ShouldConsumeSoulFragments()) &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::SOUL_CLEAVE);
}

bool PlayerbotDemonHunterAction::ShouldUseSpiritBomb() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot || !isVengeance())
        return false;
    
    return HasEnoughPain(40) && GetSoulFragments() >= 4 && CountNearbyEnemies(8.0f) >= 2 &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::SPIRIT_BOMB);
}

bool PlayerbotDemonHunterAction::ShouldUseBlur() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return bot->GetHealthPct() < 50.0f && IsInCombat() &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::BLUR);
}

bool PlayerbotDemonHunterAction::ShouldUseDarkness() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot)
        return false;
    
    return (bot->GetHealthPct() < 40.0f || CountNearbyAllies(20.0f) >= 3) && IsInCombat() &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::DARKNESS);
}

bool PlayerbotDemonHunterAction::ShouldUseLastResort() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot || !isVengeance())
        return false;
    
    return bot->GetHealthPct() < 20.0f && IsInCombat() &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::LAST_RESORT);
}

bool PlayerbotDemonHunterAction::ShouldUseSoulBarrier() const
{
    Player* bot = _ai->GetBot();
    
    if (!bot || !isVengeance())
        return false;
    
    return bot->GetHealthPct() < 60.0f && GetSoulFragments() >= 3 && IsInCombat() &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::SOUL_BARRIER);
}

bool PlayerbotDemonHunterAction::ShouldUseConsumeMagic() const
{
    Unit* target = FindBestTarget();
    
    if (!target)
        return false;
    
    // Check if target has dispellable magic effects
    return target->HasAuraType(SPELL_AURA_SCHOOL_IMMUNITY) || 
           target->HasAuraType(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
}

bool PlayerbotDemonHunterAction::ShouldUseDisrupt() const
{
    Unit* target = FindBestTarget();
    return target && target->IsNonMeleeSpellCast(false);
}

bool PlayerbotDemonHunterAction::ShouldUseThrowGlaive() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return bot->GetDistance(target) > 8.0f && bot->GetDistance(target) <= 30.0f &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::THROW_GLAIVE);
}

bool PlayerbotDemonHunterAction::ShouldUseTorment() const
{
    if (!isVengeance() || !NeedsThreat())
        return false;
    
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    return target && bot && bot->GetDistance(target) <= 30.0f &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::TORMENT);
}

bool PlayerbotDemonHunterAction::ShouldUseImprison() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    // Use on dangerous targets that are not the main target
    return CountNearbyEnemies() >= 3 && bot->GetDistance(target) <= 30.0f &&
           !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::IMPRISON);
}

// Havoc Demon Hunter Action Implementations
Unit* CastDemonsBite::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && IsInMeleeRange(target) && HasEnoughFury(20)) ? target : nullptr;
}

Unit* CastChaosStrike::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    uint32 spellId = HasMetamorphosis() ? DemonHunterSpells::ANNIHILATION : DemonHunterSpells::CHAOS_STRIKE;
    return (target && IsInMeleeRange(target) && ShouldUseChaosStrike()) ? target : nullptr;
}

Unit* CastAnnihilation::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && IsInMeleeRange(target) && HasMetamorphosis() && ShouldUseChaosStrike()) ? target : nullptr;
}

Unit* CastBladeDance::GetSpellTarget() const
{
    return ShouldUseBladeDance() ? FindBestTarget() : nullptr;
}

Unit* CastDeathSweep::GetSpellTarget() const
{
    return (HasMetamorphosis() && ShouldUseBladeDance()) ? FindBestTarget() : nullptr;
}

Unit* CastEyeBeam::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && ShouldUseEyeBeam()) ? target : nullptr;
}

Unit* CastImmolationAuraHavoc::GetSpellTarget() const
{
    Player* bot = _ai->GetBot();
    return (bot && !HasImmolationAura() && HasEnoughFury(20)) ? bot : nullptr;
}

Unit* CastMetamorphosisHavoc::GetSpellTarget() const
{
    Player* bot = _ai->GetBot();
    return (bot && !HasMetamorphosis() && ShouldUseBurstCooldowns()) ? bot : nullptr;
}

Unit* CastEssenceBreak::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && ShouldUseEssenceBreak()) ? target : nullptr;
}

Unit* CastTheHuntHavoc::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    return (target && bot && ShouldUseBurstCooldowns() && 
            !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::THE_HUNT)) ? target : nullptr;
}

Unit* CastGlaiveTempest::GetSpellTarget() const
{
    Player* bot = _ai->GetBot();
    return (bot && ShouldUseAoE() && HasEnoughFury(30) &&
            !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::GLAIVE_TEMPEST)) ? bot : nullptr;
}

Unit* CastElysianDecreeHavoc::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    return (target && bot && ShouldUseBurstCooldowns() && 
            !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::ELYSIAN_DECREE)) ? target : nullptr;
}

Unit* CastFelBarrage::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    return (target && bot && ShouldUseAoE() && HasEnoughFury(25) &&
            !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::FEL_BARRAGE)) ? target : nullptr;
}

// Vengeance Demon Hunter Action Implementations
Unit* CastDemonSpikes::GetSpellTarget() const
{
    Player* bot = _ai->GetBot();
    return (bot && ShouldUseDemonSpikes()) ? bot : nullptr;
}

Unit* CastImmolationAuraTank::GetSpellTarget() const
{
    Player* bot = _ai->GetBot();
    return (bot && !HasImmolationAura() && HasEnoughPain(20)) ? bot : nullptr;
}

Unit* CastInfernalStrike::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && ShouldUseInfernalStrike()) ? target : nullptr;
}

Unit* CastShear::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && IsInMeleeRange(target)) ? target : nullptr;
}

Unit* CastFracture::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    return (target && bot && IsInMeleeRange(target) && HasEnoughPain(25) &&
            !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::FRACTURE)) ? target : nullptr;
}

Unit* CastSoulCleave::GetSpellTarget() const
{
    return ShouldUseSoulCleave() ? FindBestTarget() : nullptr;
}

Unit* CastSpiritBomb::GetSpellTarget() const
{
    return ShouldUseSpiritBomb() ? FindBestTarget() : nullptr;
}

Unit* CastSigilOfFlame::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && ShouldUseSigilOfFlame()) ? target : nullptr;
}

Unit* CastSigilOfSilence::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    return (target && bot && target->IsNonMeleeSpellCast(false) &&
            !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::SIGIL_OF_SILENCE)) ? target : nullptr;
}

Unit* CastSigilOfMisery::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    return (target && bot && CountNearbyEnemies() >= 2 &&
            !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::SIGIL_OF_MISERY)) ? target : nullptr;
}

Unit* CastSigilOfChains::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    return (target && bot && CountNearbyEnemies() >= 2 &&
            !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::SIGIL_OF_CHAINS)) ? target : nullptr;
}

Unit* CastFieryBrand::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && ShouldUseFieryBrand()) ? target : nullptr;
}

Unit* CastMetamorphosisTank::GetSpellTarget() const
{
    Player* bot = _ai->GetBot();
    return (bot && !HasMetamorphosis() && ShouldUseBurstCooldowns()) ? bot : nullptr;
}

Unit* CastFelDevastation::GetSpellTarget() const
{
    Player* bot = _ai->GetBot();
    return (bot && HasEnoughPain(50) && CountNearbyEnemies() >= 2 &&
            !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::FEL_DEVASTATION)) ? bot : nullptr;
}

Unit* CastSoulBarrier::GetSpellTarget() const
{
    Player* bot = _ai->GetBot();
    return (bot && ShouldUseSoulBarrier()) ? bot : nullptr;
}

Unit* CastLastResort::GetSpellTarget() const
{
    Player* bot = _ai->GetBot();
    return (bot && ShouldUseLastResort()) ? bot : nullptr;
}

Unit* CastBulkExtraction::GetSpellTarget() const
{
    Player* bot = _ai->GetBot();
    return (bot && CountNearbyEnemies() >= 3 && HasEnoughPain(35) &&
            !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::BULK_EXTRACTION)) ? bot : nullptr;
}

Unit* CastTheHuntTank::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    return (target && bot && ShouldUseBurstCooldowns() && 
            !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::THE_HUNT_TANK)) ? target : nullptr;
}

Unit* CastElysianDecreeTank::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    return (target && bot && ShouldUseBurstCooldowns() && 
            !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::ELYSIAN_DECREE_TANK)) ? target : nullptr;
}

// Utility Action Implementations
Unit* CastFelRush::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    return (target && bot && bot->GetDistance(target) > 8.0f && bot->GetDistance(target) <= 15.0f &&
            !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::FEL_RUSH)) ? target : nullptr;
}

Unit* CastVengefulRetreat::GetSpellTarget() const
{
    Player* bot = _ai->GetBot();
    return (bot && bot->GetHealthPct() < 40.0f && IsInCombat() &&
            !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::VENGEFUL_RETREAT)) ? bot : nullptr;
}

Unit* CastThrowGlaive::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && ShouldUseThrowGlaive()) ? target : nullptr;
}

Unit* CastConsumeMagic::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    return (target && bot && ShouldUseConsumeMagic() &&
            !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::CONSUME_MAGIC)) ? target : nullptr;
}

Unit* CastDisrupt::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    return (target && bot && ShouldUseDisrupt() &&
            !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::DISRUPT)) ? target : nullptr;
}

Unit* CastTorment::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && ShouldUseTorment()) ? target : nullptr;
}

Unit* CastImprison::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    return (target && bot && ShouldUseImprison()) ? target : nullptr;
}

Unit* CastBlur::GetSpellTarget() const
{
    Player* bot = _ai->GetBot();
    return (bot && ShouldUseBlur()) ? bot : nullptr;
}

Unit* CastDarkness::GetSpellTarget() const
{
    Player* bot = _ai->GetBot();
    return (bot && ShouldUseDarkness()) ? bot : nullptr;
}

Unit* CastSpectralSight::GetSpellTarget() const
{
    Player* bot = _ai->GetBot();
    return (bot && !IsInCombat() &&
            !bot->GetSpellHistory()->HasCooldown(DemonHunterSpells::SPECTRAL_SIGHT)) ? bot : nullptr;
}