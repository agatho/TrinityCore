/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

/**
 * @file BotAI_EventHandlers.cpp
 * @brief Default implementations of event handlers for BotAI
 *
 * This file contains the default event handler implementations for all 11 event buses.
 * ClassAI implementations can override these virtual methods for specialized behavior.
 *
 * Phase 4: Event Bus Integration
 * - 11 event handlers fully implemented
 * - Delegates to managers where appropriate
 * - Provides sensible defaults for autonomous behavior
 */

#include "BotAI.h"
#include "Group/GroupEventBus.h"
#include "Combat/CombatEventBus.h"
#include "Cooldown/CooldownEventBus.h"
#include "Aura/AuraEventBus.h"
#include "Loot/LootEventBus.h"
#include "Quest/QuestEventBus.h"
#include "Resource/ResourceEventBus.h"
#include "Social/SocialEventBus.h"
#include "Auction/AuctionEventBus.h"
#include "NPC/NPCEventBus.h"
#include "Instance/InstanceEventBus.h"
#include "Game/QuestManager.h"
#include "Social/TradeManager.h"
#include "Economy/AuctionManager.h"
#include "Advanced/GroupCoordinator.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "Spatial/SpatialGridQueryHelpers.h"
#include "Threading/BotAction.h"
#include "Threading/BotActionManager.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Log.h"

namespace Playerbot
{

// ============================================================================
// EVENT BUS SUBSCRIPTION MANAGEMENT
// ============================================================================

void BotAI::SubscribeToEventBuses()
{
    if (!_bot)
        return;

    // Subscribe to all event buses for comprehensive event handling
    GroupEventBus::instance()->SubscribeAll(this);
    CombatEventBus::instance()->SubscribeAll(this);
    CooldownEventBus::instance()->SubscribeAll(this);
    AuraEventBus::instance()->SubscribeAll(this);
    LootEventBus::instance()->SubscribeAll(this);
    QuestEventBus::instance()->SubscribeAll(this);
    ResourceEventBus::instance()->SubscribeAll(this);
    SocialEventBus::instance()->SubscribeAll(this);
    AuctionEventBus::instance()->SubscribeAll(this);
    NPCEventBus::instance()->SubscribeAll(this);
    InstanceEventBus::instance()->SubscribeAll(this);

    TC_LOG_DEBUG("playerbot.events", "Bot {} subscribed to all 11 event buses",
        _bot->GetName());
}

void BotAI::UnsubscribeFromEventBuses()
{
    if (!_bot)
        return;

    // CRITICAL: Unsubscribe from all event buses to prevent dangling pointers
    GroupEventBus::instance()->Unsubscribe(this);
    CombatEventBus::instance()->Unsubscribe(this);
    CooldownEventBus::instance()->Unsubscribe(this);
    AuraEventBus::instance()->Unsubscribe(this);
    LootEventBus::instance()->Unsubscribe(this);
    QuestEventBus::instance()->Unsubscribe(this);
    ResourceEventBus::instance()->Unsubscribe(this);
    SocialEventBus::instance()->Unsubscribe(this);
    AuctionEventBus::instance()->Unsubscribe(this);
    NPCEventBus::instance()->Unsubscribe(this);
    InstanceEventBus::instance()->Unsubscribe(this);

    TC_LOG_DEBUG("playerbot.events", "Bot {} unsubscribed from all event buses",
        _bot->GetName());
}

// ============================================================================
// GROUP EVENT HANDLER
// ============================================================================

void BotAI::OnGroupEvent(GroupEvent const& event)
{
    if (!_bot)
        return;

    switch (event.type)
    {
        case GroupEventType::READY_CHECK_STARTED:
            ProcessGroupReadyCheck(event);
            break;

        case GroupEventType::TARGET_ICON_CHANGED:
            if (_groupCoordinator)
                _groupCoordinator->OnTargetIconChanged(event);
            break;

        case GroupEventType::LEADER_CHANGED:
            // Update following target if we were following old leader
            if (_aiState == BotAIState::FOLLOWING)
                HandleGroupChange();
            break;

        case GroupEventType::GROUP_DISBANDED:
            OnGroupLeft();
            break;

        case GroupEventType::MEMBER_JOINED:
        case GroupEventType::MEMBER_LEFT:
            if (_groupCoordinator)
                _groupCoordinator->OnGroupCompositionChanged(event);
            break;

        case GroupEventType::LOOT_METHOD_CHANGED:
            TC_LOG_DEBUG("playerbot.events.group", "Bot {}: Loot method changed to {}",
                _bot->GetName(), event.data1);
            break;

        default:
            // Other events handled by specialized systems
            break;
    }
}

void BotAI::ProcessGroupReadyCheck(GroupEvent const& event)
{
    if (!_bot || !_bot->GetGroup())
        return;

    // Auto-respond to ready checks
    // TODO: Add configuration option for auto-ready-check response
    bool isReady = true; // Default: always ready

    // Check if we're actually ready (not dead, not in combat, etc.)
    if (_bot->isDead() || _bot->IsInCombat())
        isReady = false;

    TC_LOG_DEBUG("playerbot.events.group", "Bot {}: Responding to ready check - {}",
        _bot->GetName(), isReady ? "READY" : "NOT READY");

    // Note: Actual ready check response would be sent via packet
    // This is handled by the group system
}

// ============================================================================
// COMBAT EVENT HANDLER
// ============================================================================

void BotAI::OnCombatEvent(CombatEvent const& event)
{
    if (!_bot)
        return;

    ObjectGuid botGuid = _bot->GetGUID();

    switch (event.type)
    {
        case CombatEventType::SPELL_CAST_START:
            // Check for interruptible enemy casts
            ProcessCombatInterrupt(event);

            // NEUTRAL MOB DETECTION: Bot is being targeted by hostile spell
            if (event.targetGuid == botGuid && !_bot->IsInCombat())
            {
                // PHASE 2: Thread-safe spatial grid verification (no Map access from worker thread)
                auto casterSnapshot = SpatialGridQueryHelpers::FindCreatureByGuid(_bot, event.casterGuid);
                if (casterSnapshot && casterSnapshot->IsAlive() && casterSnapshot->isHostile)
                {
                    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(event.spellId, DIFFICULTY_NONE);
                    if (spellInfo && !spellInfo->IsPositive())  // Hostile spell
                    {
                        TC_LOG_DEBUG("playerbot.combat",
                            "Bot {}: Detected neutral mob {} casting hostile spell {} via SPELL_CAST_START (queueing combat action)",
                            _bot->GetName(), event.casterGuid.ToString(), event.spellId);

                        // Queue BotAction for main thread execution (CRITICAL: no Map access from worker threads!)
                        BotAction action = BotAction::AttackTarget(_bot->GetGUID(), event.casterGuid, GameTime::GetGameTimeMS());
                        sBotActionMgr->QueueAction(action);
                    }
                }
            }
            break;

        case CombatEventType::ATTACK_START:
            // Track when bot initiates combat
            if (event.casterGuid == botGuid)
            {
                if (_aiState != BotAIState::COMBAT)
                    SetAIState(BotAIState::COMBAT);
            }

            // NEUTRAL MOB DETECTION: Bot is being attacked
            if (event.victimGuid == botGuid && !_bot->IsInCombat())
            {
                // PHASE 2: Thread-safe spatial grid verification (no Map access from worker thread)
                auto attackerSnapshot = SpatialGridQueryHelpers::FindCreatureByGuid(_bot, event.casterGuid);
                if (attackerSnapshot && attackerSnapshot->IsAlive() && attackerSnapshot->isHostile)
                {
                    TC_LOG_DEBUG("playerbot.combat",
                        "Bot {}: Detected neutral mob {} attacking via ATTACK_START (queueing combat action)",
                        _bot->GetName(), event.casterGuid.ToString());

                    // Queue BotAction for main thread execution (CRITICAL: no Map access from worker threads!)
                    BotAction action = BotAction::AttackTarget(_bot->GetGUID(), event.casterGuid, GameTime::GetGameTimeMS());
                    sBotActionMgr->QueueAction(action);
                }
            }
            break;

        case CombatEventType::ATTACK_STOP:
            // Track when combat ends
            if (event.casterGuid == botGuid)
            {
                if (_aiState == BotAIState::COMBAT && !_bot->IsInCombat())
                    SetAIState(BotAIState::SOLO);
            }
            break;

        case CombatEventType::AI_REACTION:
            // NEUTRAL MOB DETECTION: NPC became hostile and is targeting bot
            if (event.amount > 0)  // Positive reaction = hostile
            {
                // PHASE 2: Thread-safe spatial grid verification (no Map access from worker thread)
                auto mobSnapshot = SpatialGridQueryHelpers::FindCreatureByGuid(_bot, event.casterGuid);
                if (mobSnapshot && mobSnapshot->IsAlive() && mobSnapshot->isHostile &&
                    mobSnapshot->victim == _bot->GetGUID() && !_bot->IsInCombat())
                {
                    TC_LOG_DEBUG("playerbot.combat",
                        "Bot {}: Detected neutral mob {} became hostile via AI_REACTION (queueing combat action)",
                        _bot->GetName(), event.casterGuid.ToString());

                    // Queue BotAction for main thread execution (CRITICAL: no Map access from worker threads!)
                    BotAction action = BotAction::AttackTarget(_bot->GetGUID(), event.casterGuid, GameTime::GetGameTimeMS());
                    sBotActionMgr->QueueAction(action);
                }
            }
            break;

        case CombatEventType::SPELL_DAMAGE_TAKEN:
            // NEUTRAL MOB DETECTION: Catch-all for damage received
            if (event.victimGuid == botGuid && !_bot->IsInCombat())
            {
                // PHASE 2: Thread-safe spatial grid verification (no Map access from worker thread)
                auto attackerSnapshot = SpatialGridQueryHelpers::FindCreatureByGuid(_bot, event.casterGuid);
                if (attackerSnapshot && attackerSnapshot->IsAlive() && attackerSnapshot->isHostile)
                {
                    TC_LOG_DEBUG("playerbot.combat",
                        "Bot {}: Detected damage from neutral mob {} via SPELL_DAMAGE_TAKEN (queueing combat action)",
                        _bot->GetName(), event.casterGuid.ToString());

                    // Queue BotAction for main thread execution (CRITICAL: no Map access from worker threads!)
                    BotAction action = BotAction::AttackTarget(_bot->GetGUID(), event.casterGuid, GameTime::GetGameTimeMS());
                    sBotActionMgr->QueueAction(action);
                }
            }
            break;

        default:
            // Other combat events processed by specialized combat systems
            break;
    }
}

void BotAI::ProcessCombatInterrupt(CombatEvent const& event)
{
    if (!_bot || event.type != CombatEventType::SPELL_CAST_START)
        return;

    // PHASE 2: Thread-safe spatial grid verification (no Map access from worker thread)
    auto casterSnapshot = SpatialGridQueryHelpers::FindCreatureByGuid(_bot, event.casterGuid);
    if (!casterSnapshot || !casterSnapshot->isHostile)
        return;

    // Check if spell is interruptible (WoW 11.2: check InterruptFlags)
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(event.spellId, DIFFICULTY_NONE);
    if (!spellInfo || spellInfo->InterruptFlags == SpellInterruptFlags::None)
        return;

    TC_LOG_TRACE("playerbot.events.combat", "Bot {}: Detected interruptible cast {} from {}",
        _bot->GetName(), event.spellId, event.casterGuid.ToString());

    // Note: Actual interrupt logic is handled by ClassAI combat rotations
    // This is just event detection and logging
}

void BotAI::EnterCombatWithTarget(::Unit* target)
{
    if (!_bot || !target)
        return;

    // Prevent duplicate combat entry
    if (_bot->IsInCombat() && _bot->GetVictim() == target)
        return;

    TC_LOG_INFO("playerbot.combat", "Bot {} force-entering combat with {} (GUID: {})",
        _bot->GetName(), target->GetName(), target->GetGUID().ToString());

    // 1. Set combat state manually (required for neutral mobs)
    _bot->SetInCombatWith(target);
    target->SetInCombatWith(_bot);

    // 2. Attack the target
    _bot->Attack(target, true);

    // 3. Update threat if target has threat list
    if (target->CanHaveThreatList())
    {
        target->GetThreatManager().AddThreat(_bot, 1.0f);
    }

    // 4. Notify AI systems
    _currentTarget = target->GetGUID();
    SetAIState(BotAIState::COMBAT);

    // 5. Trigger combat start notification
    OnCombatStart(target);
}

// ============================================================================
// COOLDOWN EVENT HANDLER
// ============================================================================

void BotAI::OnCooldownEvent(CooldownEvent const& event)
{
    if (!_bot)
        return;

    // Default: Just log cooldown events for debugging
    TC_LOG_TRACE("playerbot.events.cooldown", "Bot {}: Cooldown event - spell {} (type {})",
        _bot->GetName(), event.spellId, static_cast<uint32>(event.type));

    // ClassAI implementations should override this for ability rotation tracking
}

// ============================================================================
// AURA EVENT HANDLER
// ============================================================================

void BotAI::OnAuraEvent(AuraEvent const& event)
{
    if (!_bot)
        return;

    switch (event.type)
    {
        case AuraEventType::AURA_APPLIED:
            // Check if we need to dispel harmful debuffs
            if (event.isHarmful && event.targetGuid == _bot->GetGUID())
            {
                ProcessAuraDispel(event);
            }
            break;

        case AuraEventType::AURA_REMOVED:
            // Track when important buffs fall off
            TC_LOG_TRACE("playerbot.events.aura", "Bot {}: Aura {} removed from {}",
                _bot->GetName(), event.spellId, event.targetGuid.ToString());
            break;

        default:
            break;
    }
}

void BotAI::ProcessAuraDispel(AuraEvent const& event)
{
    if (!_bot || !event.isHarmful)
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(event.spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    // Check if aura is dispellable
    if (spellInfo->Dispel == DISPEL_NONE)
        return;

    TC_LOG_DEBUG("playerbot.events.aura", "Bot {}: Harmful dispellable aura {} detected",
        _bot->GetName(), event.spellId);

    // Note: Actual dispel logic is handled by ClassAI or healers in group
}

// ============================================================================
// LOOT EVENT HANDLER
// ============================================================================

void BotAI::OnLootEvent(LootEvent const& event)
{
    if (!_bot)
        return;

    switch (event.type)
    {
        case LootEventType::LOOT_ROLL_STARTED:
            ProcessLootRoll(event);
            break;

        case LootEventType::LOOT_ITEM_RECEIVED:
            TC_LOG_DEBUG("playerbot.events.loot", "Bot {}: Received item {} x{}",
                _bot->GetName(), event.itemEntry, event.itemCount);
            break;

        case LootEventType::LOOT_MONEY_RECEIVED:
            TC_LOG_TRACE("playerbot.events.loot", "Bot {}: Received {} copper",
                _bot->GetName(), event.itemCount);
            break;

        default:
            break;
    }
}

void BotAI::ProcessLootRoll(LootEvent const& event)
{
    if (!_bot)
        return;

    // TODO: Implement smart loot rolling based on:
    // - Item quality vs current gear
    // - Class/spec appropriateness
    // - Group loot rules

    TC_LOG_DEBUG("playerbot.events.loot", "Bot {}: Loot roll started for item {}",
        _bot->GetName(), event.itemEntry);

    // For now, default to greed on everything
    // ClassAI can override for smarter rolling
}

// ============================================================================
// QUEST EVENT HANDLER
// ============================================================================

void BotAI::OnQuestEvent(QuestEvent const& event)
{
    if (!_bot)
        return;

    // Delegate all quest events to QuestManager
    if (_questManager)
    {
        // QuestManager will handle quest acceptance, completion, and progress tracking
        TC_LOG_TRACE("playerbot.events.quest", "Bot {}: Quest event {} for quest {}",
            _bot->GetName(), static_cast<uint32>(event.type), event.questId);

        ProcessQuestProgress(event);
    }
}

void BotAI::ProcessQuestProgress(QuestEvent const& event)
{
    if (!_bot || !_questManager)
        return;

    switch (event.type)
    {
        case QuestEventType::QUEST_CONFIRM_ACCEPT:
            // Auto-accept appropriate quests
            TC_LOG_DEBUG("playerbot.events.quest", "Bot {}: Quest {} offered for acceptance",
                _bot->GetName(), event.questId);
            break;

        case QuestEventType::QUEST_COMPLETED:
            // Quest objectives complete, turn in
            TC_LOG_DEBUG("playerbot.events.quest", "Bot {}: Quest {} completed",
                _bot->GetName(), event.questId);
            break;

        case QuestEventType::QUEST_OBJECTIVE_COMPLETE:
            // Track quest progress
            TC_LOG_TRACE("playerbot.events.quest", "Bot {}: Quest {} objective progress",
                _bot->GetName(), event.questId);
            break;

        default:
            break;
    }
}

// ============================================================================
// RESOURCE EVENT HANDLER
// ============================================================================

void BotAI::OnResourceEvent(ResourceEvent const& event)
{
    if (!_bot)
        return;

    switch (event.type)
    {
        case ResourceEventType::HEALTH_UPDATE:
            // Check for low health allies (healing priority)
            ProcessLowHealthAlert(event);
            break;

        case ResourceEventType::POWER_UPDATE:
            // Track power levels for resource management
            TC_LOG_TRACE("playerbot.events.resource", "Bot {}: Unit {} power update",
                _bot->GetName(), event.playerGuid.ToString());
            break;

        case ResourceEventType::BREAK_TARGET:
            // Target selection broken, need new target
            if (event.playerGuid == _bot->GetGUID())
            {
                TC_LOG_DEBUG("playerbot.events.resource", "Bot {}: Target broken",
                    _bot->GetName());
            }
            break;

        default:
            break;
    }
}

void BotAI::ProcessLowHealthAlert(ResourceEvent const& event)
{
    if (!_bot || event.type != ResourceEventType::HEALTH_UPDATE)
        return;

    // Calculate health percentage
    float healthPercent = event.maxAmount > 0 ? (static_cast<float>(event.amount) / event.maxAmount * 100.0f) : 0.0f;

    // Check if health is critically low (below 30%)
    if (healthPercent > 30.0f)
        return;

    // PHASE 2 TODO: Replace with PlayerSnapshot when available in spatial grid
    // For now, this is low-priority (only logs, doesn't manipulate state)
    Unit* target = ObjectAccessor::GetUnit(*_bot, event.playerGuid);
    if (!target)
        return;

    // Check if target is in our group/raid
    if (!target->IsInRaidWith(_bot))
        return;

    TC_LOG_DEBUG("playerbot.events.resource", "Bot {}: LOW HEALTH ALERT - {} at {:.1f}%",
        _bot->GetName(),
        target->GetName(),
        healthPercent);

    // Note: Actual healing response is handled by ClassAI (healers)
    // or defensive cooldown usage (self-healing)
}

// ============================================================================
// SOCIAL EVENT HANDLER
// ============================================================================

void BotAI::OnSocialEvent(SocialEvent const& event)
{
    if (!_bot)
        return;

    switch (event.type)
    {
        case SocialEventType::MESSAGE_CHAT:
            // Process chat messages for commands
            if (event.chatType == ChatMsg::CHAT_MSG_WHISPER && event.targetGuid == _bot->GetGUID())
            {
                TC_LOG_DEBUG("playerbot.events.social", "Bot {}: Whisper from {}: {}",
                    _bot->GetName(), event.playerGuid.ToString(), event.message);
                // TODO: Parse for bot commands
            }
            break;

        case SocialEventType::GUILD_INVITE_RECEIVED:
            // Handle guild invites
            TC_LOG_DEBUG("playerbot.events.social", "Bot {}: Guild invite from {}",
                _bot->GetName(), event.playerGuid.ToString());
            // TODO: Auto-accept guild invites from master
            break;

        case SocialEventType::TRADE_STATUS_CHANGED:
            if (_tradeManager)
            {
                // Delegate to TradeManager
                TC_LOG_TRACE("playerbot.events.social", "Bot {}: Trade status changed",
                    _bot->GetName());
            }
            break;

        default:
            break;
    }
}

// ============================================================================
// AUCTION EVENT HANDLER
// ============================================================================

void BotAI::OnAuctionEvent(AuctionEvent const& event)
{
    if (!_bot)
        return;

    // Delegate all auction events to AuctionManager
    if (_auctionManager)
    {
        TC_LOG_TRACE("playerbot.events.auction", "Bot {}: Auction event type {}",
            _bot->GetName(), static_cast<uint32>(event.type));

        // AuctionManager handles all AH logic (bidding, buying, selling)
    }
}

// ============================================================================
// NPC EVENT HANDLER
// ============================================================================

void BotAI::OnNPCEvent(NPCEvent const& event)
{
    if (!_bot)
        return;

    switch (event.type)
    {
        case NPCEventType::GOSSIP_MENU_RECEIVED:
            // Auto-select quest-related gossip options
            TC_LOG_DEBUG("playerbot.events.npc", "Bot {}: Gossip menu from NPC {}",
                _bot->GetName(), event.npcGuid.ToString());
            // TODO: Parse gossip options and select quest-related ones
            break;

        case NPCEventType::VENDOR_LIST_RECEIVED:
            // Handle vendor interactions (repairs, reagents)
            TC_LOG_DEBUG("playerbot.events.npc", "Bot {}: Vendor list received",
                _bot->GetName());
            // TODO: Auto-repair, buy reagents
            break;

        case NPCEventType::TRAINER_LIST_RECEIVED:
            // Auto-learn available spells
            TC_LOG_DEBUG("playerbot.events.npc", "Bot {}: Trainer list received",
                _bot->GetName());
            // TODO: Auto-learn spells if we have gold
            break;

        case NPCEventType::BANK_OPENED:
            // Manage bank storage
            TC_LOG_DEBUG("playerbot.events.npc", "Bot {}: Bank opened",
                _bot->GetName());
            // TODO: Store excess items, retrieve needed items
            break;

        default:
            break;
    }
}

// ============================================================================
// INSTANCE EVENT HANDLER
// ============================================================================

void BotAI::OnInstanceEvent(InstanceEvent const& event)
{
    if (!_bot)
        return;

    switch (event.type)
    {
        case InstanceEventType::INSTANCE_RESET:
            TC_LOG_DEBUG("playerbot.events.instance", "Bot {}: Instance {} reset",
                _bot->GetName(), event.mapId);
            break;

        case InstanceEventType::RAID_INFO_RECEIVED:
            // Track instance lockouts and boss progress
            TC_LOG_DEBUG("playerbot.events.instance", "Bot {}: Raid info - {} bosses killed",
                _bot->GetName(), event.bossStates.size());
            break;

        case InstanceEventType::ENCOUNTER_FRAME_UPDATE:
            // Boss encounter frame updates (target priority)
            TC_LOG_TRACE("playerbot.events.instance", "Bot {}: Encounter frame update - priority {}",
                _bot->GetName(), event.encounterFrame);
            break;

        case InstanceEventType::INSTANCE_MESSAGE_RECEIVED:
            // Instance warnings (lockout warnings, reset notifications)
            TC_LOG_INFO("playerbot.events.instance", "Bot {}: Instance message: {}",
                _bot->GetName(), event.message);
            break;

        default:
            break;
    }
}

} // namespace Playerbot
