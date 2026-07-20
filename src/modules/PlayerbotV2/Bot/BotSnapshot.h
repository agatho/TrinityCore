// BotSnapshot - Per-bot immutable snapshot. Built on world thread, consumed
// read-only by AI workers. See CONTRACTS.md §2.1.
//
// First-iteration shape: includes the load-bearing fields needed by the initial
// state machine (Idle, LoggingIn, Travelling) and APL evaluator. Additional
// fields are appended (never reordered) per FEATURE_MATRIX.md as subsystems
// land.

#pragma once

#include "BotTypes.h"
#include "FlatIndex.h"
#include "ObjectGuid.h"
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace Playerbot {

// Sentinel for "no map" in travel fields. Map id 0 is a REAL map (Eastern
// Kingdoms), so 0 must NOT mean "unset" — doing so silently broke every
// ship/zeppelin trip whose destination is EK (the boarding filter dropped
// dest==0 as "no destination", so no bot could ever sail to Stormwind/Ironforge/
// Undercity). Use 0xFFFFFFFF for "none" and let 0 be a valid destination.
inline constexpr uint32 kInvalidMapId = 0xFFFFFFFFu;

struct AuraEntry
{
    uint32     spell_id;
    uint8      stacks;
    Ms         remaining;
    ObjectGuid caster;
    DispelType dispel_type;
    // Mirrors SpellInfo::Mechanic (e.g. MECHANIC_FEAR=5, MECHANIC_STUN=12).
    // Lets predicates reason about CC effects without hard-coding spell IDs:
    // Tremor Totem reacts to MECHANIC_FEAR; PvP trinkets gate on stun/root,
    // etc. Zero (MECHANIC_NONE) when the spell carries no mechanic flag.
    uint32     mechanic;
    bool       is_harmful;
    bool       is_stealable;
};

struct CooldownEntry
{
    uint32 spell_id;
    Ms     remaining;
    uint8  charges;
    uint8  max_charges;
};

// Compact stat snapshot per item. Indexed by StatIndex (Str=0..Speed=11).
// Populated from Item::GetItemStatType / Item::GetItemStatValue in the
// builder so the equip-upgrade rule can score without round-tripping
// ItemTemplate on the worker thread. Includes weapon DPS for weapons —
// 0 for non-weapons. Bonding + soulbound flag drive vendor-sell
// decisions (BoP can't be traded; BoE worse-than-equipped can be sold).
struct ItemStatBlock
{
    std::array<int16, 12> stats{};   // 12 = StatIndex::Count; kept in lock-step with StatPriority.h
    uint16 weapon_dps_x10 = 0;       // (min+max)/2 / delay_seconds, scaled x10
    uint8  bonding        = 0;       // ItemTemplate::GetBonding(): 0=NONE, 1=ON_ACQUIRE, 2=ON_EQUIP, 3=ON_USE, 4=QUEST
    bool   is_soulbound   = false;   // Item::IsSoulBound() — true for BoA/BoP after binding
};

struct EquippedItem
{
    uint32 entry;
    uint16 item_level;
    uint8  durability_pct;
    // First ON_USE spell from ItemTemplate.Effects, or 0 when the item has
    // no on-use effect (most weapons/armor; trinkets / engineering tinkers
    // typically carry one). Resolved in the builder so AI can call
    // is_ready(on_use_spell_id) without dragging ItemTemplate into the
    // worker thread. Used by InCombat for trinket auto-fire on cooldown.
    uint32 on_use_spell_id = 0;
    // Stat block for the equipped item. Lets the equip-upgrade rule
    // compute score(equipped[slot]) without re-resolving the live Item*.
    ItemStatBlock stats{};
};

struct InventoryItem
{
    ObjectGuid guid;        // Item guid — required for item-targeted spells
                            // (disenchant 13262 / prospect 31252 / mill 51005).
    uint8  bag;
    uint8  slot;
    uint32 entry;
    uint16 count;
    uint8  durability_pct;
    bool   is_quest_item;
    // Item level (0 for non-equippable / non-leveled). Lets the auto-equip
    // rule compare upgrades against the currently-equipped item without
    // re-resolving the live Item* on the AI worker thread.
    uint16 item_level;
    // Target equipment slot (EQUIPMENT_SLOT_HEAD .. RANGED) the bot would
    // wear this item in. Resolved by Player::FindEquipSlot in the builder
    // — handles cursed-cant-be-removed, dual-wield restrictions, etc.
    // 0xFF when the item isn't equippable for this bot (wrong class,
    // wrong armour subclass, level requirement unmet, or no slot fits).
    uint8  equip_slot;
    // ItemTemplate::GetQuality() — POOR=0 / COMMON=1 / UNCOMMON=2 / RARE=3 /
    // EPIC=4 / LEGENDARY=5 / ARTIFACT=6 / HEIRLOOM=7. Auto-equip uses this
    // to skip swapping an equipped green for a higher-ilvl grey, which can
    // happen on fresh bots with mixed loot drops.
    uint8  quality;
    // ItemTemplate::GetClass()/GetSubClass(). Lets level-agnostic rules
    // (consume-food, consume-potion, use-bandage) walk the bag for any
    // class+subclass match without hard-coding TWW expansion item IDs.
    // Low-level bots get to use BC food etc. without the rule going
    // looking for a Conjured Mana Cake the mage hasn't trained yet.
    uint8  item_class;
    uint8  item_subclass;
    // ItemTemplate::GetContainerSlots() for ITEM_CLASS_CONTAINER items,
    // 0 otherwise. Drives the bag-upgrade rule (idle:equip_bag_upgrade):
    // containers are NOT regular equip candidates (equip_slot is clamped
    // to 0xFF for them — see B-11), so capacity is the only signal the
    // rule needs to rank a looted bag against the equipped ones.
    uint8  container_slots;
    // Stat block + bonding for the bag item. Populated by builder for
    // every equippable item (equip_slot != 0xFF) so the upgrade rule can
    // score against the matching equipped slot.
    ItemStatBlock stats{};
};

// One enumerable objective from `Quest::Objectives`. Captured per-tick so AI
// can decide which one to chase next without round-tripping the world thread
// for every (quest, obj) pair. `type` matches QuestObjectiveType (KILL=0 /
// ITEM=1 / GAMEOBJECT=2 / TALKTO=3 / AREATRIGGER=10 etc); `object_id` is the
// creature_entry / item_entry / go_entry / areatrigger_entry referenced.
// `flags` carries QUEST_OBJECTIVE_FLAG_* (HIDDEN, SEQUENCED, OPTIONAL) so
// rules can skip hidden/optional ones and respect sequencing. `storage_index`
// is the per-quest slot the server writes progress into (used to map back to
// the QuestObjective in callbacks if needed).
struct QuestObjectiveEntry
{
    uint32 id;             // QuestObjective::ID (db row id)
    uint32 quest_id;       // QuestObjective::QuestID (redundant convenience)
    uint8  type;           // QuestObjectiveType enum
    int8   storage_index;
    int32  object_id;      // creature/item/go entry, or amount-typed value
    int32  amount;         // required count
    int32  progress;       // current count from Player::GetQuestObjectiveData
    uint32 flags;          // QuestObjectiveFlags
    // KillCredit aliases: extra creature_template entries whose deaths
    // award credit for `object_id` via CreatureTemplate.KillCredit[0..2].
    // Populated for MONSTER objectives where the credit creature itself
    // has aliases registered. The kill rule walks both `object_id` and
    // every entry in this list when searching nearby_enemies. Empty for
    // typical "kill 6 wolves" quests where wolves credit themselves.
    std::vector<uint32> credit_alias_entries;
    // Label-tagged creature entries: for KILL_WITH_LABEL (type 21) the
    // `object_id` is a creature_label; the actual targets are every
    // creature_template carrying that label. Resolved via DB2 reverse-
    // index. Empty for non-type-21 objectives.
    std::vector<uint32> labeled_target_entries;
    // MONSTER objective whose target creature is FRIENDLY to the bot —
    // a "Speak with X" quest implemented as monster-credit via a gossip
    // script (extremely common starter-chain pattern, e.g. Forsaken
    // 24960 "The Wakening": speak with three Risen Dead). The bot cannot
    // attack the target (it never appears in nearby_enemies), so the
    // KILL rule can never act and the objective starves forever
    // (observed: bot at L2 for 150 played-hours, wandering beside its
    // quest NPCs). Execution must route through the TALK/gossip rule —
    // OnGossipHello / the menu script fires the KillCredit spell.
    bool talk_credit = false;
    // USE-START-ITEM objective (2026-06-20): an objective-less quest whose
    // completion comes from USING the quest's provided StartItem on a target at the
    // quest POI (the item's spell credits a hidden KillCredit / completes the quest
    // server-side). E.g. Q26118 "Seize the Ambassador": use the sledgehammer (56837)
    // on Ambassador Slaghammer. These quests have NO quest_objectives rows, so the
    // builder synthesizes this objective (object_id = the StartItem entry, POI = the
    // quest's QuestPOI centroid) and idle:quest_use_item_on_target executes it.
    bool use_start_item = false;
};

struct QuestEntry
{
    uint32 quest_id;
    uint8  state;          // 0=incomplete, 1=complete (matches QUEST_STATUS_COMPLETE)
    uint16 level;          // Quest::GetQuestLevel
    uint16 min_level;      // Quest::GetMinLevel
    uint32 xp_reward;      // Quest::XPValue(player) snapshot
    uint8  flags;          // bit0=repeatable, bit1=daily, bit2=weekly, bit3=dungeon, bit4=raid, bit5=group
    std::vector<QuestObjectiveEntry> objectives;
    // Builder marks true when any MONSTER-type objective references a
    // creature entry with zero spawns in `creature` — i.e. the kill credit
    // is script-only (pet-battle quests, phase-locked quests, removed
    // legacy creatures). Bots can never advance these objectives, so
    // State_Idle auto-abandons them and the offer-filter rejects them
    // before they can be re-accepted.
    bool   unachievable = false;
    // Builder marks true when the quest is structurally UNTURNABLE junk:
    // zero objectives AND no creature/GO questender (e.g. 55660 "Time
    // Trials"), or on the explicit quest blacklist. Unlike `unachievable`,
    // this is safe to abandon even when state==1 (complete) because the
    // quest can never yield a reward and only poisons the picker. See
    // IsBotJunkQuest in BotSnapshotBuilder.cpp.
    bool   unturnable = false;
    // Quest::SourceItemId — the item granted by the questgiver on
    // accept (e.g. "Soldier's Bandage" for Northshire's heal-the-
    // wounded quest). Drives the "use quest item on friendly NPC"
    // rule path: when current_objective.type==MONSTER and the target
    // entry shows up in nearby_friends rather than nearby_enemies,
    // and the bot still has source_item_id in inventory, the bot
    // walks to the friendly NPC and emits UseItemByEntryIntent
    // targeting it. Zero for ordinary kill quests.
    uint32 source_item_id = 0;
    // Quest-ender NPC spawn coordinates. Resolved by the builder via
    // sObjectMgr->GetCreatureQuestInvolvedRelationReverseBounds(quest_id)
    // → first spawned creature → first sCreatureSpawnDataStore entry.
    // Used by idle:walk_to_quest_ender when state == 1 (complete) AND
    // the snapshot's nearby quest_turnins[] doesn't contain the ender
    // (because it's beyond the 40y snapshot scan range). Without this
    // a bot with a complete deliver-quest can't navigate toward the
    // far-away turn-in NPC and wanders aimlessly.
    //
    // `ender_resolved` = true when a spawn lookup succeeded. False
    // when no creature template, no spawned instance, or only-WorldSafeLocs
    // rows (rare — most enders are creatures). When false the rule
    // skips this quest.
    bool   ender_resolved = false;
    uint32 ender_map_id   = 0;
    float  ender_x        = 0.f;
    float  ender_y        = 0.f;
    float  ender_z        = 0.f;
};

// Compact mail row populated from Player::GetMails(). Per-item attachment guids
// live in `item_guid_lows` (LowType so the snapshot stays POD). Holding the
// money / item count separately lets predicates ask "is there free gold to
// pick up?" without iterating items_count > 0 means take-item intents apply.
struct MailEntry
{
    uint64                  message_id;
    uint64                  money;
    // Seconds until expire / delivery. Negative deliver_in means already
    // delivered and ready to take. expire_in <= 0 means the mail will be
    // returned/deleted by the server next mail-tick.
    int64                   deliver_in_sec;
    int64                   expire_in_sec;
    uint64                  cod;
    uint16                  item_count;
    uint8                   message_type;   // MailMessageType (0=Normal..9)
    bool                    has_body;       // MAIL_CHECK_MASK_HAS_BODY
    bool                    is_returned;    // MAIL_CHECK_MASK_RETURNED — re-returning rejected
    std::vector<uint64>     item_guid_lows; // item_guid (LowType) per attachment
    // Sender's character LowGUID (Mail::sender). Only meaningful when
    // message_type == 0 (Normal player-sent mail) — for AH / system /
    // calendar / black-market mails the field is an entry id, not a
    // GUID. Drives the "thank the sender" whisper after mail-drain.
    uint64                  sender_low = 0;
};

struct NearbyUnit
{
    ObjectGuid guid;
    uint32     entry;
    uint8      level;
    int32      hp;
    int32      max_hp;
    float      x, y, z;
    // Unit facing yaw (radians). Lets melee bots check whether they're
    // in the target's frontal arc — bosses parry 25% of frontal hits
    // for level-cap players, so non-tank melee should reposition behind.
    float      o = 0.f;
    bool       is_casting;
    uint32     casting_spell_id;
    Ms         cast_remaining;
    bool       is_interruptible;
    // True when the unit's currently-casting spell carries
    // SPELL_EFFECT_INTERRUPT_CAST (Counterspell / Pummel / Kick / etc).
    // Drives the cast-fake rule: bot cancels its own cast just before
    // the kick lands so the interrupter eats the kick on nothing,
    // wasting their CD without locking out the bot's spell school.
    bool       is_casting_kick = false;
    Role       role = Role::Unknown;
    ObjectGuid victim;          // Who this unit is currently attacking (Empty if none).
    // For Creature units: lower 32 bits of UNIT_NPC_FLAG_* (vendor/repair/
    // banker/trainer/flightmaster/innkeeper/etc). Zero for player units.
    // Lets AI rules find capability-bearing NPCs without round-tripping
    // through a per-creature ObjectAccessor lookup.
    uint32     npc_flags = 0;
    // Dungeon-boss flag (creature_template.flags_extra & 0x10000000).
    // Drives the dungeon-engagement and pull-pacing rules — bots
    // distinguish boss kills from trash so they don't pull a boss
    // accidentally during routine clears.
    bool       is_dungeon_boss = false;
    // Creature kill grants NO experience (Creature::CanGiveExperience()
    // == false: CREATURE_STATIC_FLAG_NO_XP / CREATURE_FLAG_EXTRA_NO_XP —
    // training dummies, ambient critters with the flag, event props).
    // Grind/engage rules MUST skip these: a Training Dummy never dies and
    // never grants XP, so a bot that opens on one wedges InCombat forever
    // (observed live: L22 Emashen + L34 Toroek perma-fighting dummies).
    bool       no_xp_kill = false;
    // UNIT_FLAG_PACIFIED (0x20000): the unit cannot attack. Training dummies,
    // event props and friendly-quest captives carry it — they are NEVER a real
    // threat, so threat-pull / engage rules MUST skip them. This is the robust
    // catch for mis-flagged dummies: many "Training Dummy" creatures (e.g.
    // entry 44820 in the Orc Valley of Trials) lack the NO_XP static flag, so
    // no_xp_kill is false and the bot would otherwise pull the immortal dummy
    // and wedge InCombat forever firing at a target that never dies (observed
    // live: Grimfang frozen 2.5min shooting Valley-of-Trials Training Dummy).
    bool       is_pacified = false;
    // UNIT_FLAG_UNINTERACTIBLE (0x02000000, the old NOT_SELECTABLE): players
    // cannot select/target the unit, and Unit::IsValidAttackTarget rejects it —
    // so a bot can NEVER acquire it as a victim. Invisible "stalker" / trigger
    // units carry it. When such a unit is ALSO hostile (mis-authored faction) it
    // enters m_attackers and deals melee/aura damage the bot can do nothing about:
    // every StartAttack returns InvalidTarget, victim() stays empty, and the bot
    // stands in the swarm and dies (observed live 2026-06-26: 48 NOT_SELECTABLE
    // faction-14 "Vanessa Lightning Stalker" 49521 blanket the Deadmines harbor
    // floor; the group gets pinned 230y short of Admiral Ripsnarl and wipes in a
    // loop). Acquire/pull/rotation rules MUST skip these, and the in-combat
    // boss-advance treats them as un-fightable so the tank walks THROUGH them
    // (they leash as it passes) toward the objective instead of standing to die.
    // Set from BOTH the live unit flag and the creature_template flag (mirrors
    // is_pacified: a static template flag may not be applied to the live unit).
    bool       untargetable = false;
    // Creature-only: Creature::CanNotReachTarget() — the mob's OWN
    // ChaseMovementGenerator reports it cannot path to its current target
    // (!isInAccessiblePlaceFor / PATHFIND_NOPATH, maintained per-tick by TC).
    // TRUE precisely during the aggro-but-unreachable window: an aggro pack on
    // a z-disconnected ledge (WC corridor, 2026-07-03) is ordinary targetable
    // creatures — untargetable/is_pacified never encode unreachability — yet
    // any chase/gap-close toward it can never close and only steals movement
    // ownership from dungeon navigation every combat frame. The combat re-aim
    // gates (State_InCombat IsAttackerFightable) key on this. Caveat: this
    // covers the mob-cannot-reach-bot direction (the diagnosed case); the
    // asymmetric bot-cannot-reach-mob case is handled by the pull-gate /
    // disengage machinery, not here. False for players.
    bool       cannot_reach = false;
    // Creature classification is Elite / RareElite / Rare (CreatureClassifications
    // 1/2/4). These are legitimately high-HP, slow-to-kill targets (world bosses,
    // rare spawns, elite quest mobs) — the unkillable-target leash MUST NOT
    // disengage them just because their HP drops slowly. Normal/Trivial mobs and
    // training dummies are classification 0/5, so the leash still applies there.
    bool       is_elite_or_rare = false;
    // Creature-only: CreatureType (Beast=1, Demon=4, Humanoid=7,
    // Undead=8, etc.). 0 for Players (and unspecified creatures).
    // Used by hunter taming rule to filter to tameable Beasts only.
    // (creature_family was populated but had no consumer; dropped
    // per audit a9e0846b8298ae131 #2. Re-add with a real consumer
    // if a future rule needs Wolf/Cat/Spider distinction.)
    uint8      creature_type   = 0;
    // Player-only: Player::GetGroup() != nullptr. Lets the bot-to-player
    // invite rule skip already-grouped players up-front (the server would
    // reject with ERR_ALREADY_IN_GROUP_S; the cooldown absorbs the waste,
    // but pre-filtering keeps the rule cheap and the dispatch slot free
    // for productive emits).
    bool       is_in_group = false;

    // True when the unit is a Player (TYPEID_PLAYER). Without this, bots
    // can't distinguish a hostile player attacker from an angry boar —
    // both trigger normal combat. Real players treat PvP differently:
    // they flee earlier, save trinkets, don't waste long CDs on a duel.
    // Populated by the builder from `unit->GetTypeId() == TYPEID_PLAYER`.
    bool       is_player = false;

    // Player-only: Player::GetGuildId(). 0 for non-players or guildless
    // players. Used by Phase B `idle:guild_recruit_nearby` to identify
    // recruit candidates without an O(N) sGuildMgr lookup per encounter.
    uint64     guild_id = 0;

    // Subset of buff auras this unit carries — restricted to a small
    // whitelist of "affix-relevant" buffs (M+ Bolstering 209859, future
    // similar mechanics). Captured here instead of a generic auras[]
    // vector to keep the snapshot lean: nearby_enemies caps at ~24, so
    // a full aura dump would multiply memory + build cost. The whitelist
    // approach lets per-dungeon scripts query `pull_separately_auras`
    // semantics without bloating every NearbyUnit. Builder fills only
    // ids that appear in `kAffixBuffWhitelist` (see BotSnapshotBuilder).
    std::vector<uint32_t> affix_buffs;

    // Compact CC-state fields. Populated by walking the unit's harmful
    // auras for any with a CC mechanic (stun/fear/poly/sleep/charm/
    // disorient/incapacitate/horror). is_cc_locked is true when ANY
    // such aura is active; cc_caster is the caster of the highest-
    // priority CC (Stun > Poly > Fear > others). Drives the
    // "don't-break-friendly-CC" target filter — bots skip enemies
    // CC'd by their own teammates so the CC duration isn't wasted.
    bool       is_cc_locked = false;
    ObjectGuid cc_caster;

    // Line-of-sight from the bot at snapshot time. False when an
    // obstacle (terrain, building, pillar, doodad) blocks the bot's
    // sight to the unit. Drives the LoS-aware target-switch rule:
    // ranged casters skip out-of-LoS targets since their casts would
    // fail anyway. Costly per-unit raycast — the snapshot-build cost
    // is bounded by nearby_enemies/friends caps (~32 units total).
    bool       in_los = true;

    // Overflow-safe HP percent — same rationale as GroupMemberSummary::hp_pct.
    int32 hp_pct() const
    { return max_hp > 0 ? static_cast<int32>((int64_t(hp) * 100) / max_hp) : 0; }
};

// Battleground queue / live-state aggregate. One of the seven subsystem
// sub-structs designed by REFACTOR_2_SNAPSHOT_HANDOVER.md. Owns every BG-
// related field formerly flat on BotSnapshot. BgQueueEntry / BgNodeState
// types are hoisted to namespace scope so they can be referenced from
// BgState members and from external sites via the using-aliases inside
// BotSnapshot below.
struct BgQueueEntry
{
    uint16 bg_type_id;
    uint32 joined_at_sec;
    uint32 invited_to_instance;
};

struct BgNodeState
{
    float       x = 0.f, y = 0.f, z = 0.f;
    uint8       owner_team = 0;        // 0=neutral, 1=alliance, 2=horde
    bool        is_contested = false;
    // AV bunkers/towers only: the structure is razed. A destroyed tower
    // publishes attacker-control worldstates (owner_team = razing team,
    // uncontested) but can never flip again — consumers must not garrison
    // or attack it. False for every capturable node type.
    bool        is_destroyed = false;
    // Live player pressure within 40y of the node (BG audit N66): lets
    // Defender/Roamer ranking REINFORCE a threatened own node before the
    // flip even starts, instead of reacting only to is_contested. Counted
    // by the builder in one pass over the BG map's player list.
    uint8       alliance_players_near = 0;
    uint8       horde_players_near    = 0;
    uint32      entry = 0;
    // GO template name copied from GameObjectTemplate::name. Used by
    // /diag and other diagnostics to render readable node ownership
    // (e.g. "Stables: contested by Horde") without re-resolving the
    // entry against ObjectMgr from a tooling thread.
    std::string name;
};

// ---- PvE group-coordinator order (dungeon/raid coordination) ----
// Published by PveGroupCoordinator (world thread, ~500ms cadence) via the
// builder for every V2 bot whose group is inside a dungeon or raid map.
// Unlike the BG order (one dominant movement duty), PvE duties COMPOSE —
// a bot can simultaneously be interrupter rank 1, hold a kill focus and
// own a spread slot — so this is a per-duty field set, not a kind enum.
// `active == false` (coordinator disabled / group not covered / data
// cold) means every consumer falls back to the legacy per-bot logic;
// individual fields also carry their own "no opinion" sentinels so a
// plan can direct one duty and leave the rest alone.
struct PveOrder
{
    // A group plan covers this bot. When false every other field is
    // meaningless and consumers run unmodified legacy behavior.
    bool       active = false;

    // 0 = none, 1 = MAIN tank (owns tank_pull / boss face), 2 = OFF tank
    // (follows the main, taunts only on tank_swap triggers). The off-tank
    // duty is assigned ONLY in raids or script-demanded 2-tank content
    // (tank_swap_on_spells) — a classic 5-man runs 1/1/3 and a second
    // tank-spec member there keeps duty 0 and plays as DPS. Pull
    // initiative is gated on being the main_tank (below), not on this
    // field, so redundant party tanks don't double-pull either.
    uint8      tank_duty = 0;

    // Interrupt rotation rank. 0 kicks on sight; 1 is the backup (kicks
    // only when the cast is still going on a later look / about to
    // finish); 2+ HOLD their interrupt for the next cast. 0xFF = not an
    // interrupter. Replaces the per-bot self-throttle under which every
    // capable kicker burned its interrupt on the same mandatory cast.
    uint8      interrupt_rank = 0xFF;

    // Soak duty for advice.soak_spells mechanics: 0 = no opinion
    // (legacy: everyone soaks), 1 = designated soaker (move IN),
    // 2 = stay OUT (suppresses the legacy everyone-walks-in herd that
    // put the whole group inside one soak circle).
    uint8      soaker = 0;

    // Spread-mechanic bearing slot (0..N-1 around the group). 0xFF =
    // unassigned (legacy away-from-nearest-ally step). Decorrelates the
    // spread so members fan out instead of cascading off each other.
    uint8      spread_slot = 0xFF;

    // Healers: primary heal responsibility (the main tank for the
    // designated tank-healer). Empty = raid triage (legacy APL picker).
    ObjectGuid heal_focus;

    // The group's MAIN tank (set for every ordered bot when one exists).
    // The off-tank (tank_duty == 2) follows this guid between pulls
    // instead of free-roaming; other consumers may use it for assists.
    ObjectGuid main_tank;

    // DPS: synchronized kill target (the one priority add everyone
    // burns). Empty = legacy per-bot victim selection.
    ObjectGuid kill_focus;

    void reset() { *this = PveOrder{}; }
};

// Core vital stats — HP, power, combat flags, basic crowd-control state.
// REFACTOR_2 sub-struct (vitals). Power slots indexed by `Powers` enum
// (MANA=0 .. RUNE_UNHOLY=22). Sized to 26 (MAX_POWERS) so modern types
// (Holy Power 9, Maelstrom 11, Chi 12, etc.) are addressable.
struct VitalsState
{
    int32 hp = 0;
    int32 max_hp = 0;
    std::array<int32, 26> power{};
    std::array<int32, 26> max_power{};

    bool in_combat = false;
    // Milliseconds elapsed since combat started (0 when out of combat).
    Ms   combat_duration{0};
    // Milliseconds elapsed since combat LAST ended. 0 while in combat or if
    // the bot has not entered combat since login. Used by maintenance rules
    // (food/drink/bandage) to refuse to fire during the brief flicker
    // window after a fight ends — TC's IsInCombat() can drop for a tick
    // between mob deaths even when more mobs are pulling in.
    Ms   ms_since_combat_exit{0};
    bool is_alive = true;

    // Crowd-control flags.
    bool is_stunned   = false;
    bool is_silenced  = false;
    bool is_rooted    = false;

    // PvP context flags.
    bool is_pvp        = false;
    bool is_ffa_pvp    = false;
    bool is_sanctuary  = false;

    // Pure-POD (fixed arrays + scalars): default-assign is allocation-free.
    void reset() { *this = VitalsState{}; }
};

// Group identity: group GUID + bot's role in this group. The full member
// list lives in GroupSnapshot (a separate, shareable per-group snapshot);
// this is the bot's view of "am I grouped + what's my job".
struct GroupMembershipState
{
    ObjectGuid group_guid;
    Role       my_role = Role::Unknown;

    void reset() { *this = GroupMembershipState{}; }
};

// Self-cast teleport spell entry. Hoisted out of BotSnapshot so TravelState
// can hold the vector cleanly; `BotSnapshot::SelfTeleportSpell` alias
// preserved for external compatibility.
struct SelfTeleportSpell { uint32 spell_id; uint32 dest_map; };

// Travel-infrastructure aggregate: mounts, taxi mask, recommended-flight,
// portal anchor + next-hop, homebind, hearthstone, self-teleport spells.
struct TravelState
{
    // Riding skill (Player::GetSkillValue(SKILL_RIDING)). 0 = no riding learned.
    uint16 riding_skill = 0;
    // Best mount spell the bot can currently cast (highest-tier flying →
    // ground → 0 if none).
    uint32 best_mount_spell = 0;

    // Recommended same-map flight (Phase B routing). hop_count >= 2 means
    // a real flight is worth taking. dest_node == 0 when no route was found.
    // start_fm is the live FM creature guid IF one is in scan range of the
    // start node (so fly_to_taxi can interact it); it may be EMPTY while the
    // bot is still far from the start node — in that case the bot walks to
    // recommended_taxi_start_{x,y,z} (the start node's world position) first.
    // This makes flight PROACTIVE: a bot with a far same-map goal walks to the
    // nearest known flight master and flies, exactly like a real player, instead
    // of only flying when it happens to wander next to an FM.
    ObjectGuid recommended_taxi_start_fm;
    uint32     recommended_taxi_dest_node = 0;
    uint16     recommended_taxi_hop_count = 0;
    float      recommended_taxi_start_x = 0.f;
    float      recommended_taxi_start_y = 0.f;
    float      recommended_taxi_start_z = 0.f;

    // Nearest known cross-map travel anchor for the bot's goal map.
    uint32 nearest_portal_anchor_dest_map = 0;
    float  nearest_portal_anchor_x = 0.f;
    float  nearest_portal_anchor_y = 0.f;
    float  nearest_portal_anchor_z = 0.f;
    uint8  nearest_portal_anchor_kind = 0;   // 1=Portal, 2=Transport, 0=none
    // The SPECIFIC transport/portal GO entry this anchor targets. Critical for
    // the Org zeppelin towers: the Undercity (164871) and Grom'gol (175080)
    // zeppelins share one tower ~20y apart AND both have dest_map==0 (Tirisfal +
    // Stranglethorn are both Eastern Kingdoms), so the board check can't tell
    // them apart by destination map — it must match this exact entry, or a bot
    // bound for Tirisfal will board whichever zeppelin happens to be docked.
    uint32 nearest_portal_anchor_entry = 0;
    // TravelPlanner Phase E: immediate next-hop map id along the
    // shortest portal-graph route. kInvalidMapId when no route / already on
    // goal map (map id 0 = Eastern Kingdoms is a valid next hop, so 0 cannot
    // be the "none" sentinel).
    uint32 next_hop_dest_map = kInvalidMapId;

    // Homebind ("hearthstone destination") — Player::m_homebind copy.
    uint32 homebind_map_id = 0;
    float  homebind_x = 0.f, homebind_y = 0.f, homebind_z = 0.f;

    // Hearthstone (item 6948) availability + cooldown.
    bool   has_hearthstone   = false;
    uint32 hearthstone_cd_ms = 0;

    // Bytewise copy of Player::m_taxi.GetTaximask(). One bit per
    // TaxiNodes.dbc id.
    std::vector<uint8> taxi_mask;

    // Self-cast teleport spells the bot knows that have a resolvable
    // destination map (Mage city teleports, DK Death Gate, etc).
    std::vector<SelfTeleportSpell> self_teleport_spells;

    // Recycle-pool reset: clear the two vectors (retain capacity), reset
    // scalars. Mirrors the constructed defaults above exactly.
    void reset()
    {
        riding_skill = 0;
        best_mount_spell = 0;
        recommended_taxi_start_fm = ObjectGuid::Empty;
        recommended_taxi_dest_node = 0;
        recommended_taxi_hop_count = 0;
        recommended_taxi_start_x = 0.f;
        recommended_taxi_start_y = 0.f;
        recommended_taxi_start_z = 0.f;
        nearest_portal_anchor_dest_map = 0;
        nearest_portal_anchor_x = 0.f;
        nearest_portal_anchor_y = 0.f;
        nearest_portal_anchor_z = 0.f;
        nearest_portal_anchor_kind = 0;
        nearest_portal_anchor_entry = 0;
        next_hop_dest_map = kInvalidMapId;
        homebind_map_id = 0;
        homebind_x = 0.f; homebind_y = 0.f; homebind_z = 0.f;
        has_hearthstone = false;
        hearthstone_cd_ms = 0;
        taxi_mask.clear();
        self_teleport_spells.clear();
    }
};

// Guild membership / invite / events / rival state. Refactor #2 sub-
// struct (guild). 9 fields previously scattered as flat guild_* members.
struct GuildState
{
    // Set when another player has invited this bot to a guild via
    // CMSG_GUILD_INVITE_BY_NAME.
    bool   has_invite            = false;
    uint64 invite_id             = 0;
    // Bot's currently-joined guild id (Player::GetGuildId). 0 = guildless.
    uint64 id                    = 0;
    // Bot's rank id in their current guild (0=GM, 1=Officer, ...).
    // 0xFF when guildless.
    uint8  rank_id               = 0xFF;
    uint16 member_count          = 0;
    uint16 online_member_count   = 0;
    // SC-P3c: count of ONLINE guild members that are NOT V2 bots (i.e. real
    // human players). Computed in the snapshot builder during the same
    // member-map walk that produces online_member_count, using the
    // Services::Registry().has(guid) bot-check (the same predicate the
    // promote-human-to-leader logic uses). Ambient / self-initiated guild
    // chatter (smalltalk / babble / tavern_hangout) is gated on this being
    // >= 1 so bots don't babble to an empty (bot-only) guild — talking to
    // nobody is itself a tell and pointless traffic. Reactive announces
    // (ding / login-greet / quest-brag) stay UNCONDITIONAL: they fire off a
    // real event and reading them later in guild log is fine.
    uint16 online_human_member_count = 0;
    // Bot's guild's rival_guild_id (0 = no rival).
    uint64 rival_id              = 0;
    // Server-wide currently-active event (uint8 cast of GuildEventKind).
    uint8  active_event_kind     = 0;
    // True when the manager has stamped a pre-announce callout for the
    // bot's guild — first officer to fire idle:guild_event:announce wins.
    bool   has_pending_callout   = false;
    // #4C: true when this bot's guild is a bot-founded / bot-managed guild
    // (BotGuildMgr::IsBotManaged). Populated by the builder so the guild
    // idle rules (recruit / chat / event) stop doing a per-tick
    // O(N)-with-mutex IsBotManaged lookup on the worker thread. False for
    // guildless bots and for operator/player-owned guilds. Appended at the
    // END of GuildState per the POD-append rule.
    bool   is_bot_managed        = false;

    void reset() { *this = GuildState{}; }
};

// LFG queue / role-check / proposal / vote-kick state. Refactor #2 sub-
// struct (lfg) — small surface, mirrored cleanly from the legacy flat
// lfg_* fields.
struct LfgState
{
    // Set to a non-zero proposal id when LFG matched the bot into a dungeon
    // and the ready popup is up (LFG_ANSWER_PENDING). Auto-accepted by
    // State_Idle so the bot enters the dungeon without owner intervention.
    // Zero when no active proposal.
    uint32 proposal_id = 0;
    // Set when the bot's group has an active LFG role-check waiting on the
    // bot to declare its desired role.
    bool   role_check_pending = false;
    // Bot's currently-published LFG role bitmask on its group
    // (Group::GetLfgRoles). Bits: 1=LEADER, 2=TANK, 4=HEALER, 8=DAMAGE.
    uint8  published_role = 0;
    // True while the bot is in any LFG queue (LFGMgr::GetState == LFG_STATE_QUEUED).
    bool   in_queue = false;
    // True while LFGMgr tracks the bot as actively running its matched dungeon
    // (LFGMgr::GetState == LFG_STATE_DUNGEON). Unlike GROUP_FLAG_LFG (which a
    // finder-formed bot group does NOT reliably carry) and the per-bot
    // last_lfg_dungeon_id (which resets to 0 on relogin/restart), this LFGMgr
    // state SURVIVES a worldserver restart, so it is the authoritative,
    // restart-robust signal for re-arming dungeon-run mode on non-leader
    // followers after a crash/reload. Set only for bots actually inside a
    // dungeon instance (a tiny subset — no per-bot getter spam for solo bots).
    bool   in_dungeon = false;
    // True when an LFG vote-to-kick is active in the bot's group.
    bool   vote_kick_active = false;

    void reset() { *this = LfgState{}; }
};

// Per-character identity: name + level + class/race/spec/team/gender/faction
// + XP progress + honor track. REFACTOR_2 sub-struct (identity). The
// `guid` field stays on BotSnapshot directly because it's the primary
// key and accessed via many existing pointer-shaped APIs.
struct IdentityState
{
    std::string name;
    uint8       level   = 0;
    // Experience progress at the current level. xp / xp_for_level give the
    // raw counters; rest_bonus_xp is the rested xp pool (>0 in inns/cities).
    // All zero for max-level bots since GetXP returns 0 there.
    uint32      xp                  = 0;
    uint32      xp_for_level        = 0;
    uint32      rest_bonus_xp       = 0;
    // PvP — Honor track (12.0+). honor_xp / honor_xp_for_next progress the
    // honor level (m_activePlayerData->Honor / HonorNextLevel). Lifetime /
    // today / yesterday HK counts let PvP-aware idle rules know whether to
    // keep grinding or hearth out.
    uint32      honor_xp            = 0;
    uint32      honor_xp_for_next   = 0;
    uint32      honor_level         = 0;
    uint32      honor_kills_today   = 0;
    uint32      honor_kills_yesterday = 0;
    uint32      honor_kills_lifetime  = 0;
    uint8       race    = 0;
    uint8       cls     = 0;
    // Effective team in *this* match — 0=unknown, 1=Alliance, 2=Horde.
    // Populated from Player::GetEffectiveTeam() (mercenary-aware).
    uint8       team    = 0;
    // ChrSpecialization is `enum class : uint32`. Modern specs (Evoker
    // /Dracthyr) exceed 1000 so this field carries the full DB2 id.
    uint32      spec    = 0;
    uint8       gender  = 0;
    uint32      faction = 0;

    // Recycle-pool reset (Tier 3.1): clear the string (retain its capacity)
    // and reset every scalar to its constructed default. See
    // BotSnapshot::reset_for_reuse for the completeness contract.
    void reset()
    {
        name.clear();
        level = 0;
        xp = 0; xp_for_level = 0; rest_bonus_xp = 0;
        honor_xp = 0; honor_xp_for_next = 0; honor_level = 0;
        honor_kills_today = 0; honor_kills_yesterday = 0; honor_kills_lifetime = 0;
        race = 0; cls = 0; team = 0; spec = 0; gender = 0; faction = 0;
    }
};

// World position. map_id is the live map (transports remap map_id to the
// transport's current map). x/y/z are world coords; o is facing yaw.
// area_id / zone_id / is_indoors live in AreaState; instance flags in
// InstanceContextState.
struct PositionState
{
    uint32 map_id = 0;
    // Map::GetInstanceId() — 0 for continents, non-zero for dungeon /
    // raid / BG / arena Map* instances. Lets cross-map gates compare
    // *Map* identity, not just map_id (so a same-mapId-different-instance
    // mismatch is detected and handled).
    uint32 instance_id = 0;
    // MapEntry::IsBattlegroundOrArena() for the CURRENT map. Combined with
    // bg.in_battleground == false this is the BG-ORPHAN signature: the bot
    // is standing on a battleground map whose Battleground object no longer
    // exists (end-of-match removal teleport failed, BG deleted anyway —
    // live-observed 2026-06-11: 3 bots brawling forever on dead map 566).
    // Drives combat:bg_orphan_disengage + idle:bg_orphan_escape.
    bool   map_is_bg_or_arena = false;
    float  x = 0.f, y = 0.f, z = 0.f, o = 0.f;

    void reset() { *this = PositionState{}; }
};

// Quest turn-in row: nearby NPC/GO that can accept a complete quest.
// auto_accept (offers only): the quest carries QUEST_FLAGS_AUTO_ACCEPT (0x80000)
// — the client auto-grants it on querying the giver. Bots have no client, so
// idle:quest_auto_accept grabs these instantly (no hesitation) before the bot
// wanders off; otherwise AUTO_ACCEPT starter chain-heads (e.g. 25152 "Your Place
// In The World") are never accepted and gate the whole racial starter chain.
struct QuestTurnIn { ObjectGuid giver; uint32 quest_id; bool auto_accept = false; };
// Bag-item that starts a not-yet-accepted quest.
struct StartingItem { uint32 item_entry; uint32 quest_id; };
// World quest discovery row. type 0=offer, 1=turn-in.
struct WorldQuestEntry
{
    ObjectGuid giver;
    uint32     quest_id           = 0;
    uint8      type               = 0;
    float      x = 0.f, y = 0.f, z = 0.f;
    uint32     area_id            = 0;
    uint32     reward_currency_id = 0;
    uint32     reward_money       = 0;
};
// Quest discovery rollup. Lists of nearby questgivers (offers + turn-ins),
// bag items that start a quest, and world-quest givers within range. All
// resolved by the snapshot builder so AI workers don't round-trip
// ObjectMgr / Creature::hasInvolvedQuest from the worker thread.
struct QuestDiscoveryState
{
    std::vector<QuestTurnIn>      quest_turnins;
    std::vector<QuestTurnIn>      quest_offers;
    std::vector<StartingItem>     quest_starting_items;
    std::vector<WorldQuestEntry>  available_world_quests;

    void reset()
    {
        quest_turnins.clear();
        quest_offers.clear();
        quest_starting_items.clear();
        available_world_quests.clear();
    }
};

// Quest POI for the currently-focused objective. See full doc on the
// BotSnapshot::quest_log alias below — preserved as a namespace-scope
// struct so other headers can refer to it without including BotSnapshot.h.
struct QuestObjectivePoi
{
    uint32 map_id = 0;
    float  x = 0.f;
    float  y = 0.f;
    float  z = 0.f;
    float  radius = 0.f;
    bool   valid = false;
};

// Resolved AreaTrigger waypoint for QUEST_OBJECTIVE_AREATRIGGER /
// AREA_TRIGGER_ENTER objectives.
struct QuestAreaTrigger
{
    uint32 entry  = 0;
    uint32 map_id = 0;
    float  x = 0.f;
    float  y = 0.f;
    float  z = 0.f;
    float  radius = 0.f;
    bool   valid  = false;
};

// Bag-item "tool" that progresses the active objective when used.
struct QuestTool
{
    uint8  bag = 0;
    uint8  slot = 0;
    uint32 item_entry = 0;
    uint32 spell_id   = 0;
    uint32 target_entry = 0;
    bool   valid = false;
};

// Cross-quest actionable index entry: an objective in the bot's log that
// is currently progressable by an entity visible in this snapshot.
struct ActionableObjective
{
    uint32 quest_id;
    uint32 objective_id;
    uint8  type;
    int32  object_id;
    float  distance_sq;
    ObjectGuid source_guid;
};

// Scenario step tracking for an InstanceMap with a live InstanceScenario.
struct ScenarioStepInfo
{
    uint32 scenario_id           = 0;
    uint32 step_id               = 0;
    uint16 current_step_progress = 0;
};

// REFACTOR_2 sub-struct: quest log + currently-focused objective + resolved
// POI/AreaTrigger/tool waypoints + the cross-quest actionable index + scenario
// step. The bot's quest decision-making (idle:quest_* family in State_Idle)
// reads from this one struct rather than from a sprawl of flat fields.
struct QuestLogState
{
    std::vector<QuestEntry>          quests;
    // `find_quest(quest_id)` lookup. Quest-log is bounded at 25
    // active quests but find_quest is called from many rules per tick
    // (objective progress, turn-in eligibility, share-quest, etc.) so
    // the linear walk added up. Tier 3.3: sorted flat vector + binary
    // search (was unordered_map) — one contiguous alloc, capacity reused
    // across the recycle pool. Build protocol: push() per quest, then
    // finalize(). Lookup is via find().
    FlatIndexMap<uint32> quests_index;
    QuestObjectiveEntry              current_objective{};
    uint32                           current_quest_id = 0;
    QuestObjectivePoi                current_objective_poi{};
    // R7: true when current_objective_poi was SYNTHESIZED as a cross-continent
    // leveling-zone relocation goal (the bot has no real quest; current_quest_id
    // is 0). Makes the existing cross-map travel pipeline (taxi / portal / dock /
    // ship / UnifiedTravelGraph) route a starved, out-levelled bot to a
    // level-appropriate quest hub on another map. has_current_objective() reports
    // true while this is set so those POI-driven rules engage; quest-specific
    // rules stay gated on current_quest_id / objective .type and remain inert.
    bool                             objective_is_relocation = false;
    // True when the bot is questless and the R7 leveling-relocation picker has a
    // valid target hub for it — INCLUDING the directly-walkable same-map case
    // where no POI is synthesized (movement delegated to idle:travel_to_hub).
    // The opportunistic band (equip/gather/vendor…) yields to this via
    // has_actionable_quest() so a relocating bot WALKS to its doable hub instead
    // of running idle:equip_upgrade in place ("Outfitting"). Without it,
    // travel_to_hub lives at autoact priority ~50, below equip_upgrade@600, so the
    // bot never moves. objective_is_relocation (above) covers only the cross-map /
    // needs-bridge subset that gets a POI; this covers ALL relocation picks.
    bool                             has_relocation_target = false;
    // True when the bot's current objective is presently blacklisted by BotAI's
    // stuck-detection (no progress for ~5 min, or fast-blacklist on a NoPath
    // gap). Mirrors BotAI::current_objective_blacklisted(now). Lets the
    // quest-first idle gates (HasActionableQuest) NOT suppress maintenance for a
    // bot wedged near an unreachable POI — so a full-bag / broken-gear bot at a
    // GoalUnreachable wedge can still vendor/repair instead of livelocking.
    bool                             current_objective_blacklisted = false;
    // Set when the bot is STUCK reaching a SAME-MAP objective POI (path_blocked
    // high) AND the travel graph can reach it via a non-walk bridge edge
    // (elevator / areatrigger-teleport / intra-map ship). Drives the same
    // graph-route execution as objective_is_relocation, but for a normal quest
    // objective on another "floor" the naive walk can't reach (e.g. stranded on
    // the Orgrimmar zeppelin-tower deck with a ground goal — descend by lift).
    // Gated on stuck so the cached FindRoute only runs for already-failing bots.
    bool                             objective_needs_bridge = false;
    // The VALIDATED bridge route to the objective, computed by the builder's
    // world-thread probe with navmesh-checked source attaches (see
    // RouteRequest::validate_source_walk). Mirrors BotAI::PlanLeg field-for-
    // field. The AI worker MUST execute these legs instead of recomputing
    // the route: its own FindRoute can't validate attaches off the world
    // thread, so it would produce the euclidean walk-only route the wedged
    // bot provably can't follow (the exact failure this exists to fix —
    // a bot in the Undercity interior "attaching" through the wall to a
    // surface node instead of riding the elevator out). Populated only
    // while objective_needs_bridge / objective_is_relocation is set.
    struct BridgeLeg
    {
        uint8  kind   = 0;        // V2::Travel::EdgeKind numeric value
        uint32 to_map = 0;
        float  to_x = 0.f, to_y = 0.f, to_z = 0.f;
        float  from_x = 0.f, from_y = 0.f, from_z = 0.f;
        uint32 payload = 0;
        uint32 to_taxi_node = 0;
    };
    std::vector<BridgeLeg>           bridge_route;
    uint64                           bridge_route_goal_key = 0;
    QuestAreaTrigger                 current_objective_areatrigger{};
    QuestTool                        current_objective_tool{};
    std::vector<ActionableObjective> actionable_objectives;
    ScenarioStepInfo                 scenario_step{};
    // Monotonically-increasing count of completed (turned-in) quests for
    // this character — drives idle:guild_chat:quest_brag (detect transition
    // vs last-seen count). Doesn't gate on reward rarity; ambient flavour.
    uint16                           completed_quest_count = 0;

    void reset()
    {
        quests.clear();
        quests_index.clear();
        current_objective = QuestObjectiveEntry{};   // clears inner alias/label vectors
        current_quest_id = 0;
        current_objective_poi = QuestObjectivePoi{};
        objective_is_relocation = false;
        has_relocation_target = false;
        current_objective_blacklisted = false;
        objective_needs_bridge = false;
        bridge_route.clear();
        bridge_route_goal_key = 0;
        current_objective_areatrigger = QuestAreaTrigger{};
        current_objective_tool = QuestTool{};
        actionable_objectives.clear();
        scenario_step = ScenarioStepInfo{};
        completed_quest_count = 0;
    }
};

// Gossip menu state. When the bot has interacted with an NPC and a gossip
// menu is open, gossip_npc holds the NPC guid + gossip_options holds the
// menu rows. Used by the 2-phase quest-talk rule that needs to pick the
// right menu option to trigger a quest credit script.
struct GossipMenuOption
{
    uint8  order_index;
    uint8  option_npc;
    int32  gossip_option_id;
};
struct GossipState
{
    ObjectGuid                    gossip_npc;
    std::vector<GossipMenuOption> gossip_options;

    void reset()
    {
        gossip_npc = ObjectGuid::Empty;
        gossip_options.clear();
    }
};

// Mailbox state. Drained when the bot reaches a mailbox: mail vector is
// the full mail list (capped) so an at-mailbox rule can pick the next one
// to drain; unread_mail_count is Player::unReadMails (decremented on
// mark-as-read), useful for "you have N new mails" whisper feedback.
struct MailboxState
{
    std::vector<MailEntry> mail;
    uint32                 unread_mail_count = 0;

    void reset()
    {
        mail.clear();
        unread_mail_count = 0;
    }
};

// Combat targets. victim = whoever I'm attacking right now (set when the
// bot's own auto-attack engages). current_target = the bot's selected
// target (focus / sticky-target). attackers / nearby_enemies /
// nearby_friends are unit lists the snapshot builder captures via
// proximity sweeps (~40y default), used by aggro/heal/buff predicates.
struct CombatTargetsState
{
    ObjectGuid              victim;
    ObjectGuid              current_target;
    // True only when current_target resolves to a LIVE unit the bot could
    // legally attack (IsValidAttackTarget on the world thread). Autonomy
    // gates (can_autoact) must consult THIS, not mere selection presence:
    // a selfbot owner click-selecting a friendly NPC / party member / self
    // sets UNIT_FIELD_TARGET and would otherwise freeze the entire quest/
    // travel/wander cascade until the selection is cleared (observed
    // 2026-06-13: selfbot parked idle indefinitely with a non-hostile
    // unit selected — only maintenance/ambient rules kept firing).
    bool                    current_target_hostile = false;
    std::vector<NearbyUnit> attackers;
    std::vector<NearbyUnit> nearby_enemies;
    std::vector<NearbyUnit> nearby_friends;
    // Honest, stalker-free attacker count: number of `attackers` that are
    // actually FIGHTABLE (not UNIT_FLAG_UNINTERACTIBLE, not pacified, alive).
    // The raw attackers vector is flooded by untargetable trigger units
    // (e.g. the 56 static Deadmines "Vanessa Lightning Stalker" 49521 that
    // deal no damage but enter the bot's threat list), so attackers.size()
    // reads 8-12 at the harbor while only ~0-4 are real. Combat/advance gates
    // that need "real density" read THIS; the raw vector is retained intact
    // for consumers that must still see the untargetable entries (the (0c)
    // all-untargetable wedge test, ghost-heal). Built in BotSnapshotBuilder.
    uint16                  fightable_attackers = 0;

    void reset()
    {
        victim = ObjectGuid::Empty;
        current_target = ObjectGuid::Empty;
        current_target_hostile = false;
        attackers.clear();
        nearby_enemies.clear();
        nearby_friends.clear();
        fightable_attackers = 0;
    }
};

// Per-bot navigation telemetry surfaced to rules so they can short-circuit
// when the bot's been wedged. BotAI tracks path failures via
// `note_path_blocked`; the snapshot Builder copies the count + last-time
// here so AI worker rules can read without dereferencing BotAI directly.
//
// `count` is the CONSECUTIVE (current-wedge) count of API::move_to results
// that returned Result::Locked (NoPath / FarFromPolyEnd / Incomplete / Short).
// It is NOT lifetime-monotonic: note_move_succeeded() RESETS it to 0 on every
// successful move (BotAI.h note_move_succeeded). Do not "fix" it back to
// monotonic — the snapshot-cadence Cruise tier and the wedge-watchdog both rely
// on this reset (a healthy traveller's count returns to 0 so it isn't pinned to
// Active / flagged wedged forever after a single transient block). Rules
// snapshot this count when they first emit; if the next snapshot shows
// it grew by >=3, the rule's anchor is unreachable and the rule should
// fall through. `last_ms` is the GameTime::GetGameTimeMS of the most
// recent block — lets rules ignore stale baselines older than ~30s.
struct PathTelemetryState
{
    uint32 count   = 0;
    uint32 last_ms = 0;

    void reset() { *this = PathTelemetryState{}; }
};

// Zone / area context. area_id is finest-grained (AreaTable.dbc rows like
// "The Crossroads"); zone_id is the parent zone (e.g. "Barrens"); is_indoors
// flips true when the bot is inside any indoor flag-set area (caverns,
// taverns, dungeons). The latter drives the "no flying mount indoors"
// gate. map_id and x/y/z/o remain at top level until the broader
// PositionState migration lands.
struct AreaState
{
    uint32 area_id    = 0;
    uint32 zone_id    = 0;
    bool   is_indoors = false;

    void reset() { *this = AreaState{}; }
};

// Vehicle attachment. Flips true when the bot is auto-attached as a
// passenger on a vehicle creature (siege engine, gunship turret, etc.).
// Most idle behaviours are suppressed while on a vehicle (the bot's
// own movement / spell intents are largely ignored — actions route
// through the vehicle's spell deck).
struct VehicleState
{
    bool       on_vehicle           = false;
    ObjectGuid vehicle_guid;
    int8       vehicle_seat_id      = -1;
    uint32     vehicle_seat_ability = 0;
    // Creature entry of the base vehicle Unit (demolisher / siege engine
    // / glaive / catapult / cannon). Lets BG scripts pick a per-entry
    // seat ability without re-walking nearby_friends.
    uint32     vehicle_entry        = 0;

    void reset() { *this = VehicleState{}; }
};

// Active movement waypoint. path_target is the unit/object guid we're
// pathing to (empty when we're walking to a static world point). The
// path_end_* fields are the resolved world destination of the current
// movement (used by the move-stuck rule + waypoint-overshoot detection).
struct PathState
{
    ObjectGuid path_target;
    uint32     path_end_map_id = 0;
    float      path_end_x = 0.f;
    float      path_end_y = 0.f;
    float      path_end_z = 0.f;

    void reset() { *this = PathState{}; }
};

// Bank capacity. tab_count is the number of character bank tabs the bot
// has purchased (0..4 in 12.0); free_slots is the count of empty slots
// across all purchased bank-bag containers. Lets a bag-pressure rule
// decide "should I deposit overflow at the next banker".
struct BankState
{
    uint8  bank_tab_count  = 0;
    uint16 bank_free_slots = 0;

    void reset() { *this = BankState{}; }
};

// Active group/master loot rolls awaiting the bot's vote. See
// LootRollEntry for per-row semantics; AI emits LootRollIntent.
struct LootRollEntry
{
    ObjectGuid loot_object;
    uint8      loot_list_id;
    uint32     item_entry;
    uint8      vote_mask;
    bool       is_upgrade;
    bool       is_quest_item;
};
struct LootRollsState
{
    std::vector<LootRollEntry> loot_rolls;

    void reset() { loot_rolls.clear(); }
};

// Nearby GameObjects within ~30yd that the AI cares about (mailbox,
// chests, herbs/ore, fishing holes, questgivers, binders, hazards,
// portals). go_type mirrors GAMEOBJECT_TYPE_*. Sorted by distance so
// the closest survive the bounded cap.
struct NearbyObject
{
    ObjectGuid guid;
    uint32     entry;
    uint8      go_type;
    float      x, y, z;
    // kInvalidMapId (NOT 0) when this object is not a cross-map transport.
    // 0 is Eastern Kingdoms — a REAL map — so defaulting to 0 made every
    // same-map object (notably type-11 elevators, which carry no
    // TransportTemplate) collide with a bot whose cross-map goal is EK, and
    // the bot "boarded" the elevator thinking it was the EK zeppelin.
    uint32     teleport_dest_map = kInvalidMapId;
    float      teleport_dest_x = 0.f, teleport_dest_y = 0.f, teleport_dest_z = 0.f;
    bool       is_hazard     = false;
    float      hazard_radius = 0.0f;
    // DESTRUCTIBLE_BUILDING (type 33) only: true when the GO is in the
    // GO_DESTRUCTIBLE_DESTROYED state. Lets the BG siege-vehicle fire rule
    // skip already-breached IoC/SoTA gates instead of casting at rubble.
    bool       is_destroyed  = false;
};
struct NearbyObjectsState
{
    std::vector<NearbyObject> nearby_objects;

    void reset() { nearby_objects.clear(); }
};

// Instance context. is_in_instance covers everything except open-world.
// is_in_dungeon = 5-man (Map::IsDungeon), is_in_raid = raid map
// (Map::IsRaid). map_difficulty mirrors the active difficulty id
// (DifficultyEntry id; 0 for non-instance maps). Drives behaviors that
// should be suppressed inside instances (no hearth, no LFG queue, no
// logout, etc).
struct InstanceContextState
{
    bool   is_in_instance = false;
    bool   is_in_dungeon  = false;
    bool   is_in_raid     = false;
    uint32 map_difficulty = 0;

    void reset() { *this = InstanceContextState{}; }
};

// Movement flags. Cached from Player::isSwimming / IsFlying / IsMounted /
// isMoving — rules consult these to gate behaviours that require / forbid
// the bot being in motion or mounted.
struct MovementState
{
    bool is_swimming = false;
    bool is_flying   = false;
    bool is_mounted  = false;
    bool is_moving   = false;
    // True while the CORE is flying the bot on a taxi path (UNIT_STATE_IN_
    // FLIGHT). The AI must do NOTHING during a taxi flight — the
    // FlightPathMovementGenerator owns movement; any idle rule that emits a
    // move_to / teleport (watchdog_escape, unstick, hearth rescue) would
    // corrupt the flight. DispatchIdle and GlobalStuckRescue bail on this.
    bool is_in_taxi  = false;

    void reset() { *this = MovementState{}; }
};

// Transport / liquid-survival state. on_transport flips when the player
// is auto-attached to a ship/zeppelin (within ~10y of mooring) — issuing
// move_to during transit walks the bot off the deck. is_underwater drives
// the surface-to-breathe rule (LIQUID_MAP_UNDER_WATER triggers breath bar).
// is_in_damaging_liquid covers lava+slime; the flee-damaging-liquid rule
// fires whenever true.
struct EnvironmentState
{
    bool  on_transport         = false;
    bool  transport_stopped    = false;
    // True only when the ridden transport is a type-15 MO-transport (ship /
    // zeppelin) — i.e. dynamic_cast<Transport*> succeeds in the builder. False
    // for type-11 city elevators (and vehicle seats). The inline ship-disembark
    // guard gates on this so it never RemovePassenger's a nearby docked ship
    // while the bot is actually riding an elevator parked beside it.
    bool  transport_is_ship    = false;
    bool  is_underwater        = false;
    float water_surface_z      = 0.0f;
    bool  is_in_damaging_liquid = false;
    // Open-world water-escape FOCUS (FIX #12). When the bot is in swim-water the
    // builder resolves the nearest DRY navmesh footing (NearestNavPoint excludes
    // water/magma); idle:water_escape drives straight to it and PREEMPTS all
    // other movement until the bot is back on land. Without this the bot
    // oscillates — the water-exit recovery pulls it to shore while travel/wander
    // immediately walk it back in (Tindle, Stormwind harbor). valid=false when
    // not in swim-water or no dry footing within range.
    bool  water_escape_valid   = false;
    float water_escape_x       = 0.0f;
    float water_escape_y       = 0.0f;
    float water_escape_z       = 0.0f;

    void reset() { *this = EnvironmentState{}; }
};

// Dungeon execution context (Phase A of GROUP_DUNGEON_PLAN.md). Populated
// when bot is inside an instance map so the dungeon-run rules can drive
// tank pulls, interrupt rotation, wipe recovery, and run-completion
// without round-tripping the InstanceScript from the AI worker. Also holds
// the cheaper "encounter context" pair (npc_id + phase) used by per-script
// hooks. See per-field comments on the legacy flat fields (now consumed)
// for full semantics.
struct DungeonExecState
{
    // Coarse encounter identification used by per-script hooks.
    uint32 active_encounter_npc_id = 0;
    uint8  active_encounter_phase  = 0;

    bool       is_encounter_in_progress = false;
    ObjectGuid current_boss_guid;
    uint32     current_boss_entry = 0;
    int32      current_boss_hp     = 0;
    int32      current_boss_max_hp = 0;
    uint32     current_boss_casting_spell = 0;
    bool       current_boss_casting_interruptible = false;
    Ms         current_boss_cast_remaining{0};
    uint8      members_dead_count = 0;
    bool       dungeon_complete   = false;
    uint8      bosses_done_count  = 0;
    uint8      bosses_total_count = 0;
    bool       any_boss_in_special = false;
    uint32     instance_entrance_map = 0;
    float      instance_entrance_x = 0.f;
    float      instance_entrance_y = 0.f;
    float      instance_entrance_z = 0.f;
    // IN-INSTANCE entrance: the bot's first observed position after entering
    // this instance (map == the INSTANCE map). Distinct from
    // instance_entrance_* above, which is the PARENT-map ghost-port location
    // (Map.db2 CorpseMapID/Corpse pos) used by the cross-map corpse walk.
    // The wipe-regroup rule was comparing the parent-map id against the
    // instance map id — never equal, so regroup-at-entrance was dead code
    // since Phase G (audit B36). Zeroed while not in an instance.
    uint32     inside_entrance_map = 0;
    float      inside_entrance_x = 0.f;
    float      inside_entrance_y = 0.f;
    float      inside_entrance_z = 0.f;

    void reset() { *this = DungeonExecState{}; }
};

// Pet state. Hunter / Warlock / DK / Mage's Water Elemental + Hunter
// stable inventory. Empty when no pet.
struct StablePet
{
    uint32 pet_number = 0;
    uint32 creature_id = 0;
    std::string name;
    uint8  level = 0;
    uint8  slot_kind = 0;            // 0=active, 1=stabled, 2=unslotted
    uint8  slot_index = 0;
};
struct PetState
{
    ObjectGuid             pet_guid;
    int32                  pet_hp     = 0;
    int32                  pet_max_hp = 0;
    bool                   pet_alive  = false;
    bool                   pet_in_combat = false;
    // Pet knows Primal Rage 264667 (Ferocity-spec pets). APL Bloodlust rules
    // need this gate: pet_cast() emits unconditionally, so without it a
    // non-Ferocity pet would let the rule claim every tick of a boss fight.
    bool                   pet_can_bloodlust = false;
    uint8                  pet_level = 0;
    uint32                 pet_family = 0;
    std::string            pet_name;
    std::vector<AuraEntry> pet_auras;
    std::vector<StablePet> stable_pets;
    // Whom the pet is currently attacking (Empty if no victim). Drives
    // the assist-pet rule: bot picks up the pet's target so a hunter
    // standing idle while his cat melees a mob actually opens up too.
    ObjectGuid             pet_victim;
    // Mob guids actively engaging the pet (anyone with the pet in
    // m_attackers). Capped at 4 to keep the snapshot lean. Drives the
    // peel half of assist_pet: when pet melees mob A but mob B is
    // nuking the pet from range, prefer B (peel candidate) over A.
    std::vector<ObjectGuid> pet_attackers;

    void reset()
    {
        pet_guid = ObjectGuid::Empty;
        pet_hp = 0; pet_max_hp = 0;
        pet_alive = false;
        pet_in_combat = false;
        pet_can_bloodlust = false;
        pet_level = 0;
        pet_family = 0;
        pet_name.clear();
        pet_auras.clear();
        stable_pets.clear();
        pet_victim = ObjectGuid::Empty;
        pet_attackers.clear();
    }
};

// Inbound social events from other players. Group invites, summon requests,
// duel challenges, quest shares, trade requests — each one flips a flag /
// fills a guid that the dispatcher rules consult to auto-accept (group
// from anyone, summon from anyone, quest from group members) or
// auto-decline (trade from strangers, duel from non-friends).
struct SocialEventsState
{
    bool       has_group_invite        = false;
    ObjectGuid group_invite_leader;
    bool       has_summon_pending      = false;
    bool       has_duel_request        = false;
    ObjectGuid duel_initiator;
    bool       duel_initiator_is_friend = false;
    bool       has_trade_request       = false;
    ObjectGuid quest_share_sender;
    uint32     shared_quest_id         = 0;

    void reset() { *this = SocialEventsState{}; }
};

// Death-recovery state. is_ghost mirrors PLAYER_FLAGS_GHOST (set by
// BuildPlayerRepop on release). has_corpse / corpse_pos expose the bot's
// own corpse position so State_Dead can pathfind back to it for the
// sickness-free corpse run. corpse_reclaim_at_unix is the earliest
// GameTime::GetGameTime() at which Player::HandleReclaimCorpse will
// accept. corpse_to_graveyard_dist is the distance from the corpse to the
// closest graveyard for the bot's team (drives the corpse-run-vs-spirit-
// healer choice). has_resurrect_request flips true when another player
// has cast a resurrection spell on this corpse.
struct DeathState
{
    bool   has_resurrect_request    = false;
    bool   is_ghost                 = false;
    bool   has_corpse               = false;
    uint32 corpse_map_id            = 0;
    // The Corpse object's Map* instance id. Snapshot publishes 0 for the
    // continent / no-corpse cases; non-zero for instance maps. State_Dead
    // compares this against the bot's current instance_id to detect the
    // "same map_id but different Map*" case (typical: bot got teleported
    // out of the dungeon by some path while the corpse object is still
    // inside the instance Map). Without this, reclaim_corpse fires every
    // tick against a Map* mismatch — visible as "0.0y > 39y" log spam.
    uint32 corpse_instance_id       = 0;
    float  corpse_x = 0.f, corpse_y = 0.f, corpse_z = 0.f;
    int64  corpse_reclaim_at_unix   = 0;
    float  corpse_to_graveyard_dist = 0.f;

    void reset() { *this = DeathState{}; }
};

// Inventory rollup. gold (copper) + equipped slots + bag items + cached
// average item level. equipped is fixed-size 19 to match Player slot
// layout (Head..Tabard); each entry's blanks live as default-initialized
// EquippedItem rows. bag_items is unbounded (varies by bag count). The
// average_item_level is cached so /upgrades + /equip whisperers don't
// re-walk equipped[] on the worker thread.
struct InventoryState
{
    int32                        gold = 0;
    std::array<EquippedItem, 19> equipped{};
    std::vector<InventoryItem>   bag_items;
    uint16                       average_item_level = 0;
    // O(1) `item_count(entry)` lookup. Built alongside bag_items in
    // BotSnapshotBuilder. Value is the SUM of counts across all bag
    // slots holding that item entry (items stack across slots when the
    // bag is full of partial stacks). Used by has_reagents and the
    // crafting/consumable rules — was the second-hottest linear scan
    // after FindCooldown. Tier 3.3: accumulating flat vector + binary
    // search (was unordered_map) — add() per bag slot, finalize() merge-
    // sums duplicate entries; get() for the read. Capacity reused across
    // the recycle pool.
    FlatCountMap<uint32> bag_count_by_entry;

    void reset()
    {
        gold = 0;
        equipped.fill(EquippedItem{});   // EquippedItem is POD — no inner alloc
        bag_items.clear();
        average_item_level = 0;
        bag_count_by_entry.clear();
    }
};

// Auction-house row: a single listing the bot competes against.
struct AhCompetingEntry { uint32 item_entry; uint64 lowest_buyout; };

// Auction-house row: a listing the bot owns.
struct OwnedAuction
{
    uint32 auction_id;
    uint32 item_entry;
    uint32 stack_count;
    uint64 min_bid;
    uint64 buyout;
    int64  expires_in_sec;     // negative = expired
    bool   has_bidder;
    uint32 house_id;           // 1=neutral, 2=alliance, 6=horde, 7=goblin
};

// Auction-house row: a listing the bot may want to BUY (#4B buy-side).
// The cheapest current non-commodity listing for an item in the bot's
// "wanted" set (reagents for known recipes it is short on). The buy-side
// economy rule reads this to emit EconomyOp::AhBuyout / AhBid. price is the
// buyout copper (0 = bid-only); stack is the listing's total item count.
struct BuyableListing
{
    uint32 auction_id;
    uint32 item_entry;
    uint64 buyout;        // copper; 0 = no buyout (bid-only)
    uint32 stack;         // total items in the listing
    // #4B-1(b) per-unit FAIR-VALUE CEILING (copper). The buy-side rule
    // refuses any listing whose per-unit price (buyout / stack) exceeds
    // this, so a human can't post a wildly over-priced reagent and drain
    // bots through idle:ah_buy_reagents. Computed in the builder from the
    // item's vendor SellPrice * MaxReagentVendorMultiple (with a quality-
    // based flat floor when SellPrice == 0). APPEND-ONLY (POD discipline).
    uint64 fair_value_ceiling = 0;
};

// Auction-house COMMODITY row the bot may want to BUY (#4B-1 Part 3).
// Stackable trade goods (the vast majority of craft reagents) are NOT sold
// as individual auctions — they aggregate into a single commodity bucket
// bought via GetCommodityQuote -> BuyCommodity (NOT AhBuyout, which rejects
// commodities). For each wanted reagent the builder records the cheapest
// per-unit price and the total quantity available from non-self sellers, so
// the buy-side rule can size a purchase (qty = shortfall, capped by budget +
// available_qty) and emit EconomyOp::AhBuyCommodity. There is no auction_id
// here — the commodity path is keyed by item_entry across all listings.
struct BuyableCommodity
{
    uint32 item_entry;
    uint64 unit_price;     // copper per unit (cheapest current non-self listing)
    uint32 available_qty;  // total units listed by non-self sellers
    // #4B-1(b) per-unit FAIR-VALUE CEILING (copper). Same anti-pump guard
    // as BuyableListing: the buy-side rule refuses a commodity whose
    // unit_price exceeds this. Computed in the builder from vendor SellPrice
    // * MaxReagentVendorMultiple (quality-based flat floor when SellPrice ==
    // 0). APPEND-ONLY (POD discipline).
    uint64 fair_value_ceiling = 0;
};

// Auction-house rollup. ah_competing_buyout is the lowest competing buyout
// per bag item entry (scoped to faction house, populated only when the
// bot is at an auctioneer). auctions_owned is the full set of listings
// the bot owns across all four houses, capped to a reasonable size.
struct AuctionState
{
    std::vector<AhCompetingEntry> ah_competing_buyout;
    std::vector<OwnedAuction>     auctions_owned;
    // #4B buy-side. Cheapest current listing per wanted item (reagents the
    // bot is short on for its known recipes). Populated ONLY when the bot
    // is at an auctioneer (on-demand scan, same gate as ah_competing_buyout)
    // and capped to kMaxBuyableListings to bound the per-visit cost.
    // APPEND-ONLY: added at the END of AuctionState (POD discipline — see
    // project_v2_snapshot_bugs.md).
    static constexpr size_t kMaxBuyableListings = 32;
    std::vector<BuyableListing>   buyable_listings;
    // #4B-1 Part 3 buy-side COMMODITY path. Cheapest unit price + total
    // available per wanted reagent, for the SAME wanted/short set as
    // buyable_listings, but drawn from the commodity (stackable) side of the
    // faction house. Most craft reagents land here, not in buyable_listings
    // (which only catches the rare non-stackable case). Populated ONLY at an
    // auctioneer (same gate), capped to kMaxBuyableCommodities.
    // APPEND-ONLY: added at the END of AuctionState (POD discipline — see
    // project_v2_snapshot_bugs.md).
    static constexpr size_t kMaxBuyableCommodities = 32;
    std::vector<BuyableCommodity> buyable_commodities;

    void reset()
    {
        ah_competing_buyout.clear();
        auctions_owned.clear();
        buyable_listings.clear();
        buyable_commodities.clear();
    }
};

// Aura snapshots. own_auras / target_auras / victim_auras hold buffs +
// debuffs on the bot, current_target, and victim respectively. The new
// my_auras_on_others is HoT/buff bookkeeping — without it, predicates
// like "is my Renew still on the tank?" return false and the HoT gets
// re-cast every tick, wasting mana and overwriting stacks.
struct AurasState
{
    struct OutboundAura { ObjectGuid target; uint32 spell_id; Ms remaining; uint8 stacks; };

    std::vector<AuraEntry>    own_auras;
    std::vector<AuraEntry>    target_auras;
    std::vector<AuraEntry>    victim_auras;
    std::vector<OutboundAura> my_auras_on_others;
    // O(1) lookup into own_auras keyed by spell_id. Same rationale as
    // CooldownsState::spell_cooldowns_index — APL has_aura/find_aura
    // queries on own_auras are 20–60 lookups per tick per spec; linear
    // scan over ~30-aura vector wastes CPU at scale.
    //
    // Note we don't index target_auras / victim_auras: target identity
    // can flip mid-tick (focus_target vs current_target), and the
    // vectors there are typically smaller (5–15 entries on the active
    // target). own_auras is the only one queried by static spell_id
    // from the rotation often enough to benefit. my_auras_on_others
    // gets its own composite-key index for the (target, spell_id) hot
    // path used by DoT-refresh predicates. Tier 3.3: sorted flat vector +
    // binary search (was unordered_map). own_auras may carry duplicate
    // spell_ids; the FIRST inserted index wins its lookup (finalize()'s
    // stable sort preserves push order among equal keys), matching the
    // prior unordered_map::emplace "first wins" behavior.
    FlatIndexMap<uint32> own_auras_index;
    // Key = (target_counter << 32) | spell_id (see builder for the exact
    // composition; matches BotSnapshotView). Value = index into
    // my_auras_on_others. Tier 3.3: sorted flat vector + binary search
    // (was unordered_map); first-inserted index wins on duplicate keys.
    FlatIndexMap<uint64> my_auras_on_others_index;

    void reset()
    {
        own_auras.clear();
        target_auras.clear();
        victim_auras.clear();
        my_auras_on_others.clear();
        own_auras_index.clear();
        my_auras_on_others_index.clear();
    }
};

// Active cast (the bot's own currently-channeling/casting spell). Used by
// rotations to avoid cancelling their own heal mid-cast, by view's
// is_ready()/can_cast_now() helpers, and by the /cast whisper diagnostic.
struct CastState
{
    bool   is_casting           = false;
    uint32 current_cast_spell_id = 0;
    Ms     current_cast_remaining{0};
    // Unit GUID this cast is aimed at (Empty for self-cast / AoE / no
    // target). Drives the healer cast-swap rule: if a slow Greater
    // Heal is in-flight on member A but member B dropped to 15% HP,
    // bot cancels and re-aims rather than letting B die mid-cast.
    ObjectGuid current_cast_target;
    // ID of the most recent spell the bot SUCCESSFULLY cast (set by
    // the IntentVisitor on CastSpellIntent → Result::Ok). Drives the
    // Monk Windwalker Combo Strikes mastery — repeating the same
    // melee ability breaks Mastery; the rotation must alternate.
    // Builder reads `ai->last_cast_spell_id()` once per snapshot.
    uint32 last_cast_spell_id   = 0;

    void reset() { *this = CastState{}; }
};

// Cooldowns. GCD remainder + the unified spell-cooldown table. The
// builder drains Player::SpellHistory into spell_cooldowns each tick
// (sees the real Recovery + CategoryRecovery rather than racing the
// live map). Charge-bearing spells share this vector with charges /
// max_charges populated on the same CooldownEntry — there is no
// separate charge_cooldowns table. Item-on-use cooldowns also surface
// here keyed by their proc spell_id (the spell the item triggers,
// which is what cast-rules query). Removing the legacy item_cooldowns
// / charge_cooldowns vectors saved ~48 bytes/bot/snapshot and
// eliminated a "field exists but reads 0" footgun (REFACTOR_2 cleanup).
struct CooldownsState
{
    Ms gcd_remaining{0};
    std::vector<CooldownEntry> spell_cooldowns;
    // O(1) cooldown lookup index. Keyed by spell_id, value is the index
    // into spell_cooldowns. Built by BotSnapshotBuilder::CopyCooldowns
    // after the vector is populated. FindCooldown was the hottest
    // linear scan in the snapshot view — APL ticks call is_ready /
    // cd_remaining / charges 30–80× per tick per spec, each one
    // walking up to 100 entries linearly. Hash lookup makes the read
    // O(1) at the cost of one map insertion per Build per cooldown
    // (~50 entries × 20K Builds/s = 1M map ops/s — cheap with reserve).
    // Vector kept as canonical store so the existing diag iterators
    // (BotInspector, .playerbot whyidle) keep working; the index is
    // strictly an acceleration structure. Tier 3.3: sorted flat vector +
    // binary search (was unordered_map). spell_cooldowns is de-duplicated
    // by spell_id inside CopyCooldowns, so keys are unique.
    FlatIndexMap<uint32> spell_cooldowns_index;

    void reset()
    {
        gcd_remaining = Ms{0};
        spell_cooldowns.clear();
        spell_cooldowns_index.clear();
    }
};

// Spellbook + talent / glyph layout. Read by APL rules ("does the bot know
// Glyph of X?") and the talent / talent-extend / starter-build maintenance
// rules. is_starter_build flips true when the active combat trait config
// carries the StarterBuild flag — distinguishes Blizzard's curated default
// from a hand-picked custom config so auto-extend doesn't overwrite
// custom builds.
struct SpellbookState
{
    std::vector<uint32> known_spells;     // sorted ascending — binary-search lookup
    std::vector<uint32> known_recipes;    // craft / discovery recipes the bot knows
    std::vector<uint32> active_talents;   // Talent.db2 ids on currently-active spec
    std::vector<uint32> active_glyphs;    // GlyphProperties.db2 ids slotted on active spec
    bool                is_starter_build = false;

    void reset()
    {
        known_spells.clear();
        known_recipes.clear();
        active_talents.clear();
        active_glyphs.clear();
        is_starter_build = false;
    }
};

// Profession / weapon / language / armour skill row. SkillLine.dbc id +
// raw points (0..1000+). value/max/step let gather rules check "have I
// got Herbalism ≥ N for this herb" without re-resolving via the player.
struct SkillEntry { uint16 skill_id; uint16 value; uint16 max; };

// Currency wallet row. currency_id mirrors CurrencyTypes.dbc id.
// quantity is current balance; weekly_quantity is the rolling 7-day
// earned (relevant for capped currencies). Walks enumerate
// sCurrencyTypesStore + Player::GetCurrencyQuantity, since the storage
// is private. Zero-quantity rows are skipped to keep the vector tight.
struct CurrencyEntry { uint32 currency_id; uint32 quantity; };

// Reputation standing row. faction_id is FactionEntry id; standing is
// raw signed reputation; rank is 0..7 mirroring ReputationRank
// (Hated..Exalted, with Paragon as 8 in modern WoW). Only visible
// non-zero standings are surfaced (ReputationMgr filters by visibility
// internally), so the vector stays ~30-60 entries on a maxed-out bot.
struct ReputationEntry { uint32 faction_id; int32 standing; uint8 rank; };

// Bot progression rollup. Trained skills, currency wallet, faction
// reputations. Builder writes; rules read via view accessors. Separated
// out so the snapshot publisher can re-use the cached vector across
// ticks for AI threads where the underlying player object hasn't
// gained new entries (BotSetupPipeline holds the long-term cache;
// snapshot replays into the vector when invalidated).
struct ProgressionState
{
    std::vector<SkillEntry>      skills;
    std::vector<CurrencyEntry>   currencies;
    std::vector<ReputationEntry> reputations;

    void reset()
    {
        skills.clear();
        currencies.clear();
        reputations.clear();
    }
};

// Spec stat-weight rollup. 12-entry array indexed by StatIndex (Str=0..
// Speed=11) plus a weapon-DPS weight. Resolved once per snapshot from
// StatPriorityFor(cls, spec) so the equip-upgrade rule reads the row
// directly without a table lookup per tick. Weapon DPS weight is 0 for
// casters who don't melee.
struct StatWeightsState
{
    std::array<float, 12> spec_stat_weights{};
    float                 spec_weapon_dps_weight = 0.0f;

    void reset() { *this = StatWeightsState{}; }
};

// Weighted fit score for an equippable item from its (already-populated) stat
// block + the bot's spec weights. Higher = better. Used by BOTH the auto-equip
// rule and the upgrades_pending counter so a quest GREEN beats a starter WHITE
// even when WoW 12.0 level-scaling collapses both to the same effective
// item_level — the stat allocation breaks the tie that a pure item_level
// comparison silently lost (`it.item_level <= cur.item_level`), which left
// organic bots stuck at ItemLevel 1 wearing starters while quest greens sat
// unequipped in their bags. Pure arithmetic on cached snapshot data — safe on
// the synchronous build thread (no live Item / DB2 lookups).
inline float EquipFitScore(ItemStatBlock const& blk, uint16 item_level,
                           StatWeightsState const& w)
{
    float v = static_cast<float>(item_level);
    for (size_t i = 0; i < blk.stats.size(); ++i)
        v += static_cast<float>(blk.stats[i]) * w.spec_stat_weights[i];
    v += static_cast<float>(blk.weapon_dps_x10) * w.spec_weapon_dps_weight;
    return v;
}

// Bag / equipment summary. Cached aggregates the AI consults to gate
// vendor-visit and equip-upgrade decisions without re-walking equipped[] /
// bag_items every tick. bag_free_slots is the count of empty inventory
// slots across all bags (cached because GetFreeInventorySlotCount walks
// 19+ slot positions). smallest_bag_capacity is the smallest equipped
// bag's row count (0 = at least one empty bag slot — prioritize filling
// it). has_empty_bag_slot mirrors that "any empty bag" condition for the
// bag-upgrade vendor branch. upgrades_pending is the number of bag_items
// that strictly out-ilvl their current equipped slot — the equip-upgrade
// rule's "is there work to do" gate.
struct BagsState
{
    uint8 bag_free_slots         = 0;
    uint8 smallest_bag_capacity  = 0;
    bool  has_empty_bag_slot     = false;
    uint8 upgrades_pending       = 0;
    // Per-slot equipped-bag info, index 0..3 = INVENTORY_SLOT_BAG_START+i
    // (slots 30-33; the reagent bag slot 34 is intentionally excluded —
    // it takes only reagent-bag subclass containers). capacity 0 = no bag
    // equipped in that slot; subclass 0xFF = none, 0 = normal container.
    // Drives idle:equip_bag_upgrade's destination pick: fill an empty
    // slot first, else replace the smallest NORMAL bag.
    std::array<uint8, 4> equipped_bag_capacity{};
    std::array<uint8, 4> equipped_bag_subclass{ 0xFF, 0xFF, 0xFF, 0xFF };

    // Pure-POD; default-assign restores the 0xFF subclass defaults exactly.
    void reset() { *this = BagsState{}; }
};

// Secondary stat rollup. Crit / haste / mastery / versatility — percentage
// rating for the bot's relevant attack school (melee for physical classes,
// spell for casters). Sourced via Player::GetRatingBonusValue, which
// already applies diminishing returns. Stored as int16 fixed-point (×100)
// to carry one decimal without floats: 1234 = 12.34%. Used by APL rules
// that need to weigh haste-favored vs crit-favored rotation branches.
// Resilience attenuates incoming player damage; PvP power scales outgoing
// player damage — surfaced for PvP-aware rotations (defensive CD priority,
// kite distance, etc.).
struct SecondaryStatsState
{
    int16 crit_pct_x100        = 0;
    int16 haste_pct_x100       = 0;
    int16 mastery_pct_x100     = 0;
    int16 versatility_pct_x100 = 0;
    int16 resilience_pct_x100  = 0;
    int16 pvp_power_pct_x100   = 0;

    void reset() { *this = SecondaryStatsState{}; }
};

// Vendor-visit pre-computed needs. Snapshot builder evaluates repair cost +
// the 6-bit phase mask once per snapshot so the vendor-visit FSM rule can
// short-circuit instead of re-evaluating durability/gold/bag/food/etc
// every tick.
struct VendorVisitState
{
    // Estimated repair cost in copper, summed over all equipped items'
    // CalculateDurabilityRepairCost(discount=1.0). Excludes guild-bank
    // and reputation discounts; the rule applies a 1.2× safety margin
    // against bot.gold so we never trip "not enough money".
    uint32 estimated_repair_cost = 0;

    // Vendor-visit phase bitmask. Bit set = that phase has work to do
    // when the bot reaches a vendor in interact range. Bit map:
    //   bit0 — repair    (gear durability < 70% AND gold ≥ cost × 1.2)
    //   bit1 — sell trash (free slots ≤ 4 OR has soulbound junk)
    //   bit2 — buy bigger bag (smallest equipped bag < target capacity AND gold sufficient)
    //   bit3 — buy food/drink (count < target AND gold sufficient)
    //   bit4 — buy bandages (count < target AND gold sufficient)
    //   bit5 — buy class reagents (count < target AND gold sufficient)
    uint8 phases_pending = 0;

    void reset() { *this = VendorVisitState{}; }
};

// Consumables — stack-quantity counts the AI checks for OOC restock and the
// vendor-visit FSM gate. Built by the snapshot builder by walking bag_items
// and sniffing item class / subclass. Health vs mana potion split is filled
// only when the potion's primary spell effect is heal vs restore (so healers
// don't carry surplus health potions and physical classes don't carry mana).
struct ConsumablesState
{
    uint16 health_potion_count = 0;
    uint16 mana_potion_count   = 0;
    uint16 food_drink_count    = 0;   // CONSUMABLE / FOOD_DRINK (subclass 5)
    uint16 potion_count        = 0;   // CONSUMABLE / POTION (subclass 1)
    uint16 bandage_count       = 0;   // CONSUMABLE / BANDAGE (subclass 7)

    void reset() { *this = ConsumablesState{}; }
};

struct BgState
{
    // Up to PLAYER_MAX_BATTLEGROUND_QUEUES (3 in 12.0) active queues per
    // bot. AI uses count > 0 to gate against re-queuing the same BG.
    std::vector<BgQueueEntry> queues;

    // True when the bot is inside a battleground instance (vs queued for /
    // not at all). Drives BG-specific rule branches (don't hearth, don't
    // accept LFG, etc).
    bool in_battleground = false;

    // BattlemasterList.dbc id of the active BG. Authoritative values are
    // in SharedDefines.h:6856+ and each BattlegroundScript::bg_type_id()
    // override. Examples: 1 = AV, 2 = WSG, 3 = AB, 7 = EotS, 9 = SoTA,
    // 30 = IoC; modern brackets 108 = TP, 120 = BfG, 699 = TK, 708 = SM,
    // 754 = DG, 894 = SS. Zero when not in a BG. (Prior comment had
    // every id shifted off-by-one — corrected 2026-05-21.)
    uint16 current_type_id = 0;

    // ---- Team-coordinator order (BG audit N60) ----
    // Published by BgTeamCoordinator (world thread, ~750ms cadence) via
    // the builder. kind == None when no coordinator plan covers this bot
    // (coordinator disabled / human-led team / data missing) — consumers
    // MUST fall back to the per-bot greedy role logic so behavior
    // degrades gracefully instead of freezing.
    struct BgOrder
    {
        enum Kind : uint8
        {
            None        = 0,
            AttackNode  = 1,   // capture/assault the node at x/y/z
            DefendNode  = 2,   // hold / stop-the-cap at x/y/z
            EscortFC    = 3,   // stay on the friendly carrier (focus guid)
            HuntEFC     = 4,   // intercept the enemy carrier (focus guid)
            PickupFlag  = 5,   // become the carrier: go grab flag/orb at x/y/z
            CarryHome   = 6,   // carrier: run the score point at x/y/z
            Regroup     = 7,   // rally at x/y/z (GY-camp break, opening split)
            PushEndgame = 8,   // boss/gate push at x/y/z
        };
        uint8      kind = None;
        float      x = 0.f, y = 0.f, z = 0.f;
        ObjectGuid focus;          // carrier / kill-target when relevant
        uint8      squad = 0;      // cosmetic squad index for diagnostics
        uint32     target_entry = 0;  // creature entry to engage (AV captain/general)
    };
    BgOrder order;

    // Team scores; thresholds vary by BG (1500 / 1600 / 2000 typical).
    uint32 score_alliance     = 0;
    uint32 score_horde        = 0;

    // Countdown until match end.
    uint32 time_remaining_sec = 0;

    // 2 = STATUS_WAIT_JOIN (prep), 3 = STATUS_IN_PROGRESS (live). Bots
    // gate aggressive objective play on status == IN_PROGRESS to avoid
    // running through unopened start gates.
    uint8  status         = 0;
    uint32 start_delay_ms = 0;
    // Wall-clock elapsed time since the BG entered IN_PROGRESS (gates
    // dropped). Distinct from `time_remaining_sec` which is the
    // countdown to BG end and stays 0 during active play. Drives time-
    // gated arena hazards (RoV pillar elevators rise ~60s in) and any
    // future "first N seconds of match" rule. 0 outside BGs / during
    // prep. TC source: Battleground::GetInProgressDuration().
    uint32 in_progress_ms = 0;

    // CTF flag carriers + their world positions (resolved at builder time).
    // Scalar fields hold the FIRST detected carrier per side (back-compat
    // for WSG/TP/EotS/BfG/DG semantics where the BG has at most one
    // carrier per side). Kotmogu has up to 4 simultaneous carriers per
    // side — the all_* vectors below carry the full set.
    ObjectGuid friendly_flag_carrier;
    ObjectGuid enemy_flag_carrier;
    int32      friendly_carrier_hp_pct = 0;
    int32      enemy_carrier_hp_pct = 0;
    float      friendly_carrier_x = 0.f;
    float      friendly_carrier_y = 0.f;
    float      friendly_carrier_z = 0.f;
    float      enemy_carrier_x    = 0.f;
    float      enemy_carrier_y    = 0.f;
    float      enemy_carrier_z    = 0.f;

    // Full carrier sets (Kotmogu: up to 4 each side). Empty for BGs
    // with single-carrier semantics (the scalars above are sufficient).
    // Order is insertion order from the builder's scan; not stable
    // across snapshots, so consumers needing per-bot stable mapping
    // should hash by ObjectGuid.
    std::vector<ObjectGuid> all_friendly_carriers;
    std::vector<ObjectGuid> all_enemy_carriers;

    // Capture-point states (live ownership view).
    std::vector<BgNodeState> node_states;

    // SoTA round-state: which team is attacking the relic this round
    // (0=Alliance, 1=Horde, -1=not in SoTA).
    int8 sota_attacker_team = -1;

    // SoTA gate state. Indexed by GateId enum below; populated from
    // worldstates 3614 (Purple) / 3617 (Red) / 3620 (Blue) / 3623 (Green)
    // / 3638 (Yellow) / 3849 (Ancient). Values mirror TC's
    // BG_SA_GateState enum: 0=unknown/not-on-this-map, 1=OK, 2=damaged,
    // 3=destroyed. Lets the SoTA script pick the next undestroyed gate
    // as the intermediate Attacker target instead of always pointing
    // at the Ancient breach.
    enum SotaGateId : uint8 {
        SotaGateGreen = 0,   // tier 1
        SotaGateBlue,        // tier 1
        SotaGateRed,         // tier 2
        SotaGatePurple,      // tier 2
        SotaGateYellow,      // tier 3
        SotaGateAncient,     // tier 4 (final)
        SotaGateCount
    };
    uint8 sota_gate_state[SotaGateCount] = { 0, 0, 0, 0, 0, 0 };
    // Cache key in BotAI::BgAdviceCache packs each gate state into 2 bits;
    // verify SotaGateCount stays within uint32 (≤16 gates).
    static_assert(SotaGateCount <= 16, "sota_gate_state cache-key uint32 overflow");

    // IoC keep gate destruction state — 3 gates per faction (Front /
    // West / East). Populated from worldstates 4317-4328 in
    // src/server/scripts/Battlegrounds/IsleOfConquest/isle_of_conquest.h:
    //   FRONT_H 4317 (closed) / 4322 (open/destroyed)
    //   WEST_H  4318 / 4321
    //   EAST_H  4319 / 4320
    //   FRONT_A 4328 / 4323
    //   WEST_A  4327 / 4324
    //   EAST_A  4326 / 4325
    // Each ioc_gate_destroyed entry is non-zero when the gate is down.
    // Indexed by IocGateId enum. IoC script reads this to route the
    // Attacker push to the nearest standing gate before the General coord.
    enum IocGateId : uint8 {
        IocGateFrontA = 0,   // Alliance Front
        IocGateWestA,
        IocGateEastA,
        IocGateFrontH,       // Horde Front
        IocGateWestH,
        IocGateEastH,
        IocGateCount
    };
    uint8 ioc_gate_destroyed[IocGateCount] = { 0, 0, 0, 0, 0, 0 };
    // Cache key packs each gate-destroyed bool into 1 bit (uint8 packing);
    // verify count stays within byte (≤8 gates).
    static_assert(IocGateCount <= 8, "ioc_gate_destroyed cache-key uint8 overflow");

    // AV captain alive-state. Balinda Stonecaster (entry 11949, Alliance
    // captain at Stonehearth Outpost) + Captain Galvangar (entry 11947,
    // Horde captain at Iceblood Garrison). Each kill drops +100
    // reinforcements to the killing team (the largest single-kill prize
    // in AV). Once dead, the AV script drops them from `nodes[]` so bots
    // stop pathing to a corpse.
    //
    // Default true on non-AV maps (no captain creature there → assume
    // alive). The script gates consumption on bg_type_id == 1 (AV) so
    // the "alive" reads outside AV are harmless.
    bool av_balinda_alive   = true;
    bool av_galvangar_alive = true;

    // BG-role assigned to this bot by the active BattlegroundScript (per
    // BgPlanAdvice::role_by_slot — Tank/Healer/Defender/Attacker/Roamer/
    // Free). Encoded as the BgRole enum's underlying uint8; consumers
    // compare to BgRole::* values. Default 0 (Free) when outside a BG or
    // when the per-bot BgAdviceCache is cold (first BG-tick before
    // State_Idle's BgDispatch fills it). Wired 2026-05-21 — reads
    // BotAI::bg_advice_cache().cached.role_by_slot indexed by
    // BotAI::formation_slot() (with a guid-hashed fallback when slot=0).
    // Class-aware Healer fixup: bots without a healer spec land in
    // BgRole::Roamer even if the script's slot map says Healer (keeps
    // the AddonControl dashboard tally honest).
    uint8 bg_role = 0;

    // Recycle-pool reset. Clears the four vectors (retain capacity) and
    // restores every scalar/array/nested-struct to its CONSTRUCTED default
    // — note av_*_alive default to TRUE and sota_attacker_team to -1, so a
    // blanket memset would be WRONG here. node_states' inner BgNodeState
    // strings are freed by the outer clear() (acceptable; rebuilt next BG).
    void reset()
    {
        queues.clear();
        node_states.clear();
        all_friendly_carriers.clear();
        all_enemy_carriers.clear();

        in_battleground = false;
        current_type_id = 0;
        order = BgOrder{};
        score_alliance = 0;
        score_horde = 0;
        time_remaining_sec = 0;
        status = 0;
        start_delay_ms = 0;
        in_progress_ms = 0;
        friendly_flag_carrier = ObjectGuid::Empty;
        enemy_flag_carrier = ObjectGuid::Empty;
        friendly_carrier_hp_pct = 0;
        enemy_carrier_hp_pct = 0;
        friendly_carrier_x = 0.f; friendly_carrier_y = 0.f; friendly_carrier_z = 0.f;
        enemy_carrier_x = 0.f; enemy_carrier_y = 0.f; enemy_carrier_z = 0.f;
        sota_attacker_team = -1;
        for (uint8 i = 0; i < SotaGateCount; ++i) sota_gate_state[i] = 0;
        for (uint8 i = 0; i < IocGateCount; ++i)  ioc_gate_destroyed[i] = 0;
        av_balinda_alive = true;
        av_galvangar_alive = true;
        bg_role = 0;
    }
};

// Per-bot play-archetype projection. The authoritative BotArchetype lives on
// BotAI; this is the read-only slice idle rules consult so they can gate on
// "what kind of player is this" without crossing into BotAI from a worker
// thread. Populated by the builder from ai->archetype(). See BotArchetype.h.
struct ArchetypeState
{
    // Index into the curated archetype table (ArchetypeId). 0 = CasualSolo.
    uint8 archetype_id = 0;
    // EconProfile underlying value (0=Hoarder, 1=Balanced, 2=Reseller).
    uint8 econ_profile = 1;
    // Highest activity_weights slot (ArchetypeActivity::* — Solo/Group/Pvp/
    // Profession/Social). Lets a rule cheaply bias toward the dominant mode.
    uint8 dominant_activity = 0;
    // Role emphasis [Tank, Healer, Dps], sums ~1.0. Mirrored so a rule can
    // reason about role lean without a BotAI hop.
    std::array<float, 3> role_affinity{ 0.f, 0.f, 1.f };
    // Intended session length (minutes). Stored for the future session-rhythm
    // logout layer; no live consumer yet.
    uint16 target_session_minutes = 90;

    // Pure-POD; default-assign restores the role_affinity{0,0,1} and
    // target_session_minutes=90 defaults exactly.
    void reset() { *this = ArchetypeState{}; }
};

// Craft-order board projection (#4B-2(a)). Read-only slice of
// Fleet::CraftOrderBoard surfaced into each bot's snapshot so economy idle
// rules can decide WITHOUT locking the board (the world thread populates this
// from the board during Build; rules read it on the AI worker thread). All
// fields default to "no order" so an un-populated snapshot is fail-closed.
//
// my_open_order_count  : how many Open orders this bot has POSTED as requester
//                        (so the post rule can cap concurrent outstanding
//                        orders per bot instead of spamming the board).
// has_claimable_order  : true when the board holds at least one Open order whose
//                        recipe THIS bot knows (verified board-side via the
//                        crafter's spellbook), i.e. the claim rule has work to do.
// claimed_*            : the single order this bot currently owns as CRAFTER
//                        (status Claimed). claimed_order_id == 0 means none.
// want_*               : ONE crafted intermediate this bot WANTS but cannot make
//                        itself (#4B-2(a) part 2 post rule). Resolved world-thread
//                        in the builder: a reagent the bot is short on for its own
//                        known, still-skillable recipes, which is itself the
//                        product of a recipe spell the bot does NOT know (so a
//                        DIFFERENT fleet bot must craft it). want_spell_id is that
//                        producing recipe spell (the order's recipe key — only a
//                        bot that KNOWS it can claim), want_item_entry the product
//                        the requester needs, want_quantity the shortfall, and
//                        want_payment_copper a market-derived fair payment (the
//                        #4B-1 fair-value ceiling per unit * quantity). All zero
//                        when the bot has no such want (the common case), so the
//                        post rule is fail-closed on an unpopulated snapshot.
struct CraftOrderState
{
    uint32 my_open_order_count   = 0;
    bool   has_claimable_order   = false;
    uint64 claimed_order_id      = 0;
    uint32 claimed_spell_id      = 0;
    uint32 claimed_item_entry    = 0;
    uint32 claimed_quantity      = 0;
    uint64 claimed_requester_low = 0;
    // ---- appended POD (#4B-2(a) part 2 post-want signal) ----
    uint32 want_spell_id         = 0;
    uint32 want_item_entry       = 0;
    uint32 want_quantity         = 0;
    uint64 want_payment_copper   = 0;

    void reset() { *this = CraftOrderState{}; }
};

struct BotSnapshot
{
    // Re-export the namespace-scope sub-struct field types under
    // BotSnapshot:: so existing call sites that wrote
    // BotSnapshot::BgQueueEntry / BotSnapshot::BgNodeState keep
    // compiling. Cheap using-aliases — no runtime cost.
    using BgQueueEntry      = Playerbot::BgQueueEntry;
    using BgNodeState       = Playerbot::BgNodeState;
    using SelfTeleportSpell = Playerbot::SelfTeleportSpell;
    // BgState::SotaGateId enum + values surfaced at BotSnapshot::
    // scope for consistency with BgNodeState etc.
    using SotaGateId        = Playerbot::BgState::SotaGateId;
    static constexpr SotaGateId SotaGateGreen   = Playerbot::BgState::SotaGateGreen;
    static constexpr SotaGateId SotaGateBlue    = Playerbot::BgState::SotaGateBlue;
    static constexpr SotaGateId SotaGateRed     = Playerbot::BgState::SotaGateRed;
    static constexpr SotaGateId SotaGatePurple  = Playerbot::BgState::SotaGatePurple;
    static constexpr SotaGateId SotaGateYellow  = Playerbot::BgState::SotaGateYellow;
    static constexpr SotaGateId SotaGateAncient = Playerbot::BgState::SotaGateAncient;
    static constexpr SotaGateId SotaGateCount   = Playerbot::BgState::SotaGateCount;
    using IocGateId = Playerbot::BgState::IocGateId;
    static constexpr IocGateId IocGateFrontA = Playerbot::BgState::IocGateFrontA;
    static constexpr IocGateId IocGateWestA  = Playerbot::BgState::IocGateWestA;
    static constexpr IocGateId IocGateEastA  = Playerbot::BgState::IocGateEastA;
    static constexpr IocGateId IocGateFrontH = Playerbot::BgState::IocGateFrontH;
    static constexpr IocGateId IocGateWestH  = Playerbot::BgState::IocGateWestH;
    static constexpr IocGateId IocGateEastH  = Playerbot::BgState::IocGateEastH;
    static constexpr IocGateId IocGateCount   = Playerbot::BgState::IocGateCount;


    // Versioning
    SnapshotVer version = 0;
    BotId       bot_id  = 0;
    TickId      world_tick = 0;
    // GameTimeMS at publication. Lets the AI worker compute snapshot age
    // ("how stale is this fact base?") and the /age whisper report it.
    // Mirrors GameTime::GetGameTimeMS at builder-exit; uint32 so it
    // matches the underlying TC clock.
    uint32      published_at_ms = 0;

    // Identity — REFACTOR_2 sub-struct. `guid` stays at top-level because
    // it's the primary key. See IdentityState above for per-field semantics.
    ObjectGuid     guid;
    IdentityState  identity{};

    // Core vital stats (hp, power[], combat flags, CC, PvP context) —
    // REFACTOR_2 sub-struct. See VitalsState above for per-field semantics.
    VitalsState vitals{};

    // Position — REFACTOR_2 sub-struct. See PositionState above.
    PositionState position{};
    // Area / zone context — REFACTOR_2 sub-struct. See AreaState above.
    AreaState area{};
    // Instance / map context — REFACTOR_2 sub-struct. See InstanceContextState above.
    InstanceContextState instance_ctx{};
    // Movement flags — REFACTOR_2 sub-struct. See MovementState above.
    MovementState movement{};
    // Transport / liquid environment — REFACTOR_2 sub-struct. See
    // EnvironmentState above.
    EnvironmentState environment{};
    // Travel state (mount + taxi mask + recommended-flight + portal anchor +
    // homebind + hearthstone + self-teleport). See TravelState above for
    // per-field semantics. The taxi_mask + recommended_taxi_* + nearest_portal_
    // anchor_* + next_hop_dest_map + homebind_* + has_hearthstone + hearthstone_
    // cd_ms + self_teleport_spells fields that used to live further down in
    // BotSnapshot are now members of `travel`.
    TravelState travel{};

    // (in_combat / combat_duration / is_alive / is_stunned / is_silenced /
    //  is_rooted / is_pvp / is_ffa_pvp / is_sanctuary migrated into the
    //  VitalsState `vitals` sub-struct above — REFACTOR_2.)

    // Death recovery — REFACTOR_2 sub-struct. See DeathState above.
    DeathState death{};
    // Inbound social events — REFACTOR_2 sub-struct. See SocialEventsState
    // above. View accessors (has_group_invite(), has_duel_request(), etc.)
    // forward to s.social_events.<field>.
    SocialEventsState social_events{};
    // Guild state — REFACTOR_2 sub-struct rollout. See GuildState above
    // for per-field semantics. Aliased view accessors (s.guild_id() etc.)
    // remain unchanged; their bodies forward to s.guild.<field>.
    GuildState guild{};
    // Bot's owner character name resolved at snapshot time (empty when
    // unowned or the cache lookup fails). Used by mat-share-to-owner mail
    // rule so the AI worker can address mail without round-tripping the
    // owner registry + character cache from the worker thread.
    std::string owner_name;
    // (duel + quest-share + trade + summon migrated into social_events above)
    // LFG queue / role-check / proposal / vote-kick state — REFACTOR_2
    // sub-struct rollout. See LfgState above for per-field semantics.
    LfgState   lfg{};

    // (completed_quest_count migrated into quest_log.completed_quest_count — REFACTOR_2)

    // (lfg_vote_kick_active migrated into lfg.vote_kick_active; field
    // here removed as dead duplicate — builder writes lfg.vote_kick_active
    // and the view forwards through.)

    // Auction-house rollup — REFACTOR_2 sub-struct. See AuctionState above
    // for per-vector semantics.
    using AhCompetingEntry = Playerbot::AhCompetingEntry;
    using OwnedAuction     = Playerbot::OwnedAuction;
    using BuyableListing   = Playerbot::BuyableListing;
    using BuyableCommodity = Playerbot::BuyableCommodity;
    AuctionState auction{};
    // Combat targets — REFACTOR_2 sub-struct. See CombatTargetsState above.
    CombatTargetsState combat{};

    // Auras — REFACTOR_2 sub-struct. See AurasState above for per-vector
    // semantics. Aliased view accessor `OutboundAura` preserved as a
    // typedef so existing call sites keep compiling.
    using OutboundAura = AurasState::OutboundAura;
    AurasState auras{};

    // Active cast — REFACTOR_2 sub-struct. See CastState above.
    CastState cast{};

    // Cooldowns — REFACTOR_2 sub-struct. See CooldownsState above.
    CooldownsState cooldowns{};

    // Inventory & equipment — REFACTOR_2 sub-struct. See InventoryState
    // above (gold, equipped[19], bag_items, average_item_level).
    InventoryState inventory{};
    // Bag / upgrade summary — REFACTOR_2 sub-struct. See BagsState above.
    BagsState bags{};
    // Per-(class, spec) stat weights used by the equip-upgrade rule —
    // REFACTOR_2 sub-struct. See StatWeightsState above.
    StatWeightsState stat_weights{};

    // (equipped[19], bag_items, average_item_level migrated into inventory above)
    // Secondary stats (crit/haste/mastery/versatility + resilience/PvP-power)
    // — REFACTOR_2 sub-struct. See SecondaryStatsState above for per-field
    // semantics.
    SecondaryStatsState secondary_stats{};
    // (upgrades_pending migrated into bags.upgrades_pending above)
    // Aggregate stack-quantity counts of consumable categories the AI cares
    // about for OOC restock decisions. Filled by the builder by walking
    // bag_items and sniffing item template GetClass/GetSubClass — saves the
    // AI worker thread from looking up ItemTemplates for every bag item just
    // to know "do I have enough food?". Populated for the categories the
    // auto-restock rules actually consult; add more as new rules need them.
    // (smallest_bag_capacity / has_empty_bag_slot migrated into bags above)

    // Vendor-visit pre-computed needs — REFACTOR_2 sub-struct. Repair cost
    // and the visit-phase bitmask. See VendorVisitState above for per-field
    // semantics.
    VendorVisitState vendor_visit{};

    // Consumable counters — REFACTOR_2 sub-struct. Stack-quantity aggregates
    // built by the snapshot builder walking bag_items + sniffing item class /
    // subclass. Used by OOC restock rules and the vendor-visit FSM to gate
    // "do I need food/potions/bandages right now?" without re-walking the
    // bag per tick. Health/mana split is populated only when the potion's
    // primary spell effect is a heal or mana restore (so healers don't
    // hoard health potions and physical classes don't hoard mana potions).
    ConsumablesState consumables{};

    // Spellbook + talents — REFACTOR_2 sub-struct. See SpellbookState above
    // for per-field semantics. View accessors (known_spells(),
    // known_recipes(), active_talents(), active_glyphs(), is_starter_build())
    // forward to s.spellbook.<field>.
    SpellbookState spellbook{};

    // Progression rollup — REFACTOR_2 sub-struct. Trained skills,
    // currency wallet, faction reputations. See ProgressionState above
    // for per-vector semantics. Aliased view accessors (s.skills(),
    // s.currencies(), s.reputations()) forward to s.progression.<vec>.
    using SkillEntry      = Playerbot::SkillEntry;
    using CurrencyEntry   = Playerbot::CurrencyEntry;
    using ReputationEntry = Playerbot::ReputationEntry;
    ProgressionState progression{};

    // Group summary — REFACTOR_2 sub-struct. View accessors group_guid() /
    // my_role() forward to s.group.<field>.
    GroupMembershipState group{};

    // Quest log + currently-focused objective + resolved waypoint
    // resolutions (POI / AreaTrigger / Tool) + cross-quest actionable index
    // — REFACTOR_2 sub-struct. See QuestLogState in the namespace block above.
    using QuestObjectivePoi   = Playerbot::QuestObjectivePoi;
    using QuestAreaTrigger    = Playerbot::QuestAreaTrigger;
    using QuestTool           = Playerbot::QuestTool;
    using ActionableObjective = Playerbot::ActionableObjective;
    QuestLogState quest_log{};

    // Open-gossip snapshot. When the bot has emitted interact_with_npc and
    // the server has populated PlayerTalkClass with a menu, the builder
    // captures (a) the NPC the menu belongs to and (b) the menu options.
    // Used by the 2-phase idle:quest_talk rule: a TALKTO objective whose
    // Hello path doesn't credit needs the bot to *select* the right menu
    // option (typically tagged GossipOptionNpc::None for "talk" entries
    // that route to a quest-credit script). Empty when no gossip is open.
    // Gossip menu — REFACTOR_2 sub-struct. See GossipState above.
    using GossipMenuOption = Playerbot::GossipMenuOption;
    GossipState gossip{};

    // Resolved quest turn-ins: pairs of {giver_guid, quest_id} for nearby
    // questgivers (creature OR game object) that can accept a complete
    // quest from the bot's log. Built by walking nearby_friends +
    // nearby_objects in the snapshot builder against the bot's complete
    // quests via Creature/GameObject::hasInvolvedQuest. AI iterates and
    // emits QuestCompleteIntent for the first matching entry — no need
    // to round-trip the live world from the worker thread.
    using QuestTurnIn = Playerbot::QuestTurnIn;
    // Resolved quest offers: pairs of {giver_guid, quest_id} for nearby
    // questgivers that have a quest the bot doesn't yet hold and is
    // eligible for. Same shape as quest_turnins. AI emits QuestAcceptIntent.
    // (quest_turnins / quest_offers migrated into quest_discovery below)
    // Bag items that start a quest the bot hasn't picked up yet (modern WoW
    // "letter from X" / "mysterious package" pickups). Each entry is the
    // item entry + quest id; AI fires UseItemByEntryIntent to trigger the
    // pickup dialog and the snapshot's normal quest_offers accept path
    // takes it from there. Filtered by CanTakeQuest in the builder so we
    // don't surface items the bot can't actually start.
    using StartingItem = Playerbot::StartingItem;

    // World-quest discovery index. Modern WoW (Legion+) tags repeatable
    // outdoor end-game quests with QUEST_FLAGS_EX_IS_WORLD_QUEST; givers
    // appear as creatures or interactable objects on the bot's current
    // map. Builder walks `nearby_friends` + `nearby_objects`, asks
    // ObjectMgr for each entry's quest relations, and surfaces (giver,
    // quest_id, position, reward digest) for any IsWorldQuest() that
    // either: (a) the bot can take (CanTakeQuest), or (b) is already
    // accepted and complete (eligible for turn-in). The two new
    // idle:wq_accept / idle:wq_turnin rules iterate this list directly,
    // and the `wq` whisper command reports counts/distances. Capped to
    // keep snapshot bounded; closer offers/turnins win the cap budget.
    //
    // Field semantics:
    //   * giver        : creature OR game object guid; use look-up against
    //                    nearby_friends / nearby_objects for live position.
    //   * quest_id     : quest_template entry.
    //   * type         : 0 = offer (CanTakeQuest passed, not in log), 1 =
    //                    turn-in (state == 1 in bot's quest log).
    //   * x/y/z        : giver world position at capture time. Saves the
    //                    rule from re-walking nearby_* to look up coords.
    //   * area_id      : giver's AreaID (for "in current zone" filters).
    //   * reward_currency_id : first non-zero RewardCurrencyId[] slot, or
    //                    0 if the WQ rewards no currency. Stored as a
    //                    digest — the actual reward array is not surfaced.
    //   * reward_money : Quest::GetMaxMoneyReward() at builder time (the
    //                    max-level reward; bots are typically max-level
    //                    when world quests are accessible).
    using WorldQuestEntry = Playerbot::WorldQuestEntry;
    // Quest discovery rollup — REFACTOR_2 sub-struct. See QuestDiscoveryState above.
    QuestDiscoveryState quest_discovery{};

    // Scenario step tracking. Populated when the bot's current map is an
    // InstanceMap with a live InstanceScenario attached (Map::IsScenario()
    // && InstanceMap::GetInstanceScenario()). Drives future scenario-aware
    // dispatch (the dungeon-style Idle/InGroup rules can read this to
    // know "this bot is mid-step 2/3 and should not leave"). Empty
    // (scenario_id == 0) on every other map.
    //
    //   * scenario_id            : ScenarioEntry.db2 id.
    //   * step_id                : ScenarioStepEntry.db2 id of the
    //                              currently-active step. 0 when the
    //                              scenario hasn't started a step yet.
    //   * current_step_progress  : count of completed criteria so far.
    using ScenarioStepInfo = Playerbot::ScenarioStepInfo;
    // scenario_step lives inside quest_log. Keep the alias for AI callers
    // that build a ScenarioStepInfo locally (e.g. /dungeon callouts).

    // Active group/master loot rolls awaiting the bot's vote — REFACTOR_2
    // sub-struct. See LootRollsState / LootRollEntry above for per-row
    // semantics. AI emits LootRollIntent.
    using LootRollEntry = Playerbot::LootRollEntry;
    LootRollsState loot{};

    // Mailbox — REFACTOR_2 sub-struct. See MailboxState above.
    MailboxState mailbox{};

    // Battleground state — Refactor #2 sub-struct rollout (pilot subsystem).
    // PvE group-coordinator order (dungeon/raid). Default-inactive when
    // the bot's group has no coordinator plan. See PveOrder above.
    PveOrder pve_order{};

    // All BG queue + live-match fields live here. See BgState definition above
    // for per-field semantics; the legacy flat fields (bg_status / bg_queues /
    // bg_friendly_carrier_* / bg_node_states / etc.) have been removed in
    // favour of `bg.<field>`.
    BgState bg{};

    // Vehicle attachment + seat/ability/entry — REFACTOR_2 sub-struct. See
    // VehicleState above.
    VehicleState vehicle{};

    // Bank capacity — REFACTOR_2 sub-struct. See BankState above.
    BankState bank{};

    // Nearby GameObjects within ~30yd — REFACTOR_2 sub-struct. See
    // NearbyObjectsState / NearbyObject above.
    using NearbyObject = Playerbot::NearbyObject;
    NearbyObjectsState world_objects{};

    // Active movement waypoint — REFACTOR_2 sub-struct. See PathState above.
    PathState path{};

    // Path-block telemetry — Builder copies BotAI's path_blocked_count /
    // last_path_blocked_ms. Used by travel/quest rules to detect anchor
    // wedge and fall through. See PathTelemetryState above.
    PathTelemetryState path_telemetry{};

    // Dungeon execution context — REFACTOR_2 sub-struct. See DungeonExecState
    // above for per-field semantics. Also holds the coarser
    // active_encounter_npc_id / active_encounter_phase pair used by per-script
    // dungeon hooks.
    DungeonExecState dungeon_exec{};

    // Pet (Hunter / Warlock / DK / Mage's Water Elemental) + Hunter stable
    // inventory — REFACTOR_2 sub-struct. See PetState above.
    using StablePet = Playerbot::StablePet;
    PetState pet{};

    // Per-bot play archetype projection (#4A). Appended at the END of the
    // struct (POD-append rule). Populated by the builder from ai->archetype()
    // so idle rules can gate on archetype without a thread crossing. See
    // ArchetypeState above.
    ArchetypeState archetype{};

    // Bot-to-bot craft-order board projection (#4B-2(a)). Appended at the END
    // of the struct (POD-append rule). Populated by the builder from
    // Fleet::CraftOrderBoard so economy rules can post / claim / fulfil orders
    // without crossing into the board's lock from a worker thread. See
    // CraftOrderState above.
    CraftOrderState craft_orders{};

    // ---- Recycle-pool reset (SNAPSHOT_PERF_BACKLOG.md Tier 3.1) ----
    // Returns this object to the exact state of a freshly default-constructed
    // BotSnapshot, but RETAINS the heap capacity of every inner container so
    // a subsequent Build() can refill without reallocating. The world thread
    // calls this on the prior snapshot ONLY when use_count()==1 (no AI worker
    // can still be reading it) — see BotSnapshotBuilder::Build.
    //
    // COMPLETENESS IS LOAD-BEARING: a member missed here leaks last tick's
    // data into the next snapshot. Every top-level member is reset below; the
    // sub-structs each own a reset() that clears their containers + restores
    // their non-default scalars (e.g. bg.av_*_alive=true, travel.next_hop_
    // dest_map=kInvalidMapId, bags.equipped_bag_subclass=0xFF). The
    // VerifyResetClearsAll() helper (BotSnapshotResetCheck.cpp) fills every
    // container, calls this, and asserts all empty — a guard against drift
    // when new fields are appended.
    void reset_for_reuse()
    {
        // Top-level scalars / primary key.
        version = 0;
        bot_id = 0;
        world_tick = 0;
        published_at_ms = 0;
        guid = ObjectGuid::Empty;
        owner_name.clear();

        // Sub-structs (declaration order).
        identity.reset();
        vitals.reset();
        position.reset();
        area.reset();
        instance_ctx.reset();
        movement.reset();
        environment.reset();
        travel.reset();
        death.reset();
        social_events.reset();
        guild.reset();
        lfg.reset();
        auction.reset();
        combat.reset();
        auras.reset();
        cast.reset();
        cooldowns.reset();
        inventory.reset();
        bags.reset();
        stat_weights.reset();
        secondary_stats.reset();
        vendor_visit.reset();
        consumables.reset();
        spellbook.reset();
        progression.reset();
        group.reset();
        quest_log.reset();
        gossip.reset();
        quest_discovery.reset();
        loot.reset();
        mailbox.reset();
        pve_order.reset();
        bg.reset();
        vehicle.reset();
        bank.reset();
        world_objects.reset();
        path.reset();
        path_telemetry.reset();
        dungeon_exec.reset();
        pet.reset();
        archetype.reset();
        craft_orders.reset();
    }
};

// Soft size budget (CONTRACTS.md §2.1: ≤8 KB). Vectors push us over this in
// practice; we measure approximate footprint at runtime via PerfCounters.
// The static_assert guards against accidental field bloat in the POD section.
static_assert(sizeof(BotSnapshot) < 8192,
              "BotSnapshot fixed footprint exceeds 8KB; review field additions");

} // namespace Playerbot
