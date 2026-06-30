-- Migration: 0014_stuck_objective
-- Date:    2026-06-16
-- Purpose: Persistent, fleet-aggregated STUCK-OBJECTIVE LEDGER (#7 follow-up).
--          The WedgeWatchdog self-remediation layer abandons a bot's current
--          objective when it has been wedged (GoalUnreachable / CombatLoop) past
--          RemediationMs. That abandon is a 5-minute in-memory blacklist (the
--          bot RETRIES automatically once it expires) and, until now, the only
--          record of WHICH content stranded bots was the ephemeral
--          [wedge_remediate] log line (Playerbot.log is truncated every boot).
--
--          This table turns the fleet into a self-documenting content-QA system:
--          every remediation upserts a row keyed by (quest_id, obj_id), so an
--          operator can later run e.g.
--              SELECT quest_id, obj_id, category, hit_count, sample_map,
--                     sample_zone, sample_x, sample_y, sample_bot, last_seen
--              FROM playerbot_v2_stuck_objective ORDER BY hit_count DESC LIMIT 50;
--          to find the quests/objectives that strand the most bots and root-cause
--          them (navmesh gap, off-mesh bridge, bad POI data, friendly-only kill
--          target, permanent blacklist, ...). It is the durable, queryable
--          successor to the manual log-forensics + character_queststatus queries
--          used in the 2026-06-15 cross-region investigation (4494 / 876 / 55660).
--
--          Columns:
--            quest_id / obj_id  The stranded objective (obj_id 0 = quest-level /
--                               no specific sub-objective). Composite PK so each
--                               distinct objective is one accumulating row.
--            category           Last WedgeCategory that remediated it
--                               (GoalUnreachable / CombatLoop).
--            hit_count          Cumulative remediation events across the whole
--                               fleet AND across restarts (the writer flushes a
--                               per-interval delta via hit_count = hit_count +
--                               VALUES(hit_count)). A high count = chronically
--                               stuck content worth investigating.
--            first_seen         When this objective first stranded a bot (set on
--                               insert, never overwritten).
--            last_seen          Most recent remediation (updated every flush). A
--                               stale last_seen ~ the issue resolved itself.
--            sample_*           A representative stuck location + bot for the
--                               objective, so an investigator can fly there /
--                               reproduce without grepping logs.
--
-- Reverts: yes (DROP TABLE).
--
-- Retention: the writer does NOT prune — rows are small, bounded by the number
--            of DISTINCT stuck objectives (hundreds–low thousands), and the long
--            history is the point. Operators may DELETE on last_seen if desired.

CREATE TABLE IF NOT EXISTS playerbot_v2_stuck_objective (
    quest_id     INT     UNSIGNED NOT NULL,
    obj_id       INT     UNSIGNED NOT NULL DEFAULT 0,
    category     VARCHAR(24) NOT NULL DEFAULT '',
    hit_count    BIGINT  UNSIGNED NOT NULL DEFAULT 0,
    first_seen   DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_seen    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    sample_map   INT     UNSIGNED NOT NULL DEFAULT 0,
    sample_zone  INT     UNSIGNED NOT NULL DEFAULT 0,
    sample_x     FLOAT   NOT NULL DEFAULT 0,
    sample_y     FLOAT   NOT NULL DEFAULT 0,
    sample_bot   VARCHAR(48) NOT NULL DEFAULT '',
    PRIMARY KEY (quest_id, obj_id),
    KEY idx_hit_count (hit_count),
    KEY idx_last_seen (last_seen)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- Record this migration as applied (mirrors the 0011/0012/0013 pattern).
INSERT INTO playerbot_v2_schema_version (version, sha256) VALUES
    (14, REPEAT('0', 64))
ON DUPLICATE KEY UPDATE applied_at = CURRENT_TIMESTAMP;
