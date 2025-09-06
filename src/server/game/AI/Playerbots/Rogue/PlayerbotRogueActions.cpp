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

#include "PlayerbotRogueActions.h"
#include "PlayerbotPlayerAI.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "Group.h"

uint32 PlayerbotRogueAction::GetEnergyAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_ENERGY) : 0;
}

uint32 PlayerbotRogueAction::GetComboPoints() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_COMBO_POINTS) : 0;
}

uint32 PlayerbotRogueAction::GetComboPoints(Unit* target) const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_COMBO_POINTS) : 0;
}

bool PlayerbotRogueAction::HasEnoughEnergy(uint32 cost) const
{
    return GetEnergyAmount() >= cost;
}

bool PlayerbotRogueAction::HasMaxComboPoints() const
{
    return GetComboPoints() >= 5;
}

bool PlayerbotRogueAction::IsNearMaxComboPoints(uint32 threshold) const
{
    return GetComboPoints() >= threshold;
}

bool PlayerbotRogueAction::isAssassination() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(RogueSpells::MUTILATE);
}

bool PlayerbotRogueAction::isOutlaw() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(RogueSpells::SINISTER_STRIKE);
}

bool PlayerbotRogueAction::isSubtlety() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(RogueSpells::SHADOWSTRIKE);
}

bool PlayerbotRogueAction::IsStealthed() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasStealthAura();
}

bool PlayerbotRogueAction::IsBehindTarget(Unit* target) const
{
    Player* bot = _ai->GetBot();
    return bot && target && bot->IsBehind(target);
}

bool PlayerbotRogueAction::ShouldUseOpener() const
{
    return IsStealthed() || HasSpellReady(RogueSpells::SHADOWSTEP);
}

bool PlayerbotRogueAction::HasRuptureOnTarget(Unit* target) const
{
    return target && target->HasAura(RogueSpells::RUPTURE, _ai->GetBot()->GetGUID());
}

bool PlayerbotRogueAction::HasGarroteOnTarget(Unit* target) const
{
    return target && target->HasAura(RogueSpells::GARROTE, _ai->GetBot()->GetGUID());
}

bool PlayerbotRogueAction::HasSliceAndDice() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(RogueSpells::SLICE_AND_DICE);
}

bool PlayerbotRogueAction::HasRollTheBonesBuffs() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    return bot->HasAura(RogueSpells::BROADSIDE) || 
           bot->HasAura(RogueSpells::BURIED_TREASURE) || 
           bot->HasAura(RogueSpells::GRAND_MELEE) || 
           bot->HasAura(RogueSpells::RUTHLESS_PRECISION) || 
           bot->HasAura(RogueSpells::SKULL_AND_CROSSBONES) || 
           bot->HasAura(RogueSpells::TRUE_BEARING);
}

uint32 PlayerbotRogueAction::CountRollTheBonesBuffs() const
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

bool PlayerbotRogueAction::IsInMeleeRange(Unit* target) const
{
    Player* bot = _ai->GetBot();
    return bot && target && bot->IsWithinMeleeRange(target);
}

bool PlayerbotRogueAction::ShouldInterrupt() const
{
    Unit* target = _ai->GetTarget();
    if (!target || !target->IsNonMeleeSpellCast(false))
        return false;
    
    SpellInfo const* spellInfo = target->GetCurrentSpell(CURRENT_GENERIC_SPELL)->GetSpellInfo();
    return spellInfo && spellInfo->IsPositive() == false;
}

bool PlayerbotRogueAction::NeedsHealing() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetHealthPct() < 60.0f;
}

Unit* PlayerbotRogueAction::FindBestTarget() const
{
    Unit* currentTarget = _ai->GetTarget();
    if (currentTarget && currentTarget->IsAlive() && _ai->GetBot()->IsValidAttackTarget(currentTarget))
        return currentTarget;
    
    Unit* target = _ai->GetBot()->GetVictim();
    if (target && target->IsAlive())
        return target;
    
    return nullptr;
}

uint32 PlayerbotRogueAction::CountNearbyEnemies(float range) const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 count = 0;
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot, range);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, check);
    bot->VisitNearbyObject(range, searcher);
    
    for (Unit* unit : targets)
    {
        if (unit->IsAlive() && bot->IsValidAttackTarget(unit))
            count++;
    }
    
    return count;
}

bool PlayerbotRogueAction::HasSpellReady(uint32 spellId) const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(spellId) && !bot->GetSpellHistory()->HasCooldown(spellId);
}

// Assassination Rogue Actions
Unit* CastMutilate::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && IsInMeleeRange(target)) ? target : nullptr;
}

Unit* CastEnvenom::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && IsInMeleeRange(target) && GetComboPoints() >= 1) ? target : nullptr;
}

Unit* CastGarrote::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    if (!target || !IsInMeleeRange(target))
        return nullptr;
    
    // Prefer from stealth or behind target
    if (IsStealthed() || IsBehindTarget(target))
        return target;
    
    return nullptr;
}

Unit* CastRupture::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && IsInMeleeRange(target) && GetComboPoints() >= 1) ? target : nullptr;
}

Unit* CastVendetta::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && IsInMeleeRange(target)) ? target : nullptr;
}

Unit* CastDeathmark::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && IsInMeleeRange(target)) ? target : nullptr;
}

Unit* CastKingsbane::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && IsInMeleeRange(target)) ? target : nullptr;
}

Unit* CastShiv::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && IsInMeleeRange(target)) ? target : nullptr;
}

Unit* CastFanOfKnives::GetSpellTarget() const
{
    return (CountNearbyEnemies(8.0f) >= 2) ? _ai->GetBot() : nullptr;
}

// Outlaw Rogue Actions
Unit* CastSinisterStrike::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && IsInMeleeRange(target)) ? target : nullptr;
}

Unit* CastDispatch::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && IsInMeleeRange(target) && GetComboPoints() >= 1) ? target : nullptr;
}

Unit* CastPistolShot::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    // Use on Opportunity proc or when target is at range
    if (target && (bot->HasAura(RogueSpells::OPPORTUNITY) || !IsInMeleeRange(target)))
        return target;
    
    return nullptr;
}

Unit* CastBetweenTheEyes::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && GetComboPoints() >= 1) ? target : nullptr;
}

Unit* CastKillingSpree::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && IsInMeleeRange(target)) ? target : nullptr;
}

// Subtlety Rogue Actions
Unit* CastBackstab::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    if (!target || !IsInMeleeRange(target))
        return nullptr;
    
    // Prefer when behind target or in stealth
    if (IsBehindTarget(target) || IsStealthed())
        return target;
    
    return target; // Can still use when not behind
}

Unit* CastEviscerate::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && IsInMeleeRange(target) && GetComboPoints() >= 1) ? target : nullptr;
}

Unit* CastShadowstrike::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && IsStealthed()) ? target : nullptr;
}

Unit* CastShadowClone::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return target;
}

Unit* CastBlackPowder::GetSpellTarget() const
{
    return (CountNearbyEnemies(8.0f) >= 2 && GetComboPoints() >= 1) ? _ai->GetBot() : nullptr;
}

Unit* CastShurikenStorm::GetSpellTarget() const
{
    return (CountNearbyEnemies(8.0f) >= 2) ? _ai->GetBot() : nullptr;
}

// Shared utility actions
Unit* CastKick::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && ShouldInterrupt() && IsInMeleeRange(target)) ? target : nullptr;
}