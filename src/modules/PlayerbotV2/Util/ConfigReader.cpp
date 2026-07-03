#include "ConfigReader.h"
#include "Config.h"
#include "Log.h"

namespace Playerbot {

bool ConfigReader::load(std::string const& file_path)
{
    file_path_ = file_path;

    std::vector<std::string> args;   // No CLI overrides; world.conf takes care of those.
    std::string err;

    // TrinityCore's sConfigMgr is a singleton; we layer our keys onto its state.
    // If the file doesn't exist, fall through to defaults silently.
    if (!file_path_.empty() && !sConfigMgr->LoadAdditionalFile(file_path_, true, err))
    {
        TC_LOG_WARN("playerbot.v2", "[PlayerbotV2] playerbot.conf missing or unreadable ({}): {}", file_path_, err);
        // Defaults are acceptable; not a hard failure.
    }
    apply_from_loaded_config();

    // B-1 observability: playerbot.conf is layered with override=true, so
    // a Playerbot.* key edited in worldserver.conf is silently beaten by
    // the same key in playerbot.conf — operators repeatedly chased "my
    // TotalTarget change does nothing". State the effective values and
    // the precedence once per load/reload so the log answers it.
    TC_LOG_INFO("playerbot.v2",
        "[Config] effective population: TotalTarget={} Floor={} Ceiling={} AutoScale={} "
        "HordePct={} Shape={} MaxHeavyFactor={:.2f} (Playerbot.* keys in '{}' override worldserver.conf)",
        pop_target_, pop_floor_, pop_ceiling_, pop_auto_scale_ ? 1 : 0,
        pop_horde_pct_, pop_shape_, pop_max_heavy_factor_,
        file_path_.empty() ? "<none>" : file_path_);
    return true;
}

bool ConfigReader::reload()
{
    return load(file_path_);
}

void ConfigReader::apply_from_loaded_config()
{
    auto& c = *sConfigMgr;

    ai_worker_threads_     = c.GetIntDefault ("Playerbot.AiWorkerThreads",       0);
    fleet_thread_enabled_  = c.GetBoolDefault("Playerbot.FleetThreadEnabled",    true);
    tick_budget_ms_        = c.GetIntDefault ("Playerbot.TickBudgetMs",          10);
    // #5 Phase 4 parallel snapshot Build. DEFAULT ON. The 2026-06-15 crash
    // (ACCESS_VIOLATION in Build->objective_blacklisted, serial AND parallel) was
    // ultimately ABI SKEW: another session's uncommitted BotAI.h change (added
    // quest_mem_mtx_ + 5 maps, changing BotAI's layout) was only partially picked
    // up by incremental builds — 317 playerbot .obj remained compiled against the
    // OLD layout, so TUs disagreed on objective_blacklist_'s offset -> garbage
    // read. Fixed by a clean recompile of all playerbot TUs. The genuine parallel
    // concern (concurrent Registry().ai() rehash) was separately fixed by
    // pre-resolving the BotAI* on the world thread. Kill-switch: set =0 to serial.
    parallel_snapshot_build_ = c.GetBoolDefault("PlayerbotV2.ParallelSnapshotBuild", true);
    snapshot_build_threads_  = c.GetIntDefault ("PlayerbotV2.SnapshotBuildThreads", 0);

    pop_target_     = c.GetIntDefault ("Playerbot.Population.TotalTarget", 2000);
    pop_floor_      = c.GetIntDefault ("Playerbot.Population.Floor",       100);
    pop_ceiling_    = c.GetIntDefault ("Playerbot.Population.Ceiling",     5000);
    pop_auto_scale_ = c.GetBoolDefault("Playerbot.Population.AutoScale",   true);
    pop_horde_pct_  = static_cast<uint8>(c.GetIntDefault("Playerbot.Population.HordePct", 50));
    pop_shape_      = c.GetStringDefault("Playerbot.Population.Shape",     "MaxHeavy");
    pop_max_heavy_factor_ = float(c.GetFloatDefault("Playerbot.Population.MaxHeavyFactor", 0.40f));

    accept_invites_       = c.GetStringDefault("Playerbot.Interact.AutoAcceptInvites", "friends_or_higher");
    auto_leader_handover_ = c.GetBoolDefault  ("Playerbot.Interact.AutoLeaderHandover", true);
    {
        std::string prefix = c.GetStringDefault("Playerbot.Interact.CommandPrefix", "!");
        command_prefix_ = prefix.empty() ? '!' : prefix[0];
    }

    housing_enabled_   = c.GetBoolDefault("Playerbot.Housing.Enabled", true);
    nbhd_target_pct_   = static_cast<uint8>(c.GetIntDefault("Playerbot.Housing.NeighborhoodTargetOccupancy", 60));
    {
        std::string strict = c.GetStringDefault("Playerbot.Housing.PlotPurchaseEligibilityCheck", "strict");
        housing_strict_ = (strict != "lenient");
    }

    health_port_ = static_cast<uint16>(c.GetIntDefault("Playerbot.Diag.HealthEndpointPort", 0));
    log_level_   = c.GetStringDefault("Playerbot.Log.Level", "info");

    auto_resume_on_boot_ = c.GetBoolDefault("Playerbot.V2.AutoResumeOnBoot", false);
    auto_resume_cap_     = c.GetIntDefault ("Playerbot.V2.AutoResumeCap",    100);
    auto_spawn_on_boot_  = c.GetIntDefault ("Playerbot.V2.AutoSpawnOnBoot",  0);
    random_personality_  = c.GetBoolDefault("Playerbot.V2.RandomPersonality", true);
    archetype_enabled_   = c.GetBoolDefault("PlayerbotV2.Archetype.Enabled", true);
    economy_buy_enabled_ = c.GetBoolDefault("PlayerbotV2.Economy.BuyEnabled", true);
    economy_max_reagent_vendor_multiple_ = static_cast<uint32>(
        c.GetIntDefault("PlayerbotV2.Economy.MaxReagentVendorMultiple", 15));
    session_rhythm_enabled_    = c.GetBoolDefault("PlayerbotV2.SessionRhythm.Enabled", true);
    session_rhythm_multiplier_ = float(c.GetFloatDefault("PlayerbotV2.SessionRhythm.Multiplier", 1.0f));
    session_rhythm_max_logouts_per_cycle_ = static_cast<uint32>(
        c.GetIntDefault("PlayerbotV2.SessionRhythm.MaxLogoutsPerCycle", 25));

    guilds_enabled_                          = c.GetBoolDefault("PlayerbotV2.Guilds.Enabled", true);
    guilds_target_count_per_faction_         = static_cast<uint8>(c.GetIntDefault("PlayerbotV2.Guilds.TargetCountPerFaction", 6));
    guilds_max_members_per_guild_            = static_cast<uint16>(c.GetIntDefault("PlayerbotV2.Guilds.MaxMembersPerGuild", 75));
    guilds_events_enabled_                   = c.GetBoolDefault("PlayerbotV2.Guilds.EventsEnabled", true);
    guilds_recruitment_channel_enabled_      = c.GetBoolDefault("PlayerbotV2.Guilds.RecruitmentChannelEnabled", true);

    max_alts_as_bots_ = static_cast<uint8>(
        c.GetIntDefault("PlayerbotV2.MaxAltsAsBots", 5));

    wedge_watchdog_threshold_ms_ = static_cast<uint32>(
        c.GetIntDefault("PlayerbotV2.WedgeWatchdog.ThresholdMs", 90000));
    wedge_watchdog_combat_threshold_ms_ = static_cast<uint32>(
        c.GetIntDefault("PlayerbotV2.WedgeWatchdog.CombatThresholdMs", 180000));
    wedge_watchdog_min_displacement_ = static_cast<float>(
        c.GetFloatDefault("PlayerbotV2.WedgeWatchdog.MinDisplacementYards", 20.0f));
    wedge_remediation_enabled_ = c.GetBoolDefault("PlayerbotV2.WedgeWatchdog.RemediationEnabled", true);
    wedge_remediation_ms_ = static_cast<uint32>(
        c.GetIntDefault("PlayerbotV2.WedgeWatchdog.RemediationMs", 180000));
    wedge_noprogress_enabled_ = c.GetBoolDefault("PlayerbotV2.WedgeWatchdog.NoProgressEnabled", true);
    wedge_noprogress_ms_ = static_cast<uint32>(
        c.GetIntDefault("PlayerbotV2.WedgeWatchdog.NoProgressMs", 900000));
    wedge_noprogress_radius_ = static_cast<float>(
        c.GetFloatDefault("PlayerbotV2.WedgeWatchdog.NoProgressRadiusYards", 150.0f));

    alert_wedged_bots_         = static_cast<uint32>(c.GetIntDefault("PlayerbotV2.Alert.WedgedBots",        5));
    alert_tick_p99_us_         = static_cast<uint32>(c.GetIntDefault("PlayerbotV2.Alert.TickP99Us",         200000));
    alert_intent_drop_per_min_ = static_cast<uint32>(c.GetIntDefault("PlayerbotV2.Alert.IntentDropPerMin",  600));
    alert_path_fail_per_min_   = static_cast<uint32>(c.GetIntDefault("PlayerbotV2.Alert.PathFailPerMin",     300));
    alert_throttle_ms_         = static_cast<uint32>(c.GetIntDefault("PlayerbotV2.Alert.ThrottleMs",         300000));

    pull_gate_enabled_         = c.GetBoolDefault("PlayerbotV2.PullGate.Enabled",       true);
    pull_gate_max_ratio_       = float(c.GetFloatDefault("PlayerbotV2.PullGate.MaxDetourRatio", 3.0f));
    pull_gate_min_extra_yards_ = float(c.GetFloatDefault("PlayerbotV2.PullGate.MinExtraYards",  40.0f));
    pull_gate_disengage_enabled_ = c.GetBoolDefault("PlayerbotV2.PullGate.DisengageEnabled", true);
    tank_commit_enabled_  = c.GetBoolDefault("PlayerbotV2.PullGate.TankCommitEnabled", true);
    tank_commit_max_ms_   = static_cast<uint32>(
        c.GetIntDefault("PlayerbotV2.PullGate.TankCommitMaxMs", 45000));

    move_step_hold_enabled_ = c.GetBoolDefault("PlayerbotV2.Move.StepHoldEnabled", true);
}

} // namespace Playerbot
