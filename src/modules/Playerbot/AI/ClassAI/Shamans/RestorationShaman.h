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

#ifndef PLAYERBOT_RESTORATIONSHAMANREFACTORED_H
#define PLAYERBOT_RESTORATIONSHAMANREFACTORED_H

#include "../CombatSpecializationTemplates.h"
#include "Player.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "ObjectGuid.h"
#include <unordered_map>
#include "Log.h"
#include "../Decision/ActionPriorityQueue.h"
#include "../Decision/BehaviorTree.h"
#include "../BotAI.h"

namespace Playerbot
{

// WoW 11.2 (The War Within) - Restoration Shaman Spell IDs
constexpr uint32 REST_HEALING_WAVE = 77472;
constexpr uint32 REST_HEALING_SURGE = 8004;
constexpr uint32 REST_CHAIN_HEAL = 1064;
constexpr uint32 REST_RIPTIDE = 61295;
constexpr uint32 REST_HEALING_RAIN = 73920;
constexpr uint32 REST_WELLSPRING = 197995;
constexpr uint32 REST_HEALING_TIDE_TOTEM = 108280;
constexpr uint32 REST_CLOUDBURST_TOTEM = 157153;
constexpr uint32 REST_SPIRIT_LINK_TOTEM = 98008;
constexpr uint32 REST_EARTHEN_WALL_TOTEM = 198838;
constexpr uint32 REST_ANCESTRAL_PROTECTION_TOTEM = 207399;
constexpr uint32 REST_ASCENDANCE = 114052;
constexpr uint32 REST_UNLEASH_LIFE = 73685;
constexpr uint32 REST_EARTH_SHIELD = 974;
constexpr uint32 REST_WATER_SHIELD = 52127;
constexpr uint32 REST_PURIFY_SPIRIT = 77130;
constexpr uint32 REST_SPIRITWALKERS_GRACE = 79206;
constexpr uint32 REST_ASTRAL_SHIFT = 108271;
constexpr uint32 REST_WIND_SHEAR = 57994;

// ManaResource is already defined in CombatSpecializationTemplates.h
// No need to redefine it here

// Riptide HoT tracker
class RiptideTracker
{
public:
    RiptideTracker() = default;

    void ApplyRiptide(ObjectGuid guid, uint32 duration = 18000)
    {
        _riptideTargets[guid] = GameTime::GetGameTimeMS() + duration;
    }

    void RemoveRiptide(ObjectGuid guid)
    {
        _riptideTargets.erase(guid);
    }

    [[nodiscard]] bool HasRiptide(ObjectGuid guid) const
    {
        auto it = _riptideTargets.find(guid);
        if (it == _riptideTargets.end())

            return false;
        return GameTime::GetGameTimeMS() < it->second;
    }

    [[nodiscard]] uint32 GetRiptideTimeRemaining(ObjectGuid guid) const
    {
        auto it = _riptideTargets.find(guid);
        if (it == _riptideTargets.end())

            return 0;

        uint32 now = GameTime::GetGameTimeMS();
        if (now >= it->second)

            return 0;

        return it->second - now;
    }

    [[nodiscard]] bool NeedsRiptideRefresh(ObjectGuid guid, uint32 pandemicWindow = 5400) const
    {
        uint32 remaining = GetRiptideTimeRemaining(guid);
        return remaining < pandemicWindow;
    }

    [[nodiscard]] uint32 GetActiveRiptideCount() const
    {
        uint32 count = 0;
        uint32 now = GameTime::GetGameTimeMS();
        for (const auto& pair : _riptideTargets)
        {

            if (now < pair.second)

                ++count;
        }
        return count;
    }

    void Update(Player* bot)
    {
        if (!bot)

            return;

        // Clean up expired Riptides
        uint32 now = GameTime::GetGameTimeMS();
        for (auto it = _riptideTargets.begin(); it != _riptideTargets.end();)
        {

            if (now >= it->second)

                it = _riptideTargets.erase(it);

            else

                ++it;
        }
    }

private:
    CooldownManager _cooldowns;
    std::unordered_map<ObjectGuid, uint32> _riptideTargets; // GUID -> expiration time
};

// Earth Shield tracker
class EarthShieldTracker
{
public:
    EarthShieldTracker() = default;

    void ApplyEarthShield(ObjectGuid guid, uint32 duration = 600000)
    {
        _earthShieldTarget = guid;
        _earthShieldEndTime = GameTime::GetGameTimeMS() + duration;
    }

    void RemoveEarthShield()
    {
        _earthShieldTarget = ObjectGuid::Empty;
    }

    [[nodiscard]] bool HasEarthShield(ObjectGuid guid) const
    {
        return _earthShieldTarget == guid && GameTime::GetGameTimeMS() < _earthShieldEndTime;
    }

    [[nodiscard]] ObjectGuid GetEarthShieldTarget() const { return _earthShieldTarget; }

    [[nodiscard]] bool NeedsEarthShieldRefresh(uint32 refreshWindow = 60000) const
    {
        if (_earthShieldTarget.IsEmpty())

            return true;

        uint32 now = GameTime::GetGameTimeMS();
        if (now >= _earthShieldEndTime)

            return true;

        return (_earthShieldEndTime - now) < refreshWindow;
    }

    void Update(Player* bot)
    {
        if (!bot)

            return;

        // Check if Earth Shield expired
        if (!_earthShieldTarget.IsEmpty() && GameTime::GetGameTimeMS() >= _earthShieldEndTime)
        {

            _earthShieldTarget = ObjectGuid::Empty;
        }
    }

private:
    ObjectGuid _earthShieldTarget;
    uint32 _earthShieldEndTime = 0;
};

class RestorationShamanRefactored : public HealerSpecialization<ManaResource>, public ShamanSpecialization
{
public:
    using Base = HealerSpecialization<ManaResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit RestorationShamanRefactored(Player* bot)        : HealerSpecialization<ManaResource>(bot)
        , ShamanSpecialization(bot)
        , _riptideTracker()
        , _earthShieldTracker()
        , _ascendanceActive(false)
        , _ascendanceEndTime(0)
        , _lastAscendanceTime(0)
        , _lastHealingTideTotemTime(0)
        , _lastSpiritLinkTotemTime(0)
        , _lastCloudburstTotemTime(0)
        , _lastEarthenWallTotemTime(0)
        , _lastAncestralProtectionTotemTime(0)
        , _cooldowns()
    {
        // Register cooldowns for major abilities
        _cooldowns.RegisterBatch({

            {RESTO_SHAMAN_HEALING_TIDE_TOTEM, 180000, 1},

            {RESTO_SHAMAN_SPIRIT_LINK_TOTEM, 180000, 1},

            {RESTO_SHAMAN_ASCENDANCE, 180000, 1},

            {RESTO_SHAMAN_CLOUDBURST_TOTEM, 30000, 1}
        });

        // Resource initialization handled by base class CombatSpecializationTemplate

        // Phase 5: Initialize decision systems
        InitializeRestorationShamanMechanics();

        TC_LOG_DEBUG("playerbot", "RestorationShamanRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        Player* bot = this->GetBot();        if (!target || !bot)

            return;

        UpdateRestorationState();

        // Restoration is a healer - check group health first
        if (Group* group = bot->GetGroup())
        
        {

            std::vector<Unit*> groupMembers;

            for (GroupReference const& ref : group->GetMembers())

            {

                if (Player* member = ref.GetSource())
                {

                    if (member->IsAlive() && bot->IsInMap(member))

                        groupMembers.push_back(member);

                }

            }


            if (!groupMembers.empty())

            {

                if (HandleGroupHealing(groupMembers))

                    return;

            }
        }

        // Solo healing (self)
        if (bot->GetHealthPct() < 80.0f)
        {

            if (HandleSelfHealing())

                return;
        }

        // Deal damage when no healing is needed (maintain mana)
        ExecuteDamageRotation(target);
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();
        if (!bot)

            return;

        // Water Shield (mana regeneration)
        if (!bot->HasAura(REST_WATER_SHIELD))
        {

            if (this->CanCastSpell(REST_WATER_SHIELD, bot))

            {

                this->CastSpell(bot, REST_WATER_SHIELD);

            }
        }
    }

    void UpdateDefensives()
    {
        Player* bot = this->GetBot();
        if (!bot)

            return;

        float healthPct = bot->GetHealthPct();

        // Astral Shift (damage reduction)
        if (healthPct < 40.0f && this->CanCastSpell(REST_ASTRAL_SHIFT, bot))
        {

            this->CastSpell(bot, REST_ASTRAL_SHIFT);

            return;
        }

        // Spiritwalker's Grace (heal while moving)
        if (healthPct < 60.0f && bot->HasUnitMovementFlag(MOVEMENTFLAG_FORWARD))
        {

            if (this->CanCastSpell(REST_SPIRITWALKERS_GRACE, bot))

            {

                this->CastSpell(bot, REST_SPIRITWALKERS_GRACE);

                return;

            }
        }
    }

private:
    

    void UpdateRestorationState()
    {
        Player* bot = this->GetBot();
        if (!bot)

            return;

        // ManaResource (uint32) doesn't have Update method - handled by base class
        _riptideTracker.Update(bot);
        _earthShieldTracker.Update(bot);
        UpdateCooldownStates();
    }    void UpdateCooldownStates()
    {
        Player* bot = this->GetBot();
        if (!bot)

            return;

        // Ascendance state (transform into Water Ascendant)
        if (_ascendanceActive && GameTime::GetGameTimeMS() >= _ascendanceEndTime)

            _ascendanceActive = false;

        if (bot->HasAura(REST_ASCENDANCE))
        {

            _ascendanceActive = true;

            if (Aura* aura = bot->GetAura(REST_ASCENDANCE))

                _ascendanceEndTime = GameTime::GetGameTimeMS() + aura->GetDuration();
                }
    }

    bool HandleGroupHealing(const std::vector<Unit*>& group)
    {
        // Emergency cooldowns
        if (HandleEmergencyCooldowns(group))

            return true;

        // Maintain HoTs and shields
        if (HandleHoTs(group))

            return true;

        // AoE healing
        if (HandleAoEHealing(group))

            return true;

        // Direct healing
        if (HandleDirectHealing(group))

            return true;
            return false;
    }

    bool HandleEmergencyCooldowns(const std::vector<Unit*>& group)
    {
        Player* bot = this->GetBot();        if (!bot)

            return false;

        // Ancestral Protection Totem (resurrect on death)
        uint32 criticalHealthCount = 0;
        for (Unit* member : group)
        {

            if (member && member->GetHealthPct() < 20.0f)

                ++criticalHealthCount;
        }

        if (criticalHealthCount >= 2 && (GameTime::GetGameTimeMS() - _lastAncestralProtectionTotemTime) >= 300000) // 5 min CD
        {

            if (bot->HasSpell(REST_ANCESTRAL_PROTECTION_TOTEM))

            {

                if (this->CanCastSpell(REST_ANCESTRAL_PROTECTION_TOTEM, bot))

                {

                    this->CastSpell(bot, REST_ANCESTRAL_PROTECTION_TOTEM);

                    _lastAncestralProtectionTotemTime = GameTime::GetGameTimeMS();

                    return true;

                }

            }
        }

        // Healing Tide Totem (massive raid healing)
        uint32 lowHealthCount = 0;
        for (Unit* member : group)
        {

            if (member && member->GetHealthPct() < 60.0f)

                ++lowHealthCount;
        }

        if (lowHealthCount >= 4 && (GameTime::GetGameTimeMS() - _lastHealingTideTotemTime) >= 180000) // 3 min CD
        {

            if (this->CanCastSpell(REST_HEALING_TIDE_TOTEM, bot))

            {

                this->CastSpell(bot, REST_HEALING_TIDE_TOTEM);

                _lastHealingTideTotemTime = GameTime::GetGameTimeMS();

                return true;

            }
        }

        // Spirit Link Totem (equalize health)
        if (lowHealthCount >= 3 && (GameTime::GetGameTimeMS() - _lastSpiritLinkTotemTime) >= 180000) // 3 min CD        {

            if (this->CanCastSpell(REST_SPIRIT_LINK_TOTEM, bot))

            {

                this->CastSpell(bot, REST_SPIRIT_LINK_TOTEM);

                _lastSpiritLinkTotemTime = GameTime::GetGameTimeMS();

                return true;

            }
        }

        // Ascendance (healing burst mode)
        if (lowHealthCount >= 3 && (GameTime::GetGameTimeMS() - _lastAscendanceTime) >= 180000) // 3 min CD
        {

            if (this->CanCastSpell(REST_ASCENDANCE, bot))

            {

                this->CastSpell(bot, REST_ASCENDANCE);

                _ascendanceActive = true;

                _ascendanceEndTime = GameTime::GetGameTimeMS() + 15000;

                _lastAscendanceTime = GameTime::GetGameTimeMS();

                return true;

            }
        }

        // Earthen Wall Totem (shield wall)
        if (lowHealthCount >= 3 && (GameTime::GetGameTimeMS() - _lastEarthenWallTotemTime) >= 60000) // 60 sec CD

        {
        if (bot->HasSpell(REST_EARTHEN_WALL_TOTEM))

            {

                if (this->CanCastSpell(REST_EARTHEN_WALL_TOTEM, bot))

                if (!tankTarget)

                {

                    return nullptr;

                }

                {

                    this->CastSpell(bot, REST_EARTHEN_WALL_TOTEM);

                    _lastEarthenWallTotemTime = GameTime::GetGameTimeMS();

                    return true;

                }

            if (!tankTarget)

            {

                return;

            }

            }
        }

        return false;
    }

    bool HandleHoTs(const std::vector<Unit*>& group)
    {
        Player* bot = this->GetBot();
        if (!bot)

            return false;
            uint32 activeRiptides = _riptideTracker.GetActiveRiptideCount();

        // Earth Shield on tank
        Unit* tankTarget = nullptr;
        for (Unit* member : group)        {

            if (member && IsTankRole(member))

            {

                tankTarget = member;

                if (!tankTarget)

                {

                    return;

                }

                if (!tankTarget)

                {

                    return;

                }

                break;

            }
        }

        if (tankTarget && !_earthShieldTracker.HasEarthShield(tankTarget->GetGUID()))
        {

            if (this->CanCastSpell(REST_EARTH_SHIELD, tankTarget))

            {

                this->CastSpell(tankTarget, REST_EARTH_SHIELD);

                _earthShieldTracker.ApplyEarthShield(tankTarget->GetGUID(), 600000);

                return true;

            }
        }

        // Riptide on injured allies
        if (activeRiptides < group.size())
        {

            for (Unit* member : group)
            {

                if (member && member->GetHealthPct() < 90.0f)

                {

                    if (_riptideTracker.NeedsRiptideRefresh(member->GetGUID()))

                    {

                        if (this->CanCastSpell(REST_RIPTIDE, member))

                        {

                            this->CastSpell(member, REST_RIPTIDE);

                            _riptideTracker.ApplyRiptide(member->GetGUID(), 18000);

                            return true;

                        }
                        }

                }

            }
        }

        return false;
    }

    bool HandleAoEHealing(const std::vector<Unit*>& group)
    {
        Player* bot = this->GetBot();
        if (!bot)

            return false;

        // Healing Rain (ground AoE HoT)
        uint32 stackedAlliesCount = 0;
        Unit* stackedTarget = nullptr;

        for (Unit* member : group)
        {

            if (member && member->GetHealthPct() < 80.0f)

            {
                // Count nearby allies to this member

                uint32 nearbyCount = 0;

                for (Unit* other : group)

                {

                    if (other && member->GetDistance(other) <= 10.0f && other->GetHealthPct() < 80.0f)

                        ++nearbyCount;

                }


                if (nearbyCount > stackedAlliesCount)

                {

                    stackedAlliesCount = nearbyCount;

                    stackedTarget = member;

                }

            }
            }

        if (stackedAlliesCount >= 3 && stackedTarget)
        {

            if (this->CanCastSpell(REST_HEALING_RAIN, stackedTarget))

            {

                this->CastSpell(stackedTarget, REST_HEALING_RAIN);

                return true;

            }
        }

        // Wellspring (instant AoE heal)
        if (stackedAlliesCount >= 4 && stackedTarget)

        {
        if (bot->HasSpell(REST_WELLSPRING))

            {

                if (this->CanCastSpell(REST_WELLSPRING, stackedTarget))

                {

                    this->CastSpell(stackedTarget, REST_WELLSPRING);

                    return true;

                }

            }
        }

        // Chain Heal (bouncing heal)
        uint32 injuredCount = 0;
        for (Unit* member : group)
        {

            if (member && member->GetHealthPct() < 80.0f)

                ++injuredCount;
        }

        if (injuredCount >= 2)
        {

            for (Unit* member : group)

            {

                if (member && member->GetHealthPct() < 75.0f)

                {

                    if (this->CanCastSpell(REST_CHAIN_HEAL, member))

                    {

                        this->CastSpell(member, REST_CHAIN_HEAL);

                        return true;

                    }

                }

            }
        }

        // Cloudburst Totem (store healing and release)
        if (injuredCount >= 3 && (GameTime::GetGameTimeMS() - _lastCloudburstTotemTime) >= 30000) // 30 sec CD

        {
        if (bot->HasSpell(REST_CLOUDBURST_TOTEM))

            {

                if (this->CanCastSpell(REST_CLOUDBURST_TOTEM, bot))

                {

                    this->CastSpell(bot, REST_CLOUDBURST_TOTEM);

                    _lastCloudburstTotemTime = GameTime::GetGameTimeMS();

                    return true;

                }

            }
        }

        return false;
    }

    bool HandleDirectHealing(const std::vector<Unit*>& group)
    {
        // Healing Surge for emergency
        for (Unit* member : group)
        {

            if (member && member->GetHealthPct() < 50.0f)

            {

                if (this->CanCastSpell(REST_HEALING_SURGE, member))

                {

                    this->CastSpell(member, REST_HEALING_SURGE);

                    return true;

                }

            }
        }

        // Healing Wave (efficient single-target)
        for (Unit* member : group)
        {

            if (member && member->GetHealthPct() < 80.0f)

            {

                if (this->CanCastSpell(REST_HEALING_WAVE, member))

                {

                    this->CastSpell(member, REST_HEALING_WAVE);

                    return true;

                }

            }
            }

        return false;
    }

    bool HandleSelfHealing()
    {
        Player* bot = this->GetBot();
        if (!bot)

            return false;

        // Riptide
        if (_riptideTracker.NeedsRiptideRefresh(bot->GetGUID()))
        {

            if (this->CanCastSpell(REST_RIPTIDE, bot))

            {

                this->CastSpell(bot, REST_RIPTIDE);

                _riptideTracker.ApplyRiptide(bot->GetGUID(), 18000);

                return true;

            }
        }

        // Healing Surge
        if (bot->GetHealthPct() < 60.0f)
        {

            if (this->CanCastSpell(REST_HEALING_SURGE, bot))

            {

                this->CastSpell(bot, REST_HEALING_SURGE);

                return true;

            }
        }

        // Healing Wave
        if (bot->GetHealthPct() < 80.0f)
        {

            if (this->CanCastSpell(REST_HEALING_WAVE, bot))

            {

                this->CastSpell(bot, REST_HEALING_WAVE);

                return true;

            }
        }

        return false;
    }

    void ExecuteDamageRotation(::Unit* target)
    {
        // Restoration Shaman has minimal damage rotation
        // Just maintain mana and deal minimal damage when no healing needed
    }

    [[nodiscard]] bool IsTankRole(Unit* unit) const
    {
        if (!unit)

            return false;

        if (Player* player = unit->ToPlayer())
        {
            // Check if player has tank role based on class
            // Protection Paladin, Protection Warrior, Blood DK, Guardian Druid, Brewmaster Monk, Vengeance DH

            uint8 playerClass = player->GetClass();
            return (playerClass == CLASS_WARRIOR || playerClass == CLASS_PALADIN ||

                    playerClass == CLASS_DEATH_KNIGHT || playerClass == CLASS_DRUID ||

                    playerClass == CLASS_MONK || playerClass == CLASS_DEMON_HUNTER);
        }

        return false;
    }

    // Member variables
    RiptideTracker _riptideTracker;
    EarthShieldTracker _earthShieldTracker;

    bool _ascendanceActive;
    uint32 _ascendanceEndTime;

    uint32 _lastAscendanceTime;
    uint32 _lastHealingTideTotemTime;
    uint32 _lastSpiritLinkTotemTime;
    uint32 _lastCloudburstTotemTime;
    uint32 _lastEarthenWallTotemTime;
    uint32 _lastAncestralProtectionTotemTime;
    CooldownManager _cooldowns;

    // ========================================================================
    // PHASE 5: DECISION SYSTEM INTEGRATION
    // ========================================================================

    void InitializeRestorationShamanMechanics()
    {
        using namespace bot::ai;
        using namespace bot::ai::BehaviorTreeBuilder;

        BotAI* ai = this->GetBot()->GetBotAI();
        if (!ai) return;

        auto* queue = ai->GetActionPriorityQueue();
        if (queue)
        {
            // EMERGENCY: Raid-wide emergency healing

            queue->RegisterSpell(REST_HEALING_TIDE_TOTEM, SpellPriority::EMERGENCY, SpellCategory::HEALING);

            queue->AddCondition(REST_HEALING_TIDE_TOTEM, [this](Player*, Unit*) {

                auto group = this->GetGroupMembers();

                uint32 low = 0;

                for (auto* m : group) if (m && m->GetHealthPct() < 60.0f) low++;

                return low >= 4;

            }, "4+ allies < 60% HP (totem, 3min CD)");


            queue->RegisterSpell(REST_ANCESTRAL_PROTECTION_TOTEM, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);

            queue->AddCondition(REST_ANCESTRAL_PROTECTION_TOTEM, [this](Player* bot, Unit*) {

                if (!bot->HasSpell(REST_ANCESTRAL_PROTECTION_TOTEM)) return false;

                auto group = this->GetGroupMembers();

                uint32 critical = 0;

                for (auto* m : group) if (m && m->GetHealthPct() < 20.0f) critical++;

                return critical >= 2;

            }, "2+ allies < 20% HP (resurrect totem, 5min CD)");

            // CRITICAL: Major healing cooldowns

            queue->RegisterSpell(REST_ASCENDANCE, SpellPriority::CRITICAL, SpellCategory::HEALING);

            queue->AddCondition(REST_ASCENDANCE, [this](Player*, Unit*) {

                if (this->_ascendanceActive) return false;

                auto group = this->GetGroupMembers();

                uint32 injured = 0;

                for (auto* m : group) if (m && m->GetHealthPct() < 60.0f) injured++;

                return injured >= 3;

            }, "3+ allies < 60% HP (15s burst, 3min CD)");


            queue->RegisterSpell(REST_SPIRIT_LINK_TOTEM, SpellPriority::CRITICAL, SpellCategory::HEALING);

            queue->AddCondition(REST_SPIRIT_LINK_TOTEM, [this](Player*, Unit*) {

                auto group = this->GetGroupMembers();

                uint32 injured = 0;

                for (auto* m : group) if (m && m->GetHealthPct() < 60.0f) injured++;

                return injured >= 3;

            }, "3+ allies < 60% HP (equalize health, 3min CD)");


            queue->RegisterSpell(REST_EARTHEN_WALL_TOTEM, SpellPriority::CRITICAL, SpellCategory::DEFENSIVE);

            queue->AddCondition(REST_EARTHEN_WALL_TOTEM, [this](Player* bot, Unit*) {

                if (!bot->HasSpell(REST_EARTHEN_WALL_TOTEM)) return false;

                auto group = this->GetGroupMembers();

                uint32 injured = 0;

                for (auto* m : group) if (m && m->GetHealthPct() < 60.0f) injured++;

                return injured >= 3;

            }, "3+ allies < 60% HP (shield wall, 60s CD)");

            // HIGH: Core HoT and shield maintenance

            queue->RegisterSpell(REST_EARTH_SHIELD, SpellPriority::HIGH, SpellCategory::DEFENSIVE);

            queue->AddCondition(REST_EARTH_SHIELD, [this](Player*, Unit*) {

                auto group = this->GetGroupMembers();

                for (auto* m : group) {

                    if (m && this->IsTankRole(m) && !this->_earthShieldTracker.HasEarthShield(m->GetGUID()))

                        return true;

                }

                return false;

            }, "Tank needs Earth Shield (10min)");


            queue->RegisterSpell(REST_RIPTIDE, SpellPriority::HIGH, SpellCategory::HEALING);

            queue->AddCondition(REST_RIPTIDE, [this](Player*, Unit*) {

                auto group = this->GetGroupMembers();

                for (auto* m : group) {

                    if (m && m->GetHealthPct() < 90.0f && this->_riptideTracker.NeedsRiptideRefresh(m->GetGUID()))

                        return true;

                }

                return false;

            }, "Ally < 90% HP needs Riptide (18s HoT)");

            // MEDIUM: AoE healing

            queue->RegisterSpell(REST_HEALING_RAIN, SpellPriority::MEDIUM, SpellCategory::HEALING);

            queue->AddCondition(REST_HEALING_RAIN, [this](Player*, Unit*) {

                auto group = this->GetGroupMembers();

                for (auto* anchor : group) {

                    if (!anchor || anchor->GetHealthPct() >= 80.0f) continue;

                    uint32 nearby = 0;

                    for (auto* m : group)

                        if (m && anchor->GetDistance(m) <= 10.0f && m->GetHealthPct() < 80.0f)

                            nearby++;

                    if (nearby >= 3) return true;

                }

                return false;

            }, "3+ stacked allies < 80% HP (ground AoE)");


            queue->RegisterSpell(REST_WELLSPRING, SpellPriority::MEDIUM, SpellCategory::HEALING);

            queue->AddCondition(REST_WELLSPRING, [this](Player* bot, Unit*) {

                if (!bot->HasSpell(REST_WELLSPRING)) return false;

                auto group = this->GetGroupMembers();

                for (auto* anchor : group) {

                    if (!anchor || anchor->GetHealthPct() >= 80.0f) continue;

                    uint32 nearby = 0;

                    for (auto* m : group)

                        if (m && anchor->GetDistance(m) <= 10.0f && m->GetHealthPct() < 80.0f)

                            nearby++;

                    if (nearby >= 4) return true;

                }

                return false;

            }, "4+ stacked allies < 80% HP (instant AoE)");


            queue->RegisterSpell(REST_CHAIN_HEAL, SpellPriority::MEDIUM, SpellCategory::HEALING);

            queue->AddCondition(REST_CHAIN_HEAL, [this](Player*, Unit*) {

                auto group = this->GetGroupMembers();

                uint32 injured = 0;

                for (auto* m : group) if (m && m->GetHealthPct() < 80.0f) injured++;

                return injured >= 2;

            }, "2+ allies < 80% HP (bouncing heal)");


            queue->RegisterSpell(REST_CLOUDBURST_TOTEM, SpellPriority::MEDIUM, SpellCategory::HEALING);

            queue->AddCondition(REST_CLOUDBURST_TOTEM, [this](Player* bot, Unit*) {

                if (!bot->HasSpell(REST_CLOUDBURST_TOTEM)) return false;

                auto group = this->GetGroupMembers();

                uint32 injured = 0;

                for (auto* m : group) if (m && m->GetHealthPct() < 80.0f) injured++;

                return injured >= 3;

            }, "3+ allies < 80% HP (store + release heal, 30s CD)");

            // LOW: Direct single-target healing

            queue->RegisterSpell(REST_HEALING_SURGE, SpellPriority::LOW, SpellCategory::HEALING);

            queue->AddCondition(REST_HEALING_SURGE, [this](Player*, Unit*) {

                auto group = this->GetGroupMembers();

                for (auto* m : group) if (m && m->GetHealthPct() < 50.0f) return true;

                return false;

            }, "Ally < 50% HP (fast emergency heal)");


            queue->RegisterSpell(REST_HEALING_WAVE, SpellPriority::LOW, SpellCategory::HEALING);

            queue->AddCondition(REST_HEALING_WAVE, [this](Player*, Unit*) {

                auto group = this->GetGroupMembers();

                for (auto* m : group) if (m && m->GetHealthPct() < 80.0f) return true;

                return false;

            }, "Ally < 80% HP (efficient heal)");

            // UTILITY: Defensive and buffs

            queue->RegisterSpell(REST_ASTRAL_SHIFT, SpellPriority::EMERGENCY, SpellCategory::DEFENSIVE);

            queue->AddCondition(REST_ASTRAL_SHIFT, [](Player* bot, Unit*) {

                return bot && bot->GetHealthPct() < 40.0f;

            }, "HP < 40% (40% dmg reduction)");


            queue->RegisterSpell(REST_SPIRITWALKERS_GRACE, SpellPriority::HIGH, SpellCategory::UTILITY);

            queue->AddCondition(REST_SPIRITWALKERS_GRACE, [](Player* bot, Unit*) {

                return bot && bot->GetHealthPct() < 60.0f && bot->HasUnitMovementFlag(MOVEMENTFLAG_FORWARD);

            }, "HP < 60% while moving (heal while moving)");


            queue->RegisterSpell(REST_WATER_SHIELD, SpellPriority::LOW, SpellCategory::UTILITY);

            queue->AddCondition(REST_WATER_SHIELD, [](Player* bot, Unit*) {

                return bot && !bot->HasAura(REST_WATER_SHIELD);

            }, "No Water Shield (mana regen)");


            queue->RegisterSpell(REST_WIND_SHEAR, SpellPriority::HIGH, SpellCategory::CROWD_CONTROL);

            queue->AddCondition(REST_WIND_SHEAR, [](Player*, Unit* target) {

                return target && target->IsNonMeleeSpellCast(false);

            }, "Target casting (interrupt)");


            queue->RegisterSpell(REST_PURIFY_SPIRIT, SpellPriority::MEDIUM, SpellCategory::UTILITY);

            queue->AddCondition(REST_PURIFY_SPIRIT, [this](Player*, Unit*) {

                auto group = this->GetGroupMembers();

                for (auto* m : group) {

                    if (m && (m->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE) ||

                               m->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED)))

                        return true;

                }

                return false;

            }, "Ally has curse/magic/poison (dispel)");
        }

        auto* behaviorTree = ai->GetBehaviorTree();
        if (behaviorTree)
        {

            auto root = Selector("Restoration Shaman Healing", {
                // Tier 1: Emergency Raid Healing

                Sequence("Emergency Totems", {

                    Condition("4+ low HP", [this](Player*) {

                        auto group = this->GetGroupMembers();

                        uint32 low = 0;

                        for (auto* m : group) if (m && m->GetHealthPct() < 60.0f) low++;

                        return low >= 4;

                    }),

                    Selector("Use emergency", {

                        Sequence("Healing Tide Totem", {

                            Action("Cast HTT", [this](Player* bot) {

                                if (this->CanCastSpell(REST_HEALING_TIDE_TOTEM, bot)) {

                                    this->CastSpell(bot, REST_HEALING_TIDE_TOTEM);

                                    this->_lastHealingTideTotemTime = GameTime::GetGameTimeMS();

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),

                        Sequence("Ancestral Protection", {

                            Condition("2+ critical", [this](Player*) {

                                auto group = this->GetGroupMembers();

                                uint32 critical = 0;

                                for (auto* m : group) if (m && m->GetHealthPct() < 20.0f) critical++;

                                return critical >= 2;

                            }),

                            Condition("Has spell", [this](Player* bot) {

                                return bot->HasSpell(REST_ANCESTRAL_PROTECTION_TOTEM);

                            }),

                            Action("Cast APT", [this](Player* bot) {

                                if (this->CanCastSpell(REST_ANCESTRAL_PROTECTION_TOTEM, bot)) {

                                    this->CastSpell(bot, REST_ANCESTRAL_PROTECTION_TOTEM);

                                    this->_lastAncestralProtectionTotemTime = GameTime::GetGameTimeMS();

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        })

                    })

                }),

                // Tier 2: Major Healing Cooldowns

                Sequence("Major Cooldowns", {

                    Condition("3+ injured", [this](Player*) {

                        auto group = this->GetGroupMembers();

                        uint32 injured = 0;

                        for (auto* m : group) if (m && m->GetHealthPct() < 60.0f) injured++;

                        return injured >= 3;

                    }),

                    Selector("Use cooldowns", {

                        Sequence("Ascendance", {

                            Condition("Not active", [this](Player*) {

                                return !this->_ascendanceActive;

                            }),

                            Action("Cast Ascendance", [this](Player* bot) {

                                if (this->CanCastSpell(REST_ASCENDANCE, bot)) {

                                    this->CastSpell(bot, REST_ASCENDANCE);

                                    this->_ascendanceActive = true;

                                    this->_ascendanceEndTime = GameTime::GetGameTimeMS() + 15000;

                                    this->_lastAscendanceTime = GameTime::GetGameTimeMS();

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),

                        Sequence("Spirit Link Totem", {

                            Action("Cast SLT", [this](Player* bot) {

                                if (this->CanCastSpell(REST_SPIRIT_LINK_TOTEM, bot)) {

                                    this->CastSpell(bot, REST_SPIRIT_LINK_TOTEM);

                                    this->_lastSpiritLinkTotemTime = GameTime::GetGameTimeMS();

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),

                        Sequence("Earthen Wall Totem", {

                            Condition("Has spell", [this](Player* bot) {

                                return bot->HasSpell(REST_EARTHEN_WALL_TOTEM);

                            }),

                            Action("Cast EWT", [this](Player* bot) {

                                if (this->CanCastSpell(REST_EARTHEN_WALL_TOTEM, bot)) {

                                    this->CastSpell(bot, REST_EARTHEN_WALL_TOTEM);

                                    this->_lastEarthenWallTotemTime = GameTime::GetGameTimeMS();

                                    return NodeStatus::SUCCESS;

                                }

                                return NodeStatus::FAILURE;

                            })

                        })

                    })

                }),

                // Tier 3: HoT and Shield Maintenance

                Sequence("Maintain HoTs", {

                    Selector("Apply HoTs", {

                        Sequence("Earth Shield Tank", {

                            Action("Cast Earth Shield", [this](Player*) {

                                auto group = this->GetGroupMembers();

                                for (auto* m : group) {

                                    if (m && this->IsTankRole(m) && !this->_earthShieldTracker.HasEarthShield(m->GetGUID())) {

                                        if (this->CanCastSpell(REST_EARTH_SHIELD, m)) {

                                            this->CastSpell(m, REST_EARTH_SHIELD);

                                            this->_earthShieldTracker.ApplyEarthShield(m->GetGUID(), 600000);

                                            return NodeStatus::SUCCESS;

                                        }

                                    }

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),

                        Sequence("Riptide Spread", {

                            Action("Cast Riptide", [this](Player*) {

                                auto group = this->GetGroupMembers();

                                for (auto* m : group) {

                                    if (m && m->GetHealthPct() < 90.0f && this->_riptideTracker.NeedsRiptideRefresh(m->GetGUID())) {

                                        if (this->CanCastSpell(REST_RIPTIDE, m)) {

                                            this->CastSpell(m, REST_RIPTIDE);

                                            this->_riptideTracker.ApplyRiptide(m->GetGUID(), 18000);

                                            return NodeStatus::SUCCESS;

                                        }

                                    }

                                }

                                return NodeStatus::FAILURE;

                            })

                        })

                    })

                }),

                // Tier 4: AoE Healing

                Sequence("AoE Healing", {

                    Condition("2+ injured", [this](Player*) {

                        auto group = this->GetGroupMembers();

                        uint32 injured = 0;

                        for (auto* m : group) if (m && m->GetHealthPct() < 80.0f) injured++;

                        return injured >= 2;

                    }),

                    Selector("Cast AoE", {

                        Sequence("Healing Rain", {

                            Condition("3+ stacked", [this](Player*) {

                                auto group = this->GetGroupMembers();

                                for (auto* anchor : group) {

                                    if (!anchor || anchor->GetHealthPct() >= 80.0f) continue;

                                    uint32 nearby = 0;

                                    for (auto* m : group)

                                        if (m && anchor->GetDistance(m) <= 10.0f && m->GetHealthPct() < 80.0f)

                                            nearby++;

                                    if (nearby >= 3) return true;

                                }

                                return false;

                            }),

                            Action("Cast Healing Rain", [this](Player*) {

                                auto group = this->GetGroupMembers();

                                for (auto* anchor : group) {

                                    if (!anchor || anchor->GetHealthPct() >= 80.0f) continue;

                                    uint32 nearby = 0;

                                    for (auto* m : group)

                                        if (m && anchor->GetDistance(m) <= 10.0f && m->GetHealthPct() < 80.0f)

                                            nearby++;

                                    if (nearby >= 3 && this->CanCastSpell(REST_HEALING_RAIN, anchor)) {

                                        this->CastSpell(anchor, REST_HEALING_RAIN);

                                        return NodeStatus::SUCCESS;

                                    }

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),

                        Sequence("Chain Heal", {

                            Action("Cast Chain Heal", [this](Player*) {

                                auto group = this->GetGroupMembers();

                                for (auto* m : group) {

                                    if (m && m->GetHealthPct() < 75.0f) {

                                        if (this->CanCastSpell(REST_CHAIN_HEAL, m)) {

                                            this->CastSpell(m, REST_CHAIN_HEAL);

                                            return NodeStatus::SUCCESS;

                                        }

                                    }

                                }

                                return NodeStatus::FAILURE;

                            })

                        })

                    })

                }),

                // Tier 5: Direct Healing

                Sequence("Direct Healing", {

                    Selector("Cast heals", {

                        Sequence("Healing Surge", {

                            Condition("Ally < 50%", [this](Player*) {

                                auto group = this->GetGroupMembers();

                                for (auto* m : group) if (m && m->GetHealthPct() < 50.0f) return true;

                                return false;

                            }),

                            Action("Cast Healing Surge", [this](Player*) {

                                auto group = this->GetGroupMembers();

                                for (auto* m : group) {

                                    if (m && m->GetHealthPct() < 50.0f) {

                                        if (this->CanCastSpell(REST_HEALING_SURGE, m)) {

                                            this->CastSpell(m, REST_HEALING_SURGE);

                                            return NodeStatus::SUCCESS;

                                        }

                                    }

                                }

                                return NodeStatus::FAILURE;

                            })

                        }),

                        Sequence("Healing Wave", {

                            Action("Cast Healing Wave", [this](Player*) {

                                auto group = this->GetGroupMembers();

                                for (auto* m : group) {

                                    if (m && m->GetHealthPct() < 80.0f) {

                                        if (this->CanCastSpell(REST_HEALING_WAVE, m)) {

                                            this->CastSpell(m, REST_HEALING_WAVE);

                                            return NodeStatus::SUCCESS;

                                        }

                                    }

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
};

} // namespace Playerbot

#endif // PLAYERBOT_RESTORATIONSHAMANREFACTORED_H
