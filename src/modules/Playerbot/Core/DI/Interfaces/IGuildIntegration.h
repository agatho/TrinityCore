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
#include <string>

// Forward declarations
class Player;
class Guild;
struct GuildChatMessage;

namespace Playerbot
{

// Forward declarations for nested types
struct GuildProfile;
struct GuildParticipation;
struct GuildMetrics;
enum class GuildActivityType : uint8;

/**
 * @brief Interface for comprehensive guild integration and automation
 *
 * Defines the contract for automated guild participation including chat,
 * bank management, event coordination, and social activities.
 */
class TC_GAME_API IGuildIntegration
{
public:
    virtual ~IGuildIntegration() = default;

    // Core guild functionality
    virtual void ProcessGuildInteraction() = 0;
    virtual void HandleGuildChat(const GuildChatMessage& message) = 0;
    virtual void ParticipateInGuildActivities() = 0;
    virtual void ManageGuildResponsibilities() = 0;

    // Guild chat automation
    virtual void AutomateGuildChatParticipation() = 0;
    virtual void RespondToGuildChat(const GuildChatMessage& message) = 0;
    virtual void InitiateGuildConversation() = 0;
    virtual void ShareGuildInformation(const std::string& topic) = 0;

    // Guild bank management
    virtual void AutomateGuildBankInteractions() = 0;
    virtual void DepositItemsToGuildBank() = 0;
    virtual void WithdrawNeededItems() = 0;
    virtual void OrganizeGuildBank() = 0;
    virtual void ManageGuildBankPermissions() = 0;

    // Guild event coordination
    virtual void CoordinateGuildEvents() = 0;
    virtual void ScheduleGuildActivities() = 0;
    virtual void ManageGuildCalendar() = 0;
    virtual void OrganizeGuildRuns() = 0;

    // Guild profiles and participation
    virtual void SetGuildProfile(const GuildProfile& profile) = 0;
    virtual GuildProfile GetGuildProfile() = 0;
    virtual GuildParticipation GetGuildParticipation() = 0;
    virtual void UpdateGuildParticipation(GuildActivityType activityType) = 0;

    // Guild recruitment assistance
    virtual void AssistWithRecruitment() = 0;
    virtual void EvaluateRecruitmentCandidates() = 0;
    virtual void WelcomeNewGuildMembers() = 0;
    virtual void MentorJuniorMembers() = 0;

    // Guild leadership support
    virtual void SupportGuildLeadership() = 0;
    virtual void HandleOfficerDuties() = 0;
    virtual void AssistWithGuildManagement() = 0;
    virtual void ProvideMemberFeedback() = 0;

    // Chat intelligence and response generation
    virtual std::string GenerateGuildChatResponse(const GuildChatMessage& message) = 0;
    virtual std::string GenerateConversationStarter() = 0;
    virtual bool ShouldRespondToMessage(const GuildChatMessage& message) = 0;
    virtual void LearnFromGuildConversations() = 0;

    // Guild achievement coordination
    virtual void ContributeToGuildAchievements() = 0;
    virtual void CoordinateAchievementEfforts(Guild* guild) = 0;
    virtual void TrackAchievementProgress() = 0;
    virtual void CelebrateGuildAchievements() = 0;

    // Guild social features
    virtual void OrganizeSocialEvents() = 0;
    virtual void ParticipateInGuildTradition() = 0;
    virtual void MaintainGuildFriendships() = 0;
    virtual void HandleGuildConflicts() = 0;

    // Performance monitoring
    virtual GuildMetrics GetPlayerGuildMetrics() = 0;
    virtual GuildMetrics GetGuildBotMetrics(uint32 guildId) = 0;

    // Guild bank automation
    virtual void OptimizeGuildBankUsage() = 0;
    virtual void AutoDepositValuableItems() = 0;
    virtual void AutoWithdrawNeededConsumables() = 0;
    virtual void ManageGuildBankTabs() = 0;
    virtual void TrackGuildBankActivity() = 0;

    // Guild event management
    virtual void CreateGuildEvent(const std::string& eventType) = 0;
    virtual void ManageGuildCalendarEvents() = 0;
    virtual void CoordinateRaidScheduling() = 0;
    virtual void OrganizePvPEvents() = 0;

    // Advanced guild AI
    virtual void AnalyzeGuildDynamics(Guild* guild) = 0;
    virtual void AdaptToGuildCulture() = 0;
    virtual void DetectGuildMoodAndTone(Guild* guild) = 0;
    virtual void AdjustBehaviorToGuildNorms() = 0;

    // Configuration and customization
    virtual void SetGuildAutomationLevel(float level) = 0;
    virtual void EnableGuildActivity(GuildActivityType activity, bool enable) = 0;
    virtual void SetGuildChatFrequency(float frequency) = 0;
    virtual void ConfigureGuildBankAccess(bool autoDeposit, bool autoWithdraw) = 0;

    // Error handling and recovery
    virtual void HandleGuildInteractionError(const std::string& error) = 0;
    virtual void RecoverFromGuildFailure() = 0;
    virtual void HandleGuildLeaving() = 0;
    virtual void HandleGuildInvitations(uint32 guildId) = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void UpdateGuildParticipation() = 0;
    virtual void ProcessGuildEvents() = 0;
    virtual void CleanupGuildData() = 0;
};

} // namespace Playerbot
