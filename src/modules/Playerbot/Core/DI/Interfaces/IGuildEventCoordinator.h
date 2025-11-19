/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include "Player.h"
#include "Group.h"
#include <vector>
#include <string>

namespace Playerbot
{

// Forward declarations
struct GuildEvent;
enum class GuildEventType : uint8;
enum class EventStatus : uint8;
struct EventCoordinationProfile;
struct EventParticipation;
struct EventMetrics;

class TC_GAME_API IGuildEventCoordinator
{
public:
    virtual ~IGuildEventCoordinator() = default;

    // Core event management using TrinityCore's Calendar system
    virtual uint32 CreateGuildEvent(Player* organizer, const GuildEvent& eventData) = 0;
    virtual bool UpdateGuildEvent(uint32 eventId, const GuildEvent& updatedData) = 0;
    virtual bool CancelGuildEvent(Player* organizer, uint32 eventId) = 0;
    virtual void ProcessEventInvitations(uint32 eventId) = 0;

    // Event planning and scheduling
    virtual void PlanGuildEvents() = 0;
    virtual void ScheduleRecurringEvents() = 0;

    // Event recruitment and coordination
    virtual void RecruitEventParticipants(Player* organizer, uint32 eventId) = 0;
    virtual void ManageEventSignups(uint32 eventId) = 0;
    virtual void AssignEventRoles(uint32 eventId) = 0;

    // Event execution and management
    virtual void ExecuteGuildEvent(uint32 eventId) = 0;
    virtual void CoordinateEventActivities(Player* leader, uint32 eventId) = 0;
    virtual void MonitorEventProgress(uint32 eventId) = 0;
    virtual void HandleEventCompletion(uint32 eventId) = 0;

    // Event profiles
    virtual void SetEventProfile(const EventCoordinationProfile& profile) = 0;
    virtual EventCoordinationProfile GetEventProfile() = 0;

    // Event analytics
    virtual EventParticipation GetEventParticipation() = 0;

    // Event optimization
    virtual void OptimizeEventScheduling() = 0;
    virtual void AnalyzeGuildEventPatterns(uint32 guildId) = 0;

    // Event communication
    virtual void BroadcastEventUpdates(uint32 eventId, const std::string& updateMessage) = 0;
    virtual void SendEventReminders(uint32 eventId) = 0;
    virtual void UpdateEventStatus(uint32 eventId, EventStatus newStatus) = 0;

    // Group formation
    virtual Group* FormEventGroup(uint32 eventId) = 0;
    virtual void AssignGroupRoles(Group* group, uint32 eventId) = 0;

    // Performance monitoring
    virtual EventMetrics GetGuildEventMetrics(uint32 guildId) = 0;
    virtual EventMetrics GetPlayerEventMetrics() = 0;

    // Configuration
    virtual void SetEventCoordinationEnabled(uint32 guildId, bool enabled) = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void UpdateEventStates() = 0;
    virtual void CleanupExpiredEvents() = 0;
};

} // namespace Playerbot
