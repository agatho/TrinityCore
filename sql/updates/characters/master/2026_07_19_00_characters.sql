-- Perks Program (Trading Post) monthly activity completion tracking.
-- character_perks_activity      : activities a character has completed for the stored interval.
-- character_perks_activity_criteria : in-progress criteria counters for those activities.
-- `periodStart` stamps the Trading Post interval (UTC month-start unix time) the rows belong to;
-- on login the manager wipes rows from a previous interval so each month starts fresh.
CREATE TABLE IF NOT EXISTS `character_perks_activity` (
  `guid` BIGINT UNSIGNED NOT NULL COMMENT 'Global Unique Identifier',
  `activityId` INT UNSIGNED NOT NULL COMMENT 'PerksActivity.db2 ID',
  `periodStart` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Interval start (UTC unix time)',
  PRIMARY KEY (`guid`, `activityId`),
  CONSTRAINT `fk_character_perks_activity_guid` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Completed Perks Program activities per character';

CREATE TABLE IF NOT EXISTS `character_perks_activity_criteria` (
  `guid` BIGINT UNSIGNED NOT NULL COMMENT 'Global Unique Identifier',
  `criteriaId` INT UNSIGNED NOT NULL COMMENT 'Criteria.db2 ID',
  `counter` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `date` BIGINT NOT NULL DEFAULT 0 COMMENT 'Progress timestamp (unix time)',
  `periodStart` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Interval start (UTC unix time)',
  PRIMARY KEY (`guid`, `criteriaId`),
  CONSTRAINT `fk_character_perks_activity_criteria_guid` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='In-progress Perks Program activity criteria per character';
