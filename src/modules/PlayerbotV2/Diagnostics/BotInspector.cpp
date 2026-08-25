#include "BotInspector.h"
#include "PerfCounters.h"
#include "WedgeWatchdog.h"
#include "../PlayerbotV2.h"   // Module::instance().wedge_watchdog() for WedgesReport
#include "Bot/BotAI.h"
#include "Bot/Formation.h"
#include "Fleet/OwnerRegistry.h"
#include "Bot/BotRegistry.h"
#include "Bot/BotPersonality.h"
#include "../Services.h"
#include "../Bot/Dungeon/DungeonScript.h"
#include "../Threading/SnapshotPublisher.h"
#include "../Threading/AiWorkerPool.h"
#include "../Threading/IntentQueue.h"
#include "../Fleet/BotAccountMgr.h"
#include "../Fleet/BotIdentityRegistry.h"
#include "../Fleet/BotPopulationManager.h"
#include "../Fleet/BotSetupPipeline.h"
#include "../Session/BotSessionMgr.h"
#include "Bot/BotIntent.h"  // IntentKindName — shared intent-label table for /diag
#include "Bot/BotSnapshot.h"
#include "Bot/Battleground/BattlegroundScript.h"
#include "Bot/BotSnapshotView.h"
#include "Combat/ApRegistry.h"
#include "Combat/ApRotation.h"
#include "CharacterCache.h"
#include "ObjectGuid.h"
#include "DB2Stores.h"      // sAreaTableStore / sMapStore for FleetMaps zone names
#include "DatabaseEnv.h"    // CharacterDatabase for HealthReport pipeline counts + queue depth
#include "BattlegroundMgr.h"
#include "Battleground.h"
#include "Log.h"            // sLog->GetLogsDir() for DumpFreezeForensics path
#include <fstream>          // freeze-dump file output
#include <ctime>            // timestamp format
#include "World.h"          // sWorld->GetDefaultDbcLocale for FleetMaps
#include "GameTime.h"       // GameTime::GetGameTimeMS for blacklist countdown
#include "fmt/format.h"
#include "PlayerbotAPI.h"   // Result enum names for /diag

#include <algorithm>
#include <array>
#include <map>
#include <unordered_map>
#include <vector>

namespace Playerbot::Diagnostics {

namespace {

char const* RoleName(Role r)
{
    switch (r)
    {
        case Role::Tank:    return "Tank";
        case Role::Healer:  return "Healer";
        case Role::Dps:     return "Dps";
        case Role::Unknown: return "Unknown";
    }
    return "?";
}

char const* StateName(BotState s)
{
    switch (s)
    {
        case BotState::LoggingIn:    return "LoggingIn";
        case BotState::LoggingOut:   return "LoggingOut";
        case BotState::Idle:         return "Idle";
        case BotState::Travelling:   return "Travelling";
        case BotState::Questing:     return "Questing";
        case BotState::InCombat:     return "InCombat";
        case BotState::Looting:      return "Looting";
        case BotState::Dead:         return "Dead";
        case BotState::Resurrecting: return "Resurrecting";
        case BotState::AtVendor:     return "AtVendor";
        case BotState::AtMailbox:    return "AtMailbox";
        case BotState::AtAuctionHouse: return "AtAuctionHouse";
        case BotState::InGroup:      return "InGroup";
        case BotState::InInstance:   return "InInstance";
        case BotState::Decorating:   return "Decorating";
    }
    return "?";
}

} // anonymous

std::string Inspect(BotId id)
{
    if (!Services::Initialized()) return "PlayerbotV2 not initialized.";

    auto& reg = Services::Registry();
    BotAI* ai = reg.ai(id);
    if (!ai) return fmt::format("No bot registered with id {}", id);

    auto snap = Services::Snapshots().latest(id);
    auto gsnap = Services::Snapshots().latest_group(id);
    auto* iq  = reg.intents(id);
    const size_t pending_loot = reg.peek_loot_size(id);

    // Group summary (one-liner). Empty when bot is solo.
    std::string group_line = "(solo)";
    if (gsnap)
    {
        group_line = fmt::format("members={} kind={} skull={}",
                                 gsnap->members.size(),
                                 gsnap->is_raid ? "raid" : "party",
                                 gsnap->raid_marks[7].IsEmpty()
                                     ? std::string{"(none)"}
                                     : gsnap->raid_marks[7].ToString());
    }

    // Resolve which (class, spec) rotation, if any, fires for this bot.
    char const* rotation_status = "(no snapshot)";
    size_t      rotation_rules  = 0;
    if (snap)
    {
        if (ApRotation const* rot = Combat::GetRotation(snap->identity.cls, snap->identity.spec))
        {
            rotation_status = "registered";
            rotation_rules  = rot->rule_count();
        }
        else
        {
            rotation_status = "(none for cls/spec)";
        }
    }

    const int32 hp_pct = snap && snap->vitals.max_hp > 0 ? (snap->vitals.hp * 100) / snap->vitals.max_hp : 0;
    const int32 mp_pct = snap && snap->vitals.max_power[0] > 0
                        ? (snap->vitals.power[0] * 100) / snap->vitals.max_power[0] : 0;

    // Stuck-objective blacklist countdown (0 if not blacklisted).
    const uint32 blacklist_in_ms = [&]() -> uint32
    {
        auto const& tr = ai->objective_track();
        if (tr.blacklisted_until_ms == 0) return 0u;
        const uint32 now = GameTime::GetGameTimeMS();
        return tr.blacklisted_until_ms > now ? tr.blacklisted_until_ms - now : 0u;
    }();

    return fmt::format(
        "BotId      : {}\n"
        "Name       : {}\n"
        "State      : {} (was {})\n"
        "Level/Class: {}/{}\n"
        "Spec       : {} (role={})\n"
        "ItemLevel  : {} (lowest dur {}%)\n"
        "Encounter  : {}\n"
        "HP         : {}/{} ({}%)\n"
        "Mana       : {}/{} ({}%)\n"
        "Position   : ({:.1f}, {:.1f}, {:.1f}) map={}\n"
        "InCombat   : {}  Mounted: {}  Indoors: {}  PvP: {}\n"
        "Casting    : {} ({}ms remaining)\n"
        "Victim     : {}\n"
        "Attackers  : {} (interruptible: {}, players: {})\n"
        "Auras (own): {}  Outbound: {}\n"
        "Cooldowns  : {}\n"
        "Known spell: {} (recipes: {})\n"
        "Rotation   : {} ({} rules)\n"
        "LastRule   : {}\n"
        "Group      : {}\n"
        "Snapshot v : {}\n"
        "RngState   : {:#x}\n"
        "IntentQueue: ~{} pending\n"
        "PendingLoot: {} corpse(s)\n"
        "Mail       : {} message(s) ({} unread)\n"
        "BG         : in_battleground={} queues={} type={} score=A:{}/H:{} time={}s carriers=F:{} E:{}\n"
        "Vehicle    : on={} guid={} seat={} ability_spell={}\n"
        "Bank       : {} tab(s), {} free slot(s)\n"
        "Consumable : food={} potion={} bandage={}\n"
        "Pending    : summon={} duel={} trade={} lfg_prop={} quest_share={}\n"
        "Auctions   : owned={}\n"
        "Quest queue: offers={} turnins={} starting_items={}\n"
        "CurrentObj : Q{} type={} obj#{} target={} {}/{} poi=({}{:.0f},{:.0f}) blacklist_in={}ms\n"
        "Loot rolls : {} pending\n"
        "StuckChase : victim={} ticks={}\n"
        "Honor      : level={} xp={}/{} kills_today={}/total={}\n"
        "Stats      : crit={:.2f}% haste={:.2f}% mastery={:.2f}% vers={:.2f}%\n"
        "Currencies : {} known\n"
        "Reputations: {} known\n"
        "Talents    : {} picked, {} glyphs\n"
        "Pet        : {} (lvl {} fam {} hp_pct {} combat={})\n"
        "Upgrades   : {} pending strict-ilvl swap(s)\n"
        "GuildInvite: pending={} guild_id={}\n"
        "Combat     : duration={}ms\n"
        "Instance   : in_inst={} dungeon={} raid={} diff={} sanctuary={} ffa={}\n"
        "BattleRez  : target={} acked={}\n"
        "AOE pref   : {}\n"
        "Focus      : {}\n"
        "XP         : {}/{} (rest bonus={})\n"
        "Skills     : {}  Quests: {}\n"
        "PvP stats  : resilience={:.2f}% pvp_power={:.2f}%\n"
        "Prefs      : follow_dist={:.1f} role_override={} verbose={}\n"
        "Archetype  : {} (id={}) role_aff=T{:.2f}/H{:.2f}/D{:.2f} econ={} "
                     "dom_activity={} session={}min\n"
        "Squad      : owner_acct={} formation={} slot={}\n"
        "Manual     : cmd={} target={} ttl={}ms last_owner='{}'\n"
        "Dungeon    : run_mode={} encounter={} boss={} dead_in_grp={} "
                     "script='{}' progress={}/{} (expected={}) "
                     "special={} chat_pause={}ms contrib={}K/{}D\n"
        "RuleHist   : {}\n"
        "Activity   : mode={} expired={}\n",
        id,
        snap ? snap->identity.name : std::string{"(no snapshot)"},
        StateName(ai->state()),
        StateName(ai->previous_state()),
        snap ? snap->identity.level : 0,
        snap ? snap->identity.cls : 0,
        snap ? snap->identity.spec : 0,
        RoleName(snap ? snap->group.my_role : Role::Unknown),
        snap ? snap->inventory.average_item_level : 0,
        snap ? BotSnapshotView{*snap}.lowest_equipped_durability_pct() : uint8{100},
        snap ? snap->dungeon_exec.active_encounter_npc_id : 0,
        snap ? snap->vitals.hp : 0,
        snap ? snap->vitals.max_hp : 0,
        hp_pct,
        snap ? snap->vitals.power[0] : 0,
        snap ? snap->vitals.max_power[0] : 0,
        mp_pct,
        snap ? snap->position.x : 0.f,
        snap ? snap->position.y : 0.f,
        snap ? snap->position.z : 0.f,
        snap ? snap->position.map_id : 0,
        snap ? snap->vitals.in_combat : false,
        snap ? snap->movement.is_mounted : false,
        snap ? snap->area.is_indoors : false,
        snap ? snap->vitals.is_pvp : false,
        snap && snap->cast.is_casting ? snap->cast.current_cast_spell_id : 0,
        snap ? snap->cast.current_cast_remaining.count() : int64_t{0},
        snap ? snap->combat.victim.ToString() : std::string{"(empty)"},
        snap ? snap->combat.attackers.size() : 0,
        [&]{
            if (!snap) return std::size_t{0};
            std::size_t n = 0;
            for (auto const& a : snap->combat.attackers)
                if (a.is_casting && a.is_interruptible) ++n;
            return n;
        }(),
        [&]{
            if (!snap) return std::size_t{0};
            std::size_t n = 0;
            for (auto const& a : snap->combat.attackers)
                if (a.is_player) ++n;
            return n;
        }(),
        snap ? snap->auras.own_auras.size() : 0,
        snap ? snap->auras.my_auras_on_others.size() : 0,
        snap ? snap->cooldowns.spell_cooldowns.size() : 0,
        snap ? snap->spellbook.known_spells.size() : 0,
        snap ? snap->spellbook.known_recipes.size() : 0,
        rotation_status,
        rotation_rules,
        ai->last_rule_fired() ? ai->last_rule_fired() : "(none)",
        group_line,
        snap ? snap->version : 0,
        ai->rng().state(),
        iq ? iq->approximate_size() : 0,
        pending_loot,
        snap ? snap->mailbox.mail.size() : 0,
        snap ? snap->mailbox.unread_mail_count : 0u,
        snap ? snap->bg.in_battleground : false,
        snap ? snap->bg.queues.size() : 0,
        snap ? snap->bg.current_type_id : uint16{0},
        snap ? snap->bg.score_alliance  : uint32{0},
        snap ? snap->bg.score_horde     : uint32{0},
        snap ? snap->bg.time_remaining_sec : uint32{0},
        snap && !snap->bg.friendly_flag_carrier.IsEmpty()
            ? fmt::format("{}@{}%", snap->bg.friendly_flag_carrier.ToString(),
                          snap->bg.friendly_carrier_hp_pct)
            : std::string{"-"},
        snap && !snap->bg.enemy_flag_carrier.IsEmpty()
            ? fmt::format("{}@{}%", snap->bg.enemy_flag_carrier.ToString(),
                          snap->bg.enemy_carrier_hp_pct)
            : std::string{"-"},
        snap ? snap->vehicle.on_vehicle : false,
        snap && !snap->vehicle.vehicle_guid.IsEmpty()
            ? snap->vehicle.vehicle_guid.ToString()
            : std::string{"-"},
        snap ? int32(snap->vehicle.vehicle_seat_id) : int32{-1},
        snap ? snap->vehicle.vehicle_seat_ability : uint32{0},
        snap ? snap->bank.bank_tab_count : uint8{0},
        snap ? snap->bank.bank_free_slots : uint16{0},
        snap ? snap->consumables.food_drink_count : uint16{0},
        snap ? snap->consumables.potion_count : uint16{0},
        snap ? snap->consumables.bandage_count : uint16{0},
        snap ? snap->social_events.has_summon_pending : false,
        snap ? snap->social_events.has_duel_request : false,
        snap ? snap->social_events.has_trade_request : false,
        snap ? snap->lfg.proposal_id : 0u,
        snap ? snap->social_events.shared_quest_id : 0u,
        snap ? snap->auction.auctions_owned.size() : std::size_t{0},
        snap ? snap->quest_discovery.quest_offers.size() : std::size_t{0},
        snap ? snap->quest_discovery.quest_turnins.size() : std::size_t{0},
        snap ? snap->quest_discovery.quest_starting_items.size() : std::size_t{0},
        snap ? snap->quest_log.current_quest_id : 0u,
        snap ? snap->quest_log.current_objective.type : uint8{0},
        snap ? snap->quest_log.current_objective.storage_index : int8{0},
        snap ? snap->quest_log.current_objective.object_id : 0,
        snap ? snap->quest_log.current_objective.progress : 0,
        snap ? snap->quest_log.current_objective.amount : 0,
        snap && snap->quest_log.current_objective_poi.valid ? "Y " : "N ",
        snap ? snap->quest_log.current_objective_poi.x : 0.f,
        snap ? snap->quest_log.current_objective_poi.y : 0.f,
        blacklist_in_ms,
        snap ? snap->loot.loot_rolls.size() : std::size_t{0},
        ai->stuck_chase_victim().ToString(),
        ai->stuck_chase_ticks(),
        snap ? snap->identity.honor_level : uint8{0},
        snap ? snap->identity.honor_xp : 0u,
        snap ? snap->identity.honor_xp_for_next : 0u,
        snap ? snap->identity.honor_kills_today : 0u,
        snap ? snap->identity.honor_kills_lifetime : 0u,
        snap ? snap->secondary_stats.crit_pct_x100 / 100.0 : 0.0,
        snap ? snap->secondary_stats.haste_pct_x100 / 100.0 : 0.0,
        snap ? snap->secondary_stats.mastery_pct_x100 / 100.0 : 0.0,
        snap ? snap->secondary_stats.versatility_pct_x100 / 100.0 : 0.0,
        snap ? snap->progression.currencies.size() : std::size_t{0},
        snap ? snap->progression.reputations.size() : std::size_t{0},
        snap ? snap->spellbook.active_talents.size() : std::size_t{0},
        snap ? snap->spellbook.active_glyphs.size() : std::size_t{0},
        snap && !snap->pet.pet_name.empty() ? snap->pet.pet_name.c_str() : "(none)",
        snap ? snap->pet.pet_level : uint32{0},
        snap ? snap->pet.pet_family : uint32{0},
        snap && snap->pet.pet_max_hp > 0 ? (snap->pet.pet_hp * 100) / snap->pet.pet_max_hp : 0,
        snap ? snap->pet.pet_in_combat : false,
        snap ? snap->bags.upgrades_pending : uint8{0},
        snap ? snap->guild.has_invite : false,
        snap ? snap->guild.invite_id : uint64{0},
        snap ? snap->vitals.combat_duration.count() : int64_t{0},
        snap ? snap->instance_ctx.is_in_instance : false,
        snap ? snap->instance_ctx.is_in_dungeon : false,
        snap ? snap->instance_ctx.is_in_raid : false,
        snap ? snap->instance_ctx.map_difficulty : uint8{0},
        snap ? snap->vitals.is_sanctuary : false,
        snap ? snap->vitals.is_ffa_pvp : false,
        ai->brez_target().IsEmpty() ? std::string{"(none)"} : ai->brez_target().ToString(),
        ai->brez_acked(),
        ai->aoe_preference(),
        ai->focus_target().IsEmpty() ? std::string{"(none)"} : ai->focus_target().ToString(),
        snap ? snap->identity.xp : 0u,
        snap ? snap->identity.xp_for_level : 0u,
        snap ? snap->identity.rest_bonus_xp : 0u,
        snap ? snap->progression.skills.size() : std::size_t{0},
        snap ? snap->quest_log.quests.size() : std::size_t{0},
        snap ? snap->secondary_stats.resilience_pct_x100 / 100.0 : 0.0,
        snap ? snap->secondary_stats.pvp_power_pct_x100 / 100.0 : 0.0,
        ai->follow_distance(),
        RoleName(ai->role_override()),
        ai->verbose_logging(),
        // Archetype line (#4A) — WHAT/WHEN the bot plays. Sourced from BotAI
        // (the authoritative per-bot archetype), not the snapshot, so it's
        // correct even before the first snapshot is published.
        ArchetypeName(ai->archetype().archetype_id),
        uint32(ai->archetype().archetype_id),
        ai->archetype().role_affinity[0],
        ai->archetype().role_affinity[1],
        ai->archetype().role_affinity[2],
        [&]() -> char const* {
            switch (ai->archetype().econ_profile)
            {
                case EconProfile::Hoarder:  return "hoarder";
                case EconProfile::Balanced: return "balanced";
                case EconProfile::Reseller: return "reseller";
            }
            return "?";
        }(),
        [&]() -> char const* {
            switch (ai->archetype().dominant_activity())
            {
                case ArchetypeActivity::Solo:       return "solo";
                case ArchetypeActivity::Group:      return "group";
                case ArchetypeActivity::Pvp:        return "pvp";
                case ArchetypeActivity::Profession: return "prof";
                case ArchetypeActivity::Social:     return "social";
            }
            return "?";
        }(),
        uint32(ai->archetype().target_session_minutes),
        // Squad / Manual lines (Phase J).
        Services::Owners().GetOwner(id).account_id,
        FormationTypeName(ai->formation_type()),
        ai->formation_slot(),
        [&]() -> char const* {
            switch (ai->owner_command())
            {
                case BotAI::OwnerCommand::None:      return "auto";
                case BotAI::OwnerCommand::Follow:    return "follow";
                case BotAI::OwnerCommand::Hold:      return "hold";
                case BotAI::OwnerCommand::Stay:      return "stay";
                case BotAI::OwnerCommand::Engage:    return "engage";
                case BotAI::OwnerCommand::Disengage: return "diseng";
                case BotAI::OwnerCommand::Action:    return "action";
            }
            return "?";
        }(),
        ai->owner_target().ToString(),
        [&]() -> uint32 {
            const uint32 now_ms = GameTime::GetGameTimeMS();
            return ai->is_manual_mode(now_ms)
                ? (ai->manual_mode_until_ms() - now_ms) : 0u;
        }(),
        ai->last_owner_name().empty() ? std::string{"—"} : ai->last_owner_name(),
        // Dungeon line — run-mode + encounter state + visible boss
        // entry + group dead count. Critical for diagnosing whether
        // /squadrun took effect and whether wipe-detection fired.
        [&]() -> char const* {
            switch (ai->dungeon_run_mode())
            {
                case Playerbot::BotAI::DungeonRunMode::Off:    return "off";
                case Playerbot::BotAI::DungeonRunMode::Active: return "active";
                case Playerbot::BotAI::DungeonRunMode::Paused: return "paused";
            }
            return "?";
        }(),
        snap ? (snap->dungeon_exec.is_encounter_in_progress ? "in_progress" : "idle") : "—",
        snap ? snap->dungeon_exec.current_boss_entry : 0u,
        snap ? snap->dungeon_exec.members_dead_count  : uint8_t{0},
        // Active DungeonScript for the bot's current map (empty when no
        // script registered or bot not on a dungeon map).
        [&]() -> std::string {
            if (!snap) return "—";
            if (auto const* ds = Services::Dungeons().GetScriptFor(
                    snap->position.map_id, snap->instance_ctx.map_difficulty))
                return ds->name() ? std::string(ds->name()) : std::string("?");
            return std::string("none");
        }(),
        // Actual run progress from InstanceScript: bosses_done/total
        // (DONE state counted) — populated by snapshot builder.
        snap ? snap->dungeon_exec.bosses_done_count  : uint8_t{0},
        snap ? snap->dungeon_exec.bosses_total_count : uint8_t{0},
        // Expected bosses[] count from the script — operator can compare
        // expected vs done to see if the script's encounter list matches
        // the live BossInfo array (mismatch hints at a missing /
        // duplicate boss entry in the DungeonScript).
        [&]() -> size_t {
            if (!snap) return 0;
            if (auto const* ds = Services::Dungeons().GetScriptFor(
                    snap->position.map_id, snap->instance_ctx.map_difficulty))
            {
                Playerbot::BotSnapshotView sv(*snap);
                return ds->get_advice(sv).bosses.size();
            }
            return 0;
        }(),
        // Any encounter in SPECIAL state — sub-phase signal.
        snap ? (snap->dungeon_exec.any_boss_in_special ? "yes" : "no") : "—",
        // Chat-pause remaining (ms). 0 = no pause. Surfaces the
        // wait/hold/stop cue lifetime alongside the dungeon state so
        // operators can see why a tank stopped pulling.
        [&]() -> uint32 {
            const uint32 now = snap ? snap->published_at_ms
                                    : GameTime::GetGameTimeMS();
            const uint32 until = ai->chat_pause_until_ms();
            return until > now ? (until - now) : 0u;
        }(),
        // Per-run dungeon contribution counters (kills + deaths since
        // last map transition). Reset by BotAI::tick on map change.
        ai->dungeon_kills(),
        ai->dungeon_deaths(),
        [&]() {
            std::string s;
            ai->for_each_rule_history([&](size_t /*idx*/, char const* name)
            {
                if (!s.empty()) s += " > ";
                s += name;
            });
            return s.empty() ? std::string{"(empty)"} : s;
        }(),
        // Activity mode (#4A follow-up) — WHICH of the 30-60min behavior windows
        // the bot is in. `expired` true means the window elapsed and the next
        // idle tick will re-roll a fresh mode (BotAI::roll_activity_mode). A bot
        // in Professioning legitimately suppresses questing/travel until it
        // re-rolls (mostly to Questing). Sourced from BotAI (authoritative).
        [&]() -> char const* {
            switch (ai->activity_mode())
            {
                case BotAI::ActivityMode::Questing:      return "Questing";
                case BotAI::ActivityMode::Professioning: return "Professioning";
                case BotAI::ActivityMode::Wandering:     return "Wandering";
            }
            return "?";
        }(),
        ai->activity_mode_expired(GameTime::GetGameTimeMS()));
}

std::string SystemStatus()
{
    if (!Services::Initialized()) return "PlayerbotV2 not initialized.";
    auto p = Services::Perf().snapshot();
    // Walk the registry once for a fleet health summary at the top of the
    // status report. Cheap (single pass over snapshots); puts the most
    // operationally relevant numbers (alive/in_combat/avg_lvl/quests/stuck)
    // in front of the perf counters that follow.
    uint32 fleet_in_world = 0, fleet_alive = 0, fleet_in_combat = 0;
    uint32 fleet_with_quest = 0, fleet_quests_in_log = 0;
    uint32 fleet_stuck = 0;
    uint32 fleet_levels_total = 0, fleet_level_max = 0;
    const uint32 now_ms = GameTime::GetGameTimeMS();
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const& e)
    {
        if (!e.ai) return;
        ++fleet_in_world;
        if (auto snap = Services::Snapshots().latest(id))
        {
            if (snap->vitals.is_alive)  ++fleet_alive;
            if (snap->vitals.in_combat) ++fleet_in_combat;
            if (snap->identity.level > fleet_level_max) fleet_level_max = snap->identity.level;
            fleet_levels_total += snap->identity.level;
            if (snap->quest_log.current_quest_id != 0) ++fleet_with_quest;
            fleet_quests_in_log += static_cast<uint32>(snap->quest_log.quests.size());
        }
        const uint32 dead = e.ai->objective_track().blacklisted_until_ms;
        if (dead != 0 && dead > now_ms) ++fleet_stuck;
    });
    const uint32 fleet_avg_level = fleet_in_world > 0 ? fleet_levels_total / fleet_in_world : 0;
    return fmt::format(
        "==== Fleet ====\n"
        "In-world / alive / in-combat : {} / {} / {}\n"
        "Levels (avg / max)           : {} / {}\n"
        "Quests (with-current / in-log): {} / {}\n"
        "Stuck (blacklisted now)      : {}\n"
        "==== System ====\n"
        "Bots marked     : {}\n"
        "Pool accounts   : {} ({} chars / {} free slots before next auto-create)\n"
        "Bot sessions    : {}\n"
        "Bots registered : {}\n"
        "Snapshots live  : {}\n"
        "Ticks total     : {}\n"
        "Snapshots pub'd : {}\n"
        "World updates   : {}\n"
        "Intents emitted : {} (dropped={})\n"
        "Intents executed: {}\n"
        "Events pushed   : {} (dropped={})\n"
        "Intents failed  : {} (NotReady={} OutOfRange={} InvalidTarget={} "
                          "NotEnoughResource={} NotKnown={} ServerRefused={} "
                          "InventoryFull={} Locked={} Other={})\n"
        "Exceptions      : {}\n"
        "Tick lat (p50)  : {} ms\n"
        "Tick lat (p99)  : {} ms\n"
        "World lat (p50) : {} ms\n"
        "World lat (p99) : {} ms\n"
        "AI workers      : {} (queue high-watermark: {})\n",
        // ==== Fleet args ====
        fleet_in_world,
        fleet_alive,
        fleet_in_combat,
        fleet_avg_level,
        fleet_level_max,
        fleet_with_quest,
        fleet_quests_in_log,
        fleet_stuck,
        // ==== System args ====
        Services::Lifecycle().size(),
        Services::Accounts().pool_size(),
        Services::Accounts().total_chars(),
        (Services::Accounts().pool_size() * V2::BotAccountMgr::MAX_CHARS_PER_ACCOUNT
            > Services::Accounts().total_chars()
            ? Services::Accounts().pool_size() * V2::BotAccountMgr::MAX_CHARS_PER_ACCOUNT
              - Services::Accounts().total_chars()
            : 0),
        Services::SessionMgr().active_count(),
        Services::Registry().size(),
        Services::Snapshots().bot_count(),
        p.ticks_total,
        p.snapshots_published_total,
        p.world_updates_total,
        p.intents_emitted_total,
        p.intents_dropped_total,
        p.intents_executed_total,
        p.events_pushed_total,
        p.events_dropped_total,
        p.intents_failed_total,
        p.intents_by_result[1], // NotReady
        p.intents_by_result[2], // OutOfRange
        p.intents_by_result[3], // InvalidTarget
        p.intents_by_result[4], // NotEnoughResource
        p.intents_by_result[5], // NotKnown
        p.intents_by_result[6], // ServerRefused
        p.intents_by_result[7], // InventoryFull
        p.intents_by_result[8], // Locked
        p.intents_by_result[9], // Other
        p.exceptions_total,
        p.tick_p50.count(),
        p.tick_p99.count(),
        p.world_p50.count(),
        p.world_p99.count(),
        Services::AiPool().worker_count(),
        Services::AiPool().stats().queue_high_watermark);
}

namespace {

// Coarse activity bucket inferred from the rule-name prefix that fired most
// recently. State_Idle uses "idle:..." prefixes (see State_Idle.cpp); the
// combat rotations in Combat/Apl/Apl_*.cpp use free-form names like
// "Bloodthirst" or "Frostbolt" with NO prefix — so anything that isn't a
// known idle: bucket is assumed to be a fired combat-rotation rule and
// gets bucketed as "Engaged". Bots that have never ticked have nullptr
// last_rule and bucket as "Idle/none".
char const* ActivityBucket(char const* last_rule)
{
    if (!last_rule) return "Idle/none";
    auto starts_with = [last_rule](char const* prefix) -> bool
    {
        for (size_t i = 0; prefix[i]; ++i)
            if (last_rule[i] != prefix[i]) return false;
        return true;
    };
    if (starts_with("idle:engage_nearby_mob"))                 return "Engaged";
    if (starts_with("idle:quest_kill"))                        return "QuestKill";
    if (starts_with("idle:quest_collect_kill"))                return "QuestKill";
    if (starts_with("idle:quest_use_go"))                      return "QuestUse";
    if (starts_with("idle:quest_collect_use"))                 return "QuestUse";
    if (starts_with("idle:quest_talk"))                        return "QuestTalk";
    if (starts_with("idle:quest_path"))                        return "QuestPath";
    if (starts_with("idle:craft_skillup"))                     return "Crafting";
    if (starts_with("idle:craft_bandage"))                     return "Crafting";
    if (starts_with("idle:cook_self_food"))                    return "Crafting";
    if (starts_with("idle:quest_abandon_overlevel"))           return "Questing";
    if (starts_with("idle:wander_to_service"))                 return "ToVendor";
    if (starts_with("idle:wander_to_quest_hub"))               return "ToQuestHub";
    if (starts_with("idle:travel_to_hub"))                     return "TravelToHub";
    if (starts_with("idle:wander_to_node"))                    return "ToNode";
    if (starts_with("idle:wander"))                            return "Wandering";
    if (starts_with("idle:loot") || starts_with("idle:move_to_corpse")) return "Looting";
    if (starts_with("idle:gather"))                            return "Gathering";
    if (starts_with("idle:quest"))                             return "Questing";
    if (starts_with("idle:vendor") || starts_with("idle:repair") ||
        starts_with("idle:buy_reagent"))                       return "Restocking";
    if (starts_with("idle:buy"))                               return "Vendoring";
    if (starts_with("idle:trainer"))                           return "Training";
    if (starts_with("idle:bank"))                              return "Banking";
    if (starts_with("idle:taxi") || starts_with("idle:homebind")) return "Traveling";
    if (starts_with("idle:mail"))                              return "Mail";
    if (starts_with("idle:ooc_food") || starts_with("idle:ooc_potion") ||
        starts_with("idle:ooc_bandage") || starts_with("idle:cannibalize") ||
        starts_with("idle:ooc_heal")  || starts_with("idle:ooc_dispel") ||
        starts_with("idle:ooc_rez")   || starts_with("idle:self_buff") ||
        starts_with("idle:soulstone") || starts_with("idle:pet"))
                                                               return "Recovering";
    if (starts_with("idle:starter") || starts_with("idle:auto_equip") ||
        starts_with("idle:equip_upgrade"))                     return "Outfitting";
    if (starts_with("idle:group_invite") || starts_with("idle:guild_invite") ||
        starts_with("idle:lfg_proposal") || starts_with("idle:bg_port_accept") ||
        starts_with("idle:group_utility"))                     return "Group";
    if (starts_with("idle:ambient_emote") || starts_with("idle:conjured_item"))
                                                               return "Resting";
    if (starts_with("ingroup:follow_recall"))                  return "GrpFollow";
    if (starts_with("ingroup:assist") ||
        starts_with("ingroup:focus_pivot") ||
        starts_with("ingroup:skull_pivot") ||
        starts_with("ingroup:auto_skull"))                     return "GrpEngage";
    if (starts_with("ingroup:mount") ||
        starts_with("ingroup:dismount"))                       return "GrpMount";
    if (starts_with("ingroup:summon") ||
        starts_with("ingroup:share_quest") ||
        starts_with("ingroup:ready_check") ||
        starts_with("ingroup:lfg_role"))                       return "GrpRespond";
    if (starts_with("ingroup:"))                               return "Group";
    if (starts_with("combat:"))                                return "Engaged";
    if (starts_with("idle:"))                                  return "Idle";
    if (starts_with("dead:"))                                  return "Dead";
    // Unprefixed: a combat-rotation rule fired (Apl_*.cpp uses free-form
    // names without prefix). Aggregate them all under "Engaged" so 50 bots
    // in combat don't produce 50 buckets named after each spell.
    return "Engaged";
}

} // anonymous

std::string FleetActivity()
{
    if (!Services::Initialized()) return "PlayerbotV2 not initialized.";

    // Walk the registry once, bucket each bot's last_rule_fired. Bots whose
    // AI has never ticked yet (last_rule_fired == nullptr) bucket as
    // "Idle/none" — usually means just-spawned or just-respawned bots that
    // haven't had a snapshot yet.
    std::map<std::string, uint32> counts;
    uint32 total_in_world = 0;
    Services::Registry().for_each([&](BotId /*id*/, BotRegistryEntry const& e)
    {
        if (!e.ai) return;
        ++total_in_world;
        char const* bucket = ActivityBucket(e.ai->last_rule_fired());
        ++counts[std::string{bucket}];
    });

    if (total_in_world == 0)
        return "No bots in registry. (Spawn some via .playerbot spawn or AutoSpawnOnBoot.)";

    // Sort by descending count for readability — most active categories
    // first, so the GM sees at a glance "30 engaged, 12 wandering" rather
    // than alphabetical noise.
    std::vector<std::pair<std::string, uint32>> sorted(counts.begin(), counts.end());
    std::sort(sorted.begin(), sorted.end(), [](auto const& a, auto const& b)
    {
        return a.second != b.second ? a.second > b.second : a.first < b.first;
    });

    std::string out = fmt::format("Fleet activity ({} bots in registry):\n", total_in_world);
    for (auto const& [name, n] : sorted)
    {
        const uint32 pct = (n * 100u + total_in_world / 2u) / total_in_world;
        out += fmt::format("  {:<14} : {:>4}  ({:>3}%)\n", name, n, pct);
    }
    return out;
}

std::string FleetLevels()
{
    if (!Services::Initialized()) return "PlayerbotV2 not initialized.";

    // Walk every marked bot character (online or offline). Levels come from
    // the character cache so we can include offline bots in the histogram —
    // useful for "did we just create 50 fresh L1 bots?" sanity checks. The
    // cache is updated on level dings + character_create + character_load,
    // so the value is current for in-world bots and last-saved for offline.
    const auto ids = Services::Lifecycle().snapshot_ids();
    if (ids.empty())
        return "No marked bots in playerbot_v2_character.";

    constexpr uint8 kBinSize = 5;
    std::array<uint32, 21> bins{};   // 0..100 inclusive in 5-level bins
    uint32 in_world = 0, offline = 0, max_level = 0;
    uint64 sum_levels = 0;

    for (BotId id : ids)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        CharacterCacheEntry const* ce = sCharacterCache->GetCharacterCacheByGuid(g);
        if (!ce) continue;
        const uint8 lvl = ce->Level;
        const size_t bin = std::min<size_t>(lvl / kBinSize, bins.size() - 1);
        ++bins[bin];
        sum_levels += lvl;
        if (lvl > max_level) max_level = lvl;
        if (Services::Registry().has(id)) ++in_world; else ++offline;
    }

    const uint32 total = in_world + offline;
    if (total == 0)
        return "No marked bots resolved through character cache.";

    const double avg = double(sum_levels) / double(total);
    std::string out = fmt::format(
        "Fleet levels ({} marked: {} in-world, {} offline, avg L{:.1f}, max L{}):\n",
        total, in_world, offline, avg, max_level);

    // Print only the populated bins for readability — empty bins on a fresh
    // server would otherwise produce 20 lines of "0%".
    for (size_t i = 0; i < bins.size(); ++i)
    {
        if (!bins[i]) continue;
        const uint32 lo = uint32(i * kBinSize);
        const uint32 hi = lo + kBinSize - 1;
        const uint32 pct = (bins[i] * 100u + total / 2u) / total;
        out += fmt::format("  L{:>2}-L{:<2} : {:>4}  ({:>3}%)\n", lo, hi, bins[i], pct);
    }
    return out;
}

std::string FleetMaps()
{
    if (!Services::Initialized()) return "PlayerbotV2 not initialized.";

    // Walk the registry, read each bot's last published snapshot for
    // (map_id, zone_id). Bots whose snapshot hasn't been built yet (just
    // logged in this tick, no snapshot publisher run yet) are bucketed as
    // "(no snapshot)". Aggregating by (map, zone) rather than just zone
    // disambiguates same-named subzones across continents.
    struct Key {
        uint32 map_id; uint32 zone_id;
        bool operator<(Key const& o) const
        { return map_id != o.map_id ? map_id < o.map_id : zone_id < o.zone_id; }
    };
    std::map<Key, uint32> counts;
    uint32 total_in_world = 0;
    uint32 no_snap = 0;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const& e)
    {
        if (!e.ai) return;
        ++total_in_world;
        auto snap = Services::Snapshots().latest(id);
        if (!snap) { ++no_snap; return; }
        ++counts[Key{snap->position.map_id, snap->area.zone_id}];
    });

    if (total_in_world == 0)
        return "No bots in registry. (Spawn some via .playerbot spawn or AutoSpawnOnBoot.)";

    // Sort by descending count — the GM cares about the largest clusters
    // first ("are 80% of bots in Stormwind?") not alphabetical zone order.
    std::vector<std::pair<Key, uint32>> sorted(counts.begin(), counts.end());
    std::sort(sorted.begin(), sorted.end(), [](auto const& a, auto const& b)
    {
        return a.second != b.second ? a.second > b.second
             : (a.first.map_id != b.first.map_id ? a.first.map_id < b.first.map_id
                                                  : a.first.zone_id < b.first.zone_id);
    });

    const LocaleConstant loc = sWorld->GetDefaultDbcLocale();
    std::string out = fmt::format("Fleet zones ({} bots in registry, {} in {} zones):\n",
                                  total_in_world, total_in_world - no_snap, sorted.size());
    if (no_snap)
        out += fmt::format("  (no snapshot)  : {:>4}\n", no_snap);
    for (auto const& [key, n] : sorted)
    {
        char const* map_name  = "?";
        char const* zone_name = "?";
        if (MapEntry const* me = sMapStore.LookupEntry(key.map_id))
            map_name = me->MapName[loc];
        if (AreaTableEntry const* ae = sAreaTableStore.LookupEntry(key.zone_id))
            zone_name = ae->AreaName[loc];
        const uint32 pct = (n * 100u + total_in_world / 2u) / total_in_world;
        out += fmt::format("  {:<28} ({:<22}) : {:>4}  ({:>3}%)\n",
                           zone_name, map_name, n, pct);
    }
    return out;
}

std::string FleetQuests()
{
    if (!Services::Initialized()) return "PlayerbotV2 not initialized.";

    uint32 total_in_world = 0;
    uint32 total_quests   = 0;
    uint32 total_complete = 0;
    std::array<uint32, 24> by_obj_type{};
    std::map<uint32, uint32> by_quest_id;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const& e)
    {
        if (!e.ai) return;
        ++total_in_world;
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return;
        for (auto const& q : snap->quest_log.quests)
        {
            ++total_quests;
            ++by_quest_id[q.quest_id];
            if (q.state == 1) { ++total_complete; continue; }
            for (auto const& o : q.objectives)
            {
                if (o.type < by_obj_type.size())
                    ++by_obj_type[o.type];
            }
        }
    });
    if (total_in_world == 0)
        return "No bots in registry.";

    std::string out = fmt::format(
        "Fleet quests: {} bots, {} quests in log ({} complete awaiting turn-in)\n",
        total_in_world, total_quests, total_complete);
    out += "Objective types in flight:\n";
    auto add = [&](char const* name, uint32 idx)
    { if (by_obj_type[idx]) out += fmt::format("  {:<14} : {:>5}\n", name, by_obj_type[idx]); };
    add("KILL",         0);
    add("ITEM",         1);
    add("GAMEOBJECT",   2);
    add("TALKTO",       3);
    add("AREATRIGGER", 10);
    add("AREA_ENTER",  19);
    add("LEARNSPELL",   5);
    add("REPUTATION",   6);
    add("MONEY",        8);

    // Top-10 most-active quests
    std::vector<std::pair<uint32, uint32>> sorted(by_quest_id.begin(), by_quest_id.end());
    std::sort(sorted.begin(), sorted.end(), [](auto const& a, auto const& b)
    { return a.second > b.second; });
    out += "Top-10 quests by bot count:\n";
    for (size_t i = 0; i < sorted.size() && i < 10; ++i)
        out += fmt::format("  Q{:<7} : {:>4} bot(s)\n", sorted[i].first, sorted[i].second);
    return out;
}

std::string FleetCrafting()
{
    if (!Services::Initialized()) return "PlayerbotV2 not initialized.";

    struct ProfStat { uint32 known = 0; uint64 sum_value = 0; uint16 max_value = 0; };
    std::map<uint16, ProfStat> by_skill;
    uint32 total_recipes = 0;
    uint32 total_in_world = 0;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const& e)
    {
        if (!e.ai) return;
        ++total_in_world;
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return;
        total_recipes += uint32(snap->spellbook.known_recipes.size());
        for (auto const& sk : snap->progression.skills)
        {
            // Pro-skill filter: snapshot has weapons/armor/lang too. We care
            // about gathering + crafting (skill_id ranges 165-356). This is
            // approximate — modern WoW uses different IDs per expansion, so
            // catch the well-known ones.
            static constexpr std::array<uint16, 16> kInteresting = {
                129, // First Aid
                164, // Blacksmithing
                165, // Leatherworking
                171, // Alchemy
                182, // Herbalism
                185, // Cooking
                186, // Mining
                197, // Tailoring
                202, // Engineering
                202, // (dup)
                333, // Enchanting
                356, // Fishing
                393, // Skinning
                755, // Jewelcrafting
                773, // Inscription
                794, // Archaeology
            };
            bool keep = false;
            for (uint16 k : kInteresting) if (k == sk.skill_id) { keep = true; break; }
            if (!keep) continue;
            auto& ps = by_skill[sk.skill_id];
            ++ps.known;
            ps.sum_value += sk.value;
            if (sk.value > ps.max_value) ps.max_value = sk.value;
        }
    });
    if (total_in_world == 0)
        return "No bots in registry.";

    std::string out = fmt::format(
        "Fleet crafting: {} bots in registry, {} recipes total\n",
        total_in_world, total_recipes);
    out += "Per-profession skill stats:\n";
    for (auto const& [sid, ps] : by_skill)
    {
        const double avg = ps.known > 0 ? double(ps.sum_value) / ps.known : 0.0;
        out += fmt::format("  sk{:<3} : {:>4} bot(s) avg={:.0f} max={}\n",
                           sid, ps.known, avg, ps.max_value);
    }
    return out;
}

std::string FleetStuck()
{
    if (!Services::Initialized()) return "PlayerbotV2 not initialized.";

    uint32 total_in_world = 0;
    uint32 stuck_count    = 0;
    std::vector<std::string> lines;
    const uint32 now_ms = GameTime::GetGameTimeMS();
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const& e)
    {
        if (!e.ai) return;
        ++total_in_world;
        auto const& tr = e.ai->objective_track();
        // Currently blacklisted = blacklist deadline in future. After the
        // deadline expires (5 min after stuck_ticks threshold), the rule is
        // allowed to retry and the bot may un-stick — those bots aren't
        // "stuck right now" and shouldn't be reported here.
        if (tr.blacklisted_until_ms == 0 || tr.blacklisted_until_ms <= now_ms) return;
        ++stuck_count;
        // Resolve character name from cache for human-readable output.
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        std::string name = "?";
        if (CharacterCacheEntry const* ce = sCharacterCache->GetCharacterCacheByGuid(g))
            name = ce->Name;
        const uint32 remain_ms = tr.blacklisted_until_ms - now_ms;
        lines.push_back(fmt::format(
            "  {:<14} : Q{} obj={} stuck_ticks={} retry_in={}s",
            name, tr.quest_id, tr.obj_id, tr.stuck_ticks, remain_ms / 1000u));
    });
    if (total_in_world == 0)
        return "No bots in registry.";
    std::string out = fmt::format("Stuck bots: {}/{} (currently blacklisted; will auto-retry after timer)\n",
                                   stuck_count, total_in_world);
    for (auto const& l : lines) out += l + "\n";
    return out;
}

std::string WedgesReport()
{
    if (!Services::Initialized()) return "PlayerbotV2 not initialized.";

    WedgeWatchdog const& wd = Playerbot::V2::Module::instance().wedge_watchdog();
    std::vector<WedgeInfo> const& active = wd.active();

    std::string out = fmt::format(
        "Wedge watchdog: {} active (threshold {}s)\n",
        active.size(), wd.threshold_ms() / 1000u);

    // Group the active list by category, preserving the enum order so the
    // most-actionable categories (Navmesh / OffMesh / GoalUnreachable) read
    // consistently across invocations.
    if (active.empty())
    {
        out += "  (no bots currently wedged)\n";
    }
    else
    {
        const LocaleConstant loc = sWorld->GetDefaultDbcLocale();
        for (size_t ci = 0; ci < kWedgeCategoryCount; ++ci)
        {
            const WedgeCategory cat = static_cast<WedgeCategory>(ci);
            bool header_printed = false;
            for (WedgeInfo const& wi : active)
            {
                if (wi.cat != cat) continue;
                if (!header_printed)
                {
                    out += fmt::format("[{}]\n", WedgeCategoryName(cat));
                    header_printed = true;
                }
                char const* zone_name = "?";
                if (AreaTableEntry const* ae = sAreaTableStore.LookupEntry(wi.zone_id))
                    zone_name = ae->AreaName[loc];
                out += fmt::format(
                    "  {:<14} map={} zone={} pos=({:.0f},{:.0f},{:.0f}) "
                    "dur={}s obj={} rule={}\n",
                    wi.bot_name.empty() ? std::to_string(wi.bot) : wi.bot_name,
                    wi.map, zone_name, wi.x, wi.y, wi.z,
                    wi.duration_ms / 1000u, wi.objective, wi.last_rule);
            }
        }
    }

    // Per-category lifetime episode totals (cumulative since boot / last reset).
    auto const& totals = wd.category_totals();
    out += "Totals (episodes since boot):";
    for (size_t ci = 1; ci < kWedgeCategoryCount; ++ci)   // skip None (idx 0)
        out += fmt::format(" {}={}",
                           WedgeCategoryName(static_cast<WedgeCategory>(ci)),
                           totals[ci]);
    out += "\n";
    return out;
}

std::string FleetBg()
{
    if (!Services::Initialized()) return "PlayerbotV2 not initialized.";

    uint32 total_in_world  = 0;
    uint32 queued_count    = 0;
    uint32 in_bg_count     = 0;
    // Per-bg-type rollup: type_id -> {count, latest_score_a, latest_score_h, latest_time_remaining}
    struct BgRow { uint32 count = 0; uint32 score_a = 0; uint32 score_h = 0; uint32 time_remain = 0; uint32 carriers = 0; };
    std::unordered_map<uint16, BgRow> rollup;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const& e)
    {
        if (!e.ai) return;
        ++total_in_world;
        auto snap = Services::Snapshots().latest(id);
        if (!snap) return;
        if (!snap->bg.queues.empty()) ++queued_count;
        if (!snap->bg.in_battleground) return;
        ++in_bg_count;
        BgRow& r = rollup[snap->bg.current_type_id];
        ++r.count;
        r.score_a    = snap->bg.score_alliance;
        r.score_h    = snap->bg.score_horde;
        r.time_remain = snap->bg.time_remaining_sec;
        if (!snap->bg.friendly_flag_carrier.IsEmpty()) ++r.carriers;
        if (!snap->bg.enemy_flag_carrier.IsEmpty())    ++r.carriers;
    });
    if (total_in_world == 0)
        return "No bots in registry.";
    std::string out = fmt::format(
        "BG fleet: in_world={} queued={} in_bg={}\n",
        total_in_world, queued_count, in_bg_count);
    if (rollup.empty())
        out += "  (no bots currently inside a BG)\n";
    for (auto const& [type_id, r] : rollup)
    {
        // Lookup the per-BG script name for readability.
        char const* bg_name = "?";
        if (auto const* script = Services::Battlegrounds().GetScriptFor(type_id))
            bg_name = script->name();
        out += fmt::format(
            "  bg={:>4} ({:<22}) bots={:>3} score=A:{}/H:{} time={}s carriers_seen={}\n",
            type_id, bg_name, r.count, r.score_a, r.score_h, r.time_remain, r.carriers);
    }
    return out;
}

// Intent kind names come from Playerbot::IntentKindName (BotIntent.h) — the
// single source of truth maintained next to the IntentBody variant with a
// static_assert tripwire. This TU used to carry its own copy of the table;
// it drifted as the variant evolved (chat intents wrapped into ChatIntent,
// vendor/auction/mail/pet ops rolled into sub-variants) and every /diag
// label past index 10 was wrong — an EquipItem retry wedge surfaced as
// "Dismount | ServerRefused" on 2026-06-10 and misdirected a live
// investigation.
namespace {

char const* ResultName(uint8 r)
{
    switch (static_cast<Result>(r))
    {
        case Result::Ok:                return "Ok";
        case Result::NotReady:          return "NotReady";
        case Result::OutOfRange:        return "OutOfRange";
        case Result::InvalidTarget:     return "InvalidTarget";
        case Result::NotEnoughResource: return "NotEnoughRsrc";
        case Result::NotKnown:          return "NotKnown";
        case Result::ServerRefused:     return "ServerRefused";
        case Result::InventoryFull:     return "InvFull";
        case Result::Locked:            return "Locked";
        case Result::Other:             return "Other";
    }
    return "?";
}

// Read setup_pipeline_state + distribution_level + distribution_at as a
// best-effort sync query. Used for /diag's "DB pipeline state" line.
struct PipelineDbRow
{
    bool        present = false;
    uint8       state = 0;
    uint8       dist_level = 0;
    std::string dist_at_str;
};

PipelineDbRow ReadPipelineDbRow(uint64 char_guid_low)
{
    PipelineDbRow out;
    auto res = CharacterDatabase.PQuery(
        "SELECT setup_pipeline_state, distribution_level, "
        "IFNULL(DATE_FORMAT(distribution_at, '%Y-%m-%d %H:%i:%s'), '(null)') "
        "FROM playerbot_v2_character WHERE character_guid_low={}",
        char_guid_low);
    if (!res || !res->GetRowCount()) return out;
    Field* f = res->Fetch();
    out.present     = true;
    out.state       = f[0].GetUInt8();
    out.dist_level  = f[1].GetUInt8();
    out.dist_at_str = f[2].GetString();
    return out;
}

} // anonymous

std::string DiagBot(BotId id)
{
    if (!Services::Initialized()) return "PlayerbotV2 not initialized.";

    auto& reg = Services::Registry();
    BotAI* ai = reg.ai(id);
    if (!ai) return fmt::format("No bot registered with id {}", id);

    auto snap = Services::Snapshots().latest(id);
    const uint32 now_ms = GameTime::GetGameTimeMS();

    std::string out;
    out.reserve(2048);

    // ==== Identity ====
    out += "==== Diag ====\n";
    out += fmt::format("BotId   : {}\n", id);
    out += fmt::format("Name    : {}\n",
                       snap ? snap->identity.name : std::string{"(no snapshot)"});
    out += fmt::format("L{} cls={} spec={} role={}\n",
                       snap ? snap->identity.level : 0,
                       snap ? snap->identity.cls : 0,
                       snap ? snap->identity.spec : 0,
                       RoleName(snap ? snap->group.my_role : Role::Unknown));
    out += fmt::format("State   : {} (was {})\n",
                       StateName(ai->state()),
                       StateName(ai->previous_state()));
    out += fmt::format("Zone    : map={} zone={}\n",
                       snap ? snap->position.map_id : 0u,
                       snap ? snap->area.zone_id : 0u);

    // ==== DB pipeline state ====
    PipelineDbRow row = ReadPipelineDbRow(uint64(id));
    out += "==== Pipeline (DB) ====\n";
    if (!row.present)
        out += "  (no playerbot_v2_character row)\n";
    else
    {
        char const* state_label = (row.state == 0xFF) ? "AllDone"
                                : (row.state == 0)    ? "(none)"
                                                       : "(partial)";
        out += fmt::format("  setup_pipeline_state : 0x{:02X} {}\n",
                           uint32(row.state), state_label);
        out += fmt::format("  distribution_level   : {}\n",
                           uint32(row.dist_level));
        out += fmt::format("  distribution_at      : {}\n", row.dist_at_str);
    }
    out += fmt::format("  setup_done_cache hit : {}\n",
                       Services::Population().setup_done_cache_size() > 0 ? "tracked" : "n/a");

    // ==== Pipeline failures ====
    out += "==== Pipeline failures (last 8) ====\n";
    size_t fail_count = 0;
    V2::Fleet::PipelineFailureRing::Instance().ForEach(uint64(id),
        [&](size_t /*i*/, V2::Fleet::PipelineFailureEntry const& e)
        {
            const uint32 ago = (e.ts_ms <= now_ms) ? (now_ms - e.ts_ms) : 0u;
            out += fmt::format("  {}ms ago | bit=0x{:02X} when=0x{:02X} | {}\n",
                               ago, uint32(e.step_bit), uint32(e.state_when),
                               e.step_name);
            ++fail_count;
        });
    if (fail_count == 0) out += "  (none)\n";

    // ==== Last rules fired ====
    out += "==== Rule history (last 8) ====\n";
    size_t shown_rules = 0;
    constexpr size_t kRuleShowCap = 8;
    // BotAI keeps a 16-deep ring; show the last kRuleShowCap entries by
    // counting backwards from the total. for_each_rule_history visits
    // oldest-first, so we collect into a vector and slice the tail.
    std::vector<char const*> rules;
    ai->for_each_rule_history([&](size_t /*i*/, char const* name)
    {
        if (name) rules.push_back(name);
    });
    const size_t skip = rules.size() > kRuleShowCap ? rules.size() - kRuleShowCap : 0;
    for (size_t i = skip; i < rules.size(); ++i)
    {
        out += fmt::format("  {}\n", rules[i]);
        ++shown_rules;
    }
    if (shown_rules == 0) out += "  (no rules fired yet)\n";

    // ==== Intent execution ring (last 32) ====
    out += "==== Intents (last 32) ====\n";
    size_t shown_intents = 0;
    reg.for_each_intent_history(id,
        [&](size_t /*i*/, IntentHistoryEntry const& e)
        {
            const uint32 ago = (e.ts_ms <= now_ms) ? (now_ms - e.ts_ms) : 0u;
            if (e.x != 0.0f || e.y != 0.0f || e.z != 0.0f)
                out += fmt::format("  -{}ms | {} | {} | ({:.1f},{:.1f},{:.1f})\n",
                                   ago, IntentKindName(e.intent_kind),
                                   ResultName(e.result), e.x, e.y, e.z);
            else
                out += fmt::format("  -{}ms | {} | {}\n",
                                   ago, IntentKindName(e.intent_kind),
                                   ResultName(e.result));
            ++shown_intents;
        });
    if (shown_intents == 0) out += "  (no intents executed)\n";

    return out;
}

std::string HealthReport()
{
    if (!Services::Initialized()) return "PlayerbotV2 not initialized.";

    auto&  pop  = Services::Population();
    auto   psnap = pop.Snapshot();
    auto   p    = Services::Perf().snapshot();
    const uint32 now_ms = GameTime::GetGameTimeMS();

    std::string out;
    out.reserve(2048);

    out += "==== Population ====\n";
    out += fmt::format("Online bots : {} (target {})\n",
                       psnap.total_actual, psnap.total_target);
    for (auto const& b : psnap.buckets)
    {
        const int32 a_delta = int32(b.alliance_target) - int32(b.alliance_actual);
        const int32 h_delta = int32(b.horde_target)    - int32(b.horde_actual);
        out += fmt::format("  L{:>2}-L{:<2} | A {:>3}/{:<3} ({:+d}) | H {:>3}/{:<3} ({:+d})\n",
                           uint32(b.level_lo), uint32(b.level_hi),
                           b.alliance_actual, b.alliance_target, a_delta,
                           b.horde_actual,    b.horde_target,    h_delta);
    }

    out += "==== Budgets ====\n";
    out += fmt::format("Spawn budget remaining : {}\n", pop.spawn_budget());
    out += fmt::format("Login budget remaining : {}\n", pop.login_budget());
    {
        const uint32 next_ms = pop.ms_until_next_reconcile(now_ms);
        out += fmt::format("Next reconcile in      : {}s\n", next_ms / 1000u);
    }
    {
        const uint32 next_h = pop.ms_until_next_hygiene(now_ms);
        out += fmt::format("Next hygiene in        : {}s\n", next_h / 1000u);
    }
    {
        const uint32 next_r = pop.ms_until_next_rebalance(now_ms);
        out += fmt::format("Next rebalance in      : {}s\n", next_r / 1000u);
    }
    out += fmt::format("setup_done cache size  : {}\n",
                       pop.setup_done_cache_size());

    // ==== DB worker queue depth ====
    // QueueSize() reads an atomic counter on DatabaseWorkerPool; cheap and
    // safe to call from the world thread. A persistent backlog here is the
    // exact symptom that caused the 22-bot setup-pipeline loop the diag
    // surface was built to debug.
    out += "==== Database ====\n";
    out += fmt::format("CharacterDatabase async queue : {}\n",
                       CharacterDatabase.QueueSize());

    // ==== Setup pipeline distribution ====
    out += "==== Pipeline state distribution ====\n";
    if (auto res = CharacterDatabase.Query(
            "SELECT setup_pipeline_state, COUNT(*) "
            "FROM playerbot_v2_character "
            "WHERE distribution_level > 0 "
            "GROUP BY setup_pipeline_state ORDER BY setup_pipeline_state"))
    {
        do
        {
            Field* f = res->Fetch();
            const uint32 state = f[0].GetUInt8();
            const uint64 cnt   = f[1].GetUInt64();
            char const* tag = (state == 0)    ? "(none)"
                            : (state == 0xFF) ? "AllDone"
                                              : "(partial)";
            out += fmt::format("  state=0x{:02X} {:<10} : {}\n",
                               state, tag, cnt);
        } while (res->NextRow());
    }
    else
    {
        out += "  (no rows)\n";
    }

    // ==== Intent throughput ====
    out += "==== Intents (system-wide) ====\n";
    out += fmt::format("Emitted total  : {}\n", p.intents_emitted_total);
    out += fmt::format("Executed total : {}\n", p.intents_executed_total);
    out += fmt::format("Failed total   : {}\n", p.intents_failed_total);
    out += fmt::format("Dropped total  : {}\n", p.intents_dropped_total);

    return out;
}

std::string FleetHealthOneScreen()
{
    if (!Services::Initialized()) return "PlayerbotV2 not initialized.";

    auto& reg  = Services::Registry();
    auto& pop  = Services::Population();
    auto  p    = Services::Perf().snapshot();
    auto  psnap = pop.Snapshot();
    const uint32 now_ms = GameTime::GetGameTimeMS();

    uint32 in_world = 0, alive = 0, dead = 0, combat = 0;
    uint32 ally = 0, horde = 0;
    uint32 in_bg = 0, bg_queued = 0, in_dungeon = 0, in_lfg_queue = 0;
    uint32 tanks = 0, healers = 0, dps = 0;
    uint32 stuck_now = 0;
    uint32 intent_backlog = 0;
    uint64 level_sum = 0; uint32 level_max = 0;

    // Top-3 stuck bots by remaining blacklist time.
    struct StuckEntry { uint64 id; uint32 remaining_ms; };
    std::vector<StuckEntry> stuck_list;
    stuck_list.reserve(16);

    // #1C: per-zone path-fail heatmap, derived cheaply from the snapshot
    // path_telemetry.count (the bot's CONSECUTIVE current-wedge block count —
    // reset to 0 on any move success, NOT a lifetime tally) grouped by the bot's
    // current zone. So this is a near-real-time "which zones host currently
    // blocked/wedged bots" signal (a bot that recovered drops back to 0), at
    // zero extra cost — not a historical per-zone path-fail total.
    std::unordered_map<uint32, uint32> path_blocks_by_zone;

    reg.for_each([&](BotId id, BotRegistryEntry const& e)
    {
        if (!e.ai) return;
        ++in_world;
        auto snap = Services::Snapshots().latest(id);
        if (snap)
        {
            if (snap->path_telemetry.count > 0)
                path_blocks_by_zone[snap->area.zone_id] += snap->path_telemetry.count;
            if (snap->vitals.is_alive) ++alive; else ++dead;
            if (snap->vitals.in_combat) ++combat;
            level_sum += snap->identity.level;
            if (snap->identity.level > level_max) level_max = snap->identity.level;
            // identity.faction is the faction TEMPLATE id (per-race), not
            // a TEAM id — comparing it against 469 left ally permanently
            // 0. identity.team carries the effective team (1=Alliance,
            // 2=Horde) from GetEffectiveTeam().
            if (snap->identity.team == 1) ++ally;
            else                          ++horde;
            if (snap->bg.in_battleground)           ++in_bg;
            if (!snap->bg.queues.empty())           ++bg_queued;
            if (snap->instance_ctx.is_in_dungeon)                ++in_dungeon;
            if (snap->lfg.proposal_id != 0 || snap->lfg.role_check_pending)
                                                     ++in_lfg_queue;
            // Role bucketing from the snapshot's primary role field.
            // Role::Unknown / DPS-like roles fall to the dps bucket.
            switch (snap->group.my_role)
            {
                case Role::Tank:   ++tanks;   break;
                case Role::Healer: ++healers; break;
                default:           ++dps;     break;
            }
        }
        intent_backlog += uint32(e.intents ? e.intents->approximate_size() : 0u);
        const uint32 dead_at = e.ai->objective_track().blacklisted_until_ms;
        if (dead_at != 0 && dead_at > now_ms)
        {
            ++stuck_now;
            stuck_list.push_back({uint64(id), dead_at - now_ms});
        }
    });

    std::partial_sort(stuck_list.begin(),
                      stuck_list.begin() + std::min<size_t>(3, stuck_list.size()),
                      stuck_list.end(),
                      [](StuckEntry const& a, StuckEntry const& b)
                      { return a.remaining_ms > b.remaining_ms; });

    std::string out;
    out.reserve(1024);
    out += "=== Fleet Health (one-screen) ===\n";
    out += fmt::format("Bots in-world={} alive={} dead={} combat={} "
                       "ally={} horde={} avg_lvl={} max_lvl={}\n",
        in_world, alive, dead, combat, ally, horde,
        in_world > 0 ? uint32(level_sum / in_world) : 0, level_max);
    out += fmt::format("Activity: in_bg={} bg_queued={} in_dungeon={} lfg={}\n",
        in_bg, bg_queued, in_dungeon, in_lfg_queue);
    out += fmt::format("Roles: T={} H={} D={}\n", tanks, healers, dps);
    out += fmt::format("Marked total={} (online={})  intent_backlog={}\n",
        Services::Lifecycle().size(), in_world, intent_backlog);
    out += fmt::format("Population target={} actual={}  spawn_budget={} login_budget={}\n",
        psnap.total_target, psnap.total_actual,
        pop.spawn_budget(), pop.login_budget());
    out += fmt::format("DB queue={}  intents emitted={} executed={} failed={} dropped={}\n",
        CharacterDatabase.QueueSize(),
        p.intents_emitted_total, p.intents_executed_total,
        p.intents_failed_total, p.intents_dropped_total);
    {
        const uint64_t bg_total = p.bg_wins_total + p.bg_losses_total;
        const double   win_pct = bg_total > 0
            ? (100.0 * double(p.bg_wins_total) / double(bg_total)) : 0.0;
        out += fmt::format("BG outcomes: wins={} losses={}  win_pct={:.1f}%\n",
            p.bg_wins_total, p.bg_losses_total, win_pct);
    }
    out += fmt::format("Stuck (currently blacklisted)={}\n", stuck_now);
    for (size_t i = 0; i < std::min<size_t>(3, stuck_list.size()); ++i)
        out += fmt::format("  bot={} stuck_for_more={}s\n",
                           stuck_list[i].id, stuck_list[i].remaining_ms / 1000u);

    // ==== #1C: Wedge counts by category ====
    // Active list (right now) from the watchdog + lifetime episode totals from
    // PerfCounters. The active line is the actionable one ("8 OffMesh wedges
    // RIGHT NOW"); the totals give the cumulative picture since boot.
    {
        WedgeWatchdog const& wd = Playerbot::V2::Module::instance().wedge_watchdog();
        std::array<uint32, kWedgeCategoryCount> active_by_cat{};
        for (WedgeInfo const& wi : wd.active())
        {
            const size_t ci = static_cast<size_t>(wi.cat);
            if (ci < active_by_cat.size()) ++active_by_cat[ci];
        }
        out += fmt::format("Wedges active={} (", wd.active().size());
        for (size_t ci = 1; ci < kWedgeCategoryCount; ++ci)   // skip None
            out += fmt::format("{}={} ",
                               WedgeCategoryName(static_cast<WedgeCategory>(ci)),
                               active_by_cat[ci]);
        out += ")\n";
        out += "Wedge episodes (lifetime):";
        for (size_t ci = 1; ci < kWedgeCategoryCount; ++ci)
            out += fmt::format(" {}={}",
                               WedgeCategoryName(static_cast<WedgeCategory>(ci)),
                               p.wedge_by_category[ci]);
        out += "\n";
    }

    // ==== #1C: Top path-fail zones (cheap heatmap) ====
    if (!path_blocks_by_zone.empty())
    {
        std::vector<std::pair<uint32, uint32>> zsorted(
            path_blocks_by_zone.begin(), path_blocks_by_zone.end());
        std::partial_sort(zsorted.begin(),
                          zsorted.begin() + std::min<size_t>(3, zsorted.size()),
                          zsorted.end(),
                          [](auto const& a, auto const& b) { return a.second > b.second; });
        const LocaleConstant loc = sWorld->GetDefaultDbcLocale();
        out += "Top path-block zones:";
        for (size_t i = 0; i < std::min<size_t>(3, zsorted.size()); ++i)
        {
            char const* zn = "?";
            if (AreaTableEntry const* ae = sAreaTableStore.LookupEntry(zsorted[i].first))
                zn = ae->AreaName[loc];
            out += fmt::format(" [{}={}]", zn, zsorted[i].second);
        }
        out += "\n";
    }

    // ==== #1C: 1h trend deltas from the rolling window ====
    {
        PerfCounters::VitalsWindow const w = Services::Perf().vitals_window_snapshot();
        if (w.populated >= 2)
        {
            const int32 d_in_world = int32(w.newest.in_world) - int32(w.oldest.in_world);
            const int32 d_wedged   = int32(w.newest.wedged)   - int32(w.oldest.wedged);
            const int32 d_p99      = int32(w.newest.tick_p99_us) - int32(w.oldest.tick_p99_us);
            const uint32 span_min  = (w.newest.sample_at_ms > w.oldest.sample_at_ms)
                ? (w.newest.sample_at_ms - w.oldest.sample_at_ms) / 60000u : 0u;
            out += fmt::format(
                "Trend (~{}m, {} samples): in_world {:+d} | wedged {:+d} | tick_p99 {:+d}us"
                " | now p99={}us avg p99={}us max p99={}us\n",
                span_min, w.populated, d_in_world, d_wedged, d_p99,
                w.newest.tick_p99_us, w.tick_p99_us_avg, w.tick_p99_us_max);
        }
        else
        {
            out += "Trend: (insufficient samples — building 1h window)\n";
        }
    }

    return out;
}

std::string BgOutcomeReport(size_t max_rows)
{
    if (!Services::Initialized()) return "PlayerbotV2 not initialized.";
    auto buckets = Services::Perf().bg_buckets_snapshot();
    if (buckets.empty())
        return "No BG outcomes recorded yet.\n";

    struct Row {
        uint8_t  cls;
        uint16_t spec;
        uint8_t  bracket_lo10;
        uint32_t wins;
        uint32_t losses;
    };
    std::vector<Row> rows;
    rows.reserve(buckets.size());
    for (auto const& [key, b] : buckets)
    {
        Row r;
        r.cls          = uint8_t((key >> 24) & 0xFFu);
        r.spec         = uint16_t((key >> 8) & 0xFFFFu);
        r.bracket_lo10 = uint8_t(key & 0xFFu);
        r.wins         = b.wins;
        r.losses       = b.losses;
        rows.push_back(r);
    }
    std::sort(rows.begin(), rows.end(), [](Row const& a, Row const& b)
    {
        const uint32_t na = a.wins + a.losses;
        const uint32_t nb = b.wins + b.losses;
        if (na != nb) return na > nb;
        const double pa = na > 0 ? double(a.wins) / na : 0.0;
        const double pb = nb > 0 ? double(b.wins) / nb : 0.0;
        return pa > pb;
    });

    std::string out;
    out.reserve(2048);
    out += "=== BG Outcomes (top N buckets by sample count) ===\n";
    out += "  cls/spec  bracket  n     wins  losses  win%\n";
    size_t shown = 0;
    for (auto const& r : rows)
    {
        if (shown >= max_rows) break;
        const uint32_t n = r.wins + r.losses;
        const double   wp = n > 0 ? (100.0 * double(r.wins) / n) : 0.0;
        out += fmt::format("  cls={:>2} spec={:>4}  L{:>2}-L{:<2}  {:<5} {:<5} {:<5}   {:.1f}%\n",
            uint32(r.cls), uint32(r.spec),
            uint32(r.bracket_lo10) * 10u,
            uint32(r.bracket_lo10) * 10u + 9u,
            n, r.wins, r.losses, wp);
        ++shown;
    }
    if (rows.size() > max_rows)
        out += fmt::format("  ... ({} more buckets)\n", rows.size() - max_rows);
    return out;
}

std::string BgInfoReport()
{
    std::string out;
    out.reserve(2048);
    out += "=== Live BG instances ===\n";
    out += "  type  inst  status  alli/max  horde/max  invA  invH  freeA  freeH\n";

    std::unordered_set<uint32> seen_maps;
    uint32 total_bgs = 0;
    for (BattlemasterListXMapEntry const* xmap : sBattlemasterListXMapStore)
    {
        if (!xmap || xmap->MapID <= 0) continue;
        uint32 map_id = uint32(xmap->MapID);
        if (!seen_maps.insert(map_id).second) continue;
        auto& free_slot_store = sBattlegroundMgr->GetBGFreeSlotQueueStore(map_id);
        for (Battleground* bg : free_slot_store)
        {
            if (!bg) continue;
            const uint32 max_per_team = bg->GetMaxPlayersPerTeam();
            char const* status_str = "?";
            switch (bg->GetStatus())
            {
                case STATUS_NONE:       status_str = "NONE";     break;
                case STATUS_WAIT_QUEUE: status_str = "Q_WAIT";   break;
                case STATUS_WAIT_JOIN:  status_str = "PREP";     break;
                case STATUS_IN_PROGRESS:status_str = "IN_PROG";  break;
                case STATUS_WAIT_LEAVE: status_str = "ENDED";    break;
            }
            out += fmt::format(
                "  {:>4}  {:>4}  {:<7}  {:>3}/{:<3}  {:>3}/{:<3}    {:>3}   {:>3}    {:>3}    {:>3}\n",
                uint32(bg->GetTypeID()), bg->GetInstanceID(), status_str,
                bg->GetPlayersCountByTeam(ALLIANCE), max_per_team,
                bg->GetPlayersCountByTeam(HORDE),    max_per_team,
                bg->GetInvitedCount(ALLIANCE),
                bg->GetInvitedCount(HORDE),
                bg->GetFreeSlotsForTeam(ALLIANCE),
                bg->GetFreeSlotsForTeam(HORDE));
            ++total_bgs;
        }
    }
    if (total_bgs == 0)
        out += "  (no live BGs)\n";
    out += fmt::format("Total live BGs: {}\n", total_bgs);
    return out;
}

std::string DumpFreezeForensics(std::string const& reason)
{
    // Defensive: we're called from FreezeDetector::Handler immediately
    // before ABORT_MSG. The world thread is hung; PlayerbotV2 services
    // may or may not be in a coherent state. Catch everything.
    try
    {
        // Pick a path under sLog->GetLogsDir() so the dump lands next to
        // Server.log / Playerbot.log. Falls back to CWD if the logger isn't
        // configured (early startup freeze).
        std::string dir = sLog->GetLogsDir();
        if (dir.empty()) dir = ".";
        if (dir.back() != '/' && dir.back() != '\\') dir.push_back('/');

        // Timestamp similar to TC's crash-dump filename convention so the
        // freeze dump sorts next to the matching crashes/<ts>.txt file.
        std::time_t t = std::time(nullptr);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        char ts[64] = {};
        std::strftime(ts, sizeof(ts), "freeze_dump_%Y_%m_%d_%H_%M_%S.txt", &tm);
        std::string const path = dir + ts;

        std::ofstream of(path, std::ios::out | std::ios::trunc);
        if (!of.is_open()) return {};

        of << "==== Freeze forensics dump ====\n";
        of << "Reason : " << reason << "\n";
        char hr_ts[64] = {};
        std::strftime(hr_ts, sizeof(hr_ts), "%Y-%m-%d %H:%M:%S", &tm);
        of << "When   : " << hr_ts << "\n\n";

        // Health snapshot (population + budgets + intent throughput +
        // pipeline-state distribution + DB queue depth).
        of << HealthReport() << "\n";

        // Per-bot dump - mirrors the /diag whisper output for every
        // registered bot. Bounded by registry size; with a 1000-bot fleet
        // this is ~100KB on disk, fine for forensic use.
        if (Services::Initialized())
        {
            of << "\n==== Per-bot diagnostics ====\n";
            uint32 count = 0;
            Services::Registry().for_each(
                [&](BotId id, BotRegistryEntry const& /*e*/)
                {
                    of << "\n---- Bot " << id << " ----\n";
                    of << DiagBot(id);
                    ++count;
                });
            of << "\n(" << count << " bots dumped)\n";
        }
        else
        {
            of << "PlayerbotV2 services not initialized - per-bot dump skipped.\n";
        }

        of.flush();
        of.close();
        return path;
    }
    catch (...)
    {
        // We're already aborting; swallow any exception so we don't
        // double-fault on the way down.
        return {};
    }
}

} // namespace Playerbot::Diagnostics
