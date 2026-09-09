-- Migration: 0005_population_columns
-- Date:    2026-05-07
-- Purpose: Schema additions for the world-population subsystem
--          (BotPopulationManager + BotSetupPipeline). Tracks the
--          one-shot distribution levelup, idempotent setup-pipeline
--          progress, JIT queue-fill provenance, and LRU timestamps.
-- Reverts: yes (DROP COLUMN per added column).

ALTER TABLE playerbot_v2_character
    ADD COLUMN distribution_level     TINYINT UNSIGNED NOT NULL DEFAULT 0
        AFTER owner_verbose,
    ADD COLUMN distribution_at        TIMESTAMP NULL DEFAULT NULL
        AFTER distribution_level,
    ADD COLUMN setup_pipeline_state   TINYINT UNSIGNED NOT NULL DEFAULT 0
        AFTER distribution_at,
    ADD COLUMN jit_for_queue          VARCHAR(48) NULL DEFAULT NULL
        AFTER setup_pipeline_state,
    ADD COLUMN last_seen_at           TIMESTAMP NULL DEFAULT NULL
        AFTER jit_for_queue,
    ADD KEY idx_distribution_level (distribution_level),
    ADD KEY idx_jit_for_queue (jit_for_queue),
    ADD KEY idx_last_seen_at (last_seen_at);

-- Column docs (kept here below the ALTER so the migration body stays a
-- single statement free of comment-embedded semicolons that the migration
-- parser used to choke on):
--   distribution_level      Floor level the BotPopulationManager set this
--                           bot to. 0 = never distributed. The shaper
--                           refuses to lower a bot's level (see
--                           project_v2_world_population.md A5). Gameplay
--                           leveling beyond this floor proceeds normally.
--   distribution_at         When the distribution levelup happened.
--                           Used by hygiene + diagnostics; NULL until set.
--   setup_pipeline_state    Bitmask of completed BotSetupPipeline steps.
--                           bit 0 = SetLevel, 1 = StarterKit,
--                           2 = GenerateGear, 3 = AutoEquip,
--                           4 = ApplyTalents, 5 = LearnProfessions,
--                           6 = AcquireMount, 7 = PlaceInCapital,
--                           8 (high bits in 0xFF marker) = TravelToZone +
--                           Mark complete. 0xFF = pipeline complete.
--   jit_for_queue           Tag set when the bot was JIT-spawned to fill
--                           a player's queue (e.g. "BG_AB", "LFG_5man").
--                           Used by hygiene cleanup to delete bots that
--                           were created solely for queue-fill and have
--                           outlived their retention window. NULL = bot
--                           is part of the regular distribution pool.
--   last_seen_at            UTC timestamp of last login or significant
--                           AI tick. Drives LRU log-out when the
--                           population exceeds target.

-- Record this migration as applied.
INSERT INTO playerbot_v2_schema_version (version, sha256)
VALUES (5, 'pending-fill-at-release-time-with-actual-sha256-of-this-file');
