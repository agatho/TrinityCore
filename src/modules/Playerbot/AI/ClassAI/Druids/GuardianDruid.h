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

#ifndef PLAYERBOT_GUARDIANDRUIDREFACTORED_H
#define PLAYERBOT_GUARDIANDRUIDREFACTORED_H

#include "../CombatSpecializationTemplates.h"
#include "../../Services/ThreatAssistant.h"
#include "Player.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include <unordered_map>
#include "Log.h"
// Phase 5 Integration: Decision Systems
#include "../Decision/ActionPriorityQueue.h"
#include "../Decision/BehaviorTree.h"
#include "../BotAI.h"

namespace Playerbot
{

// WoW 11.2 (The War Within) - Guardian Druid Spell IDs
constexpr uint32 GUARDIAN_MANGLE = 33917;
constexpr uint32 GUARDIAN_THRASH = 77758;
constexpr uint32 GUARDIAN_SWIPE = 213771;
constexpr uint32 GUARDIAN_MAUL = 6807;
constexpr uint32 GUARDIAN_IRONFUR = 192081;
constexpr uint32 GUARDIAN_FRENZIED_REGENERATION = 22842;
constexpr uint32 GUARDIAN_BARKSKIN = 22812;
constexpr uint32 GUARDIAN_SURVIVAL_INSTINCTS = 61336;
constexpr uint32 GUARDIAN_PULVERIZE = 80313; // Talent
constexpr uint32 GUARDIAN_INCARNATION_BEAR = 102558; // Incarnation: Guardian of Ursoc
constexpr uint32 GUARDIAN_BERSERK = 50334;
constexpr uint32 GUARDIAN_MOONFIRE = 8921;
constexpr uint32 GUARDIAN_RAGE_OF_SLEEPER = 200851; // Talent
constexpr uint32 GUARDIAN_BEAR_FORM = 5487;
constexpr uint32 GUARDIAN_BRISTLING_FUR = 155835; // Talent
constexpr uint32 GUARDIAN_RENEWAL = 108238;
constexpr uint32 GUARDIAN_REGROWTH = 8936;
constexpr uint32 GUARDIAN_GROWL = 6795; // Taunt

// Ironfur stacking tracker
class GuardianIronfurTracker
{
public:
    GuardianIronfurTracker() : _ironfurStacks(0), _ironfurEndTime(0) {}

    void ApplyIronfur(uint32 duration = 7000)
    {
        _ironfurStacks = std::min(_ironfurStacks + 1, 5u); // Max 5 stacks
        _ironfurEndTime = GameTime::GetGameTimeMS() + duration;
    }

    void Update(Player* bot)
    {
        if (!bot)

            return;

        // Sync with actual aura
        if (Aura* aura = bot->GetAura(GUARDIAN_IRONFUR))
        {

            _ironfurStacks = aura->GetStackAmount();
            _ironfurEndTime = GameTime::GetGameTimeMS() + aura->GetDuration();
            }
        else
        {

            _ironfurStacks = 0;

            _ironfurEndTime = 0;
        }
    }

    [[nodiscard]] uint32 GetStacks() const { return _ironfurStacks; }
    [[nodiscard]] bool IsActive() const { return _ironfurStacks > 0 && GameTime::GetGameTimeMS() < _ironfurEndTime; }
    [[nodiscard]] bool NeedsRefresh() const
    {
        // Refresh if no stacks or about to expire
        return _ironfurStacks == 0 || (_ironfurEndTime > 0 && (GameTime::GetGameTimeMS() + 2000) >= _ironfurEndTime);
    }
    [[nodiscard]] uint32 GetTimeRemaining() const
    {
        if (_ironfurEndTime == 0)

            return 0;
        uint32 now = GameTime::GetGameTimeMS();
        return now < _ironfurEndTime ? (_ironfurEndTime - now) : 0;
    }

private:
    CooldownManager _cooldowns;
    uint32 _ironfurStacks;
    uint32 _ironfurEndTime;
};

// Thrash debuff tracker (for Pulverize talent)
class GuardianThrashTracker
{
public:
    GuardianThrashTracker() = default;

    void ApplyThrash(ObjectGuid guid, uint32 duration, uint32 stacks = 1)
    {
        auto& thrash = _thrashTargets[guid];
        thrash.endTime = GameTime::GetGameTimeMS() + duration;
        thrash.stacks = std::min(thrash.stacks + stacks, 3u); // Max 3 stacks
    }

    void RemoveThrash(ObjectGuid guid)
    {
        _thrashTargets.erase(guid);
    }

    [[nodiscard]] uint32 GetStacks(ObjectGuid guid) const
    {
        auto it = _thrashTargets.find(guid);
        if (it != _thrashTargets.end() && GameTime::GetGameTimeMS() < it->second.endTime)

            return it->second.stacks;
        return 0;
    }    [[nodiscard]] bool HasThrash(ObjectGuid guid) const
    {
        return GetStacks(guid) > 0;
    }

    void Update(Unit* target)    {
        if (!target)

            return;

        ObjectGuid guid = target->GetGUID();        // Sync with actual aura
        if (Aura* aura = target->GetAura(GUARDIAN_THRASH))
        {

            auto& thrash = _thrashTargets[guid];

            thrash.stacks = aura->GetStackAmount();
            thrash.endTime = GameTime::GetGameTimeMS() + aura->GetDuration();
            }
        else
        {

            _thrashTargets.erase(guid);
        }
    }

private:
    struct ThrashInfo
    {
        uint32 stacks = 0;
        uint32 endTime = 0;
    };

    std::unordered_map<ObjectGuid, ThrashInfo> _thrashTargets;
};

class GuardianDruidRefactored : public TankSpecialization<RageResource>
{
public:
    using Base = TankSpecialization<RageResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::GetEnemiesInRange;
    using Base::_resource;
    explicit GuardianDruidRefactored(Player* bot)        : TankSpecialization<RageResource>(bot)
        
        , _ironfurTracker()
        , _thrashTracker()
        , _frenziedRegenerationActive(false)
        , _frenziedRegenerationEndTime(0)
        , _berserkActive(false)
        , _berserkEndTime(0)
        
        , _lastFrenziedRegenerationTime(0)
        , _cooldowns()
    {
        // Register cooldowns for major abilities
        _cooldowns.RegisterBatch({

            {GUARDIAN_INCARNATION, 180000, 1},

            {GUARDIAN_BERSERK, 180000, 1},

            {GUARDIAN_BARKSKIN, 60000, 1},

            {GUARDIAN_SURVIVAL_INSTINCTS, 180000, 1},

            {GUARDIAN_FRENZIED_REGEN, 36000, 1}
        });
        TC_LOG_DEBUG("playerbot", "GuardianDruidRefactored initialized for {}", bot->GetName());

        // Phase 5: Initialize decision systems
        InitializeGuardianMechanics();
    }

    void UpdateRotation(::Unit* target) override
    {
        Player* bot = this->GetBot();
        if (!target || !bot)

            return;

        UpdateGuardianState(target);
        MaintainBearForm();
        HandleActiveMitigation();

        uint32 enemyCount = this->GetEnemiesInRange(8.0f);

        if (enemyCount >= 3)

            ExecuteAoEThreatRotation(target, enemyCount);
        else

            ExecuteSingleTargetThreatRotation(target);
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();
        if (!bot)

            return;

        MaintainBearForm();
    }

    void UpdateDefensives()
    {
        Player* bot = this->GetBot();
        if (!bot)

            return;

        float healthPct = bot->GetHealthPct();        // Survival Instincts (critical emergency - 50% damage reduction)
        if (healthPct < 30.0f && this->CanCastSpell(GUARDIAN_SURVIVAL_INSTINCTS, bot))
        {

            this->CastSpell(bot, GUARDIAN_SURVIVAL_INSTINCTS);

            return;
        }

        // Frenzied Regeneration (strong self-heal)
        if (healthPct < 50.0f && this->_resource >= 10)
        {

            if (this->CanCastSpell(GUARDIAN_FRENZIED_REGENERATION, bot))

            {

                this->CastSpell(bot, GUARDIAN_FRENZIED_REGENERATION);

                _frenziedRegenerationActive = true;

                _frenziedRegenerationEndTime = GameTime::GetGameTimeMS() + 3000; // 3 sec heal over time

                _lastFrenziedRegenerationTime = GameTime::GetGameTimeMS();

                return;

            }
        }

        // Barkskin (moderate damage reduction - 20%)
        if (healthPct < 60.0f && this->CanCastSpell(GUARDIAN_BARKSKIN, bot))
        {

            this->CastSpell(bot, GUARDIAN_BARKSKIN);

            return;
        }

        // Renewal (instant 30% heal)
        if (healthPct < 50.0f && this->CanCastSpell(GUARDIAN_RENEWAL, bot))
        {

            this->CastSpell(bot, GUARDIAN_RENEWAL);

            return;
        }

        // Regrowth (if out of combat and low health)

        if (healthPct < 70.0f && !bot->IsInCombat() && this->CanCastSpell(GUARDIAN_REGROWTH, bot))
        {

            this->CastSpell(bot, GUARDIAN_REGROWTH);
        }
    }

    // Phase 5C: Threat management using ThreatAssistant service
    void ManageThreat(::Unit* target) override
    {
        if (!target)

            return;

        // Use ThreatAssistant to determine best taunt target and execute
        Unit* tauntTarget = bot::ai::ThreatAssistant::GetTauntTarget(this->GetBot());
        if (tauntTarget && this->CanCastSpell(GUARDIAN_GROWL, tauntTarget))
        {

            bot::ai::ThreatAssistant::ExecuteTaunt(this->GetBot(), tauntTarget, GUARDIAN_GROWL);

            _lastTaunt = GameTime::GetGameTimeMS();

            TC_LOG_DEBUG("playerbot", "Guardian: Growl taunt via ThreatAssistant on {}", tauntTarget->GetName());
        }
    }

private:
    

    void UpdateGuardianState(::Unit* target)
    {
        Player* bot = this->GetBot();
        _ironfurTracker.Update(bot);
        _thrashTracker.Update(target);
        UpdateCooldownStates();
    }

    void UpdateCooldownStates()
    {
        Player* bot = this->GetBot();
        // Frenzied Regeneration state
        if (_frenziedRegenerationActive && GameTime::GetGameTimeMS() >= _frenziedRegenerationEndTime)

            _frenziedRegenerationActive = false;

        if (bot->HasAura(GUARDIAN_FRENZIED_REGENERATION))
        {

            _frenziedRegenerationActive = true;

            if (Aura* aura = bot->GetAura(GUARDIAN_FRENZIED_REGENERATION))

                _frenziedRegenerationEndTime = GameTime::GetGameTimeMS() + aura->GetDuration();
                }

        // Berserk state
        if (_berserkActive && GameTime::GetGameTimeMS() >= _berserkEndTime)

            _berserkActive = false;

        if (bot->HasAura(GUARDIAN_BERSERK) || bot->HasAura(GUARDIAN_INCARNATION_BEAR))
        {

            _berserkActive = true;

            Aura* aura = bot->GetAura(GUARDIAN_BERSERK);

            if (!aura)

                aura = bot->GetAura(GUARDIAN_INCARNATION_BEAR);

            if (aura)

                _berserkEndTime = GameTime::GetGameTimeMS() + aura->GetDuration();
        }
    }

    void MaintainBearForm()
    {
        Player* bot = this->GetBot();        if (!bot->HasAura(GUARDIAN_BEAR_FORM))
        {

            if (this->CanCastSpell(GUARDIAN_BEAR_FORM, bot))

            {

                this->CastSpell(bot, GUARDIAN_BEAR_FORM);

            }
        }
    }

    void HandleActiveMitigation()
    {
        Player* bot = this->GetBot();
        float healthPct = bot->GetHealthPct();

        // Ironfur (primary active mitigation - increases armor)
        if (_ironfurTracker.NeedsRefresh() && this->_resource >= 45)
        {

            if (this->CanCastSpell(GUARDIAN_IRONFUR, bot))

            {

                this->CastSpell(bot, GUARDIAN_IRONFUR);

                _ironfurTracker.ApplyIronfur(7000); // 7 sec duration

                return;

            }
        }

        // Stack Ironfur when taking heavy damage
        if (healthPct < 80.0f && _ironfurTracker.GetStacks() < 3 && this->_resource >= 45)
        {

            if (this->CanCastSpell(GUARDIAN_IRONFUR, bot))

            {

                this->CastSpell(bot, GUARDIAN_IRONFUR);

                _ironfurTracker.ApplyIronfur(7000);

                return;

            }
        }
    }

    void ExecuteSingleTargetThreatRotation(::Unit* target)
    {
        Player* bot = this->GetBot();        ObjectGuid targetGuid = target->GetGUID();        // Berserk/Incarnation (major cooldown - increased damage and rage gen)
        if (this->_resource < 50 && CanUseMajorCooldown())
        {

            if (this->CanCastSpell(GUARDIAN_INCARNATION_BEAR, bot))

            {

                this->CastSpell(bot, GUARDIAN_INCARNATION_BEAR);

                _berserkActive = true;

                _berserkEndTime = GameTime::GetGameTimeMS() + 30000; // 30 sec

                _lastBerserkTime = GameTime::GetGameTimeMS();

                return;

            }

            else if (this->CanCastSpell(GUARDIAN_BERSERK, bot))

            {

                this->CastSpell(bot, GUARDIAN_BERSERK);

                _berserkActive = true;

                _berserkEndTime = GameTime::GetGameTimeMS() + 15000; // 15 sec

                _lastBerserkTime = GameTime::GetGameTimeMS();

                return;

            }
        }

        // Mangle (highest priority - generates rage and threat)
        if (this->CanCastSpell(GUARDIAN_MANGLE, target))
        {

            this->CastSpell(target, GUARDIAN_MANGLE);

            GenerateRage(8);

            return;
        }

        // Thrash (apply/maintain bleed)
        if (!_thrashTracker.HasThrash(targetGuid) || _thrashTracker.GetStacks(targetGuid) < 3)
        {

            if (this->CanCastSpell(GUARDIAN_THRASH, target))

            {

                this->CastSpell(target, GUARDIAN_THRASH);

                uint32 currentStacks = _thrashTracker.GetStacks(targetGuid);
                _thrashTracker.ApplyThrash(targetGuid, 15000, 1); // 15 sec, add 1 stack

                GenerateRage(5);

                return;

            }
        }

        // Pulverize (consume Thrash stacks for damage buff - if talented)
        if (bot->HasSpell(GUARDIAN_PULVERIZE) && _thrashTracker.GetStacks(targetGuid) >= 2)        {

            if (this->CanCastSpell(GUARDIAN_PULVERIZE, target))

            {

                this->CastSpell(target, GUARDIAN_PULVERIZE);

                _thrashTracker.RemoveThrash(targetGuid); // Consumes Thrash

                return;

            }
        }

        // Moonfire (ranged filler for pulling or when target is out of melee)
        if (bot->GetDistance(target) > 8.0f && bot->GetDistance(target) < 40.0f)
        {

            if (this->CanCastSpell(GUARDIAN_MOONFIRE, target))

            {

                this->CastSpell(target, GUARDIAN_MOONFIRE);

                GenerateRage(3);

                return;

            }
        }

        // Maul (rage dump when high rage)
        if (this->_resource > 80 && this->_resource >= 40)
        {

            if (this->CanCastSpell(GUARDIAN_MAUL, target))

            {

                this->CastSpell(target, GUARDIAN_MAUL);

                return;

            }
        }

        // Swipe (filler)
        if (this->CanCastSpell(GUARDIAN_SWIPE, target))
        {

            this->CastSpell(target, GUARDIAN_SWIPE);

            GenerateRage(4);

            return;
        }
    }

    void ExecuteAoEThreatRotation(::Unit* target, uint32 enemyCount)
    {
        Player* bot = this->GetBot();
        ObjectGuid targetGuid = target->GetGUID();        // Berserk for AoE threat burst
        if (this->_resource < 50 && CanUseMajorCooldown())
        {

            if (this->CanCastSpell(GUARDIAN_INCARNATION_BEAR, bot))

            {

                this->CastSpell(bot, GUARDIAN_INCARNATION_BEAR);

                _berserkActive = true;

                _berserkEndTime = GameTime::GetGameTimeMS() + 30000;

                _lastBerserkTime = GameTime::GetGameTimeMS();

                return;

            }

            else if (this->CanCastSpell(GUARDIAN_BERSERK, bot))

            {

                this->CastSpell(bot, GUARDIAN_BERSERK);

                _berserkActive = true;

                _berserkEndTime = GameTime::GetGameTimeMS() + 15000;

                _lastBerserkTime = GameTime::GetGameTimeMS();

                return;

            }
        }

        // Thrash (AoE bleed - highest priority in AoE)
        if (!_thrashTracker.HasThrash(targetGuid) || _thrashTracker.GetStacks(targetGuid) < 3)
        {

            if (this->CanCastSpell(GUARDIAN_THRASH, target))

            {

                this->CastSpell(target, GUARDIAN_THRASH);

                _thrashTracker.ApplyThrash(targetGuid, 15000, 1);

                GenerateRage(5);

                return;

            }
        }

        // Mangle (still use for rage generation)
        if (this->CanCastSpell(GUARDIAN_MANGLE, target))
        {

            this->CastSpell(target, GUARDIAN_MANGLE);

            GenerateRage(8);

            return;
        }

        // Swipe (AoE damage and threat)
        if (this->CanCastSpell(GUARDIAN_SWIPE, target))
        {

            this->CastSpell(target, GUARDIAN_SWIPE);

            GenerateRage(4);

            return;
        }

        // Maul (if high rage and need to dump)
        if (this->_resource > 80 && this->_resource >= 40)
        {

            if (this->CanCastSpell(GUARDIAN_MAUL, target))

            {

                this->CastSpell(target, GUARDIAN_MAUL);

                return;

            }
        }
    }

    void GenerateRage(uint32 amount)
    {
        // Rage is managed by base template class
        // No direct manipulation needed
    }

    void ConsumeRage(uint32 amount)
    {
        // Rage is managed by base template class
        // No direct manipulation needed
    }

    [[nodiscard]] bool CanUseMajorCooldown() const
    {
        // Only use major cooldowns in threatening situations or during burst windows
        Player* bot = this->GetBot();
        float healthPct = bot->GetHealthPct();
        return healthPct < 80.0f;
    }

    void InitializeGuardianMechanics()
    {
        using namespace bot::ai;
        using namespace bot::ai::BehaviorTreeBuilder;

        BotAI* ai = this->GetBot()->GetBotAI();
        if (!ai) return;

        auto* queue = ai->GetActionPriorityQueue();
        if (queue)
        {
            // EMERGENCY: Critical survival cooldowns

            queue->RegisterSpell(GUARDIAN_SURVIVAL_INSTINCTS, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);

            queue->AddCondition(GUARDIAN_SURVIVAL_INSTINCTS, [this](Player* bot, Unit*) {

                return bot && bot->GetHealthPct() < 25.0f;

            }, "HP < 25% (50% damage reduction, 6s)");


            queue->RegisterSpell(GUARDIAN_FRENZIED_REGENERATION, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);

            queue->AddCondition(GUARDIAN_FRENZIED_REGENERATION, [this](Player* bot, Unit*) {

                return bot && bot->GetHealthPct() < 50.0f && !this->_frenziedRegenerationActive;

            }, "HP < 50% (healing over time)");

            // CRITICAL: Active mitigation (Ironfur stacking)

            queue->RegisterSpell(GUARDIAN_IRONFUR, SpellPriority::CRITICAL, SpellCategory::DEFENSIVE);

            queue->AddCondition(GUARDIAN_IRONFUR, [this](Player* bot, Unit*) {

                return bot && this->_resource.GetAvailable() >= 40 &&

                       this->_ironfurTracker.GetStacks() < 3;

            }, "40 rage, < 3 stacks (armor buff)");

            // CRITICAL: Major burst cooldowns

            queue->RegisterSpell(GUARDIAN_INCARNATION_BEAR, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);

            queue->AddCondition(GUARDIAN_INCARNATION_BEAR, [this](Player* bot, Unit*) {

                return bot && bot->HasSpell(GUARDIAN_INCARNATION_BEAR) && this->CanUseMajorCooldown();

            }, "Major CD (30s burst, talent)");


            queue->RegisterSpell(GUARDIAN_BERSERK, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);

            queue->AddCondition(GUARDIAN_BERSERK, [this](Player*, Unit*) {

                return this->CanUseMajorCooldown();

            }, "Burst CD (15s, rage gen + damage)");

            // HIGH: Threat generation & rage spenders

            queue->RegisterSpell(GUARDIAN_MAUL, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);

            queue->AddCondition(GUARDIAN_MAUL, [this](Player*, Unit* target) {

                return target && this->_resource.GetAvailable() >= 40 &&

                       this->_ironfurTracker.GetStacks() >= 2;

            }, "40 rage, 2+ Ironfur stacks (threat + damage)");


            queue->RegisterSpell(GUARDIAN_MANGLE, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);

            queue->AddCondition(GUARDIAN_MANGLE, [](Player*, Unit* target) {

                return target != nullptr;

            }, "Rage generator + high threat");


            queue->RegisterSpell(GUARDIAN_THRASH, SpellPriority::HIGH, SpellCategory::DAMAGE_AOE);

            queue->AddCondition(GUARDIAN_THRASH, [this](Player*, Unit* target) {

                return target && (!this->_thrashTracker.HasThrash(target->GetGUID()) ||

                                  this->_thrashTracker.GetStacks(target->GetGUID()) < 3);

            }, "Apply/maintain Thrash (AoE bleed, 3 stacks)");

            // MEDIUM: Defensive cooldowns

            queue->RegisterSpell(GUARDIAN_BARKSKIN, SpellPriority::MEDIUM, SpellCategory::DEFENSIVE);

            queue->AddCondition(GUARDIAN_BARKSKIN, [this](Player* bot, Unit*) {

                return bot && bot->GetHealthPct() < 60.0f;

            }, "HP < 60% (damage reduction)");


            queue->RegisterSpell(GUARDIAN_RENEWAL, SpellPriority::MEDIUM, SpellCategory::DEFENSIVE);

            queue->AddCondition(GUARDIAN_RENEWAL, [this](Player* bot, Unit*) {

                return bot && bot->HasSpell(GUARDIAN_RENEWAL) && bot->GetHealthPct() < 65.0f;

            }, "HP < 65% (instant heal, talent)");

            // MEDIUM: Threat tools

            queue->RegisterSpell(GUARDIAN_PULVERIZE, SpellPriority::MEDIUM, SpellCategory::DAMAGE_SINGLE);

            queue->AddCondition(GUARDIAN_PULVERIZE, [this](Player* bot, Unit* target) {

                return bot && target && bot->HasSpell(GUARDIAN_PULVERIZE) &&

                       this->_thrashTracker.GetStacks(target->GetGUID()) >= 2;

            }, "2+ Thrash stacks (consume for damage buff, talent)");


            queue->RegisterSpell(GUARDIAN_RAGE_OF_SLEEPER, SpellPriority::MEDIUM, SpellCategory::OFFENSIVE);

            queue->AddCondition(GUARDIAN_RAGE_OF_SLEEPER, [this](Player* bot, Unit*) {

                return bot && bot->HasSpell(GUARDIAN_RAGE_OF_SLEEPER);

            }, "Damage reflect + Leech (talent)");

            // LOW: Filler & utility

            queue->RegisterSpell(GUARDIAN_SWIPE, SpellPriority::LOW, SpellCategory::DAMAGE_AOE);

            queue->AddCondition(GUARDIAN_SWIPE, [this](Player*, Unit* target) {

                return target && this->GetEnemiesInRange(8.0f) >= 2;

            }, "2+ enemies (AoE filler)");


            queue->RegisterSpell(GUARDIAN_MOONFIRE, SpellPriority::LOW, SpellCategory::DAMAGE_SINGLE);

            queue->AddCondition(GUARDIAN_MOONFIRE, [this](Player* bot, Unit* target) {

                return target && bot && bot->GetDistance(target) > 8.0f && bot->GetDistance(target) < 40.0f;

            }, "Out of melee range (ranged filler)");
        }

        auto* behaviorTree = ai->GetBehaviorTree();
        if (behaviorTree)
        {

            auto root = Selector("Guardian Druid Tank", {
                // Tier 1: Emergency Survival (HP < 25-50%)

                Sequence("Emergency Survival", {

                    Condition("Critical health", [this](Player* bot) {

                        return bot && bot->GetHealthPct() < 50.0f;

                    }),

                    Selector("Use emergency cooldowns", {

                        Sequence("Survival Instincts", {

                            Condition("HP < 25%", [this](Player* bot) {

                                return bot->GetHealthPct() < 25.0f;

                            }),

                            Action("Cast Survival Instincts", [this](Player* bot) {

                                if (this->CanCastSpell(GUARDIAN_SURVIVAL_INSTINCTS, bot))

                                {

                                    this->CastSpell(bot, GUARDIAN_SURVIVAL_INSTINCTS);

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),

                        Sequence("Frenzied Regeneration", {

                            Condition("HP < 50% and not active", [this](Player* bot) {

                                return bot->GetHealthPct() < 50.0f && !this->_frenziedRegenerationActive;

                            }),

                            Action("Cast Frenzied Regeneration", [this](Player* bot) {

                                if (this->CanCastSpell(GUARDIAN_FRENZIED_REGENERATION, bot))

                                {

                                    this->CastSpell(bot, GUARDIAN_FRENZIED_REGENERATION);

                                    this->_frenziedRegenerationActive = true;

                                    this->_frenziedRegenerationEndTime = GameTime::GetGameTimeMS() + 3000;

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),

                        Sequence("Barkskin", {

                            Condition("HP < 60%", [this](Player* bot) {

                                return bot->GetHealthPct() < 60.0f;

                            }),

                            Action("Cast Barkskin", [this](Player* bot) {

                                if (this->CanCastSpell(GUARDIAN_BARKSKIN, bot))

                                {

                                    this->CastSpell(bot, GUARDIAN_BARKSKIN);

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        })

                    })

                }),

                // Tier 2: Active Mitigation (Ironfur stacking)

                Sequence("Active Mitigation", {

                    Condition("40+ rage and < 3 Ironfur stacks", [this](Player*) {

                        return this->_resource.GetAvailable() >= 40 && this->_ironfurTracker.GetStacks() < 3;

                    }),

                    Action("Cast Ironfur", [this](Player* bot) {

                        if (this->CanCastSpell(GUARDIAN_IRONFUR, bot))

                        {

                            this->CastSpell(bot, GUARDIAN_IRONFUR);

                            this->_ironfurTracker.AddStack();

                            return NodeStatus::SUCCESS;

                        }

                        return NodeStatus::FAILURE;

                    })

                }),

                // Tier 3: Threat Generation (Mangle, Thrash, Maul)

                Sequence("Threat Generation", {

                    Condition("Has target", [this](Player* bot) {

                        return bot && bot->GetVictim();

                    }),

                    Selector("Generate threat", {

                        Sequence("Berserk/Incarnation (burst)", {

                            Condition("Can use major cooldown", [this](Player*) {

                                return this->CanUseMajorCooldown();

                            }),

                            Selector("Use burst", {

                                Sequence("Incarnation (talent)", {

                                    Condition("Has Incarnation", [this](Player* bot) {

                                        return bot->HasSpell(GUARDIAN_INCARNATION_BEAR);

                                    }),

                                    Action("Cast Incarnation", [this](Player* bot) {

                                        if (this->CanCastSpell(GUARDIAN_INCARNATION_BEAR, bot))

                                        {

                                            this->CastSpell(bot, GUARDIAN_INCARNATION_BEAR);

                                            this->_berserkActive = true;

                                            this->_berserkEndTime = GameTime::GetGameTimeMS() + 30000;

                                            return NodeStatus::SUCCESS;

                                        }

                                        return NodeStatus::FAILURE;

                                    })

                                }),

                                Sequence("Berserk", {

                                    Action("Cast Berserk", [this](Player* bot) {

                                        if (this->CanCastSpell(GUARDIAN_BERSERK, bot))

                                        {

                                            this->CastSpell(bot, GUARDIAN_BERSERK);

                                            this->_berserkActive = true;

                                            this->_berserkEndTime = GameTime::GetGameTimeMS() + 15000;

                                            return NodeStatus::SUCCESS;

                                        }

                                        return NodeStatus::FAILURE;

                                    })

                                })

                            })

                        }),

                        Sequence("Mangle (priority builder)", {

                            Action("Cast Mangle", [this](Player* bot) {

                                Unit* target = bot ? bot->GetVictim() : nullptr;

                                if (target && this->CanCastSpell(GUARDIAN_MANGLE, target))

                                {

                                    this->CastSpell(target, GUARDIAN_MANGLE);

                                    this->GenerateRage(8);

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),

                        Sequence("Thrash (AoE bleed)", {

                            Condition("< 3 Thrash stacks", [this](Player* bot) {

                                Unit* target = bot ? bot->GetVictim() : nullptr;

                                return target && (!this->_thrashTracker.HasThrash(target->GetGUID()) ||

                                                  this->_thrashTracker.GetStacks(target->GetGUID()) < 3);

                            }),

                            Action("Cast Thrash", [this](Player* bot) {

                                Unit* target = bot ? bot->GetVictim() : nullptr;

                                if (target && this->CanCastSpell(GUARDIAN_THRASH, target))

                                {

                                    this->CastSpell(target, GUARDIAN_THRASH);

                                    uint32 currentStacks = this->_thrashTracker.GetStacks(target->GetGUID());

                                    this->_thrashTracker.ApplyThrash(target->GetGUID(), 15000, 1);

                                    this->GenerateRage(5);

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),

                        Sequence("Maul (rage dump)", {

                            Condition("40+ rage and 2+ Ironfur stacks", [this](Player*) {

                                return this->_resource.GetAvailable() >= 40 && this->_ironfurTracker.GetStacks() >= 2;

                            }),

                            Action("Cast Maul", [this](Player* bot) {

                                Unit* target = bot ? bot->GetVictim() : nullptr;

                                if (target && this->CanCastSpell(GUARDIAN_MAUL, target))

                                {

                                    this->CastSpell(target, GUARDIAN_MAUL);

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),

                        Sequence("Pulverize (talent)", {

                            Condition("Has talent and 2+ Thrash stacks", [this](Player* bot) {

                                Unit* target = bot ? bot->GetVictim() : nullptr;

                                return bot && target && bot->HasSpell(GUARDIAN_PULVERIZE) &&

                                       this->_thrashTracker.GetStacks(target->GetGUID()) >= 2;

                            }),

                            Action("Cast Pulverize", [this](Player* bot) {

                                Unit* target = bot ? bot->GetVictim() : nullptr;

                                if (target && this->CanCastSpell(GUARDIAN_PULVERIZE, target))

                                {

                                    this->CastSpell(target, GUARDIAN_PULVERIZE);

                                    this->_thrashTracker.RemoveThrash(target->GetGUID());

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        })

                    })

                }),

                // Tier 4: Filler (Swipe, Moonfire)

                Sequence("Filler", {

                    Condition("Has target", [this](Player* bot) {

                        return bot && bot->GetVictim();

                    }),

                    Selector("Use filler", {

                        Sequence("Swipe (AoE)", {

                            Condition("2+ enemies", [this](Player*) {

                                return this->GetEnemiesInRange(8.0f) >= 2;

                            }),

                            Action("Cast Swipe", [this](Player* bot) {

                                Unit* target = bot ? bot->GetVictim() : nullptr;

                                if (target && this->CanCastSpell(GUARDIAN_SWIPE, target))

                                {

                                    this->CastSpell(target, GUARDIAN_SWIPE);

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),

                        Sequence("Moonfire (ranged)", {

                            Condition("Out of melee range", [this](Player* bot) {

                                Unit* target = bot ? bot->GetVictim() : nullptr;

                                return bot && target && bot->GetDistance(target) > 8.0f && bot->GetDistance(target) < 40.0f;

                            }),

                            Action("Cast Moonfire", [this](Player* bot) {

                                Unit* target = bot ? bot->GetVictim() : nullptr;

                                if (target && this->CanCastSpell(GUARDIAN_MOONFIRE, target))

                                {

                                    this->CastSpell(target, GUARDIAN_MOONFIRE);

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

    // Member variables
    GuardianIronfurTracker _ironfurTracker;
    GuardianThrashTracker _thrashTracker;

    bool _frenziedRegenerationActive;
    uint32 _frenziedRegenerationEndTime;
    bool _berserkActive;
    uint32 _berserkEndTime;
    uint32 _lastFrenziedRegenerationTime;
    uint32 _lastTaunt{0}; // Phase 5C: ThreatAssistant integration
};

} // namespace Playerbot

#endif // PLAYERBOT_GUARDIANDRUIDREFACTORED_H
