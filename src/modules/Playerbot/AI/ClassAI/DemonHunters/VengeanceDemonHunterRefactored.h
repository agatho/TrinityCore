/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * Vengeance Demon Hunter Refactored - Template-Based Implementation
 *
 * This file provides a complete, template-based implementation of Vengeance Demon Hunter
 * using the TankSpecialization<PainResource> base template.
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
#include "Group.h"
#include "ObjectAccessor.h"
#include "Log.h"
#include "DemonHunterAI.h"
#include "../Decision/ActionPriorityQueue.h"
#include "../Decision/BehaviorTree.h"
#include "../BotAI.h"

namespace Playerbot
{

// ============================================================================
// VENGEANCE DEMON HUNTER SPELL IDs (WoW 11.2 - The War Within)
// ============================================================================

enum VengeanceSpells
{
    // Pain Generators (SHEAR already defined in DemonHunterAI.h)
    FRACTURE                 = 263642,  // 25 Pain, generates Soul Fragments
    FEL_DEVASTATION          = 212084,  // Channel, generates Pain

    // Pain Spenders (SOUL_CLEAVE, SPIRIT_BOMB already defined in DemonHunterAI.h)
    FEL_DEVASTATION_SPENDER  = 212084,  // 50 Pain channel (talent)

    // Active Mitigation (DEMON_SPIKES, FIERY_BRAND already defined in DemonHunterAI.h)
    // SOUL_BARRIER already defined in DemonHunterAI.h
    // METAMORPHOSIS_VENGEANCE already defined in DemonHunterAI.h

    // Sigils (SIGIL_OF_FLAME already defined in DemonHunterAI.h)
    // SIGIL_OF_SILENCE already defined in DemonHunterAI.h
    // SIGIL_OF_MISERY already defined in DemonHunterAI.h
    SIGIL_OF_CHAINS          = 202138,  // AoE slow, 1.5 min CD

    // Threat and Utility
    INFERNAL_STRIKE          = 189110,  // 2 charges, leap
    THROW_GLAIVE_TANK        = 204157,  // Ranged threat
    TORMENT                  = 185245,  // Taunt
    CONSUME_MAGIC_TANK       = 278326,  // Purge

    // Defensive Cooldowns
    LAST_RESORT              = 209258,  // Cheat death (talent)
    FEL_DEVASTATION_DEF      = 212084,  // Leech healing

    // Passives/Procs
    IMMOLATION_AURA_TANK     = 258920,  // Passive AoE damage
    SOUL_FRAGMENTS_BUFF      = 203981,  // Soul Fragment tracking
    PAINBRINGER_BUFF         = 207407,  // Shear damage increase
    FRAILTY_DEBUFF           = 247456,  // Spirit Bomb debuff

    // Talents
    AGONIZING_FLAMES         = 207548,  // Fiery Brand spread
    BURNING_ALIVE            = 207739,  // Fiery Brand duration
    FEED_THE_DEMON           = 218612,  // Demon Spikes CDR
    SPIRIT_BOMB_TALENT       = 247454,  // Enables Spirit Bomb
    FRACTURE_TALENT          = 263642,  // Alternative Pain generator
    SOUL_BARRIER_TALENT      = 263648   // Shield from Soul Fragments
};

// Pain resource type (simple uint32)
using PainResource = uint32;

// ============================================================================
// VENGEANCE SOUL FRAGMENT MANAGER
// ============================================================================

/**
 * Manages Soul Fragment generation and consumption for Vengeance
 * Soul Fragments are the primary healing/defensive mechanic
 */
class VengeanceSoulFragmentManager
{
public:
    VengeanceSoulFragmentManager()
        : _fragmentCount(0)
        , _maxFragments(5)
        , _lastFragmentTime(0)
    {
    }

    void GenerateFragments(uint32 count)
    {
        _fragmentCount = std::min(_fragmentCount + count, _maxFragments);
        _lastFragmentTime = GameTime::GetGameTimeMS();
    }

    bool ConsumeFragments(uint32 count)
    {
        if (_fragmentCount >= count)
        {
            _fragmentCount -= count;
            return true;
        }
        return false;
    }

    bool ConsumeAllFragments()
    {
        if (_fragmentCount > 0)
        {
            _fragmentCount = 0;
            return true;
        }
        return false;
    }

    uint32 GetFragmentCount() const { return _fragmentCount; }
    bool HasFragments() const { return _fragmentCount > 0; }
    bool HasMinFragments(uint32 min) const { return _fragmentCount >= min; }

    void Update()
    {
        // Soul fragments expire after 20 seconds if not consumed
        if (_fragmentCount > 0 && GameTime::GetGameTimeMS() - _lastFragmentTime > 20000)
        {
            _fragmentCount = 0;
        }
    }

private:
    CooldownManager _cooldowns;
    uint32 _fragmentCount;
    const uint32 _maxFragments;
    uint32 _lastFragmentTime;
};

// ============================================================================
// VENGEANCE DEMON SPIKES TRACKER
// ============================================================================

/**
 * Tracks Demon Spikes charges and optimal usage timing
 * Demon Spikes is the primary active mitigation ability
 */
class VengeanceDemonSpikesTracker
{
public:
    VengeanceDemonSpikesTracker()
        : _charges(2)
        , _maxCharges(2)
        , _lastUseTime(0)
        , _lastRechargeTime(0)
        , _chargeCooldown(20000) // 20 sec recharge per charge
        , _duration(6000)        // 6 sec buff duration
        , _active(false)
        , _endTime(0)
    {
    }

    void Update()
    {
        uint32 now = GameTime::GetGameTimeMS();

        // Check if Demon Spikes buff expired
        if (_active && now >= _endTime)
        {
            _active = false;
            _endTime = 0;
        }

        // Recharge charges
        if (_charges < _maxCharges)
        {
            uint32 timeSinceRecharge = now - _lastRechargeTime;
            if (timeSinceRecharge >= _chargeCooldown)
            {
                _charges++;
                _lastRechargeTime = now;
            }
        }
    }

    bool CanUse() const
    {
        return _charges > 0 && !_active;
    }

    bool IsActive() const { return _active; }
    uint32 GetCharges() const { return _charges; }

    uint32 GetTimeRemaining() const
    {
        if (!_active)
            return 0;

        uint32 now = GameTime::GetGameTimeMS();
        return _endTime > now ? _endTime - now : 0;
    }

    void Use()
    {
        if (_charges > 0)
        {
            _charges--;
            _lastUseTime = GameTime::GetGameTimeMS();
            _active = true;
            _endTime = _lastUseTime + _duration;

            if (_charges == (_maxCharges - 1))
                _lastRechargeTime = _lastUseTime;
        }
    }

    bool ShouldUse(float incomingDamageRate, float healthPct) const
    {
        // Use Demon Spikes when:
        // 1. Taking heavy damage
        // 2. Health is low
        // 3. Not already active
        // 4. Have at least 1 charge

        if (!CanUse())
            return false;

        // Emergency usage at low health
        if (healthPct < 40.0f)
            return true;

        // Heavy damage usage
        if (incomingDamageRate > 30.0f) // Taking 30%+ health per second
            return true;

        // Maintain uptime if have 2 charges
        if (_charges == _maxCharges)
            return true;

        return false;
    }

private:
    uint32 _charges;
    const uint32 _maxCharges;
    uint32 _lastUseTime;
    uint32 _lastRechargeTime;
    const uint32 _chargeCooldown;
    const uint32 _duration;
    bool _active;
    uint32 _endTime;
};

// ============================================================================
// VENGEANCE DEMON HUNTER REFACTORED
// ============================================================================

/**
 * Complete Vengeance Demon Hunter implementation using template architecture
 * Inherits from TankSpecialization<PainResource> (uint32 resource)
 */
class VengeanceDemonHunterRefactored : public TankSpecialization<PainResource>
{
public:
    using Base = TankSpecialization<PainResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::IsSpellReady;
    using Base::ConsumeResource;
    using Base::_resource;
    explicit VengeanceDemonHunterRefactored(Player* bot)
        : TankSpecialization<PainResource>(bot)
        
        , _soulFragments()
        , _demonSpikes()
        , _lastShearTime(0)
        , _lastSoulCleaveTime(0)
        , _lastSigilOfFlameTime(0)
        , _fieryBrandActive(false)
        , _fieryBrandEndTime(0)
        , _metamorphosisActive(false)
        , _metamorphosisEndTime(0)
        , _immolationAuraActive(false)
    {
        // Initialize Pain resource
        _maxResource = 120; // Vengeance has 120 max Pain
        _resource = 0;      // Start with no Pain

        // Setup Vengeance-specific cooldown tracking
        InitializeCooldowns();

        // Phase 5: Initialize decision systems
        InitializeVengeanceMechanics();

        TC_LOG_DEBUG("playerbot", "VengeanceDemonHunterRefactored initialized for {}", bot->GetName());
    }

    // ========================================================================    // CORE ROTATION - Vengeance specific logic
    // ========================================================================

    void UpdateRotation(::Unit* target) override    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))
            return;

        // Update Vengeance-specific mechanics
        UpdateVengeanceState();

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
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();

        // Maintain Immolation Aura for passive threat/damage
        if (!_immolationAuraActive && this->CanCastSpell(DemonHunterSpells::IMMOLATION_AURA, bot))
        {
            this->CastSpell(bot, DemonHunterSpells::IMMOLATION_AURA);
            _immolationAuraActive = true;
        }        // Emergency defensive cooldowns
        HandleEmergencyDefensives();
    }

    // Phase 5C: Threat management using ThreatAssistant service
    void OnTauntRequired(::Unit* target)    {
        // Use ThreatAssistant to determine best taunt target and execute
        Unit* tauntTarget = target ? target : bot::ai::ThreatAssistant::GetTauntTarget(this->GetBot());
        if (tauntTarget && this->CanCastSpell(TORMENT, tauntTarget))
        {
            bot::ai::ThreatAssistant::ExecuteTaunt(this->GetBot(), tauntTarget, TORMENT);
            TC_LOG_DEBUG("playerbot", "Vengeance: Torment taunt via ThreatAssistant on {}", tauntTarget->GetName());
        }
    }

protected:
    // ========================================================================
    // RESOURCE MANAGEMENT OVERRIDE
    // ========================================================================

    uint32 GetSpellResourceCost(uint32 spellId) const override
    {
        switch (spellId)
        {
            case DemonHunterSpells::SHEAR:              return 0;  // Generates 10 Pain
            case FRACTURE:                              return 0;  // Generates 25 Pain
            case DemonHunterSpells::SOUL_CLEAVE:        return 30;
            case DemonHunterSpells::SPIRIT_BOMB:        return 40;
            case FEL_DEVASTATION:                       return 50;
            case SOUL_BARRIER:                          return _soulFragments.GetFragmentCount() * 12; // 12 Pain per fragment
            default:                                    return 0;
        }
    }

    // ========================================================================
    // VENGEANCE SPECIFIC ROTATION LOGIC
    // ========================================================================

    void ExecuteSingleTargetThreatRotation(::Unit* target)
    {
        uint32 currentPain = _resource;
        uint32 now = GameTime::GetGameTimeMS();

        // Priority 1: Sigil of Flame for threat and damage
        if (this->CanCastSpell(DemonHunterSpells::SIGIL_OF_FLAME, target))
        {
            this->CastSpell(target, DemonHunterSpells::SIGIL_OF_FLAME);
            _lastSigilOfFlameTime = now;
            

        // Register cooldowns using CooldownManager
        _cooldowns.RegisterBatch({
            {DemonHunterSpells::METAMORPHOSIS_VENGEANCE, CooldownPresets::MAJOR_OFFENSIVE, 1},
            {DemonHunterSpells::DEMON_SPIKES, 20000, 1},
            {DemonHunterSpells::FIERY_BRAND, CooldownPresets::OFFENSIVE_60, 1},
            {DemonHunterSpells::SIGIL_OF_FLAME, CooldownPresets::OFFENSIVE_30, 1},
            {SIGIL_OF_SILENCE, CooldownPresets::OFFENSIVE_60, 1},
            {SIGIL_OF_MISERY, 90000, 1},
            {SIGIL_OF_CHAINS, 90000, 1},
            {INFERNAL_STRIKE, 20000, 1},
            {SOUL_BARRIER, CooldownPresets::OFFENSIVE_30, 1},
            {FEL_DEVASTATION, CooldownPresets::OFFENSIVE_60, 1},
            {TORMENT, CooldownPresets::DISPEL, 1},
        });

        TC_LOG_DEBUG("playerbot", "Vengeance: Sigil of Flame cast");
            return;
        }

        // Priority 2: Fiery Brand on target (major defensive)
        if (ShouldUseFieryBrand() && this->CanCastSpell(DemonHunterSpells::FIERY_BRAND, target))
        {
            this->CastSpell(target, DemonHunterSpells::FIERY_BRAND);
            _fieryBrandActive = true;
            _fieryBrandEndTime = now + 8000;
            TC_LOG_DEBUG("playerbot", "Vengeance: Fiery Brand cast");
            return;
        }

        // Priority 3: Infernal Strike for gap closing and damage
        if (GetDistanceToTarget(target) > 10.0f && GetDistanceToTarget(target) <= 30.0f)
        {
            if (this->CanCastSpell(INFERNAL_STRIKE, target))
            {
                this->CastSpell(target, INFERNAL_STRIKE);
                GeneratePain(20); // Generates Pain
                TC_LOG_DEBUG("playerbot", "Vengeance: Infernal Strike gap closer");
                return;
            }
        }

        // Priority 4: Soul Cleave for healing (high priority if low health or high Pain)
        if (ShouldUseSoulCleave(currentPain) && currentPain >= 30)
        {
            this->CastSpell(target, DemonHunterSpells::SOUL_CLEAVE);
            _lastSoulCleaveTime = now;
            this->ConsumeResource(DemonHunterSpells::SOUL_CLEAVE);
            _soulFragments.ConsumeFragments(2); // Consumes up to 2 fragments for healing
            TC_LOG_DEBUG("playerbot", "Vengeance: Soul Cleave cast");
            return;
        }

        // Priority 5: Spirit Bomb if talented and have fragments
        if (this->GetBot()->HasSpell(SPIRIT_BOMB_TALENT) && currentPain >= 40)
        {
            if (_soulFragments.HasMinFragments(4))
            {
                this->CastSpell(this->GetBot(), DemonHunterSpells::SPIRIT_BOMB);
                this->ConsumeResource(DemonHunterSpells::SPIRIT_BOMB);
                _soulFragments.ConsumeAllFragments();
                TC_LOG_DEBUG("playerbot", "Vengeance: Spirit Bomb cast");
                return;
            }
        }

        // Priority 6: Fracture for Pain generation + Soul Fragments
        if (this->GetBot()->HasSpell(FRACTURE_TALENT) && currentPain < 80)
        {
            if (this->CanCastSpell(FRACTURE, target))
            {
                this->CastSpell(target, FRACTURE);
                GeneratePain(25);
                _soulFragments.GenerateFragments(2);
                TC_LOG_DEBUG("playerbot", "Vengeance: Fracture cast");
                return;
            }
        }

        // Priority 7: Shear for basic Pain generation
        if (currentPain < 90)
        {
            if (this->CanCastSpell(DemonHunterSpells::SHEAR, target))
            {
                this->CastSpell(target, DemonHunterSpells::SHEAR);
                _lastShearTime = now;
                GeneratePain(10);
                TC_LOG_DEBUG("playerbot", "Vengeance: Shear cast");
                return;
            }
        }

        // Priority 8: Throw Glaive for ranged threat
        if (GetDistanceToTarget(target) > 5.0f && GetDistanceToTarget(target) <= 30.0f)
        {
            if (this->CanCastSpell(THROW_GLAIVE_TANK, target))
            {
                this->CastSpell(target, THROW_GLAIVE_TANK);
                TC_LOG_DEBUG("playerbot", "Vengeance: Throw Glaive ranged threat");
                return;
            }
        }
    }

    void ExecuteAoEThreatRotation(::Unit* target, uint32 enemyCount)
    {
        uint32 currentPain = _resource;

        // Priority 1: Sigil of Flame for AoE threat
        if (this->CanCastSpell(DemonHunterSpells::SIGIL_OF_FLAME, target))
        {
            this->CastSpell(target, DemonHunterSpells::SIGIL_OF_FLAME);
            _lastSigilOfFlameTime = GameTime::GetGameTimeMS();
            TC_LOG_DEBUG("playerbot", "Vengeance: Sigil of Flame AoE");
            return;
        }

        // Priority 2: Spirit Bomb for AoE damage/threat
        if (this->GetBot()->HasSpell(SPIRIT_BOMB_TALENT) && currentPain >= 40)
        {
            if (_soulFragments.HasMinFragments(3))
            {
                this->CastSpell(this->GetBot(), DemonHunterSpells::SPIRIT_BOMB);
                this->ConsumeResource(DemonHunterSpells::SPIRIT_BOMB);
                _soulFragments.ConsumeAllFragments();
                TC_LOG_DEBUG("playerbot", "Vengeance: Spirit Bomb AoE");
                return;
            }
        }

        // Priority 3: Soul Cleave for AoE healing/damage
        if (currentPain >= 30)
        {
            this->CastSpell(target, DemonHunterSpells::SOUL_CLEAVE);
            _lastSoulCleaveTime = GameTime::GetGameTimeMS();
            this->ConsumeResource(DemonHunterSpells::SOUL_CLEAVE);
            _soulFragments.ConsumeFragments(2);
            TC_LOG_DEBUG("playerbot", "Vengeance: Soul Cleave AoE");
            return;
        }

        // Priority 4: Fracture for Pain + Fragments
        if (this->GetBot()->HasSpell(FRACTURE_TALENT) && currentPain < 80)
        {
            if (this->CanCastSpell(FRACTURE, target))
            {
                this->CastSpell(target, FRACTURE);
                GeneratePain(25);
                _soulFragments.GenerateFragments(2);
                return;
            }
        }

        // Priority 5: Shear for basic Pain
        if (currentPain < 90)
        {
            if (this->CanCastSpell(DemonHunterSpells::SHEAR, target))
            {
                this->CastSpell(target, DemonHunterSpells::SHEAR);
                GeneratePain(10);
                return;
            }
        }
    }

private:
    // ========================================================================
    // VENGEANCE STATE MANAGEMENT
    // ========================================================================

    void UpdateVengeanceState()
    {
        uint32 now = GameTime::GetGameTimeMS();

        // Update Soul Fragments
        _soulFragments.Update();

        // Update Demon Spikes tracking
        _demonSpikes.Update();

        // Check Fiery Brand expiry
        if (_fieryBrandActive && now >= _fieryBrandEndTime)
        {
            _fieryBrandActive = false;
            _fieryBrandEndTime = 0;
        }

        // Check Metamorphosis expiry
        if (_metamorphosisActive && now >= _metamorphosisEndTime)
        {
            _metamorphosisActive = false;
            _metamorphosisEndTime = 0;
        }

        // Passive Pain decay out of combat
        if (!this->GetBot()->IsInCombat() && _resource > 0)
        {
            _resource = (_resource > 1) ? _resource - 1 : 0;
        }
    }

    void HandleActiveMitigation()
    {
        Player* bot = this->GetBot();
        float healthPct = bot->GetHealthPct();

        // Calculate incoming damage rate (simplified)
        float incomingDamageRate = CalculateIncomingDamageRate();

        // Use Demon Spikes for active mitigation
        if (_demonSpikes.ShouldUse(incomingDamageRate, healthPct))
        {
            if (this->CanCastSpell(DemonHunterSpells::DEMON_SPIKES, bot))
            {
                this->CastSpell(bot, DemonHunterSpells::DEMON_SPIKES);
                _demonSpikes.Use();
                TC_LOG_DEBUG("playerbot", "Vengeance: Demon Spikes activated");
            }
        }

        // Use Soul Barrier if talented and have fragments
        if (healthPct < 50.0f && this->GetBot()->HasSpell(SOUL_BARRIER_TALENT))
        {
            if (_soulFragments.HasMinFragments(5) && this->CanCastSpell(SOUL_BARRIER, bot))
            {
                this->CastSpell(bot, SOUL_BARRIER);
                _soulFragments.ConsumeAllFragments();
                TC_LOG_DEBUG("playerbot", "Vengeance: Soul Barrier emergency shield");
            }
        }

        // Use Metamorphosis for major defensive cooldown
        if (healthPct < 35.0f && this->CanCastSpell(DemonHunterSpells::METAMORPHOSIS_VENGEANCE, bot))
        {
            this->CastSpell(bot, DemonHunterSpells::METAMORPHOSIS_VENGEANCE);
            _metamorphosisActive = true;
            _metamorphosisEndTime = GameTime::GetGameTimeMS() + 15000;
            TC_LOG_DEBUG("playerbot", "Vengeance: Metamorphosis emergency defensive");
        }
    }

    void HandleEmergencyDefensives()
    {
        Player* bot = this->GetBot();
        float healthPct = bot->GetHealthPct();

        // Critical health: Use Metamorphosis
        if (healthPct < 25.0f && !_metamorphosisActive)
        {
            if (this->CanCastSpell(DemonHunterSpells::METAMORPHOSIS_VENGEANCE, bot))
            {
                this->CastSpell(bot, DemonHunterSpells::METAMORPHOSIS_VENGEANCE);
                _metamorphosisActive = true;
                _metamorphosisEndTime = GameTime::GetGameTimeMS() + 15000;
                TC_LOG_DEBUG("playerbot", "Vengeance: Emergency Metamorphosis");
            }
        }

        // Low health: Ensure Soul Cleave is used
        if (healthPct < 60.0f && _resource >= 30)
        {
            if (::Unit* target = this->GetBot()->GetVictim())
            {
                this->CastSpell(target, DemonHunterSpells::SOUL_CLEAVE);
                this->ConsumeResource(DemonHunterSpells::SOUL_CLEAVE);
                _soulFragments.ConsumeFragments(2);
            }
        }
    }

    bool ShouldUseSoulCleave(uint32 currentPain) const
    {
        Player* bot = this->GetBot();
        float healthPct = bot->GetHealthPct();

        // Use Soul Cleave when:
        // 1. Low health (<70%)
        // 2. High Pain (>80) to avoid capping
        // 3. Have Soul Fragments for extra healing

        if (healthPct < 70.0f)
            return true;

        if (currentPain > 80)
            return true;

        if (_soulFragments.HasMinFragments(3))
            return true;

        return false;
    }

    bool ShouldUseFieryBrand() const
    {
        // Use Fiery Brand when:
        // 1. Not already active
        // 2. Taking heavy damage
        // 3. Multiple enemies

        if (_fieryBrandActive)
            return false;

        Player* bot = this->GetBot();
        if (bot->GetHealthPct() < 80.0f)
            return true;

        if (this->GetEnemiesInRange(8.0f) >= 3)
            return true;

        return false;
    }

    float CalculateIncomingDamageRate() const
    {
        // Simplified incoming damage calculation
        // In a real implementation, track damage events over time
        Player* bot = this->GetBot();

        // Estimate based on health loss and enemy count
        uint32 enemyCount = this->GetEnemiesInRange(10.0f);
        float healthLost = 100.0f - bot->GetHealthPct();

        // More enemies = higher incoming damage
        return (healthLost / 10.0f) * (1.0f + (enemyCount * 0.2f));
    }

    void GeneratePain(uint32 amount)
    {
        _resource = std::min<uint32>(_resource + amount, _maxResource);
    }

    float GetDistanceToTarget(::Unit* target) const
    {
        if (!target) return 999.0f;
        return this->GetBot()->GetDistance(target);
    }

    void InitializeCooldowns()
    {
        // Register cooldowns using CooldownManager
        _cooldowns.RegisterBatch({
            {DemonHunterSpells::METAMORPHOSIS_VENGEANCE, CooldownPresets::MAJOR_OFFENSIVE, 1},
            {DemonHunterSpells::DEMON_SPIKES, 20000, 1},
            {DemonHunterSpells::FIERY_BRAND, CooldownPresets::OFFENSIVE_60, 1},
            {DemonHunterSpells::SIGIL_OF_FLAME, CooldownPresets::OFFENSIVE_30, 1},
            {SIGIL_OF_SILENCE, CooldownPresets::OFFENSIVE_60, 1},
            {SIGIL_OF_MISERY, 90000, 1},
            {SIGIL_OF_CHAINS, 90000, 1},
            {INFERNAL_STRIKE, 20000, 1},
            {SOUL_BARRIER, CooldownPresets::OFFENSIVE_30, 1},
            {FEL_DEVASTATION, CooldownPresets::OFFENSIVE_60, 1},
            {TORMENT, CooldownPresets::DISPEL, 1},
        });
    }

    // ========================================================================
    // PHASE 5: DECISION SYSTEM INTEGRATION
    // ========================================================================

    void InitializeVengeanceMechanics()
    {
        using namespace bot::ai;
        using namespace bot::ai::BehaviorTreeBuilder;

        BotAI* ai = this->GetBot()->GetBotAI();
        if (!ai) return;

        auto* queue = ai->GetActionPriorityQueue();
        if (queue)
        {
            // EMERGENCY: Major defensive cooldowns
            queue->RegisterSpell(DemonHunterSpells::METAMORPHOSIS_VENGEANCE, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);
            queue->AddCondition(DemonHunterSpells::METAMORPHOSIS_VENGEANCE, [this](Player* bot, Unit*) {
                return bot && bot->GetHealthPct() < 35.0f && !this->_metamorphosisActive;
            }, "HP < 35% (15s, armor + HP)");

            queue->RegisterSpell(SOUL_BARRIER, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);
            queue->AddCondition(SOUL_BARRIER, [this](Player* bot, Unit*) {
                return bot && bot->GetHealthPct() < 50.0f && this->_soulFragments.HasMinFragments(5) &&
                       this->GetBot()->HasSpell(SOUL_BARRIER_TALENT);
            }, "HP < 50%, 5 fragments (absorb shield)");

            // CRITICAL: Active mitigation
            queue->RegisterSpell(DemonHunterSpells::DEMON_SPIKES, SpellPriority::CRITICAL, SpellCategory::DEFENSIVE);
            queue->AddCondition(DemonHunterSpells::DEMON_SPIKES, [this](Player* bot, Unit*) {
                return bot && this->_demonSpikes.CanUse() &&
                       (bot->GetHealthPct() < 80.0f || this->_demonSpikes.GetCharges() == 2);
            }, "HP < 80% or 2 charges (6s armor)");

            queue->RegisterSpell(DemonHunterSpells::FIERY_BRAND, SpellPriority::CRITICAL, SpellCategory::DEFENSIVE);
            queue->AddCondition(DemonHunterSpells::FIERY_BRAND, [this](Player*, Unit* target) {
                return target && this->ShouldUseFieryBrand();
            }, "Heavy damage (40% dmg reduction)");

            // HIGH: Threat generation
            queue->RegisterSpell(DemonHunterSpells::SIGIL_OF_FLAME, SpellPriority::HIGH, SpellCategory::OFFENSIVE);
            queue->AddCondition(DemonHunterSpells::SIGIL_OF_FLAME, [](Player*, Unit* target) {
                return target != nullptr;
            }, "AoE threat + damage");

            queue->RegisterSpell(INFERNAL_STRIKE, SpellPriority::HIGH, SpellCategory::UTILITY);
            queue->AddCondition(INFERNAL_STRIKE, [this](Player* bot, Unit* target) {
                if (!target) return false;
                float dist = bot->GetDistance(target);
                return dist > 10.0f && dist <= 30.0f;
            }, "10-30yd gap (leap + damage)");

            queue->RegisterSpell(TORMENT, SpellPriority::HIGH, SpellCategory::UTILITY);
            queue->AddCondition(TORMENT, [](Player*, Unit* target) {
                return target != nullptr; // ThreatAssistant determines need
            }, "Taunt");

            // MEDIUM: Pain spenders (healing/damage)
            queue->RegisterSpell(DemonHunterSpells::SOUL_CLEAVE, SpellPriority::MEDIUM, SpellCategory::HEALING);
            queue->AddCondition(DemonHunterSpells::SOUL_CLEAVE, [this](Player*, Unit* target) {
                return target && this->_resource >= 30 && this->ShouldUseSoulCleave(this->_resource);
            }, "30 pain, low HP or high pain (heals)");

            queue->RegisterSpell(DemonHunterSpells::SPIRIT_BOMB, SpellPriority::MEDIUM, SpellCategory::DAMAGE_AOE);
            queue->AddCondition(DemonHunterSpells::SPIRIT_BOMB, [this](Player*, Unit*) {
                return this->GetBot()->HasSpell(SPIRIT_BOMB_TALENT) &&
                       this->_resource >= 40 && this->_soulFragments.HasMinFragments(3);
            }, "40 pain, 3+ fragments (AoE + Frailty)");

            queue->RegisterSpell(FEL_DEVASTATION, SpellPriority::MEDIUM, SpellCategory::DAMAGE_AOE);
            queue->AddCondition(FEL_DEVASTATION, [this](Player* bot, Unit*) {
                return bot && this->_resource >= 50 && bot->GetHealthPct() < 60.0f &&
                       this->GetEnemiesInRange(8.0f) >= 2;
            }, "50 pain, HP < 60%, 2+ enemies (channel)");

            // LOW: Pain generators
            queue->RegisterSpell(FRACTURE, SpellPriority::LOW, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(FRACTURE, [this](Player*, Unit* target) {
                return target && this->GetBot()->HasSpell(FRACTURE_TALENT) && this->_resource < 80;
            }, "Pain < 80 (generates 25 pain + 2 fragments)");

            queue->RegisterSpell(DemonHunterSpells::SHEAR, SpellPriority::LOW, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(DemonHunterSpells::SHEAR, [this](Player*, Unit* target) {
                return target && this->_resource < 90;
            }, "Pain < 90 (generates 10 pain)");

            queue->RegisterSpell(THROW_GLAIVE_TANK, SpellPriority::LOW, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(THROW_GLAIVE_TANK, [this](Player* bot, Unit* target) {
                if (!target) return false;
                float dist = bot->GetDistance(target);
                return dist > 5.0f && dist <= 30.0f;
            }, "5-30yd range (ranged threat)");

            // UTILITY: Crowd control
            queue->RegisterSpell(SIGIL_OF_SILENCE, SpellPriority::HIGH, SpellCategory::CROWD_CONTROL);
            queue->AddCondition(SIGIL_OF_SILENCE, [](Player*, Unit* target) {
                return target && target->IsNonMeleeSpellCast(false);
            }, "Target casting (AoE interrupt)");

            queue->RegisterSpell(SIGIL_OF_MISERY, SpellPriority::MEDIUM, SpellCategory::CROWD_CONTROL);
            queue->AddCondition(SIGIL_OF_MISERY, [this](Player*, Unit*) {
                return this->GetEnemiesInRange(8.0f) >= 4;
            }, "4+ enemies (AoE fear)");

            queue->RegisterSpell(SIGIL_OF_CHAINS, SpellPriority::MEDIUM, SpellCategory::CROWD_CONTROL);
            queue->AddCondition(SIGIL_OF_CHAINS, [this](Player*, Unit*) {
                return this->GetEnemiesInRange(8.0f) >= 3;
            }, "3+ enemies (AoE slow)");

            queue->RegisterSpell(CONSUME_MAGIC_TANK, SpellPriority::MEDIUM, SpellCategory::UTILITY);
            queue->AddCondition(CONSUME_MAGIC_TANK, [](Player*, Unit* target) {
                return target && target->HasAura(118); // Magic buff detection
            }, "Has dispellable magic");
        }

        auto* behaviorTree = ai->GetBehaviorTree();
        if (behaviorTree)
        {
            auto root = Selector("Vengeance Tank", {
                // Tier 1: Emergency Defensives
                Sequence("Emergency Defense", {
                    Condition("Critical HP", [this](Player* bot) {
                        return bot && bot->GetHealthPct() < 35.0f;
                    }),
                    Selector("Use emergency", {
                        Sequence("Metamorphosis", {
                            Condition("Not active", [this](Player*) {
                                return !this->_metamorphosisActive;
                            }),
                            Action("Cast Meta", [this](Player* bot) {
                                if (this->CanCastSpell(DemonHunterSpells::METAMORPHOSIS_VENGEANCE, bot)) {
                                    this->CastSpell(bot, DemonHunterSpells::METAMORPHOSIS_VENGEANCE);
                                    this->_metamorphosisActive = true;
                                    this->_metamorphosisEndTime = GameTime::GetGameTimeMS() + 15000;
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Soul Barrier", {
                            Condition("5 fragments", [this](Player*) {
                                return this->_soulFragments.HasMinFragments(5);
                            }),
                            Condition("Has talent", [this](Player* bot) {
                                return bot->HasSpell(SOUL_BARRIER_TALENT);
                            }),
                            Action("Cast Barrier", [this](Player* bot) {
                                if (this->CanCastSpell(SOUL_BARRIER, bot)) {
                                    this->CastSpell(bot, SOUL_BARRIER);
                                    this->_soulFragments.ConsumeAllFragments();
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 2: Active Mitigation
                Sequence("Active Mitigation", {
                    Condition("Has target", [this](Player* bot) {
                        return bot && bot->GetVictim();
                    }),
                    Selector("Use mitigation", {
                        Sequence("Demon Spikes", {
                            Condition("Can use", [this](Player*) {
                                return this->_demonSpikes.CanUse();
                            }),
                            Condition("Should use", [this](Player* bot) {
                                return bot->GetHealthPct() < 80.0f || this->_demonSpikes.GetCharges() == 2;
                            }),
                            Action("Cast Demon Spikes", [this](Player* bot) {
                                if (this->CanCastSpell(DemonHunterSpells::DEMON_SPIKES, bot)) {
                                    this->CastSpell(bot, DemonHunterSpells::DEMON_SPIKES);
                                    this->_demonSpikes.Use();
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Fiery Brand", {
                            Condition("Should use", [this](Player*) {
                                return this->ShouldUseFieryBrand();
                            }),
                            Action("Cast Fiery Brand", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(DemonHunterSpells::FIERY_BRAND, target)) {
                                    this->CastSpell(target, DemonHunterSpells::FIERY_BRAND);
                                    this->_fieryBrandActive = true;
                                    this->_fieryBrandEndTime = GameTime::GetGameTimeMS() + 8000;
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 3: Threat Generation
                Sequence("Threat Generation", {
                    Condition("Has target", [this](Player* bot) {
                        return bot && bot->GetVictim();
                    }),
                    Selector("Generate threat", {
                        Sequence("Sigil of Flame", {
                            Action("Cast Sigil", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(DemonHunterSpells::SIGIL_OF_FLAME, target)) {
                                    this->CastSpell(target, DemonHunterSpells::SIGIL_OF_FLAME);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Infernal Strike", {
                            Condition("Gap 10-30yd", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (!target) return false;
                                float dist = bot->GetDistance(target);
                                return dist > 10.0f && dist <= 30.0f;
                            }),
                            Action("Cast Strike", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(INFERNAL_STRIKE, target)) {
                                    this->CastSpell(target, INFERNAL_STRIKE);
                                    this->GeneratePain(20);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 4: Pain Spenders
                Sequence("Pain Spenders", {
                    Condition("Has target", [this](Player* bot) {
                        return bot && bot->GetVictim();
                    }),
                    Selector("Spend pain", {
                        Sequence("Soul Cleave Heal", {
                            Condition("30 pain", [this](Player*) {
                                return this->_resource >= 30;
                            }),
                            Condition("Should use", [this](Player*) {
                                return this->ShouldUseSoulCleave(this->_resource);
                            }),
                            Action("Cast Soul Cleave", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(DemonHunterSpells::SOUL_CLEAVE, target)) {
                                    this->CastSpell(target, DemonHunterSpells::SOUL_CLEAVE);
                                    this->ConsumeResource(DemonHunterSpells::SOUL_CLEAVE);
                                    this->_soulFragments.ConsumeFragments(2);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Spirit Bomb AoE", {
                            Condition("Has talent", [this](Player* bot) {
                                return bot->HasSpell(SPIRIT_BOMB_TALENT);
                            }),
                            Condition("40 pain", [this](Player*) {
                                return this->_resource >= 40;
                            }),
                            Condition("3+ fragments", [this](Player*) {
                                return this->_soulFragments.HasMinFragments(3);
                            }),
                            Action("Cast Spirit Bomb", [this](Player* bot) {
                                if (this->CanCastSpell(DemonHunterSpells::SPIRIT_BOMB, bot)) {
                                    this->CastSpell(bot, DemonHunterSpells::SPIRIT_BOMB);
                                    this->ConsumeResource(DemonHunterSpells::SPIRIT_BOMB);
                                    this->_soulFragments.ConsumeAllFragments();
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 5: Pain Generators
                Sequence("Pain Generators", {
                    Condition("Has target", [this](Player* bot) {
                        return bot && bot->GetVictim();
                    }),
                    Condition("Low pain", [this](Player*) {
                        return this->_resource < 90;
                    }),
                    Selector("Generate pain", {
                        Sequence("Fracture", {
                            Condition("Has talent", [this](Player* bot) {
                                return bot->HasSpell(FRACTURE_TALENT);
                            }),
                            Condition("Pain < 80", [this](Player*) {
                                return this->_resource < 80;
                            }),
                            Action("Cast Fracture", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(FRACTURE, target)) {
                                    this->CastSpell(target, FRACTURE);
                                    this->GeneratePain(25);
                                    this->_soulFragments.GenerateFragments(2);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Shear", {
                            Action("Cast Shear", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(DemonHunterSpells::SHEAR, target)) {
                                    this->CastSpell(target, DemonHunterSpells::SHEAR);
                                    this->GeneratePain(10);
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
    VengeanceSoulFragmentManager _soulFragments;
    VengeanceDemonSpikesTracker _demonSpikes;

    // Ability timing
    uint32 _lastShearTime;
    uint32 _lastSoulCleaveTime;
    uint32 _lastSigilOfFlameTime;

    // Buff/Debuff tracking
    bool _fieryBrandActive;
    uint32 _fieryBrandEndTime;
    bool _metamorphosisActive;
    uint32 _metamorphosisEndTime;
    bool _immolationAuraActive;
};

} // namespace Playerbot
