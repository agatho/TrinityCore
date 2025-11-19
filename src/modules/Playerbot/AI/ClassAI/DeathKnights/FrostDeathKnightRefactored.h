/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * Frost Death Knight Refactored - Template-Based Implementation
 *
 * This file provides a complete, template-based implementation of Frost Death Knight
 * using the MeleeDpsSpecialization with dual resource system (Runes + Runic Power).
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
// FROST DEATH KNIGHT SPELL IDs (WoW 11.2 - The War Within)
// ============================================================================

enum FrostDeathKnightSpells
{
    // Rune Spenders
    OBLITERATE               = 49020,   // 2 Runes, main damage dealer
    HOWLING_BLAST            = 49184,   // 1 Rune, AoE + applies Frost Fever
    REMORSELESS_WINTER       = 196770,  // 1 Rune, 20 sec CD, AoE slow
    GLACIAL_ADVANCE          = 194913,  // 2 Runes, ranged AoE (talent)
    FROSTSCYTHE              = 207230,  // 2 Runes, AoE cleave (talent)

    // Runic Power Spenders
    FROST_STRIKE             = 49143,   // 25 RP, main RP spender
    HORN_OF_WINTER           = 57330,   // 2 Runes + 25 RP gen (talent)

    // Cooldowns
    PILLAR_OF_FROST          = 51271,   // 1 min CD, major damage buff
    EMPOWER_RUNE_WEAPON      = 47568,   // 2 min CD, rune refresh
    BREATH_OF_SINDRAGOSA     = 152279,  // 2 min CD, channel (talent)
    FROSTWYRMS_FURY          = 279302,  // 3 min CD, AoE burst (talent)

    // Utility
    DEATH_GRIP_FROST         = 49576,   // 25 sec CD, pull
    MIND_FREEZE_FROST        = 47528,   // Interrupt
    CHAINS_OF_ICE            = 45524,   // Root/slow
    DARK_COMMAND_FROST       = 56222,   // Taunt
    ANTI_MAGIC_SHELL_FROST   = 48707,   // 1 min CD, magic absorption
    ICEBOUND_FORTITUDE_FROST = 48792,   // 3 min CD, damage reduction
    DEATHS_ADVANCE_FROST     = 48265,   // 1.5 min CD, speed + mitigation

    // Procs and Buffs
    KILLING_MACHINE          = 51128,   // Proc: crit on Obliterate
    RIME                     = 59052,   // Proc: free Howling Blast
    RAZORICE                 = 50401,   // Debuff: stacking damage amp
    FROZEN_PULSE             = 194909,  // Passive AoE (talent)

    // Diseases
    FROST_FEVER_DK           = 55095,   // Disease from Howling Blast

    // Talents
    OBLITERATION             = 281238,  // Pillar of Frost extension
    BREATH_OF_SINDRAGOSA_TALENT = 152279, // Channel burst
    GATHERING_STORM          = 194912,  // Remorseless Winter buff
    ICECAP                   = 207126,  // Pillar of Frost CDR
    INEXORABLE_ASSAULT       = 253593,  // Cold Heart stacking buff
    COLD_HEART               = 281208   // Chains of Ice nuke (talent)
};

// Dual resource type for Frost Death Knight (simplified runes)
struct FrostRuneRunicPowerResource
{
    uint32 bloodRunes{0};
    uint32 frostRunes{0};
    uint32 unholyRunes{0};
    uint32 runicPower{0};
    uint32 maxRunicPower{100};
    bool available{true};

    bool Consume(uint32 runesCost) {
        uint32 totalRunes = bloodRunes + frostRunes + unholyRunes;
        if (totalRunes >= runesCost) {
            uint32 remaining = runesCost;
            if (bloodRunes > 0) {
                uint32 toConsume = std::min(bloodRunes, remaining);
                bloodRunes -= toConsume;
                remaining -= toConsume;
            }
            if (remaining > 0 && frostRunes > 0) {
                uint32 toConsume = std::min(frostRunes, remaining);
                frostRunes -= toConsume;
                remaining -= toConsume;
            }
            if (remaining > 0 && unholyRunes > 0) {
                uint32 toConsume = std::min(unholyRunes, remaining);
                unholyRunes -= toConsume;
                remaining -= toConsume;
            }
            available = (bloodRunes + frostRunes + unholyRunes) > 0;
            return true;
        }
        return false;
    }

    void Regenerate(uint32 diff) {
        // Runes regenerate over time
        static uint32 regenTimer = 0;
        regenTimer += diff;
        if (regenTimer >= 10000) { // 10 seconds per rune
            uint32 totalRunes = bloodRunes + frostRunes + unholyRunes;
            if (totalRunes < 6) {
                if (bloodRunes < 2) bloodRunes++;
                else if (frostRunes < 2) frostRunes++;
                else if (unholyRunes < 2) unholyRunes++;
            }
            regenTimer = 0;
        }
        available = (bloodRunes + frostRunes + unholyRunes) > 0;
    }

    [[nodiscard]] uint32 GetAvailable() const {
        return bloodRunes + frostRunes + unholyRunes;
    }

    [[nodiscard]] uint32 GetMax() const {
        return 6; // Max 6 runes
    }

    void Initialize(Player* bot) {
        bloodRunes = 2;
        frostRunes = 2;
        unholyRunes = 2;
        runicPower = 0;
        available = true;
    }
};

// ============================================================================
// FROST KILLING MACHINE TRACKER
// ============================================================================

class FrostKillingMachineTracker
{
public:
    FrostKillingMachineTracker()
        : _kmActive(false)
        , _kmStacks(0)
    {
    }

    void ActivateProc(uint32 stacks = 1)
    {
        _kmActive = true;
        _kmStacks = stacks;
    }

    void ConsumeProc()
    {
        if (_kmStacks > 0)
            _kmStacks--;

        if (_kmStacks == 0)
            _kmActive = false;
    }

    bool IsActive() const { return _kmActive; }
    uint32 GetStacks() const { return _kmStacks; }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        if (Aura* aura = bot->GetAura(KILLING_MACHINE))
        {
            _kmActive = true;
            _kmStacks = aura->GetStackAmount();        }
        else
        {
            _kmActive = false;
            _kmStacks = 0;
        }
    }

private:
    CooldownManager _cooldowns;
    bool _kmActive;
    uint32 _kmStacks;
};

// ============================================================================
// FROST RIME TRACKER
// ============================================================================

class FrostRimeTracker
{
public:
    FrostRimeTracker()
        : _rimeActive(false)
    {
    }

    void ActivateProc()
    {
        _rimeActive = true;
    }

    void ConsumeProc()
    {
        _rimeActive = false;
    }

    bool IsActive() const { return _rimeActive; }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        _rimeActive = bot->HasAura(RIME);
    }

private:
    bool _rimeActive;
};

// ============================================================================
// FROST DEATH KNIGHT REFACTORED
// ============================================================================

class FrostDeathKnightRefactored : public MeleeDpsSpecialization<FrostRuneRunicPowerResource>
{
public:
    using Base = MeleeDpsSpecialization<FrostRuneRunicPowerResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit FrostDeathKnightRefactored(Player* bot)        : MeleeDpsSpecialization<FrostRuneRunicPowerResource>(bot)
        
        , _kmTracker()
        , _rimeTracker()
        , _pillarOfFrostActive(false)
        , _pillarEndTime(0)
        , _breathOfSindragosaActive(false)
        , _lastRemorselessWinterTime(0)
    {        // Initialize runes/runic power resources
        this->_resource.Initialize(bot);
        TC_LOG_DEBUG("playerbot", "FrostDeathKnightRefactored initialized for {}", bot->GetName());
        InitializeFrostMechanics();
    }

    void UpdateRotation(::Unit* target) override    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))
            return;

        // Update Frost state
        UpdateFrostState();

        // Use major cooldowns
        HandleCooldowns();

        // Determine if AoE or single target
        uint32 enemyCount = this->GetEnemiesInRange(10.0f);
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

        // Defensive cooldowns
        HandleDefensiveCooldowns();
    }


protected:
    void ExecuteSingleTargetRotation(::Unit* target)
    {
        uint32 rp = this->_resource.runicPower;
        uint32 totalRunes = this->_resource.bloodRunes + this->_resource.frostRunes + this->_resource.unholyRunes;

        // Priority 1: Breath of Sindragosa (if talented and high RP)
        if (_breathOfSindragosaActive)
        {
            // During Breath, spam Obliterate and Frost Strike to maintain
            if (rp < 20 && totalRunes >= 2 && this->CanCastSpell(OBLITERATE, target))
            {
                this->CastSpell(target, OBLITERATE);
                ConsumeRunes(RuneType::FROST, 2);
                GenerateRunicPower(15);
                return;
            }

            if (rp >= 25 && this->CanCastSpell(FROST_STRIKE, target))
            {
                this->CastSpell(target, FROST_STRIKE);
                ConsumeRunicPower(25);
                return;
            }
        }

        // Priority 2: Use Rime proc (free Howling Blast)
        if (_rimeTracker.IsActive() && this->CanCastSpell(HOWLING_BLAST, target))
        {
            this->CastSpell(target, HOWLING_BLAST);
            _rimeTracker.ConsumeProc();
            return;
        }

        // Priority 3: Obliterate with Killing Machine proc
        if (_kmTracker.IsActive() && totalRunes >= 2 && this->CanCastSpell(OBLITERATE, target))
        {
            this->CastSpell(target, OBLITERATE);
            _kmTracker.ConsumeProc();
            ConsumeRunes(RuneType::FROST, 2);
            GenerateRunicPower(15);
            return;
        }

        // Priority 4: Remorseless Winter (AoE slow)
        if (totalRunes >= 1 && this->CanCastSpell(REMORSELESS_WINTER, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), REMORSELESS_WINTER);
            _lastRemorselessWinterTime = GameTime::GetGameTimeMS();
            ConsumeRunes(RuneType::FROST, 1);
            return;
        }

        // Priority 5: Frost Strike (dump RP)
        if (rp >= 50 && this->CanCastSpell(FROST_STRIKE, target))
        {
            this->CastSpell(target, FROST_STRIKE);
            ConsumeRunicPower(25);
            return;
        }

        // Priority 6: Obliterate (main rune spender)
        if (totalRunes >= 2 && this->CanCastSpell(OBLITERATE, target))
        {
            this->CastSpell(target, OBLITERATE);
            ConsumeRunes(RuneType::FROST, 2);
            GenerateRunicPower(15);
            return;
        }

        // Priority 7: Frost Strike (prevent RP capping)
        if (rp >= 25 && this->CanCastSpell(FROST_STRIKE, target))
        {
            this->CastSpell(target, FROST_STRIKE);
            ConsumeRunicPower(25);
            return;
        }

        // Priority 8: Horn of Winter (talent, generate resources)
        if (totalRunes < 3 && rp < 70 && this->CanCastSpell(HORN_OF_WINTER, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), HORN_OF_WINTER);
            GenerateRunicPower(25);
            return;
        }
    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)
    {
        uint32 rp = this->_resource.runicPower;
        uint32 totalRunes = this->_resource.bloodRunes + this->_resource.frostRunes + this->_resource.unholyRunes;

        // Priority 1: Remorseless Winter
        if (totalRunes >= 1 && this->CanCastSpell(REMORSELESS_WINTER, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), REMORSELESS_WINTER);
            _lastRemorselessWinterTime = GameTime::GetGameTimeMS();
            ConsumeRunes(RuneType::FROST, 1);
            return;
        }

        // Priority 2: Howling Blast (AoE)
        if (totalRunes >= 1 && this->CanCastSpell(HOWLING_BLAST, target))
        {
            this->CastSpell(target, HOWLING_BLAST);
            ConsumeRunes(RuneType::FROST, 1);
            GenerateRunicPower(10);
            return;
        }

        // Priority 3: Frostscythe (talent, AoE cleave)
        if (totalRunes >= 2 && this->CanCastSpell(FROSTSCYTHE, target))
        {
            this->CastSpell(target, FROSTSCYTHE);
            ConsumeRunes(RuneType::FROST, 2);
            GenerateRunicPower(15);
            return;
        }

        // Priority 4: Glacial Advance (talent, ranged AoE)
        if (totalRunes >= 2 && this->CanCastSpell(GLACIAL_ADVANCE, target))
        {
            this->CastSpell(target, GLACIAL_ADVANCE);
            ConsumeRunes(RuneType::FROST, 2);
            return;
        }

        // Priority 5: Frost Strike (dump RP)
        if (rp >= 25 && this->CanCastSpell(FROST_STRIKE, target))
        {
            this->CastSpell(target, FROST_STRIKE);
            ConsumeRunicPower(25);
            return;
        }

        // Priority 6: Obliterate (if no AoE available)
        if (totalRunes >= 2 && this->CanCastSpell(OBLITERATE, target))
        {
            this->CastSpell(target, OBLITERATE);
            ConsumeRunes(RuneType::FROST, 2);
            GenerateRunicPower(15);
            return;
        }
    }

    void HandleCooldowns()
    {
        Player* bot = this->GetBot();
        uint32 rp = this->_resource.runicPower;
        uint32 totalRunes = this->_resource.bloodRunes + this->_resource.frostRunes + this->_resource.unholyRunes;

        // Pillar of Frost (major damage CD)
        if (totalRunes >= 3 && this->CanCastSpell(PILLAR_OF_FROST, bot))
        {
            this->CastSpell(bot, PILLAR_OF_FROST);
            _pillarOfFrostActive = true;
            _pillarEndTime = GameTime::GetGameTimeMS() + 12000; // 12 sec duration
            

        // Register cooldowns using CooldownManager
        _cooldowns.RegisterBatch({
            {OBLITERATE, 0, 1},
            {FROST_STRIKE, 0, 1},
            {HOWLING_BLAST, 0, 1},
            {REMORSELESS_WINTER, 20000, 1},
            {PILLAR_OF_FROST, CooldownPresets::OFFENSIVE_60, 1},
            {EMPOWER_RUNE_WEAPON, CooldownPresets::MINOR_OFFENSIVE, 1},
            {BREATH_OF_SINDRAGOSA, CooldownPresets::MINOR_OFFENSIVE, 1},
            {FROSTWYRMS_FURY, CooldownPresets::MAJOR_OFFENSIVE, 1},
            {DEATH_GRIP_FROST, 25000, 1},
            {ANTI_MAGIC_SHELL_FROST, CooldownPresets::OFFENSIVE_60, 1},
            {ICEBOUND_FORTITUDE_FROST, CooldownPresets::MAJOR_OFFENSIVE, 1},
            {DEATHS_ADVANCE_FROST, 90000, 1},
        });

        TC_LOG_DEBUG("playerbot", "Frost: Pillar of Frost activated");
        }

        // Empower Rune Weapon (rune refresh)
        if (totalRunes == 0 && this->CanCastSpell(EMPOWER_RUNE_WEAPON, bot))
        {
            this->CastSpell(bot, EMPOWER_RUNE_WEAPON);
            this->_resource.bloodRunes = 2;
            this->_resource.frostRunes = 2;
            this->_resource.unholyRunes = 2;
            GenerateRunicPower(25);
            TC_LOG_DEBUG("playerbot", "Frost: Empower Rune Weapon");
        }

        // Breath of Sindragosa (talent, channel burst)
        if (rp >= 60 && this->CanCastSpell(BREATH_OF_SINDRAGOSA, bot))
        {
            this->CastSpell(bot, BREATH_OF_SINDRAGOSA);
            _breathOfSindragosaActive = true;
            TC_LOG_DEBUG("playerbot", "Frost: Breath of Sindragosa");
        }

        // Frostwyrm's Fury (AoE burst)
        if (this->CanCastSpell(FROSTWYRMS_FURY, bot))
        {
            this->CastSpell(bot, FROSTWYRMS_FURY);
            TC_LOG_DEBUG("playerbot", "Frost: Frostwyrm's Fury");
        }
    }

    void HandleDefensiveCooldowns()
    {
        Player* bot = this->GetBot();
        float healthPct = bot->GetHealthPct();

        // Icebound Fortitude
        if (healthPct < 40.0f && this->CanCastSpell(ICEBOUND_FORTITUDE_FROST, bot))
        {
            this->CastSpell(bot, ICEBOUND_FORTITUDE_FROST);
            TC_LOG_DEBUG("playerbot", "Frost: Icebound Fortitude");
            return;
        }

        // Anti-Magic Shell
        if (healthPct < 60.0f && this->CanCastSpell(ANTI_MAGIC_SHELL_FROST, bot))
        {
            this->CastSpell(bot, ANTI_MAGIC_SHELL_FROST);
            TC_LOG_DEBUG("playerbot", "Frost: Anti-Magic Shell");
            return;
        }

        // Death's Advance
        if (healthPct < 70.0f && this->CanCastSpell(DEATHS_ADVANCE_FROST, bot))
        {
            this->CastSpell(bot, DEATHS_ADVANCE_FROST);
            TC_LOG_DEBUG("playerbot", "Frost: Death's Advance");
            return;
        }
    }

private:
    void UpdateFrostState()
    {
        // Update Killing Machine tracker
        _kmTracker.Update(this->GetBot());

        // Update Rime tracker
        _rimeTracker.Update(this->GetBot());

        // Update Pillar of Frost
        if (_pillarOfFrostActive && GameTime::GetGameTimeMS() >= _pillarEndTime)
        {
            _pillarOfFrostActive = false;
            _pillarEndTime = 0;
        }

        // Update Breath of Sindragosa
        if (_breathOfSindragosaActive)
        {
            if (!this->GetBot() || !this->GetBot()->HasAura(BREATH_OF_SINDRAGOSA))
                _breathOfSindragosaActive = false;
        }

        // Update Runic Power from bot
        if (this->GetBot())
            this->_resource.runicPower = this->GetBot()->GetPower(POWER_RUNIC_POWER);

        // Update runes (simplified)
        uint32 now = GameTime::GetGameTimeMS();
        static uint32 lastRuneUpdate = 0;
        if (now - lastRuneUpdate > 10000) // Every 10 seconds
        {
            this->_resource.bloodRunes = 2;
            this->_resource.frostRunes = 2;
            this->_resource.unholyRunes = 2;
            lastRuneUpdate = now;
        }
    }

    void GenerateRunicPower(uint32 amount)
    {
        this->_resource.runicPower = std::min(this->_resource.runicPower + amount, this->_resource.maxRunicPower);
    }

    void ConsumeRunicPower(uint32 amount)
    {
        this->_resource.runicPower = (this->_resource.runicPower > amount) ? this->_resource.runicPower - amount : 0;
    }

    void ConsumeRunes(RuneType type, uint32 count = 1) override
    {
        this->_resource.Consume(count);
    }

    void InitializeFrostMechanics()
    {
        using namespace bot::ai;
        using namespace bot::ai::BehaviorTreeBuilder;
        BotAI* ai = this->GetBot()->GetBotAI();
        if (!ai) return;

        auto* queue = ai->GetActionPriorityQueue();
        if (queue)
        {
            // EMERGENCY: Defensive cooldowns
            queue->RegisterSpell(FROST_ICEBOUND_FORTITUDE, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);
            queue->AddCondition(FROST_ICEBOUND_FORTITUDE, [](Player* bot, Unit*) {
                return bot && bot->GetHealthPct() < 35.0f;
            }, "HP < 35% (damage reduction)");

            // CRITICAL: Major burst cooldowns
            queue->RegisterSpell(FROST_PILLAR_OF_FROST, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);
            queue->AddCondition(FROST_PILLAR_OF_FROST, [this](Player*, Unit* target) {
                return target && !this->_pillarOfFrostActive;
            }, "Major burst CD (12s, Str buff)");

            queue->RegisterSpell(FROST_EMPOWER_RUNE_WEAPON, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);
            queue->AddCondition(FROST_EMPOWER_RUNE_WEAPON, [this](Player*, Unit* target) {
                return target && this->_resource.GetAvailableRunes() < 3;
            }, "< 3 runes (instant refresh)");

            // HIGH: Priority damage abilities
            queue->RegisterSpell(FROST_OBLITERATE, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(FROST_OBLITERATE, [this](Player*, Unit* target) {
                return target && (this->_kmTracker.IsActive() || this->_resource.GetAvailableRunes() >= 2);
            }, "KM proc or 2 runes (heavy damage)");

            queue->RegisterSpell(FROST_HOWLING_BLAST, SpellPriority::HIGH, SpellCategory::DAMAGE_AOE);
            queue->AddCondition(FROST_HOWLING_BLAST, [this](Player*, Unit* target) {
                return target && (this->_rimeTracker.IsActive() || this->GetEnemiesInRange(10.0f) >= 3);
            }, "Rime proc or 3+ enemies");

            queue->RegisterSpell(FROST_FROST_STRIKE, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(FROST_FROST_STRIKE, [this](Player*, Unit* target) {
                return target && this->_resource.runicPower >= 25;
            }, "25 RP (spender)");

            // MEDIUM: Cooldowns & utility
            queue->RegisterSpell(FROST_BREATH_OF_SINDRAGOSA, SpellPriority::MEDIUM, SpellCategory::OFFENSIVE);
            queue->AddCondition(FROST_BREATH_OF_SINDRAGOSA, [this](Player* bot, Unit* target) {
                return bot && target && bot->HasSpell(FROST_BREATH_OF_SINDRAGOSA) &&
                       this->_resource.runicPower >= 50 && !this->_breathOfSindragosaActive;
            }, "50 RP, talent (channeled burst)");

            queue->RegisterSpell(FROST_REMORSELESS_WINTER, SpellPriority::MEDIUM, SpellCategory::DAMAGE_AOE);
            queue->AddCondition(FROST_REMORSELESS_WINTER, [this](Player*, Unit*) {
                return this->GetEnemiesInRange(8.0f) >= 2;
            }, "2+ enemies (AoE damage)");

            queue->RegisterSpell(FROST_HORN_OF_WINTER, SpellPriority::MEDIUM, SpellCategory::UTILITY);
            queue->AddCondition(FROST_HORN_OF_WINTER, [this](Player*, Unit*) {
                return this->_resource.GetAvailableRunes() < 3 && this->_resource.runicPower < 60;
            }, "< 3 runes, < 60 RP (resource gen)");

            // LOW: Filler
            queue->RegisterSpell(FROST_FROST_STRIKE, SpellPriority::LOW, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(FROST_FROST_STRIKE, [this](Player*, Unit* target) {
                return target && this->_resource.runicPower >= 25;
            }, "Dump RP");
        }

        auto* behaviorTree = ai->GetBehaviorTree();
        if (behaviorTree)
        {
            auto root = Selector("Frost Death Knight DPS", {
                // Tier 1: Burst Cooldowns (Pillar of Frost)
                Sequence("Burst Cooldowns", {
                    Condition("Has target", [this](Player* bot) {
                        return bot && bot->GetVictim();
                    }),
                    Selector("Use burst", {
                        Sequence("Pillar of Frost", {
                            Condition("Not active", [this](Player*) {
                                return !this->_pillarOfFrostActive;
                            }),
                            Action("Cast Pillar", [this](Player* bot) {
                                if (this->CanCastSpell(FROST_PILLAR_OF_FROST, bot))
                                {
                                    this->CastSpell(bot, FROST_PILLAR_OF_FROST);
                                    this->_pillarOfFrostActive = true;
                                    this->_pillarEndTime = GameTime::GetGameTimeMS() + 12000;
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Empower Rune Weapon", {
                            Condition("< 3 runes", [this](Player*) {
                                return this->_resource.GetAvailableRunes() < 3;
                            }),
                            Action("Cast ERW", [this](Player* bot) {
                                if (this->CanCastSpell(FROST_EMPOWER_RUNE_WEAPON, bot))
                                {
                                    this->CastSpell(bot, FROST_EMPOWER_RUNE_WEAPON);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 2: Priority Abilities (KM Obliterate, Rime Howling Blast)
                Sequence("Priority Procs", {
                    Condition("Has target", [this](Player* bot) {
                        return bot && bot->GetVictim();
                    }),
                    Selector("Use procs", {
                        Sequence("KM Obliterate", {
                            Condition("KM active and 2 runes", [this](Player*) {
                                return this->_kmTracker.IsActive() && this->_resource.GetAvailableRunes() >= 2;
                            }),
                            Action("Cast Obliterate", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(FROST_OBLITERATE, target))
                                {
                                    this->CastSpell(target, FROST_OBLITERATE);
                                    this->GenerateRunicPower(20);
                                    if (this->_kmTracker.IsActive())
                                        this->_kmTracker.Consume();
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Rime Howling Blast", {
                            Condition("Rime active", [this](Player*) {
                                return this->_rimeTracker.IsActive();
                            }),
                            Action("Cast Howling Blast", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(FROST_HOWLING_BLAST, target))
                                {
                                    this->CastSpell(target, FROST_HOWLING_BLAST);
                                    if (this->_rimeTracker.IsActive())
                                        this->_rimeTracker.Consume();
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 3: Runic Power Spender (Frost Strike)
                Sequence("RP Spender", {
                    Condition("25+ RP and target", [this](Player* bot) {
                        return bot && bot->GetVictim() && this->_resource.runicPower >= 25;
                    }),
                    Action("Cast Frost Strike", [this](Player* bot) {
                        Unit* target = bot->GetVictim();
                        if (target && this->CanCastSpell(FROST_FROST_STRIKE, target))
                        {
                            this->CastSpell(target, FROST_FROST_STRIKE);
                            this->ConsumeRunicPower(25);
                            return NodeStatus::SUCCESS;
                        }
                        return NodeStatus::FAILURE;
                    })
                }),

                // Tier 4: Rune Spender (Obliterate builder)
                Sequence("Rune Spender", {
                    Condition("2+ runes and target", [this](Player* bot) {
                        return bot && bot->GetVictim() && this->_resource.GetAvailableRunes() >= 2;
                    }),
                    Selector("Spend runes", {
                        Sequence("Howling Blast (AoE)", {
                            Condition("3+ enemies", [this](Player*) {
                                return this->GetEnemiesInRange(10.0f) >= 3;
                            }),
                            Action("Cast Howling Blast", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(FROST_HOWLING_BLAST, target))
                                {
                                    this->CastSpell(target, FROST_HOWLING_BLAST);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Obliterate (ST)", {
                            Action("Cast Obliterate", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(FROST_OBLITERATE, target))
                                {
                                    this->CastSpell(target, FROST_OBLITERATE);
                                    this->GenerateRunicPower(20);
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
    FrostKillingMachineTracker _kmTracker;
    FrostRimeTracker _rimeTracker;
    bool _pillarOfFrostActive;
    uint32 _pillarEndTime;
    bool _breathOfSindragosaActive;
    uint32 _lastRemorselessWinterTime;
};

} // namespace Playerbot
