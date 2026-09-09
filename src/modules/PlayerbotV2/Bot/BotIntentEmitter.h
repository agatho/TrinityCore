// BotIntentEmitter - Push-only handle to a bot's intent queue. Held by AI
// workers during tick. CONTRACTS.md §2.4.

#pragma once

#include "BotIntent.h"
#include "ObjectGuid.h"
#include "GameTime.h"
#include <string>
#include <utility>

namespace Playerbot {

class IntentQueue;
class BotAI;

class BotIntentEmitter
{
public:
    BotIntentEmitter(IntentQueue* queue, BotId bot_id, SnapshotVer source, IntentId* next_id_counter,
                     BotAI* ai = nullptr)
        : queue_(queue), bot_id_(bot_id), source_(source), next_id_(next_id_counter), ai_(ai) {}

    // Templated emit — the canonical path. Avoids ambiguity for callers.
    // Wraps subsystem inner-intent types (`SayChatIntent`, `LfgQueueIntent`,
    // etc.) into their master variant slot (`ChatIntent`, `QueueIntent`, …)
    // via `WrapForIntentBody`. Top-level intent types pass through unchanged.
    template <class T>
    bool emit(T body)
    {
        Intent i;
        i.id              = ++(*next_id_);
        i.bot_id          = bot_id_;
        i.source_snapshot = source_;
        i.body            = WrapForIntentBody(std::move(body));
        const bool ok = push(std::move(i));
        if (ok) ++emitted_count_;
        return ok;
    }

    // Number of intents actually PUSHED through this emitter (dedup-dropped
    // and queue-overflow emits don't count). Lets the APL engine distinguish
    // "rule acted" from "rule's emit was silently dropped" — the old
    // first-predicate-wins tick consumed the whole rotation tick even when
    // the action emitted nothing (cast lockout, dedup), starving every
    // lower-priority rule (audit B02: hunters never reached their focus
    // generator; bots wedged into auto-attack-only).
    uint32 emitted_count() const { return emitted_count_; }

    // Convenience helpers — the most-used intents have shorthand methods so
    // APL rules and state code read like prose. Add more as needed; they're
    // all `emit(...)` underneath.
    // cast() applies the optimistic per-spell emit cooldown (see BotAI:
    // kCastEmitLockoutMs). When a recent cast intent for the same spell
    // hasn't aged out yet, drop the new emit silently (returns false) to
    // prevent the snapshot/server-CD race from producing 4-5 redundant
    // SPELL_FAILED_NOT_READY rejections per real cooldown cycle.
    // When ai_ is null (e.g. test harness with no per-bot state) the
    // dedup is a no-op and the emit always pushes.
    bool cast(uint32 spell_id, ObjectGuid target = ObjectGuid::Empty);

    bool cast_at(uint32 spell_id, float x, float y, float z)
    { return emit(GroundTargetSpellIntent{spell_id, x, y, z}); }

    // start_attack() drops the emit silently if the same target was just
    // refused by Player::Attack within the last 30 s — see
    // BotAI::kStartAttackLockoutMs. Without this gate, idle:quest_batch:kill
    // and similar engage rules re-emit StartAttack every snapshot tick
    // against unattackable targets (phased / immune / faction-locked),
    // saturating the intent executor and stalling the world thread.
    bool start_attack(ObjectGuid target);
    bool stop_attack(bool clear_ghost_combat = false)
    { return emit(StopAttackIntent{clear_ghost_combat}); }
    bool pet_attack(ObjectGuid target)   { return emit(PetAttackIntent{target}); }
    bool pet_cast(uint32 spell_id, ObjectGuid target = ObjectGuid::Empty)
    { return emit(PetCastSpellIntent{spell_id, target}); }

    // move_to() applies a per-bot dedup against the last emitted destination
    // (see BotAI::kMoveToEmitLockoutMs). Re-emitting MoveToIntent at near-
    // identical XYZ every snapshot tick resets the MotionMaster spline and
    // causes Detour pathfinding to re-plan, producing visible stutter and
    // breaking obstacle-avoidance mid-curve. Drop the duplicate emit silently.
    // When ai_ is null (test harness) the dedup is a no-op.
    // direct=true = straight MovePoint spline, no pathfinding (committed
    // traversal-link crossings only — see MoveToIntent).
    bool move_to(float x, float y, float z, bool run = true, bool direct = false);

    bool dismiss_pet() { return emit(DismissPetIntent{}); }
    bool sit()  { return emit(SetStandStateIntent{1}); }
    bool stand(){ return emit(SetStandStateIntent{0}); }
    // clear_generators=true tears down stale MotionMaster generators (not just
    // the active spline) — see StopMovementIntent. Use it when a movement
    // target may have outlived its validity (e.g. on a cross-map teleport).
    bool stop_movement(bool clear_generators = false)
    { return emit(StopMovementIntent{clear_generators}); }
    bool jump(float forward = 7.0f) { return emit(JumpIntent{forward}); }
    bool near_teleport_to(float x, float y, float z, float o = 0.0f)
    { return emit(NearTeleportToIntent{x, y, z, o}); }
    // Cross-map teleport (loading screen). Used by A2 to fire a CROSS-MAP
    // areatrigger_teleport server-side (clientless bots can't send the CMSG),
    // e.g. a Gilneas / Exile's Reach / allied-race starter-zone exit.
    bool teleport_to(uint32 map_id, float x, float y, float z, float o = 0.0f)
    { return emit(TeleportToIntent{map_id, x, y, z, o}); }
    bool mount_appropriate() { return emit(MountIntent{0}); }
    bool dismount() { return emit(DismountIntent{}); }
    bool hearth() { return emit(HearthIntent{}); }
    bool follow(ObjectGuid leader, float distance, float angle_radians = 0.0f)
    { return emit(FollowIntent{leader, distance, angle_radians}); }

    bool say(std::string text) { return emit(ChatIntent{PartyChatIntent{std::move(text)}}); }
    bool say_world(std::string text)  { return emit(ChatIntent{SayChatIntent{std::move(text)}}); }
    bool yell_world(std::string text) { return emit(ChatIntent{YellChatIntent{std::move(text)}}); }
    bool emote_text(std::string text) { return emit(ChatIntent{EmoteChatIntent{std::move(text)}}); }
    bool whisper(std::string to, std::string text)
    { return emit(ChatIntent{WhisperIntent{std::move(to), std::move(text)}}); }

    bool group_accept()  { return emit(GroupAcceptIntent{}); }
    bool group_decline() { return emit(GroupDeclineIntent{}); }
    bool invite_to_group(ObjectGuid target)
    { return emit(InviteToGroupIntent{target}); }
    bool guild_chat(std::string text)
    { return emit(ChatIntent{GuildChatIntent{std::move(text)}}); }
    bool group_leave()   { return emit(GroupLeaveIntent{}); }
    bool group_promote_to_leader(ObjectGuid new_leader)
    { return emit(GroupPromoteToLeaderIntent{new_leader}); }
    bool group_kick_member(ObjectGuid member)
    { return emit(GroupKickMemberIntent{member}); }
    bool group_convert_to_raid()  { return emit(GroupConvertToRaidIntent{}); }
    bool group_start_ready_check(){ return emit(GroupStartReadyCheckIntent{}); }
    bool group_set_assistant(ObjectGuid member, bool assistant)
    { return emit(GroupSetAssistantIntent{member, assistant}); }
    bool reset_instances()      { return emit(ResetInstancesIntent{}); }
    bool guild_accept_invite()  { return emit(GuildAcceptInviteIntent{}); }
    bool guild_decline_invite() { return emit(GuildDeclineInviteIntent{}); }
    bool guild_leave()          { return emit(GuildLeaveIntent{}); }
    bool toggle_afk()           { return emit(ToggleAfkIntent{}); }
    bool toggle_dnd()           { return emit(ToggleDndIntent{}); }
    bool set_dungeon_difficulty(uint32 d)   { return emit(SetDungeonDifficultyIntent{d}); }
    bool set_raid_difficulty(uint32 d, bool legacy)
    { return emit(SetRaidDifficultyIntent{d, legacy}); }
    bool officer_chat(std::string text)
    { return emit(ChatIntent{OfficerChatIntent{std::move(text)}}); }
    bool raid_warning(std::string text)
    { return emit(ChatIntent{RaidWarningIntent{std::move(text)}}); }
    bool raid_chat(std::string text)
    { return emit(ChatIntent{RaidChatIntent{std::move(text)}}); }
    bool cancel_cast() { return emit(CancelCastIntent{}); }
    bool perform_emote(uint32 emote_id, ObjectGuid target = ObjectGuid::Empty)
    { return emit(PerformEmoteIntent{emote_id, target}); }
    bool face_target(ObjectGuid target)
    { return emit(FaceTargetIntent{target}); }
    bool vendor_buy_by_entry(ObjectGuid npc, uint32 item_entry, uint32 count = 1)
    { return emit(VendorIntent{VendorBuyByEntryIntent{npc, item_entry, count}}); }
    bool toggle_pvp() { return emit(TogglePvpIntent{}); }
    bool add_friend(std::string name, std::string note = {})
    { return emit(AddFriendIntent{std::move(name), std::move(note)}); }
    bool remove_friend(ObjectGuid guid) { return emit(RemoveFriendIntent{guid}); }
    bool add_ignore(std::string name) { return emit(AddIgnoreIntent{std::move(name)}); }
    bool remove_ignore(ObjectGuid guid) { return emit(RemoveIgnoreIntent{guid}); }
    bool mail_send_money(std::string recipient, uint64 copper,
                         std::string subject = "Bot remittance", std::string body = {})
    { return emit(MailIntent{MailSendMoneyIntent{std::move(recipient), copper,
                                      std::move(subject), std::move(body)}}); }
    bool mail_send_item(std::string recipient, ObjectGuid item_guid, uint32 count = 0,
                        uint64 copper = 0, uint64 cod = 0,
                        std::string subject = "Bot delivery", std::string body = {})
    { return emit(MailIntent{MailSendItemIntent{std::move(recipient), item_guid, count, copper, cod,
                                     std::move(subject), std::move(body)}}); }
    bool calendar_rsvp_all(bool accept) { return emit(CalendarRsvpAllIntent{accept}); }
    bool swap_pet_to_slot(uint32 pet_number, uint8 dst_slot)
    { return emit(HunterPetIntent{SwapPetToSlotIntent{pet_number, dst_slot}}); }
    bool delete_stabled_pet(uint32 pet_number)
    { return emit(HunterPetIntent{DeleteStabledPetIntent{pet_number}}); }
    bool summon_pet_by_number(uint32 pet_number)
    { return emit(HunterPetIntent{SummonPetByNumberIntent{pet_number}}); }
    bool feed_pet(uint32 food_item_entry)
    { return emit(HunterPetIntent{FeedPetIntent{food_item_entry}}); }
    bool abandon_pet() { return emit(HunterPetIntent{AbandonPetIntent{}}); }
    bool guild_bank_deposit_money(ObjectGuid banker, uint64 amount)
    { return emit(GuildBankDepositMoneyIntent{banker, amount}); }
    bool guild_bank_withdraw_money(ObjectGuid banker, uint64 amount)
    { return emit(GuildBankWithdrawMoneyIntent{banker, amount}); }
    bool guild_bank_deposit_item(ObjectGuid banker, uint8 tab, uint8 bank_slot,
                                 uint8 player_bag, uint8 player_slot, uint32 count = 0)
    { return emit(GuildBankDepositItemIntent{banker, tab, bank_slot, player_bag, player_slot, count}); }
    bool guild_bank_withdraw_item(ObjectGuid banker, uint8 tab, uint8 bank_slot,
                                  uint8 player_bag, uint8 player_slot, uint32 count = 0)
    { return emit(GuildBankWithdrawItemIntent{banker, tab, bank_slot, player_bag, player_slot, count}); }
    bool share_quest(uint32 quest_id) { return emit(ShareQuestIntent{quest_id}); }
    bool pet_set_react_state(uint8 state)   { return emit(HunterPetIntent{PetSetReactStateIntent{state}}); }
    bool pet_set_command_state(uint8 cmd)   { return emit(HunterPetIntent{PetSetCommandStateIntent{cmd}}); }
    bool rename_pet(std::string name)       { return emit(HunterPetIntent{RenamePetIntent{std::move(name)}}); }
    bool pet_toggle_autocast(uint32 spell, bool enabled)
    { return emit(HunterPetIntent{PetToggleAutocastIntent{spell, enabled}}); }
    bool summon_accept()  { return emit(SummonAcceptIntent{}); }
    bool summon_decline() { return emit(SummonDeclineIntent{}); }
    bool duel_decline()   { return emit(DuelDeclineIntent{}); }
    bool duel_accept()    { return emit(DuelAcceptIntent{}); }
    bool set_raid_target_icon(uint8 symbol, ObjectGuid target)
    { return emit(SetRaidTargetIconIntent{symbol, target}); }
    bool activate_spec(uint32 spec_id) { return emit(ActivateSpecIntent{spec_id}); }
    bool trade_decline()  { return emit(TradeDeclineIntent{}); }

    // NPC / world-interaction shorthands. The corresponding API methods all
    // expect the bot to be in interact range with the target — caller is
    // responsible for moving close first (use_object/quest pickup gated by
    // CanInteract checks return InvalidTarget if not).
    bool interact_with_npc(ObjectGuid npc) { return emit(InteractWithNpcIntent{npc}); }
    bool gossip_select(ObjectGuid npc, uint8 option)
    { return emit(GossipSelectIntent{npc, option}); }
    bool use_game_object(ObjectGuid go) { return emit(UseObjectIntent{go}); }

    // Cast a spell on an Item (disenchant 13262 / prospect 31252 / mill 51005).
    bool cast_on_item(uint32 spell_id, ObjectGuid item_guid)
    { return emit(CastSpellOnItemIntent{spell_id, item_guid}); }

    // Vehicle controls. enter_vehicle takes a Unit GUID of the vehicle
    // (a Creature or other Unit with a VehicleKit) and an optional seat
    // index (-1 = first free seat). cast_vehicle / cast_vehicle_at fire
    // the bot's current seat ability.
    // Apply curated talent build for the given context (0=Default,
    // 1=Raid, 2=MythicPlus, 3=PvP, 4=Leveling). Combat-locked.
    bool apply_talent_build(uint8 context)
    { return emit(ApplyTalentBuildIntent{context}); }

    // Guild charter (Phase A.2, GUILD_PLAN.md). Wrapped in GuildIntent
    // to keep the master IntentBody variant size bounded — see
    // feedback_intent_variant_capacity.md.
    bool buy_guild_charter(ObjectGuid petitioner, std::string const& guild_name)
    { return emit(GuildIntent{GuildOp::BuyCharter{petitioner, guild_name}}); }
    bool sign_guild_charter(uint64 petition_item_low)
    { return emit(GuildIntent{GuildOp::SignCharter{petition_item_low}}); }
    bool turnin_guild_charter(ObjectGuid petitioner, uint64 petition_item_low)
    { return emit(GuildIntent{GuildOp::TurnInCharter{petitioner, petition_item_low}}); }
    // Phase B: officer (emitter) recruits `target_low` into emitter's guild.
    bool recruit_to_guild(uint64 target_guid_low)
    { return emit(GuildIntent{GuildOp::RecruitTarget{target_guid_low}}); }
    // Phase E.1: officer posts trade-channel recruit message.
    bool guild_recruit_channel_post()
    { return emit(GuildIntent{GuildOp::RecruitChannelPost{}}); }

    bool enter_vehicle(ObjectGuid vehicle, int8 seat_id = -1)
    { return emit(EnterVehicleIntent{vehicle, seat_id}); }
    bool exit_vehicle() { return emit(ExitVehicleIntent{}); }
    bool cast_vehicle(uint32 spell_id, ObjectGuid target = ObjectGuid::Empty)
    { return emit(VehicleSpellIntent{spell_id, target}); }
    bool cast_vehicle_at(uint32 spell_id, float x, float y, float z)
    { return emit(VehicleGroundSpellIntent{spell_id, x, y, z}); }
    bool accept_quest(ObjectGuid giver, uint32 quest_id)
    { return emit(QuestAcceptIntent{giver, quest_id}); }
    bool complete_quest(ObjectGuid giver, uint32 quest_id, uint8 reward_choice = 0)
    { return emit(QuestCompleteIntent{giver, quest_id, reward_choice}); }
    bool abandon_quest(uint32 quest_id) { return emit(QuestAbandonIntent{quest_id}); }
    bool accept_shared_quest() { return emit(QuestSharedAcceptIntent{}); }
    bool resolve_junk_quests() { return emit(ResolveJunkQuestsIntent{}); }

    // Mail. Mailbox guid is a GameObject (mailbox) or Creature (mailbox NPC);
    // bot must already be in interact range (use_object/move_to first).
    bool mail_take_money(ObjectGuid mailbox, uint64 mail_id)
    { return emit(MailIntent{MailTakeMoneyIntent{mailbox, mail_id}}); }
    bool mail_take_item(ObjectGuid mailbox, uint64 mail_id, uint64 item_guid_low)
    { return emit(MailIntent{MailTakeItemIntent{mailbox, mail_id, item_guid_low}}); }
    bool mail_delete(ObjectGuid mailbox, uint64 mail_id)
    { return emit(MailIntent{MailDeleteIntent{mailbox, mail_id}}); }

    // Trainer. Bot must be in interact range with the trainer NPC.
    bool trainer_buy_spell(ObjectGuid trainer, uint32 spell_id)
    { return emit(TrainerBuySpellIntent{trainer, spell_id}); }
    bool trainer_buy_all(ObjectGuid trainer)
    { return emit(TrainerBuyAllIntent{trainer}); }

    // Bank. Source slot is in inventory for deposit, in bank for withdraw.
    bool bank_deposit_item(ObjectGuid banker, uint8 bag, uint8 slot)
    { return emit(BankDepositItemIntent{banker, bag, slot}); }
    bool bank_withdraw_item(ObjectGuid banker, uint8 bag, uint8 slot)
    { return emit(BankWithdrawItemIntent{banker, bag, slot}); }

    // Hearth bind. Innkeeper must be in interact range.
    bool bind_homebind(ObjectGuid innkeeper)
    { return emit(BindHomebindIntent{innkeeper}); }

    // Taxi. discover triggers the "I am here" learn-this-node packet;
    // fly_to triggers the multi-hop route activation. Bot must be in
    // interact range with the flight master NPC.
    bool discover_taxi_node(ObjectGuid flight_master)
    { return emit(DiscoverTaxiNodeIntent{flight_master}); }
    bool fly_to_node(ObjectGuid flight_master, uint32 to_node)
    { return emit(FlyToNodeIntent{flight_master, to_node}); }

    // Auction House. run_time_minutes is one of {720, 1440, 2880}.
    bool auction_sell_item(ObjectGuid auctioneer, ObjectGuid item_guid,
                           uint64 min_bid, uint64 buyout, uint32 run_time_minutes = 1440)
    { return emit(AuctionIntent{AuctionSellItemIntent{auctioneer, item_guid, min_bid, buyout, run_time_minutes}}); }
    bool auction_cancel(ObjectGuid auctioneer, uint32 auction_id)
    { return emit(AuctionIntent{AuctionCancelIntent{auctioneer, auction_id}}); }
    bool auction_cancel_all(ObjectGuid auctioneer)
    { return emit(AuctionIntent{AuctionCancelAllIntent{auctioneer}}); }

    // Auction House BUY-side (#4B). Wrapped in EconomyIntent. Both apply a
    // per-auction_id dedup (BotAI::ActionKind::AhBuyout / AhBid, 30s) so an
    // economy rule doesn't re-emit the same buyout/bid every snapshot tick
    // before the executor settles it — that would double-spend gold and
    // race the on-demand AH snapshot rebuild. Definitions live in
    // BotIntentEmitter.cpp (need BotAI for the dedup, like cast/move_to).
    // `price` / `bid` are copper; the server-side API re-validates against
    // the live auction (still exists, not own, enough gold, silver-aligned).
    bool ah_buyout(ObjectGuid auctioneer, uint32 auction_id, uint64 price);
    bool ah_bid(ObjectGuid auctioneer, uint32 auction_id, uint64 bid);
    // Commodity buy (stackable trade-good reagents). Dedup is per ITEM_ENTRY
    // (BotAI::ActionKind::AhBuyCommodity, 30s) — commodities aggregate many
    // listings into one bucket, so the rule wants one buy per reagent per
    // visit, not one per underlying auction. `max_total` is the slippage-
    // guarded ceiling (unit_price*qty + margin); the server-side API re-quotes
    // the live bucket and refuses if the total exceeds it or the bot can't pay.
    bool ah_buy_commodity(ObjectGuid auctioneer, uint32 item_entry,
                          uint32 quantity, uint64 max_total);
    // #4B-2(a): fulfil a claimed craft order — craft the product, mail it to
    // the requester, release the escrow. Wrapped in EconomyIntent. The crafter
    // must already OWN the order (status Claimed via CraftOrderBoard); the
    // claimed order's fields come from BotSnapshot::craft_orders.claimed_*.
    bool craft_fulfill(uint64 order_id, uint32 spell_id, uint32 item_entry,
                       uint32 qty, uint64 requester_low)
    { return emit(EconomyIntent{EconomyOp::CraftFulfill{order_id, spell_id, item_entry, qty, requester_low}}); }
    // #4B-2(a) part 2: post a craft order (escrow debited world-thread by the
    // executor via CraftOrderBoard::PostOrder) / claim the oldest known-recipe
    // open order (world-thread ClaimOpenOrder). Both run server-side so the
    // escrow + spellbook + fleet-bot firewall checks stay on the world thread.
    bool craft_post(uint32 spell_id, uint32 item_entry, uint32 quantity, uint64 payment)
    { return emit(EconomyIntent{EconomyOp::CraftPost{spell_id, item_entry, quantity, payment}}); }
    bool craft_claim()
    { return emit(EconomyIntent{EconomyOp::CraftClaim{}}); }

    // Battleground / arena queue. `arena_type` 0 = battleground (solo or
    // group-leader), 2/3/5 = arena skirmish bracket (group-leader only).
    bool bg_queue(ObjectGuid battlemaster, uint16 bg_type_id, uint8 arena_type = 0)
    { return emit(QueueIntent{BgQueueIntent{battlemaster, bg_type_id, arena_type}}); }
    bool bg_leave() { return emit(QueueIntent{BgLeaveIntent{}}); }
    bool bg_port(uint16 bg_type_id, bool accept = true)
    { return emit(QueueIntent{BgPortIntent{bg_type_id, accept}}); }

    // Talents — apply Blizzard's curated starter build for the bot's spec.
    bool apply_starter_talents() { return emit(ApplyStarterTalentsIntent{}); }

    // Loot roll. vote_type: 0=Pass, 1=Need, 2=Greed, 3=Disenchant.
    bool loot_roll(ObjectGuid loot_object, uint8 list_id, uint8 vote_type)
    { return emit(LootRollIntent{loot_object, list_id, vote_type}); }

    // LFG / vendor / inventory shorthands
    bool lfg_queue(uint32 dungeon_id, Role role)
    { return emit(QueueIntent{LfgQueueIntent{dungeon_id, role}}); }
    bool lfg_unqueue() { return emit(QueueIntent{LfgUnqueueIntent{}}); }
    bool lfg_proposal_respond(uint32 proposal_id, bool accept = true)
    { return emit(QueueIntent{LfgProposalRespondIntent{proposal_id, accept}}); }
    bool lfg_role_check(uint8 roles)
    { return emit(QueueIntent{LfgRoleCheckIntent{roles}}); }
    bool vendor_buy(ObjectGuid vendor, uint8 vendor_slot, uint8 count = 1)
    { return emit(VendorIntent{VendorBuyIntent{vendor, vendor_slot, count}}); }
    bool vendor_sell(ObjectGuid vendor, uint8 bag, uint8 slot, uint8 count = 0)
    { return emit(VendorIntent{VendorSellIntent{vendor, bag, slot, count}}); }
    bool vendor_sell_trash(ObjectGuid vendor)
    { return emit(VendorIntent{VendorSellTrashIntent{vendor}}); }
    bool repair_all(ObjectGuid vendor, bool from_guild_bank = false)
    { return emit(VendorIntent{RepairAllIntent{vendor, from_guild_bank}}); }
    bool vendor_buy_category(ObjectGuid vendor, uint8 item_class, uint8 item_subclass, uint8 total_count)
    { return emit(VendorIntent{VendorBuyByCategoryIntent{vendor, item_class, item_subclass, total_count}}); }
    bool equip_item(uint8 from_bag, uint8 from_slot, uint8 to_slot)
    { return emit(EquipItemIntent{from_bag, from_slot, to_slot}); }

private:
    bool push(Intent i);

    IntentQueue* queue_;
    BotId        bot_id_;
    SnapshotVer  source_;
    IntentId*    next_id_;
    BotAI*       ai_ = nullptr;     // optional; null = no per-bot dedup
    uint32       emitted_count_ = 0;   // intents actually pushed (see emitted_count())
};

} // namespace Playerbot
