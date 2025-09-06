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

#include "PlayerbotDeathKnightStrategy.h"
#include "PlayerbotDeathKnightActions.h"
#include "Player.h"
#include "SpellAuraEffects.h"
#include "Group.h"

PlayerbotDeathKnightStrategy::PlayerbotDeathKnightStrategy(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotStrategy(ai, name) {}

void PlayerbotDeathKnightStrategy::InitializeActions()
{
    RegisterCommonActions();
}

void PlayerbotDeathKnightStrategy::InitializeTriggers()
{
    RegisterCommonTriggers();
}

void PlayerbotDeathKnightStrategy::RegisterCommonActions()
{
    // Utility actions
    _actions["death grip"] = std::make_unique<CastDeathGrip>(_ai);
    _actions["death and decay"] = std::make_unique<CastDeathAndDecay>(_ai);
    _actions["anti magic shell"] = std::make_unique<CastAntiMagicShell>(_ai);
    _actions["icebound fortitude"] = std::make_unique<CastIceboundFortitude>(_ai);
    _actions["mind freeze"] = std::make_unique<CastMindFreeze>(_ai);
    _actions["dark command"] = std::make_unique<CastDarkCommand>(_ai);
    _actions["raise dead"] = std::make_unique<CastRaiseDead>(_ai);
}

void PlayerbotDeathKnightStrategy::RegisterCommonTriggers()
{
    // TODO: Implement trigger system when base trigger framework is available
}

float PlayerbotDeathKnightStrategy::GetActionPriority(std::string const& actionName) const
{
    // Common utility priorities
    if (actionName == "mind freeze" && ShouldUseMindFreeze())
        return 0.99f;
    else if (actionName == "dark command" && ShouldUseDarkCommand())
        return 0.97f;
    else if (actionName == "anti magic shell" && ShouldUseAntiMagicShell())
        return 0.95f;
    else if (actionName == "icebound fortitude" && ShouldUseIceboundFortitude())
        return 0.93f;
    else if (actionName == "death grip" && ShouldUseDeathGrip())
        return 0.91f;
    else if (actionName == "death and decay" && ShouldUseDeathAndDecay())
        return 0.89f;
    else if (actionName == "raise dead" && ShouldUseRaiseDead())
        return 0.85f;
    
    return 0.0f;
}

bool PlayerbotDeathKnightStrategy::ShouldUseDeathGrip() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !isBlood() || !IsInCombat())
        return false;
    
    // Look for ranged targets not targeting the tank
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot, 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, check);
    bot->VisitNearbyObject(30.0f, searcher);
    
    for (Unit* unit : targets)
    {
        if (unit->IsAlive() && bot->IsValidAttackTarget(unit) && 
            !bot->IsWithinMeleeRange(unit) && unit->GetVictim() != bot)
            return true;
    }
    
    return false;
}

bool PlayerbotDeathKnightStrategy::ShouldUseDeathAndDecay() const
{
    return CountNearbyEnemies(8.0f) >= 3;
}

bool PlayerbotDeathKnightStrategy::ShouldUseAntiMagicShell() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !IsInCombat())
        return false;
    
    // Use against magical damage or debuffs
    return bot->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE) || bot->GetHealthPct() < 60.0f;
}

bool PlayerbotDeathKnightStrategy::ShouldUseIceboundFortitude() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !IsInCombat())
        return false;
    
    return bot->GetHealthPct() < 40.0f;
}

bool PlayerbotDeathKnightStrategy::ShouldUseMindFreeze() const
{
    Unit* target = FindBestTarget();
    if (!target || !target->IsNonMeleeSpellCast(false))
        return false;
    
    Player* bot = _ai->GetBot();
    return bot && bot->IsWithinMeleeRange(target);
}

bool PlayerbotDeathKnightStrategy::ShouldUseDarkCommand() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !isBlood() || !IsInCombat())
        return false;
    
    Unit* target = FindBestTankTarget();
    return target && ShouldTaunt(target);
}

bool PlayerbotDeathKnightStrategy::ShouldUseRaiseDead() const
{
    Player* bot = _ai->GetBot();
    if (!bot || bot->GetPet() || bot->IsInCombat())
        return false;
    
    return bot->HasSpell(DeathKnightSpells::RAISE_DEAD);
}

uint32 PlayerbotDeathKnightStrategy::GetRuneCount() const
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

uint32 PlayerbotDeathKnightStrategy::GetRunicPowerAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_RUNIC_POWER) : 0;
}

uint32 PlayerbotDeathKnightStrategy::GetRunicPowerPercent() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 currentRP = bot->GetPower(POWER_RUNIC_POWER);
    uint32 maxRP = bot->GetMaxPower(POWER_RUNIC_POWER);
    
    return maxRP > 0 ? (currentRP * 100 / maxRP) : 0;
}

bool PlayerbotDeathKnightStrategy::HasEnoughRunes(uint32 cost) const
{
    return GetRuneCount() >= cost;
}

bool PlayerbotDeathKnightStrategy::HasEnoughRunicPower(uint32 cost) const
{
    return GetRunicPowerAmount() >= cost;
}

bool PlayerbotDeathKnightStrategy::IsRunicPowerCapped() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    return bot->GetPower(POWER_RUNIC_POWER) >= (bot->GetMaxPower(POWER_RUNIC_POWER) - 20);
}

bool PlayerbotDeathKnightStrategy::ShouldConserveRunes() const
{
    return GetRuneCount() <= 2;
}

bool PlayerbotDeathKnightStrategy::isBlood() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(DeathKnightSpells::HEART_STRIKE);
}

bool PlayerbotDeathKnightStrategy::isFrost() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(DeathKnightSpells::OBLITERATE);
}

bool PlayerbotDeathKnightStrategy::isUnholy() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(DeathKnightSpells::FESTERING_STRIKE);
}

Unit* PlayerbotDeathKnightStrategy::FindBestTarget() const
{
    Unit* currentTarget = _ai->GetTarget();
    if (currentTarget && currentTarget->IsAlive() && _ai->GetBot()->IsValidAttackTarget(currentTarget))
        return currentTarget;
    
    Unit* target = _ai->GetBot()->GetVictim();
    if (target && target->IsAlive())
        return target;
    
    return nullptr;
}

Unit* PlayerbotDeathKnightStrategy::FindBestTankTarget() const
{
    // For tanking, prefer targets attacking group members
    Unit* currentTarget = _ai->GetTarget();
    if (currentTarget && currentTarget->IsAlive() && _ai->GetBot()->IsValidAttackTarget(currentTarget))
        return currentTarget;
    
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

bool PlayerbotDeathKnightStrategy::IsInCombat() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->IsInCombat();
}

bool PlayerbotDeathKnightStrategy::IsInMeleeRange(Unit* target) const
{
    Player* bot = _ai->GetBot();
    return bot && target && bot->IsWithinMeleeRange(target);
}

uint32 PlayerbotDeathKnightStrategy::CountNearbyEnemies(float range) const
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

bool PlayerbotDeathKnightStrategy::ShouldUseAoE() const
{
    return CountNearbyEnemies(10.0f) >= 3;
}

bool PlayerbotDeathKnightStrategy::ShouldTaunt(Unit* target) const
{
    Player* bot = _ai->GetBot();
    if (!target || !bot || !isBlood())
        return false;
    
    return target->GetVictim() != bot;
}

// Blood Death Knight Strategy
PlayerbotBloodDeathKnightStrategy::PlayerbotBloodDeathKnightStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotDeathKnightStrategy(ai, "blood") {}

void PlayerbotBloodDeathKnightStrategy::InitializeActions()
{
    PlayerbotDeathKnightStrategy::InitializeActions();
    RegisterBloodActions();
}

void PlayerbotBloodDeathKnightStrategy::InitializeTriggers()
{
    PlayerbotDeathKnightStrategy::InitializeTriggers();
    RegisterBloodTriggers();
}

void PlayerbotBloodDeathKnightStrategy::RegisterBloodActions()
{
    _actions["heart strike"] = std::make_unique<CastHeartStrike>(_ai);
    _actions["blood boil"] = std::make_unique<CastBloodBoil>(_ai);
    _actions["death strike"] = std::make_unique<CastDeathStrike>(_ai);
    _actions["marrowrend"] = std::make_unique<CastMarrowrend>(_ai);
    _actions["vampiric blood"] = std::make_unique<CastVampiricBlood>(_ai);
    _actions["dancing rune weapon"] = std::make_unique<CastDancingRuneWeapon>(_ai);
    _actions["tombstone"] = std::make_unique<CastTombstone>(_ai);
    _actions["bonestorm"] = std::make_unique<CastBonestorm>(_ai);
}

void PlayerbotBloodDeathKnightStrategy::RegisterBloodTriggers()
{
    // TODO: Implement when trigger system is available
}

float PlayerbotBloodDeathKnightStrategy::GetActionPriority(std::string const& actionName) const
{
    // Check parent priorities first
    float parentPriority = PlayerbotDeathKnightStrategy::GetActionPriority(actionName);
    if (parentPriority > 0.0f)
        return parentPriority;
    
    // Blood rotation priorities
    if (actionName == "marrowrend" && ShouldUseMarrowrend())
        return 0.95f;
    else if (actionName == "death strike" && ShouldUseDeathStrike())
        return 0.9f;
    else if (actionName == "vampiric blood" && ShouldUseVampiricBlood())
        return 0.88f;
    else if (actionName == "dancing rune weapon" && ShouldUseDancingRuneWeapon())
        return 0.86f;
    else if (actionName == "tombstone" && ShouldUseTombstone())
        return 0.84f;
    else if (actionName == "bonestorm" && ShouldUseBonestorm())
        return 0.82f;
    else if (actionName == "blood boil" && ShouldUseBloodBoil())
        return 0.8f;
    else if (actionName == "heart strike" && ShouldUseHeartStrike())
        return 0.75f;
    
    return 0.0f;
}

bool PlayerbotBloodDeathKnightStrategy::ShouldUseMarrowrend() const
{
    Unit* target = FindBestTankTarget();
    if (!target || !IsInMeleeRange(target))
        return false;
    
    return NeedsBoneShieldRefresh();
}

bool PlayerbotBloodDeathKnightStrategy::ShouldUseHeartStrike() const
{
    Unit* target = FindBestTankTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !IsInMeleeRange(target))
        return false;
    
    return HasEnoughRunes(1) && !ShouldConserveRunes();
}

bool PlayerbotBloodDeathKnightStrategy::ShouldUseBloodBoil() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !HasEnoughRunes(1))
        return false;
    
    return CountNearbyEnemies(10.0f) >= 2 || HasCrimsonScourgeProc();
}

bool PlayerbotBloodDeathKnightStrategy::ShouldUseDeathStrike() const
{
    Unit* target = FindBestTankTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot || !IsInMeleeRange(target))
        return false;
    
    return bot->GetHealthPct() < (DEATH_STRIKE_THRESHOLD * 100.0f) && HasEnoughRunes(1);
}

bool PlayerbotBloodDeathKnightStrategy::ShouldUseVampiricBlood() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    return IsInDanger() && 
           bot->HasSpell(DeathKnightSpells::VAMPIRIC_BLOOD) &&
           !bot->GetSpellHistory()->HasCooldown(DeathKnightSpells::VAMPIRIC_BLOOD);
}

bool PlayerbotBloodDeathKnightStrategy::ShouldUseDancingRuneWeapon() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    return ShouldUseMajorCooldowns() &&
           bot->HasSpell(DeathKnightSpells::DANCING_RUNE_WEAPON) &&
           !bot->GetSpellHistory()->HasCooldown(DeathKnightSpells::DANCING_RUNE_WEAPON);
}

bool PlayerbotBloodDeathKnightStrategy::ShouldUseTombstone() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    return IsInDanger() && GetBoneShieldStacks() >= 6 &&
           bot->HasSpell(DeathKnightSpells::TOMBSTONE) &&
           !bot->GetSpellHistory()->HasCooldown(DeathKnightSpells::TOMBSTONE);
}

bool PlayerbotBloodDeathKnightStrategy::ShouldUseBonestorm() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    return CountNearbyEnemies(8.0f) >= 3 && GetBoneShieldStacks() >= BONESTORM_MIN_STACKS &&
           bot->HasSpell(DeathKnightSpells::BONESTORM) &&
           !bot->GetSpellHistory()->HasCooldown(DeathKnightSpells::BONESTORM);
}

bool PlayerbotBloodDeathKnightStrategy::HasBoneShield() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DeathKnightSpells::BONE_SHIELD);
}

uint32 PlayerbotBloodDeathKnightStrategy::GetBoneShieldStacks() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    Aura* aura = bot->GetAura(DeathKnightSpells::BONE_SHIELD);
    return aura ? aura->GetStackAmount() : 0;
}

bool PlayerbotBloodDeathKnightStrategy::NeedsBoneShieldRefresh() const
{
    return GetBoneShieldStacks() < MIN_BONE_SHIELD_STACKS;
}

bool PlayerbotBloodDeathKnightStrategy::IsInDanger() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetHealthPct() < (DANGER_THRESHOLD * 100.0f);
}

bool PlayerbotBloodDeathKnightStrategy::HasCrimsonScourgeProc() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DeathKnightSpells::CRIMSON_SCOURGE);
}

bool PlayerbotBloodDeathKnightStrategy::ShouldUseMajorCooldowns() const
{
    Unit* target = FindBestTankTarget();
    return target && (IsInDanger() || target->GetHealthPct() > 50.0f);
}

bool PlayerbotBloodDeathKnightStrategy::NeedsThreat() const
{
    Unit* target = FindBestTankTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return target->GetThreatManager().GetThreat(bot) < target->GetThreatManager().GetMaxThreat() * 1.1f;
}

bool PlayerbotBloodDeathKnightStrategy::IsActiveTanking() const
{
    return IsInCombat() && isBlood();
}

// Frost Death Knight Strategy
PlayerbotFrostDeathKnightStrategy::PlayerbotFrostDeathKnightStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotDeathKnightStrategy(ai, "frost") {}

void PlayerbotFrostDeathKnightStrategy::InitializeActions()
{
    PlayerbotDeathKnightStrategy::InitializeActions();
    RegisterFrostActions();
}

void PlayerbotFrostDeathKnightStrategy::InitializeTriggers()
{
    PlayerbotDeathKnightStrategy::InitializeTriggers();
    RegisterFrostTriggers();
}

void PlayerbotFrostDeathKnightStrategy::RegisterFrostActions()
{
    _actions["obliterate"] = std::make_unique<CastObliterate>(_ai);
    _actions["frost strike"] = std::make_unique<CastFrostStrike>(_ai);
    _actions["howling blast"] = std::make_unique<CastHowlingBlast>(_ai);
    _actions["glacial advance"] = std::make_unique<CastGlacialAdvance>(_ai);
    _actions["frostscythe"] = std::make_unique<CastFrostscythe>(_ai);
    _actions["remorseless winter"] = std::make_unique<CastRemorselessWinter>(_ai);
    _actions["pillar of frost"] = std::make_unique<CastPillarOfFrost>(_ai);
    _actions["empower rune weapon"] = std::make_unique<CastEmpowerRuneWeapon>(_ai);
    _actions["breath of sindragosa"] = std::make_unique<CastBreathOfSindragosa>(_ai);
    _actions["frostwyrms fury"] = std::make_unique<CastFrostwyrmsFury>(_ai);
}

void PlayerbotFrostDeathKnightStrategy::RegisterFrostTriggers()
{
    // TODO: Implement when trigger system is available
}

float PlayerbotFrostDeathKnightStrategy::GetActionPriority(std::string const& actionName) const
{
    // Check parent priorities first
    float parentPriority = PlayerbotDeathKnightStrategy::GetActionPriority(actionName);
    if (parentPriority > 0.0f)
        return parentPriority;
    
    // Frost rotation priorities
    if (actionName == "pillar of frost" && ShouldUsePillarOfFrost())
        return 0.95f;
    else if (actionName == "breath of sindragosa" && ShouldUseBreathOfSindragosa())
        return 0.93f;
    else if (actionName == "frostwyrms fury" && ShouldUseFrostwyrmsFury())
        return 0.91f;
    else if (actionName == "obliterate" && ShouldUseObliterate())
        return 0.89f;
    else if (actionName == "frostscythe" && ShouldUseFrostscythe())
        return 0.87f;
    else if (actionName == "howling blast" && ShouldUseHowlingBlast())
        return 0.85f;
    else if (actionName == "frost strike" && ShouldUseFrostStrike())
        return 0.83f;
    else if (actionName == "glacial advance" && ShouldUseGlacialAdvance())
        return 0.81f;
    else if (actionName == "remorseless winter" && ShouldUseRemorselessWinter())
        return 0.79f;
    else if (actionName == "empower rune weapon" && ShouldUseEmpowerRuneWeapon())
        return 0.77f;
    
    return 0.0f;
}

bool PlayerbotFrostDeathKnightStrategy::ShouldUseObliterate() const
{
    Unit* target = FindBestTarget();
    if (!target || !IsInMeleeRange(target) || !HasEnoughRunes(2))
        return false;
    
    // Don't use if AoE is better (Frostscythe)
    if (CountNearbyEnemies(8.0f) >= AOE_TARGET_COUNT)
        return false;
    
    return true;
}

bool PlayerbotFrostDeathKnightStrategy::ShouldUseFrostStrike() const
{
    Unit* target = FindBestTarget();
    if (!target || !HasEnoughRunicPower(FROST_STRIKE_RP_COST))
        return false;
    
    return true;
}

bool PlayerbotFrostDeathKnightStrategy::ShouldUseHowlingBlast() const
{
    Unit* target = FindBestTarget();
    if (!target)
        return false;
    
    // Use with Rime proc or for AoE
    return HasRimeProc() || CountNearbyEnemies(8.0f) >= AOE_TARGET_COUNT;
}

bool PlayerbotFrostDeathKnightStrategy::ShouldUseGlacialAdvance() const
{
    Unit* target = FindBestTarget();
    return target && HasEnoughRunicPower(GLACIAL_ADVANCE_RP_COST);
}

bool PlayerbotFrostDeathKnightStrategy::ShouldUseFrostscythe() const
{
    Unit* target = FindBestTarget();
    if (!target || !IsInMeleeRange(target) || !HasEnoughRunes(2))
        return false;
    
    // Use for AoE situations
    return CountNearbyEnemies(8.0f) >= AOE_TARGET_COUNT;
}

bool PlayerbotFrostDeathKnightStrategy::ShouldUseRemorselessWinter() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    return CountNearbyEnemies(8.0f) >= 2 &&
           bot->HasSpell(DeathKnightSpells::REMORSELESS_WINTER) &&
           !bot->GetSpellHistory()->HasCooldown(DeathKnightSpells::REMORSELESS_WINTER);
}

bool PlayerbotFrostDeathKnightStrategy::ShouldUsePillarOfFrost() const
{
    Player* bot = _ai->GetBot();
    Unit* target = FindBestTarget();
    
    if (!bot || !target)
        return false;
    
    return ShouldUseBurstCooldowns() &&
           bot->HasSpell(DeathKnightSpells::PILLAR_OF_FROST) &&
           !bot->GetSpellHistory()->HasCooldown(DeathKnightSpells::PILLAR_OF_FROST);
}

bool PlayerbotFrostDeathKnightStrategy::ShouldUseEmpowerRuneWeapon() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    return GetRuneCount() <= 2 &&
           bot->HasSpell(DeathKnightSpells::EMPOWER_RUNE_WEAPON) &&
           !bot->GetSpellHistory()->HasCooldown(DeathKnightSpells::EMPOWER_RUNE_WEAPON);
}

bool PlayerbotFrostDeathKnightStrategy::ShouldUseBreathOfSindragosa() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return HasEnoughRunicPower(BREATH_RP_COST) && 
           target->GetHealthPct() > BURST_HEALTH_THRESHOLD * 100.0f &&
           bot->HasSpell(DeathKnightSpells::BREATH_OF_SINDRAGOSA) &&
           !bot->GetSpellHistory()->HasCooldown(DeathKnightSpells::BREATH_OF_SINDRAGOSA);
}

bool PlayerbotFrostDeathKnightStrategy::ShouldUseFrostwyrmsFury() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return ShouldUseBurstCooldowns() &&
           bot->HasSpell(DeathKnightSpells::FROSTWYRMS_FURY) &&
           !bot->GetSpellHistory()->HasCooldown(DeathKnightSpells::FROSTWYRMS_FURY);
}

bool PlayerbotFrostDeathKnightStrategy::HasKillingMachineProc() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DeathKnightSpells::KILLING_MACHINE);
}

bool PlayerbotFrostDeathKnightStrategy::HasRimeProc() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DeathKnightSpells::RIME);
}

bool PlayerbotFrostDeathKnightStrategy::IsInBreathOfSindragosa() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DeathKnightSpells::BREATH_OF_SINDRAGOSA);
}

bool PlayerbotFrostDeathKnightStrategy::ShouldUseBurstCooldowns() const
{
    Unit* target = FindBestTarget();
    return target && (target->GetHealthPct() > BURST_HEALTH_THRESHOLD * 100.0f || IsInCombat());
}

bool PlayerbotFrostDeathKnightStrategy::ShouldUseAoERotation() const
{
    return CountNearbyEnemies(8.0f) >= AOE_TARGET_COUNT;
}

bool PlayerbotFrostDeathKnightStrategy::ShouldSpendRunicPower() const
{
    return IsRunicPowerCapped() || GetRunicPowerPercent() >= 80;
}

bool PlayerbotFrostDeathKnightStrategy::ShouldPrioritizeKillingMachine() const
{
    return HasKillingMachineProc();
}

// Unholy Death Knight Strategy
PlayerbotUnholyDeathKnightStrategy::PlayerbotUnholyDeathKnightStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotDeathKnightStrategy(ai, "unholy") {}

void PlayerbotUnholyDeathKnightStrategy::InitializeActions()
{
    PlayerbotDeathKnightStrategy::InitializeActions();
    RegisterUnholyActions();
}

void PlayerbotUnholyDeathKnightStrategy::InitializeTriggers()
{
    PlayerbotDeathKnightStrategy::InitializeTriggers();
    RegisterUnholyTriggers();
}

void PlayerbotUnholyDeathKnightStrategy::RegisterUnholyActions()
{
    _actions["festering strike"] = std::make_unique<CastFesteringStrike>(_ai);
    _actions["scourge strike"] = std::make_unique<CastScourgeStrike>(_ai);
    _actions["death coil"] = std::make_unique<CastDeathCoil>(_ai);
    _actions["epidemic"] = std::make_unique<CastEpidemic>(_ai);
    _actions["outbreak"] = std::make_unique<CastOutbreak>(_ai);
    _actions["dark transformation"] = std::make_unique<CastDarkTransformation>(_ai);
    _actions["apocalypse"] = std::make_unique<CastApocalypse>(_ai);
    _actions["army of the dead"] = std::make_unique<CastArmyOfTheDead>(_ai);
    _actions["soul reaper"] = std::make_unique<CastSoulReaper>(_ai);
    _actions["defile"] = std::make_unique<CastDefile>(_ai);
}

void PlayerbotUnholyDeathKnightStrategy::RegisterUnholyTriggers()
{
    // TODO: Implement when trigger system is available
}

float PlayerbotUnholyDeathKnightStrategy::GetActionPriority(std::string const& actionName) const
{
    // Check parent priorities first
    float parentPriority = PlayerbotDeathKnightStrategy::GetActionPriority(actionName);
    if (parentPriority > 0.0f)
        return parentPriority;
    
    // Unholy rotation priorities
    if (actionName == "soul reaper" && ShouldUseSoulReaper())
        return 0.95f;
    else if (actionName == "outbreak" && ShouldUseOutbreak())
        return 0.93f;
    else if (actionName == "army of the dead" && ShouldUseArmyOfTheDead())
        return 0.91f;
    else if (actionName == "apocalypse" && ShouldUseApocalypse())
        return 0.89f;
    else if (actionName == "dark transformation" && ShouldUseDarkTransformation())
        return 0.87f;
    else if (actionName == "death coil" && ShouldUseDeathCoil())
        return 0.85f;
    else if (actionName == "epidemic" && ShouldUseEpidemic())
        return 0.83f;
    else if (actionName == "festering strike" && ShouldUseFesteringStrike())
        return 0.81f;
    else if (actionName == "scourge strike" && ShouldUseScourgeStrike())
        return 0.79f;
    else if (actionName == "defile" && ShouldUseDefile())
        return 0.77f;
    
    return 0.0f;
}

bool PlayerbotUnholyDeathKnightStrategy::ShouldUseOutbreak() const
{
    Unit* target = FindBestTarget();
    if (!target)
        return false;
    
    return NeedsDiseaseRefresh(target);
}

bool PlayerbotUnholyDeathKnightStrategy::ShouldUseFesteringStrike() const
{
    Unit* target = FindBestTarget();
    if (!target || !IsInMeleeRange(target) || !HasEnoughRunes(2))
        return false;
    
    return ShouldApplyFesteringWounds(target);
}

bool PlayerbotUnholyDeathKnightStrategy::ShouldUseScourgeStrike() const
{
    Unit* target = FindBestTarget();
    if (!target || !IsInMeleeRange(target) || !HasEnoughRunes(1))
        return false;
    
    return HasFesteringWounds(target);
}

bool PlayerbotUnholyDeathKnightStrategy::ShouldUseDeathCoil() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return HasEnoughRunicPower(DEATH_COIL_RP_THRESHOLD) || 
           HasSuddenDoomProc() || 
           IsDeathRotExpiring();
}

bool PlayerbotUnholyDeathKnightStrategy::ShouldUseEpidemic() const
{
    return CountNearbyEnemies(8.0f) >= AOE_TARGET_COUNT && 
           HasEnoughRunicPower(EPIDEMIC_RP_COST);
}

bool PlayerbotUnholyDeathKnightStrategy::ShouldUseDarkTransformation() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !bot->GetPet())
        return false;
    
    return ShouldUseBurstCooldowns() &&
           bot->HasSpell(DeathKnightSpells::DARK_TRANSFORMATION) &&
           !bot->GetSpellHistory()->HasCooldown(DeathKnightSpells::DARK_TRANSFORMATION);
}

bool PlayerbotUnholyDeathKnightStrategy::ShouldUseApocalypse() const
{
    Unit* target = FindBestTarget();
    if (!target || !IsInMeleeRange(target))
        return false;
    
    Player* bot = _ai->GetBot();
    return GetFesteringWoundStacks(target) >= MIN_FESTERING_FOR_BURST &&
           bot && bot->HasSpell(DeathKnightSpells::APOCALYPSE) &&
           !bot->GetSpellHistory()->HasCooldown(DeathKnightSpells::APOCALYPSE);
}

bool PlayerbotUnholyDeathKnightStrategy::ShouldUseArmyOfTheDead() const
{
    Player* bot = _ai->GetBot();
    Unit* target = FindBestTarget();
    
    if (!bot || !target)
        return false;
    
    return ShouldUseBurstCooldowns() && target->GetHealthPct() > 80.0f &&
           bot->HasSpell(DeathKnightSpells::ARMY_OF_THE_DEAD) &&
           !bot->GetSpellHistory()->HasCooldown(DeathKnightSpells::ARMY_OF_THE_DEAD);
}

bool PlayerbotUnholyDeathKnightStrategy::ShouldUseSoulReaper() const
{
    Unit* target = FindBestTarget();
    if (!target || !IsInMeleeRange(target))
        return false;
    
    return target->GetHealthPct() < (SOUL_REAPER_THRESHOLD * 100.0f);
}

bool PlayerbotUnholyDeathKnightStrategy::ShouldUseDefile() const
{
    Unit* target = FindBestTarget();
    Player* bot = _ai->GetBot();
    
    if (!target || !bot)
        return false;
    
    return CountNearbyEnemies(8.0f) >= AOE_TARGET_COUNT &&
           bot->HasSpell(DeathKnightSpells::DEFILE) &&
           !bot->GetSpellHistory()->HasCooldown(DeathKnightSpells::DEFILE);
}

bool PlayerbotUnholyDeathKnightStrategy::HasVirulentPlague(Unit* target) const
{
    return target && target->HasAura(DeathKnightSpells::VIRULENT_PLAGUE, _ai->GetBot()->GetGUID());
}

uint32 PlayerbotUnholyDeathKnightStrategy::GetFesteringWoundStacks(Unit* target) const
{
    if (!target)
        return 0;
    
    Aura* aura = target->GetAura(DeathKnightSpells::FESTERING_WOUND, _ai->GetBot()->GetGUID());
    return aura ? aura->GetStackAmount() : 0;
}

bool PlayerbotUnholyDeathKnightStrategy::HasFesteringWounds(Unit* target) const
{
    return GetFesteringWoundStacks(target) > 0;
}

bool PlayerbotUnholyDeathKnightStrategy::HasSuddenDoomProc() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DeathKnightSpells::SUDDEN_DOOM);
}

bool PlayerbotUnholyDeathKnightStrategy::HasRunicCorruption() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(DeathKnightSpells::RUNIC_CORRUPTION);
}

bool PlayerbotUnholyDeathKnightStrategy::IsDeathRotExpiring() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    Aura* aura = bot->GetAura(DeathKnightSpells::DEATH_ROT);
    if (!aura)
        return false;
    
    return aura->GetDuration() <= 3000; // 3 seconds remaining
}

bool PlayerbotUnholyDeathKnightStrategy::ShouldBurstFesteringWounds(Unit* target) const
{
    return GetFesteringWoundStacks(target) >= MIN_FESTERING_FOR_BURST;
}

bool PlayerbotUnholyDeathKnightStrategy::ShouldUseBurstCooldowns() const
{
    Unit* target = FindBestTarget();
    return target && (target->GetHealthPct() > 50.0f || IsInCombat());
}

bool PlayerbotUnholyDeathKnightStrategy::ShouldUseAoERotation() const
{
    return CountNearbyEnemies(8.0f) >= AOE_TARGET_COUNT;
}

bool PlayerbotUnholyDeathKnightStrategy::NeedsDiseaseRefresh(Unit* target) const
{
    if (!HasVirulentPlague(target))
        return true;
    
    return GetVirulentPlagueRemainingDuration(target) <= DISEASE_REFRESH_THRESHOLD;
}

bool PlayerbotUnholyDeathKnightStrategy::ShouldApplyFesteringWounds(Unit* target) const
{
    return GetFesteringWoundStacks(target) <= 2;
}

float PlayerbotUnholyDeathKnightStrategy::GetVirulentPlagueRemainingDuration(Unit* target) const
{
    if (Aura* aura = target->GetAura(DeathKnightSpells::VIRULENT_PLAGUE, _ai->GetBot()->GetGUID()))
        return float(aura->GetDuration()) / 1000.0f;
    return 0.0f;
}