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
#include "GuildMgr.h"
#include "Chat.h"
#include "../Core/DI/Interfaces/IGuildIntegration.h"
#include <unordered_map>
#include <vector>
#include <queue>
#include <atomic>
#include <mutex>
#include <chrono>
#include "GameTime.h"

namespace Playerbot
{

enum class GuildActivityType : uint8
{
    CHAT_PARTICIPATION      = 0,
    GUILD_BANK_INTERACTION  = 1,
    GUILD_EVENT_ATTENDANCE  = 2,
    OFFICER_DUTIES          = 3,
    RECRUITMENT_ASSISTANCE  = 4,
    GUILD_REPAIR_USAGE      = 5,
    ACHIEVEMENT_CONTRIBUTION= 6,
    SOCIAL_INTERACTION      = 7
};

enum class GuildChatStyle : uint8
{
    MINIMAL         = 0,  // Rare, essential responses only
    MODERATE        = 1,  // Regular participation
    ACTIVE          = 2,  // Frequent communication
    SOCIAL          = 3,  // Chatty and friendly
    PROFESSIONAL    = 4,  // Focused on guild business
    HELPFUL         = 5   // Offers assistance frequently
};

enum class GuildRole : uint8
{
    MEMBER          = 0,
    VETERAN         = 1,
    OFFICER         = 2,
    LEADER          = 3,
    BANKER          = 4,
    RECRUITER       = 5,
    EVENT_ORGANIZER = 6
};

struct GuildChatMessage
{
    uint32 senderId;
    std::string senderName;
    std::string content;
    ChatMsg chatType;
    uint32 timestamp;
    bool requiresResponse;
    std::vector<std::string> keywords;
    float relevanceScore;

    GuildChatMessage() : senderId(0), chatType(CHAT_MSG_GUILD), timestamp(GameTime::GetGameTimeMS())
        , requiresResponse(false), relevanceScore(0.0f) {}
};

/**
 * @brief Comprehensive guild integration system for automated guild participation
 *
 * This system provides intelligent guild interactions including chat participation,
 * guild bank management, event coordination, and social activities for playerbots.
 */
class TC_GAME_API GuildIntegration final : public IGuildIntegration
{
public:
    static GuildIntegration* instance();

    // Core guild functionality using TrinityCore's Guild system
    void ProcessGuildInteraction(Player* player) override;
    void HandleGuildChat(Player* player, const GuildChatMessage& message) override;
    void ParticipateInGuildActivities(Player* player) override;
    void ManageGuildResponsibilities(Player* player) override;

    // Guild chat automation
    void AutomateGuildChatParticipation(Player* player) override;
    void RespondToGuildChat(Player* player, const GuildChatMessage& message) override;
    void InitiateGuildConversation(Player* player) override;
    void ShareGuildInformation(Player* player, const std::string& topic) override;

    // Guild bank management
    void AutomateGuildBankInteractions(Player* player) override;
    void DepositItemsToGuildBank(Player* player) override;
    void WithdrawNeededItems(Player* player) override;
    void OrganizeGuildBank(Player* player) override;
    void ManageGuildBankPermissions(Player* player) override;

    // Guild event coordination
    void CoordinateGuildEvents(Player* player) override;
    void ScheduleGuildActivities(Player* player) override;
    void ManageGuildCalendar(Player* player) override;
    void OrganizeGuildRuns(Player* player) override;

    // Advanced guild features
    struct GuildProfile
    {
        GuildChatStyle chatStyle;
        GuildRole preferredRole;
        std::vector<GuildActivityType> activeActivities;
        float participationLevel; // 0.0 = minimal, 1.0 = maximum
        float helpfulnessLevel;
        float leadershipAmbition;
        std::vector<std::string> expertise; // Areas of knowledge
        std::vector<std::string> interests; // Topics of interest
        std::unordered_set<uint32> friendlyMembers;
        std::unordered_set<std::string> chatTriggers;
        uint32 dailyActivityQuota;
        bool autoAcceptGuildInvites;

        GuildProfile() : chatStyle(GuildChatStyle::MODERATE), preferredRole(GuildRole::MEMBER)
            , participationLevel(0.7f), helpfulnessLevel(0.8f), leadershipAmbition(0.3f)
            , dailyActivityQuota(10), autoAcceptGuildInvites(true) {}
    };

    void SetGuildProfile(uint32 playerGuid, const GuildProfile& profile) override;
    GuildProfile GetGuildProfile(uint32 playerGuid) override;

    // Guild participation tracking
    struct GuildParticipation
    {
        uint32 playerGuid;
        uint32 guildId;
        std::vector<GuildChatMessage> recentMessages;
        std::unordered_map<GuildActivityType, uint32> activityCounts;
        uint32 totalChatMessages;
        uint32 helpfulResponses;
        uint32 eventsAttended;
        float socialScore;
        float contributionScore;
        uint32 lastActivity;
        uint32 joinDate;

        // Default constructor for std::unordered_map compatibility
        GuildParticipation() : playerGuid(0), guildId(0)
            , totalChatMessages(0), helpfulResponses(0), eventsAttended(0)
            , socialScore(0.5f), contributionScore(0.5f), lastActivity(GameTime::GetGameTimeMS())
            , joinDate(GameTime::GetGameTimeMS()) {}

        GuildParticipation(uint32 pGuid, uint32 gId) : playerGuid(pGuid), guildId(gId)
            , totalChatMessages(0), helpfulResponses(0), eventsAttended(0)
            , socialScore(0.5f), contributionScore(0.5f), lastActivity(GameTime::GetGameTimeMS())
            , joinDate(GameTime::GetGameTimeMS()) {}
    };

    GuildParticipation GetGuildParticipation(uint32 playerGuid) override;
    void UpdateGuildParticipation(uint32 playerGuid, GuildActivityType activityType) override;

    // Guild recruitment assistance
    void AssistWithRecruitment(Player* player) override;
    void EvaluateRecruitmentCandidates(Player* player) override;
    void WelcomeNewGuildMembers(Player* player) override;
    void MentorJuniorMembers(Player* player) override;

    // Guild leadership support
    void SupportGuildLeadership(Player* player) override;
    void HandleOfficerDuties(Player* player) override;
    void AssistWithGuildManagement(Player* player) override;
    void ProvideMemberFeedback(Player* player) override;

    // Chat intelligence and response generation
    struct ChatIntelligence
    {
        std::unordered_map<std::string, std::vector<std::string>> responseTemplates;
        std::unordered_map<std::string, float> keywordWeights;
        std::vector<std::string> conversationStarters;
        std::vector<std::string> helpfulTips;
        std::unordered_map<std::string, std::string> topicResponses;
        uint32 lastResponseTime;
        float responseFrequency;

        ChatIntelligence() : lastResponseTime(0), responseFrequency(0.3f) {}
    };

    std::string GenerateGuildChatResponse(Player* player, const GuildChatMessage& message) override;
    std::string GenerateConversationStarter(Player* player) override;
    bool ShouldRespondToMessage(Player* player, const GuildChatMessage& message) override;
    void LearnFromGuildConversations(Player* player) override;

    // Guild achievement coordination
    void ContributeToGuildAchievements(Player* player) override;
    void CoordinateAchievementEfforts(Guild* guild) override;
    void TrackAchievementProgress(Player* player) override;
    void CelebrateGuildAchievements(Player* player) override;

    // Guild social features
    void OrganizeSocialEvents(Player* player) override;
    void ParticipateInGuildTradition(Player* player) override;
    void MaintainGuildFriendships(Player* player) override;
    void HandleGuildConflicts(Player* player) override;

    // Performance monitoring
    struct GuildMetrics
    {
        std::atomic<uint32> guildInteractions{0};
        std::atomic<uint32> chatMessages{0};
        std::atomic<uint32> bankTransactions{0};
        std::atomic<uint32> eventsParticipated{0};
        std::atomic<uint32> helpfulActions{0};
        std::atomic<float> averageParticipationScore{0.7f};
        std::atomic<float> socialIntegrationScore{0.8f};
        std::atomic<float> contributionRating{0.75f};
        std::chrono::steady_clock::time_point lastUpdate;

        // Default constructor
        GuildMetrics() = default;

        // Copy constructor for atomic members
        GuildMetrics(const GuildMetrics& other) :
            guildInteractions(other.guildInteractions.load()),
            chatMessages(other.chatMessages.load()),
            bankTransactions(other.bankTransactions.load()),
            eventsParticipated(other.eventsParticipated.load()),
            helpfulActions(other.helpfulActions.load()),
            averageParticipationScore(other.averageParticipationScore.load()),
            socialIntegrationScore(other.socialIntegrationScore.load()),
            contributionRating(other.contributionRating.load()),
            lastUpdate(other.lastUpdate) {}

        // Assignment operator for atomic members
        GuildMetrics& operator=(const GuildMetrics& other) {
            if (this != &other) {
                guildInteractions.store(other.guildInteractions.load());
                chatMessages.store(other.chatMessages.load());
                bankTransactions.store(other.bankTransactions.load());
                eventsParticipated.store(other.eventsParticipated.load());
                helpfulActions.store(other.helpfulActions.load());
                averageParticipationScore.store(other.averageParticipationScore.load());
                socialIntegrationScore.store(other.socialIntegrationScore.load());
                contributionRating.store(other.contributionRating.load());
                lastUpdate = other.lastUpdate;
            }
            return *this;
        }

        void Reset() {
            guildInteractions = 0; chatMessages = 0; bankTransactions = 0;
            eventsParticipated = 0; helpfulActions = 0; averageParticipationScore = 0.7f;
            socialIntegrationScore = 0.8f; contributionRating = 0.75f;
            lastUpdate = std::chrono::steady_clock::now();
        }
    };

    GuildMetrics GetPlayerGuildMetrics(uint32 playerGuid) override;
    GuildMetrics GetGuildBotMetrics(uint32 guildId) override;

    // Guild bank automation
    void OptimizeGuildBankUsage(Player* player) override;
    void AutoDepositValuableItems(Player* player) override;
    void AutoWithdrawNeededConsumables(Player* player) override;
    void ManageGuildBankTabs(Player* player) override;
    void TrackGuildBankActivity(Player* player) override;

    // Guild event management
    void CreateGuildEvent(Player* player, const std::string& eventType) override;
    void ManageGuildCalendarEvents(Player* player) override;
    void CoordinateRaidScheduling(Player* player) override;
    void OrganizePvPEvents(Player* player) override;

    // Advanced guild AI
    void AnalyzeGuildDynamics(Guild* guild) override;
    void AdaptToGuildCulture(Player* player) override;
    void DetectGuildMoodAndTone(Guild* guild) override;
    void AdjustBehaviorToGuildNorms(Player* player) override;

    // Configuration and customization
    void SetGuildAutomationLevel(uint32 playerGuid, float level) override;
    void EnableGuildActivity(uint32 playerGuid, GuildActivityType activity, bool enable) override;
    void SetGuildChatFrequency(uint32 playerGuid, float frequency) override;
    void ConfigureGuildBankAccess(uint32 playerGuid, bool autoDeposit, bool autoWithdraw) override;

    // Error handling and recovery
    void HandleGuildInteractionError(uint32 playerGuid, const std::string& error) override;
    void RecoverFromGuildFailure(uint32 playerGuid) override;
    void HandleGuildLeaving(Player* player) override;
    void HandleGuildInvitations(Player* player, uint32 guildId) override;

    // Update and maintenance
    void Update(uint32 diff) override;
    void UpdateGuildParticipation() override;
    void ProcessGuildEvents() override;
    void CleanupGuildData() override;

private:
    GuildIntegration();
    ~GuildIntegration() = default;

    // Core guild data
    struct PlayerState {
        uint32 lastGuildBankInteraction = 0;
        uint32 lastChatTime = 0;
        uint32 lastEventParticipation = 0;
    };

    std::unordered_map<uint32, GuildProfile> _playerProfiles; // playerGuid -> profile
    std::unordered_map<uint32, GuildParticipation> _playerParticipation; // playerGuid -> participation
    std::unordered_map<uint32, GuildMetrics> _playerMetrics; // playerGuid -> metrics
    std::unordered_map<uint32, PlayerState> _playerStates; // playerGuid -> state
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BEHAVIOR_MANAGER> _guildMutex;

    // Chat intelligence system
    std::unordered_map<uint32, ChatIntelligence> _chatIntelligence; // playerGuid -> intelligence
    std::unordered_map<std::string, std::vector<std::string>> _globalResponseTemplates;
    std::vector<std::string> _conversationTopics;
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BEHAVIOR_MANAGER> _chatMutex;

    // Guild activity tracking
    struct GuildActivityTracker
    {
        uint32 guildId;
        std::unordered_map<uint32, uint32> memberActivity; // playerGuid -> activity count
        std::vector<std::pair<std::string, uint32>> recentEvents; // event, timestamp
        std::queue<GuildChatMessage> chatHistory;
        float overallMorale;
        float activityLevel;
        uint32 lastAnalysisTime;

        // Default constructor for std::unordered_map compatibility
        GuildActivityTracker() : guildId(0), overallMorale(0.8f)
            , activityLevel(0.6f), lastAnalysisTime(GameTime::GetGameTimeMS()) {}

        GuildActivityTracker(uint32 gId) : guildId(gId), overallMorale(0.8f)
            , activityLevel(0.6f), lastAnalysisTime(GameTime::GetGameTimeMS()) {}
    };

    std::unordered_map<uint32, GuildActivityTracker> _guildTracking; // guildId -> tracker

    // Performance tracking
    GuildMetrics _globalMetrics;

    // Helper functions
    void InitializeChatTemplates();
    void LoadGuildSpecificData(uint32 guildId);
    bool IsAppropriateTimeToChat(Player* player);
    void UpdateGuildSocialGraph(uint32 guildId);

    // Chat response generation
    void OfferGuildAssistance(Player* player, const std::string& assistance);
    void SendGuildChatMessage(Player* player, const std::string& message);
    std::string SelectResponseTemplate(const std::string& category);
    std::string PersonalizeResponse(Player* player, const std::string& responseTemplate);
    float CalculateMessageRelevance(Player* player, const GuildChatMessage& message);
    std::vector<std::string> ExtractKeywords(const std::string& message);

    // Guild bank logic
    bool ShouldDepositItem(Player* player, uint32 itemId);
    bool ShouldWithdrawItem(Player* player, uint32 itemId);
    void ProcessGuildBankTransaction(Player* player, uint32 itemId, bool isDeposit);
    void OptimizeGuildBankLayout(Player* player);

    // Event coordination
    void PlanGuildEvent(Player* player, const std::string& eventType, uint32 proposedTime);
    void InviteMembersToEvent(Player* player, const std::string& eventId);
    void ManageEventAttendance(Player* player, const std::string& eventId);
    bool IsGoodTimeForEvent(uint32 proposedTime, const std::vector<uint32>& memberIds);

    // Social dynamics analysis
    void AnalyzeGuildPersonalities(uint32 guildId);
    void DetectGuildCliques(uint32 guildId);
    void IdentifyInfluentialMembers(uint32 guildId);
    void AssessGuildHealth(uint32 guildId);

    // Performance optimization
    void OptimizeGuildInteractions(Player* player);
    void CacheGuildData(uint32 guildId);
    void PrioritizeGuildActivities(Player* player);
    void UpdateGuildMetrics(uint32 playerGuid, GuildActivityType activity, bool wasSuccessful);

    // Constants
    static constexpr uint32 GUILD_UPDATE_INTERVAL = 60000; // 1 minute
    static constexpr uint32 CHAT_PROCESSING_INTERVAL = 5000; // 5 seconds
    static constexpr uint32 ACTIVITY_ANALYSIS_INTERVAL = 300000; // 5 minutes
    static constexpr uint32 MAX_CHAT_HISTORY = 100;
    static constexpr float MIN_PARTICIPATION_SCORE = 0.3f;
    static constexpr uint32 GUILD_BANK_CHECK_INTERVAL = 1800000; // 30 minutes
    static constexpr float DEFAULT_CHAT_FREQUENCY = 0.3f; // 30% chance to respond
    static constexpr uint32 EVENT_PLANNING_ADVANCE = 86400000; // 24 hours
    static constexpr uint32 MAX_GUILD_ACTIVITIES_PER_DAY = 15;
    static constexpr float SOCIAL_SCORE_DECAY = 0.01f; // Daily decay without activity
    static constexpr uint32 NEW_MEMBER_WELCOME_DELAY = 300000; // 5 minutes
};

} // namespace Playerbot