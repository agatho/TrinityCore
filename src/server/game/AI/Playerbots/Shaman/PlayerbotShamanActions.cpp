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

#include "PlayerbotShamanActions.h"
#include "PlayerbotPlayerAI.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "Group.h"

uint32 PlayerbotShamanAction::GetManaAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_MANA) : 0;
}

uint32 PlayerbotShamanAction::GetManaPercent() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 currentMana = bot->GetPower(POWER_MANA);
    uint32 maxMana = bot->GetMaxPower(POWER_MANA);
    
    return maxMana > 0 ? (currentMana * 100 / maxMana) : 0;
}

uint32 PlayerbotShamanAction::GetMaelstromWeaponStacks() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    Aura* aura = bot->GetAura(ShamanSpells::MAELSTROM_WEAPON);
    return aura ? aura->GetStackAmount() : 0;
}

bool PlayerbotShamanAction::HasMaelstromWeaponStacks(uint32 minStacks) const
{
    return GetMaelstromWeaponStacks() >= minStacks;
}

bool PlayerbotShamanAction::HasMaxMaelstromWeaponStacks() const
{
    return GetMaelstromWeaponStacks() >= 5;
}

bool PlayerbotShamanAction::ShouldSpendMaelstromWeapon() const
{
    return GetMaelstromWeaponStacks() >= 5;
}

bool PlayerbotShamanAction::isElemental() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(ShamanSpells::LAVA_BURST);
}

bool PlayerbotShamanAction::isEnhancement() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(ShamanSpells::STORMSTRIKE);
}

bool PlayerbotShamanAction::isRestoration() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(ShamanSpells::RIPTIDE);
}

bool PlayerbotShamanAction::IsInMeleeRange(Unit* target) const
{
    Player* bot = _ai->GetBot();
    return bot && target && bot->IsWithinMeleeRange(target);
}

bool PlayerbotShamanAction::ShouldInterrupt() const
{
    Unit* target = FindBestTarget();
    if (!target || !target->IsNonMeleeSpellCast(false))
        return false;
    
    SpellInfo const* spellInfo = target->GetCurrentSpell(CURRENT_GENERIC_SPELL)->GetSpellInfo();
    return spellInfo && spellInfo->IsPositive() == false;
}

bool PlayerbotShamanAction::NeedsHealing() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetHealthPct() < 70.0f;
}

Unit* PlayerbotShamanAction::FindBestTarget() const
{
    Unit* currentTarget = _ai->GetTarget();
    if (currentTarget && currentTarget->IsAlive() && _ai->GetBot()->IsValidAttackTarget(currentTarget))
        return currentTarget;
    
    Unit* target = _ai->GetBot()->GetVictim();
    if (target && target->IsAlive())
        return target;
    
    return nullptr;
}

Unit* PlayerbotShamanAction::FindBestHealingTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;
    
    // Check self first
    if (bot->GetHealthPct() < 80.0f)
        return bot;
    
    Group* group = bot->GetGroup();
    if (!group)
        return bot->GetHealthPct() < 95.0f ? bot : nullptr;
    
    Unit* bestTarget = nullptr;
    float lowestHealth = 100.0f;
    
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && bot->IsWithinDistInMap(member, 40.0f))
        {
            float healthPct = member->GetHealthPct();
            if (healthPct < 80.0f && healthPct < lowestHealth)
            {
                lowestHealth = healthPct;
                bestTarget = member;
            }
        }
    }
    
    return bestTarget;
}

uint32 PlayerbotShamanAction::CountNearbyEnemies(float range) const
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

uint32 PlayerbotShamanAction::CountInjuredGroupMembers(float threshold) const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 count = 0;
    Group* group = bot->GetGroup();
    if (!group)
        return bot->GetHealthPct() < (threshold * 100.0f) ? 1 : 0;
    
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && bot->IsWithinDistInMap(member, 40.0f))
        {
            if (member->GetHealthPct() < (threshold * 100.0f))
                count++;
        }
    }
    
    return count;
}

bool PlayerbotShamanAction::ShouldUseAoE() const
{
    return CountNearbyEnemies(8.0f) >= 3;
}

bool PlayerbotShamanAction::HasFlameShock(Unit* target) const
{
    return target && target->HasAura(ShamanSpells::FLAME_SHOCK, _ai->GetBot()->GetGUID());
}

float PlayerbotShamanAction::GetFlameShockRemainingDuration(Unit* target) const
{
    if (Aura* aura = target->GetAura(ShamanSpells::FLAME_SHOCK, _ai->GetBot()->GetGUID()))
        return float(aura->GetDuration()) / 1000.0f;
    return 0.0f;
}

bool PlayerbotShamanAction::ShouldRefreshFlameShock(Unit* target) const
{
    if (!HasFlameShock(target))
        return true;
    
    return GetFlameShockRemainingDuration(target) <= 6.0f;
}

bool PlayerbotShamanAction::HasLavaSurgeProc() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(ShamanSpells::LAVA_SURGE);
}

bool PlayerbotShamanAction::HasStormsurgeProc() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(ShamanSpells::STORMSURGE);
}

bool PlayerbotShamanAction::HasHotHandProc() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(ShamanSpells::HOT_HAND);
}

bool PlayerbotShamanAction::HasTidalWaves() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(ShamanSpells::TIDAL_WAVES);
}

uint32 PlayerbotShamanAction::GetTidalWavesStacks() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    Aura* aura = bot->GetAura(ShamanSpells::TIDAL_WAVES);
    return aura ? aura->GetStackAmount() : 0;
}

bool PlayerbotShamanAction::HasMasterOfElementsBuff() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(ShamanSpells::MASTER_OF_THE_ELEMENTS);
}

bool PlayerbotShamanAction::ShouldUseMasterOfElements() const
{
    return HasMasterOfElementsBuff();
}

uint32 PlayerbotShamanAction::GetMaelstromPower() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_MAELSTROM) : 0;
}

bool PlayerbotShamanAction::ShouldSpendMaelstrom() const
{
    return GetMaelstromPower() >= 60;
}

bool PlayerbotShamanAction::IsInDoomWinds() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(ShamanSpells::DOOM_WINDS);
}

bool PlayerbotShamanAction::ShouldUseInstantSpell() const
{
    return HasMaelstromWeaponStacks(5);
}

bool PlayerbotShamanAction::HasRiptide(Unit* target) const
{
    return target && target->HasAura(ShamanSpells::RIPTIDE, _ai->GetBot()->GetGUID());
}

bool PlayerbotShamanAction::ShouldUseChainHeal() const
{
    return CountInjuredGroupMembers(0.8f) >= 2;
}

bool PlayerbotShamanAction::ShouldUseAoEHealing() const
{
    return CountInjuredGroupMembers(0.7f) >= 3;
}

Unit* PlayerbotShamanAction::FindRiptideTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;
    
    Group* group = bot->GetGroup();
    if (!group)
        return bot->GetHealthPct() < 90.0f ? bot : nullptr;
    
    // Find lowest health member without Riptide
    Unit* bestTarget = nullptr;
    float lowestHealth = 100.0f;
    
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && bot->IsWithinDistInMap(member, 40.0f))
        {
            float healthPct = member->GetHealthPct();
            if (healthPct < 90.0f && !HasRiptide(member) && healthPct < lowestHealth)
            {
                lowestHealth = healthPct;
                bestTarget = member;
            }
        }
    }
    
    return bestTarget;
}

bool PlayerbotShamanAction::HasEarthShield(Unit* target) const
{
    return target && target->HasAura(ShamanSpells::EARTH_SHIELD, _ai->GetBot()->GetGUID());
}

// Elemental Shaman Actions
Unit* CastLightningBolt::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return nullptr;
    
    // Enhancement uses with Maelstrom Weapon stacks
    if (isEnhancement())
        return HasMaelstromWeaponStacks(5) ? target : nullptr;
    
    return target;
}

Unit* CastChainLightning::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return nullptr;
    
    // Enhancement uses with Maelstrom Weapon stacks for AoE
    if (isEnhancement())
        return (HasMaelstromWeaponStacks(5) && CountNearbyEnemies(8.0f) >= 2) ? target : nullptr;
    
    // Elemental uses for AoE
    return (CountNearbyEnemies(8.0f) >= 2) ? target : nullptr;
}

Unit* CastLavaBurst::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    if (!target)
        return nullptr;
    
    Player* bot = _ai->GetBot();
    return (bot && HasFlameShock(target)) ? target : nullptr;
}

Unit* CastFlameShock::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    if (!target)
        return nullptr;
    
    return ShouldRefreshFlameShock(target) ? target : nullptr;
}

Unit* CastEarthShock::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    if (!target || !ShouldSpendMaelstrom())
        return nullptr;
    
    return target;
}

Unit* CastElementalBlast::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return nullptr;
    
    // Use for Maelstrom spending or with Master of Elements
    return (ShouldSpendMaelstrom() || ShouldUseMasterOfElements()) ? target : nullptr;
}

Unit* CastEarthquake::GetSpellTarget() const
{
    return (CountNearbyEnemies(8.0f) >= 3 && ShouldSpendMaelstrom()) ? _ai->GetBot() : nullptr;
}

Unit* CastIcefury::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return nullptr;
    
    return bot->HasSpell(ShamanSpells::ICEFURY) ? target : nullptr;
}

Unit* CastFrostShock::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return nullptr;
    
    // Use after Icefury or for mobility
    return (bot->HasAura(ShamanSpells::ICEFURY) || !IsInMeleeRange(target)) ? target : nullptr;
}

Unit* CastPrimordialWave::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return target;
}

// Enhancement Shaman Actions
Unit* CastStormstrike::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && IsInMeleeRange(target)) ? target : nullptr;
}

Unit* CastLavaLash::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !IsInMeleeRange(target))
        return nullptr;
    
    // Prefer with Hot Hand proc or when Flametongue is active
    return (HasHotHandProc() || bot->HasAura(ShamanSpells::FLAMETONGUE_WEAPON)) ? target : nullptr;
}

Unit* CastCrashLightning::GetSpellTarget() const
{
    return (CountNearbyEnemies(8.0f) >= 2) ? _ai->GetBot() : nullptr;
}

Unit* CastSundering::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && IsInMeleeRange(target)) ? target : nullptr;
}

Unit* CastIceStrike::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && IsInMeleeRange(target)) ? target : nullptr;
}

// Restoration Shaman Actions
Unit* CastHealingWave::GetSpellTarget() const
{
    Unit* target = FindBestHealingTarget();
    if (!target)
        return nullptr;
    
    // Use with Tidal Waves for faster cast
    return HasTidalWaves() ? target : nullptr;
}

Unit* CastHealingSurge::GetSpellTarget() const
{
    Unit* target = FindBestHealingTarget();
    if (!target)
        return nullptr;
    
    // Use for emergency healing or with Tidal Waves
    return (target->GetHealthPct() < 40.0f || HasTidalWaves()) ? target : nullptr;
}

Unit* CastChainHeal::GetSpellTarget() const
{
    Unit* target = FindBestHealingTarget();
    return (target && ShouldUseChainHeal()) ? target : nullptr;
}

Unit* CastRiptide::GetSpellTarget() const
{
    return FindRiptideTarget();
}

Unit* CastHealingRain::GetSpellTarget() const
{
    return (ShouldUseAoEHealing()) ? _ai->GetBot() : nullptr;
}

Unit* CastEarthShield::GetSpellTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;
    
    Group* group = bot->GetGroup();
    if (!group)
        return !HasEarthShield(bot) ? bot : nullptr;
    
    // Find tank or lowest health member without Earth Shield
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && bot->IsWithinDistInMap(member, 40.0f))
        {
            // Prefer tanks (check if they have tanking auras or are taking damage)
            if (!HasEarthShield(member) && (member->HasAuraType(SPELL_AURA_MOD_THREAT) || member->GetHealthPct() < 90.0f))
                return member;
        }
    }
    
    return nullptr;
}

// Shared utility actions
Unit* CastWindShear::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    return (target && ShouldInterrupt()) ? target : nullptr;
}

Unit* CastPurge::GetSpellTarget() const
{
    Unit* target = FindBestTarget();
    if (!target)
        return nullptr;
    
    // Look for dispellable magic effects
    return target->HasAuraType(SPELL_AURA_MOD_INCREASE_SPEED) ? target : nullptr;
}

Unit* CastCleanseSpirit::GetSpellTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;
    
    Group* group = bot->GetGroup();
    if (!group)
    {
        return (bot->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE) || 
                bot->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED)) ? bot : nullptr;
    }
    
    // Find group member with dispellable debuffs
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && bot->IsWithinDistInMap(member, 40.0f))
        {
            if (member->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE) || 
                member->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED))
                return member;
        }
    }
    
    return nullptr;
}