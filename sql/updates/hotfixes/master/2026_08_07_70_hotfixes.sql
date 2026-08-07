--
-- Midnight Season 1 M+ season-table seeds (wago.tools MapChallengeMode/MythicPlusSeason* @ 12.0.7.68367,
-- nearest hosted build to 68275; static season rows are patch-level data).
-- DisplaySeason 34 = Midnight S1: its tracked-map set is exactly the 8-dungeon S1 pool, which also
-- settles Seat of the Triumvirate = MCM 239 (not the untuned 583 row) and Algeth'ar Academy = MCM 402.
-- Reward levels: MythicPlusSeason 117 (the ExpansionLevel-11 candidate carrying Midnight reward rows).
--

-- Season dungeon pool (8 dungeons):
-- 239 Seat of the Triumvirate, 556 Pit of Saron, 161 Skyreach, 402 Algeth'ar Academy,
-- 557 Windrunner Spire, 558 Magisters' Terrace, 559 Nexus-Point Xenas, 560 Maisara Caverns
DELETE FROM `mythic_plus_season_tracked_map` WHERE `ID` BETWEEN 253 AND 260;
INSERT INTO `mythic_plus_season_tracked_map` (`ID`, `MapChallengeModeID`, `DisplaySeasonID`, `VerifiedBuild`) VALUES
(253, 239, 34, 68275),
(254, 556, 34, 68275),
(255, 161, 34, 68275),
(256, 402, 34, 68275),
(257, 557, 34, 68275),
(258, 558, 34, 68275),
(259, 559, 34, 68275),
(260, 560, 34, 68275);

-- Season affix set + score bonus rating:
-- Bargains 148/158/162/160 (+15), Guile 147 (+30), Fortified 10 (+15), Tyrannical 9 (+15), Guidance 165 (+0)
DELETE FROM `mythic_plus_season_tracked_affix` WHERE `ID` IN (315,316,317,318,319,320,321,325);
INSERT INTO `mythic_plus_season_tracked_affix` (`ID`, `KeystoneAffixID`, `BonusRating`, `Field_9_1_0_38511_004`, `DisplaySeasonID`, `VerifiedBuild`) VALUES
(315, 148, 15, 0, 34, 68275),
(316, 158, 15, 0, 34, 68275),
(317, 162, 15, 0, 34, 68275),
(318, 160, 15, 0, 34, 68275),
(319, 147, 30, 0, 34, 68275),
(320,  10, 15, 0, 34, 68275),
(321,   9, 15, 0, 34, 68275),
(325, 165,  0, 0, 34, 68275);

-- Resilient Keystone floors +12..+30 (PlayerConditionID gates each floor on the matching
-- "time all 8 S1 dungeons at level N" achievement condition, client-evaluated)
DELETE FROM `mythic_plus_season_key_floor` WHERE `ID` BETWEEN 46 AND 64;
INSERT INTO `mythic_plus_season_key_floor` (`ID`, `KeyFloor`, `PlayerConditionID`, `DisplaySeasonID`, `VerifiedBuild`) VALUES
(46, 12, 145133, 34, 68275),
(47, 13, 145134, 34, 68275),
(48, 14, 145135, 34, 68275),
(49, 15, 145136, 34, 68275),
(50, 16, 145137, 34, 68275),
(51, 17, 145138, 34, 68275),
(52, 18, 145139, 34, 68275),
(53, 19, 145140, 34, 68275),
(54, 20, 145141, 34, 68275),
(55, 21, 145142, 34, 68275),
(56, 22, 145143, 34, 68275),
(57, 23, 145144, 34, 68275),
(58, 24, 145145, 34, 68275),
(59, 25, 145146, 34, 68275),
(60, 26, 145148, 34, 68275),
(61, 27, 145150, 34, 68275),
(62, 28, 145151, 34, 68275),
(63, 29, 145153, 34, 68275),
(64, 30, 145154, 34, 68275);

-- Great Vault reward item levels, season 117 (Midnight S1). WeeklyRewardLevel by keystone
-- DifficultyLevel across the ActivityTier variants; EndOfRunRewardLevel = 0 in modern seasons
-- (end-of-run gear flows via the reward-quest/crest track).
DELETE FROM `mythic_plus_season_reward_levels` WHERE `ID` BETWEEN 409 AND 445;
INSERT INTO `mythic_plus_season_reward_levels` (`ID`, `MythicPlusSeasonID`, `ActivityTierID`, `DifficultyLevel`, `WeeklyRewardLevel`, `EndOfRunRewardLevel`, `VerifiedBuild`) VALUES
(409, 117, 101,  0, 243, 0, 68275),
(410, 117, 102,  0, 256, 0, 68275),
(411, 117, 103,  2, 259, 0, 68275),
(412, 117, 103,  3, 259, 0, 68275),
(413, 117, 103,  4, 263, 0, 68275),
(414, 117, 103,  5, 263, 0, 68275),
(415, 117, 103,  6, 266, 0, 68275),
(416, 117, 103,  7, 269, 0, 68275),
(417, 117, 103,  8, 269, 0, 68275),
(418, 117, 103,  9, 269, 0, 68275),
(419, 117, 103, 10, 272, 0, 68275),
(420, 117, 104,  1, 233, 0, 68275),
(421, 117, 104,  2, 237, 0, 68275),
(422, 117, 105,  1, 233, 0, 68275),
(423, 117, 105,  2, 237, 0, 68275),
(424, 117, 105,  3, 240, 0, 68275),
(425, 117, 105,  4, 243, 0, 68275),
(426, 117, 105,  5, 246, 0, 68275),
(427, 117, 105,  6, 253, 0, 68275),
(428, 117, 105,  7, 256, 0, 68275),
(429, 117, 105,  8, 259, 0, 68275),
(438, 117, 112,  1, 233, 0, 68275),
(439, 117, 115,  5, 246, 0, 68275),
(440, 117, 116,  8, 259, 0, 68275),
(441, 117, 157,  4, 243, 0, 68275),
(442, 117, 157,  6, 253, 0, 68275),
(443, 117, 157,  7, 256, 0, 68275),
(444, 117, 157, 12, 263, 0, 68275),
(445, 117, 157, 13, 269, 0, 68275);
