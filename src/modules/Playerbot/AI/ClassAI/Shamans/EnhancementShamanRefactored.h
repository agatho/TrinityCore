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

#ifndef PLAYERBOT_ENHANCEMENTSHAMANREFACTORED_H
#define PLAYERBOT_ENHANCEMENTSHAMANREFACTORED_H

#include "../CombatSpecializationTemplates.h"
#include "Player.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include <unordered_map>
#include "Log.h"
// Phase 5 Integration
#include "../Decision/ActionPriorityQueue.h"
#include "../Decision/BehaviorTree.h"
#include "../BotAI.h"

namespace Playerbot
{

// WoW 11.2 (The War Within) - Enhancement Shaman Spell IDs
constexpr uint32 ENH_ROCKBITER = 193786;
constexpr uint32 ENH_STORMSTRIKE = 17364;
constexpr uint32 ENH_LAVA_LASH = 60103;
constexpr uint32 ENH_LIGHTNING_BOLT = 188196;
constexpr uint32 ENH_FLAME_SHOCK = 188389;
constexpr uint32 ENH_FROST_SHOCK = 196840;
constexpr uint32 ENH_CRASH_LIGHTNING = 187874;
constexpr uint32 ENH_SUNDERING = 197214;
constexpr uint32 ENH_FERAL_SPIRIT = 51533;
constexpr uint32 ENH_ASCENDANCE = 114051;
constexpr uint32 ENH_WINDFURY_TOTEM = 8512;
constexpr uint32 ENH_WINDSTRIKE = 115356;
constexpr uint32 ENH_ICE_STRIKE = 342240;
constexpr uint32 ENH_FIRE_NOVA = 333974;
constexpr uint32 ENH_ELEMENTAL_BLAST = 117014;
constexpr uint32 ENH_LAVA_BURST = 51505;
constexpr uint32 ENH_ASTRAL_SHIFT = 108271;
constexpr uint32 ENH_EARTH_SHIELD = 974;
constexpr uint32 ENH_WIND_SHEAR = 57994;
constexpr uint32 ENH_CAPACITOR_TOTEM = 192058;

// ManaResource is already defined in CombatSpecializationTemplates.h
// No need to redefine it here

// Maelstrom Weapon stack tracker (5 stacks = instant cast spells)
class MaelstromWeaponTracker
{
public:
    MaelstromWeaponTracker() : _maelstromStacks(0), _maelstromEndTime(0) {}

    void AddStack(uint32 amount = 1)
    {
        _maelstromStacks = std::min(_maelstromStacks + amount, 5u); // Max 5 stacks
        _maelstromEndTime = GameTime::GetGameTimeMS() + 30000; // 30 sec duration
    }

    void ConsumeStacks()
    {
        _maelstromStacks = 0;
    }

    [[nodiscard]] uint32 GetStacks() const { return _maelstromStacks; }
    [[nodiscard]] bool IsMaxStacks() const { return _maelstromStacks >= 5; }
    [[nodiscard]] bool HasStacks(uint32 amount) const { return _maelstromStacks >= amount; }

    void Update(Player* bot)
    {
        if (!bot)

            return;

        // Check if Maelstrom Weapon buff is active
        if (Aura* aura = bot->GetAura(187880)) // Maelstrom Weapon buff ID
        {

            _maelstromStacks = aura->GetStackAmount();
            _maelstromEndTime = GameTime::GetGameTimeMS() + aura->GetDuration();
            }
        else
        {

            _maelstromStacks = 0;
        }

        // Expire if time is up
        if (_maelstromStacks > 0 && GameTime::GetGameTimeMS() >= _maelstromEndTime)

            _maelstromStacks = 0;
    }

private:
    CooldownManager _cooldowns;
    uint32 _maelstromStacks;
    uint32 _maelstromEndTime;
};

// Stormbringer proc tracker (instant Stormstrike)
class StormbringerTracker
{
public:
    StormbringerTracker() : _stormbringerActive(false), _stormbringerEndTime(0) {}

    void ActivateProc()
    {
        _stormbringerActive = true;
        _stormbringerEndTime = GameTime::GetGameTimeMS() + 12000; // 12 sec duration
    }

    void ConsumeProc()
    {
        _stormbringerActive = false;
    }

    [[nodiscard]] bool IsActive() const
    {
        return _stormbringerActive && GameTime::GetGameTimeMS() < _stormbringerEndTime;
    }

    void Update(Player* bot)
    {
        if (!bot)

            return;

        // Check if Stormbringer buff is active
        if (bot->HasAura(201846)) // Stormbringer buff ID
        {

            _stormbringerActive = true;

            if (Aura* aura = bot->GetAura(201846))

                _stormbringerEndTime = GameTime::GetGameTimeMS() + aura->GetDuration();
                }
        else
        {

            _stormbringerActive = false;
        }

        // Expire if time is up
        if (_stormbringerActive && GameTime::GetGameTimeMS() >= _stormbringerEndTime)

            _stormbringerActive = false;
    }

private:
    bool _stormbringerActive;
    uint32 _stormbringerEndTime;
};

class EnhancementShamanRefactored : public MeleeDpsSpecialization<ManaResource>, public ShamanSpecialization
{
public:
    using Base = MeleeDpsSpecialization<ManaResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit EnhancementShamanRefactored(Player* bot)        : MeleeDpsSpecialization<ManaResource>(bot)
        , ShamanSpecialization(bot)
        , _maelstromWeaponTracker()
        , _stormbringerTracker()
        , _ascendanceActive(false)
        , _ascendanceEndTime(0)
        
        , _lastSunderingTime(0)
        , _cooldowns()
    {
        // Register cooldowns for major abilities
        _cooldowns.RegisterBatch({

            {ENHANCEMENT_FERAL_SPIRIT, 120000, 1},

            {ENHANCEMENT_DOOM_WINDS, 60000, 1},

            {ENHANCEMENT_ASCENDANCE, 180000, 1},

            {ENHANCEMENT_STORMSTRIKE, 9000, 2}
        });

        // Resource initialization handled by base class CombatSpecializationTemplate        TC_LOG_DEBUG("playerbot", "EnhancementShamanRefactored initialized for {}", bot->GetName());
        InitializeEnhancementMechanics();
    }

    void UpdateRotation(::Unit* target) override
    {
        if (!target || !bot)

            return;

        UpdateEnhancementState();

        uint32 enemyCount = this->GetEnemiesInRange(8.0f);

        if (enemyCount >= 3)

            ExecuteAoERotation(target, enemyCount);
        else

            ExecuteSingleTargetRotation(target);
    }

    void UpdateBuffs() override
    {
        if (!bot)

            return;

        // Windfury Totem (group melee haste buff)
        if (!bot->HasAura(ENH_WINDFURY_TOTEM))
        {

            if (this->CanCastSpell(ENH_WINDFURY_TOTEM, bot))

            {

                this->CastSpell(bot, ENH_WINDFURY_TOTEM);

            }
        }

        // Earth Shield (self-protection)
        if (!bot->HasAura(ENH_EARTH_SHIELD))
        {

            if (this->CanCastSpell(ENH_EARTH_SHIELD, bot))

            {

                this->CastSpell(bot, ENH_EARTH_SHIELD);

            }
        }
    }

    void UpdateDefensives()
    {
        if (!bot)

            return;

        float healthPct = bot->GetHealthPct();

        // Astral Shift (damage reduction)
        if (healthPct < 40.0f && this->CanCastSpell(ENH_ASTRAL_SHIFT, bot))
        {

            this->CastSpell(bot, ENH_ASTRAL_SHIFT);

            return;
        }

        // Capacitor Totem (AoE stun for escape)
        if (healthPct < 50.0f && bot->GetThreatManager().GetThreatListSize() >= 2)
        {

            if (this->CanCastSpell(ENH_CAPACITOR_TOTEM, bot))

            {

                this->CastSpell(bot, ENH_CAPACITOR_TOTEM);

                return;

            }
        }
    }

private:
    

    void UpdateEnhancementState()
    {
        // ManaResource (uint32) doesn't have Update method - handled by base class
        _maelstromWeaponTracker.Update(bot);
        _stormbringerTracker.Update(bot);
        UpdateCooldownStates();
    }

    void UpdateCooldownStates()
    {
        // Ascendance state (transforms into Air Ascendant)
        if (_ascendanceActive && GameTime::GetGameTimeMS() >= _ascendanceEndTime)

            _ascendanceActive = false;

        if (bot->HasAura(ENH_ASCENDANCE))
        {

            _ascendanceActive = true;

            if (Aura* aura = bot->GetAura(ENH_ASCENDANCE))
            _ascendanceEndTime = GameTime::GetGameTimeMS() + aura->GetDuration();
            }
    }

    void ExecuteSingleTargetRotation(::Unit* target)
    {
        uint32 maelstromStacks = _maelstromWeaponTracker.GetStacks();

        // Feral Spirit (major DPS cooldown - summon wolves)
        if ((GameTime::GetGameTimeMS() - _lastFeralSpiritTime) >= 120000) // 2 min CD
        {

            if (this->CanCastSpell(ENH_FERAL_SPIRIT, bot))

            {

                this->CastSpell(bot, ENH_FERAL_SPIRIT);

                _lastFeralSpiritTime = GameTime::GetGameTimeMS();

                return;

            }
        }

        // Ascendance (burst mode - Stormstrike becomes Windstrike)
        if ((GameTime::GetGameTimeMS() - _lastAscendanceTime) >= 180000) // 3 min CD

        {
        if (bot->HasSpell(ENH_ASCENDANCE))

            {

                if (this->CanCastSpell(ENH_ASCENDANCE, bot))

                {

                    this->CastSpell(bot, ENH_ASCENDANCE);

                    _ascendanceActive = true;

                    _ascendanceEndTime = GameTime::GetGameTimeMS() + 15000;

                    _lastAscendanceTime = GameTime::GetGameTimeMS();

                    return;
                    }

            }
        }

        // Windstrike (Ascendance version of Stormstrike)
        if (_ascendanceActive)
        {

            if (this->CanCastSpell(ENH_WINDSTRIKE, target))

            {

                this->CastSpell(target, ENH_WINDSTRIKE);

                _maelstromWeaponTracker.AddStack(1);

                return;

            }
        }

        // Stormstrike with Stormbringer proc (no cooldown)
        if (_stormbringerTracker.IsActive())
        {

            if (this->CanCastSpell(ENH_STORMSTRIKE, target))

            {

                this->CastSpell(target, ENH_STORMSTRIKE);

                _stormbringerTracker.ConsumeProc();

                _maelstromWeaponTracker.AddStack(1);
                return;

            }
        }

        // Lava Burst at 5 Maelstrom Weapon stacks (instant cast, high damage)
        if (_maelstromWeaponTracker.IsMaxStacks())
        {
            

            if (bot->HasSpell(ENH_LAVA_BURST))

            {

                if (this->CanCastSpell(ENH_LAVA_BURST, target))

                {

                    this->CastSpell(target, ENH_LAVA_BURST);

                    _maelstromWeaponTracker.ConsumeStacks();

                    return;

                }

            }
            }

        // Lightning Bolt at 5+ Maelstrom Weapon stacks (instant cast)
        if (maelstromStacks >= 5)
        {

            if (this->CanCastSpell(ENH_LIGHTNING_BOLT, target))

            {

                this->CastSpell(target, ENH_LIGHTNING_BOLT);

                _maelstromWeaponTracker.ConsumeStacks();

                return;

            }
        }

        // Elemental Blast at 5 stacks (talent)
        if (maelstromStacks >= 5 && bot->HasSpell(ENH_ELEMENTAL_BLAST))        {

            if (this->CanCastSpell(ENH_ELEMENTAL_BLAST, target))

            {

                this->CastSpell(target, ENH_ELEMENTAL_BLAST);

                _maelstromWeaponTracker.ConsumeStacks();

                return;

            }
        }

        // Flame Shock (maintain DoT)        if (!target->HasAura(ENH_FLAME_SHOCK))
        {

            if (this->CanCastSpell(ENH_FLAME_SHOCK, target))

            {

                this->CastSpell(target, ENH_FLAME_SHOCK);

                return;

            }
        }

        // Ice Strike (talent - high damage, generates Maelstrom Weapon)        if (bot->HasSpell(ENH_ICE_STRIKE))
        {

            if (this->CanCastSpell(ENH_ICE_STRIKE, target))

            {

                this->CastSpell(target, ENH_ICE_STRIKE);

                _maelstromWeaponTracker.AddStack(1);

                return;
                }
        }

        // Stormstrike (main melee attack)
        if (this->CanCastSpell(ENH_STORMSTRIKE, target))
        {

            this->CastSpell(target, ENH_STORMSTRIKE);

            _maelstromWeaponTracker.AddStack(1);

            // Chance to proc Stormbringer

            if (rand() % 100 < 10) // 10% chance (simplified)

                _stormbringerTracker.ActivateProc();


            return;
            }

        // Lava Lash (filler - consumes Flame Shock for extra damage)
        if (this->CanCastSpell(ENH_LAVA_LASH, target))
        {

            this->CastSpell(target, ENH_LAVA_LASH);

            _maelstromWeaponTracker.AddStack(1);

            return;
        }

        // Rockbiter (builder - low priority)
        if (this->CanCastSpell(ENH_ROCKBITER, target))
        {

            this->CastSpell(target, ENH_ROCKBITER);
            return;
        }    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)    {
        uint32 maelstromStacks = _maelstromWeaponTracker.GetStacks();

        // Feral Spirit for AoE burst
        if ((GameTime::GetGameTimeMS() - _lastFeralSpiritTime) >= 120000 && enemyCount >= 4)
        {

            if (this->CanCastSpell(ENH_FERAL_SPIRIT, bot))

            {

                this->CastSpell(bot, ENH_FERAL_SPIRIT);

                _lastFeralSpiritTime = GameTime::GetGameTimeMS();

                return;

            }
        }

        // Ascendance for AoE burst
        if ((GameTime::GetGameTimeMS() - _lastAscendanceTime) >= 180000 && enemyCount >= 5)

        {
        if (bot->HasSpell(ENH_ASCENDANCE))

            {

                if (this->CanCastSpell(ENH_ASCENDANCE, bot))

                {

                    this->CastSpell(bot, ENH_ASCENDANCE);

                    _ascendanceActive = true;

                    _ascendanceEndTime = GameTime::GetGameTimeMS() + 15000;

                    _lastAscendanceTime = GameTime::GetGameTimeMS();

                    return;

                }

            }
        }

        // Sundering (AoE damage + debuff)
        if (bot->HasSpell(ENH_SUNDERING) && enemyCount >= 3)        {

            if ((GameTime::GetGameTimeMS() - _lastSunderingTime) >= 40000) // 40 sec CD

            {

                if (this->CanCastSpell(ENH_SUNDERING, target))

                {

                    this->CastSpell(target, ENH_SUNDERING);

                    _lastSunderingTime = GameTime::GetGameTimeMS();

                    return;

                }

            }
        }

        // Fire Nova (AoE explosion from Flame Shock targets)

        if (bot->HasSpell(ENH_FIRE_NOVA) && enemyCount >= 3)
        {
        if (target->HasAura(ENH_FLAME_SHOCK))

            {

                if (this->CanCastSpell(ENH_FIRE_NOVA, bot))

                {

                    this->CastSpell(bot, ENH_FIRE_NOVA);

                    return;

                }

            }
        }

        // Flame Shock on priority target        if (!target->HasAura(ENH_FLAME_SHOCK))
        {

            if (this->CanCastSpell(ENH_FLAME_SHOCK, target))

            {

                this->CastSpell(target, ENH_FLAME_SHOCK);

                return;

            }
        }

        // Crash Lightning (AoE cleave enabler)
        if (enemyCount >= 2)
        {

            if (this->CanCastSpell(ENH_CRASH_LIGHTNING, bot))

            {

                this->CastSpell(bot, ENH_CRASH_LIGHTNING);

                _maelstromWeaponTracker.AddStack(1);

                return;

            }
        }

        // Chain Lightning at 5+ Maelstrom Weapon stacks
        if (maelstromStacks >= 5 && enemyCount >= 2)
        {
            // Enhancement doesn't have Chain Lightning natively, use Lightning Bolt

            if (this->CanCastSpell(ENH_LIGHTNING_BOLT, target))

            {

                this->CastSpell(target, ENH_LIGHTNING_BOLT);

                _maelstromWeaponTracker.ConsumeStacks();

                return;

            }
        }

        // Windstrike (Ascendance AoE)
        if (_ascendanceActive)
        {

            if (this->CanCastSpell(ENH_WINDSTRIKE, target))

            {

                this->CastSpell(target, ENH_WINDSTRIKE);

                _maelstromWeaponTracker.AddStack(1);

                return;

            }
        }

        // Stormstrike (AoE with Crash Lightning buff)
        if (_stormbringerTracker.IsActive() || this->CanCastSpell(ENH_STORMSTRIKE, target))
        {

            this->CastSpell(target, ENH_STORMSTRIKE);

            _stormbringerTracker.ConsumeProc();

            _maelstromWeaponTracker.AddStack(1);


            if (rand() % 100 < 10)

                _stormbringerTracker.ActivateProc();


            return;
        }

        // Lava Lash (AoE filler)
        if (this->CanCastSpell(ENH_LAVA_LASH, target))
        {

            this->CastSpell(target, ENH_LAVA_LASH);

            _maelstromWeaponTracker.AddStack(1);

            return;
        }

        // Rockbiter (builder)
        if (this->CanCastSpell(ENH_ROCKBITER, target))
        {

            this->CastSpell(target, ENH_ROCKBITER);

            return;
        }
    }

    [[nodiscard]] uint32 GetEnemiesInRange(float range) const
    {
        Player* bot = this->GetBot();
        if (!bot)

            return 0;

        uint32 count = 0;
        // Simplified enemy counting
        return std::min(count, 10u);
    }

    void InitializeEnhancementMechanics()
    {
        using namespace bot::ai;
        using namespace bot::ai::BehaviorTreeBuilder;
        BotAI* ai = this->GetBot()->GetBotAI();
        if (!ai) return;

        auto* queue = ai->GetActionPriorityQueue();
        if (queue)
        {

            queue->RegisterSpell(ENH_ASTRAL_SHIFT, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);

            queue->AddCondition(ENH_ASTRAL_SHIFT, [](Player* bot, Unit*) { return bot && bot->GetHealthPct() < 40.0f; }, "HP < 40%");


            queue->RegisterSpell(ENH_FERAL_SPIRIT, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);

            queue->AddCondition(ENH_FERAL_SPIRIT, [this](Player*, Unit*) { return this->_maelstromWeaponTracker.GetStacks() >= 5; }, "5 MW stacks");


            queue->RegisterSpell(ENH_ASCENDANCE, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);

            queue->AddCondition(ENH_ASCENDANCE, [this](Player* bot, Unit*) { return bot->HasSpell(ENH_ASCENDANCE) && !this->_ascendanceActive; }, "Ascendance burst");


            queue->RegisterSpell(ENH_WINDSTRIKE, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);

            queue->AddCondition(ENH_WINDSTRIKE, [this](Player*, Unit* target) { return target && this->_ascendanceActive; }, "Ascendance active");


            queue->RegisterSpell(ENH_LIGHTNING_BOLT, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);

            queue->AddCondition(ENH_LIGHTNING_BOLT, [this](Player*, Unit* target) { return target && this->_maelstromWeaponTracker.GetStacks() >= 5; }, "5 MW instant");


            queue->RegisterSpell(ENH_STORMSTRIKE, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);

            queue->AddCondition(ENH_STORMSTRIKE, [this](Player*, Unit* target) { return target && this->_stormbringerActive; }, "Stormbringer proc");


            queue->RegisterSpell(ENH_LAVA_LASH, SpellPriority::MEDIUM, SpellCategory::DAMAGE_SINGLE);

            queue->AddCondition(ENH_LAVA_LASH, [](Player*, Unit* target) { return target != nullptr; }, "Builder");


            queue->RegisterSpell(ENH_CRASH_LIGHTNING, SpellPriority::LOW, SpellCategory::DAMAGE_AOE);

            queue->AddCondition(ENH_CRASH_LIGHTNING, [this](Player*, Unit*) { return this->GetEnemiesInRange(8.0f) >= 2; }, "AoE 2+");
        }

        auto* tree = ai->GetBehaviorTree();
        if (tree)
        {

            auto root = Selector("Enhancement Shaman", {

                Sequence("Burst", { Condition("5 MW", [this](Player*) { return this->_maelstromWeaponTracker.GetStacks() >= 5; }),

                    Action("Feral Spirit/Ascendance", [this](Player* bot) { if (this->CanCastSpell(ENH_FERAL_SPIRIT, bot)) { this->CastSpell(bot, ENH_FERAL_SPIRIT); return NodeStatus::SUCCESS; } return NodeStatus::FAILURE; }) }),

                Sequence("MW Spender", { Condition("5 MW", [this](Player*) { return this->_maelstromWeaponTracker.GetStacks() >= 5; }),

                    Action("Lightning Bolt", [this](Player* bot) { Unit* t = bot->GetVictim(); if (t && this->CanCastSpell(ENH_LIGHTNING_BOLT, t)) { this->CastSpell(t, ENH_LIGHTNING_BOLT); return NodeStatus::SUCCESS; } return NodeStatus::FAILURE; }) }),

                Sequence("Stormstrike", { Condition("Has target", [this](Player* bot) { return bot && bot->GetVictim(); }),

                    Action("SS", [this](Player* bot) { Unit* t = bot->GetVictim(); if (t && this->CanCastSpell(ENH_STORMSTRIKE, t)) { this->CastSpell(t, ENH_STORMSTRIKE); return NodeStatus::SUCCESS; } return NodeStatus::FAILURE; }) }),

                Sequence("Builder", { Condition("Has target", [this](Player* bot) { return bot && bot->GetVictim(); }),

                    Action("Lava Lash", [this](Player* bot) { Unit* t = bot->GetVictim(); if (t && this->CanCastSpell(ENH_LAVA_LASH, t)) { this->CastSpell(t, ENH_LAVA_LASH); return NodeStatus::SUCCESS; } return NodeStatus::FAILURE; }) })

            });

            tree->SetRoot(root);
        }
    }

    // Member variables
    MaelstromWeaponTracker _maelstromWeaponTracker;
    StormbringerTracker _stormbringerTracker;

    bool _ascendanceActive;
    uint32 _ascendanceEndTime;

    uint32 _lastAscendanceTime;    uint32 _lastSunderingTime;
};

} // namespace Playerbot

#endif // PLAYERBOT_ENHANCEMENTSHAMANREFACTORED_H
