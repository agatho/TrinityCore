/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * Devastation Evoker Refactored - Enterprise-Grade Header-Only Implementation
 *
 * This file provides a complete, template-based implementation of Devastation Evoker
 * using the DPSSpecialization with Essence resource system and Empowerment mechanics.
 */

#pragma once

#include "../Common/StatusEffectTracker.h"
#include "../Common/CooldownManager.h"
#include "../Common/RotationHelpers.h"
#include "../CombatSpecializationTemplates.h"
#include "../ResourceTypes.h"
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
// DEVASTATION EVOKER SPELL IDs (WoW 11.2 - The War Within)
// ============================================================================

enum DevastationEvokerSpells
{
    // Essence Generators
    AZURE_STRIKE         = 362969,  // 25 yd, generates 2 essence
    LIVING_FLAME         = 361469,  // 25 yd, generates 1 essence

    // Essence Spenders (Empowered)
    FIRE_BREATH          = 357208,  // 3 essence, empowered (rank 1-4)
    ETERNITY_SURGE       = 359073,  // 3 essence, empowered (rank 1-4)

    // Direct Damage
    DISINTEGRATE         = 356995,  // 3 essence, channel
    PYRE                 = 357211,   // 2 essence, AoE cone
    SHATTERING_STAR      = 370452,  // 0 essence, 20s CD, debuff

    // Major Cooldowns
    DRAGONRAGE           = 375087,  // 2 min CD, 18s burst window
    DEEP_BREATH          = 357210,  // 2 min CD, flying breath attack
    TIP_THE_SCALES       = 370553,  // 2 min CD, instant empower

    // Procs and Buffs
    ESSENCE_BURST        = 359618,  // Free essence spender
    BURNOUT              = 375802,  // Living Flame damage increase
    IRIDESCENCE_BLUE     = 386399,  // Azure Strike empowerment
    IRIDESCENCE_RED      = 386353,  // Pyre/Fire Breath empowerment

    // Utility
    HOVER                = 358267,  // 10 sec CD, hover mode
    OBSIDIAN_SCALES      = 363916,  // 90 sec CD, damage reduction
    RENEWING_BLAZE       = 374348,  // 90 sec CD, self-heal
    QUELL                = 351338,  // 40 sec CD, interrupt
    TAIL_SWIPE           = 368970,  // 90 sec CD, knockback
    WING_BUFFET          = 357214,  // 90 sec CD, cone knockback

    // Talents
    ANIMOSITY            = 375797,  // Dragonrage CDR
    CATALYZE             = 386283,  // Essence Burst chance
    FEED_THE_FLAMES      = 369846,  // Fire Breath extended
    ONYX_LEGACY          = 386348   // Deep Breath enhanced
};

// Essence resource type for Evoker
struct EssenceResource
{
    uint32 essence{0};
    uint32 maxEssence{5};
    bool available{true};

    bool Consume(uint32 cost) {
        if (essence >= cost) {
            essence -= cost;
            return true;
        }
        return false;
    }

    void Regenerate(uint32 diff) {
        // Passive essence regeneration every 5 seconds
        available = true;
    }

    [[nodiscard]] uint32 GetAvailable() const { return essence; }
    [[nodiscard]] uint32 GetMax() const { return maxEssence; }

    void Initialize(Player* bot) {
        if (bot) {
            essence = 0;
            maxEssence = 5; // Devastation has 5 max essence
        }
    }
};

// ============================================================================
// EMPOWERMENT TRACKING
// ============================================================================

enum class EmpowerLevel : uint8
{
    NONE = 0,
    RANK_1 = 1,
    RANK_2 = 2,
    RANK_3 = 3,
    RANK_4 = 4
};

class DevastationEmpowermentTracker
{
public:
    DevastationEmpowermentTracker()
        : _isChanneling(false)
        , _currentSpellId(0)
        , _targetLevel(EmpowerLevel::NONE)
        , _channelStartTime(0)
    {}

    void StartEmpower(uint32 spellId, EmpowerLevel targetLevel)
    {
        _isChanneling = true;
        _currentSpellId = spellId;
        _targetLevel = targetLevel;
        _channelStartTime = GameTime::GetGameTimeMS();
    }

    void StopEmpower()
    {
        _isChanneling = false;
        _currentSpellId = 0;
        _targetLevel = EmpowerLevel::NONE;
        _channelStartTime = 0;
    }

    bool IsChanneling() const { return _isChanneling; }
    uint32 GetSpellId() const { return _currentSpellId; }

    uint32 GetChannelTime() const
    {
        if (!_isChanneling) return 0;
        return GameTime::GetGameTimeMS() - _channelStartTime;
    }

    bool ShouldRelease() const
    {
        if (!_isChanneling) return false;
        uint32 requiredTime = static_cast<uint32>(_targetLevel) * 750; // 0.75s per rank
        return GetChannelTime() >= requiredTime;
    }

    EmpowerLevel GetAchievedLevel() const
    {
        if (!_isChanneling) return EmpowerLevel::NONE;
        uint32 channelTime = GetChannelTime();

        if (channelTime >= 3000) return EmpowerLevel::RANK_4; // 3.0s+
        if (channelTime >= 2250) return EmpowerLevel::RANK_3; // 2.25s
        if (channelTime >= 1500) return EmpowerLevel::RANK_2; // 1.5s
        if (channelTime >= 750) return EmpowerLevel::RANK_1;  // 0.75s

        return EmpowerLevel::NONE;
    }

private:
    bool _isChanneling;
    uint32 _currentSpellId;
    EmpowerLevel _targetLevel;
    uint32 _channelStartTime;
};

// ============================================================================
// DRAGONRAGE TRACKER
// ============================================================================

class DragonrageTracker
{
public:
    DragonrageTracker()
        : _isActive(false)
        , _endTime(0)
    {}

    void Activate()
    {
        _isActive = true;
        _endTime = GameTime::GetGameTimeMS() + 18000; // 18 second duration
    }

    void Update()
    {
        if (_isActive && GameTime::GetGameTimeMS() >= _endTime)
        {
            _isActive = false;
            _endTime = 0;
        }
    }

    bool IsActive() const { return _isActive; }
    uint32 GetTimeRemaining() const
    {
        if (!_isActive) return 0;
        uint32 now = GameTime::GetGameTimeMS();
        return _endTime > now ? _endTime - now : 0;
    }

private:
    bool _isActive;
    uint32 _endTime;
};

// ============================================================================
// DEVASTATION EVOKER REFACTORED
// ============================================================================

class DevastationEvokerRefactored : public DPSSpecialization<EssenceResource>
{
public:
    using Base = DPSSpecialization<EssenceResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;

    explicit DevastationEvokerRefactored(Player* bot)
        : DPSSpecialization<EssenceResource>(bot)
        , _empowermentTracker()
        , _dragonrageTracker()
        , _essenceBurstStacks(0)
        , _lastEternityTime(0)
        , _lastFireBreathTime(0)
    {
        // Initialize essence resources
        this->_resource.Initialize(bot);

        // Phase 5: Initialize decision systems
        InitializeDevastationMechanics();

        TC_LOG_DEBUG("playerbot", "DevastationEvokerRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))
            return;

        // Update Devastation state
        UpdateDevastationState();

        // Update empowerment channeling
        if (_empowermentTracker.IsChanneling())
        {
            if (_empowermentTracker.ShouldRelease())
            {
                // Release empowered spell at target rank
                ReleaseEmpoweredSpell();
            }
            return; // Don't cast other spells while channeling empower
        }

        // Determine rotation based on enemy count
        uint32 enemyCount = this->GetEnemiesInRange(25.0f);
        if (enemyCount >= 3)
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
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Emergency defensives
        HandleEmergencyDefensives();
    }

    float GetOptimalRange(::Unit* target) override
    {
        return 25.0f; // Ranged caster at 25 yards
    }

protected:
    void ExecuteSingleTargetRotation(::Unit* target)
    {
        uint32 essence = this->_resource.essence;

        // Priority 1: Dragonrage burst window
        if (_dragonrageTracker.IsActive())
        {
            ExecuteDragonrageBurst(target);
            return;
        }

        // Priority 2: Shattering Star debuff
        if (this->CanCastSpell(SHATTERING_STAR, target))
        {
            this->CastSpell(target, SHATTERING_STAR);
            return;
        }

        // Priority 3: Eternity's Surge (empowered)
        if (essence >= 3 && this->CanCastSpell(ETERNITY_SURGE, target))
        {
            StartEmpoweredSpell(ETERNITY_SURGE, EmpowerLevel::RANK_3, target);
            return;
        }

        // Priority 4: Disintegrate channel
        if (essence >= 3 && this->CanCastSpell(DISINTEGRATE, target))
        {
            this->CastSpell(target, DISINTEGRATE);
            ConsumeEssence(3);
            return;
        }

        // Priority 5: Fire Breath (empowered)
        if (essence >= 3 && this->CanCastSpell(FIRE_BREATH, target))
        {
            StartEmpoweredSpell(FIRE_BREATH, EmpowerLevel::RANK_2, target);
            return;
        }

        // Priority 6: Azure Strike for essence
        if (essence < 4 && this->CanCastSpell(AZURE_STRIKE, target))
        {
            this->CastSpell(target, AZURE_STRIKE);
            GenerateEssence(2);
            return;
        }

        // Priority 7: Living Flame filler
        if (this->CanCastSpell(LIVING_FLAME, target))
        {
            this->CastSpell(target, LIVING_FLAME);
            GenerateEssence(1);
            return;
        }
    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)
    {
        uint32 essence = this->_resource.essence;

        // Priority 1: Fire Breath AoE (empowered rank 4)
        if (essence >= 3 && this->CanCastSpell(FIRE_BREATH, target))
        {
            StartEmpoweredSpell(FIRE_BREATH, EmpowerLevel::RANK_4, target);
            return;
        }

        // Priority 2: Pyre AoE
        if (essence >= 2 && this->CanCastSpell(PYRE, target))
        {
            this->CastSpell(target, PYRE);
            ConsumeEssence(2);
            return;
        }

        // Priority 3: Shattering Star
        if (this->CanCastSpell(SHATTERING_STAR, target))
        {
            this->CastSpell(target, SHATTERING_STAR);
            return;
        }

        // Priority 4: Azure Strike for essence
        if (essence < 4 && this->CanCastSpell(AZURE_STRIKE, target))
        {
            this->CastSpell(target, AZURE_STRIKE);
            GenerateEssence(2);
            return;
        }

        // Priority 5: Living Flame filler
        if (this->CanCastSpell(LIVING_FLAME, target))
        {
            this->CastSpell(target, LIVING_FLAME);
            GenerateEssence(1);
            return;
        }
    }

    void ExecuteDragonrageBurst(::Unit* target)
    {
        uint32 essence = this->_resource.essence;

        // Spam empowered spells during Dragonrage
        if (essence >= 3)
        {
            if (this->CanCastSpell(ETERNITY_SURGE, target))
            {
                StartEmpoweredSpell(ETERNITY_SURGE, EmpowerLevel::RANK_1, target); // Quick rank 1
                return;
            }

            if (this->CanCastSpell(FIRE_BREATH, target))
            {
                StartEmpoweredSpell(FIRE_BREATH, EmpowerLevel::RANK_1, target); // Quick rank 1
                return;
            }

            if (this->CanCastSpell(DISINTEGRATE, target))
            {
                this->CastSpell(target, DISINTEGRATE);
                ConsumeEssence(3);
                return;
            }
        }

        // Generate essence quickly
        if (essence < 3 && this->CanCastSpell(AZURE_STRIKE, target))
        {
            this->CastSpell(target, AZURE_STRIKE);
            GenerateEssence(2);
            return;
        }
    }

    void UpdateDevastationState()
    {
        _dragonrageTracker.Update();

        // Update Essence Burst stacks from aura
        Player* bot = this->GetBot();
        if (bot)
        {
            if (Aura* aura = bot->GetAura(ESSENCE_BURST))
                _essenceBurstStacks = aura->GetStackAmount();
            else
                _essenceBurstStacks = 0;

            // Sync essence with actual resource
            this->_resource.essence = bot->GetPower(POWER_ALTERNATE_POWER);
        }
    }

    void HandleEmergencyDefensives()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        float healthPct = bot->GetHealthPct();

        // Obsidian Scales at 40% HP
        if (healthPct < 40.0f && this->CanCastSpell(OBSIDIAN_SCALES, bot))
        {
            this->CastSpell(bot, OBSIDIAN_SCALES);
            return;
        }

        // Renewing Blaze at 50% HP
        if (healthPct < 50.0f && this->CanCastSpell(RENEWING_BLAZE, bot))
        {
            this->CastSpell(bot, RENEWING_BLAZE);
            return;
        }
    }

    void StartEmpoweredSpell(uint32 spellId, EmpowerLevel targetLevel, ::Unit* target)
    {
        _empowermentTracker.StartEmpower(spellId, targetLevel);
        this->CastSpell(target, spellId); // Start the channel
    }

    void ReleaseEmpoweredSpell()
    {
        uint32 spellId = _empowermentTracker.GetSpellId();
        EmpowerLevel achievedLevel = _empowermentTracker.GetAchievedLevel();

        // Stop the channel (release at achieved rank)
        Player* bot = this->GetBot();
        if (bot && bot->IsNonMeleeSpellCast(false))
        {
            bot->InterruptNonMeleeSpells(false);
        }

        ConsumeEssence(3); // All empowered spells cost 3 essence

        _empowermentTracker.StopEmpower();

        TC_LOG_DEBUG("playerbot", "DevastationEvoker {} released {} at rank {}",
                     bot->GetName(), spellId, static_cast<uint32>(achievedLevel));
    }

    void GenerateEssence(uint32 amount)
    {
        this->_resource.essence = std::min(this->_resource.essence + amount, this->_resource.maxEssence);
    }

    void ConsumeEssence(uint32 amount)
    {
        this->_resource.essence = (this->_resource.essence > amount) ? this->_resource.essence - amount : 0;
    }

    // ========================================================================
    // PHASE 5: DECISION SYSTEM INTEGRATION
    // ========================================================================

    void InitializeDevastationMechanics()
    {
        using namespace bot::ai;
        using namespace bot::ai::BehaviorTreeBuilder;

        BotAI* ai = this->GetBot()->GetBotAI();
        if (!ai) return;

        auto* queue = ai->GetActionPriorityQueue();
        if (queue)
        {
            // EMERGENCY: Defensive cooldowns
            queue->RegisterSpell(OBSIDIAN_SCALES, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);
            queue->AddCondition(OBSIDIAN_SCALES, [](Player* bot, Unit*) {
                return bot && bot->GetHealthPct() < 40.0f;
            }, "HP < 40% (30% dmg reduction, 90s CD)");

            queue->RegisterSpell(RENEWING_BLAZE, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);
            queue->AddCondition(RENEWING_BLAZE, [](Player* bot, Unit*) {
                return bot && bot->GetHealthPct() < 50.0f;
            }, "HP < 50% (self-heal, 90s CD)");

            // CRITICAL: Major burst cooldowns
            queue->RegisterSpell(DRAGONRAGE, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);
            queue->AddCondition(DRAGONRAGE, [this](Player*, Unit* target) {
                return target && this->_resource.essence >= 3 && !this->_dragonrageTracker.IsActive();
            }, "3+ essence, not active (18s burst, 2min CD)");

            queue->RegisterSpell(DEEP_BREATH, SpellPriority::CRITICAL, SpellCategory::DAMAGE_AOE);
            queue->AddCondition(DEEP_BREATH, [this](Player*, Unit* target) {
                return target && this->GetEnemiesInRange(25.0f) >= 3;
            }, "3+ enemies (flying breath, 2min CD)");

            // HIGH: Core rotation spells
            queue->RegisterSpell(SHATTERING_STAR, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(SHATTERING_STAR, [](Player*, Unit* target) {
                return target != nullptr;
            }, "Debuff target (20s CD)");

            queue->RegisterSpell(ETERNITY_SURGE, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(ETERNITY_SURGE, [this](Player*, Unit* target) {
                return target && this->_resource.essence >= 3 && !this->_empowermentTracker.IsChanneling();
            }, "3 essence (empowered, high ST damage)");

            queue->RegisterSpell(DISINTEGRATE, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(DISINTEGRATE, [this](Player*, Unit* target) {
                return target && this->_resource.essence >= 3;
            }, "3 essence (channel, high damage)");

            // MEDIUM: AoE and secondary spenders
            queue->RegisterSpell(FIRE_BREATH, SpellPriority::MEDIUM, SpellCategory::DAMAGE_AOE);
            queue->AddCondition(FIRE_BREATH, [this](Player*, Unit* target) {
                return target && this->_resource.essence >= 3 && !this->_empowermentTracker.IsChanneling();
            }, "3 essence (empowered, AoE DoT)");

            queue->RegisterSpell(PYRE, SpellPriority::MEDIUM, SpellCategory::DAMAGE_AOE);
            queue->AddCondition(PYRE, [this](Player*, Unit* target) {
                return target && this->_resource.essence >= 2 && this->GetEnemiesInRange(10.0f) >= 3;
            }, "2 essence, 3+ enemies (cone AoE)");

            // LOW: Essence generators
            queue->RegisterSpell(AZURE_STRIKE, SpellPriority::LOW, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(AZURE_STRIKE, [this](Player*, Unit* target) {
                return target && this->_resource.essence < 4;
            }, "Essence < 4 (generates 2 essence)");

            queue->RegisterSpell(LIVING_FLAME, SpellPriority::LOW, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(LIVING_FLAME, [this](Player*, Unit* target) {
                return target && this->_resource.essence < 5;
            }, "Essence < 5 (generates 1 essence)");

            // UTILITY: Interrupt and movement
            queue->RegisterSpell(QUELL, SpellPriority::HIGH, SpellCategory::CROWD_CONTROL);
            queue->AddCondition(QUELL, [](Player*, Unit* target) {
                return target && target->IsNonMeleeSpellCast(false);
            }, "Target casting (interrupt, 40s CD)");

            queue->RegisterSpell(HOVER, SpellPriority::MEDIUM, SpellCategory::UTILITY);
            queue->AddCondition(HOVER, [](Player* bot, Unit* target) {
                return bot && target && bot->GetDistance(target) < 15.0f;
            }, "< 15yd range (hover mode, reposition)");
        }

        auto* behaviorTree = ai->GetBehaviorTree();
        if (behaviorTree)
        {
            auto root = Selector("Devastation Evoker DPS", {
                // Tier 1: Emergency Defense
                Sequence("Emergency Defense", {
                    Condition("Low HP", [](Player* bot) {
                        return bot && bot->GetHealthPct() < 50.0f;
                    }),
                    Selector("Use defensive", {
                        Sequence("Obsidian Scales", {
                            Condition("< 40%", [](Player* bot) {
                                return bot->GetHealthPct() < 40.0f;
                            }),
                            Action("Cast Obsidian Scales", [this](Player* bot) {
                                if (this->CanCastSpell(OBSIDIAN_SCALES, bot)) {
                                    this->CastSpell(bot, OBSIDIAN_SCALES);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Renewing Blaze", {
                            Action("Cast Renewing Blaze", [this](Player* bot) {
                                if (this->CanCastSpell(RENEWING_BLAZE, bot)) {
                                    this->CastSpell(bot, RENEWING_BLAZE);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 2: Burst Cooldowns
                Sequence("Burst Phase", {
                    Condition("Has target", [this](Player* bot) {
                        return bot && bot->GetVictim();
                    }),
                    Condition("3+ essence", [this](Player*) {
                        return this->_resource.essence >= 3;
                    }),
                    Selector("Use cooldowns", {
                        Sequence("Dragonrage", {
                            Condition("Not active", [this](Player*) {
                                return !this->_dragonrageTracker.IsActive();
                            }),
                            Action("Cast Dragonrage", [this](Player* bot) {
                                if (this->CanCastSpell(DRAGONRAGE, bot)) {
                                    this->CastSpell(bot, DRAGONRAGE);
                                    this->_dragonrageTracker.Activate();
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 3: Core Rotation
                Sequence("Core Rotation", {
                    Condition("Has target", [this](Player* bot) {
                        return bot && bot->GetVictim();
                    }),
                    Condition("Not channeling", [this](Player*) {
                        return !this->_empowermentTracker.IsChanneling();
                    }),
                    Selector("Cast spells", {
                        Sequence("Shattering Star", {
                            Action("Cast Shattering Star", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(SHATTERING_STAR, target)) {
                                    this->CastSpell(target, SHATTERING_STAR);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Eternity's Surge", {
                            Condition("3+ essence", [this](Player*) {
                                return this->_resource.essence >= 3;
                            }),
                            Action("Cast Eternity's Surge", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(ETERNITY_SURGE, target)) {
                                    this->StartEmpoweredSpell(ETERNITY_SURGE, EmpowerLevel::RANK_3, target);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Disintegrate", {
                            Condition("3+ essence", [this](Player*) {
                                return this->_resource.essence >= 3;
                            }),
                            Action("Cast Disintegrate", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(DISINTEGRATE, target)) {
                                    this->CastSpell(target, DISINTEGRATE);
                                    this->ConsumeEssence(3);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 4: Essence Generation
                Sequence("Generate Essence", {
                    Condition("Has target", [this](Player* bot) {
                        return bot && bot->GetVictim();
                    }),
                    Condition("< 4 essence", [this](Player*) {
                        return this->_resource.essence < 4;
                    }),
                    Selector("Generate", {
                        Sequence("Azure Strike", {
                            Action("Cast Azure Strike", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(AZURE_STRIKE, target)) {
                                    this->CastSpell(target, AZURE_STRIKE);
                                    this->GenerateEssence(2);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Living Flame", {
                            Action("Cast Living Flame", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(LIVING_FLAME, target)) {
                                    this->CastSpell(target, LIVING_FLAME);
                                    this->GenerateEssence(1);
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
    DevastationEmpowermentTracker _empowermentTracker;
    DragonrageTracker _dragonrageTracker;
    uint32 _essenceBurstStacks;
    uint32 _lastEternityTime;
    uint32 _lastFireBreathTime;
};

} // namespace Playerbot
