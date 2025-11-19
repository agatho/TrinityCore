/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * Protection Paladin Refactored - Template-Based Implementation
 *
 * This file provides a complete, template-based implementation of Protection Paladin
 * using the TankSpecialization with dual resource system (Mana + Holy Power).
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
#include "../../Services/ThreatAssistant.h"  // Phase 5C: Unified threat service

// Phase 5 Integration: Decision Systems
#include "../../Decision/ActionPriorityQueue.h"
#include "../../Decision/BehaviorTree.h"
#include "../../BotAI.h"

namespace Playerbot
{

// ============================================================================
// PROTECTION PALADIN SPELL IDs (WoW 11.2 - The War Within)
// ============================================================================

enum ProtectionPaladinSpells
{
    // Holy Power Generators

    JUDGMENT_PROT
    = 275779,  // 3% mana, 6 sec CD, 1 HP
    HAMMER_OF_WRATH_PROT     = 24275,   // 10% mana, 7.5 sec CD, 1 HP (execute)
    BLESSED_HAMMER           = 204019,  // 10% mana, 3 charges, 1 HP per charge (talent)
    AVENGERS_SHIELD          = 31935,   // 10% mana, 15 sec CD, ranged silence

    // Holy Power Spenders
    SHIELD_OF_THE_RIGHTEOUS  = 53600,   // 3 HP, physical damage reduction
    WORD_OF_GLORY_PROT       = 85673,   // 3 HP, self-heal
    LIGHT_OF_THE_PROTECTOR   = 184092,  // 3 HP, strong self-heal (talent)

    // Threat Generation

    CONSECRATION
    = 26573,   // 18% mana, ground AoE
    HAMMER_OF_THE_RIGHTEOUS  = 53595,   // 9% mana, melee AoE

    // Active Mitigation
    GUARDIAN_OF_ANCIENT_KINGS = 86659,  // 5 min CD, 50% damage reduction
    ARDENT_DEFENDER          = 31850,   // 2 min CD, cheat death
    DIVINE_PROTECTION_PROT   = 498,     // 1 min CD, magic damage reduction
    BLESSING_OF_SPELLWARDING = 204018,  // Magic immunity (replaces Divine Protection)

    // Major Cooldowns
    AVENGING_WRATH_PROT      = 31884,   // 2 min CD, damage/healing buff

    SENTINEL
    = 389539,  // 5 min CD, massive armor (talent)

    FINAL_STAND
    = 204077,  // Increases Ardent Defender effectiveness

    // Utility
    HAND_OF_RECKONING        = 62124,   // Taunt
    BLESSING_OF_FREEDOM_PROT = 1044,    // Remove movement impairment
    BLESSING_OF_PROTECTION_PROT = 1022, // Physical immunity
    LAY_ON_HANDS_PROT        = 633,     // 10 min CD, full heal
    DIVINE_SHIELD_PROT       = 642,     // 5 min CD, immunity
    CLEANSE_TOXINS           = 213644,  // Dispel poison/disease

    // Auras
    DEVOTION_AURA_PROT       = 465,     // Armor buff
    CONCENTRATION_AURA_PROT  = 317920,  // Interrupt resistance
    RETRIBUTION_AURA_PROT    = 183435,  // Damage reflect

    // Procs and Buffs
    GRAND_CRUSADER           = 85043,   // Proc: free Avenger's Shield

    SHINING_LIGHT
    = 327510,  // Proc: free Word of Glory

    // Talents

    SERAPHIM
    = 152262,  // 3 HP, all stats buff
    BULWARK_OF_RIGHTEOUS_FURY = 386653, // Shield of the Righteous extended duration
    MOMENT_OF_GLORY          = 327193,  // Avenger's Shield CDR

    FIRST_AVENGER
    = 203776   // Avenger's Shield extra charge
};

// Forward declaration - ManaHolyPowerResource should be defined in ResourceTypes.h
// If not, we define it here as a local type
#ifndef PLAYERBOT_RESOURCE_TYPES_MANA_HOLY_POWER
#define PLAYERBOT_RESOURCE_TYPES_MANA_HOLY_POWER
struct ManaHolyPowerResource
{
    uint32 mana{0};
    uint32 holyPower{0};
    uint32 maxMana{100000};
    uint32 maxHolyPower{5};

    bool available{true};

    bool Consume(uint32 manaCost) {
        if (mana >= manaCost) {

            mana -= manaCost;

            return true;
        }
        return false;
    }

    void Regenerate(uint32 diff) {
        // Resource regeneration logic (simplified)
        available = true;
    }

    [[nodiscard]] uint32 GetAvailable() const {
        return mana > 0 ? 100 : 0; // Simplified for ComplexResource concept
    }

    [[nodiscard]] uint32 GetMax() const {
        return 100; // Simplified for ComplexResource concept
    }

    void Initialize(Player* bot) {
        if (bot) {

            maxMana = bot->GetMaxPower(POWER_MANA);

            mana = bot->GetPower(POWER_MANA);
            maxHolyPower = bot->GetMaxPower(POWER_HOLY_POWER);

            holyPower = bot->GetPower(POWER_HOLY_POWER);
            }
    }
};
#endif // PLAYERBOT_RESOURCE_TYPES_MANA_HOLY_POWER

// ============================================================================
// PROTECTION PALADIN SHIELD TRACKER
// ============================================================================

class ProtectionShieldTracker
{
public:
    ProtectionShieldTracker()
        : _shieldActive(false)
        , _shieldEndTime(0)
        , _shieldStacks(0)
    {
    }

    void ApplyShield()
    {
        _shieldActive = true;
        _shieldEndTime = GameTime::GetGameTimeMS() + 4500; // 4.5 sec duration
        _shieldStacks = 1;
    }

    bool IsActive() const { return _shieldActive; }

    uint32 GetTimeRemaining() const
    {
        if (!_shieldActive)

            return 0;

        uint32 now = GameTime::GetGameTimeMS();
        return _shieldEndTime > now ? _shieldEndTime - now : 0;
    }

    bool NeedsRefresh() const
    {
        // Refresh if less than 1.5 seconds remaining
        return !_shieldActive || GetTimeRemaining() < 1500;
    }

    void Update()
    {
        if (_shieldActive && GameTime::GetGameTimeMS() >= _shieldEndTime)
        {

            _shieldActive = false;

            _shieldEndTime = 0;

            _shieldStacks = 0;
        }
    }

private:
    CooldownManager _cooldowns;
    bool _shieldActive;
    uint32 _shieldEndTime;
    uint32 _shieldStacks;
};

// ============================================================================
// PROTECTION PALADIN REFACTORED
// ============================================================================

class ProtectionPaladinRefactored : public TankSpecialization<ManaHolyPowerResource>, public PaladinSpecialization
{
public:
    using Base = TankSpecialization<ManaHolyPowerResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit ProtectionPaladinRefactored(Player* bot)        : TankSpecialization<ManaHolyPowerResource>(bot)
        , PaladinSpecialization(bot)
        , _shieldTracker()
        , _consecrationActive(false)        , _consecrationEndTime(0)
        , _grandCrusaderProc(false)
        , _lastJudgmentTime(0)
        , _lastAvengersShieldTime(0)
    {
        // Initialize mana/holy power resources
        this->_resource.Initialize(bot);

        // Initialize Phase 5 systems
        InitializeProtectionPaladinMechanics();

        TC_LOG_DEBUG("playerbot", "ProtectionPaladinRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))

            return;

        // Update Protection state
        UpdateProtectionState();

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
    {        Player* bot = this->GetBot();
        if (!bot)

            return;

        // Maintain Devotion Aura
        if (!bot->HasAura(DEVOTION_AURA_PROT) && this->CanCastSpell(DEVOTION_AURA_PROT, bot))
        {

            this->CastSpell(bot, DEVOTION_AURA_PROT);
        }

        // Emergency defensives
        HandleEmergencyDefensives();
    }

    // OnTauntRequired - uses unified ThreatAssistant service (Phase 5C integration)
    void TauntTarget(::Unit* target) override    {
        // Use ThreatAssistant to determine best taunt target and execute
        Unit* tauntTarget = target ? target : bot::ai::ThreatAssistant::GetTauntTarget(this->GetBot());
        if (tauntTarget && this->CanCastSpell(HAND_OF_RECKONING, tauntTarget))
        {

            bot::ai::ThreatAssistant::ExecuteTaunt(this->GetBot(), tauntTarget, HAND_OF_RECKONING);

            TC_LOG_DEBUG("playerbot", "Protection: Taunt cast on {} via ThreatAssistant", tauntTarget->GetName());
        }
    }

    // Note: GetOptimalRange is declared final in base template (TankSpecialization)
    // It returns 5.0f for all tanks, so no override needed

protected:
    void ExecuteSingleTargetThreatRotation(::Unit* target)
    {
        uint32 hp = this->_resource.holyPower;

        // Priority 1: Maintain Shield of the Righteous
        if (_shieldTracker.NeedsRefresh() && hp >= 3)
        {

            if (this->CanCastSpell(SHIELD_OF_THE_RIGHTEOUS, this->GetBot()))

            {

                this->CastSpell(this->GetBot(), SHIELD_OF_THE_RIGHTEOUS);

                _shieldTracker.ApplyShield();

                ConsumeHolyPower(3);

                return;

            }
        }

        // Priority 2: Use Grand Crusader proc
        if (_grandCrusaderProc)
        {

            if (this->CanCastSpell(AVENGERS_SHIELD, target))

            {

                this->CastSpell(target, AVENGERS_SHIELD);

                _lastAvengersShieldTime = GameTime::GetGameTimeMS();

                _grandCrusaderProc = false;

                return;

            }
        }

        // Priority 3: Judgment for Holy Power
        if (hp < 5 && this->CanCastSpell(JUDGMENT_PROT, target))
        {

            this->CastSpell(target, JUDGMENT_PROT);

            _lastJudgmentTime = GameTime::GetGameTimeMS();

            GenerateHolyPower(1);

            return;
        }

        // Priority 4: Hammer of Wrath (execute range)
        if (target->GetHealthPct() < 20.0f && hp < 5)
        {

            if (this->CanCastSpell(HAMMER_OF_WRATH_PROT, target))

            {

                this->CastSpell(target, HAMMER_OF_WRATH_PROT);

                GenerateHolyPower(1);

                return;

            }
        }

        // Priority 5: Avenger's Shield on cooldown
        if (this->CanCastSpell(AVENGERS_SHIELD, target))
        {

            this->CastSpell(target, AVENGERS_SHIELD);

            _lastAvengersShieldTime = GameTime::GetGameTimeMS();

            return;
        }

        // Priority 6: Maintain Consecration
        if (!_consecrationActive && this->CanCastSpell(CONSECRATION, this->GetBot()))
        {

            this->CastSpell(this->GetBot(), CONSECRATION);

            _consecrationActive = true;

            _consecrationEndTime = GameTime::GetGameTimeMS() + 12000;

            return;
        }

        // Priority 7: Blessed Hammer (talent)
        if (hp < 5 && this->CanCastSpell(BLESSED_HAMMER, this->GetBot()))
        {

            this->CastSpell(this->GetBot(), BLESSED_HAMMER);

            GenerateHolyPower(1);

            return;
        }

        // Priority 8: Hammer of the Righteous
        if (this->CanCastSpell(HAMMER_OF_THE_RIGHTEOUS, target))
        {

            this->CastSpell(target, HAMMER_OF_THE_RIGHTEOUS);

            return;
        }
    }

    void ExecuteAoEThreatRotation(::Unit* target, uint32 enemyCount)
    {
        uint32 hp = this->_resource.holyPower;

        // Priority 1: Shield of the Righteous
        if (_shieldTracker.NeedsRefresh() && hp >= 3)
        {

            if (this->CanCastSpell(SHIELD_OF_THE_RIGHTEOUS, this->GetBot()))

            {

                this->CastSpell(this->GetBot(), SHIELD_OF_THE_RIGHTEOUS);

                _shieldTracker.ApplyShield();

                ConsumeHolyPower(3);

                return;

            }
        }

        // Priority 2: Avenger's Shield (cleaves)
        if (this->CanCastSpell(AVENGERS_SHIELD, target))
        {

            this->CastSpell(target, AVENGERS_SHIELD);

            return;
        }

        // Priority 3: Consecration for AoE threat
        if (!_consecrationActive && this->CanCastSpell(CONSECRATION, this->GetBot()))
        {

            this->CastSpell(this->GetBot(), CONSECRATION);

            _consecrationActive = true;

            _consecrationEndTime = GameTime::GetGameTimeMS() + 12000;

            return;
        }

        // Priority 4: Hammer of the Righteous AoE
        if (this->CanCastSpell(HAMMER_OF_THE_RIGHTEOUS, target))
        {

            this->CastSpell(target, HAMMER_OF_THE_RIGHTEOUS);

            return;
        }

        // Priority 5: Judgment
        if (hp < 5 && this->CanCastSpell(JUDGMENT_PROT, target))
        {

            this->CastSpell(target, JUDGMENT_PROT);

            GenerateHolyPower(1);

            return;
        }
    }

    void HandleEmergencyDefensives()
    {
        Player* bot = this->GetBot();
        float healthPct = bot->GetHealthPct();

        // Critical: Divine Shield
        if (healthPct < 15.0f && this->CanCastSpell(DIVINE_SHIELD_PROT, bot))
        {

            this->CastSpell(bot, DIVINE_SHIELD_PROT);
            

        // Register cooldowns using CooldownManager
        _cooldowns.RegisterBatch({

            {JUDGMENT_PROT, 6000, 1},

            {HAMMER_OF_WRATH_PROT, 7500, 1},

            {AVENGERS_SHIELD, CooldownPresets::INTERRUPT, 1},

            {GUARDIAN_OF_ANCIENT_KINGS, 300000, 1},

            {ARDENT_DEFENDER, CooldownPresets::MINOR_OFFENSIVE, 1},

            {DIVINE_PROTECTION_PROT, CooldownPresets::OFFENSIVE_60, 1},

            {AVENGING_WRATH_PROT, CooldownPresets::MINOR_OFFENSIVE, 1},

            {LAY_ON_HANDS_PROT, CooldownPresets::BLOODLUST, 1},

            {DIVINE_SHIELD_PROT, 300000, 1},

            {HAND_OF_RECKONING, CooldownPresets::DISPEL, 1},

            {SERAPHIM, CooldownPresets::OFFENSIVE_45, 1},
        });

        TC_LOG_DEBUG("playerbot", "Protection: Divine Shield emergency");

            return;
        }

        // Very low: Lay on Hands
        if (healthPct < 20.0f && this->CanCastSpell(LAY_ON_HANDS_PROT, bot))
        {

            this->CastSpell(bot, LAY_ON_HANDS_PROT);

            TC_LOG_DEBUG("playerbot", "Protection: Lay on Hands emergency");

            return;
        }

        // Low: Guardian of Ancient Kings
        if (healthPct < 35.0f && this->CanCastSpell(GUARDIAN_OF_ANCIENT_KINGS, bot))
        {

            this->CastSpell(bot, GUARDIAN_OF_ANCIENT_KINGS);

            TC_LOG_DEBUG("playerbot", "Protection: Guardian of Ancient Kings");

            return;
        }

        // Moderate: Ardent Defender
        if (healthPct < 50.0f && this->CanCastSpell(ARDENT_DEFENDER, bot))
        {

            this->CastSpell(bot, ARDENT_DEFENDER);

            TC_LOG_DEBUG("playerbot", "Protection: Ardent Defender");

            return;
        }

        // Heal with Word of Glory
        if (healthPct < 60.0f && this->_resource.holyPower >= 3)
        {

            if (this->CanCastSpell(WORD_OF_GLORY_PROT, bot))

            {

                this->CastSpell(bot, WORD_OF_GLORY_PROT);

                ConsumeHolyPower(3);

                return;

            }
        }
    }

private:
    void UpdateProtectionState()
    {
        uint32 now = GameTime::GetGameTimeMS();

        // Update Shield of the Righteous
        _shieldTracker.Update();

        // Update Consecration
        if (_consecrationActive && now >= _consecrationEndTime)
        {

            _consecrationActive = false;

            _consecrationEndTime = 0;
        }

        // Update Grand Crusader proc
        if (this->GetBot() && this->GetBot()->HasAura(GRAND_CRUSADER))

            _grandCrusaderProc = true;
        else

            _grandCrusaderProc = false;

        // Update Holy Power from bot
        if (this->GetBot())

            this->_resource.holyPower = this->GetBot()->GetPower(POWER_HOLY_POWER);
    }

    void GenerateHolyPower(uint32 amount)
    {
        this->_resource.holyPower = std::min(this->_resource.holyPower + amount, this->_resource.maxHolyPower);
    }

    void ConsumeHolyPower(uint32 amount)
    {
        this->_resource.holyPower = (this->_resource.holyPower > amount) ? this->_resource.holyPower - amount : 0;
    }

    void InitializeProtectionPaladinMechanics()
    {
        using namespace bot::ai;
        using namespace bot::ai::BehaviorTreeBuilder;

        // ========================================================================
        // PHASE 5 INTEGRATION: ActionPriorityQueue (Tank + Holy Power Focus)
        // ========================================================================
        BotAI* ai = this->GetBot()->GetBotAI();
        if (!ai)

            return;

        auto* queue = ai->GetActionPriorityQueue();
        if (queue)
        {
            // ====================================================================
            // EMERGENCY TIER - Life-saving defensives
            // ====================================================================

            queue->RegisterSpell(LAY_ON_HANDS_PROT,

                SpellPriority::EMERGENCY,

                SpellCategory::DEFENSIVE);

            queue->AddCondition(LAY_ON_HANDS_PROT,

                [](Player* bot, Unit*) {

                    return bot->GetHealthPct() < 20.0f;

                },

                "Self HP < 20% (Lay on Hands)");


            queue->RegisterSpell(DIVINE_SHIELD_PROT,

                SpellPriority::EMERGENCY,

                SpellCategory::DEFENSIVE);

            queue->AddCondition(DIVINE_SHIELD_PROT,

                [](Player* bot, Unit*) {

                    return bot->GetHealthPct() < 15.0f;

                },

                "Self HP < 15% (Divine Shield)");

            // ====================================================================
            // CRITICAL TIER - Active mitigation and major defensives
            // ====================================================================

            queue->RegisterSpell(SHIELD_OF_THE_RIGHTEOUS,

                SpellPriority::CRITICAL,

                SpellCategory::DEFENSIVE);

            queue->AddCondition(SHIELD_OF_THE_RIGHTEOUS,

                [this](Player* bot, Unit*) {
                    // Use SotR when we have 3+ HP and shield needs refresh

                    return this->_resource.holyPower >= 3 &&

                           this->_shieldTracker.NeedsRefresh();

                },

                "3+ HP and shield needs refresh");


            queue->RegisterSpell(GUARDIAN_OF_ANCIENT_KINGS,

                SpellPriority::CRITICAL,

                SpellCategory::DEFENSIVE);

            queue->AddCondition(GUARDIAN_OF_ANCIENT_KINGS,

                [](Player* bot, Unit*) {

                    return bot->GetHealthPct() < 35.0f;

                },

                "HP < 35% (Guardian)");


            queue->RegisterSpell(ARDENT_DEFENDER,

                SpellPriority::CRITICAL,

                SpellCategory::DEFENSIVE);

            queue->AddCondition(ARDENT_DEFENDER,

                [](Player* bot, Unit*) {

                    return bot->GetHealthPct() < 50.0f;

                },

                "HP < 50% (Ardent Defender)");


            queue->RegisterSpell(HAND_OF_RECKONING,

                SpellPriority::CRITICAL,

                SpellCategory::UTILITY);

            queue->AddCondition(HAND_OF_RECKONING,

                [](Player* bot, Unit* target) {
                    // Taunt when target not on tank

                    return target && !bot::ai::ThreatAssistant::IsTargetOnTank(bot, target);

                },

                "Target not on tank (taunt)");

            // ====================================================================
            // HIGH TIER - Threat generation and Holy Power builders
            // ====================================================================

            queue->RegisterSpell(AVENGERS_SHIELD,

                SpellPriority::HIGH,

                SpellCategory::DAMAGE_SINGLE);

            queue->AddCondition(AVENGERS_SHIELD,

                [](Player* bot, Unit*) {
                    // Always use on cooldown for threat

                    return true;

                },

                "High threat generation");


            queue->RegisterSpell(JUDGMENT_PROT,

                SpellPriority::HIGH,

                SpellCategory::DAMAGE_SINGLE);

            queue->AddCondition(JUDGMENT_PROT,

                [this](Player* bot, Unit*) {
                    // Generate HP when below max

                    return this->_resource.holyPower < 5;

                },

                "HP < 5 (HP generation)");


            queue->RegisterSpell(HAMMER_OF_WRATH_PROT,

                SpellPriority::HIGH,

                SpellCategory::DAMAGE_SINGLE);

            queue->AddCondition(HAMMER_OF_WRATH_PROT,

                [this](Player* bot, Unit* target) {
                    // Execute phase HP generation

                    return target && target->GetHealthPct() < 20.0f &&

                           this->_resource.holyPower < 5;

                },

                "Target < 20% and HP < 5");


            queue->RegisterSpell(BLESSED_HAMMER,

                SpellPriority::HIGH,

                SpellCategory::DAMAGE_AOE);

            queue->AddCondition(BLESSED_HAMMER,

                [this](Player* bot, Unit*) {
                    // Talented HP generator

                    return this->_resource.holyPower < 5;

                },

                "HP < 5 (talented)");

            // ====================================================================
            // MEDIUM TIER - Core rotation and utility
            // ====================================================================

            queue->RegisterSpell(CONSECRATION,

                SpellPriority::MEDIUM,

                SpellCategory::DAMAGE_AOE);

            queue->AddCondition(CONSECRATION,

                [this](Player* bot, Unit*) {

                    return !this->_consecrationActive;

                },

                "Consecration not active");


            queue->RegisterSpell(HAMMER_OF_THE_RIGHTEOUS,

                SpellPriority::MEDIUM,

                SpellCategory::DAMAGE_AOE);


            queue->RegisterSpell(DIVINE_PROTECTION_PROT,

                SpellPriority::MEDIUM,

                SpellCategory::DEFENSIVE);

            queue->AddCondition(DIVINE_PROTECTION_PROT,

                [](Player* bot, Unit*) {
                    // Magic damage reduction

                    return bot->GetHealthPct() < 60.0f;

                },

                "HP < 60% (magic reduction)");


            queue->RegisterSpell(AVENGING_WRATH_PROT,

                SpellPriority::MEDIUM,

                SpellCategory::OFFENSIVE);

            queue->AddCondition(AVENGING_WRATH_PROT,

                [](Player* bot, Unit* target) {
                    // Threat burst on bosses or packs

                    return (target && target->GetMaxHealth() > 500000) ||

                           bot->GetAttackersCount() >= 3;

                },

                "Boss or 3+ enemies (threat burst)");

            // ====================================================================
            // LOW TIER - Self-healing and utility
            // ====================================================================

            queue->RegisterSpell(WORD_OF_GLORY_PROT,

                SpellPriority::LOW,

                SpellCategory::HEALING);

            queue->AddCondition(WORD_OF_GLORY_PROT,

                [this](Player* bot, Unit*) {
                    // Self-heal when HP moderate and we have 3 HP

                    return bot->GetHealthPct() < 70.0f &&

                           this->_resource.holyPower >= 3;

                },

                "HP < 70% and 3+ HP");


            queue->RegisterSpell(CLEANSE_TOXINS,

                SpellPriority::LOW,

                SpellCategory::UTILITY);


            queue->RegisterSpell(BLESSING_OF_FREEDOM_PROT,

                SpellPriority::LOW,

                SpellCategory::UTILITY);


            queue->RegisterSpell(BLESSING_OF_PROTECTION_PROT,

                SpellPriority::LOW,

                SpellCategory::DEFENSIVE);


            TC_LOG_INFO("module.playerbot", "🛡️  PROTECTION PALADIN: Registered {} spells in ActionPriorityQueue",

                queue->GetSpellCount());
        }

        // ========================================================================
        // PHASE 5 INTEGRATION: BehaviorTree (Tank + Holy Power Flow)
        // ========================================================================
        auto* behaviorTree = ai->GetBehaviorTree();
        if (behaviorTree)
        {

            auto root = Selector("Protection Paladin Tank", {
                // ================================================================
                // TIER 1: EMERGENCY DEFENSIVES (HP < 35%)
                // ================================================================

                Sequence("Emergency Defensives", {

                    Condition("Critical HP < 35%", [](Player* bot, Unit*) {

                        return bot->GetHealthPct() < 35.0f;

                    }),

                    Selector("Emergency Response", {
                        // Divine Shield at critical HP

                        Action("Cast Divine Shield", [this](Player* bot, Unit* target) {

                            if (bot->GetHealthPct() < 15.0f &&

                                this->CanCastSpell(DIVINE_SHIELD_PROT, bot))

                            {

                                this->CastSpell(bot, DIVINE_SHIELD_PROT);

                                return NodeStatus::SUCCESS;

                            }

                            return NodeStatus::FAILURE;

                        }),
                        // Lay on Hands

                        Action("Cast Lay on Hands", [this](Player* bot, Unit* target) {

                            if (bot->GetHealthPct() < 20.0f &&

                                this->CanCastSpell(LAY_ON_HANDS_PROT, bot))

                            {

                                this->CastSpell(bot, LAY_ON_HANDS_PROT);

                                return NodeStatus::SUCCESS;

                            }

                            return NodeStatus::FAILURE;

                        }),
                        // Guardian of Ancient Kings

                        Action("Cast Guardian", [this](Player* bot, Unit* target) {

                            if (bot->GetHealthPct() < 35.0f &&

                                this->CanCastSpell(GUARDIAN_OF_ANCIENT_KINGS, bot))

                            {

                                this->CastSpell(bot, GUARDIAN_OF_ANCIENT_KINGS);

                                return NodeStatus::SUCCESS;

                            }

                            return NodeStatus::FAILURE;

                        }),
                        // Ardent Defender

                        Action("Cast Ardent Defender", [this](Player* bot, Unit* target) {

                            if (bot->GetHealthPct() < 50.0f &&

                                this->CanCastSpell(ARDENT_DEFENDER, bot))

                            {

                                this->CastSpell(bot, ARDENT_DEFENDER);

                                return NodeStatus::SUCCESS;

                            }

                            return NodeStatus::FAILURE;

                        }),
                        // Word of Glory emergency heal

                        Action("Cast Word of Glory", [this](Player* bot, Unit* target) {

                            if (this->_resource.holyPower >= 3 &&

                                bot->GetHealthPct() < 60.0f &&

                                this->CanCastSpell(WORD_OF_GLORY_PROT, bot))

                            {

                                this->CastSpell(bot, WORD_OF_GLORY_PROT);

                                this->ConsumeHolyPower(3);

                                return NodeStatus::SUCCESS;

                            }

                            return NodeStatus::FAILURE;

                        })

                    })

                }),

                // ================================================================
                // TIER 2: ACTIVE MITIGATION (Shield of the Righteous)
                // ================================================================

                Sequence("Active Mitigation", {

                    Condition("Shield needs refresh", [this](Player* bot, Unit*) {

                        return this->_shieldTracker.NeedsRefresh();

                    }),

                    Condition("Has 3+ Holy Power", [this](Player* bot, Unit*) {

                        return this->_resource.holyPower >= 3;

                    }),

                    Action("Cast Shield of the Righteous", [this](Player* bot, Unit* target) {

                        if (this->CanCastSpell(SHIELD_OF_THE_RIGHTEOUS, bot))

                        {

                            this->CastSpell(bot, SHIELD_OF_THE_RIGHTEOUS);

                            this->_shieldTracker.ApplyShield();

                            this->ConsumeHolyPower(3);

                            return NodeStatus::SUCCESS;

                        }

                        return NodeStatus::FAILURE;

                    })

                }),

                // ================================================================
                // TIER 3: THREAT MANAGEMENT
                // ================================================================

                Sequence("Threat Management", {

                    Condition("Target not on tank", [](Player* bot, Unit* target) {

                        return target && !bot::ai::ThreatAssistant::IsTargetOnTank(bot, target);

                    }),

                    Action("Cast Hand of Reckoning", [this](Player* bot, Unit* target) {

                        if (this->CanCastSpell(HAND_OF_RECKONING, target))

                        {

                            bot::ai::ThreatAssistant::ExecuteTaunt(bot, target, HAND_OF_RECKONING);

                            return NodeStatus::SUCCESS;

                        }

                        return NodeStatus::FAILURE;

                    })

                }),

                // ================================================================
                // TIER 4: HOLY POWER MANAGEMENT
                // ================================================================

                Sequence("Holy Power Management", {

                    Selector("HP Generation and Spending", {
                        // Spend HP at max (if shield doesn't need refresh)

                        Sequence("Spend at Max HP", {

                            Condition("HP = 5", [this](Player* bot, Unit*) {

                                return this->_resource.holyPower >= 5;

                            }),

                            Condition("Shield active", [this](Player* bot, Unit*) {

                                return this->_shieldTracker.IsActive();

                            }),

                            Action("Cast Word of Glory", [this](Player* bot, Unit* target) {

                                if (bot->GetHealthPct() < 90.0f &&

                                    this->CanCastSpell(WORD_OF_GLORY_PROT, bot))

                                {

                                    this->CastSpell(bot, WORD_OF_GLORY_PROT);

                                    this->ConsumeHolyPower(3);

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),
                        // Generate HP

                        Sequence("Generate Holy Power", {

                            Condition("HP < 5", [this](Player* bot, Unit*) {

                                return this->_resource.holyPower < 5;

                            }),

                            Selector("HP Generator Priority", {
                                // Avenger's Shield (high threat)

                                Action("Cast Avenger's Shield", [this](Player* bot, Unit* target) {

                                    if (this->CanCastSpell(AVENGERS_SHIELD, target))

                                    {

                                        this->CastSpell(target, AVENGERS_SHIELD);

                                        this->_lastAvengersShieldTime = GameTime::GetGameTimeMS();

                                        return NodeStatus::SUCCESS;

                                    }

                                    return NodeStatus::FAILURE;

                                }),
                                // Judgment

                                Action("Cast Judgment", [this](Player* bot, Unit* target) {

                                    if (this->CanCastSpell(JUDGMENT_PROT, target))

                                    {

                                        this->CastSpell(target, JUDGMENT_PROT);

                                        this->_lastJudgmentTime = GameTime::GetGameTimeMS();

                                        this->GenerateHolyPower(1);

                                        return NodeStatus::SUCCESS;

                                    }

                                    return NodeStatus::FAILURE;

                                }),
                                // Hammer of Wrath (execute)

                                Sequence("Hammer of Wrath", {

                                    Condition("Target < 20%", [](Player* bot, Unit* target) {

                                        return target && target->GetHealthPct() < 20.0f;

                                    }),

                                    Action("Cast Hammer of Wrath", [this](Player* bot, Unit* target) {

                                        if (this->CanCastSpell(HAMMER_OF_WRATH_PROT, target))

                                        {

                                            this->CastSpell(target, HAMMER_OF_WRATH_PROT);

                                            this->GenerateHolyPower(1);

                                            return NodeStatus::SUCCESS;

                                        }

                                        return NodeStatus::FAILURE;

                                    })

                                }),
                                // Blessed Hammer (talent)

                                Action("Cast Blessed Hammer", [this](Player* bot, Unit* target) {

                                    if (this->CanCastSpell(BLESSED_HAMMER, bot))

                                    {

                                        this->CastSpell(bot, BLESSED_HAMMER);

                                        this->GenerateHolyPower(1);

                                        return NodeStatus::SUCCESS;

                                    }

                                    return NodeStatus::FAILURE;

                                })

                            })

                        })

                    })

                }),

                // ================================================================
                // TIER 5: TANK ROTATION (Threat and damage)
                // ================================================================

                Sequence("Standard Tank Rotation", {

                    Selector("Rotation Priority", {
                        // Maintain Consecration

                        Sequence("Consecration", {

                            Condition("Not active", [this](Player* bot, Unit*) {

                                return !this->_consecrationActive;

                            }),

                            Action("Cast Consecration", [this](Player* bot, Unit* target) {

                                if (this->CanCastSpell(CONSECRATION, bot))

                                {

                                    this->CastSpell(bot, CONSECRATION);

                                    this->_consecrationActive = true;

                                    this->_consecrationEndTime = GameTime::GetGameTimeMS() + 12000;

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),

                        // Cooldown usage

                        Sequence("Avenging Wrath", {

                            Condition("Boss or pack", [](Player* bot, Unit* target) {

                                return (target && target->GetMaxHealth() > 500000) ||

                                       bot->GetAttackersCount() >= 3;

                            }),

                            Action("Cast Avenging Wrath", [this](Player* bot, Unit* target) {

                                if (this->CanCastSpell(AVENGING_WRATH_PROT, bot))

                                {

                                    this->CastSpell(bot, AVENGING_WRATH_PROT);

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),

                        // Hammer of the Righteous filler

                        Action("Cast Hammer of the Righteous", [this](Player* bot, Unit* target) {

                            if (this->CanCastSpell(HAMMER_OF_THE_RIGHTEOUS, target))

                            {

                                this->CastSpell(target, HAMMER_OF_THE_RIGHTEOUS);

                                return NodeStatus::SUCCESS;

                            }

                            return NodeStatus::FAILURE;

                        })

                    })

                })

            });


            behaviorTree->SetRoot(root);

            TC_LOG_INFO("module.playerbot", "🌲 PROTECTION PALADIN: BehaviorTree initialized with tank flow");
        }
    }



private:
    ProtectionShieldTracker _shieldTracker;
    bool _consecrationActive;
    uint32 _consecrationEndTime;
    bool _grandCrusaderProc;
    uint32 _lastJudgmentTime;
    uint32 _lastAvengersShieldTime;
};

} // namespace Playerbot
