--
-- RafActivity.db2 (FileDataId 3081207) - the Recruit-A-Friend reward activities. Each activity ties a progress
-- CriteriaTree to a RewardQuest that delivers the reward. Seeded with the live 12.0.7.68275 rows so the reward
-- mapping is present even without a client DB2 extraction.
--
DROP TABLE IF EXISTS `raf_activity`;
CREATE TABLE `raf_activity` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `CriteriaTreeID` int NOT NULL DEFAULT '0',
  `RewardQuestID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `raf_activity` (`ID`, `CriteriaTreeID`, `RewardQuestID`, `VerifiedBuild`) VALUES
(4, 82520, 57850, 68275),
(5, 82526, 57852, 68275),
(6, 82528, 57853, 68275);
