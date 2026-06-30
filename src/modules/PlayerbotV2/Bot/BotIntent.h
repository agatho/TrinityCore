// BotIntent - Typed messages from AI workers to world thread.
// First iteration carries the load-bearing intent variants; the full set per
// CONTRACTS.md §2.3 lands as subsystems implement the corresponding paths.

#pragma once

#include "BotTypes.h"
#include "ObjectGuid.h"
#include <string>
#include <variant>
#include <vector>

namespace Playerbot {

// ---- Combat ----
struct CastSpellIntent       { uint32 spell_id; ObjectGuid target; };
// Cast a spell on an Item (disenchant 13262 / prospect 31252 / mill 51005).
// item_guid is the bag-resident item GUID.
struct CastSpellOnItemIntent { uint32 spell_id; ObjectGuid item_guid; };
struct GroundTargetSpellIntent { uint32 spell_id; float x, y, z; };
struct CancelCastIntent      {};
// Cancel a specific aura on self by spell id (RemoveAurasDueToSpell).
// Owner-driven cleanup — e.g. paladin /cancelaura 1044 (Hand of Freedom)
// when no longer needed. Self-only; for other-target dispel use /dispel.
struct CancelAuraIntent      { uint32 spell_id; };
struct StartAttackIntent     { ObjectGuid target; };
// clear_ghost_combat: additionally drop PvE combat REFS (Unit::CombatStop)
// when the bot has no attackers — self-heal for the stuck-combat-flag wedge
// (2026-06-12 Stockades: healer InCombat=true with no victim/attackers for
// 30+ min; tank_advance's member-in-combat gate held the whole run hostage).
// The API only clears when the attacker list is empty, so a bot in a REAL
// fight can't accidentally combat-drop.
struct StopAttackIntent      { bool clear_ghost_combat = false; };
struct PetAttackIntent       { ObjectGuid target; };
// Issue a pet ability — Felhunter Spell Lock (Warlock interrupt), Hunter
// pet abilities, DK pet ghoul commands. The spell must belong to the bot's
// active pet's spellbook; the API resolves the pet and casts as charm-master
// (works for warlocks/hunters/DKs alike).
struct PetCastSpellIntent    { uint32 spell_id; ObjectGuid target; };
// Dismiss the bot's current pet — useful before flight paths (the pet
// can't follow), or for warlocks switching to a different demon. The API
// targets the active pet via Player::GetPet() and dispatches the standard
// Pet::Remove(PET_SAVE_DISMISS) path; warlock-style permanent demons get
// PET_SAVE_NOT_IN_SLOT so they're recallable.
struct DismissPetIntent      {};

// ---- Public chat ----
struct SayChatIntent         { std::string text; };
struct YellChatIntent        { std::string text; };
struct EmoteChatIntent       { std::string text; };
struct GuildChatIntent       { std::string text; };
struct RaidChatIntent        { std::string text; };

// ---- Unstuck ----
// Near-teleport the bot a few yards in their facing direction. Owner
// emergency unstuck for terrain glitches (stuck on a rock, jammed against
// a doorframe). Server-side via Player::NearTeleportTo. Distance is a
// hint — server clamps to navigable terrain.
struct UnstuckIntent         { float distance; };
// Direct near-teleport to a precise (x,y,z,o). Used by the auto-unstick
// rule (Tier 2 escalation) when the bot has been wedged on geometry for
// 15+ seconds. Stays on the bot's current map; the server clamps Z to
// navmesh ground at the destination.
struct NearTeleportToIntent  { float x, y, z, o; };

// ---- Stand-state (sit / stand / sleep / kneel) ----
// stand_state matches UnitStandStateType: 0=stand, 1=sit, 2=chair (low/med/high
// for inn chairs), 5=sleep, 6=kneel, 7=cower, 8=submerged. We only expose the
// owner-useful ones via whisper (sit/stand) but the intent carries the raw
// byte so future rules (auto-sit on inn furniture etc.) can use the rest.
struct SetStandStateIntent   { uint8 stand_state; };

// ---- Movement ----
struct MoveToIntent          { float x, y, z; bool run; };
struct TeleportToIntent      { uint32 map_id; float x, y, z, o; };
// clear_generators=false (default) only halts the active spline (Player::
// StopMoving). clear_generators=true pops EVERY MotionMaster generator back to
// Idle (Clear + MoveIdle). The distinction matters: a POINT/CHASE/FOLLOW
// generator left in place re-issues a fresh spline toward its STORED target on
// the next Update, so merely stopping the spline lets a stale destination keep
// driving the bot. Clearing the generator is the only way to drop a movement
// target that survived a cross-map teleport (e.g. a move computed on the prior
// map that resumes against meaningless coordinates on the new map).
struct StopMovementIntent    { bool clear_generators = false; };
struct JumpIntent            { float forward; };
struct MountIntent           { uint32 mount_id; };  // 0 = best for context
struct DismountIntent        {};
struct HearthIntent          {};
// Follow with optional formation offset. `angle_radians` is the
// follow-side relative to the leader's facing (0 = directly behind,
// pi/2 = right flank, etc). 0 distance + 0 angle = legacy "behind at
// default spacing" behaviour. Formation rules in State_Idle compute
// (slot, type) → (distance, angle) so multiple bots take distinct
// flank positions. Used by every /follow, /come, /formation path.
struct FollowIntent          { ObjectGuid leader; float distance; float angle_radians; };

// ---- Items / loot ----
struct UseItemIntent         { uint8 bag, slot; ObjectGuid target; };
struct UseItemByEntryIntent  { uint32 item_entry; ObjectGuid target; };
struct EquipItemIntent       { uint8 from_bag, from_slot; uint8 to_slot; };
struct LootIntent            { ObjectGuid corpse_or_object; };
struct ReleaseCorpseIntent   {};
struct ReviveAtCorpseIntent  {};
// Corpse-run completion: bot is ghost and has walked back into reclaim
// range. Calls API::reclaim_corpse → ResurrectPlayer + SpawnCorpseBones,
// no sickness.
struct ReclaimCorpseIntent   {};
// Spirit-healer rez: instant alive at graveyard with sickness + 25%
// durability hit. Calls API::spirit_resurrect.
struct SpiritResurrectIntent {};
// Accept a pending rez popup (Resurrection / Rebirth / Soulstone). The
// caster has already cast on our corpse — this intent finalizes acceptance.
struct AcceptRezIntent       {};

// ---- Vendor ----
struct VendorBuyIntent       { ObjectGuid npc; uint8 vendor_slot; uint8 count; };
struct VendorSellIntent      { ObjectGuid npc; uint8 bag, slot; uint8 count; };
struct VendorSellTrashIntent { ObjectGuid npc; };
struct RepairAllIntent       { ObjectGuid npc; bool from_guild_bank; };
// Buy first vendor item matching item_class/item_subclass that the bot meets
// the level requirement for. Used by auto-restock rules where the AI doesn't
// know the vendor's exact slot layout. `total_count` is the desired quantity;
// the API stops once that many units have been purchased (multiple buy_slot
// calls if the vendor sells the item in stacks <total_count).
struct VendorBuyByCategoryIntent { ObjectGuid npc; uint8 item_class, item_subclass; uint8 total_count; };

// ---- Quest ----
struct QuestAcceptIntent     { ObjectGuid npc; uint32 quest_id; };
// `reward_choice` selects from the quest's choice-item list (0 = first).
// Pass 0xFF to let PlayerbotAPI auto-pick (ScoreQuestReward weighs equippable
// upgrades > vendor value). Quests with only fixed rewards ignore the field.
struct QuestCompleteIntent   { ObjectGuid npc; uint32 quest_id; uint8 reward_choice; };
struct QuestAbandonIntent    { uint32 quest_id; };
// Accept a quest just shared by a group member. The receiver-side popup is
// driven by `Player::SetQuestSharingInfo`; the API uses GetSharedQuestID for
// the actual accept. No fields needed on the intent — the bot side already
// knows what's pending via the snapshot.
struct QuestSharedAcceptIntent {};
// Run JunkQuestResolver on the bot: force-complete auto-granted feature quests
// (55660 "Time Trials" etc. — abandon doesn't stick, they re-push at login) and
// resolve profession-spec choice quests. No fields — the resolver processes the
// whole log on the world thread. Emitted by idle:resolve_junk_quests.
struct ResolveJunkQuestsIntent {};

// ---- Loot rolls ----
// Cast vote on a group/master loot roll. vote_type: 0=Pass, 1=Need,
// 2=Greed, 3=Disenchant. Out-of-range votes fold to Pass server-side.
struct LootRollIntent { ObjectGuid loot_object; uint8 loot_list_id; uint8 vote_type; };

// ---- Talents ----
// Apply Blizzard's curated starter build to the bot's active combat
// trait config. Drives `apply_starter_talents` via the executor. Used
// by the auto-init layer on first login + by the `talents` whisper.
struct ApplyStarterTalentsIntent {};
// Apply curated per-spec talent build for the given context.
//   0 = Default, 1 = Raid, 2 = MythicPlus, 3 = PvP, 4 = Leveling.
struct ApplyTalentBuildIntent { uint8 context; };

// ---- Battleground ----
// Solo or group battleground/arena queue. `bg_type_id` is the
// BattlemasterList.dbc id — e.g. WSG=2, AB=3, AV=1, RB=32 (random) for
// battlegrounds; 4=Nagrand, 6=AllArenas, 8=RuinsOfLordaeron for arenas.
// `arena_type` selects the queue category: 0 = battleground (normal BG
// queue), 2/3/5 = arena skirmish bracket (2v2/3v3/5v5). When non-zero
// the API routes through the Arena queue id and requires the bot to be
// the leader of an arena_type-sized group. Defaulting it to 0 keeps the
// existing two-field aggregate inits (`BgQueueIntent{guid, bml}`) valid.
struct BgQueueIntent  { ObjectGuid battlemaster; uint16 bg_type_id; uint8 arena_type = 0; };
struct BgLeaveIntent  {};
// Port into (or decline) a pending BG invite. Bot side surfaces the queue
// type id via snapshot.bg_queues[i].bg_type_id; the API resolves the queue
// slot and dispatches to the same logic as the player-side BattlefieldPort.
struct BgPortIntent   { uint16 bg_type_id; bool accept; };

// ---- Auction House ----
// Post a single non-stackable item up for auction. `min_bid` and `buyout`
// are in copper but must be silver-aligned (% 100 == 0). `run_time_minutes`
// must be one of {720, 1440, 2880} for the 12h/24h/48h auction durations.
struct AuctionSellItemIntent { ObjectGuid auctioneer; ObjectGuid item_guid;
                               uint64 min_bid; uint64 buyout; uint32 run_time_minutes; };
// Cancel one of the bot's own auctions. `auction_id` comes from the bot's
// own owned-auctions list (snapshot does not surface this yet — driven by
// whisper command for now).
struct AuctionCancelIntent   { ObjectGuid auctioneer; uint32 auction_id; };
// Bulk-cancel every owned auction in `auctioneer`'s faction house. Cheaper
// for the AI than enumerating ids and emitting per-cancel intents — driven
// from the `/cancelall` whisper for owners who want to wipe their listings.
struct AuctionCancelAllIntent { ObjectGuid auctioneer; };

// ---- Taxi ----
// Discover the flight master's local node (auto-fires on first arrival in
// most cases, but lets bot logic explicitly trigger). Activate flies a
// known route to `to_node` (a TaxiNodes.dbc id).
struct DiscoverTaxiNodeIntent { ObjectGuid flight_master; };
struct FlyToNodeIntent        { ObjectGuid flight_master; uint32 to_node; };

// ---- Hearth bind ----
// Reset the bot's hearthstone home to the innkeeper's location.
struct BindHomebindIntent { ObjectGuid innkeeper; };

// ---- Bank ----
// Move one item between inventory and bank. The bot must already be in
// interact range with the banker. `from_bag/from_slot` is the source —
// inventory side for deposit, bank side for withdraw. The API auto-stores
// to the matching destination via NULL_BAG/NULL_SLOT.
struct BankDepositItemIntent  { ObjectGuid banker; uint8 bag, slot; };
struct BankWithdrawItemIntent { ObjectGuid banker; uint8 bag, slot; };

// ---- Trainer ----
// Learn one spell from `trainer_npc`. The bot picks `spell_id` from a known
// trainer-spell list (passed in via config / level-up trigger); execution
// resolves the trainer template via the creature entry.
struct TrainerBuySpellIntent { ObjectGuid trainer_npc; uint32 spell_id; };
// Buy every spell on the trainer the bot is currently eligible for —
// drives the "train" whisper command without per-spell scripting.
struct TrainerBuyAllIntent   { ObjectGuid trainer_npc; };

// ---- Mail ----
// All three carry the mailbox guid (GO or NPC) so the API can re-validate
// interaction range at execution time. The bot picks `mail_id` and
// `item_guid_low` from BotSnapshot::mail (populated by the world thread).
struct MailTakeMoneyIntent   { ObjectGuid mailbox; uint64 mail_id; };
struct MailTakeItemIntent    { ObjectGuid mailbox; uint64 mail_id; uint64 item_guid_low; };
struct MailDeleteIntent      { ObjectGuid mailbox; uint64 mail_id; };

// ---- Group / social ----
struct GroupAcceptIntent     {};
struct GroupDeclineIntent    {};
struct GroupLeaveIntent      {};
struct GroupReadyResponseIntent { bool ready; };
struct GroupPromoteToLeaderIntent { ObjectGuid new_leader_guid; };
// Kick a member from the bot's group (party uninvite). Bot must be leader.
struct GroupKickMemberIntent { ObjectGuid member_guid; };
// Convert party to raid (Group::ConvertToRaid). Leader-only, idempotent.
struct GroupConvertToRaidIntent {};
// Initiate a ready check on the bot's group. Leader/raid-assistant only.
struct GroupStartReadyCheckIntent {};
// Toggle MEMBER_FLAG_ASSISTANT on a raid member. Leader-only, raid-only.
struct GroupSetAssistantIntent { ObjectGuid member_guid; bool assistant; };
// Reset all (non-locked) instance binds. Bot must be group leader (when grouped).
struct ResetInstancesIntent {};
// Guild membership ops (mirror HandleGuildAccept/Decline/Leave handlers).
struct GuildAcceptInviteIntent  {};
struct GuildDeclineInviteIntent {};
struct GuildLeaveIntent         {};
struct OfficerChatIntent { std::string text; };
struct RaidWarningIntent { std::string text; };
struct PerformEmoteIntent { uint32 emote_id; ObjectGuid target; };
struct FaceTargetIntent { ObjectGuid target; };
struct VendorBuyByEntryIntent { ObjectGuid npc; uint32 item_entry; uint32 count; };
struct TogglePvpIntent {};
struct AddFriendIntent    { std::string name; std::string note; };
struct MailSendMoneyIntent { std::string recipient; uint64 copper; std::string subject; std::string body; };
// Mail an item attachment. count=0 means whole stack; copper rides along on
// the same mail (zero is fine); cod=0 means no COD. Postage 30c flat.
struct MailSendItemIntent { std::string recipient; ObjectGuid item_guid; uint32 count;
                            uint64 copper; uint64 cod;
                            std::string subject; std::string body; };
struct CalendarRsvpAllIntent { bool accept; };
// ---- Hunter pet stable management ----
// Move a stabled or active pet (by petNumber) to a new slot. Active slots
// are 0..MAX_ACTIVE_PETS-1; stable slots are 5..(5+MAX_PET_STABLES-1).
// Active<->stable swaps despawn the active pet first.
struct SwapPetToSlotIntent  { uint32 pet_number; uint8 dst_slot; };
// Permanently delete a pet from the stable. The currently summoned pet
// cannot be deleted; caller must dismiss first.
struct DeleteStabledPetIntent { uint32 pet_number; };
// Summon a pet that is in an active slot. Use SwapPetToSlotIntent first
// to bring a stabled pet into an active slot if needed.
struct SummonPetByNumberIntent { uint32 pet_number; };
// Cast Feed Pet (6991) on the active hunter pet using the named food
// item from inventory. Pet diet + level checks happen API-side.
struct FeedPetIntent { uint32 food_item_entry; };
// Permanently abandon the active hunter pet (Player::RemovePet with
// PET_SAVE_AS_DELETED). Refused mid-combat.
struct AbandonPetIntent {};
// ---- Guild bank ----
// All four require the bot is in a guild and `banker` is a Guild Vault
// (GAMEOBJECT_TYPE_GUILD_BANK) in interact range.
struct GuildBankDepositMoneyIntent  { ObjectGuid banker; uint64 amount; };
struct GuildBankWithdrawMoneyIntent { ObjectGuid banker; uint64 amount; };
struct GuildBankDepositItemIntent   { ObjectGuid banker; uint8 tab; uint8 bank_slot;
                                      uint8 player_bag; uint8 player_slot; uint32 count; };
struct GuildBankWithdrawItemIntent  { ObjectGuid banker; uint8 tab; uint8 bank_slot;
                                      uint8 player_bag; uint8 player_slot; uint32 count; };
// Push the bot's active quest to the party, popping the share dialog on
// each eligible receiver. Mirrors HandlePushQuestToParty.
struct ShareQuestIntent { uint32 quest_id; };
// Pet stance/command. Mirrors ReactStates / CommandStates respectively.
struct PetSetReactStateIntent  { uint8 state; };
struct PetSetCommandStateIntent { uint8 command; };
struct RenamePetIntent { std::string new_name; };
struct PetToggleAutocastIntent { uint32 spell_id; bool enabled; };
struct RemoveFriendIntent { ObjectGuid friend_guid; };
struct AddIgnoreIntent    { std::string name; };
struct RemoveIgnoreIntent { ObjectGuid ignore_guid; };
// Toggle the AFK / DND chat-state flag (Player::ToggleAFK / ToggleDND).
struct ToggleAfkIntent {};
struct ToggleDndIntent {};
// Change the bot's dungeon difficulty (mirrors HandleSetDungeonDifficultyOpcode).
struct SetDungeonDifficultyIntent { uint32 difficulty_id; };
// Change raid difficulty; `legacy` selects the legacy slot.
struct SetRaidDifficultyIntent    { uint32 difficulty_id; bool legacy; };
// Player-summon dialog response: warlock summon ritual, meeting stone summon,
// LFG summon. Fires after the snapshot reports has_summon_pending. Auto-accept
// in State_Idle/InGroup; decline is currently only used by whisper command.
struct SummonAcceptIntent    {};
struct SummonDeclineIntent   {};
// Decline a pending duel request. Strangers always get declined; the
// friend-aware dispatch rule sends DuelAcceptIntent for trusted initiators
// (group / guild / social-friend), which keeps owner sparring usable.
struct DuelDeclineIntent     {};
struct DuelAcceptIntent      {};
// Update a raid target marker (skull/cross/etc) on `target`. `symbol` is
// 0..7 mirroring RaidTargetIcon. Empty target clears that symbol. Used by
// the auto-skull rule (in-combat group leader marks lowest-HP enemy) and
// by the upcoming /mark whisper command.
struct SetRaidTargetIconIntent { uint8 symbol; ObjectGuid target; };
// Specialization swap. spec_id is ChrSpecialization.db2 id. Combat-gated
// by API; rejected outright when the bot is fighting (Locked).
struct ActivateSpecIntent { uint32 spec_id; };
// Diagnostic: clears all spell cooldowns. Owner-driven via /cdreset; no
// auto-firing rule. Helps iterate on combat tuning.
struct ResetCooldownsIntent {};
// Decline an open trade request. Mirrors HandleCancelTradeOpcode — closes
// the trade window without committing items. Always-on auto-decline; bots
// don't accept arbitrary trades (item-theft vector).
struct TradeDeclineIntent    {};
// Send a group invite to the named player. Mirrors HandlePartyInviteOpcode
// — same gating around faction/instance/level/social ignore. Used by the
// idle:invite_to_group rule so solo bots gather pickup parties at dungeon
// hubs / grindspots, and by the /invite whisper command.
struct InviteToGroupIntent   { ObjectGuid target; };
struct WhisperIntent         { std::string target; std::string text; };
struct PartyChatIntent       { std::string text; };

// ---- LFG ----
struct LfgQueueIntent        { uint32 dungeon_or_bg_id; Role role; };
struct LfgUnqueueIntent      {};
// Respond to the LFG dungeon-ready proposal popup. accept=true ports into
// the dungeon; false drops back into queue (or disbands the proposal).
struct LfgProposalRespondIntent { uint32 proposal_id; bool accept; };
// Respond to the group's LFG role-check with this bot's desired role bitmask
// (PLAYER_ROLE_TANK=2, HEALER=4, DAMAGE=8, LEADER=1). Auto-fired from
// State_InGroup when role_check_pending flips true.
struct LfgRoleCheckIntent { uint8 roles; };

// ---- World interaction ----
struct UseObjectIntent       { ObjectGuid object; };
struct InteractWithNpcIntent { ObjectGuid npc; };
struct GossipSelectIntent    { ObjectGuid npc; uint8 option; };

// ---- Vehicles (BG siege engines, dragons, sorters etc.) ----
// EnterVehicle: target_guid is the vehicle Unit; seat_id == -1 picks the
// first free seat. Mirrors Unit::EnterVehicle which casts the hardcoded
// VEHICLE_SPELL_RIDE spell, so the vehicle's seat config validates.
struct EnterVehicleIntent     { ObjectGuid vehicle; int8 seat_id; };
struct ExitVehicleIntent      {};
// Cast the bot's vehicle's spell while seated (e.g., demolisher boulder
// hurl). Mirrors UnitAction's "use seat ability". target may be empty
// for ground-targeted ability that uses the AT spell flow instead.
struct VehicleSpellIntent     { uint32 spell_id; ObjectGuid target; };
struct VehicleGroundSpellIntent { uint32 spell_id; float x, y, z; };

// ---- Guild subsystem intent wrapper ----
//
// All guild-related intents (Phase A.2 charter flow + future Phase B
// recruitment + Phase C chat + Phase D events + Phase E rivalry)
// nest inside ONE `GuildIntent` alternative in `IntentBody` rather
// than adding individual top-level types. This protects the master
// variant from MSVC heap-exhaustion: every Combat spec rotation .cpp
// instantiates the full visitor machinery for `IntentBody`, and at
// ~120 alternatives we're at the breaking edge. Sub-variant nesting
// keeps the master variant size constant as the guild subsystem
// grows. See feedback_intent_variant_capacity.md.
//
// Charter sub-types mirror WorldSession petition handlers (non-packet
// path through Fleet/BotGuildCharter.cpp helpers).
namespace GuildOp {
    // Buy a Guild Charter (item entry 5863) from a PETITIONER NPC.
    struct BuyCharter    { ObjectGuid petitioner_npc; std::string guild_name; };
    // Sign a petition the bot is near. `petition_item_low` is the
    // founder's charter item guid_low; signer is the emitter.
    struct SignCharter   { uint64 petition_item_low; };
    // Turn in a fully-signed petition at the petitioner NPC.
    struct TurnInCharter { ObjectGuid petitioner_npc; uint64 petition_item_low; };
    // Phase B: officer (recruiter = emitter) directly adds `target_low`
    // to the recruiter's guild. Bypasses player-style invite popup —
    // both sides are bots so there's no acceptance UI to navigate.
    struct RecruitTarget { uint64 target_guid_low; };

    // Phase E.1: officer (emitter) posts a recruit message to their
    // current zone's Trade channel. Executor resolves the channel,
    // composes the message ("<Guild Name> recruiting all levels,
    // /w <officer> for invite"), and emits via Channel::Say. No
    // payload — all data resolved server-side at execution.
    struct RecruitChannelPost {};
}

struct GuildIntent
{
    // Append new alternatives here as Phase B/C/D/E land — none of
    // them widen `IntentBody`.
    std::variant<
        GuildOp::BuyCharter,
        GuildOp::SignCharter,
        GuildOp::TurnInCharter,
        GuildOp::RecruitTarget,
        GuildOp::RecruitChannelPost
    > op;
};

// ---- Economy subsystem intent wrapper (#4B) ----
//
// Buy-side auction + (future #4B-2) craft-order economy ops nest inside ONE
// `EconomyIntent` alternative in `IntentBody`, mirroring `GuildIntent` /
// `AuctionIntent`. Today bots only SELL (post) on the AH; the buy path
// (AhBuyout / AhBid) closes the supply->demand->gold-sink loop. Wrapping
// keeps the master variant size constant as the economy subsystem grows
// (craft-orders, vendor-arbitrage, etc) — see
// feedback_intent_variant_capacity.md.
//
// All ops are EMITTED by economy rules; the executor calls the matching
// server-side PlayerbotAPI method which re-validates (auctioneer in range,
// auction still exists, not own auction, enough gold) at execution time.
namespace EconomyOp {
    // Buy out an existing auction outright at its full buyout price. The
    // bot picks `auction_id` + `auctioneer` + `price` from
    // BotSnapshot::auction.buyable_listings (populated when the bot is at
    // an auctioneer). `price` is the expected buyout (copper) carried for
    // the executor's affordability pre-check; the API re-reads the live
    // auction's BuyoutOrUnitPrice and rejects on mismatch/insufficient gold.
    struct AhBuyout { ObjectGuid auctioneer; uint32 auction_id; uint64 price; };
    // Place a bid on an existing (non-commodity) auction. `bid` is the
    // copper amount to bid; must be silver-aligned and >= the auction's
    // current min-increment. The API validates against the live auction.
    struct AhBid    { ObjectGuid auctioneer; uint32 auction_id; uint64 bid; };
    // Buy a quantity of a COMMODITY (stackable trade good / craft reagent).
    // Modern stackable goods are bought via the bucket-aggregated commodity
    // path (GetCommodityQuote -> BuyCommodity), NOT the single-auction
    // AhBuyout/AhBid above — auction_buyout rejects commodities. The bot
    // picks `item_entry` + `quantity` from BotSnapshot::auction
    // .buyable_commodities; `max_total_price` is the rule's slippage-guarded
    // ceiling (unit_price*qty + margin). The API creates a quote, refuses if
    // the live total exceeds max_total_price or the bot can't afford it, then
    // commits the purchase (items mailed to the bot).
    struct AhBuyCommodity { ObjectGuid auctioneer; uint32 item_entry;
                            uint32 quantity; uint64 max_total_price; };
    // #4B-2(a): a crafter bot FULFILS a claimed craft order. The bot has
    // already CLAIMED `order_id` via CraftOrderBoard (world-thread), and the
    // claimed order's fields ride along in the snapshot
    // (BotSnapshot::craft_orders.claimed_*). The executor calls
    // PlayerbotAPI::craft_fulfill_order which casts the craft spell(s) to
    // produce `qty` of `item_entry`, mails the product to `requester_low`, and
    // on success calls CraftOrderBoard::MarkDelivered(order_id) to RELEASE the
    // escrow to the crafter. `spell_id`/`item_entry`/`qty`/`requester_low` are
    // carried (not just order_id) so the API doesn't have to reach back into
    // the board for them — the board remains the escrow authority, while the
    // craft mechanics are pure server-side validation. Posting an order is NOT
    // an intent: it's a direct world-thread CraftOrderBoard::PostOrder call
    // from the post rule's fire, to keep the escrow debit atomic with the row
    // write (see CraftOrderBoard.h).
    struct CraftFulfill { uint64 order_id; uint32 spell_id; uint32 item_entry;
                          uint32 qty; uint64 requester_low; };
    // #4B-2(a) part 2: a requester bot POSTS a craft order for a crafted
    // intermediate it needs but cannot make itself. The executor runs on the
    // WORLD THREAD (where Player gold + the board live), so the post rule emits
    // this op instead of touching CraftOrderBoard from the worker thread — the
    // executor calls CraftOrderBoard::PostOrder, which debits the escrow atomically
    // with the row write and re-verifies the requester is a fleet bot
    // (human-firewall). `spell_id` is the PRODUCING recipe (the order's recipe key
    // — only a bot that KNOWS it can claim), `item_entry` the product, `quantity`
    // the shortfall, `payment` the market-derived fair payment escrowed up front.
    // Fields are sourced from BotSnapshot::craft_orders.want_*.
    struct CraftPost { uint32 spell_id; uint32 item_entry; uint32 quantity;
                       uint64 payment; };
    // #4B-2(a) part 2: a crafter bot CLAIMS the oldest Open order whose recipe it
    // knows. Like CraftPost this MUST run on the world thread (ClaimOpenOrder
    // re-verifies the crafter's live spellbook + fleet-bot status and flips the
    // row to Claimed), so the claim rule emits this op rather than calling the
    // board directly. No payload — the board picks the oldest claimable order for
    // the emitting bot. The claimed order then surfaces in the next snapshot's
    // craft_orders.claimed_* fields, which the fulfil rule turns into CraftFulfill.
    struct CraftClaim { };
}

struct EconomyIntent
{
    // Append new alternatives here as #4B-2 craft-orders / further economy
    // ops land — none of them widen `IntentBody`.
    std::variant<
        EconomyOp::AhBuyout,
        EconomyOp::AhBid,
        EconomyOp::AhBuyCommodity,
        EconomyOp::CraftFulfill,
        EconomyOp::CraftPost,
        EconomyOp::CraftClaim
    > op;
};

// ---- Housing (12.0+) ----
struct JoinNeighborhoodIntent  { uint32 neighborhood_id; };
struct PlotPurchaseIntent      { uint32 neighborhood_id; uint32 plot_id; };
struct PlaceDecorationIntent   { uint32 plot_id; uint32 deco_entry; bool exterior;
                                 float x, y, z, rot; };
struct VisitHouseIntent        { uint32 plot_id; };

// ---- Subsystem-wrapper intents (variant-pressure relief) ----
//
// Each wrapper carries `variant<...>` of all its subsystem's sub-intents.
// Inner sub-intent structs keep their original names so direct callers
// only need to wrap one extra layer (`ChatIntent{SayChatIntent{...}}`).
// The shorthand emitter helpers wrap automatically. See
// feedback_intent_variant_capacity.md — MSVC's variant visitor instantiation
// hits a heap-exhaustion wall around ~120 IntentBody alternatives; bundling
// related ops into wrappers keeps the master variant small.
struct ChatIntent
{
    std::variant<
        SayChatIntent, YellChatIntent, EmoteChatIntent,
        GuildChatIntent, RaidChatIntent, OfficerChatIntent,
        RaidWarningIntent, WhisperIntent, PartyChatIntent
    > op;
};
struct MailIntent
{
    std::variant<
        MailTakeMoneyIntent, MailTakeItemIntent, MailDeleteIntent,
        MailSendMoneyIntent, MailSendItemIntent
    > op;
};
struct AuctionIntent
{
    std::variant<
        AuctionSellItemIntent, AuctionCancelIntent, AuctionCancelAllIntent
    > op;
};
struct VendorIntent
{
    std::variant<
        VendorBuyIntent, VendorSellIntent, VendorSellTrashIntent,
        RepairAllIntent, VendorBuyByCategoryIntent, VendorBuyByEntryIntent
    > op;
};
// Hunter pet stable management — keeps `PetAttackIntent` / `PetCastSpellIntent`
// / `DismissPetIntent` at top level (those are combat-tier, emitted from
// per-spec rotation files; wrapping them would force every Combat .cpp to
// include this wrapper).
struct HunterPetIntent
{
    std::variant<
        SwapPetToSlotIntent, DeleteStabledPetIntent, SummonPetByNumberIntent,
        FeedPetIntent, AbandonPetIntent, PetSetReactStateIntent,
        PetSetCommandStateIntent, RenamePetIntent, PetToggleAutocastIntent
    > op;
};
struct QueueIntent
{
    std::variant<
        BgQueueIntent, BgLeaveIntent, BgPortIntent,
        LfgQueueIntent, LfgUnqueueIntent, LfgProposalRespondIntent,
        LfgRoleCheckIntent
    > op;
};
// Housing (12.0+) — 4 sub-ops. Wrapped per feedback_intent_variant_capacity
// so the master IntentBody variant stays under MSVC's
// visitor-instantiation heap limit. No emitters or executors exist yet
// (housing system not wired); wrapper reserves the type slots so future
// housing code adds sub-ops here without expanding the master variant.
struct HousingIntent
{
    std::variant<
        JoinNeighborhoodIntent, PlotPurchaseIntent,
        PlaceDecorationIntent, VisitHouseIntent
    > op;
};

// The variant. Order is fixed once shipped — appending only.
// Wrapped sub-types are NOT in this top-level list; they live inside
// their subsystem wrapper. New subsystems should follow the wrapper
// pattern to keep IntentBody bounded.
using IntentBody = std::variant<
    CastSpellIntent, GroundTargetSpellIntent, CancelCastIntent,
    StartAttackIntent, StopAttackIntent, PetAttackIntent, PetCastSpellIntent,
    DismissPetIntent, SetStandStateIntent, UnstuckIntent, CancelAuraIntent,
    MoveToIntent, TeleportToIntent, StopMovementIntent, JumpIntent,
    MountIntent, DismountIntent, HearthIntent, FollowIntent,
    UseItemIntent, UseItemByEntryIntent, EquipItemIntent, LootIntent,
    ReleaseCorpseIntent, ReviveAtCorpseIntent,
    ReclaimCorpseIntent, SpiritResurrectIntent, AcceptRezIntent,
    QuestAcceptIntent, QuestCompleteIntent, QuestAbandonIntent,
    QuestSharedAcceptIntent, ResolveJunkQuestsIntent,
    TrainerBuySpellIntent, TrainerBuyAllIntent,
    BankDepositItemIntent, BankWithdrawItemIntent,
    BindHomebindIntent,
    DiscoverTaxiNodeIntent, FlyToNodeIntent,
    ApplyStarterTalentsIntent,
    LootRollIntent,
    GroupAcceptIntent, GroupDeclineIntent, GroupLeaveIntent, GroupReadyResponseIntent,
    GroupPromoteToLeaderIntent, GroupKickMemberIntent, GroupConvertToRaidIntent,
    GroupStartReadyCheckIntent, GroupSetAssistantIntent, ResetInstancesIntent,
    GuildAcceptInviteIntent, GuildDeclineInviteIntent, GuildLeaveIntent,
    ToggleAfkIntent, ToggleDndIntent,
    SetDungeonDifficultyIntent, SetRaidDifficultyIntent,
    PerformEmoteIntent, FaceTargetIntent,
    TogglePvpIntent,
    AddFriendIntent, RemoveFriendIntent, AddIgnoreIntent, RemoveIgnoreIntent,
    CalendarRsvpAllIntent,
    GuildBankDepositMoneyIntent, GuildBankWithdrawMoneyIntent,
    GuildBankDepositItemIntent,  GuildBankWithdrawItemIntent,
    ShareQuestIntent,
    SummonAcceptIntent, SummonDeclineIntent,
    DuelDeclineIntent, DuelAcceptIntent, TradeDeclineIntent,
    InviteToGroupIntent,
    SetRaidTargetIconIntent, ActivateSpecIntent, ResetCooldownsIntent,
    NearTeleportToIntent,
    UseObjectIntent, InteractWithNpcIntent, GossipSelectIntent,
    EnterVehicleIntent, ExitVehicleIntent, VehicleSpellIntent, VehicleGroundSpellIntent,
    ApplyTalentBuildIntent, CastSpellOnItemIntent,
    GuildIntent,
    // Subsystem wrappers — see comment above. Housing wrapped here
    // 2026-05-21 (was 4 raw alternatives; rolled into HousingIntent
    // sub-variant for IntentBody capacity discipline).
    ChatIntent, MailIntent, AuctionIntent, VendorIntent,
    HunterPetIntent, QueueIntent, HousingIntent,
    // Economy buy-side wrapper (#4B). Nests EconomyOp::* sub-ops; one
    // top-level alternative keeps IntentBody under MSVC's visitor limit.
    EconomyIntent
>;

struct Intent
{
    IntentId    id              = 0;
    BotId       bot_id          = 0;
    SnapshotVer source_snapshot = 0;
    // Defer-until timestamp (GameTime::GetGameTimeMS()). When non-zero,
    // the dispatch loop checks `now < defer_until_ms` and re-queues the
    // intent without executing it. Used for human-pacing of social
    // replies (whispers, /p chat) where the intent is composed
    // immediately but the visible packet must NOT go out until 2–6s
    // later to model a human reading the message and typing. Zero
    // (the default) means "fire as soon as dispatch picks it up".
    uint32      defer_until_ms  = 0;
    IntentBody  body;
};

// Cheap kind-discrimination for diagnostics / metrics.
inline size_t IntentKind(Intent const& i) { return i.body.index(); }

// Human-readable name for a variant index. Lives HERE, next to the variant,
// as the single source of truth: BotInspector used to keep its own copy of
// this table, it drifted when the chat intents were rolled into ChatIntent,
// and every /diag intent label past index 10 was wrong (an EquipItem retry
// wedge read as "Dismount | ServerRefused" — diagnosed as the wrong bug on
// 2026-06-10). The static_asserts force this list to be updated whenever an
// alternative is added to / removed from IntentBody.
static_assert(std::variant_size_v<IntentBody> == 100,
              "IntentBody changed — update IntentKindName() below to match.");
inline char const* IntentKindName(size_t kind)
{
    static constexpr char const* kNames[] = {
        "CastSpell", "GroundTargetSpell", "CancelCast",
        "StartAttack", "StopAttack", "PetAttack", "PetCastSpell",
        "DismissPet", "SetStandState", "Unstuck", "CancelAura",
        "MoveTo", "TeleportTo", "StopMovement", "Jump",
        "Mount", "Dismount", "Hearth", "Follow",
        "UseItem", "UseItemByEntry", "EquipItem", "Loot",
        "ReleaseCorpse", "ReviveAtCorpse",
        "ReclaimCorpse", "SpiritResurrect", "AcceptRez",
        "QuestAccept", "QuestComplete", "QuestAbandon",
        "QuestSharedAccept", "ResolveJunkQuests",
        "TrainerBuySpell", "TrainerBuyAll",
        "BankDepositItem", "BankWithdrawItem",
        "BindHomebind",
        "DiscoverTaxiNode", "FlyToNode",
        "ApplyStarterTalents",
        "LootRoll",
        "GroupAccept", "GroupDecline", "GroupLeave", "GroupReadyResponse",
        "GroupPromoteToLeader", "GroupKickMember", "GroupConvertToRaid",
        "GroupStartReadyCheck", "GroupSetAssistant", "ResetInstances",
        "GuildAcceptInvite", "GuildDeclineInvite", "GuildLeave",
        "ToggleAfk", "ToggleDnd",
        "SetDungeonDifficulty", "SetRaidDifficulty",
        "PerformEmote", "FaceTarget",
        "TogglePvp",
        "AddFriend", "RemoveFriend", "AddIgnore", "RemoveIgnore",
        "CalendarRsvpAll",
        "GuildBankDepositMoney", "GuildBankWithdrawMoney",
        "GuildBankDepositItem", "GuildBankWithdrawItem",
        "ShareQuest",
        "SummonAccept", "SummonDecline",
        "DuelDecline", "DuelAccept", "TradeDecline",
        "InviteToGroup",
        "SetRaidTargetIcon", "ActivateSpec", "ResetCooldowns",
        "NearTeleportTo",
        "UseObject", "InteractWithNpc", "GossipSelect",
        "EnterVehicle", "ExitVehicle", "VehicleSpell", "VehicleGroundSpell",
        "ApplyTalentBuild", "CastSpellOnItem",
        "Guild",
        "Chat", "Mail", "Auction", "Vendor",
        "HunterPet", "Queue", "Housing",
        "Economy",
    };
    static_assert(std::size(kNames) == std::variant_size_v<IntentBody>,
                  "kNames out of sync with IntentBody");
    return kind < std::size(kNames) ? kNames[kind] : "?";
}

// ---- Auto-wrap helper for subsystem-wrapper intents ----
//
// Call sites can keep emitting the underlying intent type (e.g. `WhisperIntent`,
// `LfgQueueIntent`); this helper wraps it into the appropriate subsystem
// wrapper before it reaches `IntentBody`. Types that aren't wrapped pass
// through unchanged. Keeps Push/BroadcastToGroup helpers terse and prevents
// future regressions where someone adds a new emission site for a wrapped
// type but forgets to wrap it manually.
template <class T>
auto WrapForIntentBody(T&& body)
{
    using U = std::decay_t<T>;
    if constexpr (std::is_same_v<U, SayChatIntent>      ||
                  std::is_same_v<U, YellChatIntent>     ||
                  std::is_same_v<U, EmoteChatIntent>    ||
                  std::is_same_v<U, GuildChatIntent>    ||
                  std::is_same_v<U, RaidChatIntent>     ||
                  std::is_same_v<U, OfficerChatIntent>  ||
                  std::is_same_v<U, RaidWarningIntent>  ||
                  std::is_same_v<U, WhisperIntent>      ||
                  std::is_same_v<U, PartyChatIntent>)
        return ChatIntent{std::forward<T>(body)};
    else if constexpr (std::is_same_v<U, MailTakeMoneyIntent> ||
                       std::is_same_v<U, MailTakeItemIntent>  ||
                       std::is_same_v<U, MailDeleteIntent>    ||
                       std::is_same_v<U, MailSendMoneyIntent> ||
                       std::is_same_v<U, MailSendItemIntent>)
        return MailIntent{std::forward<T>(body)};
    else if constexpr (std::is_same_v<U, AuctionSellItemIntent>  ||
                       std::is_same_v<U, AuctionCancelIntent>    ||
                       std::is_same_v<U, AuctionCancelAllIntent>)
        return AuctionIntent{std::forward<T>(body)};
    else if constexpr (std::is_same_v<U, VendorBuyIntent>            ||
                       std::is_same_v<U, VendorSellIntent>           ||
                       std::is_same_v<U, VendorSellTrashIntent>      ||
                       std::is_same_v<U, RepairAllIntent>            ||
                       std::is_same_v<U, VendorBuyByCategoryIntent>  ||
                       std::is_same_v<U, VendorBuyByEntryIntent>)
        return VendorIntent{std::forward<T>(body)};
    else if constexpr (std::is_same_v<U, SwapPetToSlotIntent>      ||
                       std::is_same_v<U, DeleteStabledPetIntent>   ||
                       std::is_same_v<U, SummonPetByNumberIntent>  ||
                       std::is_same_v<U, FeedPetIntent>            ||
                       std::is_same_v<U, AbandonPetIntent>         ||
                       std::is_same_v<U, PetSetReactStateIntent>   ||
                       std::is_same_v<U, PetSetCommandStateIntent> ||
                       std::is_same_v<U, RenamePetIntent>          ||
                       std::is_same_v<U, PetToggleAutocastIntent>)
        return HunterPetIntent{std::forward<T>(body)};
    else if constexpr (std::is_same_v<U, BgQueueIntent>             ||
                       std::is_same_v<U, BgLeaveIntent>             ||
                       std::is_same_v<U, BgPortIntent>              ||
                       std::is_same_v<U, LfgQueueIntent>            ||
                       std::is_same_v<U, LfgUnqueueIntent>          ||
                       std::is_same_v<U, LfgProposalRespondIntent>  ||
                       std::is_same_v<U, LfgRoleCheckIntent>)
        return QueueIntent{std::forward<T>(body)};
    else
        return std::forward<T>(body);
}

} // namespace Playerbot
