/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * Augmentation Evoker Refactored - Enterprise-Grade Header-Only Implementation
 *
 * Support specialization providing damage amplification buffs to allies.
 */

#pragma once

#include "../CombatSpecializationTemplates.h"
#include "../ResourceTypes.h"
#include "Player.h"
#include "Group.h"
#include "Log.h"
#include "../Decision/ActionPriorityQueue.h"
#include "../Decision/BehaviorTree.h"
#include "../BotAI.h"

namespace Playerbot
{

enum AugmentationEvokerSpells
{
    // Core Buffs
    EBON_MIGHT           = 395152,  // 1 essence, 10s damage buff on ally
    PRESCIENCE           = 409311,  // 1 essence, crit buff on ally
    BLISTERING_SCALES    = 360827,  // Thorns buff on ally

    // Empowered Abilities
    BREATH_OF_EONS       = 403631,  // 3 essence, empowered damage + Ebon Might extension

    // Essence Generation
    AZURE_STRIKE_AUG     = 362969,  // Generates 2 essence
    ERUPTION             = 395160,  // 3 essence, AoE damage

    // Utility
    OBSIDIAN_SCALES      = 363916,
    RENEWING_BLAZE       = 374348,
    QUELL                = 351338,
    HOVER                = 358267
};

struct EssenceResourceAug
{
    uint32 essence{0};
    uint32 maxEssence{5};
    bool available{true};

    bool Consume(uint32 cost) { if (essence >= cost) { essence -= cost; return true; } return false; }
    void Regenerate(uint32) { available = true; }
    [[nodiscard]] uint32 GetAvailable() const { return essence; }
    [[nodiscard]] uint32 GetMax() const { return maxEssence; }
    void Initialize(Player* bot) { if (bot) { essence = 0; maxEssence = 5; } }
};

class AugmentationBuffTracker
{
public:
    std::unordered_map<ObjectGuid, uint32> _ebonMightTargets;
    std::unordered_map<ObjectGuid, uint32> _prescienceTargets;

    void ApplyEbonMight(ObjectGuid guid) { _ebonMightTargets[guid] = GameTime::GetGameTimeMS() + 10000; }
    void ApplyPrescience(ObjectGuid guid) { _prescienceTargets[guid] = GameTime::GetGameTimeMS() + 18000; }

    bool HasEbonMight(ObjectGuid guid) const {
        auto it = _ebonMightTargets.find(guid);
        return it != _ebonMightTargets.end() && GameTime::GetGameTimeMS() < it->second;
    }

    bool HasPrescience(ObjectGuid guid) const {
        auto it = _prescienceTargets.find(guid);
        return it != _prescienceTargets.end() && GameTime::GetGameTimeMS() < it->second;
    }

    void Update() {
        uint32 now = GameTime::GetGameTimeMS();
        for (auto it = _ebonMightTargets.begin(); it != _ebonMightTargets.end();)
            it = (now >= it->second) ? _ebonMightTargets.erase(it) : ++it;
        for (auto it = _prescienceTargets.begin(); it != _prescienceTargets.end();)
            it = (now >= it->second) ? _prescienceTargets.erase(it) : ++it;
    }
};

class AugmentationEvokerRefactored : public SupportSpecialization<EssenceResourceAug>
{
public:
    using Base = SupportSpecialization<EssenceResourceAug>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;

    explicit AugmentationEvokerRefactored(Player* bot)
        : SupportSpecialization<EssenceResourceAug>(bot)
        , _buffTracker()
    {
        this->_resource.Initialize(bot);
        InitializeAugmentationMechanics();
        TC_LOG_DEBUG("playerbot", "AugmentationEvokerRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        if (!target) return;
        UpdateAugmentationState();

        // Priority 1: Maintain Ebon Might on DPS allies
        if (MaintainEbonMight()) return;

        // Priority 2: Maintain Prescience on top DPS
        if (MaintainPrescience()) return;

        // Priority 3: Use Breath of Eons
        if (this->_resource.essence >= 3 && this->CanCastSpell(BREATH_OF_EONS, target)) {
            this->CastSpell(target, BREATH_OF_EONS);
            this->_resource.Consume(3);
            return;
        }

        // Priority 4: Eruption AoE
        if (this->_resource.essence >= 3 && this->GetEnemiesInRange(25.0f) >= 2 && this->CanCastSpell(ERUPTION, target)) {
            this->CastSpell(target, ERUPTION);
            this->_resource.Consume(3);
            return;
        }

        // Priority 5: Generate essence
        if (this->_resource.essence < 4 && this->CanCastSpell(AZURE_STRIKE_AUG, target)) {
            this->CastSpell(target, AZURE_STRIKE_AUG);
            this->_resource.essence = std::min(this->_resource.essence + 2, this->_resource.maxEssence);
        }
    }

    void UpdateBuffs() override { /* Handled in UpdateRotation */ }
    float GetOptimalRange(::Unit* target) override { return 25.0f; }

protected:
    bool MaintainEbonMight()
    {
        if (this->_resource.essence < 1) return false;

        std::vector<Unit*> allies = GetGroupDPS();
        for (Unit* ally : allies) {
            if (ally && !_buffTracker.HasEbonMight(ally->GetGUID())) {
                if (this->CanCastSpell(EBON_MIGHT, ally)) {
                    this->CastSpell(ally, EBON_MIGHT);
                    this->_resource.Consume(1);
                    _buffTracker.ApplyEbonMight(ally->GetGUID());
                    return true;
                }
            }
        }
        return false;
    }

    bool MaintainPrescience()
    {
        if (this->_resource.essence < 1) return false;

        std::vector<Unit*> allies = GetGroupDPS();
        for (Unit* ally : allies) {
            if (ally && !_buffTracker.HasPrescience(ally->GetGUID())) {
                if (this->CanCastSpell(PRESCIENCE, ally)) {
                    this->CastSpell(ally, PRESCIENCE);
                    this->_resource.Consume(1);
                    _buffTracker.ApplyPrescience(ally->GetGUID());
                    return true;
                }
            }
        }
        return false;
    }

    std::vector<Unit*> GetGroupDPS() const
    {
        std::vector<Unit*> dps;
        Player* bot = this->GetBot();
        if (!bot) return dps;

        Group* group = bot->GetGroup();
        if (!group) return dps;

        for (GroupReference const& ref : group->GetMembers()) {
            if (Player* member = ref.GetSource()) {
                if (member->IsAlive() && bot->IsInMap(member) && !IsTankOrHealer(member))
                    dps.push_back(member);
            }
        }
        return dps;
    }

    bool IsTankOrHealer(Player* player) const
    {
        // Simple heuristic: check role or class
        uint8 cls = player->GetClass();
        return (cls == CLASS_PRIEST || cls == CLASS_PALADIN || cls == CLASS_DRUID ||
                cls == CLASS_SHAMAN || cls == CLASS_MONK);
    }

    void UpdateAugmentationState()
    {
        _buffTracker.Update();
        Player* bot = this->GetBot();
        if (bot)
            this->_resource.essence = bot->GetPower(POWER_ALTERNATE_POWER);
    }

    void InitializeAugmentationMechanics()
    {
        using namespace bot::ai;
        using namespace bot::ai::BehaviorTreeBuilder;

        BotAI* ai = this->GetBot()->GetBotAI();
        if (!ai) return;

        auto* queue = ai->GetActionPriorityQueue();
        if (queue)
        {
            queue->RegisterSpell(EBON_MIGHT, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);
            queue->AddCondition(EBON_MIGHT, [this](Player*, Unit*) {
                if (this->_resource.essence < 1) return false;
                auto allies = this->GetGroupDPS();
                for (auto* a : allies) if (a && !_buffTracker.HasEbonMight(a->GetGUID())) return true;
                return false;
            }, "Ally without Ebon Might (10s damage buff)");

            queue->RegisterSpell(PRESCIENCE, SpellPriority::HIGH, SpellCategory::OFFENSIVE);
            queue->AddCondition(PRESCIENCE, [this](Player*, Unit*) {
                if (this->_resource.essence < 1) return false;
                auto allies = this->GetGroupDPS();
                for (auto* a : allies) if (a && !_buffTracker.HasPrescience(a->GetGUID())) return true;
                return false;
            }, "Ally without Prescience (18s crit buff)");

            queue->RegisterSpell(BREATH_OF_EONS, SpellPriority::HIGH, SpellCategory::DAMAGE_AOE);
            queue->AddCondition(BREATH_OF_EONS, [this](Player*, Unit* target) {
                return target && this->_resource.essence >= 3;
            }, "3 essence (empowered damage + extend Ebon Might)");

            queue->RegisterSpell(ERUPTION, SpellPriority::MEDIUM, SpellCategory::DAMAGE_AOE);
            queue->AddCondition(ERUPTION, [this](Player*, Unit* target) {
                return target && this->_resource.essence >= 3 && this->GetEnemiesInRange(25.0f) >= 2;
            }, "3 essence, 2+ enemies (AoE damage)");

            queue->RegisterSpell(AZURE_STRIKE_AUG, SpellPriority::LOW, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(AZURE_STRIKE_AUG, [this](Player*, Unit* target) {
                return target && this->_resource.essence < 4;
            }, "Essence < 4 (generates 2 essence)");

            queue->RegisterSpell(OBSIDIAN_SCALES, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);
            queue->AddCondition(OBSIDIAN_SCALES, [](Player* bot, Unit*) {
                return bot && bot->GetHealthPct() < 40.0f;
            }, "HP < 40% (30% dmg reduction)");
        }

        auto* tree = ai->GetBehaviorTree();
        if (tree)
        {
            auto root = Selector("Augmentation Evoker Support", {
                Sequence("Maintain Buffs", {
                    Condition("Has essence", [this](Player*) { return this->_resource.essence >= 1; }),
                    Selector("Apply buffs", {
                        Sequence("Ebon Might", {
                            Action("Cast Ebon Might", [this](Player*) {
                                return this->MaintainEbonMight() ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Prescience", {
                            Action("Cast Prescience", [this](Player*) {
                                return this->MaintainPrescience() ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
                            })
                        })
                    })
                }),
                Sequence("Deal Damage", {
                    Condition("Has target", [this](Player* bot) { return bot && bot->GetVictim(); }),
                    Condition("3+ essence", [this](Player*) { return this->_resource.essence >= 3; }),
                    Action("Cast Breath of Eons", [this](Player* bot) {
                        Unit* target = bot->GetVictim();
                        if (target && this->CanCastSpell(BREATH_OF_EONS, target)) {
                            this->CastSpell(target, BREATH_OF_EONS);
                            this->_resource.Consume(3);
                            return NodeStatus::SUCCESS;
                        }
                        return NodeStatus::FAILURE;
                    })
                }),
                Sequence("Generate Essence", {
                    Condition("Has target", [this](Player* bot) { return bot && bot->GetVictim(); }),
                    Condition("< 4 essence", [this](Player*) { return this->_resource.essence < 4; }),
                    Action("Cast Azure Strike", [this](Player* bot) {
                        Unit* target = bot->GetVictim();
                        if (target && this->CanCastSpell(AZURE_STRIKE_AUG, target)) {
                            this->CastSpell(target, AZURE_STRIKE_AUG);
                            this->_resource.essence = std::min(this->_resource.essence + 2, this->_resource.maxEssence);
                            return NodeStatus::SUCCESS;
                        }
                        return NodeStatus::FAILURE;
                    })
                })
            });

            tree->SetRoot(root);
        }
    }

private:
    AugmentationBuffTracker _buffTracker;
};

} // namespace Playerbot
