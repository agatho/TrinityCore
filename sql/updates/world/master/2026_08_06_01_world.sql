--
-- Mythic+ Enemy Forces requirements (server content). When a dungeon has a row here, a keystone run only
-- completes once all bosses are down AND this many hostile non-boss creatures have been killed. Dungeons
-- without a row keep the bosses-only completion rule.
--
CREATE TABLE IF NOT EXISTS `challenge_mode_enemy_forces` (
  `challengeModeId` INT UNSIGNED NOT NULL COMMENT 'MapChallengeMode.db2 ID',
  `requiredKills` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Hostile non-boss kills for 100% Enemy Forces',
  `comment` VARCHAR(255) DEFAULT NULL,
  PRIMARY KEY (`challengeModeId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Mythic+ enemy forces requirements';

-- --- merged: upstream/TDB additions under the same filename (add/add union) ---

-- Update creature_template
UPDATE `creature_template` SET `faction`=7, `unit_flags`=0x100 WHERE `entry`=42383; -- Transient
UPDATE `creature_template` SET `faction`=7, `unit_flags`=0x100 WHERE `entry`=42391; -- West Plains Drifter