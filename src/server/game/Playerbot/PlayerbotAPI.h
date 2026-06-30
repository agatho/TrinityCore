// PlayerbotAPI.h
// The single core-side API surface used by Playerbot V2. Per v2/REQUIREMENTS.md
// §3 and v2/API.md, this is the *only* core file V2 module code may include
// when accessing Player/Unit/Map/Group/Spell/Item/Quest internals.
//
// Bootstrap: this header declares the API class shell with placeholder methods.
// The full ~150-method surface (per v2/API.md) lands incrementally as subsystems
// in v2/MODULE_LAYOUT.md are implemented.

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include <chrono>
#include <string>
#include <vector>

class Player;
class Unit;
class Aura;
class Group;

namespace Playerbot {

constexpr uint32 kApiVersion = 1;

using Ms = std::chrono::milliseconds;

// All commands return Result. Snapshot-time methods return data directly.
enum class Result : uint8
{
    Ok,
    NotReady,
    OutOfRange,
    InvalidTarget,
    NotEnoughResource,
    NotKnown,
    ServerRefused,
    InventoryFull,
    Locked,
    Other
};

// Per-world-tick budget capping how long bot-initiated pathfinding (Detour
// CalculatePath) may run inside one DrainIntents pass on the world thread.
// 2026-06-17: raising the quest funnel made many bots move/charge in one tick;
// the world thread did all that synchronous pathfinding inline and hung 87s ->
// forced crash. The module calls BeginWorldTick() once at the top of DrainIntents
// and EndWorldTick() at the end; each synchronous pathfind site checks HasBudget()
// and defers (returns Result::Locked, which rules already treat as "retry next
// tick") once the tick's pathfinding window is spent. World-thread only — no
// synchronization. HasBudget() FAILS OPEN outside a Begin/End window so non-intent
// callers are never throttled.
namespace PathBudget
{
    void BeginWorldTick(uint32 now_ms, uint32 budget_ms);
    void EndWorldTick();
    bool HasBudget(uint32 now_ms);
}

// API: thin facade over Player and friends. Constructed transiently per call
// (cheap — just a pointer wrapper). All methods run on the world thread; calling
// from any other thread is a programming error and asserts in debug builds.
class API
{
public:
    explicit API(Player* p);

    // Identity
    ObjectGuid  guid()    const;
    std::string name()    const;
    uint8       level()   const;
    uint8       cls()     const;
    uint8       race()    const;
    uint8       spec()    const;
    uint32      faction() const;

    // Vital stats (snapshot-time)
    int32       hp()         const;
    int32       max_hp()     const;
    bool        is_in_combat() const;
    bool        is_alive()   const;

    // (Full surface declared in v2/API.md lands incrementally; this header
    //  is the single point of truth and grows additively.)

    // ---- Bootstrap-era no-op command stubs ---------------------------------
    // Every command in v2/API.md will appear here. For the bootstrap, only a
    // handful are declared so the class is non-empty and the linkage shape can
    // be tested. They return Result::Other until implemented.

    Result cast_spell(uint32 spell_id, ObjectGuid target = ObjectGuid::Empty);
    // Cast a ground-target spell (Death and Decay, Blizzard, Ring of Frost,
    // Healing Rain, Ursol's Vortex). Bot's facing/position constraints still
    // apply server-side; OutOfRange / InvalidTarget will return through Result.
    Result cast_spell_at_position(uint32 spell_id, float x, float y, float z);
    // Cast a spell whose primary target is an inventory Item — used by
    // disenchant (13262), prospecting (31252), milling (51005), and
    // similar item-consuming profession spells. The Item GUID is the
    // bag-resident item to be transformed.
    Result cast_spell_on_item(uint32 spell_id, ObjectGuid item_guid);
    Result move_to(float x, float y, float z, bool run = true);
    // clear_generators=true pops all MotionMaster generators (Clear + MoveIdle)
    // instead of only stopping the active spline, so a stale POINT/CHASE/FOLLOW
    // target (e.g. one that survived a cross-map teleport) cannot re-issue.
    Result stop_movement(bool clear_generators = false);
    Result hearth();

    Result start_attack(ObjectGuid target);
    // clear_ghost_combat: when the bot has NO attackers, additionally drop
    // its PvE combat references (Unit::CombatStop) — self-heal for the
    // stuck-combat-flag wedge that holds group progression gates hostage.
    Result stop_attack(bool clear_ghost_combat = false);
    Result cancel_cast();

    // Follow with optional formation offset. angle_radians is the
    // chase angle in the leader's local frame: 0 = directly behind,
    // pi/2 = right flank, pi = in front. Default 0 preserves the
    // legacy behind-leader follow.
    Result follow(ObjectGuid leader, float distance, float angle_radians = 0.0f);
    Result mount(uint32 mount_id);
    Result dismount();

    // ---- Vehicle ------------------------------------------------------
    // Enter the named vehicle Unit. seat_id == -1 picks the first free seat.
    // Mirrors Unit::EnterVehicle (which casts VEHICLE_SPELL_RIDE_HARDCODED
    // → seat-assignment aura). InvalidTarget if vehicle isn't a Unit, has
    // no VehicleKit, or is out of interact range. Locked if all seats full.
    // Apply a curated talent build for the given (class, spec, context).
    // context: 0=Default, 1=Raid, 2=MythicPlus, 3=PvP, 4=Leveling.
    // Looks up `playerbot_v2_talent_build` for the row matching
    // (class_id, spec_id, context); falls back to context=Default if no
    // context-specific row exists; falls back to TraitMgr starter build
    // if no curated row exists at all. Combat-locked.
    Result apply_talent_build(uint8 context);

    Result enter_vehicle(ObjectGuid vehicle, int8 seat_id);
    // Exit the bot's current vehicle. NoOp if not on a vehicle.
    Result exit_vehicle();
    // Cast the bot's vehicle's spell from its current seat. The seat's
    // ability spell IDs are stored on the vehicle template; the caller
    // typically reads them from snapshot's vehicle_seat_spells. Target
    // is the spell's primary target (empty for self/AOE).
    Result cast_vehicle_spell(uint32 spell_id, ObjectGuid target = ObjectGuid::Empty);
    // Ground-target variant for siege ability (boulder, mortar etc.).
    Result cast_vehicle_spell_at(uint32 spell_id, float x, float y, float z);
    Result party_chat(std::string const& text);
    // Public chat variants. say (CHAT_MSG_SAY) / yell (CHAT_MSG_YELL) /
    // emote_text (CHAT_MSG_EMOTE custom text). All bypass the group/raid
    // channels — they go to nearby players in normal chat range.
    Result say(std::string const& text);
    Result yell(std::string const& text);
    Result emote_text(std::string const& text);
    // Guild + raid chat broadcast.
    Result guild_chat(std::string const& text);
    Result raid_chat(std::string const& text);
    // Owner emergency unstuck. NearTeleport the bot N yards in their facing
    // direction; the server clamps to navigable terrain. Refused mid-cast
    // and in combat (would break engagement).
    Result unstuck(float distance);
    // Precise near-teleport. (x,y,z) on the bot's current map; z is snapped
    // to the navmesh-ground at (x,y) if the caller's z is slightly off.
    // Used by the auto-unstick rule's Tier 2 escalation: when the bot has
    // been wedged on geometry for ~15 s, this teleports them a few yards
    // away with a randomized offset to break out of the pocket.
    Result near_teleport_to(float x, float y, float z, float o);
    // Cancel a self aura by spell id. RemoveAurasDueToSpell is idempotent.
    Result cancel_aura(uint32 spell_id);
    Result whisper(std::string const& target_name, std::string const& text);

    // Death / corpse handling
    // release_corpse: BuildPlayerRepop + RepopAtGraveyard. Equivalent of the
    // client clicking "Release Spirit" in the death prompt — moves the bot
    // into PLAYER_FLAGS_GHOST and teleports the ghost to the closest
    // graveyard. Corpse stays at the death location for a corpse run.
    Result release_corpse();
    // revive_at_corpse: legacy "instant rez at current position, no sickness"
    // helper retained for the GM `.playerbot revive` command. NOT used by the
    // automatic recovery flow — that path goes through reclaim_corpse (true
    // corpse run) or spirit_resurrect (graveyard rez with sickness) instead.
    Result revive_at_corpse();
    // reclaim_corpse: completes the human corpse-run. Mirrors
    // WorldSession::HandleReclaimCorpse: requires PLAYER_FLAGS_GHOST + own
    // corpse + within CORPSE_RECLAIM_RADIUS (39yd) + ghost-time delay
    // expired, then ResurrectPlayer(0.5f) and SpawnCorpseBones. No
    // resurrection sickness.
    Result reclaim_corpse();
    // spirit_resurrect: spirit-healer instant rez. Mirrors
    // WorldSession::SendSpiritResurrect: ResurrectPlayer(0.5f, applySickness=true)
    // + DurabilityLossAll(0.25f) + SpawnCorpseBones, then teleport to the
    // graveyard nearest the corpse if it differs from the current location.
    // Skips the spirit-healer NPC interaction (no NPC needed; we trust
    // State_Dead's choice).
    Result spirit_resurrect();
    // Accept a pending resurrection request (Resurrection / Rebirth /
    // Soulstone). Returns Locked if no request is pending.
    Result accept_rez();
    // Accept the bot's pending group/raid invite. Returns Locked when no
    // invite is outstanding.
    // Player-summon dialog response: warlock summon ritual / meeting stone /
    // BG summon. accept_summon teleports to the summoner's last reported
    // location (Player::SummonIfPossible(true)). Returns Locked when no
    // pending summon exists or the request expired (2-minute window). The
    // bot's own taxi is finished as a side-effect (cannot summon mid-flight).
    Result accept_summon();
    Result decline_summon();

    // Decline a pending duel request. Mirrors HandleDuelCancelled from the
    // CHALLENGED state — drops the duel arbiter and notifies the initiator.
    // Returns Locked when no duel is pending or it's already past the
    // challenge phase (countdown / in-progress duels can't be declined; use
    // DuelComplete with DUEL_FLED for surrender). Bots auto-decline by
    // default to avoid being abused into PvP flags.
    Result decline_duel();

    // Accept a pending duel request. Mirrors HandleDuelAccepted: validates
    // the bot is the challengee (not initiator), in CHALLENGED state, and
    // the arbiter is correctly attached to the opponent. Then advances both
    // sides to DUEL_STATE_COUNTDOWN and dispatches the 3-second countdown
    // packet, enabling PvP rules on both. Returns Locked when no challenge
    // is pending or the arbiter is missing.
    Result accept_duel();

    // Set a raid target marker (skull/cross/...; symbol is 0..7 mirroring
    // RaidTargetIcon enum) on `target`. Mirrors HandleUpdateRaidTargetOpcode:
    // requires the bot to be in a group; for raid groups, the bot must be
    // leader or assistant. Returns Locked when the bot has no group, or has
    // no permission, or `symbol` is out of range. Pass ObjectGuid::Empty for
    // `target` to clear the symbol.
    Result set_raid_target_icon(uint8 symbol, ObjectGuid target);

    // Activate a different specialization. `spec_id` is ChrSpecialization.db2
    // id (250..1473 in 12.0). Resolves the entry via sChrSpecializationStore
    // and validates it matches the bot's class. Server-side ActivateTalentGroup
    // unsummons pet, clears auras, and pages in the new talent build.
    // Returns NotKnown when spec_id is unknown or not for this class;
    // Locked when the bot is mid-combat (we want a consistent gate even
    // though the game allows it; mid-combat respec is jarring).
    Result activate_spec(uint32 spec_id);

    // Diagnostic: clear every spell cooldown on the bot via SpellHistory::
    // ResetAllCooldowns. Mostly useful when iterating on combat behavior
    // — the rotation can be tested back-to-back without waiting for the
    // server-side CD walls. Always returns Ok unless p_ is null.
    Result reset_all_cooldowns();

    // Decline an open player-to-player trade. Closes the trade window via
    // Player::TradeCancel(true) which sends the cancel packet to the trader.
    // Returns Locked when there's no active trade. Always-on auto-decline
    // because bots aren't safe to trade with arbitrary players (item theft).
    Result decline_trade();

    // Accept (or decline) a pending battleground invite popup. Mirrors
    // HandleBattleFieldPortOpcode's accept-invite branch end-to-end:
    // resurrects the bot if dead, sets the BG entry point, removes any
    // current BG, and ports via BattlegroundMgr::SendToBattleground.
    // `accept=false` mirrors the leave-queue branch (drops from the queue
    // and re-schedules matchmaking). Returns NotKnown when the bot has no
    // queue for that bg_type_id, Locked when not invited / deserter / freeze,
    // ServerRefused when the BG instance was destroyed mid-invite.
    Result bg_port(uint16 bg_type_id, bool accept);

    // Respond to an LFG dungeon-ready proposal. Wraps LFGMgr::UpdateProposal
    // (the same path the live HandleLfgProposalResultOpcode uses). Returns
    // Locked when the proposal id no longer exists (timed out or another
    // member declined). The bot side passes `proposal_id` from the snapshot.
    Result lfg_proposal_respond(uint32 proposal_id, bool accept);

    // Respond to the bot's group's LFG role-check with `roles` (LFG bitmask
    // — TANK=2, HEALER=4, DAMAGE=8, LEADER=1). Returns Locked when no role
    // check is active. Mirrors the same path as HandleLfgSetRolesOpcode.
    Result lfg_role_check(uint8 roles);

    // Cancel every active auction the bot owns in `auctioneer`'s house. Walks
    // the AuctionHouseObject's owned-auction multimap (via the new
    // GetOwnedAuctionIds accessor), then runs the same single-cancel path
    // for each id — same gating around active bidder cancel-fees etc.
    // Returns Locked when no auctions are owned in this house, Ok if at
    // least one was cancelled. Used by `/cancelall` whisper.
    Result auction_cancel_all(ObjectGuid auctioneer);

    // Buy a quantity of vendor items matching item_class / item_subclass.
    // The API walks the vendor's item list (top-down in display order),
    // picks the highest level item the bot meets the level requirement for,
    // and issues BuyItemFromVendorSlot calls until total_count units are
    // purchased (or the vendor runs out of stock for the chosen slot).
    // Used by the OOC auto-restock rules: "give me 20 units of food/drink",
    // "give me 5 potions". Returns OutOfRange when the vendor has no items
    // matching the category, NotEnoughResource when gold runs out partway
    // through (caller can retry once topped up), Ok if any units bought.
    Result vendor_buy_by_category(ObjectGuid npc, uint8 item_class, uint8 item_subclass,
                                   uint8 total_count);

    // Accept the quest a group member just shared via the standard share UI.
    // Mirrors HandleQuestgiverAcceptQuestOpcode's group-share branch:
    // re-runs CanTakeQuest + CanAddQuest, then AddQuestAndCheckCompletion
    // with the sharing player as the source. Clears the bot's shared-quest
    // state so the popup goes away. Returns Locked when no share is pending,
    // NotKnown when the shared quest id was removed from quest templates,
    // ServerRefused when CanTakeQuest fails (level / class / faction / log full).
    Result accept_shared_quest();

    Result accept_group_invite();
    // Decline the bot's pending group/raid invite and notify the leader.
    // Used when the inviter isn't an authorized owner. Returns Locked if
    // no invite is outstanding.
    Result decline_group_invite();
    // Send a party invite to `target_player`. Mirrors HandlePartyInviteOpcode
    // — same gating around faction/instance/level/social ignore. If the bot
    // has no group, a fresh group is created with the bot as leader; if the
    // bot already leads a non-full group, the invitee is added there. Returns
    // InvalidTarget when the target is unreachable / same player / wrong
    // faction, Locked when the target is already in a group or the bot's
    // group is full / the bot lacks invite rights, Ok when the invite was
    // dispatched (invitee accept/decline is asynchronous and surfaces via
    // existing GroupAccept/Decline intents).
    Result invite_to_group(ObjectGuid target_player);
    // Respond to an active ready check (CMSG_READY_CHECK_RESPONSE equivalent).
    // ready=true when the bot is ready to pull. Returns Locked if no check is
    // currently active for the bot's group.
    Result group_ready_response(bool ready);
    // Leave the bot's current group/raid. Returns Locked if not grouped.
    Result leave_group();
    // Promote `new_leader_guid` to group leader. Bot must currently be the
    // group leader (the only role that can transfer). Returns Locked when
    // the bot isn't grouped or isn't leader; InvalidTarget when the named
    // GUID isn't a member.
    Result promote_to_leader(ObjectGuid new_leader_guid);
    // Kick `member_guid` from the bot's group (party uninvite). Bot must be the
    // group leader. Mirrors HandlePartyUninviteOpcode (RemoveMethod KICK).
    // Returns Locked when not leader; InvalidTarget when target isn't a member
    // or the leader tries to kick themselves.
    Result kick_group_member(ObjectGuid member_guid);
    // Convert the bot's 5-man party to a 40-man raid (Group::ConvertToRaid).
    // Leader-only. Idempotent — already-raid groups return Ok.
    Result convert_to_raid();
    // Initiate a ready check on the bot's group. Leader-only in parties; in
    // raids, assistants may also start one. Default duration READYCHECK_DURATION.
    Result start_ready_check();
    // Toggle MEMBER_FLAG_ASSISTANT on a raid member (only meaningful in raids).
    // Leader-only. Returns Locked outside a raid or when bot isn't leader.
    Result set_assistant(ObjectGuid member_guid, bool assistant);
    // Reset all (non-locked) instance binds the bot has. When grouped and the
    // bot is leader, resets the group's binds via Group::ResetInstances; ungrouped
    // falls back to Player::ResetInstances. Returns Locked if grouped + non-leader.
    Result reset_instances();
    // Accept a pending guild invite (Player::GetGuildIdInvited != 0). Mirrors
    // HandleGuildAcceptInvite: silently no-ops if no invite. Returns Locked
    // when the bot is already in a guild (server enforces this).
    Result accept_guild_invite();
    // Decline a pending guild invite (clear GuildIdInvited). Idempotent.
    Result decline_guild_invite();
    // Leave the bot's current guild (Guild::HandleLeaveMember). Returns Locked
    // when not in a guild. The guild master cannot leave without first
    // demoting — server enforces this.
    Result leave_guild();
    // Toggle the AFK flag (Player::ToggleAFK). Stays armed until cleared by
    // movement or explicit toggle. Used to mark bots as "go away" during owner
    // sessions where bots should be ignored by ad-hoc /who lookups.
    Result toggle_afk();
    // Toggle the DND flag (Player::ToggleDND). Same flow as AFK.
    Result toggle_dnd();
    // Change dungeon difficulty (HandleSetDungeonDifficultyOpcode). Validates
    // DifficultyEntry exists, is MAP_INSTANCE, has CAN_SELECT flag, bot isn't
    // currently in an instanceable map, and (if grouped) bot is leader of a
    // non-LFG group. Returns InvalidTarget when validation fails, Locked when
    // grouping/instance state forbids the change.
    Result set_dungeon_difficulty(uint32 difficulty_id);
    // Change raid difficulty (HandleSetRaidDifficultyOpcode). `legacy` selects
    // the legacy slot vs current. Same validation envelope as dungeon.
    Result set_raid_difficulty(uint32 difficulty_id, bool legacy);
    // Send a CHAT_MSG_OFFICER message to the bot's guild (BroadcastToGuild
    // with officerOnly=true). The Guild rank-perm check filters non-officers.
    Result officer_chat(std::string const& text);
    // Send a CHAT_MSG_RAID_WARNING message to the bot's group. Leader or
    // assistant required in raid groups; CONFIG_CHAT_PARTY_RAID_WARNINGS
    // gates use in plain parties (typically off in retail). Returns Locked
    // when not grouped or the bot lacks the rank.
    Result raid_warning(std::string const& text);
    // Trigger a visual emote on the bot. Mirrors the chat-handler /emote
    // command flow — calls Unit::HandleEmoteCommand which broadcasts an
    // SMSG_EMOTE packet visible to nearby players. `target` is optional and
    // changes the emote's directional cue (wave at someone, etc.).
    Result perform_emote(uint32 emote_id, ObjectGuid target = ObjectGuid::Empty);
    // Face the bot toward `target` (Unit::SetFacingToObject). Used for RP /
    // screenshots / pet-pose alignment. Returns InvalidTarget when the named
    // object isn't visible from the bot's map.
    Result face_target(ObjectGuid target);
    // Toggle the PvP flag (Player::UpdatePvP). When enabled, the bot can be
    // attacked by opposite-faction players in contested zones; when disabled,
    // it's PvE-only. Mirrors HandleTogglePvP. Sticky for 5 minutes after
    // disable per the standard PvP flag protocol.
    Result toggle_pvp();
    // Friend/ignore list management. `name` is normalized then resolved via
    // sCharacterCache (no async DB lookup — uses the cached snapshot, so newly-
    // created characters not yet flushed to cache will return InvalidTarget).
    // Add operations check faction same-team (server enforces); remove uses
    // the resolved GUID and is permissive about invalid lookups (returns Ok
    // when the guid wasn't on the list anyway).
    Result add_friend(std::string const& name, std::string const& note);
    Result remove_friend(ObjectGuid friend_guid);
    Result add_ignore(std::string const& name);
    Result remove_ignore(ObjectGuid ignore_guid);
    // Send a money-only mail to `recipient_name`. Mirrors HandleSendMail's
    // money path: validates name + cache lookup, deducts cost (30c base) and
    // money from the bot, sends via MailDraft + SendMailTo with the bot as
    // sender. No items, no COD. Returns InvalidTarget when name unknown,
    // NotEnoughResource when bot lacks gold for cost+money. Useful for bots
    // mailing farmed gold home to the owner during sessions.
    Result mail_send_money(std::string const& recipient_name, uint64 copper,
                           std::string const& subject, std::string const& body);
    // Send an item attachment (single item per call). `count` of 0 means
    // mail the entire stack; smaller values clone-split the partial. `cod`
    // gates pickup behind the recipient paying the named copper amount;
    // wrapped items can't be COD-mailed (server convention). Postage is
    // 30c flat regardless of attachment count. Returns Locked on item-state
    // disqualification (non-empty bag, non-tradeable, conjured, expiring,
    // wrapped+COD). Mirrors HandleSendMail's attachment path.
    Result mail_send_item(std::string const& recipient_name, ObjectGuid item_guid,
                          uint32 count, uint64 copper, uint64 cod,
                          std::string const& subject, std::string const& body);
    // Auto-RSVP all of the bot's pending calendar invites. `accept` selects
    // ACCEPTED (true) or DECLINED (false) for the response. Returns Ok with
    // the count returned via out_responded; OutOfRange if no pending invites.
    Result calendar_rsvp_all_pending(bool accept, uint32* out_responded = nullptr);
    // Teleport the bot to the given coordinates on the named map. Used by
    // owner whisper commands ("come" / "summon") to pull bots to the leader.
    Result teleport_to(uint32 map_id, float x, float y, float z, float orientation = 0.f);

    // Pet command (Hunter / Warlock / Death Knight). Sends the active pet to
    // attack `target`. No-op if the bot has no live pet.
    Result pet_attack(ObjectGuid target);
    // Issue a pet ability (Felhunter Spell Lock 19647, Hunter pet basic
    // attacks, DK ghoul Leap, etc.). Returns Locked when the pet does not
    // know the spell or it's still on cooldown; InvalidTarget when the
    // target couldn't be resolved.
    Result pet_cast(uint32 spell_id, ObjectGuid target = ObjectGuid::Empty);
    // Dismiss the bot's active pet. Hunter/warlock pets become recallable;
    // other charmed/temporary pets are released. Returns NotKnown if the
    // bot has no pet, ServerRefused mid-combat (pet management blocks).
    Result dismiss_pet();
    // ---- Hunter pet stable management --------------------------------
    // Move a pet (identified by petNumber from the snapshot's stable list)
    // into the given destination slot. `dst_slot` uses PetSaveMode raw
    // values: 0..MAX_ACTIVE_PETS-1 are active slots, 5..(5+MAX_PET_STABLES-1)
    // are stable slots. Mirrors the gossip-driven HandleSetPetSlot path
    // (Player::SetPetSlot) end-to-end including active<->stable despawn,
    // exotic-tame check, and updatefield sync. Returns InvalidTarget when
    // the bot is not a Hunter or `pet_number` isn't owned, OutOfRange when
    // dst_slot is past the legal range, ServerRefused when the active pet
    // is dead during an active<->stable swap, Locked when CanTameExotic
    // would fail. The Hunter's stable size is gated by purchased slots
    // (CGame::GetExtraPetStables on retail) — out-of-range slots beyond
    // the bot's purchased capacity also return OutOfRange.
    Result swap_pet_to_slot(uint32 pet_number, uint8 dst_slot);
    // Permanently delete a stabled or unslotted pet. Mirrors the live
    // delete-from-stable gossip path (Player::DeletePetFromDB). Refuses
    // to delete the currently summoned pet — caller must dismiss first.
    // Returns NotKnown when no pet with that number exists, Locked if it
    // matches the active pet, Ok on success.
    Result delete_stabled_pet(uint32 pet_number);
    // Summon a pet from one of the bot's active slots by petNumber. The
    // pet is loaded from DB at full saved state and CurrentPetIndex is
    // updated. Returns NotKnown if pet_number isn't an active-slot pet,
    // Locked if a pet is already summoned (caller must dismiss first),
    // ServerRefused if the bot is in combat or mid-cast.
    Result summon_pet_by_number(uint32 pet_number);
    // Cast Feed Pet (6991) on the active pet using the given food item.
    // Mirrors the live food-pet flow: validates item exists in inventory,
    // pet is happy-eligible, food level is appropriate. Returns NotKnown
    // when the bot has no pet or doesn't know Feed Pet, OutOfRange when
    // food is too low-level for the pet, NotEnoughResource when no item
    // matching `item_entry` is in inventory.
    Result feed_pet(uint32 food_item_entry);
    // Abandon the active pet (Hunter only). Untames the pet, freeing the
    // active slot. Mirrors HandleAbandonPet — pet is released with
    // PET_SAVE_AS_DELETED. Returns NotKnown when bot has no Hunter pet,
    // ServerRefused mid-combat.
    Result abandon_pet();
    // Set the pet's react state. `state` mirrors ReactStates: 0=Passive,
    // 1=Defensive, 2=Aggressive, 3=Assist. Persists across save/load via
    // CharmInfo. Returns NotKnown when the bot has no pet, InvalidTarget
    // for state >= 4. Useful for owner-controlled pet stance switches
    // (`/pet passive` in cities, `/pet aggressive` for AoE pulls).
    Result pet_set_react_state(uint8 state);
    // Set the pet's command stance. `command` mirrors CommandStates:
    // 0=Stay, 1=Follow, 2=Attack (no-op without target). Mirrors the pet
    // bar's stay/follow buttons. Returns NotKnown when bot has no pet.
    Result pet_set_command_state(uint8 command);
    // Rename the active hunter pet. Mirrors HandlePetRename: validates the
    // CAN_BE_RENAMED flag (pet may only be renamed once per tame), runs
    // ObjectMgr::CheckPetName + reserved-name check, sets Name on both Pet
    // and the petStable PetInfo, raises GROUP_UPDATE_FLAG_PET_NAME, and
    // clears the rename flag so subsequent renames are refused. Returns
    // NotKnown when bot has no Hunter pet, Locked when already renamed,
    // InvalidTarget for empty/invalid/reserved names.
    Result rename_pet(std::string const& new_name);
    // Toggle a pet spell's autocast state. Pet must know the spell and the
    // spell must be autocastable (SpellInfo::IsAutocastable). Mirrors
    // HandlePetSpellAutocastOpcode: calls Pet::ToggleAutocast (or
    // CharmInfo::ToggleCreatureAutocast for non-pet charmed minions) plus
    // SetSpellAutocast on the action bar. Returns NotKnown when bot has no
    // pet, Locked when the pet doesn't know the spell, InvalidTarget when
    // the spell isn't autocastable. Useful for Hunter Pet bite/claw cycle
    // toggles, Felhunter Devour Magic on/off, etc.
    Result pet_toggle_autocast(uint32 spell_id, bool enabled);
    // Set the bot's stand state (sit / stand / sleep / kneel etc.). The
    // raw byte mirrors UnitStandStateType. ServerRefused mid-cast since
    // sitting cancels the cast; otherwise idempotent.
    Result set_stand_state(uint8 state);

    // Inventory
    Result use_item_by_entry(uint32 item_entry, ObjectGuid target = ObjectGuid::Empty);
    // use_item_by_slot: same effect resolution as by-entry but addresses the
    // item by bag/slot. Used when AI rules pre-pick a specific stack (e.g.
    // a particular potion variant when several share an entry id is rare,
    // but bag/slot avoids ambiguity).
    Result use_item_by_slot(uint8 bag, uint8 slot, ObjectGuid target = ObjectGuid::Empty);

    // Loot a creature corpse: pick up gold + auto-store all items the bot has
    // permission to take. Skips group-loot blocks (Need/Greed in flight). The
    // caller is responsible for moving the bot into loot range first.
    Result loot_corpse(ObjectGuid corpse);

    // Vendor / repair
    // sell_item_by_entry: sells one stack of the given item to the nearby
    // vendor implied by `npc`. Caller must ensure the bot is near the vendor;
    // we do not move the bot.
    Result sell_item_by_entry(ObjectGuid npc, uint32 item_entry);
    // sell_item_by_slot: sells `count` units (0 = all) of the item at the
    // bag/slot to the vendor. Mirrors the live HandleSellItemOpcode path
    // with vendor-interact gating and CanSellItemToVendor pre-check.
    Result sell_item_by_slot(ObjectGuid npc, uint8 bag, uint8 slot, uint8 count = 0);
    Result repair_all(ObjectGuid npc, bool from_guild_bank);
    // Sell every grey-quality (Poor) item in bags to the named vendor. Used
    // by the "sell" whisper command and by the inventory-pressure rule.
    Result sell_trash(ObjectGuid npc);
    // Buy `count` of the item at vendor slot `vendor_slot` from `npc`. The
    // item id is read from the vendor's slot list (no client-side hint
    // needed). Auto-stores into the first available bag slot. Returns
    // InvalidTarget when the vendor isn't in interact range / lacks the
    // VENDOR flag, OutOfRange when the slot is past the vendor's list,
    // ServerRefused on inventory full / not enough money / item conditions.
    Result vendor_buy_by_slot(ObjectGuid npc, uint32 vendor_slot, uint32 count = 1);
    // Buy `count` units of `item_entry` from `npc`. Walks the vendor's slot
    // list to find the matching entry, then issues BuyItemFromVendorSlot.
    // Returns OutOfRange when the vendor doesn't sell that item; otherwise
    // same Result envelope as vendor_buy_by_slot.
    Result vendor_buy_by_entry(ObjectGuid npc, uint32 item_entry, uint32 count = 1);
    // Equip an item from `from_bag/from_slot` into the equipment slot
    // `to_slot` (0=Head, 1=Neck, ..., 17=Ranged etc — see EquipmentSlots).
    // The item already at the destination is swapped back into the source.
    // Validates source-item presence and unequip-ability of the destination.
    // Returns InvalidTarget when the source slot is empty, Locked when the
    // destination can't be unequipped (cursed gear / quest item).
    Result equip_item(uint8 from_bag, uint8 from_slot, uint8 to_slot);

    // ---- Group loot --------------------------------------------------
    // Cast the bot's vote on a master/group loot roll. `loot_object` is
    // the loot source guid (creature corpse / GO chest), `loot_list_id`
    // is the per-item index inside the loot, `vote_type` is 0=Pass,
    // 1=Need, 2=Greed, 3=Disenchant. Returns Locked when no roll is
    // active on that (loot_object, list_id) — caller race or roll already
    // finished. Caller is expected to have surfaced the roll via either
    // event hook or future snapshot enrichment.
    Result loot_roll(ObjectGuid loot_object, uint8 loot_list_id, uint8 vote_type);

    // ---- NPC / world interaction --------------------------------------
    // Open the NPC's gossip dialog. Mirrors CMSG_GOSSIP_HELLO: validates
    // interaction range and faction visibility, runs OnGossipHello, then
    // populates the player's GossipMenu with available options. Subsequent
    // gossip_select_by_index calls operate on this menu. Returns
    // InvalidTarget when the npc isn't a Creature with the GOSSIP flag in
    // interact range.
    Result interact_with_npc(ObjectGuid npc);
    // Select a previously-shown gossip option by its 0-based OrderIndex.
    // Requires interact_with_npc to have been called first this session
    // (the menu is per-player and persists until close). Returns Locked
    // when no menu is open or `order_index` is out of range, ServerRefused
    // if the underlying script rejects the selection.
    Result gossip_select_by_index(ObjectGuid npc, uint32 order_index);
    // Click a usable game object (chest, herb, ore vein, quest object).
    // Mirrors CMSG_GAMEOBJECT_USE: respects the bot's mounted state and
    // the GO's UsableMounted flag. Returns InvalidTarget when the GO isn't
    // in interact range.
    Result use_game_object(ObjectGuid go);

    // ---- Quest --------------------------------------------------------
    // Accept the given quest from the named quest-giver (Creature, GameObject,
    // or Item). The bot must be in interact range and CanTakeQuest must
    // return true. Returns InvalidTarget on missing giver / unknown quest,
    // Locked if the quest log is full or eligibility checks fail.
    Result accept_quest(ObjectGuid quest_giver, uint32 quest_id);
    // Hand in a complete quest. `reward_choice` selects from the quest's
    // choice-item array (0 = first choice). Pass 0xFF (sentinel) to let
    // the API auto-pick the best reward via ScoreQuestReward — equippable
    // upgrade for the bot's slot beats vendor value beats nothing. Quests
    // with only fixed rewards ignore the choice entirely.
    // Returns Locked when the quest isn't complete or the choice is out
    // of range.
    static constexpr uint32 kRewardChoiceAuto = 0xFF;
    Result complete_quest(ObjectGuid quest_giver, uint32 quest_id, uint32 reward_choice);
    // Drop the quest from the bot's quest log. Used by stuck/obsolete
    // cleanup. Returns NotKnown when the quest isn't in the log.
    Result abandon_quest(uint32 quest_id);

    // ---- LFG ---------------------------------------------------------
    // Queue the bot for the given dungeon/raid via the Dungeon Finder. The
    // role mask is the lfg::PLAYER_ROLE_* bits (Tank=2, Healer=4, Damage=8;
    // Leader=1 if soloing into the queue). Idempotent — already-queued bot
    // returns Ok. The LFGMgr handles role-check and proposal flow once the
    // queue fills. Returns Locked when in a battleground or already in a
    // dungeon, ServerRefused on lockout / cooldown / invalid dungeon.
    Result lfg_queue(uint32 dungeon_id, uint8 roles);
    // Pull the bot out of the LFG queue (or proposal). Idempotent — bot
    // not in queue returns Ok.
    Result lfg_leave_queue();

    // ---- Talents -----------------------------------------------------
    // Apply Blizzard's curated "starter build" trait config for the bot's
    // current spec. Mirrors HandleClassTalentsSetStarterBuildActive's
    // active-spec branch: builds a new TraitConfig, populates it via
    // TraitMgr::InitializeStarterBuildTraitConfig, flags it as
    // StarterBuild, then commits via Player::UpdateTraitConfig with cast
    // time. Lets a freshly-spawned bot get a usable rotation immediately
    // without us hand-curating per-spec talent loadouts. Returns Locked
    // when in combat (talents are combat-gated server-side), NotKnown
    // when the bot has no active combat trait config (shouldn't happen
    // post-login, but guards against partial init).
    Result apply_starter_talents();

    // ---- Battleground / Arena ----------------------------------------
    // Queue the bot for a non-rated battleground OR a non-rated arena
    // skirmish. The battlemaster_npc must be in interact range (empty
    // GUID = queue-from-anywhere). `bg_type_id` is BattlemasterList.dbc
    // id (e.g. AV=1, WSG=2, RB=32 random; arenas 4=Nagrand, 6=AllArenas,
    // 8=RuinsOfLordaeron). `arena_type` selects the queue category:
    //   0       — battleground. Mirrors HandleBattlemasterJoinOpcode's
    //             solo/group-leader path.
    //   2/3/5   — arena skirmish (2v2/3v3/5v5). Mirrors
    //             HandleBattlemasterJoinArena: requires the bot to be the
    //             leader of an arena_type-sized group; routes through the
    //             Arena queue id (BGQueueTypeId Type=Arena, rated=false).
    // Refuses when already in BG/queue/deserter/no-free-queue-slot, or
    // (arena) when not the leader of a correctly-sized group. Returns
    // InvalidTarget for missing battlemaster, NotKnown for an invalid
    // bg_type_id, Locked when ineligible, ServerRefused on join failure.
    Result bg_queue(ObjectGuid battlemaster, uint16 bg_type_id, uint8 arena_type = 0);
    // Leave the bot's current battleground (when inside) or pull out of
    // any battleground queue (when waiting). Mirrors
    // HandleBattlefieldLeaveOpcode plus per-queue removal. Returns Locked
    // when the bot is mid-combat in a non-end-state BG.
    Result bg_leave();

    // ---- Auction House -----------------------------------------------
    // Post a single non-stackable item up for auction. The item must be
    // in the bot's inventory, tradeable, non-commodity (max stack 1),
    // non-conjured, no expiration. Either min_bid OR buyout must be > 0,
    // both must be silver-aligned (% 100 == 0), and run_time_minutes
    // must be one of {12*60, 24*60, 48*60} (1/2/4 day auctions). The
    // bot pays the deposit up-front. Returns InvalidTarget for a missing
    // auctioneer, NotKnown when the item guid isn't in the bot's bags,
    // Locked when the item refuses auction (soulbound, conjured, has
    // expiration, is a non-empty bag), NotEnoughResource when the deposit
    // exceeds the bot's gold.
    Result auction_sell_item(ObjectGuid auctioneer, ObjectGuid item_guid,
                             uint64 min_bid, uint64 buyout, uint32 run_time_minutes);
    // Cancel one of the bot's own auctions. If the auction has a bidder,
    // a 5% cancel fee is debited from the bot and the bid is refunded to
    // the bidder via mail. Returns InvalidTarget for a missing auctioneer,
    // NotKnown when the auction id isn't owned by the bot, NotEnoughResource
    // when the cancel fee exceeds the bot's gold.
    Result auction_cancel(ObjectGuid auctioneer, uint32 auction_id);

    // ---- Auction House BUY-side (#4B) --------------------------------
    // Buy out an existing auction at its full buyout price. Mirrors the
    // buyout branch of WorldSession::HandleAuctionPlaceBid (bid amount ==
    // BuyoutOrUnitPrice). Validates: auctioneer is an interactable
    // AUCTIONEER NPC, the auction still exists in that faction house and is
    // a non-commodity item auction, the bot is NOT the owner, the auction
    // actually has a buyout, and the bot has enough gold. On success the
    // gold is debited, sold/won mail is sent to seller/buyer, and the
    // auction is removed. Returns InvalidTarget (no auctioneer), NotKnown
    // (auction missing / commodity / no buyout), Locked (own auction),
    // NotEnoughResource (insufficient gold).
    //
    // `max_price` is a caller-supplied server-side guard (copper): if the
    // live auction's buyout exceeds it, the purchase is refused with
    // Locked rather than silently overpaying. The economy rule carries the
    // snapshot-observed buyout here so a listing that was re-priced UP
    // between the on-demand AH scan and execution can't drain more gold
    // than the bot agreed to. Pass 0 to disable the guard (buy at any
    // price the bot can afford).
    Result auction_buyout(ObjectGuid auctioneer, uint32 auction_id, uint64 max_price = 0);

    // Buy a quantity of a COMMODITY (stackable trade good — herbs, ore,
    // cloth, gems, most craft reagents). Modern WoW routes stackable goods
    // through a separate buy path than single-item auctions: a quote is
    // created over the cheapest current listings (GetCommodityQuote), then
    // the purchase is committed (BuyCommodity), filling `quantity` units
    // from the cheapest sellers and mailing them to the bot. Mirrors
    // WorldSession::HandleAuctionGetCommodityQuote + HandleAuctionBuyCommodity,
    // but quote+buy run atomically here (no client confirm round-trip).
    //
    // Validates: auctioneer is an interactable AUCTIONEER NPC, the item is a
    // commodity with enough quantity listed (by non-self sellers), the quote
    // total is <= `max_total_price` (caller's slippage guard; 0 disables it),
    // and the bot HasEnoughMoney for the quote total. On success the gold is
    // debited, the items are mailed to the bot, and the consumed listings are
    // removed. Returns InvalidTarget (no auctioneer), NotKnown (no item
    // template / not enough listed / no quote), Locked (quote exceeds
    // max_total_price), NotEnoughResource (insufficient gold), Other (no bot
    // / commit failure).
    Result auction_buy_commodity(ObjectGuid auctioneer, uint32 item_entry,
                                 uint32 quantity, uint64 max_total_price = 0);
    // Place a bid on an existing non-commodity auction. Mirrors the bid
    // branch of HandleAuctionPlaceBid. `bid` is copper, must be
    // silver-aligned and >= the auction's current minimum (BidAmount +
    // CalculateMinIncrement, or MinBid for the first bid). Refunds a
    // prior bidder via mail. Returns InvalidTarget (no auctioneer),
    // NotKnown (auction missing / commodity), Locked (own auction /
    // bid not silver-aligned / below minimum), NotEnoughResource
    // (insufficient gold). A bid equal to the buyout completes the
    // purchase (same as auction_buyout).
    Result auction_bid(ObjectGuid auctioneer, uint32 auction_id, uint64 bid);

    // ---- Craft-order fulfilment (#4B-2(a)) ---------------------------
    // Fulfil a CLAIMED bot-to-bot craft order: the bot must KNOW `spell_id`
    // (its own claimed recipe), must hold the recipe's reagents for `qty`
    // crafts, and the produced `item_entry` is mailed to `requester_low`. This
    // method performs the CRAFT MECHANICS + DELIVERY ONLY and reports success;
    // the ESCROW RELEASE is done by the (module-side) caller via
    // CraftOrderBoard::MarkDelivered on an Ok return, so the core-side API has
    // no module dependency and the board stays the single escrow authority. A
    // failed craft (missing reagents / unknown recipe / inventory full / mail
    // refusal) returns a non-Ok Result so the caller does NOT mark delivered,
    // leaving the order Claimed to retry or time out (escrow stays held).
    // `order_id` is accepted only for logging/diagnostic context here.
    // Reagents are consumed server-side (DestroyItemCount) and the product is
    // created via StoreNewItem, mirroring a tradeskill cast without the client
    // cast pipeline (bots are clientless). Returns NotKnown (recipe not in
    // spellbook / not a create-item recipe), NotEnoughResource (missing
    // reagents), InvalidTarget (requester not a valid recipient), InventoryFull
    // (no room to stage the product), Other (no bot / mail failure).
    Result craft_fulfill_order(uint64 order_id, uint32 spell_id, uint32 item_entry,
                               uint32 qty, uint64 requester_low);

    // ---- Gold-cost estimators (#4B affordability gates) --------------
    // Read-only helpers economy / vendor rules call from their gate
    // functions to test "can the bot afford this?" BEFORE emitting an
    // intent (discovery flagged unbudgeted vendor phases that emitted
    // buys the bot couldn't pay for, churning ServerRefused). None of
    // these mutate the bot. All amounts are copper.
    //
    // Total cost to repair every damaged equipped + bagged item at the
    // standard (un-discounted) rate. Mirrors the sum in
    // Player::DurabilityRepairAll without taking the money. 0 when nothing
    // needs repair.
    uint64 gold_cost_estimate_repair_all() const;
    // Total cost to buy enough of `reagent_entry` from a vendor to reach
    // `desired_total` units in the bot's bags, priced at the item's
    // template BuyPrice. Returns 0 when the bot already holds
    // `desired_total` or the item has no buy price (vendor-untradeable).
    uint64 gold_cost_estimate_reagent_buy(uint32 reagent_entry, uint32 desired_total) const;
    // The live buyout price (copper) for `auction_id` in `auctioneer`'s
    // faction house, or 0 when the auction is missing, is a commodity, is
    // owned by the bot, or has no buyout. Lets a buy-side rule confirm the
    // snapshot's cached buyout against the live auction before emitting.
    uint64 gold_cost_estimate_ah_buyout(ObjectGuid auctioneer, uint32 auction_id) const;

    // ---- Taxi --------------------------------------------------------
    // Discover the flight master's node and add it to the bot's taxi mask
    // (so subsequent fly_to calls accept the destination). Mirrors the
    // CMSG_ENABLE_TAXI flow that fires when the player first talks to a
    // flight master. Idempotent — already-discovered nodes return Ok.
    // Returns InvalidTarget when the npc isn't a flight master in interact
    // range, NotKnown when the flight master sits at no recognised taxi
    // node (data anomaly).
    Result discover_taxi_node(ObjectGuid flight_master);
    // Activate the flight from the named flight master to `to_node` (a
    // TaxiNodes.dbc id). The full multi-hop path is computed by
    // TaxiPathGraph::GetCompleteNodeRoute, so the bot can hop between
    // continents in a single command. Returns InvalidTarget for a missing
    // flight master, OutOfRange if no taxi node sits near the flight
    // master, NotKnown when `to_node` doesn't exist, Locked when the bot
    // hasn't visited either endpoint yet, NotEnoughResource on cost.
    Result fly_to_node(ObjectGuid flight_master, uint32 to_node);

    // ---- Hearth bind -------------------------------------------------
    // Set the bot's hearthstone home to the innkeeper's location. Mirrors
    // CMSG_BINDER_ACTIVATE: validates the NPC has the INNKEEPER flag and
    // is in interact range, then casts spell 3286 (Bind) from the NPC on
    // the bot to perform the actual homebind update. Returns InvalidTarget
    // when the NPC isn't an innkeeper in range, Locked in instances (the
    // server refuses bind in instanceable maps) or while dead.
    Result bind_homebind(ObjectGuid innkeeper);

    // ---- Bank --------------------------------------------------------
    // bank_deposit_item: move one item from inventory bag/slot into the
    // character bank. Auto-stores using NULL_BAG/NULL_SLOT, mirroring the
    // client's right-click-to-bank flow (CMSG_AUTO_BANK_ITEM). The banker
    // NPC must be in interact range. Returns InvalidTarget for a missing
    // banker, NotKnown when the source slot is empty, InventoryFull when
    // every bank slot can hold the stack but only by overflowing into an
    // already-full bag, Locked when the source item refuses bank storage
    // (soulbound-to-other characters, NoStore flag, etc.).
    Result bank_deposit_item(ObjectGuid banker, uint8 from_bag, uint8 from_slot);
    // bank_withdraw_item: inverse — pulls a bank-side item back into the
    // bot's inventory. `from_bag/from_slot` identify the item as it lives
    // in the bank slots (Player::IsBankPos returns true). Returns
    // InvalidTarget when the source isn't actually in bank storage.
    Result bank_withdraw_item(ObjectGuid banker, uint8 from_bag, uint8 from_slot);

    // ---- Guild bank --------------------------------------------------
    // The guild bank is a GameObject (GAMEOBJECT_TYPE_GUILD_BANK), not an
    // NPC. The four operations below all gate on
    // Player::GetGameObjectIfCanInteractWith(banker, GUILD_BANK) and
    // require the bot's guild membership. The Guild::HandleMember* API
    // performs the full money-cap / withdrawal-rights / cash-flow logging
    // chain matching the gossip path. Returns InvalidTarget when the bot
    // isn't in a guild or the banker GO isn't reachable; NotEnoughResource
    // for under-funded deposit; Locked when the bot lacks rank-permission
    // for the requested action (gold withdraw cap, item-tab access).
    Result guild_bank_deposit_money(ObjectGuid banker, uint64 amount);
    Result guild_bank_withdraw_money(ObjectGuid banker, uint64 amount);
    // guild_bank_deposit_item: walk an inventory item into a specific
    // bank tab+slot. `count` of 0 moves the entire stack; non-zero splits.
    // Mirrors HandleAutoGuildBankItem (Guild::SwapItemsWithInventory with
    // toChar=false). Returns Locked on rank/tab denial, InventoryFull when
    // the destination slot is occupied by a non-mergeable item.
    Result guild_bank_deposit_item(ObjectGuid banker, uint8 tab, uint8 bank_slot,
                                   uint8 player_bag, uint8 player_slot, uint32 count);
    // guild_bank_withdraw_item: inverse direction. Same gating; respects
    // the per-tab withdrawal limit (rank's GUILD_BANK_RIGHT_VIEW_TAB +
    // remaining-slots-today counter).
    Result guild_bank_withdraw_item(ObjectGuid banker, uint8 tab, uint8 bank_slot,
                                    uint8 player_bag, uint8 player_slot, uint32 count);

    // ---- Quest sharing -----------------------------------------------
    // Push the bot's active quest to every group member that can take it.
    // Mirrors HandlePushQuestToParty: walks Group::GetMembers, runs the
    // full SatisfyQuest* gate per receiver (level/class/race/rep/log-full/
    // already-on/already-done), sends QuestPushReason responses, and for
    // eligible receivers triggers their quest-share confirmation popup via
    // PlayerTalkClass->SendQuestGiverQuestDetails. Returns InvalidTarget
    // when the bot doesn't own that quest, NotKnown when the quest_id has
    // no template, OutOfRange when the bot has no group, ServerRefused
    // when CanShareQuest refuses (quest non-shareable). Ok regardless of
    // how many receivers actually accepted — the per-recipient outcome
    // is reported by the live SendPushToPartyResponse path.
    Result share_quest_with_party(uint32 quest_id);

    // ---- Trainer -----------------------------------------------------
    // Buy/learn a single spell from a class or profession trainer NPC.
    // Resolves the trainer's spell list by looking up the creature's
    // default trainer id (sObjectMgr->GetCreatureDefaultTrainer); the
    // trainer object then performs the full eligibility check (level,
    // skill rank, prereq ability, primary-profession slot) and gold-cost
    // debit. Idempotent — already-known spells return Ok without re-pay.
    // Returns InvalidTarget when the npc isn't a trainer in interact
    // range, NotKnown when the npc has no trainer template registered,
    // ServerRefused when the trainer rejects the buy (level/skill/cost
    // failure — the trainer-side reason is logged but not surfaced).
    Result trainer_buy_spell(ObjectGuid trainer_npc, uint32 spell_id);
    // Iterate the trainer's spell list and buy every spell the bot is
    // currently eligible for. Returns Ok if at least one was learned (or
    // every spell was already known); ServerRefused when the trainer has
    // entries but none could be bought (insufficient funds, level/skill
    // gaps); maps the trainer-resolution failures the same way as
    // trainer_buy_spell. Counts learned + already-known + skipped via
    // out-params for caller diagnostics.
    Result trainer_buy_all_available(ObjectGuid trainer_npc,
                                     uint32* out_learned = nullptr,
                                     uint32* out_already = nullptr,
                                     uint32* out_skipped = nullptr);

    // ---- Mail --------------------------------------------------------
    // All four mail commands take the mailbox guid (a GameObject of type
    // MAILBOX or a Creature with MAILBOX npc-flag) and validate
    // interaction range up-front; returns InvalidTarget if the bot can't
    // reach the mailbox. The bot's actual mail vector lives on Player and
    // is exposed read-only via the snapshot, so callers pick the message
    // id + attachment guid from the snapshot before issuing the command.
    //
    // mail_take_money: pulls the money attachment off the mail (if any)
    // and credits the bot. Returns Locked when the mail is undelivered or
    // already drained, NotEnoughResource when the bot is over the gold cap.
    Result mail_take_money(ObjectGuid mailbox, uint64 mail_id);
    // mail_take_item: pulls one specific attached item (identified by its
    // low-guid from the snapshot) into the bot's inventory. For COD mails
    // this debits the COD value from the bot and forwards it to the sender.
    // Returns InventoryFull when no bag slot can hold the item,
    // NotEnoughResource when COD exceeds the bot's gold, Locked when the
    // mail is undelivered or doesn't carry that attachment.
    Result mail_take_item(ObjectGuid mailbox, uint64 mail_id, uint64 item_guid_low);
    // mail_delete: marks the mail for deletion. The server's mail-tick
    // flushes it from the database. Refuses to delete COD-bearing mails to
    // mirror the client (COD must be paid or returned). Returns Locked on
    // such mails, NotKnown if the mail id isn't in the bot's mail list.
    Result mail_delete(ObjectGuid mailbox, uint64 mail_id);

    // ---- Movement (extras) -------------------------------------------
    // Hop forward `forward` yards in the bot's facing direction. Used by
    // unstuck routines and for navigation over short ledges. Default ~7yd
    // matches a player Space-bar jump arc. Returns Locked when in combat
    // or already mid-air; ServerRefused on bad terrain.
    Result jump(float forward = 7.0f);

    // ---- Raw Player* escape hatch (use sparingly) --------------------
    // Some V2 paths need direct Player* access — currently the guild
    // charter intents (BuyGuildCharterIntent / SignGuildCharterIntent /
    // TurnInGuildCharterIntent) call into Fleet/BotGuildCharter.cpp's
    // helpers which mirror TC's PetitionsHandler. Going through the
    // typed API surface for these would require duplicating ~120 lines
    // of TC handler logic in PlayerbotAPI.cpp; exposing the underlying
    // pointer is the smaller maintenance burden. Per CONTRACTS.md the
    // bot still runs on the world thread so this is safe; just don't
    // capture it across ticks.
    Player* player() const { return p_; }

private:
    // Terrain-follow movement fallback for navmesh HOLES. When move_to's Detour
    // path fails (NoPath / FarFromPolyEnd) for a SHORT, NEAR-LEVEL destination,
    // validate the straight line is walkable ground via the continuous .map
    // heightfield and, if so, issue a straight terrain-skim across the gap.
    // Returns true if a fallback move was issued. Strictly gated (short range,
    // no cliff/wall Z-jumps, no lava/deep water) so it can't reintroduce the
    // wall-walking that made us disable TC's blanket straight-line fallback.
    bool TryTerrainWalkFallback(float x, float y, float z, bool run);

    // BG start-platform / graveyard STEP-DOWN. When a battleground bot is stuck
    // on an elevated, OVERHANGING start/respawn platform whose navmesh doesn't
    // connect to the arena floor (EotS), relocate it onto the field navmesh poly
    // just in front of the platform (toward the objective, at field height) — the
    // elevator-disembark pattern — then normal routing resumes. A short, designed
    // platform→field step (BG + downward only). Returns true if a step issued.
    bool TryBgDescentCrawl(float x, float y, float z, bool run);

    Player* p_;
};

} // namespace Playerbot
