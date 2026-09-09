--
-- Prey system + Voidforge (Midnight Season 1) — per-character hunt state.
-- SHIPPED ON BRANCH, NOT APPLIED to the shared realm. Additive; PreyMgr
-- tolerates its absence (OnPlayerLogin is a no-op until this is populated).
--
-- Tracks in-flight / weekly hunt bookkeeping. The weekly cap is 1 hunt per
-- difficulty per zone (up to 12 across the four Midnight zones) — [RESEARCH],
-- enforced server-side once the completion wire is captured.
--

CREATE TABLE IF NOT EXISTS `character_prey_hunt` (
  `guid`         BIGINT UNSIGNED NOT NULL COMMENT 'character guid',
  `HuntId`       INT UNSIGNED NOT NULL COMMENT 'FK prey_hunt_template.Id',
  `Difficulty`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `Status`       TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 available / 1 active / 2 completed',
  `WeekStart`    INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'unix time of the weekly-reset bucket',
  PRIMARY KEY (`guid`,`HuntId`,`WeekStart`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
