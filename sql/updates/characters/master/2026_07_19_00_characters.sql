-- Perks Program (Trading Post) monthly activity completion tracking.
-- Stores the perks activities a character has completed for the current interval; the set is
-- broadcast in SMSG_PERKS_PROGRAM_ACTIVITY_UPDATE and drives PerksActivityThreshold tender awards.
CREATE TABLE IF NOT EXISTS `character_perks_activity` (
  `guid` BIGINT UNSIGNED NOT NULL COMMENT 'Global Unique Identifier',
  `activityId` INT UNSIGNED NOT NULL COMMENT 'PerksActivity.db2 ID',
  PRIMARY KEY (`guid`, `activityId`),
  CONSTRAINT `fk_character_perks_activity_guid` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Completed Perks Program activities per character';
