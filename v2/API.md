# Playerbot V2 — Core API Surface

**Status**: Pass B
**Last updated**: 2026-05-01
**Purpose**: The exact `PlayerbotAPI.h` contract. This is the **only** interface V2 has to TrinityCore. V2 code may not include `Player.h`, `Unit.h`, `Map.h`, `Group.h`, `Spell.h`, `Item.h`, `Quest.h` directly. Lint rule enforces it.
**Location**: `src/server/game/Playerbot/PlayerbotAPI.h`
**Threading**: every method documented as either `[snapshot-time]` (runs on world thread during snapshot building, returns const data) or `[command]` (runs on world thread during intent draining, mutates game state).

---

## 1. Header skeleton

```cpp
// src/server/game/Playerbot/PlayerbotAPI.h
#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include <chrono>
#include <span>
#include <string>
#include <vector>

namespace Playerbot {

using Ms = std::chrono::milliseconds;

class API {
public:
    explicit API(class Player* p);

    // ----- Identity & vitals (snapshot-time) -----
    ObjectGuid guid() const;
    std::string name() const;
    uint8 level() const;
    uint8 race() const;
    uint8 cls() const;
    uint8 spec() const;
    uint8 gender() const;
    uint32 faction() const;
    int32 hp() const;
    int32 max_hp() const;
    int32 power(uint8 type) const;
    int32 max_power(uint8 type) const;

    // Primary/secondary stats (Strength, Agility, Intellect, Stamina, Crit, Haste,
    // Mastery, Versatility, Leech, Avoidance, Speed)
    int32 stat(uint8 stat_id) const;

    // ----- Position & locomotion (snapshot-time) -----
    uint32 map_id() const;
    void   position(float& x, float& y, float& z, float& o) const;
    uint32 area_id() const;
    uint32 zone_id() const;
    bool   is_indoors() const;
    bool   is_swimming() const;
    bool   is_flying() const;
    bool   is_mounted() const;
    bool   is_moving() const;
    bool   is_in_combat() const;

    // ----- Targets (snapshot-time) -----
    ObjectGuid target() const;
    ObjectGuid victim() const;
    ObjectGuid focus() const;
    std::vector<ObjectGuid> attackers(size_t max = 16) const;
    std::vector<ObjectGuid> nearby_enemies(float range, size_t max = 32) const;
    std::vector<ObjectGuid> nearby_friends(float range, size_t max = 32) const;

    // For one unit: snapshot-time fields (others may be derived from snapshot)
    bool   unit_exists(ObjectGuid) const;
    int32  unit_hp(ObjectGuid) const;
    int32  unit_max_hp(ObjectGuid) const;
    void   unit_position(ObjectGuid, float& x, float& y, float& z) const;
    bool   unit_is_casting(ObjectGuid) const;
    uint32 unit_casting_spell(ObjectGuid) const;
    Ms     unit_cast_remaining(ObjectGuid) const;
    bool   unit_cast_interruptible(ObjectGuid) const;
    uint8  unit_level(ObjectGuid) const;
    uint32 unit_entry(ObjectGuid) const;

    // ----- Auras (snapshot-time) -----
    struct AuraInfo {
        uint32 spell_id; uint8 stacks; Ms remaining;
        ObjectGuid caster; uint8 dispel_type;
        bool harmful; bool stealable;
    };
    std::vector<AuraInfo> auras(ObjectGuid on) const;
    bool has_aura(ObjectGuid on, uint32 spell_id) const;

    // ----- Cooldowns & GCD (snapshot-time) -----
    bool   is_spell_ready(uint32 spell_id) const;
    Ms     spell_cooldown_remaining(uint32 spell_id) const;
    uint8  spell_charges(uint32 spell_id) const;
    uint8  spell_max_charges(uint32 spell_id) const;
    Ms     gcd_remaining() const;
    Ms     item_cooldown_remaining(uint32 item_entry) const;

    // ----- Spellbook (snapshot-time) -----
    bool                     knows_spell(uint32 spell_id) const;
    std::vector<uint32>      known_spells() const;
    bool                     knows_recipe(uint32 spell_id) const;

    // ----- Inventory (snapshot-time) -----
    int32 gold() const;
    uint8 bag_free_slots() const;
    struct ItemInfo {
        uint8 bag, slot; uint32 entry; uint16 count;
        uint8 durability_pct; bool is_quest_item; bool is_soulbound;
    };
    std::vector<ItemInfo> bag_items() const;
    std::vector<ItemInfo> bank_items() const;
    std::vector<ItemInfo> reagent_bank_items() const;
    ItemInfo              equipped(uint8 slot) const;
    uint16                average_item_level() const;

    // ----- Group (snapshot-time) -----
    ObjectGuid group_guid() const;
    bool       in_group() const;
    bool       in_raid() const;
    uint8      loot_method() const;
    uint8      loot_threshold() const;
    ObjectGuid group_leader() const;
    struct MemberInfo {
        ObjectGuid guid; std::string name; uint8 level;
        uint8 cls; uint8 spec; uint8 role;
        int32 hp, max_hp; bool online; bool in_combat;
        float x, y, z; uint32 map_id;
    };
    std::vector<MemberInfo> group_members() const;
    std::array<ObjectGuid, 8> raid_marks() const;
    bool                    ready_check_active() const;
    Ms                      ready_check_remaining() const;

    // ----- Quest log (snapshot-time) -----
    struct QuestInfo {
        uint32 quest_id; uint8 state;
        std::array<uint32, 4> objectives_progress;
        std::array<uint32, 4> objectives_required;
    };
    std::vector<QuestInfo> quest_log() const;
    bool                   has_quest(uint32 quest_id) const;
    uint8                  quest_state(uint32 quest_id) const;

    // ----- Movement state (snapshot-time) -----
    ObjectGuid current_path_target() const;
    void       current_path_end(uint32& map_id, float& x, float& y, float& z) const;

    // ----- Loot windows (snapshot-time) -----
    struct LootSlot { uint32 item_entry; uint16 count; uint8 type; };
    std::vector<LootSlot> open_loot_slots() const;

    // ----- Vendor windows (snapshot-time) -----
    struct VendorOffer { uint8 slot; uint32 item_entry; uint32 cost_money; uint32 cost_currency; };
    std::vector<VendorOffer> open_vendor_offers() const;

    // ----- Mail (snapshot-time) -----
    struct MailInfo {
        uint32 mail_id; std::string sender; std::string subject;
        uint32 gold; std::vector<std::pair<uint32,uint16>> items;
        uint32 cod; Ms remaining;
    };
    std::vector<MailInfo> mail_inbox() const;

    // ----- Auction House (snapshot-time, when AH window is open) -----
    struct AhListing {
        uint64 auction_id; uint32 item_entry; uint16 count;
        uint32 bid; uint32 buyout; Ms remaining; std::string seller;
    };
    std::vector<AhListing> ah_open_listings() const;
    std::vector<AhListing> ah_my_postings() const;

    // ----- Bank (snapshot-time, when bank is open) -----
    bool bank_open() const;

    // ----- Trade (snapshot-time, when trade window open) -----
    struct TradeOffer {
        ObjectGuid partner;
        std::array<ItemInfo, 7> their_items;
        std::array<ItemInfo, 7> my_items;
        uint32 their_gold;
        uint32 my_gold;
        bool   they_accepted;
        bool   i_accepted;
        Ms     remaining;
    };
    bool        trade_open() const;
    TradeOffer  trade_state() const;

    // ----- Encounter (snapshot-time) -----
    uint32 active_encounter_npc() const;
    uint8  active_encounter_phase() const;

    // ----- Pets (snapshot-time) -----
    bool        has_pet() const;
    ObjectGuid  pet_guid() const;
    int32       pet_hp() const;
    int32       pet_max_hp() const;
    bool        pet_in_combat() const;
    uint8       pet_command_state() const;     // attack/follow/stay
    uint8       pet_react_state() const;       // passive/defensive/aggressive
    std::vector<uint32> pet_known_spells() const;

    // ----- Talents (snapshot-time) -----
    struct TalentInfo {
        uint32 talent_id; bool selected;
        uint8  rank; uint8  max_rank;
    };
    std::vector<TalentInfo> talents() const;
    std::vector<TalentInfo> pvp_talents() const;
    uint8                   active_loadout_index() const;

    // ----- Reputation (snapshot-time) -----
    struct RepInfo {
        uint32 faction_id; int32 standing; uint8 rank; bool paragon; uint32 paragon_value;
    };
    std::vector<RepInfo> reputations() const;
    int32                rep_standing(uint32 faction_id) const;

    // ----- Achievements (snapshot-time) -----
    bool                completed_achievement(uint32 ach_id) const;
    uint32              achievement_points() const;
    std::vector<uint32> tracked_achievements() const;

    // ----- Mounts / pets / toys collection (snapshot-time) -----
    std::vector<uint32> known_mounts() const;
    std::vector<uint32> known_battle_pets() const;
    std::vector<uint32> known_toys() const;

    // ----- Housing (snapshot-time, 12.0+) -----
    struct HouseInfo {
        uint32 plot_id; uint32 neighborhood_id;
        uint32 template_id; uint16 size_tier;
        uint32 decoration_count; uint32 decoration_capacity;
        bool   is_public; std::vector<ObjectGuid> co_owners;
    };
    bool                    owns_house() const;
    HouseInfo               my_house() const;
    std::vector<HouseInfo>  visited_houses() const;
    uint32                  current_neighborhood() const;
    std::vector<uint32>     available_neighborhoods() const;

    // =========================================================================
    // ----- ACTION COMMANDS [command] -----
    // =========================================================================
    // All commands return a Result enum. Commands are no-throw.

    enum class Result : uint8 {
        Ok, NotReady, OutOfRange, InvalidTarget, NotEnoughResource,
        NotKnown, ServerRefused, InventoryFull, Locked, Other
    };

    // Combat
    Result cast_spell(uint32 spell_id, ObjectGuid target = ObjectGuid::Empty);
    Result cast_ground_spell(uint32 spell_id, float x, float y, float z);
    Result cancel_cast();
    Result start_attack(ObjectGuid target);
    Result stop_attack();

    // Movement
    Result move_to(float x, float y, float z, bool run = true);
    Result move_path(std::span<float const> xyz_triples);   // x0,y0,z0,x1,y1,z1,...
    Result stop_movement();
    Result jump(float forward_force = 7.0f);
    Result mount(uint32 mount_id);                          // 0 = let server pick best
    Result dismount();
    Result hearth();
    Result use_flight_master(ObjectGuid npc, uint32 destination_node);

    // Items
    Result use_item(uint8 bag, uint8 slot, ObjectGuid target = ObjectGuid::Empty);
    Result equip_item(uint8 from_bag, uint8 from_slot, uint8 to_slot);
    Result destroy_item(uint8 bag, uint8 slot, uint16 count);

    // Loot
    Result loot(ObjectGuid corpse_or_object);
    Result pick_loot_item(ObjectGuid corpse_or_object, uint8 slot);
    Result release_corpse();
    Result revive_at_corpse();

    // Vendor
    Result vendor_buy(ObjectGuid npc, uint8 vendor_slot, uint8 count);
    Result vendor_sell(ObjectGuid npc, uint8 bag, uint8 slot, uint8 count);
    Result repair_all(ObjectGuid npc, bool from_guild_bank);

    // Quest
    Result quest_accept(ObjectGuid npc, uint32 quest_id);
    Result quest_complete(ObjectGuid npc, uint32 quest_id, uint8 reward_choice = 0);
    Result quest_abandon(uint32 quest_id);
    Result quest_share(uint32 quest_id);

    // Group
    Result group_invite(std::string const& player_name);
    Result group_accept_invite();
    Result group_decline_invite();
    Result group_leave();
    Result group_kick(ObjectGuid member);
    Result group_promote(ObjectGuid member);
    Result group_set_role(ObjectGuid member, uint8 role);
    Result group_ready_response(bool ready);
    Result group_loot_roll(uint64 loot_id, uint8 slot, uint8 choice);

    // Social
    Result whisper(std::string const& target, std::string const& text);
    Result say(std::string const& text);
    Result yell(std::string const& text);
    Result party_chat(std::string const& text);
    Result raid_chat(std::string const& text);
    Result emote(uint32 emote_id, ObjectGuid target = ObjectGuid::Empty);
    Result custom_emote(std::string const& text);

    // Trade
    Result trade_initiate(ObjectGuid target);
    Result trade_add_item(uint8 trade_slot, uint8 bag, uint8 slot);
    Result trade_set_gold(uint32 gold);
    Result trade_accept();
    Result trade_cancel();

    // Bank / mail
    Result bank_deposit(uint8 from_bag, uint8 from_slot, uint8 to_bank_slot);
    Result bank_withdraw(uint8 from_bank_slot, uint8 to_bag, uint8 to_slot);
    Result reagent_bank_deposit(uint8 from_bag, uint8 from_slot);
    Result reagent_bank_withdraw_all();
    Result mail_send(std::string const& to, std::string const& subject,
                     std::string const& body, uint32 gold,
                     std::span<std::pair<uint8,uint8> const> items, uint32 cod = 0);
    Result mail_take(uint32 mail_id, bool take_gold, bool take_items);
    Result mail_return(uint32 mail_id);

    // Auction House
    Result ah_post(uint8 bag, uint8 slot, uint8 count,
                   uint32 bid, uint32 buyout, uint32 duration_hours);
    Result ah_buy(uint64 auction_id, uint32 bid, bool use_buyout);
    Result ah_cancel(uint64 auction_id);

    // LFG / queues
    Result lfg_queue(uint32 dungeon_or_bg_id, uint8 role);
    Result lfg_unqueue();
    Result lfg_role_response(bool accept);
    Result lfg_ready_response(bool ready);
    Result bg_queue(uint32 bg_id, bool as_group);
    Result arena_queue(uint8 size_2_3_solo);

    // World interaction
    Result use_object(ObjectGuid object);
    Result interact_with_npc(ObjectGuid npc);
    Result gossip_select(ObjectGuid npc, uint8 option);

    // Stealth
    Result toggle_stealth(uint32 stealth_spell);
    Result pickpocket(ObjectGuid target);

    // Talents / specs
    Result learn_talent(uint32 talent_id);
    Result reset_talents();
    Result switch_spec(uint8 spec_index);
    Result load_loadout(uint8 loadout_index);
    Result save_loadout(uint8 loadout_index);

    // Pets
    Result pet_summon(uint8 stable_slot);
    Result pet_dismiss();
    Result pet_command(uint8 command);          // attack/follow/stay/passive/aggressive
    Result pet_revive();
    Result pet_feed(uint8 bag, uint8 slot);
    Result pet_tame(ObjectGuid beast);
    Result pet_stable_swap(uint8 active_slot, uint8 stable_slot);

    // Mounts / toys / collections
    Result use_toy(uint32 toy_id);

    // Professions
    Result craft(uint32 recipe_id, uint8 quality = 1);
    Result begin_gathering(ObjectGuid node);
    Result mill(uint8 bag, uint8 slot);
    Result prospect(uint8 bag, uint8 slot);
    Result smelt(uint32 recipe_id);
    Result disenchant(uint8 bag, uint8 slot);
    Result fish_at_node(ObjectGuid bobber);

    // Reputation / paragon
    Result equip_tabard(uint8 from_bag, uint8 from_slot);

    // Housing (12.0+)
    Result house_join_neighborhood(uint32 neighborhood_id);
    Result house_leave_neighborhood();
    Result house_purchase_plot(uint32 neighborhood_id, uint32 plot_id);
    Result house_upgrade(uint32 plot_id, uint32 template_id);
    Result house_sell(uint32 plot_id);
    Result house_set_visibility(uint32 plot_id, bool is_public);
    Result house_set_permission(uint32 plot_id, ObjectGuid player, uint8 tier);
    Result house_visit(uint32 plot_id);
    Result house_leave();
    Result deco_place(uint32 plot_id, uint32 deco_entry, bool exterior,
                      float x, float y, float z, float rot);
    Result deco_remove(uint32 plot_id, uint64 deco_instance_id);
    Result deco_move(uint32 plot_id, uint64 deco_instance_id,
                     float x, float y, float z, float rot);

    // Calendar
    Result calendar_invite(uint64 event_id, ObjectGuid target);
    Result calendar_respond(uint64 event_id, uint8 response);

private:
    Player* p_;
};

// =============================================================================
// Hook firing (called by core; module subscribes via Module::OnXxx handlers)
// =============================================================================

namespace Hooks {
    // Lifecycle
    void OnPlayerLogin(Player*);
    void OnPlayerLogout(Player*);
    void OnLevelUp(Player*, uint8 new_level);
    void OnDeath(Unit* victim, Unit* killer);
    void OnResurrect(Player*);
    void OnSpecChanged(Player*, uint8 new_spec);

    // Combat
    void OnDamageDealt(Unit* attacker, Unit* victim, int32 amount, uint32 spell_id);
    void OnDamageTaken(Unit* attacker, Unit* victim, int32 amount, uint32 spell_id);
    void OnHealReceived(Unit* healer, Unit* target, int32 amount, uint32 spell_id);
    void OnAuraApplied(Unit* target, Aura* aura);
    void OnAuraRemoved(Unit* target, Aura* aura);
    void OnSpellCastStart(Unit* caster, uint32 spell_id, Unit* target);
    void OnSpellCastSuccess(Unit* caster, uint32 spell_id, Unit* target);
    void OnSpellCastFailed(Unit* caster, uint32 spell_id, uint8 reason);
    void OnSpellInterrupted(Unit* caster, uint32 spell_id, Unit* interrupter);

    // World
    void OnLoot(Player*, Loot const&);
    void OnQuestAccepted(Player*, uint32 quest_id);
    void OnQuestObjectiveProgress(Player*, uint32 quest_id, uint8 obj_idx, uint32 progress);
    void OnQuestCompleted(Player*, uint32 quest_id);
    void OnGroupMemberJoined(Group*, Player*);
    void OnGroupMemberLeft(Group*, Player*);
    void OnReadyCheckRequested(Group*);
    void OnTradeRequested(Player* initiator, Player* target);
    void OnWhisperReceived(Player* recipient, Player* sender, std::string const& text);

    // Content
    void OnInstanceEnter(Player*, uint32 map_id);
    void OnInstanceExit(Player*, uint32 map_id);
    void OnBgEnter(Player*, uint32 bg_id);
    void OnBgExit(Player*, uint32 bg_id);
    void OnBgObjectiveProgress(Player*, uint32 bg_id, uint32 objective_id);
    void OnEncounterPhaseChange(uint32 npc_id, uint8 phase);

    // Economy
    void OnMailReceived(Player*, uint32 mail_id);
    void OnAuctionExpired(uint64 auction_id, uint64 owner_guid_low);
    void OnAuctionSold(uint64 auction_id, uint64 owner_guid_low, uint64 buyer_guid_low, uint32 price);

    // LFG
    void OnLfgQueued(Group*, uint32 dungeon_or_bg_id);
    void OnLfgRoleProposed(Player*, uint32 dungeon_id, uint8 proposed_role);
    void OnLfgReady(Group*);

    // Housing (12.0+)
    void OnPlotPurchased(Player*, uint32 neighborhood_id, uint32 plot_id);
    void OnHouseVisited(Player* visitor, uint32 plot_id);
}

} // namespace Playerbot
```

---

## 2. Friendship grants

The following one-line additions are all the V1-style "module-purity" relaxations the architecture requires. Each grants `Playerbot::API` the minimum private-member access to implement the snapshot/command surface above without copy-pasting accessor logic into the module.

```cpp
// In Player.h, inside class Player:
friend class Playerbot::API;

// Same addition in Unit.h (class Unit), Map.h (class Map), Group.h (class Group),
// Spell.h (class Spell), Item.h (class Item), Quest.h (class Quest).
```

Total friendship grants: 7 lines across 7 files. No other private-state access is permitted.

---

## 3. Concurrency rules (compile- and runtime-checked)

| Rule | Enforcement |
|---|---|
| All `API` methods must run on world thread | `assert(IsWorldThreadOrInit())` at top of every method |
| Snapshot-time methods do not mutate `Player` | `[[nodiscard]] [[const]]` annotations + reviewer attention |
| Command methods may mutate `Player` and emit packets to client | Documented in method doc-comment |
| Hooks are called only on world thread | Inline noop when not on world thread + assertion |
| Module never includes `Player.h` | CI grep + IWYU check |

---

## 4. ABI / linkage

- `PlayerbotAPI.h` is included only by `PlayerbotV2.cpp` (entry) and `PlayerbotAPI.cpp` (impl) on the core side, and by every Bot/Combat/Group/Movement/etc. file in the module.
- The `API` class is **not** virtual. Direct call only. No vtable cost in hot paths.
- The `Hooks::*` functions, when V2 is not built, are inline-empty (`{}`) and dead-code-eliminated by the compiler.
- When V2 *is* built, `Hooks::*` calls are real and dispatch to `Playerbot::V2::Module::OnXxx`.

---

## 5. Versioning

The API header carries a single version constant:
```cpp
namespace Playerbot { constexpr uint32 kApiVersion = 1; }
```

Pass B locks `kApiVersion = 1`. Future breaking additions bump it; module checks the constant at startup and refuses to load on mismatch.

---

## 6. What the API deliberately does NOT expose

To keep the surface tractable and aligned with `REQUIREMENTS.md` §1.1 #7 ("bots use systems as players do"):

- No "set HP directly", "give item directly", "teleport without flight master / hearth / portal", "max out skill", or any other admin shortcut.
- No "force enter combat", "force complete quest", "force loot drop".
- No "skip cast time" / "ignore cooldown" / "ignore resource cost" — bots cast through the same `Spell` machinery a player uses.
- No "spawn into group without queue" — groups form via invite or LFG, same as players.

If a feature seems to require one of these, that's the signal to find the player-equivalent path and use it.

---

## 7. What's locked vs open

**Locked**: every method signature above. The 7 friendship grants. The hook list. The Result enum. `kApiVersion = 1`.

**Open**:
- Field counts in info structs (e.g., `MemberInfo` may grow if encounter scripts demand additional per-member visibility — additive only).
- Whether `nearby_enemies(range, max)` is replaced by snapshot-only access (current design lets you call it on demand for un-snapshotted ranges; if perf data shows it's hot, it becomes snapshot-only).
- Housing API specifics — depends on Midnight 12.x housing schema as it stabilizes upstream.
