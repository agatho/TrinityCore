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
#include "Position.h"
#include <vector>

namespace Playerbot
{

// Forward declarations
enum class TradeDecision : uint8;
struct VendorInfo;
struct TradeSession;
struct TradeConfiguration;
struct TradeMetrics;

class TC_GAME_API ITradeSystem
{
public:
    virtual ~ITradeSystem() = default;

    // Core trade functionality
    virtual bool InitiateTrade(Player* initiator, Player* target) = 0;
    virtual void ProcessTradeRequest(Player* player, uint32 sessionId, TradeDecision decision) = 0;
    virtual void UpdateTradeSession(uint32 sessionId) = 0;
    virtual void CompleteTradeSession(uint32 sessionId) = 0;
    virtual void CancelTradeSession(uint32 sessionId) = 0;

    // Player-to-player trading
    virtual bool CanInitiateTrade(Player* initiator, Player* target) = 0;
    virtual TradeDecision EvaluateTradeRequest(Player* player, uint32 sessionId) = 0;

    // Vendor interactions using TrinityCore data
    virtual void LoadVendorDatabase() = 0;
    virtual std::vector<VendorInfo> FindNearbyVendors(Player* player, float radius = 100.0f) = 0;
    virtual bool InteractWithVendor(Player* player, uint32 vendorGuid) = 0;

    // Vendor purchasing and selling
    virtual void ProcessVendorBuy(Player* player, uint32 vendorGuid, uint32 itemId, uint32 count) = 0;
    virtual void ProcessVendorSell(Player* player, uint32 vendorGuid, uint32 itemGuid, uint32 count) = 0;
    virtual bool CanBuyFromVendor(Player* player, uint32 vendorGuid, uint32 itemId) = 0;

    // Equipment repair using TrinityCore repair vendors
    virtual void AutoRepairEquipment(Player* player) = 0;
    virtual std::vector<uint32> FindRepairVendors(Player* player, float radius = 200.0f) = 0;
    virtual void ProcessEquipmentRepair(Player* player, uint32 vendorGuid) = 0;

    // Innkeeper services using TrinityCore innkeeper data
    virtual void InteractWithInnkeeper(Player* player, uint32 innkeeperGuid) = 0;
    virtual std::vector<uint32> FindNearbyInnkeepers(Player* player, float radius = 150.0f) = 0;

    // Intelligent trade decision making
    virtual float AnalyzeTradeValue(Player* player, const TradeSession& session) = 0;
    virtual bool IsTradeWorthwhile(Player* player, const TradeSession& session) = 0;

    // Trade safety and validation
    virtual bool ValidateTradeSession(const TradeSession& session) = 0;
    virtual bool DetectSuspiciousTradeActivity(Player* player, const TradeSession& session) = 0;

    // Performance monitoring
    virtual TradeMetrics GetPlayerTradeMetrics(uint32 playerGuid) = 0;
    virtual TradeMetrics GetGlobalTradeMetrics() = 0;

    // Automated vendor management
    virtual void AutoSellJunkItems(Player* player) = 0;
    virtual void AutoBuyConsumables(Player* player) = 0;

    // Configuration and settings
    virtual void SetTradeConfiguration(uint32 playerGuid, const TradeConfiguration& config) = 0;
    virtual TradeConfiguration GetTradeConfiguration(uint32 playerGuid) = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void ProcessActiveTrades() = 0;
    virtual void CleanupExpiredTradeSessions() = 0;
};

} // namespace Playerbot
