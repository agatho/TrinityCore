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

#ifndef PLAYERBOT_RESTORATIONDRUIDREFACTORED_H
#define PLAYERBOT_RESTORATIONDRUIDREFACTORED_H

#include "../CombatSpecializationTemplates.h"
#include "Player.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Group.h"
#include <unordered_map>
#include <vector>
#include "Log.h"
#include "../Decision/ActionPriorityQueue.h"
#include "../Decision/BehaviorTree.h"
#include "../BotAI.h"

namespace Playerbot
{

// WoW 11.2 (The War Within) - Restoration Druid Spell IDs
constexpr uint32 RESTO_REJUVENATION = 774;
constexpr uint32 RESTO_REGROWTH = 8936;
constexpr uint32 RESTO_WILD_GROWTH = 48438;
constexpr uint32 RESTO_SWIFTMEND = 18562;
constexpr uint32 RESTO_LIFEBLOOM = 33763;
constexpr uint32 RESTO_EFFLORESCENCE = 145205;
constexpr uint32 RESTO_TRANQUILITY = 740;
constexpr uint32 RESTO_IRONBARK = 102342;
constexpr uint32 RESTO_NATURES_SWIFTNESS = 132158;
constexpr uint32 RESTO_CENARION_WARD = 102351; // Talent
constexpr uint32 RESTO_FLOURISH = 197721; // Talent
constexpr uint32 RESTO_INCARNATION_TREE = 33891; // Incarnation: Tree of Life
constexpr uint32 RESTO_NOURISH = 50464;
constexpr uint32 RESTO_HEALING_TOUCH = 5185;
constexpr uint32 RESTO_INNERVATE = 29166;
constexpr uint32 RESTO_BARKSKIN = 22812;
constexpr uint32 RESTO_RENEWAL = 108238;
constexpr uint32 RESTO_MOONFIRE = 8921; // For mana regen / DPS

// Mana resource is defined in CombatSpecializationTemplates.h as uint32
// No custom ManaResource struct needed - use ManaResource (uint32 typedef)

// HoT (Heal over Time) tracking system
class RestorationHoTTracker
{
public:
    RestorationHoTTracker() = default;

    void ApplyRejuvenation(ObjectGuid guid, uint32 duration = 15000)
    {
        _rejuvenationTargets[guid] = GameTime::GetGameTimeMS() + duration;
    }

    void ApplyLifebloom(ObjectGuid guid, uint32 duration = 15000)
    {
        _lifebloomTargets[guid] = GameTime::GetGameTimeMS() + duration;
    }

    void ApplyWildGrowth(ObjectGuid guid, uint32 duration = 7000)
    {
        _wildGrowthTargets[guid] = GameTime::GetGameTimeMS() + duration;
    }

    void ApplyCenarionWard(ObjectGuid guid, uint32 duration = 30000)
    {
        _cenarionWardTargets[guid] = GameTime::GetGameTimeMS() + duration;
    }

    [[nodiscard]] bool HasRejuvenation(ObjectGuid guid) const
    {
        auto it = _rejuvenationTargets.find(guid);
        return it != _rejuvenationTargets.end() && GameTime::GetGameTimeMS() < it->second;
    }

    [[nodiscard]] bool HasLifebloom(ObjectGuid guid) const
    {
        auto it = _lifebloomTargets.find(guid);
        return it != _lifebloomTargets.end() && GameTime::GetGameTimeMS() < it->second;
    }

    [[nodiscard]] bool HasWildGrowth(ObjectGuid guid) const
    {
        auto it = _wildGrowthTargets.find(guid);
        return it != _wildGrowthTargets.end() && GameTime::GetGameTimeMS() < it->second;
    }

    [[nodiscard]] bool HasCenarionWard(ObjectGuid guid) const
    {
        auto it = _cenarionWardTargets.find(guid);
        return it != _cenarionWardTargets.end() && GameTime::GetGameTimeMS() < it->second;
    }

    [[nodiscard]] uint32 GetLifebloomTimeRemaining(ObjectGuid guid) const
    {
        auto it = _lifebloomTargets.find(guid);
        if (it != _lifebloomTargets.end())
        {

            uint32 now = GameTime::GetGameTimeMS();

            return now < it->second ? (it->second - now) : 0;
        }
        return 0;
    }

    [[nodiscard]] bool NeedsLifebloomRefresh(ObjectGuid guid, uint32 pandemicWindow = 4500) const
    {
        uint32 remaining = GetLifebloomTimeRemaining(guid);
        return remaining < pandemicWindow; // Refresh in pandemic window
    }

    [[nodiscard]] uint32 GetActiveRejuvenationCount() const
    {
        uint32 count = 0;
        uint32 now = GameTime::GetGameTimeMS();
        for (const auto& pair : _rejuvenationTargets)
        {

            if (now < pair.second)

                ++count;
        }
        return count;
    }

    void Update(const std::vector<Unit*>& group)
    {
        for (Unit* member : group)
        {

            if (!member)

                continue;


            ObjectGuid guid = member->GetGUID();
            if (!member)

            {

                if (!rejuv)

                {

                    return nullptr;

                }

                return;

            }

            // Sync with actual auras

            if (Aura* rejuv = member->GetAura(RESTO_REJUVENATION))

                if (!lifebloom)

                {

                    return nullptr;

                }

                _rejuvenationTargets[guid] = GameTime::GetGameTimeMS() + rejuv->GetDuration();

                if (!rejuv)

                {

                    return nullptr;

                if (!wildGrowth)

                {

                    return;

                }

                }

            else

                _rejuvenationTargets.erase(guid);


            if (Aura* lifebloom = member->GetAura(RESTO_LIFEBLOOM))

                if (!cenarionWard)

                {

                    return nullptr;

                }

                _lifebloomTargets[guid] = GameTime::GetGameTimeMS() + lifebloom->GetDuration();

                if (!lifebloom)

                {

                    return nullptr;

                }

            else

                _lifebloomTargets.erase(guid);


            if (Aura* wildGrowth = member->GetAura(RESTO_WILD_GROWTH))

                _wildGrowthTargets[guid] = GameTime::GetGameTimeMS() + wildGrowth->GetDuration();

                if (!wildGrowth)

                {

                    return nullptr;

                }

            else

                _wildGrowthTargets.erase(guid);


            if (Aura* cenarionWard = member->GetAura(RESTO_CENARION_WARD))

                _cenarionWardTargets[guid] = GameTime::GetGameTimeMS() + cenarionWard->GetDuration();

                if (!cenarionWard)

                {

                    return nullptr;

                }

            else

                _cenarionWardTargets.erase(guid);
        }
    }

private:
    CooldownManager _cooldowns;
    std::unordered_map<ObjectGuid, uint32> _rejuvenationTargets;
    std::unordered_map<ObjectGuid, uint32> _lifebloomTargets;
    std::unordered_map<ObjectGuid, uint32> _wildGrowthTargets;
    std::unordered_map<ObjectGuid, uint32> _cenarionWardTargets;
};

// Swiftmend target tracker (requires a HoT to cast)
class RestorationSwiftmendTracker
{
public:
    RestorationSwiftmendTracker() : _lastSwiftmendTime(0) {}

    [[nodiscard]] bool CanUseSwiftmend() const
    {
        // 15 sec cooldown
        return (GameTime::GetGameTimeMS() - _lastSwiftmendTime) >= 15000;
    }

    void UseSwiftmend()
    {
        _lastSwiftmendTime = GameTime::GetGameTimeMS();
    }private:
    uint32 _lastSwiftmendTime;
};

class RestorationDruidRefactored : public HealerSpecialization<ManaResource>
{
public:
    using Base = HealerSpecialization<ManaResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit RestorationDruidRefactored(Player* bot)        : HealerSpecialization<ManaResource>(bot)
        
        , _hotTracker()
        , _swiftmendTracker()
        , _treeFormActive(false)
        , _treeFormEndTime(0)
        
        , _lastInnervateTime(0)
        , _cooldowns()
    {
        // Register cooldowns for major abilities
        _cooldowns.RegisterBatch({

            {RESTO_DRUID_TRANQUILITY, 180000, 1},

            {RESTO_DRUID_INCARNATION, 180000, 1},

            {RESTO_DRUID_CONVOKE, 120000, 1},

            {RESTO_DRUID_FLOURISH, 90000, 1},

            {RESTO_DRUID_IRONBARK, 90000, 1}
        });

        // _resource is uint32, no Initialize method - managed by base class

        // Phase 5: Initialize decision systems
        InitializeRestorationMechanics();

        TC_LOG_DEBUG("playerbot", "RestorationDruidRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        Player* bot = this->GetBot();
        if (!bot)

            return;

        UpdateRestorationState();

        std::vector<Unit*> group = GetGroupMembers();
        if (group.empty())

            group.push_back(bot); // Solo healing (self)

        ExecuteHealingRotation(group);
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();
        if (!bot)

            return;

        // Innervate for mana regeneration
        if (bot->GetPowerPct(POWER_MANA) < 30)
        {

            if (this->CanCastSpell(RESTO_INNERVATE, bot))

            {

                this->CastSpell(bot, RESTO_INNERVATE);

                _lastInnervateTime = GameTime::GetGameTimeMS();

            }
        }
    }

    void UpdateDefensives()
    {
        Player* bot = this->GetBot();
        if (!bot)

            return;

        float healthPct = bot->GetHealthPct();

        // Barkskin (personal damage reduction)
        if (healthPct < 50.0f && this->CanCastSpell(RESTO_BARKSKIN, bot))
        {

            this->CastSpell(bot, RESTO_BARKSKIN);

            return;
        }

        // Renewal (self-heal)
        if (healthPct < 60.0f && this->CanCastSpell(RESTO_RENEWAL, bot))
        {

            this->CastSpell(bot, RESTO_RENEWAL);

            return;
        }
    }

private:    

    void UpdateRestorationState()
    {
        Player* bot = this->GetBot();
        if (!bot)

            return;

        // _resource is uint32, no Update method - managed by base class
        UpdateCooldownStates();
    }

    void UpdateCooldownStates()
    {
        Player* bot = this->GetBot();
        if (!bot)

            return;

        // Tree Form state
        if (_treeFormActive && GameTime::GetGameTimeMS() >= _treeFormEndTime)

            _treeFormActive = false;

        if (bot->HasAura(RESTO_INCARNATION_TREE))
        {

            _treeFormActive = true;

            if (Aura* aura = bot->GetAura(RESTO_INCARNATION_TREE))

                _treeFormEndTime = GameTime::GetGameTimeMS() + aura->GetDuration();
                }
    }

    void ExecuteHealingRotation(const std::vector<Unit*>& group)
    {
        _hotTracker.Update(group);

        // Emergency group-wide healing
        if (HandleEmergencyHealing(group))

            return;

        // Maintain Lifebloom on tank
        if (HandleLifebloom(group))

            return;

        // Spread Rejuvenation
        if (HandleRejuvenation(group))

            return;

        // Wild Growth for AoE healing
        if (HandleWildGrowth(group))

            return;

        // Swiftmend for quick single-target healing
        if (HandleSwiftmend(group))

            return;

        // Cenarion Ward (talent)
        if (HandleCenarionWard(group))

            return;

        // Regrowth for direct healing
        if (HandleRegrowth(group))

            return;

        // DPS rotation when no healing needed
        HandleDPSRotation();
    }

    bool HandleEmergencyHealing(const std::vector<Unit*>& group)
    {
        Player* bot = this->GetBot();
        if (!bot)

            return false;

        // Count critically injured allies
        uint32 criticalCount = 0;
        for (Unit* member : group)
        {

            if (member && member->GetHealthPct() < 40.0f)

                ++criticalCount;
        }

        // Tranquility (raid-wide emergency healing)
        if (criticalCount >= 3 && (GameTime::GetGameTimeMS() - _lastTranquilityTime) >= 180000) // 3 min CD
        {

            if (this->CanCastSpell(RESTO_TRANQUILITY, bot))

            {

                this->CastSpell(bot, RESTO_TRANQUILITY);

                _lastTranquilityTime = GameTime::GetGameTimeMS();

                return true;

            }
        }

        // Incarnation: Tree of Life (major healing CD)
        if (criticalCount >= 2 && !_treeFormActive)
        {

            if (this->CanCastSpell(RESTO_INCARNATION_TREE, bot))

            {

                this->CastSpell(bot, RESTO_INCARNATION_TREE);

                _treeFormActive = true;

                _treeFormEndTime = GameTime::GetGameTimeMS() + 30000; // 30 sec

                return true;

            }
        }

        // Nature's Swiftness + Regrowth instant cast
        for (Unit* member : group)
        if (!tank)
        {

            return nullptr;
        }
        {

            if (member && member->GetHealthPct() < 30.0f)

            {

                if (this->CanCastSpell(RESTO_NATURES_SWIFTNESS, bot))

                {

                    if (!tank)

                    {

                        return nullptr;

                    }

                    this->CastSpell(bot, RESTO_NATURES_SWIFTNESS);

                    if (this->CanCastSpell(RESTO_REGROWTH, member))

                    {

                        this->CastSpell(member, RESTO_REGROWTH);

                        return true;

                    }

                }

            }
        }

        // Ironbark on tank taking heavy damage
        for (Unit* member : group)        {

            if (member && member->GetHealthPct() < 50.0f && IsTank(member))

            {

                if (this->CanCastSpell(RESTO_IRONBARK, member))

                {

                    this->CastSpell(member, RESTO_IRONBARK);

                    return true;

                }

            }
        }

        return false;
    }

    bool HandleLifebloom(const std::vector<Unit*>& group)
    {
        // Maintain Lifebloom on primary tank
        Unit* tank = GetMainTank(group);

                if (!tank)

                {

                    return;

                }

                if (!tank)

                {

                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: tank in method GetGUID");
                    return;

                }
        if (tank && _hotTracker.NeedsLifebloomRefresh(tank->GetGUID()))
        {

            if (this->CanCastSpell(RESTO_LIFEBLOOM, tank))

            {

                this->CastSpell(tank, RESTO_LIFEBLOOM);

                _hotTracker.ApplyLifebloom(tank->GetGUID(), 15000);

                return true;

            }
        }

        return false;
    }    bool HandleRejuvenation(const std::vector<Unit*>& group)
    {
        uint32 activeRejuvs = _hotTracker.GetActiveRejuvenationCount();

        // Spread Rejuvenation to injured allies (maintain on ~3-4 targets)
        if (activeRejuvs < 4)
        {

            for (Unit* member : group)
            {

                if (member && member->GetHealthPct() < 95.0f && !_hotTracker.HasRejuvenation(member->GetGUID()))

                {

                    if (this->CanCastSpell(RESTO_REJUVENATION, member))

                    {

                        this->CastSpell(member, RESTO_REJUVENATION);

                        _hotTracker.ApplyRejuvenation(member->GetGUID(), 15000);

                        return true;

                    }

                }

            }
        }

        return false;
    }

    bool HandleWildGrowth(const std::vector<Unit*>& group)
    {
        // Count injured allies without Wild Growth        uint32 needsHealing = 0;
        for (Unit* member : group)

            if (!member)

            {

                if (!tank)

                {

                    return nullptr;

                }

                return nullptr;

            }
        {

            if (member && member->GetHealthPct() < 85.0f && !_hotTracker.HasWildGrowth(member->GetGUID()))

                ++needsHealing;
        if (!tank)
        {

            return nullptr;
        }
        }

        // Use Wild Growth if 3+ allies need healing
        if (needsHealing >= 3)
        {

            Unit* target = GetGroupMemberNeedingHealing(group, 85.0f);

            if (target && this->CanCastSpell(RESTO_WILD_GROWTH, target))

            {

                this->CastSpell(target, RESTO_WILD_GROWTH);
                // Apply to all nearby allies (simplified)

                for (Unit* member : group)
                {

                    if (member)

                        _hotTracker.ApplyWildGrowth(member->GetGUID(), 7000);

                }

                return true;

            }
        }

        return false;
    }

    bool HandleSwiftmend(const std::vector<Unit*>& group)
    {
        if (!_swiftmendTracker.CanUseSwiftmend())

            return false;

        // Use Swiftmend on injured ally with a HoT
        for (Unit* member : group)        {

            if (member && member->GetHealthPct() < 70.0f)

            {

                if (_hotTracker.HasRejuvenation(member->GetGUID()) || _hotTracker.HasWildGrowth(member->GetGUID()))

                {

                    if (this->CanCastSpell(RESTO_SWIFTMEND, member))

                    {

                        this->CastSpell(member, RESTO_SWIFTMEND);

                        _swiftmendTracker.UseSwiftmend();

                        return true;

                    }

                }

            }
        }

        return false;    }

    bool HandleCenarionWard(const std::vector<Unit*>& group)
    {
        Player* bot = this->GetBot();
        
        if (!bot || !bot->HasSpell(RESTO_CENARION_WARD))

            return false;

        // Apply Cenarion Ward to tank
        Unit* tank = GetMainTank(group);

                if (!tank)

                {

                    return;

                }

                if (!tank)

                {

                    return;

                }
        if (tank && !_hotTracker.HasCenarionWard(tank->GetGUID()))
        {

            if (this->CanCastSpell(RESTO_CENARION_WARD, tank))

            {

                this->CastSpell(tank, RESTO_CENARION_WARD);

                _hotTracker.ApplyCenarionWard(tank->GetGUID(), 30000);

                return true;

            }
        }

        return false;
    }

    bool HandleRegrowth(const std::vector<Unit*>& group)
    {
        // Use Regrowth for direct healing when needed
        for (Unit* member : group)
        {

            if (member && member->GetHealthPct() < 80.0f)

            {

                if (this->CanCastSpell(RESTO_REGROWTH, member))

                {

                    this->CastSpell(member, RESTO_REGROWTH);

                    return true;

                }

            }
        }

        return false;
    }

    void HandleDPSRotation()
    {
        Player* bot = this->GetBot();
        if (!bot)

            return;

        // When no healing is needed, contribute damage
        Unit* target = bot->GetVictim();
        if (!target)

            target = FindNearbyEnemy();

        if (target)
        {
            // Moonfire for ranged damage

            if (!target->HasAura(RESTO_MOONFIRE) && this->CanCastSpell(RESTO_MOONFIRE, target))

            {

                this->CastSpell(target, RESTO_MOONFIRE);

            }
        }
    }

    [[nodiscard]] std::vector<Unit*> GetGroupMembers() const
    {
        std::vector<Unit*> members;

        Player* bot = this->GetBot();
        if (!bot)

            return members;

        Group* group = bot->GetGroup();        if (!group)

            return members;

        for (GroupReference& itr : group->GetMembers())
        {

            if (Player* member = itr.GetSource())

            {

                if (member->IsInWorld() && bot->GetDistance(member) <= 40.0f)
                members.push_back(member);

            }
        }

        return members;
    }

    [[nodiscard]] Unit* GetGroupMemberNeedingHealing(const std::vector<Unit*>& group, float healthThreshold) const
    {
        Unit* mostInjured = nullptr;
        float lowestHealth = 100.0f;

        for (Unit* member : group)
        {

            if (member && member->GetHealthPct() < healthThreshold && member->GetHealthPct() < lowestHealth)

            {

                mostInjured = member;

                lowestHealth = member->GetHealthPct();

            }
        }

        return mostInjured;
    }

    [[nodiscard]] Unit* GetMainTank(const std::vector<Unit*>& group) const
    {
        // Find the group member with tank role or highest threat
        for (Unit* member : group)
        {

            if (member && IsTank(member))

                return member;
        }

        return !group.empty() ? group[0] : nullptr;
    }

    [[nodiscard]] bool IsTank(Unit* unit) const
    {
        if (!unit)

            return false;

        // Check if player has tank spec/role
        if (Player* player = unit->ToPlayer())
        {
            // Simplified tank detection - check if unit is actively tanking
            // GetPrimaryTalentTree API is deprecated in TrinityCore
            // Use threat/role-based detection instead

            return player->GetVictim() != nullptr;
        }

        return false;
    }

    [[nodiscard]] Unit* FindNearbyEnemy() const
    {
        // Simplified enemy finding - in real implementation, query nearby hostile units
        Player* bot = this->GetBot();
        return bot ? bot->GetVictim() : nullptr;
    }

    // Member variables
    RestorationHoTTracker _hotTracker;
    RestorationSwiftmendTracker _swiftmendTracker;

    bool _treeFormActive;
    uint32 _treeFormEndTime;
    uint32 _lastInnervateTime;
    uint32 _lastTranquilityTime = 0;
    CooldownManager _cooldowns;

    // ========================================================================
    // PHASE 5: DECISION SYSTEM INTEGRATION
    // ========================================================================

    void InitializeRestorationMechanics()
    {
        using namespace bot::ai;
        using namespace bot::ai::BehaviorTreeBuilder;

        BotAI* ai = this->GetBot()->GetBotAI();
        if (!ai) return;

        auto* queue = ai->GetActionPriorityQueue();
        if (queue)
        {
            // EMERGENCY: Raid-wide emergency healing

            queue->RegisterSpell(RESTO_TRANQUILITY, SpellPriority::EMERGENCY, SpellCategory::HEALING);

            queue->AddCondition(RESTO_TRANQUILITY, [this](Player*, Unit*) {

                auto group = this->GetGroupMembers();

                uint32 critical = 0;

                for (auto* m : group) if (m && m->GetHealthPct() < 40.0f) critical++;

                return critical >= 3;

            }, "3+ allies < 40% HP (channel heal)");


            queue->RegisterSpell(RESTO_NATURES_SWIFTNESS, SpellPriority::EMERGENCY, SpellCategory::HEALING);

            queue->AddCondition(RESTO_NATURES_SWIFTNESS, [this](Player*, Unit*) {

                auto group = this->GetGroupMembers();

                for (auto* m : group) if (m && m->GetHealthPct() < 30.0f) return true;

                return false;

            }, "Ally < 30% HP (instant cast)");

            // CRITICAL: Major healing cooldowns

            queue->RegisterSpell(RESTO_INCARNATION_TREE, SpellPriority::CRITICAL, SpellCategory::HEALING);

            queue->AddCondition(RESTO_INCARNATION_TREE, [this](Player*, Unit*) {

                if (this->_treeFormActive) return false;

                auto group = this->GetGroupMembers();

                uint32 injured = 0;

                for (auto* m : group) if (m && m->GetHealthPct() < 60.0f) injured++;

                return injured >= 2;

            }, "2+ allies < 60% HP (30s form)");


            queue->RegisterSpell(RESTO_IRONBARK, SpellPriority::CRITICAL, SpellCategory::DEFENSIVE);

            queue->AddCondition(RESTO_IRONBARK, [this](Player*, Unit*) {

                auto group = this->GetGroupMembers();

                for (auto* m : group)

                    if (m && m->GetHealthPct() < 50.0f && this->IsTank(m)) return true;

                return false;

            }, "Tank < 50% HP (20% dmg reduction)");

            // HIGH: Core HoT maintenance

            queue->RegisterSpell(RESTO_LIFEBLOOM, SpellPriority::HIGH, SpellCategory::HEALING);

            queue->AddCondition(RESTO_LIFEBLOOM, [this](Player*, Unit*) {

                auto group = this->GetGroupMembers();

                Unit* tank = this->GetMainTank(group);

                return tank && this->_hotTracker.NeedsLifebloomRefresh(tank->GetGUID());

            }, "Tank needs Lifebloom (pandemic refresh)");


            queue->RegisterSpell(RESTO_SWIFTMEND, SpellPriority::HIGH, SpellCategory::HEALING);

            queue->AddCondition(RESTO_SWIFTMEND, [this](Player*, Unit*) {

                if (!this->_swiftmendTracker.CanUseSwiftmend()) return false;

                auto group = this->GetGroupMembers();

                for (auto* m : group) {

                    if (m && m->GetHealthPct() < 70.0f) {

                        auto guid = m->GetGUID();

                        if (this->_hotTracker.HasRejuvenation(guid) ||

                            this->_hotTracker.HasWildGrowth(guid)) return true;

                    }

                }

                return false;

            }, "Ally < 70% HP with HoT (instant heal)");


            queue->RegisterSpell(RESTO_WILD_GROWTH, SpellPriority::HIGH, SpellCategory::HEALING);

            queue->AddCondition(RESTO_WILD_GROWTH, [this](Player*, Unit*) {

                auto group = this->GetGroupMembers();

                uint32 needs = 0;

                for (auto* m : group)

                    if (m && m->GetHealthPct() < 85.0f && !this->_hotTracker.HasWildGrowth(m->GetGUID()))

                        needs++;

                return needs >= 3;

            }, "3+ allies < 85% HP (AoE HoT)");

            // MEDIUM: Rejuvenation spreading

            queue->RegisterSpell(RESTO_REJUVENATION, SpellPriority::MEDIUM, SpellCategory::HEALING);

            queue->AddCondition(RESTO_REJUVENATION, [this](Player*, Unit*) {

                uint32 active = this->_hotTracker.GetActiveRejuvenationCount();

                if (active >= 4) return false;

                auto group = this->GetGroupMembers();

                for (auto* m : group)

                    if (m && m->GetHealthPct() < 95.0f && !this->_hotTracker.HasRejuvenation(m->GetGUID()))

                        return true;

                return false;

            }, "Ally < 95% HP, maintain 4 HoTs");


            queue->RegisterSpell(RESTO_CENARION_WARD, SpellPriority::MEDIUM, SpellCategory::HEALING);

            queue->AddCondition(RESTO_CENARION_WARD, [this](Player* bot, Unit*) {

                if (!bot->HasSpell(RESTO_CENARION_WARD)) return false;

                auto group = this->GetGroupMembers();

                Unit* tank = this->GetMainTank(group);

                return tank && !this->_hotTracker.HasCenarionWard(tank->GetGUID());

            }, "Tank needs Cenarion Ward (30s reactive)");


            queue->RegisterSpell(RESTO_REGROWTH, SpellPriority::MEDIUM, SpellCategory::HEALING);

            queue->AddCondition(RESTO_REGROWTH, [this](Player*, Unit*) {

                auto group = this->GetGroupMembers();

                for (auto* m : group)

                    if (m && m->GetHealthPct() < 80.0f) return true;

                return false;

            }, "Ally < 80% HP (direct + HoT)");

            // LOW: Mana management

            queue->RegisterSpell(RESTO_INNERVATE, SpellPriority::LOW, SpellCategory::UTILITY);

            queue->AddCondition(RESTO_INNERVATE, [this](Player* bot, Unit*) {

                return bot && bot->GetPowerPct(POWER_MANA) < 30.0f;

            }, "Mana < 30% (100% regen)");

            // DEFENSIVE: Personal defensives

            queue->RegisterSpell(RESTO_BARKSKIN, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);

            queue->AddCondition(RESTO_BARKSKIN, [](Player* bot, Unit*) {

                return bot && bot->GetHealthPct() < 50.0f;

            }, "HP < 50% (20% dmg reduction)");


            queue->RegisterSpell(RESTO_RENEWAL, SpellPriority::HIGH, SpellCategory::HEALING);

            queue->AddCondition(RESTO_RENEWAL, [](Player* bot, Unit*) {

                return bot && bot->GetHealthPct() < 60.0f;

            }, "HP < 60% (self-heal)");

            // UTILITY: DPS contribution

            queue->RegisterSpell(RESTO_MOONFIRE, SpellPriority::LOW, SpellCategory::DAMAGE_SINGLE);

            queue->AddCondition(RESTO_MOONFIRE, [](Player* bot, Unit* target) {

                return target && !target->HasAura(RESTO_MOONFIRE);

            }, "No Moonfire (contribute DPS)");
        }

        auto* behaviorTree = ai->GetBehaviorTree();
        if (behaviorTree)
        {

            auto root = Selector("Restoration Druid Healing", {
                // Tier 1: Emergency Group Healing

                Sequence("Emergency Healing", {

                    Condition("3+ critical", [this](Player*) {

                        auto group = this->GetGroupMembers();

                        uint32 critical = 0;

                        for (auto* m : group) if (m && m->GetHealthPct() < 40.0f) critical++;

                        return critical >= 3;

                    }),

                    Selector("Use emergency", {

                        Sequence("Tranquility", {

                            Action("Cast Tranquility", [this](Player* bot) {

                                if (this->CanCastSpell(RESTO_TRANQUILITY, bot)) {

                                    this->CastSpell(bot, RESTO_TRANQUILITY);

                                    this->_lastTranquilityTime = GameTime::GetGameTimeMS();

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),

                        Sequence("Nature's Swiftness", {

                            Action("Instant Regrowth", [this](Player* bot) {

                                auto group = this->GetGroupMembers();

                                for (auto* m : group) {

                                    if (m && m->GetHealthPct() < 30.0f) {

                                        if (this->CanCastSpell(RESTO_NATURES_SWIFTNESS, bot)) {

                                            this->CastSpell(bot, RESTO_NATURES_SWIFTNESS);

                                            if (this->CanCastSpell(RESTO_REGROWTH, m)) {

                                                this->CastSpell(m, RESTO_REGROWTH);

                                                return NodeStatus::SUCCESS;

                                            }

                                        }

                                    }

                                }

                                return NodeStatus::FAILURE;

                            })

                        })

                    })

                }),

                // Tier 2: Major Healing Cooldowns

                Sequence("Major Cooldowns", {

                    Condition("2+ injured", [this](Player*) {

                        auto group = this->GetGroupMembers();

                        uint32 injured = 0;

                        for (auto* m : group) if (m && m->GetHealthPct() < 60.0f) injured++;

                        return injured >= 2;

                    }),

                    Selector("Use cooldowns", {

                        Sequence("Tree Form", {

                            Condition("Not active", [this](Player*) {

                                return !this->_treeFormActive;

                            }),

                            Action("Cast Incarnation", [this](Player* bot) {

                                if (this->CanCastSpell(RESTO_INCARNATION_TREE, bot)) {

                                    this->CastSpell(bot, RESTO_INCARNATION_TREE);

                                    this->_treeFormActive = true;

                                    this->_treeFormEndTime = GameTime::GetGameTimeMS() + 30000;

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),

                        Sequence("Ironbark Tank", {

                            Action("Cast Ironbark", [this](Player*) {

                                auto group = this->GetGroupMembers();

                                for (auto* m : group) {

                                    if (m && m->GetHealthPct() < 50.0f && this->IsTank(m)) {

                                        if (this->CanCastSpell(RESTO_IRONBARK, m)) {

                                            this->CastSpell(m, RESTO_IRONBARK);

                                            return NodeStatus::SUCCESS;

                                        }

                                    }

                                }

                                return NodeStatus::FAILURE;

                            })

                        })

                    })

                }),

                // Tier 3: HoT Maintenance

                Sequence("Maintain HoTs", {

                    Selector("Apply HoTs", {

                        Sequence("Lifebloom Tank", {

                            Action("Cast Lifebloom", [this](Player*) {

                                auto group = this->GetGroupMembers();

                                Unit* tank = this->GetMainTank(group);

                                if (tank && this->_hotTracker.NeedsLifebloomRefresh(tank->GetGUID())) {

                                    if (this->CanCastSpell(RESTO_LIFEBLOOM, tank)) {

                                        this->CastSpell(tank, RESTO_LIFEBLOOM);

                                        this->_hotTracker.ApplyLifebloom(tank->GetGUID(), 15000);

                                        return NodeStatus::SUCCESS;

                                    }

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),

                        Sequence("Wild Growth AoE", {

                            Condition("3+ need healing", [this](Player*) {

                                auto group = this->GetGroupMembers();

                                uint32 needs = 0;

                                for (auto* m : group)

                                    if (m && m->GetHealthPct() < 85.0f && !this->_hotTracker.HasWildGrowth(m->GetGUID()))

                                        needs++;

                                return needs >= 3;

                            }),

                            Action("Cast Wild Growth", [this](Player*) {

                                auto group = this->GetGroupMembers();

                                Unit* target = this->GetGroupMemberNeedingHealing(group, 85.0f);

                                if (target && this->CanCastSpell(RESTO_WILD_GROWTH, target)) {

                                    this->CastSpell(target, RESTO_WILD_GROWTH);

                                    for (auto* m : group) {

                                        if (m) this->_hotTracker.ApplyWildGrowth(m->GetGUID(), 7000);

                                    }

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),

                        Sequence("Rejuvenation Spread", {

                            Condition("< 4 active", [this](Player*) {

                                return this->_hotTracker.GetActiveRejuvenationCount() < 4;

                            }),

                            Action("Cast Rejuvenation", [this](Player*) {

                                auto group = this->GetGroupMembers();

                                for (auto* m : group) {

                                    if (m && m->GetHealthPct() < 95.0f && !this->_hotTracker.HasRejuvenation(m->GetGUID())) {

                                        if (this->CanCastSpell(RESTO_REJUVENATION, m)) {

                                            this->CastSpell(m, RESTO_REJUVENATION);

                                            this->_hotTracker.ApplyRejuvenation(m->GetGUID(), 15000);

                                            return NodeStatus::SUCCESS;

                                        }

                                    }

                                }

                                return NodeStatus::FAILURE;

                            })

                        })

                    })

                }),

                // Tier 4: Direct Healing

                Sequence("Direct Healing", {

                    Selector("Cast heals", {

                        Sequence("Swiftmend", {

                            Condition("Can use", [this](Player*) {

                                return this->_swiftmendTracker.CanUseSwiftmend();

                            }),

                            Action("Cast Swiftmend", [this](Player*) {

                                auto group = this->GetGroupMembers();

                                for (auto* m : group) {

                                    if (m && m->GetHealthPct() < 70.0f) {

                                        auto guid = m->GetGUID();

                                        if (this->_hotTracker.HasRejuvenation(guid) || this->_hotTracker.HasWildGrowth(guid)) {

                                            if (this->CanCastSpell(RESTO_SWIFTMEND, m)) {

                                                this->CastSpell(m, RESTO_SWIFTMEND);

                                                this->_swiftmendTracker.UseSwiftmend();

                                                return NodeStatus::SUCCESS;

                                            }

                                        }

                                    }

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),

                        Sequence("Regrowth", {

                            Action("Cast Regrowth", [this](Player*) {

                                auto group = this->GetGroupMembers();

                                for (auto* m : group) {

                                    if (m && m->GetHealthPct() < 80.0f) {

                                        if (this->CanCastSpell(RESTO_REGROWTH, m)) {

                                            this->CastSpell(m, RESTO_REGROWTH);

                                            return NodeStatus::SUCCESS;

                                        }

                                    }

                                }

                                return NodeStatus::FAILURE;

                            })

                        })

                    })

                }),

                // Tier 5: DPS Contribution

                Sequence("DPS Filler", {

                    Condition("Group healthy", [this](Player*) {

                        auto group = this->GetGroupMembers();

                        for (auto* m : group) if (m && m->GetHealthPct() < 90.0f) return false;

                        return true;

                    }),

                    Action("Cast Moonfire", [this](Player* bot) {

                        Unit* target = bot->GetVictim();

                        if (!target) target = this->FindNearbyEnemy();

                        if (target && !target->HasAura(RESTO_MOONFIRE)) {

                            if (this->CanCastSpell(RESTO_MOONFIRE, target)) {

                                this->CastSpell(target, RESTO_MOONFIRE);

                                return NodeStatus::SUCCESS;

                            }

                        }

                        return NodeStatus::FAILURE;

                    })

                })

            });


            behaviorTree->SetRoot(root);
        }
    }
};

} // namespace Playerbot

#endif // PLAYERBOT_RESTORATIONDRUIDREFACTORED_H
