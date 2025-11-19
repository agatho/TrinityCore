/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * Brewmaster Monk Refactored - Template-Based Implementation
 *
 * This file provides a complete, template-based implementation of Brewmaster Monk
 * using the TankSpecialization with dual resource system (Energy + Chi).
 */

#pragma once


#include "../Common/StatusEffectTracker.h"
#include "../Common/CooldownManager.h"
#include "../Common/RotationHelpers.h"
#include "../CombatSpecializationTemplates.h"
#include "../ResourceTypes.h"
#include "../../Services/ThreatAssistant.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "Log.h"
#include "../Decision/ActionPriorityQueue.h"
#include "../Decision/BehaviorTree.h"
#include "../BotAI.h"

namespace Playerbot
{

// ============================================================================
// BREWMASTER MONK SPELL IDs (WoW 11.2 - The War Within)
// ============================================================================

enum BrewmasterMonkSpells
{
    // Chi Generators
    KEG_SMASH                = 121253,  // 40 Energy, 8 sec CD, generates 2 Chi
    TIGER_PALM_BREW          = 100780,  // 25 Energy, generates 2 Chi
    EXPEL_HARM_BREW          = 322101,  // 15 Energy, generates 1 Chi, self-heal
    CHI_WAVE                 = 115098,  // 15 sec CD, generates 1 Chi (talent)
    CHI_BURST                = 123986,  // 30 sec CD, generates 1 Chi (talent)

    // Chi Spenders
    BLACKOUT_KICK_BREW       = 205523,  // 1-3 Chi, reduces brew cooldown
    BREATH_OF_FIRE           = 115181,  // 2 Chi, cone AoE + DoT
    SPINNING_CRANE_KICK_BREW = 322729,  // 2 Chi, AoE channel

    // Active Mitigation (Brews)
    PURIFYING_BREW           = 119582,  // Removes Stagger damage
    CELESTIAL_BREW           = 322507,  // 1 min CD, absorb shield
    FORTIFYING_BREW_BREW     = 115203,  // 6 min CD, damage reduction + max HP

    // Stagger Management
    IRONSKIN_BREW            = 115308,  // Increases Stagger effectiveness
    SHUFFLE                  = 215479,  // Buff from Blackout Kick

    // Threat Generation
    PROVOKE                  = 115546,  // Taunt
    RISING_SUN_KICK_BREW     = 107428,  // 2 Chi, threat modifier

    // Major Cooldowns
    INVOKE_NIUZAO            = 132578,  // 3 min CD, summon statue (talent)
    WEAPONS_OF_ORDER         = 387184,  // 2 min CD, damage/defense buff (talent)
    BONEDUST_BREW            = 386276,  // 1 min CD, damage amp (talent)

    // Utility
    TRANSCENDENCE            = 101643,  // Teleport anchor
    TRANSCENDENCE_TRANSFER   = 119996,  // Teleport to anchor
    ROLL                     = 109132,  // Mobility
    TIGER_LUST               = 116841,  // Sprint + snare removal
    DETOX                    = 218164,  // Dispel poison/disease

    // Defensive Cooldowns
    DAMPEN_HARM              = 122278,  // 2 min CD, damage reduction
    ZEN_MEDITATION           = 115176,  // 5 min CD, channel massive DR
    DIFFUSE_MAGIC            = 122783,  // 1.5 min CD, magic immunity (talent)

    // Procs and Buffs
    ELUSIVE_BRAWLER          = 195630,  // Passive dodge stacks
    GIFT_OF_THE_OX           = 124502,  // Healing orbs
    COUNTERSTRIKE            = 383800,  // Parry proc

    // Talents
    BLACK_OX_BREW            = 115399,  // Resets brew cooldowns
    CHARRED_PASSIONS         = 386965,  // Breath of Fire enhancement
    EXPLODING_KEG            = 325153   // Keg Smash knockdown
};

// Dual resource type for Monk
struct EnergyChiResource
{
    uint32 energy{0};
    uint32 chi{0};
    uint32 maxEnergy{100};
    uint32 maxChi{6};

    bool available{true};
    bool Consume(uint32 energyCost) {
        if (energy >= energyCost) {
            energy -= energyCost;
            return true;
        }
        return false;
    }
    void Regenerate(uint32 diff) {
        // Resource regeneration logic (simplified)
        available = true;
    }

    [[nodiscard]] uint32 GetAvailable() const {
        return 100; // Simplified - override in specific implementations
    }

    [[nodiscard]] uint32 GetMax() const {
        return 100; // Simplified - override in specific implementations
    }


    void Initialize(Player* bot) {
        if (bot) {
            maxEnergy = bot->GetMaxPower(POWER_ENERGY);
            energy = bot->GetPower(POWER_ENERGY);        }
        chi = 0;
    }
};

// ============================================================================
// BREWMASTER STAGGER TRACKER
// ============================================================================

class BrewmasterStaggerTracker
{
public:
    BrewmasterStaggerTracker()
        : _staggerAmount(0)
        , _staggerPercent(0.0f)
        , _lastStaggerCheck(0)
    {
    }

    void UpdateStagger(Player* bot)
    {
        if (!bot)
            return;

        uint32 now = GameTime::GetGameTimeMS();
        if (now - _lastStaggerCheck < 100) // Throttle checks
            return;

        _lastStaggerCheck = now;

        // Check for Stagger aura
        if (Aura* stagger = bot->GetAura(124255)) // Light Stagger
        {
            _staggerAmount = stagger->GetEffect(0)->GetAmount();
            _staggerPercent = (_staggerAmount * 100.0f) / bot->GetMaxHealth();
        }
        else if (Aura* stagger = bot->GetAura(124274)) // Moderate Stagger
        {
            _staggerAmount = stagger->GetEffect(0)->GetAmount();
            _staggerPercent = (_staggerAmount * 100.0f) / bot->GetMaxHealth();
        }
        else if (Aura* stagger = bot->GetAura(124273)) // Heavy Stagger
        {
            _staggerAmount = stagger->GetEffect(0)->GetAmount();
            _staggerPercent = (_staggerAmount * 100.0f) / bot->GetMaxHealth();
        }
        else
        {
            _staggerAmount = 0;
            _staggerPercent = 0.0f;
        }
    }

    uint32 GetStaggerAmount() const { return _staggerAmount; }
    float GetStaggerPercent() const { return _staggerPercent; }

    bool IsHeavyStagger() const { return _staggerPercent > 6.0f; }
    bool IsModerateStagger() const { return _staggerPercent > 3.0f; }
    bool ShouldPurify() const { return _staggerPercent > 4.0f; } // Purify at 4%+ of max HP

private:
    CooldownManager _cooldowns;
    uint32 _staggerAmount;
    float _staggerPercent;
    uint32 _lastStaggerCheck;
};

// ============================================================================
// BREWMASTER SHUFFLE TRACKER
// ============================================================================

class BrewmasterShuffleTracker
{
public:
    BrewmasterShuffleTracker()
        : _shuffleActive(false)
        , _shuffleEndTime(0)
    {
    }

    void ApplyShuffle()
    {
        _shuffleActive = true;
        _shuffleEndTime = GameTime::GetGameTimeMS() + 5000; // 5 sec base duration
    }

    void ExtendShuffle(uint32 durationMs)
    {
        if (_shuffleActive)
            _shuffleEndTime += durationMs;
        else
        {
            _shuffleActive = true;
            _shuffleEndTime = GameTime::GetGameTimeMS() + durationMs;
        }
    }

    bool IsActive() const { return _shuffleActive; }

    uint32 GetTimeRemaining() const
    {
        if (!_shuffleActive)
            return 0;

        uint32 now = GameTime::GetGameTimeMS();
        return _shuffleEndTime > now ? _shuffleEndTime - now : 0;
    }

    bool NeedsRefresh() const
    {
        // Refresh if less than 2 seconds remaining
        return !_shuffleActive || GetTimeRemaining() < 2000;
    }

    void Update()
    {
        if (_shuffleActive && GameTime::GetGameTimeMS() >= _shuffleEndTime)
        {
            _shuffleActive = false;
            _shuffleEndTime = 0;
        }
    }

private:
    bool _shuffleActive;
    uint32 _shuffleEndTime;
};

// ============================================================================
// BREWMASTER MONK REFACTORED
// ============================================================================

class BrewmasterMonkRefactored : public TankSpecialization<EnergyChiResource>, public MonkSpecialization
{
public:
    using Base = TankSpecialization<EnergyChiResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit BrewmasterMonkRefactored(Player* bot)        : TankSpecialization<EnergyChiResource>(bot)
        , MonkSpecialization(bot)
        , _staggerTracker()
        , _shuffleTracker()
        , _ironskinBrewActive(false)
        , _ironskinEndTime(0)
        , _lastKegSmashTime(0)
    {        // Initialize energy/chi resources
        this->_resource.Initialize(bot);

        // Phase 5: Initialize decision systems
        InitializeBrewmasterMechanics();

        TC_LOG_DEBUG("playerbot", "BrewmasterMonkRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))
            return;

        // Update Brewmaster state
        UpdateBrewmasterState();

        // Handle active mitigation first
        HandleActiveMitigation();

        // Determine if AoE or single target
        uint32 enemyCount = this->GetEnemiesInRange(8.0f);
        if (enemyCount >= 3)
        {
            ExecuteAoEThreatRotation(target, enemyCount);
        }
        else
        {
            ExecuteSingleTargetThreatRotation(target);
        }
    }    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Emergency defensives
        HandleEmergencyDefensives();
    }

    // Phase 5C: Threat management using ThreatAssistant service
    void OnTauntRequired(::Unit* target) override    {
        // Use ThreatAssistant to determine best taunt target and execute
        Unit* tauntTarget = target ? target : bot::ai::ThreatAssistant::GetTauntTarget(this->GetBot());
        if (tauntTarget && this->CanCastSpell(PROVOKE, tauntTarget))
        {
            bot::ai::ThreatAssistant::ExecuteTaunt(this->GetBot(), tauntTarget, PROVOKE);
            TC_LOG_DEBUG("playerbot", "Brewmaster: Provoke taunt via ThreatAssistant on {}", tauntTarget->GetName());
        }
    }

    float GetOptimalRange(::Unit* target) override
    {
        return 5.0f; // Melee range for tanking
    }

protected:
    void ExecuteSingleTargetThreatRotation(::Unit* target)
    {
        uint32 energy = this->_resource.energy;
        uint32 chi = this->_resource.chi;

        // Priority 1: Maintain Shuffle
        if (_shuffleTracker.NeedsRefresh() && chi >= 1)
        {
            if (this->CanCastSpell(BLACKOUT_KICK_BREW, target))
            {
                this->CastSpell(target, BLACKOUT_KICK_BREW);
                _shuffleTracker.ApplyShuffle();
                ConsumeChi(1);
                return;
            }
        }

        // Priority 2: Keg Smash on cooldown (best Chi generator + threat)
        if (energy >= 40 && chi < 5 && this->CanCastSpell(KEG_SMASH, target))
        {
            this->CastSpell(target, KEG_SMASH);
            _lastKegSmashTime = GameTime::GetGameTimeMS();
            GenerateChi(2);
            return;
        }

        // Priority 3: Breath of Fire (after Keg Smash for ignite)
        if (chi >= 2 && GameTime::GetGameTimeMS() - _lastKegSmashTime < 2000)
        {
            if (this->CanCastSpell(BREATH_OF_FIRE, target))
            {
                this->CastSpell(target, BREATH_OF_FIRE);
                ConsumeChi(2);
                return;
            }
        }

        // Priority 4: Rising Sun Kick for threat
        if (chi >= 2 && this->CanCastSpell(RISING_SUN_KICK_BREW, target))
        {
            this->CastSpell(target, RISING_SUN_KICK_BREW);
            ConsumeChi(2);
            return;
        }

        // Priority 5: Blackout Kick to spend Chi
        if (chi >= 3 && this->CanCastSpell(BLACKOUT_KICK_BREW, target))
        {
            this->CastSpell(target, BLACKOUT_KICK_BREW);
            _shuffleTracker.ExtendShuffle(5000);
            ConsumeChi(1);
            return;
        }

        // Priority 6: Tiger Palm for Chi generation
        if (energy >= 25 && chi < 5 && this->CanCastSpell(TIGER_PALM_BREW, target))
        {
            this->CastSpell(target, TIGER_PALM_BREW);
            GenerateChi(2);
            return;
        }

        // Priority 7: Expel Harm (heal + Chi)
        if (energy >= 15 && chi < 5 && this->GetBot()->GetHealthPct() < 90.0f)
        {
            if (this->CanCastSpell(EXPEL_HARM_BREW, this->GetBot()))
            {
                this->CastSpell(EXPEL_HARM_BREW, this->GetBot());
                GenerateChi(1);
                return;
            }
        }
    }

    void ExecuteAoEThreatRotation(::Unit* target, uint32 enemyCount)
    {
        uint32 energy = this->_resource.energy;
        uint32 chi = this->_resource.chi;

        // Priority 1: Maintain Shuffle
        if (_shuffleTracker.NeedsRefresh() && chi >= 1)
        {
            if (this->CanCastSpell(BLACKOUT_KICK_BREW, target))
            {
                this->CastSpell(target, BLACKOUT_KICK_BREW);
                _shuffleTracker.ApplyShuffle();
                ConsumeChi(1);
                return;
            }
        }

        // Priority 2: Keg Smash (AoE Chi generator)
        if (energy >= 40 && chi < 5 && this->CanCastSpell(KEG_SMASH, target))
        {
            this->CastSpell(target, KEG_SMASH);
            _lastKegSmashTime = GameTime::GetGameTimeMS();
            GenerateChi(2);
            return;
        }

        // Priority 3: Breath of Fire (AoE + DoT)
        if (chi >= 2 && this->CanCastSpell(BREATH_OF_FIRE, target))
        {
            this->CastSpell(target, BREATH_OF_FIRE);
            ConsumeChi(2);
            return;
        }

        // Priority 4: Spinning Crane Kick (AoE Chi spender)
        if (chi >= 2 && enemyCount >= 4 && this->CanCastSpell(SPINNING_CRANE_KICK_BREW, this->GetBot()))
        {
            this->CastSpell(SPINNING_CRANE_KICK_BREW, this->GetBot());
            ConsumeChi(2);
            return;
        }

        // Priority 5: Tiger Palm for Chi generation
        if (energy >= 25 && chi < 5 && this->CanCastSpell(TIGER_PALM_BREW, target))
        {
            this->CastSpell(target, TIGER_PALM_BREW);
            GenerateChi(2);
            return;
        }
    }

    void HandleActiveMitigation()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Update Ironskin Brew status
        UpdateIronskinBrew();

        // Priority 1: Purify heavy Stagger
        if (_staggerTracker.ShouldPurify() && this->CanCastSpell(PURIFYING_BREW, bot))
        {
            this->CastSpell(bot, PURIFYING_BREW);
            TC_LOG_DEBUG("playerbot", "Brewmaster: Purifying Brew used - Stagger at {:.1f}%", _staggerTracker.GetStaggerPercent());
            return;
        }

        // Priority 2: Maintain Ironskin Brew
        if (!_ironskinBrewActive || GetIronskinTimeRemaining() < 3000)
        {
            if (this->CanCastSpell(IRONSKIN_BREW, bot))
            {
                this->CastSpell(bot, IRONSKIN_BREW);
                _ironskinBrewActive = true;
                _ironskinEndTime = GameTime::GetGameTimeMS() + 7000; // 7 sec duration
                

        // Register cooldowns using CooldownManager
        _cooldowns.RegisterBatch({
            {KEG_SMASH, CooldownPresets::DISPEL, 1},
            {PROVOKE, CooldownPresets::DISPEL, 1},
            {PURIFYING_BREW, 20000, 1},
            {CELESTIAL_BREW, CooldownPresets::OFFENSIVE_60, 1},
            {FORTIFYING_BREW_BREW, 360000, 1},
            {DAMPEN_HARM, CooldownPresets::MINOR_OFFENSIVE, 1},
            {ZEN_MEDITATION, 300000, 1},
            {INVOKE_NIUZAO, CooldownPresets::MAJOR_OFFENSIVE, 1},
            {WEAPONS_OF_ORDER, CooldownPresets::MINOR_OFFENSIVE, 1},
            {BONEDUST_BREW, CooldownPresets::OFFENSIVE_60, 1},
        });

        TC_LOG_DEBUG("playerbot", "Brewmaster: Ironskin Brew applied");
                return;
            }
        }
    }

    void HandleEmergencyDefensives()
    {
        Player* bot = this->GetBot();
        float healthPct = bot->GetHealthPct();

        // Critical: Fortifying Brew
        if (healthPct < 25.0f && this->CanCastSpell(FORTIFYING_BREW_BREW, bot))
        {
            this->CastSpell(bot, FORTIFYING_BREW_BREW);
            TC_LOG_DEBUG("playerbot", "Brewmaster: Fortifying Brew emergency");
            return;
        }

        // Very low: Celestial Brew (absorb shield)
        if (healthPct < 40.0f && this->CanCastSpell(CELESTIAL_BREW, bot))
        {
            this->CastSpell(bot, CELESTIAL_BREW);
            TC_LOG_DEBUG("playerbot", "Brewmaster: Celestial Brew shield");
            return;
        }

        // Low: Zen Meditation (channeled DR)
        if (healthPct < 30.0f && this->CanCastSpell(ZEN_MEDITATION, bot))
        {
            this->CastSpell(bot, ZEN_MEDITATION);
            TC_LOG_DEBUG("playerbot", "Brewmaster: Zen Meditation");
            return;
        }

        // Moderate: Dampen Harm
        if (healthPct < 50.0f && this->CanCastSpell(DAMPEN_HARM, bot))
        {
            this->CastSpell(bot, DAMPEN_HARM);
            TC_LOG_DEBUG("playerbot", "Brewmaster: Dampen Harm");
            return;
        }
    }

private:
    void UpdateBrewmasterState()
    {
        // Update Stagger tracker
        _staggerTracker.UpdateStagger(this->GetBot());

        // Update Shuffle tracker
        _shuffleTracker.Update();

        // Update Ironskin Brew
        if (_ironskinBrewActive && GameTime::GetGameTimeMS() >= _ironskinEndTime)
        {
            _ironskinBrewActive = false;
            _ironskinEndTime = 0;
        }

        // Update Chi from bot
        if (this->GetBot())
        {
            this->_resource.chi = this->GetBot()->GetPower(POWER_CHI);
            this->_resource.energy = this->GetBot()->GetPower(POWER_ENERGY);
        }
    }

    void UpdateIronskinBrew()
    {
        if (this->GetBot() && this->GetBot()->HasAura(IRONSKIN_BREW))
        {
            _ironskinBrewActive = true;
        }
        else
        {
            _ironskinBrewActive = false;
            _ironskinEndTime = 0;
        }
    }

    uint32 GetIronskinTimeRemaining() const
    {
        if (!_ironskinBrewActive)
            return 0;

        uint32 now = GameTime::GetGameTimeMS();
        return _ironskinEndTime > now ? _ironskinEndTime - now : 0;
    }

    void GenerateChi(uint32 amount)
    {
        this->_resource.chi = std::min(this->_resource.chi + amount, this->_resource.maxChi);
    }

    void ConsumeChi(uint32 amount)
    {
        this->_resource.chi = (this->_resource.chi > amount) ? this->_resource.chi - amount : 0;
    }

    // ========================================================================
    // PHASE 5: DECISION SYSTEM INTEGRATION
    // ========================================================================

    void InitializeBrewmasterMechanics()
    {
        using namespace bot::ai;
        using namespace bot::ai::BehaviorTreeBuilder;

        BotAI* ai = this->GetBot()->GetBotAI();
        if (!ai) return;

        auto* queue = ai->GetActionPriorityQueue();
        if (queue)
        {
            // EMERGENCY: Major defensive cooldowns
            queue->RegisterSpell(ZEN_MEDITATION, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);
            queue->AddCondition(ZEN_MEDITATION, [](Player* bot, Unit*) {
                return bot && bot->GetHealthPct() < 20.0f;
            }, "HP < 20% (channel 60% DR)");

            queue->RegisterSpell(FORTIFYING_BREW_BREW, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);
            queue->AddCondition(FORTIFYING_BREW_BREW, [](Player* bot, Unit*) {
                return bot && bot->GetHealthPct() < 35.0f;
            }, "HP < 35% (20% DR + 20% HP, 6min CD)");

            // CRITICAL: Active mitigation
            queue->RegisterSpell(PURIFYING_BREW, SpellPriority::CRITICAL, SpellCategory::DEFENSIVE);
            queue->AddCondition(PURIFYING_BREW, [this](Player*, Unit*) {
                return this->_staggerTracker.ShouldPurify();
            }, "Stagger > 4% max HP (clear stagger)");

            queue->RegisterSpell(CELESTIAL_BREW, SpellPriority::CRITICAL, SpellCategory::DEFENSIVE);
            queue->AddCondition(CELESTIAL_BREW, [](Player* bot, Unit*) {
                return bot && bot->GetHealthPct() < 60.0f;
            }, "HP < 60% (absorb shield, 1min CD)");

            queue->RegisterSpell(IRONSKIN_BREW, SpellPriority::CRITICAL, SpellCategory::DEFENSIVE);
            queue->AddCondition(IRONSKIN_BREW, [this](Player*, Unit*) {
                return !this->_ironskinBrewActive || this->GetIronskinTimeRemaining() < 3000;
            }, "Ironskin down or < 3s (increases stagger)");

            // HIGH: Shuffle maintenance + threat
            queue->RegisterSpell(BLACKOUT_KICK_BREW, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(BLACKOUT_KICK_BREW, [this](Player*, Unit* target) {
                return target && this->_resource.chi >= 1 && this->_shuffleTracker.NeedsRefresh();
            }, "1 chi, Shuffle < 2s (maintain buff)");

            queue->RegisterSpell(KEG_SMASH, SpellPriority::HIGH, SpellCategory::DAMAGE_AOE);
            queue->AddCondition(KEG_SMASH, [this](Player*, Unit* target) {
                return target && this->_resource.energy >= 40 && this->_resource.chi < 5;
            }, "40 energy, chi < 5 (generates 2 chi + threat)");

            queue->RegisterSpell(PROVOKE, SpellPriority::HIGH, SpellCategory::UTILITY);
            queue->AddCondition(PROVOKE, [](Player*, Unit* target) {
                return target != nullptr;
            }, "Taunt (ThreatAssistant determines need)");

            // MEDIUM: Chi spenders and threat
            queue->RegisterSpell(BREATH_OF_FIRE, SpellPriority::MEDIUM, SpellCategory::DAMAGE_AOE);
            queue->AddCondition(BREATH_OF_FIRE, [this](Player*, Unit* target) {
                return target && this->_resource.chi >= 2 && (GameTime::GetGameTimeMS() - this->_lastKegSmashTime) < 2000;
            }, "2 chi, after Keg Smash (cone + DoT)");

            queue->RegisterSpell(SPINNING_CRANE_KICK_BREW, SpellPriority::MEDIUM, SpellCategory::DAMAGE_AOE);
            queue->AddCondition(SPINNING_CRANE_KICK_BREW, [this](Player*, Unit*) {
                return this->_resource.chi >= 2 && this->GetEnemiesInRange(8.0f) >= 3;
            }, "2 chi, 3+ enemies (AoE channel)");

            queue->RegisterSpell(RISING_SUN_KICK_BREW, SpellPriority::MEDIUM, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(RISING_SUN_KICK_BREW, [this](Player*, Unit* target) {
                return target && this->_resource.chi >= 2;
            }, "2 chi (high threat)");

            // LOW: Chi generators
            queue->RegisterSpell(TIGER_PALM_BREW, SpellPriority::LOW, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(TIGER_PALM_BREW, [this](Player*, Unit* target) {
                return target && this->_resource.energy >= 25 && this->_resource.chi < 5;
            }, "25 energy, chi < 5 (generates 2 chi)");

            queue->RegisterSpell(EXPEL_HARM_BREW, SpellPriority::LOW, SpellCategory::HEALING);
            queue->AddCondition(EXPEL_HARM_BREW, [this](Player* bot, Unit*) {
                return bot && bot->GetHealthPct() < 90.0f && this->_resource.energy >= 15 && this->_resource.chi < 5;
            }, "HP < 90%, 15 energy (heal + 1 chi)");

            // UTILITY: Major cooldowns
            queue->RegisterSpell(INVOKE_NIUZAO, SpellPriority::HIGH, SpellCategory::OFFENSIVE);
            queue->AddCondition(INVOKE_NIUZAO, [this](Player* bot, Unit* target) {
                return bot && target && bot->HasSpell(INVOKE_NIUZAO) && this->GetEnemiesInRange(10.0f) >= 2;
            }, "2+ enemies (summon statue, 3min CD)");

            queue->RegisterSpell(WEAPONS_OF_ORDER, SpellPriority::HIGH, SpellCategory::OFFENSIVE);
            queue->AddCondition(WEAPONS_OF_ORDER, [this](Player* bot, Unit* target) {
                return bot && target && bot->HasSpell(WEAPONS_OF_ORDER);
            }, "Burst window (damage/defense, 2min CD)");

            queue->RegisterSpell(DAMPEN_HARM, SpellPriority::HIGH, SpellCategory::DEFENSIVE);
            queue->AddCondition(DAMPEN_HARM, [](Player* bot, Unit*) {
                return bot && bot->GetHealthPct() < 50.0f;
            }, "HP < 50% (damage reduction, 2min CD)");
        }

        auto* behaviorTree = ai->GetBehaviorTree();
        if (behaviorTree)
        {
            auto root = Selector("Brewmaster Tank", {
                // Tier 1: Emergency Defensives
                Sequence("Emergency Defense", {
                    Condition("Critical HP", [](Player* bot) {
                        return bot && bot->GetHealthPct() < 35.0f;
                    }),
                    Selector("Use emergency", {
                        Sequence("Zen Meditation", {
                            Condition("HP < 20%", [](Player* bot) {
                                return bot->GetHealthPct() < 20.0f;
                            }),
                            Action("Cast Zen Meditation", [this](Player* bot) {
                                if (this->CanCastSpell(ZEN_MEDITATION, bot)) {
                                    this->CastSpell(bot, ZEN_MEDITATION);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Fortifying Brew", {
                            Action("Cast Fortifying Brew", [this](Player* bot) {
                                if (this->CanCastSpell(FORTIFYING_BREW_BREW, bot)) {
                                    this->CastSpell(bot, FORTIFYING_BREW_BREW);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 2: Active Mitigation (Stagger Management)
                Sequence("Stagger Management", {
                    Selector("Manage stagger", {
                        Sequence("Purifying Brew", {
                            Condition("Should purify", [this](Player*) {
                                return this->_staggerTracker.ShouldPurify();
                            }),
                            Action("Cast Purifying Brew", [this](Player* bot) {
                                if (this->CanCastSpell(PURIFYING_BREW, bot)) {
                                    this->CastSpell(bot, PURIFYING_BREW);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Celestial Brew", {
                            Condition("HP < 60%", [](Player* bot) {
                                return bot && bot->GetHealthPct() < 60.0f;
                            }),
                            Action("Cast Celestial Brew", [this](Player* bot) {
                                if (this->CanCastSpell(CELESTIAL_BREW, bot)) {
                                    this->CastSpell(bot, CELESTIAL_BREW);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Ironskin Brew", {
                            Condition("Needs refresh", [this](Player*) {
                                return !this->_ironskinBrewActive || this->GetIronskinTimeRemaining() < 3000;
                            }),
                            Action("Cast Ironskin Brew", [this](Player* bot) {
                                if (this->CanCastSpell(IRONSKIN_BREW, bot)) {
                                    this->CastSpell(bot, IRONSKIN_BREW);
                                    this->_ironskinBrewActive = true;
                                    this->_ironskinEndTime = GameTime::GetGameTimeMS() + 7000;
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 3: Shuffle Maintenance
                Sequence("Maintain Shuffle", {
                    Condition("Has target", [this](Player* bot) {
                        return bot && bot->GetVictim();
                    }),
                    Condition("Shuffle needs refresh", [this](Player*) {
                        return this->_shuffleTracker.NeedsRefresh();
                    }),
                    Condition("Has chi", [this](Player*) {
                        return this->_resource.chi >= 1;
                    }),
                    Action("Cast Blackout Kick", [this](Player* bot) {
                        Unit* target = bot->GetVictim();
                        if (target && this->CanCastSpell(BLACKOUT_KICK_BREW, target)) {
                            this->CastSpell(target, BLACKOUT_KICK_BREW);
                            this->_shuffleTracker.ApplyShuffle();
                            this->ConsumeChi(1);
                            return NodeStatus::SUCCESS;
                        }
                        return NodeStatus::FAILURE;
                    })
                }),

                // Tier 4: Chi Generation
                Sequence("Generate Chi", {
                    Condition("Has target", [this](Player* bot) {
                        return bot && bot->GetVictim();
                    }),
                    Condition("Chi < 5", [this](Player*) {
                        return this->_resource.chi < 5;
                    }),
                    Selector("Generate", {
                        Sequence("Keg Smash", {
                            Condition("40 energy", [this](Player*) {
                                return this->_resource.energy >= 40;
                            }),
                            Action("Cast Keg Smash", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(KEG_SMASH, target)) {
                                    this->CastSpell(target, KEG_SMASH);
                                    this->_lastKegSmashTime = GameTime::GetGameTimeMS();
                                    this->GenerateChi(2);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Tiger Palm", {
                            Condition("25 energy", [this](Player*) {
                                return this->_resource.energy >= 25;
                            }),
                            Action("Cast Tiger Palm", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(TIGER_PALM_BREW, target)) {
                                    this->CastSpell(target, TIGER_PALM_BREW);
                                    this->GenerateChi(2);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Expel Harm", {
                            Condition("15 energy + low HP", [this](Player* bot) {
                                return bot && this->_resource.energy >= 15 && bot->GetHealthPct() < 90.0f;
                            }),
                            Action("Cast Expel Harm", [this](Player* bot) {
                                if (this->CanCastSpell(EXPEL_HARM_BREW, bot)) {
                                    this->CastSpell(bot, EXPEL_HARM_BREW);
                                    this->GenerateChi(1);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 5: Chi Spenders (Threat + Damage)
                Sequence("Spend Chi", {
                    Condition("Has target", [this](Player* bot) {
                        return bot && bot->GetVictim();
                    }),
                    Condition("Has chi", [this](Player*) {
                        return this->_resource.chi >= 2;
                    }),
                    Selector("Spend", {
                        Sequence("Breath of Fire", {
                            Condition("After Keg Smash", [this](Player*) {
                                return (GameTime::GetGameTimeMS() - this->_lastKegSmashTime) < 2000;
                            }),
                            Action("Cast BoF", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(BREATH_OF_FIRE, target)) {
                                    this->CastSpell(target, BREATH_OF_FIRE);
                                    this->ConsumeChi(2);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Spinning Crane Kick", {
                            Condition("3+ enemies", [this](Player*) {
                                return this->GetEnemiesInRange(8.0f) >= 3;
                            }),
                            Action("Cast SCK", [this](Player* bot) {
                                if (this->CanCastSpell(SPINNING_CRANE_KICK_BREW, bot)) {
                                    this->CastSpell(bot, SPINNING_CRANE_KICK_BREW);
                                    this->ConsumeChi(2);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Rising Sun Kick", {
                            Action("Cast RSK", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(RISING_SUN_KICK_BREW, target)) {
                                    this->CastSpell(target, RISING_SUN_KICK_BREW);
                                    this->ConsumeChi(2);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                })
            });

            behaviorTree->SetRoot(root);
        }
    }

private:
    BrewmasterStaggerTracker _staggerTracker;
    BrewmasterShuffleTracker _shuffleTracker;
    bool _ironskinBrewActive;
    uint32 _ironskinEndTime;
    uint32 _lastKegSmashTime;
};

} // namespace Playerbot
