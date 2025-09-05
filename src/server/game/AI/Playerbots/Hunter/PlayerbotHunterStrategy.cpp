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

#include "PlayerbotHunterStrategy.h"
#include "PlayerbotHunterActions.h"
#include "PlayerbotPlayerAI.h"
#include "Player.h"
#include "Pet.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Group.h"
#include "Log.h"

// PlayerbotHunterStrategy Base Implementation
PlayerbotHunterStrategy::PlayerbotHunterStrategy(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotStrategy(ai, name)
{
}

void PlayerbotHunterStrategy::InitializeActions()
{
    PlayerbotStrategy::InitializeActions();
    RegisterCommonActions();
}

void PlayerbotHunterStrategy::InitializeTriggers()
{
    PlayerbotStrategy::InitializeTriggers();
    RegisterCommonTriggers();
}

void PlayerbotHunterStrategy::RegisterCommonActions()
{
    // Shared utility actions
    RegisterAction("hunters mark", std::make_unique<PlayerbotHuntersMarkAction>(_ai));
    RegisterAction("kill shot", std::make_unique<PlayerbotKillShotAction>(_ai));
    RegisterAction("call pet", std::make_unique<PlayerbotCallPetAction>(_ai));
}

void PlayerbotHunterStrategy::RegisterCommonTriggers()
{
    // TODO: Implement proper trigger registration with unique_ptr objects
    // Hunter's Mark application
    // RegisterTrigger("hunters mark", [this]() { return ShouldUseHuntersMark(); });
    
    // Execute phase with Kill Shot
    // RegisterTrigger("kill shot", [this]() {
    //     Unit* target = _ai->GetCurrentTarget();
    //     return target && IsInExecuteRange(target);
    // });
    
    // Pet management
    // RegisterTrigger("call pet", [this]() { return ShouldCallPet(); });
}

bool PlayerbotHunterStrategy::ShouldUseHuntersMark() const
{
    Unit* target = _ai->GetCurrentTarget();
    if (!target || !_ai->IsInCombat())
        return false;

    return !target->HasAura(HunterSpells::HUNTERS_MARK);
}

bool PlayerbotHunterStrategy::HasActivePet() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
    
    Unit* pet = bot->GetPet();
    return pet && pet->IsAlive();
}

bool PlayerbotHunterStrategy::ShouldCallPet() const
{
    return !HasActivePet() && !_ai->IsInCombat();
}

bool PlayerbotHunterStrategy::IsInExecuteRange(Unit* target) const
{
    if (!target)
        return false;
    
    float healthPct = float(target->GetHealth()) / float(target->GetMaxHealth());
    return healthPct <= 0.20f; // 20% execute threshold
}

// PlayerbotBeastMasteryHunterStrategy Implementation
PlayerbotBeastMasteryHunterStrategy::PlayerbotBeastMasteryHunterStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotHunterStrategy(ai, "beast_mastery")
{
}

void PlayerbotBeastMasteryHunterStrategy::InitializeActions()
{
    PlayerbotHunterStrategy::InitializeActions();
    RegisterBeastMasteryActions();
}

void PlayerbotBeastMasteryHunterStrategy::InitializeTriggers()
{
    PlayerbotHunterStrategy::InitializeTriggers();
    RegisterBeastMasteryTriggers();
}

void PlayerbotBeastMasteryHunterStrategy::RegisterBeastMasteryActions()
{
    // Beast Mastery core abilities
    RegisterAction("kill command", std::make_unique<PlayerbotKillCommandAction>(_ai));
    RegisterAction("barbed shot", std::make_unique<PlayerbotBarbedShotAction>(_ai));
    RegisterAction("cobra shot", std::make_unique<PlayerbotCobraShotAction>(_ai));
    RegisterAction("bestial wrath", std::make_unique<PlayerbotBestialWrathAction>(_ai));
}

void PlayerbotBeastMasteryHunterStrategy::RegisterBeastMasteryTriggers()
{
    // TODO: Implement proper trigger registration with unique_ptr objects
    // Kill Command priority - BM primary ability
    // RegisterTrigger("kill command", [this]() {
    //     return _ai->IsInCombat() && HasActivePet();
    // });
    
    // Barbed Shot to maintain pet frenzy
    // RegisterTrigger("barbed shot", [this]() {
    //     if (!_ai->IsInCombat() || !HasActivePet())
    //         // return false;
    //     
    //     Player* bot = _ai->GetBot();
    //     Unit* pet = bot ? bot->GetPet() : nullptr;
    //     return pet && !pet->HasAura(272790); // Pet frenzy buff
    // });
    
    // Cobra Shot as filler
    // RegisterTrigger("cobra shot", [this]() {
    //     return _ai->IsInCombat() && _ai->GetCurrentTarget();
    // });
    
    // Bestial Wrath cooldown usage
    // RegisterTrigger("bestial wrath", [this]() {
    //     if (!_ai->IsInCombat() || !HasActivePet())
    //         // return false;
    //     
    //     Unit* target = _ai->GetCurrentTarget();
    //     if (!target)
    //         // return false;
    //     
    //     float healthPct = float(target->GetHealth()) / float(target->GetMaxHealth());
    //     return healthPct > 0.5f; // Use on healthy targets
    // });
}

float PlayerbotBeastMasteryHunterStrategy::GetActionPriority(std::string const& actionName) const
{
    // Modern Beast Mastery rotation priorities - The War Within
    if (actionName == "kill shot")
        return 0.95f;  // Highest priority in execute range
    else if (actionName == "kill command")
        return 0.9f;   // Core BM ability
    else if (actionName == "barbed shot")
        return 0.85f;  // Maintain pet frenzy
    else if (actionName == "bestial wrath")
        return 0.8f;   // Major cooldown
    else if (actionName == "cobra shot")
        return 0.4f;   // Filler ability
    else if (actionName == "hunters mark")
        return 0.3f;   // Utility
    
    return PlayerbotHunterStrategy::GetActionPriority(actionName);
}

// PlayerbotMarksmanshipHunterStrategy Implementation
PlayerbotMarksmanshipHunterStrategy::PlayerbotMarksmanshipHunterStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotHunterStrategy(ai, "marksmanship")
{
}

void PlayerbotMarksmanshipHunterStrategy::InitializeActions()
{
    PlayerbotHunterStrategy::InitializeActions();
    RegisterMarksmanshipActions();
}

void PlayerbotMarksmanshipHunterStrategy::InitializeTriggers()
{
    PlayerbotHunterStrategy::InitializeTriggers();
    RegisterMarksmanshipTriggers();
}

void PlayerbotMarksmanshipHunterStrategy::RegisterMarksmanshipActions()
{
    // Marksmanship core abilities
    RegisterAction("aimed shot", std::make_unique<PlayerbotAimedShotAction>(_ai));
    RegisterAction("rapid fire", std::make_unique<PlayerbotRapidFireAction>(_ai));
    RegisterAction("arcane shot", std::make_unique<PlayerbotArcaneShotAction>(_ai));
    RegisterAction("trueshot", std::make_unique<PlayerbotTrueshotAction>(_ai));
}

void PlayerbotMarksmanshipHunterStrategy::RegisterMarksmanshipTriggers()
{
    // TODO: Fix trigger registration system
    // RegisterTrigger calls need proper implementation
}

float PlayerbotMarksmanshipHunterStrategy::GetActionPriority(std::string const& actionName) const
{
    // Modern Marksmanship rotation priorities - The War Within
    if (actionName == "kill shot")
        return 0.95f;  // Highest priority in execute range
    else if (actionName == "rapid fire")
        return 0.9f;   // Generate Streamline stacks
    else if (actionName == "aimed shot")
        return 0.85f;  // Primary ability
    else if (actionName == "trueshot")
        return 0.8f;   // Major cooldown
    else if (actionName == "arcane shot")
        return 0.6f;   // Precise Shots spender
    else if (actionName == "hunters mark")
        return 0.3f;   // Utility
    
    return PlayerbotHunterStrategy::GetActionPriority(actionName);
}

// PlayerbotSurvivalHunterStrategy Implementation
PlayerbotSurvivalHunterStrategy::PlayerbotSurvivalHunterStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotHunterStrategy(ai, "survival")
{
}

void PlayerbotSurvivalHunterStrategy::InitializeActions()
{
    PlayerbotHunterStrategy::InitializeActions();
    RegisterSurvivalActions();
}

void PlayerbotSurvivalHunterStrategy::InitializeTriggers()
{
    PlayerbotHunterStrategy::InitializeTriggers();
    RegisterSurvivalTriggers();
}

void PlayerbotSurvivalHunterStrategy::RegisterSurvivalActions()
{
    // Survival core abilities
    RegisterAction("kill command", std::make_unique<PlayerbotKillCommandAction>(_ai));
    RegisterAction("raptor strike", std::make_unique<PlayerbotRaptorStrikeAction>(_ai));
    RegisterAction("wildfire bomb", std::make_unique<PlayerbotWildfireBombAction>(_ai));
    RegisterAction("coordinated assault", std::make_unique<PlayerbotCoordinatedAssaultAction>(_ai));
}

void PlayerbotSurvivalHunterStrategy::RegisterSurvivalTriggers()
{
    // TODO: Fix trigger registration system
    // RegisterTrigger calls need proper implementation
}

float PlayerbotSurvivalHunterStrategy::GetActionPriority(std::string const& actionName) const
{
    // Modern Survival rotation priorities - The War Within
    if (actionName == "kill shot")
        return 0.95f;  // Highest priority in execute range
    else if (actionName == "wildfire bomb")
        return 0.9f;   // High priority with Tip of the Spear
    else if (actionName == "kill command")
        return 0.85f;  // Generate Tip of the Spear
    else if (actionName == "coordinated assault")
        return 0.8f;   // Major cooldown
    else if (actionName == "raptor strike")
        return 0.6f;   // Melee filler
    else if (actionName == "hunters mark")
        return 0.3f;   // Utility
    
    return PlayerbotHunterStrategy::GetActionPriority(actionName);
}

// PlayerbotHunterStrategyFactory Implementation
std::unique_ptr<PlayerbotHunterStrategy> PlayerbotHunterStrategyFactory::CreateStrategy(
    PlayerbotPlayerAI* ai, 
    std::string const& strategyName)
{
    if (strategyName == "beast_mastery")
        return std::make_unique<PlayerbotBeastMasteryHunterStrategy>(ai);
    else if (strategyName == "marksmanship")
        return std::make_unique<PlayerbotMarksmanshipHunterStrategy>(ai);
    else if (strategyName == "survival")
        return std::make_unique<PlayerbotSurvivalHunterStrategy>(ai);
    else
    {
        // Auto-determine specialization
        std::string autoSpec = DetermineSpecialization(ai);
        if (autoSpec == "beast_mastery")
            return std::make_unique<PlayerbotBeastMasteryHunterStrategy>(ai);
        else if (autoSpec == "marksmanship")
            return std::make_unique<PlayerbotMarksmanshipHunterStrategy>(ai);
        else
            return std::make_unique<PlayerbotSurvivalHunterStrategy>(ai);
    }
}

std::vector<std::string> PlayerbotHunterStrategyFactory::GetAvailableStrategies()
{
    return {"beast_mastery", "marksmanship", "survival"};
}

std::string PlayerbotHunterStrategyFactory::DetermineSpecialization(PlayerbotPlayerAI* ai)
{
    Player* bot = ai->GetBot();
    if (!bot)
        return "beast_mastery";

    // Simple specialization detection based on key spells
    if (bot->HasSpell(HunterSpells::BESTIAL_WRATH))
        return "beast_mastery";
    else if (bot->HasSpell(HunterSpells::AIMED_SHOT))
        return "marksmanship";
    else if (bot->HasSpell(HunterSpells::RAPTOR_STRIKE))
        return "survival";
    
    // Default to beast mastery for low-level characters
    return "beast_mastery";
}

#endif // WITH_PLAYERBOTS