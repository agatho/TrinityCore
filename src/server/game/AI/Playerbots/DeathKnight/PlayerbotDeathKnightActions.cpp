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

#include "PlayerbotDeathKnightActions.h"
#include "PlayerbotPlayerAI.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "Group.h"

uint32 PlayerbotDeathKnightAction::GetRuneCount() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 count = 0;
    for (uint8 i = 0; i < MAX_RUNES; ++i)
    {
        if (bot->GetCurrentRune(i) == RUNE_DEATH || bot->IsRuneReady(i))
            count++;
    }
    return count;
}

uint32 PlayerbotDeathKnightAction::GetRunicPowerAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_RUNIC_POWER) : 0;
}

bool PlayerbotDeathKnightAction::HasEnoughRunes(uint32 cost) const
{
    return GetRuneCount() >= cost;
}

bool PlayerbotDeathKnightAction::HasEnoughRunicPower(uint32 cost) const
{
    return GetRunicPowerAmount() >= cost;
}

bool PlayerbotDeathKnightAction::IsRunicPowerCapped() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    return bot->GetPower(POWER_RUNIC_POWER) >= (bot->GetMaxPower(POWER_RUNIC_POWER) - 20);
}

bool PlayerbotDeathKnightAction::ShouldConserveRunes() const
{
    return GetRuneCount() <= 2;
}

bool PlayerbotDeathKnightAction::isBlood() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(DeathKnightSpells::HEART_STRIKE);
}

bool PlayerbotDeathKnightAction::isFrost() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(DeathKnightSpells::OBLITERATE);
}

bool PlayerbotDeathKnightAction::isUnholy() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(DeathKnightSpells::FESTERING_STRIKE);
}

bool PlayerbotDeathKnightAction::IsInMeleeRange(Unit* target) const
{
    Player* bot = _ai->GetBot();
    return bot && target && bot->IsWithinMeleeRange(target);
}

bool PlayerbotDeathKnightAction::ShouldInterrupt() const
{
    Unit* target = FindBestTarget();
    if (!target || !target->IsNonMeleeSpellCast(false))
        return false;
    
    SpellInfo const* spellInfo = target->GetCurrentSpell(CURRENT_GENERIC_SPELL)->GetSpellInfo();
    return spellInfo && spellInfo->IsPositive() == false;
}

bool PlayerbotDeathKnightAction::NeedsHealing() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetHealthPct() < 70.0f;
}

Unit* PlayerbotDeathKnightAction::FindBestTarget() const
{
    Unit* currentTarget = _ai->GetTarget();
    if (currentTarget && currentTarget->IsAlive() && _ai->GetBot()->IsValidAttackTarget(currentTarget))
        return currentTarget;
    
    Unit* target = _ai->GetBot()->GetVictim();
    if (target && target->IsAlive())
        return target;
    
    return nullptr;
}

Unit* PlayerbotDeathKnightAction::FindBestTankTarget() const
{
    // For tanking, prefer targets attacking the tank or group members
    Unit* currentTarget = _ai->GetTarget();
    if (currentTarget && currentTarget->IsAlive() && _ai->GetBot()->IsValidAttackTarget(currentTarget))
        return currentTarget;
    
    // Look for targets attacking group members
    Group* group = _ai->GetBot()->GetGroup();
    if (group)
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member->IsAlive())
            {
                Unit* attacker = member->GetAttackers().empty() ? nullptr : *member->GetAttackers().begin();
                if (attacker && attacker->IsAlive() && _ai->GetBot()->IsValidAttackTarget(attacker))
                    return attacker;
            }
        }
    }
    
    return FindBestTarget();
}

uint32 PlayerbotDeathKnightAction::CountNearbyEnemies(float range) const
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

bool PlayerbotDeathKnightAction::ShouldUseAoE() const
{
    return CountNearbyEnemies(10.0f) >= 3;
}

bool PlayerbotDeathKnightAction::HasBoneShield() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DeathKnightSpells::BONE_SHIELD);
}

uint32 PlayerbotDeathKnightAction::GetBoneShieldStacks() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    Aura* aura = bot->GetAura(DeathKnightSpells::BONE_SHIELD);
    return aura ? aura->GetStackAmount() : 0;
}

bool PlayerbotDeathKnightAction::HasVirulentPlague(Unit* target) const
{
    return target && target->HasAura(DeathKnightSpells::VIRULENT_PLAGUE, _ai->GetBot()->GetGUID());
}

uint32 PlayerbotDeathKnightAction::GetFesteringWoundStacks(Unit* target) const
{
    if (!target)
        return 0;
    
    Aura* aura = target->GetAura(DeathKnightSpells::FESTERING_WOUND, _ai->GetBot()->GetGUID());
    return aura ? aura->GetStackAmount() : 0;
}

bool PlayerbotDeathKnightAction::HasFesteringWounds(Unit* target) const
{
    return GetFesteringWoundStacks(target) > 0;
}

bool PlayerbotDeathKnightAction::HasKillingMachineProc() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DeathKnightSpells::KILLING_MACHINE);
}

bool PlayerbotDeathKnightAction::HasRimeProc() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DeathKnightSpells::RIME);
}

bool PlayerbotDeathKnightAction::HasSuddenDoomProc() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DeathKnightSpells::SUDDEN_DOOM);
}

bool PlayerbotDeathKnightAction::HasRunicCorruption() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DeathKnightSpells::RUNIC_CORRUPTION);
}

bool PlayerbotDeathKnightAction::IsCurrentTarget(Unit* unit) const
{
    return unit && _ai->GetTarget() == unit;
}

bool PlayerbotDeathKnightAction::ShouldTaunt(Unit* target) const
{
    Player* bot = _ai->GetBot();
    if (!target || !bot)
        return false;
    
    // Only tanks should taunt
    if (!isBlood())
        return false;
    
    // Don't taunt if already targeting us
    return target->GetVictim() != bot;
}

bool PlayerbotDeathKnightAction::NeedsThreatOnTarget(Unit* target) const
{
    Player* bot = _ai->GetBot();
    if (!target || !bot || !isBlood())
        return false;
    
    return target->GetThreatManager().GetThreat(bot) < target->GetThreatManager().GetMaxThreat() * 1.1f;
}

// Blood Death Knight Actions
Unit* CastHeartStrike::GetSpellTarget() const
{
    Unit* target = FindBestTankTarget();
    return (target && IsInMeleeRange(target)) ? target : nullptr;
}

Unit* CastBloodBoil::GetSpellTarget() const
{
    return (CountNearbyEnemies(10.0f) >= 2) ? _ai->GetBot() : nullptr;
}

Unit* CastDeathStrike::GetSpellTarget() const
{
    Unit* target = FindBestTankTarget();
    return (target && IsInMeleeRange(target) && NeedsHealing()) ? target : nullptr;
}

Unit* CastMarrowrend::GetSpellTarget() const
{
    Unit* target = FindBestTankTarget();
    if (!target || !IsInMeleeRange(target))
        return nullptr;
    
    // Use to build/maintain Bone Shield
    return (GetBoneShieldStacks() < 5) ? target : nullptr;
}

// Frost Death Knight Actions
Unit* CastObliterate::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    if (!target || !IsInMeleeRange(target))
        return nullptr;
    
    // Prefer Frostscythe for AoE
    if (CountNearbyEnemies(8.0f) >= 2)
        return nullptr;
    
    return target;
}

Unit* CastFrostStrike::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && HasEnoughRunicPower(25)) ? target : nullptr;
}

Unit* CastHowlingBlast::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return nullptr;
    
    // Use with Rime proc or for AoE
    return (HasRimeProc() || CountNearbyEnemies(8.0f) >= 2) ? target : nullptr;
}

Unit* CastGlacialAdvance::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && HasEnoughRunicPower(35)) ? target : nullptr;
}

Unit* CastFrostscythe::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    if (!target || !IsInMeleeRange(target))
        return nullptr;
    
    // Use for AoE situations
    return (CountNearbyEnemies(8.0f) >= 2) ? target : nullptr;
}

Unit* CastBreathOfSindragosa::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return nullptr;
    
    return (HasEnoughRunicPower(60) && target->GetHealthPct() > 50.0f) ? target : nullptr;
}

Unit* CastFrostwyrmsFury::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && CountNearbyEnemies(8.0f) >= 2) ? target : nullptr;
}

// Unholy Death Knight Actions
Unit* CastFesteringStrike::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    if (!target || !IsInMeleeRange(target))
        return nullptr;
    
    // Apply Festering Wounds if target has 2 or fewer
    return (GetFesteringWoundStacks(target) <= 2) ? target : nullptr;
}

Unit* CastScourgeStrike::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    if (!target || !IsInMeleeRange(target))
        return nullptr;
    
    // Burst Festering Wounds
    return HasFesteringWounds(target) ? target : nullptr;
}

Unit* CastDeathCoil::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return nullptr;
    
    // Use when high Runic Power, Sudden Doom proc, or Death Rot expiring
    return (HasEnoughRunicPower(80) || HasSuddenDoomProc() || 
            bot->HasAura(DeathKnightSpells::DEATH_ROT)) ? target : nullptr;
}

Unit* CastEpidemic::GetSpellTarget() const
{
    return (CountNearbyEnemies(8.0f) >= 2 && HasEnoughRunicPower(30)) ? _ai->GetBot() : nullptr;
}

Unit* CastOutbreak::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    if (!target)
        return nullptr;
    
    // Maintain Virulent Plague
    return !HasVirulentPlague(target) ? target : nullptr;
}

Unit* CastApocalypse::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    if (!target || !IsInMeleeRange(target))
        return nullptr;
    
    // Use when target has Festering Wounds
    return (GetFesteringWoundStacks(target) >= 4) ? target : nullptr;
}

Unit* CastSoulReaper::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    if (!target || !IsInMeleeRange(target))
        return nullptr;
    
    // Use when target is below 35%
    return (target->GetHealthPct() < 35.0f) ? target : nullptr;
}

Unit* CastDefile::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && CountNearbyEnemies(8.0f) >= 2) ? target : nullptr;
}

// Shared utility actions
Unit* CastDeathGrip::GetSpellTarget() const
{
    // Look for ranged enemies or enemies not targeting the tank
    Player* bot = _ai->GetBot();
    if (!bot || !isBlood())
        return nullptr;
    
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot, 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, check);
    bot->VisitNearbyObject(30.0f, searcher);
    
    for (Unit* unit : targets)
    {
        if (unit->IsAlive() && bot->IsValidAttackTarget(unit) && 
            !bot->IsWithinMeleeRange(unit) && unit->GetVictim() != bot)
            return unit;
    }
    
    return nullptr;
}

Unit* CastDeathAndDecay::GetSpellTarget() const
{
    return (CountNearbyEnemies(8.0f) >= 3) ? _ai->GetBot() : nullptr;
}

Unit* CastMindFreeze::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && ShouldInterrupt() && IsInMeleeRange(target)) ? target : nullptr;
}

Unit* CastDarkCommand::GetSpellTarget() const
{
    Unit* target = FindBestTankTarget();
    return (target && ShouldTaunt(target)) ? target : nullptr;
}