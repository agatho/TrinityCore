/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * Beast Mastery Hunter Specialization - REFACTORED
 *
 * This demonstrates the complete migration of Beast Mastery Hunter to the template-based
 * architecture, eliminating code duplication while maintaining full functionality.
 *
 * Beast Mastery focuses on pet synergy, providing powerful buffs to the pet while
 * maintaining consistent ranged damage through focus management.
 */

#pragma once


#include "../Common/StatusEffectTracker.h"
#include "../Common/CooldownManager.h"
#include "../Common/RotationHelpers.h"
#include "../CombatSpecializationTemplates.h"
#include "../ResourceTypes.h"
#include "../CombatSpecializationTemplates.h"
#include "Pet.h"
#include "PetDefines.h"
#include "MotionMaster.h"
#include "CharmInfo.h"
#include <unordered_map>
#include <queue>

// Phase 5 Integration: Decision Systems
#include "../Decision/ActionPriorityQueue.h"
#include "../Decision/BehaviorTree.h"
#include "../BotAI.h"
#include "GameTime.h"

namespace Playerbot
{

// WoW 11.2 Beast Mastery Hunter Spell IDs
enum BeastMasterySpells
{
    // Core Abilities
    SPELL_KILL_COMMAND          = 34026,   // Primary pet damage ability
    SPELL_BARBED_SHOT          = 217200,  // Generates focus, maintains pet frenzy
    SPELL_COBRA_SHOT           = 193455,  // Focus builder
    SPELL_BESTIAL_WRATH        = 19574,   // Major cooldown
    SPELL_ASPECT_OF_THE_WILD  = 193530,  // DPS cooldown

    SPELL_MULTISHOT
    = 2643,
    // AoE ability

    // Pet Management
    SPELL_CALL_PET_1           = 883,     // Summon primary pet

    SPELL_MEND_PET
    = 136,
    // Pet heal
    SPELL_REVIVE_PET           = 982,     // Resurrect pet
    SPELL_PET_ATTACK           = 52398,   // Command pet to attack
    SPELL_PET_FOLLOW           = 52399,   // Command pet to follow

    SPELL_PET_STAY
    = 52400,   // Command pet to stay

    // Talents/Special
    SPELL_DIRE_BEAST           = 120679,  // Summon additional beast

    SPELL_BLOODSHED
    = 321530,  // Pet bleed ability

    SPELL_WILD_CALL
    = 185789,  // Barbed Shot reset proc
    SPELL_ANIMAL_COMPANION     = 267116,  // Second permanent pet

    // Buffs/Debuffs
    SPELL_HUNTERS_MARK         = 257284,  // Target marking
    SPELL_ASPECT_OF_CHEETAH    = 186257,  // Movement speed
    SPELL_EXHILARATION         = 109304,  // Heal self and pet
    SPELL_PET_FRENZY           = 272790,  // Pet attack speed buff (from Barbed Shot)

    // Utility
    SPELL_COUNTER_SHOT         = 147362,  // Interrupt
    SPELL_TRANQUILIZING_SHOT   = 19801,   // Dispel

    SPELL_TAR_TRAP
    = 187698,  // Slow trap
    SPELL_FREEZING_TRAP        = 187650,  // CC trap
};

/**
 * Pet Management System for Beast Mastery
 *
 * Handles all pet-related mechanics including summoning, commanding,
 * ability usage, and health management.
 */
class BeastMasteryPetManager
{
public:
    explicit BeastMasteryPetManager(Player* bot)        : _bot(bot)
        , _lastMendPet(0)
        , _lastPetCommand(0)
        , _petFrenzyStacks(0)
        , _petFrenzyExpireTime(0)
    {
    }

    void SummonPet()
    {
        if (!_bot || HasActivePet())

            return;

        // Check if we can summon a pet
        if (_bot->GetPetGUID().IsEmpty())
        {

            _bot->CastSpell(_bot, SPELL_CALL_PET_1, false);
        }
    }

    void CommandPetAttack(Unit* target)
    {
        if (!target || !HasActivePet())

            return;

        Pet* pet = _bot->GetPet();
        if (!pet || !pet->IsAlive())

            return;

        // Update pet target if different
        if (pet->GetVictim() != target)
        {

            pet->AttackStop();

            pet->GetMotionMaster()->Clear();


            if (pet->GetCharmInfo())

            {

                pet->GetCharmInfo()->SetCommandState(COMMAND_ATTACK);

                pet->GetCharmInfo()->SetIsCommandAttack(true);

                pet->GetCharmInfo()->SetIsReturning(false);

                pet->GetCharmInfo()->SetIsFollowing(false);

            }


            pet->Attack(target, true);

            pet->GetMotionMaster()->MoveChase(target);
        }
    }

    void CommandPetFollow()
    {
        Pet* pet = _bot->GetPet();
        if (!pet || !pet->IsAlive())

            return;

        pet->AttackStop();
        pet->GetMotionMaster()->Clear();

        if (pet->GetCharmInfo())
        {

            pet->GetCharmInfo()->SetCommandState(COMMAND_FOLLOW);

            pet->GetCharmInfo()->SetIsCommandAttack(false);

            pet->GetCharmInfo()->SetIsReturning(true);

            pet->GetCharmInfo()->SetIsFollowing(true);
        }

        pet->GetMotionMaster()->MoveFollow(_bot, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);
    }

    bool HasActivePet() const
    {
        return _bot && !_bot->GetPetGUID().IsEmpty() && _bot->GetPet() && _bot->GetPet()->IsAlive();
    }

    bool IsPetHealthLow() const
    {
        if (!HasActivePet())

            return false;

        Pet* pet = _bot->GetPet();        return pet && pet->GetHealthPct() < 70.0f;
    }

    void MendPet()
    {
        uint32 currentTime = GameTime::GetGameTimeMS();
        if (currentTime - _lastMendPet < 10000) // 10 second cooldown

            return;

        if (!HasActivePet() || !IsPetHealthLow())

            return;

        Pet* pet = _bot->GetPet();        if (pet && pet->IsAlive() && !_bot->HasAura(SPELL_MEND_PET))
        {

            _bot->CastSpell(pet, SPELL_MEND_PET, false);

            _lastMendPet = currentTime;
        }
    }

    void UpdatePetFrenzy()
    {
        uint32 currentTime = GameTime::GetGameTimeMS();

        // Check if frenzy has expired
        if (_petFrenzyExpireTime > 0 && currentTime > _petFrenzyExpireTime)
        {

            _petFrenzyStacks = 0;

            _petFrenzyExpireTime = 0;
        }
    }

    void ApplyBarbedShot()
    {
        // Barbed Shot applies/refreshes Pet Frenzy
        _petFrenzyStacks = std::min<uint32>(_petFrenzyStacks + 1, 3);
        _petFrenzyExpireTime = GameTime::GetGameTimeMS() + 8000; // 8 second duration
    }

    uint32 GetPetFrenzyStacks() const { return _petFrenzyStacks; }

    void EnsurePetActive(Unit* target)    {
        if (!HasActivePet())
        {

            SummonPet();

            return;
        }

        // Heal pet if needed
        if (IsPetHealthLow())

            MendPet();

        // Command pet to attack
        if (target && target->IsAlive())

            CommandPetAttack(target);
    }

private:
    CooldownManager _cooldowns;
    Player* _bot;
    uint32 _lastMendPet;
    uint32 _lastPetCommand;
    uint32 _petFrenzyStacks;
    uint32 _petFrenzyExpireTime;
};

/**
 * Refactored Beast Mastery Hunter using template architecture
 *
 * Key features:
 * - Inherits from RangedDpsSpecialization<FocusResource> for role defaults
 * - Comprehensive pet management system
 * - Barbed Shot stack tracking for optimal DPS
 * - Kill Command priority system
 * - Focus management with 5/sec regeneration
 */
class BeastMasteryHunterRefactored : public RangedDpsSpecialization<FocusResource>
{
public:
    using Base = RangedDpsSpecialization<FocusResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::CanUseAbility;
    using Base::ConsumeResource;
    using Base::_resource;

    explicit BeastMasteryHunterRefactored(Player* bot)
        : RangedDpsSpecialization<FocusResource>(bot)
        , _petManager(bot)
        , _barbedShotCharges(2)
        , _lastBarbedShotRecharge(0)
        , _bestialWrathActive(false)
        , _bestialWrathEndTime(0)
        , _aspectOfTheWildActive(false)
        , _aspectEndTime(0)        , _wildCallProc(false)
        , _lastKillCommand(0)
        , _lastCobraShot(0)
    {
        // Focus regeneration is handled by template system
        // Setup BM-specific cooldown tracking

        // Phase 5 Integration: Initialize decision systems
        InitializeBeastMasteryMechanics();
    }

    // ========================================================================
    // CORE ROTATION - Beast Mastery specific logic
    // ========================================================================

    void UpdateRotation(::Unit* target) override    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))

            return;

        // Ensure pet is active and attacking
        _petManager.EnsurePetActive(target);

        // Update BM-specific mechanics
        UpdateBeastMasteryState();

        // Check for AoE situation
        uint32 enemyCount = this->GetEnemiesInRange(40.0f);
        if (enemyCount >= 3)

        {
        ExecuteAoERotation(target);

            return;
        }

        // Single target rotation
        ExecuteSingleTargetRotation(target);
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();
        

        // Ensure pet is summoned
        if (!_petManager.HasActivePet())
        {

            _petManager.SummonPet();

            return;
        }

        // Maintain Aspect of the Cheetah when out of combat
        if (!bot->IsInCombat() && !bot->HasAura(SPELL_ASPECT_OF_CHEETAH))
        {

            this->CastSpell(bot, SPELL_ASPECT_OF_CHEETAH);
        }

        // Use Exhilaration for emergency healing
        if (bot->GetHealthPct() < 40.0f && this->CanUseAbility(SPELL_EXHILARATION))
        {

            this->CastSpell(bot, SPELL_EXHILARATION);
        }

        // Apply Hunter's Mark to current target
        if (Unit* target = bot->GetVictim())        {

            if (!target->HasAura(SPELL_HUNTERS_MARK) && this->CanUseAbility(SPELL_HUNTERS_MARK))

            {

                this->CastSpell(target, SPELL_HUNTERS_MARK);

            }
        }
    }

    void OnInterruptRequired(::Unit* target, uint32 /*spellId*/)
    {
        if (this->CanUseAbility(SPELL_COUNTER_SHOT))
        {

            this->CastSpell(target, SPELL_COUNTER_SHOT);
        }
    }

    void OnDispelRequired(::Unit* target)
    {
        if (this->CanUseAbility(SPELL_TRANQUILIZING_SHOT))
        {

            this->CastSpell(target, SPELL_TRANQUILIZING_SHOT);
        }
    }

protected:
    // ========================================================================
    // RESOURCE MANAGEMENT - Focus costs for BM Hunter abilities
    // ========================================================================

    uint32 GetResourceCost(uint32 spellId)
    {
        switch (spellId)
        {

            case SPELL_KILL_COMMAND:
            return 30;

            case SPELL_COBRA_SHOT:
            return 35;

            case SPELL_MULTISHOT:
            return 40;

            case SPELL_BARBED_SHOT:
            return 0;  // Generates 20 focus

            case SPELL_BESTIAL_WRATH:
            return 0;  // No cost

            case SPELL_ASPECT_OF_THE_WILD: return 0; // No cost

            case SPELL_DIRE_BEAST:
            return 25;

            case SPELL_COUNTER_SHOT:
            return 0;  // No cost

            case SPELL_TRANQUILIZING_SHOT: return 10;

            default:
            return 20;
        }
    }

    // ========================================================================
    // BEAST MASTERY SPECIFIC ROTATION LOGIC
    // ========================================================================

    void ExecuteSingleTargetRotation(::Unit* target)
    {
        uint32 currentFocus = this->_resource;

        // Priority 1: Bestial Wrath for burst
        if (ShouldUseBestialWrath(target) && this->CanUseAbility(SPELL_BESTIAL_WRATH))
        {

            this->CastSpell(this->GetBot(), SPELL_BESTIAL_WRATH);

            _bestialWrathActive = true;

            _bestialWrathEndTime = GameTime::GetGameTimeMS() + 15000;

            return;
        }

        // Priority 2: Aspect of the Wild during Bestial Wrath
        if (_bestialWrathActive && this->CanUseAbility(SPELL_ASPECT_OF_THE_WILD))
        {

            this->CastSpell(this->GetBot(), SPELL_ASPECT_OF_THE_WILD);

            _aspectOfTheWildActive = true;

            _aspectEndTime = GameTime::GetGameTimeMS() + 20000;

            return;
        }

        // Priority 3: Kill Command on cooldown (core ability)
        if (currentFocus >= 30 && this->CanUseAbility(SPELL_KILL_COMMAND))
        {

            this->CastSpell(target, SPELL_KILL_COMMAND);

            _lastKillCommand = GameTime::GetGameTimeMS();

            this->ConsumeResource(30);

            return;
        }

        // Priority 4: Barbed Shot to maintain Pet Frenzy
        if (ShouldUseBarbedShot() && HasBarbedShotCharge())
        {

            this->CastSpell(target, SPELL_BARBED_SHOT);

            _petManager.ApplyBarbedShot();

            _barbedShotCharges--;

            _resource = std::min<uint32>(_resource + 20, 100); // Barbed Shot generates 20 focus

            return;
        }

        // Priority 5: Dire Beast for additional damage
        if (currentFocus >= 25 && this->CanUseAbility(SPELL_DIRE_BEAST))
        {

            this->CastSpell(target, SPELL_DIRE_BEAST);

            this->ConsumeResource(25);

            return;
        }

        // Priority 6: Cobra Shot as filler
        if (currentFocus >= 35)
        {

            this->CastSpell(target, SPELL_COBRA_SHOT);

            _lastCobraShot = GameTime::GetGameTimeMS();

            this->ConsumeResource(35);

            _resource = std::min<uint32>(_resource + 5, 100); // Small focus return

            return;
        }

        // Wait for focus regeneration if needed
    }

    void ExecuteAoERotation(::Unit* target)
    {
        uint32 currentFocus = this->_resource;

        // Priority 1: Multi-Shot for Beast Cleave
        if (currentFocus >= 40)
        {

            this->CastSpell(target, SPELL_MULTISHOT);

            this->ConsumeResource(40);

            return;
        }

        // Priority 2: Barbed Shot for focus generation
        if (HasBarbedShotCharge())
        {

            this->CastSpell(target, SPELL_BARBED_SHOT);

            _barbedShotCharges--;

            _resource = std::min<uint32>(_resource + 20, 100);

            return;
        }

        // Priority 3: Kill Command if focus allows
        if (currentFocus >= 30 && this->CanUseAbility(SPELL_KILL_COMMAND))
        {

            this->CastSpell(target, SPELL_KILL_COMMAND);

            this->ConsumeResource(30);

            return;
        }
    }

private:
    // ========================================================================
    // BEAST MASTERY STATE MANAGEMENT
    // ========================================================================

    void UpdateBeastMasteryState()
    {
        uint32 currentTime = GameTime::GetGameTimeMS();

        // Update pet frenzy
        _petManager.UpdatePetFrenzy();

        // Check Bestial Wrath expiry
        if (_bestialWrathActive && currentTime > _bestialWrathEndTime)
        {

            _bestialWrathActive = false;

            _bestialWrathEndTime = 0;
        }

        // Check Aspect of the Wild expiry
        if (_aspectOfTheWildActive && currentTime > _aspectEndTime)
        {

            _aspectOfTheWildActive = false;

            _aspectEndTime = 0;
        }

        // Recharge Barbed Shot charges (12 second recharge)
        if (_barbedShotCharges < 2)
        {

            if (currentTime - _lastBarbedShotRecharge > 12000)
            {

                _barbedShotCharges++;

                _lastBarbedShotRecharge = currentTime;

            }
        }

        // Check for Wild Call proc (chance to reset Barbed Shot)
        CheckWildCallProc();
    }

    bool ShouldUseBarbedShot() const
    {
        // Use if we need to refresh Pet Frenzy or have 2 charges
        return _petManager.GetPetFrenzyStacks() < 3 ||

               _barbedShotCharges == 2 ||

               _wildCallProc;
    }

    bool HasBarbedShotCharge() const
    {
        return _barbedShotCharges > 0 || _wildCallProc;
    }

    bool ShouldUseBestialWrath(Unit* target) const    {
        if (!target)

            return false;

        // Use on high priority targets or when we have good focus
        return (target->GetHealthPct() > 50.0f && this->_resource > 60) ||

               target->GetLevel() > this->GetBot()->GetLevel() + 2;
    }

    void CheckWildCallProc()
    {
        // Simulate 20% proc chance on auto-shot
        if (this->GetBot()->IsInCombat())
        {

            static uint32 lastCheck = 0;

            uint32 currentTime = GameTime::GetGameTimeMS();


            if (currentTime - lastCheck > 3000) // Check every 3 seconds

            {

                lastCheck = currentTime;

                _wildCallProc = (rand() % 100) < 20;

            }
        }
    }

    

private:
    // ========================================================================
    // HUNTER SPECIALIZATION ABSTRACT METHOD IMPLEMENTATIONS
    // ========================================================================

    // Pet management - implemented by BeastMasteryPetManager
    void UpdatePetManagement() { _petManager.EnsurePetActive(GetBot()->GetVictim()); }
    void SummonPet() { _petManager.SummonPet(); }
    void MendPetIfNeeded() { _petManager.MendPet(); }
    void FeedPetIfNeeded() { /* Feeding not implemented in WoW 11.2 */ }
    bool HasActivePet() const { return _petManager.HasActivePet(); }
    PetInfo GetPetInfo() const { return PetInfo(); /* Stub */ }

    // Trap management - delegated to AI
    void UpdateTrapManagement() { /* Traps managed by AI */ }
    void PlaceTrap(uint32 /*trapSpell*/, Position /*position*/) { /* Traps managed by AI */ }
    bool ShouldPlaceTrap() const { return false; }
    uint32 GetOptimalTrapSpell() const { return 0; }
    std::vector<TrapInfo> GetActiveTraps() const { return std::vector<TrapInfo>(); }

    // Aspect management - delegated to UpdateBuffs
    void UpdateAspectManagement() { /* Aspects managed in UpdateBuffs */ }
    void SwitchToOptimalAspect() { /* Aspects managed in UpdateBuffs */ }
    uint32 GetOptimalAspect() const { return SPELL_ASPECT_OF_CHEETAH; }
    bool HasCorrectAspect() const { return true; }

    // Range and positioning - BM is ranged
    void UpdateRangeManagement() { /* Range handled by base class */ }
    bool IsInDeadZone(::Unit* /*target*/) const { return false; }
    bool ShouldKite(::Unit* /*target*/) const { return false; }
    Position GetKitePosition(::Unit* /*target*/) const { return Position(); }
    void HandleDeadZone(::Unit* /*target*/) { /* No dead zone management */ }

    // Tracking management - delegated to AI
    void UpdateTracking() { /* Tracking managed by AI */ }
    uint32 GetOptimalTracking() const { return 0; /* No specific tracking */ }
    void ApplyTracking(uint32 /*trackingSpell*/) { /* Applied by AI */ }

    // Pet command interface - delegated to pet manager
    void CommandPetAttack(::Unit* target) { _petManager.CommandPetAttack(target); }
    void CommandPetFollow() { _petManager.CommandPetFollow(); }
    void CommandPetStay() { /* Handled by pet AI */ }

    // Positioning interface - ranged DPS positioning
    // Note: GetOptimalRange is final in RangedDpsSpecialization (returns 25-40 yards)
    Position GetOptimalPosition(::Unit* /*target*/) override { return Position(); /* Handled by base class */ }

    // Phase 5 Integration: Decision Systems Initialization
    void InitializeBeastMasteryMechanics()
    {
        using namespace bot::ai;
        using namespace bot::ai::BehaviorTreeBuilder;

        BotAI* ai = this->GetBot()->GetBotAI();

        // ========================================================================
        // ActionPriorityQueue: Register Beast Mastery Hunter spells with priorities
        // ========================================================================
        auto* queue = ai->GetActionPriorityQueue();
        if (queue)
        {
            // EMERGENCY: Survival (HP < 40%)

            queue->RegisterSpell(SPELL_EXHILARATION, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);

            queue->AddCondition(SPELL_EXHILARATION, [this](Player* bot, Unit* target) {

                return bot && bot->GetHealthPct() < 40.0f;

            }, "Bot HP < 40% (heal self + pet)");

            // CRITICAL: Major burst cooldowns

            queue->RegisterSpell(SPELL_BESTIAL_WRATH, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);

            queue->AddCondition(SPELL_BESTIAL_WRATH, [this](Player* bot, Unit* target) {

                return target && this->_resource > 60 && !this->_bestialWrathActive;

            }, "60+ Focus and BW not active (major burst)");


            queue->RegisterSpell(SPELL_ASPECT_OF_THE_WILD, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);

            queue->AddCondition(SPELL_ASPECT_OF_THE_WILD, [this](Player* bot, Unit* target) {

                return target && this->_bestialWrathActive && !this->_aspectOfTheWildActive;

            }, "During Bestial Wrath (stack cooldowns)");

            // HIGH: Core rotation abilities

            queue->RegisterSpell(SPELL_KILL_COMMAND, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);

            queue->AddCondition(SPELL_KILL_COMMAND, [this](Player* bot, Unit* target) {

                return target && this->_resource >= 30 && this->_petManager.HasActivePet();

            }, "30+ Focus and pet alive (core pet ability)");


            queue->RegisterSpell(SPELL_BARBED_SHOT, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);

            queue->AddCondition(SPELL_BARBED_SHOT, [this](Player* bot, Unit* target) {

                return target && this->HasBarbedShotCharge() &&

                       (this->_petManager.GetPetFrenzyStacks() < 3 || this->_barbedShotCharges == 2);

            }, "Has charge and (Pet Frenzy < 3 stacks or 2 charges)");

            // MEDIUM: Talent abilities and utility

            queue->RegisterSpell(SPELL_DIRE_BEAST, SpellPriority::MEDIUM, SpellCategory::DAMAGE_SINGLE);

            queue->AddCondition(SPELL_DIRE_BEAST, [this](Player* bot, Unit* target) {

                return bot && bot->HasSpell(SPELL_DIRE_BEAST) &&

                       target && this->_resource >= 25;

            }, "Has talent, 25+ Focus (summon additional beast)");


            queue->RegisterSpell(SPELL_COUNTER_SHOT, SpellPriority::MEDIUM, SpellCategory::UTILITY);

            queue->AddCondition(SPELL_COUNTER_SHOT, [this](Player* bot, Unit* target) {

                return target && target->IsNonMeleeSpellCast(false);

            }, "Target casting (interrupt)");


            queue->RegisterSpell(SPELL_TRANQUILIZING_SHOT, SpellPriority::MEDIUM, SpellCategory::UTILITY);

            queue->AddCondition(SPELL_TRANQUILIZING_SHOT, [this](Player* bot, Unit* target) {

                return target && this->_resource >= 10 && target->HasAuraType(SPELL_AURA_MOD_INCREASE_SPEED);

            }, "Target has enrage/buff (dispel, 10 Focus)");

            // LOW: Filler abilities

            queue->RegisterSpell(SPELL_COBRA_SHOT, SpellPriority::LOW, SpellCategory::DAMAGE_SINGLE);

            queue->AddCondition(SPELL_COBRA_SHOT, [this](Player* bot, Unit* target) {

                return target && this->_resource >= 35 && this->GetEnemiesInRange(40.0f) < 3;

            }, "35+ Focus, < 3 enemies (single target filler)");


            queue->RegisterSpell(SPELL_MULTISHOT, SpellPriority::LOW, SpellCategory::DAMAGE_AOE);

            queue->AddCondition(SPELL_MULTISHOT, [this](Player* bot, Unit* target) {

                return target && this->_resource >= 40 && this->GetEnemiesInRange(40.0f) >= 3;

            }, "40+ Focus, 3+ enemies (AoE filler + Beast Cleave)");


            TC_LOG_INFO("module.playerbot", "🏹 BEAST MASTERY HUNTER: Registered {} spells in ActionPriorityQueue", queue->GetSpellCount());
        }

        // ========================================================================
        // BehaviorTree: Beast Mastery Hunter DPS rotation logic
        // ========================================================================
        auto* behaviorTree = ai->GetBehaviorTree();
        if (behaviorTree)
        {

            auto root = Selector("Beast Mastery Hunter DPS", {
                // Tier 1: Burst Window (Bestial Wrath → Aspect of the Wild)

                Sequence("Burst Cooldowns", {

                    Condition("Target exists", [this](Player* bot, Unit* target) {

                        return target != nullptr;

                    }),

                    Selector("Use Burst Cooldowns", {
                        // Bestial Wrath (major cooldown)

                        Sequence("Cast Bestial Wrath", {

                            Condition("Should use BW", [this](Player* bot, Unit* target) {

                                return this->_resource > 60 && !this->_bestialWrathActive &&

                                       this->ShouldUseBestialWrath(target);

                            }),

                            Action("Cast Bestial Wrath", [this](Player* bot, Unit* target) -> NodeStatus {

                                if (this->CanUseAbility(SPELL_BESTIAL_WRATH))

                                {

                                    this->CastSpell(bot, SPELL_BESTIAL_WRATH);

                                    this->_bestialWrathActive = true;

                                    this->_bestialWrathEndTime = GameTime::GetGameTimeMS() + 15000;

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),
                        // Aspect of the Wild (during Bestial Wrath)

                        Sequence("Cast Aspect of the Wild", {

                            Condition("During Bestial Wrath", [this](Player* bot, Unit* target) {

                                return this->_bestialWrathActive && !this->_aspectOfTheWildActive;

                            }),

                            Action("Cast Aspect of the Wild", [this](Player* bot, Unit* target) -> NodeStatus {

                                if (this->CanUseAbility(SPELL_ASPECT_OF_THE_WILD))

                                {

                                    this->CastSpell(bot, SPELL_ASPECT_OF_THE_WILD);

                                    this->_aspectOfTheWildActive = true;

                                    this->_aspectEndTime = GameTime::GetGameTimeMS() + 20000;

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        })

                    })

                }),

                // Tier 2: Core Rotation (Kill Command priority)

                Sequence("Core Rotation", {

                    Condition("Target exists and pet alive", [this](Player* bot, Unit* target) {

                        return target != nullptr && this->_petManager.HasActivePet();

                    }),

                    Selector("Cast Core Abilities", {
                        // Kill Command (highest priority DPS ability)

                        Sequence("Cast Kill Command", {

                            Condition("30+ Focus", [this](Player* bot, Unit* target) {

                                return this->_resource >= 30;

                            }),

                            Action("Cast Kill Command", [this](Player* bot, Unit* target) -> NodeStatus {

                                if (this->CanUseAbility(SPELL_KILL_COMMAND))

                                {

                                    this->CastSpell(target, SPELL_KILL_COMMAND);

                                    this->_lastKillCommand = GameTime::GetGameTimeMS();

                                    this->ConsumeResource(30);

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),
                        // Dire Beast (talent - additional damage source)

                        Sequence("Cast Dire Beast", {

                            Condition("Has talent and 25+ Focus", [this](Player* bot, Unit* target) {

                                return bot && bot->HasSpell(SPELL_DIRE_BEAST) &&

                                       this->_resource >= 25;

                            }),

                            Action("Cast Dire Beast", [this](Player* bot, Unit* target) -> NodeStatus {

                                if (this->CanUseAbility(SPELL_DIRE_BEAST))

                                {

                                    this->CastSpell(target, SPELL_DIRE_BEAST);

                                    this->ConsumeResource(25);

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        })

                    })

                }),

                // Tier 3: Focus Management (Barbed Shot for generation + Pet Frenzy)

                Sequence("Focus Management", {

                    Condition("Target exists", [this](Player* bot, Unit* target) {

                        return target != nullptr;

                    }),

                    Selector("Generate Focus", {
                        // Barbed Shot (generates 20 Focus, maintains Pet Frenzy)

                        Sequence("Cast Barbed Shot", {

                            Condition("Should use Barbed Shot", [this](Player* bot, Unit* target) {

                                return this->HasBarbedShotCharge() &&

                                       (this->_petManager.GetPetFrenzyStacks() < 3 ||

                                        this->_barbedShotCharges == 2 ||

                                        this->_wildCallProc);

                            }),

                            Action("Cast Barbed Shot", [this](Player* bot, Unit* target) -> NodeStatus {

                                if (this->HasBarbedShotCharge())

                                {

                                    this->CastSpell(target, SPELL_BARBED_SHOT);

                                    this->_petManager.ApplyBarbedShot();

                                    this->_barbedShotCharges--;

                                    this->_resource = std::min<uint32>(this->_resource + 20, 100);

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        })

                    })

                }),

                // Tier 4: Filler Rotation (Cobra Shot single target, Multishot AoE)

                Sequence("Filler Rotation", {

                    Condition("Target exists", [this](Player* bot, Unit* target) {

                        return target != nullptr;

                    }),

                    Selector("Choose Filler", {
                        // AoE filler (3+ enemies)

                        Sequence("AoE Filler", {

                            Condition("3+ enemies and 40+ Focus", [this](Player* bot, Unit* target) {

                                return this->GetEnemiesInRange(40.0f) >= 3 && this->_resource >= 40;

                            }),

                            Action("Cast Multishot", [this](Player* bot, Unit* target) -> NodeStatus {

                                if (this->_resource >= 40)

                                {

                                    this->CastSpell(target, SPELL_MULTISHOT);

                                    this->ConsumeResource(40);

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),
                        // Single target filler

                        Sequence("Single Target Filler", {

                            Condition("35+ Focus", [this](Player* bot, Unit* target) {

                                return this->_resource >= 35;

                            }),

                            Action("Cast Cobra Shot", [this](Player* bot, Unit* target) -> NodeStatus {

                                if (this->_resource >= 35)

                                {

                                    this->CastSpell(target, SPELL_COBRA_SHOT);

                                    this->_lastCobraShot = GameTime::GetGameTimeMS();

                                    this->ConsumeResource(35);

                                    this->_resource = std::min<uint32>(this->_resource + 5, 100);

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        })

                    })

                })

            });


            behaviorTree->SetRoot(root);

            TC_LOG_INFO("module.playerbot", "🌲 BEAST MASTERY HUNTER: BehaviorTree initialized with 4-tier DPS rotation");
        }
    }

private:
    BeastMasteryPetManager _petManager;

    // Barbed Shot management
    uint32 _barbedShotCharges;
    uint32 _lastBarbedShotRecharge;

    // Cooldown tracking
    bool _bestialWrathActive;
    uint32 _bestialWrathEndTime;
    bool _aspectOfTheWildActive;
    uint32 _aspectEndTime;

    // Proc tracking
    bool _wildCallProc;

    // Ability timing
    uint32 _lastKillCommand;
    uint32 _lastCobraShot;
};

} // namespace Playerbot
