/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
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

#ifndef PLAYERBOT_ARCANEMAGEREFACTORED_H
#define PLAYERBOT_ARCANEMAGEREFACTORED_H

#include "Player.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"
#include <unordered_map>
#include "../CombatSpecializationTemplates.h"

// Phase 5 Integration: Decision Systems
#include "../Decision/ActionPriorityQueue.h"
#include "../Decision/BehaviorTree.h"
#include "../BotAI.h"

namespace Playerbot
{

// WoW 11.2 (The War Within) - Arcane Mage Spell IDs
constexpr uint32 ARCANE_BLAST = 30451;
constexpr uint32 ARCANE_MISSILES = 5143;
constexpr uint32 ARCANE_BARRAGE = 44425;
constexpr uint32 ARCANE_SURGE = 365350;
constexpr uint32 ARCANE_ORB = 153626;
constexpr uint32 EVOCATION = 12051;
constexpr uint32 TOUCH_OF_MAGE = 321507; // Arcane-specific talent
constexpr uint32 ARCANE_FAMILIAR = 205022;
constexpr uint32 PRESENCE_OF_MIND = 205025;
constexpr uint32 ARCANE_INTELLECT = 1459;
constexpr uint32 ARCANE_EXPLOSION = 1449;
constexpr uint32 SUPERNOVA = 157980;
constexpr uint32 SHIFTING_POWER = 382440;
constexpr uint32 ICE_BLOCK = 45438;
constexpr uint32 MIRROR_IMAGE = 55342;
constexpr uint32 TIME_WARP = 80353;

// Arcane Charge tracker (stacks 1-4)
class ArcaneChargeTracker
{
public:
    ArcaneChargeTracker() : _charges(0), _maxCharges(4) {}

    void AddCharge(uint32 amount = 1)
    {
        _charges = std::min(_charges + amount, _maxCharges);
    }

    void ClearCharges()
    {
        _charges = 0;
    }

    [[nodiscard]] uint32 GetCharges() const { return _charges; }
    [[nodiscard]] bool IsMaxCharges() const { return _charges >= _maxCharges; }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        // Sync with actual Arcane Charges buff
        if (Aura* aura = bot->GetAura(36032)) // Arcane Charges buff ID
            _charges = aura->GetStackAmount();        else
            _charges = 0;
    }

private:
    uint32 _charges;
    uint32 _maxCharges;
};

// Clearcasting proc tracker (free Arcane Missiles)
class ClearcastingTracker
{
public:
    ClearcastingTracker() : _clearcastingActive(false), _clearcastingStacks(0), _clearcastingEndTime(0) {}

    void ActivateProc(uint32 stacks = 1)
    {
        _clearcastingActive = true;
        _clearcastingStacks = std::min(_clearcastingStacks + stacks, 3u); // Max 3 stacks
        _clearcastingEndTime = GameTime::GetGameTimeMS() + 15000; // 15 sec duration
    }

    void ConsumeProc()
    {
        if (_clearcastingStacks > 0)
            _clearcastingStacks--;

        if (_clearcastingStacks == 0)
            _clearcastingActive = false;
    }

    [[nodiscard]] bool IsActive() const
    {
        return _clearcastingActive && GameTime::GetGameTimeMS() < _clearcastingEndTime;
    }

    [[nodiscard]] uint32 GetStacks() const { return _clearcastingStacks; }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        if (Aura* aura = bot->GetAura(263725)) // Clearcasting buff ID
        {
            _clearcastingActive = true;
            _clearcastingStacks = aura->GetStackAmount();            _clearcastingEndTime = GameTime::GetGameTimeMS() + aura->GetDuration();        }
        else
        {
            _clearcastingActive = false;
            _clearcastingStacks = 0;
        }
    }

private:
    bool _clearcastingActive;
    uint32 _clearcastingStacks;
    uint32 _clearcastingEndTime;
};

class ArcaneMageRefactored : public RangedDpsSpecialization<ManaResource>
{
public:
    using Base = RangedDpsSpecialization<ManaResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::IsSpellReady;
    using Base::GetEnemiesInRange;
    using Base::_resource;

    explicit ArcaneMageRefactored(Player* bot)        : RangedDpsSpecialization<ManaResource>(bot)
        , _chargeTracker()
        , _clearcastingTracker()
        , _arcaneSurgeActive(false)
        , _arcaneSurgeEndTime(0)
        , _cooldowns()
    {
        // Register cooldowns for major abilities
        _cooldowns.RegisterBatch({
            {ARCANE_SURGE, 120000, 1},  // 2 min major DPS cooldown
            {EVOCATION, 90000, 1},  // 90 sec mana recovery
            {PRESENCE_OF_MIND, 60000, 1},  // 1 min instant cast
            {ARCANE_ORB, 60000, 1},  // 1 min AoE builder
            {SHIFTING_POWER, 60000, 1},  // 1 min cooldown reset
            {ICE_BLOCK, 240000, 1},  // 4 min immunity
            {MIRROR_IMAGE, 120000, 1},  // 2 min defensive decoy
            {TIME_WARP, 600000, 1}  // 10 min Heroism/Bloodlust
        });

        // Phase 5 Integration: Initialize decision systems
        InitializeArcaneMechanics();

        TC_LOG_DEBUG("playerbot", "ArcaneMageRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        if (!target || !this->GetBot())
            return;

        UpdateArcaneState();

        uint32 enemyCount = this->GetEnemiesInRange(40.0f);

        if (enemyCount >= 3)
            ExecuteAoERotation(target, enemyCount);        else
            ExecuteSingleTargetRotation(target);
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();        if (!bot)
            return;

        // Arcane Intellect buff
        if (!bot->HasAura(ARCANE_INTELLECT))
        {
            if (this->CanCastSpell(ARCANE_INTELLECT, bot))
            {
                this->CastSpell(bot, ARCANE_INTELLECT);
            }
        }

        // Arcane Familiar (if talented)
        if (bot->HasSpell(ARCANE_FAMILIAR) && !bot->HasAura(ARCANE_FAMILIAR))
        {
            if (this->CanCastSpell(ARCANE_FAMILIAR, bot))
            {
                this->CastSpell(bot, ARCANE_FAMILIAR);
            }
        }
    }

    void UpdateDefensives()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        float healthPct = bot->GetHealthPct();

        // Ice Block (critical emergency - immune)
        if (healthPct < 20.0f && this->CanCastSpell(ICE_BLOCK, bot))
        {
            this->CastSpell(bot, ICE_BLOCK);
            return;
        }

        // Mirror Image (defensive decoy)
        if (healthPct < 40.0f && this->CanCastSpell(MIRROR_IMAGE, bot))
        {
            this->CastSpell(bot, MIRROR_IMAGE);
            return;
        }

        // Shifting Power (reset cooldowns in emergency) - self-cast
        if (healthPct < 50.0f && this->CanCastSpell(SHIFTING_POWER, bot))
        {
            this->CastSpell(bot, SHIFTING_POWER);
            return;
        }
    }

private:
    

    void UpdateArcaneState()    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Resource is managed by base template class
        _chargeTracker.Update(bot);
        _clearcastingTracker.Update(bot);
        UpdateCooldownStates();
    }

    void UpdateCooldownStates()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Arcane Surge state
        if (_arcaneSurgeActive && GameTime::GetGameTimeMS() >= _arcaneSurgeEndTime)
            _arcaneSurgeActive = false;

        if (bot->HasAura(ARCANE_SURGE))
        {
            _arcaneSurgeActive = true;
            if (Aura* aura = bot->GetAura(ARCANE_SURGE))                _arcaneSurgeEndTime = GameTime::GetGameTimeMS() + aura->GetDuration();        }
    }

    void ExecuteSingleTargetRotation(::Unit* target)
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        uint32 charges = _chargeTracker.GetCharges();
        uint32 manaPercent = bot->GetPowerPct(POWER_MANA);

        // Arcane Surge (major DPS cooldown at 4 charges)
        if (charges >= 4 && manaPercent >= 70 && !_arcaneSurgeActive)
        {
            if (this->CanCastSpell(ARCANE_SURGE, bot))
            {
                this->CastSpell(bot, ARCANE_SURGE);
                _arcaneSurgeActive = true;
                _arcaneSurgeEndTime = GameTime::GetGameTimeMS() + 15000; // 15 sec
                return;
            }
        }

        // Touch of the Magi (apply damage amplification debuff at 4 charges)
        if (charges >= 4 && bot->HasSpell(TOUCH_OF_MAGE))        {
            if (this->CanCastSpell(TOUCH_OF_MAGE, target))
            {
                this->CastSpell(target, TOUCH_OF_MAGE);
                return;
            }
        }

        // Arcane Missiles with Clearcasting proc (free cast, no mana cost)
        if (_clearcastingTracker.IsActive())
        {
            if (this->CanCastSpell(ARCANE_MISSILES, target))
            {
                this->CastSpell(target, ARCANE_MISSILES);
                _clearcastingTracker.ConsumeProc();
                return;
            }
        }

        // Arcane Barrage (spend charges when at max or low mana)
        if (charges >= 4 || (charges >= 2 && manaPercent < 30))
        {
            if (this->CanCastSpell(ARCANE_BARRAGE, target))
            {
                this->CastSpell(target, ARCANE_BARRAGE);
                _chargeTracker.ClearCharges();
                return;
            }
        }

        // Presence of Mind (instant cast Arcane Blast)
        if (charges < 4 && this->CanCastSpell(PRESENCE_OF_MIND, bot))
        {
            this->CastSpell(bot, PRESENCE_OF_MIND);
            // Follow up with instant Arcane Blast
            if (this->CanCastSpell(ARCANE_BLAST, target))
            {
                this->CastSpell(target, ARCANE_BLAST);
                _chargeTracker.AddCharge();
                return;
            }
        }

        // Arcane Blast (builder - generates charges)
        if (manaPercent > 20 || charges < 4)
        {
            if (this->CanCastSpell(ARCANE_BLAST, target))
            {
                this->CastSpell(target, ARCANE_BLAST);
                _chargeTracker.AddCharge();

                // Chance to proc Clearcasting
                if (rand() % 100 < 10) // 10% chance (simplified)
                    _clearcastingTracker.ActivateProc();

                return;
            }
        }

        // Evocation (emergency mana regen)
        if (manaPercent < 20)
        {
            if (this->CanCastSpell(EVOCATION, bot))
            {
                this->CastSpell(bot, EVOCATION);
                return;
            }
        }
    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        uint32 charges = _chargeTracker.GetCharges();
        uint32 manaPercent = bot->GetPowerPct(POWER_MANA);

        // Arcane Surge for burst AoE
        if (charges >= 4 && manaPercent >= 70 && !_arcaneSurgeActive && enemyCount >= 4)
        {
            if (this->CanCastSpell(ARCANE_SURGE, bot))
            {
                this->CastSpell(bot, ARCANE_SURGE);
                _arcaneSurgeActive = true;
                _arcaneSurgeEndTime = GameTime::GetGameTimeMS() + 15000;
                return;
            }
        }

        // Arcane Orb (AoE builder)
        if (bot->HasSpell(ARCANE_ORB) && charges < 4)
        {
            if (this->CanCastSpell(ARCANE_ORB, target))
            {
                this->CastSpell(target, ARCANE_ORB);
                _chargeTracker.AddCharge(1);
                return;
            }
        }

        // Supernova (AoE damage and knockback)
        if (bot->HasSpell(SUPERNOVA) && enemyCount >= 3)
        {
            if (this->CanCastSpell(SUPERNOVA, target))
            {
                this->CastSpell(target, SUPERNOVA);
                return;
            }
        }

        // Arcane Barrage (AoE spender at max charges)
        if (charges >= 4)
        {
            if (this->CanCastSpell(ARCANE_BARRAGE, target))
            {
                this->CastSpell(target, ARCANE_BARRAGE);
                _chargeTracker.ClearCharges();
                return;
            }
        }

        // Arcane Missiles with Clearcasting
        if (_clearcastingTracker.IsActive())
        {
            if (this->CanCastSpell(ARCANE_MISSILES, target))
            {
                this->CastSpell(target, ARCANE_MISSILES);
                _clearcastingTracker.ConsumeProc();
                return;
            }
        }

        // Arcane Explosion (close-range AoE if enemies nearby)
        if (enemyCount >= 3 && this->GetEnemiesInRange(10.0f) >= 3)
        {
            if (this->CanCastSpell(ARCANE_EXPLOSION, bot))
            {
                this->CastSpell(bot, ARCANE_EXPLOSION);
                return;
            }
        }

        // Arcane Blast (builder)
        if (manaPercent > 20 || charges < 4)
        {
            if (this->CanCastSpell(ARCANE_BLAST, target))
            {
                this->CastSpell(target, ARCANE_BLAST);
                _chargeTracker.AddCharge();

                // Chance to proc Clearcasting
                if (rand() % 100 < 10) // 10% chance
                    _clearcastingTracker.ActivateProc();

                return;
            }
        }

        // Evocation for mana regen
        if (manaPercent < 20)
        {
            if (this->CanCastSpell(EVOCATION, bot))
            {
                this->CastSpell(bot, EVOCATION);
                return;
            }
        }
    }

    // Phase 5 Integration: Decision Systems Initialization
    void InitializeArcaneMechanics()
    {
        using namespace bot::ai;
        using namespace bot::ai::BehaviorTreeBuilder;

        BotAI* ai = this->GetBot()->GetBotAI();

        auto* queue = ai->GetActionPriorityQueue();
        if (queue)
        {
            // EMERGENCY: Defensive cooldowns
            queue->RegisterSpell(ICE_BLOCK, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);
            queue->AddCondition(ICE_BLOCK, [this](Player* bot, Unit* target) {
                return bot && bot->GetHealthPct() < 20.0f;
            }, "Bot HP < 20% (immunity)");

            queue->RegisterSpell(MIRROR_IMAGE, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);
            queue->AddCondition(MIRROR_IMAGE, [this](Player* bot, Unit* target) {
                return bot && bot->GetHealthPct() < 40.0f;
            }, "Bot HP < 40% (decoy)");

            // CRITICAL: Major burst cooldown
            queue->RegisterSpell(ARCANE_SURGE, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);
            queue->AddCondition(ARCANE_SURGE, [this](Player* bot, Unit* target) {
                return target && this->_chargeTracker.GetCharges() >= 4 &&
                       bot->GetPowerPct(POWER_MANA) >= 70 && !this->_arcaneSurgeActive;
            }, "4 charges, 70%+ mana, not active (15s burst)");

            queue->RegisterSpell(TOUCH_OF_MAGE, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);
            queue->AddCondition(TOUCH_OF_MAGE, [this](Player* bot, Unit* target) {
                return bot && bot->HasSpell(TOUCH_OF_MAGE) &&
                       target && this->_chargeTracker.GetCharges() >= 4;
            }, "Has talent, 4 charges (damage amplification)");

            // HIGH: Arcane Missiles with Clearcasting
            queue->RegisterSpell(ARCANE_MISSILES, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(ARCANE_MISSILES, [this](Player* bot, Unit* target) {
                return target && this->_clearcastingTracker.IsActive();
            }, "Clearcasting active (free cast, 3 charges)");

            queue->RegisterSpell(ARCANE_BARRAGE, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(ARCANE_BARRAGE, [this](Player* bot, Unit* target) {
                return target && (this->_chargeTracker.GetCharges() >= 4 ||
                       (this->_chargeTracker.GetCharges() >= 2 && bot->GetPowerPct(POWER_MANA) < 30));
            }, "4 charges OR (2+ charges and mana < 30%)");

            // MEDIUM: Charge builders and mana regen
            queue->RegisterSpell(PRESENCE_OF_MIND, SpellPriority::MEDIUM, SpellCategory::OFFENSIVE);
            queue->AddCondition(PRESENCE_OF_MIND, [this](Player* bot, Unit* target) {
                return target && this->_chargeTracker.GetCharges() < 4;
            }, "< 4 charges (instant Arcane Blast)");

            queue->RegisterSpell(ARCANE_ORB, SpellPriority::MEDIUM, SpellCategory::DAMAGE_AOE);
            queue->AddCondition(ARCANE_ORB, [this](Player* bot, Unit* target) {
                return bot && bot->HasSpell(ARCANE_ORB) &&
                       target && this->_chargeTracker.GetCharges() < 4;
            }, "Has talent, < 4 charges (AoE builder)");

            queue->RegisterSpell(ARCANE_BLAST, SpellPriority::MEDIUM, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(ARCANE_BLAST, [this](Player* bot, Unit* target) {
                return target && (bot->GetPowerPct(POWER_MANA) > 20 ||
                       this->_chargeTracker.GetCharges() < 4);
            }, "Mana > 20% OR < 4 charges (builder)");

            // LOW: Mana recovery
            queue->RegisterSpell(EVOCATION, SpellPriority::LOW, SpellCategory::UTILITY);
            queue->AddCondition(EVOCATION, [this](Player* bot, Unit* target) {
                return bot && bot->GetPowerPct(POWER_MANA) < 20;
            }, "Mana < 20% (channel mana regen)");

            TC_LOG_INFO("module.playerbot", "🔮 ARCANE MAGE: Registered {} spells in ActionPriorityQueue", queue->GetSpellCount());
        }

        auto* behaviorTree = ai->GetBehaviorTree();
        if (behaviorTree)
        {
            auto root = Selector("Arcane Mage DPS", {
                // Tier 1: Burst Cooldowns (Arcane Surge, Touch of the Magi)
                Sequence("Burst Cooldowns", {
                    Condition("Target exists and 4 charges", [this](Player* bot, Unit* target) {
                        return target != nullptr && this->_chargeTracker.GetCharges() >= 4;
                    }),
                    Selector("Use Burst", {
                        Sequence("Cast Arcane Surge", {
                            Condition("70%+ mana, not active", [this](Player* bot, Unit* target) {
                                return bot && bot->GetPowerPct(POWER_MANA) >= 70 &&
                                       !this->_arcaneSurgeActive;
                            }),
                            Action("Cast Arcane Surge", [this](Player* bot, Unit* target) -> NodeStatus {
                                if (this->CanCastSpell(ARCANE_SURGE, bot))
                                {
                                    this->CastSpell(bot, ARCANE_SURGE);
                                    this->_arcaneSurgeActive = true;
                                    this->_arcaneSurgeEndTime = GameTime::GetGameTimeMS() + 15000;
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Cast Touch of the Magi", {
                            Condition("Has talent", [this](Player* bot, Unit* target) {
                                return bot && bot->HasSpell(TOUCH_OF_MAGE);
                            }),
                            Action("Cast Touch of the Magi", [this](Player* bot, Unit* target) -> NodeStatus {
                                if (this->CanCastSpell(TOUCH_OF_MAGE, target))
                                {
                                    this->CastSpell(target, TOUCH_OF_MAGE);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 2: Clearcasting Proc (Arcane Missiles)
                Sequence("Clearcasting Proc", {
                    Condition("Target exists and has proc", [this](Player* bot, Unit* target) {
                        return target != nullptr && this->_clearcastingTracker.IsActive();
                    }),
                    Action("Cast Arcane Missiles", [this](Player* bot, Unit* target) -> NodeStatus {
                        if (this->CanCastSpell(ARCANE_MISSILES, target))
                        {
                            this->CastSpell(target, ARCANE_MISSILES);
                            this->_clearcastingTracker.ConsumeProc();
                            return NodeStatus::SUCCESS;
                        }
                        return NodeStatus::FAILURE;
                    })
                }),

                // Tier 3: Charge Management (Arcane Barrage to spend)
                Sequence("Charge Management", {
                    Condition("Target exists", [this](Player* bot, Unit* target) {
                        return target != nullptr;
                    }),
                    Selector("Spend or Build", {
                        // Spend at 4 charges or low mana
                        Sequence("Spend Charges", {
                            Condition("4 charges OR (2+ charges and low mana)", [this](Player* bot, Unit* target) {
                                return this->_chargeTracker.GetCharges() >= 4 ||
                                       (this->_chargeTracker.GetCharges() >= 2 &&
                                        bot->GetPowerPct(POWER_MANA) < 30);
                            }),
                            Action("Cast Arcane Barrage", [this](Player* bot, Unit* target) -> NodeStatus {
                                if (this->CanCastSpell(ARCANE_BARRAGE, target))
                                {
                                    this->CastSpell(target, ARCANE_BARRAGE);
                                    this->_chargeTracker.ClearCharges();
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        // Build with Presence of Mind
                        Sequence("Use Presence of Mind", {
                            Condition("< 4 charges", [this](Player* bot, Unit* target) {
                                return this->_chargeTracker.GetCharges() < 4;
                            }),
                            Action("Cast Presence of Mind", [this](Player* bot, Unit* target) -> NodeStatus {
                                if (this->CanCastSpell(PRESENCE_OF_MIND, bot))
                                {
                                    this->CastSpell(bot, PRESENCE_OF_MIND);
                                    // Follow with instant Arcane Blast
                                    if (this->CanCastSpell(ARCANE_BLAST, target))
                                    {
                                        this->CastSpell(target, ARCANE_BLAST);
                                        this->_chargeTracker.AddCharge();
                                    }
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 4: Charge Builder (Arcane Blast)
                Sequence("Charge Builder", {
                    Condition("Target exists", [this](Player* bot, Unit* target) {
                        return target != nullptr &&
                               (bot->GetPowerPct(POWER_MANA) > 20 ||
                                this->_chargeTracker.GetCharges() < 4);
                    }),
                    Action("Cast Arcane Blast", [this](Player* bot, Unit* target) -> NodeStatus {
                        if (this->CanCastSpell(ARCANE_BLAST, target))
                        {
                            this->CastSpell(target, ARCANE_BLAST);
                            this->_chargeTracker.AddCharge();
                            // Chance to proc Clearcasting
                            if (rand() % 100 < 10)
                                this->_clearcastingTracker.ActivateProc();
                            return NodeStatus::SUCCESS;
                        }
                        return NodeStatus::FAILURE;
                    })
                })
            });

            behaviorTree->SetRoot(root);
            TC_LOG_INFO("module.playerbot", "🌲 ARCANE MAGE: BehaviorTree initialized with 4-tier DPS rotation");
        }
    }

    // Member variables
    ArcaneChargeTracker _chargeTracker;
    ClearcastingTracker _clearcastingTracker;

    bool _arcaneSurgeActive;
    uint32 _arcaneSurgeEndTime;

    CooldownManager _cooldowns;
};

} // namespace Playerbot

#endif // PLAYERBOT_ARCANEMAGEREFACTORED_H
