/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * Assassination Rogue Refactored - Template-Based Implementation
 *
 * Fully refactored to use unified utility classes:
 * - DotTracker from Common/StatusEffectTracker.h (eliminates custom tracker)
 * - CooldownManager from Common/CooldownManager.h (eliminates InitializeCooldowns())
 * - Helper utilities from Common/RotationHelpers.h
 *
 * BEFORE: 434 lines with duplicate tracker and cooldown code
 * AFTER: ~320 lines using shared utilities
 */

#pragma once

#include "../CombatSpecializationTemplates.h"
#include "../Common/StatusEffectTracker.h"
#include "../Common/CooldownManager.h"
#include "../Common/RotationHelpers.h"
#include "RogueResourceTypes.h"
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

// ============================================================================
// ASSASSINATION ROGUE REFACTORED
// ============================================================================

class AssassinationRogueRefactored : public MeleeDpsSpecialization<ComboPointsAssassination>
{
public:
    // Assassination Rogue Spell IDs
    static constexpr uint32 GARROTE = 703;
    static constexpr uint32 RUPTURE = 1943;
    static constexpr uint32 ENVENOM = 32645;
    static constexpr uint32 VENDETTA = 79140;
    static constexpr uint32 MUTILATE = 1329;
    static constexpr uint32 FAN_OF_KNIVES = 51723;
    static constexpr uint32 CRIMSON_TEMPEST = 121411;
    static constexpr uint32 EXSANGUINATE = 200806;
    static constexpr uint32 KINGSBANE = 192759;


public:
    // Use base class members with type alias for cleaner syntax
    using Base = MeleeDpsSpecialization<ComboPointsAssassination>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::GetEnemiesInRange;
    using Base::_resource;

    explicit AssassinationRogueRefactored(Player* bot)
        : MeleeDpsSpecialization<ComboPointsAssassination>(bot)
        , _dotTracker()
        , _cooldowns()
        , _inStealth(false)
        , _lastMutilateTime(0)
        , _lastEnvenomTime(0)
        , _vendettaActive(false)
        , _vendettaEndTime(0)
    {
        // Initialize energy/combo resources
        this->_resource.maxEnergy = bot->HasSpell(RogueAI::VIGOR) ? 120 : 100;
        this->_resource.maxComboPoints = bot->HasSpell(RogueAI::DEEPER_STRATAGEM) ? 6 : 5;
        this->_resource.energy = this->_resource.maxEnergy;
        this->_resource.comboPoints = 0;

        // Register DoT tracking
        _dotTracker.RegisterDot(RogueAI::GARROTE, 18000);
        _dotTracker.RegisterDot(RogueAI::RUPTURE, 24000); // 4s base per CP
        _dotTracker.RegisterDot(RogueAI::CRIMSON_TEMPEST, 14000);

        // Register cooldowns using CooldownManager
        _cooldowns.RegisterBatch({
            {VENDETTA, CooldownPresets::MINOR_OFFENSIVE, 1},      // 2 min CD
            {RogueAI::DEATHMARK, CooldownPresets::MINOR_OFFENSIVE, 1}, // 2 min CD
            {KINGSBANE, CooldownPresets::OFFENSIVE_60, 1},        // 1 min CD
            {EXSANGUINATE, CooldownPresets::OFFENSIVE_45, 1},     // 45 sec CD
            {RogueAI::VANISH, CooldownPresets::MINOR_OFFENSIVE, 1},    // 2 min CD
            {RogueAI::CLOAK_OF_SHADOWS, CooldownPresets::MINOR_DEFENSIVE, 1}, // 2 min CD
            {RogueAI::KICK, CooldownPresets::INTERRUPT, 1},       // 15 sec CD
            {RogueAI::BLIND, CooldownPresets::MINOR_DEFENSIVE, 1} // 2 min CD
        });

        // Phase 5 Integration: Initialize decision systems
        InitializeAssassinationMechanics();

        TC_LOG_DEBUG("playerbot", "AssassinationRogueRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))
            return;

        // Update tracking systems
        UpdateAssassinationState();

        // Check stealth status
        _inStealth = this->GetBot()->HasAuraType(SPELL_AURA_MOD_STEALTH);

        // Stealth opener
        if (_inStealth)
        {
            ExecuteStealthOpener(target);
            return;
        }

        // Main rotation
        uint32 enemyCount = this->GetEnemiesInRange(10.0f);
        if (enemyCount >= 3)        {
            ExecuteAoERotation(target, enemyCount);
        }
        else
        {
            ExecuteSingleTargetRotation(target);
        }
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();        // Maintain poisons
        if (!bot->HasAura(RogueAI::DEADLY_POISON) && this->CanCastSpell(RogueAI::DEADLY_POISON, bot))
        {
            this->CastSpell(bot, RogueAI::DEADLY_POISON);
        }

        // Enter stealth out of combat
        if (!bot->IsInCombat() && !_inStealth && this->CanCastSpell(RogueAI::STEALTH, bot))
        {
            this->CastSpell(bot, RogueAI::STEALTH);
        }

        // Defensive cooldowns
        if (bot->GetHealthPct() < 30.0f && this->CanCastSpell(RogueAI::CLOAK_OF_SHADOWS, bot))
        {
            this->CastSpell(bot, RogueAI::CLOAK_OF_SHADOWS);
        }
    }

    // GetOptimalRange is final in MeleeDpsSpecialization, cannot override
    // Returns 5.0f melee range by default

protected:
    void ExecuteSingleTargetRotation(::Unit* target)
    {
        uint32 energy = this->_resource.energy;
        uint32 cp = this->_resource.comboPoints;
        uint32 maxCp = this->_resource.maxComboPoints;

        // Priority 1: Vendetta on cooldown
        if (this->CanCastSpell(VENDETTA, target))
        {
            this->CastSpell(target, VENDETTA);
            _vendettaActive = true;
            _vendettaEndTime = GameTime::GetGameTimeMS() + 20000;
            return;
        }

        // Priority 2: Deathmark on cooldown
        if (this->CanCastSpell(RogueAI::DEATHMARK, target))
        {
            this->CastSpell(target, RogueAI::DEATHMARK);
            return;
        }

        // Priority 3: Refresh Garrote
        if (_dotTracker.NeedsRefresh(target->GetGUID(), RogueAI::GARROTE) && energy >= 45)
        {
            if (this->CanCastSpell(RogueAI::GARROTE, target))
            {
                this->CastSpell(target, RogueAI::GARROTE);
                _dotTracker.ApplyDot(target->GetGUID(), RogueAI::GARROTE);
                ConsumeEnergy(45);
                return;
            }
        }

        // Priority 4: Finishers at 4-5+ CP
        if (cp >= (maxCp - 1))
        {
            // Refresh Rupture if needed
            if (_dotTracker.NeedsRefresh(target->GetGUID(), RogueAI::RUPTURE) && energy >= 25)
            {
                if (this->CanCastSpell(RogueAI::RUPTURE, target))
                {
                    this->CastSpell(target, RogueAI::RUPTURE);
                    uint32 ruptDuration = 4000 * cp; // 4s per CP
                    _dotTracker.ApplyDot(target->GetGUID(), RogueAI::RUPTURE, ruptDuration);
                    ConsumeEnergy(25);
                    this->_resource.comboPoints = 0;
                    return;
                }
            }

            // Envenom for damage
            if (energy >= 35 && this->CanCastSpell(ENVENOM, target))
            {
                this->CastSpell(target, ENVENOM);
                _lastEnvenomTime = GameTime::GetGameTimeMS();
                ConsumeEnergy(35);
                this->_resource.comboPoints = 0;
                return;
            }
        }

        // Priority 5: Kingsbane (talent)
        if (energy >= 35 && this->CanCastSpell(KINGSBANE, target))
        {
            this->CastSpell(target, KINGSBANE);
            ConsumeEnergy(35);
            return;
        }

        // Priority 6: Mutilate for combo points
        if (energy >= 50 && cp < maxCp)
        {
            if (this->CanCastSpell(MUTILATE, target))
            {
                this->CastSpell(target, MUTILATE);
                _lastMutilateTime = GameTime::GetGameTimeMS();
                ConsumeEnergy(50);
                GenerateComboPoints(2);
                return;
            }
        }

        // Priority 7: Poisoned Knife if can't melee
        if (PositionUtils::GetDistance(this->GetBot(), target) > 10.0f && energy >= 40)
        {
            if (this->CanCastSpell(RogueAI::POISONED_KNIFE, target))
            {
                this->CastSpell(target, RogueAI::POISONED_KNIFE);
                ConsumeEnergy(40);
                GenerateComboPoints(1);
                return;
            }
        }
    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)
    {
        uint32 energy = this->_resource.energy;
        uint32 cp = this->_resource.comboPoints;
        uint32 maxCp = this->_resource.maxComboPoints;

        // Priority 1: Crimson Tempest finisher
        if (cp >= 4 && energy >= 35)
        {
            if (this->GetBot()->HasSpell(RogueAI::CRIMSON_TEMPEST) && this->CanCastSpell(RogueAI::CRIMSON_TEMPEST, this->GetBot()))
            {
                this->CastSpell(this->GetBot(), RogueAI::CRIMSON_TEMPEST);
                _dotTracker.ApplyDot(target->GetGUID(), RogueAI::CRIMSON_TEMPEST);
                ConsumeEnergy(35);
                this->_resource.comboPoints = 0;
                return;
            }
        }

        // Priority 2: Fan of Knives for AoE combo building
        if (energy >= 35 && cp < maxCp)
        {
            if (this->CanCastSpell(FAN_OF_KNIVES, this->GetBot()))
            {
                this->CastSpell(this->GetBot(), FAN_OF_KNIVES);
                ConsumeEnergy(35);
                GenerateComboPoints(std::min(enemyCount, 5u)); // 1 CP per target hit
                return;
            }
        }

        // Fallback to single target
        ExecuteSingleTargetRotation(target);
    }

    void ExecuteStealthOpener(::Unit* target)
    {
        // Priority 1: Garrote from stealth (silence)
        if (this->CanCastSpell(RogueAI::GARROTE, target))
        {
            this->CastSpell(target, RogueAI::GARROTE);
            _dotTracker.ApplyDot(target->GetGUID(), RogueAI::GARROTE);
            _inStealth = false;
            return;
        }

        // Priority 2: Cheap Shot for stun
        if (this->CanCastSpell(RogueAI::CHEAP_SHOT, target))
        {
            this->CastSpell(target, RogueAI::CHEAP_SHOT);
            GenerateComboPoints(2);
            _inStealth = false;
            return;
        }

        // Priority 3: Ambush for damage
        if (this->CanCastSpell(RogueAI::AMBUSH, target))
        {
            this->CastSpell(target, RogueAI::AMBUSH);
            GenerateComboPoints(2);
            _inStealth = false;
            return;
        }
    }

private:
    void UpdateAssassinationState()
    {
        // Update DoT tracker
        _dotTracker.Update();

        // Check Vendetta expiry
        if (_vendettaActive && GameTime::GetGameTimeMS() >= _vendettaEndTime)
        {
            _vendettaActive = false;
            _vendettaEndTime = 0;
        }

        // Regenerate energy (10 per second)
        uint32 now = GameTime::GetGameTimeMS();
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
    {
        this->_resource.energy = (this->_resource.energy > amount) ? this->_resource.energy - amount : 0;
    }

    void GenerateComboPoints(uint32 amount)
    {
        this->_resource.comboPoints = std::min(this->_resource.comboPoints + amount, this->_resource.maxComboPoints);
    }

    // Phase 5 Integration: Decision Systems Initialization
    void InitializeAssassinationMechanics()
    {
        using namespace bot::ai;
        using namespace bot::ai::BehaviorTreeBuilder;

        BotAI* ai = this->GetBot()->GetBotAI();

        // ========================================================================
        // ActionPriorityQueue: Register Assassination Rogue spells with priorities
        // ========================================================================
        auto* queue = ai->GetActionPriorityQueue();
        if (queue)
        {
            // EMERGENCY: Defensive cooldowns
            queue->RegisterSpell(RogueAI::CLOAK_OF_SHADOWS, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);
            queue->AddCondition(RogueAI::CLOAK_OF_SHADOWS, [this](Player* bot, Unit* target) {
                return bot && bot->GetHealthPct() < 30.0f;
            }, "Bot HP < 30% (spell immunity)");

            // CRITICAL: Burst cooldowns and stealth openers
            queue->RegisterSpell(VENDETTA, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);
            queue->AddCondition(VENDETTA, [this](Player* bot, Unit* target) {
                return target && !this->_vendettaActive;
            }, "Not active (20s burst window, 30% damage increase)");

            queue->RegisterSpell(RogueAI::DEATHMARK, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);
            queue->AddCondition(RogueAI::DEATHMARK, [this](Player* bot, Unit* target) {
                return bot && bot->HasSpell(RogueAI::DEATHMARK) && target;
            }, "Has talent (burst cooldown)");

            queue->RegisterSpell(RogueAI::GARROTE, SpellPriority::CRITICAL, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(RogueAI::GARROTE, [this](Player* bot, Unit* target) {
                return target && this->_inStealth;
            }, "In stealth (opener with silence)");

            // HIGH: DoT maintenance and finishers
            queue->RegisterSpell(RogueAI::GARROTE, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(RogueAI::GARROTE, [this](Player* bot, Unit* target) {
                return target && !this->_inStealth &&
                       this->_resource.energy >= 45 &&
                       this->_dotTracker.NeedsRefresh(target->GetGUID(), RogueAI::GARROTE);
            }, "45+ Energy, DoT needs refresh (18s duration)");

            queue->RegisterSpell(RogueAI::RUPTURE, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(RogueAI::RUPTURE, [this](Player* bot, Unit* target) {
                return target && this->_resource.energy >= 25 &&
                       this->_resource.comboPoints >= (this->_resource.maxComboPoints - 1) &&
                       this->_dotTracker.NeedsRefresh(target->GetGUID(), RogueAI::RUPTURE);
            }, "25+ Energy, 4-5+ CP, DoT needs refresh (finisher)");

            queue->RegisterSpell(ENVENOM, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(ENVENOM, [this](Player* bot, Unit* target) {
                return target && this->_resource.energy >= 35 &&
                       this->_resource.comboPoints >= (this->_resource.maxComboPoints - 1);
            }, "35+ Energy, 4-5+ CP (finisher damage)");

            // MEDIUM: Combo builders and talents
            queue->RegisterSpell(KINGSBANE, SpellPriority::MEDIUM, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(KINGSBANE, [this](Player* bot, Unit* target) {
                return bot && bot->HasSpell(KINGSBANE) &&
                       target && this->_resource.energy >= 35;
            }, "Has talent, 35+ Energy (poisoned weapon)");

            queue->RegisterSpell(MUTILATE, SpellPriority::MEDIUM, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(MUTILATE, [this](Player* bot, Unit* target) {
                return target && this->_resource.energy >= 50 &&
                       this->_resource.comboPoints < this->_resource.maxComboPoints;
            }, "50+ Energy, not max CP (generates 2 CP)");

            queue->RegisterSpell(RogueAI::KICK, SpellPriority::MEDIUM, SpellCategory::UTILITY);
            queue->AddCondition(RogueAI::KICK, [this](Player* bot, Unit* target) {
                return target && target->IsNonMeleeSpellCast(false);
            }, "Target casting (interrupt)");

            // LOW: AoE and ranged filler
            queue->RegisterSpell(FAN_OF_KNIVES, SpellPriority::LOW, SpellCategory::DAMAGE_AOE);
            queue->AddCondition(FAN_OF_KNIVES, [this](Player* bot, Unit* target) {
                return target && this->_resource.energy >= 35 &&
                       this->GetEnemiesInRange(10.0f) >= 3 &&
                       this->_resource.comboPoints < this->_resource.maxComboPoints;
            }, "35+ Energy, 3+ enemies, not max CP (AoE combo builder)");

            queue->RegisterSpell(RogueAI::POISONED_KNIFE, SpellPriority::LOW, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(RogueAI::POISONED_KNIFE, [this](Player* bot, Unit* target) {
                return bot && bot->HasSpell(RogueAI::POISONED_KNIFE) &&
                       target && this->_resource.energy >= 40 &&
                       PositionUtils::GetDistance(bot, target) > 10.0f;
            }, "Has talent, 40+ Energy, > 10 yards (ranged builder)");

            TC_LOG_INFO("module.playerbot", "🗡️ ASSASSINATION ROGUE: Registered {} spells in ActionPriorityQueue", queue->GetSpellCount());
        }

        // ========================================================================
        // BehaviorTree: Assassination Rogue DPS rotation logic
        // ========================================================================
        auto* behaviorTree = ai->GetBehaviorTree();
        if (behaviorTree)
        {
            auto root = Selector("Assassination Rogue DPS", {
                // Tier 1: Stealth Opener
                Sequence("Stealth Opener", {
                    Condition("In stealth", [this](Player* bot, Unit* target) {
                        return this->_inStealth && target != nullptr;
                    }),
                    Selector("Choose Opener", {
                        Sequence("Cast Garrote", {
                            Action("Cast Garrote", [this](Player* bot, Unit* target) -> NodeStatus {
                                if (this->CanCastSpell(RogueAI::GARROTE, target))
                                {
                                    this->CastSpell(target, RogueAI::GARROTE);
                                    this->_dotTracker.ApplyDot(target->GetGUID(), RogueAI::GARROTE);
                                    this->_inStealth = false;
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 2: Burst Cooldowns (Vendetta → Deathmark)
                Sequence("Burst Cooldowns", {
                    Condition("Target exists", [this](Player* bot, Unit* target) {
                        return target != nullptr;
                    }),
                    Selector("Use Burst", {
                        Sequence("Cast Vendetta", {
                            Condition("Not active", [this](Player* bot, Unit* target) {
                                return !this->_vendettaActive;
                            }),
                            Action("Cast Vendetta", [this](Player* bot, Unit* target) -> NodeStatus {
                                if (this->CanCastSpell(VENDETTA, target))
                                {
                                    this->CastSpell(target, VENDETTA);
                                    this->_vendettaActive = true;
                                    this->_vendettaEndTime = GameTime::GetGameTimeMS() + 20000;
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Cast Deathmark", {
                            Condition("Has talent", [this](Player* bot, Unit* target) {
                                return bot && bot->HasSpell(RogueAI::DEATHMARK);
                            }),
                            Action("Cast Deathmark", [this](Player* bot, Unit* target) -> NodeStatus {
                                if (this->CanCastSpell(RogueAI::DEATHMARK, target))
                                {
                                    this->CastSpell(target, RogueAI::DEATHMARK);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 3: DoT Maintenance (Garrote, Rupture)
                Sequence("DoT Maintenance", {
                    Condition("Target exists", [this](Player* bot, Unit* target) {
                        return target != nullptr && this->_resource.energy >= 25;
                    }),
                    Selector("Maintain DoTs", {
                        // Garrote refresh
                        Sequence("Refresh Garrote", {
                            Condition("Garrote needs refresh", [this](Player* bot, Unit* target) {
                                return this->_resource.energy >= 45 &&
                                       this->_dotTracker.NeedsRefresh(target->GetGUID(), RogueAI::GARROTE);
                            }),
                            Action("Cast Garrote", [this](Player* bot, Unit* target) -> NodeStatus {
                                if (this->CanCastSpell(RogueAI::GARROTE, target))
                                {
                                    this->CastSpell(target, RogueAI::GARROTE);
                                    this->_dotTracker.ApplyDot(target->GetGUID(), RogueAI::GARROTE);
                                    this->ConsumeEnergy(45);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        // Rupture refresh (finisher)
                        Sequence("Refresh Rupture", {
                            Condition("Rupture needs refresh at 4-5 CP", [this](Player* bot, Unit* target) {
                                return this->_resource.comboPoints >= (this->_resource.maxComboPoints - 1) &&
                                       this->_dotTracker.NeedsRefresh(target->GetGUID(), RogueAI::RUPTURE);
                            }),
                            Action("Cast Rupture", [this](Player* bot, Unit* target) -> NodeStatus {
                                if (this->CanCastSpell(RogueAI::RUPTURE, target))
                                {
                                    this->CastSpell(target, RogueAI::RUPTURE);
                                    uint32 ruptDuration = 4000 * this->_resource.comboPoints;
                                    this->_dotTracker.ApplyDot(target->GetGUID(), RogueAI::RUPTURE, ruptDuration);
                                    this->ConsumeEnergy(25);
                                    this->_resource.comboPoints = 0;
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 4: Finisher and Builder (Envenom at 4-5 CP, Mutilate for CP)
                Sequence("Finisher and Builder", {
                    Condition("Target exists", [this](Player* bot, Unit* target) {
                        return target != nullptr;
                    }),
                    Selector("Spend or Build CP", {
                        // Envenom (finisher)
                        Sequence("Cast Envenom", {
                            Condition("4-5+ CP and 35+ Energy", [this](Player* bot, Unit* target) {
                                return this->_resource.comboPoints >= (this->_resource.maxComboPoints - 1) &&
                                       this->_resource.energy >= 35;
                            }),
                            Action("Cast Envenom", [this](Player* bot, Unit* target) -> NodeStatus {
                                if (this->CanCastSpell(ENVENOM, target))
                                {
                                    this->CastSpell(target, ENVENOM);
                                    this->_lastEnvenomTime = GameTime::GetGameTimeMS();
                                    this->ConsumeEnergy(35);
                                    this->_resource.comboPoints = 0;
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        // Kingsbane (talent)
                        Sequence("Cast Kingsbane", {
                            Condition("Has talent and 35+ Energy", [this](Player* bot, Unit* target) {
                                return bot && bot->HasSpell(KINGSBANE) &&
                                       this->_resource.energy >= 35;
                            }),
                            Action("Cast Kingsbane", [this](Player* bot, Unit* target) -> NodeStatus {
                                if (this->CanCastSpell(KINGSBANE, target))
                                {
                                    this->CastSpell(target, KINGSBANE);
                                    this->ConsumeEnergy(35);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        // Mutilate (builder)
                        Sequence("Cast Mutilate", {
                            Condition("50+ Energy, not max CP", [this](Player* bot, Unit* target) {
                                return this->_resource.energy >= 50 &&
                                       this->_resource.comboPoints < this->_resource.maxComboPoints;
                            }),
                            Action("Cast Mutilate", [this](Player* bot, Unit* target) -> NodeStatus {
                                if (this->CanCastSpell(MUTILATE, target))
                                {
                                    this->CastSpell(target, MUTILATE);
                                    this->_lastMutilateTime = GameTime::GetGameTimeMS();
                                    this->ConsumeEnergy(50);
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
            TC_LOG_INFO("module.playerbot", "🌲 ASSASSINATION ROGUE: BehaviorTree initialized with 4-tier DPS rotation");
        }
    }

private:
    // Unified utilities (eliminates duplicate tracker code)
    DotTracker _dotTracker;
    CooldownManager _cooldowns;
    bool _inStealth;
    uint32 _lastMutilateTime;
    uint32 _lastEnvenomTime;
    bool _vendettaActive;
    uint32 _vendettaEndTime;
};

} // namespace Playerbot
