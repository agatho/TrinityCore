// BotIntentExecutor - Variant visitor that turns Intents into PlayerbotAPI
// calls on the world thread. Lives in the module-side because the variant
// shape is module-internal; the API itself stays POD.

#include "BotIntent.h"
#include "BotAI.h"
#include "BotRegistry.h"
#include "../Services.h"
#include "../Fleet/BotGuildCharter.h"
#include "../Fleet/BotGuildMgr.h"
#include "../Fleet/CraftOrderBoard.h"
#include "../Fleet/JunkQuestResolver.h"
#include "../Threading/IntentQueue.h"
#include "../PlayerbotV2.h"
#include "../Diagnostics/PerfCounters.h"
#include "PlayerbotAPI.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "Creature.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "GameTime.h"
#include "Log.h"
#include <variant>
#include <fmt/format.h>

namespace Playerbot {

namespace {

// Visitor: dispatches to one of the API methods based on the active variant.
// Most intents are 1:1 with an API call; a few (Whisper / Group ops) accept
// strings.
struct IntentVisitor
{
    API&    api;
    BotAI*  bot_ai = nullptr;  // optional — Combo-Strikes-aware specs read this

    Result operator()(CastSpellIntent const& i)
    {
        Result const r = api.cast_spell(i.spell_id, i.target);
        if (r == Result::Ok && bot_ai)
            bot_ai->set_last_cast_spell_id(i.spell_id);
        return r;
    }
    Result operator()(GroundTargetSpellIntent const& i) { return api.cast_spell_at_position(i.spell_id, i.x, i.y, i.z); }
    Result operator()(MoveToIntent const& i)           { return api.move_to(i.x, i.y, i.z, i.run); }
    Result operator()(TeleportToIntent const& i)       { return api.teleport_to(i.map_id, i.x, i.y, i.z, i.o); }
    Result operator()(StopMovementIntent const& i)     { return api.stop_movement(i.clear_generators); }
    Result operator()(JumpIntent const& i)             { return api.jump(i.forward); }
    Result operator()(HearthIntent const&)             { return api.hearth(); }
    Result operator()(StartAttackIntent const& i)      { return api.start_attack(i.target); }
    Result operator()(StopAttackIntent const& i)       { return api.stop_attack(i.clear_ghost_combat); }
    Result operator()(PetAttackIntent const& i)        { return api.pet_attack(i.target); }
    Result operator()(PetCastSpellIntent const& i)     { return api.pet_cast(i.spell_id, i.target); }
    Result operator()(DismissPetIntent const&)         { return api.dismiss_pet(); }
    Result operator()(SetStandStateIntent const& i)    { return api.set_stand_state(i.stand_state); }
    // SayChat/YellChat/EmoteChat/GuildChat/RaidChat/OfficerChat/RaidWarning/
    // Whisper/PartyChat are bundled into `ChatIntent`. See operator()(ChatIntent).
    Result operator()(UnstuckIntent const& i)          { return api.unstuck(i.distance); }
    Result operator()(NearTeleportToIntent const& i)   { return api.near_teleport_to(i.x, i.y, i.z, i.o); }
    Result operator()(CancelAuraIntent const& i)       { return api.cancel_aura(i.spell_id); }
    Result operator()(CancelCastIntent const&)         { return api.cancel_cast(); }
    Result operator()(FollowIntent const& i)           { return api.follow(i.leader, i.distance, i.angle_radians); }
    Result operator()(DismountIntent const&)           { return api.dismount(); }
    Result operator()(MountIntent const& i)            { return api.mount(i.mount_id); }
    // PartyChat / Whisper bundled into ChatIntent.
    Result operator()(ReleaseCorpseIntent const&)      { return api.release_corpse(); }
    Result operator()(ReviveAtCorpseIntent const&)     { return api.revive_at_corpse(); }
    Result operator()(ReclaimCorpseIntent const&)      { return api.reclaim_corpse(); }
    Result operator()(SpiritResurrectIntent const&)    { return api.spirit_resurrect(); }
    Result operator()(AcceptRezIntent const&)          { return api.accept_rez(); }
    Result operator()(GroupAcceptIntent const&)        { return api.accept_group_invite(); }
    Result operator()(GroupDeclineIntent const&)       { return api.decline_group_invite(); }
    Result operator()(InviteToGroupIntent const& i)    { return api.invite_to_group(i.target); }
    Result operator()(GroupReadyResponseIntent const& i){ return api.group_ready_response(i.ready); }
    Result operator()(GroupLeaveIntent const&)         { return api.leave_group(); }
    Result operator()(GroupPromoteToLeaderIntent const& i) { return api.promote_to_leader(i.new_leader_guid); }
    Result operator()(GroupKickMemberIntent const& i)      { return api.kick_group_member(i.member_guid); }
    Result operator()(GroupConvertToRaidIntent const&)     { return api.convert_to_raid(); }
    Result operator()(GroupStartReadyCheckIntent const&)   { return api.start_ready_check(); }
    Result operator()(GroupSetAssistantIntent const& i)    { return api.set_assistant(i.member_guid, i.assistant); }
    Result operator()(ResetInstancesIntent const&)         { return api.reset_instances(); }
    Result operator()(GuildAcceptInviteIntent const&)      { return api.accept_guild_invite(); }
    Result operator()(GuildDeclineInviteIntent const&)     { return api.decline_guild_invite(); }
    Result operator()(GuildLeaveIntent const&)             { return api.leave_guild(); }
    Result operator()(ToggleAfkIntent const&)              { return api.toggle_afk(); }
    Result operator()(ToggleDndIntent const&)              { return api.toggle_dnd(); }
    Result operator()(SetDungeonDifficultyIntent const& i) { return api.set_dungeon_difficulty(i.difficulty_id); }
    Result operator()(SetRaidDifficultyIntent const& i)    { return api.set_raid_difficulty(i.difficulty_id, i.legacy); }
    // OfficerChat / RaidWarning bundled into ChatIntent.
    Result operator()(PerformEmoteIntent const& i)         { return api.perform_emote(i.emote_id, i.target); }
    Result operator()(FaceTargetIntent const& i)           { return api.face_target(i.target); }
    // VendorBuyByEntry / VendorBuy / VendorSell / VendorSellTrash / RepairAll /
    // VendorBuyByCategory bundled into VendorIntent.
    Result operator()(TogglePvpIntent const&)              { return api.toggle_pvp(); }
    Result operator()(AddFriendIntent const& i)            { return api.add_friend(i.name, i.note); }
    Result operator()(RemoveFriendIntent const& i)         { return api.remove_friend(i.friend_guid); }
    Result operator()(AddIgnoreIntent const& i)            { return api.add_ignore(i.name); }
    Result operator()(RemoveIgnoreIntent const& i)         { return api.remove_ignore(i.ignore_guid); }
    // MailSendMoney / MailSendItem / MailTakeMoney / MailTakeItem / MailDelete
    // bundled into MailIntent.
    Result operator()(CalendarRsvpAllIntent const& i)      { return api.calendar_rsvp_all_pending(i.accept); }
    // SwapPetToSlot / DeleteStabledPet / SummonPetByNumber / FeedPet /
    // AbandonPet / PetSetReactState / PetSetCommandState / RenamePet /
    // PetToggleAutocast bundled into HunterPetIntent.
    Result operator()(GuildBankDepositMoneyIntent const& i)  { return api.guild_bank_deposit_money(i.banker, i.amount); }
    Result operator()(GuildBankWithdrawMoneyIntent const& i) { return api.guild_bank_withdraw_money(i.banker, i.amount); }
    Result operator()(GuildBankDepositItemIntent const& i)   { return api.guild_bank_deposit_item(i.banker, i.tab, i.bank_slot, i.player_bag, i.player_slot, i.count); }
    Result operator()(GuildBankWithdrawItemIntent const& i)  { return api.guild_bank_withdraw_item(i.banker, i.tab, i.bank_slot, i.player_bag, i.player_slot, i.count); }
    Result operator()(ShareQuestIntent const& i)             { return api.share_quest_with_party(i.quest_id); }
    // PetSetReactState / PetSetCommandState / RenamePet / PetToggleAutocast
    // bundled into HunterPetIntent.
    Result operator()(SummonAcceptIntent const&)       { return api.accept_summon(); }
    Result operator()(SummonDeclineIntent const&)      { return api.decline_summon(); }
    Result operator()(DuelDeclineIntent const&)        { return api.decline_duel(); }
    Result operator()(DuelAcceptIntent const&)         { return api.accept_duel(); }
    Result operator()(SetRaidTargetIconIntent const& i){ return api.set_raid_target_icon(i.symbol, i.target); }
    Result operator()(ActivateSpecIntent const& i)     { return api.activate_spec(i.spec_id); }
    Result operator()(ResetCooldownsIntent const&)     { return api.reset_all_cooldowns(); }
    Result operator()(TradeDeclineIntent const&)       { return api.decline_trade(); }
    Result operator()(UseItemByEntryIntent const& i)   { return api.use_item_by_entry(i.item_entry, i.target); }
    Result operator()(UseItemIntent const& i)          { return api.use_item_by_slot(i.bag, i.slot, i.target); }
    Result operator()(LootIntent const& i)              { return api.loot_corpse(i.corpse_or_object); }
    Result operator()(InteractWithNpcIntent const& i)  { return api.interact_with_npc(i.npc); }
    Result operator()(GossipSelectIntent const& i)     { return api.gossip_select_by_index(i.npc, i.option); }
    Result operator()(UseObjectIntent const& i)        { return api.use_game_object(i.object); }
    Result operator()(CastSpellOnItemIntent const& i)  { return api.cast_spell_on_item(i.spell_id, i.item_guid); }
    Result operator()(ApplyTalentBuildIntent const& i) { return api.apply_talent_build(i.context); }
    Result operator()(EnterVehicleIntent const& i)     { return api.enter_vehicle(i.vehicle, i.seat_id); }
    Result operator()(ExitVehicleIntent const&)        { return api.exit_vehicle(); }
    Result operator()(VehicleSpellIntent const& i)     { return api.cast_vehicle_spell(i.spell_id, i.target); }
    Result operator()(VehicleGroundSpellIntent const& i) { return api.cast_vehicle_spell_at(i.spell_id, i.x, i.y, i.z); }
    Result operator()(QuestAcceptIntent const& i)      { return api.accept_quest(i.npc, i.quest_id); }
    Result operator()(QuestCompleteIntent const& i)    { return api.complete_quest(i.npc, i.quest_id, i.reward_choice); }
    Result operator()(QuestAbandonIntent const& i)     { return api.abandon_quest(i.quest_id); }
    Result operator()(QuestSharedAcceptIntent const&)  { return api.accept_shared_quest(); }
    Result operator()(ResolveJunkQuestsIntent const&)
    {
        // Force-complete auto-push feature quests + resolve profession-spec
        // choices on the live Player (world thread). Policy lives in the module.
        auto r = Playerbot::V2::Fleet::JunkQuestResolver::RunFor(api.player());
        return (r.rewarded || r.abandoned) ? Result::Ok : Result::Other;
    }
    // MailTakeMoney / MailTakeItem / MailDelete bundled into MailIntent.
    Result operator()(TrainerBuySpellIntent const& i)  { return api.trainer_buy_spell(i.trainer_npc, i.spell_id); }
    Result operator()(TrainerBuyAllIntent const& i)    { return api.trainer_buy_all_available(i.trainer_npc); }
    Result operator()(BankDepositItemIntent const& i)  { return api.bank_deposit_item(i.banker, i.bag, i.slot); }
    Result operator()(BankWithdrawItemIntent const& i) { return api.bank_withdraw_item(i.banker, i.bag, i.slot); }
    Result operator()(DiscoverTaxiNodeIntent const& i) { return api.discover_taxi_node(i.flight_master); }
    Result operator()(FlyToNodeIntent const& i)        { return api.fly_to_node(i.flight_master, i.to_node); }
    // AuctionSellItem / AuctionCancel / AuctionCancelAll bundled into AuctionIntent.
    // BgQueue / BgLeave / BgPort / LfgQueue / LfgUnqueue / LfgProposalRespond /
    // LfgRoleCheck bundled into QueueIntent.
    Result operator()(ApplyStarterTalentsIntent const&){ return api.apply_starter_talents(); }
    Result operator()(LootRollIntent const& i)         { return api.loot_roll(i.loot_object, i.loot_list_id, i.vote_type); }
    Result operator()(BindHomebindIntent const& i)     { return api.bind_homebind(i.innkeeper); }
    Result operator()(EquipItemIntent const& i)        { return api.equip_item(i.from_bag, i.from_slot, i.to_slot); }

    // ---- Subsystem-wrapper handlers (variant-pressure relief) ----
    //
    // Each wrapper dispatches its inner variant via `std::visit` +
    // `if constexpr` — same pattern as `GuildIntent`. Keeps IntentBody
    // bounded; see feedback_intent_variant_capacity.md.
    Result operator()(ChatIntent const& ci)
    {
        return std::visit([&](auto const& op) -> Result {
            using T = std::decay_t<decltype(op)>;
            if      constexpr (std::is_same_v<T, SayChatIntent>)      return api.say(op.text);
            else if constexpr (std::is_same_v<T, YellChatIntent>)     return api.yell(op.text);
            else if constexpr (std::is_same_v<T, EmoteChatIntent>)    return api.emote_text(op.text);
            else if constexpr (std::is_same_v<T, GuildChatIntent>)    return api.guild_chat(op.text);
            else if constexpr (std::is_same_v<T, RaidChatIntent>)     return api.raid_chat(op.text);
            else if constexpr (std::is_same_v<T, OfficerChatIntent>)  return api.officer_chat(op.text);
            else if constexpr (std::is_same_v<T, RaidWarningIntent>)  return api.raid_warning(op.text);
            else if constexpr (std::is_same_v<T, WhisperIntent>)      return api.whisper(op.target, op.text);
            else if constexpr (std::is_same_v<T, PartyChatIntent>)    return api.party_chat(op.text);
            else                                                      return Result::Other;
        }, ci.op);
    }
    Result operator()(MailIntent const& mi)
    {
        return std::visit([&](auto const& op) -> Result {
            using T = std::decay_t<decltype(op)>;
            if      constexpr (std::is_same_v<T, MailTakeMoneyIntent>) return api.mail_take_money(op.mailbox, op.mail_id);
            else if constexpr (std::is_same_v<T, MailTakeItemIntent>)  return api.mail_take_item(op.mailbox, op.mail_id, op.item_guid_low);
            else if constexpr (std::is_same_v<T, MailDeleteIntent>)    return api.mail_delete(op.mailbox, op.mail_id);
            else if constexpr (std::is_same_v<T, MailSendMoneyIntent>) return api.mail_send_money(op.recipient, op.copper, op.subject, op.body);
            else if constexpr (std::is_same_v<T, MailSendItemIntent>)  return api.mail_send_item(op.recipient, op.item_guid, op.count, op.copper, op.cod, op.subject, op.body);
            else                                                       return Result::Other;
        }, mi.op);
    }
    Result operator()(AuctionIntent const& ai)
    {
        return std::visit([&](auto const& op) -> Result {
            using T = std::decay_t<decltype(op)>;
            if      constexpr (std::is_same_v<T, AuctionSellItemIntent>)  return api.auction_sell_item(op.auctioneer, op.item_guid, op.min_bid, op.buyout, op.run_time_minutes);
            else if constexpr (std::is_same_v<T, AuctionCancelIntent>)    return api.auction_cancel(op.auctioneer, op.auction_id);
            else if constexpr (std::is_same_v<T, AuctionCancelAllIntent>) return api.auction_cancel_all(op.auctioneer);
            else                                                          return Result::Other;
        }, ai.op);
    }
    Result operator()(VendorIntent const& vi)
    {
        return std::visit([&](auto const& op) -> Result {
            using T = std::decay_t<decltype(op)>;
            if      constexpr (std::is_same_v<T, VendorBuyIntent>)            return api.vendor_buy_by_slot(op.npc, op.vendor_slot, op.count);
            else if constexpr (std::is_same_v<T, VendorSellIntent>)           return api.sell_item_by_slot(op.npc, op.bag, op.slot, op.count);
            else if constexpr (std::is_same_v<T, VendorSellTrashIntent>)      return api.sell_trash(op.npc);
            else if constexpr (std::is_same_v<T, RepairAllIntent>)            return api.repair_all(op.npc, op.from_guild_bank);
            else if constexpr (std::is_same_v<T, VendorBuyByCategoryIntent>)  return api.vendor_buy_by_category(op.npc, op.item_class, op.item_subclass, op.total_count);
            else if constexpr (std::is_same_v<T, VendorBuyByEntryIntent>)     return api.vendor_buy_by_entry(op.npc, op.item_entry, op.count);
            else                                                              return Result::Other;
        }, vi.op);
    }
    Result operator()(HunterPetIntent const& hpi)
    {
        return std::visit([&](auto const& op) -> Result {
            using T = std::decay_t<decltype(op)>;
            if      constexpr (std::is_same_v<T, SwapPetToSlotIntent>)        return api.swap_pet_to_slot(op.pet_number, op.dst_slot);
            else if constexpr (std::is_same_v<T, DeleteStabledPetIntent>)     return api.delete_stabled_pet(op.pet_number);
            else if constexpr (std::is_same_v<T, SummonPetByNumberIntent>)    return api.summon_pet_by_number(op.pet_number);
            else if constexpr (std::is_same_v<T, FeedPetIntent>)              return api.feed_pet(op.food_item_entry);
            else if constexpr (std::is_same_v<T, AbandonPetIntent>)           return api.abandon_pet();
            else if constexpr (std::is_same_v<T, PetSetReactStateIntent>)     return api.pet_set_react_state(op.state);
            else if constexpr (std::is_same_v<T, PetSetCommandStateIntent>)   return api.pet_set_command_state(op.command);
            else if constexpr (std::is_same_v<T, RenamePetIntent>)            return api.rename_pet(op.new_name);
            else if constexpr (std::is_same_v<T, PetToggleAutocastIntent>)    return api.pet_toggle_autocast(op.spell_id, op.enabled);
            else                                                              return Result::Other;
        }, hpi.op);
    }
    Result operator()(QueueIntent const& qi)
    {
        return std::visit([&](auto const& op) -> Result {
            using T = std::decay_t<decltype(op)>;
            if      constexpr (std::is_same_v<T, BgQueueIntent>)              return api.bg_queue(op.battlemaster, op.bg_type_id, op.arena_type);
            else if constexpr (std::is_same_v<T, BgLeaveIntent>)              return api.bg_leave();
            else if constexpr (std::is_same_v<T, BgPortIntent>)               return api.bg_port(op.bg_type_id, op.accept);
            else if constexpr (std::is_same_v<T, LfgQueueIntent>)
            {
                // Role enum → LFG role bitmask. PLAYER_ROLE_LEADER (1) is OR'd
                // in unconditionally — solo queueing requires the bot to be its
                // own leader so role-check passes; in a group, the bit is harmless.
                // Without LEADER, sLFGMgr->JoinLfg silently rejects the entry.
                uint8 lfg_role = /*PLAYER_ROLE_LEADER*/ 1;
                switch (op.role)
                {
                    case Role::Tank:   lfg_role |= /*TANK*/   2; break;
                    case Role::Healer: lfg_role |= /*HEALER*/ 4; break;
                    case Role::Dps:    lfg_role |= /*DAMAGE*/ 8; break;
                    default: break;
                }
                return api.lfg_queue(op.dungeon_or_bg_id, lfg_role);
            }
            else if constexpr (std::is_same_v<T, LfgUnqueueIntent>)           return api.lfg_leave_queue();
            else if constexpr (std::is_same_v<T, LfgProposalRespondIntent>)   return api.lfg_proposal_respond(op.proposal_id, op.accept);
            else if constexpr (std::is_same_v<T, LfgRoleCheckIntent>)         return api.lfg_role_check(op.roles);
            else                                                              return Result::Other;
        }, qi.op);
    }

    // ---- Economy subsystem (#4B buy-side AH + future craft-orders) ----
    //
    // Single master-variant entry; the inner variant dispatches per op.
    // The API methods re-validate server-side (auctioneer in range, auction
    // still exists, not own auction, enough gold). See
    // feedback_intent_variant_capacity.md.
    Result operator()(EconomyIntent const& ei)
    {
        return std::visit([&](auto const& op) -> Result {
            using T = std::decay_t<decltype(op)>;
            if      constexpr (std::is_same_v<T, EconomyOp::AhBuyout>) return api.auction_buyout(op.auctioneer, op.auction_id, op.price);
            else if constexpr (std::is_same_v<T, EconomyOp::AhBid>)    return api.auction_bid(op.auctioneer, op.auction_id, op.bid);
            else if constexpr (std::is_same_v<T, EconomyOp::AhBuyCommodity>) return api.auction_buy_commodity(op.auctioneer, op.item_entry, op.quantity, op.max_total_price);
            else if constexpr (std::is_same_v<T, EconomyOp::CraftFulfill>)
            {
                // #4B-2(a): craft the product + mail it to the requester via the
                // (core-side) API, then RELEASE the escrow via the (module-side)
                // board. The API does the craft mechanics only (no module
                // dependency); the board owns the escrow. We only mark delivered
                // when the craft+mail actually succeeded, so a failed craft
                // leaves the order Claimed (to retry or time out) with the
                // escrow still held — gold is never paid for an undelivered
                // order. MarkDelivered re-verifies ownership + the human-
                // firewall and is the single one-time release point.
                Player* bot = api.player();
                const uint64 crafter_low = bot ? bot->GetGUID().GetCounter() : 0;
                Result const r = api.craft_fulfill_order(op.order_id, op.spell_id,
                                               op.item_entry, op.qty, op.requester_low);
                if (r == Result::Ok && crafter_low != 0)
                    Services::CraftOrders().MarkDelivered(op.order_id, crafter_low);
                return r;
            }
            else if constexpr (std::is_same_v<T, EconomyOp::CraftPost>)
            {
                // #4B-2(a) part 2: POST a craft order on the world thread. The
                // board debits the escrow atomically with the row write and
                // re-verifies the requester is a fleet bot (human-firewall).
                // PostOrder returns 0 on refusal (not a fleet bot / not in world
                // / can't afford / bad args); map that to ServerRefused so the
                // rule's dedup lockout still arms (no per-tick re-post spam) while
                // a successful post (id != 0) reports Ok.
                Player* bot = api.player();
                const uint64 requester_low = bot ? bot->GetGUID().GetCounter() : 0;
                if (requester_low == 0) return Result::ServerRefused;
                const uint64 id = Services::CraftOrders().PostOrder(
                    requester_low, op.spell_id, op.item_entry, op.quantity, op.payment);
                return id != 0 ? Result::Ok : Result::ServerRefused;
            }
            else if constexpr (std::is_same_v<T, EconomyOp::CraftClaim>)
            {
                // #4B-2(a) part 2: CLAIM the oldest Open order whose recipe this
                // bot knows. ClaimOpenOrder re-verifies the live spellbook +
                // fleet-bot status world-thread and flips the row to Claimed; the
                // claimed order then surfaces in the next snapshot's claimed_*
                // fields (which the fulfil rule turns into CraftFulfill). A
                // returned order with id == 0 means nothing was claimable.
                Player* bot = api.player();
                const uint64 crafter_low = bot ? bot->GetGUID().GetCounter() : 0;
                if (crafter_low == 0) return Result::ServerRefused;
                V2::CraftOrder const claimed = Services::CraftOrders().ClaimOpenOrder(crafter_low);
                return claimed.id != 0 ? Result::Ok : Result::ServerRefused;
            }
            else                                                       return Result::Other;
        }, ei.op);
    }

    // ---- Guild subsystem (Phase A.2 charter + future B/C/D/E) ----
    //
    // Single master-variant entry; the inner variant dispatches per
    // operation. See feedback_intent_variant_capacity.md for the
    // rationale (MSVC heap exhaustion at ~120 IntentBody alternatives).
    Result operator()(GuildIntent const& gi)
    {
        Player* bot = api.player();
        if (!bot) return Result::ServerRefused;
        return std::visit([&](auto const& op) -> Result {
            using T = std::decay_t<decltype(op)>;

            if constexpr (std::is_same_v<T, GuildOp::BuyCharter>)
            {
                Creature* npc = ObjectAccessor::GetCreature(*bot, op.petitioner_npc);
                if (!npc) return Result::InvalidTarget;
                uint64 charter_low = 0;
                const auto r = V2::BotBuyGuildCharter(bot, npc, op.guild_name, charter_low);
                if (r != V2::CharterBuyResult::Ok)
                    return Result::ServerRefused;
                if (BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter()))
                    ai->set_guild_charter_petition_low(charter_low);
                const auto faction = (Player::TeamForRace(bot->GetRace()) == ALLIANCE)
                    ? V2::BotGuildMgr::FACTION_ALLIANCE : V2::BotGuildMgr::FACTION_HORDE;
                Services::Guilds().SetActiveFounderPetitionLow(faction, charter_low);
                return Result::Ok;
            }
            else if constexpr (std::is_same_v<T, GuildOp::SignCharter>)
            {
                const auto r = V2::BotSignGuildCharter(bot, op.petition_item_low);
                if (r != V2::CharterSignResult::Ok)
                    return Result::ServerRefused;
                return Result::Ok;
            }
            else if constexpr (std::is_same_v<T, GuildOp::TurnInCharter>)
            {
                Creature* npc = ObjectAccessor::GetCreature(*bot, op.petitioner_npc);
                if (!npc) return Result::InvalidTarget;
                uint64 guild_id = 0;
                const auto r = V2::BotTurnInGuildCharter(bot, npc, op.petition_item_low, guild_id);
                if (r != V2::CharterTurnInResult::Ok)
                    return Result::ServerRefused;
                if (BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter()))
                {
                    const std::string name = ai->guild_charter_name();
                    const auto faction = (Player::TeamForRace(bot->GetRace()) == ALLIANCE)
                        ? V2::BotGuildMgr::FACTION_ALLIANCE : V2::BotGuildMgr::FACTION_HORDE;
                    Services::Guilds().OnCharterSucceeded(faction,
                        bot->GetGUID().GetCounter(), guild_id, name);
                    ai->advance_guild_charter_phase(0xFF);
                }
                // The founder is now a member too — track in
                // bot_guild_member_meta so Phase B hygiene knows their
                // join date (the founder isn't going through the normal
                // recruit path).
                Services::Guilds().OnBotJoinedGuild(guild_id, bot->GetGUID().GetCounter());
                return Result::Ok;
            }
            else if constexpr (std::is_same_v<T, GuildOp::RecruitTarget>)
            {
                ObjectGuid target_guid = ObjectGuid::Create<HighGuid::Player>(op.target_guid_low);
                Player* target = ObjectAccessor::FindConnectedPlayer(target_guid);
                if (!target) return Result::InvalidTarget;
                uint64 gid = 0;
                const auto r = V2::BotRecruitToGuild(bot, target, gid);
                if (r != V2::RecruitResult::Ok)
                    return Result::ServerRefused;
                // Stamp join-date for hygiene.
                Services::Guilds().OnBotJoinedGuild(gid, op.target_guid_low);
                return Result::Ok;
            }
            else if constexpr (std::is_same_v<T, GuildOp::RecruitChannelPost>)
            {
                // Phase E.1: post "<Guild> recruiting all levels, /w
                // <officer> for invite" into the bot's currently-joined
                // Trade channel. Skip silently when the bot has no
                // Trade channel (e.g. leveling in the open world).
                const uint64 gid = bot->GetGuildId();
                if (gid == 0) return Result::Other;
                Guild* g = sGuildMgr->GetGuildById(gid);
                if (!g) return Result::Other;
                ChannelMgr* cMgr = ChannelMgr::ForTeam(bot->GetTeam());
                if (!cMgr) return Result::Other;
                Channel* ch = cMgr->GetChannelForPlayerByNamePart("Trade", bot);
                if (!ch) return Result::Other;
                std::string msg = fmt::format(
                    "<{}> recruiting all levels, friendly social guild — /w {} for invite",
                    g->GetName(), bot->GetName());
                ch->Say(bot->GetGUID(), msg, /*lang*/ 0 /*LANG_UNIVERSAL — channel system overrides per-channel*/);
                return Result::Ok;
            }
            else
            {
                return Result::Other;
            }
        }, gi.op);
    }

    // Until the corresponding API methods are added in subsequent iterations,
    // unhandled intents silently no-op. They'll fail unit tests for whatever
    // subsystem expected them, surfacing the gap there rather than here.
    template <class T> Result operator()(T const&)     { return Result::Other; }
};

} // anonymous

size_t V2::Module::DrainIntents()
{
    if (!Services::Initialized()) return 0;
    auto& reg = Services::Registry();

    // Tick budgets:
    //   - Global cap stops the executor from monopolising the world tick
    //     even at 100+ bots all firing intents.
    //   - Per-bot cap keeps one runaway bot from draining the global cap
    //     before later bots in registry order get their turn. With 64/bot
    //     and a typical 1-3 intents per tick from a healthy bot, the cap
    //     only bites when a backlog has built up; the next world tick
    //     drains the rest.
    constexpr size_t kIntentBudgetPerWorldTick = 4096;
    constexpr size_t kIntentBudgetPerBotPerTick = 64;
    size_t executed = 0;

    // Open the per-tick bot-PATHFINDING budget window (distinct from the intent
    // COUNT budgets above — those don't bound Detour wall-time, which is what hung
    // the world thread 87s on 2026-06-17 when the quest funnel was raised). Each
    // synchronous pathfind site (API::move_to, Charge/Leap casts, near_teleport)
    // checks PathBudget::HasBudget and DEFERS (Result::Locked, retried next tick)
    // once this window is spent, bounding aggregate world-thread Detour per tick.
    // Closed (EndWorldTick) at function exit so the budget never leaks into
    // non-DrainIntents callers. 15ms is generous vs the natural re-path rate.
    constexpr uint32 kBotPathfindBudgetMs = 15;
    Playerbot::PathBudget::BeginWorldTick(GameTime::GetGameTimeMS(), kBotPathfindBudgetMs);

    reg.for_each([&](BotId id, BotRegistryEntry const& entry)
    {
        if (!entry.intents) return;
        // Fast-path: cheap empty-queue probe before the expensive setup
        // (GetGameTimeMS + FindConnectedPlayer + API construction). Most
        // bots have an empty intents queue on any given world tick since
        // DrainIntents fires at 50Hz but typical bots emit <1 intent/sec.
        if (entry.intents->approximate_size() == 0)
            return;
        // /wait pause: skip the bot entirely until paused_until_ms <= now.
        // Intents stay in the queue; they'll drain on the next world tick
        // after the gate clears.
        const uint32_t paused_until = entry.paused_until_ms.load(std::memory_order_relaxed);
        if (paused_until && GameTime::GetGameTimeMS() < paused_until)
            return;
        Player* p = ObjectAccessor::FindConnectedPlayer(
                        ObjectGuid::Create<HighGuid::Player>(id));
        if (!p) return;

        API api(p);
        Intent intent;
        size_t bot_executed = 0;
        const uint32_t drain_now_ms = GameTime::GetGameTimeMS();
        while (executed < kIntentBudgetPerWorldTick &&
               bot_executed < kIntentBudgetPerBotPerTick &&
               entry.intents->pop(intent))
        {
            // Defer-until check. Used for human-pacing of social
            // replies — the reactor composes the intent immediately
            // but stamps a future-time so the visible packet doesn't
            // go out until a human-like response window elapses.
            // We requeue the intent so a subsequent drain catches it.
            // (Slightly inefficient if many deferred intents stack up,
            // but the queue is per-bot and the count is small.)
            if (intent.defer_until_ms != 0 && drain_now_ms < intent.defer_until_ms)
            {
                entry.intents->push(std::move(intent));
                break;  // stop draining this bot this tick to avoid loop
            }
            try
            {
                Result r = std::visit(IntentVisitor{api, entry.ai.get()}, intent.body);
                Services::Perf().record_intent_executed();
                Services::Perf().record_intent_result(static_cast<size_t>(r));
                if (r != Result::Ok)
                    Services::Perf().record_intent_failed();

                // Per-target StartAttack lockout. Player::Attack returns
                // false (→ Result::ServerRefused) when the target is
                // immune / phased / faction-locked / vehicle-locked /
                // already attacking with same melee mode etc. The AI rule
                // re-emits next snapshot if we don't gate it; without this
                // mark, ~5 emits/sec from a single wedged bot saturate
                // the executor and stall the world thread (observed 60 s
                // hang in freeze_dump_2026_05_09_17_28_14.txt, bot 87300
                // L76 Paladin emitting StartAttack 32 times in 7 s).
                //
                // Result::InvalidTarget arms the SAME lockout. PlayerbotAPI
                // rejects a StartAttack when !IsValidAttackTarget (the target
                // flipped un-attackable: evading/leashed, immune, or feigned).
                // The Deadmines harbor desync is exactly this: ~8 hostiles the
                // tank aggroed across an off-mesh/elevated edge evade (it can't
                // path to them), stay in m_attackers (contact damage holds the
                // bot in combat) yet fail IsValidAttackTarget — so every tick
                // the tank re-emits StartAttack, the world thread rejects
                // before Player::Attack, GetVictim() stays empty and the
                // in_combat flag blinds the idle boss-navigator forever. Arming
                // the lockout here stops the per-tick spam AND marks the
                // attacker start_attack_recently_refused, which the in-combat
                // boss-advance below uses to detect "every attacker is
                // unreachable → advance toward the boss instead of wedging".
                if (r == Result::ServerRefused || r == Result::InvalidTarget)
                {
                    if (auto const* sa = std::get_if<StartAttackIntent>(&intent.body))
                    {
                        if (BotAI* botai = reg.ai(id))
                            botai->note_start_attack_refused(
                                sa->target.GetCounter(),
                                GameTime::GetGameTimeMS());
                    }
                }
                // Path-blocked feedback for the wander rule. API::move_to
                // returns Result::Locked when the navmesh refused the path
                // (NoPath / FarFromPolyEnd). Bumping path_blocked_count
                // shifts the wander angle bucket on the next emit so the
                // bot tries a different direction immediately, rather than
                // hammering the same blocked bearing for 5s. The same
                // counter feeds the per-rule diagnostics surface.
                if (r == Result::Ok &&
                    std::holds_alternative<MoveToIntent>(intent.body))
                {
                    // Move issued a real path/spline → not wedged. Reset the
                    // consecutive-block tally so blocks= reflects the CURRENT
                    // wedge depth, not lifetime history.
                    if (BotAI* botai = reg.ai(id))
                        botai->note_move_succeeded();
                }
                if (r == Result::Locked &&
                    std::holds_alternative<MoveToIntent>(intent.body))
                {
                    if (BotAI* botai = reg.ai(id))
                    {
                        botai->note_path_blocked(GameTime::GetGameTimeMS());
                        // Diagnostic linkage: pair the API-side path_fail
                        // log line with the rule that emitted the move.
                        // Without this we can't tell whether wander, hub-
                        // travel, gather-walk, or something else is the
                        // dominant source of off-mesh failures.
                        // A8 (2026-06-07): THROTTLE — this fired on EVERY Locked
                        // move = 27.9M lines in 4 days (top tag) and, because the
                        // playerbot.v2 logger also lists the Server appender, was
                        // double-written into the 52GB Server.log. Emit only at
                        // block-count milestones; the wedge depth is still legible
                        // and a true wedge still surfaces, without the per-tick flood.
                        const uint32 bc = botai->path_blocked_count();
                        char const* rule = botai->last_rule_fired();
                        // Every block at DEBUG (full per-tick detail available on
                        // demand via Logger.playerbot.v2=2); milestones at INFO
                        // (default-visible wedge signal — start + escalation —
                        // without the 27.9M-line per-tick flood).
                        if (bc == 1 || bc == 5 || bc == 20 || (bc % 100) == 0)
                            TC_LOG_INFO("playerbot.v2",
                                "[move_blocked] bot={} rule={} blocks={}",
                                id, rule ? rule : "(null)", bc);
                        else
                            TC_LOG_DEBUG("playerbot.v2",
                                "[move_blocked] bot={} rule={} blocks={}",
                                id, rule ? rule : "(null)", bc);
                    }
                }
                // Cast-rejected feedback. Audit 2026-05-17 found 329k
                // cast rejections in 5MB Playerbot.log — top spells
                // (Soulstone 20707 @ 54k, Create Healthstone 6201 @
                // 47k, Revive Pet 982 @ 38k) had buff/utility rules
                // emitting against a stale "target valid" view of the
                // snapshot every 1.5s lockout window. The fix: when
                // the rejection is PERSISTENT (broken target / unknown
                // spell / anti-cheat state), back off ~10s so the rule
                // picks a different action instead of hammering.
                //
                // CRITICAL — combat behaviour MUST NOT regress.
                //   * NotReady (server CD vs stale snapshot, ~200-1000ms
                //     resolution) → keep 1.5s lockout; the next snapshot
                //     picks up the real CD and the bot retries naturally.
                //   * OutOfRange / LoS (target stepped behind a pillar
                //     mid-fight, typically resolves in <1s) → keep 1.5s;
                //     a 10s back-off here would freeze the bot's
                //     follow-up casts after every minor movement.
                //   * NotEnoughResource (mana/power tick re-floods in
                //     1-3s) → keep 1.5s; combat regen rate is fast.
                //
                // Long back-off ONLY for:
                //   * InvalidTarget — null guid / wrong target type;
                //     the rule's target picker is broken, retrying
                //     with the same selection won't help.
                //   * NotKnown — bot doesn't have the spell at all;
                //     a buff rule that fires Soulstone on a non-Warlock
                //     bot, etc. Permanent until learned.
                //   * ServerRefused — anti-cheat / state / faction reject.
                //     Usually a class of condition that persists (e.g.
                //     bot is on a vehicle, in feign-death, etc).
                //   * Other — uncategorized exception path. Safe to
                //     back off; if the underlying cause clears the
                //     rule re-fires after 10s.
                const bool persistent_reject =
                    (r == Result::InvalidTarget) ||
                    (r == Result::NotKnown)      ||
                    (r == Result::ServerRefused) ||
                    (r == Result::Other);
                if (persistent_reject)
                {
                    if (auto const* cs = std::get_if<CastSpellIntent>(&intent.body))
                    {
                        if (BotAI* botai = reg.ai(id))
                            botai->note_cast_rejected(cs->spell_id,
                                                      GameTime::GetGameTimeMS());
                    }
                }
                // OutOfRange feedback for the combat:opener approach logic.
                // The opener cannot know each APL rule's range (melee 5y vs
                // caster 40y), so it relies on the server's own verdict: a
                // cast that came back OutOfRange/LoS means "you cannot reach
                // your selection from here" → the opener steps toward the
                // victim instead of letting the APL spam doomed casts every
                // retry window forever (observed 2026-06-13: bots looping
                // CastSpell|OutOfRange at ~1.6s cadence for hours). An Ok
                // cast clears the counter — we're in reach again.
                if (std::get_if<CastSpellIntent>(&intent.body))
                {
                    if (BotAI* botai = reg.ai(id))
                    {
                        if (r == Result::OutOfRange)
                            botai->note_cast_out_of_range();
                        else if (r == Result::Ok)
                            botai->reset_cast_oor();
                    }
                }
                // Quest reward-turnin failure backoff. API::complete_quest
                // returns Result::Locked when Player::CanRewardQuest fails
                // (cant_reward_pre / cant_reward_post) — i.e. the reward
                // can't be granted (missing/un-storable item, or full bags).
                // Without a give-up path, idle:quest_turnin re-fires every
                // tick on a quest stuck at QUEST_STATUS_COMPLETE (observed:
                // quest 26712, 120k retries in a 300MB log window). Record
                // the failure so QuestTurninFire skips it for an escalating
                // back-off window. Other failure Results (InvalidTarget /
                // OutOfRange) are transient — the giver moved or we drifted
                // out of range — and must NOT trigger the long back-off.
                if (r == Result::Locked)
                {
                    if (auto const* qc = std::get_if<QuestCompleteIntent>(&intent.body))
                    {
                        if (BotAI* botai = reg.ai(id))
                            botai->note_quest_reward_failed(qc->quest_id,
                                                            GameTime::GetGameTimeMS());
                    }
                }
                // Per-bot diagnostic ring. Recorded AFTER the API call so the
                // captured Result reflects what actually happened, not what
                // the AI worker hoped would happen. Used by /diag <bot>.
                reg.record_intent_history(id,
                                          GameTime::GetGameTimeMS(),
                                          static_cast<uint32>(intent.body.index()),
                                          static_cast<uint8>(r));
            }
            catch (...)
            {
                Services::Perf().record_exception();
                // Record the exception in the ring too so /diag surfaces
                // "Other" + the kind that threw — the most useful signal
                // for debugging an unhandled API path.
                reg.record_intent_history(id,
                                          GameTime::GetGameTimeMS(),
                                          static_cast<uint32>(intent.body.index()),
                                          static_cast<uint8>(Result::Other));
            }
            ++executed;
            ++bot_executed;
        }
    });
    // Close the pathfinding-budget window so it never throttles a pathfind issued
    // outside DrainIntents (HasBudget fails open when inactive).
    Playerbot::PathBudget::EndWorldTick();
    return executed;
}

} // namespace Playerbot
