// GroupSnapshot - Read-only summary of the bot's current group, published
// alongside per-bot snapshots each tick. CONTRACTS.md §3.1.
//
// First-iteration shape: enough fields for tank/healer/dps tactics and ready
// checks. Encounter-specific debuff lists land per-encounter as they're scripted.

#pragma once

#include "Bot/BotTypes.h"
#include "Bot/BotSnapshot.h"   // For AuraEntry reuse
#include "ObjectGuid.h"
#include <array>
#include <string>
#include <vector>

namespace Playerbot {

struct GroupMemberSummary
{
    ObjectGuid             guid;
    std::string            name;
    uint8                  level   = 0;
    uint8                  cls     = 0;
    uint32                 spec    = 0;
    Role                   role    = Role::Unknown;
    int32                  hp      = 0;
    int32                  max_hp  = 0;
    int32                  mana    = 0;     // Power index 0 (mana) only.
    int32                  max_mana = 0;
    bool                   online  = false;
    bool                   in_combat = false;
    bool                   is_mounted = false;
    // V2-managed bot vs real player (Lifecycle::is_bot at build time).
    // Drives human-aware behavior: dungeon-complete auto-leave must not
    // hearth the bots out while a HUMAN group member is still inside the
    // instance (2026-06-12: Hogger died with Overheat alive → "complete"
    // → bots tried to hearth out from under the user mid-run).
    bool                   is_bot  = false;
    // True death state (Unit::IsAlive == deathState ALIVE). hp is NOT a
    // deadness proxy: a released ghost has hp == 1 (BuildPlayerRepop sets
    // DEAD + SetHealth(1)), so `hp > 0` classifies corpse-running members
    // as alive. Duty assignment (PveGroupCoordinator) and heal-target
    // guards use this; battle-rez pickers (dead_member) intentionally
    // keep the hp==0 corpse semantics they were built on.
    bool                   is_alive = false;
    float                  x = 0.f, y = 0.f, z = 0.f;
    uint32                 map_id  = 0;
    ObjectGuid             victim;          // who they're currently attacking
    bool                   is_casting       = false;
    uint32                 casting_spell    = 0;
    // Target of the in-flight cast. Empty for self-casts and AoE. State_Dead
    // uses this to detect "someone is rezzing me, hold off on releasing"; the
    // interrupt-coordination layer uses it to avoid double-interrupting the
    // same enemy cast.
    ObjectGuid             casting_target;
    std::vector<AuraEntry> debuffs;
    // Tracked raid buffs the member currently carries. Filtered by the
    // builder to a small static allowlist (Fortitude / Mark of the Wild /
    // Battle Shout / Arcane Intellect / Blessing of the Bronze / etc.) so
    // the snapshot stays small. Used by State_Idle group-buff maintenance
    // to avoid double-casting raid buffs that another caster already
    // applied.
    std::vector<AuraEntry> buffs;

    // Overflow-safe HP percent. Top-end raid characters can exceed 50M HP,
    // where (hp * 100) overflows int32; the int64 intermediate prevents that.
    int32 hp_pct() const
    { return max_hp > 0 ? static_cast<int32>((int64_t(hp) * 100) / max_hp) : 0; }
};

struct GroupSnapshot
{
    SnapshotVer                         version = 0;
    ObjectGuid                          group_guid;
    ObjectGuid                          leader;
    uint8                               loot_method    = 0;
    uint8                               loot_threshold = 0;
    bool                                is_raid        = false;
    // Group::isLFGGroup() — formed by the dungeon finder. Drives dungeon-run
    // auto-activation for NON-LEADER bots: everyone in an LFG group queued
    // for an auto-run, including when a human player holds lead (live
    // 2026-06-11: human + 4 bots in Deadmines, bots idle at the entrance
    // because only the leader-bot path armed the run).
    bool                                is_lfg         = false;
    std::vector<GroupMemberSummary>     members;
    std::array<ObjectGuid, 8>           raid_marks{};
    bool                                ready_check_active    = false;
    Ms                                  ready_check_remaining{0};
    uint32                              active_instance_id    = 0;
    uint32                              active_encounter_npc  = 0;
};

class GroupSnapshotView
{
public:
    GroupSnapshotView() : g_(nullptr) {}
    explicit GroupSnapshotView(GroupSnapshot const& g) : g_(&g) {}

    bool       exists() const { return g_ != nullptr; }
    bool       is_raid() const { return g_ && g_->is_raid; }
    bool       is_party() const { return g_ && !g_->is_raid; }
    bool       is_lfg() const { return g_ && g_->is_lfg; }
    size_t     member_count() const { return g_ ? g_->members.size() : 0; }
    ObjectGuid leader() const { return g_ ? g_->leader : ObjectGuid::Empty; }
    uint8      loot_method() const { return g_ ? g_->loot_method : uint8(0); }
    uint8      loot_threshold() const { return g_ ? g_->loot_threshold : uint8(0); }
    uint32     active_instance_id() const { return g_ ? g_->active_instance_id : 0u; }
    uint32     active_encounter_npc() const { return g_ ? g_->active_encounter_npc : 0u; }
    bool       ready_check_active() const { return g_ && g_->ready_check_active; }
    Ms         ready_check_remaining() const { return g_ ? g_->ready_check_remaining : Ms{0}; }
    // Raid marks: WoW order is Star=0, Circle=1, Diamond=2, Triangle=3,
    // Moon=4, Square=5, Cross=6, Skull=7. Skull is universally "kill first".
    ObjectGuid raid_mark(uint8 idx) const
    { return (g_ && idx < 8) ? g_->raid_marks[idx] : ObjectGuid::Empty; }
    ObjectGuid skull_target() const { return raid_mark(7); }

    GroupMemberSummary const* me(BotId id) const;
    GroupMemberSummary const* lowest_hp(Role only = Role::Unknown) const;
    // Same as lowest_hp() but filtered to members on the given map. Healers
    // pass their own map_id so a teammate who hearthed away mid-fight isn't
    // returned as a heal target (would just InvalidTarget on cast).
    // Distance gate (audit B23): a shared map_id is NOT proximity — without
    // it the picker returned members hundreds of yards away / behind
    // terrain, the top heal rule then failed LOS/OUT_OF_RANGE every tick
    // (~50% of one boot's cast rejections) while reachable wounded members
    // were ignored. Pass caster position + max_range (>0) to skip members
    // beyond it; max_range == 0 preserves legacy unfiltered behavior.
    GroupMemberSummary const* lowest_hp_on_map(uint32 map_id, Role only = Role::Unknown,
        float cx = 0.f, float cy = 0.f, float cz = 0.f, float max_range = 0.f) const;
    GroupMemberSummary const* tank() const;
    // First group member carrying a harmful aura of the given dispel type.
    // Healers use this to find dispel work; nullptr means no candidates.
    GroupMemberSummary const* dispel_candidate(DispelType type) const;
    // First group member carrying ANY debuff whose spell_id is in the
    // priority list. Used by healer dispel rules to honor encounter /
    // M+ affix-driven priority (Bursting, Raging-enrage, raid one-shot
    // debuffs) BEFORE the normal dispel-type walk. Returns nullptr when
    // no member has any of the listed debuffs. priority_spells may be
    // empty (no priority active) — caller fast-paths in that case.
    GroupMemberSummary const* priority_dispel_candidate(
        std::vector<uint32> const& priority_spells) const;
    // First group member who is online (in-world) but at 0 HP. Battle-rez
    // classes use this to find rebirth targets mid-pull. Returns nullptr if
    // nobody in the group is dead. Pass the bot's own map_id to filter out
    // corpses on a different map (someone who hearthed away while dead) —
    // the rez cast would just fail InvalidTarget. Pass 0 to skip the filter.
    GroupMemberSummary const* dead_member(uint32 map_id = 0) const;
    // Role-prioritized dead-member picker: Tank > Healer > DPS. Real
    // players don't randomly battle-rez whoever fell first — the tank
    // gets the brez during boss fights (group falls apart without
    // mitigation), the healer second (DPS can hold without sustain
    // briefly), DPS last. Returns nullptr if no dead group member on
    // the given map. Pass the bot's own map_id to filter cross-map.
    GroupMemberSummary const* dead_member_priority(uint32 map_id = 0) const;
    // How many group members are dead on the given map (or anywhere when map_id==0).
    // Used by Mass-Resurrection style abilities to gate on a critical mass.
    uint32 count_dead(uint32 map_id = 0) const;
    // Lowest-mana caster ally — Innervate / Mana Tide Totem target. Picks from
    // members with max_mana > 0; nullptr when nobody qualifies.
    GroupMemberSummary const* lowest_mana_caster() const;

    // Heal-assignment helper: returns the Nth-lowest wounded member for the
    // calling healer, where N is the bot's index among living healers in the
    // group on the same map (sorted by GUID for stability across ticks). With
    // a single healer this collapses to "lowest wounded" — same as
    // lowest_hp_on_map(). With multiple healers it staggers picks so they
    // don't all stack on the same target and waste casts to overheal.
    // Returns nullptr when no friendly is wounded; falls back to the lowest
    // when fewer wounded than healers exist (multiple healers race to top
    // off the same person, which is acceptable when the heal pool is small).
    // Same distance gate as lowest_hp_on_map (audit B23); max_range == 0
    // preserves legacy behavior.
    GroupMemberSummary const* heal_assignment(ObjectGuid me, uint32 map_id,
        float cx = 0.f, float cy = 0.f, float cz = 0.f, float max_range = 0.f) const;

    // First group member who is missing the given long-duration raid buff.
    // Used by State_Idle group-buff maintenance to find a target for the
    // bot's class buff (PW: Fortitude, Mark of the Wild, etc.). The buff
    // must be in the builder's tracked-buff allowlist for this to work.
    // Returns nullptr if every reachable member already carries the buff.
    //
    // Distance gate: a shared map_id is NOT proximity — continents span
    // ~30,000y, so two grouped members in different zones both pass the
    // map filter. Without a range check the picker returns a member who
    // may be thousands of yards away; the cast then fails OUT_OF_RANGE /
    // LINE_OF_SIGHT every tick (observed ~287k far buff-casts in a 300MB
    // log window). Pass the caster's position and a max range (>0) to skip
    // members beyond it. max_range == 0 preserves the legacy unfiltered
    // behavior for callers that don't have a position handy.
    GroupMemberSummary const* member_missing_buff(uint32 spell_id, uint32 map_id = 0,
        float cx = 0.f, float cy = 0.f, float cz = 0.f, float max_range = 0.f) const;

    // True when any reachable member on `map_id` carries a harmful aura with
    // the given Mechanic (e.g. MECHANIC_FEAR=5). Reactive AoE-removal tools
    // (Tremor Totem, Fear Ward, Berserker Rage on group via talent) gate on
    // this so they fire only when there's actually something to clear. Pass
    // 0 for `map_id` to skip the map filter.
    bool group_has_mechanic(uint32 mechanic, uint32 map_id = 0) const;

    std::vector<GroupMemberSummary> const* members() const
    { return g_ ? &g_->members : nullptr; }

private:
    GroupSnapshot const* g_;
};

} // namespace Playerbot
