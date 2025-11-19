/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * Unholy Death Knight Refactored - Template-Based Implementation
 *
 * This file provides a complete, template-based implementation of Unholy Death Knight
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
#include "Pet.h"
#include "Log.h"
#include "../Decision/ActionPriorityQueue.h"
#include "../Decision/BehaviorTree.h"
#include "../BotAI.h"

namespace Playerbot
{

// ============================================================================
// UNHOLY DEATH KNIGHT SPELL IDs (WoW 11.2 - The War Within)
// ============================================================================

enum UnholyDeathKnightSpells
{
    // Rune Spenders
    FESTERING_STRIKE         = 85948,   // 2 Runes, applies Festering Wounds
    SCOURGE_STRIKE           = 55090,   // 1 Rune, bursts Festering Wounds
    CLAWING_SHADOWS          = 207311,  // 1 Rune, ranged Scourge Strike (talent)
    EPIDEMIC                 = 207317,  // 1 Rune, spreads Virulent Plague (talent)
    DEFILE                   = 152280,  // 1 Rune, ground AoE (talent)

    // Runic Power Spenders
    DEATH_COIL               = 47541,   // 30-40 RP, main RP spender
    DARK_TRANSFORMATION      = 63560,   // 40 RP, transforms ghoul

    // Diseases
    VIRULENT_PLAGUE          = 191587,  // Main disease DoT
    OUTBREAK                 = 77575,   // Applies Virulent Plague

    // Pet Management
    RAISE_DEAD_UNHOLY        = 46585,   // Summon permanent ghoul
    SUMMON_GARGOYLE          = 49206,   // 3 min CD, summon gargoyle
    ARMY_OF_THE_DEAD_UNHOLY  = 42650,   // 8 min CD, summon ghouls
    APOCALYPSE               = 275699,  // 1.5 min CD, burst wounds + summon ghouls
    RAISE_ABOMINATION        = 455395,  // 1.5 min CD, summon abomination (talent)

    // Major Cooldowns
    UNHOLY_ASSAULT           = 207289,  // 1.5 min CD, burst damage (talent)
    UNHOLY_BLIGHT            = 115989,  // 45 sec CD, AoE disease spread (talent)
    SOUL_REAPER              = 343294,  // 6 sec CD, execute damage

    // Utility
    DEATH_GRIP_UNHOLY        = 49576,   // 25 sec CD, pull
    MIND_FREEZE_UNHOLY       = 47528,   // Interrupt
    CHAINS_OF_ICE_UNHOLY     = 45524,   // Root/slow
    DARK_COMMAND_UNHOLY      = 56222,   // Taunt
    ANTI_MAGIC_SHELL_UNHOLY  = 48707,   // 1 min CD, magic absorption
    ICEBOUND_FORTITUDE_UNHOLY = 48792,  // 3 min CD, damage reduction
    DEATHS_ADVANCE_UNHOLY    = 48265,   // 1.5 min CD, speed + mitigation
    CONTROL_UNDEAD_UNHOLY    = 111673,  // Mind control undead
    RAISE_ALLY_UNHOLY        = 61999,   // Battle res

    // Procs and Buffs
    SUDDEN_DOOM              = 49530,   // Proc: free Death Coil
    RUNIC_CORRUPTION         = 51460,   // Proc: increased rune regen
    FESTERING_WOUND          = 194310,  // Debuff on target (stacks)
    UNHOLY_STRENGTH          = 53365,   // Passive: pet damage buff

    // Talents
    BURSTING_SORES           = 207264,  // Festering Wound burst AoE
    INFECTED_CLAWS           = 207272,  // Pet applies Festering Wounds
    ALL_WILL_SERVE           = 194916,  // Summon skeleton on Death Coil
    UNHOLY_PACT              = 319230,  // Dark Transformation damage buff
    SUPERSTRAIN              = 390283   // Disease damage buff
};

// Dual resource type for Unholy Death Knight
struct UnholyRuneRunicPowerResource
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
            return true;
        }
        return false;
    }
    void Regenerate(uint32 diff) {
        // Resource regeneration logic (simplified)
        available = true;
    }

    [[nodiscard]] uint32 GetAvailable() const {
        return 100; // Simplified - override in specific implementations
    }

    [[nodiscard]] uint32 GetMax() const {
        return 100; // Simplified - override in specific implementations
    }


    void Initialize(Player* bot) {
        bloodRunes = 2;
        frostRunes = 2;
        unholyRunes = 2;
        runicPower = 0;
    }
};

// ============================================================================
// UNHOLY FESTERING WOUND TRACKER
// ============================================================================

class UnholyFesteringWoundTracker
{
public:
    UnholyFesteringWoundTracker()
        : _trackedTargets()
    {
    }

    void ApplyWounds(ObjectGuid guid, uint32 count)
    {
        _trackedTargets[guid] = std::min(_trackedTargets[guid] + count, 6u); // Max 6 stacks
    }

    void BurstWounds(ObjectGuid guid, uint32 count)
    {
        if (_trackedTargets[guid] > count)
            _trackedTargets[guid] -= count;
        else
            _trackedTargets.erase(guid);
    }

    uint32 GetWoundCount(ObjectGuid guid) const
    {
        auto it = _trackedTargets.find(guid);
        return (it != _trackedTargets.end()) ? it->second : 0;
    }

    void Update(Unit* target)
    {
        if (!target)
            return;

        ObjectGuid guid = target->GetGUID();        

        // Sync with actual aura
        if (Aura* aura = target->GetAura(FESTERING_WOUND))
            _trackedTargets[guid] = aura->GetStackAmount();        else
            _trackedTargets.erase(guid);
    }

private:
    CooldownManager _cooldowns;
    std::unordered_map<ObjectGuid, uint32> _trackedTargets;
};

// ============================================================================
// UNHOLY PET TRACKER
// ============================================================================

class UnholyPetTracker
{
public:
    UnholyPetTracker()
        : _ghoulActive(false)
        , _gargoyleActive(false)
        , _gargoyleEndTime(0)
        , _darkTransformationActive(false)
        , _darkTransformationEndTime(0)
    {
    }

    void SummonGhoul() { _ghoulActive = true; }
    bool IsGhoulActive() const { return _ghoulActive; }

    void SummonGargoyle()
    {
        _gargoyleActive = true;
        _gargoyleEndTime = GameTime::GetGameTimeMS() + 30000; // 30 sec duration
    }

    bool IsGargoyleActive() const { return _gargoyleActive; }

    void ActivateDarkTransformation()
    {
        _darkTransformationActive = true;
        _darkTransformationEndTime = GameTime::GetGameTimeMS() + 15000; // 15 sec duration
    }

    bool IsDarkTransformationActive() const { return _darkTransformationActive; }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        // Update ghoul status
        Pet* pet = bot->GetPet();
        _ghoulActive = (pet && pet->IsAlive());

        // Update gargoyle
        uint32 now = GameTime::GetGameTimeMS();
        if (_gargoyleActive && now >= _gargoyleEndTime)
        {
            _gargoyleActive = false;
            _gargoyleEndTime = 0;
        }

        // Update Dark Transformation
        if (_darkTransformationActive && now >= _darkTransformationEndTime)
        {
            _darkTransformationActive = false;
            _darkTransformationEndTime = 0;
        }
    }

private:
    bool _ghoulActive;
    bool _gargoyleActive;
    uint32 _gargoyleEndTime;
    bool _darkTransformationActive;
    uint32 _darkTransformationEndTime;
};

// ============================================================================
// UNHOLY DEATH KNIGHT REFACTORED
// ============================================================================

class UnholyDeathKnightRefactored : public MeleeDpsSpecialization<UnholyRuneRunicPowerResource>
{
public:
    using Base = MeleeDpsSpecialization<UnholyRuneRunicPowerResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit UnholyDeathKnightRefactored(Player* bot)        : MeleeDpsSpecialization<UnholyRuneRunicPowerResource>(bot)        , _woundTracker()
        , _petTracker()
        , _suddenDoomProc(false)
        , _lastOutbreakTime(0)
    {
        // Initialize runes/runic power resources
        this->_resource.Initialize(bot);
        TC_LOG_DEBUG("playerbot", "UnholyDeathKnightRefactored initialized for {}", bot->GetName());
        InitializeUnholyMechanics();
    }

    void UpdateRotation(::Unit* target) override    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))
            return;

        // Update Unholy state
        UpdateUnholyState(target);

        // Ensure ghoul is summoned
        EnsureGhoulActive();

        // Use major cooldowns
        HandleCooldowns(target);

        // Determine if AoE or single target
        uint32 enemyCount = this->GetEnemiesInRange(10.0f);
        if (enemyCount >= 3)
        {
            ExecuteAoERotation(target, enemyCount);
        }        else
        {
            ExecuteSingleTargetRotation(target);
        }
    }    void UpdateBuffs() override
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
        ObjectGuid targetGuid = target->GetGUID();        uint32 rp = this->_resource.runicPower;
        uint32 totalRunes = this->_resource.bloodRunes + this->_resource.frostRunes + this->_resource.unholyRunes;
        uint32 wounds = _woundTracker.GetWoundCount(targetGuid);        // Priority 1: Apply/maintain Virulent Plague
        if (!target->HasAura(VIRULENT_PLAGUE) && this->CanCastSpell(OUTBREAK, target))
        {
            this->CastSpell(target, OUTBREAK);
            _lastOutbreakTime = GameTime::GetGameTimeMS();
            return;
        }

        // Priority 2: Soul Reaper (execute < 35%)
        if (target->GetHealthPct() < 35.0f && this->CanCastSpell(SOUL_REAPER, target))
        {
            this->CastSpell(target, SOUL_REAPER);
            return;
        }

        // Priority 3: Death Coil with Sudden Doom proc
        if (_suddenDoomProc && this->CanCastSpell(DEATH_COIL, target))
        {
            this->CastSpell(target, DEATH_COIL);
            _suddenDoomProc = false;
            return;
        }

        // Priority 4: Scourge Strike (burst wounds if 4+ stacks)
        if (wounds >= 4 && totalRunes >= 1 && this->CanCastSpell(SCOURGE_STRIKE, target))
        {
            this->CastSpell(target, SCOURGE_STRIKE);
            _woundTracker.BurstWounds(targetGuid, wounds);
            ConsumeRunes(RuneType::UNHOLY, 1);
            GenerateRunicPower(10);
            return;
        }

        // Priority 5: Festering Strike (apply wounds)
        if (wounds < 4 && totalRunes >= 2 && this->CanCastSpell(FESTERING_STRIKE, target))
        {
            this->CastSpell(target, FESTERING_STRIKE);
            _woundTracker.ApplyWounds(targetGuid, 4);
            ConsumeRunes(RuneType::UNHOLY, 2);
            GenerateRunicPower(15);
            return;
        }

        // Priority 6: Dark Transformation (empower ghoul)
        if (rp >= 40 && !_petTracker.IsDarkTransformationActive() && this->CanCastSpell(DARK_TRANSFORMATION, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), DARK_TRANSFORMATION);
            _petTracker.ActivateDarkTransformation();
            ConsumeRunicPower(40);
            return;
        }

        // Priority 7: Death Coil (dump RP)
        if (rp >= 50 && this->CanCastSpell(DEATH_COIL, target))        {
            this->CastSpell(target, DEATH_COIL);
            ConsumeRunicPower(30);
            return;
        }

        // Priority 8: Scourge Strike (burst any wounds)
        if (wounds > 0 && totalRunes >= 1 && this->CanCastSpell(SCOURGE_STRIKE, target))
        {
            this->CastSpell(target, SCOURGE_STRIKE);
            _woundTracker.BurstWounds(targetGuid, wounds);
            ConsumeRunes(RuneType::UNHOLY, 1);
            GenerateRunicPower(10);
            return;
        }

        // Priority 9: Death Coil (prevent RP capping)
        if (rp >= 30 && this->CanCastSpell(DEATH_COIL, target))
        {
            this->CastSpell(target, DEATH_COIL);
            ConsumeRunicPower(30);
            return;
        }
    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)    {
        uint32 rp = this->_resource.runicPower;
        uint32 totalRunes = this->_resource.bloodRunes + this->_resource.frostRunes + this->_resource.unholyRunes;

        // Priority 1: Apply Virulent Plague
        if (!target->HasAura(VIRULENT_PLAGUE) && this->CanCastSpell(OUTBREAK, target))
        {
            this->CastSpell(target, OUTBREAK);
            return;
        }

        // Priority 2: Epidemic (spread disease)
        if (totalRunes >= 1 && this->CanCastSpell(EPIDEMIC, target))        {
            this->CastSpell(target, EPIDEMIC);
            ConsumeRunes(RuneType::UNHOLY, 1);
            GenerateRunicPower(10);
            return;
        }

        // Priority 3: Defile (talent, ground AoE)
        if (totalRunes >= 1 && this->CanCastSpell(DEFILE, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), DEFILE);
            ConsumeRunes(RuneType::UNHOLY, 1);
            return;
        }

        // Priority 4: Death Coil
        if (rp >= 30 && this->CanCastSpell(DEATH_COIL, target))
        {
            this->CastSpell(target, DEATH_COIL);
            ConsumeRunicPower(30);
            return;
        }

        // Priority 5: Scourge Strike
        if (totalRunes >= 1 && this->CanCastSpell(SCOURGE_STRIKE, target))
        {
            this->CastSpell(target, SCOURGE_STRIKE);
            ConsumeRunes(RuneType::UNHOLY, 1);
            GenerateRunicPower(10);
            return;
        }
    }

    void HandleCooldowns(::Unit* target)
    {
        ObjectGuid targetGuid = target->GetGUID();        uint32 wounds = _woundTracker.GetWoundCount(targetGuid);
        uint32 totalRunes = this->_resource.bloodRunes + this->_resource.frostRunes + this->_resource.unholyRunes;

        // Apocalypse (burst wounds + summon ghouls)
        if (wounds >= 4 && this->CanCastSpell(APOCALYPSE, target))
        {
            this->CastSpell(target, APOCALYPSE);
            _woundTracker.BurstWounds(targetGuid, wounds);
            

        // Register cooldowns using CooldownManager
        _cooldowns.RegisterBatch({
            {FESTERING_STRIKE, 0, 1},
            {SCOURGE_STRIKE, 0, 1},
            {DEATH_COIL, 0, 1},
            {OUTBREAK, 0, 1},
            {DARK_TRANSFORMATION, 0, 1},
            {APOCALYPSE, 90000, 1},
            {ARMY_OF_THE_DEAD_UNHOLY, 480000, 1},
            {SUMMON_GARGOYLE, CooldownPresets::MAJOR_OFFENSIVE, 1},
            {UNHOLY_ASSAULT, 90000, 1},
            {UNHOLY_BLIGHT, CooldownPresets::OFFENSIVE_45, 1},
            {SOUL_REAPER, 6000, 1},
            {DEATH_GRIP_UNHOLY, 25000, 1},
            {ANTI_MAGIC_SHELL_UNHOLY, CooldownPresets::OFFENSIVE_60, 1},
            {ICEBOUND_FORTITUDE_UNHOLY, CooldownPresets::MAJOR_OFFENSIVE, 1},
            {DEATHS_ADVANCE_UNHOLY, 90000, 1},
        });

        TC_LOG_DEBUG("playerbot", "Unholy: Apocalypse");
        }

        // Army of the Dead
        if (this->CanCastSpell(ARMY_OF_THE_DEAD_UNHOLY, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), ARMY_OF_THE_DEAD_UNHOLY);
            TC_LOG_DEBUG("playerbot", "Unholy: Army of the Dead");
        }

        // Summon Gargoyle
        if (this->CanCastSpell(SUMMON_GARGOYLE, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), SUMMON_GARGOYLE);
            _petTracker.SummonGargoyle();
            TC_LOG_DEBUG("playerbot", "Unholy: Summon Gargoyle");
        }

        // Unholy Assault (talent)
        if (totalRunes >= 2 && this->CanCastSpell(UNHOLY_ASSAULT, target))
        {
            this->CastSpell(target, UNHOLY_ASSAULT);
            TC_LOG_DEBUG("playerbot", "Unholy: Unholy Assault");
        }

        // Unholy Blight (talent)
        if (this->CanCastSpell(UNHOLY_BLIGHT, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), UNHOLY_BLIGHT);
            TC_LOG_DEBUG("playerbot", "Unholy: Unholy Blight");
        }
    }

    void HandleDefensiveCooldowns()
    {
        Player* bot = this->GetBot();
        float healthPct = bot->GetHealthPct();

        // Icebound Fortitude
        if (healthPct < 40.0f && this->CanCastSpell(ICEBOUND_FORTITUDE_UNHOLY, bot))
        {
            this->CastSpell(bot, ICEBOUND_FORTITUDE_UNHOLY);
            TC_LOG_DEBUG("playerbot", "Unholy: Icebound Fortitude");
            return;
        }

        // Anti-Magic Shell
        if (healthPct < 60.0f && this->CanCastSpell(ANTI_MAGIC_SHELL_UNHOLY, bot))
        {
            this->CastSpell(bot, ANTI_MAGIC_SHELL_UNHOLY);
            TC_LOG_DEBUG("playerbot", "Unholy: Anti-Magic Shell");
            return;
        }

        // Death's Advance
        if (healthPct < 70.0f && this->CanCastSpell(DEATHS_ADVANCE_UNHOLY, bot))
        {
            this->CastSpell(bot, DEATHS_ADVANCE_UNHOLY);
            TC_LOG_DEBUG("playerbot", "Unholy: Death's Advance");
            return;
        }
    }

    void EnsureGhoulActive()
    {
        if (_petTracker.IsGhoulActive())
            return;

        Player* bot = this->GetBot();
        if (!bot)
            return;

        if (this->CanCastSpell(RAISE_DEAD_UNHOLY, bot))
        {
            this->CastSpell(bot, RAISE_DEAD_UNHOLY);
            _petTracker.SummonGhoul();
            TC_LOG_DEBUG("playerbot", "Unholy: Raise Dead (ghoul)");
        }
    }

private:
    void UpdateUnholyState(::Unit* target)
    {
        // Update Festering Wound tracker
        _woundTracker.Update(target);

        // Update pet tracker
        _petTracker.Update(this->GetBot());

        // Update Sudden Doom proc
        if (this->GetBot() && this->GetBot()->HasAura(SUDDEN_DOOM))
            _suddenDoomProc = true;
        else
            _suddenDoomProc = false;

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

    void InitializeUnholyMechanics()
    {
        using namespace bot::ai;
        using namespace bot::ai::BehaviorTreeBuilder;
        BotAI* ai = this->GetBot()->GetBotAI();
        if (!ai) return;

        auto* queue = ai->GetActionPriorityQueue();
        if (queue) {
            queue->RegisterSpell(UNHOLY_ANTIMAGIC_SHELL, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);
            queue->AddCondition(UNHOLY_ANTIMAGIC_SHELL, [](Player* bot, Unit*) { return bot && bot->GetHealthPct() < 40.0f; }, "HP < 40%");

            queue->RegisterSpell(UNHOLY_ARMY_OF_DEAD, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);
            queue->AddCondition(UNHOLY_ARMY_OF_DEAD, [](Player*, Unit* target) { return target != nullptr; }, "Major CD");

            queue->RegisterSpell(UNHOLY_APOCALYPSE, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);
            queue->AddCondition(UNHOLY_APOCALYPSE, [this](Player*, Unit* target) { return target && this->_woundTracker.GetWounds(target->GetGUID()) >= 4; }, "4+ wounds");

            queue->RegisterSpell(UNHOLY_FESTERING_STRIKE, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(UNHOLY_FESTERING_STRIKE, [this](Player*, Unit* target) { return target && this->_woundTracker.GetWounds(target->GetGUID()) < 4; }, "< 4 wounds");

            queue->RegisterSpell(UNHOLY_SCOURGE_STRIKE, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(UNHOLY_SCOURGE_STRIKE, [this](Player*, Unit* target) { return target && this->_woundTracker.GetWounds(target->GetGUID()) > 0; }, "Has wounds");

            queue->RegisterSpell(UNHOLY_DEATH_COIL, SpellPriority::MEDIUM, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(UNHOLY_DEATH_COIL, [this](Player*, Unit* target) { return target && (this->_suddenDoomProc || this->_resource.runicPower >= 40); }, "Sudden Doom or 40 RP");
        }

        auto* tree = ai->GetBehaviorTree();
        if (tree) {
            auto root = Selector("Unholy DK", {
                Sequence("Burst", { Condition("Has target", [this](Player* bot) { return bot && bot->GetVictim(); }),
                    Action("Army/Apocalypse", [this](Player* bot) { Unit* t = bot->GetVictim(); if (t && this->CanCastSpell(UNHOLY_ARMY_OF_DEAD, bot)) { this->CastSpell(bot, UNHOLY_ARMY_OF_DEAD); return NodeStatus::SUCCESS; } return NodeStatus::FAILURE; }) }),
                Sequence("Wounds", { Condition("< 4 wounds", [this](Player* bot) { Unit* t = bot->GetVictim(); return t && this->_woundTracker.GetWounds(t->GetGUID()) < 4; }),
                    Action("Festering Strike", [this](Player* bot) { Unit* t = bot->GetVictim(); if (t && this->CanCastSpell(UNHOLY_FESTERING_STRIKE, t)) { this->CastSpell(t, UNHOLY_FESTERING_STRIKE); this->_woundTracker.AddWounds(t->GetGUID(), 2); return NodeStatus::SUCCESS; } return NodeStatus::FAILURE; }) }),
                Sequence("Pop Wounds", { Condition("Has wounds", [this](Player* bot) { Unit* t = bot->GetVictim(); return t && this->_woundTracker.GetWounds(t->GetGUID()) > 0; }),
                    Action("Scourge Strike", [this](Player* bot) { Unit* t = bot->GetVictim(); if (t && this->CanCastSpell(UNHOLY_SCOURGE_STRIKE, t)) { this->CastSpell(t, UNHOLY_SCOURGE_STRIKE); this->_woundTracker.ConsumeWounds(t->GetGUID(), 1); return NodeStatus::SUCCESS; } return NodeStatus::FAILURE; }) }),
                Sequence("RP Dump", { Condition("40+ RP", [this](Player*) { return this->_resource.runicPower >= 40; }),
                    Action("Death Coil", [this](Player* bot) { Unit* t = bot->GetVictim(); if (t && this->CanCastSpell(UNHOLY_DEATH_COIL, t)) { this->CastSpell(t, UNHOLY_DEATH_COIL); this->ConsumeRunicPower(40); return NodeStatus::SUCCESS; } return NodeStatus::FAILURE; }) })
            });
            tree->SetRoot(root);
        }
    }

private:
    UnholyFesteringWoundTracker _woundTracker;
    UnholyPetTracker _petTracker;
    bool _suddenDoomProc;
    uint32 _lastOutbreakTime;
};

} // namespace Playerbot
