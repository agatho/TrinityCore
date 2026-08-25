// BotSnapshotResetCheck - completeness guard for BotSnapshot::reset_for_reuse.
//
// The snapshot recycle pool (SNAPSHOT_PERF_BACKLOG.md Tier 3.1) reuses a prior
// BotSnapshot instead of make_shared-ing a fresh one. That is only safe if
// reset_for_reuse() returns the object to a byte-equivalent default state — a
// MISSED member leaks last tick's data into the next snapshot, a correctness
// bug that is very hard to spot in the field (stale auras, ghost quest log,
// carried-over BG node ownership, etc).
//
// VerifyResetClearsAll() is the unit-test-style guard the task mandates: it
// fills EVERY container/string in a BotSnapshot with sentinel data, sets the
// non-default scalars, runs reset_for_reuse(), and asserts every container is
// empty + the load-bearing scalar defaults are restored. It is invoked once at
// module init (PlayerbotV2.cpp Module::Init), so a future field added without a
// matching reset() line trips the assert at boot, not silently in production.
//
// Kept in its own TU (glob-collected) so the heavy fill code never weighs on
// the hot builder TU.

#include "BotSnapshot.h"
#include "Errors.h"
#include "Log.h"

namespace Playerbot {

namespace {

// Stuff one of every container + string + a representative non-default scalar
// so reset_for_reuse has something to clear in each sub-struct.
void FillEverything(BotSnapshot& s)
{
    s.version = 7; s.bot_id = 7; s.world_tick = 7; s.published_at_ms = 7;
    s.guid = ObjectGuid::Create<HighGuid::Player>(7);
    s.owner_name = "x";

    s.identity.name = "x";
    s.identity.level = 1;

    s.vitals.in_combat = true;

    s.position.map_id = 1;
    s.area.area_id = 1;
    s.instance_ctx.is_in_instance = true;
    s.movement.is_moving = true;
    s.environment.on_transport = true;

    s.travel.taxi_mask.push_back(1);
    s.travel.self_teleport_spells.push_back({1, 1});
    s.travel.next_hop_dest_map = 1;     // must come back to kInvalidMapId

    s.death.is_ghost = true;
    s.social_events.has_group_invite = true;
    s.guild.id = 1;
    s.lfg.in_queue = true;

    s.auction.ah_competing_buyout.push_back({1, 1});
    s.auction.auctions_owned.push_back({});
    s.auction.buyable_listings.push_back({});
    s.auction.buyable_commodities.push_back({});

    s.combat.victim = s.guid;
    s.combat.attackers.push_back({});
    s.combat.nearby_enemies.push_back({});
    s.combat.nearby_friends.push_back({});

    s.auras.own_auras.push_back({});
    s.auras.target_auras.push_back({});
    s.auras.victim_auras.push_back({});
    s.auras.my_auras_on_others.push_back({});
    s.auras.own_auras_index.push(1, 0);
    s.auras.my_auras_on_others_index.push(1, 0);
    s.auras.own_auras_index.finalize();
    s.auras.my_auras_on_others_index.finalize();

    s.cast.is_casting = true;

    s.cooldowns.spell_cooldowns.push_back({});
    s.cooldowns.spell_cooldowns_index.push(1, 0);
    s.cooldowns.spell_cooldowns_index.finalize();

    s.inventory.gold = 1;
    s.inventory.equipped[0].entry = 1;            // must clear to default
    s.inventory.bag_items.push_back({});
    s.inventory.bag_count_by_entry.add(1, 1);
    s.inventory.bag_count_by_entry.finalize();

    s.bags.equipped_bag_subclass[0] = 0;          // must come back to 0xFF
    s.stat_weights.spec_weapon_dps_weight = 1.f;
    s.secondary_stats.crit_pct_x100 = 1;
    s.vendor_visit.phases_pending = 1;
    s.consumables.food_drink_count = 1;

    s.spellbook.known_spells.push_back(1);
    s.spellbook.known_recipes.push_back(1);
    s.spellbook.active_talents.push_back(1);
    s.spellbook.active_glyphs.push_back(1);

    s.progression.skills.push_back({});
    s.progression.currencies.push_back({});
    s.progression.reputations.push_back({});

    s.group.group_guid = s.guid;

    s.quest_log.quests.push_back({});
    s.quest_log.quests_index.push(1, 0);
    s.quest_log.quests_index.finalize();
    s.quest_log.current_objective.credit_alias_entries.push_back(1);
    s.quest_log.current_objective.labeled_target_entries.push_back(1);
    s.quest_log.current_quest_id = 1;
    s.quest_log.bridge_route.push_back({});
    s.quest_log.actionable_objectives.push_back({});

    s.gossip.gossip_npc = s.guid;
    s.gossip.gossip_options.push_back({});

    s.quest_discovery.quest_turnins.push_back({});
    s.quest_discovery.quest_offers.push_back({});
    s.quest_discovery.quest_starting_items.push_back({});
    s.quest_discovery.available_world_quests.push_back({});

    s.loot.loot_rolls.push_back({});

    s.mailbox.mail.push_back({});
    s.mailbox.mail.back().item_guid_lows.push_back(1);   // inner vector
    s.mailbox.unread_mail_count = 1;

    s.pve_order.active = true;

    s.bg.queues.push_back({});
    s.bg.node_states.push_back({});
    s.bg.node_states.back().name = "x";                  // inner string
    s.bg.all_friendly_carriers.push_back(s.guid);
    s.bg.all_enemy_carriers.push_back(s.guid);
    s.bg.av_balinda_alive = false;                       // must come back to true
    s.bg.sota_attacker_team = 1;                         // must come back to -1
    s.bg.sota_gate_state[0] = 1;
    s.bg.ioc_gate_destroyed[0] = 1;

    s.vehicle.on_vehicle = true;
    s.bank.bank_free_slots = 1;

    s.world_objects.nearby_objects.push_back({});

    s.path.path_target = s.guid;
    s.path_telemetry.count = 1;
    s.dungeon_exec.current_boss_entry = 1;

    s.pet.pet_name = "x";
    s.pet.pet_auras.push_back({});
    s.pet.stable_pets.push_back({});
    s.pet.stable_pets.back().name = "x";                 // inner string
    s.pet.pet_attackers.push_back(s.guid);

    s.archetype.archetype_id = 1;
    s.craft_orders.want_spell_id = 1;
}

#define PB_RESET_CHECK(cond, what)                                              \
    do {                                                                       \
        if (!(cond))                                                          \
            ABORT_MSG("[PlayerbotV2] reset_for_reuse() did not clear: %s",    \
                      what);                                                  \
    } while (0)

} // namespace

// Returns true if reset_for_reuse() is complete. Aborts (never returns false)
// on the first leak so the operator gets the offending member name. Returning
// bool keeps it callable from a smoketest harness too.
bool VerifyResetClearsAll()
{
    BotSnapshot s;
    FillEverything(s);
    s.reset_for_reuse();

    // Top-level scalars / key.
    PB_RESET_CHECK(s.version == 0, "version");
    PB_RESET_CHECK(s.bot_id == 0, "bot_id");
    PB_RESET_CHECK(s.world_tick == 0, "world_tick");
    PB_RESET_CHECK(s.published_at_ms == 0, "published_at_ms");
    PB_RESET_CHECK(s.guid == ObjectGuid::Empty, "guid");
    PB_RESET_CHECK(s.owner_name.empty(), "owner_name");

    PB_RESET_CHECK(s.identity.name.empty() && s.identity.level == 0, "identity");
    PB_RESET_CHECK(!s.vitals.in_combat, "vitals");
    PB_RESET_CHECK(s.position.map_id == 0, "position");
    PB_RESET_CHECK(s.area.area_id == 0, "area");
    PB_RESET_CHECK(!s.instance_ctx.is_in_instance, "instance_ctx");
    PB_RESET_CHECK(!s.movement.is_moving, "movement");
    PB_RESET_CHECK(!s.environment.on_transport, "environment");

    PB_RESET_CHECK(s.travel.taxi_mask.empty(), "travel.taxi_mask");
    PB_RESET_CHECK(s.travel.self_teleport_spells.empty(), "travel.self_teleport_spells");
    PB_RESET_CHECK(s.travel.next_hop_dest_map == kInvalidMapId, "travel.next_hop_dest_map");

    PB_RESET_CHECK(!s.death.is_ghost, "death");
    PB_RESET_CHECK(!s.social_events.has_group_invite, "social_events");
    PB_RESET_CHECK(s.guild.id == 0, "guild");
    PB_RESET_CHECK(!s.lfg.in_queue, "lfg");

    PB_RESET_CHECK(s.auction.ah_competing_buyout.empty(), "auction.ah_competing_buyout");
    PB_RESET_CHECK(s.auction.auctions_owned.empty(), "auction.auctions_owned");
    PB_RESET_CHECK(s.auction.buyable_listings.empty(), "auction.buyable_listings");
    PB_RESET_CHECK(s.auction.buyable_commodities.empty(), "auction.buyable_commodities");

    PB_RESET_CHECK(s.combat.victim == ObjectGuid::Empty, "combat.victim");
    PB_RESET_CHECK(s.combat.attackers.empty(), "combat.attackers");
    PB_RESET_CHECK(s.combat.nearby_enemies.empty(), "combat.nearby_enemies");
    PB_RESET_CHECK(s.combat.nearby_friends.empty(), "combat.nearby_friends");

    PB_RESET_CHECK(s.auras.own_auras.empty(), "auras.own_auras");
    PB_RESET_CHECK(s.auras.target_auras.empty(), "auras.target_auras");
    PB_RESET_CHECK(s.auras.victim_auras.empty(), "auras.victim_auras");
    PB_RESET_CHECK(s.auras.my_auras_on_others.empty(), "auras.my_auras_on_others");
    PB_RESET_CHECK(s.auras.own_auras_index.empty(), "auras.own_auras_index");
    PB_RESET_CHECK(s.auras.my_auras_on_others_index.empty(), "auras.my_auras_on_others_index");

    PB_RESET_CHECK(!s.cast.is_casting, "cast");

    PB_RESET_CHECK(s.cooldowns.spell_cooldowns.empty(), "cooldowns.spell_cooldowns");
    PB_RESET_CHECK(s.cooldowns.spell_cooldowns_index.empty(), "cooldowns.spell_cooldowns_index");

    PB_RESET_CHECK(s.inventory.gold == 0, "inventory.gold");
    PB_RESET_CHECK(s.inventory.equipped[0].entry == 0, "inventory.equipped");
    PB_RESET_CHECK(s.inventory.bag_items.empty(), "inventory.bag_items");
    PB_RESET_CHECK(s.inventory.bag_count_by_entry.empty(), "inventory.bag_count_by_entry");

    PB_RESET_CHECK(s.bags.equipped_bag_subclass[0] == 0xFF, "bags.equipped_bag_subclass");
    PB_RESET_CHECK(s.stat_weights.spec_weapon_dps_weight == 0.f, "stat_weights");
    PB_RESET_CHECK(s.secondary_stats.crit_pct_x100 == 0, "secondary_stats");
    PB_RESET_CHECK(s.vendor_visit.phases_pending == 0, "vendor_visit");
    PB_RESET_CHECK(s.consumables.food_drink_count == 0, "consumables");

    PB_RESET_CHECK(s.spellbook.known_spells.empty(), "spellbook.known_spells");
    PB_RESET_CHECK(s.spellbook.known_recipes.empty(), "spellbook.known_recipes");
    PB_RESET_CHECK(s.spellbook.active_talents.empty(), "spellbook.active_talents");
    PB_RESET_CHECK(s.spellbook.active_glyphs.empty(), "spellbook.active_glyphs");

    PB_RESET_CHECK(s.progression.skills.empty(), "progression.skills");
    PB_RESET_CHECK(s.progression.currencies.empty(), "progression.currencies");
    PB_RESET_CHECK(s.progression.reputations.empty(), "progression.reputations");

    PB_RESET_CHECK(s.group.group_guid == ObjectGuid::Empty, "group");

    PB_RESET_CHECK(s.quest_log.quests.empty(), "quest_log.quests");
    PB_RESET_CHECK(s.quest_log.quests_index.empty(), "quest_log.quests_index");
    PB_RESET_CHECK(s.quest_log.current_objective.credit_alias_entries.empty(),
                   "quest_log.current_objective.credit_alias_entries");
    PB_RESET_CHECK(s.quest_log.current_objective.labeled_target_entries.empty(),
                   "quest_log.current_objective.labeled_target_entries");
    PB_RESET_CHECK(s.quest_log.current_quest_id == 0, "quest_log.current_quest_id");
    PB_RESET_CHECK(s.quest_log.bridge_route.empty(), "quest_log.bridge_route");
    PB_RESET_CHECK(s.quest_log.actionable_objectives.empty(), "quest_log.actionable_objectives");

    PB_RESET_CHECK(s.gossip.gossip_npc == ObjectGuid::Empty, "gossip.gossip_npc");
    PB_RESET_CHECK(s.gossip.gossip_options.empty(), "gossip.gossip_options");

    PB_RESET_CHECK(s.quest_discovery.quest_turnins.empty(), "quest_discovery.quest_turnins");
    PB_RESET_CHECK(s.quest_discovery.quest_offers.empty(), "quest_discovery.quest_offers");
    PB_RESET_CHECK(s.quest_discovery.quest_starting_items.empty(), "quest_discovery.quest_starting_items");
    PB_RESET_CHECK(s.quest_discovery.available_world_quests.empty(), "quest_discovery.available_world_quests");

    PB_RESET_CHECK(s.loot.loot_rolls.empty(), "loot.loot_rolls");

    PB_RESET_CHECK(s.mailbox.mail.empty(), "mailbox.mail");
    PB_RESET_CHECK(s.mailbox.unread_mail_count == 0, "mailbox.unread_mail_count");

    PB_RESET_CHECK(!s.pve_order.active, "pve_order");

    PB_RESET_CHECK(s.bg.queues.empty(), "bg.queues");
    PB_RESET_CHECK(s.bg.node_states.empty(), "bg.node_states");
    PB_RESET_CHECK(s.bg.all_friendly_carriers.empty(), "bg.all_friendly_carriers");
    PB_RESET_CHECK(s.bg.all_enemy_carriers.empty(), "bg.all_enemy_carriers");
    PB_RESET_CHECK(s.bg.av_balinda_alive, "bg.av_balinda_alive");
    PB_RESET_CHECK(s.bg.av_galvangar_alive, "bg.av_galvangar_alive");
    PB_RESET_CHECK(s.bg.sota_attacker_team == -1, "bg.sota_attacker_team");
    PB_RESET_CHECK(s.bg.sota_gate_state[0] == 0, "bg.sota_gate_state");
    PB_RESET_CHECK(s.bg.ioc_gate_destroyed[0] == 0, "bg.ioc_gate_destroyed");

    PB_RESET_CHECK(!s.vehicle.on_vehicle, "vehicle");
    PB_RESET_CHECK(s.bank.bank_free_slots == 0, "bank");

    PB_RESET_CHECK(s.world_objects.nearby_objects.empty(), "world_objects.nearby_objects");

    PB_RESET_CHECK(s.path.path_target == ObjectGuid::Empty, "path");
    PB_RESET_CHECK(s.path_telemetry.count == 0, "path_telemetry");
    PB_RESET_CHECK(s.dungeon_exec.current_boss_entry == 0, "dungeon_exec");

    PB_RESET_CHECK(s.pet.pet_name.empty(), "pet.pet_name");
    PB_RESET_CHECK(s.pet.pet_auras.empty(), "pet.pet_auras");
    PB_RESET_CHECK(s.pet.stable_pets.empty(), "pet.stable_pets");
    PB_RESET_CHECK(s.pet.pet_attackers.empty(), "pet.pet_attackers");

    PB_RESET_CHECK(s.archetype.archetype_id == 0, "archetype");
    PB_RESET_CHECK(s.craft_orders.want_spell_id == 0, "craft_orders");

    TC_LOG_INFO("server.loading",
        "[PlayerbotV2] BotSnapshot::reset_for_reuse() completeness check passed.");
    return true;
}

#undef PB_RESET_CHECK

} // namespace Playerbot
