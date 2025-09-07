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

#include "PlayerbotEvokerActions.h"

#ifdef WITH_PLAYERBOTS

// ===========================================
// PlayerbotEvokerAction (Base Class)
// ===========================================

PlayerbotEvokerAction::PlayerbotEvokerAction(PlayerbotPlayerAI* ai, std::string const& name, uint32 spellId)
    : PlayerbotSpellAction(ai, name, spellId)
{
}

uint32 PlayerbotEvokerAction::GetEssenceAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_ESSENCE) : 0;
}

uint32 PlayerbotEvokerAction::GetEssencePercent() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 maxEssence = bot->GetMaxPower(POWER_ESSENCE);
    if (maxEssence == 0)
        return 0;
    
    return (GetEssenceAmount() * 100) / maxEssence;
}

bool PlayerbotEvokerAction::HasEnoughEssence(uint32 cost) const
{
    return GetEssenceAmount() >= cost;
}

bool PlayerbotEvokerAction::ShouldConserveEssence() const
{
    return GetEssenceAmount() <= 1 && IsInCombat();
}

bool PlayerbotEvokerAction::isDevastation() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(EvokerSpells::DISINTEGRATE);
}

bool PlayerbotEvokerAction::isPreservation() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(EvokerSpells::VERDANT_EMBRACE);
}

bool PlayerbotEvokerAction::isAugmentation() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(EvokerSpells::ERUPTION);
}

bool PlayerbotEvokerAction::ShouldChargeEmpoweredSpell() const
{
    return HasTimeForEmpower() && HasEnoughEssence(3);
}

bool PlayerbotEvokerAction::HasTimeForEmpower() const
{
    Unit* target = GetTarget();
    return target && !target->IsNonMeleeSpellCast(false) && IsInCombat();
}

uint32 PlayerbotEvokerAction::GetOptimalEmpowerLevel() const
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

bool PlayerbotEvokerAction::IsChannelingEmpoweredSpell() const
{
    Player* bot = _ai->GetBot();
    Spell* currentSpell = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
    return currentSpell != nullptr;
}

bool PlayerbotEvokerAction::HasDragonrage() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(EvokerSpells::DRAGONRAGE);
}

bool PlayerbotEvokerAction::HasSnapfire() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(EvokerSpells::SNAPFIRE);
}

bool PlayerbotEvokerAction::HasBurnout() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(EvokerSpells::BURNOUT);
}

bool PlayerbotEvokerAction::HasChargedBlast() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(EvokerSpells::CHARGED_BLAST);
}

bool PlayerbotEvokerAction::HasIridescenceRed() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(EvokerSpells::IRIDESCENCE_RED);
}

bool PlayerbotEvokerAction::HasIridescenceBlue() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(EvokerSpells::IRIDESCENCE_BLUE);
}

bool PlayerbotEvokerAction::HasEssenceBurst() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(EvokerSpells::ESSENCE_BURST);
}

bool PlayerbotEvokerAction::HasLeapingFlames(Unit* target) const
{
    if (!target)
        target = GetTarget();
    
    return target && target->HasAura(EvokerSpells::LEAPING_FLAMES);
}

bool PlayerbotEvokerAction::HasCallOfYsera() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(EvokerSpells::CALL_OF_YSERA);
}

bool PlayerbotEvokerAction::HasEmeraldTrance() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(EvokerSpells::EMERALD_TRANCE);
}

Unit* PlayerbotEvokerAction::FindBestDpsTarget() const
{
    Unit* currentTarget = GetTarget();
    if (currentTarget && currentTarget->IsAlive() && IsInOptimalRange(currentTarget, 25.0f))
        return currentTarget;
    
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_ai->GetBot(), _ai->GetBot(), 25.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_ai->GetBot(), targets, u_check);
    Cell::VisitAllObjects(_ai->GetBot(), searcher, 25.0f);
    
    if (!targets.empty())
        return targets.front();
    
    return nullptr;
}

Unit* PlayerbotEvokerAction::FindBestHealTarget() const
{
    Unit* lowestHealthTarget = FindLowestHealthAlly();
    Unit* tankTarget = FindTankTarget();
    
    if (tankTarget && tankTarget->GetHealthPct() < 60.0f)
        return tankTarget;
    
    return lowestHealthTarget;
}

Unit* PlayerbotEvokerAction::FindLowestHealthAlly() const
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

Unit* PlayerbotEvokerAction::FindTankTarget() const
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

std::vector<Unit*> PlayerbotEvokerAction::FindHealingTargets(uint32 maxTargets) const
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

uint32 PlayerbotEvokerAction::CountNearbyEnemies(float range) const
{
    uint32 count = 0;
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_ai->GetBot(), _ai->GetBot(), range);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_ai->GetBot(), targets, u_check);
    Cell::VisitAllObjects(_ai->GetBot(), searcher, range);
    return static_cast<uint32>(targets.size());
}

uint32 PlayerbotEvokerAction::CountNearbyAllies(float range) const
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

uint32 PlayerbotEvokerAction::CountInjuredAllies(float healthPercent, float range) const
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

bool PlayerbotEvokerAction::IsInOptimalRange(Unit* target, float range) const
{
    return target && _ai->GetBot()->IsInRange(target, range);
}

bool PlayerbotEvokerAction::ShouldUseAoE() const
{
    return CountNearbyEnemies(15.0f) >= 3;
}

bool PlayerbotEvokerAction::IsInCombat() const
{
    return _ai->GetBot()->IsInCombat();
}

bool PlayerbotEvokerAction::ShouldUseBurstCooldowns() const
{
    Unit* target = GetTarget();
    return target && (target->GetHealthPct() > 80.0f || CountNearbyEnemies(15.0f) >= 3);
}

bool PlayerbotEvokerAction::IsInDragonrageWindow() const
{
    return HasDragonrage();
}

bool PlayerbotEvokerAction::IsInEmergencyMode() const
{
    return _ai->GetBot()->GetHealthPct() < 30.0f;
}

bool PlayerbotEvokerAction::NeedsHealing() const
{
    return _ai->GetBot()->GetHealthPct() < 70.0f;
}

// ===========================================
// Devastation Evoker Actions (DPS)
// ===========================================

PlayerbotAzureStrikeAction::PlayerbotAzureStrikeAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "azure strike", EvokerSpells::AZURE_STRIKE)
{
}

bool PlayerbotAzureStrikeAction::Execute(Unit* target)
{
    return target && _ai->GetBot()->CastSpell(target, _spellId, false);
}

bool PlayerbotAzureStrikeAction::isUseful()
{
    return isDevastation() && !ShouldConserveEssence() && GetEssenceAmount() < 4;
}

PlayerbotLivingFlameAction::PlayerbotLivingFlameAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "living flame", EvokerSpells::LIVING_FLAME)
{
}

bool PlayerbotLivingFlameAction::Execute(Unit* target)
{
    return target && _ai->GetBot()->CastSpell(target, _spellId, false);
}

bool PlayerbotLivingFlameAction::isUseful()
{
    Unit* target = GetTarget();
    return target && (!HasLivingFlameDoT(target) || ShouldRefreshDoT(target)) && 
           IsInOptimalRange(target, 25.0f);
}

bool PlayerbotLivingFlameAction::HasLivingFlameDoT(Unit* target) const
{
    return target && target->HasAura(EvokerSpells::LIVING_FLAME_DOT);
}

bool PlayerbotLivingFlameAction::ShouldRefreshDoT(Unit* target) const
{
    if (!target)
        return false;
    
    if (Aura* aura = target->GetAura(EvokerSpells::LIVING_FLAME_DOT))
        return aura->GetDuration() < (aura->GetMaxDuration() * 0.3f);
    
    return true;
}

PlayerbotDisintegrateAction::PlayerbotDisintegrateAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "disintegrate", EvokerSpells::DISINTEGRATE)
{
}

bool PlayerbotDisintegrateAction::Execute(Unit* target)
{
    return target && _ai->GetBot()->CastSpell(target, _spellId, false);
}

bool PlayerbotDisintegrateAction::isUseful()
{
    return isDevastation() && HasEnoughEssence(3) && GetEssenceAmount() >= 4 && 
           CountNearbyEnemies(25.0f) <= 2;
}

PlayerbotFireBreathAction::PlayerbotFireBreathAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "fire breath", EvokerSpells::FIRE_BREATH)
{
}

bool PlayerbotFireBreathAction::Execute(Unit* target)
{
    return target && _ai->GetBot()->CastSpell(target, _spellId, false);
}

bool PlayerbotFireBreathAction::isUseful()
{
    return isDevastation() && HasEnoughEssence(4) && ShouldChargeEmpoweredSpell() && 
           HasTimeForEmpower();
}

PlayerbotEternitySurgeAction::PlayerbotEternitySurgeAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "eternity surge", EvokerSpells::ETERNITY_SURGE)
{
}

bool PlayerbotEternitySurgeAction::Execute(Unit* target)
{
    return target && _ai->GetBot()->CastSpell(target, _spellId, false);
}

bool PlayerbotEternitySurgeAction::isUseful()
{
    return isDevastation() && HasEnoughEssence(3) && ShouldChargeEmpoweredSpell() && 
           HasTimeForEmpower() && (CountNearbyEnemies(25.0f) >= 2 || IsInDragonrageWindow());
}

PlayerbotPyreAction::PlayerbotPyreAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "pyre", EvokerSpells::PYRE)
{
}

bool PlayerbotPyreAction::Execute(Unit* target)
{
    return target && _ai->GetBot()->CastSpell(target, _spellId, false);
}

bool PlayerbotPyreAction::isUseful()
{
    return isDevastation() && HasEnoughEssence(3) && CountNearbyEnemies(8.0f) >= 3;
}

PlayerbotDragonrageAction::PlayerbotDragonrageAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "dragonrage", EvokerSpells::DRAGONRAGE)
{
}

bool PlayerbotDragonrageAction::Execute(Unit* target)
{
    return _ai->GetBot()->CastSpell(_ai->GetBot(), _spellId, false);
}

bool PlayerbotDragonrageAction::isUseful()
{
    return isDevastation() && ShouldUseBurstCooldowns() && HasEnoughEssence(3) && 
           !HasDragonrage() && IsInCombat();
}

PlayerbotTipTheScalesAction::PlayerbotTipTheScalesAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "tip the scales", EvokerSpells::TIP_THE_SCALES)
{
}

bool PlayerbotTipTheScalesAction::Execute(Unit* target)
{
    return _ai->GetBot()->CastSpell(_ai->GetBot(), _spellId, false);
}

bool PlayerbotTipTheScalesAction::isUseful()
{
    return IsInDragonrageWindow() && ShouldChargeEmpoweredSpell() && HasEnoughEssence(4);
}

PlayerbotTimeSkipAction::PlayerbotTimeSkipAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "time skip", EvokerSpells::TIME_SKIP)
{
}

bool PlayerbotTimeSkipAction::Execute(Unit* target)
{
    return _ai->GetBot()->CastSpell(_ai->GetBot(), _spellId, false);
}

bool PlayerbotTimeSkipAction::isUseful()
{
    return ShouldUseBurstCooldowns() && IsInCombat() && HasEnoughEssence(2);
}

// ===========================================
// Preservation Evoker Actions (Healing)
// ===========================================

PlayerbotEmeraldBlossomAction::PlayerbotEmeraldBlossomAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "emerald blossom", EvokerSpells::EMERALD_BLOSSOM)
{
}

bool PlayerbotEmeraldBlossomAction::Execute(Unit* target)
{
    return _ai->GetBot()->CastSpell(_ai->GetBot(), _spellId, false);
}

bool PlayerbotEmeraldBlossomAction::isUseful()
{
    return isPreservation() && CountInjuredAllies(70.0f) >= 3 && 
           HasEnoughEssence(3) && !ShouldConserveEssence();
}

PlayerbotVerdantEmbraceAction::PlayerbotVerdantEmbraceAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "verdant embrace", EvokerSpells::VERDANT_EMBRACE)
{
}

bool PlayerbotVerdantEmbraceAction::Execute(Unit* target)
{
    Unit* healTarget = FindBestHealTarget();
    return healTarget && _ai->GetBot()->CastSpell(healTarget, _spellId, false);
}

bool PlayerbotVerdantEmbraceAction::isUseful()
{
    Unit* healTarget = FindBestHealTarget();
    return isPreservation() && healTarget && healTarget->GetHealthPct() < 80.0f && 
           HasEnoughEssence(3);
}

PlayerbotDreamBreathAction::PlayerbotDreamBreathAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "dream breath", EvokerSpells::DREAM_BREATH)
{
}

bool PlayerbotDreamBreathAction::Execute(Unit* target)
{
    return target && _ai->GetBot()->CastSpell(target, _spellId, false);
}

bool PlayerbotDreamBreathAction::isUseful()
{
    return isPreservation() && CountInjuredAllies(70.0f) >= 3 && HasEnoughEssence(4) && 
           HasTimeForEmpower() && ShouldChargeEmpoweredSpell();
}

PlayerbotSpiritbloomAction::PlayerbotSpiritbloomAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "spiritbloom", EvokerSpells::SPIRITBLOOM)
{
}

bool PlayerbotSpiritbloomAction::Execute(Unit* target)
{
    return target && _ai->GetBot()->CastSpell(target, _spellId, false);
}

bool PlayerbotSpiritbloomAction::isUseful()
{
    return isPreservation() && CountInjuredAllies(60.0f) >= 2 && 
           HasEnoughEssence(3) && HasTimeForEmpower();
}

PlayerbotLifeGiversFlameAction::PlayerbotLifeGiversFlameAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "life givers flame", EvokerSpells::LIFE_GIVERS_FLAME)
{
}

bool PlayerbotLifeGiversFlameAction::Execute(Unit* target)
{
    Unit* healTarget = FindLowestHealthAlly();
    return healTarget && _ai->GetBot()->CastSpell(healTarget, _spellId, false);
}

bool PlayerbotLifeGiversFlameAction::isUseful()
{
    Unit* healTarget = FindLowestHealthAlly();
    return isPreservation() && healTarget && healTarget->GetHealthPct() < 30.0f;
}

PlayerbotReversionAction::PlayerbotReversionAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "reversion", EvokerSpells::REVERSION)
{
}

bool PlayerbotReversionAction::Execute(Unit* target)
{
    Unit* healTarget = FindBestHealTarget();
    return healTarget && _ai->GetBot()->CastSpell(healTarget, _spellId, false);
}

bool PlayerbotReversionAction::isUseful()
{
    Unit* healTarget = FindBestHealTarget();
    return isPreservation() && healTarget && !HasReversionHoT(healTarget) && 
           healTarget->GetHealthPct() < 70.0f && HasEnoughEssence(1);
}

bool PlayerbotReversionAction::HasReversionHoT(Unit* target) const
{
    return target && target->HasAura(EvokerSpells::REVERSION);
}

PlayerbotEchoAction::PlayerbotEchoAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "echo", EvokerSpells::ECHO)
{
}

bool PlayerbotEchoAction::Execute(Unit* target)
{
    Unit* healTarget = FindBestHealTarget();
    return healTarget && _ai->GetBot()->CastSpell(healTarget, _spellId, false);
}

bool PlayerbotEchoAction::isUseful()
{
    Unit* healTarget = FindBestHealTarget();
    return isPreservation() && healTarget && !HasEchoEffect(healTarget) && 
           IsInCombat() && HasEnoughEssence(2);
}

bool PlayerbotEchoAction::HasEchoEffect(Unit* target) const
{
    return target && target->HasAura(EvokerSpells::ECHO);
}

PlayerbotStasisAction::PlayerbotStasisAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "stasis", EvokerSpells::STASIS)
{
}

bool PlayerbotStasisAction::Execute(Unit* target)
{
    return _ai->GetBot()->CastSpell(_ai->GetBot(), _spellId, false);
}

bool PlayerbotStasisAction::isUseful()
{
    return isPreservation() && CountInjuredAllies(30.0f) >= 1 && HasEnoughEssence(4);
}

PlayerbotRewindAction::PlayerbotRewindAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "rewind", EvokerSpells::REWIND)
{
}

bool PlayerbotRewindAction::Execute(Unit* target)
{
    return _ai->GetBot()->CastSpell(_ai->GetBot(), _spellId, false);
}

bool PlayerbotRewindAction::isUseful()
{
    return isPreservation() && CountInjuredAllies(30.0f) >= 2 && IsInCombat();
}

// ===========================================
// Utility Evoker Actions
// ===========================================

PlayerbotQuellAction::PlayerbotQuellAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "quell", EvokerSpells::QUELL)
{
}

bool PlayerbotQuellAction::Execute(Unit* target)
{
    return target && _ai->GetBot()->CastSpell(target, _spellId, false);
}

bool PlayerbotQuellAction::isUseful()
{
    Unit* target = GetTarget();
    return target && target->IsNonMeleeSpellCast(false) && IsInOptimalRange(target, 25.0f);
}

PlayerbotRescueAction::PlayerbotRescueAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "rescue", EvokerSpells::RESCUE)
{
}

bool PlayerbotRescueAction::Execute(Unit* target)
{
    Player* bot = _ai->GetBot();
    if (!bot->GetGroup())
        return false;
        
    Group::MemberSlotList const& groupSlot = bot->GetGroup()->GetMemberSlots();
    for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); ++itr)
    {
        Player* member = ObjectAccessor::FindPlayer(itr->guid);
        if (member && member->IsAlive() && bot->IsInRange(member, 40.0f) && 
            member->GetHealthPct() < 30.0f && member->GetVictim())
        {
            return _ai->GetBot()->CastSpell(member, _spellId, false);
        }
    }
    return false;
}

bool PlayerbotRescueAction::isUseful()
{
    Player* bot = _ai->GetBot();
    if (!bot->GetGroup())
        return false;
        
    Group::MemberSlotList const& groupSlot = bot->GetGroup()->GetMemberSlots();
    for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); ++itr)
    {
        Player* member = ObjectAccessor::FindPlayer(itr->guid);
        if (member && member->IsAlive() && bot->IsInRange(member, 40.0f) && 
            member->GetHealthPct() < 30.0f && member->GetVictim())
            return true;
    }
    return false;
}

PlayerbotExpungeAction::PlayerbotExpungeAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "expunge", EvokerSpells::EXPUNGE)
{
}

bool PlayerbotExpungeAction::Execute(Unit* target)
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
            {
                return _ai->GetBot()->CastSpell(member, _spellId, false);
            }
        }
    }
    return false;
}

bool PlayerbotExpungeAction::isUseful()
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

PlayerbotNaturalizeAction::PlayerbotNaturalizeAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "naturalize", EvokerSpells::NATURALIZE)
{
}

bool PlayerbotNaturalizeAction::Execute(Unit* target)
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
            {
                return _ai->GetBot()->CastSpell(member, _spellId, false);
            }
        }
    }
    return false;
}

bool PlayerbotNaturalizeAction::isUseful()
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

PlayerbotHoverAction::PlayerbotHoverAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "hover", EvokerSpells::HOVER)
{
}

bool PlayerbotHoverAction::Execute(Unit* target)
{
    return _ai->GetBot()->CastSpell(_ai->GetBot(), _spellId, false);
}

bool PlayerbotHoverAction::isUseful()
{
    Unit* target = GetTarget();
    return target && !IsInOptimalRange(target, 15.0f) && IsInOptimalRange(target, 30.0f);
}

PlayerbotDeepBreathAction::PlayerbotDeepBreathAction(PlayerbotPlayerAI* ai)
    : PlayerbotEvokerAction(ai, "deep breath", EvokerSpells::DEEP_BREATH)
{
}

bool PlayerbotDeepBreathAction::Execute(Unit* target)
{
    return target && _ai->GetBot()->CastSpell(target, _spellId, false);
}

bool PlayerbotDeepBreathAction::isUseful()
{
    return CountNearbyEnemies(50.0f) >= 5 && ShouldUseBurstCooldowns();
}

#endif // WITH_PLAYERBOTS