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
#include "Threading/LockHierarchy.h"
#include "Player.h"
#include "Guild.h"
#include "Group.h"
#include "Calendar.h"
#include "../Core/DI/Interfaces/IGuildEventCoordinator.h"
#include <unordered_map>
#include <vector>
#include <queue>
#include <atomic>
#include <mutex>
#include <chrono>

namespace Playerbot
{

enum class GuildEventType : uint8
{
    RAID_DUNGEON        = 0,
    PVP_BATTLEGROUND    = 1,
    PVP_ARENA           = 2,
    GUILD_MEETING       = 3,
    SOCIAL_GATHERING    = 4,
    ACHIEVEMENT_RUN     = 5,
    LEVELING_GROUP      = 6,
    CRAFTING_SESSION    = 7,
    CONTEST_COMPETITION = 8,
    OFFICER_MEETING     = 9
};

enum class EventStatus : uint8
{
    PLANNING        = 0,
    RECRUITING      = 1,
    CONFIRMED       = 2,
    IN_PROGRESS     = 3,
    COMPLETED       = 4,
    CANCELLED       = 5,
    POSTPONED       = 6
};

enum class EventPriority : uint8
{
    LOW             = 0,
    NORMAL          = 1,
    HIGH            = 2,
    CRITICAL        = 3
};

struct GuildEvent
{
    uint32 eventId;
    uint32 guildId;
    uint32 organizerGuid;
    std::string organizerName;
    std::string eventTitle;
    std::string eventDescription;
    GuildEventType eventType;
    EventStatus status;
    EventPriority priority;
    uint32 scheduledTime;
    uint32 duration;
    uint32 maxParticipants;
    uint32 minParticipants;
    std::vector<uint32> invitedMembers;
    std::vector<uint32> confirmedMembers;
    std::vector<uint32> declinedMembers;
    std::unordered_map<uint32, std::string> memberRoles; // memberGuid -> role
    uint32 creationTime;
    uint32 lastUpdateTime;
    bool isRecurring;
    uint32 recurringInterval;

    GuildEvent() : eventId(0), guildId(0), organizerGuid(0)
        , eventType(GuildEventType::SOCIAL_GATHERING), status(EventStatus::PLANNING)
        , priority(EventPriority::NORMAL), scheduledTime(0), duration(7200000) // 2 hours
        , maxParticipants(40), minParticipants(5), creationTime(GameTime::GetGameTimeMS())
        , lastUpdateTime(GameTime::GetGameTimeMS()), isRecurring(false), recurringInterval(0) {}
};

/**
 * @brief Advanced guild event coordination system for automated event management
 *
 * This system provides intelligent guild event planning, scheduling, coordination,
 * and management for playerbots using TrinityCore's calendar and guild systems.
 */
class TC_GAME_API GuildEventCoordinator final : public IGuildEventCoordinator
{
public:
    static GuildEventCoordinator* instance();

    // Core event management using TrinityCore's Calendar system
    uint32 CreateGuildEvent(Player* organizer, const GuildEvent& eventData) override;
    bool UpdateGuildEvent(uint32 eventId, const GuildEvent& updatedData) override;
    bool CancelGuildEvent(Player* organizer, uint32 eventId) override;
    void ProcessEventInvitations(uint32 eventId) override;

    // Event planning and scheduling
    void PlanGuildEvents(Player* player) override;
    void ScheduleRecurringEvents(Player* player) override;
    void ProposeEventIdeas(Player* player);
    void CoordinateEventTiming(Player* player, uint32 eventId);

    // Event recruitment and coordination
    void RecruitEventParticipants(Player* organizer, uint32 eventId) override;
    void ManageEventSignups(Player* player, uint32 eventId) override;
    void AssignEventRoles(Player* player, uint32 eventId) override;
    void HandleEventChanges(Player* player, uint32 eventId);

    // Event execution and management
    void ExecuteGuildEvent(uint32 eventId) override;
    void CoordinateEventActivities(Player* leader, uint32 eventId) override;
    void MonitorEventProgress(uint32 eventId) override;
    void HandleEventCompletion(uint32 eventId) override;

    // Advanced event features
    struct EventCoordinationProfile
    {
        bool enableEventPlanning;
        bool enableEventParticipation;
        bool enableEventLeadership;
        std::vector<GuildEventType> preferredEventTypes;
        std::vector<GuildEventType> availableLeadershipTypes;
        float planningProactiveness; // How often to propose events
        float participationRate; // Likelihood to join events
        std::vector<std::pair<uint32, uint32>> availabilityWindows; // startTime, endTime pairs
        uint32 maxEventsPerWeek;
        bool autoAcceptInvitations;

        EventCoordinationProfile() : enableEventPlanning(true), enableEventParticipation(true)
            , enableEventLeadership(false), planningProactiveness(0.3f)
            , participationRate(0.8f), maxEventsPerWeek(7), autoAcceptInvitations(false) {}
    };

    void SetEventProfile(uint32 playerGuid, const EventCoordinationProfile& profile) override;
    EventCoordinationProfile GetEventProfile(uint32 playerGuid) override;

    // Event analytics and tracking
    struct EventParticipation
    {
        uint32 playerGuid;
        uint32 guildId;
        std::vector<uint32> organizedEvents;
        std::vector<uint32> participatedEvents;
        std::unordered_map<GuildEventType, uint32> eventTypePreferences;
        uint32 totalEventsCreated;
        uint32 totalEventsAttended;
        float organizationRating;
        float participationRating;
        uint32 lastEventActivity;

        EventParticipation(uint32 pGuid, uint32 gId) : playerGuid(pGuid), guildId(gId)
            , totalEventsCreated(0), totalEventsAttended(0), organizationRating(0.5f)
            , participationRating(0.7f), lastEventActivity(GameTime::GetGameTimeMS()) {}
    };

    EventParticipation GetEventParticipation(uint32 playerGuid) override;
    void UpdateEventParticipation(uint32 playerGuid, uint32 eventId, bool wasOrganizer);

    // Event type specific coordination
    void CoordinateRaidEvent(Player* leader, uint32 eventId);
    void CoordinatePvPEvent(Player* leader, uint32 eventId);
    void CoordinateSocialEvent(Player* leader, uint32 eventId);
    void CoordinateLevelingEvent(Player* leader, uint32 eventId);

    // Event optimization and intelligence
    void OptimizeEventScheduling(Player* player) override;
    void AnalyzeGuildEventPatterns(uint32 guildId) override;
    void SuggestOptimalEventTimes(uint32 guildId);
    std::vector<GuildEventType> RecommendEventTypes(Player* player);

    // Event communication and updates
    void BroadcastEventUpdates(uint32 eventId, const std::string& updateMessage) override;
    void SendEventReminders(uint32 eventId) override;
    void NotifyEventChanges(uint32 eventId);
    void UpdateEventStatus(uint32 eventId, EventStatus newStatus) override;

    // Group formation for events
    Group* FormEventGroup(uint32 eventId) override;
    void AssignGroupRoles(Group* group, uint32 eventId) override;
    void CoordinateGroupForEvent(Group* group, uint32 eventId);
    void HandleEventGroupChanges(Group* group, uint32 eventId);

    // Event conflict resolution
    void ResolveEventConflicts(Player* player);
    void HandleOverlappingEvents(uint32 eventId1, uint32 eventId2);
    void ManageEventPriorities(uint32 guildId);
    void RescheduleConflictingEvents(Player* player);

    // Performance monitoring
    struct EventMetrics
    {
        std::atomic<uint32> eventsCreated{0};
        std::atomic<uint32> eventsCompleted{0};
        std::atomic<uint32> eventsCancelled{0};
        std::atomic<uint32> totalParticipants{0};
        std::atomic<float> averageAttendance{0.75f};
        std::atomic<float> organizationEfficiency{0.8f};
        std::atomic<float> memberSatisfaction{0.85f};
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            eventsCreated = 0; eventsCompleted = 0; eventsCancelled = 0;
            totalParticipants = 0; averageAttendance = 0.75f;
            organizationEfficiency = 0.8f; memberSatisfaction = 0.85f;
            lastUpdate = std::chrono::steady_clock::now();
        }

        float GetCompletionRate() const {
            uint32 created = eventsCreated.load();
            uint32 completed = eventsCompleted.load();
            return created > 0 ? (float)completed / created : 0.0f;
        }
    };

    EventMetrics GetGuildEventMetrics(uint32 guildId) override;
    EventMetrics GetPlayerEventMetrics(uint32 playerGuid) override;

    // Event templates and presets
    void CreateEventTemplate(const std::string& templateName, const GuildEvent& templateData);
    GuildEvent LoadEventTemplate(const std::string& templateName);
    std::vector<std::string> GetAvailableTemplates();
    void CustomizeEventFromTemplate(GuildEvent& event, const std::string& templateName);

    // Seasonal and special events
    void PlanSeasonalEvents(uint32 guildId);
    void HandleHolidayEvents(uint32 guildId);
    void OrganizeSpecialCelebrations(uint32 guildId);
    void CoordinateGuildAnniversary(uint32 guildId);

    // Configuration and customization
    void SetEventCoordinationEnabled(uint32 guildId, bool enabled) override;
    void SetMaxConcurrentEvents(uint32 guildId, uint32 maxEvents);
    void ConfigureEventNotifications(uint32 playerGuid, bool enableReminders);
    void SetEventAutoSignup(uint32 playerGuid, GuildEventType eventType, bool autoSignup);

    // Error handling and recovery
    void HandleEventError(uint32 eventId, const std::string& error);
    void RecoverFromEventFailure(uint32 eventId);
    void HandleMissingOrganizer(uint32 eventId);
    void EmergencyEventCancellation(uint32 eventId);

    // Update and maintenance
    void Update(uint32 diff) override;
    void UpdateEventStates() override;
    void ProcessEventReminders();
    void CleanupExpiredEvents() override;

private:
    GuildEventCoordinator();
    ~GuildEventCoordinator() = default;

    // Core event data
    std::unordered_map<uint32, GuildEvent> _guildEvents; // eventId -> event
    std::unordered_map<uint32, EventCoordinationProfile> _playerProfiles; // playerGuid -> profile
    std::unordered_map<uint32, EventParticipation> _playerParticipation; // playerGuid -> participation
    std::atomic<uint32> _nextEventId{1};
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BEHAVIOR_MANAGER> _eventMutex;

    // Guild event tracking
    std::unordered_map<uint32, std::vector<uint32>> _guildActiveEvents; // guildId -> eventIds
    std::unordered_map<uint32, EventMetrics> _guildMetrics; // guildId -> metrics

    // Event templates
    std::unordered_map<std::string, GuildEvent> _eventTemplates; // templateName -> template
    std::unordered_map<GuildEventType, std::vector<std::string>> _typeTemplates; // eventType -> templateNames

    // Event scheduling intelligence
    struct GuildEventScheduler
    {
        uint32 guildId;
        std::vector<std::pair<uint32, uint32>> popularTimes; // startTime, endTime
        std::unordered_map<uint32, float> memberAvailability; // memberGuid -> availability score
        std::vector<uint32> conflictingEvents;
        uint32 optimalEventDuration;
        float averageAttendanceRate;

        GuildEventScheduler(uint32 gId) : guildId(gId), optimalEventDuration(7200000)
            , averageAttendanceRate(0.75f) {}
    };

    std::unordered_map<uint32, GuildEventScheduler> _guildSchedulers; // guildId -> scheduler

    // Performance tracking
    EventMetrics _globalMetrics;

    // Helper functions
    void InitializeEventTemplates();
    void LoadGuildEventData(uint32 guildId);
    bool ValidateEventData(const GuildEvent& event);
    void UpdateEventCalendar(uint32 eventId);

    // Event planning algorithms
    uint32 FindOptimalEventTime(uint32 guildId, GuildEventType eventType, uint32 duration);
    std::vector<uint32> SelectEventParticipants(uint32 eventId);
    void OptimizeEventComposition(uint32 eventId);
    bool IsEventViable(const GuildEvent& event);

    // Event coordination implementations
    void CoordinateEventPreparation(uint32 eventId);
    void ManageEventExecution(uint32 eventId);
    void HandleEventLogistics(uint32 eventId);
    void MonitorEventHealth(uint32 eventId);

    // Communication helpers
    void SendEventInvitation(uint32 playerGuid, uint32 eventId);
    void BroadcastEventAnnouncement(uint32 eventId);
    void UpdateEventParticipants(uint32 eventId, const std::string& message);
    void LogEventActivity(uint32 eventId, const std::string& activity);

    // Performance optimization
    void OptimizeEventScheduling(uint32 guildId);
    void CacheEventData(uint32 guildId);
    void PreloadEventInformation(uint32 eventId);
    void UpdateEventMetrics(uint32 eventId, bool wasSuccessful);

    // Constants
    static constexpr uint32 EVENT_UPDATE_INTERVAL = 60000; // 1 minute
    static constexpr uint32 REMINDER_CHECK_INTERVAL = 300000; // 5 minutes
    static constexpr uint32 PLANNING_ADVANCE_TIME = 604800000; // 1 week
    static constexpr uint32 MAX_EVENTS_PER_GUILD = 20;
    static constexpr uint32 MIN_EVENT_DURATION = 1800000; // 30 minutes
    static constexpr uint32 MAX_EVENT_DURATION = 14400000; // 4 hours
    static constexpr float MIN_ATTENDANCE_RATE = 0.4f; // 40% minimum for viability
    static constexpr uint32 EVENT_REMINDER_TIMES[3] = {86400000, 3600000, 1800000}; // 24h, 1h, 30m
    static constexpr uint32 MAX_RECURRING_EVENTS = 5;
    static constexpr uint32 EVENT_HISTORY_RETENTION = 2592000000; // 30 days
};

} // namespace Playerbot