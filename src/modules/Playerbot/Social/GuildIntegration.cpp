/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "GuildIntegration.h"
#include "Player.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Chat.h"
#include "Group.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Util.h"
#include "World.h"
#include "WorldSession.h"
#include "Bag.h"
#include "ObjectMgr.h"
#include <algorithm>
#include <random>
#include "GameTime.h"

namespace Playerbot
{

GuildIntegration* GuildIntegration::instance()
{
    static GuildIntegration instance;
    return &instance;
}

GuildIntegration::GuildIntegration()
{
    _globalMetrics.Reset();
    InitializeChatTemplates();
}

void GuildIntegration::ProcessGuildInteraction(Player* player)
{
    if (!player)
        return;

    Guild* guild = player->GetGuild();
    if (!guild)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    uint32 guildId = guild->GetId();

    // Initialize tracking data if needed
    if (_guildTracking.find(guildId) == _guildTracking.end())
    {
        _guildTracking[guildId] = GuildActivityTracker(guildId);
    }

    // Update player participation
    UpdateGuildParticipation(playerGuid, GuildActivityType::SOCIAL_INTERACTION);

    // Handle guild-specific interactions
    AutomateGuildChatParticipation(player);
    AutomateGuildBankInteractions(player);
    ParticipateInGuildActivities(player);
}

void GuildIntegration::HandleGuildChat(Player* player, const GuildChatMessage& message)
{
    if (!player || !player->GetGuild())
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    // Store message in chat history
    auto& tracker = _guildTracking[player->GetGuild()->GetId()];
    tracker.chatHistory.push(message);

    // Limit chat history size
    while (tracker.chatHistory.size() > MAX_CHAT_HISTORY)
        tracker.chatHistory.pop();

    // Decide whether to respond
    if (ShouldRespondToMessage(player, message))
    {
        RespondToGuildChat(player, message);
    }

    // Update participation metrics
    UpdateGuildParticipation(playerGuid, GuildActivityType::CHAT_PARTICIPATION);
}

void GuildIntegration::ParticipateInGuildActivities(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    GuildProfile profile = GetGuildProfile(playerGuid);
    // Participate in various guild activities based on profile
    for (GuildActivityType activity : profile.activeActivities)
    {
        switch (activity)
        {
            case GuildActivityType::GUILD_BANK_INTERACTION:
                if (rand() % 100 < 10) // 10% chance per update
                    AutomateGuildBankInteractions(player);
                break;

            case GuildActivityType::GUILD_EVENT_ATTENDANCE:
                // Check for scheduled events
                break;

            case GuildActivityType::OFFICER_DUTIES:
                if (profile.preferredRole == GuildRole::OFFICER || profile.preferredRole == GuildRole::LEADER)
                    SupportGuildLeadership(player);
                break;

            case GuildActivityType::RECRUITMENT_ASSISTANCE:
                if (profile.preferredRole == GuildRole::RECRUITER || rand() % 100 < 5)
                    AssistWithRecruitment(player);
                break;

            case GuildActivityType::ACHIEVEMENT_CONTRIBUTION:
                ContributeToGuildAchievements(player);
                break;

            default:
                break;
        }
    }
}

void GuildIntegration::ManageGuildResponsibilities(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    GuildProfile profile = GetGuildProfile(player->GetGUID().GetCounter());
    // Handle role-specific responsibilities
    switch (profile.preferredRole)
    {
        case GuildRole::OFFICER:
        case GuildRole::LEADER:
            HandleOfficerDuties(player);
            break;

        case GuildRole::BANKER:
            OrganizeGuildBank(player);
            break;

        case GuildRole::RECRUITER:
            EvaluateRecruitmentCandidates(player);
            break;

        case GuildRole::EVENT_ORGANIZER:
            CoordinateGuildEvents(player);
            break;

        default:
            // Regular member activities
            break;
    }
}

void GuildIntegration::AutomateGuildChatParticipation(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    GuildProfile profile = GetGuildProfile(playerGuid);

    if (!IsAppropriateTimeToChat(player))
        return;

    // Check if we should initiate conversation
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    float chatChance = DEFAULT_CHAT_FREQUENCY * profile.participationLevel;

    if (dis(gen) < chatChance)
    {
        // Decide what type of interaction to make
        std::uniform_int_distribution<int> actionDis(1, 100);
        int action = actionDis(gen);

        if (action <= 30)
        {
            InitiateGuildConversation(player);
        }
        else if (action <= 60)
        {
            // Share information about something relevant
            ShareGuildInformation(player, "general");
        }
        else if (action <= 80 && profile.helpfulnessLevel > 0.7f)
        {
            // Offer help or assistance
            OfferGuildAssistance(player, "");
        }
    }
}

void GuildIntegration::RespondToGuildChat(Player* player, const GuildChatMessage& message)
{
    if (!player)
        return;

    std::string response = GenerateGuildChatResponse(player, message);
    if (!response.empty())
    {
        SendGuildChatMessage(player, response);
        UpdateGuildParticipation(player->GetGUID().GetCounter(), GuildActivityType::CHAT_PARTICIPATION);
    }
}

void GuildIntegration::InitiateGuildConversation(Player* player)
{
    if (!player)
        return;

    std::string message = GenerateConversationStarter(player);
    if (!message.empty())
    {
        SendGuildChatMessage(player, message);
    }
}

void GuildIntegration::ShareGuildInformation(Player* player, const std::string& topic)
{
    if (!player)
        return;

    // Generate informative messages based on topic
    std::vector<std::string> infoMessages;

    if (topic == "general" || topic == "events")
    {
        infoMessages = {
            "Don't forget about our guild event this weekend!",
            "The guild bank has some useful consumables available.",
            "Anyone need help with dungeons or quests?",
            "Remember to contribute to guild achievements when you can."
        };
    }
    else if (topic == "tips")
    {
        infoMessages = {
            "Pro tip: Check the guild bank for consumables before raiding.",
            "Don't forget to repair before dungeon runs!",
            "Guild tabard gives a nice reputation bonus.",
            "The guild calendar has upcoming events marked."
        };
    }
    if (!infoMessages.empty())
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, infoMessages.size() - 1);

        SendGuildChatMessage(player, infoMessages[dis(gen)]);
    }
}

void GuildIntegration::AutomateGuildBankInteractions(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    uint32 currentTime = GameTime::GetGameTimeMS();
    auto& state = _playerStates[playerGuid];

    // Check cooldown
    if (currentTime - state.lastGuildBankInteraction < GUILD_BANK_CHECK_INTERVAL)
        return;

    // Check if player is near a guild bank
    // This would require checking proximity to guild bank NPCs

    // Decide what to do with guild bank
    GuildProfile profile = GetGuildProfile(playerGuid);
    if (rand() % 100 < 30) // 30% chance to deposit
    {
        DepositItemsToGuildBank(player);
    }
    else if (rand() % 100 < 20) // 20% chance to withdraw
    {
        WithdrawNeededItems(player);
    }

    state.lastGuildBankInteraction = currentTime;
    UpdateGuildParticipation(playerGuid, GuildActivityType::GUILD_BANK_INTERACTION);
}

void GuildIntegration::DepositItemsToGuildBank(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    // Find items suitable for guild bank deposit
    std::vector<Item*> itemsToDeposit;

    // Check player's inventory for valuable items that could benefit the guild
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        if (Bag* pBag = player->GetBagByPos(bag))
        {
            for (uint32 slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                if (Item* item = pBag->GetItemByPos(slot))
                {
                    if (ShouldDepositItem(player, item->GetEntry()))
                    {
                        itemsToDeposit.push_back(item);
                    }
                }
            }
        }
    }

    // Execute deposits (simplified - would need actual guild bank interaction)
    for (Item* item : itemsToDeposit)
    {
        // In a real implementation, this would interact with the guild bank system
        TC_LOG_DEBUG("playerbot.guild", "Player {} depositing item {} to guild bank",
                    player->GetName(), item->GetEntry());
        if (itemsToDeposit.size() >= 3) // Limit deposits per session
            break;
    }
}

void GuildIntegration::WithdrawNeededItems(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    // Identify items needed by the player
    std::vector<uint32> neededItems;

    // Check for consumables, reagents, etc.
    // This would analyze player's current needs and available guild bank items

    // Execute withdrawals (simplified)
    for (uint32 itemId : neededItems)
    {
        if (ShouldWithdrawItem(player, itemId))
        {
            // In a real implementation, this would interact with the guild bank system
            TC_LOG_DEBUG("playerbot.guild", "Player {} withdrawing item {} from guild bank",
                        player->GetName(), itemId);
        }
    }
}

void GuildIntegration::OrganizeGuildBank(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    // Banker role: organize and manage guild bank efficiently
    // Sort items by category
    // Stack similar items
    // Move items to appropriate tabs
    // Clean up expired items
}

void GuildIntegration::ManageGuildBankPermissions(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    // Officer/Leader role: manage bank access permissions
    // Review member access levels
    // Adjust permissions based on member activity
    // Set up proper tab restrictions
}

void GuildIntegration::CoordinateGuildEvents(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    // Event organizer role: plan and coordinate guild activities
    // Check guild calendar
    // Plan upcoming events
    // Send reminders
    // Coordinate with other officers
}

void GuildIntegration::ScheduleGuildActivities(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    // Schedule various guild activities
    // Raid nights
    // Social events
    // Achievement runs
    // Guild meetings
}

void GuildIntegration::ManageGuildCalendar(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    // Manage guild calendar events
    // Create new events
    // Update existing events
    // Send invitations
    // Track attendance
}

void GuildIntegration::OrganizeGuildRuns(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    // Organize guild dungeon/raid runs
    // Form groups from guild members
    // Plan difficulty appropriate content
    // Coordinate schedules
}

void GuildIntegration::SetGuildProfile(uint32 playerGuid, const GuildProfile& profile)
{
    std::lock_guard lock(_guildMutex);
    _playerProfiles[playerGuid] = profile;
}

GuildIntegration::GuildProfile GuildIntegration::GetGuildProfile(uint32 playerGuid)
{
    std::lock_guard lock(_guildMutex);
    auto it = _playerProfiles.find(playerGuid);
    if (it != _playerProfiles.end())
        return it->second;

    return GuildProfile(); // Return default profile
}

GuildIntegration::GuildParticipation GuildIntegration::GetGuildParticipation(uint32 playerGuid)
{
    std::lock_guard lock(_guildMutex);
    auto it = _playerParticipation.find(playerGuid);
    if (it != _playerParticipation.end())
        return it->second;

    return GuildParticipation(playerGuid, 0); // Return default participation
}

void GuildIntegration::UpdateGuildParticipation(uint32 playerGuid, GuildActivityType activityType)
{
    std::lock_guard lock(_guildMutex);

    auto& participation = _playerParticipation[playerGuid];
    participation.activityCounts[activityType]++;
    participation.lastActivity = GameTime::GetGameTimeMS();

    // Update specific metrics based on activity type
    switch (activityType)
    {
        case GuildActivityType::CHAT_PARTICIPATION:
            participation.totalChatMessages++;
            break;
        case GuildActivityType::GUILD_EVENT_ATTENDANCE:
            participation.eventsAttended++;
            break;
        case GuildActivityType::ACHIEVEMENT_CONTRIBUTION:
            participation.contributionScore += 0.1f;
            break;
        default:
            break;
    }

    // Update social score based on activity
    participation.socialScore = std::min(1.0f, participation.socialScore + 0.01f);
}

void GuildIntegration::AssistWithRecruitment(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    // Help with guild recruitment
    // Look for potential recruits
    // Evaluate player suitability
    // Send recruitment messages
    // Welcome new members
}

void GuildIntegration::EvaluateRecruitmentCandidates(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    // Evaluate potential guild recruits
    // Check player level and activity
    // Assess compatibility with guild culture
    // Make recruitment recommendations
}

void GuildIntegration::WelcomeNewGuildMembers(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    // Welcome new guild members
    // Send welcome messages
    // Explain guild rules and culture
    // Offer assistance and guidance
    // Introduce to other members
}

void GuildIntegration::MentorJuniorMembers(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    // Mentor newer or lower-level guild members
    // Offer advice and guidance
    // Help with quests and dungeons
    // Share game knowledge
    // Provide encouragement
}

void GuildIntegration::SupportGuildLeadership(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    // Support guild leadership activities
    // Assist with decision making
    // Help enforce guild rules
    // Coordinate with other officers
    // Handle administrative tasks
}

void GuildIntegration::HandleOfficerDuties(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    // Handle officer-specific duties
    // Manage guild bank
    // Handle member issues
    // Coordinate events
    // Make guild decisions
}

void GuildIntegration::AssistWithGuildManagement(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    // Assist with general guild management
    // Monitor guild health
    // Handle conflicts
    // Maintain guild standards
    // Support guild goals
}

void GuildIntegration::ProvideMemberFeedback(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    // Provide feedback to guild members
    // Recognize achievements
    // Offer constructive criticism
    // Suggest improvements
    // Celebrate successes
}

std::string GuildIntegration::GenerateGuildChatResponse(Player* player, const GuildChatMessage& message)
{
    if (!player)
        return "";

    // Analyze message content and generate appropriate response
    float relevance = CalculateMessageRelevance(player, message);
    if (relevance < 0.3f) // Not relevant enough to respond
        return "";

    // Extract keywords from message
    std::vector<std::string> keywords = ExtractKeywords(message.content);

    // Generate response based on keywords and context
    std::string response = SelectResponseTemplate("general");

    // Personalize the response
    response = PersonalizeResponse(player, response);

    return response;
}

std::string GuildIntegration::GenerateConversationStarter(Player* player)
{
    if (!player)
        return "";

    GuildProfile profile = GetGuildProfile(player->GetGUID().GetCounter());
    // Select conversation starter based on profile and current context
    std::vector<std::string> starters;

    switch (profile.chatStyle)
    {
        case GuildChatStyle::HELPFUL:
            starters = {
                "Anyone need help with anything?",
                "How's everyone doing today?",
                "Any interesting quests or adventures happening?",
                "Don't forget to check the guild bank for useful items!"
            };
            break;

        case GuildChatStyle::SOCIAL:
            starters = {
                "Good morning, guild!",
                "Hope everyone's having a great day!",
                "What's everyone up to?",
                "Anyone have any fun stories to share?"
            };
            break;

        case GuildChatStyle::PROFESSIONAL:
            starters = {
                "Guild meeting reminder: check the calendar for upcoming events.",
                "Don't forget about our scheduled raid this week.",
                "Please review the guild bank organization guidelines.",
                "Achievement progress update: we're close to completing [achievement]."
            };
            break;

        default:
            starters = {
                "Hello everyone!",
                "How's the adventuring going?",
                "Any news from the field?",
                "Safe travels, everyone!"
            };
            break;
    }

    if (!starters.empty())
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, starters.size() - 1);
        return starters[dis(gen)];
    }

    return "";
}

bool GuildIntegration::ShouldRespondToMessage(Player* player, const GuildChatMessage& message)
{
    if (!player)
        return false;

    // Don't respond to own messages
    if (message.senderId == player->GetGUID().GetCounter())
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();
    GuildProfile profile = GetGuildProfile(playerGuid);

    // Check if message requires response
    if (message.requiresResponse)
        return true;

    // Calculate relevance and decide based on chat style
    float relevance = CalculateMessageRelevance(player, message);
    float threshold = 0.7f;

    switch (profile.chatStyle)
    {
        case GuildChatStyle::MINIMAL:
            threshold = 0.9f;
            break;
        case GuildChatStyle::MODERATE:
            threshold = 0.7f;
            break;
        case GuildChatStyle::ACTIVE:
        case GuildChatStyle::SOCIAL:
            threshold = 0.5f;
            break;
        case GuildChatStyle::HELPFUL:
            threshold = 0.4f;
            break;
        case GuildChatStyle::PROFESSIONAL:
            threshold = 0.6f;
            break;
    }

    return relevance > threshold;
}

void GuildIntegration::LearnFromGuildConversations(Player* player)
{
    if (!player)
        return;

    // Learn from guild chat patterns
    // Identify popular topics
    // Adapt response styles
    // Improve conversation quality
}

void GuildIntegration::ContributeToGuildAchievements(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    // Actively work towards guild achievements
    // Identify incomplete guild achievements
    // Prioritize achievements player can contribute to
    // Execute achievement-oriented activities
}

void GuildIntegration::CoordinateAchievementEfforts(Guild* guild)
{
    if (!guild)
        return;

    // Coordinate guild-wide achievement efforts
    // Analyze achievement progress
    // Assign tasks to members
    // Track completion status
}

void GuildIntegration::TrackAchievementProgress(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    // Track progress towards guild achievements
    // Monitor contribution levels
    // Identify bottlenecks
    // Suggest focus areas
}

void GuildIntegration::CelebrateGuildAchievements(Player* player)
{
    if (!player || !player->GetGuild())
        return;

    // Celebrate completed guild achievements
    // Send congratulatory messages
    // Acknowledge contributors
    // Boost guild morale
}

GuildIntegration::GuildMetrics GuildIntegration::GetPlayerGuildMetrics(uint32 playerGuid)
{
    std::lock_guard lock(_guildMutex);
    auto it = _playerMetrics.find(playerGuid);
    if (it != _playerMetrics.end())
        return it->second;

    GuildMetrics metrics;
    metrics.Reset();
    return metrics;
}

GuildIntegration::GuildMetrics GuildIntegration::GetGuildBotMetrics(uint32 guildId)
{
    GuildMetrics combinedMetrics;
    combinedMetrics.Reset();

    // Aggregate metrics from all bots in the guild
    std::lock_guard lock(_guildMutex);

    for (const auto& metricsPair : _playerMetrics)
    {
        // In a real implementation, we'd check if the player belongs to the specified guild
        const GuildMetrics& playerMetrics = metricsPair.second;

        combinedMetrics.guildInteractions += playerMetrics.guildInteractions.load();
        combinedMetrics.chatMessages += playerMetrics.chatMessages.load();
        combinedMetrics.bankTransactions += playerMetrics.bankTransactions.load();
        combinedMetrics.eventsParticipated += playerMetrics.eventsParticipated.load();
        combinedMetrics.helpfulActions += playerMetrics.helpfulActions.load();
    }

    return combinedMetrics;
}

void GuildIntegration::InitializeChatTemplates()
{
    // Initialize chat response templates
    _globalResponseTemplates["greeting"] = {
        "Hello there!",
        "Good to see you!",
        "Hey, how's it going?",
        "Greetings, friend!"
    };

    _globalResponseTemplates["help"] = {
        "I'd be happy to help!",
        "What do you need assistance with?",
        "Count me in if you need help!",
        "Let me know if I can assist!"
    };

    _globalResponseTemplates["thanks"] = {
        "You're welcome!",
        "Happy to help!",
        "No problem at all!",
        "Anytime!"
    };

    _globalResponseTemplates["farewell"] = {
        "Safe travels!",
        "See you later!",
        "Take care!",
        "Until next time!"
    };

    // Initialize conversation topics
    _conversationTopics = {
        "guild events",
        "achievements",
        "dungeon runs",
        "guild bank",
        "member assistance",
        "upcoming raids",
        "guild news",
        "general chat"
    };
}

void GuildIntegration::LoadGuildSpecificData(uint32 guildId)
{
    // Load guild-specific configuration and data
    // Custom chat templates
    // Guild-specific rules and preferences
    // Member interaction history
}

bool GuildIntegration::IsAppropriateTimeToChat(Player* player)
{
    if (!player)
        return false;

    // Check if it's an appropriate time to participate in guild chat
    // Consider time of day, recent activity, etc.

    uint32 currentTime = GameTime::GetGameTimeMS();
    auto& chatIntel = _chatIntelligence[player->GetGUID().GetCounter()];
    // Don't chat too frequently
    if (currentTime - chatIntel.lastResponseTime < 30000) // 30 seconds cooldown
        return false;

    // Check if guild is active
    Guild* guild = player->GetGuild();
    if (!guild)
        return false;

    // More sophisticated timing logic could be added here
    return true;
}

void GuildIntegration::UpdateGuildSocialGraph(uint32 guildId)
{
    // Update social interaction graph for the guild
    // Track member interactions
    // Identify social clusters
    // Monitor guild dynamics
}

std::string GuildIntegration::SelectResponseTemplate(const std::string& category)
{
    auto it = _globalResponseTemplates.find(category);
    if (it == _globalResponseTemplates.end() || it->second.empty())
        return "";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, it->second.size() - 1);

    return it->second[dis(gen)];
}

std::string GuildIntegration::PersonalizeResponse(Player* player, const std::string& templateStr)
{
    if (!player)
        return templateStr;

    // Personalize the response based on player and context
    std::string response = templateStr;

    // Add player-specific touches based on guild profile
    GuildProfile profile = GetGuildProfile(player->GetGUID().GetCounter());
    // Modify tone based on chat style
    switch (profile.chatStyle)
    {
        case GuildChatStyle::PROFESSIONAL:
            // Keep formal tone
            break;
        case GuildChatStyle::SOCIAL:
            // Add friendly emotes or expressions
            response += " :)";
            break;
        default:
            break;
    }

    return response;
}

float GuildIntegration::CalculateMessageRelevance(Player* player, const GuildChatMessage& message)
{
    if (!player)
        return 0.0f;

    float relevance = 0.0f;

    // Check for keywords that indicate relevance
    std::vector<std::string> keywords = ExtractKeywords(message.content);

    GuildProfile profile = GetGuildProfile(player->GetGUID().GetCounter());
    for (const std::string& keyword : keywords)
    {
        // Check against player's interests and expertise
        for (const std::string& interest : profile.interests)
        {
            if (keyword.find(interest) != std::string::npos)
            {
                relevance += 0.3f;
            }
        }

        for (const std::string& expertise : profile.expertise)
        {
            if (keyword.find(expertise) != std::string::npos)
            {
                relevance += 0.4f;
            }
        }

        // Check against common guild-related keywords
        std::vector<std::string> guildKeywords = {
            "help", "assist", "raid", "dungeon", "event", "bank", "achievement"
        };

        for (const std::string& guildKeyword : guildKeywords)
        {
            if (keyword.find(guildKeyword) != std::string::npos)
            {
                relevance += 0.2f;
            }
        }
    }

    // Check if message is directed at the bot
    if (message.content.find(player->GetName()) != std::string::npos)
    {
        relevance += 0.5f;
    }

    return std::min(1.0f, relevance);
}

std::vector<std::string> GuildIntegration::ExtractKeywords(const std::string& message)
{
    std::vector<std::string> keywords;

    // Simple keyword extraction (in a real implementation, this would be more sophisticated)
    std::istringstream iss(message);
    std::string word;

    while (iss >> word)
    {
        // Convert to lowercase and remove punctuation
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        word.erase(std::remove_if(word.begin(), word.end(), ::ispunct), word.end());

        if (word.length() > 2) // Ignore very short words
        {
            keywords.push_back(word);
        }
    }

    return keywords;
}

bool GuildIntegration::ShouldDepositItem(Player* player, uint32 itemId)
{
    if (!player)
        return false;

    // Determine if item should be deposited to guild bank
    const ItemTemplate* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
    if (!itemTemplate)
        return false;

    // Deposit consumables, crafting materials, and other useful items
    switch (itemTemplate->GetClass())
    {
        case ITEM_CLASS_CONSUMABLE:
        case ITEM_CLASS_TRADE_GOODS:
        case ITEM_CLASS_MISCELLANEOUS:
            return itemTemplate->GetQuality() >= ITEM_QUALITY_UNCOMMON;
        default:
            return false;
    }
}

bool GuildIntegration::ShouldWithdrawItem(Player* player, uint32 itemId)
{
    if (!player)
        return false;

    // Determine if player should withdraw item from guild bank
    // Check if player needs the item
    // Consider player's class, level, and current equipment

    return false; // Placeholder
}

void GuildIntegration::SendGuildChatMessage(Player* player, const std::string& message)
{
    if (!player || !player->GetGuild() || message.empty())
        return;

    // Send message to guild chat using proper TrinityCore API
    player->GetGuild()->BroadcastToGuild(player->GetSession(), false, message, LANG_UNIVERSAL);

    // Update metrics
    uint32 playerGuid = player->GetGUID().GetCounter();
    UpdateGuildMetrics(playerGuid, GuildActivityType::CHAT_PARTICIPATION, true);
}

void GuildIntegration::OfferGuildAssistance(Player* player, const std::string& assistance)
{
    if (!player)
        return;

    // If specific assistance is provided, use it; otherwise use default messages
    if (!assistance.empty())
    {
        SendGuildChatMessage(player, assistance);
        return;
    }

    std::vector<std::string> helpMessages = {
        "Anyone need help with quests or dungeons?",
        "I'm available to assist with any guild activities!",
        "Let me know if you need help with anything!",
        "Happy to lend a hand wherever needed!"
    };

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, helpMessages.size() - 1);

    SendGuildChatMessage(player, helpMessages[dis(gen)]);
}

void GuildIntegration::UpdateGuildMetrics(uint32 playerGuid, GuildActivityType activity, bool wasSuccessful)
{
    std::lock_guard lock(_guildMutex);

    auto& metrics = _playerMetrics[playerGuid];
    metrics.guildInteractions++;

    if (wasSuccessful)
    {
        switch (activity)
        {
            case GuildActivityType::CHAT_PARTICIPATION:
                metrics.chatMessages++;
                break;
            case GuildActivityType::GUILD_BANK_INTERACTION:
                metrics.bankTransactions++;
                break;
            case GuildActivityType::GUILD_EVENT_ATTENDANCE:
                metrics.eventsParticipated++;
                break;
            case GuildActivityType::RECRUITMENT_ASSISTANCE:
            case GuildActivityType::ACHIEVEMENT_CONTRIBUTION:
                metrics.helpfulActions++;
                break;
            default:
                break;
        }
    }

    // Update global metrics
    _globalMetrics.guildInteractions++;
    if (wasSuccessful)
    {
        switch (activity)
        {
            case GuildActivityType::CHAT_PARTICIPATION:
                _globalMetrics.chatMessages++;
                break;
            case GuildActivityType::GUILD_BANK_INTERACTION:
                _globalMetrics.bankTransactions++;
                break;
            case GuildActivityType::GUILD_EVENT_ATTENDANCE:
                _globalMetrics.eventsParticipated++;
                break;
            case GuildActivityType::RECRUITMENT_ASSISTANCE:
            case GuildActivityType::ACHIEVEMENT_CONTRIBUTION:
                _globalMetrics.helpfulActions++;
                break;
            default:
                break;
        }
    }

    metrics.lastUpdate = std::chrono::steady_clock::now();
    _globalMetrics.lastUpdate = std::chrono::steady_clock::now();
}

void GuildIntegration::Update(uint32 diff)
{
    static uint32 lastUpdate = 0;
    uint32 currentTime = GameTime::GetGameTimeMS();

    if (currentTime - lastUpdate < GUILD_UPDATE_INTERVAL)
        return;

    lastUpdate = currentTime;

    // Update guild participation
    UpdateGuildParticipation();

    // Process guild events
    ProcessGuildEvents();

    // Clean up old data
    CleanupGuildData();
}

void GuildIntegration::UpdateGuildParticipation()
{
    std::lock_guard lock(_guildMutex);

    uint32 currentTime = GameTime::GetGameTimeMS();

    // Apply social score decay for inactive players
    for (auto& participationPair : _playerParticipation)
    {
        GuildParticipation& participation = participationPair.second;

        if (currentTime - participation.lastActivity > 86400000) // 24 hours
        {
            participation.socialScore = std::max(0.0f, participation.socialScore - SOCIAL_SCORE_DECAY);
        }
    }
}

void GuildIntegration::ProcessGuildEvents()
{
    // Process scheduled guild events
    // Send reminders
    // Track attendance
    // Coordinate activities
}

void GuildIntegration::CleanupGuildData()
{
    std::lock_guard lock(_guildMutex);

    uint32 currentTime = GameTime::GetGameTimeMS();

    // Clean up old participation data for inactive players
    for (auto it = _playerParticipation.begin(); it != _playerParticipation.end();)
    {
        if (currentTime - it->second.lastActivity > 30 * 86400000) // 30 days
        {
            it = _playerParticipation.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

} // namespace Playerbot