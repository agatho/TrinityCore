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

#ifdef WITH_PLAYERBOTS

#include "PlayerbotHunterActions.h"
#include "PlayerbotPlayerAI.h"
#include "Player.h"
#include "Pet.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Log.h"

// PlayerbotHunterAction Base Class Implementation
bool PlayerbotHunterAction::HasActivePet() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    Unit* pet = bot->GetPet();
    return pet && pet->IsAlive();
}

bool PlayerbotHunterAction::IsInRangedRange(Unit* target) const
{
    if (!target)
        return false;
    
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    float distance = bot->GetDistance(target);
    return distance >= 5.0f && distance <= 40.0f;
}

uint32 PlayerbotHunterAction::GetFocusAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_FOCUS) : 0;
}

bool PlayerbotHunterAction::HasEnoughFocus(uint32 focusCost) const
{
    return GetFocusAmount() >= focusCost;
}

bool PlayerbotHunterAction::IsBeastMastery() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(HunterSpells::BESTIAL_WRATH);
}

bool PlayerbotHunterAction::IsMarksmanship() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(HunterSpells::AIMED_SHOT);
}

bool PlayerbotHunterAction::IsSurvival() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(HunterSpells::RAPTOR_STRIKE);
}

// Kill Command Action Implementation
bool PlayerbotKillCommandAction::Execute(PlayerbotEvent const& event)
{
    Player* bot = _ai->GetBot();
    if (!bot || !HasActivePet())
        return false;

    Unit* target = _ai->GetCurrentTarget();
    if (!target || !IsInRangedRange(target))
        return false;

    if (!HasEnoughFocus(30))
        return false;

    return bot->CastSpell(target, HunterSpells::KILL_COMMAND, false);
}

bool PlayerbotKillCommandAction::isUseful()
{
    return HasActivePet() && _ai->IsInCombat() && HasEnoughFocus(30);
}

float PlayerbotKillCommandAction::GetRelevance() const
{
    if (IsBeastMastery() || IsSurvival())
        return 0.9f;
    return 0.3f;
}

// Barbed Shot Action Implementation
bool PlayerbotBarbedShotAction::Execute(PlayerbotEvent const& event)
{
    Player* bot = _ai->GetBot();
    if (!bot || !HasActivePet() || !IsBeastMastery())
        return false;

    Unit* target = _ai->GetCurrentTarget();
    if (!target || !IsInRangedRange(target))
        return false;

    return bot->CastSpell(target, HunterSpells::BARBED_SHOT, false);
}

bool PlayerbotBarbedShotAction::isUseful()
{
    return HasActivePet() && _ai->IsInCombat() && IsBeastMastery();
}

float PlayerbotBarbedShotAction::GetRelevance() const
{
    return 0.85f;
}

// Cobra Shot Action Implementation
bool PlayerbotCobraShotAction::Execute(PlayerbotEvent const& event)
{
    Player* bot = _ai->GetBot();
    if (!bot || !IsBeastMastery())
        return false;

    Unit* target = _ai->GetCurrentTarget();
    if (!target || !IsInRangedRange(target))
        return false;

    if (!HasEnoughFocus(25))
        return false;

    return bot->CastSpell(target, HunterSpells::COBRA_SHOT, false);
}

bool PlayerbotCobraShotAction::isUseful()
{
    return _ai->IsInCombat() && IsBeastMastery() && HasEnoughFocus(25);
}

float PlayerbotCobraShotAction::GetRelevance() const
{
    return 0.4f;
}

// Bestial Wrath Action Implementation
bool PlayerbotBestialWrathAction::Execute(PlayerbotEvent const& event)
{
    Player* bot = _ai->GetBot();
    if (!bot || !HasActivePet() || !IsBeastMastery())
        return false;

    Unit* target = _ai->GetCurrentTarget();
    if (!target)
        return false;

    return bot->CastSpell(bot, HunterSpells::BESTIAL_WRATH, false);
}

bool PlayerbotBestialWrathAction::isUseful()
{
    return HasActivePet() && _ai->IsInCombat() && IsBeastMastery();
}

float PlayerbotBestialWrathAction::GetRelevance() const
{
    return 0.8f;
}

// Aimed Shot Action Implementation
bool PlayerbotAimedShotAction::Execute(PlayerbotEvent const& event)
{
    Player* bot = _ai->GetBot();
    if (!bot || !IsMarksmanship())
        return false;

    Unit* target = _ai->GetCurrentTarget();
    if (!target || !IsInRangedRange(target))
        return false;

    if (!HasEnoughFocus(35))
        return false;

    return bot->CastSpell(target, HunterSpells::AIMED_SHOT, false);
}

bool PlayerbotAimedShotAction::isUseful()
{
    return _ai->IsInCombat() && IsMarksmanship() && HasEnoughFocus(35);
}

float PlayerbotAimedShotAction::GetRelevance() const
{
    return 0.85f;
}

// Rapid Fire Action Implementation
bool PlayerbotRapidFireAction::Execute(PlayerbotEvent const& event)
{
    Player* bot = _ai->GetBot();
    if (!bot || !IsMarksmanship())
        return false;

    Unit* target = _ai->GetCurrentTarget();
    if (!target || !IsInRangedRange(target))
        return false;

    return bot->CastSpell(target, HunterSpells::RAPID_FIRE, false);
}

bool PlayerbotRapidFireAction::isUseful()
{
    return _ai->IsInCombat() && IsMarksmanship();
}

float PlayerbotRapidFireAction::GetRelevance() const
{
    return 0.9f;
}

// Arcane Shot Action Implementation
bool PlayerbotArcaneShotAction::Execute(PlayerbotEvent const& event)
{
    Player* bot = _ai->GetBot();
    if (!bot || !IsMarksmanship())
        return false;

    Unit* target = _ai->GetCurrentTarget();
    if (!target || !IsInRangedRange(target))
        return false;

    if (!HasEnoughFocus(25))
        return false;

    return bot->CastSpell(target, HunterSpells::ARCANE_SHOT, false);
}

bool PlayerbotArcaneShotAction::isUseful()
{
    return _ai->IsInCombat() && IsMarksmanship() && HasEnoughFocus(25);
}

float PlayerbotArcaneShotAction::GetRelevance() const
{
    return 0.6f;
}

// Trueshot Action Implementation
bool PlayerbotTrueshotAction::Execute(PlayerbotEvent const& event)
{
    Player* bot = _ai->GetBot();
    if (!bot || !IsMarksmanship())
        return false;

    Unit* target = _ai->GetCurrentTarget();
    if (!target)
        return false;

    return bot->CastSpell(bot, HunterSpells::TRUESHOT, false);
}

bool PlayerbotTrueshotAction::isUseful()
{
    return _ai->IsInCombat() && IsMarksmanship();
}

float PlayerbotTrueshotAction::GetRelevance() const
{
    return 0.8f;
}

// Raptor Strike Action Implementation
bool PlayerbotRaptorStrikeAction::Execute(PlayerbotEvent const& event)
{
    Player* bot = _ai->GetBot();
    if (!bot || !IsSurvival())
        return false;

    Unit* target = _ai->GetCurrentTarget();
    if (!target)
        return false;

    float distance = bot->GetDistance(target);
    if (distance > 5.0f)
        return false;

    if (!HasEnoughFocus(25))
        return false;

    return bot->CastSpell(target, HunterSpells::RAPTOR_STRIKE, false);
}

bool PlayerbotRaptorStrikeAction::isUseful()
{
    Unit* target = _ai->GetCurrentTarget();
    if (!target || !_ai->IsInCombat() || !IsSurvival())
        return false;
        
    Player* bot = _ai->GetBot();
    float distance = bot ? bot->GetDistance(target) : 100.0f;
    return distance <= 5.0f && HasEnoughFocus(25);
}

float PlayerbotRaptorStrikeAction::GetRelevance() const
{
    return 0.6f;
}

// Wildfire Bomb Action Implementation
bool PlayerbotWildfireBombAction::Execute(PlayerbotEvent const& event)
{
    Player* bot = _ai->GetBot();
    if (!bot || !IsSurvival())
        return false;

    Unit* target = _ai->GetCurrentTarget();
    if (!target || !IsInRangedRange(target))
        return false;

    if (!HasEnoughFocus(30))
        return false;

    return bot->CastSpell(target, HunterSpells::WILDFIRE_BOMB, false);
}

bool PlayerbotWildfireBombAction::isUseful()
{
    return _ai->IsInCombat() && IsSurvival() && HasEnoughFocus(30);
}

float PlayerbotWildfireBombAction::GetRelevance() const
{
    return 0.8f;
}

// Coordinated Assault Action Implementation
bool PlayerbotCoordinatedAssaultAction::Execute(PlayerbotEvent const& event)
{
    Player* bot = _ai->GetBot();
    if (!bot || !IsSurvival())
        return false;

    Unit* target = _ai->GetCurrentTarget();
    if (!target)
        return false;

    return bot->CastSpell(bot, HunterSpells::COORDINATED_ASSAULT, false);
}

bool PlayerbotCoordinatedAssaultAction::isUseful()
{
    return _ai->IsInCombat() && IsSurvival();
}

float PlayerbotCoordinatedAssaultAction::GetRelevance() const
{
    return 0.85f;
}

// Hunter's Mark Action Implementation
bool PlayerbotHuntersMarkAction::Execute(PlayerbotEvent const& event)
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;

    Unit* target = _ai->GetCurrentTarget();
    if (!target || !_ai->IsInCombat())
        return false;

    if (target->HasAura(HunterSpells::HUNTERS_MARK))
        return false;

    return bot->CastSpell(target, HunterSpells::HUNTERS_MARK, false);
}

bool PlayerbotHuntersMarkAction::isUseful()
{
    Unit* target = _ai->GetCurrentTarget();
    return target && _ai->IsInCombat() && !target->HasAura(HunterSpells::HUNTERS_MARK);
}

float PlayerbotHuntersMarkAction::GetRelevance() const
{
    return 0.3f;
}

// Kill Shot Action Implementation
bool PlayerbotKillShotAction::Execute(PlayerbotEvent const& event)
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;

    Unit* target = _ai->GetCurrentTarget();
    if (!target || !_ai->IsInCombat())
        return false;

    float healthPct = float(target->GetHealth()) / float(target->GetMaxHealth());
    if (healthPct > 0.20f)
        return false;

    if (!HasEnoughFocus(25))
        return false;

    return bot->CastSpell(target, HunterSpells::KILL_SHOT, false);
}

bool PlayerbotKillShotAction::isUseful()
{
    Unit* target = _ai->GetCurrentTarget();
    if (!target || !_ai->IsInCombat())
        return false;

    float healthPct = float(target->GetHealth()) / float(target->GetMaxHealth());
    return healthPct <= 0.20f && HasEnoughFocus(25);
}

float PlayerbotKillShotAction::GetRelevance() const
{
    return 0.95f;
}

// Call Pet Action Implementation
bool PlayerbotCallPetAction::Execute(PlayerbotEvent const& event)
{
    Player* bot = _ai->GetBot();
    if (!bot || HasActivePet())
        return false;

    return bot->CastSpell(bot, HunterSpells::CALL_PET, false);
}

bool PlayerbotCallPetAction::isUseful()
{
    return !HasActivePet() && !_ai->IsInCombat();
}

#endif // WITH_PLAYERBOTS