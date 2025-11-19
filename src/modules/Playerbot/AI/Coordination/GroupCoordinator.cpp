/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
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

#include "GroupCoordinator.h"
#include "Group.h"
#include "Player.h"
#include "Unit.h"
#include "Log.h"
#include "ObjectAccessor.h"

namespace Playerbot
{
namespace Coordination
{

// ============================================================================
// TacticalAssignment
// ============================================================================

bool TacticalAssignment::IsExpired() const
{
    return GameTime::GetGameTimeMS() > expirationTime;
}

bool TacticalAssignment::IsValid() const
{
    return !targetGuid.IsEmpty() && !IsExpired();
}

// ============================================================================
// GroupCoordinator
// ============================================================================

GroupCoordinator::GroupCoordinator(Group* group)
    : _group(group)
{

    TC_LOG_DEBUG("playerbot.coordination", "GroupCoordinator created for group {}", _group->GetGUID().ToString());

    // Initialize tactical state
    _tacticalState.lastUpdateTime = GameTime::GetGameTimeMS();
}

void GroupCoordinator::Update(uint32 diff)
{
    if (!_group)
        return;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Throttle updates
    _lastUpdateTime += diff;
    if (_lastUpdateTime < _updateInterval)
        return;

    _lastUpdateTime = 0;
    _totalUpdates++;

    // Update tactical state timestamp
    _tacticalState.lastUpdateTime = GameTime::GetGameTimeMS();

    // Update role assignments
    UpdateRoleAssignments();

    // Check combat state
    bool wasInCombat = _tacticalState.inCombat;
    _tacticalState.inCombat = false;

    for (GroupReference& ref : _group->GetMembers())
    {
        Player* member = ref.GetSource();
        if (member && member->IsInCombat())
        {
            _tacticalState.inCombat = true;
            break;
        }
    }

    // Track combat start
    if (_tacticalState.inCombat && !wasInCombat)
    {
        _tacticalState.combatStartTime = GameTime::GetGameTimeMS();
        TC_LOG_DEBUG("playerbot.coordination", "Group {} entered combat", _group->GetGUID().ToString());
    }

    // Update coordination only if in combat
    if (_tacticalState.inCombat)
    {
        UpdateFocusTarget();
        UpdateInterruptRotation();
        UpdateDispelAssignments();
    }

    // Clean up expired data
    CleanupExpiredData();

    // Track performance
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    _totalUpdateTime += duration.count();

    if (_totalUpdates % 100 == 0)
    {
        uint64 avgTime = _totalUpdateTime / _totalUpdates;
        TC_LOG_TRACE("playerbot.coordination", "GroupCoordinator average update time: {}μs", avgTime);
    }
}

GroupRole GroupCoordinator::GetBotRole(ObjectGuid botGuid) const
{
    auto it = _roleAssignments.find(botGuid);
    if (it != _roleAssignments.end())
        return it->second;

    return GroupRole::UNKNOWN;
}

void GroupCoordinator::SetFocusTarget(ObjectGuid targetGuid)
{
    if (_tacticalState.focusTarget != targetGuid)
    {
        _tacticalState.focusTarget = targetGuid;
        TC_LOG_DEBUG("playerbot.coordination", "Group {} focus target changed to {}",
            _group->GetGUID().ToString(), targetGuid.ToString());
    }
}

TacticalAssignment const* GroupCoordinator::GetAssignment(ObjectGuid botGuid) const
{
    auto it = _assignments.find(botGuid);
    if (it != _assignments.end() && it->second.IsValid())
        return &it->second;

    return nullptr;
}

ObjectGuid GroupCoordinator::AssignInterrupt(ObjectGuid targetGuid)
{
    ObjectGuid interrupter = GetNextInterrupter();

    if (!interrupter.IsEmpty())
    {
        TacticalAssignment assignment;
        assignment.targetGuid = targetGuid;
        assignment.priority = 90; // High priority
        assignment.taskType = "interrupt";
        assignment.timestamp = GameTime::GetGameTimeMS();
        assignment.expirationTime = assignment.timestamp + 5000; // 5 second window

        _assignments[interrupter] = assignment;
        _tacticalState.interruptQueue[interrupter] = GameTime::GetGameTimeMS() + 20000; // 20s CD

        TC_LOG_DEBUG("playerbot.coordination", "Assigned interrupt to {} for target {}",
            interrupter.ToString(), targetGuid.ToString());
    }

    return interrupter;
}

ObjectGuid GroupCoordinator::AssignDispel(ObjectGuid targetGuid)
{
    // Find healer with dispel available
    std::vector<ObjectGuid> healers = GetBotsByRole(GroupRole::HEALER);

    for (ObjectGuid healerGuid : healers)
    {
        // Check if healer is already assigned
        auto it = _tacticalState.dispelAssignments.find(healerGuid);
        if (it != _tacticalState.dispelAssignments.end())
            continue; // Already assigned

        // Assign dispel
        TacticalAssignment assignment;
        assignment.targetGuid = targetGuid;
        assignment.priority = 80;
        assignment.taskType = "dispel";
        assignment.timestamp = GameTime::GetGameTimeMS();
        assignment.expirationTime = assignment.timestamp + 3000; // 3 second window

        _assignments[healerGuid] = assignment;
        _tacticalState.dispelAssignments[healerGuid] = targetGuid;

        TC_LOG_DEBUG("playerbot.coordination", "Assigned dispel to {} for target {}",
            healerGuid.ToString(), targetGuid.ToString());

        return healerGuid;
    }

    return ObjectGuid::Empty;
}

bool GroupCoordinator::IsGroupCooldownAvailable(std::string const& cooldownName) const
{
    auto it = _tacticalState.groupCooldowns.find(cooldownName);
    if (it == _tacticalState.groupCooldowns.end())
        return true;

    return GameTime::GetGameTimeMS() > it->second;
}

void GroupCoordinator::UseGroupCooldown(std::string const& cooldownName, uint32 durationMs)
{
    _tacticalState.groupCooldowns[cooldownName] = GameTime::GetGameTimeMS() + durationMs;

    TC_LOG_DEBUG("playerbot.coordination", "Group {} used cooldown: {} ({}ms)",
        _group->GetGUID().ToString(), cooldownName, durationMs);
}

uint32 GroupCoordinator::GetRoleCount(GroupRole role) const
{
    uint32 count = 0;

    for (auto const& pair : _roleAssignments)
    {
        if (pair.second == role)
            count++;
    }

    return count;
}

std::vector<ObjectGuid> GroupCoordinator::GetBotsByRole(GroupRole role) const
{
    std::vector<ObjectGuid> bots;

    for (auto const& pair : _roleAssignments)
    {
        if (pair.second == role)
            bots.push_back(pair.first);
    }

    return bots;
}

uint32 GroupCoordinator::GetCombatDuration() const
{
    if (!_tacticalState.inCombat || _tacticalState.combatStartTime == 0)
        return 0;

    return getMSTimeDiff(_tacticalState.combatStartTime, GameTime::GetGameTimeMS());
}

void GroupCoordinator::UpdateRoleAssignments()
{
    _roleAssignments.clear();

    for (GroupReference& ref : _group->GetMembers())
    {
        Player* member = ref.GetSource();
        if (!member)
            continue;

        GroupRole role = DetectRole(member);
        _roleAssignments[member->GetGUID()] = role;
    }

    TC_LOG_TRACE("playerbot.coordination", "Group {} role distribution: T={} H={} MDPS={} RDPS={}",
        _group->GetGUID().ToString(),
        GetRoleCount(GroupRole::TANK),
        GetRoleCount(GroupRole::HEALER),
        GetRoleCount(GroupRole::MELEE_DPS),
        GetRoleCount(GroupRole::RANGED_DPS));
}

void GroupCoordinator::UpdateFocusTarget()
{
    // Find best focus target
    ObjectGuid newFocus = FindBestFocusTarget();

    if (newFocus != _tacticalState.focusTarget)
    {
        SetFocusTarget(newFocus);
    }
}

void GroupCoordinator::UpdateInterruptRotation()
{
    // Clean up expired interrupt CDs
    uint32 now = GameTime::GetGameTimeMS();

    for (auto it = _tacticalState.interruptQueue.begin(); it != _tacticalState.interruptQueue.end();)
    {
        if (now > it->second)
            it = _tacticalState.interruptQueue.erase(it);
        else
            ++it;
    }
}

void GroupCoordinator::UpdateDispelAssignments()
{
    // Clean up completed dispels
    for (auto it = _tacticalState.dispelAssignments.begin(); it != _tacticalState.dispelAssignments.end();)
    {
        // Check if assignment is still valid
        auto assignmentIt = _assignments.find(it->first);
        if (assignmentIt == _assignments.end() || !assignmentIt->second.IsValid())
        {
            it = _tacticalState.dispelAssignments.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void GroupCoordinator::CleanupExpiredData()
{
    uint32 now = GameTime::GetGameTimeMS();

    // Clean up expired assignments
    for (auto it = _assignments.begin(); it != _assignments.end();)
    {
        if (it->second.IsExpired())
            it = _assignments.erase(it);
        else
            ++it;
    }

    // Clean up expired group cooldowns
    for (auto it = _tacticalState.groupCooldowns.begin(); it != _tacticalState.groupCooldowns.end();)
    {
        if (now > it->second)
            it = _tacticalState.groupCooldowns.erase(it);
        else
            ++it;
    }
}

GroupRole GroupCoordinator::DetectRole(Player* player) const
{
    if (!player)
        return GroupRole::UNKNOWN;

    uint8 classId = player->getClass();
    uint8 spec = player->GetPrimaryTalentTree(player->GetActiveSpec());

    // Tank specs
    if ((classId == CLASS_WARRIOR && spec == 2) ||     // Protection Warrior
        (classId == CLASS_PALADIN && spec == 1) ||     // Protection Paladin
        (classId == CLASS_DEATH_KNIGHT && spec == 0) || // Blood DK
        (classId == CLASS_DRUID && spec == 1) ||       // Guardian Druid
        (classId == CLASS_MONK && spec == 0))          // Brewmaster Monk
    {
        return GroupRole::TANK;
    }

    // Healer specs
    if ((classId == CLASS_PRIEST && (spec == 0 || spec == 1)) || // Disc/Holy Priest
        (classId == CLASS_PALADIN && spec == 0) ||     // Holy Paladin
        (classId == CLASS_SHAMAN && spec == 2) ||      // Resto Shaman
        (classId == CLASS_DRUID && spec == 3) ||       // Resto Druid
        (classId == CLASS_MONK && spec == 1))          // Mistweaver Monk
    {
        return GroupRole::HEALER;
    }

    // Ranged DPS
    if (classId == CLASS_MAGE ||
        classId == CLASS_WARLOCK ||
        (classId == CLASS_HUNTER && spec != 2) || // Not Survival
        (classId == CLASS_PRIEST && spec == 2) || // Shadow
        (classId == CLASS_SHAMAN && spec == 0))   // Elemental
    {
        return GroupRole::RANGED_DPS;
    }

    // Default to melee DPS
    return GroupRole::MELEE_DPS;
}

ObjectGuid GroupCoordinator::FindBestFocusTarget() const
{
    // Find target with highest threat/priority
    ObjectGuid bestTarget;
    float highestPriority = 0.0f;

    for (GroupReference& ref : _group->GetMembers())
    {
        Player* member = ref.GetSource();
        if (!member || !member->IsInCombat())
            continue;

        Unit* target = member->GetSelectedUnit();
        if (!target || !target->IsAlive())
            continue;

        // Calculate priority (simplified)
        float priority = 50.0f; // Base priority

        // Prioritize targets attacking healers
        if (target->GetVictim() && target->GetVictim()->IsPlayer())
        {
            Player* victim = target->GetVictim()->ToPlayer();
            GroupRole victimRole = GetBotRole(victim->GetGUID());
            if (victimRole == GroupRole::HEALER)
                priority += 30.0f;
            else if (victimRole == GroupRole::TANK)
                priority += 10.0f;
        }

        // Prioritize low health targets
        float healthPct = target->GetHealthPct();
        if (healthPct < 30.0f)
            priority += 20.0f;
        else if (healthPct < 50.0f)
            priority += 10.0f;

        if (priority > highestPriority)
        {
            highestPriority = priority;
            bestTarget = target->GetGUID();
        }
    }

    return bestTarget;
}

ObjectGuid GroupCoordinator::GetNextInterrupter() const
{
    uint32 now = GameTime::GetGameTimeMS();

    // Find bot with interrupt ready
    std::vector<ObjectGuid> mdps = GetBotsByRole(GroupRole::MELEE_DPS);
    std::vector<ObjectGuid> rdps = GetBotsByRole(GroupRole::RANGED_DPS);

    // Combine all DPS
    std::vector<ObjectGuid> allDps;
    allDps.insert(allDps.end(), mdps.begin(), mdps.end());
    allDps.insert(allDps.end(), rdps.begin(), rdps.end());

    for (ObjectGuid botGuid : allDps)
    {
        auto it = _tacticalState.interruptQueue.find(botGuid);
        if (it == _tacticalState.interruptQueue.end() || now > it->second)
        {
            // Interrupt is ready
            return botGuid;
        }
    }

    return ObjectGuid::Empty;
}

} // namespace Playerbot
