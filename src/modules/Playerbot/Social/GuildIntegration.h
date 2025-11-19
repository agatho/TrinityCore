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
    explicit GuildIntegration(Player* bot);
    ~GuildIntegration();
    GuildIntegration(GuildIntegration const&) = delete;
    GuildIntegration& operator=(GuildIntegration const&) = delete;

    // Core guild functionality using TrinityCore's Guild system
    void ProcessGuildInteraction() override;
    void HandleGuildChat(const GuildChatMessage& message) override;
    void ParticipateInGuildActivities() override;
    void ManageGuildResponsibilities() override;

    // Guild chat automation
    void AutomateGuildChatParticipation() override;
    void RespondToGuildChat(const GuildChatMessage& message) override;
    void InitiateGuildConversation() override;
    void ShareGuildInformation(const std::string& topic) override;

    // Guild bank management
    void AutomateGuildBankInteractions() override;
    void DepositItemsToGuildBank() override;
    void WithdrawNeededItems() override;
    void OrganizeGuildBank() override;
    void ManageGuildBankPermissions() override;

    // Guild event coordination
    void CoordinateGuildEvents() override;
    void ScheduleGuildActivities() override;
    void ManageGuildCalendar() override;
    void OrganizeGuildRuns() override;

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

    void SetGuildProfile(const GuildProfile& profile) override;
    GuildProfile GetGuildProfile() override;

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

    GuildParticipation GetGuildParticipation() override;
    void UpdateGuildParticipation(GuildActivityType activityType) override;

    // Guild recruitment assistance
    void AssistWithRecruitment() override;
    void EvaluateRecruitmentCandidates() override;
    void WelcomeNewGuildMembers() override;
    void MentorJuniorMembers() override;

    // Guild leadership support
    void SupportGuildLeadership() override;
    void HandleOfficerDuties() override;
    void AssistWithGuildManagement() override;
    void ProvideMemberFeedback() override;

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

    std::string GenerateGuildChatResponse(const GuildChatMessage& message) override;
    std::string GenerateConversationStarter() override;
    bool ShouldRespondToMessage(const GuildChatMessage& message) override;
    void LearnFromGuildConversations() override;

    // Guild achievement coordination
    void ContributeToGuildAchievements() override;
    void CoordinateAchievementEfforts(Guild* guild) override;
    void TrackAchievementProgress() override;
    void CelebrateGuildAchievements() override;

    // Guild social features
    void OrganizeSocialEvents() override;
    void ParticipateInGuildTradition() override;
    void MaintainGuildFriendships() override;
    void HandleGuildConflicts() override;

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

    GuildMetrics GetPlayerGuildMetrics() override;
    GuildMetrics GetGuildBotMetrics(uint32 guildId) override;

    // Guild bank automation
    void OptimizeGuildBankUsage() override;
    void AutoDepositValuableItems() override;
    void AutoWithdrawNeededConsumables() override;
    void ManageGuildBankTabs() override;
    void TrackGuildBankActivity() override;

    // Guild event management
    void CreateGuildEvent(const std::string& eventType) override;
    void ManageGuildCalendarEvents() override;
    void CoordinateRaidScheduling() override;
    void OrganizePvPEvents() override;

    // Advanced guild AI
    void AnalyzeGuildDynamics(Guild* guild) override;
    void AdaptToGuildCulture() override;
    void DetectGuildMoodAndTone(Guild* guild) override;
    void AdjustBehaviorToGuildNorms() override;

    // Configuration and customization
    void SetGuildAutomationLevel(float level) override;
    void EnableGuildActivity(GuildActivityType activity, bool enable) override;
    void SetGuildChatFrequency(float frequency) override;
    void ConfigureGuildBankAccess(bool autoDeposit, bool autoWithdraw) override;

    // Error handling and recovery
    void HandleGuildInteractionError(const std::string& error) override;
    void RecoverFromGuildFailure() override;
    void HandleGuildLeaving() override;
    void HandleGuildInvitations(uint32 guildId) override;

    // Update and maintenance
    void Update(uint32 diff) override;
    void UpdateGuildParticipation() override;
    void ProcessGuildEvents() override;
    void CleanupGuildData() override;

private:
    Player* _bot;
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
    

    // Chat intelligence system
    std::unordered_map<uint32, ChatIntelligence> _chatIntelligence; // playerGuid -> intelligence
    std::unordered_map<std::string, std::vector<std::string>> _globalResponseTemplates;
    std::vector<std::string> _conversationTopics;
    

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
    bool IsAppropriateTimeToChat();
    void UpdateGuildSocialGraph(uint32 guildId);

    // Chat response generation
    void OfferGuildAssistance(const std::string& assistance);
    void SendGuildChatMessage(const std::string& message);
    std::string SelectResponseTemplate(const std::string& category);
    std::string PersonalizeResponse(const std::string& responseTemplate);
    float CalculateMessageRelevance(const GuildChatMessage& message);
    std::vector<std::string> ExtractKeywords(const std::string& message);

    // Guild bank logic
    bool ShouldDepositItem(uint32 itemId);
    bool ShouldWithdrawItem(uint32 itemId);
    void ProcessGuildBankTransaction(uint32 itemId, bool isDeposit);
    void OptimizeGuildBankLayout();

    // Event coordination
    void PlanGuildEvent(const std::string& eventType, uint32 proposedTime);
    void InviteMembersToEvent(const std::string& eventId);
    void ManageEventAttendance(const std::string& eventId);
    bool IsGoodTimeForEvent(uint32 proposedTime, const std::vector<uint32>& memberIds);

    // Social dynamics analysis
    void AnalyzeGuildPersonalities(uint32 guildId);
    void DetectGuildCliques(uint32 guildId);
    void IdentifyInfluentialMembers(uint32 guildId);
    void AssessGuildHealth(uint32 guildId);

    // Performance optimization
    void OptimizeGuildInteractions();
    void CacheGuildData(uint32 guildId);
    void PrioritizeGuildActivities();
    void UpdateGuildMetrics(GuildActivityType activity, bool wasSuccessful);

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