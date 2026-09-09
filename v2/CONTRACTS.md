# Playerbot V2 — Concrete Contracts

**Status**: Pass B
**Last updated**: 2026-05-01
**Purpose**: The exact class signatures, method shapes, and type definitions an implementer needs to start writing code. Where this doc and `ARCHITECTURE.md` disagree, this doc wins (it's the higher-fidelity descendant).

All types live in namespace `Playerbot` (V2). C++20.

---

## 1. Foundational types

### 1.1 IDs and tags
```cpp
namespace Playerbot {

using BotId       = uint64;          // Stable identity, == ObjectGuid::GetCounter() of bot's character
using TickId      = uint64;          // Monotonic per world tick
using IntentId    = uint64;          // Monotonic per intent emitted (for tracing)
using SnapshotVer = uint64;          // Monotonic per snapshot publication

enum class ActivityTier : uint8 {
    Combat = 0,    // ≥10 Hz
    Active = 1,    // 5 Hz
    Idle   = 2,    // 1 Hz
    Hibernate = 3, // 0.05 Hz
};

enum class BotState : uint8 {
    LoggingIn,
    Idle,
    Travelling,
    Questing,
    InCombat,
    Looting,
    Dead,
    Resurrecting,
    LoggingOut,
    // Cross-cutting (re-entrant, not exclusive):
    AtVendor, AtMailbox, AtAuctionHouse,
    InGroup, InInstance,
    Decorating,
};

enum class Role : uint8 { Tank, Healer, Dps, Unknown };
enum class DispelType : uint8 { Magic, Curse, Poison, Disease, Bleed, Enrage, None };

} // namespace Playerbot
```

### 1.2 Time types
All time-based fields use `std::chrono::milliseconds` from `<chrono>`. No raw `uint32` ticks.

---

## 2. Per-bot layer (`Bot/`)

### 2.1 BotSnapshot
```cpp
namespace Playerbot {

// Immutable. Built on world thread, consumed read-only by AI workers.
struct BotSnapshot {
    SnapshotVer version;
    BotId       bot_id;
    TickId      world_tick;

    // Identity
    ObjectGuid  guid;
    std::string name;
    uint8       level;
    uint8       race;
    uint8       cls;
    uint8       spec;
    uint8       gender;
    uint32      faction;

    // Vital stats
    int32       hp;
    int32       max_hp;
    std::array<int32, 8> power;       // Indexed by Powers enum (mana, rage, etc.)
    std::array<int32, 8> max_power;

    // Position
    uint32      map_id;
    float       x, y, z, o;
    uint32      area_id;
    uint32      zone_id;
    bool        is_indoors;
    bool        is_swimming;
    bool        is_flying;
    bool        is_mounted;
    bool        is_moving;

    // Combat state
    bool        in_combat;
    ObjectGuid  victim;
    ObjectGuid  current_target;
    std::vector<NearbyUnit> attackers;       // Bounded, top N by threat to me
    std::vector<NearbyUnit> nearby_enemies;  // Bounded by range filter
    std::vector<NearbyUnit> nearby_friends;

    // Auras
    std::vector<AuraEntry> own_auras;        // On self
    std::vector<AuraEntry> target_auras;     // On current target
    std::vector<AuraEntry> victim_auras;     // On victim

    // Cooldowns
    Milliseconds gcd_remaining;
    std::vector<CooldownEntry> spell_cooldowns;
    std::vector<CooldownEntry> item_cooldowns;
    std::vector<CooldownEntry> charge_cooldowns;

    // Inventory & equipment (summary; full read via API on demand)
    int32 gold;
    uint8 bag_free_slots;
    std::array<EquippedItem, 19> equipped;   // 19 slots incl. tabard/shirt
    std::vector<InventoryItem> bag_items;
    uint16 average_item_level;

    // Spellbook
    std::vector<uint32> known_spells;        // Sorted, binary-searchable
    std::vector<uint32> known_recipes;

    // Group (small; full group via group snapshot)
    ObjectGuid group_guid;
    Role       my_role;

    // Quest log (summary)
    std::vector<QuestEntry> quests;

    // Movement state
    ObjectGuid path_target;
    uint32     path_end_map_id;
    float      path_end_x, path_end_y, path_end_z;

    // Encounter context
    uint32     active_encounter_npc_id;      // 0 if none
    uint8      active_encounter_phase;

    // Sized at compile time. Verified ≤ 8KB.
    static_assert_size_le<8192>();
};

struct NearbyUnit {
    ObjectGuid guid;
    uint32     entry;
    uint8      level;
    int32      hp;
    int32      max_hp;
    float      x, y, z;
    bool       is_casting;
    uint32     casting_spell_id;
    Milliseconds cast_remaining;
    bool       is_interruptible;
    Role       role;            // For friends only
};

struct AuraEntry {
    uint32      spell_id;
    uint8       stacks;
    Milliseconds remaining;
    ObjectGuid  caster;
    DispelType  dispel_type;
    bool        is_harmful;
    bool        is_stealable;
};

struct CooldownEntry {
    uint32       spell_id;
    Milliseconds remaining;
    uint8        charges;
    uint8        max_charges;
};

struct EquippedItem {
    uint32 entry;
    uint16 item_level;
    uint8  durability_pct;
};

struct InventoryItem {
    uint8  bag;
    uint8  slot;
    uint32 entry;
    uint16 count;
    uint8  durability_pct;
    bool   is_quest_item;
};

struct QuestEntry {
    uint32 quest_id;
    uint8  state;       // 0=incomplete, 1=complete
    std::array<uint32, 4> objectives_progress;
    std::array<uint32, 4> objectives_required;
};

} // namespace Playerbot
```

### 2.2 BotSnapshotView
Ergonomic facade. Thin wrapper around `BotSnapshot const&` with predicate helpers. **Stateless. No allocation.**

```cpp
class BotSnapshotView {
    BotSnapshot const& s_;
public:
    explicit BotSnapshotView(BotSnapshot const& s) : s_(s) {}

    // Identity
    BotId    bot_id()  const { return s_.bot_id; }
    uint8    level()   const { return s_.level; }
    uint8    cls()     const { return s_.cls; }
    uint8    spec()    const { return s_.spec; }

    // Stats
    bool     in_combat()    const { return s_.in_combat; }
    int32    hp_pct()       const { return s_.max_hp > 0 ? (s_.hp * 100) / s_.max_hp : 0; }
    int32    power(uint8 type) const;
    int32    power_pct(uint8 type) const;

    // Cooldowns / readiness
    bool     is_ready(uint32 spell_id) const;
    Milliseconds cd_remaining(uint32 spell_id) const;
    bool     gcd_active() const { return s_.gcd_remaining.count() > 0; }
    uint8    charges(uint32 spell_id) const;

    // Auras
    bool     has_aura(uint32 spell_id, ObjectGuid on = ObjectGuid::Empty) const;
    AuraEntry const* find_aura(uint32 spell_id, ObjectGuid on = ObjectGuid::Empty) const;
    bool     target_dispellable(DispelType type) const;

    // Targets
    ObjectGuid current_target()  const { return s_.current_target; }
    NearbyUnit const* target_info() const;
    NearbyUnit const* lowest_hp_friend() const;
    NearbyUnit const* highest_threat_attacker() const;

    // Movement
    bool     is_moving() const { return s_.is_moving; }
    bool     is_indoors() const { return s_.is_indoors; }

    // Inventory
    bool     has_item(uint32 entry) const;
    uint8    bag_free_slots() const { return s_.bag_free_slots; }
    int32    gold() const { return s_.gold; }

    // Spellbook
    bool     knows_spell(uint32 spell_id) const;
};
```

### 2.3 Intents
```cpp
namespace Playerbot {

// Discriminated union (variant). Each alternative is POD.

struct CastSpellIntent       { uint32 spell_id; ObjectGuid target; };
struct GroundTargetSpellIntent { uint32 spell_id; float x, y, z; };
struct CancelCastIntent      {};
struct StartAttackIntent     { ObjectGuid target; };
struct StopAttackIntent      {};

struct MoveToIntent          { float x, y, z; bool run; };
struct MovePathIntent        { std::vector<G3D::Vector3> points; };
struct StopMovementIntent    {};
struct JumpIntent            { float forward; }; // for unstick
struct MountIntent           { uint32 mount_id; }; // 0 = best for context
struct DismountIntent        {};
struct HearthIntent          {};
struct UseFlightMasterIntent { ObjectGuid npc; uint32 destination_node; };

struct UseItemIntent         { uint8 bag, slot; ObjectGuid target; };
struct EquipItemIntent       { uint8 from_bag, from_slot; uint8 to_slot; };
struct LootIntent            { ObjectGuid corpse_or_object; };
struct PickLootSlotIntent    { ObjectGuid corpse_or_object; uint8 slot; };
struct ReleaseCorpseIntent   {};
struct ReviveAtCorpseIntent  {};

struct VendorBuyIntent       { ObjectGuid npc; uint8 vendor_slot; uint8 count; };
struct VendorSellIntent      { ObjectGuid npc; uint8 bag, slot; uint8 count; };
struct RepairAllIntent       { ObjectGuid npc; bool from_guild_bank; };

struct QuestAcceptIntent     { ObjectGuid npc; uint32 quest_id; };
struct QuestCompleteIntent   { ObjectGuid npc; uint32 quest_id; uint8 reward_choice; };
struct QuestAbandonIntent    { uint32 quest_id; };

struct GroupInviteIntent     { std::string player_name; };
struct GroupAcceptIntent     {};
struct GroupDeclineIntent    {};
struct GroupLeaveIntent      {};
struct GroupReadyResponseIntent { bool ready; };
struct GroupRoleSetIntent    { Role role; };
struct GroupLootRollIntent   { uint64 loot_id; uint8 slot; uint8 choice; }; // 0=pass 1=greed 2=need 3=de

struct WhisperIntent         { std::string target; std::string text; };
struct SayIntent             { std::string text; };
struct PartyChatIntent       { std::string text; };
struct EmoteIntent           { uint32 emote_id; ObjectGuid target; };

struct TradeInitiateIntent   { ObjectGuid target; };
struct TradeAddItemIntent    { uint8 trade_slot; uint8 bag, slot; };
struct TradeSetGoldIntent    { uint32 gold; };
struct TradeAcceptIntent     {};
struct TradeCancelIntent     {};

struct BankDepositIntent     { uint8 from_bag, from_slot; uint8 to_bank_slot; };
struct BankWithdrawIntent    { uint8 from_bank_slot; uint8 to_bag, to_slot; };
struct MailSendIntent        { std::string to; std::string subject; std::string body;
                               uint32 gold; std::vector<std::pair<uint8,uint8>> items;
                               uint32 cod; };
struct MailTakeIntent        { uint32 mail_id; bool take_gold; bool take_items; };

struct AhPostIntent          { uint8 bag, slot; uint8 count; uint32 bid; uint32 buyout;
                               uint32 duration_hours; };
struct AhBuyIntent           { uint64 auction_id; bool use_buyout; uint32 bid; };
struct AhCancelIntent        { uint64 auction_id; };

struct LfgQueueIntent        { uint32 dungeon_or_bg_id; Role role; };
struct LfgUnqueueIntent      {};
struct LfgRoleResponseIntent { bool accept; };
struct LfgReadyResponseIntent { bool ready; };

struct UseObjectIntent       { ObjectGuid object; };
struct InteractWithNpcIntent { ObjectGuid npc; };
struct GossipSelectIntent    { ObjectGuid npc; uint8 option; };

struct LearnTalentIntent     { uint32 talent_id; };
struct ResetTalentsIntent    {};
struct SwitchSpecIntent      { uint8 spec_index; };

struct PetSummonIntent       { uint8 stable_slot; }; // Hunter
struct PetDismissIntent      {};
struct PetCommandIntent      { uint8 command; };     // attack/follow/stay/passive/aggressive
struct PetReviveIntent       {};
struct PetFeedIntent         { uint8 bag, slot; };

struct JoinNeighborhoodIntent  { uint32 neighborhood_id; };
struct LeaveNeighborhoodIntent {};
struct PlotPurchaseIntent      { uint32 neighborhood_id; uint32 plot_id; };
struct PlotUpgradeIntent       { uint32 plot_id; uint32 template_id; };
struct PlaceDecorationIntent   { uint32 plot_id; uint32 deco_entry; bool exterior;
                                 float x, y, z, rot; };
struct RemoveDecorationIntent  { uint32 plot_id; uint64 deco_instance_id; };
struct VisitHouseIntent        { uint32 plot_id; };

// The variant. Order matters for the compile-time visitor.
using IntentBody = std::variant<
    CastSpellIntent, GroundTargetSpellIntent, CancelCastIntent,
    StartAttackIntent, StopAttackIntent,
    MoveToIntent, MovePathIntent, StopMovementIntent, JumpIntent,
    MountIntent, DismountIntent, HearthIntent, UseFlightMasterIntent,
    UseItemIntent, EquipItemIntent,
    LootIntent, PickLootSlotIntent, ReleaseCorpseIntent, ReviveAtCorpseIntent,
    VendorBuyIntent, VendorSellIntent, RepairAllIntent,
    QuestAcceptIntent, QuestCompleteIntent, QuestAbandonIntent,
    GroupInviteIntent, GroupAcceptIntent, GroupDeclineIntent, GroupLeaveIntent,
    GroupReadyResponseIntent, GroupRoleSetIntent, GroupLootRollIntent,
    WhisperIntent, SayIntent, PartyChatIntent, EmoteIntent,
    TradeInitiateIntent, TradeAddItemIntent, TradeSetGoldIntent,
    TradeAcceptIntent, TradeCancelIntent,
    BankDepositIntent, BankWithdrawIntent,
    MailSendIntent, MailTakeIntent,
    AhPostIntent, AhBuyIntent, AhCancelIntent,
    LfgQueueIntent, LfgUnqueueIntent, LfgRoleResponseIntent, LfgReadyResponseIntent,
    UseObjectIntent, InteractWithNpcIntent, GossipSelectIntent,
    LearnTalentIntent, ResetTalentsIntent, SwitchSpecIntent,
    PetSummonIntent, PetDismissIntent, PetCommandIntent, PetReviveIntent, PetFeedIntent,
    JoinNeighborhoodIntent, LeaveNeighborhoodIntent,
    PlotPurchaseIntent, PlotUpgradeIntent,
    PlaceDecorationIntent, RemoveDecorationIntent, VisitHouseIntent
>;

struct Intent {
    IntentId    id;
    BotId       bot_id;
    SnapshotVer source_snapshot;
    IntentBody  body;
};

} // namespace Playerbot
```

### 2.4 IntentEmitter
```cpp
class BotIntentEmitter {
    IntentQueue* queue_;
    BotId        bot_id_;
    SnapshotVer  source_;
    IntentId*    next_id_;
public:
    template<class T> void emit(T&& body) {
        Intent i{ ++(*next_id_), bot_id_, source_, std::forward<T>(body) };
        queue_->push(std::move(i));
    }

    // Convenience helpers (saves typing in APL/state code)
    void cast(uint32 spell, ObjectGuid target = ObjectGuid::Empty);
    void move_to(float x, float y, float z, bool run = true);
    void follow(ObjectGuid leader, float distance);
    void hearth();
    void mount_appropriate();
    void say(std::string text);
    void whisper(std::string to, std::string text);
};
```

### 2.5 BotEvent + EventInbox
```cpp
struct BotEvent {
    enum class Kind : uint16 {
        DamageTaken, DamageDealt, HealReceived,
        AuraApplied, AuraRemoved,
        SpellCastStart, SpellCastSuccess, SpellCastFailed, SpellInterrupted,
        Loot,
        QuestAccepted, QuestObjectiveProgress, QuestCompleted,
        GroupMemberJoined, GroupMemberLeft, ReadyCheckRequested,
        TradeRequested, WhisperReceived,
        InstanceEntered, InstanceExited,
        BgObjectiveProgress, EncounterPhaseChange,
        MailReceived, AuctionExpired, AuctionSold,
        LfgRoleProposed, LfgReady,
        LevelUp, Death, Resurrected, SpecChanged,
    };
    Kind        kind;
    Milliseconds timestamp;
    ObjectGuid  source;
    ObjectGuid  target;
    uint32      spell_id;
    int32       value;
    uint32      aux1;
    uint32      aux2;
    char        text[64];   // Bounded; longer strings dropped or truncated
};

class BotEventInbox {
    static constexpr size_t kCapacity = 256;
    std::array<BotEvent, kCapacity> ring_;
    std::atomic<uint32> head_{0};   // World thread writes
    uint32 tail_{0};                 // AI worker reads
public:
    void push(BotEvent ev);          // World thread only
    bool pop(BotEvent& out);         // AI worker only
    size_t pending() const;
};
```

### 2.6 BotAI
```cpp
class BotAI {
public:
    BotAI(BotId id, BotPersonality personality, BotRng rng);

    // Called by AI worker. Pure: snapshot in + events in → intents out.
    void tick(BotSnapshotView snapshot,
              GroupSnapshotView group,           // empty if not grouped
              BotEventInbox& events,
              BotIntentEmitter& emit);

    // Called when entering/leaving a state (by tick logic itself).
    BotState state() const { return state_; }
    void     transition_to(BotState s);

    // Inspection (diagnostic)
    BotPersonality const& personality() const { return personality_; }
    std::array<Intent, 8> const& last_intents() const { return last_intents_; }
private:
    BotId            bot_id_;
    BotPersonality   personality_;
    BotRng           rng_;
    BotState         state_ = BotState::LoggingIn;
    Milliseconds     state_entered_;
    std::array<Intent, 8> last_intents_;  // Ring buffer for diagnostics
    uint8            last_intents_head_ = 0;
};
```

### 2.7 State dispatch
Each state file in `Bot/States/` exports one function:
```cpp
namespace Playerbot::States {
    void DispatchIdle      (BotAI&, BotSnapshotView, GroupSnapshotView, BotEventInbox&, BotIntentEmitter&);
    void DispatchTravelling(BotAI&, BotSnapshotView, GroupSnapshotView, BotEventInbox&, BotIntentEmitter&);
    void DispatchQuesting  (...);
    void DispatchInCombat  (...);
    void DispatchLooting   (...);
    void DispatchDead      (...);
    void DispatchResurrecting(...);
    void DispatchAtVendor  (...);
    void DispatchAtMailbox (...);
    void DispatchAtAh      (...);
    void DispatchInGroup   (...);     // Layered, runs alongside primary state
    void DispatchInInstance(...);     // Layered
    void DispatchDecorating(...);
    void DispatchLoggingIn (...);
    void DispatchLoggingOut(...);
}
```

`BotAI::tick` dispatches to one primary state function and any active cross-cutting layer functions.

---

## 3. Group layer (`Group/`)

### 3.1 GroupSnapshot
```cpp
struct GroupMemberSummary {
    ObjectGuid   guid;
    std::string  name;
    uint8        level;
    uint8        cls;
    uint8        spec;
    Role         role;
    int32        hp;
    int32        max_hp;
    bool         online;
    bool         in_combat;
    float        x, y, z;
    uint32       map_id;
    bool         is_casting;
    uint32       casting_spell;
    std::vector<AuraEntry> debuffs;       // Bounded, dispellable + key encounter debuffs
};

struct GroupSnapshot {
    SnapshotVer version;
    ObjectGuid  group_guid;
    ObjectGuid  leader;
    uint8       loot_method;
    uint8       loot_threshold;
    bool        is_raid;
    std::vector<GroupMemberSummary> members;
    std::array<ObjectGuid, 8> raid_marks;     // skull/cross/square/...
    bool        ready_check_active;
    Milliseconds ready_check_remaining;
    uint32      active_instance_id;       // 0 if not in instance
    uint32      active_encounter_npc;     // 0 if no encounter
};

class GroupSnapshotView {
    GroupSnapshot const* g_;
public:
    bool                         exists() const { return g_ != nullptr; }
    ObjectGuid                   leader() const { return g_->leader; }
    GroupMemberSummary const*    me(BotId) const;
    GroupMemberSummary const*    lowest_hp(Role only = Role::Unknown) const;
    GroupMemberSummary const*    tank() const;
    std::vector<GroupMemberSummary> const& members() const { return g_->members; }
    bool                         is_player_led() const;
};
```

### 3.2 Role functions
```cpp
namespace Playerbot::GroupTactics {

void TankUpdate  (BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void HealerUpdate(BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DpsUpdate   (BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);

Role ResolveRole(uint8 cls, uint8 spec);

} // namespace
```

Each `*Update` function is **pure**: same inputs → same intents. No member state. They are the architectural answer to V1's manager-of-managers. Combined target LOC: ≤ 600 across the three files.

---

## 4. Combat (`Combat/`)

### 4.1 ApRotation + ApRule
```cpp
struct ApPredicateContext {
    BotSnapshotView   bot;
    GroupSnapshotView group;
};

using ApPredicate = bool (*)(ApPredicateContext const&);
using ApAction    = void (*)(ApPredicateContext const&, BotIntentEmitter&);

struct ApRule {
    ApPredicate predicate;
    ApAction    action;
    char const* name;       // For diagnostics, not behavior
};

class ApRotation {
    std::span<ApRule const> rules_;
public:
    explicit ApRotation(std::span<ApRule const> rules) : rules_(rules) {}

    // Evaluate top-to-bottom; on first match, run action and return true.
    bool tick(ApPredicateContext const& ctx, BotIntentEmitter& emit) const;
};

// Per-spec APL definition (in Apl_*.cpp)
namespace Apl::Hunter::BeastMastery {
    extern ApRule const kRules[];
    extern size_t const kRuleCount;
}

// Registry lookup
namespace Playerbot::Combat {
    ApRotation const* GetRotation(uint8 cls, uint8 spec);
}
```

Example file `Apl_Hunter_BeastMastery.cpp`:
```cpp
namespace Playerbot::Apl::Hunter::BeastMastery {

static bool ShouldKillShot(ApPredicateContext const& c) {
    auto t = c.bot.target_info();
    return t && (t->hp * 100 / t->max_hp) < 20 && c.bot.is_ready(SPELL_KILL_SHOT);
}
static void DoKillShot(ApPredicateContext const& c, BotIntentEmitter& e) {
    e.cast(SPELL_KILL_SHOT, c.bot.current_target());
}
// ...

ApRule const kRules[] = {
    { ShouldKillShot,        DoKillShot,        "Kill Shot < 20%" },
    { ShouldBestialWrath,    DoBestialWrath,    "Bestial Wrath on CD" },
    { ShouldBarbedShot,      DoBarbedShot,      "Maintain Frenzy / charges" },
    { ShouldKillCommand,     DoKillCommand,     "Kill Command" },
    { ShouldCobraShot,       DoCobraShot,       "Cobra Shot filler" },
    { AlwaysTrue,            DoAutoShot,        "Auto Shot" },
};
size_t const kRuleCount = std::size(kRules);

} // namespace
```

### 4.2 EncounterScript
```cpp
class EncounterScript {
public:
    virtual ~EncounterScript() = default;

    // Called every AI tick while in combat with the encounter's primary NPC.
    virtual void OnSnapshot(BotSnapshotView, GroupSnapshotView,
                             BotIntentEmitter&) = 0;

    // Called for events forwarded from the inbox that match this encounter.
    virtual void OnEvent(BotEvent const&, BotIntentEmitter&) = 0;

    // Returns priority of an intent vs APL: encounter overrides APL when true.
    virtual bool overrides_apl_for(IntentBody const&) const { return false; }

    // Identifying NPCs (for registry)
    virtual std::span<uint32 const> npc_ids() const = 0;
};

namespace Playerbot::Combat::Encounters {
    EncounterScript* Find(uint32 npc_id);
    void RegisterAll();   // Called at module init
}
```

### 4.3 TargetSelector
```cpp
struct TargetCandidate {
    ObjectGuid guid;
    float      score;
    char const* reason;
};

namespace Playerbot::Combat::TargetSelection {
    // Utility-AI scoring. Returns top candidate.
    ObjectGuid PickTarget(BotSnapshotView, GroupSnapshotView,
                          uint8 cls, uint8 spec, Role);

    ObjectGuid PickInterruptTarget(BotSnapshotView, GroupSnapshotView);
    ObjectGuid PickDispelTarget   (BotSnapshotView, GroupSnapshotView, DispelType);
    ObjectGuid PickHealTarget     (BotSnapshotView, GroupSnapshotView);
    ObjectGuid PickCcTarget       (BotSnapshotView, GroupSnapshotView, uint32 cc_spell);
}
```

---

## 5. Threading (`Threading/`)

### 5.1 IntentQueue
```cpp
// Lock-free MPSC queue — many AI workers + fleet thread produce, world thread consumes.
class IntentQueue {
public:
    void push(Intent intent);            // Producer side, lock-free
    bool pop (Intent& out);              // Consumer side (world thread only)
    size_t approximate_size() const;
};
```
Implementation: ring buffer with `std::atomic<uint32>` head/tail and CAS.

### 5.2 SnapshotPublisher
```cpp
class SnapshotPublisher {
public:
    // Called on world thread at start of each world tick.
    void publish(BotId, std::shared_ptr<BotSnapshot const>);

    // Called by AI worker. Returns latest atomically.
    std::shared_ptr<BotSnapshot const> latest(BotId) const;

private:
    // Per-bot atomic shared_ptr slot. shared_ptr is atomic-load/store-safe in C++20.
    std::unordered_map<BotId, std::atomic<std::shared_ptr<BotSnapshot const>>> slots_;
    mutable std::shared_mutex slot_map_mtx_;   // ONLY mutex in V2 hot path. Protects map insertion only;
                                                // per-slot access is lock-free atomic.
};
```

**Note**: this is the only `std::shared_mutex` permitted by the architecture. It guards *map structure* (insert on bot creation), not per-slot access. AI workers reading a slot never wait for it.

### 5.3 AiWorkerPool
```cpp
class AiWorkerPool {
public:
    AiWorkerPool(size_t worker_count);
    ~AiWorkerPool();

    // Called by TickScheduler. Owns the bot's tick.
    void schedule_tick(BotId);

    // Lifecycle
    void start();
    void stop();

    // Diagnostics
    PerfStats stats() const;
};
```

### 5.4 FleetThread
```cpp
class FleetThread {
public:
    FleetThread();
    ~FleetThread();

    void start();
    void stop();

    // Fleet operations submit themselves here from world-thread hooks.
    template<class F> void post(F&& fn);
};
```

---

## 6. Fleet (`Fleet/`)

### 6.1 PopulationManager
```cpp
struct PopulationTarget {
    uint32 total;
    std::array<uint32, 2>  per_faction;
    std::array<uint32, 90> per_level;
    std::array<uint32, 13> per_class;
    std::array<uint32, 3>  per_role;     // Tank/Healer/DPS
};

class PopulationManager {
public:
    void tick(Milliseconds now,
              uint32 real_player_count,
              std::function<void(BotId)> spawn_request,
              std::function<void(BotId)> despawn_request);

    PopulationTarget compute_target(Milliseconds now, uint32 real_players) const;
    PopulationTarget current() const;
};
```

### 6.2 LfgMediator
```cpp
class LfgMediator {
public:
    // Called from OnLfgQueued hook
    void on_player_queued(ObjectGuid leader, uint32 dungeon_id,
                          std::vector<ObjectGuid> existing_members);

    // Selects bots to fill missing roles, emits LfgQueueIntent into their queues.
    // Bots then queue *normally* — LFG matchmaker pairs them with the player's group.
};
```

### 6.3 BgFiller
```cpp
class BgFiller {
public:
    void tick(Milliseconds now);

    // Maintains bot queue presence to fill BGs in shortage of real players.
    // Faction-balanced. Skill-tier matched.
};
```

### 6.4 NeighborhoodPopulator
```cpp
class NeighborhoodPopulator {
public:
    void tick(Milliseconds now);

    // For under-populated neighborhoods, sends ConsiderBuyingPlotIntent
    // (a *suggestion*) to eligible bots. Bots evaluate per personality.
};
```

### 6.5 BotLifecycleManager
```cpp
class BotLifecycleManager {
public:
    bool can_despawn(BotId) const;     // Returns false if mid-content / mid-group
    void request_spawn(BotPersonality);
    void request_despawn(BotId);

    // Persistence
    void save_state(BotId);
    bool load_state(BotId);
};
```

---

## 7. Persistence (`Persistence/`)

```cpp
class BotPersistence {
public:
    // Per `SCHEMA.md`. Reads/writes only V2 tables; never touches `characters`.
    BotPersonality   load_personality(BotId) const;
    void             save_personality(BotId, BotPersonality const&);

    BotPreferences   load_preferences(BotId) const;
    void             save_preferences(BotId, BotPreferences const&);

    BotProgress      load_progress(BotId) const;
    void             save_progress(BotId, BotProgress const&);
};
```

---

## 8. Diagnostics

### 8.1 PerfCounters
```cpp
class PerfCounters {
public:
    void record_tick_latency(Milliseconds);
    void record_intent_emitted();
    void record_exception();
    void record_snapshot_publish();

    PerfSnapshot snapshot() const;
};

struct PerfSnapshot {
    uint64 ticks_total;
    Milliseconds tick_latency_p50, tick_latency_p99;
    uint64 intents_emitted_total;
    uint64 exceptions_total;
    uint64 snapshots_published_total;
};
```

### 8.2 BotInspector
```cpp
namespace Playerbot::Diagnostics {
    std::string Inspect(BotId);   // Used by `.playerbot inspect <name>` GM command
}
```

---

## 9. Module entry

```cpp
namespace Playerbot::V2 {

class Module {
public:
    static Module& instance();

    void Init();           // Called once during worldserver startup
    void Shutdown();       // Called during shutdown
    void OnWorldUpdate(Milliseconds diff);  // Called every world tick

    // Hook handlers (called by core via PlayerbotHooks.h)
    void OnPlayerLogin(Player*);
    void OnPlayerLogout(Player*);
    void OnDamageDealt(Unit* attacker, Unit* victim, int32 amount);
    void OnAuraApplied(Unit*, Aura*);
    // ... one per hook
};

} // namespace
```

---

## 10. Service locator (no DI framework)

V2 uses a single static service locator — not because DI is bad, but because a DI framework is exactly the kind of complexity this rewrite exists to avoid.

```cpp
class Services {
public:
    static SnapshotPublisher&   Snapshots();
    static IntentQueue&         Intents(BotId);
    static AiWorkerPool&        AiPool();
    static FleetThread&         Fleet();
    static PopulationManager&   Population();
    static LfgMediator&         Lfg();
    static BgFiller&            BgFill();
    static NeighborhoodPopulator& Neighborhoods();
    static BotLifecycleManager& Lifecycle();
    static BotPersistence&      Persistence();
    static PerfCounters&        Perf();
    static ConfigReader&        Config();
    static EncounterRegistry&   Encounters();
};
```

Initialized in `Module::Init`, torn down in `Module::Shutdown`. Tests can swap implementations.

---

## 11. Concurrency invariants (compile- and runtime-checked)

- AI worker code paths must not include `Player.h`, `Unit.h`, `Map.h`, etc. — enforced by include-what-you-use linter rule + CI grep.
- Intent execution paths must run on world thread — enforced by `assert(IsWorldThread())` at `PlayerbotAPI.cpp` entry points.
- `BotAI::tick` must not mutate any state outside the bot itself — enforced by code review and unit tests checking determinism (same input twice → same output twice).
- `EncounterScript` and APL rules must be stateless — enforced by `static_assert(std::is_empty_v<T> || /* ... */)` where applicable.

---

## 12. Worked sketch: a Hunter's first tick after entering combat

To make the contracts concrete, here's the call shape end-to-end:

1. Mob attacks bot. World thread fires `OnDamageTaken`. `Module::OnDamageTaken` writes a `BotEvent{Kind::DamageTaken, ...}` into the bot's `EventInbox`.
2. World thread tick begins. `SnapshotBuilder::build(bot_id)` produces a fresh `BotSnapshot`. `SnapshotPublisher::publish(bot_id, snap)` swaps the atomic pointer.
3. World thread also publishes the bot's group snapshot (if any) into the bundle.
4. `TickScheduler::schedule_for_tick(bot_id)` posts to `AiWorkerPool`. Worker picks it up.
5. Worker reads `Services::Snapshots().latest(bot_id)`, constructs `BotSnapshotView`. Reads `EventInbox` events.
6. `BotAI::tick(view, group_view, inbox, emit)`:
   - State is `Idle` but `view.in_combat()` is now true → `transition_to(InCombat)`.
   - Dispatch: `States::DispatchInCombat(...)`.
   - Inside: `Combat::TargetSelection::PickTarget(view, ...)` → `ObjectGuid mob`. Emit `StartAttackIntent{mob}`.
   - `auto rot = Combat::GetRotation(CLASS_HUNTER, SPEC_BM)`; `rot->tick({view, group_view}, emit)` runs APL.
   - APL evaluates rules; `ShouldBarbedShot` returns true; `DoBarbedShot` calls `emit.cast(SPELL_BARBED_SHOT, mob)`.
7. Two intents now in queue: `StartAttackIntent`, `CastSpellIntent`.
8. World thread on next tick: `IntentQueue::pop(...)` drains them. Each maps to a `PlayerbotAPI` action (`api.start_attack(mob)`, `api.cast_spell(SPELL_BARBED_SHOT, mob)`), which call into the existing `Player`/`Unit` paths *exactly as a real player's input would*.

Per `REQUIREMENTS.md` §1.1 #7: the bot took no path a player can't take.

---

## 13. What's locked vs open

**Locked**: every signature above. Implementation is free in *body*; signatures are not.

**Open** (will be settled at first review of this doc):
- Exact split between `BotSnapshot` fields and "fetch on demand via API" — current design favors batch in snapshot to avoid per-tick API calls; if size budget exceeds 8KB, demote rare fields to on-demand.
- Whether `EncounterScript` registers per NPC entry or per encounter ID (raid encounter ID exists in TrinityCore for some content). Likely both, with `npc_ids()` and an optional encounter-id grouping.
- `BotEvent::text` size (currently 64). May grow if whisper truncation hurts; cost is per-event memory.
