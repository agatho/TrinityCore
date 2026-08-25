-- Migration: 0010_leveling_zone
-- Date:    2026-06-03
-- Purpose: Persistent per-bot leveling-zone target for R7 cross-map hub
--          relocation. When a bot exhausts its current zone's quests, it
--          picks the best level-appropriate hub from QuestHubDatabase::
--          GetQuestHubsForBot (ContentTuning DB2 bracket matrix) and STORES
--          the choice here so it is STICKY: on server restart the bot reads
--          this target and resumes traveling toward it instead of re-picking
--          (which, being distance-ranked, would flip-flop / relocate every
--          restart). The choice is re-evaluated only with HYSTERESIS — when
--          the bot's level moves OUTSIDE [bracket_lo, bracket_hi], the stored
--          hub no longer fits and a fresh pick + store happens. 0 hub = unset
--          (bot has never needed relocation, or is mid-pick).
-- Reverts: yes (DROP COLUMN per added column).

ALTER TABLE playerbot_v2_character
    ADD COLUMN leveling_target_hub  INT UNSIGNED     NOT NULL DEFAULT 0,
    ADD COLUMN leveling_target_map  SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    ADD COLUMN leveling_bracket_lo  TINYINT UNSIGNED NOT NULL DEFAULT 0,
    ADD COLUMN leveling_bracket_hi  TINYINT UNSIGNED NOT NULL DEFAULT 0,
    ADD COLUMN leveling_chosen_at   TIMESTAMP        NULL DEFAULT NULL,
    ADD KEY idx_leveling_target_hub (leveling_target_hub);

-- Column docs (kept below the ALTER so the migration body stays a single
-- statement, per the 0005 convention — the migration parser chokes on
-- comment-embedded semicolons inside the statement):
--   leveling_target_hub   QuestHub.hubId the bot is relocating toward (the
--                         sticky leveling-zone choice). 0 = unset.
--   leveling_target_map   Map id of that hub. Drives the UnifiedTravelGraph
--                         cross-map route (walk/fly/ship — never teleport).
--                         NOTE map 0 (Eastern Kingdoms) is a REAL map, so
--                         this field is only meaningful when target_hub != 0.
--   leveling_bracket_lo   Inclusive min level of the chosen hub's
--   leveling_bracket_hi   ContentTuning bracket. Hysteresis gate: re-pick a
--                         new hub only when bot.level < lo OR > hi, so a bot
--                         that out-levels its zone moves on, but small level
--                         gains inside the bracket do NOT churn the choice.
--   leveling_chosen_at    When the current target was chosen. Diagnostics +
--                         a safety re-pick if a target somehow goes stale
--                         (e.g. the hub became unreachable).

-- Record this migration as applied.
INSERT INTO playerbot_v2_schema_version (version, sha256)
VALUES (10, 'pending-fill-at-release-time-with-actual-sha256-of-this-file');
