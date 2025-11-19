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
    virtual void ProcessTradeRequest(uint32 sessionId, TradeDecision decision) = 0;
    virtual void UpdateTradeSession(uint32 sessionId) = 0;
    virtual void CompleteTradeSession(uint32 sessionId) = 0;
    virtual void CancelTradeSession(uint32 sessionId) = 0;

    // Player-to-player trading
    virtual bool CanInitiateTrade(Player* initiator, Player* target) = 0;
    virtual TradeDecision EvaluateTradeRequest(uint32 sessionId) = 0;

    // Vendor interactions using TrinityCore data
    virtual void LoadVendorDatabase() = 0;
    virtual std::vector<VendorInfo> FindNearbyVendors(float radius = 100.0f) = 0;
    virtual bool InteractWithVendor(uint32 vendorGuid) = 0;

    // Vendor purchasing and selling
    virtual void ProcessVendorBuy(uint32 vendorGuid, uint32 itemId, uint32 count) = 0;
    virtual void ProcessVendorSell(uint32 vendorGuid, uint32 itemGuid, uint32 count) = 0;
    virtual bool CanBuyFromVendor(uint32 vendorGuid, uint32 itemId) = 0;

    // Equipment repair using TrinityCore repair vendors
    virtual void AutoRepairEquipment() = 0;
    virtual std::vector<uint32> FindRepairVendors(float radius = 200.0f) = 0;
    virtual void ProcessEquipmentRepair(uint32 vendorGuid) = 0;

    // Innkeeper services using TrinityCore innkeeper data
    virtual void InteractWithInnkeeper(uint32 innkeeperGuid) = 0;
    virtual std::vector<uint32> FindNearbyInnkeepers(float radius = 150.0f) = 0;

    // Intelligent trade decision making
    virtual float AnalyzeTradeValue(const TradeSession& session) = 0;
    virtual bool IsTradeWorthwhile(const TradeSession& session) = 0;

    // Trade safety and validation
    virtual bool ValidateTradeSession(const TradeSession& session) = 0;
    virtual bool DetectSuspiciousTradeActivity(const TradeSession& session) = 0;

    // Performance monitoring
    virtual TradeMetrics GetPlayerTradeMetrics() = 0;
    virtual TradeMetrics GetGlobalTradeMetrics() = 0;

    // Automated vendor management
    virtual void AutoSellJunkItems() = 0;
    virtual void AutoBuyConsumables() = 0;

    // Configuration and settings
    virtual void SetTradeConfiguration(const TradeConfiguration& config) = 0;
    virtual TradeConfiguration GetTradeConfiguration() = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void ProcessActiveTrades() = 0;
    virtual void CleanupExpiredTradeSessions() = 0;
};

} // namespace Playerbot
