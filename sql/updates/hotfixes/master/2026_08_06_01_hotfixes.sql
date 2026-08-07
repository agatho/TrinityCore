--
-- Mythic+ season DB2 hotfix mirrors: MythicPlusSeasonKeyFloor (Resilient Keystone floor),
-- MythicPlusSeasonRewardLevels (Great Vault reward item levels + ActivityTierID),
-- MythicPlusSeasonTrackedAffix / MythicPlusSeasonTrackedMap (season affix set / dungeon pool).
--
DROP TABLE IF EXISTS `mythic_plus_season_key_floor`;
CREATE TABLE `mythic_plus_season_key_floor` (
  `ID` INT UNSIGNED NOT NULL DEFAULT '0',
  `KeyFloor` INT NOT NULL DEFAULT '0',
  `PlayerConditionID` INT NOT NULL DEFAULT '0',
  `DisplaySeasonID` INT UNSIGNED NOT NULL DEFAULT '0',
  `VerifiedBuild` INT NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `mythic_plus_season_reward_levels`;
CREATE TABLE `mythic_plus_season_reward_levels` (
  `ID` INT UNSIGNED NOT NULL DEFAULT '0',
  `MythicPlusSeasonID` INT UNSIGNED NOT NULL DEFAULT '0',
  `ActivityTierID` INT NOT NULL DEFAULT '0',
  `DifficultyLevel` INT NOT NULL DEFAULT '0',
  `WeeklyRewardLevel` INT NOT NULL DEFAULT '0',
  `EndOfRunRewardLevel` INT NOT NULL DEFAULT '0',
  `VerifiedBuild` INT NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `mythic_plus_season_tracked_affix`;
CREATE TABLE `mythic_plus_season_tracked_affix` (
  `ID` INT UNSIGNED NOT NULL DEFAULT '0',
  `KeystoneAffixID` INT NOT NULL DEFAULT '0',
  `BonusRating` INT NOT NULL DEFAULT '0',
  `Field_9_1_0_38511_004` INT NOT NULL DEFAULT '0',
  `DisplaySeasonID` INT UNSIGNED NOT NULL DEFAULT '0',
  `VerifiedBuild` INT NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `mythic_plus_season_tracked_map`;
CREATE TABLE `mythic_plus_season_tracked_map` (
  `ID` INT UNSIGNED NOT NULL DEFAULT '0',
  `MapChallengeModeID` INT NOT NULL DEFAULT '0',
  `DisplaySeasonID` INT UNSIGNED NOT NULL DEFAULT '0',
  `VerifiedBuild` INT NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
