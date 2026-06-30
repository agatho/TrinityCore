-- Migration: 0011_fleet_vitals
-- Date:    2026-06-14
-- Purpose: Persist the fleet-vitals rolling window (#1C of
--          docs/LIVING_SERVER_PLAN_20260614.md) so telemetry survives a
--          worldserver restart and the operator gets a REAL trend / incident
--          replay instead of an in-memory window that resets to zero on every
--          bounce. A 60s job in PlayerbotV2.cpp computes one sample bucket
--          (the same VitalsBucket pushed into PerfCounters' in-memory ring)
--          and writes one row here.
--
--          Columns mirror PerfCounters::VitalsBucket plus the per-category
--          wedge breakdown (cheap to store, makes the persisted history
--          self-describing for a later trend dashboard). Counts are the
--          instantaneous fleet census at sample time; the *_per_* columns are
--          rates derived over the 60s window from cumulative-counter deltas.
--
-- Reverts: yes (DROP TABLE).
--
-- Retention: the writer does NOT prune — a row/min is ~525K rows/yr, trivial
--            for InnoDB, and operators want the long-tail history for incident
--            forensics. If retention is ever needed it's a single scheduled
--            DELETE on sample_at; intentionally left to operator policy.
--
-- Indexing: PRIMARY KEY on the auto-increment id; a secondary KEY on
--           sample_at for the common "last 1h / last 24h" range scan the trend
--           reader issues.

CREATE TABLE IF NOT EXISTS playerbot_v2_fleet_vitals_sample (
    id                  BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    sample_at           DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    in_world            INT  UNSIGNED NOT NULL DEFAULT 0,
    alive               INT  UNSIGNED NOT NULL DEFAULT 0,
    in_combat           INT  UNSIGNED NOT NULL DEFAULT 0,
    wedged              INT  UNSIGNED NOT NULL DEFAULT 0,
    -- Per-category active wedge counts (WedgeCategory enum order; None at
    -- index 0 is normally 0). Stored as discrete columns so a SQL trend query
    -- can GROUP/aggregate a single category without parsing a blob.
    wedged_navmesh      INT  UNSIGNED NOT NULL DEFAULT 0,
    wedged_offmesh      INT  UNSIGNED NOT NULL DEFAULT 0,
    wedged_travel       INT  UNSIGNED NOT NULL DEFAULT 0,
    wedged_combatloop   INT  UNSIGNED NOT NULL DEFAULT 0,
    wedged_pickernone   INT  UNSIGNED NOT NULL DEFAULT 0,
    wedged_goalunreach  INT  UNSIGNED NOT NULL DEFAULT 0,
    tick_p50_us         INT  UNSIGNED NOT NULL DEFAULT 0,
    tick_p99_us         INT  UNSIGNED NOT NULL DEFAULT 0,
    intents_per_sec     INT  UNSIGNED NOT NULL DEFAULT 0,
    intents_dropped     INT  UNSIGNED NOT NULL DEFAULT 0,
    path_fail_per_min   INT  UNSIGNED NOT NULL DEFAULT 0,
    avg_level           FLOAT NOT NULL DEFAULT 0,
    KEY idx_sample_at (sample_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- Record this migration as applied.
INSERT INTO playerbot_v2_schema_version (version, sha256) VALUES
    (11, REPEAT('0', 64))
ON DUPLICATE KEY UPDATE applied_at = CURRENT_TIMESTAMP;
