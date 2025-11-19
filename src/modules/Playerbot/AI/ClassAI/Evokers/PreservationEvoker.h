/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * Preservation Evoker Refactored - Enterprise-Grade Header-Only Implementation
 *
 * This file provides a complete, template-based implementation of Preservation Evoker
 * using the HealerSpecialization with Essence resource system and Echo mechanics.
 */

#pragma once

#include "../Common/StatusEffectTracker.h"
#include "../Common/CooldownManager.h"
#include "../Common/RotationHelpers.h"
#include "../CombatSpecializationTemplates.h"
#include "../ResourceTypes.h"
#include "../../Services/HealingTargetSelector.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "Group.h"
#include "Log.h"
#include "../Decision/ActionPriorityQueue.h"
#include "../Decision/BehaviorTree.h"
#include "../BotAI.h"

namespace Playerbot
{

// ============================================================================
// PRESERVATION EVOKER SPELL IDs (WoW 11.2 - The War Within)
// ============================================================================

enum PreservationEvokerSpells
{
    // Direct Heals
    EMERALD_BLOSSOM      = 355916,  // 3 essence, AoE heal
    VERDANT_EMBRACE      = 360995,  // 1 essence, single-target heal + teleport
    LIVING_FLAME_HEAL    = 361509,  // Heal version of Living Flame

    // Empowered Heals
    DREAM_BREATH         = 355936,  // 3 essence, empowered (rank 1-4), HoT
    SPIRIT_BLOOM         = 367226,  // 3 essence, empowered (rank 1-4), smart heal

    // Echo System
    ECHO                 = 364343,  // Creates healing echo on target
    REVERSION            = 366155,  // 1 essence, HoT with Echo

    // Major Cooldowns
    EMERALD_COMMUNION    = 370960,  // 3 min CD, massive AoE heal
    TEMPORAL_ANOMALY     = 373861,  // 3 min CD, heal after damage taken
    REWIND               = 363534,  // 2.5 min CD, undo damage

    // Utility
    LIFEBIND             = 373267,  // Link two allies, share healing
    BLESSING_BRONZE      = 364342,  // CDR on ally
    TIME_DILATION        = 357170,  // Extend HoTs/buffs
    STASIS               = 370537,  // Suspend friendly target
    RESCUE               = 370665,  // Pull ally to you

    // Defensive
    OBSIDIAN_SCALES      = 363916,  // 90 sec CD, damage reduction
    RENEWING_BLAZE       = 374348,  // 90 sec CD, self-heal
    TWIN_GUARDIAN        = 370888,  // Shield another player

    // Essence Generation
    AZURE_STRIKE_PRES    = 362969,  // Generates 2 essence
    DISINTEGRATE_PRES    = 356995,  // 3 essence, damage for essence gen

    // Procs
    ESSENCE_BURST_PRES   = 369299,  // Free essence spender
    CALL_OF_YSERA        = 373835,  // Dream Breath proc

    // Talents
    FIELD_OF_DREAMS      = 370062,  // Dream Breath AoE larger
    FLOW_STATE           = 385696,  // Essence regen
    LIFEFORCE_MENDER     = 376179,  // Healing increase
    TEMPORAL_COMPRESSION = 362877   // Echo burst heal
};

// Essence resource (same as Devastation)
struct EssenceResourcePres
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

    void Regenerate(uint32 diff) { available = true; }
    [[nodiscard]] uint32 GetAvailable() const { return essence; }
    [[nodiscard]] uint32 GetMax() const { return maxEssence; }

    void Initialize(Player* bot) {
        if (bot) {
            essence = 0;
            maxEssence = 5;
        }
    }
};

// ============================================================================
// EMPOWERMENT TRACKING
// ============================================================================

enum class EmpowerLevelPres : uint8
{
    NONE = 0,
    RANK_1 = 1,
    RANK_2 = 2,
    RANK_3 = 3,
    RANK_4 = 4
};

class PreservationEmpowermentTracker
{
public:
    PreservationEmpowermentTracker()
        : _isChanneling(false), _currentSpellId(0), _targetLevel(EmpowerLevelPres::NONE), _channelStartTime(0)
    {}

    void StartEmpower(uint32 spellId, EmpowerLevelPres targetLevel)
    {
        _isChanneling = true;
        _currentSpellId = spellId;
        _targetLevel = targetLevel;
        _channelStartTime = GameTime::GetGameTimeMS();
    }

    void StopEmpower() { _isChanneling = false; _currentSpellId = 0; }
    bool IsChanneling() const { return _isChanneling; }
    uint32 GetSpellId() const { return _currentSpellId; }

    bool ShouldRelease() const
    {
        if (!_isChanneling) return false;
        uint32 requiredTime = static_cast<uint32>(_targetLevel) * 750;
        uint32 channelTime = GameTime::GetGameTimeMS() - _channelStartTime;
        return channelTime >= requiredTime;
    }

private:
    bool _isChanneling;
    uint32 _currentSpellId;
    EmpowerLevelPres _targetLevel;
    uint32 _channelStartTime;
};

// ============================================================================
// ECHO SYSTEM
// ============================================================================

struct Echo
{
    ObjectGuid targetGuid;
    uint32 remainingHeals;
    uint32 healAmount;
    uint32 lastHealTime;
    uint32 healInterval;

    Echo() : remainingHeals(0), healAmount(0), lastHealTime(0), healInterval(2000) {}

    Echo(ObjectGuid guid, uint32 heals, uint32 amount)
        : targetGuid(guid), remainingHeals(heals), healAmount(amount), lastHealTime(GameTime::GetGameTimeMS()), healInterval(2000) {}

    bool ShouldHeal() const {
        return GameTime::GetGameTimeMS() - lastHealTime >= healInterval && remainingHeals > 0;
    }

    void ProcessHeal() {
        if (remainingHeals > 0) {
            remainingHeals--;
            lastHealTime = GameTime::GetGameTimeMS();
        }
    }

    bool IsExpired() const {
        return remainingHeals == 0;
    }
};

class EchoTracker
{
public:
    EchoTracker() : _maxEchoes(8) {}

    void CreateEcho(ObjectGuid targetGuid, uint32 healAmount, uint32 numHeals = 4)
    {
        // Remove existing echo on same target
        RemoveEcho(targetGuid);

        // Add new echo
        if (_echoes.size() < _maxEchoes)
        {
            _echoes.emplace_back(targetGuid, numHeals, healAmount);
        }
    }

    void RemoveEcho(ObjectGuid targetGuid)
    {
        _echoes.erase(
            std::remove_if(_echoes.begin(), _echoes.end(),
                [targetGuid](const Echo& echo) { return echo.targetGuid == targetGuid; }),
            _echoes.end()
        );
    }

    void Update(Player* bot)
    {
        if (!bot) return;

        // Process echo healing
        for (auto& echo : _echoes)
        {
            if (echo.ShouldHeal())
            {
                Unit* target = ObjectAccessor::GetUnit(*bot, echo.targetGuid);
                if (target && target->IsAlive())
                {
                    // Heal target via echo
                    echo.ProcessHeal();
                }
            }
        }

        // Remove expired echoes
        _echoes.erase(
            std::remove_if(_echoes.begin(), _echoes.end(),
                [](const Echo& echo) { return echo.IsExpired(); }),
            _echoes.end()
        );
    }

    [[nodiscard]] uint32 GetActiveEchoCount() const { return _echoes.size(); }

    [[nodiscard]] bool HasEcho(ObjectGuid targetGuid) const
    {
        return std::any_of(_echoes.begin(), _echoes.end(),
            [targetGuid](const Echo& echo) { return echo.targetGuid == targetGuid; });
    }

private:
    std::vector<Echo> _echoes;
    uint32 _maxEchoes;
};

// ============================================================================
// PRESERVATION EVOKER REFACTORED
// ============================================================================

class PreservationEvokerRefactored : public HealerSpecialization<EssenceResourcePres>
{
public:
    using Base = HealerSpecialization<EssenceResourcePres>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;

    explicit PreservationEvokerRefactored(Player* bot)
        : HealerSpecialization<EssenceResourcePres>(bot)
        , _empowermentTracker()
        , _echoTracker()
        , _essenceBurstStacks(0)
    {
        // Initialize essence resources
        this->_resource.Initialize(bot);

        // Phase 5: Initialize decision systems
        InitializePreservationMechanics();

        TC_LOG_DEBUG("playerbot", "PreservationEvokerRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        // Preservation focuses on healing, not DPS rotation
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Update Preservation state
        UpdatePreservationState();

        // Handle empowerment channeling
        if (_empowermentTracker.IsChanneling())
        {
            if (_empowermentTracker.ShouldRelease())
            {
                ReleaseEmpoweredSpell();
            }
            return;
        }

        // Get group members for healing
        std::vector<Unit*> group = GetGroupMembers();
        if (group.empty())
            return;

        // Execute healing rotation
        ExecuteHealingRotation(group);
    }

    float GetOptimalRange(::Unit* target) override
    {
        return 30.0f; // Ranged healer at 30 yards
    }

protected:
    void ExecuteHealingRotation(const std::vector<Unit*>& group)
    {
        uint32 essence = this->_resource.essence;

        // Priority 1: Emergency healing
        if (HandleEmergencyHealing(group))
            return;

        // Priority 2: Maintain Echoes
        if (HandleEchoMaintenance(group))
            return;

        // Priority 3: HoT maintenance
        if (essence >= 3 && HandleHoTMaintenance(group))
            return;

        // Priority 4: Direct healing
        if (essence >= 3 && HandleDirectHealing(group))
            return;

        // Priority 5: Generate essence if low
        if (essence < 3)
            GenerateEssence();
    }

    bool HandleEmergencyHealing(const std::vector<Unit*>& group)
    {
        uint32 criticalCount = 0;
        for (Unit* member : group)
            if (member && member->GetHealthPct() < 40.0f)
                criticalCount++;

        // Emerald Communion for 3+ critical
        if (criticalCount >= 3 && this->CanCastSpell(EMERALD_COMMUNION, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), EMERALD_COMMUNION);
            return true;
        }

        // Spirit Bloom for single critical target
        if (criticalCount >= 1 && this->_resource.essence >= 3)
        {
            Unit* target = GetLowestHealthTarget(group);
            if (target && this->CanCastSpell(SPIRIT_BLOOM, target))
            {
                StartEmpoweredSpell(SPIRIT_BLOOM, EmpowerLevelPres::RANK_3, target);
                return true;
            }
        }

        return false;
    }

    bool HandleEchoMaintenance(const std::vector<Unit*>& group)
    {
        // Maintain 3-4 echoes on injured allies
        uint32 activeEchoes = _echoTracker.GetActiveEchoCount();
        if (activeEchoes >= 4)
            return false;

        // Find target without echo and below 95% HP
        for (Unit* member : group)
        {
            if (member && member->GetHealthPct() < 95.0f && !_echoTracker.HasEcho(member->GetGUID()))
            {
                // Cast Reversion to create echo
                if (this->_resource.essence >= 1 && this->CanCastSpell(REVERSION, member))
                {
                    this->CastSpell(member, REVERSION);
                    this->_resource.Consume(1);
                    _echoTracker.CreateEcho(member->GetGUID(), 5000, 4); // 4 heals
                    return true;
                }
            }
        }

        return false;
    }

    bool HandleHoTMaintenance(const std::vector<Unit*>& group)
    {
        // Dream Breath for group-wide HoT
        uint32 injuredCount = 0;
        for (Unit* member : group)
            if (member && member->GetHealthPct() < 85.0f)
                injuredCount++;

        if (injuredCount >= 3)
        {
            Unit* target = GetMostInjuredTarget(group);
            if (target && this->CanCastSpell(DREAM_BREATH, target))
            {
                StartEmpoweredSpell(DREAM_BREATH, EmpowerLevelPres::RANK_2, target);
                return true;
            }
        }

        return false;
    }

    bool HandleDirectHealing(const std::vector<Unit*>& group)
    {
        // Emerald Blossom for AoE healing
        uint32 injuredCount = 0;
        for (Unit* member : group)
            if (member && member->GetHealthPct() < 80.0f)
                injuredCount++;

        if (injuredCount >= 3 && this->CanCastSpell(EMERALD_BLOSSOM, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), EMERALD_BLOSSOM);
            this->_resource.Consume(3);
            return true;
        }

        // Verdant Embrace for single target
        Unit* target = GetLowestHealthTarget(group);
        if (target && target->GetHealthPct() < 70.0f && this->CanCastSpell(VERDANT_EMBRACE, target))
        {
            this->CastSpell(target, VERDANT_EMBRACE);
            this->_resource.Consume(1);
            return true;
        }

        return false;
    }

    void GenerateEssence()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        Unit* target = bot->GetVictim();
        if (!target)
            target = FindNearbyEnemy();

        if (target && this->CanCastSpell(AZURE_STRIKE_PRES, target))
        {
            this->CastSpell(target, AZURE_STRIKE_PRES);
            this->_resource.essence = std::min(this->_resource.essence + 2, this->_resource.maxEssence);
        }
    }

    Unit* GetLowestHealthTarget(const std::vector<Unit*>& group) const
    {
        return bot::ai::HealingTargetSelector::SelectTarget(this->GetBot(), 30.0f, 100.0f);
    }

    Unit* GetMostInjuredTarget(const std::vector<Unit*>& group) const
    {
        Unit* mostInjured = nullptr;
        float lowestPct = 100.0f;

        for (Unit* member : group)
        {
            if (member && member->GetHealthPct() < lowestPct)
            {
                lowestPct = member->GetHealthPct();
                mostInjured = member;
            }
        }

        return mostInjured;
    }

    [[nodiscard]] std::vector<Unit*> GetGroupMembers() const
    {
        std::vector<Unit*> members;
        Player* bot = this->GetBot();
        if (!bot) return members;

        Group* group = bot->GetGroup();
        if (!group) return members;

        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                if (member->IsAlive() && bot->IsInMap(member))
                    members.push_back(member);
            }
        }
        return members;
    }

    Unit* FindNearbyEnemy() const
    {
        Player* bot = this->GetBot();
        if (!bot) return nullptr;

        // Find nearest enemy within 30 yards
        Unit* nearestEnemy = nullptr;
        float nearestDist = 30.0f;

        bot->GetMap()->DoForAllPlayers([&](Player* player) {
            if (player && player->IsHostileTo(bot) && player->IsAlive())
            {
                float dist = bot->GetDistance(player);
                if (dist < nearestDist)
                {
                    nearestDist = dist;
                    nearestEnemy = player;
                }
            }
        });

        return nearestEnemy;
    }

    void UpdatePreservationState()
    {
        Player* bot = this->GetBot();
        if (!bot) return;

        // Update echo system
        _echoTracker.Update(bot);

        // Sync essence with actual resource
        this->_resource.essence = bot->GetPower(POWER_ALTERNATE_POWER);

        // Update Essence Burst stacks
        if (Aura* aura = bot->GetAura(ESSENCE_BURST_PRES))
            _essenceBurstStacks = aura->GetStackAmount();
        else
            _essenceBurstStacks = 0;
    }

    void StartEmpoweredSpell(uint32 spellId, EmpowerLevelPres targetLevel, Unit* target)
    {
        _empowermentTracker.StartEmpower(spellId, targetLevel);
        this->CastSpell(target, spellId);
    }

    void ReleaseEmpoweredSpell()
    {
        Player* bot = this->GetBot();
        if (bot && bot->IsNonMeleeSpellCast(false))
        {
            bot->InterruptNonMeleeSpells(false);
        }

        this->_resource.Consume(3);
        _empowermentTracker.StopEmpower();
    }

    // ========================================================================
    // PHASE 5: DECISION SYSTEM INTEGRATION
    // ========================================================================

    void InitializePreservationMechanics()
    {
        using namespace bot::ai;
        using namespace bot::ai::BehaviorTreeBuilder;

        BotAI* ai = this->GetBot()->GetBotAI();
        if (!ai) return;

        auto* queue = ai->GetActionPriorityQueue();
        if (queue)
        {
            // EMERGENCY: Raid-wide emergency healing
            queue->RegisterSpell(EMERALD_COMMUNION, SpellPriority::EMERGENCY, SpellCategory::HEALING);
            queue->AddCondition(EMERALD_COMMUNION, [this](Player*, Unit*) {
                auto group = this->GetGroupMembers();
                uint32 critical = 0;
                for (auto* m : group) if (m && m->GetHealthPct() < 40.0f) critical++;
                return critical >= 3;
            }, "3+ allies < 40% HP (massive heal, 3min CD)");

            queue->RegisterSpell(REWIND, SpellPriority::EMERGENCY, SpellCategory::HEALING);
            queue->AddCondition(REWIND, [this](Player*, Unit*) {
                auto group = this->GetGroupMembers();
                for (auto* m : group) if (m && m->GetHealthPct() < 30.0f) return true;
                return false;
            }, "Ally < 30% HP (undo damage, 2.5min CD)");

            // CRITICAL: Empowered heals
            queue->RegisterSpell(SPIRIT_BLOOM, SpellPriority::CRITICAL, SpellCategory::HEALING);
            queue->AddCondition(SPIRIT_BLOOM, [this](Player*, Unit*) {
                if (this->_resource.essence < 3 || this->_empowermentTracker.IsChanneling()) return false;
                auto group = this->GetGroupMembers();
                for (auto* m : group) if (m && m->GetHealthPct() < 50.0f) return true;
                return false;
            }, "Ally < 50% HP, 3 essence (empowered smart heal)");

            queue->RegisterSpell(DREAM_BREATH, SpellPriority::CRITICAL, SpellCategory::HEALING);
            queue->AddCondition(DREAM_BREATH, [this](Player*, Unit*) {
                if (this->_resource.essence < 3 || this->_empowermentTracker.IsChanneling()) return false;
                auto group = this->GetGroupMembers();
                uint32 injured = 0;
                for (auto* m : group) if (m && m->GetHealthPct() < 85.0f) injured++;
                return injured >= 3;
            }, "3+ allies < 85% HP, 3 essence (empowered HoT)");

            // HIGH: Direct heals and Echo maintenance
            queue->RegisterSpell(EMERALD_BLOSSOM, SpellPriority::HIGH, SpellCategory::HEALING);
            queue->AddCondition(EMERALD_BLOSSOM, [this](Player*, Unit*) {
                if (this->_resource.essence < 3) return false;
                auto group = this->GetGroupMembers();
                uint32 injured = 0;
                for (auto* m : group) if (m && m->GetHealthPct() < 80.0f) injured++;
                return injured >= 3;
            }, "3+ allies < 80% HP, 3 essence (AoE heal)");

            queue->RegisterSpell(REVERSION, SpellPriority::HIGH, SpellCategory::HEALING);
            queue->AddCondition(REVERSION, [this](Player*, Unit*) {
                if (this->_resource.essence < 1) return false;
                auto group = this->GetGroupMembers();
                for (auto* m : group) {
                    if (m && m->GetHealthPct() < 95.0f && !this->_echoTracker.HasEcho(m->GetGUID()))
                        return true;
                }
                return false;
            }, "Ally < 95% without Echo, 1 essence (HoT + Echo)");

            queue->RegisterSpell(VERDANT_EMBRACE, SpellPriority::HIGH, SpellCategory::HEALING);
            queue->AddCondition(VERDANT_EMBRACE, [this](Player*, Unit*) {
                if (this->_resource.essence < 1) return false;
                auto group = this->GetGroupMembers();
                for (auto* m : group) if (m && m->GetHealthPct() < 70.0f) return true;
                return false;
            }, "Ally < 70% HP, 1 essence (heal + teleport)");

            // MEDIUM: Utility and support
            queue->RegisterSpell(TEMPORAL_ANOMALY, SpellPriority::MEDIUM, SpellCategory::DEFENSIVE);
            queue->AddCondition(TEMPORAL_ANOMALY, [this](Player*, Unit*) {
                auto group = this->GetGroupMembers();
                uint32 injured = 0;
                for (auto* m : group) if (m && m->GetHealthPct() < 70.0f) injured++;
                return injured >= 2;
            }, "2+ allies < 70% HP (heal after dmg, 3min CD)");

            queue->RegisterSpell(TIME_DILATION, SpellPriority::MEDIUM, SpellCategory::UTILITY);
            queue->AddCondition(TIME_DILATION, [this](Player*, Unit*) {
                return this->_echoTracker.GetActiveEchoCount() >= 3;
            }, "3+ active Echoes (extend HoTs)");

            // LOW: Essence generation
            queue->RegisterSpell(AZURE_STRIKE_PRES, SpellPriority::LOW, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(AZURE_STRIKE_PRES, [this](Player*, Unit* target) {
                return target && this->_resource.essence < 4;
            }, "Essence < 4 (generates 2 essence)");

            // DEFENSIVE: Personal survival
            queue->RegisterSpell(OBSIDIAN_SCALES, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);
            queue->AddCondition(OBSIDIAN_SCALES, [](Player* bot, Unit*) {
                return bot && bot->GetHealthPct() < 40.0f;
            }, "HP < 40% (30% dmg reduction)");

            queue->RegisterSpell(RENEWING_BLAZE, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);
            queue->AddCondition(RENEWING_BLAZE, [](Player* bot, Unit*) {
                return bot && bot->GetHealthPct() < 50.0f;
            }, "HP < 50% (self-heal)");
        }

        auto* behaviorTree = ai->GetBehaviorTree();
        if (behaviorTree)
        {
            auto root = Selector("Preservation Evoker Healing", {
                // Tier 1: Emergency Healing
                Sequence("Emergency Healing", {
                    Condition("3+ critical", [this](Player*) {
                        auto group = this->GetGroupMembers();
                        uint32 critical = 0;
                        for (auto* m : group) if (m && m->GetHealthPct() < 40.0f) critical++;
                        return critical >= 3;
                    }),
                    Action("Cast Emerald Communion", [this](Player* bot) {
                        if (this->CanCastSpell(EMERALD_COMMUNION, bot)) {
                            this->CastSpell(bot, EMERALD_COMMUNION);
                            return NodeStatus::SUCCESS;
                        }
                        return NodeStatus::FAILURE;
                    })
                }),

                // Tier 2: Empowered Heals
                Sequence("Empowered Heals", {
                    Condition("3+ essence", [this](Player*) {
                        return this->_resource.essence >= 3;
                    }),
                    Condition("Not channeling", [this](Player*) {
                        return !this->_empowermentTracker.IsChanneling();
                    }),
                    Selector("Cast empowered", {
                        Sequence("Spirit Bloom Critical", {
                            Condition("Ally < 50%", [this](Player*) {
                                auto group = this->GetGroupMembers();
                                for (auto* m : group) if (m && m->GetHealthPct() < 50.0f) return true;
                                return false;
                            }),
                            Action("Cast Spirit Bloom", [this](Player*) {
                                auto group = this->GetGroupMembers();
                                Unit* target = this->GetLowestHealthTarget(group);
                                if (target && this->CanCastSpell(SPIRIT_BLOOM, target)) {
                                    this->StartEmpoweredSpell(SPIRIT_BLOOM, EmpowerLevelPres::RANK_3, target);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Dream Breath AoE", {
                            Condition("3+ injured", [this](Player*) {
                                auto group = this->GetGroupMembers();
                                uint32 injured = 0;
                                for (auto* m : group) if (m && m->GetHealthPct() < 85.0f) injured++;
                                return injured >= 3;
                            }),
                            Action("Cast Dream Breath", [this](Player*) {
                                auto group = this->GetGroupMembers();
                                Unit* target = this->GetMostInjuredTarget(group);
                                if (target && this->CanCastSpell(DREAM_BREATH, target)) {
                                    this->StartEmpoweredSpell(DREAM_BREATH, EmpowerLevelPres::RANK_2, target);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 3: Echo Maintenance
                Sequence("Echo Maintenance", {
                    Condition("< 4 echoes", [this](Player*) {
                        return this->_echoTracker.GetActiveEchoCount() < 4;
                    }),
                    Condition("Has essence", [this](Player*) {
                        return this->_resource.essence >= 1;
                    }),
                    Action("Cast Reversion", [this](Player*) {
                        auto group = this->GetGroupMembers();
                        for (Unit* member : group) {
                            if (member && member->GetHealthPct() < 95.0f && !this->_echoTracker.HasEcho(member->GetGUID())) {
                                if (this->CanCastSpell(REVERSION, member)) {
                                    this->CastSpell(member, REVERSION);
                                    this->_resource.Consume(1);
                                    this->_echoTracker.CreateEcho(member->GetGUID(), 5000, 4);
                                    return NodeStatus::SUCCESS;
                                }
                            }
                        }
                        return NodeStatus::FAILURE;
                    })
                }),

                // Tier 4: Direct Healing
                Sequence("Direct Healing", {
                    Condition("Has essence", [this](Player*) {
                        return this->_resource.essence >= 1;
                    }),
                    Selector("Cast heals", {
                        Sequence("Emerald Blossom AoE", {
                            Condition("3+ injured", [this](Player*) {
                                if (this->_resource.essence < 3) return false;
                                auto group = this->GetGroupMembers();
                                uint32 injured = 0;
                                for (auto* m : group) if (m && m->GetHealthPct() < 80.0f) injured++;
                                return injured >= 3;
                            }),
                            Action("Cast Emerald Blossom", [this](Player* bot) {
                                if (this->CanCastSpell(EMERALD_BLOSSOM, bot)) {
                                    this->CastSpell(bot, EMERALD_BLOSSOM);
                                    this->_resource.Consume(3);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Verdant Embrace", {
                            Action("Cast Verdant Embrace", [this](Player*) {
                                Unit* target = this->GetLowestHealthTarget(this->GetGroupMembers());
                                if (target && target->GetHealthPct() < 70.0f && this->CanCastSpell(VERDANT_EMBRACE, target)) {
                                    this->CastSpell(target, VERDANT_EMBRACE);
                                    this->_resource.Consume(1);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 5: Generate Essence
                Sequence("Generate Essence", {
                    Condition("< 3 essence", [this](Player*) {
                        return this->_resource.essence < 3;
                    }),
                    Action("Cast Azure Strike", [this](Player*) {
                        Unit* target = this->FindNearbyEnemy();
                        if (target && this->CanCastSpell(AZURE_STRIKE_PRES, target)) {
                            this->CastSpell(target, AZURE_STRIKE_PRES);
                            this->_resource.essence = std::min(this->_resource.essence + 2, this->_resource.maxEssence);
                            return NodeStatus::SUCCESS;
                        }
                        return NodeStatus::FAILURE;
                    })
                })
            });

            behaviorTree->SetRoot(root);
        }
    }

private:
    PreservationEmpowermentTracker _empowermentTracker;
    EchoTracker _echoTracker;
    uint32 _essenceBurstStacks;
};

} // namespace Playerbot
