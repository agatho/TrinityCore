/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * Subtlety Rogue Refactored - Template-Based Implementation
 *
 * This file provides a complete, template-based implementation of Subtlety Rogue
 * using the MeleeDpsSpecialization with dual resource system (Energy + Combo Points).
 */

#pragma once


#include "../Common/StatusEffectTracker.h"
#include "../Common/CooldownManager.h"
#include "../Common/RotationHelpers.h"
#include "../CombatSpecializationTemplates.h"
#include "RogueResourceTypes.h"  // Shared EnergyComboResource definition
#include "Player.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "Log.h"

// Phase 5 Integration: Decision Systems
#include "../Decision/ActionPriorityQueue.h"
#include "../Decision/BehaviorTree.h"
#include "../BotAI.h"

namespace Playerbot
{

// NOTE: Common Rogue spell IDs are defined in RogueSpecialization.h
// NOTE: Shared spells (SHADOW_DANCE, SYMBOLS_OF_DEATH, EVASION, SAP, etc.) are in RogueSpecialization.h
// Only Subtlety-unique spell IDs defined below to avoid duplicate definition errors
// NOTE: ComboPointsSubtlety is defined in RogueResourceTypes.h (spec-specific resource type)

// ============================================================================
// SUBTLETY ROGUE SPELL IDs (WoW 11.2 - The War Within)
// ============================================================================

enum SubtletySpells
{
    // NOTE: Shared spells (BACKSTAB, RUPTURE, STEALTH, VANISH, KICK, etc.) are in RogueSpecialization.h
    // Only Subtlety-UNIQUE spells defined here to avoid duplicate definitions

    // Combo Point Builders (Unique to Subtlety)
    SHADOWSTRIKE_SUB         = 185438,  // From stealth/Shadow Dance, 2 CP (unique ID)
    SHURIKEN_STORM           = 197835,  // 35 Energy, AoE, 1 CP per target

    // Combo Point Spenders (Unique to Subtlety)
    EVISCERATE_SUB           = 196819,  // Finisher, high damage (unique Subtlety version)
    BLACK_POWDER             = 319175,  // AoE finisher
    SECRET_TECHNIQUE         = 280719,  // Finisher, teleport attacks (talent)

    // Major Cooldowns
    SHADOW_BLADES            = 121471,  // 3 min CD, all attacks generate CP (talent)
    SHURIKEN_TORNADO         = 277925,  // 1 min CD, sustained AoE (talent)

    // Shadow Techniques
    SHADOW_TECHNIQUES_PROC   = 196911,  // Passive extra CP generation

    // Finisher Buffs
    SLICE_AND_DICE_SUB       = 315496,  // Attack speed buff (unique ID)

    // Procs and Buffs
    DANSE_MACABRE            = 393969,  // Buff from spending CP
    DEEPER_DAGGERS           = 383405,  // Eviscerate increases next Eviscerate

    // Talents
    DARK_SHADOW              = 245687,  // Shadow Dance CDR
    DEEPER_STRATAGEM_SUB     = 193531,  // 6 max combo points
    MARKED_FOR_DEATH_SUB     = 137619,  // Instant 5 CP
    SHURIKEN_TORNADO_TALENT  = 277925   // AoE sustained damage
};

// ============================================================================
// SHADOW DANCE TRACKER
// ============================================================================

class ShadowDanceTracker
{
public:
    ShadowDanceTracker()
        : _charges(3)
        , _maxCharges(3)
        , _active(false)
        , _endTime(0)
        , _lastUseTime(0)
        , _lastRechargeTime(0)
        , _chargeCooldown(60000) // 60 sec per charge
        , _duration(8000)        // 8 sec duration
    {
    }

    void Update()
    {
        uint32 now = GameTime::GetGameTimeMS();

        // Check if Shadow Dance expired
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

    bool ShouldUse(uint32 comboPoints) const
    {
        // Use Shadow Dance when:
        // 1. Have charges available
        // 2. Low combo points to build quickly
        // 3. Not already active

        if (!CanUse())
            return false;

        // Use to build combo points quickly
        if (comboPoints < 3)
            return true;

        // Use all charges eventually
        if (_charges == _maxCharges)
            return true;

        return false;
    }

private:
    CooldownManager _cooldowns;
    uint32 _charges;
    const uint32 _maxCharges;
    bool _active;
    uint32 _endTime;
    uint32 _lastUseTime;
    uint32 _lastRechargeTime;
    const uint32 _chargeCooldown;
    const uint32 _duration;
};

// ============================================================================
// SUBTLETY ROGUE REFACTORED
// ============================================================================

class SubtletyRogueRefactored : public MeleeDpsSpecialization<ComboPointsSubtlety>
{
public:
    // Use base class members with type alias for cleaner syntax
    using Base = MeleeDpsSpecialization<ComboPointsSubtlety>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::GetEnemiesInRange;
    using Base::IsBehindTarget;
    using Base::_resource;

    explicit SubtletyRogueRefactored(Player* bot)
        : MeleeDpsSpecialization<ComboPointsSubtlety>(bot)
        , _shadowDanceTracker()
        , _inStealth(false)
        , _symbolsOfDeathActive(false)
        , _symbolsOfDeathEndTime(0)
        , _shadowBladesActive(false)
        , _shadowBladesEndTime(0)
        , _lastBackstabTime(0)
        , _lastShadowstrikeTime(0)
        , _lastEviscerateTime(0)
    {
        // Initialize energy/combo resources
        this->_resource.maxEnergy = 100;
        this->_resource.maxComboPoints = bot->HasSpell(DEEPER_STRATAGEM_SUB) ? 6 : 5;        this->_resource.energy = this->_resource.maxEnergy;
        this->_resource.comboPoints = 0;

        // Phase 5 Integration: Initialize decision systems
        InitializeSubtletyMechanics();

        TC_LOG_DEBUG("playerbot", "SubtletyRogueRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))
            return;

        // Update tracking systems
        UpdateSubtletyState();

        // Check stealth status (stealth or Shadow Dance)
        _inStealth = this->GetBot()->HasAuraType(SPELL_AURA_MOD_STEALTH) || _shadowDanceTracker.IsActive();

        // Main rotation
        uint32 enemyCount = this->GetEnemiesInRange(10.0f);        if (enemyCount >= 3)
        {
            ExecuteAoERotation(target, enemyCount);
        }
        else
        {
            ExecuteSingleTargetRotation(target);
        }
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();        // Enter stealth out of combat
        if (!bot->IsInCombat() && !_inStealth && this->CanCastSpell(RogueAI::STEALTH, bot))
        {
            this->CastSpell(bot, RogueAI::STEALTH);
        }

        // Defensive cooldowns
        if (bot->GetHealthPct() < 30.0f && this->CanCastSpell(RogueAI::CLOAK_OF_SHADOWS, bot))
        {
            this->CastSpell(bot, RogueAI::CLOAK_OF_SHADOWS);
        }

        if (bot->GetHealthPct() < 50.0f && this->CanCastSpell(RogueAI::EVASION, bot))
        {
            this->CastSpell(bot, RogueAI::EVASION);
        }
    }

    // NOTE: GetOptimalRange is final in base class, cannot override
    // Melee range (5.0f) is already handled by MeleeDpsSpecialization

protected:
    void ExecuteSingleTargetRotation(::Unit* target)
    {
        uint32 energy = this->_resource.energy;
        uint32 cp = this->_resource.comboPoints;
        uint32 maxCp = this->_resource.maxComboPoints;

        // Priority 1: Symbols of Death on cooldown
        if (this->CanCastSpell(RogueAI::SYMBOLS_OF_DEATH, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), RogueAI::SYMBOLS_OF_DEATH);
            _symbolsOfDeathActive = true;
            _symbolsOfDeathEndTime = GameTime::GetGameTimeMS() + 10000;
            return;
        }

        // Priority 2: Shadow Blades on cooldown (talent)
        if (this->CanCastSpell(RogueAI::SHADOW_BLADES, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), RogueAI::SHADOW_BLADES);
            _shadowBladesActive = true;
            _shadowBladesEndTime = GameTime::GetGameTimeMS() + 20000;
            return;
        }

        // Priority 3: Shadow Dance to build combo points
        if (_shadowDanceTracker.ShouldUse(cp))
        {
            if (this->CanCastSpell(RogueAI::SHADOW_DANCE, this->GetBot()))
            {
                this->CastSpell(this->GetBot(), RogueAI::SHADOW_DANCE);
                _shadowDanceTracker.Use();
                _inStealth = true; // Enables stealth abilities
                return;
            }
        }

        // Priority 4: Shadowstrike from stealth/Shadow Dance
        if (_inStealth && energy >= 40)
        {
            if (this->CanCastSpell(SHADOWSTRIKE_SUB, target))
            {
                this->CastSpell(target, SHADOWSTRIKE_SUB);
                _lastShadowstrikeTime = GameTime::GetGameTimeMS();
                ConsumeEnergy(40);
                GenerateComboPoints(2);
                // Shadow Blades makes all attacks give CP
                if (_shadowBladesActive)
                    GenerateComboPoints(1);
                return;
            }
        }

        // Priority 5: Secret Technique finisher at 5-6 CP (talent)
        if (cp >= maxCp && energy >= 30)
        {
            if (this->GetBot()->HasSpell(SECRET_TECHNIQUE) && this->CanCastSpell(SECRET_TECHNIQUE, target))
            {
                this->CastSpell(target, SECRET_TECHNIQUE);
                ConsumeEnergy(30);
                this->_resource.comboPoints = 0;
                return;
            }
        }

        // Priority 6: Eviscerate finisher at 5-6 CP
        if (cp >= (maxCp - 1) && energy >= 35)
        {
            if (this->CanCastSpell(EVISCERATE_SUB, target))
            {
                this->CastSpell(target, EVISCERATE_SUB);
                _lastEviscerateTime = GameTime::GetGameTimeMS();
                ConsumeEnergy(35);
                this->_resource.comboPoints = 0;
                return;
            }
        }

        // Priority 7: Rupture if not active
        if (!HasRupture(target) && cp >= 4 && energy >= 25)
        {
            if (this->CanCastSpell(RogueAI::RUPTURE, target))
            {
                this->CastSpell(target, RogueAI::RUPTURE);
                ConsumeEnergy(25);
                this->_resource.comboPoints = 0;
                return;
            }
        }

        // Priority 8: Backstab for combo points (from behind)
        if (energy >= 35 && cp < maxCp)
        {
            if (this->IsBehindTarget(target) && this->CanCastSpell(RogueAI::BACKSTAB, target))
            {
                this->CastSpell(target, RogueAI::BACKSTAB);
                _lastBackstabTime = GameTime::GetGameTimeMS();
                ConsumeEnergy(35);
                GenerateComboPoints(1);
                if (_shadowBladesActive)
                    GenerateComboPoints(1);
                return;
            }
        }

        // Priority 9: Shadowstrike if can't get behind (less efficient)
        if (energy >= 40 && cp < maxCp)
        {
            if (this->CanCastSpell(SHADOWSTRIKE_SUB, target))
            {
                this->CastSpell(target, SHADOWSTRIKE_SUB);
                ConsumeEnergy(40);
                GenerateComboPoints(2);
                return;
            }
        }
    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)
    {
        uint32 energy = this->_resource.energy;
        uint32 cp = this->_resource.comboPoints;
        uint32 maxCp = this->_resource.maxComboPoints;

        // Priority 1: Shuriken Tornado (talent, sustained AoE)
        if (this->CanCastSpell(SHURIKEN_TORNADO_TALENT, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), SHURIKEN_TORNADO_TALENT);
            return;
        }

        // Priority 2: Symbols of Death
        if (this->CanCastSpell(RogueAI::SYMBOLS_OF_DEATH, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), RogueAI::SYMBOLS_OF_DEATH);
            _symbolsOfDeathActive = true;
            _symbolsOfDeathEndTime = GameTime::GetGameTimeMS() + 10000;
            return;
        }

        // Priority 3: Shadow Dance
        if (_shadowDanceTracker.CanUse())
        {
            if (this->CanCastSpell(RogueAI::SHADOW_DANCE, this->GetBot()))
            {
                this->CastSpell(this->GetBot(), RogueAI::SHADOW_DANCE);
                _shadowDanceTracker.Use();
                _inStealth = true;
                return;
            }
        }

        // Priority 4: Black Powder finisher at 5+ CP
        if (cp >= 5 && energy >= 35)
        {
            if (this->CanCastSpell(BLACK_POWDER, this->GetBot()))
            {
                this->CastSpell(this->GetBot(), BLACK_POWDER);
                ConsumeEnergy(35);
                this->_resource.comboPoints = 0;
                return;
            }
        }

        // Priority 5: Shuriken Storm for AoE combo building
        if (energy >= 35 && cp < maxCp)
        {
            if (this->CanCastSpell(SHURIKEN_STORM, this->GetBot()))
            {
                this->CastSpell(this->GetBot(), SHURIKEN_STORM);
                ConsumeEnergy(35);
                GenerateComboPoints(std::min(enemyCount, 5u));
                return;
            }
        }

        // Fallback to single target if AoE abilities on CD
        ExecuteSingleTargetRotation(target);
    }

private:
    void UpdateSubtletyState()
    {
        uint32 now = GameTime::GetGameTimeMS();

        // Update Shadow Dance tracker
        _shadowDanceTracker.Update();

        // Check Symbols of Death expiry
        if (_symbolsOfDeathActive && now >= _symbolsOfDeathEndTime)
        {
            _symbolsOfDeathActive = false;
            _symbolsOfDeathEndTime = 0;
        }

        // Check Shadow Blades expiry
        if (_shadowBladesActive && now >= _shadowBladesEndTime)
        {
            _shadowBladesActive = false;
            _shadowBladesEndTime = 0;
        }

        // Regenerate energy (10 per second)
        static uint32 lastRegenTime = now;
        uint32 timeDiff = now - lastRegenTime;

        if (timeDiff >= 100) // Every 100ms
        {
            uint32 energyRegen = (timeDiff / 100);
            this->_resource.energy = std::min(this->_resource.energy + energyRegen, this->_resource.maxEnergy);
            lastRegenTime = now;
        }
    }

    void ConsumeEnergy(uint32 amount)
    {        this->_resource.energy = (this->_resource.energy > amount) ? this->_resource.energy - amount : 0;
    }

    void GenerateComboPoints(uint32 amount)
    {
        this->_resource.comboPoints = std::min(this->_resource.comboPoints + amount, this->_resource.maxComboPoints);
    }

    bool IsBehindTarget(::Unit* target) const
    {
        if (!target || !this->GetBot())
            return false;

        // Check if bot is behind target (simplified)
        return GetBot()->isInBack(target);
    }

    bool HasRupture(::Unit* target) const    {
        // Simplified - check if target has Rupture aura
        return target && target->HasAura(RogueAI::RUPTURE, this->GetBot()->GetGUID());
    }

    // Phase 5 Integration: Decision Systems Initialization
    void InitializeSubtletyMechanics()
    {
        using namespace bot::ai;
        using namespace bot::ai::BehaviorTreeBuilder;

        BotAI* ai = this->GetBot()->GetBotAI();

        // ========================================================================
        // ActionPriorityQueue: Register Subtlety Rogue spells with priorities
        // ========================================================================
        auto* queue = ai->GetActionPriorityQueue();
        if (queue)
        {
            // EMERGENCY: Defensive cooldowns
            queue->RegisterSpell(RogueAI::CLOAK_OF_SHADOWS, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);
            queue->AddCondition(RogueAI::CLOAK_OF_SHADOWS, [this](Player* bot, Unit* target) {
                return bot && bot->GetHealthPct() < 30.0f;
            }, "Bot HP < 30% (spell immunity)");

            queue->RegisterSpell(RogueAI::EVASION, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);
            queue->AddCondition(RogueAI::EVASION, [this](Player* bot, Unit* target) {
                return bot && bot->GetHealthPct() < 50.0f;
            }, "Bot HP < 50% (dodge boost)");

            // CRITICAL: Burst cooldowns and Shadow Dance
            queue->RegisterSpell(RogueAI::SYMBOLS_OF_DEATH, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);
            queue->AddCondition(RogueAI::SYMBOLS_OF_DEATH, [this](Player* bot, Unit* target) {
                return target && !this->_symbolsOfDeathActive;
            }, "Not active (10s burst, 15% damage increase)");

            queue->RegisterSpell(RogueAI::SHADOW_BLADES, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);
            queue->AddCondition(RogueAI::SHADOW_BLADES, [this](Player* bot, Unit* target) {
                return bot && bot->HasSpell(RogueAI::SHADOW_BLADES) &&
                       target && !this->_shadowBladesActive;
            }, "Has talent, not active (20s burst, all attacks give CP)");

            queue->RegisterSpell(RogueAI::SHADOW_DANCE, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);
            queue->AddCondition(RogueAI::SHADOW_DANCE, [this](Player* bot, Unit* target) {
                return target && this->_shadowDanceTracker.ShouldUse(this->_resource.comboPoints);
            }, "Should use (3 charges, 8s duration, enables Shadowstrike)");

            // HIGH: Stealth abilities and finishers
            queue->RegisterSpell(SHADOWSTRIKE_SUB, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(SHADOWSTRIKE_SUB, [this](Player* bot, Unit* target) {
                return target && this->_resource.energy >= 40 && this->_inStealth;
            }, "40+ Energy, in stealth/Shadow Dance (generates 2 CP)");

            queue->RegisterSpell(SECRET_TECHNIQUE, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(SECRET_TECHNIQUE, [this](Player* bot, Unit* target) {
                return bot && bot->HasSpell(SECRET_TECHNIQUE) &&
                       target && this->_resource.energy >= 30 &&
                       this->_resource.comboPoints >= this->_resource.maxComboPoints;
            }, "Has talent, 30+ Energy, max CP (finisher, teleport attacks)");

            queue->RegisterSpell(EVISCERATE_SUB, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(EVISCERATE_SUB, [this](Player* bot, Unit* target) {
                return target && this->_resource.energy >= 35 &&
                       this->_resource.comboPoints >= (this->_resource.maxComboPoints - 1);
            }, "35+ Energy, 4-5+ CP (finisher damage)");

            queue->RegisterSpell(RogueAI::RUPTURE, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(RogueAI::RUPTURE, [this](Player* bot, Unit* target) {
                return target && this->_resource.energy >= 25 &&
                       this->_resource.comboPoints >= 4 &&
                       !this->HasRupture(target);
            }, "25+ Energy, 4+ CP, DoT not active (finisher bleed)");

            // MEDIUM: Combo builders
            queue->RegisterSpell(RogueAI::BACKSTAB, SpellPriority::MEDIUM, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(RogueAI::BACKSTAB, [this](Player* bot, Unit* target) {
                return target && this->_resource.energy >= 35 &&
                       this->_resource.comboPoints < this->_resource.maxComboPoints &&
                       this->IsBehindTarget(target);
            }, "35+ Energy, not max CP, behind target (generates 1 CP)");

            queue->RegisterSpell(SHADOWSTRIKE_SUB, SpellPriority::MEDIUM, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(SHADOWSTRIKE_SUB, [this](Player* bot, Unit* target) {
                return target && this->_resource.energy >= 40 &&
                       this->_resource.comboPoints < this->_resource.maxComboPoints &&
                       !this->_inStealth;
            }, "40+ Energy, not max CP, not in stealth (fallback builder)");

            queue->RegisterSpell(RogueAI::KICK, SpellPriority::MEDIUM, SpellCategory::UTILITY);
            queue->AddCondition(RogueAI::KICK, [this](Player* bot, Unit* target) {
                return target && target->IsNonMeleeSpellCast(false);
            }, "Target casting (interrupt)");

            // LOW: AoE abilities
            queue->RegisterSpell(SHURIKEN_STORM, SpellPriority::LOW, SpellCategory::DAMAGE_AOE);
            queue->AddCondition(SHURIKEN_STORM, [this](Player* bot, Unit* target) {
                return target && this->_resource.energy >= 35 &&
                       this->GetEnemiesInRange(10.0f) >= 3 &&
                       this->_resource.comboPoints < this->_resource.maxComboPoints;
            }, "35+ Energy, 3+ enemies, not max CP (AoE combo builder)");

            queue->RegisterSpell(BLACK_POWDER, SpellPriority::LOW, SpellCategory::DAMAGE_AOE);
            queue->AddCondition(BLACK_POWDER, [this](Player* bot, Unit* target) {
                return target && this->_resource.energy >= 35 &&
                       this->GetEnemiesInRange(10.0f) >= 3 &&
                       this->_resource.comboPoints >= 5;
            }, "35+ Energy, 3+ enemies, 5+ CP (AoE finisher)");

            TC_LOG_INFO("module.playerbot", "🗡️ SUBTLETY ROGUE: Registered {} spells in ActionPriorityQueue", queue->GetSpellCount());
        }

        // ========================================================================
        // BehaviorTree: Subtlety Rogue DPS rotation logic
        // ========================================================================
        auto* behaviorTree = ai->GetBehaviorTree();
        if (behaviorTree)
        {
            auto root = Selector("Subtlety Rogue DPS", {
                // Tier 1: Burst Cooldowns (Symbols of Death, Shadow Blades)
                Sequence("Burst Cooldowns", {
                    Condition("Target exists", [this](Player* bot, Unit* target) {
                        return target != nullptr;
                    }),
                    Selector("Use Burst", {
                        Sequence("Cast Symbols of Death", {
                            Condition("Not active", [this](Player* bot, Unit* target) {
                                return !this->_symbolsOfDeathActive;
                            }),
                            Action("Cast Symbols of Death", [this](Player* bot, Unit* target) -> NodeStatus {
                                if (this->CanCastSpell(RogueAI::SYMBOLS_OF_DEATH, bot))
                                {
                                    this->CastSpell(bot, RogueAI::SYMBOLS_OF_DEATH);
                                    this->_symbolsOfDeathActive = true;
                                    this->_symbolsOfDeathEndTime = GameTime::GetGameTimeMS() + 10000;
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Cast Shadow Blades", {
                            Condition("Has talent and not active", [this](Player* bot, Unit* target) {
                                return bot && bot->HasSpell(RogueAI::SHADOW_BLADES) &&
                                       !this->_shadowBladesActive;
                            }),
                            Action("Cast Shadow Blades", [this](Player* bot, Unit* target) -> NodeStatus {
                                if (this->CanCastSpell(RogueAI::SHADOW_BLADES, bot))
                                {
                                    this->CastSpell(bot, RogueAI::SHADOW_BLADES);
                                    this->_shadowBladesActive = true;
                                    this->_shadowBladesEndTime = GameTime::GetGameTimeMS() + 20000;
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 2: Shadow Dance (enable stealth abilities)
                Sequence("Shadow Dance", {
                    Condition("Target exists and should use", [this](Player* bot, Unit* target) {
                        return target != nullptr &&
                               this->_shadowDanceTracker.ShouldUse(this->_resource.comboPoints);
                    }),
                    Action("Cast Shadow Dance", [this](Player* bot, Unit* target) -> NodeStatus {
                        if (this->CanCastSpell(RogueAI::SHADOW_DANCE, bot))
                        {
                            this->CastSpell(bot, RogueAI::SHADOW_DANCE);
                            this->_shadowDanceTracker.Use();
                            this->_inStealth = true;
                            return NodeStatus::SUCCESS;
                        }
                        return NodeStatus::FAILURE;
                    })
                }),

                // Tier 3: Finishers (Secret Technique, Eviscerate, Rupture at 4-5+ CP)
                Sequence("Finishers", {
                    Condition("Target exists and has CP", [this](Player* bot, Unit* target) {
                        return target != nullptr &&
                               this->_resource.comboPoints >= (this->_resource.maxComboPoints - 1);
                    }),
                    Selector("Choose Finisher", {
                        // Rupture if not active
                        Sequence("Cast Rupture", {
                            Condition("Rupture missing and 4+ CP", [this](Player* bot, Unit* target) {
                                return this->_resource.comboPoints >= 4 &&
                                       !this->HasRupture(target) &&
                                       this->_resource.energy >= 25;
                            }),
                            Action("Cast Rupture", [this](Player* bot, Unit* target) -> NodeStatus {
                                if (this->CanCastSpell(RogueAI::RUPTURE, target))
                                {
                                    this->CastSpell(target, RogueAI::RUPTURE);
                                    this->ConsumeEnergy(25);
                                    this->_resource.comboPoints = 0;
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        // Secret Technique at max CP
                        Sequence("Cast Secret Technique", {
                            Condition("Has talent and max CP", [this](Player* bot, Unit* target) {
                                return bot && bot->HasSpell(SECRET_TECHNIQUE) &&
                                       this->_resource.comboPoints >= this->_resource.maxComboPoints &&
                                       this->_resource.energy >= 30;
                            }),
                            Action("Cast Secret Technique", [this](Player* bot, Unit* target) -> NodeStatus {
                                if (this->CanCastSpell(SECRET_TECHNIQUE, target))
                                {
                                    this->CastSpell(target, SECRET_TECHNIQUE);
                                    this->ConsumeEnergy(30);
                                    this->_resource.comboPoints = 0;
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        // Eviscerate at 4-5+ CP
                        Sequence("Cast Eviscerate", {
                            Condition("35+ Energy", [this](Player* bot, Unit* target) {
                                return this->_resource.energy >= 35;
                            }),
                            Action("Cast Eviscerate", [this](Player* bot, Unit* target) -> NodeStatus {
                                if (this->CanCastSpell(EVISCERATE_SUB, target))
                                {
                                    this->CastSpell(target, EVISCERATE_SUB);
                                    this->_lastEviscerateTime = GameTime::GetGameTimeMS();
                                    this->ConsumeEnergy(35);
                                    this->_resource.comboPoints = 0;
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 4: Combo Builders (Shadowstrike in stealth, Backstab from behind)
                Sequence("Combo Builders", {
                    Condition("Target exists", [this](Player* bot, Unit* target) {
                        return target != nullptr &&
                               this->_resource.comboPoints < this->_resource.maxComboPoints;
                    }),
                    Selector("Build Combo Points", {
                        // Shadowstrike from stealth/Shadow Dance
                        Sequence("Cast Shadowstrike in stealth", {
                            Condition("In stealth and 40+ Energy", [this](Player* bot, Unit* target) {
                                return this->_inStealth && this->_resource.energy >= 40;
                            }),
                            Action("Cast Shadowstrike", [this](Player* bot, Unit* target) -> NodeStatus {
                                if (this->CanCastSpell(SHADOWSTRIKE_SUB, target))
                                {
                                    this->CastSpell(target, SHADOWSTRIKE_SUB);
                                    this->_lastShadowstrikeTime = GameTime::GetGameTimeMS();
                                    this->ConsumeEnergy(40);
                                    this->GenerateComboPoints(2);
                                    if (this->_shadowBladesActive)
                                        this->GenerateComboPoints(1);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        // Backstab from behind
                        Sequence("Cast Backstab", {
                            Condition("Behind target and 35+ Energy", [this](Player* bot, Unit* target) {
                                return this->IsBehindTarget(target) && this->_resource.energy >= 35;
                            }),
                            Action("Cast Backstab", [this](Player* bot, Unit* target) -> NodeStatus {
                                if (this->CanCastSpell(RogueAI::BACKSTAB, target))
                                {
                                    this->CastSpell(target, RogueAI::BACKSTAB);
                                    this->_lastBackstabTime = GameTime::GetGameTimeMS();
                                    this->ConsumeEnergy(35);
                                    this->GenerateComboPoints(1);
                                    if (this->_shadowBladesActive)
                                        this->GenerateComboPoints(1);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        // Shadowstrike fallback
                        Sequence("Cast Shadowstrike fallback", {
                            Condition("40+ Energy", [this](Player* bot, Unit* target) {
                                return this->_resource.energy >= 40;
                            }),
                            Action("Cast Shadowstrike", [this](Player* bot, Unit* target) -> NodeStatus {
                                if (this->CanCastSpell(SHADOWSTRIKE_SUB, target))
                                {
                                    this->CastSpell(target, SHADOWSTRIKE_SUB);
                                    this->ConsumeEnergy(40);
                                    this->GenerateComboPoints(2);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                })
            });

            behaviorTree->SetRoot(root);
            TC_LOG_INFO("module.playerbot", "🌲 SUBTLETY ROGUE: BehaviorTree initialized with 4-tier DPS rotation");
        }
    }

private:
    ShadowDanceTracker _shadowDanceTracker;
    bool _inStealth;
    bool _symbolsOfDeathActive;
    uint32 _symbolsOfDeathEndTime;
    bool _shadowBladesActive;
    uint32 _shadowBladesEndTime;
    uint32 _lastBackstabTime;
    uint32 _lastShadowstrikeTime;
    uint32 _lastEviscerateTime;
};

} // namespace Playerbot
