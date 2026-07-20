// BotSnapshotView - Ergonomic, stateless facade over BotSnapshot const&.
// Used by APL rules and state dispatch functions. CONTRACTS.md §2.2.

#pragma once

#include "BotSnapshot.h"
#include <algorithm>
#include <deque>

namespace Playerbot {

class BotSnapshotView
{
public:
    explicit BotSnapshotView(BotSnapshot const& s) : s_(&s) {}

    // Identity
    BotId  bot_id()  const { return s_->bot_id; }
    std::string const& name() const { return s_->identity.name; }
    uint8  level()   const { return s_->identity.level; }
    uint8  race()    const { return s_->identity.race; }
    // 0 = unknown, 1 = Alliance, 2 = Horde. Mercenary-aware.
    uint8  team()    const { return s_->identity.team; }
    bool   is_alliance() const { return s_->identity.team == 1u; }
    bool   is_horde()    const { return s_->identity.team == 2u; }
    uint8  cls()     const { return s_->identity.cls; }
    uint32 spec()    const { return s_->identity.spec; }
    uint8  gender()  const { return s_->identity.gender; }
    uint32 faction() const { return s_->identity.faction; }
    ObjectGuid guid() const { return s_->guid; }

    // Vital
    bool   in_combat()    const { return s_->vitals.in_combat; }
    bool   is_alive()     const { return s_->vitals.is_alive; }
    bool   is_stunned()   const { return s_->vitals.is_stunned; }
    bool   is_silenced()  const { return s_->vitals.is_silenced; }
    bool   is_rooted()    const { return s_->vitals.is_rooted; }
    bool   is_pvp()       const { return s_->vitals.is_pvp; }
    // True when any state would block cast attempts (stun + silence + active
    // cast). Caller should still check spell-specific gates (CD, resource).
    bool   can_cast() const { return !s_->vitals.is_stunned && !s_->vitals.is_silenced &&
                                     !(s_->cast.is_casting && s_->cast.current_cast_remaining.count() > 0); }
    bool   has_resurrect_request() const { return s_->death.has_resurrect_request; }
    bool   is_ghost()              const { return s_->death.is_ghost; }
    bool   has_corpse()            const { return s_->death.has_corpse; }
    uint32 corpse_map_id()         const { return s_->death.corpse_map_id; }
    float  corpse_x()              const { return s_->death.corpse_x; }
    float  corpse_y()              const { return s_->death.corpse_y; }
    float  corpse_z()              const { return s_->death.corpse_z; }
    int64  corpse_reclaim_at_unix() const { return s_->death.corpse_reclaim_at_unix; }
    float  corpse_to_graveyard_dist() const { return s_->death.corpse_to_graveyard_dist; }
    bool   has_group_invite()      const { return s_->social_events.has_group_invite; }
    ObjectGuid group_invite_leader() const { return s_->social_events.group_invite_leader; }
    bool   has_summon_pending()    const { return s_->social_events.has_summon_pending; }
    bool   has_guild_invite()      const { return s_->guild.has_invite; }
    uint64 guild_invite_id()       const { return s_->guild.invite_id; }
    uint64 guild_id()              const { return s_->guild.id; }
    bool   in_guild()              const { return s_->guild.id != 0; }
    uint8  guild_rank_id()         const { return s_->guild.rank_id; }
    uint16 guild_member_count()    const { return s_->guild.member_count; }
    uint16 guild_online_member_count() const { return s_->guild.online_member_count; }
    // SC-P3c: online guild members that are NOT bots (real humans). Used to
    // gate ambient/self-initiated guild chatter so bots don't babble to an
    // empty (bot-only) guild. See GuildState::online_human_member_count.
    uint16 guild_online_human_member_count() const { return s_->guild.online_human_member_count; }
    uint64 guild_rival_id()            const { return s_->guild.rival_id; }
    uint8  guild_active_event_kind()   const { return s_->guild.active_event_kind; }
    bool   guild_has_pending_callout() const { return s_->guild.has_pending_callout; }
    // #4C: bot-managed guild flag, resolved by the builder (world thread) so
    // guild idle rules avoid a per-tick BotGuildMgr::IsBotManaged lookup.
    bool   guild_is_bot_managed()      const { return s_->guild.is_bot_managed; }

    // #4A/#4C archetype projection. Idle rules read the read-only slice the
    // builder mirrored from BotAI::archetype() (no thread crossing).
    uint8  archetype_id()              const { return s_->archetype.archetype_id; }
    uint8  archetype_dominant_activity() const { return s_->archetype.dominant_activity; }
    uint8  archetype_econ_profile()    const { return s_->archetype.econ_profile; }
    // #4B-2(a) craft-order board projection. Read-only slice the post/claim
    // idle rules consult so they never lock the board from a worker thread.
    CraftOrderState const& craft_orders() const { return s_->craft_orders; }
    // Role-affinity lean [0=Tank,1=Healer,2=Dps] mirrored from the archetype.
    float  archetype_role_affinity(uint8 slot) const
    { return slot < s_->archetype.role_affinity.size() ? s_->archetype.role_affinity[slot] : 0.f; }
    bool   lfg_in_queue()              const { return s_->lfg.in_queue; }
    bool   lfg_in_dungeon()            const { return s_->lfg.in_dungeon; }
    uint16 completed_quest_count()     const { return s_->quest_log.completed_quest_count; }
    std::string const& owner_name() const { return s_->owner_name; }
    bool   has_owner_character()   const { return !s_->owner_name.empty(); }
    bool   has_duel_request()      const { return s_->social_events.has_duel_request; }
    ObjectGuid duel_initiator()    const { return s_->social_events.duel_initiator; }
    bool   duel_initiator_is_friend() const { return s_->social_events.duel_initiator_is_friend; }
    bool   has_quest_share()       const { return s_->social_events.shared_quest_id != 0; }
    bool   has_trade_request()     const { return s_->social_events.has_trade_request; }
    uint32 lfg_proposal_id()       const { return s_->lfg.proposal_id; }
    bool   has_lfg_proposal()      const { return s_->lfg.proposal_id != 0; }
    bool   lfg_role_check_pending() const { return s_->lfg.role_check_pending; }
    uint8  lfg_published_role()    const { return s_->lfg.published_role; }
    bool   lfg_vote_kick_active()  const { return s_->lfg.vote_kick_active; }
    size_t auctions_owned_count()  const { return s_->auction.auctions_owned.size(); }
    // #4B buy-side: cheapest current listings for wanted reagents. Empty
    // unless the bot is at an auctioneer (on-demand scan).
    std::vector<BotSnapshot::BuyableListing> const& buyable_listings() const
    { return s_->auction.buyable_listings; }
    // #4B-1 Part 3 buy-side: cheapest commodity unit price + available qty
    // per wanted reagent. Empty unless the bot is at an auctioneer. Most
    // craft reagents are commodities and land here, not in buyable_listings.
    std::vector<BotSnapshot::BuyableCommodity> const& buyable_commodities() const
    { return s_->auction.buyable_commodities; }
    ObjectGuid quest_share_sender() const { return s_->social_events.quest_share_sender; }
    uint32 shared_quest_id()       const { return s_->social_events.shared_quest_id; }
    uint16 food_drink_count()      const { return s_->consumables.food_drink_count; }
    uint16 potion_count()          const { return s_->consumables.potion_count; }
    uint16 bandage_count()         const { return s_->consumables.bandage_count; }
    int32  hp()           const { return s_->vitals.hp; }
    int32  max_hp()       const { return s_->vitals.max_hp; }
    // int64 intermediate prevents overflow at high HP pools — modern raid
    // characters can hit 50M+ max HP, and (hp * 100) overflows int32 there.
    int32  hp_pct()       const { return s_->vitals.max_hp > 0 ? static_cast<int32>((int64_t(s_->vitals.hp) * 100) / s_->vitals.max_hp) : 0; }
    int32  power(uint8 type) const { return type < s_->vitals.power.size() ? s_->vitals.power[type] : 0; }
    int32  max_power(uint8 type) const { return type < s_->vitals.max_power.size() ? s_->vitals.max_power[type] : 0; }
    int32  power_pct(uint8 type) const
    {
        const int32 maxp = max_power(type);
        return maxp > 0 ? (power(type) * 100) / maxp : 0;
    }

    // Position & motion
    bool   is_moving()    const { return s_->movement.is_moving; }
    bool   is_indoors()   const { return s_->area.is_indoors; }
    bool   is_swimming()   const { return s_->movement.is_swimming; }
    bool   on_transport()      const { return s_->environment.on_transport; }
    bool   transport_stopped() const { return s_->environment.transport_stopped; }
    bool   transport_is_ship() const { return s_->environment.transport_is_ship; }
    bool   is_underwater() const { return s_->environment.is_underwater; }
    float  water_surface_z() const { return s_->environment.water_surface_z; }
    bool   is_in_damaging_liquid() const { return s_->environment.is_in_damaging_liquid; }
    // Open-world water-escape focus (FIX #12): nearest DRY footing the builder
    // resolved while the bot is in swim-water; idle:water_escape drives to it.
    bool   water_escape_valid() const { return s_->environment.water_escape_valid; }
    float  water_escape_x() const { return s_->environment.water_escape_x; }
    float  water_escape_y() const { return s_->environment.water_escape_y; }
    float  water_escape_z() const { return s_->environment.water_escape_z; }
    bool   is_flying()     const { return s_->movement.is_flying; }
    bool   is_mounted()   const { return s_->movement.is_mounted; }
    uint32 map_id()       const { return s_->position.map_id; }
    // BG-orphan signature helper: standing on a battleground/arena map while
    // no Battleground object claims the bot (end-of-match removal missed it).
    bool   is_bg_orphan() const
    { return s_->position.map_is_bg_or_arena && !s_->bg.in_battleground; }
    uint32 zone_id()      const { return s_->area.zone_id; }
    uint32 area_id()      const { return s_->area.area_id; }
    uint32 map_difficulty() const { return s_->instance_ctx.map_difficulty; }
    bool   is_in_instance() const { return s_->instance_ctx.is_in_instance; }
    bool   is_in_dungeon()  const { return s_->instance_ctx.is_in_dungeon; }
    bool   is_in_raid()     const { return s_->instance_ctx.is_in_raid; }
    bool   is_sanctuary()   const { return s_->vitals.is_sanctuary; }
    bool   is_ffa_pvp()     const { return s_->vitals.is_ffa_pvp; }
    void   position(float& x, float& y, float& z) const { x = s_->position.x; y = s_->position.y; z = s_->position.z; }

    // Cooldowns / readiness
    bool   gcd_active()   const { return s_->cooldowns.gcd_remaining.count() > 0; }
    Ms     gcd_remaining() const { return s_->cooldowns.gcd_remaining; }
    bool   is_casting()   const { return s_->cast.is_casting; }
    uint32 current_cast_spell_id() const { return s_->cast.current_cast_spell_id; }
    Ms     current_cast_remaining() const { return s_->cast.current_cast_remaining; }
    ObjectGuid current_cast_target() const { return s_->cast.current_cast_target; }
    uint32 last_cast_spell_id() const { return s_->cast.last_cast_spell_id; }
    bool   is_ready(uint32 spell_id) const;
    Ms     cd_remaining(uint32 spell_id) const;
    uint8  charges(uint32 spell_id) const;
    // True if `spell_id` can be cast while moving (instant cast OR no
    // Movement interrupt flag). Looked up from SpellInfo via the global
    // SpellMgr — that data is read-only after init, so the call is safe
    // from AI worker threads. APL rules pair this with `is_moving()` to
    // gate hard-cast spells: `if (s.is_moving() && !s.can_cast_while_moving(SP)) return false;`.
    // Returns true when the spell is unknown (no filtering — let the cast
    // attempt and its server-side gates decide).
    bool   can_cast_while_moving(uint32 spell_id) const;

    // Auras
    bool             has_aura(uint32 spell_id, ObjectGuid on = ObjectGuid::Empty) const;
    AuraEntry const* find_aura(uint32 spell_id, ObjectGuid on = ObjectGuid::Empty) const;
    // Stack count of the bot's `spell_id` aura on `on`. Returns 0 when absent.
    // Use for proc spending (Maelstrom Weapon at 5, Demonic Core at 4, etc.)
    // and stack-aware refresh (Festering Wound, Vampiric Touch dot ramp).
    uint8            aura_stacks(uint32 spell_id, ObjectGuid on = ObjectGuid::Empty) const;
    // Find an enemy in `range` yards that the bot could DoT but doesn't yet
    // carry the bot's `spell_id`. Used by multi-DoT specs (Affliction Warlock
    // Agony/Corruption/UA, Boomy Moonfire/Sunfire, Shadow Priest SW:P/VT,
    // Feral Rake/Rip). Returns nullptr when every visible enemy already has
    // the dot, or when the spec isn't on the multi-dot scan list (the
    // builder only populates outbound auras on enemies for those specs).
    NearbyUnit const* enemy_without_my_aura(uint32 spell_id, float range = 40.0f) const;
    bool             target_dispellable(DispelType type) const;
    // True if the bot itself has a harmful aura of the given dispel type.
    // Solo / non-grouped healers need this — group-level dispel_candidate()
    // returns nullptr when there's no group, so a solo Resto Druid couldn't
    // Nature's Cure their own Curse without it.
    bool             self_dispellable(DispelType type) const;
    // True if any harmful aura on the bot carries the given Mechanic
    // (e.g. MECHANIC_FEAR=5, MECHANIC_STUN=12, MECHANIC_SILENCE=9). Used
    // by reactive defensives like Tremor Totem (fear), trinkets (stun/root),
    // Berserker Rage (fear/sap immunity).
    bool             has_mechanic(uint32 mechanic) const;

    // Targets
    ObjectGuid        current_target() const { return s_->combat.current_target; }
    // True only when current_target is a live, legally-attackable unit
    // (world-thread IsValidAttackTarget). Autonomy gates use this instead
    // of selection presence — a selfbot owner's friendly/self selection
    // must not freeze the quest/travel/wander cascade.
    bool              current_target_hostile() const { return s_->combat.current_target_hostile; }
    ObjectGuid        victim() const { return s_->combat.victim; }
    NearbyUnit const* target_info() const;
    // Look up the bot's auto-attack victim() in attackers / nearby_enemies.
    // Use this for predicates that should reason about the bot's actual cast
    // target — execute-range checks, gap-close distance, ground-AoE
    // placement. target_info() is keyed off `current_target` which can be a
    // friendly unit (heal target) for healers and is also stale until tab
    // updates; victim_info() always reflects the unit DPS abilities will hit.
    NearbyUnit const* victim_info() const;
    NearbyUnit const* lowest_hp_friend() const;
    NearbyUnit const* highest_threat_attacker() const;
    // First attacker in the threat list that is currently casting an
    // interruptible spell. APL rules use this for kick / counterspell.
    NearbyUnit const* interruptible_caster() const;
    // PvP-priority interrupt target. Walks nearby_enemies (NOT just
    // attackers) for the closest enemy whose role is Healer AND who is
    // currently casting an interruptible spell. Returns nullptr if no
    // such healer is in range.
    //
    // Why this is distinct from `interruptible_caster()`: the existing
    // accessor only returns from `combat.attackers` — a friendly-target
    // enemy healer is INVISIBLE to it because they're not attacking the
    // bot. In PvP, the enemy healer hardly ever attacks; APLs that gate
    // interrupts on `interruptible_caster()` alone never kick healers,
    // which is the highest-impact PvP CC target.
    //
    // PvP APLs should use this FIRST, then fall back to
    // `interruptible_caster()` if it returns nullptr.
    NearbyUnit const* enemy_healer_to_interrupt(float range = 30.f) const;
    // PvP-aware interrupt target selector. Prefers enemy_healer_to_interrupt
    // when given an `in_pvp` hint (battleground or arena), then falls back
    // to interruptible_caster(). Predicates call this with the spell's
    // actual range. Returns nullptr when nothing kickable is in range.
    //
    // This is the canonical interrupt picker — APLs that just need "who
    // should I kick" should call this rather than interruptible_caster()
    // directly, so the picker can evolve (healer escalation, focus-target,
    // etc.) in one place.
    NearbyUnit const* kick_target(bool in_pvp, float range = 30.f) const;
    // Returns closest enemy within `range` of the friendly flag carrier
    // (Defense of the Ancients / WSG style). The carrier itself is located
    // by matching `bg.friendly_flag_carrier` against `nearby_friends`. If
    // the carrier isn't in our snapshot (out of range), returns nullptr.
    //
    // Use case: FC-peel rules. Warrior Hamstring, Rogue Crippling Poison,
    // Druid Feral Maim should fire on enemies near the carrier — not just
    // on the bot's current victim. Returns nullptr when not in a BG that
    // has a friendly flag carrier.
    NearbyUnit const* enemy_near_friendly_carrier(float range = 8.f) const;
    // Count of units actively attacking us. Used by AoE rule predicates.
    // NOTE: raw size — INCLUDES untargetable trigger units (49521 stalkers).
    // For "real combat density" use fightable_attackers_count() below.
    size_t attackers_count() const { return s_->combat.attackers.size(); }
    // Stalker-free count of FIGHTABLE attackers (excludes UNINTERACTIBLE /
    // pacified / dead) — see CombatTargetsState::fightable_attackers. Combat
    // and advance density gates read THIS so the untargetable-trigger flood
    // (Deadmines harbor: 8-12 no-damage "Vanessa Lightning Stalker" 49521)
    // can neither jam a pull-segmentation gate nor mis-fire AoE/panic rules.
    size_t fightable_attackers_count() const { return s_->combat.fightable_attackers; }
    // True when any active attacker is a hostile Player (vs a creature).
    // Drives the open-world-PvP awareness rules: bots flee at a higher HP
    // threshold against players because real players are unpredictable
    // (kiting, vanishes, escape CDs) while mobs are not. Cheap O(N≤16) walk.
    bool under_player_attack() const
    {
        for (auto const& a : s_->combat.attackers)
            if (a.is_player) return true;
        return false;
    }
    // Count of nearby hostile units within `range` yards of the bot.
    size_t enemies_within(float range) const;
    // Count of units ACTIVELY ATTACKING the bot that are within `range`
    // yards (i.e. real melee danger). Distinct from enemies_within, which
    // counts ANY nearby hostile — including a pet-class bot's pet's own
    // targets that are not attacking the owner at all. "Kite N melee"
    // decisions must use THIS: a full-HP hunter whose pet pulled a camp had
    // enemies_within(8) >= 2 yet attackers_count() == 0, and so leapt away
    // from its pet's fight forever (Zekani, 60min in-combat, 0 XP).
    size_t melee_attackers_within(float range) const
    {
        float bx, by, bz; position(bx, by, bz);
        const float r2 = range * range;
        size_t n = 0;
        for (auto const& a : s_->combat.attackers)
        {
            if (a.hp <= 0) continue;
            // Skip un-fightable trigger units (UNINTERACTIBLE stalkers /
            // pacified): they exert no melee pressure, so counting them as
            // "melee on me" mis-drives kite/flee decisions (the harbor
            // northward-fragmentation; mirrors the pet-camp guard above).
            if (a.untargetable || a.is_pacified) continue;
            const float dx = a.x - bx, dy = a.y - by, dz = a.z - bz;
            if (dx * dx + dy * dy + dz * dz <= r2) ++n;
        }
        return n;
    }
    // Tank helper: first nearby enemy that is in combat with someone other than
    // this bot (and not already in our attackers list). Used to drive taunt /
    // threat-grab behavior. Returns nullptr if every nearby enemy is on us.
    NearbyUnit const* untaunted_enemy(float range = 30.f) const;

    // Directional look-ahead. Returns the closest threatening hostile
    // creature inside a `half_width`-yard corridor from the bot's
    // current position toward (tx, ty), bounded at `max_forward` yards
    // forward. Excludes `exclude_guid` (use for the bot's intended
    // walk target so we don't flag the very mob we're trying to
    // reach), corpses, players (PvP routing differs — see
    // `under_player_attack`), and grey-conned mobs (level + 2 < bot
    // level → no aggro / trivial). Returns nullptr when the corridor
    // is clear.
    //
    // Drives the "look-ahead before moving into a mob pack" behavior
    // that humans do reflexively: scan the planned path, route around
    // or pull single targets before they pull the whole pack. Without
    // this, bots walk in a straight line through aggro radii and die.
    NearbyUnit const* path_threat(float tx, float ty,
                                  float max_forward = 35.0f,
                                  float half_width  = 10.0f,
                                  ObjectGuid exclude_guid = ObjectGuid::Empty) const;

    // Same filters as path_threat but counts ALL hostiles in the
    // corridor instead of returning the nearest. Drives the lateral
    // re-route rule: when ≥3 hostiles cluster on the path, pulling
    // the nearest chain-aggros the rest. Players sidestep around
    // the pack — emit a perpendicular move instead.
    size_t path_threat_count(float tx, float ty,
                              float max_forward = 35.0f,
                              float half_width  = 10.0f,
                              ObjectGuid exclude_guid = ObjectGuid::Empty) const;

    // Inventory
    bool   has_item(uint32 entry) const;
    uint8  bag_free_slots() const { return s_->bags.bag_free_slots; }
    int32  gold() const { return s_->inventory.gold; }
    uint32 xp() const { return s_->identity.xp; }
    uint32 xp_for_level() const { return s_->identity.xp_for_level; }
    uint32 rest_bonus_xp() const { return s_->identity.rest_bonus_xp; }
    uint32 honor_xp() const { return s_->identity.honor_xp; }
    uint32 honor_xp_for_next() const { return s_->identity.honor_xp_for_next; }
    uint32 honor_level() const { return s_->identity.honor_level; }
    uint32 honor_kills_today() const { return s_->identity.honor_kills_today; }
    uint32 honor_kills_yesterday() const { return s_->identity.honor_kills_yesterday; }
    uint32 honor_kills_lifetime() const { return s_->identity.honor_kills_lifetime; }
    uint8  upgrades_pending() const { return s_->bags.upgrades_pending; }
    uint32 published_at_ms() const { return s_->published_at_ms; }
    uint8  pet_level() const { return s_->pet.pet_level; }
    uint32 pet_family() const { return s_->pet.pet_family; }
    bool   pet_in_combat() const { return s_->pet.pet_in_combat; }

    // Combat stats — post-DR percent × 100 (so 1234 = 12.34%). Sourced from
    // Player::GetRatingBonusValue at builder time. Used by the /sheet whisper
    // and by gear-quality heuristics; APL doesn't currently consume them
    // because spec-DPS shapes prefer to spend procs/CDs over rating thresholds.
    int16 crit_pct_x100()        const { return s_->secondary_stats.crit_pct_x100; }
    int16 haste_pct_x100()       const { return s_->secondary_stats.haste_pct_x100; }
    int16 mastery_pct_x100()     const { return s_->secondary_stats.mastery_pct_x100; }
    int16 versatility_pct_x100() const { return s_->secondary_stats.versatility_pct_x100; }
    int16 resilience_pct_x100()  const { return s_->secondary_stats.resilience_pct_x100; }
    int16 pvp_power_pct_x100()   const { return s_->secondary_stats.pvp_power_pct_x100; }

    // Equipment
    uint16 average_item_level() const { return s_->inventory.average_item_level; }
    EquippedItem const& equipped(uint8 slot) const { return s_->inventory.equipped[slot < 19 ? slot : 0]; }
    // Per-slot equipped-bag info (index 0..3 = bag slots 30-33). capacity 0
    // = empty slot; subclass 0xFF = none. See BagsState for semantics.
    std::array<uint8, 4> const& equipped_bag_capacity() const { return s_->bags.equipped_bag_capacity; }
    std::array<uint8, 4> const& equipped_bag_subclass() const { return s_->bags.equipped_bag_subclass; }
    // Lowest durability across populated equipment slots (0 = broken,
    // 100 = pristine). Returns 100 when no equipped items track durability —
    // vendor-trigger callers can compare against a low threshold safely.
    uint8 lowest_equipped_durability_pct() const
    {
        uint8 lo = 100;
        bool seen = false;
        for (auto const& e : s_->inventory.equipped)
        {
            if (e.entry == 0) continue;
            seen = true;
            if (e.durability_pct < lo) lo = e.durability_pct;
        }
        return seen ? lo : 100;
    }

    // First trinket on-use spell that's actually ready to fire. Returns 0
    // when the bot has no equipped on-use trinket or both are on cooldown.
    // Trinkets live in EQUIPMENT_SLOT_TRINKET1 (13) and TRINKET2 (14); the
    // builder pre-resolved each slot's ON_USE spell so we just probe the
    // ready bit. InCombat fires this on cooldown to extract trinket value
    // without per-tick ItemTemplate lookups.
    uint32 ready_on_use_trinket() const
    {
        constexpr uint8 TRINKET1 = 13, TRINKET2 = 14;
        for (uint8 slot : {TRINKET1, TRINKET2})
        {
            const uint32 sid = s_->inventory.equipped[slot].on_use_spell_id;
            if (sid != 0 && is_ready(sid)) return sid;
        }
        return 0;
    }

    // Active path. has_path_destination() is false when the bot isn't in
    // CHASE / FOLLOW / POINT motion. Rules can check distance-to-destination
    // to avoid re-emitting MoveTo every tick.
    bool       has_path_destination() const { return s_->path.path_end_map_id != 0; }
    void       path_destination(float& x, float& y, float& z) const
                 { x = s_->path.path_end_x; y = s_->path.path_end_y; z = s_->path.path_end_z; }
    ObjectGuid path_target() const { return s_->path.path_target; }

    // Path-block telemetry — Builder copies BotAI's path_blocked_count
    // + last_path_blocked_ms. Travel/quest rules read these to detect
    // anchor wedge: snapshot the count on first emit, fall through if
    // it grows by >=3 over the next ticks (anchor unreachable). Use
    // `path_blocked_recently(now_ms, window)` for the common case of
    // "had a block in the last N ms".
    uint32 path_blocked_count()   const { return s_->path_telemetry.count; }
    uint32 last_path_blocked_ms() const { return s_->path_telemetry.last_ms; }
    bool   path_blocked_recently(uint32 now_ms, uint32 window_ms = 5000) const
    {
        const uint32 last = s_->path_telemetry.last_ms;
        return last != 0 && (now_ms - last) < window_ms;
    }

    // Spellbook
    bool   knows_spell(uint32 spell_id) const;

    // Talents (Talent.db2 ids — NOT spell ids; resolve via sTalentStore for
    // the granted spell). Returns the active spec's talent picks; empty when
    // the bot has none chosen yet.
    std::vector<uint32> const& active_talents() const { return s_->spellbook.active_talents; }
    bool has_talent(uint32 talent_id) const
    {
        return std::binary_search(s_->spellbook.active_talents.begin(),
                                  s_->spellbook.active_talents.end(), talent_id);
    }
    // Glyphs slotted on the active spec. GlyphProperties.db2 ids.
    std::vector<uint32> const& active_glyphs() const { return s_->spellbook.active_glyphs; }
    // True when the active combat trait config is the StarterBuild flagged by
    // TraitMgr's curated init. Drives the "auto-extend on ding" rule —
    // re-fires apply_starter_talents only when starter, never wipes a custom
    // build the owner picked.
    bool is_starter_build() const { return s_->spellbook.is_starter_build; }
    bool has_glyph(uint32 glyph_id) const
    {
        for (uint32 g : s_->spellbook.active_glyphs) if (g == glyph_id) return true;
        return false;
    }

    // Quest log
    QuestEntry const* find_quest(uint32 quest_id) const;
    // The single objective the bot should be actively pursuing. Empty
    // (current_quest_id() == 0) when the bot has nothing actionable.
    QuestObjectiveEntry const& current_objective() const { return s_->quest_log.current_objective; }
    uint32                     current_quest_id()  const { return s_->quest_log.current_quest_id; }
    // True when the bot has something to walk toward: a real quest objective,
    // OR a synthesized R7 leveling-zone relocation goal (current_quest_id==0
    // but current_objective_poi is a far quest hub). The POI-driven travel
    // rules key off this; quest-specific rules additionally gate on
    // current_quest_id()/objective().type so a relocation leaves them inert.
    bool                       has_current_objective() const { return s_->quest_log.current_quest_id != 0 || s_->quest_log.objective_is_relocation; }
    // R7: the current objective is a synthesized cross-map relocation goal,
    // not a real quest. Lets the few quest-assuming sites (progress observer,
    // "truly idle" wander gate) stay correct.
    bool                       objective_is_relocation() const { return s_->quest_log.objective_is_relocation; }
    // Stuck on a same-map objective the navmesh can't reach but the travel graph
    // can via a non-walk bridge (elevator / AT-teleport / intra-map ship) — drive
    // the graph route. See BotSnapshot::objective_needs_bridge.
    bool                       objective_needs_bridge() const { return s_->quest_log.objective_needs_bridge; }
    // Builder-validated bridge route legs (world-thread navmesh-checked
    // attaches). When non-empty for the current goal key, the travel-plan
    // executor MUST use these instead of recomputing the route.
    std::vector<QuestLogState::BridgeLeg> const& bridge_route() const { return s_->quest_log.bridge_route; }
    uint64                     bridge_route_goal_key() const { return s_->quest_log.bridge_route_goal_key; }
    BotSnapshot::QuestObjectivePoi const& current_objective_poi() const { return s_->quest_log.current_objective_poi; }
    BotSnapshot::QuestAreaTrigger const& current_objective_areatrigger() const { return s_->quest_log.current_objective_areatrigger; }
    BotSnapshot::QuestTool const& current_objective_tool() const { return s_->quest_log.current_objective_tool; }
    uint16 riding_skill() const { return s_->travel.riding_skill; }
    uint32 best_mount_spell() const { return s_->travel.best_mount_spell; }
    uint32 estimated_repair_cost() const { return s_->vendor_visit.estimated_repair_cost; }
    uint8  vendor_visit_phases_pending() const { return s_->vendor_visit.phases_pending; }
    uint8  smallest_bag_capacity() const { return s_->bags.smallest_bag_capacity; }
    bool   has_empty_bag_slot()    const { return s_->bags.has_empty_bag_slot; }
    uint16 health_potion_count() const { return s_->consumables.health_potion_count; }
    uint16 mana_potion_count()   const { return s_->consumables.mana_potion_count; }
    std::array<float, 12> const& spec_stat_weights() const { return s_->stat_weights.spec_stat_weights; }
    float spec_weapon_dps_weight() const { return s_->stat_weights.spec_weapon_dps_weight; }
    std::vector<BotSnapshot::ActionableObjective> const& actionable_objectives() const { return s_->quest_log.actionable_objectives; }
    ObjectGuid gossip_npc() const { return s_->gossip.gossip_npc; }
    std::vector<BotSnapshot::GossipMenuOption> const& gossip_options() const { return s_->gossip.gossip_options; }
    // First nearby NPC/GO that accepts a turn-in for one of the bot's
    // complete quests, with the quest id. Lets a State_Idle rule emit
    // QuestCompleteIntent without re-doing the giver/quest matching.
    BotSnapshot::QuestTurnIn const* nearest_quest_turnin() const
    {
        return s_->quest_discovery.quest_turnins.empty() ? nullptr : &s_->quest_discovery.quest_turnins.front();
    }
    BotSnapshot::QuestTurnIn const* nearest_quest_offer() const
    {
        return s_->quest_discovery.quest_offers.empty() ? nullptr : &s_->quest_discovery.quest_offers.front();
    }
    bool current_objective_blacklisted() const { return s_->quest_log.current_objective_blacklisted; }
    bool has_relocation_target() const { return s_->quest_log.has_relocation_target; }

    // Quest-first arbitration (2026-06-16). True when the bot has a quest ACTION
    // worth prioritizing over opportunistic maintenance (equip/vendor/gather/
    // mail/AH/loot_chest). The single shared predicate every opportunistic idle
    // gate calls so it YIELDS to questing — fixing the priority inversion where a
    // bot AT its objective ran idle:equip_upgrade (the dominant GoalUnreachable
    // wedge). Three arms:
    //   (1) a ready turn-in giver in interaction scan range,
    //   (2) an acceptable quest-offer giver in interaction scan range,
    //   (3) a REAL quest (NOT an R7 relocation — those keep using the travel
    //       pipeline and must NOT starve maintenance on long hauls) that is
    //       either currently executable (a nearby actionable objective — the
    //       builder pre-filters blacklisted ones out) OR has a valid same-map
    //       objective POI within reach_yd (+ POI radius).
    // Arm (3)'s POI branch is guarded by current_objective_blacklisted(): a bot
    // stranded within reach of an UNREACHABLE/wedged POI must NOT keep
    // suppressing vendor/repair or it livelocks (the dominant live wedge class).
    // Arms (1)/(2)/actionable are already proximity-scoped by the builder.
    bool has_actionable_quest(float reach_yd = 80.0f) const
    {
        if (nearest_quest_turnin() != nullptr) return true;
        if (nearest_quest_offer()  != nullptr) return true;
        // A questless bot with an R7 relocation target is doing quest WORK (going
        // to where quests are) — yield maintenance so travel_to_hub can move it
        // instead of equip_upgrade firing in place. Covers the directly-walkable
        // same-map relocation that synthesizes no POI (see BotSnapshot.h note).
        if (has_relocation_target()) return true;
        if (current_quest_id() == 0 || objective_is_relocation()) return false;
        if (!actionable_objectives().empty()) return true;
        auto const& poi = current_objective_poi();
        if (!poi.valid || poi.map_id != map_id() || current_objective_blacklisted())
            return false;
        float sx = 0.f, sy = 0.f, sz = 0.f;
        position(sx, sy, sz);
        const float dx = poi.x - sx, dy = poi.y - sy;
        const float reach = reach_yd + poi.radius;
        return (dx * dx + dy * dy) <= (reach * reach);
    }

    // Skills. skill_value(skill_id) returns 0 when the bot doesn't have
    // the skill. Useful for gather rules ("Herbalism ≥ 200 to gather").
    uint16 skill_value(uint16 skill_id) const
    {
        for (auto const& e : s_->progression.skills)
            if (e.skill_id == skill_id) return e.value;
        return 0;
    }
    bool   has_skill(uint16 skill_id) const { return skill_value(skill_id) > 0; }
    uint16 skill_max(uint16 skill_id) const
    {
        for (auto const& e : s_->progression.skills)
            if (e.skill_id == skill_id) return e.max;
        return 0;
    }
    bool   is_skill_capped(uint16 skill_id) const
    {
        for (auto const& e : s_->progression.skills)
            if (e.skill_id == skill_id)
                return e.value > 0 && e.value >= e.max;
        return false;
    }

    // Mail. has_drainable_mail() returns true when at least one mail is
    // delivered AND carries something to take (money OR an item) — what an
    // at-mailbox rule actually cares about. next_drainable_mail() returns
    // the oldest such mail (front of the vector, since GetMails() returns
    // delivery order); rules drive a take-money + take-each-item +
    // (when empty, no COD) delete sequence over successive ticks.
    size_t            mail_count() const { return s_->mailbox.mail.size(); }
    uint32            unread_mail_count() const { return s_->mailbox.unread_mail_count; }
    bool              has_drainable_mail() const;
    MailEntry const*  next_drainable_mail() const;

    // Taxi — has the bot visited / been granted node `node_id`? Lets AI
    // gate fly_to_node intents on pre-known endpoints rather than
    // catching the Result::Locked rebound. node_id is a TaxiNodes.dbc id.
    bool              is_taxi_node_known(uint32 node_id) const
    {
        const size_t byte = node_id / 8;
        const uint8  bit  = uint8(1) << (node_id % 8);
        return byte < s_->travel.taxi_mask.size() && (s_->travel.taxi_mask[byte] & bit) != 0;
    }

    // Recommended taxi route accessors — populated by the snapshot builder
    // when the bot has a long-distance same-map goal and a viable known-FM
    // → known-FM route exists. has_recommended_taxi_route() reflects "≥2
    // hops, both endpoints known"; the AI rule walks to the start FM and
    // emits fly_to_node(start_fm, dest_node) on arrival.
    ObjectGuid recommended_taxi_start_fm() const { return s_->travel.recommended_taxi_start_fm; }
    uint32     recommended_taxi_dest_node() const { return s_->travel.recommended_taxi_dest_node; }
    uint16     recommended_taxi_hop_count() const { return s_->travel.recommended_taxi_hop_count; }
    float      recommended_taxi_start_x() const { return s_->travel.recommended_taxi_start_x; }
    float      recommended_taxi_start_y() const { return s_->travel.recommended_taxi_start_y; }
    float      recommended_taxi_start_z() const { return s_->travel.recommended_taxi_start_z; }
    // A flight route to the goal exists (≥2 hops, both endpoints known). This is
    // now PROACTIVE: it does NOT require the start FM to be in scan range — the
    // bot walks to recommended_taxi_start_{x,y,z} first, then flies once the FM
    // is visible. start_fm may be empty during the walk phase.
    bool       has_recommended_taxi_route() const
    {
        return s_->travel.recommended_taxi_dest_node != 0 &&
               s_->travel.recommended_taxi_hop_count >= 2 &&
               (s_->travel.recommended_taxi_start_x != 0.f ||
                s_->travel.recommended_taxi_start_y != 0.f ||
                !s_->travel.recommended_taxi_start_fm.IsEmpty());
    }

    // Nearest known cross-map travel anchor. Builder fills from the
    // global PortalIndex when the bot has a cross-map goal; empty
    // otherwise. AI uses these to walk toward the portal/dock from
    // anywhere on the source map (the GO itself is invisible past 30 y
    // so we can't rely on nearby_objects for the long-distance leg).
    bool   has_nearest_portal_anchor() const
    // Use `kind` (0=none, 1=Portal, 2=Transport) as the validity sentinel,
    // NOT dest_map: map 0 (Eastern Kingdoms) is a legitimate anchor
    // destination, so `dest_map != 0` wrongly reported "no anchor" for every
    // cross-map trip whose goal is EK (e.g. Org→Undercity zeppelin). `kind`
    // is only set to a non-zero value when the snapshot actually resolves an
    // anchor (BotSnapshotBuilder publishes all five fields together).
    { return s_->travel.nearest_portal_anchor_kind != 0; }
    uint32 nearest_portal_anchor_dest_map() const { return s_->travel.nearest_portal_anchor_dest_map; }
    uint32 next_hop_dest_map()             const { return s_->travel.next_hop_dest_map; }
    bool   has_multi_hop_route()           const { return s_->travel.next_hop_dest_map != kInvalidMapId; }
    float  nearest_portal_anchor_x() const { return s_->travel.nearest_portal_anchor_x; }
    float  nearest_portal_anchor_y() const { return s_->travel.nearest_portal_anchor_y; }
    float  nearest_portal_anchor_z() const { return s_->travel.nearest_portal_anchor_z; }
    uint8  nearest_portal_anchor_kind() const { return s_->travel.nearest_portal_anchor_kind; }
    uint32 nearest_portal_anchor_entry() const { return s_->travel.nearest_portal_anchor_entry; }

    // Dungeon execution context (Phase A of GROUP_DUNGEON_PLAN.md).
    // Reads pulled from snapshot; all return zero / Empty values
    // when bot isn't in an instance map.
    bool       is_encounter_in_progress() const { return s_->dungeon_exec.is_encounter_in_progress; }
    ObjectGuid current_boss_guid() const { return s_->dungeon_exec.current_boss_guid; }
    uint32     current_boss_entry() const { return s_->dungeon_exec.current_boss_entry; }
    int32      current_boss_hp() const { return s_->dungeon_exec.current_boss_hp; }
    int32      current_boss_max_hp() const { return s_->dungeon_exec.current_boss_max_hp; }
    int32      current_boss_hp_pct() const
    {
        return s_->dungeon_exec.current_boss_max_hp > 0
            ? static_cast<int32>((int64_t(s_->dungeon_exec.current_boss_hp) * 100) / s_->dungeon_exec.current_boss_max_hp)
            : 0;
    }
    uint32     current_boss_casting_spell() const { return s_->dungeon_exec.current_boss_casting_spell; }
    bool       current_boss_casting_interruptible() const { return s_->dungeon_exec.current_boss_casting_interruptible; }
    Ms         current_boss_cast_remaining() const { return s_->dungeon_exec.current_boss_cast_remaining; }
    bool       has_visible_boss() const
    { return !s_->dungeon_exec.current_boss_guid.IsEmpty() && s_->dungeon_exec.current_boss_hp > 0; }
    uint8      members_dead_count() const { return s_->dungeon_exec.members_dead_count; }
    bool       dungeon_complete() const { return s_->dungeon_exec.dungeon_complete; }

    // Homebind position (Player::m_homebind copy). Used by the
    // hearth-leg of the cross-map cascade.
    uint32 homebind_map_id() const { return s_->travel.homebind_map_id; }
    float  homebind_x() const { return s_->travel.homebind_x; }
    float  homebind_y() const { return s_->travel.homebind_y; }
    float  homebind_z() const { return s_->travel.homebind_z; }

    // Instance entrance position (for wipe regroup). map_id == 0 means
    // not populated (open world or Map::GetEntrancePosition returned
    // none). Outparam form mirrors position() for ergonomic call sites.
    void   instance_entrance(uint32& map, float& x, float& y, float& z) const
    {
        map = s_->dungeon_exec.instance_entrance_map;
        x   = s_->dungeon_exec.instance_entrance_x;
        y   = s_->dungeon_exec.instance_entrance_y;
        z   = s_->dungeon_exec.instance_entrance_z;
    }

    // Hearthstone availability. Item 6948 in inventory AND spell 8690
    // off cooldown. Bot rules MUST gate every hearth emit on this —
    // otherwise fresh / low-level bots that never picked up a Hearthstone
    // produce a continuous "not in spellbook" log spam.
    bool   has_hearthstone() const { return s_->travel.has_hearthstone; }
    uint32 hearthstone_cd_ms() const { return s_->travel.hearthstone_cd_ms; }
    bool   can_hearth() const
    { return s_->travel.has_hearthstone && s_->travel.hearthstone_cd_ms == 0; }

    // Self-cast teleport spells the bot knows that have a resolvable
    // destination map (Mage Teleport: City, DK Death Gate, Druid
    // Teleport: Moonglade, etc). Builder pre-resolves each spell's
    // dest map via spell_target_position so this is a cheap snapshot
    // scan with no DB lookups on the AI worker.
    std::vector<BotSnapshot::SelfTeleportSpell> const& self_teleport_spells() const
    { return s_->travel.self_teleport_spells; }

    // Battleground state. queued_for(bg_type_id) gates against re-queuing.
    bool   in_battleground() const { return s_->bg.in_battleground; }
    bool   queued_for_bg() const   { return !s_->bg.queues.empty(); }
    bool   queued_for_bg(uint16 bg_type_id) const
    {
        for (auto const& q : s_->bg.queues)
            if (q.bg_type_id == bg_type_id) return true;
        return false;
    }
    size_t bg_queue_count() const { return s_->bg.queues.size(); }

    // Active BG state — populated only when in_battleground() is true.
    // current_bg_type_id() returns the BattlemasterList.dbc id of the
    // current BG (e.g., 3=WSG, 4=AB). Zero when not in a BG.
    uint16     current_bg_type_id() const { return s_->bg.current_type_id; }
    uint32     bg_score_alliance()  const { return s_->bg.score_alliance; }
    uint32     bg_score_horde()     const { return s_->bg.score_horde; }
    uint32     bg_time_remaining_sec() const { return s_->bg.time_remaining_sec; }
    // Wall-clock ms since the BG entered IN_PROGRESS (gates dropped).
    // 0 during prep / outside BGs. Drives time-gated arena hazards.
    uint32     bg_in_progress_ms()    const { return s_->bg.in_progress_ms; }
    // BG live status helpers. bg_is_live() = gates open / objective play
    // valid. bg_in_prep() = gates closed / prep phase. start_delay_ms
    // counts down to gates-open during prep.
    uint8      bg_status()          const { return s_->bg.status; }
    bool       bg_is_live()         const { return s_->bg.status == 3; }
    bool       bg_in_prep()         const { return s_->bg.status == 2; }
    uint32     bg_start_delay_ms()  const { return s_->bg.start_delay_ms; }

    // CTF flag carrier accessors. ObjectGuid is empty when there's no
    // active flag carrier on the corresponding side. carrier_hp_pct
    // values are unspecified when the corresponding GUID is empty.
    ObjectGuid const& bg_friendly_flag_carrier() const { return s_->bg.friendly_flag_carrier; }
    ObjectGuid const& bg_enemy_flag_carrier()    const { return s_->bg.enemy_flag_carrier; }
    int32 bg_friendly_carrier_hp_pct() const { return s_->bg.friendly_carrier_hp_pct; }
    int32 bg_enemy_carrier_hp_pct()    const { return s_->bg.enemy_carrier_hp_pct; }

    // BG capture-point states (live ownership view across the whole BG
    // map). Empty when bot isn't in a BG, or when the BG has no
    // CAPTURE_POINT GOs (e.g., CTF-only BGs like WSG).
    std::vector<BotSnapshot::BgNodeState> const& bg_node_states() const { return s_->bg.node_states; }

    // Vehicle state. on_vehicle gates the vehicle-action rules.
    bool       on_vehicle()             const { return s_->vehicle.on_vehicle; }
    ObjectGuid const& vehicle_guid()    const { return s_->vehicle.vehicle_guid; }
    int8       vehicle_seat_id()        const { return s_->vehicle.vehicle_seat_id; }
    uint32     vehicle_seat_ability()   const { return s_->vehicle.vehicle_seat_ability; }
    uint32     vehicle_entry()          const { return s_->vehicle.vehicle_entry; }
    int8       bg_sota_attacker_team()  const { return s_->bg.sota_attacker_team; }
    // SoTA gate state (0=unknown, 1=OK, 2=damaged, 3=destroyed) per gate.
    // Returns 0 (treat as not-on-this-map) when bot isn't in SoTA.
    uint8 bg_sota_gate_state(BotSnapshot::SotaGateId g) const
    { return s_->bg.sota_gate_state[g]; }
    // IoC keep gate destruction. Returns non-zero when gate is down.
    // Returns 0 (treat as intact / not-in-IoC) outside IoC.
    uint8 bg_ioc_gate_destroyed(BotSnapshot::IocGateId g) const
    { return s_->bg.ioc_gate_destroyed[g]; }
    // AV captain alive-state. Default true outside AV — scripts gate on
    // bg_type_id == 1 (AV) so the dead-default would never apply elsewhere.
    bool bg_av_balinda_alive()   const { return s_->bg.av_balinda_alive; }
    bool bg_av_galvangar_alive() const { return s_->bg.av_galvangar_alive; }
    // Multi-carrier vectors for BGs with concurrent carriers (Kotmogu).
    // For single-carrier BGs these contain either 0 or 1 entries.
    std::vector<ObjectGuid> const& bg_all_friendly_carriers() const
    { return s_->bg.all_friendly_carriers; }
    std::vector<ObjectGuid> const& bg_all_enemy_carriers() const
    { return s_->bg.all_enemy_carriers; }

    // Bank capacity. AI uses bank_free_slots() == 0 to skip deposit attempts;
    // bank_tab_count() lets the "buy a bank tab" rule fire when the bot
    // has gold and < 4 tabs.
    uint8  bank_tab_count()  const { return s_->bank.bank_tab_count; }
    uint16 bank_free_slots() const { return s_->bank.bank_free_slots; }

    // Nearby GameObjects. nearest_object_of_type returns the closest GO of
    // the given GAMEOBJECT_TYPE_* (mailbox/chest/herb/ore/etc) within the
    // snapshot's scan radius (~30yd), or nullptr if none. Caller can then
    // emit use_game_object(go.guid) directly.
    BotSnapshot::NearbyObject const* nearest_object_of_type(uint8 go_type) const
    {
        // nearby_objects is already sorted by distance from the bot.
        for (auto const& o : s_->world_objects.nearby_objects)
            if (o.go_type == go_type) return &o;
        return nullptr;
    }
    std::vector<BotSnapshot::NearbyObject> const& nearby_objects() const { return s_->world_objects.nearby_objects; }

    // Find the closest friendly NPC carrying any of the given npc_flag bits
    // (UNIT_NPC_FLAG_VENDOR / REPAIR / BANKER / TRAINER / FLIGHTMASTER / etc).
    // nearby_friends is sorted by distance, so the first match is the
    // closest. Returns nullptr when no nearby NPC carries the flag.
    NearbyUnit const* nearest_npc_with_flag(uint32 flag_mask) const
    {
        for (auto const& u : s_->combat.nearby_friends)
            if ((u.npc_flags & flag_mask) != 0) return &u;
        return nullptr;
    }

    // Group
    Role       my_role() const { return s_->group.my_role; }

    // Auto-detected AoE situation: in combat with 3+ attackers (the
    // canonical "this is an AoE pull" cutoff). Used by spec rotations to
    // bias toward Multi-Shot / Whirlwind / Consecration / etc. without
    // needing the owner to manually flag it. The owner-pinned aoe_preference
    // on BotAI is a separate signal — together they let the rotation
    // decide AoE-vs-ST per-tick. View access is read-only.
    bool       is_aoe_situation() const
    { return s_->vitals.in_combat && s_->combat.fightable_attackers >= 3; }
    Ms         combat_duration() const { return s_->vitals.combat_duration; }
    int64      combat_duration_ms() const { return s_->vitals.combat_duration.count(); }
    Ms         ms_since_combat_exit() const { return s_->vitals.ms_since_combat_exit; }
    int64      ms_since_combat_exit_ms() const { return s_->vitals.ms_since_combat_exit.count(); }
    // True when combat ended within the last `ms`. 0 (= never been in
    // combat, or still in combat) reads as "not recently in combat".
    bool       recently_in_combat(int64 ms = 5000) const
    {
        const int64 v = s_->vitals.ms_since_combat_exit.count();
        return v > 0 && v < ms;
    }
    ObjectGuid group_guid() const { return s_->group.group_guid; }
    bool       in_group() const { return s_->group.group_guid != ObjectGuid::Empty; }

    // ---- World metadata (operator-curated knowledge) -----------------
    //
    // Queries the singleton WorldMetadataStore loaded at server boot
    // from characters.playerbot_v2_world_metadata. The store is read-only
    // post-init (modifications via `.playerbot meta add/delete` go through
    // the same path), so no per-snapshot caching is needed — the store
    // is a stable global for the lifetime of these reads.
    //
    // Usage in rules:
    //   if (s.in_city()) { ... safer behavior, more chatty ... }
    //   if (s.near_metadata_kind(WorldMetadataKind::Danger, 80.f)) flee()
    //
    // Implemented out-of-line in BotSnapshotView.cpp so the include of
    // WorldMetadata.h doesn't leak through every TU that consumes views.

    // Distance² in yards (planar XY) to the nearest metadata point of
    // the given kind on the same map. Returns FLT_MAX if no match.
    float metadata_dist_sq(uint32 kind /*WorldMetadataKind*/) const;

    // True when bot is inside the radius of ANY metadata point of the
    // given kind on the current map.
    bool inside_metadata(uint32 kind) const;

    // Convenience predicates — common cases the rules ask about most.
    bool in_city() const;
    bool in_village() const;
    bool in_danger_zone() const;

    // "Is there a known metadata point of this kind within `range`
    // yards of the bot?" Independent of the point's own radius.
    // Useful for sniff-test queries like "does this zone have any
    // known vendor annotation?".
    bool any_metadata_within(uint32 kind, float range) const;

    // Encounter
    uint32 active_encounter_npc() const { return s_->dungeon_exec.active_encounter_npc_id; }
    uint8  active_encounter_phase() const { return s_->dungeon_exec.active_encounter_phase; }
    // True when the bot is engaged with a boss-tier target (>=5M HP). Used
    // to gate raid cooldowns (Bloodlust, Time Warp, Fury of the Aspects)
    // so they're not blown on trash. Checks the active encounter id (set by
    // builder from the attackers list) and falls back to the current target.
    bool on_boss_encounter() const
    {
        if (s_->dungeon_exec.active_encounter_npc_id != 0) return true;
        constexpr int32 BOSS_HP_THRESHOLD = 5'000'000;
        if (auto const* t = target_info())
            if (t->max_hp >= BOSS_HP_THRESHOLD) return true;
        return false;
    }

    // Pet
    ObjectGuid pet_guid() const { return s_->pet.pet_guid; }
    bool       has_pet()  const { return !s_->pet.pet_guid.IsEmpty() && s_->pet.pet_alive; }
    int32      pet_hp_pct() const { return s_->pet.pet_max_hp > 0 ? static_cast<int32>((int64_t(s_->pet.pet_hp) * 100) / s_->pet.pet_max_hp) : 0; }
    std::string const& pet_name() const { return s_->pet.pet_name; }
    bool       pet_can_bloodlust() const { return s_->pet.pet_can_bloodlust; }
    ObjectGuid pet_victim() const { return s_->pet.pet_victim; }
    std::vector<ObjectGuid> const& pet_attackers() const { return s_->pet.pet_attackers; }
    AuraEntry const* find_pet_aura(uint32 spell_id) const;
    std::vector<BotSnapshot::StablePet> const& stable_pets() const { return s_->pet.stable_pets; }
    bool       has_stabled_pets() const
    {
        for (auto const& sp : s_->pet.stable_pets)
            if (sp.slot_kind != 0 /*active*/) return true;
        return false;
    }

    // Currencies / reputations / loot rolls / nearby — raw vector accessors.
    // Read-only; APL rules walk these for predicate logic without dragging the
    // raw snapshot into rule code.
    std::vector<BotSnapshot::CurrencyEntry>   const& currencies()      const { return s_->progression.currencies; }
    std::vector<BotSnapshot::ReputationEntry> const& reputations()     const { return s_->progression.reputations; }
    std::vector<BotSnapshot::LootRollEntry>   const& loot_rolls()      const { return s_->loot.loot_rolls; }
    std::vector<NearbyUnit>                   const& attackers()       const { return s_->combat.attackers; }
    std::vector<NearbyUnit>                   const& nearby_friends()  const { return s_->combat.nearby_friends; }
    std::vector<NearbyUnit>                   const& nearby_enemies()  const { return s_->combat.nearby_enemies; }
    std::vector<BotSnapshot::QuestTurnIn>     const& quest_offers()    const { return s_->quest_discovery.quest_offers; }
    std::vector<BotSnapshot::QuestTurnIn>     const& quest_turnins()   const { return s_->quest_discovery.quest_turnins; }
    std::vector<BotSnapshot::StartingItem>    const& quest_starting_items() const { return s_->quest_discovery.quest_starting_items; }
    // Modern WoW world quest discovery index. Vector of {giver, quest_id,
    // type=0(offer)/1(turnin), pos, area, reward digest}. Empty when the
    // bot has no nearby world quest givers (typical pre-Legion zones,
    // or world quests just not deployed in DB).
    std::vector<BotSnapshot::WorldQuestEntry> const& available_world_quests() const
    { return s_->quest_discovery.available_world_quests; }
    // Scenario step tracking. scenario_id() == 0 means bot isn't in a
    // scenario instance map. Read by future scenario-aware dispatch;
    // for now consumed by the `wq` whisper for diagnostics.
    BotSnapshot::ScenarioStepInfo const& scenario_step() const { return s_->quest_log.scenario_step; }
    bool   in_scenario() const { return s_->quest_log.scenario_step.scenario_id != 0; }
    std::vector<MailEntry>                    const& mail()            const { return s_->mailbox.mail; }
    std::vector<BotSnapshot::BgQueueEntry>    const& bg_queues()       const { return s_->bg.queues; }
    std::vector<QuestEntry>                   const& quests()          const { return s_->quest_log.quests; }
    std::vector<InventoryItem>                const& bag_items()       const { return s_->inventory.bag_items; }

    // O(1) sum-of-stacks lookup via inventory.bag_count_by_entry. The
    // index is built alongside bag_items in BotSnapshotBuilder so each
    // entry maps to the SUM of stack counts across all bag slots.
    // Pre-fix: linear walk of up to 120 items per call; called from
    // has_reagents() which itself fires from many crafting / consumable
    // rules per tick.
    uint32 item_count(uint32 entry) const
    {
        // Tier 3.3: bag_count_by_entry is an accumulating sorted flat vector;
        // get() returns the summed count (0 when absent), same semantics as
        // the prior unordered_map lookup.
        return s_->inventory.bag_count_by_entry.get(entry);
    }

    // Reagent check for a recipe spell. Walks SpellInfo::Reagent[] and
    // compares each requirement against bag stacks. Returns false if any
    // reagent is short. NOTE: pulls SpellMgr — call site must be in a TU
    // that already includes SpellMgr.h. View is header-only otherwise so
    // we declare here and define in BotSnapshotView.cpp.
    bool has_reagents(uint32 spell_id) const;
    std::vector<uint32>                       const& known_spells()    const { return s_->spellbook.known_spells; }
    std::vector<uint32>                       const& known_recipes()   const { return s_->spellbook.known_recipes; }
    std::vector<BotSnapshot::SkillEntry>      const& skills()          const { return s_->progression.skills; }
    std::vector<CooldownEntry>                const& spell_cooldowns() const { return s_->cooldowns.spell_cooldowns; }

    // Currency lookup helper. Returns 0 when the bot has no balance for that id.
    uint32 currency_quantity(uint32 currency_id) const
    {
        for (auto const& c : s_->progression.currencies)
            if (c.currency_id == currency_id) return c.quantity;
        return 0;
    }
    // Reputation rank lookup (0..7 or 8 for Paragon). 0 = Hated when present;
    // returns 0 + standing 0 when the bot has no rep with that faction
    // (caller can disambiguate via reputations() walk).
    uint8 reputation_rank(uint32 faction_id) const
    {
        for (auto const& r : s_->progression.reputations)
            if (r.faction_id == faction_id) return r.rank;
        return 0;
    }
    int32 reputation_standing(uint32 faction_id) const
    {
        for (auto const& r : s_->progression.reputations)
            if (r.faction_id == faction_id) return r.standing;
        return 0;
    }

    // Raw access (last-resort)
    BotSnapshot const& raw() const { return *s_; }

private:
    BotSnapshot const* s_;
    // Backing store for find_aura()'s outbound (auras-on-others) case. The
    // builder stores outbound auras as compact OutboundAura rows, but
    // find_aura must return an AuraEntry const*. Materialising the matched
    // row here — instead of a reused thread_local — keeps every returned
    // pointer valid for the whole lifetime of this view (one AI tick), so a
    // caller holding two find_aura results (or calling again before using
    // the first) never sees aliased/overwritten data. std::deque is chosen
    // because it never invalidates existing element pointers on push_back,
    // unlike std::vector. Mutable so the const find_aura accessor can append.
    mutable std::deque<AuraEntry> outbound_aura_rows_;
};

} // namespace Playerbot
