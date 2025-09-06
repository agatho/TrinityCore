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

#include "PlayerbotShamanStrategy.h"
#include "PlayerbotShamanActions.h"
#include "PlayerbotPlayerAI.h"
#include "Player.h"
#include "SpellAuraEffects.h"
#include "SpellHistory.h"
#include "Group.h"

PlayerbotShamanStrategy::PlayerbotShamanStrategy(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotStrategy(ai, name) {}

void PlayerbotShamanStrategy::InitializeActions()
{
    RegisterCommonActions();
}

void PlayerbotShamanStrategy::InitializeTriggers()
{
    RegisterCommonTriggers();
}

void PlayerbotShamanStrategy::RegisterCommonActions()
{
    // Utility actions
    RegisterAction("ghost wolf", std::make_unique<CastGhostWolf>(_ai));
    RegisterAction("astral shift", std::make_unique<CastAstralShift>(_ai));
    RegisterAction("wind shear", std::make_unique<CastWindShear>(_ai));
    RegisterAction("purge", std::make_unique<CastPurge>(_ai));
    RegisterAction("cleanse spirit", std::make_unique<CastCleanseSpirit>(_ai));
    RegisterAction("tremor totem", std::make_unique<CastTremorTotem>(_ai));
    RegisterAction("windfury totem", std::make_unique<CastWindfuryTotem>(_ai));
    RegisterAction("earth elemental", std::make_unique<CastEarthElemental>(_ai));
    RegisterAction("fire elemental", std::make_unique<CastFireElemental>(_ai));
}

void PlayerbotShamanStrategy::RegisterCommonTriggers()
{
    // TODO: Implement trigger system when base trigger framework is available
}

float PlayerbotShamanStrategy::GetActionPriority(std::string const& actionName) const
{
    // Common utility priorities
    if (actionName == "wind shear" && ShouldUseWindShear())
        return 0.99f;
    else if (actionName == "astral shift" && ShouldUseAstralShift())
        return 0.95f;
    else if (actionName == "cleanse spirit" && ShouldUseCleanseSpirit())
        return 0.93f;
    else if (actionName == "tremor totem" && ShouldUseTremorTotem())
        return 0.91f;
    else if (actionName == "purge" && ShouldUsePurge())
        return 0.89f;
    else if (actionName == "earth elemental" && ShouldUseEarthElemental())
        return 0.87f;
    else if (actionName == "fire elemental" && ShouldUseFireElemental())
        return 0.85f;
    else if (actionName == "windfury totem" && ShouldUseWindfuryTotem())
        return 0.83f;
    else if (actionName == "ghost wolf" && ShouldUseGhostWolf())
        return 0.81f;
    
    return 0.0f;
}

bool PlayerbotShamanStrategy::ShouldUseGhostWolf() const
{
    Player* bot = _ai->GetBot();
    Unit* target = FindBestTarget();
    
    if (!bot || !target || bot->IsInCombat())
        return false;
    
    // Use for travel when target is far away
    return bot->GetDistance(target) > 20.0f;
}

bool PlayerbotShamanStrategy::ShouldUseAstralShift() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !IsInCombat())
        return false;
    
    return bot->GetHealthPct() < 50.0f;
}

bool PlayerbotShamanStrategy::ShouldUseWindShear() const
{
    Unit* target = FindBestTarget();
    if (!target || !target->IsNonMeleeSpellCast(false))
        return false;
    
    Player* bot = _ai->GetBot();
    return bot && bot->IsWithinDistInMap(target, 30.0f);
}

bool PlayerbotShamanStrategy::ShouldUsePurge() const
{
    Unit* target = FindBestTarget();
    if (!target)
        return false;
    
    return target->HasAuraType(SPELL_AURA_MOD_INCREASE_SPEED) ||
           target->HasAuraType(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
}

bool PlayerbotShamanStrategy::ShouldUseCleanseSpirit() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    Group* group = bot->GetGroup();
    if (!group)
    {
        return bot->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE) ||
               bot->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED);
    }
    
    // Check group members for dispellable debuffs
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && bot->IsWithinDistInMap(member, 40.0f))
        {
            if (member->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE) ||
                member->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED))
                return true;
        }
    }
    
    return false;
}

bool PlayerbotShamanStrategy::ShouldUseTremorTotem() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !IsInCombat())
        return false;
    
    // Use against fear, charm, sleep effects
    return bot->HasAuraType(SPELL_AURA_MOD_FEAR) ||
           bot->HasAuraType(SPELL_AURA_MOD_CHARM) ||
           bot->HasAuraType(SPELL_AURA_MOD_STUN);
}

bool PlayerbotShamanStrategy::ShouldUseWindfuryTotem() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !IsInCombat())
        return false;
    
    Group* group = bot->GetGroup();
    if (!group)
        return isEnhancement();
    
    // Check for melee group members
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && bot->IsWithinDistInMap(member, 40.0f))
        {
            if (member->getClass() == CLASS_WARRIOR || 
                member->getClass() == CLASS_PALADIN || 
                member->getClass() == CLASS_ROGUE ||
                member->getClass() == CLASS_DEATH_KNIGHT)
                return true;
        }
    }
    
    return false;
}

bool PlayerbotShamanStrategy::ShouldUseEarthElemental() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !IsInCombat())
        return false;
    
    return bot->GetHealthPct() < 30.0f || CountNearbyEnemies(10.0f) >= 3;
}

bool PlayerbotShamanStrategy::ShouldUseFireElemental() const
{
    Player* bot = _ai->GetBot();
    Unit* target = FindBestTarget();
    
    if (!bot || !target)
        return false;
    
    return target->GetHealthPct() > 50.0f && IsInCombat();
}

uint32 PlayerbotShamanStrategy::GetManaAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_MANA) : 0;
}

uint32 PlayerbotShamanStrategy::GetManaPercent() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 currentMana = bot->GetPower(POWER_MANA);
    uint32 maxMana = bot->GetMaxPower(POWER_MANA);
    
    return maxMana > 0 ? (currentMana * 100 / maxMana) : 0;
}

uint32 PlayerbotShamanStrategy::GetMaelstromPower() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_MAELSTROM) : 0;
}

uint32 PlayerbotShamanStrategy::GetMaelstromWeaponStacks() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    Aura* aura = bot->GetAura(ShamanSpells::MAELSTROM_WEAPON);
    return aura ? aura->GetStackAmount() : 0;
}

bool PlayerbotShamanStrategy::HasMaelstromWeaponStacks(uint32 minStacks) const
{
    return GetMaelstromWeaponStacks() >= minStacks;
}

bool PlayerbotShamanStrategy::HasMaxMaelstromWeaponStacks() const
{
    return GetMaelstromWeaponStacks() >= 5;
}

bool PlayerbotShamanStrategy::ShouldSpendMaelstromWeapon() const
{
    return GetMaelstromWeaponStacks() >= 5;
}

bool PlayerbotShamanStrategy::ShouldSpendMaelstrom() const
{
    return GetMaelstromPower() >= 60;
}

bool PlayerbotShamanStrategy::isElemental() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(ShamanSpells::LAVA_BURST);
}

bool PlayerbotShamanStrategy::isEnhancement() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(ShamanSpells::STORMSTRIKE);
}

bool PlayerbotShamanStrategy::isRestoration() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(ShamanSpells::RIPTIDE);
}

Unit* PlayerbotShamanStrategy::FindBestTarget() const
{
    Unit* currentTarget = _ai->GetTarget();
    if (currentTarget && currentTarget->IsAlive() && _ai->GetBot()->IsValidAttackTarget(currentTarget))
        return currentTarget;
    
    Unit* target = _ai->GetBot()->GetVictim();
    if (target && target->IsAlive())
        return target;
    
    return nullptr;
}

Unit* PlayerbotShamanStrategy::FindBestHealingTarget() const
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

bool PlayerbotShamanStrategy::IsInCombat() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->IsInCombat();
}

bool PlayerbotShamanStrategy::IsInMeleeRange(Unit* target) const
{
    Player* bot = _ai->GetBot();
    return bot && target && bot->IsWithinMeleeRange(target);
}

uint32 PlayerbotShamanStrategy::CountNearbyEnemies(float range) const
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

uint32 PlayerbotShamanStrategy::CountInjuredGroupMembers(float threshold) const
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

bool PlayerbotShamanStrategy::ShouldUseAoE() const
{
    return CountNearbyEnemies(8.0f) >= 3;
}

bool PlayerbotShamanStrategy::ShouldUseAoEHealing() const
{
    return CountInjuredGroupMembers(0.7f) >= 3;
}

bool PlayerbotShamanStrategy::HasFlameShock(Unit* target) const
{
    return target && target->HasAura(ShamanSpells::FLAME_SHOCK, _ai->GetBot()->GetGUID());
}

bool PlayerbotShamanStrategy::ShouldRefreshFlameShock(Unit* target) const
{
    if (!HasFlameShock(target))
        return true;
    
    if (Aura* aura = target->GetAura(ShamanSpells::FLAME_SHOCK, _ai->GetBot()->GetGUID()))
    {
        float remaining = float(aura->GetDuration()) / 1000.0f;
        return remaining <= 6.0f;
    }
    
    return true;
}

bool PlayerbotShamanStrategy::HasLavaSurgeProc() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(ShamanSpells::LAVA_SURGE);
}

bool PlayerbotShamanStrategy::HasTidalWaves() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(ShamanSpells::TIDAL_WAVES);
}

bool PlayerbotShamanStrategy::HasMasterOfElementsBuff() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(ShamanSpells::MASTER_OF_THE_ELEMENTS);
}

bool PlayerbotShamanStrategy::ShouldUseMasterOfElements() const
{
    return HasMasterOfElementsBuff();
}

// Elemental Shaman Strategy
PlayerbotElementalShamanStrategy::PlayerbotElementalShamanStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotShamanStrategy(ai, "elemental") {}

void PlayerbotElementalShamanStrategy::InitializeActions()
{
    PlayerbotShamanStrategy::InitializeActions();
    RegisterElementalActions();
}

void PlayerbotElementalShamanStrategy::InitializeTriggers()
{
    PlayerbotShamanStrategy::InitializeTriggers();
    RegisterElementalTriggers();
}

void PlayerbotElementalShamanStrategy::RegisterElementalActions()
{
    RegisterAction("lightning bolt", std::make_unique<CastLightningBolt>(_ai));
    RegisterAction("chain lightning", std::make_unique<CastChainLightning>(_ai));
    RegisterAction("lava burst", std::make_unique<CastLavaBurst>(_ai));
    RegisterAction("flame shock", std::make_unique<CastFlameShock>(_ai));
    RegisterAction("earth shock", std::make_unique<CastEarthShock>(_ai));
    RegisterAction("elemental blast", std::make_unique<CastElementalBlast>(_ai));
    RegisterAction("earthquake", std::make_unique<CastEarthquake>(_ai));
    RegisterAction("icefury", std::make_unique<CastIcefury>(_ai));
    RegisterAction("frost shock", std::make_unique<CastFrostShock>(_ai));
    RegisterAction("storm elemental", std::make_unique<CastStormElemental>(_ai));
    RegisterAction("stormkeeper", std::make_unique<CastStormkeeper>(_ai));
    RegisterAction("ascendance elemental", std::make_unique<CastAscendanceElemental>(_ai));
    RegisterAction("primordial wave", std::make_unique<CastPrimordialWave>(_ai));
}

void PlayerbotElementalShamanStrategy::RegisterElementalTriggers()
{
    // TODO: Implement when trigger system is available
}

float PlayerbotElementalShamanStrategy::GetActionPriority(std::string const& actionName) const
{
    // Check parent priorities first
    float parentPriority = PlayerbotShamanStrategy::GetActionPriority(actionName);
    if (parentPriority > 0.0f)
        return parentPriority;
    
    // Elemental rotation priorities
    if (actionName == "ascendance elemental" && ShouldUseAscendance())
        return 0.95f;
    else if (actionName == "storm elemental" && ShouldUseStormElemental())
        return 0.93f;
    else if (actionName == "stormkeeper" && ShouldUseStormkeeper())
        return 0.91f;
    else if (actionName == "primordial wave" && ShouldUsePrimordialWave())
        return 0.89f;
    else if (actionName == "flame shock" && ShouldUseFlameShock())
        return 0.87f;
    else if (actionName == "lava burst" && ShouldUseLavaBurst())
        return 0.85f;
    else if (actionName == "elemental blast" && ShouldUseElementalBlast())
        return 0.83f;
    else if (actionName == "earth shock" && ShouldUseEarthShock())
        return 0.81f;
    else if (actionName == "earthquake" && ShouldUseEarthquake())
        return 0.79f;
    else if (actionName == "chain lightning" && ShouldUseChainLightning())
        return 0.77f;
    else if (actionName == "icefury" && ShouldUseIcefury())
        return 0.75f;
    else if (actionName == "frost shock" && ShouldUseFrostShock())
        return 0.73f;
    else if (actionName == "lightning bolt" && ShouldUseLightningBolt())
        return 0.7f;
    
    return 0.0f;
}

bool PlayerbotElementalShamanStrategy::ShouldUseFlameShock() const
{
    Unit* target = FindBestTarget();
    if (!target)
        return false;
    
    return ShouldRefreshFlameShock(target);
}

bool PlayerbotElementalShamanStrategy::ShouldUseLavaBurst() const
{
    Unit* target = FindBestTarget();
    if (!target)
        return false;
    
    Player* bot = _ai->GetBot();
    return bot && (HasFlameShock(target) || HasLavaSurgeProc());
}

bool PlayerbotElementalShamanStrategy::ShouldUseLightningBolt() const
{
    Unit* target = FindBestTarget();
    return target && !ShouldUseAoE();
}

bool PlayerbotElementalShamanStrategy::ShouldUseChainLightning() const
{
    Unit* target = FindBestTarget();
    return target && IsInAoESituation();
}

bool PlayerbotElementalShamanStrategy::ShouldUseEarthShock() const
{
    Unit* target = FindBestTarget();
    if (!target)
        return false;
    
    return ShouldPrioritizeMaelstromSpending() && !ShouldUseMasterOfElements();
}

bool PlayerbotElementalShamanStrategy::ShouldUseElementalBlast() const
{
    Unit* target = FindBestTarget();
    if (!target)
        return false;
    
    return ShouldPrioritizeMaelstromSpending() || ShouldUseMasterOfElements();
}

bool PlayerbotElementalShamanStrategy::ShouldUseEarthquake() const
{
    return IsInAoESituation() && ShouldPrioritizeMaelstromSpending();
}

bool PlayerbotElementalShamanStrategy::ShouldUseIcefury() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return bot->HasSpell(ShamanSpells::ICEFURY) &&
           !bot->GetSpellHistory()->HasCooldown(ShamanSpells::ICEFURY);
}

bool PlayerbotElementalShamanStrategy::ShouldUseFrostShock() const
{
    Unit* target = FindBestTarget();
    if (!target)
        return false;
    
    return HasIcefuryBuff() || !IsInMeleeRange(target);
}

bool PlayerbotElementalShamanStrategy::ShouldUseStormkeeper() const
{
    Player* bot = _ai->GetBot();
    Unit* target = FindBestTarget();
    
    if (!bot || !target)
        return false;
    
    return ShouldUseBurstCooldowns() &&
           bot->HasSpell(ShamanSpells::STORMKEEPER) &&
           !bot->GetSpellHistory()->HasCooldown(ShamanSpells::STORMKEEPER);
}

bool PlayerbotElementalShamanStrategy::ShouldUseAscendance() const
{
    Player* bot = _ai->GetBot();
    Unit* target = FindBestTarget();
    
    if (!bot || !target)
        return false;
    
    return ShouldUseBurstCooldowns() &&
           bot->HasSpell(ShamanSpells::ASCENDANCE_ELEMENTAL) &&
           !bot->GetSpellHistory()->HasCooldown(ShamanSpells::ASCENDANCE_ELEMENTAL);
}

bool PlayerbotElementalShamanStrategy::ShouldUseStormElemental() const
{
    Player* bot = _ai->GetBot();
    Unit* target = FindBestTarget();
    
    if (!bot || !target)
        return false;
    
    return ShouldUseBurstCooldowns() &&
           bot->HasSpell(ShamanSpells::STORM_ELEMENTAL) &&
           !bot->GetSpellHistory()->HasCooldown(ShamanSpells::STORM_ELEMENTAL);
}

bool PlayerbotElementalShamanStrategy::ShouldUsePrimordialWave() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return bot->HasSpell(ShamanSpells::PRIMORDIAL_WAVE) &&
           !bot->GetSpellHistory()->HasCooldown(ShamanSpells::PRIMORDIAL_WAVE);
}

bool PlayerbotElementalShamanStrategy::ShouldUseBurstCooldowns() const
{
    Unit* target = FindBestTarget();
    return target && target->GetHealthPct() > (BURST_COOLDOWN_HEALTH * 100.0f);
}

bool PlayerbotElementalShamanStrategy::IsInAoESituation() const
{
    return CountNearbyEnemies(8.0f) >= AOE_TARGET_COUNT;
}

bool PlayerbotElementalShamanStrategy::ShouldPrioritizeMaelstromSpending() const
{
    return GetMaelstromPower() >= MAELSTROM_SPENDER_THRESHOLD;
}

bool PlayerbotElementalShamanStrategy::HasIcefuryBuff() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(ShamanSpells::ICEFURY);
}

bool PlayerbotElementalShamanStrategy::ShouldConserveMana() const
{
    return GetManaPercent() < (MANA_CONSERVATION_THRESHOLD * 100.0f);
}

bool PlayerbotElementalShamanStrategy::CanCastExpensiveSpell() const
{
    return !ShouldConserveMana();
}

// Enhancement Shaman Strategy
PlayerbotEnhancementShamanStrategy::PlayerbotEnhancementShamanStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotShamanStrategy(ai, "enhancement") {}

void PlayerbotEnhancementShamanStrategy::InitializeActions()
{
    PlayerbotShamanStrategy::InitializeActions();
    RegisterEnhancementActions();
}

void PlayerbotEnhancementShamanStrategy::InitializeTriggers()
{
    PlayerbotShamanStrategy::InitializeTriggers();
    RegisterEnhancementTriggers();
}

void PlayerbotEnhancementShamanStrategy::RegisterEnhancementActions()
{
    RegisterAction("stormstrike", std::make_unique<CastStormstrike>(_ai));
    RegisterAction("lava lash", std::make_unique<CastLavaLash>(_ai));
    RegisterAction("crash lightning", std::make_unique<CastCrashLightning>(_ai));
    RegisterAction("sundering", std::make_unique<CastSundering>(_ai));
    RegisterAction("ice strike", std::make_unique<CastIceStrike>(_ai));
    RegisterAction("lightning bolt enh", std::make_unique<CastLightningBolt>(_ai));
    RegisterAction("chain lightning enh", std::make_unique<CastChainLightning>(_ai));
    RegisterAction("elemental blast enh", std::make_unique<CastElementalBlast>(_ai));
    RegisterAction("doom winds", std::make_unique<CastDoomWinds>(_ai));
    RegisterAction("feral spirit", std::make_unique<CastFeralSpirit>(_ai));
    RegisterAction("ascendance enhancement", std::make_unique<CastAscendanceEnhancement>(_ai));
    RegisterAction("windfury weapon", std::make_unique<CastWindfuryWeapon>(_ai));
    RegisterAction("flametongue weapon", std::make_unique<CastFlametongueWeapon>(_ai));
    RegisterAction("primordial wave enh", std::make_unique<CastPrimordialWave>(_ai));
}

void PlayerbotEnhancementShamanStrategy::RegisterEnhancementTriggers()
{
    // TODO: Implement when trigger system is available
}

float PlayerbotEnhancementShamanStrategy::GetActionPriority(std::string const& actionName) const
{
    // Check parent priorities first
    float parentPriority = PlayerbotShamanStrategy::GetActionPriority(actionName);
    if (parentPriority > 0.0f)
        return parentPriority;
    
    // Enhancement rotation priorities
    if (actionName == "windfury weapon" && ShouldUseWindfuryWeapon())
        return 0.98f;
    else if (actionName == "flametongue weapon" && ShouldUseFlametongueWeapon())
        return 0.96f;
    else if (actionName == "doom winds" && ShouldUseDoomWinds())
        return 0.94f;
    else if (actionName == "feral spirit" && ShouldUseFeralSpirit())
        return 0.92f;
    else if (actionName == "ascendance enhancement" && ShouldUseAscendance())
        return 0.9f;
    else if (actionName == "primordial wave enh" && ShouldUsePrimordialWave())
        return 0.88f;
    else if (actionName == "lightning bolt enh" && ShouldUseInstantLightningBolt())
        return 0.86f;
    else if (actionName == "chain lightning enh" && ShouldUseInstantChainLightning())
        return 0.84f;
    else if (actionName == "elemental blast enh" && ShouldUseElementalBlast())
        return 0.82f;
    else if (actionName == "sundering" && ShouldUseSundering())
        return 0.8f;
    else if (actionName == "stormstrike" && ShouldUseStormstrike())
        return 0.78f;
    else if (actionName == "ice strike" && ShouldUseIceStrike())
        return 0.76f;
    else if (actionName == "lava lash" && ShouldUseLavaLash())
        return 0.74f;
    else if (actionName == "crash lightning" && ShouldUseCrashLightning())
        return 0.72f;
    
    return 0.0f;
}

bool PlayerbotEnhancementShamanStrategy::ShouldUseStormstrike() const
{
    Unit* target = FindBestTarget();
    return target && IsInMeleeRange(target);
}

bool PlayerbotEnhancementShamanStrategy::ShouldUseLavaLash() const
{
    Unit* target = FindBestTarget();
    if (!target || !IsInMeleeRange(target))
        return false;
    
    return HasHotHandProc() || HasFlametongueWeapon();
}

bool PlayerbotEnhancementShamanStrategy::ShouldUseCrashLightning() const
{
    return ShouldUseAoEAbilities();
}

bool PlayerbotEnhancementShamanStrategy::ShouldUseSundering() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !IsInMeleeRange(target))
        return false;
    
    return bot->HasSpell(ShamanSpells::SUNDERING) &&
           !bot->GetSpellHistory()->HasCooldown(ShamanSpells::SUNDERING);
}

bool PlayerbotEnhancementShamanStrategy::ShouldUseIceStrike() const
{
    Unit* target = FindBestTarget();
    return target && IsInMeleeRange(target);
}

bool PlayerbotEnhancementShamanStrategy::ShouldUseInstantLightningBolt() const
{
    Unit* target = FindBestTarget();
    return target && ShouldPrioritizeMaelstromSpending() && !ShouldUseAoEAbilities();
}

bool PlayerbotEnhancementShamanStrategy::ShouldUseInstantChainLightning() const
{
    Unit* target = FindBestTarget();
    return target && ShouldPrioritizeMaelstromSpending() && ShouldUseAoEAbilities();
}

bool PlayerbotEnhancementShamanStrategy::ShouldUseElementalBlast() const
{
    Unit* target = FindBestTarget();
    return target && ShouldPrioritizeMaelstromSpending();
}

bool PlayerbotEnhancementShamanStrategy::ShouldUseDoomWinds() const
{
    Player* bot = _ai->GetBot();
    Unit* target = FindBestTarget();
    
    if (!bot || !target)
        return false;
    
    return ShouldUseBurstCooldowns() &&
           bot->HasSpell(ShamanSpells::DOOM_WINDS) &&
           !bot->GetSpellHistory()->HasCooldown(ShamanSpells::DOOM_WINDS);
}

bool PlayerbotEnhancementShamanStrategy::ShouldUseFeralSpirit() const
{
    Player* bot = _ai->GetBot();
    Unit* target = FindBestTarget();
    
    if (!bot || !target)
        return false;
    
    return ShouldUseBurstCooldowns() &&
           bot->HasSpell(ShamanSpells::FERAL_SPIRIT) &&
           !bot->GetSpellHistory()->HasCooldown(ShamanSpells::FERAL_SPIRIT);
}

bool PlayerbotEnhancementShamanStrategy::ShouldUseAscendance() const
{
    Player* bot = _ai->GetBot();
    Unit* target = FindBestTarget();
    
    if (!bot || !target)
        return false;
    
    return ShouldUseBurstCooldowns() &&
           bot->HasSpell(ShamanSpells::ASCENDANCE_ENHANCEMENT) &&
           !bot->GetSpellHistory()->HasCooldown(ShamanSpells::ASCENDANCE_ENHANCEMENT);
}

bool PlayerbotEnhancementShamanStrategy::ShouldUsePrimordialWave() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return bot->HasSpell(ShamanSpells::PRIMORDIAL_WAVE_ENH) &&
           !bot->GetSpellHistory()->HasCooldown(ShamanSpells::PRIMORDIAL_WAVE_ENH);
}

bool PlayerbotEnhancementShamanStrategy::ShouldUseBurstCooldowns() const
{
    Unit* target = FindBestTarget();
    return target && target->GetHealthPct() > (BURST_COOLDOWN_HEALTH * 100.0f);
}

bool PlayerbotEnhancementShamanStrategy::ShouldUseAoEAbilities() const
{
    return CountNearbyEnemies(8.0f) >= AOE_TARGET_COUNT;
}

bool PlayerbotEnhancementShamanStrategy::ShouldPrioritizeMaelstromSpending() const
{
    return GetMaelstromWeaponStacks() >= MAELSTROM_WEAPON_SPEND_THRESHOLD;
}

bool PlayerbotEnhancementShamanStrategy::ShouldUseWindfuryWeapon() const
{
    return !HasWindfuryWeapon();
}

bool PlayerbotEnhancementShamanStrategy::ShouldUseFlametongueWeapon() const
{
    return !HasFlametongueWeapon();
}

bool PlayerbotEnhancementShamanStrategy::HasWindfuryWeapon() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(ShamanSpells::WINDFURY_WEAPON);
}

bool PlayerbotEnhancementShamanStrategy::HasFlametongueWeapon() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(ShamanSpells::FLAMETONGUE_WEAPON);
}

bool PlayerbotEnhancementShamanStrategy::HasHotHandProc() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(ShamanSpells::HOT_HAND);
}

bool PlayerbotEnhancementShamanStrategy::HasStormsurgeProc() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(ShamanSpells::STORMSURGE);
}

bool PlayerbotEnhancementShamanStrategy::IsInDoomWinds() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(ShamanSpells::DOOM_WINDS);
}

bool PlayerbotEnhancementShamanStrategy::NeedsWeaponImbues() const
{
    return !HasWindfuryWeapon() || !HasFlametongueWeapon();
}

// Restoration Shaman Strategy
PlayerbotRestorationShamanStrategy::PlayerbotRestorationShamanStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotShamanStrategy(ai, "restoration") {}

void PlayerbotRestorationShamanStrategy::InitializeActions()
{
    PlayerbotShamanStrategy::InitializeActions();
    RegisterRestorationActions();
}

void PlayerbotRestorationShamanStrategy::InitializeTriggers()
{
    PlayerbotShamanStrategy::InitializeTriggers();
    RegisterRestorationTriggers();
}

void PlayerbotRestorationShamanStrategy::RegisterRestorationActions()
{
    RegisterAction("healing wave", std::make_unique<CastHealingWave>(_ai));
    RegisterAction("healing surge", std::make_unique<CastHealingSurge>(_ai));
    RegisterAction("chain heal", std::make_unique<CastChainHeal>(_ai));
    RegisterAction("riptide", std::make_unique<CastRiptide>(_ai));
    RegisterAction("healing rain", std::make_unique<CastHealingRain>(_ai));
    RegisterAction("healing stream totem", std::make_unique<CastHealingStreamTotem>(_ai));
    RegisterAction("spirit link totem", std::make_unique<CastSpiritLinkTotem>(_ai));
    RegisterAction("healing tide totem", std::make_unique<CastHealingTideTotem>(_ai));
    RegisterAction("ancestral guidance", std::make_unique<CastAncestralGuidance>(_ai));
    RegisterAction("ascendance restoration", std::make_unique<CastAscendanceRestoration>(_ai));
    RegisterAction("earth shield", std::make_unique<CastEarthShield>(_ai));
    RegisterAction("primordial wave resto", std::make_unique<CastPrimordialWave>(_ai));
    
    // DPS abilities for when healing not needed
    RegisterAction("lightning bolt resto", std::make_unique<CastLightningBolt>(_ai));
    RegisterAction("flame shock resto", std::make_unique<CastFlameShock>(_ai));
    RegisterAction("lava burst resto", std::make_unique<CastLavaBurst>(_ai));
}

void PlayerbotRestorationShamanStrategy::RegisterRestorationTriggers()
{
    // TODO: Implement when trigger system is available
}

float PlayerbotRestorationShamanStrategy::GetActionPriority(std::string const& actionName) const
{
    // Check parent priorities first
    float parentPriority = PlayerbotShamanStrategy::GetActionPriority(actionName);
    if (parentPriority > 0.0f)
        return parentPriority;
    
    // Restoration healing priorities
    if (actionName == "healing surge" && NeedsEmergencyHealing())
        return 0.98f;
    else if (actionName == "spirit link totem" && ShouldUseSpiritLinkTotem())
        return 0.96f;
    else if (actionName == "healing tide totem" && ShouldUseHealingTideTotem())
        return 0.94f;
    else if (actionName == "ascendance restoration" && ShouldUseAscendance())
        return 0.92f;
    else if (actionName == "ancestral guidance" && ShouldUseAncestralGuidance())
        return 0.9f;
    else if (actionName == "chain heal" && ShouldUseChainHeal())
        return 0.88f;
    else if (actionName == "healing rain" && ShouldUseHealingRain())
        return 0.86f;
    else if (actionName == "riptide" && ShouldUseRiptide())
        return 0.84f;
    else if (actionName == "healing wave" && ShouldUseHealingWave())
        return 0.82f;
    else if (actionName == "earth shield" && ShouldUseEarthShield())
        return 0.8f;
    else if (actionName == "healing stream totem" && ShouldUseHealingStreamTotem())
        return 0.78f;
    else if (actionName == "primordial wave resto" && ShouldUsePrimordialWave())
        return 0.76f;
    
    // DPS priorities when healing not needed
    else if (actionName == "flame shock resto" && ShouldDPS() && ShouldUseFlameShock())
        return 0.4f;
    else if (actionName == "lava burst resto" && ShouldDPS() && ShouldUseLavaBurst())
        return 0.38f;
    else if (actionName == "lightning bolt resto" && ShouldDPS() && ShouldUseLightningBolt())
        return 0.36f;
    
    return 0.0f;
}

bool PlayerbotRestorationShamanStrategy::ShouldUseRiptide() const
{
    return FindBestRiptideTarget() != nullptr;
}

bool PlayerbotRestorationShamanStrategy::ShouldUseChainHeal() const
{
    return ShouldUseGroupHealing() && CountMembersNeedingHealing(CHAIN_HEAL_THRESHOLD) >= MIN_CHAIN_HEAL_TARGETS;
}

bool PlayerbotRestorationShamanStrategy::ShouldUseHealingSurge() const
{
    Unit* target = FindBestHealingTarget();
    return target && (target->GetHealthPct() < (EMERGENCY_THRESHOLD * 100.0f) || HasTidalWavesBuff());
}

bool PlayerbotRestorationShamanStrategy::ShouldUseHealingWave() const
{
    Unit* target = FindBestHealingTarget();
    return target && HasTidalWavesBuff() && !NeedsEmergencyHealing();
}

bool PlayerbotRestorationShamanStrategy::ShouldUseHealingRain() const
{
    return ShouldUseGroupHealing() && CountMembersNeedingHealing(0.8f) >= MIN_AOE_HEAL_TARGETS && IsGroupStacked();
}

bool PlayerbotRestorationShamanStrategy::ShouldUseHealingStreamTotem() const
{
    return !HasActiveHealingTotem() && (IsInCombat() || CountMembersNeedingHealing(0.9f) >= 2);
}

bool PlayerbotRestorationShamanStrategy::ShouldUseSpiritLinkTotem() const
{
    return CountMembersNeedingHealing(SPIRIT_LINK_THRESHOLD) >= 2;
}

bool PlayerbotRestorationShamanStrategy::ShouldUseHealingTideTotem() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    return ShouldUseMajorCooldowns() && CountMembersNeedingHealing(0.6f) >= 3 &&
           bot->HasSpell(ShamanSpells::HEALING_TIDE_TOTEM) &&
           !bot->GetSpellHistory()->HasCooldown(ShamanSpells::HEALING_TIDE_TOTEM);
}

bool PlayerbotRestorationShamanStrategy::ShouldUseAncestralGuidance() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    return ShouldUseMajorCooldowns() && CountMembersNeedingHealing(0.7f) >= 3 &&
           bot->HasSpell(ShamanSpells::ANCESTRAL_GUIDANCE) &&
           !bot->GetSpellHistory()->HasCooldown(ShamanSpells::ANCESTRAL_GUIDANCE);
}

bool PlayerbotRestorationShamanStrategy::ShouldUseAscendance() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    return ShouldUseMajorCooldowns() && CountMembersNeedingHealing(0.5f) >= 2 &&
           bot->HasSpell(ShamanSpells::ASCENDANCE_RESTORATION) &&
           !bot->GetSpellHistory()->HasCooldown(ShamanSpells::ASCENDANCE_RESTORATION);
}

bool PlayerbotRestorationShamanStrategy::ShouldUseEarthShield() const
{
    return FindEarthShieldTarget() != nullptr;
}

bool PlayerbotRestorationShamanStrategy::ShouldUsePrimordialWave() const
{
    Player* bot = _ai->GetBot();
    Unit* target = FindBestHealingTarget();
    
    if (!bot || !target)
        return false;
    
    return bot->HasSpell(ShamanSpells::PRIMORDIAL_WAVE_RESTO) &&
           !bot->GetSpellHistory()->HasCooldown(ShamanSpells::PRIMORDIAL_WAVE_RESTO);
}

bool PlayerbotRestorationShamanStrategy::NeedsEmergencyHealing() const
{
    Unit* target = FindBestHealingTarget();
    return target && target->GetHealthPct() < (EMERGENCY_THRESHOLD * 100.0f);
}

bool PlayerbotRestorationShamanStrategy::ShouldUseGroupHealing() const
{
    return CountMembersNeedingHealing(HEALING_THRESHOLD) >= MIN_CHAIN_HEAL_TARGETS;
}

bool PlayerbotRestorationShamanStrategy::ShouldUseMajorCooldowns() const
{
    return IsInCombat() && (NeedsEmergencyHealing() || ShouldUseGroupHealing());
}

Unit* PlayerbotRestorationShamanStrategy::FindBestRiptideTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;
    
    Group* group = bot->GetGroup();
    if (!group)
        return (bot->GetHealthPct() < 90.0f && !bot->HasAura(ShamanSpells::RIPTIDE)) ? bot : nullptr;
    
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && bot->IsWithinDistInMap(member, 40.0f))
        {
            if (member->GetHealthPct() < (HEALING_THRESHOLD * 100.0f) && 
                !member->HasAura(ShamanSpells::RIPTIDE, bot->GetGUID()))
                return member;
        }
    }
    
    return nullptr;
}

Unit* PlayerbotRestorationShamanStrategy::FindBestChainHealTarget() const
{
    // Chain Heal works best when cast on a target that is grouped with others
    Unit* bestTarget = FindBestHealingTarget();
    return bestTarget;
}

Unit* PlayerbotRestorationShamanStrategy::FindEarthShieldTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;
    
    Group* group = bot->GetGroup();
    if (!group)
        return (!bot->HasAura(ShamanSpells::EARTH_SHIELD)) ? bot : nullptr;
    
    // Find tank or member taking most damage
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && bot->IsWithinDistInMap(member, 40.0f))
        {
            if (!member->HasAura(ShamanSpells::EARTH_SHIELD, bot->GetGUID()) &&
                (member->HasAuraType(SPELL_AURA_MOD_THREAT) || member->GetHealthPct() < 85.0f))
                return member;
        }
    }
    
    return nullptr;
}

uint32 PlayerbotRestorationShamanStrategy::CountMembersNeedingHealing(float threshold) const
{
    return CountInjuredGroupMembers(threshold);
}

bool PlayerbotRestorationShamanStrategy::IsGroupStacked() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    Group* group = bot->GetGroup();
    if (!group)
        return false;
    
    uint32 nearbyMembers = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && bot->IsWithinDistInMap(member, 15.0f))
            nearbyMembers++;
    }
    
    return nearbyMembers >= 3;
}

bool PlayerbotRestorationShamanStrategy::HasTidalWavesBuff() const
{
    return HasTidalWaves();
}

uint32 PlayerbotRestorationShamanStrategy::GetTidalWavesStacks() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    Aura* aura = bot->GetAura(ShamanSpells::TIDAL_WAVES);
    return aura ? aura->GetStackAmount() : 0;
}

bool PlayerbotRestorationShamanStrategy::ShouldUseTidalWavesHealing() const
{
    return HasTidalWavesBuff() && GetTidalWavesStacks() > 0;
}

bool PlayerbotRestorationShamanStrategy::HasActiveHealingTotem() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(ShamanSpells::HEALING_STREAM_TOTEM);
}

bool PlayerbotRestorationShamanStrategy::ShouldDPS() const
{
    return CountMembersNeedingHealing(0.9f) == 0 && GetManaPercent() > 50;
}

bool PlayerbotRestorationShamanStrategy::ShouldUseLightningBolt() const
{
    Unit* target = FindBestTarget();
    return target != nullptr;
}

bool PlayerbotRestorationShamanStrategy::ShouldUseFlameShock() const
{
    Unit* target = FindBestTarget();
    return target && ShouldRefreshFlameShock(target);
}

bool PlayerbotRestorationShamanStrategy::ShouldUseLavaBurst() const
{
    Unit* target = FindBestTarget();
    return target && HasFlameShock(target);
}