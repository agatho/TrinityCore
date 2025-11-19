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
    virtual void ProcessGuildInteraction(Player* player) = 0;
    virtual void HandleGuildChat(Player* player, const GuildChatMessage& message) = 0;
    virtual void ParticipateInGuildActivities(Player* player) = 0;
    virtual void ManageGuildResponsibilities(Player* player) = 0;

    // Guild chat automation
    virtual void AutomateGuildChatParticipation(Player* player) = 0;
    virtual void RespondToGuildChat(Player* player, const GuildChatMessage& message) = 0;
    virtual void InitiateGuildConversation(Player* player) = 0;
    virtual void ShareGuildInformation(Player* player, const std::string& topic) = 0;

    // Guild bank management
    virtual void AutomateGuildBankInteractions(Player* player) = 0;
    virtual void DepositItemsToGuildBank(Player* player) = 0;
    virtual void WithdrawNeededItems(Player* player) = 0;
    virtual void OrganizeGuildBank(Player* player) = 0;
    virtual void ManageGuildBankPermissions(Player* player) = 0;

    // Guild event coordination
    virtual void CoordinateGuildEvents(Player* player) = 0;
    virtual void ScheduleGuildActivities(Player* player) = 0;
    virtual void ManageGuildCalendar(Player* player) = 0;
    virtual void OrganizeGuildRuns(Player* player) = 0;

    // Guild profiles and participation
    virtual void SetGuildProfile(uint32 playerGuid, const GuildProfile& profile) = 0;
    virtual GuildProfile GetGuildProfile(uint32 playerGuid) = 0;
    virtual GuildParticipation GetGuildParticipation(uint32 playerGuid) = 0;
    virtual void UpdateGuildParticipation(uint32 playerGuid, GuildActivityType activityType) = 0;

    // Guild recruitment assistance
    virtual void AssistWithRecruitment(Player* player) = 0;
    virtual void EvaluateRecruitmentCandidates(Player* player) = 0;
    virtual void WelcomeNewGuildMembers(Player* player) = 0;
    virtual void MentorJuniorMembers(Player* player) = 0;

    // Guild leadership support
    virtual void SupportGuildLeadership(Player* player) = 0;
    virtual void HandleOfficerDuties(Player* player) = 0;
    virtual void AssistWithGuildManagement(Player* player) = 0;
    virtual void ProvideMemberFeedback(Player* player) = 0;

    // Chat intelligence and response generation
    virtual std::string GenerateGuildChatResponse(Player* player, const GuildChatMessage& message) = 0;
    virtual std::string GenerateConversationStarter(Player* player) = 0;
    virtual bool ShouldRespondToMessage(Player* player, const GuildChatMessage& message) = 0;
    virtual void LearnFromGuildConversations(Player* player) = 0;

    // Guild achievement coordination
    virtual void ContributeToGuildAchievements(Player* player) = 0;
    virtual void CoordinateAchievementEfforts(Guild* guild) = 0;
    virtual void TrackAchievementProgress(Player* player) = 0;
    virtual void CelebrateGuildAchievements(Player* player) = 0;

    // Guild social features
    virtual void OrganizeSocialEvents(Player* player) = 0;
    virtual void ParticipateInGuildTradition(Player* player) = 0;
    virtual void MaintainGuildFriendships(Player* player) = 0;
    virtual void HandleGuildConflicts(Player* player) = 0;

    // Performance monitoring
    virtual GuildMetrics GetPlayerGuildMetrics(uint32 playerGuid) = 0;
    virtual GuildMetrics GetGuildBotMetrics(uint32 guildId) = 0;

    // Guild bank automation
    virtual void OptimizeGuildBankUsage(Player* player) = 0;
    virtual void AutoDepositValuableItems(Player* player) = 0;
    virtual void AutoWithdrawNeededConsumables(Player* player) = 0;
    virtual void ManageGuildBankTabs(Player* player) = 0;
    virtual void TrackGuildBankActivity(Player* player) = 0;

    // Guild event management
    virtual void CreateGuildEvent(Player* player, const std::string& eventType) = 0;
    virtual void ManageGuildCalendarEvents(Player* player) = 0;
    virtual void CoordinateRaidScheduling(Player* player) = 0;
    virtual void OrganizePvPEvents(Player* player) = 0;

    // Advanced guild AI
    virtual void AnalyzeGuildDynamics(Guild* guild) = 0;
    virtual void AdaptToGuildCulture(Player* player) = 0;
    virtual void DetectGuildMoodAndTone(Guild* guild) = 0;
    virtual void AdjustBehaviorToGuildNorms(Player* player) = 0;

    // Configuration and customization
    virtual void SetGuildAutomationLevel(uint32 playerGuid, float level) = 0;
    virtual void EnableGuildActivity(uint32 playerGuid, GuildActivityType activity, bool enable) = 0;
    virtual void SetGuildChatFrequency(uint32 playerGuid, float frequency) = 0;
    virtual void ConfigureGuildBankAccess(uint32 playerGuid, bool autoDeposit, bool autoWithdraw) = 0;

    // Error handling and recovery
    virtual void HandleGuildInteractionError(uint32 playerGuid, const std::string& error) = 0;
    virtual void RecoverFromGuildFailure(uint32 playerGuid) = 0;
    virtual void HandleGuildLeaving(Player* player) = 0;
    virtual void HandleGuildInvitations(Player* player, uint32 guildId) = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void UpdateGuildParticipation() = 0;
    virtual void ProcessGuildEvents() = 0;
    virtual void CleanupGuildData() = 0;
};

} // namespace Playerbot
