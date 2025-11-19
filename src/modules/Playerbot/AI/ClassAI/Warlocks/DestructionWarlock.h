/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * Destruction Warlock Refactored - Template-Based Implementation
 *
 * This file provides a complete, template-based implementation of Destruction Warlock
 * using the RangedDpsSpecialization with dual resource system (Mana + Soul Shards).
 */

#pragma once


#include "../Common/StatusEffectTracker.h"
#include "../Common/CooldownManager.h"
#include "../Common/RotationHelpers.h"
#include "../CombatSpecializationTemplates.h"
#include "../ResourceTypes.h"
#include "Player.h"
#include "Pet.h"
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
// DESTRUCTION WARLOCK SPELL IDs (WoW 11.2 - The War Within)
// ============================================================================

enum DestructionWarlockSpells
{
    // Core Spells
    CHAOS_BOLT               = 116858,  // 2 shards, heavy direct damage
    INCINERATE               = 29722,   // Filler, generates shards
    CONFLAGRATE              = 17962,   // 2 charges, 13 sec CD, generates shards
    IMMOLATE                 = 348,     // DoT, enables Conflagrate

    // AoE Spells
    RAIN_OF_FIRE             = 5740,    // 3 shards, ground AoE
    CHANNEL_DEMONFIRE        = 196447,  // Channel, requires Immolate (talent)
    CATACLYSM                = 152108,  // 30 sec CD, AoE + Immolate (talent)
    HAVOC                    = 80240,   // 30 sec CD, cleave on 2nd target

    // Major Cooldowns
    SUMMON_INFERNAL          = 1122,    // 3 min CD, summons Infernal (major CD)
    DARK_SOUL_INSTABILITY    = 113858,  // 2 min CD, crit buff (talent)
    SOUL_FIRE                = 6353,    // 20 sec CD, strong direct damage (talent)

    // Pet Management
    SUMMON_IMP_DESTRO        = 688,
    SUMMON_VOIDWALKER_DESTRO = 697,
    SUMMON_SUCCUBUS_DESTRO   = 712,
    SUMMON_FELHUNTER_DESTRO  = 691,
    COMMAND_DEMON_DESTRO     = 119898,

    // Utility
    CURSE_OF_TONGUES_DESTRO  = 1714,    // Casting slow (talent)
    CURSE_OF_WEAKNESS_DESTRO = 702,     // Reduces physical damage
    CURSE_OF_EXHAUSTION_DESTRO = 334275,// Movement slow
    SHADOWBURN               = 17877,   // Execute, generates shards (talent)
    BACKDRAFT                = 196406,  // Buff: reduces Incinerate cast time

    // Defensives
    UNENDING_RESOLVE_DESTRO  = 104773,  // 3 min CD, damage reduction
    DARK_PACT_DESTRO         = 108416,  // 1 min CD, shield (talent)
    MORTAL_COIL_DESTRO       = 6789,    // Heal + fear (talent)
    HOWL_OF_TERROR_DESTRO    = 5484,    // AoE fear (talent)
    FEAR_DESTRO              = 5782,    // CC
    BANISH_DESTRO            = 710,     // CC (demons/elementals)
    DEMONIC_CIRCLE_TELEPORT_DESTRO = 48020, // Teleport
    DEMONIC_GATEWAY_DESTRO   = 111771,  // Portal
    BURNING_RUSH_DESTRO      = 111400,  // Speed, drains health

    // Procs and Buffs
    BACKDRAFT_BUFF           = 117828,  // Buff from Conflagrate
    REVERSE_ENTROPY          = 205148,  // Buff from Rain of Fire (talent)
    ERADICATION              = 196412,  // Debuff: increases damage taken (talent)
    FLASHOVER                = 267115,  // Backdraft on Conflagrate CD end (talent)

    // Talents
    ROARING_BLAZE            = 205184,  // Conflagrate buff
    INTERNAL_COMBUSTION      = 266134,  // Chaos Bolt consumes Immolate
    FIRE_AND_BRIMSTONE       = 196408,  // Incinerate cleaves
    INFERNO                  = 270545,  // Rain of Fire stun
    GRIMOIRE_OF_SUPREMACY    = 266086   // Better pets
};

// Dual resource type for Destruction Warlock
struct ManaSoulShardResourceDestro
{
    uint32 mana{0};
    uint32 soulShards{0};
    uint32 maxMana{100000};
    uint32 maxSoulShards{5};

    bool available{true};

    bool Consume(uint32 manaCost) {
        if (mana >= manaCost) {
            mana -= manaCost;
            return true;
        }
        return false;
    }

    void Regenerate(uint32 diff) {
        if (mana < maxMana) {
            uint32 regenAmount = (maxMana / 100) * (diff / 1000);
            mana = std::min(mana + regenAmount, maxMana);
        }
        available = mana > 0;
    }

    [[nodiscard]] uint32 GetAvailable() const { return mana; }
    [[nodiscard]] uint32 GetMax() const { return maxMana; }

    void Initialize(Player* bot) {
        if (bot) {
            maxMana = bot->GetMaxPower(POWER_MANA);
            mana = bot->GetPower(POWER_MANA);        }
        soulShards = 0;
    }
};

// ============================================================================
// DESTRUCTION IMMOLATE TRACKER
// ============================================================================

class DestructionImmolateTracker
{
public:
    DestructionImmolateTracker()
        : _trackedTargets()
    {
    }

    void ApplyImmolate(ObjectGuid guid, uint32 duration)
    {
        _trackedTargets[guid] = GameTime::GetGameTimeMS() + duration;
    }

    void RemoveImmolate(ObjectGuid guid)
    {
        _trackedTargets.erase(guid);
    }

    bool HasImmolate(ObjectGuid guid) const
    {
        auto it = _trackedTargets.find(guid);
        if (it == _trackedTargets.end())
            return false;

        return GameTime::GetGameTimeMS() < it->second;
    }

    uint32 GetTimeRemaining(ObjectGuid guid) const
    {
        auto it = _trackedTargets.find(guid);
        if (it == _trackedTargets.end())
            return 0;

        uint32 now = GameTime::GetGameTimeMS();
        return (it->second > now) ? (it->second - now) : 0;
    }

    bool NeedsRefresh(ObjectGuid guid, uint32 pandemicWindow = 5400) const
    {
        uint32 remaining = GetTimeRemaining(guid);
        return remaining < pandemicWindow; // Pandemic window
    }

    void Update()
    {
        uint32 now = GameTime::GetGameTimeMS();
        for (auto it = _trackedTargets.begin(); it != _trackedTargets.end();)
        {
            if (now >= it->second)
                it = _trackedTargets.erase(it);
            else
                ++it;
        }
    }

private:
    CooldownManager _cooldowns;
    std::unordered_map<ObjectGuid, uint32> _trackedTargets;
};

// ============================================================================
// DESTRUCTION HAVOC TRACKER
// ============================================================================

class DestructionHavocTracker
{
public:
    DestructionHavocTracker()
        : _havocTargetGuid()
        , _havocEndTime(0)
        , _havocActive(false)
    {
    }

    void ApplyHavoc(ObjectGuid guid)
    {
        _havocTargetGuid = guid;
        _havocEndTime = GameTime::GetGameTimeMS() + 12000; // 12 sec duration
        _havocActive = true;
    }

    bool IsActive() const { return _havocActive; }
    ObjectGuid GetTarget() const { return _havocTargetGuid; }

    void Update()
    {
        if (_havocActive && GameTime::GetGameTimeMS() >= _havocEndTime)
        {
            _havocActive = false;
            _havocTargetGuid = ObjectGuid::Empty;
            _havocEndTime = 0;
        }
    }

private:
    ObjectGuid _havocTargetGuid;
    uint32 _havocEndTime;
    bool _havocActive;
};

// ============================================================================
// DESTRUCTION WARLOCK REFACTORED
// ============================================================================

class DestructionWarlockRefactored : public RangedDpsSpecialization<ManaSoulShardResourceDestro>, public WarlockSpecialization
{
public:
    // Use base class members with type alias for cleaner syntax
    using Base = RangedDpsSpecialization<ManaSoulShardResourceDestro>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit DestructionWarlockRefactored(Player* bot)        : RangedDpsSpecialization<ManaSoulShardResourceDestro>(bot)
        , WarlockSpecialization(bot)
        , _immolateTracker()
        , _havocTracker()
        , _backdraftStacks(0)
        , _lastInfernalTime(0)
    {        // Initialize mana/soul shard resources
        this->_resource.Initialize(bot);
        TC_LOG_DEBUG("playerbot", "DestructionWarlockRefactored initialized for {}", bot->GetName());

        // Phase 5: Initialize decision systems
        InitializeDestructionMechanics();
    }

    void UpdateRotation(::Unit* target) override    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))
            return;

        // Update Destruction state
        UpdateDestructionState();

        // Ensure pet is active
        EnsurePetActive();

        // Determine if AoE or single target
        uint32 enemyCount = this->GetEnemiesInRange(40.0f);
        if (enemyCount >= 3)
        {
            ExecuteAoERotation(target, enemyCount);
        }
        else if (enemyCount == 2)
        {
            ExecuteCleaveRotation(target);
        }
        else
        {
            ExecuteSingleTargetRotation(target);        }
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Defensive cooldowns
        HandleDefensiveCooldowns();
    }

    void ExecuteSingleTargetRotation(::Unit* target)
    {
        ObjectGuid targetGuid = target->GetGUID();        uint32 shards = this->_resource.soulShards;
        float targetHpPct = target->GetHealthPct();

        // Priority 1: Use Summon Infernal (major CD)
        if (shards >= 2 && this->CanCastSpell(SUMMON_INFERNAL, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), SUMMON_INFERNAL);
            _lastInfernalTime = GameTime::GetGameTimeMS();
            

        // Register cooldowns using CooldownManager
        _cooldowns.RegisterBatch({
            {CONFLAGRATE, 13000, 1},
            {SUMMON_INFERNAL, CooldownPresets::MAJOR_OFFENSIVE, 1},
            {DARK_SOUL_INSTABILITY, CooldownPresets::MINOR_OFFENSIVE, 1},
            {SOUL_FIRE, 20000, 1},
            {CATACLYSM, CooldownPresets::OFFENSIVE_30, 1},
            {HAVOC, CooldownPresets::OFFENSIVE_30, 1},
            {SHADOWBURN, 12000, 1},
            {UNENDING_RESOLVE_DESTRO, CooldownPresets::MAJOR_OFFENSIVE, 1},
            {DARK_PACT_DESTRO, CooldownPresets::OFFENSIVE_60, 1},
            {MORTAL_COIL_DESTRO, CooldownPresets::OFFENSIVE_45, 1},
            {HOWL_OF_TERROR_DESTRO, 40000, 1},
        });

        TC_LOG_DEBUG("playerbot", "Destruction: Summon Infernal");
            // Continue rotation during Infernal
        }

        // Priority 2: Dark Soul: Instability (burst CD)
        if (this->CanCastSpell(DARK_SOUL_INSTABILITY, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), DARK_SOUL_INSTABILITY);
            TC_LOG_DEBUG("playerbot", "Destruction: Dark Soul Instability");
        }

        // Priority 3: Maintain Immolate
        if (_immolateTracker.NeedsRefresh(targetGuid))
        {
            if (this->CanCastSpell(IMMOLATE, target))
            {
                this->CastSpell(target, IMMOLATE);
                _immolateTracker.ApplyImmolate(targetGuid, 18000); // 18 sec duration
                return;
            }
        }

        // Priority 4: Conflagrate (generate shards + Backdraft)
        if (this->CanCastSpell(CONFLAGRATE, target))
        {
            this->CastSpell(target, CONFLAGRATE);
            GenerateSoulShard(1);
            _backdraftStacks = std::min(_backdraftStacks + 2, 4u); // Grants 2 stacks
            return;
        }

        // Priority 5: Soul Fire (talent, strong direct damage)
        if (this->CanCastSpell(SOUL_FIRE, target))
        {
            this->CastSpell(target, SOUL_FIRE);
            return;
        }

        // Priority 6: Chaos Bolt (shard spender)
        if (shards >= 2 && this->CanCastSpell(CHAOS_BOLT, target))        {
            this->CastSpell(target, CHAOS_BOLT);
            ConsumeSoulShard(2);
            return;
        }

        // Priority 7: Channel Demonfire (talent, requires Immolate)
        if (_immolateTracker.HasImmolate(targetGuid) && this->CanCastSpell(CHANNEL_DEMONFIRE, target))
        {
            this->CastSpell(target, CHANNEL_DEMONFIRE);
            return;
        }

        // Priority 8: Shadowburn (execute < 20%)
        if (targetHpPct < 20.0f && this->CanCastSpell(SHADOWBURN, target))
        {
            this->CastSpell(target, SHADOWBURN);
            GenerateSoulShard(1);
            return;
        }        // Priority 9: Incinerate (filler + shard gen)
        if (shards < 5 && this->CanCastSpell(INCINERATE, target))
        {
            this->CastSpell(target, INCINERATE);
            GenerateSoulShard(1);
            if (_backdraftStacks > 0)
                _backdraftStacks--;
            return;
        }
    }

    void ExecuteCleaveRotation(::Unit* target)
    {
        ObjectGuid targetGuid = target->GetGUID();        uint32 shards = this->_resource.soulShards;

        // Priority 1: Havoc on secondary target
        if (!_havocTracker.IsActive() && this->CanCastSpell(HAVOC, target))
        {
            // Find best secondary target for Havoc (not the primary target)
            ::Unit* secondaryTarget = FindBestHavocTarget(target);
            if (secondaryTarget)
            {
                if (this->CastSpell(secondaryTarget, HAVOC))
                {
                    _havocTracker.ApplyHavoc(secondaryTarget->GetGUID());
                    TC_LOG_DEBUG("playerbot", "Destruction: Havoc applied to {} (secondary target, primary: {})",
                                 secondaryTarget->GetName(), target->GetName());
                }
            }
        }

        // Priority 2: Maintain Immolate on primary
        if (_immolateTracker.NeedsRefresh(targetGuid))
        {
            if (this->CanCastSpell(IMMOLATE, target))
            {
                this->CastSpell(target, IMMOLATE);
                _immolateTracker.ApplyImmolate(targetGuid, 18000);
                return;
            }
        }

        // Priority 3: Conflagrate
        if (this->CanCastSpell(CONFLAGRATE, target))
        {
            this->CastSpell(target, CONFLAGRATE);
            GenerateSoulShard(1);
            _backdraftStacks = std::min(_backdraftStacks + 2, 4u);
            return;
        }

        // Priority 4: Chaos Bolt (cleaves with Havoc)
        if (shards >= 2 && this->CanCastSpell(CHAOS_BOLT, target))
        {
            this->CastSpell(target, CHAOS_BOLT);
            ConsumeSoulShard(2);
            return;
        }

        // Priority 5: Incinerate filler
        if (shards < 5 && this->CanCastSpell(INCINERATE, target))
        {
            this->CastSpell(target, INCINERATE);
            GenerateSoulShard(1);
            if (_backdraftStacks > 0)
                _backdraftStacks--;
            return;
        }
    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)    {
        uint32 shards = this->_resource.soulShards;

        // Priority 1: Summon Infernal
        if (shards >= 2 && this->CanCastSpell(SUMMON_INFERNAL, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), SUMMON_INFERNAL);
            _lastInfernalTime = GameTime::GetGameTimeMS();
            return;
        }

        // Priority 2: Cataclysm (AoE + applies Immolate)
        if (this->CanCastSpell(CATACLYSM, target))
        {
            this->CastSpell(target, CATACLYSM);
            TC_LOG_DEBUG("playerbot", "Destruction: Cataclysm");
            return;
        }

        // Priority 3: Rain of Fire (AoE shard spender)
        if (shards >= 3 && this->CanCastSpell(RAIN_OF_FIRE, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), RAIN_OF_FIRE);
            ConsumeSoulShard(3);
            return;
        }

        // Priority 4: Channel Demonfire (if targets have Immolate)
        if (this->CanCastSpell(CHANNEL_DEMONFIRE, target))
        {
            this->CastSpell(target, CHANNEL_DEMONFIRE);
            return;
        }

        // Priority 5: Havoc on secondary target
        if (!_havocTracker.IsActive() && this->CanCastSpell(HAVOC, target))
        {
            this->CastSpell(target, HAVOC);            _havocTracker.ApplyHavoc(target->GetGUID());
            return;
        }

        // Priority 6: Conflagrate
        if (this->CanCastSpell(CONFLAGRATE, target))
        {
            this->CastSpell(target, CONFLAGRATE);
            GenerateSoulShard(1);
            return;
        }

        // Priority 7: Incinerate filler
        if (shards < 5 && this->CanCastSpell(INCINERATE, target))
        {
            this->CastSpell(target, INCINERATE);
            GenerateSoulShard(1);
            return;
        }
    }

    void HandleDefensiveCooldowns()
    {
        Player* bot = this->GetBot();
        float healthPct = bot->GetHealthPct();

        // Unending Resolve
        if (healthPct < 40.0f && this->CanCastSpell(UNENDING_RESOLVE_DESTRO, bot))
        {
            this->CastSpell(bot, UNENDING_RESOLVE_DESTRO);
            TC_LOG_DEBUG("playerbot", "Destruction: Unending Resolve");
            return;
        }

        // Dark Pact
        if (healthPct < 50.0f && this->CanCastSpell(DARK_PACT_DESTRO, bot))
        {
            this->CastSpell(bot, DARK_PACT_DESTRO);
            TC_LOG_DEBUG("playerbot", "Destruction: Dark Pact");
            return;
        }

        // Mortal Coil
        if (healthPct < 60.0f && this->CanCastSpell(MORTAL_COIL_DESTRO, bot))
        {
            this->CastSpell(bot, MORTAL_COIL_DESTRO);
            TC_LOG_DEBUG("playerbot", "Destruction: Mortal Coil");
            return;
        }
    }    void EnsurePetActive()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Check if pet exists
        if (Pet* pet = bot->GetPet())
        {
            if (pet->IsAlive())
                return; // Pet is active
        }

        // Summon Imp (best for Destruction - ranged DPS)
        if (this->CanCastSpell(SUMMON_IMP_DESTRO, bot))
        {
            this->CastSpell(bot, SUMMON_IMP_DESTRO);
            TC_LOG_DEBUG("playerbot", "Destruction: Summon Imp");
        }
    }

private:
    void UpdateDestructionState()
    {
        // Update Immolate tracker
        _immolateTracker.Update();

        // Update Havoc tracker
        _havocTracker.Update();

        // Update Backdraft stacks
        if (this->GetBot())
        {
            if (Aura* aura = this->GetBot()->GetAura(BACKDRAFT_BUFF))
                _backdraftStacks = aura->GetStackAmount();            else
                _backdraftStacks = 0;
        }

        // Update soul shards from bot
        if (this->GetBot())
        {
            this->_resource.soulShards = this->GetBot()->GetPower(POWER_SOUL_SHARDS);
            this->_resource.mana = this->GetBot()->GetPower(POWER_MANA);
        }
    }

    void GenerateSoulShard(uint32 amount)
    {
        this->_resource.soulShards = std::min(this->_resource.soulShards + amount, this->_resource.maxSoulShards);
    }

    void ConsumeSoulShard(uint32 amount)
    {
        this->_resource.soulShards = (this->_resource.soulShards > amount) ? this->_resource.soulShards - amount : 0;
    }

    ::Unit* FindBestHavocTarget(::Unit* primaryTarget)
    {
        if (!primaryTarget || !this->GetBot())
            return nullptr;

        Player* bot = this->GetBot();
        ObjectGuid primaryGuid = primaryTarget->GetGUID();

        // Get all nearby enemies within 40 yards
        std::list<::Unit*> nearbyEnemies;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(bot, bot, 40.0f);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, nearbyEnemies, u_check);
        Cell::VisitAllObjects(bot, searcher, 40.0f);

        ::Unit* bestTarget = nullptr;
        float bestScore = 0.0f;

        for (::Unit* enemy : nearbyEnemies)
        {
            // Skip invalid targets, dead targets, or the primary target
            if (!enemy || !enemy->IsAlive() || !bot->IsValidAttackTarget(enemy))
                continue;

            if (enemy->GetGUID() == primaryGuid)
                continue; // Don't Havoc the primary target

            // Calculate target priority score
            float score = 100.0f;

            // Prefer targets with high health (long-lived targets)
            float healthPct = enemy->GetHealthPct();
            if (healthPct > 80.0f)
                score += 50.0f;
            else if (healthPct > 50.0f)
                score += 30.0f;
            else if (healthPct < 20.0f)
                score -= 20.0f; // Deprioritize targets about to die

            // Prefer targets close to the primary target (for cleave efficiency)
            float distanceToPrimary = enemy->GetDistance(primaryTarget);
            if (distanceToPrimary < 10.0f)
                score += 40.0f;
            else if (distanceToPrimary < 20.0f)
                score += 20.0f;
            else if (distanceToPrimary > 30.0f)
                score -= 30.0f; // Deprioritize far targets

            // Prefer targets without Havoc already applied
            if (_havocTracker.GetHavocTarget() == enemy->GetGUID())
                score -= 100.0f;

            // Prefer elite/boss targets over normal mobs
            if (enemy->GetTypeId() == TYPEID_UNIT)
            {
                Creature* creature = enemy->ToCreature();
                if (creature)
                {
                    if (creature->IsWorldBoss() || creature->IsDungeonBoss())
                        score += 100.0f; // Highest priority for bosses
                    else if (creature->GetCreatureTemplate()->rank >= CREATURE_ELITE_ELITE)
                        score += 50.0f; // High priority for elites
                }
            }

            // Update best target if this one has a higher score
            if (score > bestScore)
            {
                bestScore = score;
                bestTarget = enemy;
            }
        }

        if (bestTarget)
        {
            TC_LOG_DEBUG("playerbot", "Destruction: Found best Havoc target: {} (score: {:.1f})",
                         bestTarget->GetName(), bestScore);
        }
        else
        {
            TC_LOG_DEBUG("playerbot", "Destruction: No suitable Havoc target found");
        }

        return bestTarget;
    }

    void InitializeDestructionMechanics()
    {
        using namespace bot::ai;
        using namespace bot::ai::BehaviorTreeBuilder;

        BotAI* ai = this->GetBot()->GetBotAI();
        if (!ai) return;

        auto* queue = ai->GetActionPriorityQueue();
        if (queue)
        {
            // EMERGENCY: Defensive cooldowns
            queue->RegisterSpell(UNENDING_RESOLVE_DESTRO, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);
            queue->AddCondition(UNENDING_RESOLVE_DESTRO, [this](Player* bot, Unit*) {
                return bot && bot->GetHealthPct() < 40.0f;
            }, "HP < 40% (damage reduction)");

            // CRITICAL: Major burst cooldown - Summon Infernal
            queue->RegisterSpell(SUMMON_INFERNAL, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);
            queue->AddCondition(SUMMON_INFERNAL, [this](Player*, Unit*) {
                return this->_resource.soulShards >= 2;
            }, "Major CD (3min, Infernal)");

            // CRITICAL: Dark Soul: Instability
            queue->RegisterSpell(DARK_SOUL_INSTABILITY, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);
            queue->AddCondition(DARK_SOUL_INSTABILITY, [this](Player* bot, Unit*) {
                return bot->HasSpell(DARK_SOUL_INSTABILITY);
            }, "Burst CD (2min, crit buff)");

            // HIGH: Maintain Immolate
            queue->RegisterSpell(IMMOLATE, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(IMMOLATE, [this](Player*, Unit* target) {
                return target && this->_immolateTracker.NeedsRefresh(target->GetGUID());
            }, "Refresh Immolate (enables Conflagrate)");

            // HIGH: Conflagrate (shard generation + Backdraft)
            queue->RegisterSpell(CONFLAGRATE, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(CONFLAGRATE, [](Player*, Unit* target) {
                return target != nullptr;
            }, "Shard gen + Backdraft (2 stacks)");

            // HIGH: Soul Fire (talent, strong damage)
            queue->RegisterSpell(SOUL_FIRE, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(SOUL_FIRE, [this](Player* bot, Unit* target) {
                return target && bot->HasSpell(SOUL_FIRE);
            }, "Strong direct damage (20s CD, talent)");

            // MEDIUM: Chaos Bolt (shard spender)
            queue->RegisterSpell(CHAOS_BOLT, SpellPriority::MEDIUM, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(CHAOS_BOLT, [this](Player*, Unit* target) {
                return target && this->_resource.soulShards >= 2;
            }, "2 shards (heavy damage)");

            // MEDIUM: Rain of Fire (AoE shard spender)
            queue->RegisterSpell(RAIN_OF_FIRE, SpellPriority::MEDIUM, SpellCategory::DAMAGE_AOE);
            queue->AddCondition(RAIN_OF_FIRE, [this](Player*, Unit*) {
                return this->_resource.soulShards >= 3 && this->GetEnemiesInRange(40.0f) >= 3;
            }, "3 shards, 3+ enemies (AoE)");

            // MEDIUM: Havoc (cleave on 2nd target)
            queue->RegisterSpell(HAVOC, SpellPriority::MEDIUM, SpellCategory::UTILITY);
            queue->AddCondition(HAVOC, [this](Player*, Unit* target) {
                return target && !this->_havocTracker.IsActive() && this->GetEnemiesInRange(40.0f) >= 2;
            }, "2+ enemies (cleave to 2nd target)");

            // MEDIUM: Cataclysm (AoE + applies Immolate)
            queue->RegisterSpell(CATACLYSM, SpellPriority::MEDIUM, SpellCategory::DAMAGE_AOE);
            queue->AddCondition(CATACLYSM, [this](Player* bot, Unit* target) {
                return target && bot->HasSpell(CATACLYSM) && this->GetEnemiesInRange(40.0f) >= 3;
            }, "3+ enemies (AoE + Immolate, 30s CD)");

            // MEDIUM: Channel Demonfire (requires Immolate)
            queue->RegisterSpell(CHANNEL_DEMONFIRE, SpellPriority::MEDIUM, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(CHANNEL_DEMONFIRE, [this](Player* bot, Unit* target) {
                return target && bot->HasSpell(CHANNEL_DEMONFIRE) &&
                       this->_immolateTracker.HasImmolate(target->GetGUID());
            }, "Requires Immolate (channeled, talent)");

            // MEDIUM: Shadowburn (execute)
            queue->RegisterSpell(SHADOWBURN, SpellPriority::MEDIUM, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(SHADOWBURN, [this](Player* bot, Unit* target) {
                return target && bot->HasSpell(SHADOWBURN) && target->GetHealthPct() < 20.0f;
            }, "Execute < 20% (generates shard)");

            // LOW: Incinerate (filler + shard generator)
            queue->RegisterSpell(INCINERATE, SpellPriority::LOW, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(INCINERATE, [this](Player*, Unit* target) {
                return target && this->_resource.soulShards < 5;
            }, "Filler (generates shards, Backdraft)");
        }

        auto* behaviorTree = ai->GetBehaviorTree();
        if (behaviorTree)
        {
            auto root = Selector("Destruction Warlock DPS", {
                // Tier 1: Burst Cooldowns (Summon Infernal, Dark Soul)
                Sequence("Burst Cooldowns", {
                    Condition("Has shards and target", [this](Player* bot) {
                        return bot && bot->GetVictim() && this->_resource.soulShards >= 2;
                    }),
                    Selector("Use burst cooldowns", {
                        Sequence("Summon Infernal", {
                            Condition("Can summon Infernal", [this](Player* bot) {
                                return this->CanCastSpell(SUMMON_INFERNAL, bot);
                            }),
                            Action("Cast Summon Infernal", [this](Player* bot) {
                                this->CastSpell(bot, SUMMON_INFERNAL);
                                return NodeStatus::SUCCESS;
                            })
                        }),
                        Sequence("Dark Soul: Instability", {
                            Condition("Has Dark Soul talent", [this](Player* bot) {
                                return bot->HasSpell(DARK_SOUL_INSTABILITY);
                            }),
                            Action("Cast Dark Soul", [this](Player* bot) {
                                if (this->CanCastSpell(DARK_SOUL_INSTABILITY, bot))
                                {
                                    this->CastSpell(bot, DARK_SOUL_INSTABILITY);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 2: DoT Maintenance & Shard Generation (Immolate, Conflagrate)
                Sequence("DoT & Shard Gen", {
                    Condition("Has target", [this](Player* bot) {
                        return bot && bot->GetVictim();
                    }),
                    Selector("Maintain DoT and generate shards", {
                        Sequence("Immolate", {
                            Condition("Needs Immolate", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                return target && this->_immolateTracker.NeedsRefresh(target->GetGUID());
                            }),
                            Action("Cast Immolate", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(IMMOLATE, target))
                                {
                                    this->CastSpell(target, IMMOLATE);
                                    this->_immolateTracker.ApplyImmolate(target->GetGUID(), 18000);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Conflagrate", {
                            Condition("Can cast Conflagrate", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                return target && this->CanCastSpell(CONFLAGRATE, target);
                            }),
                            Action("Cast Conflagrate", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                this->CastSpell(target, CONFLAGRATE);
                                this->GenerateSoulShard(1);
                                this->_backdraftStacks = std::min(this->_backdraftStacks + 2, 4u);
                                return NodeStatus::SUCCESS;
                            })
                        })
                    })
                }),

                // Tier 3: Shard Spender (Chaos Bolt, Rain of Fire)
                Sequence("Shard Spender", {
                    Condition("Has 2+ shards and target", [this](Player* bot) {
                        return bot && bot->GetVictim() && this->_resource.soulShards >= 2;
                    }),
                    Selector("Spend shards", {
                        Sequence("Rain of Fire (AoE)", {
                            Condition("3+ enemies and 3+ shards", [this](Player*) {
                                return this->_resource.soulShards >= 3 && this->GetEnemiesInRange(40.0f) >= 3;
                            }),
                            Action("Cast Rain of Fire", [this](Player* bot) {
                                if (this->CanCastSpell(RAIN_OF_FIRE, bot))
                                {
                                    this->CastSpell(bot, RAIN_OF_FIRE);
                                    this->ConsumeSoulShard(3);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Chaos Bolt (single target)", {
                            Condition("2+ shards", [this](Player*) {
                                return this->_resource.soulShards >= 2;
                            }),
                            Action("Cast Chaos Bolt", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(CHAOS_BOLT, target))
                                {
                                    this->CastSpell(target, CHAOS_BOLT);
                                    this->ConsumeSoulShard(2);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        })
                    })
                }),

                // Tier 4: Shard Generator (Incinerate filler)
                Sequence("Shard Generator", {
                    Condition("Has target and < 5 shards", [this](Player* bot) {
                        return bot && bot->GetVictim() && this->_resource.soulShards < 5;
                    }),
                    Selector("Generate shards", {
                        Sequence("Shadowburn (execute)", {
                            Condition("Target < 20% HP and has spell", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                return target && bot->HasSpell(SHADOWBURN) && target->GetHealthPct() < 20.0f;
                            }),
                            Action("Cast Shadowburn", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(SHADOWBURN, target))
                                {
                                    this->CastSpell(target, SHADOWBURN);
                                    this->GenerateSoulShard(1);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),
                        Sequence("Incinerate (filler)", {
                            Action("Cast Incinerate", [this](Player* bot) {
                                Unit* target = bot->GetVictim();
                                if (target && this->CanCastSpell(INCINERATE, target))
                                {
                                    this->CastSpell(target, INCINERATE);
                                    this->GenerateSoulShard(1);
                                    if (this->_backdraftStacks > 0)
                                        this->_backdraftStacks--;
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
    DestructionImmolateTracker _immolateTracker;
    DestructionHavocTracker _havocTracker;
    uint32 _backdraftStacks;
    uint32 _lastInfernalTime;
};

} // namespace Playerbot
